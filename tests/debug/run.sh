#!/usr/bin/env bash
#
# tests/debug/run.sh -- source-level debugging of the x86 target.
#
# Builds tests/debug/prog.km (which imports a second file, dbgutil.kmm)
# with KM_DEBUG=1 + `nasm -g -F dwarf` + `gcc -g`, then asserts with gdb that
#   * a breakpoint on a Komodo file:line stops there (in both files),
#   * the backtrace names Komodo functions and Komodo source lines.
# Skipped (exit 0) when gdb, nasm or gcc is absent.
set -uo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd "$here/../.." && pwd)"
for t in gdb nasm gcc; do
    command -v "$t" >/dev/null || { echo "debug: skipped ($t not installed)"; exit 0; }
done
[[ -x "$root/kmc" ]] || "$root/build-self.sh" >/dev/null 2>&1 || { echo "debug: cannot build kmc"; exit 1; }

work="$(mktemp -d)"; trap 'rm -rf "$work"' EXIT
cp "$here"/*.km "$here"/dbgutil.kmm "$work"/
cd "$work"
# build <name> [debug]: the recipe in docs/debugging.md
build() {
    if [[ ${2:-} == debug ]]; then
        KM_DEBUG=1 "$root/kmc" "$1.km" >/dev/null && nasm -g -F dwarf -f elf64 "$1.km.asm" -o "$1.o" \
            && gcc -g -no-pie -I"$root/stdlib" -o "$1" "$1.o" "$root"/stdlib/*.c -lm -l:libgc.so.1
    else
        "$root/kmc" "$1.km" >/dev/null && nasm -f elf64 "$1.km.asm" -o "$1.o" \
            && gcc -no-pie -I"$root/stdlib" -o "$1" "$1.o" "$root"/stdlib/*.c -lm -l:libgc.so.1
    fi
}
build prog debug && build trace debug && build crash debug
build_plain() { mv trace trace.dbg; build trace && mv trace trace.plain; mv trace.dbg trace; }
build_plain || { echo "debug: build failed"; exit 1; }

fail=0
check() {   # check <what> <gdb output> <pattern>
    if grep -q -- "$3" <<<"$2"; then echo "  ok   $1"; else echo "  FAIL $1 (want /$3/)"; echo "$2" | sed 's/^/       /'; fail=1; fi
}
gdbrun() { gdb -batch -ex "set debuginfod enabled off" "$@" ./prog 2>&1; }

out="$(gdbrun -ex 'break prog.km:13' -ex run -ex bt)"
check "break in main file stops at line" "$out" 'Breakpoint 1, square () at prog.km:13'
check "bt names the caller and its line"  "$out" '#1 .* in km_program_main () at prog.km:20'

out="$(gdbrun -ex 'break dbgutil.kmm:5' -ex run -ex bt)"
check "break in imported module stops"    "$out" 'triple () at .*dbgutil.kmm:5'
check "bt crosses files and methods"      "$out" '#1 .* in Box__scaled () at prog.km:8'
check "bt reaches main"                   "$out" '#2 .* in km_program_main () at prog.km:21'

out="$(gdbrun -ex 'break Box__scaled' -ex run -ex 'info symbol $pc')"
check "function symbols are plain Komodo names" "$out" '^Box__scaled + '

# runtime call chains (stderr; deterministic: names and Komodo lines only)
check_eq() {   # check_eq <what> <got> <want>
    if [[ "$2" == "$3" ]]; then echo "  ok   $1"; else echo "  FAIL $1"; diff <(echo "$2") <(echo "$3") | sed 's/^/       /'; fail=1; fi
}
check_eq "uncaught error prints a Komodo call chain with lines" \
    "$(./trace 2>&1 >/dev/null)" "$(cat "$here/trace.expected")"
check_eq "crash report names the line" \
    "$(./crash 2>&1 >/dev/null)" "$(cat "$here/crash.expected")"
check_eq "plain build: error line only" "$(./trace.plain 2>&1 >/dev/null)" "Uncaught error: boom"
check_eq "KM_TRACE=1 on a plain build: function names" \
    "$(KM_TRACE=1 ./trace.plain 2>&1 >/dev/null)" "$(cat "$here/trace_names.expected")"

[[ $fail -eq 0 ]] && echo "debug: all passed" || echo "debug: FAILED"
exit $fail
