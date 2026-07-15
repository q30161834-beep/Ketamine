#include "include/types.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// ═══════════════════════════════════════════════════════════════════════════════
// SYMBOL TABLE — Scope-aware name resolution with hash tables
// ═══════════════════════════════════════════════════════════════════════════════

#define SYM_BUCKETS 4096

static uint32_t hash_str(const char *s, int len) {
    uint32_t h = 0x811C9DC5;
    for (int i = 0; i < len; i++) {
        h = (h ^ (unsigned char)s[i]) * 0x01000193;
    }
    return h;
}

SymTable *ket_sym_table_new(SymTable *parent, void *arena) {
    SymTable *t = (SymTable*)arena_alloc_zero(arena, sizeof(SymTable));
    t->parent = parent;
    t->buckets = (Symbol**)arena_alloc_zero(arena, sizeof(Symbol*) * SYM_BUCKETS);
    t->bucket_count = SYM_BUCKETS;
    t->count = 0;
    t->depth = parent ? parent->depth + 1 : 0;
    t->arena = arena;
    return t;
}

Symbol *ket_sym_lookup(SymTable *table, const char *name) {
    int len = (int)strlen(name);
    uint32_t h = hash_str(name, len);
    int bucket = h % table->bucket_count;

    // Search current scope
    Symbol *sym = table->buckets[bucket];
    while (sym) {
        if (sym->hash == h && strcmp(sym->name, name) == 0) {
            return sym;
        }
        sym = sym->next;
    }

    // Search parent scopes
    if (table->parent) {
        return ket_sym_lookup(table->parent, name);
    }

    return NULL;
}

Symbol *ket_sym_lookup_current(SymTable *table, const char *name) {
    int len = (int)strlen(name);
    uint32_t h = hash_str(name, len);
    int bucket = h % table->bucket_count;

    Symbol *sym = table->buckets[bucket];
    while (sym) {
        if (sym->hash == h && strcmp(sym->name, name) == 0) {
            return sym;
        }
        sym = sym->next;
    }

    return NULL;
}

Symbol *ket_sym_insert(SymTable *table, SymKind kind, const char *name,
                        struct Type *type, ASTNode *decl, Location loc) {
    // Check for duplicate
    Symbol *existing = ket_sym_lookup_current(table, name);
    if (existing) {
        return existing;
    }

    int len = (int)strlen(name);
    uint32_t h = hash_str(name, len);
    int bucket = h % table->bucket_count;

    Symbol *sym = (Symbol*)arena_alloc_zero(table->arena, sizeof(Symbol));
    sym->kind = kind;
    sym->name = name;
    sym->type = type;
    sym->decl = decl;
    sym->loc = loc;
    sym->depth = table->depth;
    sym->hash = h;
    sym->is_pub = false;
    sym->is_mut = false;
    sym->child_scope = NULL;
    sym->next = table->buckets[bucket];
    table->buckets[bucket] = sym;
    table->count++;

    return sym;
}

// ─── Type Table Implementation ────────────────────────────────────────────────

