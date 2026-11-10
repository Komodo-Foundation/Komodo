#!/usr/bin/env python3
"""Scripted LSP client for tools/km-lsp.

Spawns the server, speaks LSP over stdio, and asserts:
  * initialize advertises documentSymbol and full text sync
  * didOpen of a file with an undefined name publishes an error with the
    exact range and message the compiler gives
  * didChange does not compile (no publishDiagnostics before the next reply)
  * didSave of a fixed buffer clears the error, even though the file on disk
    still has the old text
  * a lint warning is published with severity 2
  * documentSymbol lists top-level declarations only
  * shutdown then exit ends the process with status 0

    python3 tests/lsp/run.py
"""
import json
import os
import queue
import subprocess
import sys
import tempfile
import threading
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SERVER = ROOT / 'tools' / 'km-lsp'
TIMEOUT = 120


class Client:
    def __init__(self):
        self.proc = subprocess.Popen([sys.executable, str(SERVER)],
                                     stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                     stderr=None, cwd=str(ROOT))
        self.inbox = queue.Queue()
        self.next_id = 1
        threading.Thread(target=self._reader, daemon=True).start()

    def _reader(self):
        stream = self.proc.stdout
        while True:
            length = None
            while True:
                line = stream.readline()
                if not line:
                    self.inbox.put(None)
                    return
                line = line.strip()
                if not line:
                    break
                if line.lower().startswith(b'content-length:'):
                    length = int(line.split(b':', 1)[1])
            body = stream.read(length)
            self.inbox.put(json.loads(body.decode('utf-8')))

    def send(self, obj):
        body = json.dumps(obj).encode('utf-8')
        self.proc.stdin.write(b'Content-Length: %d\r\n\r\n' % len(body) + body)
        self.proc.stdin.flush()

    def notify(self, method, params):
        self.send({'jsonrpc': '2.0', 'method': method, 'params': params})

    def request(self, method, params):
        """Send a request and return (response, notifications seen before it)."""
        req_id = self.next_id
        self.next_id += 1
        self.send({'jsonrpc': '2.0', 'id': req_id, 'method': method, 'params': params})
        seen = []
        while True:
            msg = self.get()
            if msg.get('id') == req_id and 'method' not in msg:
                return msg, seen
            seen.append(msg)

    def get(self):
        try:
            msg = self.inbox.get(timeout=TIMEOUT)
        except queue.Empty:
            raise AssertionError('timed out waiting for the server')
        if msg is None:
            raise AssertionError('server closed its output')
        return msg

    def diagnostics_for(self, uri):
        """Wait for the next publishDiagnostics for uri."""
        while True:
            msg = self.get()
            if msg.get('method') == 'textDocument/publishDiagnostics' and \
                    msg['params']['uri'] == uri:
                return msg['params']['diagnostics']


failures = []


def check(cond, what):
    print(('ok   ' if cond else 'FAIL ') + what)
    if not cond:
        failures.append(what)


