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

/* handle struct/union type specifiers. note that, unlike K&R, 
   struct tags are distinct from union tags, and each struct/union
   constitutes an independent namespace for its members. */

static 
declare_member(ss, id, type, tag)
    struct string * id;
    struct type *   type;
    struct symbol * tag;
{
    int             align_bytes = align_of(type);
    int             type_bits = size_of(type) * BITS;
    long            offset_bits; /* long to catch overflows */
    struct symbol * member;
    int             member_bits;

    if (ss) error(ERROR_SCLASS);

    if (tag->ss & S_UNION)
        offset_bits = 0;
    else
        offset_bits = tag->i;

    tag->align = MAX(tag->align, align_bytes);
    if (id && find_symbol_list(id, &(tag->list))) error(ERROR_REMEMBER);

    if (type->ts & T_FIELD) {
        member_bits = T_GET_SIZE(type->ts);

        if (    (member_bits == 0) 
                || (ROUND_DOWN(offset_bits, type_bits) 
                    != ROUND_DOWN(offset_bits + member_bits - 1, type_bits)))
        {
            offset_bits = ROUND_UP(offset_bits, type_bits);
        }
            
        T_SET_SHIFT(type->ts, offset_bits % type_bits);
    } else {
        offset_bits = ROUND_UP(offset_bits, align_bytes * BITS);
        member_bits = type_bits;
    }

    if (id) {
        member = new_symbol(id, S_MEMBER, type);
        member->i = ROUND_DOWN(offset_bits / BITS, align_bytes);
        put_symbol(member, current_scope);
        put_symbol_list(member, &(tag->list));
    } else
        free_type(type);

    if (tag->ss & S_STRUCT) {
        offset_bits += member_bits;
        if (offset_bits > MAX_SIZE) error(ERROR_TYPESIZE);
        tag->i = offset_bits;
    } else
        tag->i = MAX(tag->i, member_bits);

    return 0;
}

static struct type *
struct_or_union()
{
    struct symbol * tag;
    int             ss;
    struct string * id;
    struct type *   type;

    ss = (token.kk == KK_UNION) ? S_UNION : S_STRUCT;
    lex();

    if (token.kk == KK_IDENT) {
        id = token.u.text;
        lex();
        if (token.kk == KK_LBRACE) 
            tag = find_symbol(id, ss, current_scope, current_scope);
        else
            tag = find_symbol(id, ss, SCOPE_GLOBAL, current_scope);
    } else {
        id = NULL;
        tag = NULL;
        expect(KK_LBRACE);
    }

    if (!tag) {
        tag = new_symbol(id, ss, NULL);
        put_symbol(tag, current_scope);
    }

    if (token.kk == KK_LBRACE) {
        lex();
        if (tag->ss & S_DEFINED) error(ERROR_TAGREDEF);
        declarations(declare_member, DECLARATIONS_FIELDS, tag);
        match(KK_RBRACE);
        if (tag->i == 0) error(ERROR_EMPTY);
        tag->i = ROUND_UP(tag->i, tag->align * BITS);
        tag->i /= BITS;
        tag->ss |= S_DEFINED;
    }
    
    type = new_type(T_TAG);
    type->tag = tag;
    return type;
}

/* parse type specifier. if 'ss' is not NULL, then also allow
   a storage-class specifier. if no type specifiers at all 
   are encountered, return NULL. 
   
   this function is perhaps a bit too picky about the order of 
   the specifiers - K&R is actually unclear on this point. err 
   on the side of simplicity. who writes 'int long extern' anyway? */

static struct type *
type_specifier(ss)
    int * ss;
{
    struct symbol * symbol;

    if (ss) {
        *ss = S_NONE;

        switch (token.kk) {
        case KK_AUTO:       *ss = S_AUTO; break;
        case KK_REGISTER:   *ss = S_REGISTER; break;
        case KK_EXTERN:     *ss = S_EXTERN; break;
        case KK_STATIC:     *ss = S_STATIC; break;
        case KK_TYPEDEF:    *ss = S_TYPEDEF; break;
        }

        if (*ss != S_NONE) lex();
    }

    switch (token.kk) {
    case KK_STRUCT:
    case KK_UNION:
        return struct_or_union();

    case KK_IDENT:
        symbol = find_typedef(token.u.text);
        if (symbol) {
            lex();
            return copy_type(symbol->type);
        }
        break;

    case KK_UNSIGNED:
        lex();
        if (token.kk == KK_SHORT) {
            lex();
            if (token.kk == KK_INT) lex();
            return new_type(T_USHORT);
        } else if (token.kk == KK_LONG) {
            lex();
            if (token.kk == KK_INT) lex();
            return new_type(T_ULONG);
        } else if (token.kk == KK_CHAR) {
            lex();
            return new_type(T_UCHAR);
        }
        if (token.kk == KK_INT) lex();
        return new_type(T_UINT);
    
    case KK_SHORT:
        lex();
        if (token.kk == KK_INT) lex();
        return new_type(T_SHORT);

    case KK_LONG:
        lex();
        if (token.kk == KK_FLOAT) {
            lex();
            return new_type(T_LFLOAT);
        }
        if (token.kk == KK_INT) lex();
        return new_type(T_LONG);

    case KK_INT: lex(); return new_type(T_INT);
    case KK_FLOAT: lex(); return new_type(T_FLOAT);
    case KK_CHAR: lex(); return new_type(T_CHAR);
    }

    if (!(ss && *ss)) 
        return NULL;
    else
        return new_type(T_INT);
}

