/* Copyright 2026 OpenSolaris AArch64 Contributors. */

#include <sys/types.h>
#include <sys/bootmem.h>

extern char __image_end[];

static uint64_t bootmem_start;
static uint64_t bootmem_cursor;
static uint64_t bootmem_limit;
static uint64_t bootmem_reserved_start;
static uint64_t bootmem_reserved_end;

static uint64_t
align_up(uint64_t value, uint64_t alignment)
{
	return ((value + alignment - 1) & ~(alignment - 1));
}

int
arm64_bootmem_init(const struct arm64_platform *platform, uint64_t fdt,
    uint64_t fdt_size)
{
	uint64_t image_end = (uint64_t)(uintptr_t)__image_end;

	if (platform == 0 || platform->ram_size == 0 ||
	    image_end < platform->ram_base ||
	    image_end >= platform->ram_base + platform->ram_size)
		return (-1);
	bootmem_start = align_up(image_end, 4096);
	bootmem_cursor = bootmem_start;
	bootmem_limit = platform->ram_base + platform->ram_size;
	bootmem_reserved_start = fdt;
	bootmem_reserved_end = fdt + fdt_size;
	return (0);
}

void *
arm64_bootmem_alloc(uint64_t size, uint64_t alignment)
{
	uint64_t start, end;
	uint8_t *memory;

	if (size == 0 || alignment == 0 || (alignment & (alignment - 1)) != 0)
		return (0);
	start = align_up(bootmem_cursor, alignment);
	end = start + size;
	if (end < start)
		return (0);
	if (start < bootmem_reserved_end && end > bootmem_reserved_start) {
		start = align_up(bootmem_reserved_end, alignment);
		end = start + size;
	}
	if (end < start || end > bootmem_limit)
		return (0);

	memory = (uint8_t *)(uintptr_t)start;
	bootmem_cursor = end;
	while (size-- != 0)
		*memory++ = 0;
	return ((void *)(uintptr_t)start);
}

uint64_t
arm64_bootmem_used(void)
{
	return (bootmem_cursor - bootmem_start);
}

/*
 * Hand the tail of the boot arena to a higher-level allocator.  The
 * boot allocator keeps `reserve' bytes above its cursor for later
 * boot-time callers and stops growing past the returned boundary.
 */
uint64_t
arm64_bootmem_carve_tail(uint64_t reserve, uint64_t alignment)
{
	uint64_t boundary;

	if (alignment == 0 || (alignment & (alignment - 1)) != 0)
		return (0);
	boundary = align_up(bootmem_cursor + reserve, alignment);
	if (boundary >= bootmem_limit)
		return (0);
	bootmem_limit = boundary;
	return (boundary);
}
