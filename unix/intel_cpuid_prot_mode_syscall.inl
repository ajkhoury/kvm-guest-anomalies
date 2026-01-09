/*
 * Reproducer code for testing side effecting of pending debug exceptions in
 * KVM guest VMs.
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


#include "ptrace.h"
#include <sys/mman.h>
#include <ucontext.h>
#include <setjmp.h>

/* Segment selectors for x86-64 Linux */
#define CS_32BIT_USER  0x0023  /* 32-bit user code segment (compatibility mode) */
#define CS_64BIT_USER  0x0033  /* 64-bit user code segment (long mode) */

/* Global state for signal handler */
static sigjmp_buf g_jmp_buf;
static volatile uint64_t g_cpuid_edx = 0;

/*
 * 32-bit compatibility mode code stub.
 * This will be copied to low memory and executed.
 * Executes CPUID with EAX=0x80000001 and then HLT.
 */
__attribute__((naked)) void cpuid_80000001_hlt(void)
{
	__asm__ __volatile__(
		".code32\n"
		"xor %%ecx, %%ecx\n"
		"mov $0x80000001, %%eax\n"
		"cpuid\n"
		"hlt\n"
		".code64\n"
		: : : "memory"
	);
}

/*
 * Signal handler for SIGSEGV from HLT instruction in compatibility mode.
 * Uses siglongjmp to escape back to 64-bit mode cleanly.
 */
static void intel_cpuid_prot_mode_handler(int sig, siginfo_t *info, void *ucontext)
{
	(void)info;
	if (g_cpuid_edx != (uint64_t)-1 || sig != SIGSEGV) {
		_exit(sig | 0x80); /* Not our signal, pass it on */
	}

	ucontext_t *ctx = (ucontext_t *)ucontext;

	/* Save EDX from CPUID result before we longjmp away */
	g_cpuid_edx = ctx->uc_mcontext.gregs[REG_RDX];

	/* Jump back to our saved context - this properly restores long mode */
	siglongjmp(g_jmp_buf, 1);
}

static inline uintptr_t read_rflags(void)
{
	uintptr_t flags;
	__asm__ __volatile__("pushfq; popq %0" : "=r"(flags) : : "memory");
	return flags;
}

/*
 * Switch to compatibility mode and execute the code at 'ip'.
 */
static void switch_mode(uintptr_t cs, uintptr_t ip, uintptr_t sp, uintptr_t flags)
{
	__asm__ __volatile__(
		"movq %%ss,%%rax\n" /* Build IRETQ frame on current stack */
		"pushq %%rax\n"     /* SS */
		"pushq %[sp]\n"     /* RSP - new stack pointer */
		"pushq %[flags]\n"  /* RFLAGS */
		"pushq %[cs]\n"     /* CS - 0x23 for 32-bit */
		"pushq %[ip]\n"     /* RIP - code to execute */
		"iretq\n"           /* IRETQ pops: RIP, CS, RFLAGS, RSP, SS */
		:
		: [cs]"r"(cs), [ip]"r"(ip), [sp]"r"(sp), [flags]"r"(flags)
		: "memory", "cc", "rax"
	);
}

int anomaly_intel_cpuid_prot_mode_syscall(void)
{
	struct sigaction sa, old_sa;
	void *prot_mode_buf = MAP_FAILED;
	int rc = -1;
	int result;
	stack_t ss, old_ss;
	void *alt_stack = NULL;

	/* Allocate alternate signal stack */
	alt_stack = mmap(NULL, SIGSTKSZ, PROT_READ | PROT_WRITE,
			 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (alt_stack == MAP_FAILED) {
		perror("mmap alt stack failed");
		return -1;
	}

	ss.ss_sp = alt_stack;
	ss.ss_size = SIGSTKSZ;
	ss.ss_flags = 0;
	if (sigaltstack(&ss, &old_ss) < 0) {
		perror("sigaltstack failed");
		munmap(alt_stack, SIGSTKSZ);
		return -1;
	}

	/* Set up signal handler for SIGSEGV */
	memset(&sa, 0, sizeof(sa));
	sa.sa_sigaction = intel_cpuid_prot_mode_handler;
	sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
	sigemptyset(&sa.sa_mask);

	if (sigaction(SIGSEGV, &sa, &old_sa) < 0) {
		perror("sigaction failed");
		goto cleanup_stack;
	}

	/* Allocate memory in low 32-bit address space */
	prot_mode_buf = mmap(NULL, 0x8000, PROT_READ | PROT_WRITE | PROT_EXEC,
			     MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);

	if (prot_mode_buf == MAP_FAILED) {
		for (uintptr_t addr = 0x10000000; addr < 0x70000000; addr += 0x1000000) {
			prot_mode_buf = mmap((void *)addr, 0x8000,
					     PROT_READ | PROT_WRITE | PROT_EXEC,
					     MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE,
					     -1, 0);
			if (prot_mode_buf != MAP_FAILED)
				break;
		}
	}

	if (prot_mode_buf == MAP_FAILED) {
		perror("mmap failed - could not allocate low memory");
		fprintf(stderr, "Try sudo sysctl -w vm.mmap_min_addr=65536\n");
		goto cleanup_signal;
	}

	if ((uint64_t)(uintptr_t)prot_mode_buf > 0xFFFFFFFF) {
		fprintf(stderr, "buffer address exceeds 32-bit range\n");
		goto cleanup_mmap;
	}

	/* Copy compatibility mode code stub */
	memcpy(prot_mode_buf, cpuid_80000001_hlt, 16);

	/* Stack pointer at end of buffer */
	uintptr_t new_sp = (uintptr_t)prot_mode_buf + 0x7000;
	new_sp &= ~0xFUL;

#if 0
	printf("Switching to 32-bit compatibility mode...\n");
	printf("  CS: 0x%02x\n", CS_32BIT_USER);
	printf("  IP: %p\n", prot_mode_buf);
	printf("  SP: 0x%lx\n", new_sp);
#endif

	g_cpuid_edx = -1;

	/* Save current context - siglongjmp will return 1 here */
	if (sigsetjmp(g_jmp_buf, 1) == 0) {
		/* First time - switch to compat mode */
		const uintptr_t flags = read_rflags() & ~0x300; /* clear TF+IF */
		switch_mode(CS_32BIT_USER, (uintptr_t)prot_mode_buf, new_sp, flags);
		/* Should never reach here */
		fprintf(stderr, "ERROR: switch_mode returned!\n");
		goto cleanup_mmap;
	}

	/* Returned from signal handler via siglongjmp */
	result = !!(g_cpuid_edx & (1 << 11));
	printf("CPUID[80000001H].EDX bit 11 (SYSCALL): %d\n", result);

	rc = 0;

cleanup_mmap:
	if (prot_mode_buf != MAP_FAILED)
		munmap(prot_mode_buf, 0x8000);

cleanup_signal:
	sigaction(SIGSEGV, &old_sa, NULL);

cleanup_stack:
	sigaltstack(&old_ss, NULL);
	if (alt_stack != MAP_FAILED)
		munmap(alt_stack, SIGSTKSZ);

	return rc;
}