/* these three functions parse a declarator - declarator() is the entry.

   '*id' will be set to the identifier declared, unless 'id' is NULL, in
   which case an identifier will be prohibited (for abstract declarators).

   if 'args' is not NULL, then formal arguments will be permitted. they are
   placed in the symbol table as S_ARG at SCOPE_FUNCTION, linked together in a 
   list. '*args' will point to the first symbol of that list. 

   this scheme is fragile and an abuse of the symbol table, but it works because:
   1. arguments are only allowed in external definitions (i.e., SCOPE_GLOBAL), 
   2. the caller will immediately enter SCOPE_FUNCTION after the declarator is read. */

static struct type * direct_declarator();

static struct type *
declarator(id, args)
    struct string ** id;
    struct symbol ** args;
{
    struct type * type;

    if (token.kk == KK_STAR) {
        lex();
        type = splice_types(declarator(id, args), new_type(T_PTR));
    } else
        type = direct_declarator(id, args);

    return type;
}

static struct type *
formal_arguments(args)
    struct symbol ** args;
{
    struct symbol * symbol;

    match(KK_LPAREN);

    if (token.kk == KK_IDENT) {
        if (!args) error(ERROR_NOARGS);

        for (;;) {
            expect(KK_IDENT);
            symbol = new_symbol(token.u.text, S_ARG, NULL);
            put_symbol(symbol, SCOPE_FUNCTION);
            put_symbol_list(symbol, args);
            lex();

            if (token.kk == KK_COMMA)
                lex();
            else
                break;
        }
    }

    match(KK_RPAREN);
    return new_type(T_FUNC);
}

static struct type *
direct_declarator(id, args)
    struct string ** id;
    struct symbol ** args;
{
    struct type * type = NULL;
    struct type * array_type;

    if (token.kk == KK_LPAREN) {
        if (peek(NULL) != KK_RPAREN) {
            lex();
            type = declarator(id, args, NULL);
            match(KK_RPAREN);
        }
    } else if (token.kk == KK_IDENT) {
        if (!id) error(ERROR_ABSTRACT);
        *id = token.u.text;
        lex();
    } else if (id) error(ERROR_MISSING);

    for (;;) {
        if (token.kk == KK_LBRACK) {
            lex();
            array_type = new_type(T_ARRAY);
            if (token.kk != KK_RBRACK) {
                array_type->nr_elements = constant_expression();
                if (array_type->nr_elements == 0) error(ERROR_ILLARRAY);
            }
            match(KK_RBRACK);
            type = splice_types(type, array_type);
        } else if (token.kk == KK_LPAREN) {
            type = splice_types(type, formal_arguments(type ? NULL : args));
        } else break;
    }

    return type;
}

/* parse an abstract type and return the resultant type.
   abstract types only appear in K&R C in casts and sizeof(). */

struct type *
abstract_type()
{
    struct type * type;

    type = type_specifier(NULL);
    if (type == NULL) error(ERROR_ABSTRACT);
    type = splice_types(declarator(NULL, NULL), type);
    validate_type(type);
    return type;
}

/* process a set of declarations. after each declarator is read, the declare()
   function is invoked with as declare(ss, id, type, ptr) where:

   struct string * id   is the identifier declared,
   int ss               is the explicit storage class (or S_NONE),
   struct type * type   is the type declared (ownership given to declare()),
   
   and 'ptr' is either struct symbol *, when mode is DECLARATIONS_ARGS,
   giving the head of the formal argument list in the symbol table. otherwise
   it's char * 'data', just copied from the call to declarations().

   the flags are:
   DECLARATIONS_ARGS    allow formal arguments (i.e., allow function definitions)
   DECLARATIONS_INT     in the absence of any storage class or type specifier, assume 'int'
   DECLARATIONS_FIELDS  allow bit field types

   the first two are used only for external definitions (at file scope), and the last
   for struct/union member declarations. all other callers set flags to 0.
 */

