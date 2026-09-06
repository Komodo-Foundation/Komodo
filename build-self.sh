#!/usr/bin/env bash
#
# build-self.sh — build the Komodo compiler with itself.
#
#   main.km (+ every modules/**/*.kmm it loads)  ──bootstrap──▶  main.km.asm
#     ──nasm──▶ .o ──gcc + stdlib/*.c──▶  ./kmc
#     ──kmc recompiles main.km──▶  byte-identical main.km.asm   (fixpoint)
#
# The driver (main.km) does module loading itself — resolve `import`,
# then merge every sibling file of each module — so there is no
# concatenated unit file.  Equivalent to `make bootstrap && make fixpoint`.
#
# Bootstrap compiler, in order of preference:
#   1. $KMC        — a prebuilt kmc binary
#   2. ./kmc       — one already built here
#   3. interpreter — $KOMODO_MAIN, else a couple of well-known locations
#
# The interpreter bootstrap is the slow step (~2 min).  It runs under
# PyPy when available (roughly 5-15x faster), else CPython; force one
# with $KM_PY (e.g. KM_PY=python3).
#
set -euo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
asm="$here/main.km.asm"

pick_py() {
    if [[ -n "${KM_PY:-}" ]]; then echo "$KM_PY"; return; fi
    for p in pypy3 pypy python3 python; do
        command -v "$p" >/dev/null 2>&1 && { echo "$p"; return; }
    done
    echo python3
}

run_interp() {
    local interp="${KOMODO_MAIN:-}"
    if [[ -z "$interp" || ! -f "$interp" ]]; then
        for c in "$here/../Python/KomodoPy/main.py" \
                 "$HOME/Documents/Developer Projects/Python/KomodoPy/main.py"; do
            [[ -f "$c" ]] && interp="$c" && break
        done
    fi
    [[ -f "$interp" ]] || { echo "no bootstrap compiler — set \$KMC or \$KOMODO_MAIN" >&2; exit 1; }
    local py; py="$(pick_py)"
    echo "bootstrap: interpreter ($interp) via $py"
    rm -rf "$HOME/.km/cache"
    ( cd "$(dirname "$interp")" && "$py" -c \
      "import sys; sys.setrecursionlimit(200000); \
sys.argv=['main.py','$here/main.km','$here/main.km','x86']; exec(open('main.py').read())" )
}

if [[ -n "${KMC:-}" && -x "${KMC:-}" ]]; then
    echo "bootstrap: \$KMC ($KMC)";  "$KMC" "$here/main.km" x86
elif [[ -x "$here/kmc" ]]; then
    echo "bootstrap: ./kmc";         "$here/kmc" "$here/main.km" x86
else
    run_interp
fi
[[ -f "$asm" ]] || { echo "bootstrap produced no $asm" >&2; exit 1; }

nasm -f elf64 "$asm" -o "$here/build.kmc.o"
gcc -no-pie -I"$here/stdlib" -o "$here/kmc" "$here/build.kmc.o" "$here"/stdlib/*.c -lm
rm -f "$here/build.kmc.o"
echo "built $here/kmc"

# fixpoint — the fresh kmc must reproduce that assembly byte-for-byte
cp "$asm" "$here/main.km.stage.asm"
"$here/kmc" "$here/main.km" x86
if diff -q "$here/main.km.stage.asm" "$asm" >/dev/null; then
    echo "fixpoint OK — kmc compiles itself to identical assembly"
    rm -f "$here/main.km.stage.asm"
else
    echo "fixpoint FAILED — bootstrap and self-built assembly differ" >&2
    exit 1
fi
