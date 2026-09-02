#!/usr/bin/env bash
#
# build.sh — compile a Komodo source file to a native executable.
#
#   Komodo source ──▶ NASM (.asm) ──nasm──▶ .o ──gcc──▶ executable
#                                                 └─ + stdlib/*.c
#
# Usage:  kmsrc/compiler/build.sh <file.km> [output-name]
#
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd "$here/../.." && pwd)"

src="${1:?usage: build.sh <file.km> [output-name]}"
out="${2:-${src%.km}}"
asm="$src.asm"
obj="$src.o"

# 1. Komodo front-end + codegen  →  NASM assembly
python3 "$root/main.py" "$here/main.km" "$src"

# 2. Assemble with NASM
nasm -f elf64 "$asm" -o "$obj"

# 3. Link against the standard library written in C
gcc -no-pie -o "$out" "$obj" "$here"/stdlib/*.c -lm

echo "built $out"

echo "running $out"
"$out"