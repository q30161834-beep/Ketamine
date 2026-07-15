# Ketamine Tutorial — Pas cu Pas

Bun venit! Acest ghid te va duce de la zero la programe complexe în Ketamine.

---

## 1. Hello, World!

```ketamine
fn main() -> int {
    print("Hello, World!")
    return 0
}
```

Compilează și rulează:
```bash
ketc hello.kt -o hello.ll          # LLVM IR
llc -filetype=obj hello.ll -o hello.o
clang hello.o -o hello
./hello
```

Sau rapid cu JIT:
```bash
ketc hello.kt --target jit
```

---

## 2. Variabile și Tipuri

```ketamine
fn main() -> int {
    let x = 42               // int (inferat)
    let y: float = 3.14      // float (explicit)
    let name = "Ketamine"    // string
    let is_true = true       // bool

    print("x = " + str(x))
    print("y = " + str(y))
    print("name = " + name)
    return 0
}
```

**Reguli:**
- `let` → imutabil
- `mut let` → mutabil
- Fără `null` — folosește `Option<T>` sau `Result<T>`

---

## 3. Funcții

```ketamine
fn adunare(a: int, b: int) -> int {
    return a + b
}

fn factorial(n: int) -> int {
    if n <= 1 { return 1 }
    return n * factorial(n - 1)
}

fn main() -> int {
    print(str(adunare(3, 4)))       // 7
    print(str(factorial(10)))        // 3628800
    return 0
}
```

---

## 4. Control Flow

```ketamine
fn main() -> int {
    // if / else
    let x = 10
    if x > 0 {
        print("pozitiv")
    } else {
        print("non-pozitiv")
    }

    // while
    mut let i = 0
    while i < 5 {
        print(str(i))
        i = i + 1
    }

    // for
    let arr = [1, 2, 3, 4, 5]
    for val in arr {
        print(str(val))
    }

    return 0
}
```

---

## 5. Structuri și Metode

```ketamine
struct Point {
    x: float,
    y: float
}

impl Point {
    fn new(x: float, y: float) -> Point {
        return Point { x, y }
    }

    fn dist(self) -> float {
        return sqrt(self.x * self.x + self.y * self.y)
    }

    fn scale(self, factor: float) -> Point {
        return Point {
            x: self.x * factor,
            y: self.y * factor
        }
    }
}

fn main() -> int {
    let p = Point::new(3.0, 4.0)
    print("distanța: " + str(p.dist()))   // 5.0

    let p2 = p.scale(2.0)
    print("dublu: " + str(p2.dist()))      // 10.0
    return 0
}
```

---

## 6. Enum-uri și Pattern Matching

```ketamine
enum Option<T> {
    Some(T),
    None
}

enum Color {
    Red,
    Green,
    Blue,
    Rgb(int, int, int)
}

fn describe(c: Color) -> str {
    match c {
        Red       => return "roșu"
        Green     => return "verde"
        Blue      => return "albastru"
        Rgb(r, g, b) => {
            return "rgb(" + str(r) + "," + str(g) + "," + str(b) + ")"
        }
    }
}

fn main() -> int {
    let c = Color::Rgb(255, 0, 0)
    print(describe(c))
    return 0
}
```

---

## 7. Generice și Trait-uri

```ketamine
trait Display {
    fn fmt(self) -> str
}

impl Display for int {
    fn fmt(self) -> str { return str(self) }
}

impl Display for float {
    fn fmt(self) -> str { return str(self) }
}

fn print_all<T: Display>(items: [T]) {
    for item in items {
        print(item.fmt())
    }
}

fn main() -> int {
    let nums = [1, 2, 3, 4, 5]
    print_all(nums)
    return 0
}
```

---

## 8. Ownership și Borrowing

```ketamine
fn main() -> int {
    let s = "salut"

    // Borrow (împrumut)
    let len = str_length(&s)
    print(str(len))

    // s este încă valid aici (doar l-am împrumutat)
    print(s)

    return 0
}

fn str_length(s: &str) -> int {
    return len(s)   // lungimea string-ului
}
```

**Reguli de ownership:**
- Fiecare valoare are un singur owner
- `&T` — borrow partajat (citește)
- `&mut T` — borrow exclusiv (scrie)
- Borrow-ul nu poate depăși lifetime-ul valorii originale

---

## 9. Program complet: HTTP Server

```ketamine
import http
import json

struct User {
    name: str,
    age: int
}

fn main() -> int {
    let server = http::Server::new("0.0.0.0:8080")

    server.get("/hello", fn(req) -> http::Response {
        return http::Response::json(json::object(
            "message", "Hello from Ketamine!"
        ))
    })

    server.get("/user/:name", fn(req) -> http::Response {
        let user = User {
            name: req.param("name"),
            age: 25
        }
        return http::Response::json(json::to_string(user))
    })

    print("Server pornit pe http://0.0.0.0:8080")
    server.listen()
    return 0
}
```

Compilează:
```bash
ketc server.kt --target go -o server.go
go run server.go
```

---

## 10. Ce Urmează

| Resursă | Descriere |
|---------|-----------|
| `examples/` | 11 exemple didactice |
| `docs/language/` | Referința completă a limbajului |
| `docs/stdlib/` | Documentația bibliotecii standard |
| `docs/compiler/` | Cum funcționează compilatorul |

Pentru ajutor:
```bash
ketc --help          # Toate opțiunile
ketc --target list   # Backend-uri disponibile
ketc --version       # Versiunea
```

---

*Ketamine Language — Fiindcă un compilator adevărat merită utilizatori adevărați.*
