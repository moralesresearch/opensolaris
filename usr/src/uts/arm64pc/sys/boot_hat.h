/* Copyright 2026 OpenSolaris AArch64 Contributors. */

#ifndef _SYS_ARM64PC_BOOT_HAT_H
#define	_SYS_ARM64PC_BOOT_HAT_H

#include <sys/types.h>
#include <sys/fdt.h>

extern int arm64_boot_hat_init(const struct arm64_platform *);
#define	ARM64_HAT_WRITE	0x01
#define	ARM64_HAT_EXEC		0x02
#define	ARM64_HAT_USER		0x04
#define	ARM64_HAT_DEVICE	0x08

struct arm64_boot_as {
	uint64_t aba_root;
	uint16_t aba_asid;
};

extern int arm64_boot_hat_map(uint64_t, uint64_t, uint_t);
extern int arm64_boot_hat_unmap(uint64_t);
extern int arm64_boot_hat_lookup(uint64_t, uint64_t *);
extern int arm64_boot_hat_as_create(struct arm64_boot_as *, uint16_t);
extern int arm64_boot_hat_as_map(struct arm64_boot_as *, uint64_t,
    uint64_t, uint_t);
extern int arm64_boot_hat_as_unmap(struct arm64_boot_as *, uint64_t);
extern int arm64_boot_hat_as_lookup(struct arm64_boot_as *, uint64_t,
    uint64_t *);
extern uint64_t arm64_boot_hat_as_switch(struct arm64_boot_as *);
extern void arm64_boot_hat_as_restore(uint64_t);
extern void arm64_boot_hat_tlb_asid(uint16_t);
extern uint64_t arm64_boot_phys_to_kva(uint64_t);
extern uint64_t arm64_boot_hat_root(void);
extern uint64_t arm64_boot_hat_table_pages(void);
extern void arm64_boot_high_probe(void);

#endif /* _SYS_ARM64PC_BOOT_HAT_H */
