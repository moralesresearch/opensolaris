/* Copyright 2026 OpenSolaris AArch64 Contributors. */

/*
 * Early AArch64 interrupt registration and dispatch.  The public shape
 * follows the common autovector contract: vectors may be shared, every
 * installed handler runs, and the combined result records whether the
 * interrupt was claimed.  Caller-owned nodes avoid a bootstrap allocator.
 */

#include <sys/types.h>
#include <sys/controlregs.h>
#include <sys/privregs.h>
#include <sys/machlock.h>
#include <sys/intr.h>
#include <sys/platform.h>
#include <sys/smp.h>

extern void arch_timer_stop(void);
extern volatile uint64_t arm64_timer_ticks;

static struct arm64_intr_handler *arm64_intr_heads[ARM64_INTR_MAX_INTID];
static struct arm64_intr_handler arm64_xcall_handler;
static struct arm64_intr_handler arm64_timer_handler;

/*
 * Local DAIF masking alone only serialises this CPU against its own interrupt
 * handlers; registration also runs on secondary CPUs during their bring-up, so
 * the shared handler chains need a real lock.  lock_set()/lock_clear() are the
 * AArch64 acquire/release spin primitives from ml/lock_prim.s.  Interrupts stay
 * masked across the critical section so an interrupt cannot re-enter the lock
 * on the holding CPU.
 */
static lock_t arm64_intr_lock;

volatile uint64_t arm64_intr_unclaimed;

int
arm64_intr_register(uint_t intid, uint_t pil, arm64_intr_func_t func,
    void *arg1, void *arg2, struct arm64_intr_handler *handler)
{
	struct arm64_intr_handler *cur;
	uint64_t daif;

	if (intid >= ARM64_INTR_MAX_INTID || pil == 0 || pil > 15 ||
	    func == 0 || handler == 0)
		return (-1);

	daif = arm64_intr_save();
	lock_set(&arm64_intr_lock);
	for (cur = arm64_intr_heads[intid]; cur != 0; cur = cur->aih_next) {
		if (cur == handler || (cur->aih_func == func &&
		    cur->aih_arg1 == arg1 && cur->aih_arg2 == arg2)) {
			lock_clear(&arm64_intr_lock);
			arm64_intr_restore(daif);
			return (-1);
		}
	}
	handler->aih_func = func;
	handler->aih_arg1 = arg1;
	handler->aih_arg2 = arg2;
	handler->aih_intid = intid;
	handler->aih_pil = pil;
	handler->aih_next = arm64_intr_heads[intid];
	dmb(ish);
	arm64_intr_heads[intid] = handler;
	dsb(ish);
	lock_clear(&arm64_intr_lock);
	arm64_intr_restore(daif);
	return (0);
}

int
arm64_intr_unregister(struct arm64_intr_handler *handler)
{
	struct arm64_intr_handler **link;
	uint64_t daif;

	if (handler == 0 || handler->aih_intid >= ARM64_INTR_MAX_INTID)
		return (-1);
	daif = arm64_intr_save();
	lock_set(&arm64_intr_lock);
	link = &arm64_intr_heads[handler->aih_intid];
	while (*link != 0 && *link != handler)
		link = &(*link)->aih_next;
	if (*link == 0) {
		lock_clear(&arm64_intr_lock);
		arm64_intr_restore(daif);
		return (-1);
	}
	*link = handler->aih_next;
	dsb(ish);
	handler->aih_next = 0;
	handler->aih_func = 0;
	lock_clear(&arm64_intr_lock);
	arm64_intr_restore(daif);
	return (0);
}

uint_t
arm64_intr_dispatch(uint_t intid)
{
	struct arm64_intr_handler *handler;
	uint_t claimed = ARM64_INTR_UNCLAIMED;

	if (intid >= ARM64_INTR_MAX_INTID) {
		(void) arm64_atomic_add_64(&arm64_intr_unclaimed, 1);
		return (ARM64_INTR_UNCLAIMED);
	}
	for (handler = arm64_intr_heads[intid]; handler != 0;
	    handler = handler->aih_next)
		claimed |= handler->aih_func(handler->aih_arg1,
		    handler->aih_arg2);
	if (claimed == ARM64_INTR_UNCLAIMED)
		(void) arm64_atomic_add_64(&arm64_intr_unclaimed, 1);
	return (claimed);
}

static uint_t
arm64_xcall_intr(void *arg1, void *arg2)
{
	(void) arg1;
	(void) arg2;
	arm64_smp_ipi_handler();
	return (ARM64_INTR_CLAIMED);
}

static uint_t
arm64_timer_intr(void *arg1, void *arg2)
{
	(void) arg1;
	(void) arg2;
	arch_timer_stop();
	arm64_timer_ticks++;
	return (ARM64_INTR_CLAIMED);
}

void
arm64_intr_bootstrap_init(void)
{
	(void) arm64_intr_register(ARM64PC_XCALL_IRQ, 13, arm64_xcall_intr,
	    0, 0, &arm64_xcall_handler);
	(void) arm64_intr_register(ARM64PC_TIMER_IRQ, 14, arm64_timer_intr,
	    0, 0, &arm64_timer_handler);
}

static uint_t
arm64_intr_test_handler(void *arg1, void *arg2)
{
	uint_t *count = arg1;

	*count += (uint_t)(uintptr_t)arg2;
	return (ARM64_INTR_CLAIMED);
}

int
arm64_intr_selftest(void)
{
	struct arm64_intr_handler first = { 0 };
	struct arm64_intr_handler second = { 0 };
	uint_t count = 0;
	uint64_t unclaimed;

	if (arm64_intr_register(1000, 5, arm64_intr_test_handler, &count,
	    (void *)(uintptr_t)1, &first) != 0 ||
	    arm64_intr_register(1000, 5, arm64_intr_test_handler, &count,
	    (void *)(uintptr_t)2, &second) != 0)
		return (-1);
	if (arm64_intr_dispatch(1000) != ARM64_INTR_CLAIMED || count != 3)
		return (-1);
	if (arm64_intr_unregister(&second) != 0 ||
	    arm64_intr_unregister(&first) != 0)
		return (-1);
	unclaimed = arm64_intr_unclaimed;
	if (arm64_intr_dispatch(1000) != ARM64_INTR_UNCLAIMED ||
	    arm64_intr_unclaimed != unclaimed + 1)
		return (-1);
	return (0);
}
