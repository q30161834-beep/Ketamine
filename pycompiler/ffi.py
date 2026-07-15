"""
Ketamine FFI Runtime Bridge
Dispatches calls to Python, C, Rust, and JS foreign libraries.
"""
import ctypes
import ctypes.util
import importlib
import platform
import sys
from typing import Any, Dict, List, Optional

# ─── FFI Registry ─────────────────────────────────────────────────────────────
# Maps (lang, module, function) → wrapper callable.
# Populated lazily on first call.

_registry: Dict[str, Any] = {}

def resolve(lang: str, path: List[str], args: List[Any]) -> Any:
    """
    Main dispatch entry point called by generated code at runtime.

    lang  : 'python' | 'c' | 'rust' | 'js' | 'go'
    path  : ['module', 'submodule', 'function']   e.g. ['requests', 'get']
    args  : already-evaluated argument values
    """
    if lang == "python":
        return PythonBridge.call(path, args)
    elif lang == "c":
        return CForeignBridge.call(path, args)
    elif lang == "rust":
        return RustBridge.call(path, args)
    elif lang == "go":
        return GoForeignBridge().call(path, args)
    elif lang == "js":
        # JS target: no bridging needed — codegen handles it directly
        raise NotImplementedError("JS FFI is resolved at codegen time, not runtime")
    else:
        raise ValueError(f"Unknown FFI lang: '{lang}'")

# ─── Python Bridge ────────────────────────────────────────────────────────────

class PythonBridge:
    """
    Calls Python library functions directly using importlib.
    Works when the Ketamine compiler itself runs under CPython.
    """

    _module_cache: Dict[str, Any] = {}

    @classmethod
    def call(cls, path: List[str], args: List[Any]) -> Any:
        """
        path = ['requests', 'get']         → requests.get(*args)
        path = ['json', 'dumps']           → json.dumps(*args)
        path = ['os', 'environ', 'get']    → os.environ.get(*args)
        """
        if not path:
            raise ValueError("Empty FFI path for python::")

        module_name = path[0]
        attr_chain  = path[1:]

        # Import module (cached)
        if module_name not in cls._module_cache:
            try:
                cls._module_cache[module_name] = importlib.import_module(module_name)
            except ImportError as e:
                raise ImportError(
                    f"FFI: Python module '{module_name}' not found.\n"
                    f"  Install with: pip install {module_name}\n"
                    f"  Original error: {e}"
                )

        obj = cls._module_cache[module_name]

        # Walk attribute chain
        for i, attr in enumerate(attr_chain[:-1]):
            obj = getattr(obj, attr)

        # Last element is the function to call
        if attr_chain:
            func_name = attr_chain[-1]
            func = getattr(obj, func_name)
            return func(*args)
        else:
            # Called the module itself (unusual but valid)
            return obj(*args)

    @classmethod
    def import_module(cls, module_name: str) -> Any:
        """Pre-import a module. Called when the compiler processes import stmts."""
        if module_name not in cls._module_cache:
            try:
                cls._module_cache[module_name] = importlib.import_module(module_name)
            except ImportError:
                pass  # deferred — will fail at call time with a nice message
        return cls._module_cache.get(module_name)


# ─── C Bridge ─────────────────────────────────────────────────────────────────

# Maps logical library names to platform-specific shared library names
C_LIB_NAMES: Dict[str, Dict[str, str]] = {
    "sodium": {
        "Linux":  "libsodium.so",
        "Darwin": "libsodium.dylib",
        "Windows": "libsodium.dll",
    },
    "openssl": {
        "Linux":  "libssl.so",
        "Darwin": "libssl.dylib",
        "Windows": "libssl-1_1.dll",
    },
    "crypto": {          # OpenSSL crypto
        "Linux":  "libcrypto.so",
        "Darwin": "libcrypto.dylib",
        "Windows": "libcrypto-1_1.dll",
    },
    "sqlite3": {
        "Linux":  "libsqlite3.so",
        "Darwin": "libsqlite3.dylib",
        "Windows": "sqlite3.dll",
    },
    "z": {               # zlib
        "Linux":  "libz.so",
        "Darwin": "libz.dylib",
        "Windows": "zlib1.dll",
    },
}

