/* Copyright 2026 OpenSolaris AArch64 Contributors. */

#ifndef _SYS_ARM64_KOBJ_RELOC_H
#define	_SYS_ARM64_KOBJ_RELOC_H

#include <sys/types.h>

extern int arm64_reloc_apply(uint32_t, void *, uint64_t, int64_t);

#endif /* _SYS_ARM64_KOBJ_RELOC_H */
