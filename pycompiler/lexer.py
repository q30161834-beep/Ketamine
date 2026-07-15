"""Lexical analyzer for Ketamine"""
import re
from dataclasses import dataclass
from enum import Enum, auto
from typing import List

class TokenType(Enum):
    # Literals
    INT = auto()
    FLOAT = auto()
    STRING = auto()
    BOOL = auto()
    
    # Keywords
    FN = auto()
    LET = auto()
    MUT = auto()
    RETURN = auto()
    IF = auto()
    ELSE = auto()
    WHILE = auto()
    FOR = auto()
    IN = auto()
    STRUCT = auto()
    IMPL = auto()
    ENUM = auto()
    MATCH = auto()
    IMPORT = auto()
    PUB = auto()
    SELF = auto()
    
    # Types
    TYPE_INT = auto()
    TYPE_FLOAT = auto()
    TYPE_BOOL = auto()
    TYPE_STR = auto()
    TYPE_VOID = auto()
    
    # Operators
    PLUS = auto()
    MINUS = auto()
    STAR = auto()
    SLASH = auto()
    PERCENT = auto()
    EQ = auto()
    NEQ = auto()
    LT = auto()
    GT = auto()
    LTE = auto()
    GTE = auto()
    AND = auto()
    OR = auto()
    NOT = auto()
    ASSIGN = auto()
    ARROW = auto()
    FAT_ARROW = auto()
    COLONCOLON = auto()   # ::  (namespace separator for FFI)
    
    # Delimiters
    LPAREN = auto()
    RPAREN = auto()
    LBRACE = auto()
    RBRACE = auto()
    LBRACKET = auto()
    RBRACKET = auto()
    COMMA = auto()
    COLON = auto()
    SEMICOLON = auto()
    DOT = auto()
    
    IDENT = auto()
    EOF = auto()

@dataclass
class Token:
    type: TokenType
    value: any
    line: int
    col: int
    
    def __str__(self):
        return f"[{self.line}:{self.col}] {self.type.name:12} {repr(self.value)}"