class CForeignBridge:
    """
    Calls C shared library functions via ctypes.
    Argument types default to c_int64 unless a signature override is registered.
    """

    _lib_cache: Dict[str, ctypes.CDLL] = {}

    # Function signature overrides: (lib_name, func_name) → (argtypes, restype)
    SIGNATURES: Dict[tuple, tuple] = {
        # libsodium
        ("sodium", "randombytes_buf"):     ([ctypes.c_void_p, ctypes.c_size_t], None),
        ("sodium", "crypto_secretbox_easy"): (
            [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_ulonglong,
             ctypes.c_void_p, ctypes.c_void_p], ctypes.c_int),
        ("sodium", "crypto_pwhash_str"):   (
            [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_ulonglong,
             ctypes.c_ulonglong, ctypes.c_size_t], ctypes.c_int),
        ("sodium", "sodium_init"):         ([], ctypes.c_int),
    }

    @classmethod
    def load(cls, lib_name: str) -> ctypes.CDLL:
        if lib_name in cls._lib_cache:
            return cls._lib_cache[lib_name]

        system = platform.system()

        # Check registry
        if lib_name in C_LIB_NAMES:
            names = C_LIB_NAMES[lib_name]
            lib_file = names.get(system) or names.get("Linux")
        else:
            # Generic attempt: libNAME.so
            lib_file = ctypes.util.find_library(lib_name)
            if not lib_file:
                lib_file = f"lib{lib_name}.so"

        try:
            lib = ctypes.CDLL(lib_file)
            cls._lib_cache[lib_name] = lib
            return lib
        except OSError as e:
            raise OSError(
                f"FFI: C library '{lib_name}' not found ({lib_file}).\n"
                f"  Linux:   sudo apt install lib{lib_name}-dev\n"
                f"  macOS:   brew install {lib_name}\n"
                f"  Windows: install {lib_file} and add to PATH\n"
                f"  Error: {e}"
            )

    @classmethod
    def call(cls, path: List[str], args: List[Any]) -> Any:
        """
        path = ['sodium', 'randombytes_buf']
        path = ['sodium', 'crypto_aead_aes256gcm_encrypt']
        """
        if len(path) < 2:
            raise ValueError(f"C FFI path too short: {path}")

        lib_name  = path[0]
        func_name = "_".join(path[1:])   # sodium::crypto::aead → crypto_aead

        lib  = cls.load(lib_name)
        func = getattr(lib, func_name)

        # Apply signature if known
        sig_key = (lib_name, func_name)
        if sig_key in cls.SIGNATURES:
            argtypes, restype = cls.SIGNATURES[sig_key]
            func.argtypes = argtypes
            func.restype  = restype

        # Convert Python args to ctypes
        cargs = [cls._convert_arg(a) for a in args]
        result = func(*cargs)
        return cls._convert_result(result)

    @staticmethod
    def _convert_arg(arg: Any) -> Any:
        if isinstance(arg, str):
            return arg.encode("utf-8")
        if isinstance(arg, bytes):
            return arg
        if isinstance(arg, int):
            return ctypes.c_int64(arg)
        if isinstance(arg, float):
            return ctypes.c_double(arg)
        if arg is None:
            return ctypes.c_void_p(None)
        return arg

    @staticmethod
    def _convert_result(result: Any) -> Any:
        if isinstance(result, bytes):
            return result.decode("utf-8", errors="replace")
        return result


# ─── Rust Bridge ──────────────────────────────────────────────────────────────

class RustBridge:
    """
    Calls Rust shared libraries compiled with `crate-type = ["cdylib"]`.
    Rust functions must be  #[no_mangle] extern "C".
    """

    _lib_cache: Dict[str, ctypes.CDLL] = {}

    @classmethod
    def load(cls, crate_name: str) -> ctypes.CDLL:
        if crate_name in cls._lib_cache:
            return cls._lib_cache[crate_name]

        system   = platform.system()
        suffixes = {"Linux": ".so", "Darwin": ".dylib", "Windows": ".dll"}
        suffix   = suffixes.get(system, ".so")
        lib_file = f"lib{crate_name}{suffix}"

        # Search common paths
        import os
        search_paths = [
            ".",
            "./target/release",
            "./target/debug",
            os.path.expanduser("~/.cargo/lib"),
        ]

        for base in search_paths:
            full = os.path.join(base, lib_file)
            if os.path.exists(full):
                lib = ctypes.CDLL(full)
                cls._lib_cache[crate_name] = lib
                return lib

        raise OSError(
            f"FFI: Rust crate '{crate_name}' library not found ({lib_file}).\n"
            f"  Build with: cargo build --release\n"
            f"  Ensure crate-type = [\"cdylib\"] in Cargo.toml"
        )

    @classmethod
    def call(cls, path: List[str], args: List[Any]) -> Any:
        crate_name = path[0]
        func_name  = "_".join(path[1:])

        lib  = cls.load(crate_name)
        func = getattr(lib, func_name)

        cargs = [CForeignBridge._convert_arg(a) for a in args]
        result = func(*cargs)
        return CForeignBridge._convert_result(result)


