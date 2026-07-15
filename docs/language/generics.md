# Ketamine Language — Generics

## 1. Generic Functions

```ketamine
// Single type parameter
fn identity<T>(x: T) -> T {
    return x;
}

// Multiple type parameters
fn pair<A, B>(a: A, b: B) -> (A, B) {
    return (a, b);
}
```

## 2. Generic Structs

```ketamine
struct Box<T> {
    value: T,
}

impl<T> Box<T> {
    fn get(self) -> T {
        return self.value;
    }

    fn set(self, val: T) {
        self.value = val;
    }
}
```

## 3. Generic Enums

```ketamine
enum Option<T> {
    None,
    Some(T),
}

enum Result<T, E> {
    Ok(T),
    Err(E),
}
```

## 4. Where Clauses

```ketamine
fn max<T>(a: T, b: T) -> T where T: Ord {
    if a > b { return a; }
    return b;
}

// Multiple bounds
fn process<T>(val: T) where T: Display + Clone {
    let s = val.display();
    let copy = val;
}
```

## 5. Trait Bounds on Parameters

```ketamine
// Short form
fn print_display<T: Display>(val: T) {
    print(val.display());
}

// Equivalent with where
fn print_display<T>(val: T) where T: Display {
    print(val.display());
}
```

## 6. Monomorphization

Generics are compiled via monomorphization (like C++ templates and Rust):

```ketamine
let a = identity(42);      // generates identity$int
let b = identity("hi");    // generates identity$str
```

Each unique combination of type arguments produces a separate specialized copy.
