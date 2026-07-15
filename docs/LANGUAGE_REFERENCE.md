# Ketamine Language Reference

## Table of Contents
1. [Overview](#overview)
2. [Type System](#type-system)
3. [Variables](#variables)
4. [Functions](#functions)
5. [Control Flow](#control-flow)
6. [Structs & Enums](#structs--enums)
7. [Arrays](#arrays)
8. [FFI — Foreign Function Interface](#ffi)
9. [Pattern Matching](#pattern-matching)
10. [Operators](#operators)
11. [Standard Library](#standard-library)
12. [Security Model](#security-model)

---

## Overview

Ketamine is a compiled, statically-typed language with:
- Immutable-by-default variables
- Type inference
- Native FFI for Python, C, Rust, and JavaScript libraries
- Compilation targets: JavaScript, WebAssembly, LLVM IR (native)
- Built-in security analysis and optional obfuscation

File extension: `.kt`

---

## Type System

### Primitive Types

| Type    | Size   | Description              | Example           |
|---------|--------|--------------------------|-------------------|
| `int`   | 64-bit | Signed integer           | `42`, `-7`, `0`   |
| `float` | 64-bit | IEEE 754 double          | `3.14`, `-0.5`    |
| `bool`  | 1-bit  | Boolean                  | `true`, `false`   |
| `str`   | UTF-8  | Immutable string         | `"hello"`         |
| `void`  | —      | No value / no return     | —                 |

### Composite Types

```ketamine
// Fixed-size array
let arr: [int; 5] = [1, 2, 3, 4, 5]

// Generic (from stdlib or FFI)
let result: Result<int> = Ok(42)
let maybe:  Option<str> = Some("hi")
```

### Type Inference

The compiler infers types from the initializer when `: type` is omitted:

```ketamine
let x = 10       // x: int
let y = 3.14     // y: float
let s = "hello"  // s: str
let b = true     // b: bool
```

---

## Variables

```ketamine
// Immutable (default)
let name = "Ketamine"
let count: int = 0

// Mutable — requires 'mut'
mut let score = 0
score = score + 1

// Both forms are valid
let mut counter = 0
```

Attempting to reassign an immutable variable is a compile-time error.

---

## Functions

```ketamine
// Basic function
fn add(a: int, b: int) -> int {
    return a + b
}

// No return value
fn greet(name: str) -> void {
    print("Hello, " + name)
}

// Type inference for return (planned)
fn square(n: int) -> int {
    return n * n
}

// Public function (exported from module)
pub fn factorial(n: int) -> int {
    if n <= 1 {
        return 1
    }
    return n * factorial(n - 1)
}
```

### Parameters

Parameters are always immutable inside the function body unless declared `mut`:

```ketamine
fn increment(mut n: int) -> int {
    n = n + 1
    return n
}
```

---

## Control Flow

### if / else

```ketamine
if x > 0 {
    print("positive")
} else if x == 0 {
    print("zero")
} else {
    print("negative")
}
```

### while

```ketamine
mut let i = 0
while i < 10 {
    print(i)
    i = i + 1
}
```

### for … in

```ketamine
let numbers = [1, 2, 3, 4, 5]
for n in numbers {
    print(n * n)
}
```

---

## Structs & Enums

### Struct

```ketamine
struct Point {
    x: float,
    y: float
}

struct User {
    id:       int,
    username: str,
    active:   bool
}
```

### Impl (methods)

```ketamine
impl Point {
    fn new(x: float, y: float) -> Point {
        return Point { x: x, y: y }
    }

    fn distance(self, other: Point) -> float {
        let dx = self.x - other.x
        let dy = self.y - other.y
        return sqrt(dx * dx + dy * dy)
    }
}
```

### Enum

```ketamine
enum Direction { North, South, East, West }

enum Result<T> {
    Ok(T),
    Err(str)
}
```

---

## Arrays

```ketamine
// Fixed-size, type-annotated
let scores: [int; 3] = [100, 95, 87]

// Access
let first = scores[0]

// Iteration
for score in scores {
    print(score)
}
```

---

## FFI

The FFI system lets you call external libraries directly.

### Import syntax

```ketamine
import core                      // Ketamine stdlib
import python::requests          // Python library
import python::requests as req   // With alias
import c::sodium                 // C library (libsodium)
import rust::tokio               // Rust crate
import js::fetch                 // JavaScript API
```

### Calling FFI functions

```ketamine
import python::requests

fn get_user(id: str) -> str {
    let url = "https://api.example.com/users/" + id
    let resp = python::requests::get(url)
    return resp.text()
}
```

```ketamine
import c::sodium

fn generate_key() -> str {
    let key = c::sodium::randombytes_buf(32)
    return key.to_hex()
}

fn encrypt(plaintext: str, key: str) -> str {
    let nonce = c::sodium::randombytes_buf(12)
    let ct = c::sodium::crypto_aead_aes256gcm_encrypt(
        plaintext, key, nonce, null
    )
    return ct.to_base64()
}
```

```ketamine
import rust::tokio

fn start_server() -> void {
    let task = rust::tokio::spawn(fn() {
        // async logic
    })
    rust::tokio::block_on(task)
}
```

### FFI Lang Resolution

| Prefix     | Backend              | Mechanism           |
|------------|----------------------|---------------------|
| `python::` | CPython via ctypes   | `importlib` bridge  |
| `c::`      | Native C shared lib  | `ctypes.CDLL`       |
| `rust::`   | Rust `.so`/`.dll`    | `ctypes` + FFI ABI  |
| `js::`     | Node.js / browser    | Native JS interop   |
| *(none)*   | Ketamine module      | Direct linking      |

---

## Pattern Matching

```ketamine
let result = divide(10.0, 0.0)

match result {
    Ok(val)  => print("Result: " + str(val))
    Err(msg) => print("Error: " + msg)
}

match direction {
    North => print("going north")
    South => print("going south")
    _     => print("going somewhere")
}
```

---

## Operators

| Operator | Description          | Types              |
|----------|----------------------|--------------------|
| `+`      | Add / concatenate    | int, float, str    |
| `-`      | Subtract             | int, float         |
| `*`      | Multiply             | int, float         |
| `/`      | Divide               | int, float         |
| `%`      | Modulo               | int                |
| `==`     | Equal                | all primitives     |
| `!=`     | Not equal            | all primitives     |
| `<`      | Less than            | int, float         |
| `>`      | Greater than         | int, float         |
| `<=`     | Less or equal        | int, float         |
| `>=`     | Greater or equal     | int, float         |
| `&&`     | Logical AND          | bool               |
| `\|\|`   | Logical OR           | bool               |
| `!`      | Logical NOT          | bool               |
| `-`      | Unary minus          | int, float         |
| `=`      | Assignment           | mutable only       |

### Precedence (high to low)

| Level | Operators              |
|-------|------------------------|
| 6     | `*`  `/`  `%`          |
| 5     | `+`  `-`               |
| 4     | `<`  `>`  `<=`  `>=`  |
| 3     | `==`  `!=`             |
| 2     | `&&`                   |
| 1     | `\|\|`                 |

---

## Standard Library

```ketamine
import core
```

### I/O
| Function       | Signature                | Description               |
|----------------|--------------------------|---------------------------|
| `print`        | `(val: str) -> void`     | Print with newline        |
| `println`      | `(val: str) -> void`     | Alias for print           |
| `read_line`    | `() -> str`              | Read line from stdin      |

### Type Conversion
| Function       | Signature                | Description               |
|----------------|--------------------------|---------------------------|
| `str`          | `(val: int) -> str`      | int to string             |
| `str`          | `(val: float) -> str`    | float to string           |
| `str`          | `(val: bool) -> str`     | bool to string            |
| `int`          | `(val: str) -> int`      | Parse integer             |
| `float`        | `(val: str) -> float`    | Parse float               |

### Math
| Function       | Signature                            | Description         |
|----------------|--------------------------------------|---------------------|
| `sqrt`         | `(x: float) -> float`                | Square root         |
| `abs`          | `(x: int) -> int`                    | Absolute value      |
| `pow`          | `(base: float, exp: float) -> float` | Power               |
| `min`          | `(a: int, b: int) -> int`            | Minimum             |
| `max`          | `(a: int, b: int) -> int`            | Maximum             |
| `clamp`        | `(v: int, lo: int, hi: int) -> int`  | Clamp to range      |

### Process
| Function       | Signature                | Description               |
|----------------|--------------------------|---------------------------|
| `exit`         | `(code: int) -> void`    | Exit with code            |
| `panic`        | `(msg: str) -> void`     | Fatal error + abort       |

---

## Security Model

Ketamine has a built-in security layer activated with `--secure`:

- Division-by-zero detection (compile-time when operands are constants)
- Integer overflow detection for constant expressions
- Warning on unchecked user input
- Blocking of dangerous built-in names (`eval`, `exec`, `system`)
- Parameter count limit (max 32) to prevent stack exhaustion

```bash
ketc program.kt --secure        # analyze + block on critical issues
ketc program.kt --secure -o out.js
```

### Security Levels

| Flag              | Effect                                          |
|-------------------|-------------------------------------------------|
| *(none)*          | No security analysis                            |
| `--secure`        | Full static analysis, block on critical errors  |
| `--obfuscate`     | Obfuscate output (rename idents, dead code)     |
| `--no-anti-debug` | Disable debugger detection                      |
