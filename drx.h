/**
 * Copyright (C) 2017-2026 Aidan Khoury
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
 * @file drx.h
 * @author Aidan Khoury (ajkhoury)
 * @date 8/1/2017
 */

#ifndef _ARCH_X86_DRX_
#define _ARCH_X86_DRX_
#if defined(_MSC_VER) && (_MSC_VER > 1000)
#pragma once
#endif

/**
 * Indicates (when set) that its associated breakpoint condition was met when a debug exception was generated.
 * These flags are set if the condition described for each breakpoint by the LENn, and R/Wn flags in debug
 * control  register DR7 is true. They may or may not be set if the breakpoint is not enabled by the Ln or
 * the Gn flags in register DR7. Therefore on a #DB, a debug handler should check only those B0-B3 bits which
 * corresponds to an enabled breakpoint.
 */
#define DR6_B0_BIT      0x1     /* bit 0 */
#define DR6_B1_BIT      0x2     /* bit 1 */
#define DR6_B2_BIT      0x4     /* bit 2 */
#define DR6_B3_BIT      0x8     /* bit 3 */
#define DR6_TRAP_BITS   (DR6_B0_BIT|DR6_B1_BIT|DR6_B2_BIT|DR6_B3_BIT)

/**
 * Indicates that the next instruction in the instruction stream accesses one of the debug registers
 * (DR0 through DR7). This flag is enabled when the GD (general detect) flag in debug control register
 * DR7 is set. See Section 17.2.4, "Debug Control Register (DR7)", for further explanation of the purpose
 * of this flag.
 */
#define DR6_BD_BIT      0x2000  /* bit 13 */

/**
 * Indicates (when set) that the debug exception was triggered by the singlestep execution mode
 * (enabled with the TF flag in the EFLAGS register). The single-step mode is the highest priority
 * debug exception. When the BS flag is set, any of the other debug status bits also may be set.
 */
#define DR6_BS_BIT      0x4000  /* bit 14 */

/**
 * Indicates (when set) that the debug exception resulted from a task switch where the T flag (debug trap
 * flag) in the TSS of the target task was set. See Section 7.2.1, "Task-State Segment (TSS)", for the format
 * of a TSS. There is no flag in debug control register DR7 to enable or disable this exception; the T flag
 * of the TSS is the only enabling flag.
 */
#define DR6_BT_BIT      0x8000  /* bit 15 */

/**
 * Indicates (when clear) that a debug exception (#DB) or breakpoint exception (#BP) occurred inside an RTM
 * region while advanced debugging of RTM transactional regions was enabled (see Section 17.3.3). This bit is
 * set for any other debug exception (including all those that occur when advanced debugging of RTM transactional
 * regions is not enabled). This bit is always 1 if the processor does not support RTM.
 */
#define DR6_RTM_BIT     0x10000 /* bit 16 */

/** Define reserved bits in DR6 which are always set to 1. */
#define DR6_FIXED       0xFFFE0FF0
#define DR6_RESERVED    0xFFFF0FF0
#define DR6_INIT        DR6_RESERVED
#define DR6_VOLATILE    0x0001E00F


/**
 * Setting up debug registers
 *
 * Intel Manual Vol. 3B: Part 2, 17.2 DEBUG REGISTERS
 * AMD Manual Vol. 2, 13.1.1 - Debug Registers
 *
 * DR7 bits:
 *  0th bit = DR0 local breakpoint enable
 *  1st bit = DR0 global breakpoint enable
 *  2nd bit = DR1 local breakpoint enable
 *  3rd bit = DR1 global breakpoint enable
 *  4th bit = DR2 local breakpoint enable
 *  5th bit = DR2 global breakpoint enable
 *  6th bit = DR3 local breakpoint enable
 *  7th bit = DR3 global breakpoint enable
 *
 *  8th bit = Local exact breakpoint enable
 *  9th bit = Global exact breakpoint enable
 *
 *  10th bit = Reserved always set
 *
 *  11th bit = Restricted transactional memory flag
 *
 *  13th bit = General detect enable flag
 *
 *  16th and 17th bits = R/W0 breakpoint condition field
 *  18th and 19th bits = LEN0 breakpoint size/alignment field
 *  20th and 21st bits = R/W1 breakpoint condition field
 *  22nd and 23rd bits = LEN1 breakpoint size/alignment field
 *  24th and 25th bits = R/W2 breakpoint condition field
 *  26th and 27th bits = LEN2 breakpoint size/alignment field
 *  28th and 29th bits = R/W3 breakpoint condition field
 *  30th and 31st bits = LEN3 breakpoint size/alignment field
 *
 *  R/W0 through R/W3 fields:
 *      00 - Break on instruction execution only.
 *      01 - Break on data writes only.
 *      10 - Break on I/O reads or writes.
 *      11 - Break on data reads or writes but not instruction fetches.
 *
 *  LEN0 through LEN3 fields:
 *      00 - 1-byte length.
 *      01 - 2-byte length.
 *      10 - Undefined (or 8 byte length).
 *      11 - 4-byte length.
 */
