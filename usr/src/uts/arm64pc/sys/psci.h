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
 * ARM Power State Coordination Interface (PSCI) definitions.
 * PSCI is the standard interface for CPU power management on ARM64 servers.
 */

#ifndef _SYS_PSCI_H
#define	_SYS_PSCI_H

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * PSCI function IDs (SMC64 convention — 64-bit calling).
 * See ARM DEN0022D (PSCI specification).
 */
#define	PSCI_VERSION		0x84000000
#define	PSCI_CPU_SUSPEND_64	0xC4000001
#define	PSCI_CPU_OFF		0x84000002
#define	PSCI_CPU_ON_64		0xC4000003
#define	PSCI_AFFINITY_INFO_64	0xC4000004
#define	PSCI_MIGRATE_64		0xC4000005
#define	PSCI_MIGRATE_INFO_TYPE	0x84000006
#define	PSCI_SYSTEM_OFF		0x84000008
#define	PSCI_SYSTEM_RESET	0x84000009
#define	PSCI_FEATURES		0x8400000A

/*
 * PSCI return values.
 */
#define	PSCI_SUCCESS		0
#define	PSCI_NOT_SUPPORTED	(-1)
#define	PSCI_INVALID_PARAMS	(-2)
#define	PSCI_DENIED		(-3)
#define	PSCI_ALREADY_ON		(-4)
#define	PSCI_ON_PENDING		(-5)
#define	PSCI_INTERNAL_FAILURE	(-6)
#define	PSCI_NOT_PRESENT	(-7)
#define	PSCI_DISABLED		(-8)
#define	PSCI_INVALID_ADDRESS	(-9)

/*
 * PSCI affinity info return values.
 */
#define	PSCI_AFFINITY_ON	0
#define	PSCI_AFFINITY_OFF	1
#define	PSCI_AFFINITY_ON_PENDING 2

/*
 * PSCI conduit: SMC vs HVC.
 * QEMU virt uses HVC; real hardware typically uses SMC.
 */
#define	PSCI_CONDUIT_SMC	1
#define	PSCI_CONDUIT_HVC	2

#if !defined(_ASM)

#include <sys/types.h>

/*
 * The SMC Calling Convention (ARM DEN 0028) states that x0-x3 carry the
 * results and that x4-x17 are unpredictable on return from an SMC64/HVC64
 * call, so the compiler must not keep live values in them across the
 * conduit instruction.  x0-x3 are already tied to explicit register
 * variables below and therefore must not appear here.
 */
#define	PSCI_SMCCC_CLOBBERS						\
	"x4", "x5", "x6", "x7", "x8", "x9", "x10", "x11",		\
	"x12", "x13", "x14", "x15", "x16", "x17"

/*
 * Issue a PSCI call via SMC (Secure Monitor Call).
 */
static inline int64_t
psci_smc(uint64_t func, uint64_t arg0, uint64_t arg1, uint64_t arg2)
{
	register uint64_t x0 __asm__("x0") = func;
	register uint64_t x1 __asm__("x1") = arg0;
	register uint64_t x2 __asm__("x2") = arg1;
	register uint64_t x3 __asm__("x3") = arg2;

	__asm__ __volatile__(
	    "smc #0"
	    : "+r" (x0)
	    : "r" (x1), "r" (x2), "r" (x3)
	    : "memory", PSCI_SMCCC_CLOBBERS);

	return ((int64_t)x0);
}

/*
 * Issue a PSCI call via HVC (Hypervisor Call).
 */
static inline int64_t
psci_hvc(uint64_t func, uint64_t arg0, uint64_t arg1, uint64_t arg2)
{
	register uint64_t x0 __asm__("x0") = func;
	register uint64_t x1 __asm__("x1") = arg0;
	register uint64_t x2 __asm__("x2") = arg1;
	register uint64_t x3 __asm__("x3") = arg2;

	__asm__ __volatile__(
	    "hvc #0"
	    : "+r" (x0)
	    : "r" (x1), "r" (x2), "r" (x3)
	    : "memory", PSCI_SMCCC_CLOBBERS);

	return ((int64_t)x0);
}

extern int psci_conduit;	/* PSCI_CONDUIT_SMC or PSCI_CONDUIT_HVC */

extern int64_t psci_call(uint64_t func, uint64_t arg0,
    uint64_t arg1, uint64_t arg2);
extern int psci_cpu_on(uint64_t target_cpu, uint64_t entry_point);
extern int psci_cpu_on_context(uint64_t, uint64_t, uint64_t);
extern void psci_system_reset(void);
extern void psci_system_off(void);

#endif /* !_ASM */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_PSCI_H */
