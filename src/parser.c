#include "include/parser.h"
#include "include/arena.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// ═══════════════════════════════════════════════════════════════════════════════
// PARSER — Full recursive descent with Pratt expression parsing
// ═══════════════════════════════════════════════════════════════════════════════

// ─── Precedence table ─────────────────────────────────────────────────────────
static Precedence token_precedence(KetTokenType type) {
    switch (type) {
        case TK_EQ: case TK_PLUSEQ: case TK_MINUSEQ:
        case TK_STAREQ: case TK_SLASHEQ: case TK_PERCENTEQ:
        case TK_AMPERSANDEQ: case TK_PIPEEQ: case TK_CARETEQ:
        case TK_LTLTEQ: case TK_GTGTEQ:
            return PREC_ASSIGN;

        case TK_PIPEPIPE: return PREC_OR;
        case TK_AMPAMP:   return PREC_AND;

        case TK_EQEQ: case TK_BANGEQ:
        case TK_LT: case TK_GT: case TK_LTEQ: case TK_GTEQ:
            return PREC_COMPARE;

        case TK_PIPE:           return PREC_BIT_OR;
        case TK_CARET:          return PREC_BIT_XOR;
        case TK_AMPERSAND:      return PREC_BIT_AND;
        case TK_LTLT: case TK_GTGT: return PREC_SHIFT;

        case TK_DOTDOT: case TK_DOTEQ: return PREC_RANGE;

        case TK_PLUS:  case TK_MINUS:   return PREC_TERM;
        case TK_STAR:  case TK_SLASH:
        case TK_PERCENT:                return PREC_FACTOR;

        case TK_POWER: return PREC_POWER;

        default: return PREC_NONE;
    }
}

// ─── Pratt infix binding power ────────────────────────────────────────────────
static Precedence infix_prec(KetTokenType type) {
    return token_precedence(type);
}

static Precedence prefix_prec(KetTokenType type) {
    (void)type;
    return PREC_UNARY;
}

// ═══════════════════════════════════════════════════════════════════════════════
// PARSER INIT / UTILITY
// ═══════════════════════════════════════════════════════════════════════════════

void parser_init(Parser *p, Lexer *lexer, Context *ctx) {
    p->lexer = lexer;
    p->ctx = ctx;
    p->panic_mode = false;
    p->error_count = 0;
    p->recovery_tokens = 0;
    p->nesting_depth = 0;
    p->brace_depth = 0;
    p->paren_depth = 0;
    p->labels = NULL;
    p->label_count = 0;
    p->label_cap = 0;
    p->generics = NULL;
    p->generic_count = 0;
    p->generic_cap = 0;

    // Prime tokens
    p->current = lexer_next(lexer);
    p->peek = lexer_next(lexer);
}

void parser_advance(Parser *p) {
    p->current = p->peek;
    p->peek = lexer_next(p->lexer);
}

Token parser_cur(Parser *p) { return p->current; }
Token parser_peek(Parser *p) { return p->peek; }

bool parser_check(Parser *p, KetTokenType type) {
    return p->current.type == type;
}

bool parser_match(Parser *p, KetTokenType type) {
    if (p->current.type == type) {
        parser_advance(p);
        return true;
    }
    return false;
}

Token parser_expect(Parser *p, KetTokenType type, const char *msg) {
    if (p->current.type == type) {
        Token t = p->current;
        parser_advance(p);
        return t;
    }
    parser_error(p, p->current.loc, "expected %s, got %s", msg,
                 token_type_name(p->current.type));
    Token t;
    t.type = TK_ERROR;
    t.loc = p->current.loc;
    return t;
}

void parser_error(Parser *p, Location loc, const char *fmt, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    ket_diag_push(p->ctx, SEV_ERROR, loc, 0, "%s", buf);
    p->error_count++;
    p->panic_mode = true;
    p->recovery_tokens = 3; // skip 3 tokens to recover
}

void parser_sync(Parser *p) {
    if (!p->panic_mode) return;

    while (p->current.type != TK_EOF) {
        if (p->recovery_tokens > 0) {
            p->recovery_tokens--;
            parser_advance(p);
            continue;
        }

        // Stop at statement boundaries
        switch (p->current.type) {
            case KW_FN: case KW_LET: case KW_CONST:
            case KW_STRUCT: case KW_ENUM: case KW_TRAIT:
            case KW_IMPL: case KW_IMPORT: case KW_PUB:
            case KW_EXTERN: case KW_UNSAFE:
            case KW_IF: case KW_WHILE: case KW_FOR:
            case KW_LOOP: case KW_RETURN: case KW_MATCH:
            case KW_BREAK: case KW_CONTINUE:
            case TK_SEMICOLON: case TK_RBRACE:
                p->panic_mode = false;
                return;
            default:
                parser_advance(p);
        }
    }
    p->panic_mode = false;
}

// ═══════════════════════════════════════════════════════════════════════════════
// SPAN HELPERS
// ═══════════════════════════════════════════════════════════════════════════════

static Span span_from(Parser *p, Location start) {
    Span s;
    s.start = start;
    s.end = p->current.loc;
    return s;
}

static Span span_from_to(Location a, Location b) {
    Span s;
    s.start = a;
    s.end = b;
    return s;
}

// ═══════════════════════════════════════════════════════════════════════════════
// AST NODE CREATION
// ═══════════════════════════════════════════════════════════════════════════════

static ASTNode *new_node(Parser *p, NodeKind kind, Span span) {
    ASTNode *n = arena_alloc_zero(p->ctx->arena, sizeof(ASTNode));
    n->kind = kind;
    n->span = span;
    n->type = NULL;
    n->flags = NF_NONE;
    return n;
}

static ASTNode *new_node_tok(Parser *p, NodeKind kind, Token tok) {
    Span s;
    s.start = tok.loc;
    s.end = tok.loc;
    return new_node(p, kind, s);
}

// ═══════════════════════════════════════════════════════════════════════════════
// PARSING: TOP-LEVEL
// ═══════════════════════════════════════════════════════════════════════════════

ASTNode *parse_program(Parser *p) {
    Location start = p->current.loc;

    // Allocate statements array
    ASTNode **stmts = arena_alloc_zero(p->ctx->arena, sizeof(ASTNode*) * 65536);
    int count = 0;

    while (p->current.type != TK_EOF && !p->ctx->diag.fatal) {
        parser_sync(p);
        if (p->current.type == TK_EOF) break;

        ASTNode *stmt = parse_decl(p);
        if (stmt) {
            stmts[count++] = stmt;
        }
    }

    ASTNode *prog = new_node(p, N_MODULE, span_from(p, start));
    prog->block.stmts = stmts;
    prog->block.stmt_count = count;
    return prog;
}

