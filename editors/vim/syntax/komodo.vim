" Vim syntax file for Komodo (.km, .kmm)
if exists("b:current_syntax")
  finish
endif

syn case match

" Comments: // to end of line, /* ... */ and /** ... */ doc comments.
syn keyword komodoTodo contained TODO FIXME XXX NOTE
syn match   komodoLineComment "//.*$" contains=komodoTodo
syn region  komodoBlockComment start="/\*" end="\*/" contains=komodoTodo
syn region  komodoDocComment start="/\*\*" end="\*/" contains=komodoTodo

" Strings: "..." with escapes; `...` interpolated, with {expr} inside.
syn match   komodoEscape contained "\\[ntr0\\\"]"
syn match   komodoEscape contained "\\[\\`{}]"
syn region  komodoString start=+"+ skip=+\\"+ end=+"+ contains=komodoEscape
syn region  komodoTemplate start=+`+ skip=+\\`+ end=+`+ contains=komodoEscape,komodoInterp
syn region  komodoInterp contained matchgroup=komodoDelim start="{" end="}" contains=TOP
syn match   komodoChar "'\(\\.\|[^'\\]\)'"

" Numbers
syn match   komodoNumber "\<0[xX][0-9a-fA-F_]\+\>"
syn match   komodoNumber "\<0[bB][01_]\+\>"
syn match   komodoNumber "\<[0-9][0-9_]*\>"
" defined after the integer rule so it wins where both match (2.5)
syn match   komodoFloat  "\<[0-9][0-9_]*\.[0-9][0-9_]*[fF]\=\>"

" Declarations and names
syn keyword komodoDeclare fn void mut struct class enum module
syn match   komodoFuncName "\<\(fn\|void\|mut\)\s\+\zs[A-Za-z_][A-Za-z0-9_]*"
syn match   komodoTypeName "\<\(struct\|class\|enum\)\s\+\zs[A-Za-z_][A-Za-z0-9_]*"

" Keywords
syn keyword komodoConditional if then else match case
syn keyword komodoRepeat while for do to down until in
syn keyword komodoStatement return throw break continue
syn keyword komodoOther let be new import link extern constructor
syn keyword komodoModifier public private abstract shared unique export
syn keyword komodoOperator and or not xor is
syn keyword komodoConstant true false null self
syn keyword komodoType int uint float f32 f64 bool char string i8 i16 i32 i64 u8 u16 u32 u64
syn match   komodoBuiltin "\<\(print\|assert\|copy\|data\|join\)\ze\s*("
syn match   komodoCall "\<[A-Za-z_][A-Za-z0-9_]*\ze\s*("
syn match   komodoNamespace "\<[A-Za-z_][A-Za-z0-9_]*\ze::"

hi def link komodoTodo          Todo
hi def link komodoLineComment   Comment
hi def link komodoBlockComment  Comment
hi def link komodoDocComment    SpecialComment
hi def link komodoEscape        SpecialChar
hi def link komodoString        String
hi def link komodoTemplate      String
hi def link komodoDelim         Delimiter
hi def link komodoChar          Character
hi def link komodoNumber        Number
hi def link komodoFloat         Float
hi def link komodoDeclare       StorageClass
hi def link komodoFuncName      Function
hi def link komodoTypeName      Type
hi def link komodoConditional   Conditional
hi def link komodoRepeat        Repeat
hi def link komodoStatement     Statement
hi def link komodoOther         Keyword
hi def link komodoModifier      StorageClass
hi def link komodoOperator      Operator
hi def link komodoConstant      Boolean
hi def link komodoType          Type
hi def link komodoBuiltin       Function
hi def link komodoCall          Function
hi def link komodoNamespace     Identifier

let b:current_syntax = "komodo"