# ─── JS Codegen Bridge (compile-time only) ───────────────────────────────────

def gen_ffi_js(lang: str, path: List[str], args_js: List[str]) -> str:
    """
    Called by codegen_js.py to emit JavaScript for an FFI call.
    Returns a JS expression string.

    lang     : 'python' | 'c' | 'rust' | 'js'
    path     : e.g. ['requests', 'get']
    args_js  : already-serialized JS argument expressions
    """

    if lang == "js":
        # Direct call — strip the 'js::' prefix, call native API
        dotted = ".".join(path)
        return f"{dotted}({', '.join(args_js)})"

    elif lang == "python":
        # Emit a Node.js child_process call to invoke Python
        module   = path[0]
        func_path = ".".join(path[1:])
        args_str  = ", ".join(args_js)
        return (
            f"JSON.parse(require('child_process').execSync("
            f"'python3 -c \"import {module}, json; "
            f"print(json.dumps({module}.{func_path}(' + "
            f"JSON.stringify([{args_str}]) + '))\"', "
            f"{{encoding:'utf8'}}))"
        )

    elif lang in ("c", "rust"):
        # Emit a native addon call (requires the ffi_native.node addon)
        lib_name  = path[0]
        func_name = "_".join(path[1:])
        args_str  = ", ".join(args_js)
        return (
            f"(require('./ffi_native').call("
            f"'{lib_name}', '{func_name}', [{args_str}]))"
        )

    else:
        raise ValueError(f"Unknown FFI lang for JS codegen: '{lang}'")


# ─── LLVM IR Bridge (compile-time only) ──────────────────────────────────────

def gen_ffi_llvm(lang: str, path: List[str], args_llvm: List[str]) -> str:
    """
    Called by codegen_llvm.py to emit LLVM IR for an FFI call.
    Returns an LLVM instruction string.
    """
    if lang == "python":
        # Can't call Python directly from LLVM IR; use libpython3 embedding
        func_name = "_".join(path)
        args_str  = ", ".join(f"i8* {a}" for a in args_llvm)
        return f"call i8* @ket_python_{func_name}({args_str})"

    elif lang in ("c", "rust"):
        # Direct foreign function call — must declare extern in module header
        func_name = "_".join(path[1:])  # strip lib name
        args_str  = ", ".join(f"i64 {a}" for a in args_llvm)
        return f"call i64 @{func_name}({args_str})"

    else:
        return "; FFI not supported in LLVM target"


# ─── Import Resolution ────────────────────────────────────────────────────────

class ImportResolver:
    """
    Processes ImportDecl nodes and prepares runtime state.
    Called by the compiler during the semantic/codegen phase.
    """

    def __init__(self):
        self.python_imports: Dict[str, str] = {}  # alias → module
        self.c_imports:      Dict[str, str] = {}
        self.rust_imports:   Dict[str, str] = {}

    def resolve(self, import_decl) -> Optional[str]:
        """
        import_decl: ImportDecl AST node
        Returns a warning string if the import can't be verified, None if OK.
        """
        lang   = import_decl.lang
        module = import_decl.module
        alias  = import_decl.alias or module

        if lang == "ketamine":
            # Standard library — always available
            return None

        elif lang == "python":
            self.python_imports[alias] = module
            # Try to pre-load
            mod = PythonBridge.import_module(module)
            if mod is None:
                return f"Warning: Python module '{module}' not available at compile time"
            return None

        elif lang == "c":
            self.c_imports[alias] = module
            try:
                CForeignBridge.load(module)
            except OSError as e:
                return f"Warning: {e}"
            return None

        elif lang == "rust":
            self.rust_imports[alias] = module
            return None  # Rust libs validated lazily

        elif lang == "js":
            # JS imports are a no-op — handled at codegen
            return None

        elif lang == "go":
            if not GoForeignBridge.check_go_installed():
                return f"Warning: 'go' not found in PATH — go:: FFI will fail at runtime"
            return None

        return f"Warning: Unknown FFI language '{lang}'"

    def emit_js_preamble(self) -> str:
        """
        Generate JS require() / import statements for all registered imports.
        Inserted at the top of the generated JS file.
        """
        lines = []

        for alias, module in self.python_imports.items():
            safe_alias = alias.replace("-", "_")
            lines.append(f"// FFI: python::{module} available via child_process")

        for alias, module in self.c_imports.items():
            lines.append(f"// FFI: c::{module} — load via ffi_native addon")

        return "\n".join(lines)

    def emit_llvm_declarations(self) -> str:
        """
        Generate LLVM extern declarations for C/Rust functions.
        Must be emitted before any function definitions.
        """
        lines = ["", "; FFI extern declarations"]

        for alias, module in self.c_imports.items():
            # Generic declaration; specific functions declared as needed
            lines.append(f"; c::{module} — link with -l{module}")

        for alias, module in self.rust_imports.items():
            lines.append(f"; rust::{module} — link with lib{module}.so")

        return "\n".join(lines)


