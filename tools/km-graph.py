#!/usr/bin/env python3
"""Emit a Graphify-compatible graph fragment for Komodo sources (.km, .kmm).

Graphify has no Komodo extractor, so this adds one. It is line-based, not a
parser: it finds module, import, class, struct, enum, object, type and fn
declarations. Merge the output into the main graph (see docs/graphify note in CLAUDE.md).

usage: tools/km-graph.py [OUT.json]   (default: graphify-out/km-graph.json)
"""
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OUT = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "graphify-out" / "km-graph.json"
DECL = re.compile(r"^(\s*)(class|struct|enum|object|type|fn)\s+(\w+)")
IMPORT = re.compile(r"^\s*import\s+([\w.]+)")
MODULE = re.compile(r"^module\s+([\w.]+)")

nodes, links = {}, []


def node(nid, label, path, line, kind):
    nodes.setdefault(nid, {"id": nid, "label": label, "file_type": "code",
                           "source_file": path, "source_location": f"L{line}",
                           "node_kind": kind})


def link(src, dst, rel, path, line):
    links.append({"source": src, "target": dst, "relation": rel,
                  "confidence": "EXTRACTED", "confidence_score": 1.0,
                  "source_file": path, "source_location": f"L{line}", "weight": 1.0})


for src in sorted(ROOT.rglob("*")):
    if src.suffix not in (".km", ".kmm") or ".git" in src.parts or "graphify-out" in src.parts:
        continue
    path = src.relative_to(ROOT).as_posix()
    fid = "km_file_" + re.sub(r"\W", "_", path)
    node(fid, path, path, 1, "file")
    module = None
    owner = fid  # class that a following indented fn belongs to
    for i, text in enumerate(src.read_text(errors="replace").splitlines(), 1):
        if m := MODULE.match(text):
            module = m.group(1)
            node("km_module_" + module.replace(".", "_"), module, path, i, "module")
            link(fid, "km_module_" + module.replace(".", "_"), "defines", path, i)
        elif m := IMPORT.match(text):
            link(fid, "km_module_" + m.group(1).replace(".", "_"), "imports", path, i)
        elif m := DECL.match(text):
            indent, kind, name = m.groups()
            nid = f"km_{re.sub(r'\W', '_', path)}_{name}_{i}"
            node(nid, name, path, i, kind)
            if not indent:
                owner = nid
                link(fid, nid, "contains", path, i)
            else:
                link(owner, nid, "contains", path, i)

OUT.parent.mkdir(parents=True, exist_ok=True)
OUT.write_text(json.dumps({"directed": True, "multigraph": False, "graph": {},
                           "nodes": list(nodes.values()), "links": links,
                           "hyperedges": []}, indent=1))
print(f"{len(nodes)} nodes, {len(links)} edges -> {OUT}")
