// ═══════════════════════════════════════════════════════════════════════════════
// Tutorial 03: Control Flow — if, while, for, loop, break, continue
// ═══════════════════════════════════════════════════════════════════════════════
//
// Concepte noi:
//   - `if cond { ... } else { ... }` — condițional
//   - `while cond { ... }` — buclă cu condiție
//   - `for var in iter { ... }` — iterație peste un interval
//   - `loop { ... }` — buclă infinită
//   - `break` — ieșire din buclă
//   - `continue` — sare la următoarea iterație
//
// ═══════════════════════════════════════════════════════════════════════════════

fn main() {
    // ─── if / else ───
    let x = 10;

    if x > 0 {
        print("pozitiv");
    } else {
        print("negativ sau zero");
    }

    // if expresie (returnează valoare)
    let result = if x >= 10 { "mare" } else { "mic" };
    print(result);

    // ─── while ───
    let mut i = 0;
    while i < 5 {
        print(i);
        i = i + 1;
    }

    // ─── for (range) ───
    for j in 0..5 {
        print(j);
    }

    // ─── loop + break ───
    let mut k = 0;
    loop {
        print(k);
        k = k + 1;
        if k >= 3 {
            break;
        }
    }

    // ─── continue ───
    let mut sum = 0;
    for n in 1..10 {
        if n % 2 == 0 {
            continue;  // sare peste numerele pare
        }
        sum = sum + n;
    }
    print(sum);

    // ─── match (pattern matching) ───
    let val = 2;
    match val {
        1 => { print("unu"); },
        2 => { print("doi"); },
        _ => { print("altceva"); },
    }
}