# ─── Go Bridge ────────────────────────────────────────────────────────────────

class GoForeignBridge:
    """
    Runs Go snippets via subprocess for runtime FFI calls.
    For compiled targets (--target go) the GoCodeGen handles this at codegen time.
    """

    PKG_MAP: Dict[str, str] = {
        "net":            "net",
        "net::http":      "net/http",
        "fmt":            "fmt",
        "sync":           "sync",
        "time":           "time",
        "os":             "os",
        "bufio":          "bufio",
        "log":            "log",
        "math":           "math",
        "strings":        "strings",
        "strconv":        "strconv",
        "encoding::json": "encoding/json",
        "crypto::tls":    "crypto/tls",
        "crypto::rand":   "crypto/rand",
        "context":        "context",
        "errors":         "errors",
    }

    def __init__(self):
        self._tmpdir: Optional[str] = None
        self._initialized = False

    def _init(self):
        if self._initialized:
            return
        import tempfile, os
        self._tmpdir = tempfile.mkdtemp(prefix="ket_go_")
        with open(os.path.join(self._tmpdir, "go.mod"), "w") as f:
            f.write("module ketamine_go_bridge\n\ngo 1.21\n")
        self._initialized = True

    def call(self, path: List[str], args: List[Any]) -> Any:
        import subprocess, json as _json, os

        self._init()

        if len(path) < 2:
            raise ValueError(f"Go FFI path too short: {path}")

        pkg_key   = "::".join(path[:-1])
        go_import = self.PKG_MAP.get(pkg_key, "/".join(path[:-1]))
        pkg_alias = go_import.split("/")[-1]
        func_name = path[-1]
        go_args   = self._serialize_args(args)

        go_src = (
            'package main\n\n'
            'import (\n'
            '\t"encoding/json"\n'
            '\t"fmt"\n'
            f'\t"{go_import}"\n'
            ')\n\n'
            'func main() {\n'
            f'\tresult := {pkg_alias}.{func_name}({go_args})\n'
            '\tout, _ := json.Marshal(result)\n'
            '\tfmt.Println(string(out))\n'
            '}\n'
        )

        src_path = os.path.join(self._tmpdir, "bridge_main.go")
        with open(src_path, "w") as f:
            f.write(go_src)

        proc = subprocess.run(
            ["go", "run", src_path],
            capture_output=True, text=True, cwd=self._tmpdir
        )
        if proc.returncode != 0:
            raise RuntimeError(
                f"Go FFI error calling {pkg_alias}.{func_name}:\n{proc.stderr}"
            )

        raw = proc.stdout.strip()
        try:
            return _json.loads(raw)
        except Exception:
            return raw

    @staticmethod
    def _serialize_args(args: List[Any]) -> str:
        parts = []
        for a in args:
            if isinstance(a, str):
                e = a.replace("\\", "\\\\").replace('"', '\\"')
                parts.append(f'"{e}"')
            elif isinstance(a, bool):
                parts.append("true" if a else "false")
            elif isinstance(a, (int, float)):
                parts.append(str(a))
            elif a is None:
                parts.append("nil")
            else:
                parts.append(f'"{a}"')
        return ", ".join(parts)

    @staticmethod
    def check_go_installed() -> bool:
        import subprocess
        try:
            r = subprocess.run(["go", "version"], capture_output=True, timeout=5)
            return r.returncode == 0
        except Exception:
            return False
