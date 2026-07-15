#ifndef KETAMINE_TYPES_H
#define KETAMINE_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdarg.h>

// ═══════════════════════════════════════════════════════════════════════════════
// KETAMINE COMPILER — Core Type Definitions
// ═══════════════════════════════════════════════════════════════════════════════

// ─── Version ──────────────────────────────────────────────────────────────────
#define KET_VERSION_MAJOR 1
#define KET_VERSION_MINOR 0
#define KET_VERSION_PATCH 0

// ─── Limits ───────────────────────────────────────────────────────────────────
#define MAX_IDENT_LEN      255
#define MAX_STRING_LEN     (1 << 20)     // 1 MB
#define MAX_SOURCE_SIZE    (1 << 24)     // 16 MB
#define MAX_LINE_LEN       (1 << 16)     // 64 KB
#define MAX_TOKEN_LEN      (1 << 14)     // 16 KB
#define MAX_ERRORS         256
#define MAX_WARNINGS       512
#define MAX_INCLUDE_DEPTH  128
#define MAX_FN_PARAMS      64
#define MAX_TUPLE_FIELDS   32
#define MAX_MATCH_ARMS     256
#define MAX_ENUM_VARIANTS  64
#define MAX_STRUCT_FIELDS  128
#define MAX_GENERIC_PARAMS 16
#define MAX_NESTING_DEPTH  512
#define MAX_ALIGNMENT      16
#define MAX_SSA_REGS       (1 << 24)
#define MAX_BASIC_BLOCKS   (1 << 20)

// ─── Compiler Phases ──────────────────────────────────────────────────────────
typedef enum {
    PHASE_LEX,
    PHASE_PARSE,
    PHASE_SEMA,
    PHASE_TYPE_CHECK,
    PHASE_BORROW_CHECK,
    PHASE_IR_GEN,
    PHASE_IR_OPTIMIZE,
    PHASE_CODEGEN,
    PHASE_LINK,
    PHASE_COUNT
} CompilerPhase;

// ─── Severity ─────────────────────────────────────────────────────────────────
typedef enum {
    SEV_NOTE,
    SEV_WARNING,
    SEV_ERROR,
    SEV_FATAL,
    SEV_ICE     // Internal Compiler Error
} Severity;

// ─── Source Location ──────────────────────────────────────────────────────────
typedef struct {
    const char *file;       // filename
    const char *source;     // pointer to source text
    int         offset;     // byte offset from start
    int         line;       // 1-based
    int         col;        // 1-based
    int         length;     // token length in bytes
} Location;

#define LOC_NONE ((Location){NULL, NULL, 0, 0, 0, 0})

// ─── Span (range in source) ───────────────────────────────────────────────────
typedef struct {
    Location start;
    Location end;
} Span;

// ─── String Intern ────────────────────────────────────────────────────────────
typedef struct InternStr {
    struct InternStr *next;
    uint32_t          hash;
    int               len;
    char              data[];
} InternStr;

