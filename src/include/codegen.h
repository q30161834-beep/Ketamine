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

// ─── Native Codegen (x86-64) — raw machine code ──────────────────────────────
// Direct machine code generation (no LLVM dependency).
// Uses a simple two-pass approach: instruction selection + register allocation.
// Output: flat binary (.bin) with raw x86-64 instructions.

int codegen_native_module(IRModule *mod, const char *path, CompileOptions *opts);

// ─── Assembly Codegen (x86-64) — Intel-syntax text ───────────────────────────
// Output: human-readable .asm file (NASM-compatible Intel syntax).
// Can be assembled with: nasm -f win64 file.asm (Windows)
//                      nasm -f elf64 file.asm (Linux)

int codegen_asm_module(IRModule *mod, const char *path, CompileOptions *opts);

// ─── JIT Compilation — Compile & Execute at runtime ──────────────────────────
// Compiles an IRFunction to x86-64 machine code in executable memory.
// Returns a function pointer that can be called directly.
// Handles both Windows x64 and System V AMD64 ABIs.

typedef void* (*JITFunc)(void** args);

// Compile an entire IRModule into executable memory.
// Returns 0 on success. The JIT'd code is callable via jit_get_function().
int jit_compile_module(IRModule *mod, CompileOptions *opts);

// Get a function pointer to a JIT-compiled function by name.
JITFunc jit_get_function(const char *name);

// Free all JIT-compiled code.
void jit_free(void);

// ─── Forth Codegen ────────────────────────────────────────────────────────────
// Translates Ketamine IR to Forth source code.
// Output: .fs file with ANS Forth-compatible postfix notation.
// Handles: stack-based operations, IF/ELSE/THEN, word definitions, print.

int codegen_forth_module(IRModule *mod, const char *path, CompileOptions *opts);

// ─── Helper: Emit type string ─────────────────────────────────────────────────
const char *codegen_type_str(struct Type *type);
const char *codegen_llvm_type_str(struct Type *type);
const char *codegen_go_type_str(struct Type *type);
const char *codegen_python_type_str(struct Type *type);

#endif // KETAMINE_CODEGEN_H
