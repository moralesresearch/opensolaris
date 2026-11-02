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
 * amd64pc bootstrap main line.  Like arm64pc's arm64_bootstrap_main(),
 * this deliberately stops short of uts/common/os/main.c: it proves the
 * x86_64 machine layer (boot protocol, descriptor tables, traps, ACPI,
 * APIC interrupts, physical pages, page tables, ring 3, SMP) on real
 * firmware, printing one marker per milestone for the smoke tests.
 */

#include <sys/types.h>
#include <sys/controlregs.h>
#include <sys/amd64pc.h>

struct amd64pc_bootinfo amd64pc_bootinfo;

extern char boot_stack_top[];
extern char __image_start[], __image_end[];

static void
check(int rv, const char *what, const char *marker)
{
	if (rv != 0) {
		cons_puts("amd64pc: ");
		cons_puts(what);
		cons_puts(" FAILED, code ");
		cons_puthex((uint64_t)(int64_t)rv);
		cons_puts("\n");
		amd64pc_panic("bootstrap self-test failed");
	}
	cons_puts("amd64pc: ");
	cons_puts(what);
	cons_puts(" ");
	cons_puts(marker);
	cons_puts("\n");
}

static void
print_cpu(void)
{
	uint32_t regs[13], a, b, c, d, i;
	char *s = (char *)regs;

	pc_cpuid(0, 0, &a, &regs[0], &regs[2], &regs[1]);
	regs[3] = 0;
	cons_puts("amd64pc: CPU vendor ");
	cons_puts(s);
	pc_cpuid(0x80000000, 0, &a, &b, &c, &d);
	if (a >= 0x80000004) {
		for (i = 0; i < 3; i++)
			pc_cpuid(0x80000002 + i, 0, &regs[i * 4],
			    &regs[i * 4 + 1], &regs[i * 4 + 2],
			    &regs[i * 4 + 3]);
		regs[12] = 0;
		while (*s == ' ')
			s++;
		cons_puts(", ");
		cons_puts(s);
	}
	cons_puts("\n");
}

