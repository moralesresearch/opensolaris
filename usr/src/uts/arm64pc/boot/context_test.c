/* Copyright 2026 OpenSolaris AArch64 Contributors. */

#include <sys/types.h>
#include <sys/context.h>

#define	FIBER_STACK_SIZE	4096

static label_t bootstrap_context;
static label_t fiber_context;
static uint8_t fiber_stack[FIBER_STACK_SIZE] __attribute__((aligned(16)));
static volatile int fiber_stage;

static void __attribute__((noreturn, noinline))
context_fiber(void)
{
	fiber_stage = 1;
	arm64_context_switch(&fiber_context, &bootstrap_context);
	fiber_stage = 2;
	arm64_context_switch(&fiber_context, &bootstrap_context);
	for (;;)
		__asm__ __volatile__("wfi");
}

/*
 * This exercises the non-local return primitive used by on_fault(), panic
 * recovery, and the common kernel's saved thread context.
 */
int __attribute__((noinline))
arm64_context_selftest(void)
{
	label_t context;
	volatile int pass = 0;

	if (setjmp(&context) == 0) {
		pass = 1;
		longjmp(&context);
	}
	return (pass == 1 ? 0 : -1);
}

int
arm64_switch_selftest(void)
{
	uint64_t stack_top = (uint64_t)(uintptr_t)&fiber_stack[FIBER_STACK_SIZE];

	fiber_stage = 0;
	fiber_context.val[REG_LABEL_PC] = (long)(uintptr_t)context_fiber;
	fiber_context.val[REG_LABEL_SP] = (long)(stack_top & ~0xfULL);
	arm64_context_switch(&bootstrap_context, &fiber_context);
	if (fiber_stage != 1)
		return (-1);
	arm64_context_switch(&bootstrap_context, &fiber_context);
	return (fiber_stage == 2 ? 0 : -1);
}
