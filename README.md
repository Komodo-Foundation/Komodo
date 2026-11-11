# Komodo

[![CI](https://github.com/Komodo-Foundation/Komodo/actions/workflows/ci.yml/badge.svg)](https://github.com/Komodo-Foundation/Komodo/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

**A small, statically-typed, self-hosting language that compiles to native code and talks to C, C++ and the rest of your system with no ceremony.**

```km
import zlib                       // a system C library: bound and linked automatically

class Greeter
    constructor(name: string)
        m_name = name
    fn greet() : string
        return `Hello, {m_name}!`
    m_name: string

void main()
    let g = new Greeter("Komodo")
    print(g.greet())
    print(`zlib {zlibVersion()}`)
```

```bash
$ komodo
Hello, Komodo!
zlib 1.3
```

## Why Komodo

- **Zero-config.** Type `komodo` in a directory with a `main.km`. No build files, no flags, no makefile.
- **Native and fast to build.** Compiles to x86-64 assembly; the compiler rebuilds itself in about a second.
- **Seamless interop.** `import zlib` binds a system library from its header; `native/` C and C++ is compiled and linked automatically; `export fn` makes Komodo callable from C, C++, Python (`ctypes`) and Rust.
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
komodo test                     # run *_test.km files
komodo watch                    # rebuild and rerun on every change
komodo repl                     # interactive prompt
komodo -t wasm                  # WebAssembly via emcc and node
komodo lib                      # .o, .a, .so and a C/C++ header for `export fn`s
```

Everything else is in the docs.

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

MIT — see [LICENSE](LICENSE). Copyright (c) 2026 Lukas Perkins.
