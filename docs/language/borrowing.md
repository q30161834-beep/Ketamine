# Ketamine Language — Borrow Checker

## 1. Ownership Rules

1. Each value in Ketamine has a single **owner**.
2. When the owner goes out of scope, the value is **dropped**.
3. A value can be **moved** to a new owner.

## 2. Move Semantics

```ketamine
let a = 42;
let b = a;           // a is MOVED to b
// print(a);         // ERROR: use of moved value

struct Large { data: [100]byte }
let x = Large { ... };
let y = x;           // moved (not shallow copy)
```

## 3. Borrowing (&)

```ketamine
let a = 42;
let b = &a;          // shared borrow
let c = &a;          // multiple shared borrows OK
print(*b);           // read through borrow
print(*c);           // read through borrow
```

## 4. Mutable Borrowing (&mut)

```ketamine
let mut a = 42;
let b = &mut a;      // mutable borrow (exclusive)
*b = 100;            // write through borrow
// let c = &a;       // ERROR: already mutably borrowed
// let d = &mut a;   // ERROR: can't borrow mutably twice
```

## 5. Rules Summary

| Action | Shared Borrow | Mutable Borrow | Moved |
|--------|---------------|----------------|-------|
| Read | OK | OK | ERROR |
| Write | ERROR | OK | ERROR |
| Move | ERROR | ERROR | ERROR |
| Shared Borrow | OK | ERROR | ERROR |
| Mutable Borrow | ERROR | ERROR | ERROR |

## 6. Lifetime Elision

Lifetimes are inferred where possible:

```ketamine
fn first(a: &str, b: &str) -> &str    // lifetimes inferred
fn longest<'a>(a: &'a str, b: &'a str) -> &'a str  // explicit
```