// ─── Token Types ──────────────────────────────────────────────────────────────
typedef enum {
    // ── Special ──
    TK_EOF = 0,
    TK_ERROR,
    TK_INVALID,

    // ── Literals ──
    TK_INT_LIT,           // 42, 0xFF, 0b101, 1_000
    TK_FLOAT_LIT,         // 3.14, 1e-10, 0x1.0p3
    TK_STR_LIT,           // "hello"
    TK_RAW_STR_LIT,       // r"hello" / #"hello"#
    TK_BYTE_LIT,          // b'A'
    TK_CHAR_LIT,          // 'A', '\n'
    TK_BOOL_LIT,          // true, false
    TK_NULL_LIT,          // null

    // ── Identifiers ──
    TK_IDENT,

    // ── Keywords ──
    KW_FN,      KW_LET,     KW_MUT,    KW_CONST,
    KW_RETURN,  KW_IF,      KW_ELSE,   KW_WHILE,
    KW_FOR,     KW_IN,      KW_LOOP,   KW_BREAK,
    KW_CONTINUE,KW_STRUCT,  KW_ENUM,   KW_IMPL,
    KW_MATCH,   KW_IMPORT,  KW_PUB,    KW_SELF,
    KW_SUPER,   KW_TYPE,    KW_TRAIT,  KW_WHERE,
    KW_AS,      KW_USE,     KW_MOD,    KW_REF,
    KW_MOVE,    KW_STATIC,  KW_EXTERN, KW_UNSAFE,
    KW_ASYNC,   KW_AWAIT,   KW_YIELD,  KW_VIRTUAL,
    KW_OVERRIDE,KW_ABSTRACT,KW_FINAL,  KW_DEFAULT,
    KW_MACRO,   KW_INLINE,  KW_ALIAS,  KW_DELEGATE,
    KW_CHECKED, KW_UNCHECKED,

    // ── Types ──
    TK_TYPE_VOID,   TK_TYPE_BOOL,   TK_TYPE_BYTE,
    TK_TYPE_INT,    TK_TYPE_I8,     TK_TYPE_I16,
    TK_TYPE_I32,    TK_TYPE_I64,    TK_TYPE_I128,
    TK_TYPE_U8,     TK_TYPE_U16,    TK_TYPE_U32,
    TK_TYPE_U64,    TK_TYPE_U128,   TK_TYPE_F16,
    TK_TYPE_F32,    TK_TYPE_F64,    TK_TYPE_F128,
    TK_TYPE_STR,    TK_TYPE_CHAR,   TK_TYPE_NEVER,
    TK_TYPE_SELF,   TK_TYPE_AUTO,

    // ── Operators ──
    TK_PLUS,        TK_MINUS,       TK_STAR,
    TK_SLASH,       TK_PERCENT,     TK_POWER,
    TK_AMPERSAND,   TK_PIPE,        TK_CARET,
    TK_TILDE,       TK_EXCLAM,      TK_QUESTION,
    TK_LTLT,        TK_GTGT,        TK_AMPAMP,
    TK_PIPEPIPE,    TK_EQEQ,        TK_BANGEQ,
    TK_LT,          TK_GT,          TK_LTEQ,
    TK_GTEQ,        TK_PLUSEQ,      TK_MINUSEQ,
    TK_STAREQ,      TK_SLASHEQ,     TK_PERCENTEQ,
    TK_AMPERSANDEQ, TK_PIPEEQ,      TK_CARETEQ,
    TK_LTLTEQ,      TK_GTGTEQ,      TK_EQ,
    TK_ARROW,       TK_FAT_ARROW,   TK_DOT,
    TK_DOTDOT,      TK_DOTDOTDOT,   TK_DOTEQ,
    TK_POUND,       TK_AT,          TK_DOLLAR,
    TK_UNDERSCORE,  TK_BACKSLASH,   TK_COLON,
    TK_COLONCOLON,  TK_SEMICOLON,   TK_COMMA,
    TK_LPAREN,      TK_RPAREN,      TK_LBRACKET,
    TK_RBRACKET,    TK_LBRACE,      TK_RBRACE,
    TK_HASH,        TK_PIPE_FORWARD,

    // ── Custom operators (for DSL) ──
    TK_CUSTOM_OP_START,
    // ── Count ──
    TK_COUNT
} TokenType;

// ─── Token ────────────────────────────────────────────────────────────────────
typedef struct {
    TokenType   type;
    Location    loc;
    const char *start;
    int         length;
    int64_t     ival;       // for integer literals
    double      fval;       // for float literals
    bool        overflow;   // literal overflow flag
} Token;

// ─── Diagnostic ───────────────────────────────────────────────────────────────
typedef struct {
    Severity    severity;
    Location    loc;
    int         code;
    const char *message;
    const char *suggestion;
    struct {
        Location    loc;
        const char *label;
    } notes[8];
    int note_count;
} Diagnostic;

// ─── Diagnostics Bag ──────────────────────────────────────────────────────────
typedef struct {
    Diagnostic  diags[MAX_ERRORS + MAX_WARNINGS];
    int         count;
    int         error_count;
    int         warning_count;
    bool        fatal;
} Diagnostics;

// ═══════════════════════════════════════════════════════════════════════════════
// TYPE SYSTEM
// ═══════════════════════════════════════════════════════════════════════════════

