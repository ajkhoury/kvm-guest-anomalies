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

#if defined(_WIN32)
#include <windows.h>
#else
#define _GNU_SOURCE
#include <unistd.h>
#endif
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include <errno.h>
#include "drx.h"

/* Include platform-specific anomaly implementations */
#include "pending_dbg_causes.inl"

void print_help(const char *progname)
{
	printf("Usage: %s [test...]\n", progname);
	printf("Available tests:\n");
	printf("  pending-dbg-causes    Test if pending debug exceptions cause anomalies\n");
}

int main(int argc, char *argv[])
{
	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "help") == 0 || strcmp(argv[i], "-help") == 0 ||
		    strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
			print_help(argv[0]);
			return 1;
		}

		if (strcmp(argv[i], "pending-dbg-causes") == 0) {
			return anomaly_pending_dbg_causes();
		} else {
			fprintf(stderr, "Unknown test: %s\n", argv[i]);
			return EINVAL;
		}
	}

	return 0;
}