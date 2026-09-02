# Komodo

A self-hosting compiler for the **Komodo** language, written in Komodo itself.

The compiler front-end and code generator live in `modules/*.kmm`; the driver
is `main.km`. Until the compiler can build itself, it runs on the reference
Komodo interpreter (a separate project, `KomodoPy`). It lowers a program to
native **x86-64** assembly (NASM) or to a textual **stack-VM** bytecode, and
links the result against a small C runtime in `stdlib/`.

For the language itself — syntax, types, semantics — see [`design.md`](design.md).

## Layout

| Path | What |
|---|---|
| `main.km` | driver: resolves `import`s, concatenates sources, invokes the compiler |
| `modules/lexer.kmm` | tokenizer |
| `modules/parser.kmm` | recursive-descent parser → AST |
| `modules/ast.kmm` | AST node definitions |
| `modules/codegen*.kmm` | target-independent lowering walk (frame layout, type inference, statements, expressions) |
| `modules/backend.kmm` | the `Backend` interface every target implements |
| `modules/backend_x86.kmm` | System V AMD64 / NASM backend (reference lowering) |
| `modules/backend_stackvm.kmm` | textual stack-VM bytecode backend |
| `modules/km_compiler.kmm` | `compile` / `save` entry points, `selectBackend` |
| `stdlib/*.c` | C runtime: operator lowerings + the `console` / `math` / `algorithm` modules |
| `examples/*.km` | sample programs, each with a `.expected` output fixture |
| `demos/` | larger standalone demos (e.g. an SDL window loop) |

## Requirements

- Python 3 and the Komodo interpreter (`KomodoPy/main.py`)
- `nasm` and `gcc` (for the x86 target)

Point the build scripts at the interpreter:

```bash
export KOMODO_MAIN="/path/to/KomodoPy/main.py"
```

## Building a program

```bash
./build.sh <file.km> [target] [output] [extra cc inputs...]
```

- `target`: `x86` (default) or `stackvm`
- x86: compiles → `<file>.km.asm`, assembles, links `stdlib/*.c` + any trailing
  `.c` / `.o` / `-l` arguments (also read from `$KM_CC_INPUTS`), then runs it
- `stackvm`: writes the bytecode text; there is no runner yet

```bash
./build.sh examples/virtual_demo.km            # compile + run (x86)
./build.sh demos/sdl_window.km x86 ./w -lSDL2   # link an external library
```

Under the hood: `python3 $KOMODO_MAIN main.km <file.km> <target>`. The
interpreter caches parsed ASTs under `~/.km/cache`; `build.sh` clears it each
run so edits to the `.kmm` sources always take effect.

## Testing

```bash
./run-tests.sh              # every examples/*.km
./run-tests.sh oop_demo     # a subset
```

Each example is compiled + run on the x86 target and its output diffed against
`examples/<name>.expected`, then also compiled with the `stackvm` target to keep
the target-independent path honest.

## Code generation

The lowering walk in `codegen*.kmm` never names a machine instruction — it
drives an operand stack through the `Backend` interface. To add a target,
implement `<T>Backend : Backend` and register it in `selectBackend`
(`km_compiler.kmm`); nothing in the lexer, parser, AST, or lowering walk
changes.

Objects carry a one-word header holding an integer type id. Structs are flat C
aggregates plus that tag (single inheritance appends fields); classes are
reference types with method inheritance, `abstract`, field defaults, and
virtual dispatch (direct call when monomorphic, otherwise through a per-class
vtable indexed by type id). `x is Type` is a compile-time-expanded type-id
check.

## Status

Working: variables and macros, control flow, functions with the `fn` / `mut` /
`void` contract, integers / floats / strings / bools / arrays, array & string
**slices** (`a[i to j]`), structs and classes with single inheritance and
virtual dispatch, `x is Type`, multi-file modules, and `extern fn` C FFI
(full System V AMD64 ABI).

Not yet self-hosting: the compiler's own parser still lacks a few constructs
its sources use — `throw` in expression position (`x or throw "..."`), `match`
statements — and a handful of runtime intrinsics. `python3 $KOMODO_MAIN
main.km main.km` gets partway and then stops on the first of these.

## License

MIT — see [`LICENSE`](LICENSE). Copyright (c) 2026 Lukas Perkins.
