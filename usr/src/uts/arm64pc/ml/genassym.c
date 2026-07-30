/* Copyright 2026 OpenSolaris AArch64 Contributors. */

#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/thread.h>
#include <sys/cpuvar.h>

#define	ASSYM(name, value) \
	__asm__ __volatile__("// GENASSYM #define " #name " %c0" :: \
	    "i" (value))

void
arm64_genassym(void)
{
	ASSYM(T_LINK, offsetof(kthread_t, t_link));
	ASSYM(T_STK, offsetof(kthread_t, t_stk));
	ASSYM(T_STARTPC, offsetof(kthread_t, t_startpc));
	ASSYM(T_PCB, offsetof(kthread_t, t_pcb));
	ASSYM(T_CTX, offsetof(kthread_t, t_ctx));
	ASSYM(T_LOCK, offsetof(kthread_t, t_lock));
	ASSYM(T_CPU, offsetof(kthread_t, t_cpu));
	ASSYM(T_LWP, offsetof(kthread_t, t_lwp));
	ASSYM(T_PROCP, offsetof(kthread_t, t_procp));

	ASSYM(CPU_SELF, offsetof(cpu_t, cpu_self));
	ASSYM(CPU_THREAD, offsetof(cpu_t, cpu_thread));
	ASSYM(CPU_IDLE_THREAD, offsetof(cpu_t, cpu_idle_thread));
	ASSYM(CPU_LWP, offsetof(cpu_t, cpu_lwp));
	ASSYM(CPU_DISPTHREAD, offsetof(cpu_t, cpu_dispthread));
	ASSYM(CPU_THREAD_LOCK, offsetof(cpu_t, cpu_thread_lock));
	ASSYM(CPU_BASE_SPL, offsetof(cpu_t, cpu_base_spl));
	ASSYM(CPU_M, offsetof(cpu_t, cpu_m));
}