ASTNode *parse_decl(Parser *p) {
    parser_sync(p);
    Location start = p->current.loc;
    NodeFlags flags = NF_NONE;

    // Attributes
    while (p->current.type == TK_AT) {
        parser_advance(p);
        if (p->current.type == TK_IDENT) {
            // Simple attribute
            parser_advance(p);
        }
    }

    // Visibility
    if (parser_match(p, KW_PUB)) flags |= NF_PUB;

    // Extern
    if (parser_match(p, KW_EXTERN)) flags |= NF_EXTERN;

    // Unsafe
    if (parser_match(p, KW_UNSAFE)) flags |= NF_UNSAFE;

    switch (p->current.type) {
        case KW_FN:     return parse_fn_decl(p, flags);
        case KW_STRUCT: return parse_struct_decl(p, flags);
        case KW_ENUM:   return parse_enum_decl(p, flags);
        case KW_TRAIT:  return parse_trait_decl(p, flags);
        case KW_IMPL:   return parse_impl_block(p);
        case KW_IMPORT: return parse_import(p);
        case KW_TYPE:   return parse_type_alias(p, flags);
        case KW_CONST:
        case KW_LET:    return parse_var_decl(p, flags);
        case KW_MUT:    return parse_var_decl(p, flags | NF_MUT);
        default:        return parse_stmt(p);
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// IMPORT
// ═══════════════════════════════════════════════════════════════════════════════

static ASTNode *parse_import(Parser *p) {
    Location start = p->current.loc;
    parser_advance(p); // skip 'import'

    ASTNode *n = new_node(p, N_IMPORT, span_from(p, start));

    // Parse path a::b::c
    ASTNode **segs = arena_alloc_zero(p->ctx->arena, sizeof(ASTNode*) * 64);
    int seg_count = 0;

    if (p->current.type == TK_IDENT || p->current.type == KW_SELF ||
        p->current.type == KW_SUPER) {
        segs[seg_count++] = new_node_tok(p, N_IDENT, p->current);
        parser_advance(p);

        while (parser_match(p, TK_COLONCOLON)) {
            if (p->current.type == TK_IDENT || p->current.type == TK_STAR ||
                p->current.type == KW_SELF) {
                segs[seg_count++] = new_node_tok(p, N_IDENT, p->current);
                parser_advance(p);
            } else {
                parser_error(p, p->current.loc, "expected identifier after '::'");
                break;
            }
        }
    }

    n->path.segments = segs;
    n->path.path_count = seg_count;

    // Optional alias
    n->import.alias = NULL;
    n->import.is_wildcard = seg_count > 0 &&
        segs[seg_count - 1]->kind == N_IDENT &&
        strncmp(segs[seg_count - 1]->ident.name, "*", 1) == 0;

    parser_expect(p, TK_SEMICOLON, "';' after import");
    return n;
}

// ═══════════════════════════════════════════════════════════════════════════════
// FUNCTION DECLARATION
// ═══════════════════════════════════════════════════════════════════════════════

ASTNode *parse_fn_decl(Parser *p, NodeFlags flags) {
    Location start = p->current.loc;
    parser_advance(p); // skip 'fn'

    // Async
    if (parser_match(p, KW_ASYNC)) flags |= NF_ASYNC;

    // Function name
    Token name = parser_expect(p, TK_IDENT, "function name");
    ASTNode *n = new_node(p, N_FN_DECL, span_from(p, start));
    n->fn_decl.fn_name = name.start;
    n->fn_decl.fn_namelen = name.length;
    n->flags = flags;

    // Generic parameters
    n->fn_decl.fn_generics = NULL;
    n->fn_decl.fn_generic_count = 0;
    if (p->current.type == TK_LT) {
        n->fn_decl.fn_generics = parse_generic_params(p)->decl.decl_fields;
        n->fn_decl.fn_generic_count = parse_generic_params(p)->decl.decl_field_count;
    }

    // Parameters
    parser_expect(p, TK_LPAREN, "'(' for parameter list");
    ASTNode **params = arena_alloc_zero(p->ctx->arena, sizeof(ASTNode*) * MAX_FN_PARAMS);
    int param_count = 0;

    if (p->current.type != TK_RPAREN) {
        do {
            if (param_count >= MAX_FN_PARAMS) {
                parser_error(p, p->current.loc, "too many parameters (max %d)", MAX_FN_PARAMS);
                break;
            }

            // Self parameter
            if (p->current.type == KW_SELF) {
                ASTNode *self = new_node_tok(p, N_IDENT, p->current);
                self->ident.name = "self";
                self->ident.namelen = 4;
                params[param_count++] = self;
                parser_advance(p);
            } else {
                Token pname = parser_expect(p, TK_IDENT, "parameter name");
                parser_expect(p, TK_COLON, "':' after parameter name");

                ASTNode *param = new_node(p, N_VAR_DECL, span_from(p, start));
                param->var_decl.var_name = pname.start;
                param->var_decl.var_namelen = pname.length;
                param->var_decl.var_type = parse_type(p);
                param->var_decl.var_init = NULL;

                // Default value
                if (parser_match(p, TK_EQ)) {
                    param->var_decl.var_init = parse_expr(p);
                }

                params[param_count++] = param;
            }
        } while (parser_match(p, TK_COMMA));
    }
    parser_expect(p, TK_RPAREN, "')' after parameters");

    // Return type
    n->fn_decl.fn_ret_type = NULL;
    n->fn_decl.fn_variadic = false;
    n->fn_decl.fn_cconv = KET_CC_DEFAULT;
    if (parser_match(p, TK_ARROW)) {
        n->fn_decl.fn_ret_type = parse_type(p);
    }

    // Where clause
    n->fn_decl.fn_where = NULL;
    n->fn_decl.fn_where_count = 0;
    if (p->current.type == KW_WHERE) {
        ASTNode *where = parse_where_clause(p);
        n->fn_decl.fn_where = where->block.stmts;
        n->fn_decl.fn_where_count = where->block.stmt_count;
    }

    // Body
    n->fn_decl.fn_params = params;
    n->fn_decl.fn_param_count = param_count;
    n->fn_decl.fn_body = parse_block(p);

    return n;
}

// ═══════════════════════════════════════════════════════════════════════════════
// STRUCT DECLARATION
// ═══════════════════════════════════════════════════════════════════════════════

ASTNode *parse_struct_decl(Parser *p, NodeFlags flags) {
    Location start = p->current.loc;
    parser_advance(p); // skip 'struct'

    Token name = parser_expect(p, TK_IDENT, "struct name");
    ASTNode *n = new_node(p, N_STRUCT_DECL, span_from(p, start));
    n->decl.decl_name = name.start;
    n->decl.decl_namelen = name.length;
    n->flags = flags;

    // Generic params
    if (p->current.type == TK_LT) {
        ASTNode *g = parse_generic_params(p);
        n->decl.decl_generics = g->block.stmts;
        n->decl.decl_generic_count = g->block.stmt_count;
    } else {
        n->decl.decl_generics = NULL;
        n->decl.decl_generic_count = 0;
    }

    // Fields
    parser_expect(p, TK_LBRACE, "'{' for struct body");
    ASTNode **fields = arena_alloc_zero(p->ctx->arena, sizeof(ASTNode*) * MAX_STRUCT_FIELDS);
    int field_count = 0;

    while (p->current.type != TK_RBRACE && p->current.type != TK_EOF) {
        if (p->current.type == KW_PUB) parser_advance(p);
        if (p->current.type == KW_MUT) parser_advance(p);

        Token fname = parser_expect(p, TK_IDENT, "field name");
        parser_expect(p, TK_COLON, "':' after field name");

        ASTNode *field = new_node(p, N_VAR_DECL, span_from(p, start));
        field->var_decl.var_name = fname.start;
        field->var_decl.var_namelen = fname.length;
        field->var_decl.var_type = parse_type(p);
        field->var_decl.var_init = NULL;

        fields[field_count++] = field;

        if (!parser_match(p, TK_COMMA)) break;
    }
    parser_expect(p, TK_RBRACE, "'}' after struct fields");

    n->decl.decl_fields = fields;
    n->decl.decl_field_count = field_count;
    return n;
}

// ═══════════════════════════════════════════════════════════════════════════════
// ENUM DECLARATION
// ═══════════════════════════════════════════════════════════════════════════════

ASTNode *parse_enum_decl(Parser *p, NodeFlags flags) {
    Location start = p->current.loc;
    parser_advance(p); // skip 'enum'

    Token name = parser_expect(p, TK_IDENT, "enum name");
    ASTNode *n = new_node(p, N_ENUM_DECL, span_from(p, start));
    n->decl.decl_name = name.start;
    n->decl.decl_namelen = name.length;
    n->flags = flags;

    // Generic params
    if (p->current.type == TK_LT) {
        ASTNode *g = parse_generic_params(p);
        n->decl.decl_generics = g->block.stmts;
        n->decl.decl_generic_count = g->block.stmt_count;
    } else {
        n->decl.decl_generics = NULL;
        n->decl.decl_generic_count = 0;
    }

    // Variants
    parser_expect(p, TK_LBRACE, "'{' for enum body");
    ASTNode **variants = arena_alloc_zero(p->ctx->arena, sizeof(ASTNode*) * MAX_ENUM_VARIANTS);
    int var_count = 0;

    while (p->current.type != TK_RBRACE && p->current.type != TK_EOF) {
        if (p->current.type == KW_PUB) parser_advance(p);

        Token vname = parser_expect(p, TK_IDENT, "enum variant name");

        ASTNode *variant = new_node(p, N_ENUM_INIT, span_from(p, start));
        variant->decl.decl_name = vname.start;
        variant->decl.decl_namelen = vname.length;

        // Payload types
        if (parser_match(p, TK_LPAREN)) {
            ASTNode **payloads = arena_alloc_zero(p->ctx->arena,
                                                   sizeof(ASTNode*) * MAX_TUPLE_FIELDS);
            int pcount = 0;
            if (p->current.type != TK_RPAREN) {
                do {
                    struct Type *ptype = parse_type(p);
                    ASTNode *pnode = new_node(p, N_TYPE_PATH, span_from(p, start));
                    pnode->type_node.type_expr = ptype;
                    payloads[pcount++] = pnode;
                } while (parser_match(p, TK_COMMA));
            }
            parser_expect(p, TK_RPAREN, "')' after variant payload");
            variant->decl.decl_fields = payloads;
            variant->decl.decl_field_count = pcount;
        }

        // Discriminant value
        if (parser_match(p, TK_EQ)) {
            // Optional explicit discriminant
            variant->var_decl.var_init = parse_expr(p);
        }

        variants[var_count++] = variant;
        if (!parser_match(p, TK_COMMA)) break;
    }
    parser_expect(p, TK_RBRACE, "'}' after enum variants");

    n->decl.decl_fields = variants;
    n->decl.decl_field_count = var_count;
    return n;
}

// ═══════════════════════════════════════════════════════════════════════════════
// TRAIT DECLARATION
// ═══════════════════════════════════════════════════════════════════════════════

ASTNode *parse_trait_decl(Parser *p, NodeFlags flags) {
    Location start = p->current.loc;
    parser_advance(p); // skip 'trait'

    Token name = parser_expect(p, TK_IDENT, "trait name");
    ASTNode *n = new_node(p, N_TRAIT_DECL, span_from(p, start));
    n->decl.decl_name = name.start;
    n->decl.decl_namelen = name.length;
    n->flags = flags;

    // Generic params
    if (p->current.type == TK_LT) {
        ASTNode *g = parse_generic_params(p);
        n->decl.decl_generics = g->block.stmts;
        n->decl.decl_generic_count = g->block.stmt_count;
    }

    // Trait bounds
    if (parser_match(p, TK_COLON)) {
        // Super-traits
        while (p->current.type == TK_IDENT) {
            parser_advance(p);
            if (!parser_match(p, TK_PLUS)) break;
        }
    }

    // Methods
    parser_expect(p, TK_LBRACE, "'{' for trait body");
    ASTNode **methods = arena_alloc_zero(p->ctx->arena, sizeof(ASTNode*) * 256);
    int mcount = 0;

    while (p->current.type != TK_RBRACE && p->current.type != TK_EOF) {
        if (parser_match(p, KW_PUB)) {}
        if (parser_match(p, KW_FN)) {
            ASTNode *method = parse_fn_decl(p, NF_NONE);
            methods[mcount++] = method;
        }
    }
    parser_expect(p, TK_RBRACE, "'}' after trait methods");

    n->decl.decl_fields = methods;
    n->decl.decl_field_count = mcount;
    return n;
}

// ═══════════════════════════════════════════════════════════════════════════════
// IMPL BLOCK
// ═══════════════════════════════════════════════════════════════════════════════

ASTNode *parse_impl_block(Parser *p) {
    Location start = p->current.loc;
    parser_advance(p); // skip 'impl'

    ASTNode *n = new_node(p, N_IMPL_BLOCK, span_from(p, start));

    // Generic params
    if (p->current.type == TK_LT) {
        ASTNode *g = parse_generic_params(p);
        n->impl_block.impl_generics = g->block.stmts;
        n->impl_block.impl_generic_count = g->block.stmt_count;
    } else {
        n->impl_block.impl_generics = NULL;
        n->impl_block.impl_generic_count = 0;
    }

    // Trait impl or inherent impl
    n->impl_block.impl_trait = NULL;
    struct Type *first_type = parse_type(p);

    if (parser_match(p, KW_FOR)) {
        // Trait impl
        n->impl_block.impl_trait = first_type;
        n->impl_block.impl_type = parse_type(p);
    } else {
        // Inherent impl
        n->impl_block.impl_type = first_type;
    }

    // Methods
    parser_expect(p, TK_LBRACE, "'{' for impl block");
    ASTNode **methods = arena_alloc_zero(p->ctx->arena, sizeof(ASTNode*) * 256);
    int mcount = 0;

    while (p->current.type != TK_RBRACE && p->current.type != TK_EOF) {
        ASTNode *method = parse_fn_decl(p, NF_NONE);
        methods[mcount++] = method;
    }
    parser_expect(p, TK_RBRACE, "'}' after impl methods");

    n->impl_block.impl_methods = methods;
    n->impl_block.impl_method_count = mcount;

    return n;
}

// ═══════════════════════════════════════════════════════════════════════════════
// TYPE ALIAS
// ═══════════════════════════════════════════════════════════════════════════════

static ASTNode *parse_type_alias(Parser *p, NodeFlags flags) {
    Location start = p->current.loc;
    parser_advance(p); // skip 'type'

    Token name = parser_expect(p, TK_IDENT, "type name");
    parser_expect(p, TK_EQ, "'=' in type alias");

    ASTNode *n = new_node(p, N_TYPE_ALIAS, span_from(p, start));
    n->decl.decl_name = name.start;
    n->decl.decl_namelen = name.length;
    n->flags = flags;

    // For now store the type in a var_init
    struct Type *alias_type = parse_type(p);
    n->type_node.type_expr = alias_type;

    parser_expect(p, TK_SEMICOLON, "';' after type alias");
    return n;
}

// ═══════════════════════════════════════════════════════════════════════════════
// GENERIC PARAMETERS & ARGUMENTS
// ═══════════════════════════════════════════════════════════════════════════════

ASTNode *parse_generic_params(Parser *p) {
    // Parse <T, U: Bound, V = Default>
    parser_expect(p, TK_LT, "'<' for generic parameters");

    ASTNode *n = new_node(p, N_BLOCK, span_from(p, p->current.loc));
    ASTNode **params = arena_alloc_zero(p->ctx->arena, sizeof(ASTNode*) * MAX_GENERIC_PARAMS);
    int count = 0;

    while (p->current.type != TK_GT && p->current.type != TK_EOF) {
        Token name;
        if (p->current.type == TK_IDENT) {
            name = p->current;
            parser_advance(p);
        } else {
            parser_error(p, p->current.loc, "expected generic parameter name");
            break;
        }

        ASTNode *gp = new_node(p, N_GENERIC_PARAM, span_from(p, name.loc));
        gp->generic.generic_name = name.start;
        gp->generic.generic_namelen = name.length;
        gp->generic.generic_bound = NULL;

        // Trait bound
        if (parser_match(p, TK_COLON)) {
            gp->generic.generic_bound = parse_type(p);
        }

        params[count++] = gp;
        if (!parser_match(p, TK_COMMA)) break;
    }
    parser_expect(p, TK_GT, "'>' after generic parameters");

    n->block.stmts = params;
    n->block.stmt_count = count;
    return n;
}

ASTNode *parse_generic_args(Parser *p, ASTNode *base) {
    (void)base;
    // Parse <int, str, T>
    parser_expect(p, TK_LT, "'<' for generic args");

    ASTNode **args = arena_alloc_zero(p->ctx->arena, sizeof(ASTNode*) * MAX_GENERIC_PARAMS);
    int count = 0;

    while (p->current.type != TK_GT && p->current.type != TK_EOF) {
        struct Type *t = parse_type(p);
        ASTNode *arg = new_node(p, N_GENERIC_ARG, span_from(p, p->current.loc));
        arg->type_node.type_expr = t;
        args[count++] = arg;

        if (!parser_match(p, TK_COMMA)) break;
    }
    parser_expect(p, TK_GT, "'>' after generic args");

    ASTNode *n = new_node(p, N_BLOCK, span_from(p, p->current.loc));
    n->block.stmts = args;
    n->block.stmt_count = count;
    return n;
}

ASTNode *parse_where_clause(Parser *p) {
    parser_expect(p, KW_WHERE, "'where'");

    ASTNode *n = new_node(p, N_BLOCK, span_from(p, p->current.loc));
    ASTNode **clauses = arena_alloc_zero(p->ctx->arena, sizeof(ASTNode*) * 64);
    int count = 0;

    while (p->current.type != TK_LBRACE && p->current.type != TK_EOF) {
        struct Type *type = parse_type(p);
        parser_expect(p, TK_COLON, "':' in where clause");
        struct Type *bound = parse_type(p);

        ASTNode *wc = new_node(p, N_WHERE_CLAUSE, span_from(p, p->current.loc));
        wc->where_clause.where_type = type;
        wc->where_clause.where_bound = bound;
        clauses[count++] = wc;

        if (!parser_match(p, TK_COMMA)) break;
    }

    n->block.stmts = clauses;
    n->block.stmt_count = count;
    return n;
}

// ═══════════════════════════════════════════════════════════════════════════════
// TYPE PARSING
// ═══════════════════════════════════════════════════════════════════════════════

struct Type *parse_type(Parser *p) {
    return parse_type_path(p);
}

struct Type *parse_type_path(Parser *p) {
    TypeTable *tt = &p->ctx->types;

    // Tuple type
    if (parser_match(p, TK_LPAREN)) {
        struct Type **elems = arena_alloc_zero(p->ctx->arena,
                                                sizeof(struct Type*) * MAX_TUPLE_FIELDS);
        int count = 0;

        if (p->current.type != TK_RPAREN) {
            do {
                elems[count++] = parse_type(p);
            } while (parser_match(p, TK_COMMA));
        }
        parser_expect(p, TK_RPAREN, "')' in tuple type");

        if (count == 0) return tt->ty_void;
        if (count == 1) return elems[0];
        return ket_type_tuple(tt, elems, count);
    }

    // Slice type
    if (parser_match(p, TK_LBRACKET)) {
        if (parser_match(p, TK_RBRACKET)) {
            // []T — slice
            struct Type *elem = parse_type(p);
            return ket_type_array(tt, elem, -1);
        }
        // [T; N] or [T]
        struct Type *elem = parse_type(p);
        int64_t count = -1;
        if (parser_match(p, TK_SEMICOLON)) {
            if (p->current.type == TK_INT_LIT) {
                count = p->current.ival;
                parser_advance(p);
            }
        }
        parser_expect(p, TK_RBRACKET, "']' in array type");
        return ket_type_array(tt, elem, count);
    }

    // Function type
    if (parser_match(p, KW_FN)) {
        return parse_type_fn(p);
    }

    // Reference type
    if (parser_match(p, TK_AMPERSAND)) {
        bool mut = parser_match(p, KW_MUT);
        struct Type *inner = parse_type(p);
        return ket_type_ref(tt, inner, mut);
    }

    // Pointer type
    if (parser_match(p, TK_STAR)) {
        bool mut = parser_match(p, KW_MUT);
        struct Type *inner = parse_type(p);
        return ket_type_ptr(tt, inner);
    }

    // Identifier / path type
    if (p->current.type == TK_IDENT || token_is_type_keyword(p->current.type)) {
        struct Type *base = NULL;
        const char *name = p->current.start;
        int name_len = p->current.length;

        // Check for built-in types
        switch (p->current.type) {
            case TK_TYPE_VOID:  base = tt->ty_void; break;
            case TK_TYPE_BOOL:  base = tt->ty_bool; break;
            case TK_TYPE_BYTE:  base = tt->ty_byte; break;
            case TK_TYPE_INT:   base = tt->ty_i64;  break;
            case TK_TYPE_I8:    base = ket_type_get(tt, TY_I8); break;
            case TK_TYPE_I16:   base = ket_type_get(tt, TY_I16); break;
            case TK_TYPE_I32:   base = ket_type_get(tt, TY_I32); break;
            case TK_TYPE_I64:   base = tt->ty_i64; break;
            case TK_TYPE_U8:    base = ket_type_get(tt, TY_U8); break;
            case TK_TYPE_U16:   base = ket_type_get(tt, TY_U16); break;
            case TK_TYPE_U32:   base = ket_type_get(tt, TY_U32); break;
            case TK_TYPE_U64:   base = ket_type_get(tt, TY_U64); break;
            case TK_TYPE_F32:   base = ket_type_get(tt, TY_F32); break;
            case TK_TYPE_F64:   base = tt->ty_f64; break;
            case TK_TYPE_STR:   base = tt->ty_str; break;
            case TK_TYPE_CHAR:  base = tt->ty_char; break;
            case TK_TYPE_NEVER: base = tt->ty_never; break;
            case TK_TYPE_AUTO:  base = tt->ty_infer; break;
            default:
                base = ket_type_get(tt, TY_STRUCT);
                base->struct_type.name = name;
                break;
        }

        parser_advance(p);

        // Generic args
        if (p->current.type == TK_LT && base) {
            struct Type **args = arena_alloc_zero(p->ctx->arena,
                                                   sizeof(struct Type*) * MAX_GENERIC_PARAMS);
            int arg_count = 0;
            parser_advance(p); // skip '<'
            while (p->current.type != TK_GT && p->current.type != TK_EOF) {
                args[arg_count++] = parse_type(p);
                if (!parser_match(p, TK_COMMA)) break;
            }
            parser_expect(p, TK_GT, "'>' after generic args");
            // For now, just return the base type
        }

        return base ? base : tt->ty_infer;
    }

    // Impl Trait
    if (parser_match(p, KW_IMPL)) {
        struct Type *trait_type = parse_type(p);
        (void)trait_type;
        return tt->ty_infer;
    }

    // Error
    parser_error(p, p->current.loc, "expected type");
    return tt->ty_error;
}

struct Type *parse_type_fn(Parser *p) {
    parser_expect(p, TK_LPAREN, "'(' in function type");

    struct Type **params = arena_alloc_zero(p->ctx->arena,
                                             sizeof(struct Type*) * MAX_FN_PARAMS);
    int pcount = 0;
    bool variadic = false;

    if (p->current.type != TK_RPAREN) {
        do {
            if (parser_match(p, TK_DOTDOTDOT)) {
                variadic = true;
                break;
            }
            params[pcount++] = parse_type(p);
        } while (parser_match(p, TK_COMMA));
    }
    parser_expect(p, TK_RPAREN, "')' in function type");

    struct Type *ret = NULL;
    if (parser_match(p, TK_ARROW)) {
        ret = parse_type(p);
    }

    TypeTable *tt = &p->ctx->types;
    return ket_type_fn(tt, params, pcount, ret, variadic, KET_CC_DEFAULT);
}

// ═══════════════════════════════════════════════════════════════════════════════
// STATEMENTS
// ═══════════════════════════════════════════════════════════════════════════════

ASTNode *parse_block(Parser *p) {
    Location start = p->current.loc;
    parser_expect(p, TK_LBRACE, "'{' to start block");
    p->brace_depth++;
    p->nesting_depth++;

    ASTNode **stmts = arena_alloc_zero(p->ctx->arena, sizeof(ASTNode*) * 4096);
    int count = 0;

    while (p->current.type != TK_RBRACE && p->current.type != TK_EOF) {
        parser_sync(p);
        ASTNode *stmt = parse_stmt(p);
        if (stmt) {
            stmts[count++] = stmt;
        }
    }

    parser_expect(p, TK_RBRACE, "'}' to end block");
    p->brace_depth--;
    p->nesting_depth--;

    ASTNode *n = new_node(p, N_BLOCK, span_from(p, start));
    n->block.stmts = stmts;
    n->block.stmt_count = count;
    return n;
}

ASTNode *parse_stmt(Parser *p) {
    // Skip semicolons
    while (parser_match(p, TK_SEMICOLON)) {}

    Location start = p->current.loc;

    switch (p->current.type) {
        case KW_LET:
        case KW_MUT:
        case KW_CONST:
            return parse_var_decl(p, NF_NONE);

        case KW_RETURN:
            return parse_return(p);

        case KW_IF:
            return parse_if(p);

        case KW_WHILE:
            return parse_while(p);

        case KW_FOR:
            return parse_for(p);

        case KW_LOOP:
            return parse_loop(p);

        case KW_BREAK:
            return parse_break(p);

        case KW_CONTINUE:
            return parse_continue(p);

        case KW_MATCH:
            return parse_match(p);

        case KW_STRUCT:
            return parse_struct_decl(p, NF_NONE);

        case KW_ENUM:
            return parse_enum_decl(p, NF_NONE);

        case KW_TRAIT:
            return parse_trait_decl(p, NF_NONE);

        case KW_IMPL:
            return parse_impl_block(p);

        case TK_LBRACE:
            return parse_block(p);

        case KW_UNSAFE:
            parser_advance(p);
            return parse_block(p);

        default: {
            // Expression statement
            ASTNode *expr = parse_expr(p);
            parser_match(p, TK_SEMICOLON);

            ASTNode *n = new_node(p, N_EXPR_STMT, span_from(p, start));
            n->ret_val = expr;
            return n;
        }
    }
}

ASTNode *parse_var_decl(Parser *p, NodeFlags flags) {
    Location start = p->current.loc;

    bool is_mut = (flags & NF_MUT) || parser_match(p, KW_MUT);
    bool is_const = parser_match(p, KW_CONST);
    parser_expect(p, KW_LET, "'let' for variable declaration");

    // Destructuring tuple
    if (p->current.type == TK_LPAREN) {
        parser_advance(p);
        ASTNode **vars = arena_alloc_zero(p->ctx->arena, sizeof(ASTNode*) * MAX_TUPLE_FIELDS);
        int vcount = 0;

        do {
            Token vname = parser_expect(p, TK_IDENT, "variable name");
            ASTNode *v = new_node_tok(p, N_VAR_DECL, vname);
            v->var_decl.var_name = vname.start;
            v->var_decl.var_namelen = vname.length;
            vars[vcount++] = v;
        } while (parser_match(p, TK_COMMA));

        parser_expect(p, TK_RPAREN, "')' in destructuring");

        ASTNode *init = NULL;
        if (parser_match(p, TK_COLON)) {
            // Type annotation
            parse_type(p);
        }
        if (parser_match(p, TK_EQ)) {
            init = parse_expr(p);
        }

        ASTNode *n = new_node(p, N_VAR_DECL_TUPLE, span_from(p, start));
        n->tuple.tuple_elems = vars;
        n->tuple.tuple_count = vcount;
        n->var_decl.var_init = init;
        n->flags = is_mut ? NF_MUT : NF_NONE;

        parser_expect(p, TK_SEMICOLON, "';'");
        return n;
    }

    Token name = parser_expect(p, TK_IDENT, "variable name");

    struct Type *type = NULL;
    if (parser_match(p, TK_COLON)) {
        type = parse_type(p);
    }

    ASTNode *init = NULL;
    if (parser_match(p, TK_EQ)) {
        init = parse_expr(p);
    }

    parser_expect(p, TK_SEMICOLON, "';' after variable declaration");

    ASTNode *n = new_node(p, N_VAR_DECL, span_from(p, start));
    n->var_decl.var_name = name.start;
    n->var_decl.var_namelen = name.length;
    n->var_decl.var_type = type;
    n->var_decl.var_init = init;
    n->flags = is_mut ? NF_MUT : NF_NONE;
    if (is_const) n->flags |= NF_CONST;

    return n;
}

ASTNode *parse_return(Parser *p) {
    Location start = p->current.loc;
    parser_advance(p);

    ASTNode *val = NULL;
    if (p->current.type != TK_SEMICOLON && p->current.type != TK_RBRACE &&
        p->current.type != TK_EOF) {
        val = parse_expr(p);
    }

    parser_expect(p, TK_SEMICOLON, "';' after return");

    ASTNode *n = new_node(p, N_RETURN, span_from(p, start));
    n->ret_val = val;
    return n;
}

ASTNode *parse_if(Parser *p) {
    Location start = p->current.loc;
    parser_advance(p); // skip 'if'

    ASTNode *cond = parse_expr(p);
    ASTNode *then_body = parse_block(p);

    // Label for if-else chain
    ASTNode *else_body = NULL;
    if (parser_match(p, KW_ELSE)) {
        if (p->current.type == KW_IF) {
            else_body = parse_if(p);
        } else if (p->current.type == TK_LBRACE) {
            else_body = parse_block(p);
        }
    }

    ASTNode *n = new_node(p, N_IF, span_from(p, start));
    n->if_node.if_cond = cond;
    n->if_node.if_then = then_body;
    n->if_node.if_else = else_body;
    return n;
}

ASTNode *parse_while(Parser *p) {
    Location start = p->current.loc;
    parser_advance(p); // skip 'while'

    ASTNode *n = new_node(p, N_WHILE, span_from(p, start));

    // Optional label
    if (p->current.type == TK_COLON) {
        parser_advance(p);
        n->loop.loop_label = NULL;
    }

    n->loop.loop_cond = parse_expr(p);
    n->loop.loop_body = parse_block(p);
    return n;
}

ASTNode *parse_for(Parser *p) {
    Location start = p->current.loc;
    parser_advance(p); // skip 'for'

    ASTNode *n = new_node(p, N_FOR, span_from(p, start));

    // for pattern in iterable { body }
    Token var = parser_expect(p, TK_IDENT, "loop variable");
    n->loop.loop_var = var.start;

    parser_expect(p, KW_IN, "'in' in for loop");
    n->loop.loop_iter = parse_expr(p);
    n->loop.loop_body = parse_block(p);
    return n;
}

ASTNode *parse_loop(Parser *p) {
    Location start = p->current.loc;
    parser_advance(p); // skip 'loop'

    ASTNode *n = new_node(p, N_LOOP, span_from(p, start));
    n->loop.loop_cond = NULL;
    n->loop.loop_body = parse_block(p);
    return n;
}

ASTNode *parse_break(Parser *p) {
    Location start = p->current.loc;
    parser_advance(p); // skip 'break'

    ASTNode *val = NULL;
    if (p->current.type != TK_SEMICOLON) {
        val = parse_expr(p);
    }

    parser_expect(p, TK_SEMICOLON, "';' after break");

    ASTNode *n = new_node(p, N_BREAK, span_from(p, start));
    n->break_node.break_value = val;
    return n;
}

ASTNode *parse_continue(Parser *p) {
    Location start = p->current.loc;
    parser_advance(p); // skip 'continue'
    parser_expect(p, TK_SEMICOLON, "';' after continue");

    return new_node(p, N_CONTINUE, span_from(p, start));
}

ASTNode *parse_match(Parser *p) {
    Location start = p->current.loc;
    parser_advance(p); // skip 'match'

    ASTNode *expr = parse_expr(p);
    parser_expect(p, TK_LBRACE, "'{' for match body");

    ASTNode **arms = arena_alloc_zero(p->ctx->arena, sizeof(ASTNode*) * MAX_MATCH_ARMS);
    int arm_count = 0;

    while (p->current.type != TK_RBRACE && p->current.type != TK_EOF) {
        ASTNode *pat = parse_pattern(p);

        ASTNode *guard = NULL;
        if (parser_match(p, KW_IF)) {
            guard = parse_expr(p);
        }

        parser_expect(p, TK_FAT_ARROW, "'=>' in match arm");

        ASTNode *body = NULL;
        if (p->current.type == TK_LBRACE) {
            body = parse_block(p);
        } else {
            body = parse_expr(p);
            parser_match(p, TK_COMMA);
        }

        ASTNode *arm = new_node(p, N_MATCH_ARM, span_from(p, start));
        arm->arm.arm_pat = pat;
        arm->arm.arm_guard = guard;
        arm->arm.arm_body = body;
        arms[arm_count++] = arm;

        // Allow trailing comma
        parser_match(p, TK_COMMA);
    }
    parser_expect(p, TK_RBRACE, "'}' after match body");

    ASTNode *n = new_node(p, N_MATCH, span_from(p, start));
    n->match.match_expr = expr;
    n->match.match_arms = arms;
    n->match.match_arm_count = arm_count;
    return n;
}

// ═══════════════════════════════════════════════════════════════════════════════
// PATTERN PARSING
// ═══════════════════════════════════════════════════════════════════════════════

ASTNode *parse_pattern(Parser *p) {
    Location start = p->current.loc;

    // Wildcard
    if (parser_match(p, TK_UNDERSCORE)) {
        return new_node(p, N_PAT_WILD, span_from(p, start));
    }

    // Literal
    if (token_is_literal(p->current.type)) {
        ASTNode *lit = new_node_tok(p, N_PAT_LIT, p->current);
        parser_advance(p);
        return lit;
    }

    // Identifier (binding or enum)
    if (p->current.type == TK_IDENT) {
        Token name = p->current;
        parser_advance(p);

        // Enum pattern: Variant(...) or Variant{...}
        if (p->current.type == TK_LPAREN || p->current.type == TK_LBRACE) {
            bool is_paren = parser_match(p, TK_LPAREN);

            ASTNode *pat = new_node(p, N_PAT_ENUM, span_from(p, start));
            pat->pat_name = name.start;
            pat->pat_namelen = name.length;

            ASTNode **elems = NULL;
            int ecount = 0;

            if (is_paren) {
                // Tuple-like: Variant(a, b, _)
                elems = arena_alloc_zero(p->ctx->arena, sizeof(ASTNode*) * MAX_TUPLE_FIELDS);
                while (p->current.type != TK_RPAREN && p->current.type != TK_EOF) {
                    elems[ecount++] = parse_pattern(p);
                    if (!parser_match(p, TK_COMMA)) break;
                }
                parser_expect(p, TK_RPAREN, "')' in enum pattern");
            } else {
                // Struct-like: Variant { field: pattern }
                elems = arena_alloc_zero(p->ctx->arena, sizeof(ASTNode*) * MAX_STRUCT_FIELDS);
                while (p->current.type != TK_RBRACE && p->current.type != TK_EOF) {
                    elems[ecount++] = parse_pattern(p);
                    if (!parser_match(p, TK_COMMA)) break;
                }
                parser_expect(p, TK_RBRACE, "'}' in enum pattern");
            }

            pat->pat_elems = elems;
            pat->pat_elem_count = ecount;
            return pat;
        }

        // Simple binding
        bool is_ref = false;
        bool is_mut = false;
        ASTNode *pat = new_node(p, N_PAT_BIND, span_from(p, start));
        pat->pat_name = name.start;
        pat->pat_namelen = name.length;

        // Sub-pattern after @
        if (parser_match(p, TK_AT)) {
            pat->pat_sub = parse_pattern(p);
        }

        return pat;
    }

    // Tuple pattern (a, b, c)
    if (parser_match(p, TK_LPAREN)) {
        ASTNode **elems = arena_alloc_zero(p->ctx->arena, sizeof(ASTNode*) * MAX_TUPLE_FIELDS);
        int count = 0;

        do {
            elems[count++] = parse_pattern(p);
        } while (parser_match(p, TK_COMMA));

        parser_expect(p, TK_RPAREN, "')' in tuple pattern");

        ASTNode *pat = new_node(p, N_PAT_TUPLE, span_from(p, start));
        pat->pat_elems = elems;
        pat->pat_elem_count = count;
        return pat;
    }

    // Range pattern a..b
    if (p->current.type == TK_INT_LIT || p->current.type == TK_FLOAT_LIT) {
        ASTNode *low = new_node_tok(p, N_PAT_LIT, p->current);
        parser_advance(p);

        if (p->current.type == TK_DOTDOT || p->current.type == TK_DOTEQ) {
            bool inclusive = parser_match(p, TK_DOTEQ) || parser_match(p, TK_DOTDOT);
            ASTNode *high = new_node_tok(p, N_PAT_LIT, p->current);
            parser_advance(p);

            ASTNode *pat = new_node(p, N_PAT_RANGE, span_from(p, start));
            pat->pat_low = low;
            pat->pat_high = high;
            pat->range.range_inclusive = inclusive;
            return pat;
        }

        // Single literal pattern
        ASTNode *pat = new_node(p, N_PAT_LIT, span_from(p, start));
        pat->pat_low = low;
        return pat;
    }

    // Reference pattern &x / &mut x
    if (parser_match(p, TK_AMPERSAND)) {
        bool mut = parser_match(p, KW_MUT);
        ASTNode *sub = parse_pattern(p);
        ASTNode *pat = new_node(p, N_PAT_REF, span_from(p, start));
        pat->pat_sub = sub;
        pat->pat_ref = true;
        pat->pat_mut = mut;
        return pat;
    }

    // Struct pattern Type { field, .. }
    if (p->current.type == TK_IDENT) {
        ASTNode *pat = new_node(p, N_PAT_STRUCT, span_from(p, start));
        pat->pat_name = p->current.start;
        pat->pat_namelen = p->current.length;
        parser_advance(p);

        parser_expect(p, TK_LBRACE, "'{' in struct pattern");
        ASTNode **fields = arena_alloc_zero(p->ctx->arena, sizeof(ASTNode*) * MAX_STRUCT_FIELDS);
        int fcount = 0;

        while (p->current.type != TK_RBRACE && p->current.type != TK_EOF) {
            fields[fcount++] = parse_pattern(p);
            if (!parser_match(p, TK_COMMA)) break;
        }
        parser_expect(p, TK_RBRACE, "'}' after struct pattern");

        pat->pat_elems = fields;
        pat->pat_elem_count = fcount;
        return pat;
    }

    parser_error(p, p->current.loc, "expected pattern");
    ASTNode *n = new_node(p, N_PAT_WILD, span_from(p, start));
    return n;
}

// ═══════════════════════════════════════════════════════════════════════════════
// EXPRESSION PARSING (Pratt)
// ═══════════════════════════════════════════════════════════════════════════════

ASTNode *parse_expr(Parser *p) {
    return parse_expr_prec(p, PREC_NONE);
}

ASTNode *parse_primary_expr(Parser *p) {
    Location start = p->current.loc;

    switch (p->current.type) {
        case TK_INT_LIT: {
            ASTNode *n = new_node_tok(p, N_LITERAL_INT, p->current);
            n->ival = p->current.ival;
            parser_advance(p);
            return n;
        }
        case TK_FLOAT_LIT: {
            ASTNode *n = new_node_tok(p, N_LITERAL_FLOAT, p->current);
            n->fval = p->current.fval;
            parser_advance(p);
            return n;
        }
        case TK_STR_LIT:
        case TK_RAW_STR_LIT: {
            ASTNode *n = new_node_tok(p, N_LITERAL_STR, p->current);
            n->sval = p->current.start;
            n->slen = p->current.length;
            // Strip quotes
            if (p->current.length >= 2) {
                n->sval = p->current.start + 1;
                n->slen = p->current.length - 2;
            }
            parser_advance(p);
            return n;
        }
        case TK_CHAR_LIT: {
            ASTNode *n = new_node_tok(p, N_LITERAL_CHAR, p->current);
            n->cval = p->current.start[1]; // simplified
            parser_advance(p);
            return n;
        }
        case TK_BOOL_LIT: {
            ASTNode *n = new_node_tok(p, N_LITERAL_BOOL, p->current);
            n->bval = (p->current.start[0] == 't');
            parser_advance(p);
            return n;
        }
        case TK_NULL_LIT: {
            ASTNode *n = new_node_tok(p, N_LITERAL_NULL, p->current);
            parser_advance(p);
            return n;
        }
        case TK_IDENT: {
            ASTNode *n = new_node_tok(p, N_IDENT, p->current);
            n->ident.name = p->current.start;
            n->ident.namelen = p->current.length;
            n->ident.resolved = NULL;
            parser_advance(p);
            return n;
        }
        case KW_SELF: {
            ASTNode *n = new_node_tok(p, N_IDENT, p->current);
            n->ident.name = "self";
            n->ident.namelen = 4;
            parser_advance(p);
            return n;
        }
        case TK_LPAREN: {
            parser_advance(p);
            // Empty tuple or grouped expression
            if (parser_match(p, TK_RPAREN)) {
                ASTNode *n = new_node(p, N_TUPLE, span_from(p, start));
                n->tuple.tuple_elems = NULL;
                n->tuple.tuple_count = 0;
                return n;
            }

            ASTNode *first = parse_expr(p);

            if (parser_match(p, TK_COMMA)) {
                // Tuple
                ASTNode **elems = arena_alloc_zero(p->ctx->arena,
                                                    sizeof(ASTNode*) * MAX_TUPLE_FIELDS);
                int count = 0;
                elems[count++] = first;

                do {
                    elems[count++] = parse_expr(p);
                } while (parser_match(p, TK_COMMA));

                parser_expect(p, TK_RPAREN, "')' after tuple");

                ASTNode *n = new_node(p, N_TUPLE, span_from(p, start));
                n->tuple.tuple_elems = elems;
                n->tuple.tuple_count = count;
                return n;
            }

            parser_expect(p, TK_RPAREN, "')' after expression");
            return first;
        }
        case TK_LBRACKET: {
            parser_advance(p);
            ASTNode **elems = arena_alloc_zero(p->ctx->arena,
                                                sizeof(ASTNode*) * 4096);
            int count = 0;

            while (p->current.type != TK_RBRACKET && p->current.type != TK_EOF) {
                elems[count++] = parse_expr(p);
                if (!parser_match(p, TK_COMMA)) break;
            }
            parser_expect(p, TK_RBRACKET, "']' after array literal");

            ASTNode *n = new_node(p, N_ARRAY_INIT, span_from(p, start));
            n->init.init_args = elems;
            n->init.init_argc = count;
            return n;
        }
        case KW_MOVE: {
            parser_advance(p);
            ASTNode *inner = parse_expr(p);
            ASTNode *n = new_node(p, N_UNARY, span_from(p, start));
            n->unary.unary_op = KW_MOVE;
            n->unary.unary_right = inner;
            return n;
        }
        default:
            parser_error(p, p->current.loc, "expected expression, got %s",
                         token_type_name(p->current.type));
            ASTNode *n = new_node(p, N_LITERAL_INT, span_from(p, start));
            n->ival = 0;
            return n;
    }
}

ASTNode *parse_prefix_expr(Parser *p) {
    Location start = p->current.loc;

    switch (p->current.type) {
        case TK_MINUS:
        case TK_EXCLAM:
        case TK_TILDE:
        case TK_STAR: {
            KetTokenType op = p->current.type;
            parser_advance(p);
            ASTNode *right = parse_expr_prec(p, PREC_UNARY);
            ASTNode *n = new_node(p, N_UNARY, span_from(p, start));
            n->unary.unary_op = op;
            n->unary.unary_right = right;
            return n;
        }
        case TK_AMPERSAND: {
            parser_advance(p);
            bool mut = parser_match(p, KW_MUT);
            ASTNode *right = parse_expr_prec(p, PREC_UNARY);
            ASTNode *n = new_node(p, N_REF_EXPR, span_from(p, start));
            n->unary.unary_op = TK_AMPERSAND;
            n->unary.unary_right = right;
            n->flags = mut ? NF_MUT : NF_NONE;
            return n;
        }
        case TK_AND:  // 'and' keyword
        case TK_NOT:  // 'not' keyword
            // Handled by the lexer mapping
            return parse_primary_expr(p);
        default:
            return parse_primary_expr(p);
    }
}

ASTNode *parse_postfix_expr(Parser *p, ASTNode *left) {
    for (;;) {
        // Call: foo()
        if (p->current.type == TK_LPAREN) {
            Location start = p->current.loc;
            parser_advance(p);
            ASTNode **args = arena_alloc_zero(p->ctx->arena,
                                               sizeof(ASTNode*) * 256);
            int argc = 0;

            if (p->current.type != TK_RPAREN) {
                do {
                    args[argc++] = parse_expr(p);
                } while (parser_match(p, TK_COMMA) && argc < 256);
            }
            parser_expect(p, TK_RPAREN, "')' after arguments");

            ASTNode *n = new_node(p, N_CALL, span_from(p, start));
            n->call.call_callee = left;
            n->call.call_args = args;
            n->call.call_argc = argc;
            n->call.call_method = NULL;
            left = n;
            continue;
        }

        // Index: arr[i]
        if (p->current.type == TK_LBRACKET) {
            parser_advance(p);
            ASTNode *index = parse_expr(p);
            parser_expect(p, TK_RBRACKET, "']' after index");

            ASTNode *n = new_node(p, N_INDEX, span_from(p, left->span.start));
            n->index.index_target = left;
            n->index.index_expr = index;
            left = n;
            continue;
        }

        // Member: a.b
        if (parser_match(p, TK_DOT)) {
            if (p->current.type == TK_IDENT || token_is_keyword(p->current.type)) {
                ASTNode *n = new_node(p, N_MEMBER, span_from(p, left->span.start));
                n->member.member_target = left;
                n->member.member_name = p->current.start;
                n->member.member_namelen = p->current.length;
                parser_advance(p);
                left = n;
                continue;
            }
            if (p->current.type == TK_INT_LIT) {
                // Tuple index: a.0
                ASTNode *n = new_node(p, N_INDEX, span_from(p, left->span.start));
                n->index.index_target = left;
                ASTNode *idx = new_node_tok(p, N_LITERAL_INT, p->current);
                idx->ival = p->current.ival;
                parser_advance(p);
                n->index.index_expr = idx;
                left = n;
                continue;
            }
            parser_error(p, p->current.loc, "expected field name after '.'");
            break;
        }

        // Question mark: expr?
        if (parser_match(p, TK_QUESTION)) {
            ASTNode *n = new_node(p, N_TRY, span_from(p, left->span.start));
            n->unary.unary_right = left;
            left = n;
            continue;
        }

        break;
    }

    return left;
}

ASTNode *parse_expr_prec(Parser *p, Precedence min_prec) {
    ASTNode *left = parse_prefix_expr(p);
    left = parse_postfix_expr(p, left);

    for (;;) {
        KetTokenType op = p->current.type;
        Precedence prec = infix_prec(op);

        // Range
        if (op == TK_DOTDOT || op == TK_DOTEQ) {
            if (prec < min_prec) break;
            parser_advance(p);
            ASTNode *right = parse_expr_prec(p, (Precedence)(prec + 1));

            ASTNode *n = new_node(p, N_RANGE, span_from(p, left->span.start));
            n->range.range_start = left;
            n->range.range_end = right;
            n->range.range_inclusive = (op == TK_DOTEQ);
            left = n;
            continue;
        }

        // Assignment
        if (token_is_assignment(op) && op != TK_EQEQ) {
            if (PREC_ASSIGN < min_prec) break;
            parser_advance(p);
            ASTNode *right = parse_expr_prec(p, PREC_ASSIGN);

            ASTNode *n = new_node(p, N_ASSIGN, span_from(p, left->span.start));
            n->assign.assign_target = left;
            n->assign.assign_value = right;
            n->assign.assign_op = op;
            left = n;
            continue;
        }

        // As cast
        if (op == KW_AS) {
            if (PREC_CAST < min_prec) break;
            parser_advance(p);
            struct Type *cast_type = parse_type(p);

            ASTNode *n = new_node(p, N_CAST, span_from(p, left->span.start));
            n->cast.cast_expr = left;
            n->cast.cast_type = cast_type;
            left = n;
            continue;
        }

        // Pipe forward: a |> b
        if (op == TK_PIPE_FORWARD) {
            if (PREC_CALL < min_prec) break;
            parser_advance(p);

            // Create a call: b(a)
            ASTNode *callee = parse_primary_expr(p);
            ASTNode *n = new_node(p, N_CALL, span_from(p, left->span.start));
            n->call.call_callee = callee;
            n->call.call_args = arena_alloc_zero(p->ctx->arena, sizeof(ASTNode*) * 2);
            n->call.call_args[0] = left;
            n->call.call_argc = 1;
            left = n;
            continue;
        }

        if (prec < min_prec || prec == PREC_NONE) break;

        parser_advance(p);
        ASTNode *right = parse_expr_prec(p, (Precedence)(prec + 1));

        ASTNode *n = new_node(p, N_BINARY, span_from(p, left->span.start));
        n->binary.bin_left = left;
        n->binary.bin_right = right;
        n->binary.bin_op = op;
        left = n;
    }

    return left;
}
