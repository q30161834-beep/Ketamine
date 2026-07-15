// Ketamine Language - Example Program
// File: hello.kt

import core

// Basic types: int, float, bool, str, void
// All variables are immutable by default, use 'mut' for mutable

fn greet(name: str) -> str {
    return "Hello, " + name + "!"
}

fn factorial(mut n: int) -> int {
    if n <= 1 {
        return 1
    }
    return n * factorial(n - 1)
}

// Structs with methods
struct Point {
    x: float
    y: float
}

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

// Enums with data
enum Result<T> {
    Ok(T),
    Err(str)
}

fn divide(a: float, b: float) -> Result<float> {
    if b == 0.0 {
        return Err("Division by zero")
    }
    return Ok(a / b)
}

fn main() -> int {
    // Immutable by default
    let msg = greet("World")
    print(msg)

    // Mutable variable
    mut let counter = 0
    while counter < 5 {
        print(factorial(counter))
        counter = counter + 1
    }

    // Pattern matching
    let result = divide(10.0, 2.0)
    match result {
        Ok(val) => print("Result: " + str(val))
        Err(e)  => print("Error: " + e)
    }

    // Arrays and loops
    let numbers: [int; 5] = [1, 2, 3, 4, 5]
    for num in numbers {
        print(num * num)
    }

    return 0
}
