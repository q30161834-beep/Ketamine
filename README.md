[![Build & Test](https://github.com/ketamine-lang/ketamine/actions/workflows/build.yml/badge.svg)](https://github.com/ketamine-lang/ketamine/actions/workflows/build.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C Standard](https://img.shields.io/badge/C-11-blue.svg)](https://en.wikipedia.org/wiki/C11_(C_standard_revision))
[![Version](https://img.shields.io/badge/version-0.1.0--alpha-orange.svg)](https://github.com/ketamine-lang/ketamine/releases)
[![Lines of Code](https://img.shields.io/tokei/lines/github/ketamine-lang/ketamine)](https://github.com/ketamine-lang/ketamine)
[![PRs Welcome](https://img.shields.io/badge/PRs-welcome-brightgreen.svg)](CONTRIBUTING.md)
[![Sponsor](https://img.shields.io/badge/sponsor-30363D?logo=GitHub-Sponsors&logoColor=#EA54AE)](https://github.com/sponsors/q30161834-beep)
[![Discord](https://img.shields.io/badge/Discord-5865F2?logo=discord&logoColor=white)](https://discord.gg/kbz7GpYAM)

# Ketamine Programming Language

A compiled, statically-typed language focused on performance and security.  
Compiles to LLVM IR → native binary.

## Quick syntax

```ketamine
fn add(a: int, b: int) -> int {
    return a + b
}

fn main() -> int {
    let result = add(3, 4)
    print(str(result))
    return 0
}
```

## Key features

- Static typing with type inference (`let x = 42` infers `int`)
- Immutable by default, `mut` keyword for mutable variables
- No null — use `Result<T>` and `Option<T>` enums
- Pattern matching with `match`
- Generics with `<T>` syntax
- Structs + `impl` blocks for methods
- Native compilation via LLVM (zero runtime overhead)
- Memory safety without GC: ownership + borrowing model
- Built-in security: bounds-checked arrays, no implicit casts

## Build the compiler

```bash
# Linux / macOS
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
./ketc --help

# Windows (MSVC or MinGW)
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
mingw32-make
```

**Requirements:** CMake 3.16+, GCC or Clang, LLVM toolchain (for final binary)

## Compile a program

```bash
# Step 1: parse + emit LLVM IR
./ketc examples/hello.kt -o hello.ll

# Step 2: LLVM → object file
llc -filetype=obj hello.ll -o hello.o

# Step 3: link
clang hello.o -o hello

# Run
./hello
```

Or shorthand with clang directly:
```bash
./ketc examples/hello.kt -o hello.ll && clang hello.ll -o hello && ./hello
```

## Debug tools

```bash
./ketc --lex  examples/hello.kt    # dump all tokens
./ketc --parse examples/hello.kt   # check syntax only
```

## Language reference

### Types
| Type    | Description              | Example           |
|---------|--------------------------|-------------------|
| `int`   | 64-bit signed integer    | `42`, `-7`        |
| `float` | 64-bit double            | `3.14`, `-0.5`    |
| `bool`  | Boolean                  | `true`, `false`   |
| `str`   | UTF-8 string             | `"hello"`         |
| `[T;N]` | Fixed-size array         | `[int; 10]`       |
| `void`  | No return value          |                   |

### Variables
```ketamine
let x = 10          // immutable int
mut let y = 0       // mutable
let name: str = "Keto"
```

### Functions
```ketamine
fn square(n: int) -> int {
    return n * n
}
```

### Control flow
```ketamine
if x > 0 { print("positive") }
else { print("non-positive") }

while x < 10 { x = x + 1 }

for item in array { print(item) }
```

### Structs
```ketamine
struct Vec2 { x: float  y: float }
impl Vec2 {
    fn len(self) -> float { return sqrt(self.x*self.x + self.y*self.y) }
}
```

### Enums + match
```ketamine
enum Color { Red, Green, Blue }
match color {
    Red   => print("red")
    Green => print("green")
    Blue  => print("blue")
}
```

## Community

- 💬 [Discord](https://discord.gg/kbz7GpYAM) — chat, support, development
- 🐛 [Issues](https://github.com/q30161834-beep/Ketamine/issues) — bug reports & feature requests
- ⭐ [Sponsor](https://github.com/sponsors/q30161834-beep) — susține dezvoltarea

## Quick Install

```bash
curl -fsSL https://raw.githubusercontent.com/q30161834-beep/Ketamine/main/scripts/install.sh | bash
```

## Roadmap

- [x] Type checker pass
- [x] Generics full support (monomorphization + traits + where clauses)
- [x] Borrow checker
- [x] Standard library in Ketamine itself (8 modules)
- [x] Package manager (`ket init`, `ket install`, `ket add`)
- [x] WASM backend
- [x] LSP server (completions, hover, goto-def, diagnostics)
- [x] CI/CD pipeline (GitHub Actions — Linux, macOS, Windows)
- [x] Native x86-64 backend (raw machine code + assembly text)
- [x] JIT compilation (runtime compile & execute)
- [x] WASM in-browser playground (`docs/playground/index.html`)
- [ ] Self-hosting compiler (write Ketamine compiler in Ketamine)
- [ ] WASM binary format output (.wasm) instead of WAT
- [ ] AArch64 backend
