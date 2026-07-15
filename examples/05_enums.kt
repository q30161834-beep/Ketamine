// ═══════════════════════════════════════════════════════════════════════════════
// Tutorial 05: Enums + Match — tipuri sumă și pattern matching
// ═══════════════════════════════════════════════════════════════════════════════
//
// Concepte noi:
//   - `enum Name { Variant, Variant(type), ... }` — tip sumă
//   - `Name::Variant(val)` — construcție variantă
//   - `match expr { pattern => body, ... }` — pattern matching
//   - `_` — wildcard pattern (catch-all)
//   - Pattern-uri: literale, bindings, variante, tuple
//
// ═══════════════════════════════════════════════════════════════════════════════

// Enum simplu (ca în C)
enum Color {
    Red,
    Green,
    Blue,
}

// Enum cu date asociate
enum Shape {
    Circle(int),           // rază
    Rectangle(int, int),   // width, height
    Point,
}

// Enum cu Option și Result (ca în Rust)
enum Option {
    None,
    Some(int),
}

enum Result {
    Ok(int),
    Err(str),
}

fn main() {
    // ─── Enum simplu ───
    let c = Color::Red;
    match c {
        Color::Red => { print("roșu"); },
        Color::Green => { print("verde"); },
        Color::Blue => { print("albastru"); },
    }

    // ─── Enum cu date ───
    let shape = Shape::Circle(10);
    let area = match shape {
        Shape::Circle(r) => {
            r * r * 3  // 3.14 aproximat
        },
        Shape::Rectangle(w, h) => {
            w * h
        },
        Shape::Point => {
            0
        },
    };
    print(area);

    // ─── Option pattern ───
    let val = Option::Some(42);
    match val {
        Option::None => {
            print("nicio valoare");
        },
        Option::Some(v) => {
            print(v);
        },
    }

    // ─── Result pattern ───
    let res = Result::Ok(100);
    match res {
        Result::Ok(v) => {
            print(v);
        },
        Result::Err(msg) => {
            print(msg);
        },
    }
}
