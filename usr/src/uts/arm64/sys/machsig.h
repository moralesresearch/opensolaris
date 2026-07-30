/* Copyright 2026 OpenSolaris AArch64 Contributors. */

#ifndef _SYS_MACHSIG_H
#define	_SYS_MACHSIG_H

#include <sys/feature_tests.h>

#if !defined(_POSIX_C_SOURCE) || defined(_XPG4_2) || defined(__EXTENSIONS__)

#if !defined(_XPG4_2) || defined(__EXTENSIONS__)
#include <vm/faultcode.h>
#endif

#define	ILL_ILLOPC	1
#define	ILL_ILLOPN	2
#define	ILL_ILLADR	3
#define	ILL_ILLTRP	4
#define	ILL_PRVOPC	5
#define	ILL_PRVREG	6
#define	ILL_COPROC	7
#define	ILL_BADSTK	8
#define	NSIGILL		8

#define	EMT_CPCOVF	1
#define	NSIGEMT		1

#define	FPE_INTDIV	1
#define	FPE_INTOVF	2
#define	FPE_FLTDIV	3
#define	FPE_FLTOVF	4
#define	FPE_FLTUND	5
#define	FPE_FLTRES	6
#define	FPE_FLTINV	7
#define	FPE_FLTSUB	8
#define	FPE_FLTDEN	9
#define	NSIGFPE		9

#define	SEGV_MAPERR	1
#define	SEGV_ACCERR	2
#define	NSIGSEGV	2

#define	BUS_ADRALN	1
#define	BUS_ADRERR	2
#ifndef BUS_OBJERR
#define	BUS_OBJERR	3
#endif
#define	NSIGBUS		3

#endif

#endif /* _SYS_MACHSIG_H */
