// ═══════════════════════════════════════════════════════════════════════════════
// Tutorial 02: Functions — fn, return, parameters
// ═══════════════════════════════════════════════════════════════════════════════
//
// Concepte noi:
//   - `fn name(params) -> return_type { body }` — definiție funcție
//   - `return expr` — returnează o valoare
//   - Ultima expresie dintr-un bloc este returnată implicit
//   - Funcțiile pot fi apelată recursiv
//
// ═══════════════════════════════════════════════════════════════════════════════

// Cea mai simplă funcție — fără parametri, fără return
fn greet() {
    print("Salut!");
}

// Funcție cu parametri și return explicit
fn add(a: int, b: int) -> int {
    return a + b;
}

// Funcție cu return implicit (ultima expresie)
fn multiply(a: int, b: int) -> int {
    a * b
}

// Funcție cu mai mulți parametri
fn calculate(a: int, b: int, op: str) -> int {
    if op == "add" {
        return a + b;
    } else if op == "sub" {
        return a - b;
    } else {
        return a * b;
    }
}

// Funcție recursivă — factorial
fn factorial(n: int) -> int {
    if n <= 1 {
        return 1;
    }
    return n * factorial(n - 1);
}

// Funcție void (nu returnează nimic)
fn log_message(msg: str) {
    print("[LOG]: ");
    print(msg);
}

fn main() {
    greet();

    let sum = add(10, 20);
    print(sum);

    let product = multiply(5, 6);
    print(product);

    let result = calculate(10, 5, "sub");
    print(result);

    let fact = factorial(5);
    print(fact);

    log_message("Done!");
}
