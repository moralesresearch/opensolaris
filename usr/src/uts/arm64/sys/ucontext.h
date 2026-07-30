/* Copyright 2026 OpenSolaris AArch64 Contributors. */

#ifndef _SYS_UCONTEXT_H
#define	_SYS_UCONTEXT_H

#include <sys/feature_tests.h>
#include <sys/types.h>
#include <sys/regset.h>
#if !defined(_XPG4_2) || defined(__EXTENSIONS__)
#include <sys/signal.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_XPG4_2) && !defined(__EXTENSIONS__)
#ifndef _SIGSET_T
#define	_SIGSET_T
typedef struct { unsigned long __sigbits[4]; } sigset_t;
#endif
#ifndef _STACK_T
#define	_STACK_T
typedef struct {
	void *ss_sp;
	size_t ss_size;
	int ss_flags;
} stack_t;
#endif
#endif

#if !defined(_XPG4_2) || defined(__EXTENSIONS__)
typedef struct ucontext ucontext_t;
struct ucontext {
#else
typedef struct __ucontext ucontext_t;
struct __ucontext {
#endif
	unsigned long uc_flags;
	ucontext_t *uc_link;
	sigset_t uc_sigmask;
	stack_t uc_stack;
	mcontext_t uc_mcontext;
	long uc_filler[4];
};

#if !defined(_XPG4_2) || defined(__EXTENSIONS__)
#define	GETCONTEXT	0
#define	SETCONTEXT	1
#define	GETUSTACK	2
#define	SETUSTACK	3

#define	UC_SIGMASK	0x01
#define	UC_STACK	0x02
#define	UC_CPU		0x04
#define	UC_FPU		0x08
#define	UC_MCONTEXT	(UC_CPU | UC_FPU)
#define	UC_ALL		(UC_SIGMASK | UC_STACK | UC_MCONTEXT)
#endif

#ifdef _KERNEL
extern void savecontext(ucontext_t *, k_sigset_t);
extern void restorecontext(ucontext_t *);
#endif

#ifdef __cplusplus
}
#endif

#endif /* _SYS_UCONTEXT_H */
