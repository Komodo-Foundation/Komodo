#!/usr/bin/env bash
#
# tests/bind/run.sh -- tests for tools/km-bind.
#
# Each tests/bind/<name>.km starts with one or more directive lines:
#
#   // bind: <km-bind arguments>     run tools/km-bind in a scratch dir
#                                    (@here@ = this directory)
#
# then the program is compiled with `komodo -q` and its stdout must equal
# <name>.expected. Cases whose header is not installed (zlib) are skipped.
#
# Usage: tests/bind/run.sh [name ...]
set -uo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd "$here/../.." && pwd)"
komodo="${KOMODO:-$root/komodo}"
kmbind="$root/tools/km-bind"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

names=("$@")
if [[ ${#names[@]} -eq 0 ]]; then
    for f in "$here"/*.km; do names+=("$(basename "$f" .km)"); done
fi

pass=0 fail=0 skip=0
for name in "${names[@]}"; do
    src="$here/$name.km"
    dir="$work/$name"
    mkdir -p "$dir"
    cp "$here"/*.c "$here"/*.h "$dir"/ 2>/dev/null
    cp "$src" "$dir/$name.km"
    skipit=0 bad=0
    while IFS= read -r line; do
        args="${line#// bind: }"
        args="${args//@here@/$dir}"
        # shellcheck disable=SC2086
        ( cd "$dir" && "$kmbind" $args ) 2>"$dir/err"
        rc=$?
        if [[ $rc -eq 3 ]] || grep -q "cannot find header" "$dir/err"; then skipit=1; break; fi
        if [[ $rc -ne 0 ]]; then
            echo "FAIL     bind/$name  (km-bind $args)"; sed 's/^/           /' "$dir/err"
            bad=1; break
        fi
    done < <(grep '^// bind: ' "$src")
    if [[ $skipit -eq 1 ]]; then
        echo "skip     bind/$name  ($(cat "$dir/err"))"; skip=$((skip+1)); continue
    fi
    [[ $bad -eq 1 ]] && { fail=$((fail+1)); continue; }
    got="$("$komodo" -q "$dir/$name.km" 2>"$dir/err")"
    if ! diff -q <(printf '%s\n' "$got") "$here/$name.expected" >/dev/null; then
        echo "FAIL     bind/$name  (output mismatch)"
        diff <(printf '%s\n' "$got") "$here/$name.expected" | sed 's/^/           /'
        head -20 "$dir/err" | sed 's/^/           /'
        fail=$((fail+1)); continue
    fi
    echo "ok       bind/$name"; pass=$((pass+1))
done

# ---- tool-level checks ------------------------------------------------
check() {  # check <label> <condition-exit-status>
    if [[ $2 -eq 0 ]]; then echo "ok       bind/$1"; pass=$((pass+1))
    else echo "FAIL     bind/$1"; fail=$((fail+1)); fi
}
if [[ $# -eq 0 ]]; then
    "$kmbind" --auto no_such_library_xyz >"$work/o" 2>"$work/e"; rc=$?
    [[ $rc -eq 3 ]] && grep -q "no header for 'no_such_library_xyz'" "$work/e"; check auto-missing $?

    # deterministic output (so komodo can cache it)
    "$kmbind" --auto string >"$work/d1" 2>/dev/null; "$kmbind" --auto string >"$work/d2" 2>/dev/null
    cmp -s "$work/d1" "$work/d2"; check auto-deterministic $?

    # --prefix filters; skipped things are reported
    "$kmbind" "$here/shapes.h" --prefix canvas_ >"$work/p" 2>/dev/null
    grep -q 'canvas_new' "$work/p" && ! grep -q 'sum_array' "$work/p"; check prefix $?
    "$kmbind" "$here/shapes.h" >"$work/s" 2>/dev/null
    # a variadic prototype keeps its fixed prefix and the `...`
    grep -q 'extern fn shape_log(fmt: string, ...): int' "$work/s" && grep -q 'skipped: sh_twice' "$work/s"; check skips $?
    "$kmbind" "$here/shapes.h" -o "$work/shapes.kmm" >/dev/null 2>&1; grep -q 'shape_log(fmt: string, ...)' "$work/shapes.kmm" 2>/dev/null; check variadic-prefix $?
fi

echo "────────────────────────────────"
echo "bind: $pass passed, $fail failed, $skip skipped"
[[ $fail -eq 0 ]]
