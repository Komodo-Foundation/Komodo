# Changelog

## 0.3.1

- Komodo Dark: conventional syntax colors (muted keywords, strings, numbers, types, comments); green kept as the accent.

## 0.3.0

- Komodo Dark color theme: near-black background, Komodo green accent, tuned for Komodo token scopes.

## 0.2.1

- Breakpoints can be set in `.km` and `.kmm` files.
- The debugger finds Komodo sources when the working directory is not the file's directory.

## 0.2.0

- Language server client for `tools/km-lsp`: compiler diagnostics on open and save, document outline.
- Format Document through `komodo fmt`.
- Commands: Run, Build, Check, Debug File, Run Tests, Open REPL, Check Toolchain, Restart Language Server.
- `komodo` task type and `$komodo` problem matcher.
- `komodo` debug type: builds with `komodo build -g`, debugs with gdb (C/C++) or lldb (CodeLLDB).
- Grammar: type names, TODO markers in comments, built-in conversions, Komodo blocks in Markdown.
- More snippets, doc-comment continuation, `// region` folding, file icon.

## 0.1.0

- Syntax highlighting, indentation and snippets.
