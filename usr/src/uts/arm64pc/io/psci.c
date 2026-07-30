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
 * ARM PSCI (Power State Coordination Interface) driver.
 * Used for SMP CPU startup, system reset, and power management.
 * On QEMU virt, PSCI uses HVC calls.
 */

#include <sys/types.h>
#include <sys/psci.h>

/* QEMU virt advertises the SMC conduit when virtualization is enabled. */
int psci_conduit = PSCI_CONDUIT_SMC;

/*
 * Issue a PSCI call using the configured conduit.
 */
int64_t
psci_call(uint64_t func, uint64_t arg0, uint64_t arg1, uint64_t arg2)
{
	if (psci_conduit == PSCI_CONDUIT_HVC)
		return (psci_hvc(func, arg0, arg1, arg2));
	else
		return (psci_smc(func, arg0, arg1, arg2));
}

/*
 * Start a secondary CPU.
 * target_cpu: MPIDR affinity value of the target CPU.
 * entry_point: physical address where the CPU starts executing.
 */
int
psci_cpu_on(uint64_t target_cpu, uint64_t entry_point)
{
	return (psci_cpu_on_context(target_cpu, entry_point, 0));
}

int
psci_cpu_on_context(uint64_t target_cpu, uint64_t entry_point,
    uint64_t context)
{
	return ((int)psci_call(PSCI_CPU_ON_64, target_cpu, entry_point,
	    context));
}

/*
 * Reset the system.
 */
void
psci_system_reset(void)
{
	(void) psci_call(PSCI_SYSTEM_RESET, 0, 0, 0);
	/* Should not return */
	for (;;)
		__asm__ __volatile__("wfi");
}

/*
 * Power off the system.
 */
void
psci_system_off(void)
{
	(void) psci_call(PSCI_SYSTEM_OFF, 0, 0, 0);
	/* Should not return */
	for (;;)
		__asm__ __volatile__("wfi");
}

/*
 * Query PSCI version.
 * Returns version as (major << 16 | minor).
 */
uint32_t
psci_version(void)
{
	return ((uint32_t)psci_call(PSCI_VERSION, 0, 0, 0));
}

/*
 * Query CPU state.
 */
int
psci_affinity_info(uint64_t target_affinity)
{
	return ((int)psci_call(PSCI_AFFINITY_INFO_64, target_affinity, 0, 0));
}
