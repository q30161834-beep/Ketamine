# Instalare Ketamine Compiler

## Cerințe de Sistem

| Componentă | Versiune minimă | Recomandată |
|------------|----------------|-------------|
| CMake      | 3.16           | 3.25+       |
| GCC (Linux/macOS/Windows) | 6.3 | 11+    |
| Clang (opțional) | 12.0     | 17+         |
| LLVM (opțional, pentru `--target llvm`) | 14 | 17+ |
| NASM (opțional, pentru `--target asm`) | 2.15 | 2.16+ |
| Python (opțional, pentru `--target python`) | 3.8 | 3.12+ |
| Go (opțional, pentru `--target go`) | 1.18 | 1.22+ |
| Node.js (opțional, pentru `--target js`) | 16 | 20+ |
| Git | 2.20 | 2.40+ |

## Instalare pe Linux (Ubuntu/Debian)

```bash
# 1. Instalează dependențele
sudo apt update
sudo apt install -y build-essential cmake git llvm-17-dev nasm

# 2. Clonează repository-ul
git clone https://github.com/q30161834-beep/Ketamine.git
cd Ketamine

# 3. Build cu CMake
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)

# 4. Adaugă în PATH
echo 'export PATH="$PATH:'$(pwd)'/build"' >> ~/.bashrc
source ~/.bashrc

# 5. Verifică instalarea
ketc --version
```

## Instalare pe macOS

```bash
# 1. Instalează Homebrew (dacă nu există)
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# 2. Instalează dependențele
brew install cmake llvm nasm git

# 3. Clonează și build
git clone https://github.com/q30161834-beep/Ketamine.git
cd Ketamine
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(sysctl -n hw.logicalcpu)

# 4. Adaugă în PATH
echo 'export PATH="$PATH:'$(pwd)'"' >> ~/.zshrc
source ~/.zshrc

# 5. Verifică
ketc --version
```

## Instalare pe Windows

### Opțiunea 1: MSYS2 (recomandat)

```bash
# 1. Instalează MSYS2 de la https://www.msys2.org/
# 2. Deschide MSYS2 UCRT64 și rulează:
pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake \
          mingw-w64-ucrt-x86_64-nasm git make

# 3. Clonează și build
git clone https://github.com/q30161834-beep/Ketamine.git
cd Ketamine
mkdir build && cd build
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
mingw32-make -j$(nproc)

# 4. Adaugă în PATH (permanent)
echo 'export PATH="$PATH:/c/Users/$(whoami)/Ketamine/build"' >> ~/.bashrc
```

### Opțiunea 2: Visual Studio

```powershell
# 1. Instalează Visual Studio 2022 cu "Desktop development with C++"
#    de la https://visualstudio.microsoft.com/

# 2. Instalează CMake și Git
winget install CMake
winget install Git.Git

# 3. Deschide "Developer Command Prompt for VS 2022"
git clone https://github.com/q30161834-beep/Ketamine.git
cd Ketamine
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release

# 4. Adaugă în PATH (System Properties → Environment Variables)
#    Adaugă: C:\Users\<nume>\Ketamine\build\Release
```

### Opțiunea 3: MinGW direct

```powershell
# 1. Instalează MinGW de la https://www.mingw-w64.org/
# 2. Adaugă C:\mingw-w64\bin în PATH
# 3. Deschide Command Prompt și rulează:
git clone https://github.com/q30161834-beep/Ketamine.git
cd Ketamine
mkdir build && cd build
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
mingw32-make -j
```

## Instalare rapidă (one-liner)

```bash
curl -fsSL https://raw.githubusercontent.com/q30161834-beep/Ketamine/main/scripts/install.sh | bash
```

## Verificare

```bash
# După instalare, verifică:
ketc --version        # Versiunea compilatorului
ketc --help           # Toate opțiunile disponibile

# Rulează un program simplu:
echo 'fn main() -> int { print("Hello, Ketamine!"); return 0 }' > hello.kt
ketc hello.kt         # Compilează în LLVM IR
ketc --target jit hello.kt  # Compilează și rulează cu JIT
```

## Structură Proiect

```
Ketamine/
├── src/              # Codul sursă al compilatorului
│   ├── include/      # Headere
│   └── *.c           # Modulele compilatorului
├── stdlib/           # Biblioteca standard
├── tests/            # Teste unitare și de integrare
├── benchmarks/       # Benchmark-uri
├── examples/         # Exemple de programe
├── docs/             # Documentație și website
├── vscode-ketamine/  # Extensie VS Code
└── scripts/          # Scripturi de instalare/utilitare
```

## Build cu opțiuni avansate

```bash
# Build cu suport pentru AddressSanitizer (debugging)
cmake .. -DCMAKE_BUILD_TYPE=Debug -DKET_ENABLE_ASAN=ON

# Build cu optimizări LTO
cmake .. -DCMAKE_BUILD_TYPE=Release -DKET_ENABLE_LTO=ON

# Build fără teste
cmake .. -DKET_BUILD_TESTS=OFF

# Build shared library
cmake .. -DKET_BUILD_SHARED=ON
```

## Depanare

### Eroare: `gcc not found`
Instalează GCC:
- **Linux**: `sudo apt install build-essential`
- **macOS**: `xcode-select --install`
- **Windows**: Instalează MinGW sau MSYS2

### Eroare: `CMake 3.16+ required`
Instalează CMake:
- **Linux**: `sudo apt install cmake`
- **macOS**: `brew install cmake`
- **Windows**: `winget install CMake`

### Eroare: `LLVM not found`
Instalează LLVM sau folosește `--target native` în loc de `--target llvm`:
```bash
ketc program.kt --target jit   # Fără LLVM
ketc program.kt --target native # Fără LLVM
ketc program.kt --target asm   # Fără LLVM
```