typedef enum {
    // Primitives
    TY_NEVER,
    TY_VOID,
    TY_BOOL,
    TY_BYTE,
    TY_CHAR,
    TY_I8,    TY_I16,   TY_I32,   TY_I64,   TY_I128,
    TY_U8,    TY_U16,   TY_U32,   TY_U64,   TY_U128,
    TY_F16,   TY_F32,   TY_F64,   TY_F128,
    TY_STR,

    // Composites
    TY_ARRAY,           // [T; N] or [T]
    TY_SLICE,           // []T (unsized)
    TY_PTR,             // *T (raw pointer)
    TY_REF,             // &T (reference)
    TY_MUT_REF,         // &mut T
    TY_TUPLE,           // (T1, T2, ...)
    TY_FN,              // fn(T1, T2) -> T3
    TY_STRUCT,          // named struct
    TY_ENUM,            // named enum
    TY_TRAIT,           // trait object
    TY_IMPL_TRAIT,      // impl Trait for Type
    TY_ALIAS,           // type alias
    TY_GENERIC,         // generic parameter
    TY_INFER,           // type to be inferred
    TY_ERROR,           // error sentinel
    TY_OPAQUE,          // opaque external type
} TypeKind;

// ─── Forward declarations ─────────────────────────────────────────────────────
typedef struct Type      Type;
typedef struct TypeTable TypeTable;

// ─── Type flags ───────────────────────────────────────────────────────────────
typedef enum {
    TF_NONE       = 0,
    TF_CONST      = 1 << 0,
    TF_VOLATILE   = 1 << 1,
    TF_RESTRICT   = 1 << 2,
    TF_ALIGNED    = 1 << 3,
    TF_PACKED     = 1 << 4,
    TF_UNSIGNED   = 1 << 5,
} TypeFlags;

// ─── Calling convention ───────────────────────────────────────────────────────
typedef enum {
    CC_DEFAULT,
    CC_CDECL,
    CC_STDCALL,
    CC_FASTCALL,
    CC_VECTORCALL,
    CC_THISCALL,
    CC_REGCALL,
    CC_KETAMINE,
} CallingConv;

// ─── Type ─────────────────────────────────────────────────────────────────────
struct Type {
    TypeKind    kind;
    TypeFlags   flags;
    int         size;       // size in bytes (0 for unsized)
    int         align;      // alignment in bytes

    union {
        // Integer types (I8-U128, F16-F128)
        int bit_width;

        // Array / Slice
        struct {
            struct Type *elem;
            int64_t      count;     // -1 for slices, 0+ for arrays
        } array;

        // Pointer / Reference
        struct {
            struct Type *pointee;
        } ptr;

        // Tuple
        struct {
            struct Type **elems;
            int           elem_count;
        } tuple;

        // Function
        struct {
            struct Type **param_types;
            struct Type  *return_type;
            int           param_count;
            bool          is_variadic;
            CallingConv   calling_conv;
        } fn;

        // Struct
        struct {
            const char   *name;
            struct Field *fields;
            int           field_count;
            void         *decl_ast;    // backref to AST
        } struct_type;

        // Enum
        struct {
            const char     *name;
            struct Variant *variants;
            int             variant_count;
            struct Type    *discriminant;  // backing int type
            void           *decl_ast;
        } enum_type;

        // Trait
        struct {
            const char       *name;
            struct TraitFn   *methods;
            int               method_count;
        } trait;

        // Generic
        struct {
            const char    *name;
            int            index;
            struct Type   *bound;       // trait bound
        } generic;

        // Alias
        struct {
            const char  *name;
            struct Type *underlying;
        } alias;
    };
};

// ─── Struct Field ─────────────────────────────────────────────────────────────
typedef struct Field {
    const char     *name;
    struct Type    *type;
    int             offset;
    bool           is_pub;
    Location        loc;
} Field;

// ─── Enum Variant ─────────────────────────────────────────────────────────────
typedef struct Variant {
    const char     *name;
    struct Type   **payload_types;
    int             payload_count;
    int64_t         discriminant;
    Location        loc;
} Variant;

// ─── Trait Method ─────────────────────────────────────────────────────────────
typedef struct TraitFn {
    const char     *name;
    struct Type    *fn_type;
    bool           has_default;
} TraitFn;

// ─── Type Table (arena-backed dedup) ──────────────────────────────────────────
struct TypeTable {
    struct Type  **types;
    int            count;
    int            capacity;
    void          *arena;
    // Built-in types (cached)
    struct Type   *ty_never;
    struct Type   *ty_void;
    struct Type   *ty_bool;
    struct Type   *ty_byte;
    struct Type   *ty_char;
    struct Type   *ty_i8;
    struct Type   *ty_i16;
    struct Type   *ty_i32;
    struct Type   *ty_i64;
    struct Type   *ty_i128;
    struct Type   *ty_u8;
    struct Type   *ty_u16;
    struct Type   *ty_u32;
    struct Type   *ty_u64;
    struct Type   *ty_u128;
    struct Type   *ty_f32;
    struct Type   *ty_f64;
    struct Type   *ty_str;
    struct Type   *ty_infer;
    struct Type   *ty_error;
};

