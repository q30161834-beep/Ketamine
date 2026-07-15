#ifndef KETAMINE_CODEGEN_H
#define KETAMINE_CODEGEN_H

#include "types.h"

// ═══════════════════════════════════════════════════════════════════════════════
// CODEGEN — Backend code generation
// ═══════════════════════════════════════════════════════════════════════════════

// ─── LLVM IR Codegen ──────────────────────────────────────────────────────────
// Emits proper LLVM IR text for the entire IR module.
// Handles: all types, SSA, PHI, control flow, aggregates, intrinsics.

int codegen_llvm_module(IRModule *mod, const char *path, CompileOptions *opts);

// ─── Go Codegen ───────────────────────────────────────────────────────────────
// Translates Ketamine IR to Go source code.
// Handles: goroutines, channels, Go FFI imports, native Go types.

int codegen_go_module(IRModule *mod, const char *path, CompileOptions *opts);

// ─── Python Codegen ───────────────────────────────────────────────────────────
// Translates Ketamine IR to Python 3 source code.
// Handles: dynamic typing, Python FFI (import python::module), native Python types.

int codegen_python_module(IRModule *mod, const char *path, CompileOptions *opts);

// ─── JavaScript Codegen ───────────────────────────────────────────────────────
// Translates Ketamine IR to JavaScript/Node.js source code.

int codegen_js_module(IRModule *mod, const char *path, CompileOptions *opts);

// ─── WASM Codegen ─────────────────────────────────────────────────────────────
// Experimental: WebAssembly text format output.

int codegen_wasm_module(IRModule *mod, const char *path, CompileOptions *opts);

// ─── Native Codegen (x86-64 / AArch64) ───────────────────────────────────────
// Direct machine code generation (no LLVM dependency).
// Uses a simple two-pass approach: instruction selection + register allocation.

int codegen_native_module(IRModule *mod, const char *path, CompileOptions *opts);

// ─── Helper: Emit type string ─────────────────────────────────────────────────
const char *codegen_type_str(struct Type *type);
const char *codegen_llvm_type_str(struct Type *type);
const char *codegen_go_type_str(struct Type *type);
const char *codegen_python_type_str(struct Type *type);

#endif // KETAMINE_CODEGEN_H
