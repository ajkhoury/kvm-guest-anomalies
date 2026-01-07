/*
 * Wrappers around ptrace system calls.
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
 */

#ifndef PTRACE_WRAPPER_H
#define PTRACE_WRAPPER_H 1
#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/wait.h>

static inline int ptrace_trace(void)
{
	if (ptrace(PTRACE_TRACEME, 0, 0, 0) != 0)
		return -1;
	return 0;
}

static inline int ptrace_continue(pid_t pid, int signal)
{
	if (ptrace(PTRACE_CONT, pid, 0, (void *)(uintptr_t)signal) != 0)
		return -1;
	return 0;
}

static inline int ptrace_write_regs(pid_t pid, struct user_regs_struct *regs)
{
	if (ptrace(PTRACE_SETREGS, pid, 0, regs) != 0) {
		return -1;
	}
	return 0;
}

static inline int ptrace_read_regs(pid_t pid, struct user_regs_struct *result_regs)
{
	if (ptrace(PTRACE_GETREGS, pid, 0, result_regs) != 0)
		return -1;
	return 0;
}

static inline int ptrace_write_debugreg(pid_t pid, int index, uintptr_t value)
{
	const size_t off = offsetof(struct user, u_debugreg[index]);
	if (ptrace(PTRACE_POKEUSER, pid, (void *)off, (void *)value) == -1 && errno)
		return -1;
	return 0;
}

static inline int ptrace_read_debugreg(pid_t pid, int index, uint64_t *result_value)
{
	const size_t off = offsetof(struct user, u_debugreg[index]);
	long result = ptrace(PTRACE_PEEKUSER, pid, (void *)off, 0);
	if (result == -1 && errno)
		return -1;
	*result_value = (uint64_t)(unsigned long)result;
	return 0;
}

#endif /* PTRACE_WRAPPER_H */
