# Politica de Securitate Ketamine

## Versiuni Suportate

| Versiune | Suportată |
|----------|-----------|
| 0.1.x    | ✅ (parțial) |
| < 0.1    | ❌ |

## Raportarea unei Vulnerabilități

Luăm securitatea în serios. Dacă ai descoperit o vulnerabilitate de securitate în compilatorul Ketamine, te rugăm să urmezi acești pași:

### 1. **NU deschide un Issue public**
Deschide un issue **PRIVAT** pe GitHub folosind [Security Advisories](https://github.com/q30161834-beep/Ketamine/security/advisories) sau trimite un email la: **q30161834@gmail.com**

### 2. Ce să incluzi în raport:
- **Descrierea vulnerabilității** — ce se întâmplă și de ce e periculos
- **Pași de reproducere** — cod sau comenzi care demonstrează problema
- **Impact potențial** — execuție de cod arbitrar, denial of service, etc.
- **Versiunea afectată** — commit hash sau versiunea
- **Platforma** — OS, compilator, arhitectură
- **Fix sugerat** — dacă ai deja o soluție

### 3. Ce să aștepți:
- **Confirmare** în 48 de ore că am primit raportul
- **Evaluare** în 5 zile lucrătoare
- **Patch** în 14 zile pentru vulnerabilități critice
- **Atribuire** în CHANGELOG și commit message (dacă dorești)

### 4. Recompensă:
În această fază incipientă, nu oferim recompense financiare, dar:
- Vei fi listat în SECURITY.md și CHANGELOG ca descoperitor
- Vei primi acces timpuriu la feature-uri noi
- Vei fi invitat în programul de bug bounty când va fi lansat

## Zone de Risc

Zonele cu risc ridicat de securitate în compilator:

| Zonă | Risc | Descriere |
|------|------|-----------|
| **Parser** | Mediu | Fișiere sursă malițioase pot cauza stack overflow |
| **Codegen LLVM** | Scăzut | LLVM e bine testat, dar generarea de cod IR poate avea bug-uri |
| **JIT** | **Ridicat** | Execuția de cod arbitrar în memorie. Rulează doar cod de încredere |
| **Native/x86-64** | Mediu | Emiterea directă de instrucțiuni mașină poate fi periculoasă |
| **FFI** | **Ridicat** | Interfața cu biblioteci externe poate introduce vulnerabilități |
| **REPL** | Mediu | Evaluează cod arbitrar — nu rula în producție |
| **Package Registry** | Mediu | Pachete terțe pot conține cod malițios |
| **Macro-uri** | Scăzut | Expansiunea macro-urilor poate duce la cod neașteptat |

## Best Practices pentru Utilizatori

1. **Rulează doar cod de încredere** în JIT/REPL
2. **Verifică hash-urile** pachetelor descărcate din registry
3. **Nu folosi `--unsafe`** decât dacă ai înțeles riscurile
4. **Păstrează compilatorul actualizat** la ultima versiune
5. **Folosește `--secure`** pentru analiză de securitate automată

## Politica de Divulgare

- **Vulnerabilitățile critice** vor fi patch-uite în 14 zile
- **Vulnerabilitățile medii** vor fi patch-uite în 30 de zile
- **Vulnerabilitățile low** vor fi patch-uite în 60 de zile

## Mulțumiri Speciale

Le mulțumim următorilor cercetători pentru raportări responsabile:

*(Nimeni încă — poți fi primul!)*
