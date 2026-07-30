/* Copyright 2026 OpenSolaris AArch64 Contributors. */

#include <sys/types.h>
#include <sys/thread.h>
#include <sys/cpuvar.h>
#include <sys/swtch.h>
#include <sys/machlock.h>

#define	THREAD_TEST_STACK_SIZE	4096

static kthread_t test_thread;
static uint8_t test_stack[THREAD_TEST_STACK_SIZE] __attribute__((aligned(16)));
static volatile int thread_stage;
static kthread_t resume_thread;
static kthread_t idle_thread;
static uint8_t resume_stack[THREAD_TEST_STACK_SIZE]
    __attribute__((aligned(16)));
static uint8_t idle_stack[THREAD_TEST_STACK_SIZE]
    __attribute__((aligned(16)));
static volatile int resume_stage;

extern void resume(kthread_t *);
extern int getpil(void);
extern int splhigh(void);

static void __attribute__((noreturn, noinline))
test_thread_entry(void)
{
	if (curthread != &test_thread || CPU->cpu_thread != &test_thread) {
		thread_stage = -1;
		arm64_thread_switch(&test_thread, &t0);
	}
	thread_stage = 1;
	arm64_thread_switch(&test_thread, &t0);
	if (curthread != &test_thread || CPU->cpu_thread != &test_thread) {
		thread_stage = -1;
		arm64_thread_switch(&test_thread, &t0);
	}
	thread_stage = 2;
	arm64_thread_switch(&test_thread, &t0);
	for (;;)
		__asm__ __volatile__("wfi");
}

int
arm64_thread_selftest(void)
{
	uint64_t stack_top =
	    (uint64_t)(uintptr_t)&test_stack[THREAD_TEST_STACK_SIZE];

	thread_stage = 0;
	if (curthread != &t0 || CPU->cpu_thread != &t0)
		return (-1);
	test_thread.t_cpu = CPU;
	test_thread.t_pc = (long)(uintptr_t)test_thread_entry;
	test_thread.t_sp = (long)(stack_top & ~0xfULL);
	arm64_thread_switch(&t0, &test_thread);
	if (thread_stage != 1 || curthread != &t0 || CPU->cpu_thread != &t0)
		return (-1);
	arm64_thread_switch(&t0, &test_thread);
	return (thread_stage == 2 && curthread == &t0 &&
	    CPU->cpu_thread == &t0 ? 0 : -1);
}

static void __attribute__((noreturn, noinline))
resume_thread_entry(void)
{
	cpu_t *cp = CPU;

	if (curthread != &resume_thread || cp->cpu_thread != &resume_thread ||
	    cp->cpu_lwp != resume_thread.t_lwp ||
	    resume_thread.t_cpu != cp || !LOCK_HELD(&resume_thread.t_lock) ||
	    LOCK_HELD(&t0.t_lock) || getpil() != 0) {
		resume_stage = -1;
		for (;;)
			__asm__ __volatile__("wfi");
	}

	resume_stage = 1;
	cp->cpu_dispthread = &t0;
	(void) splhigh();
	resume(&t0);
	for (;;)
		__asm__ __volatile__("wfi");
}

int
arm64_resume_selftest(void)
{
	cpu_t *cp = CPU;
	kthread_t *saved_idle = cp->cpu_idle_thread;
	uint64_t resume_top =
	    (uint64_t)(uintptr_t)&resume_stack[THREAD_TEST_STACK_SIZE];
	uint64_t idle_top =
	    (uint64_t)(uintptr_t)&idle_stack[THREAD_TEST_STACK_SIZE];

	resume_stage = 0;
	idle_thread.t_cpu = cp;
	idle_thread.t_sp = (long)(idle_top & ~0xfULL);
	resume_thread.t_cpu = NULL;
	resume_thread.t_lwp = (klwp_t *)(uintptr_t)0x12340000;
	resume_thread.t_pc = (long)(uintptr_t)resume_thread_entry;
	resume_thread.t_sp = (long)(resume_top & ~0xfULL);
	LOCK_INIT_CLEAR(&resume_thread.t_lock);
	if (!LOCK_HELD(&t0.t_lock))
		return (-1);

	cp->cpu_idle_thread = &idle_thread;
	cp->cpu_dispthread = &resume_thread;
	(void) splhigh();
	resume(&resume_thread);
	cp->cpu_idle_thread = saved_idle;

	if (resume_stage != 1 || curthread != &t0 ||
	    cp->cpu_thread != &t0 || cp->cpu_lwp != t0.t_lwp ||
	    !LOCK_HELD(&t0.t_lock) || LOCK_HELD(&resume_thread.t_lock) ||
	    getpil() != 0)
		return (-1);
	return (0);
}
