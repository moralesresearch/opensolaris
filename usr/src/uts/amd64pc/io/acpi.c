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
 * Allocation-free ACPI table discovery for the bootstrap: locate the
 * RSDP (from Multiboot2, else the BIOS areas), walk the XSDT/RSDT and
 * decode the MADT into the CPU and interrupt-controller inventory that
 * SMP start-up needs.  The table layouts are the in-tree ACPICA ones
 * (uts/intel/sys/acpi), the same definitions the full kernel's
 * acpica and pcplusmp modules consume.
 */

#include <sys/types.h>
#include <sys/acpi/acpi.h>
#include <sys/apic.h>
#include <sys/amd64pc.h>

struct amd64pc_platform amd64pc_platform;

static int
sig_eq(const char *a, const char *b, int n)
{
	while (n-- > 0)
		if (*a++ != *b++)
			return (0);
	return (1);
}

static int
checksum_ok(const void *p, uint32_t len)
{
	const uint8_t *b = p;
	uint8_t sum = 0;

	while (len-- != 0)
		sum += *b++;
	return (sum == 0);
}

static uint64_t
rsdp_scan(uint64_t start, uint64_t len)
{
	uint64_t p;

	for (p = start; p + ACPI_RSDP_REV0_SIZE <= start + len; p += 16) {
		const ACPI_TABLE_RSDP *r =
		    (const ACPI_TABLE_RSDP *)(uintptr_t)p;

		if (sig_eq(r->Signature, "RSD PTR ", 8) &&
		    checksum_ok(r, ACPI_RSDP_REV0_SIZE))
			return (p);
	}
	return (0);
}

static void
madt_parse(const ACPI_TABLE_MADT *madt)
{
	struct amd64pc_platform *pl = &amd64pc_platform;
	const uint8_t *p = (const uint8_t *)(madt + 1);
	const uint8_t *end = (const uint8_t *)madt + madt->Header.Length;

	pl->lapic_base = madt->Address;
	while (p + sizeof (ACPI_SUBTABLE_HEADER) <= end) {
		const ACPI_SUBTABLE_HEADER *h =
		    (const ACPI_SUBTABLE_HEADER *)p;

		if (h->Length < sizeof (*h) || p + h->Length > end)
			break;
		switch (h->Type) {
		case ACPI_MADT_TYPE_LOCAL_APIC: {
			const ACPI_MADT_LOCAL_APIC *la =
			    (const ACPI_MADT_LOCAL_APIC *)h;

			if ((la->LapicFlags & ACPI_MADT_ENABLED) != 0 &&
			    pl->cpu_count < AMD64PC_MAX_CPUS)
				pl->cpu_apicid[pl->cpu_count++] = la->Id;
			break;
		}
		case ACPI_MADT_TYPE_LOCAL_X2APIC: {
			const ACPI_MADT_LOCAL_X2APIC *lx =
			    (const ACPI_MADT_LOCAL_X2APIC *)h;

			if ((lx->LapicFlags & ACPI_MADT_ENABLED) != 0 &&
			    lx->LocalApicId < 0xff &&
			    pl->cpu_count < AMD64PC_MAX_CPUS)
				pl->cpu_apicid[pl->cpu_count++] =
				    lx->LocalApicId;
			break;
		}
		case ACPI_MADT_TYPE_IO_APIC: {
			const ACPI_MADT_IO_APIC *io =
			    (const ACPI_MADT_IO_APIC *)h;

			if (pl->ioapic_count++ == 0)
				pl->ioapic_base = io->Address;
			break;
		}
		case ACPI_MADT_TYPE_LOCAL_APIC_OVERRIDE: {
			const ACPI_MADT_LOCAL_APIC_OVERRIDE *ov =
			    (const ACPI_MADT_LOCAL_APIC_OVERRIDE *)h;

			pl->lapic_base = ov->Address;
			break;
		}
		default:
			break;
		}
		p += h->Length;
	}
}

int
acpi_init(uint64_t rsdp_pa)
{
	const ACPI_TABLE_RSDP *rsdp;
	const ACPI_TABLE_HEADER *sdt;
	uint32_t i, n, entsz;
	int i_madt = 0;

	if (rsdp_pa == 0) {
		uint64_t ebda = (uint64_t)*(volatile uint16_t *)0x40e << 4;

		if (ebda != 0)
			rsdp_pa = rsdp_scan(ebda, 1024);
		if (rsdp_pa == 0)
			rsdp_pa = rsdp_scan(0xe0000, 0x20000);
	}
	if (rsdp_pa == 0)
		return (-1);
	rsdp = (const ACPI_TABLE_RSDP *)(uintptr_t)rsdp_pa;
	if (!sig_eq(rsdp->Signature, "RSD PTR ", 8))
		return (-2);
	for (i = 0; i < 6; i++)
		amd64pc_platform.oem[i] = rsdp->OemId[i];
	amd64pc_platform.oem[6] = '\0';

	if (rsdp->Revision >= 2 && rsdp->XsdtPhysicalAddress != 0) {
		sdt = (const ACPI_TABLE_HEADER *)(uintptr_t)
		    rsdp->XsdtPhysicalAddress;
		entsz = 8;
	} else {
		sdt = (const ACPI_TABLE_HEADER *)(uintptr_t)
		    rsdp->RsdtPhysicalAddress;
		entsz = 4;
	}
	if ((uint64_t)(uintptr_t)sdt >= (4ULL << 30) ||
	    !checksum_ok(sdt, sdt->Length))
		return (-3);

	n = (sdt->Length - sizeof (*sdt)) / entsz;
	for (i = 0; i < n; i++) {
		const uint8_t *ent = (const uint8_t *)(sdt + 1) + i * entsz;
		uint64_t pa = entsz == 8 ? *(const uint64_t *)ent :
		    *(const uint32_t *)ent;
		const ACPI_TABLE_HEADER *t =
		    (const ACPI_TABLE_HEADER *)(uintptr_t)pa;

		if (pa == 0 || pa >= (4ULL << 30))
			continue;
		if (sig_eq(t->Signature, ACPI_SIG_MADT, 4) &&
		    checksum_ok(t, t->Length)) {
			madt_parse((const ACPI_TABLE_MADT *)t);
			i_madt = 1;
		}
	}
	if (!i_madt) {
		/* Uniprocessor fallback: the architectural APIC address. */
		amd64pc_platform.lapic_base = APIC_LOCAL_ADDR;
		amd64pc_platform.cpu_count = 0;
		return (-4);
	}
	return (0);
}
