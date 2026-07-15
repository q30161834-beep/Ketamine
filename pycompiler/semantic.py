"""Semantic analyzer - type checking, scope resolution"""
from typing import Dict, List, Optional
from parser import *

class Scope:
    def __init__(self, parent: Optional['Scope'] = None):
        self.parent = parent
        self.symbols: Dict[str, str] = {}  # name -> type
        self.functions: Dict[str, FnDecl] = {}
    
    def define(self, name: str, typ: str):
        if name in self.symbols:
            raise ValueError(f"Redefinition of '{name}'")
        self.symbols[name] = typ
    
    def lookup(self, name: str) -> Optional[str]:
        if name in self.symbols:
            return self.symbols[name]
        if self.parent:
            return self.parent.lookup(name)
        return None
    
    def define_fn(self, name: str, fn: FnDecl):
        self.functions[name] = fn
    
    def lookup_fn(self, name: str) -> Optional[FnDecl]:
        if name in self.functions:
            return self.functions[name]
        if self.parent:
            return self.parent.lookup_fn(name)
        return None

class SemanticAnalyzer:
    def __init__(self):
        self.global_scope = Scope()
        self.current_scope = self.global_scope
        self.errors: List[str] = []
        self.current_fn_ret_type: Optional[str] = None
        
        # Built-in functions
        self._define_builtins()
    
    def _define_builtins(self):
        builtins = ['print', 'println', 'str', 'int', 'float', 'sqrt', 'panic', 'exit']
        for b in builtins:
            self.global_scope.define(b, 'builtin')
    
    def error(self, node: ASTNode, msg: str):
        self.errors.append(f"Line {node.line}: {msg}")
    
    def enter_scope(self):
        self.current_scope = Scope(parent=self.current_scope)
    
    def exit_scope(self):
        if self.current_scope.parent:
            self.current_scope = self.current_scope.parent
    
    def analyze(self, program: Program):
        # First pass: collect function declarations
        for decl in program.declarations:
            if isinstance(decl, FnDecl):
                self.global_scope.define_fn(decl.name, decl)
            elif isinstance(decl, StructDecl):
                self.global_scope.define(decl.name, 'struct')
        
        # Second pass: analyze function bodies
        for decl in program.declarations:
            self.visit(decl)
    
    def visit(self, node: ASTNode) -> Optional[str]:
        method = f'visit_{node.__class__.__name__}'
        visitor = getattr(self, method, self.generic_visit)
        return visitor(node)
    
    def generic_visit(self, node: ASTNode):
        pass
    
    def visit_FnDecl(self, node: FnDecl) -> str:
        self.enter_scope()
        self.current_fn_ret_type = node.ret_type
        
        # Define parameters
        for param in node.params:
            self.current_scope.define(param.name, param.var_type or 'unknown')
        
        # Check body
        if node.body:
            self.visit(node.body)
        
        self.current_fn_ret_type = None
        self.exit_scope()
        return node.ret_type or 'void'
    
    def visit_Block(self, node: Block):
        for stmt in node.stmts:
            self.visit(stmt)
    
    def visit_VarDecl(self, node: VarDecl) -> str:
        # Check initialization
        if node.init:
            init_type = self.visit(node.init)
            if node.var_type and init_type and node.var_type != init_type:
                if not self._compatible(node.var_type, init_type):
                    self.error(node, f"Type mismatch: expected {node.var_type}, got {init_type}")
        
        var_type = node.var_type or (self.visit(node.init) if node.init else 'unknown')
        
        try:
            self.current_scope.define(node.name, var_type)
        except ValueError as e:
            self.error(node, str(e))
        
        return var_type
    
    def visit_ReturnStmt(self, node: ReturnStmt):
        if node.value:
            ret_type = self.visit(node.value)
            if self.current_fn_ret_type and ret_type != self.current_fn_ret_type:
                if not self._compatible(self.current_fn_ret_type, ret_type):
                    self.error(node, f"Return type mismatch: expected {self.current_fn_ret_type}, got {ret_type}")
    
    def visit_IfStmt(self, node: IfStmt):
        cond_type = self.visit(node.cond)
        if cond_type and cond_type != 'bool':
            self.error(node, f"Condition must be bool, got {cond_type}")
        
        if node.then_branch:
            self.visit(node.then_branch)
        if node.else_branch:
            self.visit(node.else_branch)
    
    def visit_WhileStmt(self, node: WhileStmt):
        cond_type = self.visit(node.cond)
        if cond_type and cond_type != 'bool':
            self.error(node, f"Condition must be bool, got {cond_type}")
        if node.body:
            self.visit(node.body)
    
    def visit_ForStmt(self, node: ForStmt):
        self.enter_scope()
        iter_type = self.visit(node.iter)
        # Simplified: assume iterable yields elements
        self.current_scope.define(node.var, 'int')  # TODO: proper type inference
        if node.body:
            self.visit(node.body)
        self.exit_scope()
    
    def visit_ExprStmt(self, node: ExprStmt):
        if node.expr:
            self.visit(node.expr)
    
    def visit_BinaryExpr(self, node: BinaryExpr) -> str:
        left_type = self.visit(node.left)
        right_type = self.visit(node.right)
        
        # Comparison operators
        if node.op in ['==', '!=', '<', '>', '<=', '>=']:
            if left_type != right_type:
                if not self._compatible(left_type, right_type):
                    self.error(node, f"Type mismatch: {left_type} vs {right_type}")
            return 'bool'
        
        # Logical operators
        if node.op in ['&&', '||']:
            if left_type != 'bool' or right_type != 'bool':
                self.error(node, "Logical operators require bool operands")
            return 'bool'
        
        # Arithmetic operators
        if node.op in ['+', '-', '*', '/', '%']:
            if left_type not in ['int', 'float'] or right_type not in ['int', 'float']:
                if node.op != '+' or (left_type != 'str' and right_type != 'str'):
                    self.error(node, f"Arithmetic requires numeric types, got {left_type} and {right_type}")
            if left_type == 'float' or right_type == 'float':
                return 'float'
            if left_type == 'str' or right_type == 'str':
                return 'str'
            return 'int'
        
        return 'unknown'
    
    def visit_UnaryExpr(self, node: UnaryExpr) -> str:
        operand_type = self.visit(node.operand)
        if node.op == '!':
            if operand_type != 'bool':
                self.error(node, f"! requires bool operand, got {operand_type}")
            return 'bool'
        if node.op == '-':
            if operand_type not in ['int', 'float']:
                self.error(node, f"- requires numeric operand, got {operand_type}")
            return operand_type
        return 'unknown'
    
    def visit_CallExpr(self, node: CallExpr) -> str:
        if isinstance(node.callee, Ident):
            fn = self.current_scope.lookup_fn(node.callee.name)
            if not fn:
                # Check if builtin
                if self.current_scope.lookup(node.callee.name) == 'builtin':
                    return 'void'  # simplified
                self.error(node, f"Undefined function '{node.callee.name}'")
                return 'unknown'
            
            # Check argument count
            if len(node.args) != len(fn.params):
                self.error(node, f"Expected {len(fn.params)} arguments, got {len(node.args)}")
            
            # Check argument types
            for i, (arg, param) in enumerate(zip(node.args, fn.params)):
                arg_type = self.visit(arg)
                if arg_type and param.var_type and arg_type != param.var_type:
                    if not self._compatible(param.var_type, arg_type):
                        self.error(node, f"Argument {i+1} type mismatch: expected {param.var_type}, got {arg_type}")
            
            return fn.ret_type or 'void'
        
        return 'unknown'
    
    def visit_AssignExpr(self, node: AssignExpr) -> str:
        target_type = self.visit(node.target)
        value_type = self.visit(node.value)
        
        if target_type and value_type and target_type != value_type:
            if not self._compatible(target_type, value_type):
                self.error(node, f"Assignment type mismatch: {target_type} = {value_type}")
        
        return value_type
    
    def visit_Ident(self, node: Ident) -> str:
        typ = self.current_scope.lookup(node.name)
        if not typ:
            self.error(node, f"Undefined variable '{node.name}'")
            return 'unknown'
        return typ
    
    def visit_IntLiteral(self, node: IntLiteral) -> str:
        return 'int'
    
    def visit_FloatLiteral(self, node: FloatLiteral) -> str:
        return 'float'
    
    def visit_StringLiteral(self, node: StringLiteral) -> str:
        return 'str'
    
    def visit_BoolLiteral(self, node: BoolLiteral) -> str:
        return 'bool'
    
    def visit_MemberExpr(self, node: MemberExpr) -> str:
        # Simplified: no struct type checking yet
        return 'unknown'
    
    def visit_IndexExpr(self, node: IndexExpr) -> str:
        # Simplified: assume array access
        return 'int'
    
    def _compatible(self, expected: str, actual: str) -> bool:
        # Allow int -> float promotion
        if expected == 'float' and actual == 'int':
            return True
        # Allow unknown types (for now)
        if expected == 'unknown' or actual == 'unknown':
            return True
        return False
