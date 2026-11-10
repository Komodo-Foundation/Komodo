#!/usr/bin/env python3
"""check-grammar — keep every editor grammar in step with the lexer.

Reads the keyword list from modules/frontend/lexer.kmm (the `for k in [...]`
list in isKeyword, cross-checked against the `case "..."` arms of readIdentifier)
and checks that each keyword appears as a whole word in:

  * editors/vscode/syntaxes/komodo.tmLanguage.json  (its regex strings)
  * editors/vim/syntax/komodo.vim
  * editors/emacs/komodo-mode.el

It also checks that every JSON file under editors/vscode parses.

    python3 editors/check-grammar.py        exit 0 when all checks pass
"""
import json, re, sys
from pathlib import Path

root = Path(__file__).resolve().parent.parent
editors = root / 'editors'
lexer = (root / 'modules/frontend/lexer.kmm').read_text()

fails = []

def fail(msg):
    fails.append(msg)
    print('FAIL', msg)

# 1. keyword list from the lexer
m = re.search(r'for k in \[(.*?)\] do', lexer, re.S)
if not m:
    sys.exit('check-grammar: could not find the keyword list in lexer.kmm')
keywords = re.findall(r'"([A-Za-z_]+)"', m.group(1))
cases = re.findall(r'^\s*case "([A-Za-z_]+)"', lexer, re.M)
cases = [c for c in cases if c in keywords or c.isalpha()]
if set(keywords) != set(cases):
    fail('lexer keyword list and readIdentifier case arms differ: %s' %
         sorted(set(keywords) ^ set(cases)))
print('lexer keywords (%d): %s' % (len(keywords), ' '.join(keywords)))

# 2. every JSON file in the VS Code extension parses
for jp in sorted((editors / 'vscode').rglob('*.json')):
    try:
        json.loads(jp.read_text())
    except ValueError as e:
        fail('%s does not parse: %s' % (jp.relative_to(root), e))

# 3. each keyword is present in each grammar
def words_in(text, kw):
    return re.search(r'(?<![A-Za-z0-9_])' + re.escape(kw) + r'(?![A-Za-z0-9_])', text) is not None

def tm_patterns(node, out):
    if isinstance(node, dict):
        for k, v in node.items():
            if k in ('match', 'begin', 'end') and isinstance(v, str):
                out.append(v)
            else:
                tm_patterns(v, out)
    elif isinstance(node, list):
        for v in node:
            tm_patterns(v, out)
    return out

grammars = {
    'vscode grammar': '\n'.join(tm_patterns(
        json.loads((editors / 'vscode/syntaxes/komodo.tmLanguage.json').read_text()), [])),
    'vim syntax': (editors / 'vim/syntax/komodo.vim').read_text(),
    'emacs mode': (editors / 'emacs/komodo-mode.el').read_text(),
}
for name, text in grammars.items():
    for kw in keywords:
        if not words_in(text, kw):
            fail('%s does not cover keyword %r' % (name, kw))

print('checked %d keywords against %d grammars' % (len(keywords), len(grammars)))
if fails:
    print('%d failure(s)' % len(fails))
    sys.exit(1)
print('ok')
