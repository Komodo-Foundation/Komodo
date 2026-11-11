# Komodo

[![CI](https://github.com/Komodo-Foundation/Komodo/actions/workflows/ci.yml/badge.svg)](https://github.com/Komodo-Foundation/Komodo/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

**A small, statically-typed, self-hosting language that compiles to native code and talks to C, C++ and the rest of your system with no ceremony.**

```km
import console

abstract struct Shape
struct Circle : Shape
    r: int
struct Rect : Shape
    w: int
    h: int

fn area(s: Shape) : int
    match s
        case Circle(r)
            return 3 * r * r
        case Rect(w, h)
            return w * h
    return 0

void main()
    let shapes = [new Circle(2), new Rect(3, 5)]
    for s in shapes
        console.print(`area {area(s)}`)
```

```bash
$ komodo
area 12
area 15
```

A trimmed version of [`examples/shapes.km`](examples/shapes.km); [`examples/`](examples/) has two more showcases.

## Why Komodo

- **Zero-config.** Type `komodo` in a directory with a `main.km`. No build files, no flags, no makefile.
- **Native and fast to build.** Compiles to x86-64 assembly; the compiler rebuilds itself in about a second.
- **Seamless interop.** `import zlib` (a system C library) binds a system library from its header; `native/` C and C++ is compiled and linked automatically; `export fn` makes Komodo callable from C, C++, Python (`ctypes`) and Rust.
- **Safe by default.** Static types, null-safety with flow-sensitive narrowing, ownership/move checks, bounds-checked arrays, garbage-collected memory.
- **Several targets.** x86-64 native, portable C, WebAssembly, and a stack VM.
- **Self-hosting.** The compiler is written in Komodo and compiles itself to byte-identical output.

## Install

```bash
curl -fsSL https://raw.githubusercontent.com/Komodo-Foundation/Komodo/main/install.sh | bash
komodo doctor            # checks nasm, gcc and libgc, and says how to install any that are missing
```

or as a Debian/Ubuntu package (`packaging/build-deb.sh`, then `sudo apt install ./dist/komodo_*.deb`; each tagged release attaches one), or from a checkout: `git clone https://github.com/Komodo-Foundation/Komodo && cd Komodo && ./install.sh`. Without installing: `docker build -t komodo . && docker run --rm -it komodo komodo repl`.

Requirements: Linux x86-64 with `nasm`, `gcc` and the Boehm GC (`libgc`); `g++`, `pkg-config`, `emcc` and `node` are optional (C++ interop, library lookup, the WebAssembly target). The compiler builds itself from a checked-in seed on first run (about 2 seconds); `komodo upgrade` updates the installation.

Editor support: install the VS Code extension with `code --install-extension komodo-foundation.komodo-lang` (or from a checkout, see [editors/README.md](editors/README.md#vs-code)). Vim, Emacs and a language server are covered there too.

## Using it

```bash
komodo new hello && cd hello    # scaffold a project
komodo                          # build (incrementally) and run
komodo build --release          # optimised executable ./hello
komodo check                    # type-check only, print diagnostics
komodo lint                     # check plus unused names and style warnings
komodo test                     # run *_test.km files
komodo watch                    # rebuild and rerun on every change
komodo repl                     # interactive prompt
komodo -t wasm                  # WebAssembly via emcc and node
komodo lib                      # .o, .a, .so and a C/C++ header for `export fn`s
```

Everything else is in the docs.

## Memory management

Memory is garbage-collected, so nothing is freed by hand. Two modifiers control how an object can be shared:

- **`shared struct`** (the default; plain `struct` means the same) — values alias. `let b = a` gives two names for one object. `copy(a)` makes a deep copy.
- **`unique struct`** — one owner. Assigning it to another name or returning it moves it, and the old name is invalid until it is assigned again. Passing it to a function only borrows it.

`class` takes the same modifiers.

```km
import console

shared struct Box
    v: int

unique struct Handle
    id: int

void main()
    let a = new Box(1)
    let b = a
    b.v = 7
    console.print("shared:", a.v)

    let h = new Handle(1)
    let g = h
    h = new Handle(2)
    console.print("unique:", h.id, g.id)
```

```text
shared: 7
unique: 2 1
```

Full rules: [language reference, section 15](docs/language/reference.md#15-memory-and-ownership).

## Documentation

| | |
|---|---|
| [Documentation index](docs/README.md) | every guide, grouped by what you want to do |
| [Getting started](docs/getting-started.md) | install, first program, a five-minute tour |
| [Language basics](docs/language/basics.md) and [language reference](docs/language/reference.md) | the guide, and every rule |
| [The `komodo` command](docs/cli.md) | every command, target and option |
| [C and C++ interop](docs/interop.md) | `import zlib`, `native/`, `export fn` |
| [Compiler internals](docs/internals.md) | pipeline, backends, bootstrapping, adding a feature |

For AI agents: [`llms.txt`](llms.txt) is a one-page map of the docs.

## Status

Komodo is **pre-1.0**: the language and compiler are usable and well tested (examples, diagnostics, interop and binding suites run in CI), but the syntax may still change between minor versions — see the [changelog](CHANGELOG.md). The native target and the C FFI are x86-64 Linux; the C and WebAssembly targets are portable but don't support FFI yet. Known gaps are listed at the end of [`docs/language/types.md`](docs/language/types.md).

## Repository layout

| Path | What |
|---|---|
| `komodo`, `kmrepl`, `install.sh` | the CLI, the REPL, the installer |
| `main.km` | compiler driver: resolves imports, picks a target |
| `modules/frontend/` | lexer, parser, AST |
| `modules/analyze/` | type system and semantic analysis |
| `modules/optimize/` | constant folding, algebraic simplification, dead-code elimination |
| `modules/codegen/` | target-independent lowering walk |
| `modules/backend/` | `Backend` interface and the x86-64, C/WebAssembly and stack-VM backends |
| `stdlib/` | C runtime and the standard-library interface modules |
| `tools/` | `km-bind` (C header → Komodo binding), assembly splitter |
| `examples/`, `demos/` | showcase programs; `tests/examples/` holds the test fixtures (`.km` + `.expected`) |
| `tests/` | analyser, interop and binding tests |
| `bootstrap/` | the checked-in seed that builds the compiler without any other Komodo implementation |
| `docs/` | documentation |

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md). Bugs and ideas go in [issues](https://github.com/Komodo-Foundation/Komodo/issues); security reports follow [SECURITY.md](SECURITY.md).

## License

MIT — see [LICENSE](LICENSE). Copyright (c) 2026 Komodo Foundation.
