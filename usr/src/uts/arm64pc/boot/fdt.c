/*
 * Copyright 2026 OpenSolaris AArch64 Contributors.
 *
 * Allocation-free flattened-device-tree reader for early AArch64 boot.
 */

#include <sys/types.h>
#include <sys/fdt.h>
#include <sys/psci.h>

#define	FDT_MAGIC	0xd00dfeedU
#define	FDT_BEGIN_NODE	1U
#define	FDT_END_NODE	2U
#define	FDT_PROP	3U
#define	FDT_NOP		4U
#define	FDT_END		9U
#define	FDT_MAX_DEPTH	16

struct fdt_header {
	uint32_t magic;
	uint32_t totalsize;
	uint32_t off_dt_struct;
	uint32_t off_dt_strings;
	uint32_t off_mem_rsvmap;
	uint32_t version;
	uint32_t last_comp_version;
	uint32_t boot_cpuid_phys;
	uint32_t size_dt_strings;
	uint32_t size_dt_struct;
};

struct fdt_node_state {
	uint32_t child_address_cells;
	uint32_t child_size_cells;
	uint32_t parent_address_cells;
	uint32_t parent_size_cells;
	uint64_t reg_address[2];
	uint64_t reg_size[2];
	uint32_t reg_count;
	uint8_t is_memory;
	uint8_t is_cpu;
	uint8_t is_uart;
	uint8_t is_gicv3;
	uint8_t is_psci;
};

