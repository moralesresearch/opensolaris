/* Copyright 2026 OpenSolaris AArch64 Contributors. */

#ifndef _SYS_ARM64_SWTCH_H
#define	_SYS_ARM64_SWTCH_H

struct _kthread;
extern void arm64_thread_switch(struct _kthread *, struct _kthread *);

#endif /* _SYS_ARM64_SWTCH_H */
