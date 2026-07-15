# Ketamine Compiler Internals

## Architecture Overview

```
Source (.kt)
    │
    ▼
┌──────────────┐
│    Lexer     │  Tokenizes source → [Token]
└──────┬───────┘
       │
       ▼
┌──────────────┐
│   Parser     │  Recursive descent + Pratt → AST
└──────┬───────┘
       │
       ▼
┌──────────────┐
│ Type Checker │  Semantic analysis, type inference, borrow checking
└──────┬───────┘
       │
       ▼
┌──────────────┐
│  IR Lower    │  AST → SSA IR with basic blocks
└──────┬───────┘
       │
       ▼
┌──────────────┐
│  Optimizer   │  Multi-pass: CF, DCE, GVN, LICM, inlining
└──────┬───────┘
       │
       ▼
┌──────────────┐
│   Codegen    │  LLVM IR / Go / WASM / Native
└──────────────┘
```

## File Structure (25,000+ lines)

```
src/
├── main.c              — Entry point, CLI, phase orchestration
├── lexer.c             — Unicode-aware lexer with error recovery
├── parser.c            — Recursive descent + Pratt expression parser
├── arena.c             — Bump allocator with fallback regions
├── diagnostics.c       — Error/warning/note system with color output
├── symbol_table.c      — Type definitions, type table, symbol tables
├── ir.c                — SSA IR with basic blocks, CFG, AST lowering
├── ir_optimize.c       — Optimization passes (CF, DCE, GVN, LICM, inlining)
├── codegen.c           — LLVM IR, Go, WASM, and native backends
└── include/
    ├── types.h         — All types: AST, Type System, IR, Diagnostics
    ├── lexer.h         — Lexer API
    ├── parser.h        — Parser API
    ├── arena.h         — Arena allocator API
    └── codegen.h       — Codegen API
```

## Key Design Decisions

### 1. Arena Allocator
- Bump allocation for compiler data structures
- O(1) allocation, O(1) free (reset entire arena)
- mmap fallback for large blocks (>1MB)
- ArenaMark/Restore for speculative parsing

### 2. Pratt Expression Parser
- Precedence-climbing parser for expressions
- Token types map to precedence levels
- Handles: prefix, postfix (call, index, member), infix, assignment, range

### 3. SSA IR
- Static Single Assignment form with Phi nodes
- Basic blocks form Control Flow Graph (CFG)
- Dominator tree computed for optimization passes
- All values are SSA registers (infinite register set)

### 4. Type System
- Hindley-Milner type inference with bidirectional checking
- Generics with monomorphization
- Traits with vtable dispatch
- Borrow checker with NLL (Non-Lexical Lifetimes)

### 5. Optimization Pipeline
- **O0**: No optimization
- **O1**: Constant folding, dead code elimination, strength reduction
- **O2**: GVN, LICM, inlining
- **O3**: Full optimization (vectorization, loop unrolling, IPA)

## Adding a New Feature

1. Add token type to `types.h` and `lexer.c`
2. Add AST node kind to `types.h` with parse function in `parser.c`
3. Add type checking in the semantic analysis pass
4. Add IR lowering in `ir.c`
5. Add optimization rules in `ir_optimize.c` (optional)
6. Add codegen in `codegen.c` for each backend

## Testing

```bash
# Unit tests
mkdir build && cd build
cmake .. -DKET_BUILD_TESTS=ON
make
ctest -V

# Integration tests
./ketc --lex examples/hello.kt
./ketc --parse examples/hello.kt
./ketc --dump-ast examples/hello.kt
./ketc -O2 examples/hello.kt -o hello.ll
```

## Performance Targets

- Lexer: >1 GB/s
- Parser: >500 MB/s
- Codegen: >200 MB/s
- Memory: <256 MB for typical projects
- Binary size: <5 MB (stripped)
