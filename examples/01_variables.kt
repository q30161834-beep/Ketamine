// ═══════════════════════════════════════════════════════════════════════════════
// Tutorial 01: Variables — let, mut, const
// ═══════════════════════════════════════════════════════════════════════════════
//
// Concepte noi:
//   - `let`  → variabilă imutabilă (nu poate fi reatribuită)
//   - `mut`  → variabilă mutabilă (poate fi schimbată)
//   - `:`    → specifică tipul variabilei (opțional, compilatorul inferă)
//   - `const` → constantă în timpul compilării (nivel modul)
//
// ═══════════════════════════════════════════════════════════════════════════════

// Constantă la nivel de modul
const MAX_PLAYERS: int = 4;
const GAME_NAME: str = "Ketamine Quest";

fn main() {
    // Imutabilă — valoarea nu poate fi schimbată după inițializare
    let nume = "Ketamine";

    // Cu tip explicit
    let varsta: int = 1;

    // Mutabilă — putem schimba valoarea
    let mut puncte = 0;
    puncte = puncte + 10;
    puncte += 5;

    // Constantele din modul sunt accesibile peste tot
    print("Joc: " + GAME_NAME);
    print("Jucători maximi: ");
    print(MAX_PLAYERS);

    // Variabilele pot umbri (shadow) altele din outer scope
    let x = 10;
    {
        let x = "shadow";  // aceasta o umbrește pe cea de sus
        print(x);
    }
    print(x);  // încă 10
}
