/* Copyright 2026 OpenSolaris AArch64 Contributors. */

/*
 * The initial OpenSolaris CPU and thread objects.  This is the first piece of
 * the common dispatcher ABI instantiated by the AArch64 bootstrap image.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/thread.h>
#include <sys/cpuvar.h>
#include <sys/stack.h>
#include <sys/fdt.h>
#include <sys/machlock.h>

extern char t0stack[];
extern uint64_t ap_stacks[];

struct cpu cpus[ARM64_FDT_MAX_CPUS];
struct cpu *cpu[NCPU] = { &cpus[0] };
cpu_core_t cpu_core[NCPU];
kthread_t t0;
static kthread_t ap_threads[ARM64_FDT_MAX_CPUS];

int arm64_boot_cpu_selftest_id(uint32_t);

void
arm64_boot_cpu_init(void)
{
	cpu_t *cp = &cpus[0];
	uintptr_t stack_top = (uintptr_t)t0stack + 16384;

	/* Do not depend on relocation data surviving a raw-image loader. */
	cpu[0] = cp;
	cp->cpu_id = 0;
	cp->cpu_seqid = 0;
	cp->cpu_flags = CPU_RUNNING | CPU_READY | CPU_EXISTS | CPU_ENABLE;
	cp->cpu_self = cp;
	cp->cpu_thread = &t0;
	cp->cpu_dispthread = &t0;
	cp->cpu_idle_thread = &t0;

	t0.t_stkbase = t0stack;
	t0.t_stk = (caddr_t)((stack_top - MINFRAME) &
	    ~(uintptr_t)(STACK_ALIGN - 1));
	t0.t_schedflag = TS_LOAD | TS_DONT_SWAP;
	t0.t_state = TS_ONPROC;
	LOCK_INIT_HELD(&t0.t_lock);
	t0.t_lockp = &cp->cpu_thread_lock;
	t0.t_cpu = cp;
	t0.t_forw = &t0;
	t0.t_back = &t0;
	t0.t_next = &t0;
	t0.t_prev = &t0;

	__asm__ __volatile__("msr tpidr_el1, %0\n\tisb" :: "r" (&t0) :
	    "memory");
}

int
arm64_boot_cpu_selftest(void)
{
	return (arm64_boot_cpu_selftest_id(0));
}

int
arm64_boot_cpu_init_secondary(uint32_t id)
{
	cpu_t *cp;
	kthread_t *tp;
	uintptr_t stack_top;

	if (id == 0 || id >= ARM64_FDT_MAX_CPUS || ap_stacks[id] == 0)
		return (-1);
	cp = &cpus[id];
	tp = &ap_threads[id];
	stack_top = (uintptr_t)ap_stacks[id];

	cpu[id] = cp;
	cp->cpu_id = (processorid_t)id;
	cp->cpu_seqid = (processorid_t)id;
	cp->cpu_flags = CPU_RUNNING | CPU_READY | CPU_EXISTS | CPU_ENABLE;
	cp->cpu_self = cp;
	cp->cpu_thread = tp;
	cp->cpu_dispthread = tp;
	cp->cpu_idle_thread = tp;

	tp->t_stkbase = (caddr_t)(stack_top - 16384);
	tp->t_stk = (caddr_t)((stack_top - MINFRAME) &
	    ~(uintptr_t)(STACK_ALIGN - 1));
	tp->t_schedflag = TS_LOAD | TS_DONT_SWAP;
	tp->t_state = TS_ONPROC;
	LOCK_INIT_HELD(&tp->t_lock);
	tp->t_lockp = &cp->cpu_thread_lock;
	tp->t_cpu = cp;
	tp->t_forw = tp;
	tp->t_back = tp;
	tp->t_next = tp;
	tp->t_prev = tp;

	__asm__ __volatile__("msr tpidr_el1, %0\n\tisb" :: "r" (tp) :
	    "memory");
	return (0);
}

int
arm64_boot_cpu_selftest_id(uint32_t id)
{
	cpu_t *cp;
	kthread_t *tp;

	if (id >= ARM64_FDT_MAX_CPUS)
		return (-6);
	cp = &cpus[id];
	tp = id == 0 ? &t0 : &ap_threads[id];

	if (threadp() != tp)
		return (-1);
	if (CPU != cp)
		return (-4);
	if (cpu[id] != cp)
		return (-5);
	if (cp->cpu_self != cp || cp->cpu_thread != tp ||
	    cp->cpu_dispthread != tp || cp->cpu_idle_thread != tp)
		return (-2);
	if (tp->t_cpu != cp || tp->t_state != TS_ONPROC ||
	    tp->t_lockp != &cp->cpu_thread_lock)
		return (-3);
	return (0);
}
