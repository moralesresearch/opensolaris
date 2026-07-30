/* Copyright 2026 OpenSolaris AArch64 Contributors. */

#ifndef _SYS_ARM64PC_FDT_H
#define	_SYS_ARM64PC_FDT_H

#include <sys/types.h>

#define	ARM64_FDT_MAX_CPUS	16

struct arm64_platform {
	uint64_t ram_base;
	uint64_t ram_size;
	uint64_t uart_base;
	uint64_t uart_size;
	uint64_t gicd_base;
	uint64_t gicd_size;
	uint64_t gicr_base;
	uint64_t gicr_size;
	uint64_t cpu_mpidr[ARM64_FDT_MAX_CPUS];
	uint32_t cpu_count;
	uint32_t psci_conduit;
};

extern int arm64_fdt_validate(uint64_t);
extern uint32_t arm64_fdt_size(uint64_t);
extern int arm64_fdt_psci_conduit(uint64_t);
extern int arm64_fdt_parse(uint64_t, struct arm64_platform *);

#endif /* _SYS_ARM64PC_FDT_H */
