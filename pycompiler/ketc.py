#!/usr/bin/env python3
"""
Ketamine Compiler - Python Implementation
Secure, fast, with anti-debug and obfuscation
"""
import sys
import argparse
from pathlib import Path
from lexer import Lexer
from parser import Parser
from semantic import SemanticAnalyzer
from codegen_js import JSCodeGen
from codegen_wasm import WASMCodeGen
from codegen_llvm import LLVMCodeGen
from codegen_go import GoCodeGen
from security import SecurityAnalyzer, AntiDebug
from obfuscator import CodeObfuscator
from ffi import ImportResolver

VERSION = "0.3.0"

def main():
    parser = argparse.ArgumentParser(
        description=f"Ketamine Compiler v{VERSION} - Secure & Fast"
    )
    parser.add_argument("input", help="Source file (.kt)")
    parser.add_argument("-o", "--output", help="Output file", default="out.js")
    parser.add_argument("--target", choices=["js", "wasm", "llvm", "go"], default="js",
                        help="Compilation target")
    parser.add_argument("--lex", action="store_true", help="Dump tokens only")
    parser.add_argument("--parse", action="store_true", help="Parse only (check syntax)")
    parser.add_argument("--secure", action="store_true", help="Enable security analysis")
    parser.add_argument("--obfuscate", action="store_true", help="Obfuscate output")
    parser.add_argument("--no-anti-debug", action="store_true", help="Disable anti-debug")
    parser.add_argument("-O", "--optimize", type=int, default=1, choices=[0,1,2,3],
                        help="Optimization level")
    
    args = parser.parse_args()
    
    # Anti-debug check
    if not args.no_anti_debug:
        if AntiDebug.is_debugger_present():
            print("⚠️  Debugger detected! Compilation may be compromised.")
            return 1
    
    # Read source
    try:
        source = Path(args.input).read_text(encoding="utf-8")
    except Exception as e:
        print(f"ketc: error reading '{args.input}': {e}", file=sys.stderr)
        return 1
    
    # Lexical analysis
    lexer = Lexer(source, args.input)
    tokens = lexer.tokenize()
    
    if lexer.errors:
        for err in lexer.errors:
            print(err, file=sys.stderr)
        return 1
    
    if args.lex:
        for tok in tokens:
            print(tok)
        return 0
    
    # Parse
    parser = Parser(tokens)
    ast = parser.parse()
    
    if parser.errors:
        for err in parser.errors:
            print(err, file=sys.stderr)
        return 1
    
    if args.parse:
        print(f"✓ Syntax OK ({len(ast.declarations)} declarations, {len(ast.imports)} imports)")
        return 0

    # Resolve imports (validate FFI availability)
    resolver = ImportResolver()
    import_warnings = []
    for imp in ast.imports:
        warn = resolver.resolve(imp)
        if warn:
            import_warnings.append(warn)

    if import_warnings:
        for w in import_warnings:
            print(f"  {w}", file=sys.stderr)
    
    # Semantic analysis
    analyzer = SemanticAnalyzer()
    analyzer.analyze(ast)
    
    if analyzer.errors:
        for err in analyzer.errors:
            print(err, file=sys.stderr)
        return 1
    
    # Security analysis
    if args.secure:
        sec = SecurityAnalyzer()
        issues = sec.analyze(ast)
        if issues:
            print("🔒 Security issues found:")
            for issue in issues:
                print(f"  - {issue}")
            if sec.has_critical:
                return 1
    
    # Code generation
    if args.target == "js":
        codegen = JSCodeGen(optimize=args.optimize)
    elif args.target == "wasm":
        codegen = WASMCodeGen(optimize=args.optimize)
    elif args.target == "go":
        codegen = GoCodeGen(optimize=args.optimize)
        if not args.output.endswith(".go"):
            args.output = args.output.rsplit(".", 1)[0] + ".go" if "." in args.output else args.output + ".go"
    else:
        codegen = LLVMCodeGen(optimize=args.optimize)
    
    output = codegen.generate(ast)
    
    # Obfuscation
    if args.obfuscate:
        obf = CodeObfuscator()
        output = obf.obfuscate(output, args.target)
    
    # Write output
    try:
        Path(args.output).write_text(output, encoding="utf-8")
        print(f"✓ Compiled {args.input} → {args.output} ({args.target})")
        return 0
    except Exception as e:
        print(f"ketc: error writing output: {e}", file=sys.stderr)
        return 1

if __name__ == "__main__":
    sys.exit(main())
