#!/usr/bin/env python3
"""check_links.py DIR -- every relative link in DIR's .md/.html pages must
resolve: the target file must exist, and a #fragment must name an id or
anchor (id="...") defined in that file. Exits 1 on the first broken link set."""
import html
import os
import re
import sys

ID_RE = re.compile(r'id="([^"]+)"')
MD_LINK = re.compile(r'\]\(([^)\s]+)\)')
HTML_LINK = re.compile(r'href="([^"]+)"')


def main(root):
    pages = []
    for dirpath, _, names in os.walk(root):
        for n in names:
            if n.endswith(('.md', '.html')):
                pages.append(os.path.join(dirpath, n))
    ids = {}
    for p in pages:
        with open(p, encoding='utf-8') as fh:
            ids[p] = set(html.unescape(x) for x in ID_RE.findall(fh.read()))
    errors = []
    links = 0
    for p in pages:
        with open(p, encoding='utf-8') as fh:
            text = fh.read()
        found = MD_LINK.findall(text) if p.endswith('.md') else HTML_LINK.findall(text)
        for target in found:
            if re.match(r'^[a-z]+:', target) or target.startswith('//'):
                continue
            links += 1
            file_part, _, frag = target.partition('#')
            dest = os.path.normpath(os.path.join(os.path.dirname(p), file_part)) if file_part else p
            if not os.path.exists(dest):
                errors.append('%s: missing file in link %r' % (p, target))
                continue
            if frag and dest.endswith(('.md', '.html')):
                if dest not in ids:
                    with open(dest, encoding='utf-8') as fh:
                        ids[dest] = set(html.unescape(x) for x in ID_RE.findall(fh.read()))
                if frag not in ids[dest]:
                    errors.append('%s: dangling anchor %r' % (p, target))
    for e in errors:
        sys.stderr.write('check_links: %s\n' % e)
    if not links:
        sys.stderr.write('check_links: no links found under %s\n' % root)
        return 1
    print('check_links: %d pages, %d links, %d broken' % (len(pages), links, len(errors)))
    return 1 if errors else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1]))