def main():
    tmp = tempfile.TemporaryDirectory(prefix='km-lsp-test-')
    work = Path(tmp.name)
    bad = work / 'bad.km'
    bad_text = ('import console\n'
                '\n'
                'void main()\n'
                '    let count = 10\n'
                '    console.print(cont)\n')
    bad.write_text(bad_text)
    uri = bad.as_uri()

    shapes = work / 'shapes.km'
    shapes_text = ('import console\n'
                   '\n'
                   'struct Token\n'
                   '    kind: string\n'
                   '\n'
                   'class Counter\n'
                   '    constructor(start: int)\n'
                   '        m_n = start\n'
                   '\n'
                   '    fn value(): int\n'
                   '        return m_n\n'
                   '\n'
                   '    m_n: int\n'
                   '\n'
                   'enum Color\n'
                   '    Red\n'
                   '    Green\n'
                   '\n'
                   'fn add(a: int, b: int): int\n'
                   '    return a + b\n'
                   '\n'
                   'void main()\n'
                   '    console.print(add(1, 2))\n')
    shapes.write_text(shapes_text)
    shapes_uri = shapes.as_uri()

    lint = work / 'lint.km'
    lint_text = ('void main()\n'
                 '    let y = 3\n'
                 '    if y == y then\n'
                 '        print("same")\n')
    lint.write_text(lint_text)
    lint_uri = lint.as_uri()

    c = Client()

    # initialize
    resp, _ = c.request('initialize', {'processId': os.getpid(), 'rootUri': work.as_uri(),
                                       'capabilities': {}})
    caps = resp['result']['capabilities']
    check(caps.get('documentSymbolProvider') is True, 'initialize advertises documentSymbol')
    check(caps['textDocumentSync']['change'] == 1, 'initialize asks for full text sync')
    c.notify('initialized', {})

    # open with an undefined name: exact range and message
    c.notify('textDocument/didOpen', {'textDocument': {
        'uri': uri, 'languageId': 'komodo', 'version': 1, 'text': bad_text}})
    diags = c.diagnostics_for(uri)
    check(len(diags) == 1, 'didOpen publishes one diagnostic for an undefined name (got %d)'
          % len(diags))
    if diags:
        d = diags[0]
        check(d['range'] == {'start': {'line': 4, 'character': 18},
                             'end': {'line': 4, 'character': 22}},
              'diagnostic range covers the word "cont" (got %s)' % json.dumps(d['range']))
        check(d['message'] == "undefined name 'cont' (did you mean 'count'?)",
              'diagnostic message matches the compiler (got %r)' % d['message'])
        check(d['severity'] == 1 and d['source'] == 'komodo', 'diagnostic is an error from komodo')

    # didChange: buffer only, so no publish before the next reply
    fixed_text = bad_text.replace('cont)', 'count)')
    c.notify('textDocument/didChange', {
        'textDocument': {'uri': uri, 'version': 2},
        'contentChanges': [{'text': fixed_text}]})
    resp, seen = c.request('textDocument/documentSymbol', {'textDocument': {'uri': uri}})
    check(not any(m.get('method') == 'textDocument/publishDiagnostics' for m in seen),
          'didChange does not compile (no publishDiagnostics before the next reply)')

    # didSave of the fixed buffer: disk still has the old text, so a temporary copy is compiled
    c.notify('textDocument/didSave', {'textDocument': {'uri': uri}})
    diags = c.diagnostics_for(uri)
    check(diags == [], 'didSave of the fixed buffer clears the error (got %s)' % json.dumps(diags))

    # lint warning on a second document
    c.notify('textDocument/didOpen', {'textDocument': {
        'uri': lint_uri, 'languageId': 'komodo', 'version': 1, 'text': lint_text}})
    diags = c.diagnostics_for(lint_uri)
    warn = [d for d in diags if 'compares a value with itself' in d['message']]
    check(len(warn) == 1 and warn[0]['severity'] == 2,
          'lint warning is published with severity 2 (got %s)' % json.dumps(diags))

    # document symbols
    c.notify('textDocument/didOpen', {'textDocument': {
        'uri': shapes_uri, 'languageId': 'komodo', 'version': 1, 'text': shapes_text}})
    c.diagnostics_for(shapes_uri)
    resp, _ = c.request('textDocument/documentSymbol', {'textDocument': {'uri': shapes_uri}})
    symbols = resp['result']
    got = [(s['name'], s['kind']) for s in symbols]
    want = [('Token', 23), ('Counter', 5), ('Color', 10), ('add', 12), ('main', 12)]
    check(got == want, 'documentSymbol lists top-level declarations (got %s)' % got)
    if symbols:
        add = [s for s in symbols if s['name'] == 'add'][0]
        check(add['selectionRange']['start'] == {'line': 18, 'character': 3},
              'symbol selectionRange points at the name')
        check(add['range']['end']['line'] == 19, 'symbol range ends at its last body line')

    # shutdown and exit
    resp, _ = c.request('shutdown', None)
    check(resp.get('result') is None and 'error' not in resp, 'shutdown answers null')
    c.notify('exit', None)
    try:
        code = c.proc.wait(timeout=30)
    except subprocess.TimeoutExpired:
        code = None
        c.proc.kill()
    check(code == 0, 'exit after shutdown ends the server with status 0 (got %s)' % code)

    project_files = sorted(p.name for p in work.iterdir())
    check(project_files == ['bad.km', 'lint.km', 'shapes.km'],
          'compiler wrote nothing into the project directory (found %s)' % project_files)
    tmp.cleanup()
    if failures:
        print('%d failure(s)' % len(failures))
        return 1
    print('all LSP checks passed')
    return 0


if __name__ == '__main__':
    sys.exit(main())
