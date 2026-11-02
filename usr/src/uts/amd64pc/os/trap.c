/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 *
 * CDDL HEADER END
 */

/*
 * Copyright 2026 OpenSolaris x86_64 Contributors.
 *
 * Trap and interrupt dispatch for the amd64pc bootstrap.  The frame is
 * the real amd64 `struct regs' (uts/intel/amd64/sys/privregs.h); the
 * offsets hard-coded in ml/exception.s are verified against it here.
 */

#include <sys/types.h>
#include <sys/regset.h>
#include <sys/privregs.h>
#include <sys/trap.h>
#include <sys/segments.h>
#include <sys/amd64pc.h>

#define	CHECK_OFF(field, off) \
	_Static_assert(__builtin_offsetof(struct regs, field) == (off), \
	    "ml/exception.s offset for " #field " is stale")

CHECK_OFF(r_savfp, 0);
CHECK_OFF(r_savpc, 8);
CHECK_OFF(r_rdi, 16);
CHECK_OFF(r_rax, 64);
CHECK_OFF(r_r15, 128);
CHECK_OFF(r_ds, 152);
CHECK_OFF(r_gs, 176);
CHECK_OFF(r_trapno, 184);
CHECK_OFF(r_err, 192);
CHECK_OFF(r_rip, 200);
CHECK_OFF(r_cs, 208);
CHECK_OFF(r_rfl, 216);
CHECK_OFF(r_rsp, 224);
CHECK_OFF(r_ss, 232);
_Static_assert(sizeof (struct regs) == 240, "struct regs size changed");

extern char amd64pc_probe_insn[], amd64pc_probe_fault[];
extern int amd64pc_probe_read(uint64_t, uint64_t *);

volatile uint64_t trap_count[256];
static amd64pc_handler_t handlers[256];
static volatile int expect_ud;

/* Set by os/user.c while a ring-3 test program runs. */
void (*trap_user_hook)(struct regs *);

static const char *const trap_names[32] = {
	"#DE divide error", "#DB debug", "NMI", "#BP breakpoint",
	"#OF overflow", "#BR bound", "#UD invalid opcode",
	"#NM device not available", "#DF double fault", "coprocessor overrun",
	"#TS invalid TSS", "#NP segment not present", "#SS stack fault",
	"#GP general protection", "#PF page fault", "reserved",
	"#MF x87 error", "#AC alignment check", "#MC machine check",
	"#XM SIMD error", "#VE virtualization", "#CP control protection",
};

void
trap_set_handler(uint_t vector, amd64pc_handler_t h)
{
	handlers[vector & 0xff] = h;
}

static uint64_t
getcr2(void)
{
	uint64_t v;

	__asm__ __volatile__("movq %%cr2, %0" : "=r"(v));
	return (v);
}

static void
trap_dump(struct regs *rp)
{
	const char *name = (uint64_t)rp->r_trapno < 32 ?
	    trap_names[rp->r_trapno] : 0;

	cons_puts("\namd64pc: unexpected trap ");
	cons_puthex(rp->r_trapno);
	if (name != 0) {
		cons_puts(" (");
		cons_puts(name);
		cons_puts(")");
	}
	cons_puts(" on CPU ");
	cons_putdec(amd64pc_cpuid());
	cons_puts("\n  err ");
	cons_puthex(rp->r_err);
	cons_puts(" rip ");
	cons_puthex(rp->r_rip);
	cons_puts(" cs ");
	cons_puthex(rp->r_cs);
	cons_puts("\n  rsp ");
	cons_puthex(rp->r_rsp);
	cons_puts(" rfl ");
	cons_puthex(rp->r_rfl);
	cons_puts(" cr2 ");
	cons_puthex(getcr2());
	cons_puts("\n  rax ");
	cons_puthex(rp->r_rax);
	cons_puts(" rdi ");
	cons_puthex(rp->r_rdi);
	cons_puts(" rsi ");
	cons_puthex(rp->r_rsi);
	cons_puts("\n");
}

void
amd64pc_trap(struct regs *rp)
{
	uint64_t vec = rp->r_trapno & 0xff;

	__atomic_add_fetch(&trap_count[vec], 1, __ATOMIC_RELAXED);

	if (vec >= 32) {
		if (handlers[vec] != 0)
			handlers[vec](rp);
		if (vec != AMD64PC_VEC_SPURIOUS)
			lapic_eoi();
		return;
	}

	/* Ring-3 faults belong to the user harness, not the kernel. */
	if ((rp->r_cs & 3) == SEL_UPL && trap_user_hook != 0 &&
	    vec != T_BPTFLT) {
		trap_user_hook(rp);
		return;
	}

	switch (vec) {
	case T_BPTFLT:
		/* int3 already advanced %rip past itself. */
		return;
	case T_ILLINST:
		if (expect_ud) {
			expect_ud = 0;
			rp->r_rip += 2;		/* ud2 */
			return;
		}
		break;
	case T_PGFLT:
		/* The bootstrap's on_trap()/t_lofault equivalent. */
		if ((uint64_t)rp->r_rip ==
		    (uint64_t)(uintptr_t)amd64pc_probe_insn) {
			rp->r_rip = (greg_t)(uintptr_t)amd64pc_probe_fault;
			return;
		}
		break;
	default:
		break;
	}
	trap_dump(rp);
	amd64pc_panic("fatal trap");
}

/*
 * Exercise the exception path end to end: a breakpoint, an invalid
 * opcode that is skipped and a kernel page fault that is recovered.
 */
int
trap_selftest(void)
{
	uint64_t bp = trap_count[T_BPTFLT];
	uint64_t ud = trap_count[T_ILLINST];
	uint64_t pf = trap_count[T_PGFLT];
	uint64_t value = 0x5a5a;
	uint64_t unmapped = 0x0000700000000000ULL;
	static volatile uint64_t mapped = 0xfeedfacecafebeefULL;

	__asm__ __volatile__("int3");
	if (trap_count[T_BPTFLT] != bp + 1)
		return (-1);

	expect_ud = 1;
	__asm__ __volatile__("ud2");
	if (trap_count[T_ILLINST] != ud + 1 || expect_ud != 0)
		return (-2);

	if (amd64pc_probe_read(unmapped, &value) != -1 ||
	    trap_count[T_PGFLT] != pf + 1 || getcr2() != unmapped ||
	    value != 0x5a5a)
		return (-3);
	if (amd64pc_probe_read((uint64_t)(uintptr_t)&mapped, &value) != 0 ||
	    value != mapped)
		return (-4);
	return (0);
}
