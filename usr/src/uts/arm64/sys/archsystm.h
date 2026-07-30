/* Copyright 2026 OpenSolaris AArch64 Contributors. */

#ifndef _SYS_ARCHSYSTM_H
#define	_SYS_ARCHSYSTM_H

#include <sys/types.h>
#include <sys/regset.h>
#include <vm/seg_enum.h>
#include <vm/page.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_KERNEL) && !defined(_ASM)
struct _klwp;
struct regs;

extern greg_t getfp(void);
extern int getpil(void);
extern void setpil(uint_t);
extern void bind_hwcap(void);

extern void setgregs(struct _klwp *, gregset_t);
extern void getgregs(struct _klwp *, gregset_t);
extern void setfpregs(struct _klwp *, fpregset_t *);
extern void getfpregs(struct _klwp *, fpregset_t *);

extern uint_t cpu_hwcap_flags;
extern uint_t cpu_freq;
extern uint64_t cpu_freq_hz;

extern void switch_sp_and_call(void *, void (*)(uint_t, uint_t), uint_t,
    uint_t);
extern hrtime_t (*gethrtimef)(void);
extern hrtime_t (*gethrtimeunscaledf)(void);
extern void (*scalehrtimef)(hrtime_t *);
extern uint64_t (*unscalehrtimef)(hrtime_t);
extern void (*gethrestimef)(timestruc_t *);

extern void av_dispatch_softvect(uint_t);
extern void av_dispatch_autovect(uint_t);
extern caddr_t arm64devmap(pfn_t, pgcnt_t, uint_t);
extern page_t *page_numtopp_alloc(pfn_t);

#define	cpr_dprintf	prom_printf

#endif /* _KERNEL && !_ASM */

#ifdef __cplusplus
}
#endif

#endif /* _SYS_ARCHSYSTM_H */