TypeTable *ket_type_table_init(void *arena) {
    TypeTable *tt = (TypeTable*)arena_alloc_zero(arena, sizeof(TypeTable));
    tt->arena = arena;
    tt->types = NULL;
    tt->count = 0;
    tt->capacity = 0;

    // Create built-in types
    #define MK_TYPE(var, kind) do { \
        tt->var = (Type*)arena_alloc_zero(arena, sizeof(Type)); \
        tt->var->kind = kind; \
        tt->var->size = 0; \
        tt->var->align = 0; \
    } while(0)

    MK_TYPE(ty_never,  TY_NEVER);  tt->ty_never->size  = 0;  tt->ty_never->align  = 0;
    MK_TYPE(ty_void,   TY_VOID);   tt->ty_void->size   = 0;  tt->ty_void->align   = 0;
    MK_TYPE(ty_bool,   TY_BOOL);   tt->ty_bool->size   = 1;  tt->ty_bool->align   = 1;
    MK_TYPE(ty_byte,   TY_BYTE);   tt->ty_byte->size   = 1;  tt->ty_byte->align   = 1;
    MK_TYPE(ty_char,   TY_CHAR);   tt->ty_char->size   = 4;  tt->ty_char->align   = 4;
    MK_TYPE(ty_i8,     TY_I8);     tt->ty_i8->size      = 1;  tt->ty_i8->align     = 1;  tt->ty_i8->bit_width = 8;
    MK_TYPE(ty_i16,    TY_I16);    tt->ty_i16->size     = 2;  tt->ty_i16->align    = 2;  tt->ty_i16->bit_width = 16;
    MK_TYPE(ty_i32,    TY_I32);    tt->ty_i32->size     = 4;  tt->ty_i32->align    = 4;  tt->ty_i32->bit_width = 32;
    MK_TYPE(ty_i64,    TY_I64);    tt->ty_i64->size     = 8;  tt->ty_i64->align    = 8;  tt->ty_i64->bit_width = 64;
    MK_TYPE(ty_i128,   TY_I128);   tt->ty_i128->size    = 16; tt->ty_i128->align   = 16; tt->ty_i128->bit_width = 128;
    MK_TYPE(ty_u8,     TY_U8);     tt->ty_u8->size      = 1;  tt->ty_u8->align     = 1;  tt->ty_u8->bit_width = 8;  tt->ty_u8->flags = TF_UNSIGNED;
    MK_TYPE(ty_u16,    TY_U16);    tt->ty_u16->size     = 2;  tt->ty_u16->align    = 2;  tt->ty_u16->bit_width = 16; tt->ty_u16->flags = TF_UNSIGNED;
    MK_TYPE(ty_u32,    TY_U32);    tt->ty_u32->size     = 4;  tt->ty_u32->align    = 4;  tt->ty_u32->bit_width = 32; tt->ty_u32->flags = TF_UNSIGNED;
    MK_TYPE(ty_u64,    TY_U64);    tt->ty_u64->size     = 8;  tt->ty_u64->align    = 8;  tt->ty_u64->bit_width = 64; tt->ty_u64->flags = TF_UNSIGNED;
    MK_TYPE(ty_u128,   TY_U128);   tt->ty_u128->size    = 16; tt->ty_u128->align   = 16; tt->ty_u128->bit_width = 128; tt->ty_u128->flags = TF_UNSIGNED;
    MK_TYPE(ty_f32,    TY_F32);    tt->ty_f32->size     = 4;  tt->ty_f32->align    = 4;  tt->ty_f32->bit_width = 32;
    MK_TYPE(ty_f64,    TY_F64);    tt->ty_f64->size     = 8;  tt->ty_f64->align    = 8;  tt->ty_f64->bit_width = 64;
    MK_TYPE(ty_str,    TY_STR);    tt->ty_str->size     = 8;  tt->ty_str->align    = 8;  // pointer
    MK_TYPE(ty_infer,  TY_INFER);  tt->ty_infer->kind   = TY_INFER;
    MK_TYPE(ty_error,  TY_ERROR);  tt->ty_error->kind   = TY_ERROR;

    #undef MK_TYPE

    return tt;
}

Type *ket_type_get(TypeTable *tt, TypeKind kind) {
    switch (kind) {
        case TY_NEVER:  return tt->ty_never;
        case TY_VOID:   return tt->ty_void;
        case TY_BOOL:   return tt->ty_bool;
        case TY_BYTE:   return tt->ty_byte;
        case TY_CHAR:   return tt->ty_char;
        case TY_I8:     return tt->ty_i8;
        case TY_I16:    return tt->ty_i16;
        case TY_I32:    return tt->ty_i32;
        case TY_I64:    return tt->ty_i64;
        case TY_I128:   return tt->ty_i128;
        case TY_U8:     return tt->ty_u8;
        case TY_U16:    return tt->ty_u16;
        case TY_U32:    return tt->ty_u32;
        case TY_U64:    return tt->ty_u64;
        case TY_U128:   return tt->ty_u128;
        case TY_F32:    return tt->ty_f32;
        case TY_F64:    return tt->ty_f64;
        case TY_F128:   return tt->ty_f128 ? tt->ty_f128 : tt->ty_f64;
        case TY_STR:    return tt->ty_str;
        case TY_INFER:  return tt->ty_infer;
        case TY_ERROR:  return tt->ty_error;
        default: {
            // Allocate user-defined type
            Type *t = (Type*)arena_alloc_zero(tt->arena, sizeof(Type));
            t->kind = kind;
            return t;
        }
    }
}