class Lexer:
    KEYWORDS = {
        'fn': TokenType.FN, 'let': TokenType.LET, 'mut': TokenType.MUT,
        'return': TokenType.RETURN, 'if': TokenType.IF, 'else': TokenType.ELSE,
        'while': TokenType.WHILE, 'for': TokenType.FOR, 'in': TokenType.IN,
        'struct': TokenType.STRUCT, 'impl': TokenType.IMPL, 'enum': TokenType.ENUM,
        'match': TokenType.MATCH, 'import': TokenType.IMPORT, 'pub': TokenType.PUB,
        'self': TokenType.SELF, 'true': TokenType.BOOL, 'false': TokenType.BOOL,
        'int': TokenType.TYPE_INT, 'float': TokenType.TYPE_FLOAT,
        'bool': TokenType.TYPE_BOOL, 'str': TokenType.TYPE_STR,
        'void': TokenType.TYPE_VOID,
    }
    
    def __init__(self, source: str, filename: str = "<stdin>"):
        self.source = source
        self.filename = filename
        self.pos = 0
        self.line = 1
        self.col = 1
        self.errors = []
    
    def error(self, msg: str):
        self.errors.append(f"{self.filename}:{self.line}:{self.col}: error: {msg}")
    
    def peek(self, offset=0):
        pos = self.pos + offset
        return self.source[pos] if pos < len(self.source) else '\0'
    
    def advance(self):
        if self.pos < len(self.source):
            if self.source[self.pos] == '\n':
                self.line += 1
                self.col = 1
            else:
                self.col += 1
            self.pos += 1
    
    def skip_whitespace(self):
        while self.peek() in ' \t\n\r':
            self.advance()
    
    def skip_comment(self):
        if self.peek() == '/' and self.peek(1) == '/':
            while self.peek() not in '\n\0':
                self.advance()
        elif self.peek() == '/' and self.peek(1) == '*':
            self.advance(); self.advance()
            while not (self.peek() == '*' and self.peek(1) == '/'):
                if self.peek() == '\0':
                    self.error("Unclosed block comment")
                    break
                self.advance()
            if self.peek() == '*':
                self.advance(); self.advance()
    
    def read_string(self):
        line, col = self.line, self.col
        self.advance()  # skip opening "
        chars = []
        while self.peek() not in '"\0':
            if self.peek() == '\\':
                self.advance()
                esc = self.peek()
                if esc == 'n': chars.append('\n')
                elif esc == 't': chars.append('\t')
                elif esc == '\\': chars.append('\\')
                elif esc == '"': chars.append('"')
                else: chars.append(esc)
                self.advance()
            else:
                chars.append(self.peek())
                self.advance()
        if self.peek() == '"':
            self.advance()
        else:
            self.error("Unterminated string")
        return Token(TokenType.STRING, ''.join(chars), line, col)
    
    def read_number(self):
        line, col = self.line, self.col
        num_str = ''
        is_float = False
        while self.peek().isdigit() or self.peek() == '.':
            if self.peek() == '.':
                if is_float:
                    break
                is_float = True
            num_str += self.peek()
            self.advance()
        value = float(num_str) if is_float else int(num_str)
        typ = TokenType.FLOAT if is_float else TokenType.INT
        return Token(typ, value, line, col)
    
    def read_ident(self):
        line, col = self.line, self.col
        ident = ''
        while self.peek().isalnum() or self.peek() == '_':
            ident += self.peek()
            self.advance()
        
        typ = self.KEYWORDS.get(ident, TokenType.IDENT)
        value = ident
        if typ == TokenType.BOOL:
            value = (ident == 'true')
        return Token(typ, value, line, col)
    
    def tokenize(self) -> List[Token]:
        tokens = []
        
        while self.pos < len(self.source):
            self.skip_whitespace()
            
            if self.peek() == '\0':
                break
            
            # Comments
            if self.peek() == '/' and self.peek(1) in '/*':
                self.skip_comment()
                continue
            
            line, col = self.line, self.col
            c = self.peek()
            
            # String
            if c == '"':
                tokens.append(self.read_string())
                continue
            
            # Number
            if c.isdigit():
                tokens.append(self.read_number())
                continue
            
            # Identifier / keyword
            if c.isalpha() or c == '_':
                tokens.append(self.read_ident())
                continue
            
            # Two-char operators
            if c == '-' and self.peek(1) == '>':
                tokens.append(Token(TokenType.ARROW, '->', line, col))
                self.advance(); self.advance()
                continue
            if c == '=' and self.peek(1) == '>':
                tokens.append(Token(TokenType.FAT_ARROW, '=>', line, col))
                self.advance(); self.advance()
                continue
            if c == '=' and self.peek(1) == '=':
                tokens.append(Token(TokenType.EQ, '==', line, col))
                self.advance(); self.advance()
                continue
            if c == '!' and self.peek(1) == '=':
                tokens.append(Token(TokenType.NEQ, '!=', line, col))
                self.advance(); self.advance()
                continue
            if c == '<' and self.peek(1) == '=':
                tokens.append(Token(TokenType.LTE, '<=', line, col))
                self.advance(); self.advance()
                continue
            if c == '>' and self.peek(1) == '=':
                tokens.append(Token(TokenType.GTE, '>=', line, col))
                self.advance(); self.advance()
                continue
            if c == '&' and self.peek(1) == '&':
                tokens.append(Token(TokenType.AND, '&&', line, col))
                self.advance(); self.advance()
                continue
            if c == '|' and self.peek(1) == '|':
                tokens.append(Token(TokenType.OR, '||', line, col))
                self.advance(); self.advance()
                continue
            if c == ':' and self.peek(1) == ':':
                tokens.append(Token(TokenType.COLONCOLON, '::', line, col))
                self.advance(); self.advance()
                continue
            
            # Single-char
            single = {
                '+': TokenType.PLUS, '-': TokenType.MINUS, '*': TokenType.STAR,
                '/': TokenType.SLASH, '%': TokenType.PERCENT, '<': TokenType.LT,
                '>': TokenType.GT, '!': TokenType.NOT, '=': TokenType.ASSIGN,
                '(': TokenType.LPAREN, ')': TokenType.RPAREN,
                '{': TokenType.LBRACE, '}': TokenType.RBRACE,
                '[': TokenType.LBRACKET, ']': TokenType.RBRACKET,
                ',': TokenType.COMMA, ':': TokenType.COLON,
                ';': TokenType.SEMICOLON, '.': TokenType.DOT,
            }
            
            if c in single:
                tokens.append(Token(single[c], c, line, col))
                self.advance()
                continue
            
            self.error(f"Unexpected character '{c}'")
            self.advance()
        
        tokens.append(Token(TokenType.EOF, None, self.line, self.col))
        return tokens
