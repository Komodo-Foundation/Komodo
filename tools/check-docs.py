#!/usr/bin/env python3
"""check-docs — compile and run every ```km block in the docs.

A ```km block immediately followed by a ```text block is a claim: running the
program must print that text (or, if the text starts with `error:` / `warning:`,
the compiler must report that message). A ```km block with no text block must
at least compile. `<!-- doc-skip -->` before a block skips it (multi-file or fragment examples);
`<!-- doc-error -->` says the block must be rejected by the compiler.

    tools/check-docs.py [file.md ...]      default: docs/**/*.md
"""
import re, subprocess, sys, tempfile, os, textwrap
from pathlib import Path

root = Path(__file__).resolve().parent.parent
komodo = str(root / 'komodo')
BLOCK = re.compile(r'(<!-- doc-(skip|error) -->\s*\n\s*)?```km\n(.*?)```(\s*\n\s*```text\n(.*?)```)?', re.S)

def run(src, expect):
    with tempfile.TemporaryDirectory() as d:
        Path(d, 'main.km').write_text(src)
        p = subprocess.run([komodo, '-q', 'main.km'], cwd=d, capture_output=True, text=True, timeout=120)
        out = (p.stdout + p.stderr)
        # drop the `komodo:` chatter and the source-line snippet under each error
        out = '\n'.join(l for l in out.splitlines() if not l.startswith('komodo:') and not re.match(r'\s*\d*\s*\|', l))
        if expect is None:
            return ('error:' not in out and 'undefined reference' not in out), out
        want = expect.strip()
        norm = lambda t: sorted(l.rstrip() for l in re.sub(r'[\w./-]*\.km(:\d+)', r'FILE\1', t).splitlines() if l.strip())
        if re.match(r'(error|warning):', want):
            msg = want.split(':', 1)[1].strip()
            return msg in out, out
        return norm(out) == norm(want), out

files = [Path(a).resolve() for a in sys.argv[1:]] or sorted((root / 'docs').rglob('*.md'))
ok = bad = skipped = 0
for f in files:
    text = f.read_text()
    for m in BLOCK.finditer(text):
        line = text[:m.start()].count('\n') + 1
        if m.group(2) == 'skip':
            skipped += 1; continue
        good, out = run(textwrap.dedent(m.group(3)), textwrap.dedent(m.group(5)) if m.group(5) else None)
        if m.group(2) == 'error': good = 'error:' in out
        if good: ok += 1
        else:
            bad += 1
            print(f'FAIL {f.relative_to(root)}:{line}')
            print('  expected:', (m.group(5) or '(compiles)').strip()[:200].replace('\n', '\n           '))
            print('  got:     ', out.strip()[:200].replace('\n', '\n           '))
print(f'docs: {ok} ok, {bad} failed, {skipped} skipped')
sys.exit(1 if bad else 0)
