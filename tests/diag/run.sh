#!/usr/bin/env bash
#
# tests/diag/run.sh — the diagnostics layer: catalog, rich layout, machine
# format, `komodo explain`, multiple errors.
#
#   tests/diag/run.sh            run everything (needs ./kmc: ./build-self.sh)
#   UPDATE=1 tests/diag/run.sh   rewrite golden/*.out from the current output
#
set -uo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$here" || exit 1
kmc="${KMC:-$here/kmc}"
komodo="$here/komodo"
[[ -x "$kmc" ]] || { echo "error: no kmc — run ./build-self.sh" >&2; exit 1; }
work="$(mktemp -d)"; trap 'rm -rf "$work"' EXIT
pass=0 fail=0
ok()  { echo "ok       $1"; pass=$((pass+1)); }
bad() { echo "FAIL     $1"; shift; [[ $# -gt 0 ]] && printf '%s\n' "$@" | sed 's/^/           /'; fail=$((fail+1)); }
esc=$'\033'

# 1. the catalog: complete entries, every example behaves as documented
if python3 tests/diag/check_catalog.py >"$work/cat.out" 2>&1; then ok "catalog: $(tail -1 "$work/cat.out")"; else bad "catalog" "$(cat "$work/cat.out")"; fi
if python3 tools/gen-diag.py --check; then ok "generated files are up to date"; else bad "generated files are stale (run tools/gen-diag.py)"; fi

# 2. golden rich layouts (colour stripped) + the machine format of the same input
for k in tests/diag/golden/*.km; do
    n="$(basename "$k" .km)"; args=""
    [[ -f "tests/diag/golden/$n.args" ]] && args="$(cat "tests/diag/golden/$n.args")"
    got="$(cd tests/diag/golden && "$kmc" "$n.km" check $args rich color 2>&1 | sed "s/$esc\[[0-9;]*m//g")"
    if [[ -n "${UPDATE:-}" ]]; then printf '%s\n' "$got" > "tests/diag/golden/$n.out"; fi
    if [[ "$got" == "$(cat "tests/diag/golden/$n.out" 2>/dev/null)" ]]; then ok "rich layout: $n"; else
        bad "rich layout: $n"; diff <(printf '%s\n' "$got") "tests/diag/golden/$n.out" | head -20; fi
    plain="$(cd tests/diag/golden && "$kmc" "$n.km" check $args rich 2>&1)"
    if [[ "$plain" == "$got" ]]; then ok "colour adds only escapes: $n"; else bad "colour changes text: $n"; fi
    short="$(cd tests/diag/golden && "$kmc" "$n.km" check $args 2>&1)"
    if ! grep -q "$esc" <<<"$short" && ! grep -q -- '-->' <<<"$short" && grep -Eq '^[^ ]+:[0-9]+:[0-9]+: (error|warning): ' <<<"$short"; then
        ok "machine format (default): $n"; else bad "machine format (default): $n" "$short"; fi
done

# 3. exact machine lines (what editors and the LSP parse)
want="undefined_name.km:5:19: error: undefined name 'cont' (did you mean 'count'?)"
got="$(cd tests/diag/golden && "$kmc" undefined_name.km check 2>&1)"
[[ "$got" == "$want" ]] && ok "machine line is byte-identical to the old format" || bad "machine line" "$got"
got="$(cd tests/diag/golden && "$kmc" multi.km check 2>&1 | wc -l)"
[[ "$got" == 3 ]] && ok "three errors in three lines" || bad "multi-error line count" "$got"

# 4. a message the catalog does not know: snippet, no code
got="$(cd tests/diag/golden && "$kmc" arg_type.km check rich "render=arg_type.km:7:19: error: a brand new kind of problem" 2>&1)"
if grep -q '^error: a brand new kind of problem' <<<"$got" && grep -q '\^' <<<"$got" && ! grep -q 'K0' <<<"$got"; then ok "uncatalogued message: snippet, no code"; else bad "uncatalogued message" "$got"; fi

# 5. the cap: 25 broken functions report 20 errors
{ for i in $(seq 1 25); do printf 'fn f%d(): int\n    return zz\n\n' "$i"; done; printf 'void main()\n    print(1)\n'; } > "$work/many.km"
got="$("$kmc" "$work/many.km" check 2>&1 | grep -c ': error: ')"
[[ "$got" == 20 ]] && ok "error cap: 20 of 25 reported" || bad "error cap" "$got"

# 6. komodo explain: every code has a page; bad codes are refused
codes="$(grep -o '^@ K[0-9]*' docs/errors/catalog.txt | cut -c3-)"
n=0; badc=""
for c in $codes; do
    out="$("$komodo" explain "$c" 2>&1)" && grep -q "^# $c:" <<<"$out" && grep -q '## What went wrong' <<<"$out" || badc="$badc $c"
    n=$((n+1))
done
[[ -z "$badc" ]] && ok "komodo explain: all $n codes" || bad "komodo explain failed for:$badc"
"$komodo" explain K9999 >/dev/null 2>&1; [[ $? -eq 2 ]] && ok "explain: unknown code refused" || bad "explain: unknown code"
[[ "$("$komodo" explain k1 2>&1 | head -1)" == "# K0001: undefined name" ]] && ok "explain: k1 == K0001" || bad "explain: lenient code spelling"

# 7. komodo CLI layout selection (a pty is needed to test the tty default)
cp tests/diag/golden/undefined_name.km "$work/main.km"
sh="$(cd "$work" && "$komodo" -q main.km 2>&1)"
grep -q 'main.km:5:19: error: undefined name' <<<"$sh" && ! grep -q -- '-->' <<<"$sh" && ok "komodo (not a tty): machine format" || bad "komodo non-tty" "$sh"
sh="$(cd "$work" && "$komodo" -q --errors=rich main.km 2>&1)"
grep -q 'error\[K0001\]' <<<"$sh" && ! grep -q "$esc" <<<"$sh" && ok "--errors=rich: rich, no colour off a tty" || bad "--errors=rich" "$sh"
sh="$(cd "$work" && KM_ERRORS=rich "$komodo" check main.km 2>&1)"
grep -q 'error\[K0001\]' <<<"$sh" && ok "KM_ERRORS=rich (check)" || bad "KM_ERRORS=rich" "$sh"
sh="$(cd "$work" && "$komodo" --explain -q main.km 2>&1)"
grep -q 'error\[K0001\]' <<<"$sh" && ok "--explain" || bad "--explain" "$sh"
if command -v script >/dev/null 2>&1; then
    t() { (cd "$work" && script -qec "$1" /dev/null 2>&1 | tr -d '\r'); }
    q="$(printf '%q' "$komodo")"
    sh="$(t "$q -q main.km")"
    grep -q "$esc\[" <<<"$sh" && grep -q 'K0001' <<<"$sh" && ok "tty default: rich + colour" || bad "tty default" "$sh"
    sh="$(t "env NO_COLOR=1 $q -q main.km")"
    ! grep -q "$esc" <<<"$sh" && grep -q 'K0001' <<<"$sh" && ok "tty + NO_COLOR: rich, no colour" || bad "NO_COLOR" "$sh"
    sh="$(t "env KM_ERRORS=short $q -q main.km")"
    grep -q 'main.km:5:19: error: undefined name' <<<"$sh" && ! grep -q -- '-->' <<<"$sh" && ok "tty + KM_ERRORS=short: machine format" || bad "KM_ERRORS=short" "$sh"
else
    echo "skip     tty tests (no \`script\` command)"
fi

echo "────────────────────────────────"
echo "$pass passed, $fail failed"
[[ $fail -eq 0 ]]
