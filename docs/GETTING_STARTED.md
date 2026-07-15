# Getting Started with Ketamine

## Installation

### Prerequisites

| Tool        | Required for               | Install                                      |
|-------------|----------------------------|----------------------------------------------|
| Python 3.10+| Python compiler (ketc.py)  | https://python.org                           |
| GCC / Clang | C compiler backend         | `apt install build-essential` / Xcode        |
| CMake 3.16+ | C backend build            | https://cmake.org                            |
| LLVM / llc  | Native binary from LLVM IR | `apt install llvm` / `brew install llvm`     |
| Node.js 18+ | Run JS output              | https://nodejs.org                           |
| WABT        | Convert WAT → WASM         | https://github.com/WebAssembly/wabt          |

### Clone and run

```bash
git clone <repo>
cd ketamine

# Run the Python compiler directly — no build needed
python3 pycompiler/ketc.py examples/hello.kt -o hello.js
node hello.js
```

### Build the C compiler

```bash
mkdir build && cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/ketc examples/hello.kt -o hello.ll
llc -filetype=obj hello.ll -o hello.o
clang hello.o -o hello
./hello
```

---

## Your First Program

Create `hello.kt`:

```ketamine
fn main() -> int {
    print("Hello, Ketamine!")
    return 0
}
```

Compile and run:

```bash
python3 pycompiler/ketc.py hello.kt -o hello.js
node hello.js
# → Hello, Ketamine!
```

---

## Examples

### Fibonacci

```ketamine
fn fib(n: int) -> int {
    if n <= 1 {
        return n
    }
    return fib(n - 1) + fib(n - 2)
}

fn main() -> int {
    mut let i = 0
    while i < 10 {
        print(str(fib(i)))
        i = i + 1
    }
    return 0
}
```

### Struct + method

```ketamine
struct Rectangle {
    width:  float,
    height: float
}

impl Rectangle {
    fn area(self) -> float {
        return self.width * self.height
    }

    fn perimeter(self) -> float {
        return 2.0 * (self.width + self.height)
    }
}

fn main() -> int {
    let r = Rectangle { width: 10.0, height: 5.0 }
    print("Area: " + str(r.area()))
    print("Perimeter: " + str(r.perimeter()))
    return 0
}
```

### Using FFI — HTTP request

```ketamine
import python::requests

fn fetch_post(id: int) -> str {
    let url = "https://jsonplaceholder.typicode.com/posts/" + str(id)
    let resp = python::requests::get(url)
    return resp.text()
}

fn main() -> int {
    let data = fetch_post(1)
    print(data)
    return 0
}
```

### Using FFI — Encryption

```ketamine
import c::sodium

fn main() -> int {
    // Generate 32 random bytes for a key
    let key = c::sodium::randombytes_buf(32)
    let message = "Secret message"

    let encrypted = c::sodium::crypto_secretbox_easy(message, key)
    let decrypted = c::sodium::crypto_secretbox_open_easy(encrypted, key)

    print(decrypted)
    return 0
}
```

---

## Compiler Flags Reference

```
python3 pycompiler/ketc.py [flags] <source.kt>

Flags:
  -o FILE           Output file (default: out.js)
  --target          js | wasm | llvm  (default: js)
  --lex             Dump tokens only
  --parse           Check syntax only, no codegen
  --secure          Enable static security analysis
  --obfuscate       Obfuscate output code
  --no-anti-debug   Skip debugger detection at startup
  -O 0..3           Optimization level
```

### Common workflows

```bash
# Check syntax
python3 pycompiler/ketc.py program.kt --parse

# Inspect tokens
python3 pycompiler/ketc.py program.kt --lex

# Compile to JS with security scan
python3 pycompiler/ketc.py program.kt --secure -o out.js

# Compile to WASM
python3 pycompiler/ketc.py program.kt --target wasm -o out.wat

# Compile to LLVM IR
python3 pycompiler/ketc.py program.kt --target llvm -o out.ll

# Secure + obfuscated release build
python3 pycompiler/ketc.py program.kt --secure --obfuscate -O3 -o release.js
```

---

## Project Layout

```
ketamine/
├── pycompiler/          Python compiler (main)
│   ├── ketc.py          Entry point / CLI
│   ├── lexer.py         Tokenizer
│   ├── parser.py        AST builder
│   ├── semantic.py      Type checker
│   ├── codegen_js.py    → JavaScript
│   ├── codegen_wasm.py  → WebAssembly (WAT)
│   ├── codegen_llvm.py  → LLVM IR
│   ├── security.py      Static analysis + anti-debug
│   ├── obfuscator.py    Output obfuscation
│   └── ffi.py           Foreign function bridge
│
├── src/                 C compiler (alternative backend)
│   ├── main.c
│   ├── lexer.c
│   ├── parser.c
│   ├── codegen.c
│   └── include/
│
├── stdlib/
│   └── core.kt          Standard library signatures
│
├── examples/
│   └── hello.kt
│
├── docs/
│   ├── GETTING_STARTED.md   ← you are here
│   ├── LANGUAGE_REFERENCE.md
│   ├── COMPILER_INTERNALS.md
│   ├── FFI_GUIDE.md
│   └── SECURITY.md
│
├── CMakeLists.txt
└── README.md
```
