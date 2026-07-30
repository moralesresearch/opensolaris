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
 * AArch64 stack definitions.
 */

#ifndef _SYS_STACK_H
#define	_SYS_STACK_H

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * AArch64 AAPCS64 requires 16-byte stack alignment.
 */
#define	STACK_ALIGN	16
#define	STACK_ENTRY_ALIGN	16
#define	SA(x)			(((x) + (STACK_ALIGN - 1)) & ~(STACK_ALIGN - 1))
#define	STACK_RESERVE		0

/*
 * Minimum stack frame: x29 (FP) + x30 (LR) = 16 bytes.
 */
#define	MINFRAME	16

/*
 * Default thread stack sizes (same as x86 for now).
 */
#ifndef DEFAULTSTKSZ
#define	DEFAULTSTKSZ	(2 * 4096)
#endif
#define	T_STACK_SIZE	DEFAULTSTKSZ

/*
 * Stack bias: AArch64 does not use a stack bias (unlike SPARC v9).
 */
#define	STACK_BIAS	0

#if defined(_KERNEL) && !defined(_ASM)
#if defined(DEBUG)
#define	ASSERT_STACK_ALIGNED() do { \
	uint64_t __stack_probe; \
	ASSERT((((uintptr_t)&__stack_probe) & (STACK_ALIGN - 1)) == 0); \
} while (0)
#else
#define	ASSERT_STACK_ALIGNED()
#endif

struct regs;
extern void traceregs(struct regs *);
extern void traceback(caddr_t);
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_STACK_H */
