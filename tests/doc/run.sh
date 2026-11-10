#!/usr/bin/env bash
#
# tests/doc/run.sh -- tests for tools/km-doc.
#
# Generates md and html pages for the fixture module and for the whole
# standard library (stdlib/*.kmm) into a scratch directory, then checks that
# known entries appear, that private members stay hidden, and that every
# link resolves (check_links.py). Exits non-zero on any failure.
#
# Usage: tests/doc/run.sh
set -uo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd "$here/../.." && pwd)"
tool="$root/tools/km-doc"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

pass=0 fail=0
ok()  { pass=$((pass + 1)); }
bad() { fail=$((fail + 1)); echo "FAIL: $*"; }

# has FILE PATTERN: FILE must contain PATTERN (fixed string)
has() { if grep -qF -- "$2" "$1" 2>/dev/null; then ok; else bad "$1 lacks: $2"; fi; }
# lacks FILE PATTERN: FILE must not contain PATTERN
lacks() { if grep -qF -- "$2" "$1" 2>/dev/null; then bad "$1 leaks: $2"; else ok; fi; }

fixture="$here/fixtures/shapes.kmm"

# --- fixture, markdown
python3 "$tool" -o "$work/fx-md" "$fixture" || bad "km-doc failed on fixture (md)"
fx="$work/fx-md/shapes.md"
has "$fx" "Shape geometry used by the km-doc tests."
has "$fx" "fn area(c: Circle): int"
has "$fx" "Area of a circle, rounded down. Returns 0 for a zero radius."
has "$fx" "Extends [\`Shape\`](#Shape)."
has "$fx" "Subtypes: [\`Circle\`](#Circle)."
has "$fx" "Variants are written \`Color::Name\`."
has "$fx" "let MAX be 99"
has "$fx" "Starts counting from start."
lacks "$fx" "secret"
lacks "$fx" "hidden"
lacks "$fx" "Internal count."
lacks "$fx" "m_n"

# --- fixture, html
python3 "$tool" --format html -o "$work/fx-html" "$fixture" || bad "km-doc failed on fixture (html)"
fxh="$work/fx-html/shapes.html"
has "$fxh" 'href="#Shape"'
has "$fxh" 'id="area"'
has "$fxh" 'id="q"'
has "$fxh" "<style>"
has "$fxh" "<script>"
lacks "$fxh" "secret"
has "$work/fx-html/index.html" 'href="shapes.html"'

# --- whole standard library, both formats
python3 "$tool" -o "$work/std-md" "$root"/stdlib/*.kmm || bad "km-doc failed on stdlib (md)"
python3 "$tool" --format html -o "$work/std-html" "$root"/stdlib/*.kmm || bad "km-doc failed on stdlib (html)"
has "$work/std-md/math.md" "math_sqrt"
has "$work/std-md/math.md" "Square root of x."
has "$work/std-md/string.md" "string_split"
has "$work/std-md/index.md" "[\`math\`](math.md)"
has "$work/std-html/math.html" 'id="math_sqrt"'
has "$work/std-html/string.html" 'id="string_split"'

# --- an unreadable path must fail
if python3 "$tool" -o "$work/none" "$here/fixtures/does-not-exist.kmm" 2>/dev/null; then
    bad "km-doc accepted a missing path"
else
    ok
fi

# --- links resolve in every generated tree
for dir in "$work/fx-md" "$work/fx-html" "$work/std-md" "$work/std-html"; do
    if ! python3 "$here/check_links.py" "$dir"; then bad "dangling links in $dir"; else ok; fi
done

echo "km-doc tests: $pass passed, $fail failed"
[[ $fail -eq 0 ]]