// ═══════════════════════════════════════════════════════════════════════════════
// AST — Abstract Syntax Tree
// ═══════════════════════════════════════════════════════════════════════════════

typedef enum {
    // ── Top-level ──
    N_MODULE,
    N_IMPORT,
    N_FN_DECL,
    N_STRUCT_DECL,
    N_ENUM_DECL,
    N_TRAIT_DECL,
    N_IMPL_BLOCK,
    N_TYPE_ALIAS,
    N_CONST_DECL,
    N_STATIC_DECL,
    N_EXTERN_BLOCK,
    N_MACRO_DEF,

    // ── Statements ──
    N_BLOCK,
    N_VAR_DECL,
    N_VAR_DECL_TUPLE,
    N_ASSIGN,
    N_ASSIGN_OP,
    N_RETURN,
    N_YIELD,
    N_IF,
    N_WHILE,
    N_FOR,
    N_LOOP,
    N_BREAK,
    N_CONTINUE,
    N_MATCH,
    N_MATCH_ARM,
    N_EXPR_STMT,
    N_EMPTY_STMT,

    // ── Expressions ──
    N_LITERAL_INT,
    N_LITERAL_FLOAT,
    N_LITERAL_STR,
    N_LITERAL_CHAR,
    N_LITERAL_BOOL,
    N_LITERAL_NULL,
    N_IDENT,
    N_PATH,             // foo::bar::baz
    N_BINARY,
    N_UNARY,
    N_CALL,
    N_METHOD_CALL,
    N_INDEX,
    N_MEMBER,           // a.b
    N_TUPLE,
    N_ARRAY_INIT,
    N_STRUCT_INIT,
    N_ENUM_INIT,
    N_CLOSURE,
    N_BLOCK_EXPR,
    N_IF_EXPR,
    N_MATCH_EXPR,
    N_CAST,
    N_TYPE_AS,
    N_REF_EXPR,         // &expr
    N_DEREF_EXPR,       // *expr
    N_AWAIT,
    N_TRY,              // expr?
    N_TRY_BANG,         // expr!
    N_RANGE,            // a..b, a..=b
    N_FORWARD_PIPE,     // a |> b

    // ── Types ──
    N_TYPE_PATH,
    N_TYPE_TUPLE,
    N_TYPE_FN,
    N_TYPE_REF,
    N_TYPE_PTR,
    N_TYPE_ARRAY,
    N_TYPE_SLICE,
    N_TYPE_GENERIC,
    N_TYPE_IMPL_TRAIT,
    N_TYPE_OPAQUE,

    // ── Patterns ──
    N_PAT_WILD,
    N_PAT_LIT,
    N_PAT_BIND,
    N_PAT_ENUM,
    N_PAT_STRUCT,
    N_PAT_TUPLE,
    N_PAT_RANGE,
    N_PAT_GUARD,
    N_PAT_REF,
    N_PAT_SLICE,

    // ── Other ──
    N_GENERIC_PARAM,
    N_GENERIC_ARG,
    N_WHERE_CLAUSE,
    N_ATTR,
    N_COMMENT,
} NodeKind;

// ─── AST Node flags ───────────────────────────────────────────────────────────
typedef enum {
    NF_NONE         = 0,
    NF_PUB          = 1 << 0,
    NF_MUT          = 1 << 1,
    NF_CONST        = 1 << 2,
    NF_STATIC       = 1 << 3,
    NF_EXTERN       = 1 << 4,
    NF_UNSAFE       = 1 << 5,
    NF_ASYNC        = 1 << 6,
    NF_INLINE       = 1 << 7,
    NF_VIRTUAL      = 1 << 8,
    NF_OVERRIDE     = 1 << 9,
    NF_ABSTRACT     = 1 << 10,
    NF_FINAL        = 1 << 11,
    NF_REF          = 1 << 12,
    NF_MOVE         = 1 << 13,
    NF_BREAK_LABEL  = 1 << 14,
    NF_COMPTIME     = 1 << 15,
    NF_HAS_BLOCK    = 1 << 16,
} NodeFlags;

