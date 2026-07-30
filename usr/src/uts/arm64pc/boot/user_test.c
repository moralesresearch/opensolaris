/* Copyright 2026 OpenSolaris AArch64 Contributors. */

#include <sys/types.h>
#include <sys/controlregs.h>
#include <sys/privregs.h>
#include <sys/bootmem.h>
#include <sys/boot_hat.h>
#include <sys/usermode.h>

#define	EL0_TEXT_VA	0x0000000500000000ULL
#define	EL0_STACK_VA	0x0000000500002000ULL
#define	EL0_FAULT_TEXT_VA	0x0000000500004000ULL
#define	EL0_FAULT_DATA_VA	0x0000000600000000ULL

static struct arm64_boot_as *fault_as;
static uint64_t fault_page;
static uint_t fault_count;

static void
arm64_sync_text(uint32_t *text, size_t size)
{
	size_t i;

	for (i = 0; i != size; i += sizeof (uint32_t)) {
		arm64_dc_civac((void *)((uintptr_t)text + i));
		arm64_ic_ivau((void *)((uintptr_t)text + i));
	}
	dsb(ish);
	isb();
}

int
arm64_el0_selftest(void)
{
	struct arm64_boot_as as;
	uint32_t *text;
	void *stack;
	uint64_t old_ttbr0, result;
	unsigned int i;
	static const uint32_t program[] = {
		0xd2800240U, /* mov x0, #0x12 */
		0xd2800601U, /* mov x1, #0x30 */
		0xd2800028U, /* mov x8, #1 */
		0xd4000001U, /* svc #0 */
		0xd4000021U, /* svc #1: bootstrap return to EL1 */
		0x14000000U  /* b . */
	};

	text = arm64_bootmem_alloc(4096, 4096);
	stack = arm64_bootmem_alloc(4096, 4096);
	if (text == 0 || stack == 0 ||
	    arm64_boot_hat_as_create(&as, 8) != 0)
		return (-1);
	for (i = 0; i != sizeof (program) / sizeof (program[0]); i++)
		text[i] = program[i];
	arm64_sync_text(text, sizeof (program));

	if (arm64_boot_hat_as_map(&as, EL0_TEXT_VA,
	    (uint64_t)(uintptr_t)text, ARM64_HAT_EXEC) != 0 ||
	    arm64_boot_hat_as_map(&as, EL0_STACK_VA,
	    (uint64_t)(uintptr_t)stack, ARM64_HAT_WRITE) != 0)
		return (-1);
	arm64_boot_hat_tlb_asid(as.aba_asid);
	old_ttbr0 = arm64_boot_hat_as_switch(&as);
	result = arm64_el0_enter(EL0_TEXT_VA, EL0_STACK_VA + 4096 - 16);
	arm64_boot_hat_as_restore(old_ttbr0);
	return (result == 0x42 ? 0 : -1);
}

static int
arm64_test_pagefault(uint64_t address, uint_t protection, uint_t write)
{
	if (fault_as == 0 || protection != 0 || write != 0 ||
	    (address & ~(4096ULL - 1)) != EL0_FAULT_DATA_VA)
		return (-1);
	fault_count++;
	return (arm64_boot_hat_as_map(fault_as, EL0_FAULT_DATA_VA,
	    fault_page, ARM64_HAT_WRITE));
}

int
arm64_el0_fault_selftest(void)
{
	struct arm64_boot_as as;
	uint32_t *text;
	volatile uint64_t *data;
	uint64_t old_ttbr0, result;
	unsigned int i;
	static const uint32_t program[] = {
		0x580000c2U, /* ldr x2, literal at pc + 24 */
		0xf9400040U, /* ldr x0, [x2] -- initially unmapped */
		0xd4000021U, /* svc #1: bootstrap return to EL1 */
		0x14000000U, /* b . */
		0xd503201fU, /* nop */
		0xd503201fU, /* nop */
		(uint32_t)EL0_FAULT_DATA_VA,
		(uint32_t)(EL0_FAULT_DATA_VA >> 32)
	};

	text = arm64_bootmem_alloc(4096, 4096);
	data = arm64_bootmem_alloc(4096, 4096);
	if (text == 0 || data == 0 ||
	    arm64_boot_hat_as_create(&as, 9) != 0)
		return (-1);
	for (i = 0; i != sizeof (program) / sizeof (program[0]); i++)
		text[i] = program[i];
	*data = 0xfeedfacecafebeefULL;
	arm64_sync_text(text, sizeof (program));
	if (arm64_boot_hat_as_map(&as, EL0_FAULT_TEXT_VA,
	    (uint64_t)(uintptr_t)text, ARM64_HAT_EXEC) != 0)
		return (-1);
	fault_as = &as;
	fault_page = (uint64_t)(uintptr_t)data;
	fault_count = 0;
	arm64_pagefault_dispatch = arm64_test_pagefault;
	old_ttbr0 = arm64_boot_hat_as_switch(&as);
	result = arm64_el0_enter(EL0_FAULT_TEXT_VA,
	    EL0_STACK_VA + 4096 - 16);
	arm64_boot_hat_as_restore(old_ttbr0);
	arm64_pagefault_dispatch = 0;
	fault_as = 0;
	return (result == *data && fault_count == 1 ? 0 : -1);
}
