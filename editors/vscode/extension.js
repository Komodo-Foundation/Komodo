// Komodo for VS Code: language server client, formatter, tasks, commands
// and a `komodo` debug type. Every feature shells out to the `komodo`
// command or to tools/km-lsp; nothing about the language is reimplemented here.
'use strict';
const vscode = require('vscode');
const cp = require('child_process');
const fs = require('fs');
const os = require('os');
const path = require('path');
const { LanguageClient } = require('vscode-languageclient/node');

let client;
let output;

function config() {
    return vscode.workspace.getConfiguration('komodo');
}

// Resolve a command name through PATH; an absolute path is returned as is.
function which(cmd) {
    if (path.isAbsolute(cmd)) return fs.existsSync(cmd) ? cmd : undefined;
    for (const dir of (process.env.PATH || '').split(path.delimiter)) {
        const p = path.join(dir, cmd);
        try {
            fs.accessSync(p, fs.constants.X_OK);
            return p;
        } catch (_) { /* keep looking */ }
    }
    return undefined;
}

function komodoPath() {
    const setting = config().get('path') || 'komodo';
    // The Komodo repository itself: prefer its own driver.
    for (const f of vscode.workspace.workspaceFolders || []) {
        const local = path.join(f.uri.fsPath, 'komodo');
        if (setting === 'komodo' && fs.existsSync(path.join(f.uri.fsPath, 'tools', 'km-lsp')) && fs.existsSync(local)) {
            return local;
        }
    }
    return which(setting) || setting;
}

function lspPath() {
    const setting = config().get('languageServer.path');
    if (setting) return setting;
    for (const f of vscode.workspace.workspaceFolders || []) {
        const p = path.join(f.uri.fsPath, 'tools', 'km-lsp');
        if (fs.existsSync(p)) return p;
    }
    const k = which(komodoPath());
    if (k) {
        const p = path.join(path.dirname(fs.realpathSync(k)), 'tools', 'km-lsp');
        if (fs.existsSync(p)) return p;
    }
    return undefined;
}

// ---------------------------------------------------------------- language server

async function startClient(context) {
    if (!config().get('languageServer.enabled')) return;
    const server = lspPath();
    if (!server) {
        output.appendLine('km-lsp not found: set komodo.languageServer.path, or put komodo on PATH.');
        vscode.window.showWarningMessage(
            'Komodo: language server not found. Install Komodo (install.sh puts `komodo` on PATH) or set komodo.languageServer.path.',
            'Open Settings').then(choice => {
            if (choice) vscode.commands.executeCommand('workbench.action.openSettings', 'komodo.languageServer');
        });
        return;
    }
    const run = {
        command: 'python3',
        args: [server],
        options: { env: { ...process.env, KOMODO: komodoPath() } },
    };
    client = new LanguageClient('komodo', 'Komodo Language Server', { run, debug: run }, {
        documentSelector: [{ scheme: 'file', language: 'komodo' }],
        outputChannel: output,
    });
    try {
        await client.start();
    } catch (e) {
        output.appendLine(`km-lsp failed to start: ${e.message || e}`);
    }
}

async function stopClient() {
    if (client) {
        const c = client;
        client = undefined;
        try { await c.stop(); } catch (_) { /* already gone */ }
    }
}

// ---------------------------------------------------------------- formatter

// Formats through `komodo fmt -w` on a temporary copy, so an unsaved buffer
// is formatted as it is in the editor. km-fmt only changes whitespace.
function formatDocument(doc) {
    const ext = path.extname(doc.fileName) || '.km';
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'komodo-fmt-'));
    const tmp = path.join(dir, 'buffer' + ext);
    try {
        const text = doc.getText();
        fs.writeFileSync(tmp, text);
        const r = cp.spawnSync(komodoPath(), ['fmt', '-w', tmp], { encoding: 'utf8' });
        if (r.error || r.status !== 0) {
            output.appendLine(`komodo fmt: ${r.error ? r.error.message : (r.stderr || r.stdout).trim()}`);
            return [];
        }
        const formatted = fs.readFileSync(tmp, 'utf8');
        if (formatted === text) return [];
        const all = new vscode.Range(doc.positionAt(0), doc.positionAt(text.length));
        return [vscode.TextEdit.replace(all, formatted)];
    } finally {
        fs.rmSync(dir, { recursive: true, force: true });
    }
}

