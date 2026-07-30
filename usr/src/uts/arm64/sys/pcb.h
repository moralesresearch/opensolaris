/* Copyright 2026 OpenSolaris AArch64 Contributors. */

#ifndef _SYS_PCB_H
#define	_SYS_PCB_H

#include <sys/types.h>
#include <sys/fp.h>

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(_ASM)
typedef struct fpu_ctx fpu_ctx_t;

typedef struct pcb {
	fpu_ctx_t pcb_fpu;
	uint_t pcb_flags;
	uint_t pcb_instr;
	int pcb_step;
	caddr_t pcb_tracepc;
} pcb_t;
#endif

#define	PRSTOP_CALLED	0x0001
#define	INSTR_VALID	0x0002
#define	NORMAL_STEP	0x0004
#define	WATCH_STEP	0x0008
#define	CPC_OVERFLOW	0x0010
#define	REQUEST_STEP	0x0020
#define	REQUEST_NOSTEP	0x0040
#define	ASYNC_HWERR	0x0080

#define	STEP_NONE	0
#define	STEP_REQUESTED	1
#define	STEP_ACTIVE	2
#define	STEP_WASACTIVE	3

#ifdef __cplusplus
}
#endif

#endif /* _SYS_PCB_H */
