# Ketamine Language — Syntax Reference

## 1. Comments

```ketamine
// Line comment
/* Block comment */
/* Nested /* block */ comment */
```

## 2. Literals

```ketamine
42          // decimal integer
0xFF        // hex integer
0b1010      // binary integer
3.14        // float
"hello"     // string
'A'         // char
true        // bool
false       // bool
null        // null value
```

## 3. Variables

```ketamine
let x = 42;              // immutable, type inferred
let y: int = 10;         // immutable, explicit type
let mut z = 5;           // mutable
let mut w: float = 3.14; // mutable + explicit type
```

## 4. Functions

```ketamine
fn greet() {
    print("hello");
}

fn add(a: int, b: int) -> int {
    return a + b;
}

fn multiply(a: int, b: int) -> int {
    a * b  // implicit return
}

fn factorial(n: int) -> int {
    if n <= 1 { return 1; }
    n * factorial(n - 1)
}
```

## 5. Control Flow

```ketamine
// if / else
if x > 0 {
    print("positive");
} else if x < 0 {
    print("negative");
} else {
    print("zero");
}

// if as expression
let result = if x >= 10 { "big" } else { "small" };

// while
let mut i = 0;
while i < 10 {
    i = i + 1;
}

// for
for i in 0..10 {
    print(i);
}

// loop + break + continue
loop {
    if done { break; }
    if skip { continue; }
}

// match
match value {
    1 => { print("one"); },
    2 => { print("two"); },
    _ => { print("other"); },
}
```

## 6. Operators (Precedence)

| Level | Operators | Assoc |
|-------|-----------|-------|
| 1 | `.` `()` `[]` | left |
| 2 | `!` `-` `*` `&` | right |
| 3 | `as` | left |
| 4 | `*` `/` `%` | left |
| 5 | `+` `-` | left |
| 6 | `<<` `>>` | left |
| 7 | `&` | left |
| 8 | `^` | left |
| 9 | `\|` | left |
| 10 | `==` `!=` `<` `>` `<=` `>=` | left |
| 11 | `&&` | left |
| 12 | `\|\|` | left |
| 13 | `..` `..=` | left |
| 14 | `=` `+=` `-=` `*=` `/=` | right |
| 15 | `\|>` | left |

## 7. Compound Assignments

```ketamine
x += 1;   x -= 2;   x *= 3;
x /= 4;   x %= 5;   x &= 6;
x |= 7;   x ^= 8;   x <<= 9;
x >>= 10;
```

## 8. Closure / Lambda

```ketamine
let f = || -> int { return 42; };
let g = |x: int| -> int { x * 2; };
let result = f();
```

## 9. Pipe Forward

```ketamine
let result = 5 |> double |> add_one;
// equivalent to: add_one(double(5))
```

## 10. Range

```ketamine
0..10       // exclusive: 0..9
0..=10      // inclusive: 0..10
```
