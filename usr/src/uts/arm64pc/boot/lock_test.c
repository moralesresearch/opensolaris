/* Copyright 2026 OpenSolaris AArch64 Contributors. */

#include <sys/types.h>
#include <sys/machlock.h>
#include <sys/fdt.h>
#include <sys/smp.h>

#define	LOCK_TEST_LOOPS	1000

extern struct arm64_platform arm64_platform;

static lock_t test_lock;
static uint64_t test_counter;

static int
arm64_lock_worker(xc_arg_t a1, xc_arg_t a2, xc_arg_t a3)
{
	unsigned int i;

	(void) a1;
	(void) a2;
	(void) a3;
	for (i = 0; i != LOCK_TEST_LOOPS; i++) {
		lock_set(&test_lock);
		test_counter++;
		lock_clear(&test_lock);
	}
	return (0);
}

int
arm64_lock_selftest(void)
{
	test_lock = 0;
	test_counter = 0;
	if (arm64_xcall_sync(&arm64_platform, arm64_lock_worker,
	    0, 0, 0, 0) != 0)
		return (-1);
	(void) arm64_lock_worker(0, 0, 0);
	return (test_counter ==
	    (uint64_t)arm64_platform.cpu_count * LOCK_TEST_LOOPS ? 0 : -1);
}