// ─── Forward declaration ─────────────────────────────────────────────────────
typedef struct ASTNode ASTNode;
typedef struct ASTPool ASTPool;

// ─── AST Node ─────────────────────────────────────────────────────────────────
struct ASTNode {
    NodeKind      kind;
    NodeFlags     flags;
    Span          span;
    struct Type  *type;        // resolved type (after type checking)

    union {
        // Literals
        int64_t     ival;
        uint64_t    uval;
        double      fval;
        union { const char *sval; int slen; };
        bool        bval;
        char        cval;

        // Identifier / Path
        struct {
            const char    *name;
            int            namelen;
            struct ASTNode *resolved;  // points to declaration
        } ident;

        // Path (a::b::c)
        struct {
            struct ASTNode **segments;
            int              seg_count;
        } path;

        // Import
        struct {
            struct ASTNode **path;
            int              path_count;
            const char      *alias;
            bool             is_wildcard;
        } import;

        // Function declaration
        struct {
            const char       *fn_name;
            int               fn_namelen;
            struct ASTNode  **fn_params;
            int               fn_param_count;
            struct Type      *fn_ret_type;
            struct ASTNode   *fn_body;
            struct ASTNode  **fn_generics;
            int               fn_generic_count;
            struct ASTNode  **fn_where;
            int               fn_where_count;
            bool              fn_variadic;
            CallingConv       fn_cconv;
        } fn_decl;

        // Variable declaration
        struct {
            const char       *var_name;
            int               var_namelen;
            struct Type      *var_type;
            struct ASTNode   *var_init;
        } var_decl;

        // Assignment
        struct {
            struct ASTNode *assign_target;
            struct ASTNode *assign_value;
            TokenType       assign_op;   // TK_EQ, TK_PLUSEQ, etc.
        } assign;

        // If / If-Expr
        struct {
            struct ASTNode *if_cond;
            struct ASTNode *if_then;
            struct ASTNode *if_else;
        } if_node;

        // While / For / Loop
        struct {
            const char       *loop_label;
            struct ASTNode   *loop_cond;     // NULL for loop
            struct ASTNode   *loop_iter;     // for: iterable
            const char       *loop_var;      // for: loop variable
            struct ASTNode   *loop_body;
        } loop;

        // Break / Continue
        struct {
            const char  *label;          // target label (NULL if innermost)
            struct ASTNode *break_value; // for break with value
        } break_node;

        // Match
        struct {
            struct ASTNode  *match_expr;
            struct ASTNode **match_arms;
            int              match_arm_count;
        } match;

        // Match arm
        struct {
            struct ASTNode  *arm_pat;
            struct ASTNode  *arm_guard;
            struct ASTNode  *arm_body;
        } arm;

        // Binary expression
        struct {
            struct ASTNode *bin_left;
            struct ASTNode *bin_right;
            TokenType       bin_op;
        } binary;

        // Unary expression
        struct {
            struct ASTNode *unary_right;
            TokenType       unary_op;
        } unary;

        // Call / Method call
        struct {
            struct ASTNode  *call_callee;
            struct ASTNode **call_args;
            int              call_argc;
            struct ASTNode  *call_method;  // for method calls
        } call;

        // Index
        struct {
            struct ASTNode *index_target;
            struct ASTNode *index_expr;
        } index;

        // Member access
        struct {
            struct ASTNode *member_target;
            const char     *member_name;
            int             member_namelen;
        } member;

        // Tuple
        struct {
            struct ASTNode **tuple_elems;
            int              tuple_count;
        } tuple;

        // Array / Struct / Enum init
        struct {
            struct ASTNode  *init_type;       // type being constructed
            struct ASTNode **init_args;
            int              init_argc;
            struct Field    *init_fields;     // named fields for struct
            int              init_field_count;
        } init;

        // Closure
        struct {
            struct ASTNode **closure_params;
            int              closure_param_count;
            struct ASTNode  *closure_body;
            bool             closure_move;
        } closure;

        // Range
        struct {
            struct ASTNode *range_start;
            struct ASTNode *range_end;
            bool            range_inclusive;
        } range;

        // Cast
        struct {
            struct ASTNode  *cast_expr;
            struct Type     *cast_type;
        } cast;

        // Block / Module
        struct {
            struct ASTNode **stmts;
            int              stmt_count;
        } block;

