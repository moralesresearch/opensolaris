/*
 * Copyright 2026 OpenSolaris AArch64 Contributors.
 *
 * AArch64 RELA instruction/data relocation core for krtld.  The wrapper that
 * walks OpenSolaris module symbol tables can call this routine once it has
 * resolved S and A.  Keeping instruction encoding here also makes it directly
 * testable by the bootstrap image.
 */

#include <sys/types.h>
#include <sys/elf_AARCH64.h>
#include <sys/kobj_reloc.h>

static int
fits_signed(int64_t value, unsigned int bits)
{
	int64_t limit = (int64_t)1 << (bits - 1);

	return (value >= -limit && value < limit);
}

static int
reloc_ldst(uint32_t *where, uint64_t value, unsigned int scale)
{
	uint32_t insn;

	if ((value & (((uint64_t)1 << scale) - 1)) != 0)
		return (-1);
	insn = *where;
	insn &= ~(0xfffU << 10);
	insn |= (uint32_t)(((value & 0xfff) >> scale) << 10);
	*where = insn;
	return (0);
}

int
arm64_reloc_apply(uint32_t type, void *location, uint64_t symbol,
    int64_t addend)
{
	uint64_t place = (uint64_t)(uintptr_t)location;
	uint64_t value = symbol + (uint64_t)addend;
	int64_t delta;
	uint32_t insn;

	switch (type) {
	case R_AARCH64_NONE:
		return (0);
	case R_AARCH64_ABS64:
		*(uint64_t *)location = value;
		return (0);
	case R_AARCH64_ABS32:
		if (value > UINT32_MAX)
			return (-1);
		*(uint32_t *)location = (uint32_t)value;
		return (0);
	case R_AARCH64_PREL64:
		*(uint64_t *)location = value - place;
		return (0);
	case R_AARCH64_PREL32:
		delta = (int64_t)(value - place);
		if (!fits_signed(delta, 32))
			return (-1);
		*(uint32_t *)location = (uint32_t)delta;
		return (0);
	case R_AARCH64_ADR_PREL_PG_HI21:
	case R_AARCH64_ADR_PREL_PG_HI21_NC: {
		uint64_t imm;

		delta = (int64_t)((value & ~0xfffULL) -
		    (place & ~0xfffULL)) >> 12;
		if (type == R_AARCH64_ADR_PREL_PG_HI21 &&
		    !fits_signed(delta, 21))
			return (-1);
		imm = (uint64_t)delta & 0x1fffff;
		insn = *(uint32_t *)location;
		insn &= ~((3U << 29) | (0x7ffffU << 5));
		insn |= (uint32_t)((imm & 3) << 29) |
		    (uint32_t)(((imm >> 2) & 0x7ffff) << 5);
		*(uint32_t *)location = insn;
		return (0);
	}
	case R_AARCH64_ADD_ABS_LO12_NC:
		insn = *(uint32_t *)location;
		insn &= ~(0xfffU << 10);
		insn |= (uint32_t)((value & 0xfff) << 10);
		*(uint32_t *)location = insn;
		return (0);
	case R_AARCH64_LDST8_ABS_LO12_NC:
		return (reloc_ldst(location, value, 0));
	case R_AARCH64_LDST16_ABS_LO12_NC:
		return (reloc_ldst(location, value, 1));
	case R_AARCH64_LDST32_ABS_LO12_NC:
		return (reloc_ldst(location, value, 2));
	case R_AARCH64_LDST64_ABS_LO12_NC:
		return (reloc_ldst(location, value, 3));
	case R_AARCH64_LDST128_ABS_LO12_NC:
		return (reloc_ldst(location, value, 4));
	case R_AARCH64_JUMP26:
	case R_AARCH64_CALL26:
		delta = (int64_t)(value - place);
		if ((delta & 3) != 0 || !fits_signed(delta, 28))
			return (-1);
		insn = *(uint32_t *)location;
		insn = (insn & 0xfc000000U) |
		    ((uint32_t)(delta >> 2) & 0x03ffffffU);
		*(uint32_t *)location = insn;
		return (0);
	default:
		return (-1);
	}
}
