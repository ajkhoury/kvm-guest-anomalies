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

/* Segment selectors for AMD64 Windows */
#ifndef RPL_MASK
#define RPL_MASK 3
#endif
#define KGDT64_R3_CMCODE 0x0020
#define KGDT64_R3_CODE 0x0030
#define X86AMD64_R3_COMPAT_MODE_CODE (KGDT64_R3_CMCODE | RPL_MASK)  /* AMD64 compatibility mode switching */
#define X86AMD64_R3_LONG_MODE_CODE (KGDT64_R3_CODE | RPL_MASK)

__attribute__((naked))
uintptr_t switch_mode(uintptr_t cs, uintptr_t ip, uintptr_t sp, uintptr_t flags)
{
	__asm__ __volatile__(
		"pushq %%rbp\n"
		"pushq %%rbx\n"
		"pushq %%rdi\n"
		"pushq %%rsi\n"
		"movq %%rsp, (%%r8)\n"  /* store current rsp in new stack. */
		"movq %%ss, %%rax\n"    /* store current ss in rax */
		"pushq %%rax\n"         /* push ss */
		"pushq %%r8\n"          /* push rsp */
		"pushq %%r9\n"          /* push rflags */
		"pushq %%rcx\n"         /* push cs */
		"pushq %%rdx\n"         /* push rip */
		"iretq\n"               /* switch to new context via iret */
		: : : "memory"
	);
}

__attribute__((naked)) void cpuid_80000001_hlt(void)
{
	__asm__ __volatile__(
		"xor %%ecx, %%ecx\n"
		"mov $0x80000001, %%eax\n"
		"cpuid\n"
		"hlt\n"
		: : : "memory"
	);
}

LONG WINAPI intel_cpuid_prot_mode_syscall_veh(EXCEPTION_POINTERS* info)
{
	if (info->ExceptionRecord->ExceptionCode != EXCEPTION_PRIV_INSTRUCTION)
		return EXCEPTION_CONTINUE_SEARCH;

	const PCONTEXT ctx = info->ContextRecord;
	if (ctx->SegCs != X86AMD64_R3_COMPAT_MODE_CODE)
		return EXCEPTION_CONTINUE_SEARCH;

	/* restore context to return to caller after CPUID */
	ctx->Rsp = *(uint64_t *)ctx->Rsp;
	ctx->Rsi = *(volatile uint64_t *)(ctx->Rsp);
	ctx->Rsp += 8;
	ctx->Rdi = *(volatile uint64_t *)(ctx->Rsp);
	ctx->Rsp += 8;
	ctx->Rbx = *(volatile uint64_t *)(ctx->Rsp);
	ctx->Rsp += 8;
	ctx->Rbp = *(volatile uint64_t *)(ctx->Rsp);
	ctx->Rsp += 8;
	ctx->Rip = *(volatile uint64_t *)(ctx->Rsp);
	ctx->Rsp += 8;
	ctx->SegCs = X86AMD64_R3_LONG_MODE_CODE; /* 64-bit user-mode code selector */
	ctx->Rax = !!(ctx->Rdx & 0x800 /* bit 11 */);

	return EXCEPTION_CONTINUE_EXECUTION;
}

int anomaly_intel_cpuid_prot_mode_syscall(void)
{
	/* Register VEH handler for PRIV_INSTRUCTION from prot-mode. */
	void *const veh_handle = AddVectoredExceptionHandler(1, intel_cpuid_prot_mode_syscall_veh);
	if (veh_handle == NULL) {
		fprintf(stderr, "failed to add veh!");
		return -1;
	}

	/* Allocate a low memory page for prot-mode code and stack. */
	void *prot_mode_buf = NULL;
	for (uint64_t addr = 0x100000; addr < 0xFFFF0000; addr += 0x1000) {
		prot_mode_buf = VirtualAlloc((void *)addr, 0x8000, MEM_COMMIT | MEM_RESERVE,
					     PAGE_EXECUTE_READWRITE);
		if (prot_mode_buf)
			break;
	}
	if (prot_mode_buf == NULL) {
		RemoveVectoredExceptionHandler(veh_handle);
		fprintf(stderr, "failed to allocate low memory pages!");
		return -1;
	}

	/* Copy prot-mode code to allocated page. */
	memcpy(prot_mode_buf, (void *)cpuid_80000001_hlt, 16);

	/* Switch to prot-mode and execute CPUID + HLT. */
	const uintptr_t result = switch_mode(X86AMD64_R3_COMPAT_MODE_CODE,
					     (uintptr_t)prot_mode_buf,
					     (uintptr_t)prot_mode_buf + 0x8000-8,
					     __readeflags());

	RemoveVectoredExceptionHandler(veh_handle);

	VirtualFree(prot_mode_buf, 0, MEM_RELEASE);

	printf("CPUID[80000001H].EDX[11]: %i\n", (int)result);

	return 0;
}
