/* Copyright 2026 OpenSolaris AArch64 Contributors. */

#ifndef _SYS_ARM64PC_SMP_H
#define	_SYS_ARM64PC_SMP_H

#include <sys/types.h>
#include <sys/fdt.h>
#include <sys/x_call.h>

extern volatile uint64_t arm64_online_cpus;
extern volatile uint64_t arm64_ipi_count;
extern volatile uint64_t arm64_last_irq;
extern int arm64_smp_start(const struct arm64_platform *);
extern int arm64_smp_test_ipi(const struct arm64_platform *);
extern int arm64_smp_test_xcall(const struct arm64_platform *);
extern int arm64_smp_test_cpu_abi(const struct arm64_platform *);
extern int arm64_xcall_sync(const struct arm64_platform *, xc_func_t,
    xc_arg_t, xc_arg_t, xc_arg_t, int *);
extern uint32_t arm64_smp_current_cpuid(void);
extern void arm64_smp_ipi_handler(void);
extern void mp_startup(uint64_t);

#endif /* _SYS_ARM64PC_SMP_H */
