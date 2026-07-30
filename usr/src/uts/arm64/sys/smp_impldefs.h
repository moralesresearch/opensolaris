/* Copyright 2026 OpenSolaris AArch64 Contributors. */

#ifndef _SYS_SMP_IMPLDEFS_H
#define	_SYS_SMP_IMPLDEFS_H

#include <sys/types.h>
#include <sys/avintr.h>

#ifdef __cplusplus
extern "C" {
#endif

struct av_softinfo;

/* Machine hooks used by the common autovector implementation. */
extern int (*slvltovect)(int);
extern int (*addspl)(int, int, int, int);
extern int (*delspl)(int, int, int, int);
extern void (*setsoftint)(int, struct av_softinfo *);
extern void (*kdisetsoftint)(int, struct av_softinfo *);
extern void av_set_softint_pending(int, struct av_softinfo *);
extern void kdi_av_set_softint_pending(int, struct av_softinfo *);

extern struct av_head autovect[];

#ifdef __cplusplus
}
#endif

#endif /* _SYS_SMP_IMPLDEFS_H */
