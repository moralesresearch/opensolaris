/* Copyright 2026 OpenSolaris AArch64 Contributors. */

#ifndef _SYS_VMPARAM_H
#define	_SYS_VMPARAM_H

#if (defined(_KERNEL) || defined(_KMEMUSER)) && defined(_MACHDEP)
#include <sys/vm_machparam.h>
#endif

#define	SSIZE		4096
#define	SINCR		4096
#define	USRSTACK	USERLIMIT
#define	USRSTACK32	0xfffff000UL
#define	USRSTACK64_32	USRSTACK32

#if (defined(_KERNEL) || defined(_KMEMUSER)) && !defined(_MACHDEP)
#if defined(_KERNEL) && !defined(_ASM)
extern const unsigned int _diskrpm;
extern const unsigned long _dsize_limit;
extern const unsigned long _ssize_limit;
extern const unsigned long _pgthresh;
extern const unsigned int _maxslp;
extern const unsigned long _maxhandspreadpages;
#endif

#define	DISKRPM		_diskrpm
#define	DSIZE_LIMIT	_dsize_limit
#define	SSIZE_LIMIT	_ssize_limit
#define	PGTHRESH	_pgthresh
#define	MAXSLP		_maxslp
#define	MAXHANDSPREADPAGES _maxhandspreadpages
#endif

#endif /* _SYS_VMPARAM_H */
