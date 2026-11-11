#!/usr/bin/env bash
#
# scripts/build-bindings.sh -- build and run every bindings/<lib>/example.km.
#
# Each example starts with `// requires: <pkg-config name>`. If that package is
# not installed the example is skipped (not failed). Otherwise it is compiled
# and run with `komodo -q`, and its stdout must equal example.expected.
#
# Usage: scripts/build-bindings.sh [name ...]     (names are folder names)
set -uo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
komodo="${KOMODO:-$root/komodo}"

names=("$@")
if [[ ${#names[@]} -eq 0 ]]; then
    for d in "$root"/bindings/*/; do names+=("$(basename "$d")"); done
fi

pass=0 fail=0 skip=0
for name in "${names[@]}"; do
    dir="$root/bindings/$name"
    ex="$dir/example.km"
    [[ -f "$ex" ]] || { echo "FAIL     bindings/$name  (no example.km)"; fail=$((fail+1)); continue; }

    pkg="$(sed -n 's|^// requires: *||p' "$ex" | head -1)"
    if [[ -n "$pkg" ]] && ! pkg-config --exists "$pkg"; then
        echo "skip     bindings/$name  (pkg-config '$pkg' not installed)"
        skip=$((skip+1)); continue
    fi

    got="$("$komodo" -q "$ex" 2>&1)"
    if [[ $? -ne 0 ]] || ! diff -q <(printf '%s\n' "$got") "$dir/example.expected" >/dev/null; then
        echo "FAIL     bindings/$name"
        diff <(printf '%s\n' "$got") "$dir/example.expected" | sed 's/^/           /'
        fail=$((fail+1)); continue
    fi
    echo "ok       bindings/$name"; pass=$((pass+1))
done

echo "bindings: $pass passed, $fail failed, $skip skipped"
[[ $fail -eq 0 ]]
