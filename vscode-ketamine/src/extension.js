const vscode = require('vscode');
const cp = require('child_process');
const path = require('path');

function activate(context) {
    let output = vscode.window.createOutputChannel('Ketamine');

    // ── LSP Client ──────────────────────────────────────────────────────────
    let lspClient = null;
    function startLsp() {
        const config = vscode.workspace.getConfiguration('ketamine');
        const ketcPath = config.get('compilerPath') || 'ketc';

        try {
            lspClient = cp.spawn(ketcPath, ['--lsp']);
            let buffer = '';

            lspClient.stdout.on('data', (data) => {
                buffer += data.toString();
                const parts = buffer.split('\n');
                buffer = parts.pop() || '';
                for (const line of parts) {
                    if (line.trim()) {
                        try {
                            const msg = JSON.parse(line);
                            handleLspMessage(msg);
                        } catch (e) { /* ignore partial */ }
                    }
                }
            });

            lspClient.on('exit', () => { lspClient = null; });
            output.appendLine('LSP server started');
        } catch (e) {
            output.appendLine('Failed to start LSP: ' + e.message);
        }
    }

    function sendLsp(msg) {
        if (lspClient) {
            lspClient.stdin.write(JSON.stringify(msg) + '\n');
        }
    }

    function handleLspMessage(msg) {
        if (msg.method === 'textDocument/publishDiagnostics') {
            const uri = vscode.Uri.parse(msg.params.uri);
            const diagnostics = msg.params.diagnostics.map(d => {
                return new vscode.Diagnostic(
                    new vscode.Range(d.range.start.line, d.range.start.character,
                                    d.range.end.line, d.range.end.character),
                    d.message,
                    d.severity === 1 ? vscode.DiagnosticSeverity.Error
                        : d.severity === 2 ? vscode.DiagnosticSeverity.Warning
                        : vscode.DiagnosticSeverity.Information
                );
            });
            const diagCollection = vscode.languages.createDiagnosticCollection('ketamine');
            diagCollection.set(uri, diagnostics);
        }
    }

    // ── Commands ────────────────────────────────────────────────────────────

    // Run File (JIT)
    context.subscriptions.push(
        vscode.commands.registerCommand('ketamine.runFile', async () => {
            const editor = vscode.window.activeTextEditor;
            if (!editor) return;
            const filePath = editor.document.fileName;
            output.clear();
            output.show();
            output.appendLine(`▶ Running: ${filePath}`);

            try {
                const result = cp.execSync(`ketc "${filePath}" --target jit 2>&1`, {
                    cwd: path.dirname(filePath)
                });
                output.appendLine(result.toString());
            } catch (e) {
                output.appendLine('✗ Error: ' + e.message);
                output.appendLine(e.stderr?.toString() || '');
            }
        })
    );

    // Compile to LLVM
    context.subscriptions.push(
        vscode.commands.registerCommand('ketamine.compileToLLVM', async () => {
            const editor = vscode.window.activeTextEditor;
            if (!editor) return;
            const filePath = editor.document.fileName;
            const outPath = filePath.replace('.kt', '.ll');
            output.clear();
            output.show();
            output.appendLine(`▶ Compiling: ${filePath} → ${outPath}`);

            try {
                const result = cp.execSync(
                    `ketc "${filePath}" -o "${outPath}" --target llvm 2>&1`,
                    { cwd: path.dirname(filePath) }
                );
                output.appendLine(result.toString());
                output.appendLine('✓ Compilation complete');
            } catch (e) {
                output.appendLine('✗ Error: ' + e.message);
                output.appendLine(e.stderr?.toString() || '');
            }
        })
    );

    // ── Diagnostics on save ────────────────────────────────────────────────
    context.subscriptions.push(
        vscode.workspace.onDidSaveTextDocument(doc => {
            if (doc.languageId === 'ketamine') {
                try {
                    const result = cp.execSync(
                        `ketc "${doc.fileName}" --parse 2>&1`,
                        { cwd: path.dirname(doc.fileName) }
                    );
                    vscode.window.showInformationMessage('✓ Ketamine: No errors');
                } catch (e) {
                    const stderr = e.stderr?.toString() || '';
                    vscode.window.showErrorMessage('✗ Ketamine: ' + stderr.split('\n')[0]);
                }
            }
        })
    );

    // Start LSP
    startLsp();

    output.appendLine('Ketamine extension activated');
}

function deactivate() {}

module.exports = { activate, deactivate };
