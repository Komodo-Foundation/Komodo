# Changelog

All notable changes. Versions follow [Semantic Versioning](https://semver.org/);
the number lives in [`VERSION`](VERSION).

## 0.1.0 — first public release

- Self-hosting compiler: x86-64 (NASM), stack-VM, portable C, WebAssembly
  (`-t x86|stackvm|c|wasm`), and a position-independent x86 mode for shared
  libraries.
- Language: structs and classes with single inheritance and virtual dispatch,
  enums and `match` with struct patterns, generic and overloaded free functions,
  default parameter values, `assert`, nullable types with flow-sensitive
  narrowing, `throw` / `f(args)?` error handling, unique/move ownership with
  deep `copy()`, string interpolation, slices, hex/binary literals with `_`.
- Interop: `extern fn` (full System V ABI, variadics, callbacks, by-value
  structs, extern globals, symbol aliases), `export fn` callable from C/C++,
  `komodo lib` (object, `.a`, `.so`, header), `native/` auto-linking,
  `import zlib`-style imports through `tools/km-bind`.
- Packaging: `install.sh` (also `curl | bash`), a Debian package (`packaging/build-deb.sh`), a Dockerfile, CI.
- Tooling: zero-config `komodo` command (`run`, `build`, `test`, `watch`,
  `new`, `repl`, `doctor`, `lib`, `bind`, `upgrade`), static analyser with
  `--strict` diagnostics, optimizer, incremental parallel compiler build
  (~1 s), documentation under `docs/`.