Type *ket_type_array(TypeTable *tt, Type *elem, int64_t count) {
    Type *t = (Type*)arena_alloc_zero(tt->arena, sizeof(Type));
    t->kind = TY_ARRAY;
    t->array.elem = elem;
    t->array.count = count;
    if (count > 0 && elem) {
        t->size = (int)(count * elem->size);
        t->align = elem->align;
    } else {
        t->size = 0;
        t->align = elem ? elem->align : 1;
    }
    return t;
}

Type *ket_type_ptr(TypeTable *tt, Type *pointee) {
    Type *t = (Type*)arena_alloc_zero(tt->arena, sizeof(Type));
    t->kind = TY_PTR;
    t->ptr.pointee = pointee;
    t->size = sizeof(void*);
    t->align = sizeof(void*);
    return t;
}

Type *ket_type_ref(TypeTable *tt, Type *pointee, bool mut) {
    Type *t = (Type*)arena_alloc_zero(tt->arena, sizeof(Type));
    t->kind = mut ? TY_MUT_REF : TY_REF;
    t->ptr.pointee = pointee;
    t->size = sizeof(void*);
    t->align = sizeof(void*);
    t->flags = mut ? TF_NONE : TF_CONST;
    return t;
}

Type *ket_type_tuple(TypeTable *tt, Type **elems, int count) {
    Type *t = (Type*)arena_alloc_zero(tt->arena, sizeof(Type));
    t->kind = TY_TUPLE;
    t->tuple.elems = elems;
    t->tuple.elem_count = count;
    // Calculate size/align
    int size = 0;
    int align = 1;
    for (int i = 0; i < count; i++) {
        if (elems[i]->align > align) align = elems[i]->align;
        // Align field
        size = (size + elems[i]->align - 1) & ~(elems[i]->align - 1);
        size += elems[i]->size;
    }
    size = (size + align - 1) & ~(align - 1);
    t->size = size;
    t->align = align;
    return t;
}

Type *ket_type_fn(TypeTable *tt, Type **params, int pcount, Type *ret,
                  bool variadic, CallingConv cc) {
    Type *t = (Type*)arena_alloc_zero(tt->arena, sizeof(Type));
    t->kind = TY_FN;
    t->fn.param_types = params;
    t->fn.param_count = pcount;
    t->fn.return_type = ret ? ret : tt->ty_void;
    t->fn.is_variadic = variadic;
    t->fn.calling_conv = cc;
    t->size = sizeof(void*); // function pointer
    t->align = sizeof(void*);
    return t;
}

Type *ket_type_struct(TypeTable *tt, const char *name, Field *fields, int fcount) {
    Type *t = (Type*)arena_alloc_zero(tt->arena, sizeof(Type));
    t->kind = TY_STRUCT;
    t->struct_type.name = name;
    t->struct_type.fields = fields;
    t->struct_type.field_count = fcount;

    // Calculate layout
    int size = 0;
    int align = 1;
    for (int i = 0; i < fcount; i++) {
        if (fields[i].type) {
            if (fields[i].type->align > align) align = fields[i].type->align;
            int off = (size + fields[i].type->align - 1) & ~(fields[i].type->align - 1);
            fields[i].offset = off;
            size = off + fields[i].type->size;
        }
    }
    size = (size + align - 1) & ~(align - 1);
    t->size = size;
    t->align = align;
    return t;
}

Type *ket_type_enum(TypeTable *tt, const char *name, Variant *variants, int vcount) {
    Type *t = (Type*)arena_alloc_zero(tt->arena, sizeof(Type));
    t->kind = TY_ENUM;
    t->enum_type.name = name;
    t->enum_type.variants = variants;
    t->enum_type.variant_count = vcount;
    t->enum_type.discriminant = tt->ty_i64;

    // Size = max variant size + discriminant
    int max_size = 8; // discriminant
    int align = 8;
    for (int i = 0; i < vcount; i++) {
        int vsize = 0;
        for (int j = 0; j < variants[i].payload_count; j++) {
            if (variants[i].payload_types[j]) {
                vsize += variants[i].payload_types[j]->size;
                if (variants[i].payload_types[j]->align > align)
                    align = variants[i].payload_types[j]->align;
            }
        }
        if (vsize > max_size) max_size = vsize;
    }
    t->size = 8 + max_size; // tag + payload
    t->align = align;
    return t;
}