static uint32_t
be32(const void *address)
{
	const uint8_t *p = address;

	return (((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
	    ((uint32_t)p[2] << 8) | p[3]);
}

static uint64_t
be_cells(const uint8_t *data, uint32_t cells)
{
	uint64_t value = 0;
	uint32_t i;

	if (cells > 2)
		return (0);
	for (i = 0; i < cells; i++)
		value = (value << 32) | be32(data + i * 4);
	return (value);
}

static void
zero_bytes(void *target, uint32_t length)
{
	uint8_t *p = target;

	while (length-- != 0)
		*p++ = 0;
}

static int
string_is(const uint8_t *data, uint32_t length, const char *expected)
{
	uint32_t i = 0;

	while (i < length && expected[i] != '\0' &&
	    data[i] == (uint8_t)expected[i])
		i++;
	return (i < length && data[i] == '\0' && expected[i] == '\0');
}

static int
string_list_has(const uint8_t *data, uint32_t length, const char *expected)
{
	uint32_t offset = 0;

	while (offset < length) {
		uint32_t item_length = 0;
		while (offset + item_length < length &&
		    data[offset + item_length] != '\0')
			item_length++;
		if (offset + item_length == length)
			return (0);
		if (string_is(data + offset, item_length + 1, expected))
			return (1);
		offset += item_length + 1;
	}
	return (0);
}

static void
parse_reg(struct fdt_node_state *node, const uint8_t *data, uint32_t length)
{
	uint32_t address_cells = node->parent_address_cells;
	uint32_t size_cells = node->parent_size_cells;
	uint32_t tuple_cells = address_cells + size_cells;
	uint32_t offset = 0;

	if (address_cells > 2 || size_cells > 2 || tuple_cells == 0)
		return;
	while (node->reg_count < 2 && offset + tuple_cells * 4 <= length) {
		uint32_t slot = node->reg_count++;
		node->reg_address[slot] = be_cells(data + offset, address_cells);
		offset += address_cells * 4;
		node->reg_size[slot] = be_cells(data + offset, size_cells);
		offset += size_cells * 4;
	}
}

static void
finish_node(const struct fdt_node_state *node, struct arm64_platform *platform)
{
	if (node->is_memory && node->reg_count != 0) {
		platform->ram_base = node->reg_address[0];
		platform->ram_size = node->reg_size[0];
	}
	if (node->is_uart && node->reg_count != 0) {
		platform->uart_base = node->reg_address[0];
		platform->uart_size = node->reg_size[0];
	}
	if (node->is_gicv3 && node->reg_count >= 2) {
		platform->gicd_base = node->reg_address[0];
		platform->gicd_size = node->reg_size[0];
		platform->gicr_base = node->reg_address[1];
		platform->gicr_size = node->reg_size[1];
	}
	if (node->is_cpu && node->reg_count != 0 &&
	    platform->cpu_count < ARM64_FDT_MAX_CPUS) {
		platform->cpu_mpidr[platform->cpu_count++] =
		    node->reg_address[0];
	}
}

int
arm64_fdt_validate(uint64_t address)
{
	const struct fdt_header *header;
	uint32_t total, struct_off, struct_size, strings_off, strings_size;

	if (address == 0 || (address & 7) != 0)
		return (-1);
	header = (const struct fdt_header *)(uintptr_t)address;
	if (be32(&header->magic) != FDT_MAGIC)
		return (-1);
	total = be32(&header->totalsize);
	struct_off = be32(&header->off_dt_struct);
	struct_size = be32(&header->size_dt_struct);
	strings_off = be32(&header->off_dt_strings);
	strings_size = be32(&header->size_dt_strings);
	if (total < sizeof (*header) || total > (16U * 1024U * 1024U))
		return (-1);
	if (struct_off > total || struct_size > total - struct_off)
		return (-1);
	if (strings_off > total || strings_size > total - strings_off)
		return (-1);
	return (0);
}

uint32_t
arm64_fdt_size(uint64_t address)
{
	const struct fdt_header *header;

	if (arm64_fdt_validate(address) != 0)
		return (0);
	header = (const struct fdt_header *)(uintptr_t)address;
	return (be32(&header->totalsize));
}

int
arm64_fdt_parse(uint64_t address, struct arm64_platform *platform)
{
	const struct fdt_header *header;
	const uint8_t *base, *cursor, *struct_end, *strings;
	struct fdt_node_state nodes[FDT_MAX_DEPTH];
	uint32_t strings_size;
	int depth = -1;

	if (platform == 0 || arm64_fdt_validate(address) != 0)
		return (-1);
	zero_bytes(platform, sizeof (*platform));
	zero_bytes(nodes, sizeof (nodes));

	base = (const uint8_t *)(uintptr_t)address;
	header = (const struct fdt_header *)base;
	cursor = base + be32(&header->off_dt_struct);
	struct_end = cursor + be32(&header->size_dt_struct);
	strings = base + be32(&header->off_dt_strings);
	strings_size = be32(&header->size_dt_strings);

	while (cursor + 4 <= struct_end) {
		uint32_t token = be32(cursor);
		cursor += 4;

		if (token == FDT_BEGIN_NODE) {
			struct fdt_node_state *node;
			const uint8_t *name;

			if (++depth >= FDT_MAX_DEPTH)
				return (-1);
			node = &nodes[depth];
			zero_bytes(node, sizeof (*node));
			if (depth == 0) {
				node->parent_address_cells = 2;
				node->parent_size_cells = 1;
			} else {
				node->parent_address_cells =
				    nodes[depth - 1].child_address_cells;
				node->parent_size_cells =
				    nodes[depth - 1].child_size_cells;
			}
			node->child_address_cells = node->parent_address_cells;
			node->child_size_cells = node->parent_size_cells;
			name = cursor;
			while (cursor < struct_end && *cursor != '\0')
				cursor++;
			if (cursor == struct_end)
				return (-1);
			node->is_psci = string_is(name,
			    (uint32_t)(cursor - name) + 1, "psci");
			cursor++;
			cursor = (const uint8_t *)(((uintptr_t)cursor + 3) & ~3UL);
		} else if (token == FDT_END_NODE) {
			if (depth < 0)
				return (-1);
			finish_node(&nodes[depth], platform);
			depth--;
		} else if (token == FDT_PROP) {
			struct fdt_node_state *node;
			uint32_t length, nameoff;
			const char *name;

			if (depth < 0 || cursor + 8 > struct_end)
				return (-1);
			length = be32(cursor);
			nameoff = be32(cursor + 4);
			cursor += 8;
			if (length > (uint32_t)(struct_end - cursor) ||
			    nameoff >= strings_size)
				return (-1);
			name = (const char *)(strings + nameoff);
			node = &nodes[depth];
			if (string_is((const uint8_t *)name,
			    strings_size - nameoff, "#address-cells") && length >= 4)
				node->child_address_cells = be32(cursor);
			else if (string_is((const uint8_t *)name,
			    strings_size - nameoff, "#size-cells") && length >= 4)
				node->child_size_cells = be32(cursor);
			else if (string_is((const uint8_t *)name,
			    strings_size - nameoff, "reg"))
				parse_reg(node, cursor, length);
			else if (string_is((const uint8_t *)name,
			    strings_size - nameoff, "device_type")) {
				node->is_memory |= string_is(cursor, length, "memory");
				node->is_cpu |= string_is(cursor, length, "cpu");
			} else if (string_is((const uint8_t *)name,
			    strings_size - nameoff, "compatible")) {
				node->is_uart |= string_list_has(cursor, length,
				    "arm,pl011");
				node->is_gicv3 |= string_list_has(cursor, length,
				    "arm,gic-v3");
				node->is_psci |= string_list_has(cursor, length,
				    "arm,psci");
			} else if (node->is_psci &&
			    string_is((const uint8_t *)name,
			    strings_size - nameoff, "method")) {
				if (string_is(cursor, length, "smc"))
					platform->psci_conduit = PSCI_CONDUIT_SMC;
				else if (string_is(cursor, length, "hvc"))
					platform->psci_conduit = PSCI_CONDUIT_HVC;
			}
			cursor += length;
			cursor = (const uint8_t *)(((uintptr_t)cursor + 3) & ~3UL);
		} else if (token == FDT_END) {
			break;
		} else if (token != FDT_NOP) {
			return (-1);
		}
	}

	if (platform->ram_size == 0 || platform->uart_base == 0 ||
	    platform->gicd_base == 0 || platform->gicr_base == 0 ||
	    platform->cpu_count == 0 || platform->psci_conduit == 0)
		return (-1);
	return (0);
}

int
arm64_fdt_psci_conduit(uint64_t address)
{
	struct arm64_platform platform;

	if (arm64_fdt_parse(address, &platform) != 0)
		return (0);
	return (platform.psci_conduit);
}
