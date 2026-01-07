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

#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include "drx.h"

/* Global variables. */
static uint64_t g_dr6 = 0;
static uint16_t ss_probe = 0;

void print_dr6(uint64_t dr6)
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

LONG WINAPI veh_handler(EXCEPTION_POINTERS* info)
{
	if (info->ExceptionRecord->ExceptionCode != EXCEPTION_SINGLE_STEP)
		return EXCEPTION_CONTINUE_SEARCH;

	PCONTEXT ctx = info->ContextRecord;
	g_dr6 = ctx->Dr6;

	ctx->EFlags &= ~0x100; /* clear trap flag (single-step). */
	ctx->Dr0 = 0;
	ctx->Dr6 = 0;
	ctx->Dr7 = 0;

	return EXCEPTION_CONTINUE_EXECUTION;
}

int main(void)
{
	void *veh_handle = AddVectoredExceptionHandler(1, veh_handler);
	if (veh_handle == NULL) {
		fprintf(stderr, "failed to add veh!");
		return -1;
	}

	/* Load SS selector */
	__asm__ __volatile__("movw %%ss, %0" : "=r"(ss_probe) : : );

	/* Set hardware breakpoint on SS access. */
	CONTEXT ctx = { .ContextFlags = CONTEXT_DEBUG_REGISTERS };
	GetThreadContext(GetCurrentThread(), &ctx);
	ctx.Dr0 = (uint64_t)&ss_probe;
	ctx.Dr7 = DR7_L0_BIT | DR7_G0_BIT | DR7_RW0_DATA_RW | DR7_LEN0_2_BYTE;
	SetThreadContext(GetCurrentThread(), &ctx);

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
		"orl $0x100, (%%rsp)\n"    /* set TF in saved RFLAGS on stack */
		"popfq\n"
		"movw (%0), %%ss\n"        /* load SS from probe page */
		"cpuid\n"                  /* trigger intercepting instruction */
		"popq %%rbx\n"             /* #DB fires here with DR6 missing B0 under KVM/VMX pre-patch */
		: : "r"(&ss_probe) : "rax", "rcx", "rdx", "memory"
	);

	print_dr6(g_dr6);

	RemoveVectoredExceptionHandler(veh_handle);

	return 0;
}
