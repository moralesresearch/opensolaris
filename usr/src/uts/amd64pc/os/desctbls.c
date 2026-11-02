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
 * GDT, IDT and TSS setup for the amd64pc bootstrap.
 *
 * The descriptor layout is the real OpenSolaris amd64 one from
 * uts/intel/sys/segments.h: KCS_SEL/KDS_SEL for the kernel, the
 * U32CS_SEL, UDS_SEL, UCS_SEL triple that makes SYSRET land on the user
 * code and stack selectors, KTSS_SEL for the per-CPU TSS, and IST1 for
 * double faults.  set_usegd(), set_syssegd() and set_gatesegd() follow
 * the native (non-__xpv) bodies in uts/intel/ia32/os/desctbls.c and
 * keep their segments.h prototypes.
 */

#include <sys/types.h>
#include <sys/segments.h>
#include <sys/tss.h>
#include <sys/trap.h>
#include <sys/controlregs.h>
#include <sys/psw.h>
#include <sys/amd64pc.h>

#define	DFSTKSZ		8192

extern char amd64pc_trap_stubs[];
extern void amd64pc_syscall_entry(void);

static user_desc_t gdt[AMD64PC_MAX_CPUS][NGDT] __attribute__((aligned(16)));
static gate_desc_t idt[NIDT] __attribute__((aligned(16)));
static struct tss ktss[AMD64PC_MAX_CPUS] __attribute__((aligned(16)));
static uint8_t dftss_stack[AMD64PC_MAX_CPUS][DFSTKSZ]
    __attribute__((aligned(16)));
static int idt_ready;

void
set_usegd(user_desc_t *dp, uint_t lmode, void *base, size_t size,
    uint_t type, uint_t dpl, uint_t gran, uint_t defopsz)
{
	if (lmode == SDP_LONG)
		dp->usd_def32 = 0;
	else
		dp->usd_def32 = defopsz;
	dp->usd_long = lmode;
	dp->usd_type = type;
	dp->usd_dpl = dpl;
	dp->usd_p = 1;
	dp->usd_gran = gran;

	dp->usd_lobase = (uintptr_t)base;
	dp->usd_midbase = (uintptr_t)base >> 16;
	dp->usd_hibase = (uintptr_t)base >> (16 + 8);
	dp->usd_lolimit = size;
	dp->usd_hilimit = (uintptr_t)size >> 16;
}

void
set_syssegd(system_desc_t *dp, void *base, size_t size, uint_t type,
    uint_t dpl)
{
	dp->ssd_lolimit = size;
	dp->ssd_hilimit = (uintptr_t)size >> 16;

	dp->ssd_lobase = (uintptr_t)base;
	dp->ssd_midbase = (uintptr_t)base >> 16;
	dp->ssd_hibase = (uintptr_t)base >> (16 + 8);
	dp->ssd_hi64base = (uintptr_t)base >> (16 + 8 + 8);

	dp->ssd_type = type;
	dp->ssd_zero1 = 0;
	dp->ssd_zero2 = 0;
	dp->ssd_dpl = dpl;
	dp->ssd_p = 1;
	dp->ssd_gran = 0;
}

void
set_gatesegd(gate_desc_t *dp, void (*func)(void), selector_t sel,
    uint_t type, uint_t dpl, uint_t vector)
{
	dp->sgd_looffset = (uintptr_t)func;
	dp->sgd_hioffset = (uintptr_t)func >> 16;
	dp->sgd_hi64offset = (uintptr_t)func >> (16 + 16);

	dp->sgd_selector = (uint16_t)sel;

	/* Double faults run on their own IST stack, as in the real kernel. */
	if (vector == T_DBLFLT)
		dp->sgd_ist = 1;
	else
		dp->sgd_ist = 0;

	dp->sgd_type = type;
	dp->sgd_dpl = dpl;
	dp->sgd_p = 1;
}

static void
idt_init(void)
{
	uint_t vec;

	for (vec = 0; vec < NIDT; vec++) {
		void (*stub)(void) = (void (*)(void))
		    (amd64pc_trap_stubs + 16 * vec);

		/*
		 * #BP and #OF may be raised from user mode (int3/into), as
		 * on the real kernel; everything else is kernel-only.
		 */
		set_gatesegd(&idt[vec], stub, KCS_SEL, SDT_SYSIGT,
		    (vec == T_BPTFLT || vec == T_OVFLW) ? SEL_UPL : SEL_KPL,
		    vec);
	}
	idt_ready = 1;
}

