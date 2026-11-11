# Diagnostics

The compiler reports problems in two layouts. Both carry the same information.
The layout is chosen for you, and you can override it.

| Layout | When | Shape |
|---|---|---|
| short | output is not a terminal (pipes, files, editors, CI, `komodo check` in a script) | `file:line:col: error: message`, one line each |
| rich | output is a terminal, or you ask for it | source snippet, caret, error code, help and explanation |

The short layout is the stable, machine-readable one. Editors, the `km-lsp`
language server and the test suite parse it, so it never changes.

## The rich layout

```text
error[K0001]: undefined name 'cont'
 --> app.km:5:19
  |
3 | void main()
4 |     let count = 10
5 |     console.print(cont)
  |                   ^^^^ undefined name 'cont'
  = help: did you mean 'count'?
  = help: Check the spelling, or declare 'cont' with `let cont = ...` before this line.
  = why: The compiler looked for a variable, parameter, macro or function called 'cont' and found
         none in scope. Names must be declared with `let` (or as parameters) before the line that
         uses them.
  = explain: run `komodo explain K0001` for more detail

error: aborting due to previous error
```

- The header carries a stable code (`K0001`). The code never changes meaning.
- The caret line underlines the token the error points at and repeats the message.
- Up to two lines of context come before the line.
- `help` says how to fix it. When the compiler has a spelling suggestion it is
  shown first.
- `why` states the rule that was broken, in plain words.
- Tabs are shown as four spaces and the carets stay aligned. A long line is cut
  with `...` around the error, to fit the terminal width.
- Colour is used only when output is a terminal. `NO_COLOR` turns it off.

An error about a whole block, such as a function that does not return on every
path, underlines the block:

```text
error[K0064]: function 'pick' must return a value on every path
 --> pick.km:1:1
  |
1 | / fn pick(n: int): int
2 | |     if n > 0 then
3 | |         return 1
  | |________________^ function 'pick' must return a value on every path
  = help: Add a final `return`, or an `else` branch that returns.
  = why: A function with a return type must end in a `return` (or `throw`) no matter which branch
         runs. Here control can reach the end of 'pick' without returning anything.
  = explain: run `komodo explain K0064` for more detail
```

Warnings (lints) use the same layout, with `warning[K0164]` in the header and
no abort line.

A message the catalog does not know still gets the snippet and the caret. It
just has no code, help or explanation.

## Choosing the layout

```bash
komodo app.km                  # rich on a terminal, short otherwise
komodo --errors=rich app.km    # always rich (no colour unless on a terminal)
komodo --errors=short app.km   # always the one-line form
komodo --explain app.km        # same as --errors=rich
KM_ERRORS=short komodo app.km  # the same, from the environment
NO_COLOR=1 komodo app.km       # rich without colour
```

`komodo check` follows the same rule, using its standard output to decide.

The short layout is unchanged:

```km
import console

void main()
    let count = 10
    console.print(cont)
```

```text
example.km:5:19: error: undefined name 'cont' (did you mean 'count'?)
```

## Several errors in one run

An error inside one function body no longer hides the errors in the others.
The analyser records the error, goes on to the next function, and reports them
all together, at most 20. The exit status is 1.

```km
fn a(): int
    return "x"

fn b(): int
    return zz

void main()
    break
```

```text
example.km:2:5: error: return value of 'a': type 'string' is not assignable to 'int'
example.km:5:12: error: undefined name 'zz'
example.km:8:5: error: `break` outside a loop
```

Each function reports at most one error, the first it finds, because an error
in the middle of a body leaves nothing reliable to check the rest against.
Declaration-level errors (a duplicate type, a bad inheritance chain) and syntax
errors still stop at the first one: later errors would mostly be consequences of
it.

## Error codes: `komodo explain`

Every catalogued error has a code. `komodo explain` prints its long
explanation: what went wrong, how to fix it, a minimal wrong program and the
corrected one.

```bash
komodo explain K0001
```

The full list of codes is in [errors/README.md](errors/README.md). Every example
on those pages is compiled by `tests/diag/run.sh`, so they stay true.

Here is one of them. A pure `fn` may not change what its caller can see:

```km
struct Box
    v: int = 0

fn poke(b: Box): int
    b.v = 9
    return b.v

void main()
    console.print(poke(new Box()))
```

```text
error: pure `fn poke` mutates state — declare it `mut`
```

Declaring it `mut` fixes it:

```km
struct Box
    v: int = 0

mut poke(b: Box): int
    b.v = 9
    return b.v

void main()
    console.print(poke(new Box()))
```

```text
9
```

## For contributors: adding or changing a message

Nothing in the analyser knows about codes. The renderer works on the finished
one-line message and looks it up in a catalog:

1. Edit `docs/errors/catalog.txt`. An entry gives the code, a message pattern
   (`*` matches any text and is available as `{1}`, `{2}` in the help and why
   text), the `why` and `help` text, and a wrong/fixed example pair.
2. Run `tools/gen-diag.py`. It rewrites `modules/support/diag_catalog.kmm` (read
   by the compiler) and the `docs/errors/Kxxxx.md` pages (read by
   `komodo explain`).
3. Run `tests/diag/run.sh`. It compiles every example, checks that the bad one
   reports that entry's code and that the fixed one is clean, and checks that
   every entry has its why and help text.

The first matching entry wins, so put specific patterns before general ones.
The renderer is `modules/support/diag.kmm`; it runs only when the rich layout
was requested, so a successful build pays nothing for it.
