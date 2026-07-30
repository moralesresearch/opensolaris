/*
 * Copyright 2026 OpenSolaris AArch64 Contributors.
 *
 * Bootstrap AArch64 HAT.  It owns a TTBR1 hierarchy with a 2 MiB direct map
 * and supports page-granular mappings for the VM, krtld and device layers
 * that are brought up before the full common HAT allocator is available.
 */

#include <sys/types.h>
#include <sys/controlregs.h>
#include <sys/machparam.h>
#include <sys/bootmem.h>
#include <sys/boot_hat.h>
#include <sys/boot_page.h>

#define	PTE_VALID	(1ULL << 0)
#define	PTE_TABLE	(1ULL << 1)
#define	PTE_ATTR_DEVICE	(0ULL << 2)
#define	PTE_ATTR_WB	(3ULL << 2)
#define	PTE_AP_USER	(1ULL << 6)
#define	PTE_AP_RO	(2ULL << 6)
#define	PTE_SH_IS	(3ULL << 8)
#define	PTE_AF		(1ULL << 10)
#define	PTE_PXN		(1ULL << 53)
#define	PTE_UXN		(1ULL << 54)
#define	PTE_ADDR	0x0000fffffffff000ULL

#define	L0_SHIFT	39
#define	L1_SHIFT	30
#define	L2_SHIFT	21
#define	L3_SHIFT	12
#define	LEVEL_MASK	0x1ffULL
#define	TABLE_SIZE	4096
#define	BLOCK_2M	(1ULL << L2_SHIFT)
#define	PAGE_SIZE_4K	(1ULL << L3_SHIFT)

static uint64_t hat_ram_base;
static uint64_t hat_ttbr1;
static uint64_t hat_table_pages;

extern void pl011_puts(const char *);

/*
 * Translation tables come from the common page_t freelist once it is
 * initialized; only the early direct map and the very first page-table
 * pages fall back to the linear boot allocator.
 */
static uint64_t *
hat_new_table(void)
{
	struct page *pp;

	if (arm64_page_initialized() != 0) {
		pp = arm64_page_alloc();
		if (pp == 0)
			return (0);
		hat_table_pages++;
		return ((uint64_t *)(uintptr_t)arm64_page_pa(pp));
	}
	return (arm64_bootmem_alloc(TABLE_SIZE, TABLE_SIZE));
}

uint64_t
arm64_boot_hat_table_pages(void)
{
	return (hat_table_pages);
}

static uint64_t *
hat_next_table(uint64_t *parent, uint64_t index)
{
	uint64_t entry = parent[index];
	uint64_t *child;

	if ((entry & PTE_VALID) != 0) {
		if ((entry & PTE_TABLE) == 0)
			return (0);
		return ((uint64_t *)(uintptr_t)(entry & PTE_ADDR));
	}
	child = hat_new_table();
	if (child == 0)
		return (0);
	parent[index] = (uint64_t)(uintptr_t)child | PTE_VALID | PTE_TABLE;
	return (child);
}

static uint64_t
hat_page_attrs(uint_t flags)
{
	uint64_t attrs = PTE_AF;

	if ((flags & ARM64_HAT_DEVICE) != 0)
		attrs |= PTE_ATTR_DEVICE | PTE_PXN | PTE_UXN;
	else
		attrs |= PTE_ATTR_WB | PTE_SH_IS;
	if ((flags & ARM64_HAT_WRITE) == 0)
		attrs |= PTE_AP_RO;
	if ((flags & ARM64_HAT_USER) != 0) {
		attrs |= PTE_AP_USER | PTE_PXN;
		if ((flags & ARM64_HAT_EXEC) == 0)
			attrs |= PTE_UXN;
	} else {
		attrs |= PTE_UXN;
		if ((flags & ARM64_HAT_EXEC) == 0)
			attrs |= PTE_PXN;
	}
	return (attrs);
}

static int
hat_map_2m(uint64_t root, uint64_t virtual, uint64_t physical, uint_t flags)
{
	uint64_t *l0 = (uint64_t *)(uintptr_t)root;
	uint64_t *l1, *l2;

	l1 = hat_next_table(l0, (virtual >> L0_SHIFT) & LEVEL_MASK);
	if (l1 == 0)
		return (-1);
	l2 = hat_next_table(l1, (virtual >> L1_SHIFT) & LEVEL_MASK);
	if (l2 == 0)
		return (-1);
	l2[(virtual >> L2_SHIFT) & LEVEL_MASK] =
	    (physical & ~(BLOCK_2M - 1)) | PTE_VALID |
	    hat_page_attrs(flags);
	return (0);
}