void
desctbl_init(uint32_t cpu, uint64_t rsp0)
{
	user_desc_t *g = gdt[cpu];
	struct tss *t = &ktss[cpu];

	/* Boot-time selectors kept for parity with the real layout. */
	set_usegd(&g[GDT_B32DATA], SDP_SHORT, 0, -1, SDT_MEMRWA, SEL_KPL,
	    SDP_PAGES, SDP_OP32);
	set_usegd(&g[GDT_B32CODE], SDP_SHORT, 0, -1, SDT_MEMERA, SEL_KPL,
	    SDP_PAGES, SDP_OP32);
	set_usegd(&g[GDT_B64CODE], SDP_LONG, 0, 0, SDT_MEMERA, SEL_KPL,
	    SDP_PAGES, SDP_OP32);

	set_usegd(&g[GDT_KCODE], SDP_LONG, 0, 0, SDT_MEMERA, SEL_KPL,
	    SDP_PAGES, SDP_OP32);
	set_usegd(&g[GDT_KDATA], SDP_SHORT, 0, -1, SDT_MEMRWA, SEL_KPL,
	    SDP_PAGES, SDP_OP32);
	set_usegd(&g[GDT_U32CODE], SDP_SHORT, 0, -1, SDT_MEMERA, SEL_UPL,
	    SDP_PAGES, SDP_OP32);
	set_usegd(&g[GDT_UDATA], SDP_SHORT, 0, -1, SDT_MEMRWA, SEL_UPL,
	    SDP_PAGES, SDP_OP32);
	set_usegd(&g[GDT_UCODE], SDP_LONG, 0, 0, SDT_MEMERA, SEL_UPL,
	    SDP_PAGES, SDP_OP32);

	t->tss_rsp0 = rsp0;
	t->tss_ist1 = (uint64_t)(uintptr_t)&dftss_stack[cpu][DFSTKSZ];
	t->tss_bitmapbase = sizeof (*t);	/* no I/O permission bitmap */
	set_syssegd((system_desc_t *)&g[GDT_KTSS], t, sizeof (*t) - 1,
	    SDT_SYSTSS, SEL_KPL);

	if (!idt_ready)
		idt_init();
}

void
desctbl_set_rsp0(uint32_t cpu, uint64_t rsp0)
{
	ktss[cpu].tss_rsp0 = rsp0;
}

void
desctbl_load(uint32_t cpu)
{
	desctbr_t gdtr, idtr;

	gdtr.dtr_limit = sizeof (gdt[cpu]) - 1;
	gdtr.dtr_base = (uint64_t)(uintptr_t)gdt[cpu];
	idtr.dtr_limit = sizeof (idt) - 1;
	idtr.dtr_base = (uint64_t)(uintptr_t)idt;

	__asm__ __volatile__("lgdt %0" :: "m"(gdtr));
	__asm__ __volatile__("lidt %0" :: "m"(idtr));

	/* Reload %cs through a far return, then the data selectors. */
	__asm__ __volatile__(
	    "pushq %0\n\t"
	    "leaq 1f(%%rip), %%rax\n\t"
	    "pushq %%rax\n\t"
	    "lretq\n"
	    "1:\n\t"
	    "movw %w1, %%ds\n\t"
	    "movw %w1, %%es\n\t"
	    "movw %w1, %%ss\n\t"
	    :: "i"((uint64_t)KCS_SEL), "r"((uint32_t)KDS_SEL) : "rax",
	    "memory");
	__asm__ __volatile__("movw %w0, %%fs; movw %w0, %%gs" ::
	    "r"((uint32_t)KFS_SEL));
	__asm__ __volatile__("ltr %w0" :: "r"((uint32_t)KTSS_SEL));

	/*
	 * SYSCALL/SYSRET.  STAR[47:32] = KCS_SEL (kernel %ss = KCS + 8 =
	 * KDS_SEL); STAR[63:48] = U32CS_SEL, so SYSRET to 64-bit mode loads
	 * %cs = U32CS + 16 = UCS_SEL and %ss = U32CS + 8 = UDS_SEL.
	 */
	pc_wrmsr(MSR_AMD_EFER, pc_rdmsr(MSR_AMD_EFER) | AMD_EFER_SCE);
	pc_wrmsr(MSR_AMD_STAR, ((uint64_t)(U32CS_SEL & ~3) << 48) |
	    ((uint64_t)KCS_SEL << 32));
	pc_wrmsr(MSR_AMD_LSTAR, (uint64_t)(uintptr_t)amd64pc_syscall_entry);
	pc_wrmsr(MSR_AMD_SFMASK, PS_IE | PS_T | PS_D | PS_ACHK | PS_NT);
}
