# Ketamine Security Architecture

## Overview

Security in Ketamine operates at three layers:

```
┌─────────────────────────────────────────────┐
│ 1. Compiler Security (static analysis)       │
│    SecurityAnalyzer — scans AST              │
├─────────────────────────────────────────────┤
│ 2. Runtime Security (generated code)         │
│    CFI, stack canaries, bounds checks        │
├─────────────────────────────────────────────┤
│ 3. Compiler Self-Protection                  │
│    AntiDebug, SandboxRunner                  │
└─────────────────────────────────────────────┘
```

---

## Layer 1: Static Security Analysis

Activate with `--secure` flag:

```bash
ketc program.kt --secure
```

### Checks performed

| Check                          | Severity | Blocks compilation? |
|--------------------------------|----------|---------------------|
| Dangerous function call        | CRITICAL | Yes                 |
| Division by zero (constant)    | CRITICAL | Yes                 |
| Integer overflow (constant)    | WARNING  | No                  |
| Potential div-by-zero (var)    | WARNING  | No                  |
| Unchecked user input           | WARNING  | No                  |
| Parameter count > 32           | WARNING  | No                  |

### Example output

```
🔒 Security issues found:
  - Line 14: CRITICAL: Division by zero
  - Line 22: Potential division by zero (unchecked variable)
  - Line 31: Unchecked user input from 'read_line'
ketc: compilation failed (critical security issues)
```

---

## Layer 2: Runtime Security in Generated Code

### Control Flow Integrity (CFI)

The LLVM backend can emit CFI checks. Every function gets a prologue/epilogue:

```c
// Generated CFI prologue
uintptr_t __stack_chk_value = __stack_chk_guard;

// Generated CFI epilogue
if (__stack_chk_value != __stack_chk_guard) {
    __stack_chk_fail();
}
```

Compile the C backend with:
```bash
cmake .. -DCMAKE_BUILD_TYPE=Release
# Already includes: -fstack-protector-strong -D_FORTIFY_SOURCE=2
```

### Shadow Stack

Protects return addresses from corruption:

```c
// runtime_protect.c (planned)
static void* shadow_stack[256];
static int   shadow_idx = 0;

void push_shadow(void* ret_addr) {
    if (shadow_idx < 256)
        shadow_stack[shadow_idx++] = ret_addr;
}

void verify_shadow(void* ret_addr) {
    if (shadow_idx == 0 || shadow_stack[--shadow_idx] != ret_addr)
        __stack_chk_fail();
}
```

### Bounds Checking (Arrays)

All array accesses in generated JS include bounds checks:

```javascript
// Ketamine: arr[i]
// Generated JS:
if (i < 0 || i >= arr.length) throw new Error(`Index ${i} out of bounds`);
arr[i];
```

### Memory Protection

For the LLVM/C target, sensitive data (keys, passwords) can be marked read-only after use:

```c
// mprotect wrapper
mprotect(key_buffer, KEY_SIZE, PROT_READ);
// ... use key ...
mprotect(key_buffer, KEY_SIZE, PROT_READ | PROT_WRITE);
memset(key_buffer, 0, KEY_SIZE);   // secure zero
mprotect(key_buffer, KEY_SIZE, PROT_NONE);
```

---

## Layer 3: Compiler Self-Protection

### Anti-Debugging

`AntiDebug.is_debugger_present()` checks at compiler startup (unless `--no-anti-debug` is passed):

**Windows:**
```python
kernel32 = ctypes.windll.kernel32
return kernel32.IsDebuggerPresent() != 0
```

**Linux:**
```python
with open('/proc/self/status') as f:
    for line in f:
        if line.startswith('TracerPid:'):
            return int(line.split(':')[1].strip()) != 0
```

**macOS:**
```python
# checks P_TRACED flag via sysctl
```

### Environment Checks

```python
AntiDebug.check_environment()
# Flags: LD_PRELOAD, DYLD_INSERT_LIBRARIES
```

### Timing Check

```python
AntiDebug.timing_check()
# A trivial sum(range(1000)) taking > 10ms indicates
# breakpoints or single-stepping
```

---

## Output Code Protection

### Obfuscation (`--obfuscate`)

