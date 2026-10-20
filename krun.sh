#!/usr/bin/env bash
#
# krun.sh — compile, link and run a Komodo program with the NATIVE
#           compiler (`./kmc`), no interpreter involved.
#
#   file.km ──kmc──▶ file.km.asm ──nasm──▶ .o ──gcc + stdlib/*.c──▶ exe ──▶ run
#
# Usage:  ./krun.sh <file.km> [output-name] [extra cc inputs...]
#           output-name   executable path (default: <file> without .km)
#           extra inputs  passed to gcc at the link step — .c / .o / .a
#                         files, -l flags, -I/-L paths.  Also read from
#                         $KM_CC_INPUTS (word-split).  stdlib/*.c is
#                         always linked.
#
# Example:  ./krun.sh demos/sdl_window.km ./sdl_window -lSDL2
#
# Uses the `kmc` already present next to this script (build it first with
# `make kmc`).  It must sit beside stdlib/ so it can locate the standard-
# library modules — the repo root, where `make kmc` puts it.  Point $KMC
# at a kmc elsewhere to override.
#
# $KM_OPT selects the optimisation level (none | low | medium | max).
#
set -euo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

src="${1:?usage: krun.sh <file.km> [output-name] [extra cc inputs...]}"
out="${2:-${src%.km}}"
shift $(( $# < 2 ? $# : 2 ))
extra_cc=( ${KM_CC_INPUTS:-} "$@" )

asm="$src.asm"
obj="$src.o"
kmc="${KMC:-$here/kmc}"

[[ -f "$src" ]] || { echo "krun: no such file: $src" >&2; exit 1; }
[[ -x "$kmc" ]] || { echo "krun: no native compiler at $kmc — build it with 'make kmc' (or set \$KMC)" >&2; exit 1; }

# 1. Komodo front end + codegen  →  NASM assembly (written next to <src>)
echo "compiling $src  (native kmc${KM_OPT:+, -O$KM_OPT})"
"$kmc" "$src" x86 ${KM_OPT:-}

# 2. assemble
nasm -f elf64 "$asm" -o "$obj"

# 3. link: the Komodo object + the C runtime + any user-supplied inputs
gcc -no-pie -I"$here/stdlib" -o "$out" "$obj" "$here"/stdlib/*.c \
    ${extra_cc[@]+"${extra_cc[@]}"} -lm -l:libgc.so.1
echo "built $out"

# 4. run
echo "── running $out ──"
set +e
case "$out" in */*|/*) "$out" ;; *) "./$out" ;; esac
status=$?
set -e
echo "── exit $status ──"
exit $status
