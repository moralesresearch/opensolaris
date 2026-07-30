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
 * CDDL HEADER END
 */

/*
 * Copyright 2026 OpenSolaris AArch64 Contributors.
 * AArch64 register set definitions.
 */

#ifndef _SYS_REGSET_H
#define	_SYS_REGSET_H

#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * AArch64 general-purpose register indices.
 */
#define	REG_X0		0
#define	REG_X1		1
#define	REG_X2		2
#define	REG_X3		3
#define	REG_X4		4
#define	REG_X5		5
#define	REG_X6		6
#define	REG_X7		7
#define	REG_X8		8
#define	REG_X9		9
#define	REG_X10		10
#define	REG_X11		11
#define	REG_X12		12
#define	REG_X13		13
#define	REG_X14		14
#define	REG_X15		15
#define	REG_X16		16
#define	REG_X17		17
#define	REG_X18		18
#define	REG_X19		19
#define	REG_X20		20
#define	REG_X21		21
#define	REG_X22		22
#define	REG_X23		23
#define	REG_X24		24
#define	REG_X25		25
#define	REG_X26		26
#define	REG_X27		27
#define	REG_X28		28
#define	REG_X29		29	/* Frame pointer (FP) */
#define	REG_X30		30	/* Link register (LR) */
#define	REG_SP		31
#define	REG_PC		32
#define	REG_PSTATE	33
#define	_NGREG		34

#if !defined(_ASM)

typedef	uint64_t	greg_t;
typedef	greg_t		gregset_t[_NGREG];

/*
 * AArch64 trap frame — saved on exception entry.
 * Layout matches the order pushed by the exception vector stubs.
 */
struct regs {
	greg_t	r_x0;
	greg_t	r_x1;
	greg_t	r_x2;
	greg_t	r_x3;
	greg_t	r_x4;
	greg_t	r_x5;
	greg_t	r_x6;
	greg_t	r_x7;
	greg_t	r_x8;
	greg_t	r_x9;
	greg_t	r_x10;
	greg_t	r_x11;
	greg_t	r_x12;
	greg_t	r_x13;
	greg_t	r_x14;
	greg_t	r_x15;
	greg_t	r_x16;
	greg_t	r_x17;
	greg_t	r_x18;
	greg_t	r_x19;
	greg_t	r_x20;
	greg_t	r_x21;
	greg_t	r_x22;
	greg_t	r_x23;
	greg_t	r_x24;
	greg_t	r_x25;
	greg_t	r_x26;
	greg_t	r_x27;
	greg_t	r_x28;
	greg_t	r_x29;		/* Frame pointer */
	greg_t	r_x30;		/* Link register */
	greg_t	r_sp;		/* Stack pointer (SP_EL0 or SP_ELx) */
	greg_t	r_pc;		/* Program counter (ELR_ELx) */
	greg_t	r_pstate;	/* Saved PSTATE (SPSR_ELx) */
	greg_t	r_esr;		/* Exception Syndrome Register */
	greg_t	r_far;		/* Fault Address Register */
};

/*
 * Floating-point / NEON register state.
 * AArch64 has 32 128-bit SIMD/FP registers (V0-V31).
 */
typedef struct {
	uint64_t	fpu_q[32][2];	/* V0-V31, 128-bit each */
	uint32_t	fpu_fpcr;	/* FP Control Register */
	uint32_t	fpu_fpsr;	/* FP Status Register */
} fpregset_t;

/*
 * Structure for signal delivery and /proc.
 */
typedef struct {
	gregset_t	gregs;
	fpregset_t	fpregs;
} mcontext_t;

#endif	/* !_ASM */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_REGSET_H */