#define DR7_L0_BIT              0x1     /* Local breakpoint enable for DR0 */
#define DR7_G0_BIT              0x2     /* Global breakpoint enable for DR0 */
#define DR7_L1_BIT              0x4     /* Local breakpoint enable for DR1 */
#define DR7_G1_BIT              0x8     /* Global breakpoint enable for DR1 */
#define DR7_L2_BIT              0x10    /* Local breakpoint enable for DR2 */
#define DR7_G2_BIT              0x20    /* Global breakpoint enable for DR2 */
#define DR7_L3_BIT              0x40    /* Local breakpoint enable for DR3 */
#define DR7_G3_BIT              0x80    /* Global breakpoint enable for DR3 */
#define DR7_LE_BIT              0x100   /* Local exact breakpoint enable */
#define DR7_GE_BIT              0x200   /* Global exact breakpoint enable */
#define DR7_FIXED               0x400   /* Bit 10 is always set */
#define DR7_RTM_BIT             0x800   /* Restricted transactional memory flag */
#define DR7_GD_BIT              0x2000  /* General detect enable flag */
#define DR7_VOLATILE            0xFFFF2BFF

#define DR7_RW0_BITS            0x30000    /* R/W0 breakpoint condition field */
#define DR7_RW0_EX              (0u << 16) /* R/W0 break on instruction execution only. */
#define DR7_RW0_DATA_W          (1u << 16) /* R/W0 break on data writes only. */
#define DR7_RW0_IO_RW           (2u << 16) /* R/W0 break on I/O reads or writes. */
#define DR7_RW0_DATA_RW         (3u << 16) /* R/W0 break on data reads or writes but not instruction fetches. */

#define DR7_LEN0_BITS           0xC0000    /* LEN0 breakpoint size/alignment field */
#define DR7_LEN0_1_BYTE         (0u << 18) /* LEN0 1-byte length/alignment. */
#define DR7_LEN0_2_BYTE         (1u << 18) /* LEN0 2-byte length/alignment. */
#define DR7_LEN0_8_BYTE         (2u << 18) /* LEN0 8-byte length/alignment. */
#define DR7_LEN0_4_BYTE         (3u << 18) /* LEN0 4-byte length/alignment. */

#define DR7_RW1_BITS            0x300000   /* R/W1 breakpoint condition field */
#define DR7_RW1_EX              (0u << 20) /* R/W1 break on instruction execution only. */
#define DR7_RW1_DATA_W          (1u << 20) /* R/W1 break on data writes only. */
#define DR7_RW1_IO_RW           (2u << 20) /* R/W1 break on I/O reads or writes. */
#define DR7_RW1_DATA_RW         (3u << 20) /* R/W1 break on data reads or writes but not instruction fetches. */

#define DR7_LEN1_BITS           0xC00000   /* LEN1 breakpoint size/alignment field */
#define DR7_LEN1_1_BYTE         (0u << 22) /* LEN1 1-byte length/alignment. */
#define DR7_LEN1_2_BYTE         (1u << 22) /* LEN1 2-byte length/alignment. */
#define DR7_LEN1_8_BYTE         (2u << 22) /* LEN1 8-byte length/alignment. */
#define DR7_LEN1_4_BYTE         (3u << 22) /* LEN1 4-byte length/alignment. */

#define DR7_RW2_BITS            0x3000000  /* R/W2 breakpoint condition field */
#define DR7_RW2_EX              (0u << 24) /* R/W2 break on instruction execution only. */
#define DR7_RW2_DATA_W          (1u << 24) /* R/W2 break on data writes only. */
#define DR7_RW2_IO_RW           (2u << 24) /* R/W2 break on I/O reads or writes. */
#define DR7_RW2_DATA_RW         (3u << 24) /* R/W2 break on data reads or writes but not instruction fetches. */

#define DR7_LEN2_BITS           0xC000000  /* LEN2 breakpoint size/alignment field */
#define DR7_LEN2_1_BYTE         (0u << 26) /* LEN2 1-byte length/alignment. */
#define DR7_LEN2_2_BYTE         (1u << 26) /* LEN2 2-byte length/alignment. */
#define DR7_LEN2_8_BYTE         (2u << 26) /* LEN2 8-byte length/alignment. */
#define DR7_LEN2_4_BYTE         (3u << 26) /* LEN2 4-byte length/alignment. */

#define DR7_RW3_BITS            0x30000000 /* R/W3 breakpoint condition field */
#define DR7_RW3_EX              (0u << 28) /* R/W3 break on instruction execution only. */
#define DR7_RW3_DATA_W          (1u << 28) /* R/W3 break on data writes only. */
#define DR7_RW3_IO_RW           (2u << 28) /* R/W3 break on I/O reads or writes. */
#define DR7_RW3_DATA_RW         (3u << 28) /* R/W3 break on data reads or writes but not instruction fetches. */

#define DR7_LEN3_BITS           0xC0000000 /* LEN3 breakpoint size/alignment field */
#define DR7_LEN3_1_BYTE         (0u << 30) /* LEN3 1-byte length/alignment. */
#define DR7_LEN3_2_BYTE         (1u << 30) /* LEN3 2-byte length/alignment. */
#define DR7_LEN3_8_BYTE         (2u << 30) /* LEN3 8-byte length/alignment. */
#define DR7_LEN3_4_BYTE         (3u << 30) /* LEN3 4-byte length/alignment. */

#endif /* _ARCH_X86_DRX_ */
