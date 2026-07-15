#include "include/macros.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_MACRO_ARGS 64

MacroRule *macro_register(Context *ctx, const char *name, Location loc) {
    (void)ctx;
    MacroRule *rule = (MacroRule*)calloc(1, sizeof(MacroRule));
    rule->name = name;
    rule->loc = loc;
    rule->is_hygienic = true;
    return rule;
}

bool macro_is_fragment_spec(Token tok) {
    if (tok.type != TK_IDENT) return false;
    const char *s = tok.start;
    int len = tok.length;

    if (len == 4 && memcmp(s, "expr", 4) == 0) return true;
    if (len == 2 && memcmp(s, "ty", 2) == 0) return true;
    if (len == 5 && memcmp(s, "ident", 5) == 0) return true;
    if (len == 7 && memcmp(s, "literal", 7) == 0) return true;
    if (len == 3 && memcmp(s, "pat", 3) == 0) return true;
    if (len == 4 && memcmp(s, "stmt", 4) == 0) return true;
    if (len == 5 && memcmp(s, "block", 5) == 0) return true;
    if (len == 4 && memcmp(s, "meta", 4) == 0) return true;
    if (len == 2 && memcmp(s, "tt", 2) == 0) return true;

    return false;
}

bool macro_match(MacroRule *rule, Token *input, int input_count,
                 MatchPattern *out_match) {
    (void)rule;
    (void)input;
    (void)input_count;
    (void)out_match;
    // Simplified pattern matching — real implementation would use
    // recursive descent on token trees
    return false;
}

Token *macro_expand(MacroRule *rule, MatchPattern *match,
                    int *out_count, Arena *arena) {
    (void)rule;
    (void)match;
    (void)arena;
    *out_count = 0;
    return NULL;
}

Token *macro_expand_all(Token *tokens, int count, int *out_count,
                        Context *ctx, Arena *arena) {
    (void)ctx;
    // Pass through — no macro expansion yet
    Token *result = (Token*)arena_alloc_zero(arena, (size_t)count * sizeof(Token));
    memcpy(result, tokens, (size_t)count * sizeof(Token));
    *out_count = count;
    return result;
}
