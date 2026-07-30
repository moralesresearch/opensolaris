/*
 * Copyright 2026 OpenSolaris AArch64 Contributors.
 * FDT/UEFI boot configuration contract used by common startup code.
 */

#ifndef _SYS_BOOTCONF_H
#define	_SYS_BOOTCONF_H

#include <sys/types.h>
#include <sys/memlist.h>
#include <net/if.h>

#ifdef __cplusplus
extern "C" {
#endif

#define	BO_NO_ALIGN	0x1000
#define	BOOT_SUCCESS	0
#define	BOOT_FAILURE	(-1)
#define	BO_VERSION	10

struct bsys_mem {
	struct memlist *physinstalled;
	struct memlist *rsvdmem;
	struct memlist *physavail;
	struct memlist *virtavail;
	struct memlist *pcimem;
	uint_t extent;
};

typedef struct bootops {
	uint_t bsys_version;
	struct bsys_mem *boot_mem;
	caddr_t (*bsys_alloc)(struct bootops *, caddr_t, size_t, int);
	void (*bsys_free)(struct bootops *, caddr_t, size_t);
	int (*bsys_getproplen)(struct bootops *, const char *);
	int (*bsys_getprop)(struct bootops *, const char *, void *);
	char *(*bsys_nextprop)(struct bootops *, char *);
	void (*bsys_printf)(struct bootops *, const char *, ...);
	caddr_t (*bsys_ealloc)(struct bootops *, caddr_t, size_t, int, int);
} bootops_t;

#define	BOP_GETVERSION(bop)	((bop)->bsys_version)
#define	BOP_ALLOC(bop, va, sz, al) ((bop)->bsys_alloc((bop), (va), (sz), (al)))
#define	BOP_FREE(bop, va, sz)	((bop)->bsys_free((bop), (va), (sz)))
#define	BOP_GETPROPLEN(bop, n)	((bop)->bsys_getproplen((bop), (n)))
#define	BOP_GETPROP(bop, n, v)	((bop)->bsys_getprop((bop), (n), (v)))
#define	BOP_NEXTPROP(bop, n)	((bop)->bsys_nextprop((bop), (n)))
#define	BOP_PUTSARG(bop, m, a)	((bop)->bsys_printf((bop), (m), (a)))

#if defined(_KERNEL) && !defined(_BOOT)
#define	BO_MAXFSNAME	16
#define	BO_MAXOBJNAME	256

struct bootobj {
	char bo_fstype[BO_MAXFSNAME];
	char bo_name[BO_MAXOBJNAME];
	int bo_flags;
	int bo_size;
	struct vnode *bo_vp;
	char bo_devname[BO_MAXOBJNAME];
	char bo_ifname[BO_MAXOBJNAME];
	int bo_ppa;
};

#define	BO_VALID	0x01
#define	BO_BUSY		0x02

extern struct bootobj rootfs;
extern struct bootobj swapfile;
extern struct bootops *bootops;
extern char obp_bootpath[BO_MAXOBJNAME];
extern char kern_bootargs[];
extern char kern_bootfile[];
extern char *kobj_module_path;
extern char *default_path;
extern int netboot;
extern int swaploaded;
extern int modrootloaded;

extern dev_t getrootdev(void);
extern int loadrootmodules(void);
extern int strplumb(void);
extern int strplumb_load(void);
extern char *strplumb_get_netdev_path(void);
extern void consconfig(void);
extern void release_bootstrap(void);
extern void param_check(void);
extern void bop_no_more_mem(void);
extern void bop_printf(struct bootops *, const char *, ...)
    __KPRINTFLIKE(2);
extern void bop_panic(const char *, ...)
    __KPRINTFLIKE(1) __NORETURN;
extern void boot_prop_finish(void);
extern int bootprop_getval(const char *, u_longlong_t *);
#endif

#ifdef __cplusplus
}
#endif

#endif /* _SYS_BOOTCONF_H */
