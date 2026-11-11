# Working on Komodo (instructions for Claude sessions)

Komodo is a self-hosting language/compiler (x86-64 NASM, portable C, WebAssembly, stack VM). Read `docs/HANDOFF.md` for the current state and open work, `docs/ROADMAP.md` for the road to 1.0, `docs/internals.md` for the pipeline.

## Standing rules from the maintainer
1. **Verify, never assume.** Check any Komodo language/tooling claim against `docs/language/reference.md` or by compiling a snippet (`./komodo -q file.km`). Never say a feature is missing without trying it (range loops exist: `for i = 0 to 5`, `down to`, `until`, `for x in xs`). `python3 tools/check-docs.py` runs every doc snippet and is in CI. If unsure, ask the maintainer; do not guess.
2. **Additive, not rewrites.** Extend the codebase in small mergeable steps; keep every current program compiling. Deprecate before removing, and only with the maintainer's approval.
3. **Fast and lightweight.** `tools/perf-budget.sh` enforces size/speed budgets (kmc, source lines, seed, self-compile, hello build). Measure before/after; raise a budget only with a stated reason.
4. **Subagents:** prefer Haiku; Sonnet only for hard compiler work. Usage is tight.
5. **Pushing:** push finished work and open PRs without asking; never force-push; don't enable auto-merge. Do not touch another session's branch (check `gh pr list` and `git branch -r` first).

## Build and test
- `./build-self.sh` builds `./kmc` (~2 s) from the checked-in seed `bootstrap/main.km.asm.gz`; must print `fixpoint OK`. After compiler changes run `./build-self.sh --force --update-seed` and commit the seed. New syntax must not be used in the compiler's own sources until a seed supports it.
- Suites: `./scripts/run-tests.sh` (examples on x86/stackvm/c/wasm + interop), `./scripts/run-sema-tests.sh`, `bash tests/{bind,debug,widths,diag,fmt,doc}/run.sh`, `python3 tests/lsp/run.py`, `python3 tools/check-docs.py`, `tools/perf-budget.sh`. `komodo self test` runs the main ones.
- Restore generated build records before committing: `git checkout -- '*.deps'`.
- NASM 3 rejects `$name` in `extern`/`global`; CI uses the latest Ubuntu nasm and emsdk 3.1.6.

## Decisions already made
- First-class `type` values (compile-time + runtime), ids = 64-bit hash of `module::Name`, `Box<int>` syntax, `new Box(7)` infers, object header unchanged: `docs/design-notes/type-values.md`.
- Typed IR = annotate the existing AST, monomorphize in sema: `docs/design-notes/typed-ir.md` (generic returns must agree; widths/bool/char are distinct overloads; uncalled generics unchecked; `Type` moves to `modules/support/`).
- Integer widths: wrap on overflow, explicit narrowing, packed arrays/fields (phase 2, not done).
