#!/usr/bin/env bash
#
# tests/interop/run.sh — C / C++ interoperability tests.
#
# Each tests/interop/<name>.km starts with one directive line:
#
#   // interop: extras a.c b.cpp   build with `komodo <name>.km -- a.c b.cpp`
#                                  (.cpp via g++) and run; stdout must equal
#                                  <name>.expected
#   // interop: lib main.c         build `komodo --lib` (object, static archive
#                                  and shared object), link main.c against
#                                  each, run
#
# Any program containing `export fn` also gets its header generated
# (`--emit-header`) into a scratch dir, put on the include path, and
# checked to parse as both C and C++.
#
# Usage:  tests/interop/run.sh [name ...]
#
set -uo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd "$here/../.." && pwd)"
komodo="${KOMODO:-$root/komodo}"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

names=("$@")
if [[ ${#names[@]} -eq 0 ]]; then
    for f in "$here"/*.km; do names+=("$(basename "$f" .km)"); done
fi

pass=0 fail=0
for name in "${names[@]}"; do
    src="$here/$name.km"
    exp="$here/$name.expected"
    directive="$(sed -n '1s|^// interop: *||p' "$src")"
    kind="${directive%% *}"
    rest="${directive#"$kind"}"
    files=()
    for f in $rest; do files+=( "$here/$f" ); done

    if grep -q '^export ' "$src"; then
        if ! "$komodo" -q --emit-header "$work/$name.h" "$src" 2>"$work/err"; then
            echo "FAIL     interop/$name  (--emit-header)"; sed 's/^/           /' "$work/err"
            fail=$((fail+1)); continue
        fi
        if ! gcc -fsyntax-only -Wall -Werror -x c "$work/$name.h" 2>"$work/err" \
           || ! g++ -fsyntax-only -Wall -Werror -x c++ "$work/$name.h" 2>>"$work/err"; then
            echo "FAIL     interop/$name  (generated header does not compile as C and C++)"
            sed 's/^/           /' "$work/err"; fail=$((fail+1)); continue
        fi
    fi

    case "$kind" in
        extras)
            got="$("$komodo" -q "$src" -- -I"$work" "${files[@]}" 2>"$work/err")" ;;
        lib)
            if ! "$komodo" -q --lib -o "$work/$name" "$src" 2>"$work/err"; then
                echo "FAIL     interop/$name  (--lib build)"; sed 's/^/           /' "$work/err"
                fail=$((fail+1)); continue
            fi
            if ! gcc -no-pie -I"$work" "${files[0]}" "$work/lib$name.a" -l:libgc.so.1 -lm \
                    -o "$work/$name.bin" 2>"$work/err"; then
                echo "FAIL     interop/$name  (link against lib$name.a)"; sed 's/^/           /' "$work/err"
                fail=$((fail+1)); continue
            fi
            got="$("$work/$name.bin" 2>"$work/err")"
            # the shared object built alongside it, found through an rpath
            if ! gcc -I"$work" "${files[0]}" -L"$work" -l"$name" -Wl,-rpath,"$work" -l:libgc.so.1 -lm \
                    -o "$work/$name.so.bin" 2>"$work/err"; then
                echo "FAIL     interop/$name  (link against lib$name.so)"; sed 's/^/           /' "$work/err"
                fail=$((fail+1)); continue
            fi
            got2="$("$work/$name.so.bin" 2>"$work/err")"
            if [[ "$got" != "$got2" ]]; then
                echo "FAIL     interop/$name  (lib$name.a and lib$name.so disagree)"; fail=$((fail+1)); continue
            fi ;;
        *)
            echo "FAIL     interop/$name  (no '// interop:' directive)"; fail=$((fail+1)); continue ;;
    esac
    rc=$?
    if [[ ! -f "$exp" ]]; then
        echo "FAIL     interop/$name  (no $exp)"; fail=$((fail+1)); continue
    fi
    if ! diff -q <(printf '%s\n' "$got") "$exp" >/dev/null; then
        echo "FAIL     interop/$name  (output mismatch)"
        diff <(printf '%s\n' "$got") "$exp" | sed 's/^/           /'
        sed 's/^/           /' "$work/err" | head -20
        fail=$((fail+1)); continue
    fi
    echo "ok       interop/$name"
    pass=$((pass+1))
done

# `export fn` is x86-only: other targets must refuse it clearly
if [[ $# -eq 0 ]]; then
    for tgt in c stackvm; do
        if "$komodo" -q -t "$tgt" -S "$here/export_lib.km" 2>"$work/err"; then
            echo "FAIL     interop/export-target-$tgt  (compiled; expected an error)"; fail=$((fail+1))
        elif grep -qF "export fn is not supported on target '$tgt'" "$work/err"; then
            echo "ok       interop/export-target-$tgt"; pass=$((pass+1))
        else
            echo "FAIL     interop/export-target-$tgt  (wrong message)"; sed 's/^/           /' "$work/err"; fail=$((fail+1))
        fi
    done
fi

echo "────────────────────────────────"
echo "interop: $pass passed, $fail failed"
[[ $fail -eq 0 ]]
