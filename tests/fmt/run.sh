#!/usr/bin/env bash
#
# tests/fmt/run.sh — safety tests for tools/km-fmt.
#
#   1. golden:      formatting golden/input.km gives golden/expected.km;
#                   golden/mixed_indent.km is refused (exit 2).
#   2. safety:      every examples/*.km, stdlib/*.kmm, modules/**/*.kmm,
#                   main.km and tests/sema/*.km is compiled with `kmc <f> x86`
#                   twice, once as written and once formatted, in two copies
#                   of the tree. Exit status, output and the generated .asm
#                   must be byte-identical. For tests/sema the diagnostics must
#                   match once the column numbers are removed (layout never
#                   changes lines).
#   3. idempotence: formatting the formatted tree again changes nothing, and
#                   `km-fmt --check` on it passes.
#
# Usage:  tests/fmt/run.sh          (builds ./kmc first if missing)

set -uo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd "$here/../.." && pwd)"
fmt="$root/tools/km-fmt"
kmc="${KMC:-$root/kmc}"

cd "$root"
if [[ ! -x "$kmc" ]]; then
    ./build-self.sh >/dev/null 2>&1 || { echo "error: could not build $kmc" >&2; exit 1; }
fi

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

pass=0 fail=0
ok()   { pass=$((pass + 1)); }
bad()  { fail=$((fail + 1)); echo "FAIL: $*"; }

# ---------------------------------------------------------------- 1. golden
python3 "$fmt" "$here/golden/input.km" > "$work/golden.out" 2>"$work/golden.err"
if cmp -s "$work/golden.out" "$here/golden/expected.km" && [[ ! -s "$work/golden.err" ]]; then
    ok
else
    bad "golden: output differs from golden/expected.km"
    diff "$here/golden/expected.km" "$work/golden.out" | head -20
fi

python3 "$fmt" "$here/golden/expected.km" > "$work/golden2.out" 2>&1
if cmp -s "$work/golden2.out" "$here/golden/expected.km"; then ok; else
    bad "golden: expected.km is not a fixed point"; fi

python3 "$fmt" "$here/golden/mixed_indent.km" >/dev/null 2>"$work/mixed.err"
rc=$?
if [[ $rc -eq 2 ]] && grep -q "mixed tabs" "$work/mixed.err"; then ok
else bad "golden: mixed tab/space indentation was not refused (rc=$rc)"; fi

# ------------------------------------------------------ 2. safety + 3. idem
# Collect the files named by the spec, relative to the repo root.
list="$work/files.txt"
{
    find examples stdlib modules tests/sema -maxdepth 1 -type f \( -name '*.km' -o -name '*.kmm' \) 2>/dev/null
    find modules -mindepth 2 -type f -name '*.kmm' 2>/dev/null
    echo main.km
} | sort -u > "$list"

stage() {  # <dest>: copy the listed files into <dest>
    mkdir -p "$1"
    tar -cf - -T "$list" | tar -xf - -C "$1"
}
stage "$work/orig"
stage "$work/fmt"

python3 "$fmt" -w "$work/fmt/examples" "$work/fmt/stdlib" "$work/fmt/modules" \
    "$work/fmt/tests/sema" "$work/fmt/main.km" 2>"$work/fmt-w.err" || {
    bad "km-fmt -w failed on the staged tree"; cat "$work/fmt-w.err"; }

changed=0 with_asm=0 diag=0
while read -r f; do
    [[ -f "$work/fmt/$f" ]] || { bad "missing staged file $f"; continue; }
    if ! cmp -s "$work/orig/$f" "$work/fmt/$f"; then changed=$((changed + 1)); fi

    base="$(basename "$f")"
    dir="$(dirname "$f")"
    for side in orig fmt; do
        ( cd "$work/$side/$dir" && "$kmc" "$base" x86 >"$work/$side.out" 2>"$work/$side.err"
          echo $? >"$work/$side.rc" )
        cp -f "$work/$side/$dir/$base.asm" "$work/$side.asm" 2>/dev/null || : > "$work/$side.asm"
        rm -f "$work/$side/$dir/$base.asm" "$work/$side/$dir/$base.deps" \
              "$work/$side/$dir/$base.links" "$work/$side/$dir/$base.o"
    done

    [[ -s "$work/orig.asm" ]] && with_asm=$((with_asm + 1))
    if [[ "$f" == tests/sema/* ]]; then
        diag=$((diag + 1))
        # Diagnostics: same text, ignoring the column (layout may shift it).
        strip() { sed -E 's/^([^:]+:[0-9]+):[0-9]+:/\1:/' "$1"; }
        if [[ "$(cat "$work/orig.rc")" != "$(cat "$work/fmt.rc")" ]] ||
           ! cmp -s <(strip "$work/orig.err") <(strip "$work/fmt.err"); then
            bad "$f: diagnostics differ after formatting"
            diff "$work/orig.err" "$work/fmt.err" | head -6
        else ok; fi
    else
        if [[ "$(cat "$work/orig.rc")" != "$(cat "$work/fmt.rc")" ]] ||
           ! cmp -s "$work/orig.out" "$work/fmt.out" ||
           ! cmp -s "$work/orig.err" "$work/fmt.err" ||
           ! cmp -s "$work/orig.asm" "$work/fmt.asm"; then
            bad "$f: compiler output differs after formatting"
            diff "$work/orig.err" "$work/fmt.err" | head -6
        else ok; fi
    fi
done < "$list"

# Idempotence on the whole staged tree.
cp -r "$work/fmt" "$work/fmt2"
python3 "$fmt" -w "$work/fmt2/examples" "$work/fmt2/stdlib" "$work/fmt2/modules" \
    "$work/fmt2/tests/sema" "$work/fmt2/main.km" >/dev/null 2>&1
if diff -r "$work/fmt" "$work/fmt2" >/dev/null; then ok
else bad "idempotence: a second pass changed files"; diff -r "$work/fmt" "$work/fmt2" | head; fi

if python3 "$fmt" --check "$work/fmt/examples" "$work/fmt/stdlib" "$work/fmt/modules" \
        "$work/fmt/tests/sema" "$work/fmt/main.km" >/dev/null; then ok
else bad "idempotence: --check still reports changes on formatted tree"; fi

echo "files checked: $(wc -l < "$list"), would change before formatting: $changed"
echo "compiled to assembly: $with_asm, sema diagnostics compared: $diag"
echo "tests passed: $pass, failed: $fail"
[[ $fail -eq 0 ]]
