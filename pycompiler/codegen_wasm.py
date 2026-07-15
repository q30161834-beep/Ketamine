"""WebAssembly code generator (WAT format)"""
from parser import *

class WASMCodeGen:
    def __init__(self, optimize: int = 1):
        self.optimize = optimize
        self.output = []
        self.locals_counter = 0
    
    def generate(self, program: Program) -> str:
        self.output = [
            "(module",
            "  ;; Ketamine -> WASM",
            "  (import \"env\" \"print_i32\" (func $print_i32 (param i32)))",
            "  (import \"env\" \"print_f64\" (func $print_f64 (param f64)))",
            "  (memory (export \"memory\") 1)",
            ""
        ]
        
        for decl in program.declarations:
            if isinstance(decl, FnDecl):
                self._gen_function(decl)
        
        # Export main
        self.output.append('  (export "main" (func $main))')
        self.output.append(")")
        
        return "\n".join(self.output)
    
    def _gen_function(self, node: FnDecl):
        params = " ".join(f"(param ${p.name} i64)" for p in node.params)
        ret = "(result i64)" if node.ret_type and node.ret_type != "void" else ""
        
        self.output.append(f"  (func ${node.name} {params} {ret}")
        
        # Locals (simplified)
        self.output.append("    (local $tmp i64)")
        
        if node.body:
            for stmt in node.body.stmts:
                self._gen_stmt(stmt, indent=2)
        
        self.output.append("  )")
        self.output.append("")
    
    def _gen_stmt(self, node: ASTNode, indent: int):
        prefix = "  " * indent
        
        if isinstance(node, ReturnStmt):
            if node.value:
                expr = self._gen_expr(node.value)
                self.output.append(f"{prefix}{expr}")
            self.output.append(f"{prefix}return")
        
        elif isinstance(node, ExprStmt):
            expr = self._gen_expr(node.expr)
            self.output.append(f"{prefix}{expr}")
            self.output.append(f"{prefix}drop")  # discard result
        
        elif isinstance(node, VarDecl):
            # Simplified: store to local
            if node.init:
                expr = self._gen_expr(node.init)
                self.output.append(f"{prefix}{expr}")
                self.output.append(f"{prefix}local.set ${node.name}")
    
    def _gen_expr(self, node: ASTNode) -> str:
        if isinstance(node, IntLiteral):
            return f"i64.const {node.value}"
        
        elif isinstance(node, BinaryExpr):
            left = self._gen_expr(node.left)
            right = self._gen_expr(node.right)
            
            op_map = {
                '+': 'i64.add', '-': 'i64.sub',
                '*': 'i64.mul', '/': 'i64.div_s',
                '%': 'i64.rem_s',
                '==': 'i64.eq', '!=': 'i64.ne',
                '<': 'i64.lt_s', '>': 'i64.gt_s',
                '<=': 'i64.le_s', '>=': 'i64.ge_s',
            }
            
            wasm_op = op_map.get(node.op, 'i64.add')
            return f"{left}\n    {right}\n    {wasm_op}"
        
        elif isinstance(node, Ident):
            return f"local.get ${node.name}"
        
        elif isinstance(node, CallExpr):
            if isinstance(node.callee, Ident):
                name = node.callee.name
                args = "\n    ".join(self._gen_expr(arg) for arg in node.args)
                return f"{args}\n    call ${name}"
        
        return "i64.const 0"

    def to_binary(self, wat: str) -> bytes:
        """Convert WAT to WASM binary (requires wat2wasm from WABT)"""
        import subprocess
        import tempfile
        
        try:
            with tempfile.NamedTemporaryFile(mode='w', suffix='.wat', delete=False) as f:
                f.write(wat)
                wat_file = f.name
            
            wasm_file = wat_file.replace('.wat', '.wasm')
            subprocess.run(['wat2wasm', wat_file, '-o', wasm_file], check=True)
            
            with open(wasm_file, 'rb') as f:
                binary = f.read()
            
            import os
            os.unlink(wat_file)
            os.unlink(wasm_file)
            
            return binary
        except Exception as e:
            print(f"Warning: wat2wasm not available ({e})")
            return b""
