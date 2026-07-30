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
 * Privileged register access definitions.
 */

#ifndef _SYS_PRIVREGS_H
#define	_SYS_PRIVREGS_H

#include <sys/controlregs.h>
#include <sys/regset.h>

#ifdef	__cplusplus
extern "C" {
#endif

#if !defined(_ASM)

/*
 * Determine if a trap frame came from userland (EL0).
 * EL0 has mode bits [3:0] == 0x0 in SPSR.
 */
#define	USERMODE(pstate)	(((pstate) & 0xf) == 0)

/*
 * Convenience: check if regs are from user mode.
 */
#define	ISUSER(rp)		USERMODE((rp)->r_pstate)

/*
 * Interrupt enable/disable via DAIF bits.
 * On AArch64, interrupts are masked by setting DAIF.I.
 */
static inline void
arm64_intr_disable(void)
{
	__asm__ __volatile__("msr daifset, #2" ::: "memory");
}

static inline void
arm64_intr_enable(void)
{
	__asm__ __volatile__("msr daifclr, #2" ::: "memory");
}

static inline uint64_t
arm64_intr_save(void)
{
	uint64_t daif;
	__asm__ __volatile__("mrs %0, daif" : "=r" (daif));
	__asm__ __volatile__("msr daifset, #2" ::: "memory");
	return (daif);
}

static inline void
arm64_intr_restore(uint64_t daif)
{
	__asm__ __volatile__("msr daif, %0" :: "r" (daif) : "memory");
}

/*
 * Get current exception level.
 */
static inline uint64_t
arm64_current_el(void)
{
	return ((read_sysreg(CurrentEL) >> 2) & 0x3);
}

/*
 * Cache maintenance operations.
 */
static inline void
arm64_ic_ialluis(void)
{
	__asm__ __volatile__("ic ialluis" ::: "memory");
}

static inline void
arm64_dc_civac(void *addr)
{
	__asm__ __volatile__("dc civac, %0" :: "r" (addr) : "memory");
}

static inline void
arm64_ic_ivau(void *addr)
{
	__asm__ __volatile__("ic ivau, %0" :: "r" (addr) : "memory");
}

#endif /* !_ASM */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_PRIVREGS_H */
