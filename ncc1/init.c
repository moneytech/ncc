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

#include "ncc1.h"

/* convert a floating-point E_CON into an E_MEM node referencing
   an anonymous, static symbol in the text segment. */

struct tree *
float_literal(tree)
    struct tree * tree;
{
    struct symbol * symbol;

    symbol = new_symbol(NULL, S_STATIC, copy_type(tree->type));
    symbol->i = next_asm_label++;
    put_symbol(symbol, SCOPE_RETIRED);
    segment(SEGMENT_TEXT);
    output("%G: %s %O\n", symbol, (tree->type->ts & T_LFLOAT) ? ".qword" : ".dword", tree);
    free_tree(tree);
    return memory_tree(symbol);
}

/* this function tracks state to achieve bit granularity in initializer output. */

static 
initialize_bits(i, n)
    long i;
{
    static char buf;
    static char pos;

    while (n--) {
        buf >>= 1;

        if (i & 1) 
            buf |= 0x80;
        else
            buf &= 0x7F;

        i >>= 1;
        pos++;
        if ((pos % 8) == 0) output(" .byte %d\n", buf & 255);
    }
}

static initialize();

/* read one scalar value and output it to the current 
   position in the data segment. 'type' is NOT consumed. */

static
initialize_scalar(type)
    struct type * type;
{
    struct symbol * symbol;
    struct tree *   tree;

    /* fake an assignment to a fake symbol of the right type, then discard
       everything but the right side of the assignment expression. */

    symbol = new_symbol(NULL, S_AUTO, copy_type(type));
    tree = symbol_tree(symbol);
    tree = assignment_expression(tree);
    decap_tree(tree, NULL, NULL, &tree, NULL);
    tree = generate(tree, GOAL_VALUE, NULL);
    free_symbol(symbol);

    if ((tree->op != E_CON) && (tree->op != E_IMM)) error(ERROR_BADINIT);

    /* static/extern address have RIP set - lose it */

    if (tree->op == E_IMM) {
        tree->u.mi.rip = 0;
        if ((tree->u.mi.b != R_NONE) || (tree->u.mi.i != R_NONE)) error(ERROR_BADINIT);
    }

    if (type->ts & T_FIELD) {
        if (tree->op != E_CON) error(ERROR_BADINIT);
        initialize_bits(tree->u.con.i, T_GET_SIZE(type->ts));
    } else {
        if (type->ts & T_IS_BYTE) output(" .byte ");
        if (type->ts & T_IS_WORD) output(" .word ");
        if (type->ts & T_IS_DWORD) output(" .dword ");
        if (type->ts & T_IS_QWORD) output(" .qword ");
        output("%O\n", tree);
    }

    free_tree(tree);
}

static
initialize_array(type)
    struct type * type;
{
    struct type * element_type;
    int           nr_elements = 0;

    element_type = type->next;

    if ((element_type->ts & T_IS_CHAR) && (token.kk == KK_STRLIT)) {
        nr_elements = token.u.text->length;
        if (nr_elements != type->nr_elements) nr_elements++;
        output_string(token.u.text, nr_elements);
        lex();
    } else {
        match(KK_LBRACE);
        while (token.kk != KK_RBRACE) {
            initialize(element_type);
            nr_elements++;

            if (token.kk == KK_COMMA) {
                lex();
                prohibit(KK_RBRACE);
            } else
                break;
        }
        match(KK_RBRACE);
    }

    if (type->nr_elements == 0) type->nr_elements = nr_elements;
    if ((type->nr_elements == 0) || (nr_elements > type->nr_elements)) error(ERROR_BADINIT);

    if (nr_elements < type->nr_elements) 
        output(" .fill %d,0\n", (type->nr_elements - nr_elements) * size_of(element_type));
}

static 
initialize_struct(type)
    struct type * type;
{
    struct symbol * member;
    int             offset_bits = 0;
    int             adjust_bits;

    match(KK_LBRACE);
    member = type->tag->list;
    if (type->tag->ss & S_UNION) error(ERROR_BADINIT);

    while (token.kk != KK_RBRACE) {
        if (member == NULL) error(ERROR_BADINIT);
        adjust_bits = (member->i * BITS) + T_GET_SHIFT(member->type->ts) - offset_bits;
        initialize_bits(0, adjust_bits % 8);
        if (adjust_bits / BITS) output(" .fill %d,0\n", adjust_bits / 8);
        offset_bits += adjust_bits;
        initialize(member->type);

        if (member->type->ts & T_FIELD) 
            offset_bits += T_GET_SIZE(member->type->ts);
        else
            offset_bits += size_of(member->type) * 8;

        if (token.kk == KK_COMMA) {
            lex();
            prohibit(KK_RBRACE);
        } else
            break;

        member = member->list;
    }

    match(KK_RBRACE);
    adjust_bits = (size_of(type) * BITS) - offset_bits;
    initialize_bits(0, adjust_bits % BITS);
    if (adjust_bits / BITS) output(" .fill %d,0\n", adjust_bits / 8);
}

static
initialize(type)
    struct type * type;
{
    if (type->ts & T_IS_SCALAR)
        initialize_scalar(type);
    else if (type->ts & T_ARRAY)
        initialize_array(type);
    else if (type->ts & T_TAG)
        initialize_struct(type);
    else
        error(ERROR_INTERNAL);
}

/* just declared the 'symbol' with the explicit storage class 'ss'.
   (we only care about 'ss' to distinguish between explicit and 
   implicit 'extern'). the job of initializer() is to process an 
   initializer, if present, or reserve uninitialized storage instead. */

initializer(symbol, ss)
    struct symbol * symbol;
{
    struct tree *  tree;
    struct block * saved_block;

    if (symbol->ss & S_BLOCK) size_of(symbol->type);

    if ((symbol->ss & (S_STATIC | S_EXTERN)) && !(ss & S_EXTERN)) {
        if (symbol->ss & S_DEFINED) error(ERROR_DUPDEF);
        if (symbol->ss & S_STATIC) symbol->i = next_asm_label++;
        if (symbol->ss & S_EXTERN) output(".global %G\n", symbol);
        symbol->ss |= S_DEFINED;

        if (token.kk == KK_EQ) {
            lex();
            saved_block = current_block;    /* don't allow code generation */
            current_block = NULL;

            segment(SEGMENT_DATA);
            output(".align %d\n", align_of(symbol->type));
            output("%G:", symbol);
            initialize(symbol->type);
            symbol->ss |= S_DEFINED;
            current_block = saved_block;
        } else
            output(".bss %G,%d,%d\n", symbol, size_of(symbol->type), align_of(symbol->type));
    } else {
        if (token.kk == KK_EQ) {
            lex();
            if (symbol->ss & S_TYPEDEF) error(ERROR_BADINIT);
            if (ss & S_EXTERN) error(ERROR_BADINIT);
            if (!(symbol->type->ts & T_IS_SCALAR)) error(ERROR_BADINIT);
            tree = symbol_tree(symbol);
            tree = assignment_expression(tree);
            generate(tree, GOAL_EFFECT, NULL);
        }
    }
}
