# Road to Komodo 1.0

1.0 means: the language is stable (no breaking syntax changes without a major
version), the documented behaviour matches the compiler, and a newcomer can
write a useful program with only the standard library. Status as of 0.1.0.

## 1. Language completeness
- [ ] **Collections.** There is no map/dictionary or set, and arrays are the only
      container. Needs a design decision (built-in `{K: V}` type vs generic
      library classes) — the single biggest gap.
- [ ] **Generic types.** Only functions can be implicit-type generics; classes and
      structs cannot take type parameters.
- [ ] **Template bodies** are not type-checked per instantiation; `module.fn()`
      inside a template fails at link time.
- [ ] **By-value C structs:** nested structs and arrays inside them.
- [ ] **Syntax gaps:** ranges as values (`let r = 1 to 4`), one-line
      `if … then … else …`, `?` on indexing.
- [ ] **Spec = compiler.** (`tools/check-docs.py` now verifies every doc snippet in CI.) Every rule in `docs/language/reference.md` has a conformance test, and
      every divergence is fixed on one side.

## 2. Standard library
- [ ] `os` / `env` (arguments, environment, exit codes), `time`, `random`,
      `path`, `process` (run a command, capture output)
- [ ] `json` (parse/emit), `fmt`-style formatting, `net` (TCP client) — stretch
- [ ] Documented, tested, and available on every target that can support it.

## 3. Correctness and quality
- [ ] Differential testing: every example produces identical output on x86, c and
      wasm (`run-tests.sh` already runs all three).
- [ ] Fuzz the lexer/parser/analyser: no crash on any input; errors carry
      `file:line:col`.
- [ ] Memory: leak/ASan runs of the runtime, bounded-heap tests.
- [ ] Error messages reviewed for the 20 most common mistakes.

## 4. Tooling
- [ ] `komodo fmt` (canonical formatter) and `komodo check` (analyse only).
- [ ] Editor support: syntax highlighting grammar, then a language server.
- [ ] `komodo doc` — generate API docs from comments.
- [ ] A package story for sharing modules (git URL imports).

## 5. Platforms
- [x] Linux x86-64 (native), portable C, WebAssembly, stack VM
- [ ] FFI on the `c` and `wasm` targets
- [ ] macOS and arm64 (portable-C backend first)

## 6. Release engineering
- [x] CI (build, suites, wasm), Debian package, installer, Docker image
- [ ] Versioning policy + deprecation rules in `CONTRIBUTING.md`
- [x] Size/speed budgets enforced in CI (`tools/perf-budget.sh`)
- [ ] Reproducible release builds, checksums, signed tags
- [ ] Website/landing page built from `docs/`
- [ ] A "release candidate" period with at least three real projects written in
      Komodo and no open P0/P1 bugs.
