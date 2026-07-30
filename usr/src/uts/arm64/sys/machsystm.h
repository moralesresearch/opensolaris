/* Copyright 2026 OpenSolaris AArch64 Contributors. */

#ifndef _SYS_MACHSYSTM_H
#define	_SYS_MACHSYSTM_H

/* Machine interfaces consumed by otherwise common kernel code. */

#include <sys/machparam.h>
#include <sys/thread.h>
#include <sys/cpuvar.h>
#include <sys/privregs.h>
#include <sys/systm.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _KERNEL

extern void mach_cpu_idle(void);
extern void mach_cpu_halt(char *);
extern int mach_cpu_start(cpu_t *, void *);
extern int mach_cpuid_start(processorid_t, void *);
extern int mach_cpu_create_device_node(cpu_t *, dev_info_t **);

extern void send_dirint(int, int);
extern void siron(void);
extern void sir_on(int);
extern void return_instr(void);

struct panic_trap_info {
	struct regs *trap_regs;
	uint_t trap_type;
	caddr_t trap_addr;
};

extern int use_mp;
extern struct cpu cpus[];
extern struct cpu *cpu[];

struct memlist;
extern void memlist_add(uint64_t, uint64_t, struct memlist *,
    struct memlist **);

#endif /* _KERNEL */

#ifdef __cplusplus
}
#endif

#endif /* _SYS_MACHSYSTM_H */
