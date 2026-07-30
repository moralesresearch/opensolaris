/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */

/*
 * Copyright 2026 OpenSolaris AArch64 Contributors.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef _SYS_ELF_AARCH64_H
#define	_SYS_ELF_AARCH64_H

#ifdef	__cplusplus
extern "C" {
#endif

/* AArch64 ELF relocation numbers from the AAELF64 ABI. */
#define	R_AARCH64_NONE			0
#define	R_AARCH64_ABS64			0x101
#define	R_AARCH64_ABS32			0x102
#define	R_AARCH64_ABS16			0x103
#define	R_AARCH64_PREL64		0x104
#define	R_AARCH64_PREL32		0x105
#define	R_AARCH64_PREL16		0x106
#define	R_AARCH64_MOVW_UABS_G0		0x107
#define	R_AARCH64_MOVW_UABS_G0_NC	0x108
#define	R_AARCH64_MOVW_UABS_G1		0x109
#define	R_AARCH64_MOVW_UABS_G1_NC	0x10a
#define	R_AARCH64_MOVW_UABS_G2		0x10b
#define	R_AARCH64_MOVW_UABS_G2_NC	0x10c
#define	R_AARCH64_MOVW_UABS_G3		0x10d
#define	R_AARCH64_LD_PREL_LO19		0x111
#define	R_AARCH64_ADR_PREL_LO21		0x112
#define	R_AARCH64_ADR_PREL_PG_HI21	0x113
#define	R_AARCH64_ADR_PREL_PG_HI21_NC	0x114
#define	R_AARCH64_ADD_ABS_LO12_NC	0x115
#define	R_AARCH64_LDST8_ABS_LO12_NC	0x116
#define	R_AARCH64_TSTBR14		0x117
#define	R_AARCH64_CONDBR19		0x118
#define	R_AARCH64_JUMP26		0x11a
#define	R_AARCH64_CALL26		0x11b
#define	R_AARCH64_LDST16_ABS_LO12_NC	0x11c
#define	R_AARCH64_LDST32_ABS_LO12_NC	0x11d
#define	R_AARCH64_LDST64_ABS_LO12_NC	0x11e
#define	R_AARCH64_LDST128_ABS_LO12_NC	0x12b
#define	R_AARCH64_COPY			0x400
#define	R_AARCH64_GLOB_DAT		0x401
#define	R_AARCH64_JUMP_SLOT		0x402
#define	R_AARCH64_RELATIVE		0x403
#define	R_AARCH64_TLS_DTPMOD64		0x404
#define	R_AARCH64_TLS_DTPREL64		0x405
#define	R_AARCH64_TLS_TPREL64		0x406
#define	R_AARCH64_TLSDESC		0x407
#define	R_AARCH64_IRELATIVE		0x408

#define	ELF_AARCH64_MAXPGSZ		0x10000

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_ELF_AARCH64_H */
