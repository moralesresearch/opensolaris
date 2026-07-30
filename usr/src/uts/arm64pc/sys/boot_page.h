/* Copyright 2026 OpenSolaris AArch64 Contributors. */

#ifndef _SYS_ARM64PC_BOOT_PAGE_H
#define	_SYS_ARM64PC_BOOT_PAGE_H

#include <sys/types.h>
#include <sys/fdt.h>

/*
 * Boot-time physical page management backed by the common page_t.  RAM
 * above a small boot-allocator reserve is described by a real page_t
 * array and served from a freelist, so the page-table engine draws its
 * translation tables from page_t allocation instead of bootmem, keeps a
 * per-page HAT mapping list and maintains common page attributes.
 */

struct page;

struct arm64_hment {
	struct arm64_hment *hm_next;
	uint64_t	hm_va;
	uint16_t	hm_asid;
};

extern int arm64_page_init(const struct arm64_platform *, uint64_t,
    uint64_t);
extern int arm64_page_initialized(void);
extern struct page *arm64_page_alloc(void);
extern int arm64_page_free(struct page *);
extern uint64_t arm64_page_pa(const struct page *);
extern uint64_t arm64_page_total(void);
extern uint64_t arm64_page_freecnt(void);
extern int arm64_page_mapping_add(uint64_t, uint64_t, uint16_t, uint_t);
extern int arm64_page_mapping_remove(uint64_t, uint64_t, uint16_t);
extern struct arm64_hment *arm64_page_mapping_find(uint64_t, uint64_t,
    uint16_t);

#endif /* _SYS_ARM64PC_BOOT_PAGE_H */