        // Struct / Enum / Trait declaration
        struct {
            const char       *decl_name;
            int               decl_namelen;
            struct ASTNode  **decl_generics;
            int               decl_generic_count;
            struct ASTNode  **decl_fields;  // or variants
            int               decl_field_count;
            struct ASTNode  **decl_where;
            int               decl_where_count;
        } decl;

        // Impl block
        struct {
            struct Type      *impl_type;
            struct Type      *impl_trait;   // NULL for inherent impl
            struct ASTNode  **impl_methods;
            int               impl_method_count;
            struct ASTNode  **impl_generics;
            int               impl_generic_count;
        } impl_block;

        // Generic param
        struct {
            const char       *generic_name;
            int               generic_namelen;
            struct Type      *generic_bound;
        } generic;

        // Where clause
        struct {
            struct Type  *where_type;
            struct Type  *where_bound;
        } where_clause;

        // Pattern
        struct {
            struct ASTNode  *pat_sub;       // sub-pattern
            const char      *pat_name;      // binding name
            int              pat_namelen;
            struct ASTNode **pat_elems;     // for tuple/struct patterns
            int              pat_elem_count;
            struct ASTNode  *pat_low;       // range low
            struct ASTNode  *pat_high;      // range high
            bool             pat_ref;
            bool             pat_mut;
        } pattern;

        // Type expression
        struct {
            struct Type *type_expr;
        } type_node;

        // Attribute
        struct {
            const char      *attr_name;
            struct ASTNode **attr_args;
            int              attr_argc;
        } attr;
    };
};

// ─── AST Pool (arena allocator for AST nodes) ─────────────────────────────────
struct ASTPool {
    struct ASTNode **nodes;
    int              count;
    int              capacity;
    void            *arena;
};

// ═══════════════════════════════════════════════════════════════════════════════
// SYMBOL TABLE
// ═══════════════════════════════════════════════════════════════════════════════

typedef enum {
    SYM_VARIABLE,
    SYM_FUNCTION,
    SYM_STRUCT,
    SYM_ENUM,
    SYM_TRAIT,
    SYM_TYPE_ALIAS,
    SYM_MODULE,
    SYM_GENERIC_PARAM,
    SYM_LABEL,
    SYM_MACRO,
} SymKind;

typedef struct Symbol Symbol;
typedef struct SymTable SymTable;

struct Symbol {
    SymKind        kind;
    const char    *name;
    struct Type   *type;
    struct ASTNode *decl;
    SymTable       *child_scope;
    int             depth;
    bool            is_pub;
    bool            is_mut;
    Location        loc;
    Symbol         *next;       // hash chain
    uint32_t        hash;
};

struct SymTable {
    SymTable   *parent;
    Symbol    **buckets;
    int         bucket_count;
    int         count;
    int         depth;
    void       *arena;
};

// ═══════════════════════════════════════════════════════════════════════════════
// INTERMEDIATE REPRESENTATION
// ═══════════════════════════════════════════════════════════════════════════════

typedef enum {
    IR_NONE,
    // Terminator
    IR_RET,     IR_BR,      IR_BR_COND,  IR_UNREACHABLE,
    // Unary
    IR_NEG,     IR_NOT,     IR_SQRT,     IR_ABS,
    // Binary
    IR_ADD,     IR_SUB,     IR_MUL,      IR_DIV,
    IR_MOD,     IR_POW,     IR_AND,      IR_OR,
    IR_XOR,     IR_SHL,     IR_SHR,      IR_EQ,
    IR_NE,      IR_LT,      IR_GT,       IR_LE,
    IR_GE,
    // Memory
    IR_ALLOCA,  IR_LOAD,    IR_STORE,    IR_PTR_ADD,
    IR_MEMCPY,  IR_MEMSET,
    // Conversions
    IR_SEXT,    IR_ZEXT,    IR_TRUNC,    IR_FPEXT,
    IR_FPTRUNC, IR_FPTOUI,  IR_FPTOSI,  IR_UITOFP,
    IR_SITOFP,  IR_BITCAST, IR_PTRTOINT,IR_INTTOPTR,
    // Aggregate
    IR_INSERT,  IR_EXTRACT, IR_STRUCT_GP, IR_ARRAY_GP,
    // Call
    IR_CALL,    IR_INVOKE,
    // Phi
    IR_PHI,
    // Atomic
    IR_ATOMIC_LOAD,  IR_ATOMIC_STORE, IR_ATOMIC_CMPXCHG,
    IR_ATOMIC_RMW,   IR_FENCE,
    // Intrinsic
    IR_INTRINSIC,
} IROp;

