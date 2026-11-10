# Contributing to Komodo

Thanks for helping. Komodo is a self-hosting compiler: the compiler is written
in Komodo and compiles itself, so most contributions are Komodo code.

## Setup

```bash
git clone https://github.com/lrperks/Komodo && cd Komodo
./install.sh            # symlinks `komodo` onto your PATH
komodo doctor           # checks nasm, gcc, libgc (and optional emcc, node, g++)
komodo self test        # builds the compiler and runs every suite
```

The compiler builds in about two seconds from the checked-in seed
(`bootstrap/main.km.asm.gz`), so no other Komodo implementation is needed.

## Workflow

1. Branch from `main`.
2. Change code under `modules/` (compiler), `stdlib/` (runtime) or `docs/`.
3. `komodo self build` rebuilds `./kmc`; it must end with `fixpoint OK`
   (the new compiler compiles itself to identical assembly).
4. `komodo self test` runs the example, interop, analyser and binding suites.
5. If the compiler's own output changed, run `komodo self seed` and commit the
   refreshed `bootstrap/main.km.asm.gz` with your change.
6. Open a pull request; CI runs the same suites.

**New syntax can't be used inside the compiler's own sources** until a seed
that understands it is committed (land the compiler change, refresh the seed,
then use the syntax).

## Keep it lightweight

Komodo's pitch is a small, fast compiler. `tools/perf-budget.sh` (run in CI) fails
if `kmc`, the compiler source, the bootstrap seed, a hello-world binary or the
build times grow past their budgets. Prefer adding small functions alongside
existing code over new layers, and say why in the commit if you raise a budget.

## Tests

- `examples/<name>.km` + `<name>.expected`: a program and its exact output. It
  runs on the x86, stackvm, c and wasm targets; opt out of a target with a
  `// (c: skip — reason)` comment.
- `tests/sema/<name>.km` + `<name>.experr`: a program that must be rejected, with
  the expected diagnostic.
- `tests/interop/`, `tests/bind/`: C/C++ interop and the binding generator.

## Where things live

See [docs/internals.md](docs/internals.md) for the pipeline, the backend
interface, and a checklist for adding a language feature or a target.

## Style

Follow the surrounding code: 4-space indentation, short comments that explain
why. Keep changes focused; unrelated cleanups belong in their own pull request.

## Reporting bugs and security issues

Use the issue templates for bugs and feature requests. For security issues see
[SECURITY.md](SECURITY.md). By contributing you agree your work is released
under the [MIT license](LICENSE) and that you will follow the
[Code of Conduct](CODE_OF_CONDUCT.md).
