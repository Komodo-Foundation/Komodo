#!/usr/bin/env python3
"""Verify the diagnostics catalog: every entry is complete, and every
example in it really behaves as documented when compiled with $KMC.

  bad:  `kmc main.km check|lint [strict] rich` must report that entry's code
  fix:  `kmc main.km check [strict]` must print nothing and succeed
  all:  why + help present, codes unique, `komodo explain` page exists
"""
import importlib.util, os, re, subprocess, sys, tempfile
from pathlib import Path

root = Path(__file__).resolve().parent.parent.parent
spec = importlib.util.spec_from_file_location('gen_diag', root / 'tools/gen-diag.py')
gen = importlib.util.module_from_spec(spec); spec.loader.exec_module(gen)
kmc = os.environ.get('KMC', str(root / 'kmc'))
ents = gen.parse()
fails = 0


def fail(code, msg):
    global fails
    fails += 1
    print(f'FAIL  {code}: {msg}')


def run(e, body, extra):
    with tempfile.TemporaryDirectory() as d:
        for n, text in e['files'].items():
            Path(d, n).write_text('\n'.join(text) + '\n')
        Path(d, 'main.km').write_text('\n'.join(body) + '\n')
        tgt = {'compile': 'x86', 'lint': 'lint'}.get(e.get('mode'), 'check')
        args = [kmc, 'main.km', tgt] + (['strict'] if e.get('mode') == 'strict' else []) + extra
        p = subprocess.run(args, cwd=d, capture_output=True, text=True, timeout=20)
        out = ''.join(l + '\n' for l in (p.stdout + p.stderr).splitlines() if not l.startswith('wrote '))
        return p.returncode, out


seen = set()
checked = 0
for e in ents:
    c = e['code']
    if c in seen:
        fail(c, 'duplicate code')
    seen.add(c)
    for k in ('title', 'match', 'why', 'help'):
        if not e.get(k):
            fail(c, f'missing {k}')
    if 'bad' not in e and not e.get('note'):
        fail(c, 'needs a bad:/fix: example or a note:')
    if 'bad' in e:
        if 'fix' not in e:
            fail(c, 'has bad: but no fix:')
        rc, out = run(e, e['bad'], ['rich'])
        if not re.search(r'(error|warning)\[%s\]' % c, out):
            fail(c, 'bad example does not report this code; got:\n' + out)
        if e.get('mode') != 'strict' and rc == 0 and 'warning' not in out:
            fail(c, 'bad example compiled clean')
    if 'fix' in e:
        rc, out = run(e, e['fix'], [])
        if rc != 0 or out.strip():
            fail(c, 'fix example is not clean; got:\n' + out)
    if not (root / 'docs/errors' / (c + '.md')).exists():
        fail(c, 'no docs/errors page (run tools/gen-diag.py)')
    checked += 1
print(f'catalog: {checked} entries checked, {fails} failure(s)')
sys.exit(1 if fails else 0)
