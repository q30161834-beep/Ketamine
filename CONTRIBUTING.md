# Contributing to Ketamine

🎯 **Mulțumim că vrei să contribuiești!**

## 🚀 Quick Start

```bash
git clone https://github.com/ketamine-lang/ketamine.git
cd ketamine
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build .
ctest --output-on-failure
```

## 🧪 Teste

Rulează toate testele înainte de a trimite un PR:

```bash
./scripts/test_all.sh
```

Sau individual:
```bash
./build/test_lexer
./build/test_parser
./build/test_typecheck
./build/test_codegen
```

## 📝 Coding Style

- C11 standard
- Whitespace: 4 spaces (no tabs)
- Naming: `snake_case` pentru funcții și variabile
- Headers: nume de fișier în `types.h` style
- Comentarii doar în engleză sau română
- Fără // TODO în codul trimis

Formatare automată:
```bash
cmake --build build --target format
```

## 🏗 Arhitectură

```
src/
├── main.c              # CLI entry point
├── lexer.c             # Tokenizer
├── parser.c            # AST builder
├── typecheck.c         # Type inference (HM)
├── borrow.c            # Borrow checker
├── monomorph.c         # Generic specialization
├── trait_resolve.c     # Trait resolution
├── ir.c                # SSA IR lowering
├── ir_optimize.c       # Optimization passes
├── codegen.c           # Backends (LLVM/Go/Python/WASM)
├── package.c           # Package manager
├── lsp.c               # LSP server
├── arena.c             # Arena allocator
├── diagnostics.c       # Error reporting
└── symbol_table.c      # Symbol table
```

## 🔀 Branch Strategy

- `main` — stabil, release-ready
- `develop` — branch de integrare
- `feature/<name>` — features noi
- `fix/<name>` — bug fixes
- `docs/<name>` — documentație

## 📥 Pull Request

1. Fork și branch nou din `develop`
2. Scrie teste pentru codul nou
3. Asigură-te că `ctest` trece
4. Asigură-te că exemplele compilează
5. Creează PR către `develop`

## 🐛 Bug Reports

Deschide un issue cu:
- Versiunea compilatorului (`ketc --version`)
- Sistemul de operare
- Codul care cauzează problema
- Expected vs actual behavior

## 💬 Comunitate

- Discord: [https://discord.gg/ketamine-lang](https://discord.gg/ketamine-lang)
- GitHub Discussions: [https://github.com/ketamine-lang/ketamine/discussions](https://github.com/ketamine-lang/ketamine/discussions)

---

**Ketamine Language** — *Fiindcă un compilator adevărat merită contribuitori adevărați.*
