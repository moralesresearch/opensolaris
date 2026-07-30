/* Copyright 2026 OpenSolaris AArch64 Contributors. */

#ifndef _SYS_ARM64PC_BOOTMEM_H
#define	_SYS_ARM64PC_BOOTMEM_H

#include <sys/types.h>
#include <sys/fdt.h>

extern int arm64_bootmem_init(const struct arm64_platform *, uint64_t,
    uint64_t);
extern void *arm64_bootmem_alloc(uint64_t, uint64_t);
extern uint64_t arm64_bootmem_used(void);
extern uint64_t arm64_bootmem_carve_tail(uint64_t, uint64_t);

#endif /* _SYS_ARM64PC_BOOTMEM_H */
