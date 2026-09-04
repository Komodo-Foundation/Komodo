#!/usr/bin/env bash
#
# build-self.sh — build the Komodo compiler with itself.
#
#   modules/*.kmm + main.km  ──concat──▶  <out>/kmc_unit.km
#     ──bootstrap──▶ .asm ──nasm──▶ .o ──gcc + stdlib/*.c──▶ <out>/kmc
#     ──kmc recompiles the same unit──▶ byte-identical .asm   (fixpoint)
#
# Bootstrap compiler, in order of preference:
#   1. $KMC        — a prebuilt kmc binary
#   2. <out>/kmc   — one already built by a previous run
#   3. interpreter — $KOMODO_MAIN, else a couple of well-known locations
#
# Usage:  ./build-self.sh [out-dir]      (default: this directory)
#
set -euo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
out="${1:-$here}"
mkdir -p "$out"
unit="$out/kmc_unit.km"
asm="$unit.asm"

# 1. one translation unit — every compiler module then the driver, with
#    each file's `module` / `import` lines dropped (already merged).
mods=(ast types lexer parser sema sema_expr sema_stmt
      backend backend_x86 backend_stackvm
      codegen codegen_frame codegen_stmt codegen_expr km_compiler)
: > "$unit"
for m in "${mods[@]}"; do
    grep -v '^module \|^import ' "$here/modules/$m.kmm" >> "$unit"
    echo >> "$unit"
done
grep -v '^module \|^import ' "$here/main.km" >> "$unit"
echo "unit: $(wc -l < "$unit") lines"

# 2. bootstrap: produce $asm from $unit
run_interp() {
    local interp="${KOMODO_MAIN:-}"
    if [[ -z "$interp" || ! -f "$interp" ]]; then
        for c in "$here/../Python/KomodoPy/main.py" \
                 "$HOME/Documents/Developer Projects/Python/KomodoPy/main.py"; do
            [[ -f "$c" ]] && interp="$c" && break
        done
    fi
    [[ -f "$interp" ]] || { echo "no bootstrap compiler — set \$KMC or \$KOMODO_MAIN" >&2; exit 1; }
    echo "bootstrap: interpreter ($interp)"
    rm -rf "$HOME/.km/cache"
    ( cd "$(dirname "$interp")" && python3 -c \
      "import sys; sys.setrecursionlimit(200000); \
sys.argv=['main.py','$here/main.km','$unit','x86']; exec(open('main.py').read())" )
}

if [[ -n "${KMC:-}" && -x "${KMC:-}" ]]; then
    echo "bootstrap: \$KMC ($KMC)";  "$KMC" "$unit" x86
elif [[ -x "$out/kmc" ]]; then
    echo "bootstrap: $out/kmc";      "$out/kmc" "$unit" x86
else
    run_interp
fi
[[ -f "$asm" ]] || { echo "bootstrap produced no $asm" >&2; exit 1; }

# 3. assemble + link the C runtime
nasm -f elf64 "$asm" -o "$unit.o"
gcc -no-pie -I"$here/stdlib" -o "$out/kmc" "$unit.o" "$here"/stdlib/*.c -lm
echo "built $out/kmc"

# 4. fixpoint — the fresh kmc must reproduce its own assembly byte-for-byte
cp "$asm" "$out/kmc_stage.asm"
"$out/kmc" "$unit" x86
if diff -q "$out/kmc_stage.asm" "$asm" >/dev/null; then
    echo "fixpoint OK — kmc compiles itself to identical assembly"
else
    echo "fixpoint FAILED — stage-2 and stage-3 assembly differ" >&2
    exit 1
fi
