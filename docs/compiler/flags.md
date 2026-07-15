# Compiler Flags Reference

## Basic Usage

```bash
ketc <source.kt> [options]
```

## Output Options

| Flag | Description |
|------|-------------|
| `-o <file>` | Output file path (default: `out.ll`) |
| `--target <t>` | Target backend: `llvm`, `go`, `python`, `js`, `wasm` |

## Optimization

| Flag | Level |
|------|-------|
| `-O0` | No optimization (default) |
| `-O1` | Basic optimizations |
| `-O2` | Standard optimizations |
| `-O3` | Aggressive optimizations |

## Diagnostic Flags

| Flag | Description |
|------|-------------|
| `--lex` | Dump tokens and exit |
| `--parse` | Parse only (check syntax) |
| `--dump-ast` | Print AST and exit |
| `--dump-ir` | Print IR and exit |

## Output Control

| Flag | Description |
|------|-------------|
| `--emit-llvm` | Emit LLVM IR |
| `--emit-ir` | Emit internal IR |
| `-S` | Emit assembly |
| `-c` | Compile to object file |

## Analysis Flags

| Flag | Description |
|------|-------------|
| `--time` | Show phase timing + memory profiling |
| `--verbose` | Verbose output |
| `--secure` | Enable security analysis |
| `--obfuscate` | Obfuscate output |
| `-j <n>` | Parallel jobs |

## Examples

```bash
# Compile to LLVM IR
ketc hello.kt -o hello.ll

# Compile to Go
ketc hello.kt --target go -o hello.go

# Compile to Python
ketc hello.kt --target python -o hello.py

# Dump tokens
ketc --lex hello.kt

# Dump AST
ketc --dump-ast hello.kt

# Show profile
ketc hello.kt --time -O2

# Full pipeline
ketc hello.kt -o hello.ll && llc -filetype=obj hello.ll -o hello.o && clang hello.o -o hello
```
