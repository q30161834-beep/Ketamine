// ═══════════════════════════════════════════════════════════════════════════════
// Tutorial 00: Hello, World!
// ═══════════════════════════════════════════════════════════════════════════════
//
// Cum compilezi și rulezi:
//   ketc 00_hello.kt -o hello.ll
//   llc -filetype=obj hello.ll -o hello.o
//   clang hello.o -o hello
//   ./hello
//
// Sau direct cu Go backend:
//   ketc 00_hello.kt --target go -o hello.go
//   go run hello.go
//
// Sau cu Python backend:
//   ketc 00_hello.kt --target python -o hello.py
//   python3 hello.py
//
// ═══════════════════════════════════════════════════════════════════════════════

fn main() {
    // Afișează mesajul în consolă
    print("Hello, World!");
}