static void
hat_tlb_flush(void)
{
	dsb(ishst);
	__asm__ __volatile__("tlbi vmalle1is");
	dsb(ish);
	isb();
}

static uint64_t *
hat_l3_for_page(uint64_t root, uint64_t virtual)
{
	uint64_t *l0 = (uint64_t *)(uintptr_t)root;
	uint64_t *l1, *l2, *l3;
	uint64_t l2_index, old, base, i;

	l1 = hat_next_table(l0, (virtual >> L0_SHIFT) & LEVEL_MASK);
	if (l1 == 0)
		return (0);
	l2 = hat_next_table(l1, (virtual >> L1_SHIFT) & LEVEL_MASK);
	if (l2 == 0)
		return (0);
	l2_index = (virtual >> L2_SHIFT) & LEVEL_MASK;
	old = l2[l2_index];
	if ((old & PTE_VALID) != 0 && (old & PTE_TABLE) != 0)
		return ((uint64_t *)(uintptr_t)(old & PTE_ADDR));
	l3 = hat_new_table();
	if (l3 == 0)
		return (0);
	if ((old & PTE_VALID) != 0) {
		/*
		 * Splitting a live 2 MiB block descriptor requires
		 * break-before-make: the block and the table that replaces it
		 * describe the same VA range at different granularities, and
		 * the architecture leaves the TLB behaviour of a direct
		 * block-to-table swap UNPREDICTABLE.  Invalidate the block
		 * first, then rebuild it at 4 KiB granularity.
		 */
		l2[l2_index] = 0;
		hat_tlb_flush();
		base = old & PTE_ADDR;
		for (i = 0; i <= LEVEL_MASK; i++)
			l3[i] = (base + i * PAGE_SIZE_4K) |
			    (old & ~PTE_ADDR) | PTE_TABLE;
	}
	/* The new L3 entries must be visible before the table descriptor is. */
	dsb(ishst);
	l2[l2_index] = (uint64_t)(uintptr_t)l3 | PTE_VALID | PTE_TABLE;
	dsb(ishst);
	isb();
	return (l3);
}

/*
 * Install a 4 KiB translation.  The caller passes the ASID the mapping
 * belongs to (0 for the kernel TTBR1 map) so that a remap of a live VA can
 * retire the hment the old translation left on the old page's mapping list.
 */
static int
hat_map_page(uint64_t root, uint64_t virtual, uint64_t physical,
    uint_t flags, uint16_t asid)
{
	uint64_t *l3;
	uint64_t index, old;

	if (((virtual | physical) & (PAGE_SIZE_4K - 1)) != 0 || root == 0)
		return (-1);
	l3 = hat_l3_for_page(root, virtual);
	if (l3 == 0)
		return (-1);
	index = (virtual >> L3_SHIFT) & LEVEL_MASK;
	old = l3[index];
	if ((old & PTE_VALID) != 0) {
		/*
		 * Remapping a VA that already has a live translation.  Two
		 * things have to happen before the new PTE is written.
		 *
		 * Break-before-make: the old and the new translation describe
		 * the same VA with different output addresses or attributes,
		 * and the architecture leaves the TLB behaviour of a live
		 * swap UNPREDICTABLE.  Invalidate first, then rebuild.
		 *
		 * Mapping-list hygiene: the hment recorded for the old
		 * translation would otherwise be orphaned on the old page,
		 * leaving p_share and p_mapping permanently overstated and the
		 * page unfreeable.  Retire it here.  Untracked mappings --
		 * device MMIO with no page_t and anything established before
		 * arm64_page_init() -- have no hment, so the removal simply
		 * reports nothing to do and is ignored.
		 */
		l3[index] = 0;
		hat_tlb_flush();
		(void) arm64_page_mapping_remove(old & PTE_ADDR, virtual,
		    asid);
	}
	l3[index] = (physical & PTE_ADDR) |
	    PTE_VALID | PTE_TABLE | hat_page_attrs(flags);
	return (0);
}

