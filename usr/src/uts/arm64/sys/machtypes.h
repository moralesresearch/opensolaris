/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE.
 *
 * CDDL HEADER END
 */

/*
 * Copyright 2026 OpenSolaris AArch64 Contributors.
 */

#ifndef _SYS_MACHTYPES_H
#define	_SYS_MACHTYPES_H

#include <sys/feature_tests.h>

#ifdef __cplusplus
extern "C" {
#endif

#if (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)) || \
	defined(__EXTENSIONS__)

/* Saved by setjmp/longjmp and the kernel context-switch paths. */
#define	REG_LABEL_PC	0
#define	REG_LABEL_SP	1
#define	REG_LABEL_X19	2
#define	REG_LABEL_X20	3
#define	REG_LABEL_X21	4
#define	REG_LABEL_X22	5
#define	REG_LABEL_X23	6
#define	REG_LABEL_X24	7
#define	REG_LABEL_X25	8
#define	REG_LABEL_X26	9
#define	REG_LABEL_X27	10
#define	REG_LABEL_X28	11
#define	REG_LABEL_X29	12
#define	REG_LABEL_X30	13
#define	REG_LABEL_MAX	14

typedef struct _label_t { long val[REG_LABEL_MAX]; } label_t;

#endif

typedef unsigned char lock_t;

#ifdef __cplusplus
}
#endif

#endif /* _SYS_MACHTYPES_H */
