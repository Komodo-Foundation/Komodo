#!/usr/bin/env python3
"""km-graph — structural extractor for .km/.kmm sources, for graphify.

graphify has no Komodo parser; this emits nodes (files, types, functions,
methods) and edges (contains, method, inherits, calls) as JSON that
`graphify`'s build/cluster/report steps accept.   usage: km-graph.py [out.json]
Calls are matched by name, so a name defined in several places yields
AMBIGUOUS edges to each of them.
"""
import json, re, sys
from pathlib import Path

root = Path(__file__).resolve().parent.parent
files = sorted(root.glob('modules/**/*.kmm')) + [root / 'main.km'] + sorted(root.glob('stdlib/*.kmm'))
TYPE = re.compile(r'^(?:abstract\s+|shared\s+|unique\s+)*(class|struct|enum)\s+(\w+)(?:\s*:\s*(\w+))?')
OOL = re.compile(r'^(?:[\w\[\]?]+\s+)*(\w+)::(\w+)\s*\(')
TOP = re.compile(r'^(?:fn |mut |void |extern fn |extern void )?(\w+)\s*\(')
METH = re.compile(r'^    (?:fn |mut |static )?(\w+)\s*\(.*\)\s*(?::\s*[\w\[\]?, ]+)?\s*$')
KW = {'if', 'while', 'for', 'match', 'return', 'case', 'constructor', 'print', 'new', 'throw', 'module', 'import', 'link'}
CALL = re.compile(r'\b(\w+)\s*\(')
nodes, edges, defs, bodies = [], [], {}, {}

def add(i, label, rel, line):
    nodes.append(dict(id=i, label=label, file_type='code', source_file=rel, source_location=f'L{line}', _origin='ast'))

for f in files:
    rel = str(f.relative_to(root)); fid = 'file::' + rel
    add(fid, rel, rel, 1)
    ctype = cur = None
    for n, line in enumerate(f.read_text(errors='replace').splitlines(), 1):
        m = TYPE.match(line)
        if m:
            ctype, cur = m.group(2), None
            t = f'{f.stem}::{ctype}'; defs.setdefault(ctype, []).append(t); add(t, ctype, rel, n)
            edges.append(dict(source=fid, target=t, relation='contains', confidence='EXTRACTED', source_file=rel))
            if m.group(3): edges.append(dict(source=t, target=('T', m.group(3)), relation='inherits', confidence='EXTRACTED', source_file=rel))
            continue
        if line and not line[0].isspace():
            m = OOL.match(line)
            if m:
                ty, name = m.groups(); cur = f'{f.stem}::{ty}.{name}'
                defs.setdefault(name, []).append(cur); add(cur, f'{ty}.{name}()', rel, n)
                edges.append(dict(source=('T', ty), target=cur, relation='method', confidence='EXTRACTED', source_file=rel))
                continue
            m = TOP.match(line)
            if m and m.group(1) not in KW:
                ctype, name = None, m.group(1); cur = f'{f.stem}::{name}'
                defs.setdefault(name, []).append(cur); add(cur, name + '()', rel, n)
                edges.append(dict(source=fid, target=cur, relation='contains', confidence='EXTRACTED', source_file=rel))
                continue
        m = METH.match(line)
        if m and ctype and m.group(1) not in KW:
            name = m.group(1); cur = f'{f.stem}::{ctype}.{name}'
            defs.setdefault(name, []).append(cur); add(cur, f'{ctype}.{name}()', rel, n)
            edges.append(dict(source=f'{f.stem}::{ctype}', target=cur, relation='method', confidence='EXTRACTED', source_file=rel))
        elif cur:
            bodies.setdefault(cur, []).append((line.split('//')[0], rel, n))

out = []
def resolve(x):
    if isinstance(x, tuple):
        t = defs.get(x[1]); return t[0] if t else None
    return x
for src, lines in bodies.items():
    seen = set()
    for code, rel, n in lines:
        for c in CALL.findall(code):
            if c in defs and c not in seen:
                seen.add(c); tg = defs[c]
                for t in tg[:3]:
                    if t != src:
                        out.append(dict(source=src, target=t, relation='calls', source_file=rel, source_location=f'L{n}',
                                        confidence='EXTRACTED' if len(tg) == 1 else 'AMBIGUOUS'))
for e in edges:
    e['source'], e['target'] = resolve(e['source']), resolve(e['target'])
    if e['source'] and e['target']: out.append(e)
for e in out: e['_origin'] = 'ast'
Path(sys.argv[1] if len(sys.argv) > 1 else 'graphify-out/.km_ast.json').write_text(json.dumps(dict(nodes=nodes, edges=out)))
print(len(nodes), 'nodes', len(out), 'edges')
