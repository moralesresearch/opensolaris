/* Copyright 2026 OpenSolaris AArch64 Contributors. */

#ifndef _SYS_OBPDEFS_H
#define	_SYS_OBPDEFS_H

/* Compatibility names used by the common devinfo tree over an FDT backend. */
typedef int ihandle_t;
typedef int phandle_t;
typedef phandle_t pnode_t;

#define	OBP_NONODE	((pnode_t)0)
#define	OBP_BADNODE	((pnode_t)-1)

#define	OBP_NAME	"name"
#define	OBP_REG		"reg"
#define	OBP_INTR	"intr"
#define	OBP_RANGES	"ranges"
#define	OBP_INTERRUPTS	"interrupts"
#define	OBP_COMPATIBLE	"compatible"
#define	OBP_STATUS	"status"
#define	OBP_BOARDNUM	"board#"
#define	OBP_MAC_ADDR	"mac-address"
#define	OBP_STDINPATH	"stdin-path"
#define	OBP_STDOUTPATH	"stdout-path"
#define	OBP_IDPROM	"idprom"
#define	OBP_DEVICETYPE	"device_type"
#define	OBP_DISPLAY	"display"
#define	OBP_NETWORK	"network"
#define	OBP_BYTE	"byte"
#define	OBP_BLOCK	"block"
#define	OBP_SERIAL	"serial"
#define	OBP_HIERARCHICAL "hierarchical"
#define	OBP_CPU		"cpu"
#define	OBP_ADDRESS	"address"

#define	OBP_ST_OKAY	"okay"
#define	OBP_ST_DISABLED	"disabled"
#define	OBP_ST_FAIL	"fail"

#define	OBP_MAXDRVNAME	32
#define	OBP_MAXPROPNAME	32
#define	OBP_MAXPATHLEN	256
#define	OBP_STACKDEPTH	(OBP_MAXPATHLEN / 3)

#endif /* _SYS_OBPDEFS_H */
