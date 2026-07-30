/* Copyright 2026 OpenSolaris AArch64 Contributors. */

#ifndef _SYS_ARM64PC_USERMODE_H
#define	_SYS_ARM64PC_USERMODE_H

#include <sys/types.h>

extern uint64_t arm64_el0_enter(uint64_t, uint64_t);
extern void arm64_el0_resume(void);
extern int arm64_el0_selftest(void);
extern int arm64_el0_fault_selftest(void);

typedef int (*arm64_pagefault_func_t)(uint64_t, uint_t, uint_t);
extern arm64_pagefault_func_t arm64_pagefault_dispatch;

#endif /* _SYS_ARM64PC_USERMODE_H */
