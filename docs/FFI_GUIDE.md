# Ketamine FFI Guide

Foreign Function Interface — how to call Python, C, Rust, and JS libraries from Ketamine code.

## How It Works

When you write `import python::requests`, the compiler:
1. Records the import in `Program.imports` as `ImportDecl(lang="python", module="requests")`
2. At codegen time, translates `python::requests::get(url)` into the appropriate bridge call
3. The runtime bridge (`ffi.py`) dispatches to the correct language runtime

---

## Python FFI

### Import

```ketamine
import python::requests
import python::json
import python::hashlib
```

### Usage

```ketamine
import python::requests

fn fetch(url: str) -> str {
    let resp = python::requests::get(url)
    return resp.text()
}

fn fetch_json(url: str) -> str {
    let resp = python::requests::get(url)
    let data = resp.json()
    return python::json::dumps(data)
}
```

### What the codegen emits (JS target)

```javascript
// python::requests::get(url)  becomes:
const _ket_py = require('child_process');
function _py_call(module, fn, ...args) {
    const result = _ket_py.execSync(
        `python3 -c "import ${module}; import json; print(json.dumps(${fn}(${args.map(JSON.stringify).join(',')})))"`,
        { encoding: 'utf8' }
    );
    return JSON.parse(result);
}
const resp = _py_call('requests', 'get', url);
```

### Supported Python patterns

| Ketamine                                  | Python equivalent             |
|-------------------------------------------|-------------------------------|
| `python::requests::get(url)`              | `requests.get(url)`           |
| `python::json::loads(s)`                  | `json.loads(s)`               |
| `python::hashlib::sha256(data).hexdigest()` | `hashlib.sha256(data).hexdigest()` |
| `python::os::environ["PATH"]`             | `os.environ["PATH"]`          |

---

## C FFI (`c::`)

Calls into native C shared libraries via `ctypes`.

### Import

```ketamine
import c::sodium     // libsodium
import c::openssl    // OpenSSL
import c::sqlite3    // SQLite
```

### Usage — libsodium example

```ketamine
import c::sodium

fn generate_keypair() -> str {
    let pk = c::sodium::randombytes_buf(32)
    let sk = c::sodium::randombytes_buf(64)
    return pk.to_hex() + ":" + sk.to_hex()
}

fn encrypt_aes256gcm(plaintext: str, key: [u8; 32]) -> str {
    let nonce = c::sodium::randombytes_buf(12)
    let ct = c::sodium::crypto_aead_aes256gcm_encrypt(
        plaintext,
        key,
        nonce,
        null
    )
    return ct.to_base64()
}

fn decrypt_aes256gcm(ciphertext: str, key: [u8; 32], nonce: str) -> str {
    let pt = c::sodium::crypto_aead_aes256gcm_decrypt(
        ciphertext, key, nonce, null
    )
    return pt.to_str()
}
```

### Library name mapping

| `c::name`    | Shared library searched        |
|--------------|--------------------------------|
| `c::sodium`  | `libsodium.so` / `libsodium.dylib` / `libsodium.dll` |
| `c::openssl` | `libssl.so` / `libcrypto.so`   |
| `c::sqlite3` | `libsqlite3.so`                |
| `c::z`       | `libz.so` (zlib)               |

Custom: `import c::mylib` → searches for `libmylib.so` in `LD_LIBRARY_PATH` or system paths.

### What the runtime bridge does

```python
# ffi.py - CForeignBridge
import ctypes

lib = ctypes.CDLL("libsodium.so")
buf = ctypes.create_string_buffer(32)
lib.randombytes_buf(buf, 32)
result = buf.raw
```

---

## Rust FFI (`rust::`)

Calls compiled Rust crates that expose a C ABI (`#[no_mangle] extern "C"`).

### Import

```ketamine
import rust::tokio      // async runtime
import rust::serde      // serialization
import rust::ring       // cryptography
```

### Usage

```ketamine
import rust::tokio

fn run_async_task() -> void {
    let runtime = rust::tokio::Runtime::new()
    runtime.block_on(fn() {
        let result = rust::tokio::time::sleep(1000)
    })
}
```

### Requirement

The Rust crate must expose a C ABI. Example `Cargo.toml` + lib:

```toml
[lib]
crate-type = ["cdylib"]
```

```rust
#[no_mangle]
pub extern "C" fn my_function(input: i64) -> i64 {
    input * 2
}
```

---

## JavaScript FFI (`js::`)

For the JS compilation target, `js::` maps directly to global JS APIs.

### Import

```ketamine
import js::fetch        // browser Fetch API
import js::crypto       // Web Crypto API
import js::localStorage // Browser storage
```

### Usage

```ketamine
import js::fetch

fn get_data(url: str) -> str {
    let response = js::fetch(url)
    return response.text()
}
```

### Codegen output

```javascript
// js::fetch(url)  →  direct JS call
const response = fetch(url);
```

Since `js::` targets the same runtime, no bridging is needed — the codegen simply strips the `js::` prefix.

---

## FFI Security Considerations

| Risk                         | Mitigation                                      |
|------------------------------|-------------------------------------------------|
| Unsafe C memory access       | Bounds-checked wrapper generated automatically  |
| Python code injection        | Arguments are serialized, not interpolated raw  |
| Untrusted library loading    | `--secure` flag validates library signatures    |
| Shared library hijacking     | Use absolute paths or verified system paths     |
| Data type mismatch at ABI    | FFI bridge performs type coercion with checks   |

### Secure FFI example

```ketamine
import c::sodium

fn safe_encrypt(data: str, key: str) -> Result<str> {
    // Validate key length before passing to C
    if key.len() != 32 {
        return Err("Key must be exactly 32 bytes")
    }

    let nonce = c::sodium::randombytes_buf(12)
    let ct = c::sodium::crypto_aead_aes256gcm_encrypt(
        data, key, nonce, null
    )

    return Ok(ct.to_base64())
}
```

---

## Adding a New FFI Language

To add support for a new language (e.g., `go::`):

1. Add `"go"` to `FFI_LANGS` in `parser.py`
2. Add a `GoForeignBridge` class in `ffi.py`
3. Add `_gen_NamespaceExpr_go` handler in `codegen_js.py`
4. Map function signatures in `ffi_registry.json`