typedef enum {
    IR_VAL_NONE,
    IR_VAL_CONST_INT,
    IR_VAL_CONST_FLOAT,
    IR_VAL_CONST_BOOL,
    IR_VAL_UNDEF,
    IR_VAL_REG,
    IR_VAL_GLOBAL,
    IR_VAL_FUNCTION,
    IR_VAL_BLOCK,
} IRValueKind;

typedef struct IRValue {
    IRValueKind kind;
    union {
        int64_t     const_int;
        double      const_float;
        bool        const_bool;
        int         reg_id;
        const char *global_name;
        const char *func_name;
        int         block_id;
    };
    struct Type *type;
} IRValue;

typedef struct IRInst {
    IROp           op;
    int            id;
    IRValue        dst;
    IRValue        operands[4];
    struct Type   *type;
    Location       loc;
    struct IRInst *next;
} IRInst;

typedef struct IRBlock {
    int             id;
    const char     *name;
    struct IRInst  *first;
    struct IRInst  *last;
    int             inst_count;
    // Predecessors / successors for CFG
    struct IRBlock **preds;
    int             pred_count;
    struct IRBlock **succs;
    int             succ_count;
    // Dominator tree
    struct IRBlock  *idom;
    struct IRBlock **dom_children;
    int              dom_child_count;
    // Loop info
    struct IRBlock  *loop_header;
    bool             is_loop;
} IRBlock;

typedef struct IRFunction {
    const char     *name;
    struct Type    *type;
    IRBlock       **blocks;
    int             block_count;
    IRBlock        *entry;
    int             reg_count;
    int             var_arg_count;
    bool            is_extern;
} IRFunction;

typedef struct IRModule {
    IRFunction   **functions;
    int             func_count;
    struct Type   **types;
    int             type_count;
    // Global variables
    struct IRGlobal *globals;
    int              global_count;
} IRModule;

// ═══════════════════════════════════════════════════════════════════════════════
// CODEGEN OPTIONS
// ═══════════════════════════════════════════════════════════════════════════════

typedef enum {
    TARGET_LLVM_IR,
    TARGET_GO,
    TARGET_PYTHON,
    TARGET_JS,
    TARGET_WASM,
    TARGET_X86_64,
    TARGET_ASM,         // Intel-syntax assembly text
    TARGET_JIT,         // Just-In-Time compilation
    TARGET_FORTH,       // Forth stack-based language
    TARGET_AARCH64,
    TARGET_RISCV64,
} TargetBackend;

typedef enum {
    OPT_NONE,
    OPT_O1,
    OPT_O2,
    OPT_O3,
    OPT_OZ,     // size
    OPT_OG,     // debug
} OptLevel;

typedef struct CompileOptions {
    TargetBackend   target;
    OptLevel        opt_level;
    const char     *output_path;
    const char     *input_path;
    const char    **include_dirs;
    int             include_dir_count;
    bool            emit_llvm;
    bool            emit_asm;
    bool            emit_object;
    bool            emit_ir;
    bool            dump_tokens;
    bool            dump_ast;
    bool            dump_type;
    bool            dump_ir;
    bool            check_only;
    bool            time_phases;
    bool            verbose;
    bool            safe_mode;       // --secure
    bool            obfuscate;
    int             jobs;           // parallel jobs
} CompileOptions;

// ═══════════════════════════════════════════════════════════════════════════════
// COMPILER CONTEXT
// ═══════════════════════════════════════════════════════════════════════════════

typedef struct Context {
    CompileOptions   options;
    CompilerPhase    phase;
    Diagnostics      diag;

    // Source files
    struct SourceFile {
        const char *path;
        const char *source;
        int         length;
    } *files;
    int file_count;

    // String interner
    struct InternStr **intern_table;
    int                intern_count;
    int                intern_capacity;

    // Type table
    TypeTable    types;

    // AST
    ASTPool      ast;

    // Symbol tables
    SymTable    *global_syms;

    // IR
    IRModule    *ir_module;

    // Arena (general purpose)
    void        *arena;
    size_t       arena_pos;
    size_t       arena_cap;

    // Timer data
    double       phase_times[PHASE_COUNT];
} Context;

// ═══════════════════════════════════════════════════════════════════════════════
// INTERNAL COMPILER ERROR (ICE) MACRO
// ═══════════════════════════════════════════════════════════════════════════════

