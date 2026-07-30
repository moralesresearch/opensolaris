/* Copyright 2026 OpenSolaris AArch64 Contributors. */

#ifndef _SYS_ARM64_CONTEXT_H
#define	_SYS_ARM64_CONTEXT_H

#include <sys/machtypes.h>

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(_ASM)
extern int setjmp(label_t *) __attribute__((returns_twice));
extern void longjmp(label_t *) __attribute__((noreturn));
extern void arm64_context_switch(label_t *, const label_t *);
#endif

#ifdef __cplusplus
}
#endif

#endif /* _SYS_ARM64_CONTEXT_H */