void
amd64pc_main(uint32_t magic, uint32_t mbi)
{
	struct amd64pc_bootinfo *bi = &amd64pc_bootinfo;
	uint64_t mem = 0, t0;
	uint32_t m;
	int rv;

	/*
	 * %gs:0 identifies the current CPU once smp_start() publishes it;
	 * until then make sure nothing the firmware left in GSBASE is
	 * mistaken for a per-CPU structure (the trap path reads it).
	 */
	pc_wrmsr(MSR_AMD_GSBASE, 0);
	uart_init();
	cons_puts("\nOpenSolaris x86_64 (amd64pc) bootstrap\n");
	cons_puts("Copyright 2026 OpenSolaris Contributors.  "
	    "Use is subject to license terms.\n");

	if (mb2_parse(magic, mbi, bi) != 0)
		amd64pc_panic("not loaded by a Multiboot2 boot loader");
	fbcons_init(&bi->bi_fb);
	if (bi->bi_fb.fb_addr != 0) {
		/* Repeat the banner on the display that just came up. */
		cons_puts("\nOpenSolaris x86_64 (amd64pc) bootstrap\n");
	}
	for (m = 0; m < bi->bi_nmem; m++)
		mem += bi->bi_mem[m].ms_size;
	cons_puts("amd64pc: loader ");
	cons_puts(bi->bi_loader != 0 ? bi->bi_loader : "(unknown)");
	cons_puts(bi->bi_efi ? ", UEFI firmware" : ", BIOS firmware");
	cons_puts("\namd64pc: cmdline ");
	cons_puts(bi->bi_cmdline != 0 ? bi->bi_cmdline : "");
	cons_puts("\namd64pc: kernel ");
	cons_puthex((uint64_t)(uintptr_t)__image_start);
	cons_puts(" - ");
	cons_puthex((uint64_t)(uintptr_t)__image_end);
	cons_puts(", RAM ");
	cons_putdec(mem >> 20);
	cons_puts(" MiB in ");
	cons_putdec(bi->bi_nmem);
	cons_puts(" ranges MB2_OK\n");
	if (bi->bi_fb.fb_addr != 0) {
		cons_puts("amd64pc: display ");
		cons_puts(bi->bi_fb.fb_type == 2 ? "EGA text " :
		    "framebuffer ");
		cons_putdec(bi->bi_fb.fb_width);
		cons_puts("x");
		cons_putdec(bi->bi_fb.fb_height);
		cons_puts("x");
		cons_putdec(bi->bi_fb.fb_bpp);
		cons_puts(" at ");
		cons_puthex(bi->bi_fb.fb_addr);
		cons_puts("\n");
	}
	print_cpu();

	desctbl_init(0, (uint64_t)(uintptr_t)boot_stack_top);
	desctbl_load(0);
	cons_puts("amd64pc: GDT/IDT/TSS with segments.h selectors "
	    "DESCTBL_OK\n");
	check(trap_selftest(), "breakpoint/invalid-opcode/page-fault traps",
	    "TRAP_OK");

	rv = acpi_init(bi->bi_rsdp);
	if (rv != 0) {
		cons_puts("amd64pc: no usable ACPI MADT (");
		cons_puthex((uint64_t)(int64_t)rv);
		cons_puts("); uniprocessor\n");
	} else {
		cons_puts("amd64pc: ACPI OEM ");
		cons_puts(amd64pc_platform.oem);
		cons_puts(", ");
		cons_putdec(amd64pc_platform.cpu_count);
		cons_puts(" CPUs, local APIC ");
		cons_puthex(amd64pc_platform.lapic_base);
		cons_puts(", I/O APIC ");
		cons_puthex(amd64pc_platform.ioapic_base);
		cons_puts(" ACPI_OK\n");
	}

	tsc_calibrate();
	cons_puts("amd64pc: TSC ");
	cons_putdec(tsc_hz / 1000000);
	cons_puts(" MHz, hrtime ");
	cons_putdec(amd64pc_gethrtime() / 1000000);
	cons_puts(" ms TSC_OK\n");
	t0 = amd64pc_gethrtime();

	lapic_init();
	cons_puts("amd64pc: local APIC id ");
	cons_putdec(lapic_id());
	cons_puts(" enabled, 8259 PICs masked\n");
	check(lapic_timer_test(), "local APIC timer interrupt", "IRQ_OK");

	check(page_boot_init(bi), "common page_t freelist", "PAGE_INIT_OK");
	cons_puts("amd64pc: page_t pages ");
	cons_putdec(page_boot_total());
	cons_puts(" free ");
	cons_putdec(page_boot_freecnt());
	cons_puts("\n");
	check(page_selftest(), "page_t alloc/free round trip", "PAGE_OK");

	check(hat_boot_init(), "4-level page table engine", "HAT_INIT_OK");
	check(hat_selftest(), "high-half alias map/lookup/unmap",
	    "HAT_HIGH_OK");

	check(context_selftest(), "setjmp/longjmp", "CONTEXT_OK");
	check(user_selftest(), "ring 3 user mode", "USER_OK");
	check(reloc_selftest(), "uts/intel/amd64/krtld/doreloc.c relocations",
	    "RELOC_OK");
	check(common_selftest(), "linked uts/common genunix list_t",
	    "COMMON_OK");

	(void) smp_start();
	cons_puts("amd64pc: online CPUs ");
	cons_putdec(smp_ncpus_online());
	cons_puts(" SMP_OK\n");
	check(smp_selftest(), "cross-CPU tests", "XCPU_OK");

	cons_puts("amd64pc: self-tests took ");
	cons_putdec((amd64pc_gethrtime() - t0) / 1000000);
	cons_puts(" ms\n");
	cons_puts("OpenSolaris/x86_64 BOOT_OK\n");
	cons_puts("amd64pc: common kernel handoff (uts/common/os/main.c) is "
	    "the next milestone; halting.\n");
	for (;;)
		__asm__ __volatile__("cli; hlt");
}
