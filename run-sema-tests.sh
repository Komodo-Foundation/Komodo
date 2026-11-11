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
#   tests/sema/lint/<name>.km     analysed with kmc's `lint` target; it must
#                                 succeed and print exactly <name>.expout
#                                 (paths relative to tests/sema/lint)
#
# Uses the native self-hosted compiler (kmc); build one with
# ./build-self.sh.  Set $KMC to override.
#
# Usage:  ./run-sema-tests.sh [name ...]
#
set -uo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$here"

kmc="${KMC:-$here/kmc}"
[[ -x "$kmc" ]] || { echo "error: no kmc at $kmc — run ./build-self.sh first" >&2; exit 1; }

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

pass=0 fail=0

# prints kmc's stderr; returns 0 iff compilation FAILED (what these want)
compile_expect_err() {  # <src> [strict]
    local src="$1" mode="${2:-}"
    if "$kmc" "$src" x86 $mode >/dev/null 2>"$work/err"; then
        cat "$work/err"; return 1
    fi
    cat "$work/err"; return 0
}

check() {  # <label> <experr-file> <compile-rc> <actual-output>
    local label="$1" expf="$2" rc="$3" got="$4"
    local want; want="$(cat "$expf")"
    if [[ "$rc" -ne 0 ]]; then
        echo "FAIL     $label (compiled clean, expected an error)"
        fail=$((fail+1)); return
    fi
    if grep -qF -- "$want" <<<"$got"; then
        echo "ok       $label"; pass=$((pass+1))
    else
        echo "FAIL     $label"
        echo "           want substring: $want"
        printf '%s\n' "$got" | sed 's/^/           got:  /'
        fail=$((fail+1))
    fi
}

sel=("$@")
match() {  # <name> — true if no filter given or name matches one
    [[ ${#sel[@]} -eq 0 ]] && return 0
    local n; for n in "${sel[@]}"; do [[ "$1" == *"$n"* ]] && return 0; done
    return 1
}

for src in tests/sema/*.km; do
    [[ -e "$src" ]] || continue
    name="$(basename "$src" .km)"
    match "$name" || continue
    got="$(compile_expect_err "$src")"; rc=$?
    check "$name" "tests/sema/$name.experr" "$rc" "$got"
done

for src in tests/sema/strict/*.km; do
    [[ -e "$src" ]] || continue
    base="$(basename "$src" .km)"
    name="strict/$base"
    match "$name" || continue
    got="$(compile_expect_err "$src" strict)"; rc=$?
    check "$name" "tests/sema/strict/$base.experr" "$rc" "$got"
    # and it must compile clean without --strict
    if "$kmc" "$src" x86 >/dev/null 2>"$work/e2"; then
        echo "ok       $name (clean in default mode)"; pass=$((pass+1))
    else
        echo "FAIL     $name (should compile in default mode)"
        sed 's/^/           /' "$work/e2"; fail=$((fail+1))
    fi
done

for src in tests/sema/lint/*.km; do
    [[ -e "$src" ]] || continue
    base="$(basename "$src" .km)"
    name="lint/$base"
    match "$name" || continue
    if got="$("$kmc" "$src" lint 2>&1 | sed 's#^tests/sema/lint/##')" && \
       [[ "$got" == "$(cat "tests/sema/lint/$base.expout")" ]]; then
        echo "ok       $name"; pass=$((pass+1))
    else
        echo "FAIL     $name"
        diff <(printf '%s\n' "$got") "tests/sema/lint/$base.expout" | sed 's/^/           /'
        fail=$((fail+1))
    fi
done

echo "────────────────────────────────"
echo "$pass passed, $fail failed"
[[ $fail -eq 0 ]]
