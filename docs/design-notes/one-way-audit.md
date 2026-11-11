# One way to do each thing: audit

Status: proposal, nothing removed. Each item needs the maintainer's approval
before anything is deprecated, and removal only follows a deprecation step.

Method: read `docs/language/*.md`, the stdlib, and the examples; compile
the snippets that matter (`./komodo -q`). Items marked **verified** were
compiled. The others come from the docs and stdlib source and still need a
snippet check before any change.

Usage counts are files under `.km`/`.kmm` that contain the form. `!` is
inflated by `!=`, so that count is not reliable.

## Verified duplicates

| # | Keep | Deprecate | Verified | Uses in repo | Notes |
|---|---|---|---|---|---|
| 1 | `and`, `or` | `&&`, `\|\|` | yes: `print(1 && 1, 1 && 0, 0 \|\| 1)` prints `1 0 1` | `&&` 50 files, `\|\|` 36 | Not in the reference at all. Silent alias. Strongest candidate. |
| 2 | `not` | `!` | yes: `print(!0)` prints `1`; the reference (6.4, open Q2) says `!` is unreliable | many (grep is noisy) | The doc already calls it broken. |
| 3 | `then` (or nothing) | `do` on `if` | yes: `if a == 1 do` compiles | 0 files for `if ... do` | `do` is allowed on `while`/`for`. Keep it there only if the maintainer wants it. |
| 4 | `#x` | `x.length` or `len`, pick one | per docs 4.3 and 6.6, not re-verified | n/a | Both give the same result. Pick one name. |
| 5 | `print` (builtin) | `console.print` for plain output | per docs, not re-verified | `console.print` 113 files | `console.write` and `console.error` add power (no newline, stderr). Keep `console` for those, drop plain `console.print` only if the maintainer agrees. Larger migration. |
| 6 | `s.length` / `#s` | `string.length(s)` | per stdlib/string.kmm:14 | 4 files | Module form adds nothing. |
| 7 | `extern fn f(): T` | `extern void f()` | per reference 19.1 | 17 files | Only the return type differs. |
| 8 | `shared struct` (or just `struct`) | `shared struct` | per reference 15.2 | 7 files | Docs use both and do not say they are equal. Decide which one is the spelling. |
| 9 | `m::f` for modules | `m.f` for module functions | per reference 14 and 18 | not counted | Docs say `file` members are `::` only. Pick one rule for every module. |
| 10 | `math.abs/min/max/pow` overloads | `math.fabs`, `math.powf` | per stdlib/math.kmm | `fabs` 1 file, `powf` 2 files | The language has overloading (9.6). The suffixed names are not needed. |

## Sprawl to decide together

These are not pure duplicates. Each is a set of overlapping mechanisms for
one job. They are the biggest design question.

- **Errors and absence (item A).** Four ways to say "no value":
  `throw` with `f()?` (the error becomes `null`), `T?` returns, `value or throw "msg"`,
  and `assert`. `?` swallows any error and `T?` only signals absence.
  Recommended default: keep `throw` and `T?`. Drop `f()?` and `or throw`, or keep
  `or throw` as the only shorthand. Needs a decision.
- **Link and extern libraries (item B).** `link Name` (module only) and
  `extern "lib"` both add libraries. They resolve paths differently
  (module directory vs cwd). Recommended default: keep `extern "lib"` and drop
  `link`, or document the path rule and keep both.
- **Modules: `::` vs `.` (item C).** See item 9 above.

## Not duplicates (keep as is)

- `string(x)` builtin vs the `string` module: name collision only.
- `copy(x)`: one builtin, two meanings chosen by context (15.2 vs 19.8).
- `index_of` and `contains` in `algorithm` and `string`: different types.
- `extern fn name = "sym"` vs plain `extern fn`: the rename is real power
  (for C names that clash with keywords).
- `os.getenv` vs a raw `extern` wrapper: the stdlib wraps a libc call that
  already has an `extern` form; the `extern` form is the escape hatch, not a
  competing style.
- `file.exists` vs `path.exists`: different results for directories. Fix the
  docs or the names, do not remove either.
- `assert` vs `throw`: `assert` adds file and line and takes a condition.

## Doc conflict found (not a duplicate)

`docs/ffi.md` says exporting to C is "not covered (see Planned)", but
`reference.md` 19.9 documents `export fn`.

## Suggested first step

Add deprecation warnings for items 1, 2 and 3 to `kmc lint` (PR #16), since
they are the cheapest to migrate. Each warning should name the replacement.
Leave the others for the maintainer to choose.

## Questions for the maintainer

1. Approve deprecating `&&`, `||` and `!`? (Recommended: yes.)
2. Plain output: `print` only, or `console.print` too? (Recommended: keep `print`.)
3. Error and absence: keep `throw` plus `T?` and drop `f()?` and `or throw`? (Recommended: yes.)
4. Module access: `::` only, or `.` only? (Recommended: `::`.)
5. Keep `link`, or only `extern "lib"`? (Recommended: keep `extern "lib"` only.)
