/* Copyright 2026 OpenSolaris AArch64 Contributors. */

#include <sys/types.h>
#include <sys/machparam.h>
#include <sys/bootmem.h>
#include <sys/boot_hat.h>
#include <sys/smp.h>

#define	TEST_VA	(KERNELBASE + 0x400000000ULL)
#define	TEST_USER_VA	0x0000000400000000ULL

extern struct arm64_platform arm64_platform;

static int
arm64_hat_tlb_xcall(xc_arg_t asid, xc_arg_t unused1, xc_arg_t unused2)
{
	(void) unused1;
	(void) unused2;
	arm64_boot_hat_tlb_asid((uint16_t)asid);
	return (0);
}

int
arm64_hat_selftest(void)
{
	volatile uint64_t *physical;
	volatile uint64_t *alias = (volatile uint64_t *)(uintptr_t)TEST_VA;
	uint64_t translated;

	physical = arm64_bootmem_alloc(4096, 4096);
	if (physical == 0)
		return (-1);
	*physical = 0x1122334455667788ULL;
	if (arm64_boot_hat_map(TEST_VA, (uint64_t)(uintptr_t)physical,
	    ARM64_HAT_WRITE) != 0 ||
	    arm64_boot_hat_lookup(TEST_VA, &translated) != 0 ||
	    translated != (uint64_t)(uintptr_t)physical ||
	    *alias != *physical)
		return (-1);
	*alias = 0x8877665544332211ULL;
	if (*physical != 0x8877665544332211ULL ||
	    arm64_boot_hat_unmap(TEST_VA) != 0 ||
	    arm64_boot_hat_lookup(TEST_VA, &translated) == 0)
		return (-1);
	return (0);
}

int
arm64_user_hat_selftest(void)
{
	struct arm64_boot_as as;
	volatile uint64_t *first, *second;
	volatile uint64_t *alias =
	    (volatile uint64_t *)(uintptr_t)TEST_USER_VA;
	uint64_t old_ttbr0, translated;

	first = arm64_bootmem_alloc(4096, 4096);
	second = arm64_bootmem_alloc(4096, 4096);
	if (first == 0 || second == 0 ||
	    arm64_boot_hat_as_create(&as, 7) != 0)
		return (-1);
	*first = 0x0102030405060708ULL;
	*second = 0x8899aabbccddeeffULL;
	if (arm64_boot_hat_as_map(&as, TEST_USER_VA,
	    (uint64_t)(uintptr_t)first, ARM64_HAT_WRITE) != 0 ||
	    arm64_boot_hat_as_lookup(&as, TEST_USER_VA, &translated) != 0 ||
	    translated != (uint64_t)(uintptr_t)first)
		return (-1);

	old_ttbr0 = arm64_boot_hat_as_switch(&as);
	if (*alias != *first) {
		arm64_boot_hat_as_restore(old_ttbr0);
		return (-1);
	}
	*alias = 0x1020304050607080ULL;
	arm64_boot_hat_as_restore(old_ttbr0);
	if (*first != 0x1020304050607080ULL ||
	    arm64_boot_hat_as_unmap(&as, TEST_USER_VA) != 0 ||
	    arm64_boot_hat_as_map(&as, TEST_USER_VA,
	    (uint64_t)(uintptr_t)second, ARM64_HAT_WRITE) != 0)
		return (-1);

	arm64_boot_hat_tlb_asid(as.aba_asid);
	if (arm64_xcall_sync(&arm64_platform, arm64_hat_tlb_xcall,
	    as.aba_asid, 0, 0, 0) != 0)
		return (-1);
	old_ttbr0 = arm64_boot_hat_as_switch(&as);
	if (*alias != *second) {
		arm64_boot_hat_as_restore(old_ttbr0);
		return (-1);
	}
	arm64_boot_hat_as_restore(old_ttbr0);
	return (0);
}
