"""LLVM IR code generator (Python version)"""
from parser import *

class LLVMCodeGen:
    def __init__(self, optimize: int = 1):
        self.optimize = optimize
        self.output = []
        self.tmp_counter = 0
        self.str_counter = 0
    
    def next_tmp(self) -> str:
        self.tmp_counter += 1
        return f"%t{self.tmp_counter}"
    
    def generate(self, program: Program) -> str:
        self.output = [
            "; Ketamine -> LLVM IR",
            'target triple = "x86_64-pc-linux-gnu"',
            "",
            "declare i32 @puts(i8*)",
            "declare i32 @printf(i8*, ...)",
            '@.fmt_int = private constant [5 x i8] c"%ld\\0A\\00"',
            ""
        ]
        
        for decl in program.declarations:
            if isinstance(decl, FnDecl):
                self._gen_function(decl)
        
        return "\n".join(self.output)
    
    def _gen_function(self, node: FnDecl):
        params = ", ".join(f"i64 %{p.name}" for p in node.params)
        ret_type = "i64" if node.ret_type and node.ret_type != "void" else "void"
        
        self.output.append(f"define {ret_type} @{node.name}({params}) {{")
        self.output.append("entry:")
        
        self.tmp_counter = 0
        
        if node.body:
            for stmt in node.body.stmts:
                self._gen_stmt(stmt)
        
        # Ensure terminator
        if ret_type == "void":
            self.output.append("  ret void")
        
        self.output.append("}")
        self.output.append("")
    
    def _gen_stmt(self, node: ASTNode):
        if isinstance(node, ReturnStmt):
            if node.value:
                val = self._gen_expr(node.value)
                self.output.append(f"  ret i64 {val}")
            else:
                self.output.append("  ret void")
        
        elif isinstance(node, VarDecl):
            self.output.append(f"  %{node.name} = alloca i64")
            if node.init:
                val = self._gen_expr(node.init)
                self.output.append(f"  store i64 {val}, i64* %{node.name}")
        
        elif isinstance(node, ExprStmt):
            self._gen_expr(node.expr)
        
        elif isinstance(node, IfStmt):
            cond = self._gen_expr(node.cond)
            then_label = self.tmp_counter + 1
            else_label = self.tmp_counter + 2
            merge_label = self.tmp_counter + 3
            self.tmp_counter += 3
            
            self.output.append(f"  br i1 {cond}, label %then{then_label}, label %else{else_label}")
            self.output.append(f"then{then_label}:")
            if node.then_branch:
                for stmt in node.then_branch.stmts:
                    self._gen_stmt(stmt)
            self.output.append(f"  br label %merge{merge_label}")
            
            self.output.append(f"else{else_label}:")
            if node.else_branch:
                for stmt in node.else_branch.stmts:
                    self._gen_stmt(stmt)
            self.output.append(f"  br label %merge{merge_label}")
            
            self.output.append(f"merge{merge_label}:")
    
    def _gen_expr(self, node: ASTNode) -> str:
        if isinstance(node, IntLiteral):
            tmp = self.next_tmp()
            self.output.append(f"  {tmp} = add i64 0, {node.value}")
            return tmp
        
        elif isinstance(node, BinaryExpr):
            left = self._gen_expr(node.left)
            right = self._gen_expr(node.right)
            tmp = self.next_tmp()
            
            op_map = {
                '+': 'add', '-': 'sub', '*': 'mul', '/': 'sdiv', '%': 'srem',
                '==': 'icmp eq', '!=': 'icmp ne',
                '<': 'icmp slt', '>': 'icmp sgt',
                '<=': 'icmp sle', '>=': 'icmp sge',
            }
            
            llvm_op = op_map.get(node.op, 'add')
            
            if 'icmp' in llvm_op:
                self.output.append(f"  {tmp} = {llvm_op} i64 {left}, {right}")
            else:
                self.output.append(f"  {tmp} = {llvm_op} i64 {left}, {right}")
            
            return tmp
        
        elif isinstance(node, Ident):
            tmp = self.next_tmp()
            self.output.append(f"  {tmp} = load i64, i64* %{node.name}")
            return tmp
        
        elif isinstance(node, CallExpr):
            if isinstance(node.callee, Ident):
                name = node.callee.name
                
                # Built-in print
                if name == "print":
                    if node.args:
                        arg = self._gen_expr(node.args[0])
                        self.output.append(f"  call i32 (i8*, ...) @printf(i8* @.fmt_int, i64 {arg})")
                    return "%0"
                
                # User function
                args_str = ", ".join(f"i64 {self._gen_expr(arg)}" for arg in node.args)
                tmp = self.next_tmp()
                self.output.append(f"  {tmp} = call i64 @{name}({args_str})")
                return tmp
        
        return "%0"
