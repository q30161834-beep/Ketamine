# Ketamine Language — Type System

## 1. Primitive Types

| Type | Description | Size |
|------|-------------|------|
| `bool` | Boolean | 1 byte |
| `byte` | Unsigned byte | 1 byte |
| `char` | Unicode character | 4 bytes |
| `i8` / `i16` / `i32` / `i64` | Signed integers | 1/2/4/8 bytes |
| `u8` / `u16` / `u32` / `u64` | Unsigned integers | 1/2/4/8 bytes |
| `f32` / `f64` | Floating point | 4/8 bytes |
| `int` | Platform integer (alias for i64) | 8 bytes |
| `float` | Platform float (alias for f64) | 8 bytes |
| `str` | String (immutable, UTF-8) | pointer |
| `void` | No value | 0 |
| `never` | Never returns | 0 |

## 2. Compound Types

```ketamine
// Array
let arr: [10]int;                // fixed-size array
let arr_init = [1, 2, 3];        // array literal

// Pointer
let ptr: *int;                    // raw pointer
let ref: &int;                    // reference

// Tuple
let t: (int, str, bool) = (42, "hello", true);
let first = t.0;                 // access by index

// Function
let f: fn(int, int) -> int;      // function type
```

## 3. User-Defined Types

```ketamine
// Struct
struct Point {
    x: int,
    y: int,
}

// Enum
enum Option {
    None,
    Some(int),
}

// Type Alias
type MyInt = int;
```

## 4. Generic Types

```ketamine
struct Box<T> {
    value: T,
}

enum Option<T> {
    None,
    Some(T),
}
```

## 5. Type Inference

Ketamine uses Hindley-Milner type inference. You rarely need to specify types:

```ketamine
fn identity<T>(x: T) -> T { return x; }

let x = identity(42);        // int inferred
let y = identity("hello");   // str inferred
let z = identity(true);      // bool inferred
```

## 6. Casting

```ketamine
let x = 42 as float;     // int → float
let y = 3.14 as int;     // float → int (truncates)
```

## 7. Ownership & Borrowing

```ketamine
// Move (default)
let a = 42;
let b = a;               // a is MOVED to b
// print(a);             // ERROR: a is moved

// Shared borrow (&)
let c = &a;              // borrow
let d = &a;              // multiple shared borrows OK
print(*c);               // read-only

// Mutable borrow (&mut)
let mut e = 42;
let f = &mut e;          // exclusive borrow
*f = 100;                // write through borrow
// let g = &e;           // ERROR: already mutably borrowed
```