declarations(declare, flags, data)
    int    declare();
    char * data;
{
    struct type *   base;
    struct type *   type;
    int             ss;
    struct string * id;
    struct symbol * args;
    int             first;
    int             bits;

    while (token.kk != KK_NONE) {
        first = 1;
        base = type_specifier(&ss);

        if (base == NULL) {
            if (flags & DECLARATIONS_INTS)
                base = new_type(T_INT);
            else
                break;
        }

        if (token.kk != KK_SEMI) {
            for (;;) {
                args = NULL;

                if (token.kk == KK_COLON) {
                    id = NULL;
                    type = copy_type(base);
                } else {
                    type = declarator(&id, ((flags & DECLARATIONS_ARGS) && first) ? &args : NULL);
                    type = splice_types(type, copy_type(base));
                }

                if (token.kk == KK_COLON) {
                    if (!(flags & DECLARATIONS_FIELDS)) error(ERROR_ILLFIELD);
                    if (!(type->ts & T_IS_INTEGRAL)) error(ERROR_FIELDTY);
                    lex();
                    bits = constant_expression();
                    if ((bits < 0) || (bits > (size_of(type) * BITS))) error(ERROR_FIELDSZ);
                    if ((bits == 0) && id) error(ERROR_FIELDSZ);
                    type->ts |= T_FIELD;
                    T_SET_SIZE(type->ts, bits);
                }

                validate_type(type);
                first = 0;

                if (declare(ss, id, type, (flags & DECLARATIONS_ARGS) ? (char *) args : data)) 
                    goto was_definition;

                if (token.kk == KK_COMMA)
                    lex();
                else
                    break;
            }
        }

        match(KK_SEMI);

        was_definition:
        free_type(base);
    }
}

/* parse the top-of-block declarations that are local in scope. */

static
declare_local(ss, id, type)
    struct string * id;
    struct type *   type;
{
    struct symbol * symbol;

    if (ss & S_EXTERN) error(ERROR_SCLASS);
    if (ss == S_NONE) ss = S_LOCAL;
    if (type->ts & T_FUNC) error(ERROR_NOFUNC);

    symbol = find_symbol(id, S_NORMAL, current_scope, current_scope);
    if (symbol) error(ERROR_REDECL);
    symbol = new_symbol(id, ss, type);
    put_symbol(symbol, current_scope);
    initializer(symbol, ss);

    return 0;
}

local_declarations()
{
    declarations(declare_local, 0);
}

/* parse a function definition. this actually encapsulates and drives
   the entire code-generation process. */

static
declare_argument(ss, id, type, args)
    struct string * id;
    struct type *   type;
    struct symbol * args;
{
    struct symbol * symbol;

    symbol = find_symbol_list(id, &args);
    if (symbol == NULL) error(ERROR_NOTARG);
    if (symbol->type) error(ERROR_REDECL);
    symbol->type = argument_type(type);

    switch (ss) 
    {
    case S_AUTO:
    case S_REGISTER:  
        symbol->ss = ss;
        break;
    case S_NONE:
        symbol->ss = S_LOCAL;
        break;
    default:
        error(ERROR_SCLASS);
    }

    return 0;
}

static 
function_definition(symbol, args)
    struct symbol * symbol;
    struct symbol * args;
{
    if (symbol->ss & S_DEFINED) error(ERROR_DUPDEF);

    current_function = symbol;
    frame_offset = FRAME_ARGUMENTS;
    declarations(declare_argument, 0, args);

    for (symbol = args; symbol; symbol = symbol->list) {
        if (symbol->type == NULL) {
            symbol->type = new_type(T_INT);
            symbol->ss = S_LOCAL;
        }

        symbol->i = frame_offset;
        frame_offset += size_of(symbol->type);
        frame_offset = ROUND_UP(frame_offset, FRAME_ALIGN);
    }

    frame_offset = 0;
    setup_blocks();
    compound();         /* will enter_scope() to capture the arguments */
    optimize();
    output_function();
    free_blocks();
    free_symbols();
    current_function->ss |= S_DEFINED;
    current_function = NULL;
}

/* translation_unit() is the goal symbol of the parser. a C
   source file boils down to a list of external definitions. */

static
declare_global(ss, id, type, args)
    struct string * id;
    struct type   * type;
    struct symbol * args;
{
    struct symbol * symbol;
    int             effective_ss;

    if (ss & (S_AUTO | S_REGISTER)) error(ERROR_SCLASS);
    effective_ss = (ss == S_NONE) ? S_EXTERN : ss;

    symbol = find_symbol(id, S_NORMAL, SCOPE_GLOBAL, SCOPE_GLOBAL);

    if (symbol) {
        if (symbol->ss & effective_ss & (S_EXTERN | S_STATIC)) {
            check_types(symbol->type, type, CHECK_TYPES_COMPOSE);
            free_type(type);
        } else
            error(ERROR_REDECL);
    } else {
        symbol = new_symbol(id, effective_ss, type);
        put_symbol(symbol, SCOPE_GLOBAL);
    }

    if (symbol->type->ts & T_FUNC) {
        if (args || ((token.kk != KK_COMMA) && (token.kk != KK_SEMI))) {
            function_definition(symbol, args);
            return 1;
        }
    } else
        initializer(symbol, ss);

    return 0;
}

translation_unit()
{
    lex();  
    declarations(declare_global, DECLARATIONS_INTS | DECLARATIONS_ARGS);
    expect(KK_NONE);
}
