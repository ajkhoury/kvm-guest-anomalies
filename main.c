/*
 * Reproducer code for testing anomalies within KVM guest VMs.
 *
 * Copyright (C) 2024-2026 Aidan Khoury
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 * Authors: Aidan Khoury <aidan@aktech.ai>
 */

#define _GNU_SOURCE
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <inttypes.h>
#include <unistd.h>

#include "ptrace.h"
#include "drx.h"

static uint16_t ss_probe = 0;

static void print_dr6(uint64_t dr6)
{
	printf("DR6: 0x%" PRIx64 "\n", dr6);
	printf("    B0 (DR0 hit):     %i\n", (dr6 & DR6_B0_BIT) != 0);
	printf("    B1 (DR1 hit):     %i\n", (dr6 & DR6_B1_BIT) != 0);
	printf("    B2 (DR2 hit):     %i\n", (dr6 & DR6_B2_BIT) != 0);
	printf("    B3 (DR3 hit):     %i\n", (dr6 & DR6_B3_BIT) != 0);
	printf("    BD (DR access):   %i\n", (dr6 & DR6_BD_BIT) != 0);
	printf("    BS (single-step): %i\n", (dr6 & DR6_BS_BIT) != 0);
	printf("    BT (task switch): %i\n", (dr6 & DR6_BT_BIT) != 0);
}

static int run(int pipefd)
{
	uintptr_t addr;

	/* Load current SS selector into ss_probe */
	__asm__ __volatile__("movw %%ss, %0" : "=r"(ss_probe) : : );

	/* Send address of ss_probe to parent */
	addr = (uintptr_t)&ss_probe;
	if (write(pipefd, &addr, sizeof(addr)) != sizeof(addr))
		return 2;
	close(pipefd);

	/* Become trace and stop so parent can program DRx */
	if (ptrace_trace())
		return 3;
	raise(SIGSTOP);

	/*
	 * Trigger the test.
	 * - DR7 should be programmed to watch ss_probe
	 * - Enable single-stepping
	 * - MOV SS from probe page, to cause delayed B0+BS conditions
	 * - Execute intercepting instruction (i.e. CPUID)
	 */
	__asm__ __volatile__(
		"pushq %%rbx\n"
		"pushfq\n"
		"orl $0x100, (%%rsp)\n" /* set TF in saved RFLAGS on stack */
		"popfq\n"
		"movw (%0), %%ss\n"     /* block exceptions on mov ss */
		"cpuid\n"               /* trigger vm intercept */
		"popq %%rbx\n"          /* only single #DB should fire here with DR6 B0+BS set */
		: : "r"(&ss_probe) : "rax", "rcx", "rdx", "memory"
	);

	return 0;
}

int main(void)
{
	int status = 0;
	int pipefd[2];
	pid_t child;
	uintptr_t addr;
	int signal;
	uint64_t dr6 = 0;
	int got_trap = 0;

	if (pipe(pipefd) != 0) {
		perror("pipe");
		return 1;
	}

	child = fork();
	if (child < 0) {
		perror("fork");
		return 1;
	}

	if (child == 0) {
		close(pipefd[0]); /* close read end */
		return run(pipefd[1]);
	}

	/* Read probe address from child */
	addr = 0;
	if (read(pipefd[0], &addr, sizeof(addr)) != sizeof(addr)) {
		fprintf(stderr, "failed to read probe addr from child\n");
		return 1;
	}
	close(pipefd[0]);

	/* Wait for child to stop on SIGSTOP */
	if (waitpid(child, &status, 0) < 0) {
		perror("waitpid(SIGSTOP)");
		return 1;
	} else if (!WIFSTOPPED(status)) {
		fprintf(stderr, "child did not stop as expected\n");
		return 1;
	}

	/*
	 * Program the hwbp watchpoint.
	 * DR0 = probe addr
	 * DR7 = L0|G0|RW=read/write|LEN=2 bytes
	 */
	const uint64_t dr7 = DR7_L0_BIT | DR7_G0_BIT | DR7_RW0_DATA_RW | DR7_LEN0_2_BYTE;
	if (ptrace_write_debugreg(child, 0, (uint64_t)addr) != 0 ||
	    ptrace_write_debugreg(child, 6, 0) != 0 ||
	    ptrace_write_debugreg(child, 7, (uint64_t)dr7) != 0) {
		perror("ptrace write DRx");
		return 1;
	}

	/* Run until #DB shows up as SIGTRAP */
	if (ptrace_continue(child, 0) != 0) {
		perror("ptrace continue");
		return 1;
	}

	/* Wait for child to hit watchpoint or exit */
	for (;;) {
		if (waitpid(child, &status, 0) < 0) {
			perror("waitpid(run)");
			return 1;
		}

		/* Check for exit */
		if (WIFEXITED(status) || WIFSIGNALED(status))
			break;
		else if (!WIFSTOPPED(status))
			continue;

		signal = WSTOPSIG(status);

		/* Handle SIGTRAP from #DB */
		if (signal == SIGTRAP && !got_trap) {
			struct user_regs_struct regs;

			/* Read DR6 from ptrace */
			if (ptrace_read_debugreg(child, 6, &dr6) != 0) {
				perror("ptrace read DR6");
				return 1;
			}

			/* Clear TF in RFLAGS, clear DR0/DR6/DR7 */
			if (ptrace_read_regs(child, &regs) != 0) {
				perror("PTRACE_GETREGS");
				return 1;
			}
			regs.eflags &= ~0x100; /* TF */
			if (ptrace_write_regs(child, &regs) != 0 ||
			    ptrace_write_debugreg(child, 0, 0) != 0 ||
			    ptrace_write_debugreg(child, 6, 0) != 0 ||
			    ptrace_write_debugreg(child, 7, 0) != 0) {
				perror("ptrace clear regs");
				return 1;
			}

			/* Resume without re-delivering SIGTRAP */
			if (ptrace_continue(child, 0) != 0) {
				perror("ptrace continue (after trap)");
				return 1;
			}

			got_trap = 1;
			continue;
		}

		/* Any other stop just continue and pass the signal through */
		if (ptrace_continue(child, signal) != 0) {
			perror("ptrace continue (pass signal)");
			return 1;
		}
	}

	if (!got_trap) {
		printf("    No SIGTRAP/#DB observed (unexpected)\n");
		return 0;
	}

	print_dr6(dr6);

	return 0;
}