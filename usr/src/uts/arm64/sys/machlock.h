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
 * Machine-dependent lock definitions for AArch64.
 */

#ifndef _SYS_MACHLOCK_H
#define	_SYS_MACHLOCK_H

#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifndef _ASM

#define	LOCK_HELD_VALUE		0xff
#define	LOCK_INIT_CLEAR(lp)	(*(lp) = 0)
#define	LOCK_INIT_HELD(lp)	(*(lp) = LOCK_HELD_VALUE)
#define	LOCK_HELD(lp)		(*(volatile lock_t *)(lp) != 0)

typedef lock_t disp_lock_t;

#if defined(_KERNEL)
extern void lock_set(lock_t *);
extern int lock_try(lock_t *);
extern int lock_spin_try(lock_t *);
extern int ulock_try(lock_t *);
extern void lock_clear(lock_t *);
extern void ulock_clear(lock_t *);
extern void lock_set_spl(lock_t *, int, ushort_t *);
extern void lock_clear_splx(lock_t *, int);
#endif

#define	SPIN_LOCK(pl)		((pl) > LOCK_LEVEL)
#define	LOCK_SAMPLE_INTERVAL(i)	(((i) & 0xff) == 0)

#define	PIL_MAX		15
#define	CLOCK_LEVEL	10
#define	LOCK_LEVEL	10
#define	DISP_LEVEL	(LOCK_LEVEL + 1)
#define	HIGH_LEVELS	(PIL_MAX - LOCK_LEVEL)
#define	CPU_INTR_ACTV_HIGH_LEVEL_MASK	0xf800

#define	PTR24_LSB	5
#define	PTR24_MSB	(PTR24_LSB + 24)
#define	PTR24_ALIGN	32
#define	PTR24_BASE	0xe0000000

/*
 * Atomic compare-and-swap.
 * Returns the old value.
 */
static inline uint64_t
arm64_cas64(volatile uint64_t *target, uint64_t cmp, uint64_t new)
{
	uint64_t old;
	uint32_t tmp;

	__asm__ __volatile__(
	    "1:	ldxr	%0, [%2]\n"
	    "	cmp	%0, %3\n"
	    "	b.ne	2f\n"
	    "	stxr	%w1, %4, [%2]\n"
	    "	cbnz	%w1, 1b\n"
	    "2:"
	    : "=&r" (old), "=&r" (tmp)
	    : "r" (target), "r" (cmp), "r" (new)
	    : "memory", "cc");

	return (old);
}

static inline uint32_t
arm64_cas32(volatile uint32_t *target, uint32_t cmp, uint32_t new)
{
	uint32_t old;
	uint32_t tmp;

	__asm__ __volatile__(
	    "1:	ldxr	%w0, [%2]\n"
	    "	cmp	%w0, %w3\n"
	    "	b.ne	2f\n"
	    "	stxr	%w1, %w4, [%2]\n"
	    "	cbnz	%w1, 1b\n"
	    "2:"
	    : "=&r" (old), "=&r" (tmp)
	    : "r" (target), "r" (cmp), "r" (new)
	    : "memory", "cc");

	return (old);
}

/*
 * Atomic operations using exclusives.
 */
static inline uint64_t
arm64_atomic_add_64(volatile uint64_t *target, int64_t delta)
{
	uint64_t old, new;
	uint32_t tmp;

	__asm__ __volatile__(
	    "1:	ldxr	%0, [%3]\n"
	    "	add	%1, %0, %4\n"
	    "	stxr	%w2, %1, [%3]\n"
	    "	cbnz	%w2, 1b\n"
	    : "=&r" (old), "=&r" (new), "=&r" (tmp)
	    : "r" (target), "r" (delta)
	    : "memory", "cc");

	return (old);
}

/*
 * WFE/SEV for spin-lock contention.
 */
#define	SMT_PAUSE()	__asm__ __volatile__("yield" ::: "memory")

#endif /* !_ASM */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_MACHLOCK_H */
