#!/usr/bin/env bash
#
# run-sema-tests.sh — exercise the semantic analyser's diagnostics.
#
#   tests/sema/<name>.km          compiled in DEFAULT mode; stderr must
#                                 contain tests/sema/<name>.experr
#   tests/sema/strict/<name>.km   compiled in STRICT mode; stderr must
#                                 contain the .experr — and the SAME file
#                                 compiled in default mode must succeed
#                                 (proves the lint is strict-only)
#
# Usage:  ./run-sema-tests.sh
#
set -uo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$here"

find_interp() {
    if [[ -n "${KOMODO_MAIN:-}" && -f "$KOMODO_MAIN" ]]; then echo "$KOMODO_MAIN"; return 0; fi
    local c
    for c in \
        "$here/../Python/KomodoPy/main.py" \
        "$HOME/Documents/Developer Projects/Python/KomodoPy/main.py" \
        "$here/../Python/Komodo/main.py"
    do
        [[ -f "$c" ]] && { echo "$c"; return 0; }
    done
    return 1
}
interp="$(find_interp)" || { echo "error: interpreter not found — set \$KOMODO_MAIN" >&2; exit 1; }

rm -rf "${HOME}/.km/cache"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

pass=0 fail=0

# returns 0 and prints stderr if compilation FAILED (as these tests want)
compile_expect_err() {  # <src> <mode>
    local src="$1" mode="${2:-}"
    python3 "$interp" "$here/main.km" "$src" x86 $mode 2>"$work/err" 1>/dev/null
    cat "$work/err"
}

check() {  # <label> <experr-file> <actual-output>
    local label="$1" expf="$2" got="$3"
    local want; want="$(cat "$expf")"
    if grep -qF -- "$want" <<<"$got"; then
        echo "ok       $label"; pass=$((pass+1))
    else
        echo "FAIL     $label"
        echo "           want substring: $want"
        printf '%s\n' "$got" | sed 's/^/           got:  /'
        fail=$((fail+1))
    fi
}

for src in tests/sema/*.km; do
    [[ -e "$src" ]] || continue
    name="$(basename "$src" .km)"
    check "$name" "tests/sema/$name.experr" "$(compile_expect_err "$src")"
done

for src in tests/sema/strict/*.km; do
    [[ -e "$src" ]] || continue
    name="strict/$(basename "$src" .km)"
    base="$(basename "$src" .km)"
    check "$name" "tests/sema/strict/$base.experr" "$(compile_expect_err "$src" strict)"
    # and it must compile clean without --strict
    if python3 "$interp" "$here/main.km" "$src" x86 >/dev/null 2>"$work/e2"; then
        echo "ok       $name (clean in default mode)"; pass=$((pass+1))
    else
        echo "FAIL     $name (should compile in default mode)"
        sed 's/^/           /' "$work/e2"; fail=$((fail+1))
    fi
done

echo "────────────────────────────────"
echo "$pass passed, $fail failed"
[[ $fail -eq 0 ]]
