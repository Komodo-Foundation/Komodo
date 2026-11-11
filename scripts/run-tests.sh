#!/usr/bin/env bash
#
# scripts/run-tests.sh — compile every examples/*.km and tests/examples/*.km, run it, and check
# its output against the matching <name>.expected file (next to the .km).  Also compiles each
# with the `stackvm` backend to confirm the target-independent path stays
# wired up, and builds + runs it through the `c` backend (portable C via
# gcc) against the same expected output.
#
# Uses the native, self-hosted compiler via the `komodo` CLI (build one
# first with ./build-self.sh).  Set $KOMODO to point at a different driver.
#
# Usage:  ./scripts/run-tests.sh [name ...]     (default: all examples)
#
set -uo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$here"

komodo="${KOMODO:-$here/komodo}"
[[ -x "$komodo" ]] || { echo "error: no komodo driver at $komodo" >&2; exit 1; }
[[ -x "$here/kmc" ]] || { echo "error: no kmc — run ./build-self.sh first" >&2; exit 1; }

names=("$@")
if [[ ${#names[@]} -eq 0 ]]; then
    for f in examples/*.km tests/examples/*.km; do names+=("$(basename "$f" .km)"); done
fi

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

have_wasm=0 wasm_ran=0
if command -v emcc >/dev/null 2>&1 && command -v node >/dev/null 2>&1; then have_wasm=1; fi

pass=0 fail=0
for name in "${names[@]}"; do
    src="examples/$name.km"
    [[ -f "$src" ]] || src="tests/examples/$name.km"
    exp="${src%.km}.expected"
    if [[ ! -f "$src" ]]; then
        echo "MISSING  $name  (no $src)"; fail=$((fail+1)); continue
    fi

    # x86: compile + assemble + link + run (import resolution is relative to
    # the source dir, so build in place — no tmp copy needed).
    if ! got="$("$komodo" -q "$src" 2>"$work/$name.err")"; then
        echo "FAIL     $name  (x86 build/run errored)"
        sed 's/^/           /' "$work/$name.err"
        fail=$((fail+1)); continue
    fi

    # stackvm: compile only (must not error).  A source that needs the
    # native C ABI opts out with a `stackvm: skip` comment.
    if ! grep -q 'stackvm: skip' "$src" && ! "$komodo" -q -t stackvm -S "$src" >/dev/null 2>"$work/$name.vm.err"; then
        echo "FAIL     $name  (stackvm compile errored)"
        sed 's/^/           /' "$work/$name.vm.err"
        fail=$((fail+1)); continue
    fi

    if [[ -f "$exp" ]] && ! diff -q <(printf '%s\n' "$got") "$exp" >/dev/null; then
        echo "FAIL     $name  (output mismatch)"
        diff <(printf '%s\n' "$got") "$exp" | sed 's/^/           /'
        fail=$((fail+1)); continue
    fi

    # c: portable C via gcc, built and run (must match the same expected
    # output).  A source that needs the native C ABI opts out with a
    # `c: skip` comment.
    if ! grep -q 'c: skip' "$src"; then
        if ! cgot="$("$komodo" -q -t c "$src" 2>"$work/$name.c.err")"; then
            echo "FAIL     $name  (c build/run errored)"
            sed 's/^/           /' "$work/$name.c.err"
            fail=$((fail+1)); continue
        fi
        if [[ -f "$exp" ]] && ! diff -q <(printf '%s\n' "$cgot") "$exp" >/dev/null; then
            echo "FAIL     $name  (c output mismatch)"
            diff <(printf '%s\n' "$cgot") "$exp" | sed 's/^/           /'
            fail=$((fail+1)); continue
        fi
    fi

    # wasm: the same C output built with emcc and run under node (must match
    # the same expected output).  Skipped entirely when emcc / node are not
    # installed; a source that needs the native C ABI (FFI, SDL, ...) opts
    # out with a `wasm: skip` comment.
    if [[ $have_wasm -eq 1 ]] && ! grep -q 'wasm: skip' "$src"; then
        if ! wgot="$("$komodo" -q -t wasm "$src" 2>"$work/$name.wasm.err")"; then
            echo "FAIL     $name  (wasm build/run errored)"
            sed 's/^/           /' "$work/$name.wasm.err"
            fail=$((fail+1)); continue
        fi
        if [[ -f "$exp" ]] && ! diff -q <(printf '%s\n' "$wgot") "$exp" >/dev/null; then
            echo "FAIL     $name  (wasm output mismatch)"
            diff <(printf '%s\n' "$wgot") "$exp" | sed 's/^/           /'
            fail=$((fail+1)); continue
        fi
        wasm_ran=$((wasm_ran+1))
    fi

    echo "ok       $name"
    pass=$((pass+1))
done

echo "────────────────────────────────"
echo "$pass passed, $fail failed"
if [[ $have_wasm -eq 1 ]]; then echo "(wasm leg ran for $wasm_ran examples)"; else echo "(wasm leg skipped: no emcc / node)"; fi

# C / C++ interop (export fn, --lib, C++ sources): tests/interop/run.sh.
# Run with the full suite only, not when a name subset was asked for.
if [[ $# -eq 0 ]]; then
    "$here/tests/interop/run.sh" || fail=$((fail+1))
fi
[[ $fail -eq 0 ]]
