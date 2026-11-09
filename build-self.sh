#!/usr/bin/env bash
#
# build-self.sh — build the Komodo compiler with itself.
#
#   main.km (+ every modules/**/*.kmm it loads)  ──bootstrap──▶  main.km.asm
#     ──nasm──▶ .o ──gcc + stdlib/*.c──▶  ./kmc
#     ──kmc recompiles main.km──▶  byte-identical main.km.asm   (fixpoint)
#
# You rarely run this by hand: `komodo` calls it on demand (first run, or
# when the compiler sources are newer than ./kmc), and `komodo self build`
# is the explicit form.  There is no Makefile.
#
#   ./build-self.sh               build ./kmc (no-op when nothing changed)
#   ./build-self.sh --force       rebuild even if the sources are unchanged
#   ./build-self.sh --fast        skip the fixpoint re-check (~0.5s faster)
#   ./build-self.sh --update-seed refresh bootstrap/main.km.asm.gz afterwards
#
# Bootstrap compiler, in order of preference:
#   1. $KMC          — a prebuilt kmc binary
#   2. ./kmc         — one already built here
#   3. the seed      — bootstrap/main.km.asm.gz, a checked-in assembly of
#                      the compiler: needs only nasm + gcc, no interpreter
#   4. interpreter   — $KOMODO_MAIN, else a couple of well-known locations
#                      (slow, ~2 min; runs under PyPy when available)
#
# Incremental: a hash of the compiler sources, the C runtime and this
# script is stamped in .build/; if it matches and ./kmc exists there is
# nothing to do.  The C runtime (stdlib/*.c) is compiled to .build/obj/
# in parallel, once per content hash.
#
# The fixpoint check recompiles main.km with the fresh kmc.  If the
# assembly differs (the sources changed how the compiler compiles
# itself) it goes one stage further: a build is at its fixpoint when two
# consecutive stages agree.
#
set -euo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
asm="$here/main.km.asm"
bdir="$here/.build"
seed="$here/bootstrap/main.km.asm.gz"

force=0 fast=0 update_seed=0
for a in "$@"; do
    case "$a" in
        --force) force=1 ;;
        --fast) fast=1 ;;
        --update-seed) update_seed=1 ;;
        -h|--help) sed -n 2,35p "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "build-self.sh: unknown option '$a'" >&2; exit 2 ;;
    esac
done