#define ICE(msg, ...) \
    ket_ice(__FILE__, __LINE__, __func__, msg, ##__VA_ARGS__)

// ═══════════════════════════════════════════════════════════════════════════════
// API FUNCTIONS
// ═══════════════════════════════════════════════════════════════════════════════

// Context
Context *ket_context_create(const CompileOptions *opts);
void     ket_context_destroy(Context *ctx);

// Diagnostics
void ket_diag_push(Context *ctx, Severity sev, Location loc, int code,
                   const char *fmt, ...);
void ket_diag_note(Context *ctx, Location loc, const char *label);
void ket_diag_print_all(Context *ctx);
bool ket_diag_has_errors(Context *ctx);

// String interning
const char *ket_intern(Context *ctx, const char *str, int len);
const char *ket_intern_fmt(Context *ctx, const char *fmt, ...);

// Arena
void *ket_arena_alloc(Context *ctx, size_t size);
void  ket_arena_reset(Context *ctx);
void *ket_arena_alloc_aligned(Context *ctx, size_t size, int align);

// Type table
TypeTable *ket_type_table_init(void *arena);
Type      *ket_type_get(TypeTable *tt, TypeKind kind);
Type      *ket_type_array(TypeTable *tt, Type *elem, int64_t count);
Type      *ket_type_ptr(TypeTable *tt, Type *pointee);
Type      *ket_type_ref(TypeTable *tt, Type *pointee, bool mut);
Type      *ket_type_tuple(TypeTable *tt, Type **elems, int count);
Type      *ket_type_fn(TypeTable *tt, Type **params, int pcount, Type *ret,
                       bool variadic, CallingConv cc);
Type      *ket_type_struct(TypeTable *tt, const char *name, Field *fields,
                           int fcount);
Type      *ket_type_enum(TypeTable *tt, const char *name, Variant *variants,
                         int vcount);

// AST pool
ASTNode *ket_ast_new_node(Context *ctx, NodeKind kind, Span span);
ASTNode *ket_ast_new_literal_int(Context *ctx, int64_t val, Span span);
ASTNode *ket_ast_new_literal_float(Context *ctx, double val, Span span);
ASTNode *ket_ast_new_literal_str(Context *ctx, const char *s, int len, Span span);
ASTNode *ket_ast_new_ident(Context *ctx, const char *name, int len, Span span);
ASTNode *ket_ast_new_binary(Context *ctx, TokenType op, ASTNode *l, ASTNode *r, Span span);
ASTNode *ket_ast_new_unary(Context *ctx, TokenType op, ASTNode *r, Span span);

// Symbol table
SymTable *ket_sym_table_new(SymTable *parent, void *arena);
Symbol   *ket_sym_lookup(SymTable *table, const char *name);
Symbol   *ket_sym_lookup_current(SymTable *table, const char *name);
Symbol   *ket_sym_insert(SymTable *table, SymKind kind, const char *name,
                          struct Type *type, ASTNode *decl, Location loc);

// IR
IRModule     *ket_ir_new_module(Context *ctx);
IRFunction   *ket_ir_new_function(IRModule *mod, const char *name, Type *type);
IRBlock      *ket_ir_new_block(IRFunction *fn, const char *name);
IRInst       *ket_ir_add_inst(IRBlock *block, IROp op, Type *type, Location loc);
void          ket_ir_verify(IRModule *mod, Diagnostics *diag);

// Optimizer
void ket_opt_constant_folding(IRModule *mod, Diagnostics *diag);
void ket_opt_dead_code_elim(IRModule *mod, Diagnostics *diag);
void ket_opt_inlining(IRModule *mod, Diagnostics *diag);
void ket_opt_gvn(IRModule *mod, Diagnostics *diag);  // Global Value Numbering
void ket_opt_licm(IRModule *mod, Diagnostics *diag);  // Loop Invariant Code Motion
void ket_opt_run_passes(IRModule *mod, OptLevel level, Diagnostics *diag);

// Codegen
int ket_codegen_llvm(IRModule *mod, const char *path, CompileOptions *opts);
int ket_codegen_go(IRModule *mod, const char *path, CompileOptions *opts);

// ICE handler
void ket_ice(const char *file, int line, const char *func,
             const char *msg, ...)
    __attribute__((noreturn));

#endif // KETAMINE_TYPES_H
