"""Parser for Ketamine - builds AST with FFI support"""
from dataclasses import dataclass, field
from typing import List, Optional
from lexer import Token, TokenType

# ─── AST Nodes ────────────────────────────────────────────────────────────────

@dataclass
class ASTNode:
    line: int = 0

@dataclass
class Program(ASTNode):
    declarations: List[ASTNode] = field(default_factory=list)
    imports: List['ImportDecl'] = field(default_factory=list)

# ─── Import / FFI nodes ───────────────────────────────────────────────────────

@dataclass
class ImportDecl(ASTNode):
    """import core  /  import python::requests  /  import c::sodium"""
    lang: str = "ketamine"   # ketamine | python | c | rust | js
    module: str = ""         # requests, sodium, tokio, core …
    alias: Optional[str] = None  # as alias

@dataclass
class FFICallExpr(ASTNode):
    """python::requests::get(url)  /  c::sodium::randombytes_buf(12)"""
    lang: str = ""           # python | c | rust | js
    path: List[str] = field(default_factory=list)   # ['requests', 'get']
    args: List[ASTNode] = field(default_factory=list)

@dataclass
class NamespaceExpr(ASTNode):
    """requests::get  /  sodium::crypto_aead_aes256gcm_encrypt"""
    parts: List[str] = field(default_factory=list)

# ─── Declarations ─────────────────────────────────────────────────────────────

@dataclass
class FnDecl(ASTNode):
    name: str = ""
    params: List['VarDecl'] = field(default_factory=list)
    ret_type: Optional[str] = None
    body: Optional['Block'] = None
    is_pub: bool = False
    is_async: bool = False

@dataclass
class VarDecl(ASTNode):
    name: str = ""
    var_type: Optional[str] = None
    init: Optional[ASTNode] = None
    is_mut: bool = False

@dataclass
class StructDecl(ASTNode):
    name: str = ""
    fields: List[VarDecl] = field(default_factory=list)

@dataclass
class EnumDecl(ASTNode):
    name: str = ""
    variants: List[str] = field(default_factory=list)

# ─── Statements ───────────────────────────────────────────────────────────────

@dataclass
class Block(ASTNode):
    stmts: List[ASTNode] = field(default_factory=list)

@dataclass
class ReturnStmt(ASTNode):
    value: Optional[ASTNode] = None

@dataclass
class IfStmt(ASTNode):
    cond: Optional[ASTNode] = None
    then_branch: Optional[Block] = None
    else_branch: Optional[Block] = None

@dataclass
class WhileStmt(ASTNode):
    cond: Optional[ASTNode] = None
    body: Optional[Block] = None

@dataclass
class ForStmt(ASTNode):
    var: str = ""
    iter: Optional[ASTNode] = None
    body: Optional[Block] = None

@dataclass
class ExprStmt(ASTNode):
    expr: Optional[ASTNode] = None

# ─── Expressions ─────────────────────────────────────────────────────────────

@dataclass
class BinaryExpr(ASTNode):
    op: str = ""
    left: Optional[ASTNode] = None
    right: Optional[ASTNode] = None

@dataclass
class UnaryExpr(ASTNode):
    op: str = ""
    operand: Optional[ASTNode] = None

@dataclass
class CallExpr(ASTNode):
    callee: Optional[ASTNode] = None
    args: List[ASTNode] = field(default_factory=list)

@dataclass
class MemberExpr(ASTNode):
    obj: Optional[ASTNode] = None
    member: str = ""

@dataclass
class IndexExpr(ASTNode):
    obj: Optional[ASTNode] = None
    index: Optional[ASTNode] = None

@dataclass
class AssignExpr(ASTNode):
    target: Optional[ASTNode] = None
    value: Optional[ASTNode] = None

@dataclass
class Ident(ASTNode):
    name: str = ""

@dataclass
class IntLiteral(ASTNode):
    value: int = 0

@dataclass
class FloatLiteral(ASTNode):
    value: float = 0.0

@dataclass
class StringLiteral(ASTNode):
    value: str = ""

@dataclass
class BoolLiteral(ASTNode):
    value: bool = False

@dataclass
class NullLiteral(ASTNode):
    pass

@dataclass
class ArrayLiteral(ASTNode):
    elements: List[ASTNode] = field(default_factory=list)

# ─── Parser ───────────────────────────────────────────────────────────────────

# FFI langs recognized as namespace prefixes
FFI_LANGS = {'python', 'c', 'rust', 'js', 'go'}

