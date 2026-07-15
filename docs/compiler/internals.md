# Compiler Internals

## Architecture Overview

```
┌──────────┐   ┌──────────┐   ┌────────────┐   ┌──────────────┐
│  Source  │──▶│  Lexer   │──▶│  Parser    │──▶│ Type Checker │
│  .kt     │   │ tokens   │   │ AST        │   │ typed AST    │
└──────────┘   └──────────┘   └────────────┘   └──────┬───────┘
                                                        │
                                                        ▼
┌──────────┐   ┌──────────┐   ┌────────────┐   ┌──────────────┐
│ Output   │◀──│ Codegen  │◀──│ Optimizer  │◀──│  IR Lowering │
│ .ll/.go  │   │ LLVM/Go  │   │ DCE/GVN    │   │  SSA IR      │
└──────────┘   └──────────┘   └────────────┘   └──────────────┘
```

## Compilation Phases

| # | Phase | Description |
|---|-------|-------------|
| 1 | **Lex** | Tokenize source into tokens (Unicode, nested comments, raw strings) |
| 2 | **Parse** | Build AST (recursive descent + Pratt parsing) |
| 3 | **Sema** | Semantic analysis stub |
| 4 | **Type Check** | Hindley-Milner type inference with unification |
| 5 | **Borrow Check** | Ownership, borrowing, and lifetime analysis |
| 6 | **IR Gen** | Lower AST to SSA IR with basic blocks |
| 7 | **Optimize** | Optimization passes (constant folding, DCE, GVN, LICM) |
| 8 | **Codegen** | Emit target code (LLVM IR, Go, Python, WASM) |

## Memory Management

Arena allocator (`arena.c`):
- 256 MB default arena
- Bump allocation (O(1)) for AST nodes, types, IR
- ArenaMark/Restore for speculative parsing
- No per-node free needed — entire arena freed at once

## Type System

Hindley-Milner bidirectional type inference:
- Union-find unification with occurs check
- Let-polymorphism (generic instantiation at call sites)
- 40+ type kinds: primitives, arrays, pointers, tuples, fn types, structs, enums, generics, traits

## IR

SSA form with basic blocks:
- 60+ IR operations (arithmetic, memory, control flow, aggregates, atomics)
- Verification pass
- Multi-pass optimization pipeline