sources_hash() {
    ( cd "$here"
      cat build-self.sh tools/asm-split.awk main.km stdlib/*.c stdlib/*.h 2>/dev/null
      find modules -name '*.kmm' | LC_ALL=C sort | xargs cat ) | sha256sum | cut -d' ' -f1
}
stamp="$(sources_hash)"
if [[ $force -eq 0 && $update_seed -eq 0 && -x "$here/kmc" && -f "$bdir/stamp" \
      && "$(cat "$bdir/stamp")" == "$stamp" ]]; then
    echo "kmc is up to date (use --force to rebuild)"
    exit 0
fi
mkdir -p "$bdir/obj"

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

# stdlib/*.c → .build/obj/<name>-<hash>.o, in parallel, cached by content
build_runtime() {
    local jobs; jobs="$(nproc 2>/dev/null || echo 4)"
    local todo=() c h o
    objs=()
    for c in "$here"/stdlib/*.c; do
        h="$(cat "$c" "$here"/stdlib/*.h | sha256sum | cut -c1-12)"
        o="$bdir/obj/$(basename "$c" .c)-$h.o"
        objs+=("$o")
        [[ -f "$o" ]] || todo+=("$c|$o")
    done
    # drop objects no current source hashes to
    find "$bdir/obj" -name '*.o' | while read -r f; do
        keep=0; for o in "${objs[@]}"; do [[ "$o" == "$f" ]] && keep=1; done
        [[ $keep -eq 1 ]] || rm -f "$f"
    done
    if [[ ${#todo[@]} -gt 0 ]]; then
        printf '%s\n' "${todo[@]}" | xargs -P "$jobs" -I{} bash -c \
            'in="{}"; gcc -no-pie -I"'"$here"'/stdlib" -c "${in%%|*}" -o "${in##*|}"'
    fi
}

# asm → objects: the assembly is cut at function boundaries (tools/asm-split.awk),
# chunks are assembled in parallel, and each object is cached by the hash of
# its body — editing one function reassembles only the chunk it lives in.
# Sets kobjs=( … ) for the link line.
assemble() {
    local in="$1" d="$bdir/split" jobs; jobs="$(nproc 2>/dev/null || echo 4)"
    rm -rf "$d"; mkdir -p "$d" "$bdir/asm"
    awk -v outdir="$d" -v K=16 -f "$here/tools/asm-split.awk" "$in" >/dev/null
    local nv; nv="$(nasm -v | head -1 | sha256sum | cut -c1-6)"
    local todo=() f key o
    kobjs=()
    while read -r key f; do
        o="$bdir/asm/${key:0:20}$nv.o"
        kobjs+=("$o")
        if [[ -f "$o" ]]; then touch "$o"; else todo+=("${f%.body}.asm|$o"); fi
    done < <(sha256sum "$d"/c_*.body)
    if [[ ${#todo[@]} -gt 0 ]]; then
        printf '%s\n' "${todo[@]}" | xargs -P "$jobs" -I{} bash -c \
            'in="{}"; nasm -f elf64 "${in%%|*}" -o "${in##*|}"'
    fi
    find "$bdir/asm" -name '*.o' -mtime +7 -delete 2>/dev/null || true
}

# bootstrap stage: produce main.km.asm from whatever compiler we have
if [[ -n "${KMC:-}" && -x "${KMC:-}" ]]; then
    echo "bootstrap: \$KMC ($KMC)";  "$KMC" "$here/main.km" x86
elif [[ -x "$here/kmc" ]]; then
    echo "bootstrap: ./kmc";         "$here/kmc" "$here/main.km" x86
elif [[ -f "$seed" ]]; then
    echo "bootstrap: seed ($seed)"
    gzip -dc "$seed" > "$asm"
    assemble "$asm"
    build_runtime
    gcc -no-pie -o "$bdir/kmc.seed" "${kobjs[@]}" "${objs[@]}" -lm -l:libgc.so.1
    "$bdir/kmc.seed" "$here/main.km" x86
    rm -f "$bdir/kmc.seed"
else
    run_interp
fi
[[ -f "$asm" ]] || { echo "bootstrap produced no $asm" >&2; exit 1; }

build_runtime
link_kmc() {
    assemble "$asm"
    gcc -no-pie -o "$here/kmc" "${kobjs[@]}" "${objs[@]}" -lm -l:libgc.so.1
}
link_kmc
echo "built $here/kmc"

if [[ $fast -eq 0 ]]; then
    # fixpoint — the fresh kmc must reproduce its own assembly byte-for-byte
    prev="$bdir/stage.asm"
    cp "$asm" "$prev"
    "$here/kmc" "$here/main.km" x86 >/dev/null
    if ! cmp -s "$prev" "$asm"; then
        echo "stage 2 differs from the bootstrap output; checking one stage further"
        cp "$asm" "$prev"
        link_kmc
        "$here/kmc" "$here/main.km" x86 >/dev/null
        if ! cmp -s "$prev" "$asm"; then
            echo "fixpoint FAILED — two consecutive stages still differ" >&2
            exit 1
        fi
    fi
    rm -f "$prev"
    echo "fixpoint OK — kmc compiles itself to identical assembly"
fi
rm -rf "$bdir/split"

if [[ $update_seed -eq 1 ]]; then
    mkdir -p "$here/bootstrap"
    gzip -9 -n -c "$asm" > "$seed"
    echo "wrote $seed ($(stat -c %s "$seed") bytes) — commit it with the source change"
fi
echo "$stamp" > "$bdir/stamp"