// ---------------------------------------------------------------- tasks

function folderFor(uri) {
    return (uri && vscode.workspace.getWorkspaceFolder(uri)) ||
        (vscode.workspace.workspaceFolders || [])[0];
}

function makeTask(def, scope) {
    const args = [def.command];
    if (def.command === 'run' || def.command === 'build') {
        const target = config().get('target');
        if (target && target !== 'x86') args.push('-t', target);
    }
    args.push(...(def.args || []));
    if (def.file) args.push(def.file);
    const label = def.file ? `${def.command} ${path.basename(def.file)}` : def.command;
    const cwd = def.file ? path.dirname(def.file) : undefined;
    const task = new vscode.Task(def, scope || vscode.TaskScope.Workspace, label, 'komodo',
        new vscode.ProcessExecution(komodoPath(), args, cwd ? { cwd } : undefined), ['$komodo']);
    if (def.command === 'build') task.group = vscode.TaskGroup.Build;
    if (def.command === 'test') task.group = vscode.TaskGroup.Test;
    task.presentationOptions = { reveal: vscode.TaskRevealKind.Always, clear: true };
    return task;
}

const taskProvider = {
    provideTasks() {
        const folder = folderFor();
        if (!folder) return [];
        return ['run', 'build', 'check', 'test'].map(command => makeTask({ type: 'komodo', command }, folder));
    },
    resolveTask(task) {
        const def = task.definition;
        if (!def.command) return undefined;
        const t = makeTask(def, task.scope);
        t.name = task.name;
        return t;
    },
};

async function activeKomodoFile() {
    const ed = vscode.window.activeTextEditor;
    if (!ed || ed.document.languageId !== 'komodo') {
        vscode.window.showErrorMessage('Komodo: open a .km file first.');
        return undefined;
    }
    if (ed.document.isDirty) await ed.document.save();
    return ed.document.uri;
}

function fileCommand(command) {
    return async () => {
        const uri = await activeKomodoFile();
        if (uri) vscode.tasks.executeTask(makeTask({ type: 'komodo', command, file: uri.fsPath }, folderFor(uri)));
    };
}

function terminalCommand(name, args) {
    return () => {
        const term = vscode.window.createTerminal({ name, shellPath: komodoPath(), shellArgs: args,
            cwd: folderFor(vscode.window.activeTextEditor && vscode.window.activeTextEditor.document.uri)?.uri.fsPath });
        term.show();
    };
}

// ---------------------------------------------------------------- debugging

// The `komodo` launch type builds the file with `komodo build -g` and then
// hands the executable to gdb (C/C++ extension) or lldb (CodeLLDB).
function pickDebugger() {
    const want = config().get('debugger');
    const has = id => !!vscode.extensions.getExtension(id);
    if (want === 'cppdbg' || (want === 'auto' && has('ms-vscode.cpptools'))) return 'cppdbg';
    if (want === 'lldb' || (want === 'auto' && has('vadimcn.vscode-lldb'))) return 'lldb';
    return undefined;
}

function build(file, out) {
    return new Promise(resolve => {
        cp.execFile(komodoPath(), ['build', '-g', '-o', out, file], { cwd: path.dirname(file) }, (err, stdout, stderr) => {
            const text = (stdout + stderr).trim();
            if (text) output.appendLine(text);
            resolve(err ? text || err.message : undefined);
        });
    });
}

