# Standard Library — Core

## `Option`

```ketamine
enum Option<T> {
    None,
    Some(T),
}
```

Methods:
- `is_some() -> bool`
- `is_none() -> bool`
- `unwrap() -> T`
- `unwrap_or(default: T) -> T`
- `map<U>(f: fn(T) -> U) -> Option<U>`

## `Result`

```ketamine
enum Result<T, E> {
    Ok(T),
    Err(E),
}
```

Methods:
- `is_ok() -> bool`
- `is_err() -> bool`
- `unwrap() -> T`
- `unwrap_err() -> E`
- `map<U>(f: fn(T) -> U) -> Result<U, E>`

## `Iterator`

```ketamine
trait Iterator {
    type Item;
    fn next(self) -> Option<Item>;
}
```

## `Clone`

```ketamine
trait Clone {
    fn clone(self) -> Self;
}
```

## `Display`

```ketamine
trait Display {
    fn display(self) -> str;
}
```

## `Eq` / `Ord`

```ketamine
trait Eq {
    fn eq(self, other: Self) -> bool;
}

trait Ord: Eq {
    fn lt(self, other: Self) -> bool;
    fn le(self, other: Self) -> bool;
    fn gt(self, other: Self) -> bool;
    fn ge(self, other: Self) -> bool;
}
```
