/* Copyright 2026 OpenSolaris AArch64 Contributors. */

#ifndef _SYS_ARM64PC_INTR_H
#define	_SYS_ARM64PC_INTR_H

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define	ARM64_INTR_MAX_INTID	1020
#define	ARM64_INTR_UNCLAIMED	0
#define	ARM64_INTR_CLAIMED	1

typedef uint_t (*arm64_intr_func_t)(void *, void *);

/* Caller-owned storage permits registration before kmem is initialized. */
struct arm64_intr_handler {
	struct arm64_intr_handler *aih_next;
	arm64_intr_func_t aih_func;
	void *aih_arg1;
	void *aih_arg2;
	uint_t aih_intid;
	uint_t aih_pil;
};

extern int arm64_intr_register(uint_t, uint_t, arm64_intr_func_t, void *,
    void *, struct arm64_intr_handler *);
extern int arm64_intr_unregister(struct arm64_intr_handler *);
extern uint_t arm64_intr_dispatch(uint_t);
extern void arm64_intr_bootstrap_init(void);
extern int arm64_intr_selftest(void);
extern volatile uint64_t arm64_intr_unclaimed;

#ifdef __cplusplus
}
#endif

#endif /* _SYS_ARM64PC_INTR_H */
