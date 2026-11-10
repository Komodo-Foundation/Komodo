#!/usr/bin/env bash
#
# perf-budget.sh — fail if the compiler or its output gets slower or bigger.
#
#   tools/perf-budget.sh            check the budgets below (needs ./kmc)
#   KM_PERF_SLACK=2.5 tools/perf-budget.sh    scale the TIME budgets (CI is noisy)
#
# Budgets are ceilings, set ~30% above the numbers measured when they were
# introduced (2026-11: kmc 1.07 MB, self-compile 0.57 s, hello cold 0.14 s,
# hello binary 73 KB, 13.2k compiler lines). Lower them when you make
# something faster; raise one only with a reason in the commit message.
set -uo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$here" || exit 1
slack="${KM_PERF_SLACK:-1}"
fail=0

check() {  # name value budget unit
    if [[ -z "$2" ]]; then printf 'FAIL  %-28s no measurement\n' "$1"; fail=1; return; fi
    if awk -v v="$2" -v b="$3" 'BEGIN{exit !(v+0 <= b+0)}'; then
        printf 'ok    %-28s %s %s (budget %s)\n' "$1" "$2" "$4" "$3"
    else
        printf 'FAIL  %-28s %s %s exceeds budget %s\n' "$1" "$2" "$4" "$3"; fail=1
    fi
}
scaled() { awk -v b="$1" -v s="$slack" 'BEGIN{printf "%.2f", b*s}'; }
best() {   # best-of-3 wall time of a command, seconds
    local f out=(); f="$(mktemp)"
    for _ in 1 2 3; do
        /usr/bin/time -f '%e' -o "$f" "$@" >/dev/null 2>&1 || true
        out+=("$(cat "$f")")
    done
    rm -f "$f"
    printf '%s\n' "${out[@]}" | sort -n | head -1
}

[[ -x ./kmc ]] || { echo "perf-budget: build ./kmc first (./build-self.sh)" >&2; exit 2; }

check "kmc binary size"        "$(stat -c %s kmc)"                         1400000 bytes
check "compiler source lines"  "$(cat main.km modules/*.kmm modules/*/*.kmm | wc -l)" 15500 lines
check "bootstrap seed size"    "$(stat -c %s bootstrap/main.km.asm.gz)"    200000  bytes

t="$(best ./kmc main.km x86)"
check "kmc compiles itself"    "$t" "$(scaled 0.80)" s

d="$(mktemp -d)"; trap 'rm -rf "$d"' EXIT
printf 'void main()\n    print("hi")\n' > "$d/main.km"
( cd "$d" && "$here/komodo" -q >/dev/null 2>&1 )            # warm the runtime cache
rm -rf "$d/.komodo"
t="$( cd "$d" && { /usr/bin/time -f '%e' "$here/komodo" -q >/dev/null; } 2>&1 )"
check "hello: build+run (cold)" "$t" "$(scaled 0.25)" s
( cd "$d" && "$here/komodo" build -q -o hello >/dev/null 2>&1 )
check "hello binary size"      "$(stat -c %s "$d/hello")"                  100000  bytes
exit $fail
