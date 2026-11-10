# Komodo for VS Code

Language support for [Komodo](https://github.com/Komodo-Foundation/Komodo)
programs (`.km`) and modules (`.kmm`).

## Features

| Feature | How |
|---|---|
| Syntax highlighting | TextMate grammar, also inside ` ```km ` blocks in Markdown |
| Errors and warnings as you work | the Komodo language server (`tools/km-lsp`) runs the compiler on open and save |
| Outline, breadcrumbs, Go to Symbol | top-level `fn`, `struct`, `class`, `enum`, `module` from the language server |
| Format Document | `komodo fmt` (whitespace only, never changes code) |
| Run, build, check, test | commands, the ▷ button in the editor title, `Ctrl+F5`, and `komodo` tasks |
| Problems panel | the `$komodo` problem matcher reads `file:line:col: error: message` |
| Debugging | `F5` builds with `komodo build -g` and starts gdb or lldb on Komodo source lines |
| Editing | comment toggling, bracket and quote pairs, indentation, `/** */` continuation, indentation folding and `// region` markers |
| Snippets | `main`, `fn`, `void`, `mut`, `class`, `struct`, `enum`, `module`, `if`, `ifelse`, `for`, `forin`, `while`, `match`, `import`, `letbe`, `try`, `test`, ... |
| File icon | `.km` and `.kmm` files show the Komodo icon in themes that use language icons |

## Requirements

Install Komodo so `komodo` is on your `PATH` (`./install.sh` in a checkout).
The language server also needs `python3`. Run **Komodo: Check Toolchain** to
see anything missing.

Debugging needs one of:
[C/C++](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools) (gdb) or
[CodeLLDB](https://marketplace.visualstudio.com/items?itemName=vadimcn.vscode-lldb).
It works on the default `x86` target.

## Commands

| Command | What it does |
|---|---|
| Komodo: Run File | `komodo run <file>` (`Ctrl+F5`) |
| Komodo: Build File | `komodo build <file>` |
| Komodo: Check File | `komodo check <file>`, results in Problems |
| Komodo: Run Tests | `komodo test` in the workspace folder |
| Komodo: Debug File | build with debug info and start the debugger |
| Komodo: Open REPL | `komodo repl` in a terminal |
| Komodo: Check Toolchain (doctor) | `komodo doctor` |
| Komodo: Restart Language Server | restart `km-lsp` |

## Tasks

The extension provides `komodo: run`, `komodo: build`, `komodo: check` and
`komodo: test` for the workspace project. In `tasks.json`:

```json
{
  "type": "komodo",
  "command": "build",
  "args": ["--release"],
  "problemMatcher": ["$komodo"],
  "group": "build"
}
```

## Debugging

With no `launch.json`, `F5` on a `.km` file just works. To keep a
configuration:

```json
{
  "type": "komodo",
  "request": "launch",
  "name": "Debug Komodo file",
  "program": "${file}",
  "args": []
}
```

The executable goes to `.komodo/debug/` next to the file.

## Settings

| Setting | Default | Meaning |
|---|---|---|
| `komodo.path` | `komodo` | the `komodo` command |
| `komodo.target` | `x86` | target for Run and Build: `x86`, `c`, `wasm`, `stackvm` |
| `komodo.languageServer.enabled` | `true` | start the language server |
| `komodo.languageServer.path` | empty | `km-lsp` to use; empty finds `tools/km-lsp` in the workspace or next to `komodo` |
| `komodo.debugger` | `auto` | `cppdbg`, `lldb`, or `auto` |

To format on save, add `"[komodo]": { "editor.formatOnSave": true }` to your settings.