class Parser:
    def __init__(self, tokens: List[Token]):
        self.tokens = tokens
        self.pos = 0
        self.errors = []

    # ─── Helpers ──────────────────────────────────────────────────────────────

    def error(self, msg: str):
        tok = self.current()
        self.errors.append(f"Line {tok.line}:{tok.col}: {msg}")

    def current(self) -> Token:
        return self.tokens[self.pos] if self.pos < len(self.tokens) else self.tokens[-1]

    def peek(self, offset: int = 1) -> Token:
        pos = self.pos + offset
        return self.tokens[pos] if pos < len(self.tokens) else self.tokens[-1]

    def advance(self):
        if self.pos < len(self.tokens) - 1:
            self.pos += 1

    def check(self, *types: TokenType) -> bool:
        return self.current().type in types

    def match(self, *types: TokenType) -> bool:
        if self.check(*types):
            self.advance()
            return True
        return False

    def expect(self, typ: TokenType, msg: str):
        if not self.match(typ):
            self.error(f"Expected {msg}, got '{self.current().value}' ({self.current().type.name})")

    # ─── Program ──────────────────────────────────────────────────────────────

    def parse(self) -> Program:
        prog = Program(line=1)

        while not self.check(TokenType.EOF):
            if self.check(TokenType.IMPORT):
                imp = self.parse_import()
                prog.imports.append(imp)
                prog.declarations.append(imp)
                continue

            is_pub = self.match(TokenType.PUB)

            if self.check(TokenType.FN):
                prog.declarations.append(self.parse_fn(is_pub))
            elif self.check(TokenType.STRUCT):
                prog.declarations.append(self.parse_struct())
            elif self.check(TokenType.ENUM):
                prog.declarations.append(self.parse_enum())
            else:
                self.error(f"Unexpected token '{self.current().value}'")
                self.advance()

        return prog

    # ─── Import / FFI ─────────────────────────────────────────────────────────

    def parse_import(self) -> ImportDecl:
        """
        import core
        import python::requests
        import c::sodium
        import rust::tokio
        import python::requests as req
        """
        line = self.current().line
        self.advance()  # consume 'import'

        first = self.current().value
        self.expect(TokenType.IDENT, "module name")

        lang = "ketamine"
        module = first
        alias = None

        # Check for  lang::module
        if self.check(TokenType.COLONCOLON):
            self.advance()
            if first in FFI_LANGS:
                lang = first
                module = self.current().value
                self.expect(TokenType.IDENT, "module name")
            else:
                # namespace chain like  std::io
                parts = [first, self.current().value]
                self.expect(TokenType.IDENT, "module name")
                module = "::".join(parts)

        # Optional  as alias
        if self.check(TokenType.IDENT) and self.current().value == 'as':
            self.advance()
            alias = self.current().value
            self.expect(TokenType.IDENT, "alias name")

        return ImportDecl(lang=lang, module=module, alias=alias, line=line)

    # ─── Declarations ─────────────────────────────────────────────────────────

    def parse_fn(self, is_pub: bool) -> FnDecl:
        line = self.current().line
        self.advance()  # consume 'fn'

        name = self.current().value
        self.expect(TokenType.IDENT, "function name")
        self.expect(TokenType.LPAREN, "(")

        params = []
        while not self.check(TokenType.RPAREN, TokenType.EOF):
            if self.check(TokenType.SELF):
                self.advance()
                params.append(VarDecl(name="self", var_type="Self", line=line))
            else:
                pname = self.current().value
                pline = self.current().line
                self.expect(TokenType.IDENT, "parameter name")
                self.expect(TokenType.COLON, ":")
                ptype = self.parse_type()
                params.append(VarDecl(name=pname, var_type=ptype, line=pline))
            if not self.match(TokenType.COMMA):
                break

        self.expect(TokenType.RPAREN, ")")

        ret_type = "void"
        if self.match(TokenType.ARROW):
            ret_type = self.parse_type()

        body = self.parse_block()
        return FnDecl(name=name, params=params, ret_type=ret_type,
                      body=body, is_pub=is_pub, line=line)

    def parse_struct(self) -> StructDecl:
        line = self.current().line
        self.advance()
        name = self.current().value
        self.expect(TokenType.IDENT, "struct name")
        self.expect(TokenType.LBRACE, "{")

        fields = []
        while not self.check(TokenType.RBRACE, TokenType.EOF):
            fname = self.current().value
            fline = self.current().line
            self.expect(TokenType.IDENT, "field name")
            self.expect(TokenType.COLON, ":")
            ftype = self.parse_type()
            fields.append(VarDecl(name=fname, var_type=ftype, line=fline))
            self.match(TokenType.COMMA)

        self.expect(TokenType.RBRACE, "}")
        return StructDecl(name=name, fields=fields, line=line)

    def parse_enum(self) -> EnumDecl:
        line = self.current().line
        self.advance()
        name = self.current().value
        self.expect(TokenType.IDENT, "enum name")
        self.expect(TokenType.LBRACE, "{")

        variants = []
        while not self.check(TokenType.RBRACE, TokenType.EOF):
            variants.append(self.current().value)
            self.expect(TokenType.IDENT, "variant name")
            self.match(TokenType.COMMA)

        self.expect(TokenType.RBRACE, "}")
        return EnumDecl(name=name, variants=variants, line=line)

    # ─── Type ─────────────────────────────────────────────────────────────────

    def parse_type(self) -> str:
        if self.check(TokenType.TYPE_INT, TokenType.TYPE_FLOAT,
                      TokenType.TYPE_BOOL, TokenType.TYPE_STR, TokenType.TYPE_VOID):
            typ = self.current().value
            self.advance()
            return typ

        if self.check(TokenType.IDENT):
            name = self.current().value
            self.advance()
            # Namespaced type: rust::tokio::Task
            while self.check(TokenType.COLONCOLON):
                self.advance()
                name += "::" + self.current().value
                self.expect(TokenType.IDENT, "type name")
            # Generic: Result<T>
            if self.check(TokenType.LT):
                self.advance()
                inner = self.parse_type()
                self.expect(TokenType.GT, ">")
                return f"{name}<{inner}>"
            return name

        if self.check(TokenType.LBRACKET):
            self.advance()
            inner = self.parse_type()
            size = ""
            if self.match(TokenType.SEMICOLON):
                size = str(self.current().value)
                self.advance()
            self.expect(TokenType.RBRACKET, "]")
            return f"[{inner};{size}]" if size else f"[{inner}]"

        self.error("Expected type")
        return "unknown"

    # ─── Block & Statements ───────────────────────────────────────────────────

    def parse_block(self) -> Block:
        line = self.current().line
        self.expect(TokenType.LBRACE, "{")
        stmts = []
        while not self.check(TokenType.RBRACE, TokenType.EOF):
            stmts.append(self.parse_stmt())
        self.expect(TokenType.RBRACE, "}")
        return Block(stmts=stmts, line=line)

    def parse_stmt(self) -> ASTNode:
        self.match(TokenType.SEMICOLON)

        if self.check(TokenType.LET, TokenType.MUT):
            return self.parse_var_decl()
        if self.check(TokenType.RETURN):
            return self.parse_return()
        if self.check(TokenType.IF):
            return self.parse_if()
        if self.check(TokenType.WHILE):
            return self.parse_while()
        if self.check(TokenType.FOR):
            return self.parse_for()
        if self.check(TokenType.LBRACE):
            return self.parse_block()

        expr = self.parse_expr()
        self.match(TokenType.SEMICOLON)
        return ExprStmt(expr=expr, line=expr.line)

    def parse_var_decl(self) -> VarDecl:
        line = self.current().line
        is_mut = self.match(TokenType.MUT)
        self.expect(TokenType.LET, "let")
        if not is_mut:
            is_mut = self.match(TokenType.MUT)

        name = self.current().value
        self.expect(TokenType.IDENT, "variable name")

        var_type = None
        if self.match(TokenType.COLON):
            var_type = self.parse_type()

        init = None
        if self.match(TokenType.ASSIGN):
            init = self.parse_expr()

        return VarDecl(name=name, var_type=var_type, init=init,
                       is_mut=is_mut, line=line)

    def parse_return(self) -> ReturnStmt:
        line = self.current().line
        self.advance()
        val = None
        if not self.check(TokenType.SEMICOLON, TokenType.RBRACE):
            val = self.parse_expr()
        return ReturnStmt(value=val, line=line)

    def parse_if(self) -> IfStmt:
        line = self.current().line
        self.advance()
        cond = self.parse_expr()
        then = self.parse_block()
        els = None
        if self.match(TokenType.ELSE):
            if self.check(TokenType.IF):
                els = Block(stmts=[self.parse_if()], line=line)
            else:
                els = self.parse_block()
        return IfStmt(cond=cond, then_branch=then, else_branch=els, line=line)

    def parse_while(self) -> WhileStmt:
        line = self.current().line
        self.advance()
        cond = self.parse_expr()
        body = self.parse_block()
        return WhileStmt(cond=cond, body=body, line=line)

    def parse_for(self) -> ForStmt:
        line = self.current().line
        self.advance()
        var = self.current().value
        self.expect(TokenType.IDENT, "loop variable")
        self.expect(TokenType.IN, "in")
        iter_expr = self.parse_expr()
        body = self.parse_block()
        return ForStmt(var=var, iter=iter_expr, body=body, line=line)

    # ─── Expressions ─────────────────────────────────────────────────────────

    def parse_expr(self) -> ASTNode:
        left = self.parse_binary(1)
        if self.check(TokenType.ASSIGN):
            self.advance()
            right = self.parse_expr()
            return AssignExpr(target=left, value=right, line=left.line)
        return left

    def parse_binary(self, min_prec: int) -> ASTNode:
        left = self.parse_unary()
        while True:
            prec = self._prec(self.current().type)
            if prec < min_prec:
                break
            op = self.current().value
            self.advance()
            right = self.parse_binary(prec + 1)
            left = BinaryExpr(op=op, left=left, right=right, line=left.line)
        return left

    def _prec(self, typ: TokenType) -> int:
        return {
            TokenType.OR: 1,
            TokenType.AND: 2,
            TokenType.EQ: 3, TokenType.NEQ: 3,
            TokenType.LT: 4, TokenType.GT: 4,
            TokenType.LTE: 4, TokenType.GTE: 4,
            TokenType.PLUS: 5, TokenType.MINUS: 5,
            TokenType.STAR: 6, TokenType.SLASH: 6, TokenType.PERCENT: 6,
        }.get(typ, 0)

    def parse_unary(self) -> ASTNode:
        if self.check(TokenType.MINUS, TokenType.NOT):
            op = self.current().value
            line = self.current().line
            self.advance()
            return UnaryExpr(op=op, operand=self.parse_unary(), line=line)
        return self.parse_postfix()

    def parse_postfix(self) -> ASTNode:
        left = self.parse_primary()
        while True:
            if self.check(TokenType.LPAREN):
                # call
                self.advance()
                args = []
                while not self.check(TokenType.RPAREN, TokenType.EOF):
                    args.append(self.parse_expr())
                    if not self.match(TokenType.COMMA):
                        break
                self.expect(TokenType.RPAREN, ")")
                left = CallExpr(callee=left, args=args, line=left.line)

            elif self.check(TokenType.DOT):
                self.advance()
                member = self.current().value
                self.expect(TokenType.IDENT, "member name")
                left = MemberExpr(obj=left, member=member, line=left.line)

            elif self.check(TokenType.LBRACKET):
                self.advance()
                idx = self.parse_expr()
                self.expect(TokenType.RBRACKET, "]")
                left = IndexExpr(obj=left, index=idx, line=left.line)

            else:
                break
        return left

    def parse_primary(self) -> ASTNode:
        line = self.current().line

        # Literals
        if self.check(TokenType.INT):
            v = self.current().value; self.advance()
            return IntLiteral(value=v, line=line)

        if self.check(TokenType.FLOAT):
            v = self.current().value; self.advance()
            return FloatLiteral(value=v, line=line)

        if self.check(TokenType.STRING):
            v = self.current().value; self.advance()
            return StringLiteral(value=v, line=line)

        if self.check(TokenType.BOOL):
            v = self.current().value; self.advance()
            return BoolLiteral(value=v, line=line)

        if self.check(TokenType.IDENT) and self.current().value == 'null':
            self.advance()
            return NullLiteral(line=line)

        # Array literal: [1, 2, 3]
        if self.check(TokenType.LBRACKET):
            self.advance()
            elements = []
            while not self.check(TokenType.RBRACKET, TokenType.EOF):
                elements.append(self.parse_expr())
                if not self.match(TokenType.COMMA):
                    break
            self.expect(TokenType.RBRACKET, "]")
            return ArrayLiteral(elements=elements, line=line)

        # Namespaced path / FFI call:
        #   python::requests::get(url)
        #   c::sodium::randombytes_buf(12)
        #   requests::get(url)          ← shorthand after import
        if self.check(TokenType.IDENT, TokenType.SELF):
            first = self.current().value
            self.advance()

            # Check for ::  (namespace / FFI)
            if self.check(TokenType.COLONCOLON):
                parts = [first]
                while self.check(TokenType.COLONCOLON):
                    self.advance()
                    parts.append(self.current().value)
                    self.expect(TokenType.IDENT, "identifier")

                # If starts with known FFI lang → FFICallExpr (resolved at call site)
                lang = parts[0] if parts[0] in FFI_LANGS else "ketamine"
                path = parts[1:] if parts[0] in FFI_LANGS else parts

                node = NamespaceExpr(parts=parts, line=line)
                return node

            return Ident(name=first, line=line)

        if self.check(TokenType.LPAREN):
            self.advance()
            expr = self.parse_expr()
            self.expect(TokenType.RPAREN, ")")
            return expr

        self.error(f"Unexpected token '{self.current().value}'")
        self.advance()
        return IntLiteral(value=0, line=line)