static int
hat_unmap_page(uint64_t root, uint64_t virtual)
{
	uint64_t *l0, *l1, *l2, *l3;
	uint64_t entry;

	if ((virtual & (PAGE_SIZE_4K - 1)) != 0 || root == 0)
		return (-1);
	l0 = (uint64_t *)(uintptr_t)root;
	entry = l0[(virtual >> L0_SHIFT) & LEVEL_MASK];
	if ((entry & (PTE_VALID | PTE_TABLE)) !=
	    (PTE_VALID | PTE_TABLE))
		return (-1);
	l1 = (uint64_t *)(uintptr_t)(entry & PTE_ADDR);
	entry = l1[(virtual >> L1_SHIFT) & LEVEL_MASK];
	if ((entry & (PTE_VALID | PTE_TABLE)) !=
	    (PTE_VALID | PTE_TABLE))
		return (-1);
	l2 = (uint64_t *)(uintptr_t)(entry & PTE_ADDR);
	entry = l2[(virtual >> L2_SHIFT) & LEVEL_MASK];
	if ((entry & (PTE_VALID | PTE_TABLE)) !=
	    (PTE_VALID | PTE_TABLE))
		return (-1);
	l3 = (uint64_t *)(uintptr_t)(entry & PTE_ADDR);
	l3[(virtual >> L3_SHIFT) & LEVEL_MASK] = 0;
	return (0);
}

static int
hat_lookup_root(uint64_t root, uint64_t virtual, uint64_t *physical)
{
	uint64_t *table = (uint64_t *)(uintptr_t)root;
	uint64_t entry;
	unsigned int level;
	static const unsigned int shifts[] = {
		L0_SHIFT, L1_SHIFT, L2_SHIFT, L3_SHIFT
	};

	if (table == 0 || physical == 0)
		return (-1);
	for (level = 0; level != 4; level++) {
		entry = table[(virtual >> shifts[level]) & LEVEL_MASK];
		if ((entry & PTE_VALID) == 0)
			return (-1);
		if (level == 3 || (entry & PTE_TABLE) == 0) {
			uint64_t mask = (1ULL << shifts[level]) - 1;
			*physical = (entry & PTE_ADDR & ~mask) |
			    (virtual & mask);
			return (0);
		}
		table = (uint64_t *)(uintptr_t)(entry & PTE_ADDR);
	}
	return (-1);
}

int
arm64_boot_hat_init(const struct arm64_platform *platform)
{
	uint64_t virtual, physical, mapped, tcr;

	if (platform == 0 || platform->ram_size == 0 ||
	    (platform->ram_base & (BLOCK_2M - 1)) != 0)
		return (-1);
	hat_ttbr1 = (uint64_t)(uintptr_t)hat_new_table();
	if (hat_ttbr1 == 0)
		return (-1);
	hat_ram_base = platform->ram_base;
	virtual = KERNELBASE;
	physical = platform->ram_base;
	for (mapped = 0; mapped < platform->ram_size; mapped += BLOCK_2M) {
		if (hat_map_2m(hat_ttbr1, virtual + mapped, physical + mapped,
		    ARM64_HAT_WRITE | ARM64_HAT_EXEC) != 0)
			return (-1);
	}

	write_sysreg(ttbr1_el1, hat_ttbr1);
	tcr = read_sysreg(tcr_el1);
	tcr &= ~(1ULL << 23);
	write_sysreg(tcr_el1, tcr);
	hat_tlb_flush();
	return (0);
}

int
arm64_boot_hat_map(uint64_t virtual, uint64_t physical, uint_t flags)
{
	if (hat_map_page(hat_ttbr1, virtual, physical, flags, 0) != 0)
		return (-1);
	if (arm64_page_mapping_add(physical, virtual, 0, flags) != 0) {
		/*
		 * The PTE was already written, so the rollback needs the same
		 * barrier and invalidation the success path uses; otherwise a
		 * stale translation can survive the failed mapping.
		 */
		(void) hat_unmap_page(hat_ttbr1, virtual);
		hat_tlb_flush();
		return (-1);
	}
	hat_tlb_flush();
	return (0);
}

