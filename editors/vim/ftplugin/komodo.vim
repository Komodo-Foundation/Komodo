" Komodo blocks are indented; four spaces per level, no tabs.
if exists("b:did_ftplugin")
  finish
endif
let b:did_ftplugin = 1

setlocal shiftwidth=4 tabstop=4 softtabstop=4 expandtab
setlocal commentstring=//\ %s
setlocal comments=s1:/*,mb:*,ex:*/,://
setlocal autoindent
let b:undo_ftplugin = "setlocal sw< ts< sts< et< cms< com< ai<"
