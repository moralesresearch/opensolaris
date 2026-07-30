/* Copyright 2026 OpenSolaris AArch64 Contributors. */

#ifndef _ASM_THREAD_H
#define	_ASM_THREAD_H

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(__lint) && defined(__GNUC__)

struct _kthread;

/*
 * TPIDR_EL1 is reserved for the kernel's current-thread pointer.  Unlike
 * TPIDR_EL0, user code cannot observe or modify this register.
 */
extern __inline__ __attribute__((__gnu_inline__)) struct _kthread *
threadp(void)
{
	struct _kthread *thread;

	__asm__ __volatile__("mrs %0, tpidr_el1" : "=r" (thread));
	return (thread);
}

#endif /* !__lint && __GNUC__ */

#ifdef __cplusplus
}
#endif

#endif /* _ASM_THREAD_H */
