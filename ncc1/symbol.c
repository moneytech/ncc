/* Copyright (c) 2018 Charles E. Youse (charles@gnuless.org). 
   All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:

 * Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

 * Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. */

#include <string.h>
#include <stdlib.h>
#include "ncc1.h"

/* return the string table entry associated with a string 
   containing 'length' bytes at 'data', creating one if necessary. */

static struct string * string_buckets[NR_STRING_BUCKETS];

struct string *
stringize(data, length)
    char * data;
    int    length;
{
    struct string *  string;
    struct string ** stringp;
    unsigned         hash;
    int              i;

    for (i = 0, hash = 0; i < length; i++) {
        hash <<= 4;
        hash ^= (data[i] & 0xff);
    }

    i = hash % NR_STRING_BUCKETS;
    for (stringp = &(string_buckets[i]); (string = *stringp); stringp = &((*stringp)->link)) {
        if (string->length != length) continue;
        if (string->hash != hash) continue;
        if (memcmp(string->data, data, length)) continue;

        *stringp = string->link; 
        string->link = string_buckets[i];
        string_buckets[i] = string;

        return string; 
    }

    string = (struct string *) allocate(sizeof(struct string));
    string->link = string_buckets[i];
    string_buckets[i] = string;
    string->hash = hash;
    string->length = length;
    string->asm_label = 0;
    string->token = KK_IDENT;

    string->data = allocate(length + 1);
    memcpy(string->data, data, length);
    string->data[length] = 0;

    return string;
}

/* walk the string table and output all the pending string literals. */

literals()
{
    struct string * string;
    int             i;

    for (i = 0; i < NR_STRING_BUCKETS; i++) 
        for (string = string_buckets[i]; string; string = string->link)
            if (string->asm_label) {
                segment(SEGMENT_TEXT);
                output("%L:\n", string->asm_label);
                output_string(string, string->length + 1);
            }
}

/* walk the symbol table and output directives for all undefined externs */

externs1(symbol)
    struct symbol * symbol;
{
    if ((symbol->ss & S_EXTERN) && !(symbol->ss & S_DEFINED) && (symbol->ss & S_REFERENCED))
        output(".global %G\n", symbol);
}

externs()
{
    walk_symbols(SCOPE_GLOBAL, SCOPE_GLOBAL, externs1);
}

/* return the symbol table entry associated with a string literal. */

struct symbol *
string_symbol(string)
    struct string * string;
{
    struct symbol * symbol;
    struct type *   type;

    type = splice_types(new_type(T_ARRAY), new_type(T_CHAR));
    type->nr_elements = string->length + 1;
    if (string->asm_label == 0) string->asm_label = next_asm_label++;
    symbol = new_symbol(NULL, S_STATIC, type);
    symbol->i = string->asm_label;
    put_symbol(symbol, current_scope);
    return symbol;
}

/* the symbol table borrows the hash from the symbol identifiers 
   to use for its own purposes. since anonymous symbols are possible,
   there's an extra bucket just for them - putting them in the main
   table would serve no purpose, since we never find them by name. */

static struct symbol * symbol_buckets[NR_SYMBOL_BUCKETS + 1];

#define EXTRA_BUCKET        NR_SYMBOL_BUCKETS 
#define SYMBOL_BUCKET(id)   ((id) ? ((id)->hash % NR_SYMBOL_BUCKETS) : EXTRA_BUCKET)

/* allocate a new symbol. if 'type' is supplied, 
   the caller yields ownership. */

struct symbol *
new_symbol(id, ss, type)
    struct string * id;
    struct type *   type;
{
    struct symbol * symbol;

    symbol = (struct symbol *) allocate(sizeof(struct symbol));

    symbol->id = id;
    symbol->ss = ss;
    symbol->type = type;
    symbol->scope = SCOPE_NONE;
    symbol->reg = R_NONE;
    symbol->align = 0;
    symbol->target = NULL;
    symbol->link = NULL;
    symbol->i = 0;
    symbol->list = NULL;

    return symbol;
}

/* put the symbol in the symbol table at the specified scope level. */

put_symbol(symbol, scope)
    struct symbol * symbol;
{
    struct symbol ** bucketp;

    symbol->scope = scope;
    bucketp = &symbol_buckets[SYMBOL_BUCKET(symbol->id)];

    while (*bucketp && ((*bucketp)->scope > symbol->scope)) 
        bucketp = &((*bucketp)->link);

    symbol->link = *bucketp;
    *bucketp = symbol;
}

/* remove the symbol from the symbol table. */

get_symbol(symbol)
    struct symbol * symbol;
{
    struct symbol ** bucketp;

    bucketp = &symbol_buckets[SYMBOL_BUCKET(symbol->id)];
    while (*bucketp != symbol) bucketp = &((*bucketp)->link);
    *bucketp = symbol->link;
}

/* put the symbol on the end of the list. */

put_symbol_list(symbol, list)
    struct symbol *  symbol;
    struct symbol ** list;
{
    while (*list) list = &((*list)->list);
    *list = symbol;
}

/* find a symbol on the list. returns NULL if not found. */

struct symbol *
find_symbol_list(id, list)
    struct string *  id;
    struct symbol ** list;
{
    struct symbol * symbol = *list;

