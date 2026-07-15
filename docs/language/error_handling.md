# Ketamine Language — Error Handling

## 1. Option Type

```ketamine
enum Option {
    None,
    Some(int),
}

fn divide(a: int, b: int) -> Option {
    if b == 0 {
        return Option::None;
    }
    return Option::Some(a / b);
}

fn main() {
    let result = divide(10, 2);
    match result {
        Option::Some(v) => { print(v); },
        Option::None => { print("division by zero"); },
    }
}
```

## 2. Result Type

```ketamine
enum Result {
    Ok(int),
    Err(str),
}

fn parse_int(s: str) -> Result {
    // Simplified parsing
    if s.len() > 0 {
        return Result::Ok(42);
    }
    return Result::Err("empty string");
}

fn main() {
    let res = parse_int("42");
    match res {
        Result::Ok(v) => { print(v); },
        Result::Err(e) => { print("error: " + e); },
    }
}
```

## 3. Pattern Matching

```ketamine
// Exhaustive matching
match opt {
    Option::None => { handle_error(); },
    Option::Some(v) => { use_value(v); },
}

// With guard
match x {
    n if n > 0 => { print("positive"); },
    n if n < 0 => { print("negative"); },
    _ => { print("zero"); },
}
```

## 4. Propagating Errors

```ketamine
fn safe_divide(a: int, b: int) -> Result {
    if b == 0 {
        return Result::Err("divide by zero");
    }
    return Result::Ok(a / b);
}

fn compute(a: int, b: int) -> Result {
    let step1 = safe_divide(a, b);
    match step1 {
        Result::Err(e) => { return Result::Err(e); },
        Result::Ok(v) => {
            return safe_divide(v, 2);
        },
    }
}
```
