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
 * Floating-point / NEON / SVE state management.
 */

#ifndef _SYS_FP_H
#define	_SYS_FP_H

#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * CPACR_EL1 — Architectural Feature Access Control Register
 * Controls EL0/EL1 access to floating-point and SIMD.
 */
#define	CPACR_FPEN_SHIFT	20
#define	CPACR_FPEN_MASK		(0x3UL << CPACR_FPEN_SHIFT)
#define	CPACR_FPEN_TRAP_ALL	(0x0UL << CPACR_FPEN_SHIFT)	/* Trap both */
#define	CPACR_FPEN_TRAP_EL0	(0x1UL << CPACR_FPEN_SHIFT)	/* Trap EL0 only */
#define	CPACR_FPEN_ENABLE	(0x3UL << CPACR_FPEN_SHIFT)	/* No traps */

#define	CPACR_ZEN_SHIFT		16
#define	CPACR_ZEN_MASK		(0x3UL << CPACR_ZEN_SHIFT)
#define	CPACR_ZEN_TRAP_ALL	(0x0UL << CPACR_ZEN_SHIFT)
#define	CPACR_ZEN_ENABLE	(0x3UL << CPACR_ZEN_SHIFT)

#if !defined(_ASM)

/*
 * FP/NEON register state for context switching.
 * 32 × 128-bit vector registers + FPCR + FPSR.
 */
struct fpu_ctx {
	uint64_t	fpu_q[32][2];	/* V0-V31, each 128-bit */
	uint32_t	fpu_fpcr;
	uint32_t	fpu_fpsr;
	uint32_t	fpu_flags;
	uint32_t	fpu_pad;
};

/* fpu_flags values */
#define	FPU_VALID	0x01	/* FPU state is valid */
#define	FPU_EN		0x02	/* FPU enabled for this thread */

extern void fp_save(struct fpu_ctx *);
extern void fp_restore(struct fpu_ctx *);
extern void fp_init(void);

#endif /* !_ASM */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_FP_H */
