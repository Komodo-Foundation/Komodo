# Design notes: developer experience and the language

Status: proposal. Nothing here is implemented. Each item is a small,
additive step with a recommended default; pick the ones to pursue.

The ideas start from what Komodo already is rather than from other
languages: a tiny self-hosting compiler, four backends from one front end,
a `fn` / `mut` / `void` split that the analyser enforces, docs whose
examples are real program output, and a perf-budget culture.

Facts below were checked against `main` (48dd2b6) by building `kmc` with
`./build-self.sh` and compiling the snippets shown.

## Findings that motivate this

1. **The docs drift.** `docs/language/basics.md` (Diagnostics) says the
   "pure `fn` mutates state" and "must return a value on every path" errors
   print without a location. They now do:
   `a.km:4:1: error: pure `fn poke` mutates state — declare it `mut``.
   Fixed in this PR, but nothing stops the next drift.
2. **Integer widths are not enforced.** `let a: u8 = 250; a = a + 10`
   prints `260`; `let b: i8 = 127; b = b + 1` prints `128` (x86 and C).
   `types.md` lists `u8`..`u64`, `i8`..`i64` as types, so this is silent
   wrong output.
3. **A null `int?` prints as `0`.** `let a: int? = null; print(a)` prints
   `0`, the same as a real zero.
4. **`kmc` with no arguments crashes** with
   `Uncaught error: array index 1 out of range (length 1) in main`
   instead of printing its usage.
5. **A protected call drops the error message.** `f(x)?` gives `null`; the
   text passed to `throw` is gone.

## Developer experience

### D1. Executable docs (recommended first)

A script, `tools/check-docs.sh`, extracts each ```` ```km ```` block and
the ```` ```text ```` block right after it from `docs/**/*.md`, compiles
and runs it, and diffs the output. An HTML comment such as
`<!-- doc-test: skip -->` or `<!-- doc-test: strict -->` above a block
controls it. Run it in CI next to `run-tests.sh`.

- Pros: the docs already follow this shape, so most blocks work as is.
  It turns the "verify language claims against the docs" rule into a
  check, and finding 1 could not happen again. No compiler change.
- Cons: blocks that need arguments, files or a missing toolchain need a
  skip marker. Adds CI time (small: the examples are tiny).
- Default: do it, skip-mark what does not fit, fix what it finds.

### D2. `komodo check` for editors, instead of an LSP

Add `komodo check [file]`: run the analyser only (no codegen, no link)
and print the existing `file:line:col: error|warning: ...` lines. The VS
Code extension runs it on save with a problem matcher, so errors show
inline.

- Pros: tiny; reuses the diagnostics as they are; fast because `kmc` is
  fast. Gives most of what people want from an LSP on day one.
- Cons: no hover, go-to-definition or completion. Needs a `kmc` flag to
  stop after analysis (or the driver discards the output).
- Default: do it; revisit an LSP only if hover/go-to is actually missed.

### D3. One test format: expectations inside the file

Let a test file say what it expects in trailing comments:

```km
void main()
    print(2 + 2)

// expect: 4
```

and `// expect-error: undefined name 'x'` for analyser tests.
`komodo test` checks stdout (or the diagnostic) against them, in addition
to the exit status it checks today.

- Pros: one format for users and for the repo; `examples/*.expected` and
  `tests/sema/*.experr` can move over gradually (or never). The test and
  its answer sit together.
- Cons: a second convention while the old files remain.
- Default: add it to `komodo test`; leave existing scripts alone.

### D4. Backend drift as a test mode

`komodo test --all-targets` runs each test on every runnable backend
(x86, C, and wasm when `emcc`/`node` exist) and fails when their outputs
differ, naming the backend that disagrees.

- Pros: four backends are Komodo's distinctive promise, and today only the
  repo's own `run-tests.sh` checks it. Users writing libraries get the same
  guarantee. Small: it is a loop in the driver.
- Cons: test time scales with backends. Opt-in, so cost is chosen.
- Default: add the flag; use it in the repo CI.

### D5. Small fixes (safe, can be done now)

- `kmc` with no arguments prints its usage line (finding 4). Needs a seed
  refresh, so bundle it with the next compiler change.
- Doc fix for finding 1 (in this PR).

## Language

### L1. Facts: compile-time examples on pure functions

`fn` is already checked pure. Let a `fact` line follow a function and
state an example that must hold:

<!-- doc-skip -->
```km
fn square(x: int): int
    return x * x
fact square(3) == 9
fact square(-2) == 4
```

Step 1: `komodo test` (and `--strict`) evaluate every `fact` and report a
failure at its line. Step 2: the optimizer evaluates facts at compile time
(the folder already folds literal operations) so a false fact is a compile
error, and facts never reach the binary.

- Pros: examples live next to the code and cost nothing at run time. It
  makes the purity rule pay off visibly: only `fn` can have facts, because
  only `fn` is guaranteed to have no side effects. Fits the "examples are
  real output" doc culture.
- Cons: a new keyword. Step 2 needs a compile-time evaluator for `fn`
  bodies, which is real work; step 1 does not.
- Default: step 1 only; decide on step 2 after using it.

### L2. Pure macros

`let NAME be EXPR` substitutes text, and basics.md warns that a body that
reads a variable sees its value at each use. Restrict a `be` body to
literals, other macros and calls to `fn` (pure) functions.

- Pros: removes a documented footgun; with L1 step 2 a macro becomes a
  true constant.
- Cons: breaks macros that read variables.
- Default: warn first (deprecate), make it an error later with approval.

### L3. Integer widths that mean something (the open decision)

The open choice after const arrays is (a) nested/struct const arrays vs
(b) real integer-width semantics. Finding 2 tips it: (b) is silent wrong
output for types the docs already advertise, while (a) is a missing
feature that fails loudly.

Proposed rule: arithmetic on a sized integer wraps to its width when it
is stored (`u8` 250 + 10 stores `4`; `i8` 127 + 1 stores `-128`), and
`--strict` reports a literal that does not fit (`let a: u8 = 300`).

- Pros: matches what the C backend and C callers already assume at a
  boundary; fixes wrong output.
- Cons: every backend must truncate on store (a mask or sign-extend per
  store; measure against the perf budget). Wrap vs trap is a design call.
- Default: (b) first, wrap on store; (a) after.

### L4. `null` prints as `null`

Make `print` / string conversion of a null nullable produce `null`
(finding 3).

- Pros: removes an ambiguity that hides bugs.
- Cons: changes output of existing programs that print nullables.
- Default: do it; it is a bug fix in effect.

### L5. Keep the error message of a protected call

Add a built-in `last_error()`: after a protected call `f(x)?` returns
`null`, it gives the thrown message (or `null` if the last protected call
succeeded).

- Pros: no new syntax; `?` stays as it is.
- Cons: hidden state, though it is per protected call and easy to reason
  about in straight-line code.
- Default: optional; lower priority than L1–L4.

## Suggested order

D1, then L3 and L4 (wrong output first), then D2, L1 step 1, D3, D4.
L2 and L5 when convenient. REPL work stays last.
