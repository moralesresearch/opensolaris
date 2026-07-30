/* Copyright 2026 OpenSolaris AArch64 Contributors. */

#ifndef _SYS_MACHCPUVAR_H
#define	_SYS_MACHCPUVAR_H

#include <sys/types.h>
#include <sys/x_call.h>
#include <sys/avintr.h>

#if !defined(_ASM)
struct hat;
struct hat_cpu_info;
struct _kthread;

typedef void *cpu_pri_lev_t;

struct machcpu {
	/* Generic cross-call mailbox, delivered through a GICv3 SGI. */
	struct xc_msg *xc_msgbox;
	struct xc_msg *xc_free;
	xc_data_t xc_data;
	uint32_t xc_wait_cnt;
	volatile uint32_t xc_work_cnt;

	int mcpu_nodeid;
	int mcpu_pri;
	cpu_pri_lev_t mcpu_pri_data;
	struct hat *mcpu_current_hat;
	struct hat_cpu_info *mcpu_hat_info;
	volatile ulong_t mcpu_tlb_info;

	struct softint mcpu_softinfo;
	uint64_t pil_high_start[HIGH_LEVELS];
	uint64_t intrstat[PIL_MAX + 1][2];

	uint64_t mcpu_mpidr;
	uint64_t mcpu_gicr_base;
	volatile uint32_t mcpu_ipi_pending;
	void (*mcpu_idle_cpu)(void);
	struct _kthread *startup_thread;
};

typedef struct machcpu machcpu_t;

#define	cpu_nodeid		cpu_m.mcpu_nodeid
#define	cpu_pri			cpu_m.mcpu_pri
#define	cpu_pri_data		cpu_m.mcpu_pri_data
#define	cpu_current_hat		cpu_m.mcpu_current_hat
#define	cpu_hat_info		cpu_m.mcpu_hat_info
#define	cpu_softinfo		cpu_m.mcpu_softinfo
#define	cpu_startup_thread	cpu_m.startup_thread
#define	NINTR_THREADS		(LOCK_LEVEL - 1)
#endif

#endif /* _SYS_MACHCPUVAR_H */
