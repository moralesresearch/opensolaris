/* Copyright 2026 OpenSolaris AArch64 Contributors. */

#ifndef _SYS_DDI_ISA_H
#define	_SYS_DDI_ISA_H

#include <sys/isa_defs.h>
#include <sys/dditypes.h>
#include <sys/ndifm.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _KERNEL
/* AArch64 uses memory-mapped device access; port I/O is not present. */
#define	DDI_ACCATTR_CONFIG_SPACE	0x1
#define	DDI_ACCATTR_CPU_VADDR		0x4
#define	DDI_ACCATTR_DIRECT		0x8

typedef struct ddi_acc_impl {
	ddi_acc_hdl_t ahi_common;
	uint_t ahi_acc_attr;
	uint8_t (*ahi_get8)(struct ddi_acc_impl *, uint8_t *);
	uint16_t (*ahi_get16)(struct ddi_acc_impl *, uint16_t *);
	uint32_t (*ahi_get32)(struct ddi_acc_impl *, uint32_t *);
	uint64_t (*ahi_get64)(struct ddi_acc_impl *, uint64_t *);
	void (*ahi_put8)(struct ddi_acc_impl *, uint8_t *, uint8_t);
	void (*ahi_put16)(struct ddi_acc_impl *, uint16_t *, uint16_t);
	void (*ahi_put32)(struct ddi_acc_impl *, uint32_t *, uint32_t);
	void (*ahi_put64)(struct ddi_acc_impl *, uint64_t *, uint64_t);
	void (*ahi_rep_get8)(struct ddi_acc_impl *, uint8_t *, uint8_t *,
	    size_t, uint_t);
	void (*ahi_rep_get16)(struct ddi_acc_impl *, uint16_t *, uint16_t *,
	    size_t, uint_t);
	void (*ahi_rep_get32)(struct ddi_acc_impl *, uint32_t *, uint32_t *,
	    size_t, uint_t);
	void (*ahi_rep_get64)(struct ddi_acc_impl *, uint64_t *, uint64_t *,
	    size_t, uint_t);
	void (*ahi_rep_put8)(struct ddi_acc_impl *, uint8_t *, uint8_t *,
	    size_t, uint_t);
	void (*ahi_rep_put16)(struct ddi_acc_impl *, uint16_t *, uint16_t *,
	    size_t, uint_t);
	void (*ahi_rep_put32)(struct ddi_acc_impl *, uint32_t *, uint32_t *,
	    size_t, uint_t);
	void (*ahi_rep_put64)(struct ddi_acc_impl *, uint64_t *, uint64_t *,
	    size_t, uint_t);
	int (*ahi_fault_check)(struct ddi_acc_impl *);
	void (*ahi_fault_notify)(struct ddi_acc_impl *);
	uint32_t ahi_fault;
	ndi_err_t *ahi_err;
} ddi_acc_impl_t;
#endif

#ifdef __cplusplus
}
#endif

#endif /* _SYS_DDI_ISA_H */
