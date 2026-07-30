/* Copyright 2026 OpenSolaris AArch64 Contributors. */

#ifndef _SYS_FRAME_H
#define	_SYS_FRAME_H

#include <sys/regset.h>

#ifdef __cplusplus
extern "C" {
#endif

/* AAPCS64 frame record, pointed to by x29 when frame pointers are enabled. */
struct frame {
	greg_t fr_savfp;
	greg_t fr_savpc;
};

#ifdef __cplusplus
}
#endif

#endif /* _SYS_FRAME_H */