```bash
ketc program.kt --obfuscate -o out.js
```

Techniques applied to JS output:

1. **Identifier renaming** — all user symbols become `_1_abc`, `_2_def`, …
2. **Dead code insertion** — 10% of lines get unreachable snippets
3. **String encoding** — all string literals Base64-encoded with `atob()`
4. **Control flow flattening** — ~20% of `if` blocks wrapped in opaque predicates like `(1 === 1)`

### Anti-Tampering

```python
from obfuscator import AntiTampering

protected = AntiTampering.generate_check(my_js_code)
```

Prepends a SHA-256 self-check that aborts execution if the JS file is modified after compilation.

### Polymorphic Output

```python
from obfuscator import PolyEngine

# Each build produces a differently-fingerprinted binary
v1 = PolyEngine.polymorphize(code, seed=1)
v2 = PolyEngine.polymorphize(code, seed=2)
# v1 and v2 behave identically but differ byte-for-byte
```

---

## Cryptography via FFI

Ketamine does not implement its own crypto. Instead it bridges to audited libraries:

### libsodium (via `c::sodium`)

| Function                                  | Algorithm           |
|-------------------------------------------|---------------------|
| `crypto_aead_aes256gcm_encrypt`           | AES-256-GCM         |
| `crypto_aead_chacha20poly1305_encrypt`    | ChaCha20-Poly1305   |
| `crypto_sign_keypair`                     | Ed25519             |
| `crypto_sign_detached`                    | Ed25519 signature   |
| `crypto_pwhash`                           | Argon2id            |
| `randombytes_buf`                         | CSPRNG              |

Example:

```ketamine
import c::sodium

fn hash_password(password: str) -> str {
    let hash = c::sodium::crypto_pwhash_str(
        password,
        c::sodium::OPSLIMIT_INTERACTIVE,
        c::sodium::MEMLIMIT_INTERACTIVE
    )
    return hash
}

fn verify_password(hash: str, password: str) -> bool {
    return c::sodium::crypto_pwhash_str_verify(hash, password) == 0
}
```

### Python cryptography lib (via `python::cryptography`)

```ketamine
import python::cryptography.fernet

fn encrypt_secret(data: str, key: str) -> str {
    let f = python::cryptography.fernet::Fernet(key)
    return f.encrypt(data.to_bytes()).decode()
}
```

---

## C Backend Hardening Flags

`CMakeLists.txt` includes these security flags in Release builds:

```cmake
-Wall -Wextra -Wpedantic
-Wformat=2 -Wformat-security
-fstack-protector-strong
-fPIE -pie
-D_FORTIFY_SOURCE=2
-O2 -march=native
-ftrapv          # trap on integer overflow
-fwrapv          # defined wrapping semantics
```

Link flags:
```
-Wl,-z,relro     # relocation read-only
-Wl,-z,now       # full RELRO
```

---

## Security Checklist

| Protection                      | Status         | How to enable                    |
|---------------------------------|----------------|----------------------------------|
| Static analysis                 | ✅ Implemented  | `--secure`                       |
| Identifier obfuscation          | ✅ Implemented  | `--obfuscate`                    |
| Dead code injection             | ✅ Implemented  | `--obfuscate`                    |
| String encoding                 | ✅ Implemented  | `--obfuscate`                    |
| Anti-tampering checksum         | ✅ Implemented  | `AntiTampering.generate_check`   |
| Polymorphic output              | ✅ Implemented  | `PolyEngine.polymorphize`        |
| Anti-debug (Win/Linux/macOS)    | ✅ Implemented  | default on (disable `--no-anti-debug`) |
| Sandbox execution               | ✅ Implemented  | `SandboxRunner`                  |
| Stack canaries (C backend)      | ✅ CMake flags  | Release build                    |
| ASLR / PIE (C backend)          | ✅ CMake flags  | Release build                    |
| CFI prologue/epilogue           | 🔲 Planned      | Future `--cfi` flag              |
| Shadow stack                    | 🔲 Planned      | Future runtime                   |
| Bounds-checked arrays (C)       | 🔲 Planned      | Future semantic pass             |
| Memory zeroing after use        | 🔲 Planned      | Future ownership system          |
