#!/bin/sh

set -eu

input=$1
output=$2

sed -n 's|^[[:space:]]*// GENASSYM | |p' "$input" >"$output.tmp"
{
	echo '/* Generated from the AArch64 C structure layout. */'
	echo '#ifndef _ARM64_ASSYM_H'
	echo '#define _ARM64_ASSYM_H'
	sed 's/^ //' "$output.tmp"
	echo '#endif'
} >"$output"
rm -f "$output.tmp"
