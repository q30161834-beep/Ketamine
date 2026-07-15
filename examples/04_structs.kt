// ═══════════════════════════════════════════════════════════════════════════════
// Tutorial 04: Structs — definire, creare, metode, field-uri
// ═══════════════════════════════════════════════════════════════════════════════
//
// Concepte noi:
//   - `struct Name { field: type, ... }` — definește un tip de date
//   - `Name { field: val, ... }` — creează o instanță
//   - `obiect.field` — accesează un câmp
//   - `impl Name { fn method(self) { ... } }` — metode
//   - `self` — referință la instanța curentă
//
// ═══════════════════════════════════════════════════════════════════════════════

// Definirea unui struct
struct Point {
    x: int,
    y: int,
}

// Struct cu metode
impl Point {
    // Metodă care primește self (instanța)
    fn distance_from_origin(self) -> int {
        return self.x * self.x + self.y * self.y;
    }

    // Metodă care modifică self (mutabil)
    fn translate(self, dx: int, dy: int) {
        self.x = self.x + dx;
        self.y = self.y + dy;
    }
}

// Struct cu field-uri publice și metode statice
struct Rectangle {
    width: int,
    height: int,
}

impl Rectangle {
    // Constructor-style (fără self → metodă statică)
    fn square(size: int) -> Rectangle {
        return Rectangle { width: size, height: size };
    }

    fn area(self) -> int {
        return self.width * self.height;
    }
}

// Struct în alt struct
struct Circle {
    center: Point,
    radius: int,
}

impl Circle {
    fn area(self) -> float {
        return 3.14159 * self.radius * self.radius;
    }
}

fn main() {
    // Creare instanță
    let p = Point { x: 3, y: 4 };

    // Accesare câmpuri
    print(p.x);
    print(p.y);

    // Apelare metodă
    let dist = p.distance_from_origin();
    print(dist);

    // Struct cu constructor
    let square = Rectangle::square(5);
    print(square.area());

    // Struct imbricată
    let c = Circle {
        center: Point { x: 0, y: 0 },
        radius: 10,
    };
    print(c.area());

    // Apelare metodă mutabilă
    let mut p2 = Point { x: 1, y: 2 };
    p2.translate(5, 5);
    print(p2.x);
    print(p2.y);
}
