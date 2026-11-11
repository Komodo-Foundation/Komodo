# Komodo documentation

Start with the first row for your task. Every page is one topic, and every code sample is compiled and run in CI (`tools/check-docs.py`).

## I want to…

| Goal | Read |
|---|---|
| Install and run my first program | [Getting started](getting-started.md) |
| Learn the language | [Language basics](language/basics.md), then [types and objects](language/types.md) |
| Look up an exact syntax rule | [Language reference](language/reference.md) |
| Know what a number type does | [Integers and `f32`](language/integers.md) |
| Use a C or C++ library | [C and C++ interop](interop.md), then the [FFI reference](ffi.md) |
| Run, build, test or format a project | [The `komodo` command](cli.md) |
| Understand an error or debug a crash | [Diagnostics](diagnostics.md), [Debugging](debugging.md) |
| Edit the compiler or add a feature | [Compiler internals](internals.md) |
| Set up an editor | [Editor support](../editors/README.md) |
| See what is planned | [Roadmap to 1.0](ROADMAP.md), [Handoff](HANDOFF.md) |

## Reference pages

- [Language reference](language/reference.md): every syntax form and rule, numbered, with a compile-error catalogue (section 21).
- [Error codes](errors/): one page per code (`K0001` and so on). Run `komodo explain K0001` to read one in the terminal.
- [Design notes](design-notes/): decisions that are made but not yet fully built, such as typed IR and first-class types.

## For AI agents

Read [`../llms.txt`](../llms.txt) first. It lists the pages above with one line each. [`../CLAUDE.md`](../CLAUDE.md) holds the rules for changing the repository.
