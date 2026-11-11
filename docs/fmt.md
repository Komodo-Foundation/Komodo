# km-fmt: the Komodo formatter

`tools/km-fmt` normalises whitespace and layout in `.km` and `.kmm` files. It
is a conservative formatter: it changes only spacing, blank lines, trailing
whitespace and tab indentation. It never reorders code, never reflows lines,
and never changes program meaning. It uses only the Python 3 standard library.

## Usage

```bash
tools/km-fmt file.km              # print the formatted text to stdout
tools/km-fmt -w file.km           # rewrite the file in place
tools/km-fmt --check .            # list files that would change; exit 1 if any
tools/km-fmt --diff tests/examples/     # print a unified diff; exit 1 if any changed
tools/km-fmt -w .                 # rewrite every .km/.kmm under the directory
```

- Directories are searched recursively for `.km` and `.kmm` files. Hidden
  directories and `__pycache__` are skipped.
- Printing to stdout takes exactly one file. Several files need `--check`,
  `--diff` or `-w`.
- `--check` and `--diff` exit 0 when nothing would change, 1 when something
  would change, and 2 on error. `-w` exits 0 on success and 2 on error.
- An error (such as an unterminated string, or indentation that tab expansion
  would change) is reported as `km-fmt: file:line: message` on stderr. The
  file is left alone and the exit status is 2.

## Rules

Tokens are read the way `modules/frontend/lexer.kmm` reads them. Strings,
char literals, backtick strings with their `{…}` holes, and comments are never
rewritten, apart from trailing whitespace on comment lines.

Whitespace and layout:

1. Tabs in the indentation become 4 spaces each. Other indentation is kept.
   Tabs elsewhere on a line become a single space.
2. Trailing whitespace and `\r` at the end of a line are removed. Line endings
   are not changed otherwise.
3. Blank lines: leading blank lines are removed. A run of blank lines is cut
   to at most 2 at top level, and to at most 1 when the next line is indented.
4. Exactly one final newline is written. A file with no code becomes empty.

Spacing between tokens on one line:

5. Binary operators (`== != < <= > >= + * / % = += -= *= /= %= & | ^ << >> ->`,
   and `-` when it is binary) have one space on each side.
6. Unary `-`, `!` and `#` have no space after them.
7. A comma has no space before it and one after it.
8. `(` and `[` have no space after them, and `)`, `]` have none before them.
   `.` and `::` have no space around them. `?` has no space before it, so
   `f(x)?` and `T?` stay tight.
9. After a `:` that is a separator (parameters, fields, `let x: T`), there is
   one space. Inside `[...]` (slices such as `a[1:]`) the spacing is kept as
   written.
10. A `(` or `[` that follows a name or `)` keeps its spacing (`f(x)`,
    `constructor(src)`, `xs[0]`). Other spacing between words collapses to one
    space.
11. A trailing `//` comment keeps the spacing written before it, with tabs
    widened, so aligned comments stay aligned.

The spacing before `:` is kept as written, so `b : int` stays `b : int`.

## What is checked before a file is reported

Each rewrite is checked. If a check fails, the file is refused with an error:

- The significant token stream of the output (kinds and texts, excluding
  comments) must equal that of the input. The output is re-tokenised to
  confirm this.
- Indentation: the lexer's INDENT and DEDENT decisions at every line start
  must be the same before and after tab expansion. A file that mixes tabs and
  spaces in a way that would change block structure is refused, and must be
  fixed by hand.

## Tests

`tests/fmt/run.sh` runs three groups of checks:

- Golden tests: `tests/fmt/golden/input.km` formats to `expected.km`, and
  `mixed_indent.km` is refused.
- Safety: each `tests/examples/*.km`, `stdlib/*.kmm`, `modules/**/*.kmm`, `main.km`
  and `tests/sema/*.km` file is compiled with `kmc <file> x86`, once as written
  and once formatted, in two copies of the tree. The exit status, output and
  generated `.asm` must match. For `tests/sema`, the diagnostics must match
  once the column numbers are removed.
- Idempotence: formatting the formatted tree again changes nothing, and
  `--check` passes on it.

Run it with `tests/fmt/run.sh`. It builds `./kmc` with `./build-self.sh` if
needed.

## Not done

- No line wrapping, no reordering of imports or declarations, and no changes
  to alignment inside code.
- Layout inside brackets is not reflowed. Continuation lines keep their
  indentation, except for tab expansion.
- The formatter does not check the program. A file that does not compile
  before formatting still does not compile after it.
