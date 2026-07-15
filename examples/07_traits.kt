// ═══════════════════════════════════════════════════════════════════════════════
// Tutorial 07: Traits + Impl — polymorphism și shared behavior
// ═══════════════════════════════════════════════════════════════════════════════
//
// Concepte noi:
//   - `trait Name { fn method(self); }` — definește un comportament
//   - `impl Trait for Type { fn method(self) { ... } }` — implementează
//   - `T: Trait` — constrângere de trait pe generic
//   - `impl<T: Trait> Type<T>` — implementare condiționată
//
// ═══════════════════════════════════════════════════════════════════════════════

// Definirea unui trait
trait Display {
    fn display(self) -> str;
}

// Implementare pentru int
impl Display for int {
    fn display(self) -> str {
        return "int";
    }
}

// Implementare pentru str
impl Display for str {
    fn display(self) -> str {
        return self;
    }
}

// Trait cu metodă default
trait Greeter {
    fn greet(self) -> str;

    // Metodă cu implementare default
    fn greet_loudly(self) -> str {
        return self.greet();
    }
}

// Implementare pentru struct
struct Person {
    name: str,
}

impl Greeter for Person {
    fn greet(self) -> str {
        return "Salut, eu sunt " + self.name;
    }
}

// Funcție generică cu trait bound
fn print_display<T: Display>(val: T) {
    let s = val.display();
    print(s);
}

fn main() {
    // Trait pe tipuri built-in
    let s1 = 42.display();
    print(s1);

    let s2 = "hello".display();
    print(s2);

    // Funcție cu trait bound
    print_display(100);
    print_display("world");

    // Metodă default
    let p = Person { name: "Ana" };
    let g = p.greet();
    print(g);

    let g2 = p.greet_loudly();
    print(g2);
}
