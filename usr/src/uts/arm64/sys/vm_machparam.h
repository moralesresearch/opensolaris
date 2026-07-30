/* Copyright 2026 OpenSolaris AArch64 Contributors. */

#ifndef _SYS_VM_MACHPARAM_H
#define	_SYS_VM_MACHPARAM_H

#ifdef __cplusplus
extern "C" {
#endif

/* Machine-dependent VM policy constants consumed by the common kernel. */
#define	USRTEXT		0x10000UL
#define	MAXSSIZ		(32ULL * 1024ULL * 1024ULL * 1024ULL)
#define	DFLSSIZ		(8ULL * 1024ULL * 1024ULL)
#define	DSIZE_LIMIT	(USERLIMIT - USRTEXT)
#define	SSIZE_LIMIT	(USRSTACK)
#define	SYSPTSIZE	((64 * 1024 * 1024) / MMU_PAGESIZE)
#define	E_SYSPTSIZE	((32 * 1024 * 1024) / MMU_PAGESIZE)
#define	SEGMAPDEFAULT	(64 * 1024 * 1024)
#define	MAXSLP		20
#define	SAFERSS		3
#define	DISKRPM		60
#define	MAXHANDSPREADPAGES	((64 * 1024 * 1024) / PAGESIZE)
#define	PGTHRESH	(280 * 1024)

#ifdef __cplusplus
}
#endif

#endif /* _SYS_VM_MACHPARAM_H */
