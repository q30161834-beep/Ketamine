#include "include/closures.h"
#include "include/arena.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

ClosureInfo *closure_analyze(ASTNode *closure_node, Context *ctx) {
    (void)ctx;
    if (!closure_node || closure_node->kind != N_CLOSURE) return NULL;

    ClosureInfo *ci = (ClosureInfo*)calloc(1, sizeof(ClosureInfo));
    ci->kind = CLOSURE_FN;
    ci->params = closure_node->closure.closure_params;
    ci->param_count = closure_node->closure.param_count;
    ci->body = closure_node->closure.body;
    ci->capture_count = 0;
    ci->captures = NULL;
    ci->env_size = 0;

    // Generate unique name
    static int closure_counter = 0;
    char buf[64];
    snprintf(buf, sizeof(buf), "__closure_%d", closure_counter++);
    ci->name = strdup(buf);

    return ci;
}

ClosureKind closure_classify(ClosureInfo *ci) {
    // For now, default to FnMut (most permissive in early stages)
    (void)ci;
    return CLOSURE_FN_MUT;
}

struct Type *closure_build_env_type(ClosureInfo *ci, TypeTable *tt) {
    (void)tt;
    (void)ci;
    // Would create a struct type with fields for each captured variable
    return NULL;
}

ASTNode *closure_lower(ClosureInfo *ci, Arena *arena) {
    (void)ci;
    (void)arena;
    return NULL;
}

const char *closure_go_codegen(ClosureInfo *ci) {
    // Go supports closures natively
    static char buf[256];
    snprintf(buf, sizeof(buf), "func(%s) { /* %s */ }", ci->name, ci->name);
    return buf;
}

const char *closure_py_codegen(ClosureInfo *ci) {
    // Python supports closures natively
    static char buf[256];
    snprintf(buf, sizeof(buf), "(lambda: None) # %s", ci->name);
    return buf;
}
