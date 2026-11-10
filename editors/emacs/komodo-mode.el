;;; komodo-mode.el --- Major mode for the Komodo language (.km, .kmm) -*- lexical-binding: t; -*-

;; Syntax highlighting, comments and four-space indentation for Komodo.
;; Install: put this file on `load-path' and add
;;   (require 'komodo-mode)
;; to your init file.  See ../README.md for details.

;;; Code:

(defgroup komodo nil
  "Major mode for editing Komodo source."
  :group 'languages)

(defcustom komodo-indent-offset 4
  "Spaces per indentation level in Komodo blocks."
  :type 'integer
  :safe #'integerp)

(defconst komodo-keywords
  '("if" "then" "else" "while" "do" "for" "to" "down" "until" "in"
    "match" "case" "return" "throw" "break" "continue"
    "let" "be" "new" "import" "link" "extern" "constructor"
    "public" "private" "abstract" "shared" "unique" "export")
  "Keywords that are not declarations or operators.")

(defconst komodo-declaration-keywords
  '("fn" "void" "mut" "struct" "class" "enum" "module")
  "Keywords that introduce a declaration.")

(defconst komodo-word-operators '("and" "or" "not" "xor" "is")
  "Keyword operators.")

(defconst komodo-types
  '("int" "uint" "float" "f32" "f64" "bool" "char" "string"
    "i8" "i16" "i32" "i64" "u8" "u16" "u32" "u64")
  "Primitive type names.")

(defconst komodo-constants '("true" "false" "null" "self"))

(defconst komodo-builtins '("print" "assert" "copy" "data" "join"))

(defvar komodo-font-lock-keywords
  `((,(regexp-opt komodo-keywords 'symbols) . font-lock-keyword-face)
    (,(regexp-opt komodo-word-operators 'symbols) . font-lock-builtin-face)
    (,(regexp-opt komodo-declaration-keywords 'symbols) . font-lock-type-face)
    (,(regexp-opt komodo-types 'symbols) . font-lock-type-face)
    (,(regexp-opt komodo-constants 'symbols) . font-lock-constant-face)
    (,(regexp-opt komodo-builtins 'symbols) . font-lock-builtin-face)
    ;; fn/void/mut NAME and struct/class/enum NAME
    ("\\_<\\(?:fn\\|void\\|mut\\)[ \t]+\\([A-Za-z_][A-Za-z0-9_]*\\)"
     1 font-lock-function-name-face)
    ("\\_<\\(?:struct\\|class\\|enum\\|module\\)[ \t]+\\([A-Za-z_][A-Za-z0-9_]*\\)"
     1 font-lock-type-face)
    ;; Module::member and Enum::Variant qualifiers
    ("\\([A-Za-z_][A-Za-z0-9_]*\\)::" 1 font-lock-variable-name-face)
    ;; calls: name(
    ("\\_<\\([A-Za-z_][A-Za-z0-9_]*\\)[ \t]*(" 1 font-lock-function-call-face t))
  "Highlighting rules for `komodo-mode'.")

(defvar komodo-mode-syntax-table
  (let ((st (make-syntax-table)))
    ;; // line comments and /* ... */ block comments
    (modify-syntax-entry ?/ ". 124b" st)
    (modify-syntax-entry ?* ". 23" st)
    (modify-syntax-entry ?\n "> b" st)
    (modify-syntax-entry ?\r "> b" st)
    (modify-syntax-entry ?\" "\"" st)
    (modify-syntax-entry ?` "\"" st)
    (modify-syntax-entry ?' "\"" st)
    (modify-syntax-entry ?_ "w" st)
    (modify-syntax-entry ?: "." st)
    st)
  "Syntax table for `komodo-mode'.")

(defun komodo--block-opener-p (line)
  "Return non-nil when LINE (a string, no indentation) opens an indented block."
  (and (not (string-match-p "\\_<\\(then\\|do\\)[ \t]+[^ \t\n]" line))
       (or (string-match-p
            "\\`\\(?:\\(?:abstract\\|shared\\|unique\\|export\\|extern\\|public\\|private\\)[ \t]+\\)*\\(?:fn\\|void\\|mut\\|struct\\|class\\|enum\\|match\\|case\\|constructor\\|else\\|if\\|for\\|while\\|public\\|private\\)\\_>"
            line)
           (string-match-p "\\_<\\(then\\|do\\)[ \t]*\\'" line))))

(defun komodo-indent-line ()
  "Indent the current line like the previous non-blank line.
One level deeper after a block opener; one level shallower for `else'."
  (interactive)
  (let* ((this (save-excursion
                 (back-to-indentation)
                 (buffer-substring-no-properties (point) (line-end-position))))
         (target 0))
    (save-excursion
      (forward-line -1)
      (while (and (not (bobp)) (string-blank-p (thing-at-point 'line t)))
        (forward-line -1))
      (unless (string-blank-p (thing-at-point 'line t))
        (back-to-indentation)
        (let ((prev-indent (current-column))
              (prev-text (buffer-substring-no-properties
                          (point) (line-end-position))))
          (setq target (if (komodo--block-opener-p prev-text)
                           (+ prev-indent komodo-indent-offset)
                         prev-indent)))))
    (when (string-match-p "\\`else\\_>" this)
      (setq target (max 0 (- target komodo-indent-offset))))
    (indent-line-to target)))

;;;###autoload
(define-derived-mode komodo-mode prog-mode "Komodo"
  "Major mode for editing Komodo source (.km, .kmm)."
  :syntax-table komodo-mode-syntax-table
  (setq-local comment-start "// ")
  (setq-local comment-end "")
  (setq-local comment-start-skip "\\(?://+\\|/\\*+\\)[ \t]*")
  (setq-local comment-end-skip "[ \t]*\\(?:\\*+/\\|\n\\)")
  (setq-local indent-line-function #'komodo-indent-line)
  (setq-local indent-tabs-mode nil)
  (setq-local tab-width komodo-indent-offset)
  (setq-local standard-indent komodo-indent-offset)
  (setq-local font-lock-defaults '(komodo-font-lock-keywords)))

;;;###autoload
(add-to-list 'auto-mode-alist '("\\.kmm?\\'" . komodo-mode))

(provide 'komodo-mode)
;;; komodo-mode.el ends here
