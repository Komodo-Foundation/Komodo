# km-doc: API reference generator

`tools/km-doc` turns Komodo source files into an API reference: one page per
module, plus an index. It uses only the Python 3 standard library and reads
`.kmm` and `.km` files directly, so it needs no build step and no compiler.

```bash
tools/km-doc [-o outdir] [--format md|html] paths...
```

- `paths` are files or directories. Directories are scanned recursively for
  `*.kmm` and `*.km`.
- `-o outdir` sets the output directory (default `./doc`).
- `--format md` (the default) writes Markdown pages, `index.md` and one
  `<module>.md` per module.
- `--format html` writes a self-contained static site: `index.html` and one
  `<module>.html` per module. Each page embeds its CSS, follows the light or dark
  theme of the browser, and has a search box driven by a small inline script.
  The index filters every public symbol, and a module page filters its own
  members. No other JavaScript is needed to read the pages.

```bash
tools/km-doc -o /tmp/stdlib-doc stdlib/*.kmm
tools/km-doc --format html -o /tmp/stdlib-html stdlib/*.kmm
```

## Doc comment convention

The tool defines the convention below. The standard library follows it.

- **Declarations.** A run of `//` lines written directly above a declaration,
  with no blank line between them and the declaration, documents that
  declaration. This covers:
  - functions (`fn`, `void`, `mut`, `extern`, `export`);
  - `let` globals and `let NAME be EXPR` constants;
  - `struct`, `class` and `enum`;
  - class constructors and methods, struct and class fields, and enum variants.
- **Modules.** The first `//` block after the `module NAME` line describes the
  module. It must be followed by a blank line, so that it is not taken as the
  comment of the declaration below it.
- **Visibility.** Members in a `private` section are not shown. A `public` or
  `private` line applies to the declarations after it, until the next such
  line or the end of its indented block. In a class body the label may sit at
  the class's own column, as in `docs/language/types.md`.
- **Text.** The lines of a block are joined into one paragraph. Backquoted
  text renders as code.

<!-- doc-skip -->
```km
// Module description: the first block after `module`, then a blank line.

// Absolute value of n. The smallest int wraps and stays negative.
fn math_abs(n: int): int
    ...
```

## Output

- **Signatures** are copied from the source as written, with the trailing
  `//` comment removed. Each one sits in a code block.
- **Types** that a signature mentions link to their entry. Structs and classes
  show what they extend and what extends them, and enums list their variants.
- **Anchors** are the declared names (`math_sqrt`), and members are
  `Type.member` (`Counter.bump`). A name declared twice (an overload) gets a
  `-2` suffix.
- **Index.** The index lists the modules with the first sentence of each module
  description.

## Limits

- Each signature is read from one line. A declaration whose parameter list
  spans several lines is not documented correctly.
- A file without a `module` line is named after its file name.
- Two files that declare the same module name: the second is skipped with a
  warning on stderr.
- Function bodies are skipped, so the tool never reads code inside them.

## Tests

`tests/doc/run.sh` generates pages for a fixture module
(`tests/doc/fixtures/shapes.kmm`) and for the whole standard library, then
checks that:

- known entries appear, such as `math_sqrt` with its comment;
- private members stay hidden;
- every relative link resolves to an existing file and anchor, which
  `tests/doc/check_links.py` checks in both formats.

The script exits non-zero on the first failure.
