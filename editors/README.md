# Editor support for Komodo

This directory holds editor modes for Komodo source files (`.km` programs and
`.kmm` modules), and `tools/km-lsp` is a language server that any LSP-capable
editor can use for diagnostics and document symbols.

| Path | What it gives you |
|---|---|
| `vscode/` | VS Code extension: highlighting, diagnostics and outline (via `km-lsp`), formatting, run/build/test tasks, debugging, snippets. See `vscode/README.md` |
| `vim/` | Vim and Neovim: syntax, filetype detection, four-space indentation |
| `emacs/komodo-mode.el` | Emacs major mode: font-lock, comments, indentation |
| `check-grammar.py` | checks every keyword in the lexer is covered by each grammar |
| `../tools/km-lsp` | language server: compiler diagnostics and top-level symbols |

The keyword list comes from `modules/frontend/lexer.kmm`. When you add a
keyword there, the check fails until each grammar has it.

## Language server

`tools/km-lsp` is an executable Python 3 script. It needs only Python 3 and
the `komodo` compiler. It uses `$KOMODO` if that is set, then the compiler in
the repository it lives in, then `komodo` on `PATH`.

What it does:

- Diagnostics come from the compiler. The file is checked on open and on
  save. Edits in between only update the buffer, so the compiler does not run
  on every keystroke.
- A buffer with unsaved changes is checked as it is in the editor, from a
  temporary copy of the source tree. The project directory is never written to.
- Document symbols list the top-level `fn`, `mut`, `void`, `struct`, `class`,
  `enum` and `module` declarations.
- Not yet: completion, hover, go-to-definition, formatting.

Point your editor at the script with its absolute path, for example
`/home/you/Komodo/tools/km-lsp`. It speaks over stdio, so no arguments are
needed. Set the language id to `komodo` and the file pattern to `*.km` and
`*.kmm`.

## VS Code

From the Marketplace (search "Komodo" in the Extensions view, or):

```bash
code --install-extension komodo-foundation.komodo-lang
```

From a `.vsix` file, built from a checkout with `vsce` (needs Node):

```bash
cd editors/vscode && npm install && npx @vscode/vsce package    # writes komodo-lang-0.2.0.vsix
code --install-extension komodo-lang-0.2.0.vsix
```

Or, while working on the extension, run `npm install` there and link the folder so edits show up on reload:

```bash
ln -s /path/to/Komodo/editors/vscode ~/.vscode/extensions/komodo-foundation.komodo-lang-0.2.0
```

Reload the window (Developer: Reload Window). `.km` and `.kmm` files now
open as Komodo. The extension starts `tools/km-lsp` itself (from the open
workspace, or next to the `komodo` on `PATH`), so no separate LSP client is
needed. See `vscode/README.md` for commands, tasks, debugging and settings.

## Vim and Neovim

Add the directory to the runtime path. For Vim, put this in `~/.vimrc`:

```vim
set runtimepath+=/path/to/Komodo/editors/vim
```

Or copy `vim/syntax`, `vim/ftdetect` and `vim/ftplugin` into `~/.vim/`.

Diagnostics with Neovim's built-in client, in `init.lua`:

```lua
vim.api.nvim_create_autocmd('FileType', {
  pattern = 'komodo',
  callback = function()
    vim.lsp.start({
      name = 'km-lsp',
      cmd = { '/path/to/Komodo/tools/km-lsp' },
      root_dir = vim.fs.dirname(vim.api.nvim_buf_get_name(0)),
    })
  end,
})
```

With `vim-lsp`:

```vim
if executable('/path/to/Komodo/tools/km-lsp')
  au User lsp_setup call lsp#register_server({
        \ 'name': 'km-lsp',
        \ 'cmd': {server_info -> ['/path/to/Komodo/tools/km-lsp']},
        \ 'allowlist': ['komodo'],
        \ })
endif
```

## Emacs

Add the directory to `load-path` and load the mode in your init file:

```elisp
(add-to-list 'load-path "/path/to/Komodo/editors/emacs")
(require 'komodo-mode)
```

The mode opens `.km` and `.kmm` files. Indentation is four spaces; change it
with `komodo-indent-offset`.

Diagnostics with Eglot (built into Emacs 29 and later):

```elisp
(add-to-list 'eglot-server-programs
             '(komodo-mode . ("/path/to/Komodo/tools/km-lsp")))
```

With `lsp-mode`:

```elisp
(lsp-register-client
 (make-lsp-client
  :new-connection (lsp-stdio-connection "/path/to/Komodo/tools/km-lsp")
  :major-modes '(komodo-mode)
  :server-id 'km-lsp))
```

## Tests

```bash
python3 editors/check-grammar.py    # keywords vs. every grammar; JSON parses
python3 tests/lsp/run.py            # starts km-lsp and drives it over LSP
```

`tests/lsp/run.py` needs the repository's `komodo` binary. It fails if the
server does not publish the expected diagnostic, if a change triggers a
compile, if the compiler writes into the project, or if the server does not
exit cleanly after `shutdown`.

## Limits

- The Vim syntax file was checked by loading it in Vim and comparing the
  highlight group of each token. The Emacs mode has not been loaded in Emacs
  yet.
- The highlighting is by keyword and pattern, not by parser. A keyword such as
  `in` inside a string or comment is not highlighted as code.
