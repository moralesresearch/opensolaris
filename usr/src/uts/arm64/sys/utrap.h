/* Copyright 2026 OpenSolaris AArch64 Contributors. */

#ifndef _SYS_UTRAP_H
#define	_SYS_UTRAP_H

#if !defined(_ASM)
typedef int utrap_entry_t;
typedef void *utrap_handler_t;

#define	UTH_NOCHANGE		((utrap_handler_t)(-1))
#define	UTRAP_UTH_NOCHANGE	UTH_NOCHANGE
#endif

#endif /* _SYS_UTRAP_H */
