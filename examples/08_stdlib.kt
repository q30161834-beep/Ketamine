// ═══════════════════════════════════════════════════════════════════════════════
// Tutorial 08: Standard Library — folosirea modulelor std
// ═══════════════════════════════════════════════════════════════════════════════
//
// Concepte noi:
//   - `import core;` — import module
//   - `import core::*;` — import totul dintr-un modul
//   - Folosirea colecțiilor: Vec, HashMap, etc.
//   - Funcții matematice
//   - I/O
//
// ═══════════════════════════════════════════════════════════════════════════════

import core;

// Funcții ajutătoare din stdlib (simulate)
fn print_vec(v: Vec<int>) {
    for i in 0..v.len() {
        print(v.get(i));
    }
}

fn main() {
    // Vec — listă dinamică
    let mut numbers = Vec::new();
    numbers.push(1);
    numbers.push(2);
    numbers.push(3);

    let first = numbers.get(0);
    print(first);

    let len = numbers.len();
    print(len);

    // Iterare peste vector
    print_vec(numbers);

    // HashMap — dicționar (dacă există în stdlib)
    // let mut map = HashMap::new();
    // map.insert("cheie", 42);
    // let val = map.get("cheie");

    // Funcții matematice
    // let abs_val = math::abs(-5);
    // let sqrt_val = math::sqrt(16.0);

    // Option — gestionarea valorilor nule
    let opt = Option::Some(42);
    let val = match opt {
        Option::Some(v) => { v },
        Option::None => { 0 },
    };
    print(val);

    // Result — gestionarea erorilor
    let res = Result::Ok("succes");
    match res {
        Result::Ok(msg) => { print(msg); },
        Result::Err(e) => { print("eroare: " + e); },
    }
}
