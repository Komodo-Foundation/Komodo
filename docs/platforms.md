# Platform support (host)

This page covers where the **compiler itself** (`kmc`, its bootstrap, build and
test scripts) runs. It does not cover code-generation targets; those are listed
in `docs/internals.md`.

## Status today

| Host | Build `kmc` | Run tests | CI |
|---|---|---|---|
| Linux x86-64 (Debian/Ubuntu) | yes | yes | yes (`ci.yml`, `ubuntu-latest`) |
| Other Linux x86-64 (Fedora, Arch) | likely, untested | untested | no |
| Linux arm64 | no | no | no |
| macOS (arm64 or x86-64) | no | no | no |
| Windows | no | no | no |

## Why only Linux x86-64 builds today

- **Bootstrap seed** `bootstrap/main.km.asm.gz` is NASM output in ELF64 format.
  It only assembles with `nasm -f elf64` and links with `gcc -no-pie`.
- **Linker flags** use `-l:libgc.so.1`, which is GNU ld syntax.
- **Runtime** reads `/proc/self/exe` (ELF symbol table for stack traces),
  `/proc/self/cgroup` and `/sys/fs/cgroup` (memory limits), and maps the
  executable with `mmap`.
- **Build script** `build-self.sh` uses GNU tools: `sha256sum`, `stat -c`,
  `nproc`, `xargs -P`, `gzip -n`. macOS ships BSD versions of several of these.
- **Process builtins** (`stdlib/process.c`) call `system` and `popen`, which
  expect a POSIX shell.
- **C backend cannot compile the compiler yet.** `kmc main.km c` fails with
  "extern fn (C FFI call) not supported yet" because 13 `extern fn` declarations
  in `main.km` and `modules/` have no C-backend lowering.

## Options for host portability (decision needed)

1. **C seed.** Generate a C bootstrap from the compiler and build it with any C
   compiler plus Boehm GC. Needs C-backend support for `extern fn`. Works on
   every host with a C compiler, and removes nasm from the bootstrap.
2. **Keep the asm seed; add per-OS object formats.** Mach-O for macOS, COFF for
   Windows. More work, and each platform needs its own seed.
3. **Keep the asm seed; Linux only.** Document the limit and provide a
   Docker image for everything else.

Recommended default: option 1, because it removes the per-platform assembler
work and reuses the existing C backend.

## Small steps that do not depend on the decision

- Replace GNU-only shell tools in `build-self.sh` with portable equivalents.
- Add a macOS CI job that runs `./build-self.sh` once the seed question is settled.
- Make `stdlib/km_error.c` and `runtime.c` skip `/proc` probes when the file is missing.
