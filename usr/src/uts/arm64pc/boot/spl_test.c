/* Copyright 2026 OpenSolaris AArch64 Contributors. */

#include <sys/types.h>
#include <sys/privregs.h>
#include <sys/platform.h>
#include <sys/systm.h>
#include <sys/archsystm.h>

extern void arch_timer_start(uint64_t);
extern uint64_t arch_timer_gethrtime(void);
extern void gicv3_irq_enable(uint32_t);
extern volatile uint64_t arm64_timer_ticks;

int
arm64_spl_selftest(void)
{
	uint64_t deadline;
	int old;

	splx(0);
	if (getpil() != 0)
		return (-1);
	old = splr(5);
	if (old != 0 || getpil() != 5 || splr(3) != 5 || getpil() != 5)
		return (-1);

	arm64_timer_ticks = 0;
	gicv3_irq_enable(ARM64PC_TIMER_IRQ);
	arch_timer_start(1000);
	(void) splhigh();
	arm64_intr_enable();
	deadline = arch_timer_gethrtime() + 5000000ULL;
	while (arch_timer_gethrtime() < deadline)
		__asm__ __volatile__("yield");
	arm64_intr_disable();
	if (arm64_timer_ticks != 0)
		return (-1);

	splx(0);
	arm64_intr_enable();
	while (arm64_timer_ticks == 0)
		__asm__ __volatile__("wfi");
	arm64_intr_disable();
	return (getpil() == 0 ? 0 : -1);
}
