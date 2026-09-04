#!/usr/bin/env bash
#
# build.sh — compile a Komodo source file and run it.
#
#   x86 target (default):
#     Komodo source ──▶ NASM (.asm) ──nasm──▶ .o ──gcc──▶ executable ──▶ run
#                                                    └─ + stdlib/*.c
#
#   stackvm target:
#     Komodo source ──▶ stack-VM bytecode (.asm)   [printed; no runner yet]
#
# Usage:  ./build.sh <file.km> [target] [output-name] [extra cc inputs...]
#           target       x86 (default) | stackvm
#           output-name  executable path (x86 only; default: <file> w/o .km)
#           extra inputs any further args are handed to gcc at the link
#                        step — .c / .o / .a files, -l flags, -I/-L paths.
#                        The same can be supplied via $KM_CC_INPUTS (word-
#                        split).  stdlib/*.c is always linked in.
#
# Example:  ./build.sh app.km x86 app  vendor/foo.c bar.o -lssl
#
# The Komodo interpreter that runs the (self-hosted) compiler is found via
# $KOMODO_MAIN, else a few well-known locations.
#
KOMODO_MAIN="${KOMODO_MAIN:-kmpy}"

set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

src="${1:?usage: build.sh <file.km> [target] [output-name] [extra cc inputs...]}"
target="${2:-x86}"
out="${3:-${src%.km}}"
shift $(( $# < 3 ? $# : 3 ))
# Anything left on the command line, plus $KM_CC_INPUTS, is extra link input.
extra_cc=( ${KM_CC_INPUTS:-} "$@" )
asm="$src.asm"
obj="$src.o"

# ── locate the interpreter (runs modules/*.kmm) ──────────────────────
find_interp() {
    if [[ -n "${KOMODO_MAIN:-}" && -f "$KOMODO_MAIN" ]]; then
        echo "$KOMODO_MAIN"; return 0
    fi
    local c
    for c in \
        "$here/../Python/KomodoPy/main.py" \
        "$HOME/Documents/Developer Projects/Python/KomodoPy/main.py" \
        "$here/../Python/Komodo/main.py" \
        "$HOME/Documents/Developer Projects/Python/Komodo/main.py" \
        "$here/main.py"
    do
        [[ -f "$c" ]] && { echo "$c"; return 0; }
    done
    return 1
}

interp="$(find_interp)" || {
    echo "error: Komodo interpreter (main.py) not found — set \$KOMODO_MAIN" >&2
    exit 1
}

# The interpreter memoises parsed ASTs; a stale entry survives edits to the
# compiler's own modules, so clear it for a reproducible build.
rm -rf "${HOME}/.km/cache"

# 1. Komodo front-end + codegen  →  target text (always written to <src>.asm)
# $KM_OPT (none|low|medium|max) selects the optimiser level.
echo "compiling $src  (target: $target${KM_OPT:+, -O$KM_OPT})"
python3 "$interp" "$here/main.km" "$src" "$target" ${KM_OPT:-}

if [[ "$target" != "x86" ]]; then
    echo "built $asm  (target '$target' has no runner yet)"
    exit 0
fi

# 2. Assemble with NASM
nasm -f elf64 "$asm" -o "$obj"

# 3. Link: the Komodo object + stdlib/*.c + any user-supplied C / objects.
gcc -no-pie -I"$here/stdlib" -o "$out" "$obj" "$here"/stdlib/*.c ${extra_cc[@]+"${extra_cc[@]}"} -lm
echo "built $out"

# 4. Run it
echo "── running $out ──"
"$out"
status=$?
echo "── exit $status ──"
exit $status