int
arm64_boot_hat_unmap(uint64_t virtual)
{
	uint64_t physical = 0;
	int tracked;

	tracked = hat_lookup_root(hat_ttbr1, virtual, &physical) == 0;
	if (hat_unmap_page(hat_ttbr1, virtual) != 0)
		return (-1);
	if (tracked)
		(void) arm64_page_mapping_remove(physical, virtual, 0);
	hat_tlb_flush();
	return (0);
}

int
arm64_boot_hat_lookup(uint64_t virtual, uint64_t *physical)
{
	return (hat_lookup_root(hat_ttbr1, virtual, physical));
}

int
arm64_boot_hat_as_create(struct arm64_boot_as *as, uint16_t asid)
{
	uint64_t *source, *root;
	uint64_t ttbr0;
	unsigned int i;

	if (as == 0 || asid == 0)
		return (-1);
	ttbr0 = read_sysreg(ttbr0_el1);
	source = (uint64_t *)(uintptr_t)(ttbr0 & PTE_ADDR);
	root = hat_new_table();
	if (source == 0 || root == 0)
		return (-1);
	for (i = 0; i != TABLE_SIZE / sizeof (*root); i++)
		root[i] = source[i];
	as->aba_root = (uint64_t)(uintptr_t)root;
	as->aba_asid = asid;
	return (0);
}

int
arm64_boot_hat_as_map(struct arm64_boot_as *as, uint64_t virtual,
    uint64_t physical, uint_t flags)
{
	if (as == 0)
		return (-1);
	if (hat_map_page(as->aba_root, virtual, physical,
	    flags | ARM64_HAT_USER, as->aba_asid) != 0)
		return (-1);
	if (arm64_page_mapping_add(physical, virtual, as->aba_asid,
	    flags) != 0) {
		/*
		 * The PTE was already written, so the rollback needs the same
		 * barrier and invalidation the success path uses; otherwise a
		 * stale translation can survive the failed mapping.
		 */
		(void) hat_unmap_page(as->aba_root, virtual);
		hat_tlb_flush();
		return (-1);
	}
	dsb(ishst);
	isb();
	return (0);
}

int
arm64_boot_hat_as_unmap(struct arm64_boot_as *as, uint64_t virtual)
{
	uint64_t physical = 0;
	int tracked;

	if (as == 0)
		return (-1);
	tracked = hat_lookup_root(as->aba_root, virtual, &physical) == 0;
	if (hat_unmap_page(as->aba_root, virtual) != 0)
		return (-1);
	if (tracked)
		(void) arm64_page_mapping_remove(physical, virtual,
		    as->aba_asid);
	return (0);
}

int
arm64_boot_hat_as_lookup(struct arm64_boot_as *as, uint64_t virtual,
    uint64_t *physical)
{
	if (as == 0)
		return (-1);
	return (hat_lookup_root(as->aba_root, virtual, physical));
}

/*
 * Install an address space on TTBR0 and return the previous TTBR0 value so the
 * caller can restore it.  A null address space returns 0, matching the failure
 * convention of the sibling arm64_boot_hat_as_* entry points.
 */
uint64_t
arm64_boot_hat_as_switch(struct arm64_boot_as *as)
{
	uint64_t old, next;

	if (as == 0)
		return (0);
	old = read_sysreg(ttbr0_el1);
	next = as->aba_root | ((uint64_t)as->aba_asid << 48);

	dsb(ishst);
	write_sysreg(ttbr0_el1, next);
	isb();
	return (old);
}

void
arm64_boot_hat_as_restore(uint64_t ttbr0)
{
	dsb(ishst);
	write_sysreg(ttbr0_el1, ttbr0);
	isb();
}

void
arm64_boot_hat_tlb_asid(uint16_t asid)
{
	uint64_t operand = (uint64_t)asid << 48;

	dsb(ishst);
	__asm__ __volatile__("tlbi aside1is, %0" :: "r" (operand));
	dsb(ish);
	isb();
}

uint64_t
arm64_boot_phys_to_kva(uint64_t physical)
{
	return (KERNELBASE + (physical - hat_ram_base));
}

uint64_t
arm64_boot_hat_root(void)
{
	return (hat_ttbr1);
}

__attribute__((noinline)) void
arm64_boot_high_probe(void)
{
	pl011_puts("bootstrap: TTBR1 high alias HAT_HIGH_OK\n");
}
