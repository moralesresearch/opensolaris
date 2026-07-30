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
 *
 * ARM Generic Timer driver.
 * Provides high-resolution timekeeping and periodic timer interrupts.
 * Replaces HPET/APIC timer on x86.
 *
 * The ARM Generic Timer provides:
 * - CNTPCT_EL0: Physical counter (always incrementing)
 * - CNTP_CTL_EL0: Physical timer control
 * - CNTP_CVAL_EL0: Physical timer comparator value
 * - CNTFRQ_EL0: Counter frequency
 *
 * On QEMU virt, the timer interrupt is PPI 30 (INTID = 16 + 14 = 30).
 */

#include <sys/types.h>
#include <sys/controlregs.h>

/* Timer PPI interrupt IDs */
#define	TIMER_PPI_PHYS		30	/* Physical timer: PPI 14 → INTID 30 */
#define	TIMER_PPI_VIRT		27	/* Virtual timer: PPI 11 → INTID 27 */

/* CNTP_CTL_EL0 bits */
#define	CNTP_CTL_ENABLE		(1ULL << 0)
#define	CNTP_CTL_IMASK		(1ULL << 1)	/* Interrupt mask */
#define	CNTP_CTL_ISTATUS	(1ULL << 2)	/* Interrupt pending */

static uint64_t timer_freq;	/* Counter frequency in Hz */

/*
 * Read the counter frequency.
 */
static inline uint64_t
read_cntfrq(void)
{
	return (read_sysreg(cntfrq_el0));
}

/*
 * Read the physical counter value.
 */
static inline uint64_t
read_cntpct(void)
{
	uint64_t val;

	isb();
	val = read_sysreg(cntpct_el0);
	return (val);
}

/*
 * Set the timer comparator value (fire at this count).
 */
static inline void
write_cntp_cval(uint64_t val)
{
	write_sysreg(cntp_cval_el0, val);
	isb();
}

/*
 * Set the timer control register.
 */
static inline void
write_cntp_ctl(uint64_t val)
{
	write_sysreg(cntp_ctl_el0, val);
	isb();
}

/*
 * Initialize the generic timer.
 */
void
arch_timer_init(void)
{
	timer_freq = read_cntfrq();

	/* Disable timer initially */
	write_cntp_ctl(0);
}

/*
 * Start periodic timer interrupts.
 * interval_us: interval in microseconds.
 */
void
arch_timer_start(uint64_t interval_us)
{
	uint64_t ticks;
	uint64_t now;

	ticks = (timer_freq * interval_us) / 1000000ULL;
	now = read_cntpct();

	/* Set comparator to fire after 'ticks' */
	write_cntp_cval(now + ticks);

	/* Enable timer, unmask interrupt */
	write_cntp_ctl(CNTP_CTL_ENABLE);
}

/*
 * Acknowledge timer interrupt and schedule next tick.
 * Called from the timer interrupt handler.
 */
void
arch_timer_ack(uint64_t interval_us)
{
	uint64_t ticks;
	uint64_t cval;

	ticks = (timer_freq * interval_us) / 1000000ULL;

	/* Read current comparator and advance by one interval */
	__asm__ __volatile__("mrs %0, cntp_cval_el0" : "=r"(cval));
	write_cntp_cval(cval + ticks);
}

/*
 * Stop the timer.
 */
void
arch_timer_stop(void)
{
	write_cntp_ctl(CNTP_CTL_IMASK);	/* Mask and disable */
}

/*
 * Get current time in nanoseconds (for gethrtime()).
 */
uint64_t
arch_timer_gethrtime(void)
{
	uint64_t count = read_cntpct();
	uint64_t sec, rem;

	if (timer_freq == 0)
		return (0);

	/*
	 * ns = count * 1000000000 / freq, computed without overflowing 64
	 * bits.  A direct count * 1000000000 wraps after roughly 295 seconds
	 * of uptime at QEMU's 62.5 MHz counter, so split the counter into
	 * whole seconds and a sub-second remainder first.  The remainder is
	 * always less than freq, so rem * 1000000000 cannot overflow for any
	 * counter frequency below ~18.4 GHz.
	 */
	sec = count / timer_freq;
	rem = count % timer_freq;
	return (sec * 1000000000ULL + (rem * 1000000000ULL) / timer_freq);
}

/*
 * Get counter frequency.
 */
uint64_t
arch_timer_get_freq(void)
{
	return (timer_freq);
}
