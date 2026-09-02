#!/usr/bin/env bash
#
# run-tests.sh — compile every examples/*.km, run it, and check its output
# against the matching examples/<name>.expected file.  Also compiles each
# with the `stackvm` backend to confirm the target-independent path stays
# wired up.
#
# Usage:  ./run-tests.sh [name ...]     (default: all examples)
#
set -uo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$here"

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

names=("$@")
if [[ ${#names[@]} -eq 0 ]]; then
    for f in examples/*.km; do names+=("$(basename "$f" .km)"); done
fi

pass=0 fail=0
for name in "${names[@]}"; do
    src="examples/$name.km"
    exp="examples/$name.expected"
    if [[ ! -f "$src" ]]; then
        echo "MISSING  $name  (no $src)"; fail=$((fail+1)); continue
    fi

    cp "$src" "$work/$name.km"
    # sibling Komodo modules an example may `import`
    cp examples/*.kmm "$work"/ 2>/dev/null || true
    ws="$work/$name.km"

    # x86: compile + assemble + link + run
    if ! got="$(./build.sh "$ws" x86 "$work/$name.bin" 2>"$work/$name.err")"; then
        echo "FAIL     $name  (x86 build/run errored)"
        sed 's/^/           /' "$work/$name.err"
        fail=$((fail+1)); continue
    fi
    # keep only the program's own stdout (between the run markers)
    got="$(printf '%s\n' "$got" | sed -n '/^── running /,/^── exit /p' | sed '1d;$d')"

    # stackvm: compile only (must not error)
    if ! ./build.sh "$ws" stackvm >/dev/null 2>"$work/$name.vm.err"; then
        echo "FAIL     $name  (stackvm compile errored)"
        sed 's/^/           /' "$work/$name.vm.err"
        fail=$((fail+1)); continue
    fi

    if [[ -f "$exp" ]] && ! diff -q <(printf '%s\n' "$got") "$exp" >/dev/null; then
        echo "FAIL     $name  (output mismatch)"
        diff <(printf '%s\n' "$got") "$exp" | sed 's/^/           /'
        fail=$((fail+1)); continue
    fi

    echo "ok       $name"
    pass=$((pass+1))
done

echo "────────────────────────────────"
echo "$pass passed, $fail failed"
[[ $fail -eq 0 ]]