const debugProvider = {
    async resolveDebugConfiguration(folder, cfg) {
        if (!cfg.type && !cfg.request) {
            const ed = vscode.window.activeTextEditor;
            if (!ed || ed.document.languageId !== 'komodo') return cfg;
            cfg = { type: 'komodo', request: 'launch', name: 'Debug Komodo file', program: '${file}' };
        }
        return cfg;
    },
    // Runs after ${file} and friends are substituted.
    async resolveDebugConfigurationWithSubstitutedVariables(folder, cfg) {
        const file = cfg.program;
        if (!file || !fs.existsSync(file)) {
            vscode.window.showErrorMessage(`Komodo: program not found: ${file}`);
            return undefined;
        }
        const type = pickDebugger();
        if (!type) {
            const pick = await vscode.window.showErrorMessage(
                'Komodo: debugging needs the C/C++ extension (gdb) or CodeLLDB.', 'Install C/C++');
            if (pick) vscode.commands.executeCommand('workbench.extensions.search', 'ms-vscode.cpptools');
            return undefined;
        }
        const doc = vscode.workspace.textDocuments.find(d => d.fileName === file);
        if (doc && doc.isDirty) await doc.save();
        const outDir = path.join(path.dirname(file), '.komodo', 'debug');
        fs.mkdirSync(outDir, { recursive: true });
        const ignore = path.join(outDir, '..', '.gitignore');
        if (!fs.existsSync(ignore)) fs.writeFileSync(ignore, '*\n');
        const exe = path.join(outDir, path.basename(file, path.extname(file)));
        const err = await build(file, exe);
        if (err) {
            output.show(true);
            vscode.window.showErrorMessage('Komodo: build failed, see the Komodo output.');
            return undefined;
        }
        const cwd = cfg.cwd || (folder ? folder.uri.fsPath : path.dirname(file));
        const args = cfg.args || [];
        if (type === 'lldb') {
            return { type, request: 'launch', name: cfg.name, program: exe, args, cwd, stopOnEntry: !!cfg.stopAtEntry };
        }
        return {
            type, request: 'launch', name: cfg.name, program: exe, args, cwd,
            stopAtEntry: !!cfg.stopAtEntry, MIMode: process.platform === 'darwin' ? 'lldb' : 'gdb',
            externalConsole: false,
        };
    },
};

// ---------------------------------------------------------------- activation

async function activate(context) {
    output = vscode.window.createOutputChannel('Komodo');
    const sel = { language: 'komodo' };
    context.subscriptions.push(
        output,
        vscode.languages.registerDocumentFormattingEditProvider(sel, { provideDocumentFormattingEdits: formatDocument }),
        vscode.tasks.registerTaskProvider('komodo', taskProvider),
        vscode.debug.registerDebugConfigurationProvider('komodo', debugProvider),
        vscode.commands.registerCommand('komodo.run', fileCommand('run')),
        vscode.commands.registerCommand('komodo.build', fileCommand('build')),
        vscode.commands.registerCommand('komodo.check', fileCommand('check')),
        vscode.commands.registerCommand('komodo.test', () => {
            const folder = folderFor();
            if (folder) vscode.tasks.executeTask(makeTask({ type: 'komodo', command: 'test' }, folder));
        }),
        vscode.commands.registerCommand('komodo.debug', async () => {
            const uri = await activeKomodoFile();
            if (uri) vscode.debug.startDebugging(folderFor(uri),
                { type: 'komodo', request: 'launch', name: 'Debug Komodo file', program: uri.fsPath });
        }),
        vscode.commands.registerCommand('komodo.repl', terminalCommand('Komodo REPL', ['repl'])),
        vscode.commands.registerCommand('komodo.doctor', () => {
            output.show(true);
            cp.execFile(komodoPath(), ['doctor'], (err, stdout, stderr) =>
                output.appendLine((stdout + stderr).trim() || (err ? err.message : '')));
        }),
        vscode.commands.registerCommand('komodo.restartServer', async () => {
            await stopClient();
            await startClient(context);
        }),
        vscode.workspace.onDidChangeConfiguration(async e => {
            if (e.affectsConfiguration('komodo.languageServer') || e.affectsConfiguration('komodo.path')) {
                await stopClient();
                await startClient(context);
            }
        }),
    );
    await startClient(context);
}

function deactivate() {
    return stopClient();
}

module.exports = { activate, deactivate };
