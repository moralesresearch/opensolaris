/* Copyright 2026 OpenSolaris AArch64 Contributors. */

#ifndef _SYS_SPL_H
#define	_SYS_SPL_H

/* GIC priority levels and OpenSolaris PILs are represented identically. */
#define	pritospl(n)	(n)
#define	ipltospl(n)	(n)
#define	spltoipl(n)	(n)
#define	spltopri(n)	(n)

#define	SPL8	15
#define	SPL7	13
#define	SPL5	12
#define	SPLTTY	SPL5
#define	SPL3	6

#endif /* _SYS_SPL_H */
