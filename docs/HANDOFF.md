# Handoff — state of the project

Written when work moved from local sessions to the cloud. Branch `feat/repl-and-fixes` (PR #6) holds everything below; nothing is left only on a local machine.

## Done (all merged, all suites green)
Optimizer passes; portable C, WebAssembly (`-t wasm`), position-independent x86 + shared libraries; default parameters, `assert`, literal forms; class `private`, `unique` moves, deep `copy()`, typed `main(args)`; `export fn`, C++ interop, variadic C calls; `import zlib`-style auto-binding (`tools/km-bind`), `native/` auto-link; zero-config `komodo` CLI (`run build test check fmt doc repl new bind watch clean doctor lib self upgrade completions`, `-g` debug builds with DWARF and call-chain traces); `komodo fmt`, `komodo doc`, editor support (`editors/`, `tools/km-lsp`); standard library `os time random path process`; **integer widths phase 1** (wrap, unsigned ops, `u8(x)` conversions, true `f32`: `docs/language/integers.md`); **rich diagnostics** (`error[K0001]`, snippet + caret, `komodo explain`, up to 20 errors per run: `docs/diagnostics.md`); **compile speed** (self-compile 0.5 s -> ~0.3 s, `tools/km-prof`); installer, Debian package, Docker, CI, docs and a verified language reference.

## Overlap to reconcile (cloud session PR #7)
The cloud session's branch `feat/repl-and-fixes-nq5r08` (PR #7 into `main`) was NOT merged here. It overlaps with work now in PR #6: allocation-free peephole helpers (also done here), hash-indexed class member/function sets in `sema.kmm`/`backend.kmm` (also done here), and source-line carets plus `help:` lines on errors (superseded by `modules/support/diag.kmm` + `docs/errors/catalog.txt` here). Rebase/merge it onto PR #6 and keep the PR #6 versions where they do the same thing.

## Open work
- **Typed IR** implementation (13 steps in `docs/design-notes/typed-ir.md`), then **first-class types** (12 steps in `type-values.md`); generic classes/collections (Map/Set) come from these.
- **Integer widths phase 2:** packed `[u8]` arrays and struct/class fields (changes the runtime array/object layout); stack-VM support for unsigned/wrap ops.
- **Reference open questions** (31, end of `docs/language/reference.md`): real bugs include `!` on booleans, `1e3` in calls, ignored enum values, `for c in "ab"` and `join` on `[int]` crashing, lexer hangs on unterminated string/array, `new shapes.Sq(3)` hang, int passed to `string` extern accepted.
- Integer-width open questions: `uint` = `u64`?, float -> `f32` implicit?, shift counts >= width, `x >= -1` on unsigned (lint vs error), u64 from float >= 2^63.
- Diagnostics gaps: one error per function body; parse/registration errors stop at the first; codegen overload errors have no position.
- Remaining roadmap: native calling convention, module system/separate compilation, one memory model, length-prefixed strings, CLI in Komodo, FFI on c/wasm targets, macOS/arm64 (`docs/ROADMAP.md`).
