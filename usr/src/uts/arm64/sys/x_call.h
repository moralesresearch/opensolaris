/* Copyright 2026 OpenSolaris AArch64 Contributors. */

#ifndef _SYS_X_CALL_H
#define	_SYS_X_CALL_H

#include <sys/types.h>

#if !defined(_ASM)
typedef uintptr_t xc_arg_t;
typedef int (*xc_func_t)(xc_arg_t, xc_arg_t, xc_arg_t);

typedef struct xc_data {
	xc_func_t xc_func;
	xc_arg_t xc_a1;
	xc_arg_t xc_a2;
	xc_arg_t xc_a3;
} xc_data_t;

typedef struct xc_msg {
	uint8_t xc_command;
	uint16_t xc_master;
	uint16_t xc_slave;
	struct xc_msg *xc_next;
} xc_msg_t;

#if defined(_KERNEL)
struct cpu;
extern void xc_init_cpu(struct cpu *);
extern uint_t xc_serv(caddr_t, caddr_t);
#define	CPUSET2BV(set)	((ulong_t *)(void *)&(set))
extern void xc_call(xc_arg_t, xc_arg_t, xc_arg_t, ulong_t *, xc_func_t);
extern void xc_call_nowait(xc_arg_t, xc_arg_t, xc_arg_t, ulong_t *,
    xc_func_t);
extern void xc_sync(xc_arg_t, xc_arg_t, xc_arg_t, ulong_t *, xc_func_t);
extern void xc_priority(xc_arg_t, xc_arg_t, xc_arg_t, ulong_t *, xc_func_t);
#endif
#endif

#endif /* _SYS_X_CALL_H */