    while (symbol && (symbol->id != id)) symbol = symbol->list;
    return symbol;
}

/* if 'id' is a typedef'd name visible in the current scope,
   return its symbol, otherwise NULL. note we can't just look
   for S_TYPEDEF directly, because that would skip names that
   hide the typedef. */

struct symbol *
find_typedef(id)
    struct string * id;
{
    struct symbol * symbol;

    symbol = find_symbol(id, S_NORMAL, SCOPE_GLOBAL, current_scope);

    if (symbol && (symbol->ss & S_TYPEDEF)) 
        return symbol;
    else
        return NULL;
}

/* find, or create, the label symbol */

struct symbol *
find_label(id)
    struct string * id;
{
    struct symbol * symbol;

    symbol = find_symbol(id, S_LABEL, SCOPE_FUNCTION, SCOPE_FUNCTION);
    if (symbol == NULL) {
        symbol = new_symbol(id, S_LABEL, NULL);
        symbol->target = new_block();
        put_symbol(symbol, SCOPE_FUNCTION);
    }
    return symbol;
}

/* find a symbol with the given 'id' in the namespace 'ss'
   between scopes 'start' and 'end' (inclusive). returns NULL
   if nothing found. */

struct symbol *
find_symbol(id, ss, start, end)
    struct string * id;
{
    struct symbol * symbol;
    int             i;

    i = SYMBOL_BUCKET(id);
    for (symbol = symbol_buckets[i]; symbol; symbol = symbol->link) {
        if (symbol->scope < start) break;
        if (symbol->scope > end) continue;
        if (symbol->id != id) continue;
        if ((symbol->ss & ss) == 0) continue;
        return symbol;
    }

    return NULL;
}

/* find a symbol by (pseudo) register. return NULL if not found. this 
   is very slow, because the symbol table isn't indexed by register. */

struct symbol *
find_symbol_by_reg(reg)
{
    struct symbol * symbol;
    int             i;

    for (i = 0; i <= EXTRA_BUCKET; i++) 
        for (symbol = symbol_buckets[i]; symbol; symbol = symbol->link) 
            if (symbol->reg == reg) return symbol;

    return NULL;
}

/* someone's interested in the memory allocated to this symbol,
   so make sure it's allocated. */

store_symbol(symbol)
    struct symbol * symbol;
{
    if ((symbol->ss & S_BLOCK) && (symbol->i == 0)) {
        frame_offset += size_of(symbol->type);
        frame_offset = ROUND_UP(frame_offset, align_of(symbol->type));
        symbol->i = -frame_offset;
    }
}

/* return the pseudo register associated with the symbol, allocating
   one of appropriate type, if necessary. */

symbol_reg(symbol)
    struct symbol * symbol;
{
    if (symbol->reg != R_NONE) return symbol->reg;

    if (symbol->type->ts & (T_IS_INTEGRAL | T_PTR)) 
        symbol->reg = next_iregister++;
    else if (symbol->type->ts & T_IS_FLOAT)
        symbol->reg = next_fregister++;
    else error(ERROR_INTERNAL);

    return symbol->reg;
}

/* create an anonymous temporary symbol of the given type.
   these are always S_REGISTER (because the compiler uses them
   internally and will never take their addresses) and always 
   SCOPE_RETIRED (because they were never in scope to begin with).

   caller yields ownership of 'type'. */

struct symbol *
temporary_symbol(type)
    struct type * type;
{
    struct symbol * symbol;

    symbol = new_symbol(NULL, S_REGISTER, type);
    put_symbol(symbol, SCOPE_RETIRED);
    return symbol;
}

/* walk the symbol table between scopes 'start' and 'end', inclusive,
   calling f() on each one. the EXTRA_BUCKET is included in the traversal. */

walk_symbols(start, end, f)
    int f();
{
    struct symbol * symbol;
    struct symbol * link;
    int             i;

    for (i = 0; i <= EXTRA_BUCKET; i++) {
        for (symbol = symbol_buckets[i]; symbol; symbol = link) {
            link = symbol->link;
            if (symbol->scope < start) break;
            if (symbol->scope > end) continue;
            f(symbol);
        }
    }
}

/* entering a scope is trivial. */

enter_scope()
{
    ++current_scope;
    if (current_scope > SCOPE_MAX) error(ERROR_NESTING);
}

/* exiting a scope just moves all symbols in the current 
   scope into SCOPE_RETIRED, so they can't be seen. */

static
exit1(symbol)
    struct symbol * symbol;
{
    get_symbol(symbol);
    put_symbol(symbol, SCOPE_RETIRED);
}

exit_scope()
{
    walk_symbols(current_scope, SCOPE_MAX, exit1);
    --current_scope;
}

/* after a function definition is completed, call free_symbols()
   to finally clear out all the out-of-scope symbols. */

free_symbol(symbol)
    struct symbol * symbol;
{
    free_type(symbol->type);
    free(symbol);
}

static
free_symbols1(symbol)
    struct symbol * symbol;
{
    get_symbol(symbol);
    free_symbol(symbol);
}

free_symbols()
{
    walk_symbols(SCOPE_FUNCTION, SCOPE_RETIRED, free_symbols1);
}

