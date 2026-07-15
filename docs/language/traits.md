# Ketamine Language — Traits & Impl

## 1. Defining a Trait

```ketamine
trait Display {
    fn display(self) -> str;
}

trait Greeter {
    fn greet(self) -> str;

    // Method with default implementation
    fn greet_loudly(self) -> str {
        return self.greet();
    }
}
```

## 2. Implementing a Trait

```ketamine
struct Person {
    name: str,
}

impl Display for Person {
    fn display(self) -> str {
        return "Person: " + self.name;
    }
}

impl Greeter for Person {
    fn greet(self) -> str {
        return "Hello, " + self.name;
    }
}
```

## 3. Using Traits

```ketamine
// Direct method call
let p = Person { name: "Ana" };
let s = p.display();

// Generic function with trait bound
fn print_twice<T: Display>(val: T) {
    print(val.display());
    print(val.display());
}
```

## 4. Inherent Impl (no trait)

```ketamine
impl Point {
    fn distance_from_origin(self) -> int {
        return self.x * self.x + self.y * self.y;
    }

    // Static method (no self)
    fn origin() -> Point {
        return Point { x: 0, y: 0 };
    }
}
```

## 5. Trait with Generics

```ketamine
trait Into<T> {
    fn into(self) -> T;
}

impl Into<str> for int {
    fn into(self) -> str {
        return int_to_str(self);
    }
}
```
