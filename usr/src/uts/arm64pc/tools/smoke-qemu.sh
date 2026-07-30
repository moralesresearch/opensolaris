#!/bin/sh

set -eu

kernel=${1:-../../../../build/arm64pc/opensolaris-aarch64.elf}
qemu=${QEMU_SYSTEM_AARCH64:-qemu-system-aarch64}
machine=${QEMU_MACHINE:-virt,gic-version=3,virtualization=on}
smp=${QEMU_SMP:-1}
log=$(mktemp "${TMPDIR:-/tmp}/opensolaris-arm64.XXXXXX")
pid=

cleanup()
{
	if [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null; then
		kill "$pid" 2>/dev/null || true
		wait "$pid" 2>/dev/null || true
	fi
	rm -f "$log"
}
trap cleanup EXIT HUP INT TERM

"$qemu" \
	-machine "$machine" \
	-cpu cortex-a57 \
	-smp "$smp" \
	-m 512M \
	-nographic \
	-monitor none \
	-serial stdio \
	-no-reboot \
	-kernel "$kernel" >"$log" 2>&1 &
pid=$!

i=0
while [ "$i" -lt 100 ]; do
	if grep -q 'OpenSolaris/AArch64 BOOT_OK' "$log"; then
		for marker in CPU_ABI_OK FDT_OK AUTOVECT_OK HAT_HIGH_OK \
		    PAGE_INIT_OK PAGE_HAT_OK PAGE_OK \
		    SMP_OK SMP_CPU_ABI_OK IPI_OK XCALL_OK LOCK_OK SPL_OK \
		    USER_HAT_OK SYSCALL_OK \
		    PAGEFAULT_OK \
		    CONTEXT_OK SWITCH_OK \
		    KTHREAD_OK \
		    DISP_RESUME_OK \
		    RELOC_OK COMMON_OK IRQ_OK; do
			if ! grep -q "$marker" "$log"; then
				cat "$log" >&2
				echo "AArch64 QEMU smoke test missed $marker" >&2
				exit 1
			fi
		done
		cat "$log"
		exit 0
	fi
	if ! kill -0 "$pid" 2>/dev/null; then
		break
	fi
	i=$((i + 1))
	sleep 0.1
done

cat "$log" >&2
echo "AArch64 QEMU smoke test did not reach BOOT_OK" >&2
exit 1
