// ═══════════════════════════════════════════════════════════════════════════════
// Tutorial 06: Generics — funcții și structuri parametrizate pe tipuri
// ═══════════════════════════════════════════════════════════════════════════════
//
// Concepte noi:
//   - `fn name<T>(param: T) -> T` — funcție generică
//   - `struct Name<T> { field: T }` — structură generică
//   - `<T, U: Bound>` — parametri generici cu constrângeri
//   - `where T: Trait` — where clause
//   - Inferența tipurilor la call-site
//
// ═══════════════════════════════════════════════════════════════════════════════

// Funcție generică simplă — identitate
fn identity<T>(x: T) -> T {
    return x;
}

// Funcție cu două tipuri generice
fn pair<T, U>(first: T, second: U) -> (T, U) {
    return (first, second);
}

// Structură generică — un container simplu
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

// Structură cu două tipuri generice
struct Pair<A, B> {
    first: A,
    second: B,
}

impl<A, B> Pair<A, B> {
    fn swap(self) -> Pair<B, A> {
        return Pair { first: self.second, second: self.first };
    }
}

// Funcție cu where clause
fn max<T>(a: T, b: T) -> T where T: Ord {
    if a > b {
        return a;
    } else {
        return b;
    }
}

fn main() {
    // Apel funcție generică — tipul e inferat
    let x = identity(42);
    print(x);

    let s = identity("hello");
    print(s);

    // Pair
    let p = pair(1, "doi");
    print(p.0);
    print(p.1);

    // Struct generică
    let box_int = Box { value: 100 };
    print(box_int.get());

    let box_str = Box { value: "text" };
    print(box_str.get());

    // Pair generic
    let num_pair = Pair { first: 1, second: "unu" };
    let swapped = num_pair.swap();
    print(swapped.first);
    print(swapped.second);

    // max cu where clause
    let m = max(10, 20);
    print(m);
}
