/* Copyright 2026 OpenSolaris AArch64 Contributors. */

#include <sys/types.h>
#include <sys/elf_AARCH64.h>
#include <sys/kobj_reloc.h>

int
arm64_reloc_selftest(void)
{
	uint64_t data64 = 0;
	uint32_t data32 = 0;
	uint32_t adrp = 0x90000000U;
	uint32_t add = 0x91000000U;
	uint32_t ldr = 0xf9400000U;
	uint32_t call = 0x94000000U;
	uint64_t place;

	if (arm64_reloc_apply(R_AARCH64_ABS64, &data64, 0x1000, 0x20) != 0 ||
	    data64 != 0x1020)
		return (-1);
	if (arm64_reloc_apply(R_AARCH64_ABS32, &data32, 0x12340000, 0x5678) !=
	    0 || data32 != 0x12345678)
		return (-1);
	place = (uint64_t)(uintptr_t)&adrp;
	if (arm64_reloc_apply(R_AARCH64_ADR_PREL_PG_HI21, &adrp,
	    (place & ~0xfffULL) + 0x2000, 0) != 0 ||
	    (adrp & ((3U << 29) | (0x7ffffU << 5))) != (2U << 29))
		return (-1);
	if (arm64_reloc_apply(R_AARCH64_ADD_ABS_LO12_NC, &add, 0xabc, 0) != 0 ||
	    ((add >> 10) & 0xfff) != 0xabc)
		return (-1);
	if (arm64_reloc_apply(R_AARCH64_LDST64_ABS_LO12_NC, &ldr, 0x5a8, 0) !=
	    0 || ((ldr >> 10) & 0xfff) != (0x5a8 >> 3))
		return (-1);
	place = (uint64_t)(uintptr_t)&call;
	if (arm64_reloc_apply(R_AARCH64_CALL26, &call, place + 0x100, 0) != 0 ||
	    (call & 0x03ffffffU) != 0x40)
		return (-1);
	return (0);
}
