/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 *
 * CDDL HEADER END
 */

/*
 * Copyright 2026 OpenSolaris AArch64 Contributors.
 * AArch64 trap type definitions.
 */

#ifndef _SYS_TRAP_H
#define	_SYS_TRAP_H

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * AArch64 exception types.
 * These map to the vector table entry groupings.
 */
#define	T_SYNC_EL1T	0	/* Synchronous, current EL, SP_EL0 */
#define	T_IRQ_EL1T	1	/* IRQ, current EL, SP_EL0 */
#define	T_FIQ_EL1T	2	/* FIQ, current EL, SP_EL0 */
#define	T_SERROR_EL1T	3	/* SError, current EL, SP_EL0 */

#define	T_SYNC_EL1H	4	/* Synchronous, current EL, SP_ELx */
#define	T_IRQ_EL1H	5	/* IRQ, current EL, SP_ELx */
#define	T_FIQ_EL1H	6	/* FIQ, current EL, SP_ELx */
#define	T_SERROR_EL1H	7	/* SError, current EL, SP_ELx */

#define	T_SYNC_EL0_64	8	/* Synchronous, lower EL, AArch64 */
#define	T_IRQ_EL0_64	9	/* IRQ, lower EL, AArch64 */
#define	T_FIQ_EL0_64	10	/* FIQ, lower EL, AArch64 */
#define	T_SERROR_EL0_64	11	/* SError, lower EL, AArch64 */

#define	T_SYNC_EL0_32	12	/* Synchronous, lower EL, AArch32 */
#define	T_IRQ_EL0_32	13	/* IRQ, lower EL, AArch32 */
#define	T_FIQ_EL0_32	14	/* FIQ, lower EL, AArch32 */
#define	T_SERROR_EL0_32	15	/* SError, lower EL, AArch32 */

/*
 * Software-defined trap types (for trap() dispatch).
 * These are NOT hardware exception classes; they are used internally.
 */
#define	T_SVC		0x100	/* System call (SVC #0) */
#define	T_PGFLT		0x101	/* Page fault (translation/permission) */
#define	T_ALIGNMENT	0x102	/* Alignment fault */
#define	T_BRKPT		0x103	/* Software breakpoint (BRK) */
#define	T_HW_WATCHPOINT	0x104	/* Hardware watchpoint */
#define	T_FPFAULT	0x105	/* FP/NEON exception */
#define	T_ILLOP		0x106	/* Illegal operation */
#define	T_EXTERR	0x107	/* External abort */

/*
 * Trap frame size (number of 8-byte slots saved).
 * 31 GP regs + SP + PC + PSTATE + ESR + FAR = 36 slots = 288 bytes
 * Padded to 16-byte alignment = 288 bytes (already aligned).
 */
#define	TRAP_FRAME_SIZE	288

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_TRAP_H */
