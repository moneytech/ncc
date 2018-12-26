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

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <sys/types.h>
#include <time.h>
#include "ncpp.h"

/* we keep the macros in a hash table, borrowing the hash value
   from the 'name' string. */

#define NR_BUCKETS 16

static struct macro * buckets[NR_BUCKETS];

/* ANSI declares a few predefined, and sometimes dynamic, macros.
   note that some predefined macros, like __STDC__, are not dealt with
   here; those differ based on the compiler invoked. the driver can
   handle those with -D directives on the command line. */

#define PREDEFINED_LINE    1
#define PREDEFINED_FILE    2
#define PREDEFINED_DATE    3
#define PREDEFINED_TIME    4
#define PREDEFINED_DEFINED 5

/* callied during initialization to create the predefined macro entries. */

macro_predefine()
{
    static char *    names[] = { "__LINE__", "__FILE__", "__DATE__", 
                                 "__TIME__", "defined" };
                                /* N.B. order is important, match PREDEFINED_* */
    struct vstring * name;
    int              i;
    struct macro *   macro;

    for (i = 0; i < (sizeof(names)/sizeof(*names)); i++) {
        name = vstring_new(names[i]);
        macro = macro_lookup(name, MACRO_LOOKUP_CREATE);
        macro->predefined = i + 1;
        vstring_free(name);
    }
}

static
macro_update(macro)
    struct macro * macro;
{
    static char      buffer[64];
    struct token *   token;
    time_t           epoch;

    switch (macro->predefined) {
        case PREDEFINED_LINE:
            if (macro->replacement) 
                list_clear(macro->replacement);
            else
                macro->replacement = list_new();

            sprintf(buffer, "%d", input_stack->line_number);
            token = token_new(TOKEN_NUMBER);
            token->u.text = vstring_new(buffer);
            list_insert(macro->replacement, token, NULL);
            break;

        case PREDEFINED_FILE:
            if (macro->replacement) 
                list_clear(macro->replacement);
            else
                macro->replacement = list_new();

            token = token_new(TOKEN_STRING);
            token->u.text = vstring_new(NULL);
            vstring_putc(token->u.text, '"');
            vstring_concat(token->u.text, input_stack->path);
            vstring_putc(token->u.text, '"');
            list_insert(macro->replacement, token, NULL);
            break;

        case PREDEFINED_TIME:
            if (macro->replacement) return 0;
            macro->replacement = list_new();
            time(&epoch);
            token = token_new(TOKEN_STRING);
            token->u.text = vstring_new(NULL);
            vstring_putc(token->u.text, '"');
#if 0
            strftime(buffer, sizeof(buffer), "%H:%M:%S", localtime(&epoch));
#else
            sprintf(buffer, "--TIME--");
#endif
            vstring_puts(token->u.text, buffer);
            vstring_putc(token->u.text, '"');
            list_insert(macro->replacement, token, NULL);
            break;

        case PREDEFINED_DATE:
            if (macro->replacement) return 0;
            macro->replacement = list_new();
            time(&epoch);
            token = token_new(TOKEN_STRING);
            token->u.text = vstring_new(NULL);
            vstring_putc(token->u.text, '"');
#if 0
            strftime(buffer, sizeof(buffer), "%b %d %Y", localtime(&epoch));
#else
            sprintf(buffer, "--TIME--");
#endif
            vstring_puts(token->u.text, buffer);
            vstring_putc(token->u.text, '"');
            list_insert(macro->replacement, token, NULL);
            break;

        case PREDEFINED_DEFINED:
            if (macro->replacement) return 0;
            macro->replacement = list_new();
            token = token_new(TOKEN_EXEMPT_NAME);
            token->u.text = vstring_new("defined");
            list_insert(macro->replacement, token, NULL);
            break;

        default:
            fail("don't do that macro yet");
    }
}

/* look up a name in the macro table, and return its entry.
   if it doesn't exist then NULL is returned, unless 'mode'
   is MACRO_LOOKUP_CREATE, in which case a new entry is made.  
   (the caller can tell the entry is new if 'replacement' is NULL). */

struct macro *
macro_lookup(name, mode)
    struct vstring * name;
{
    struct macro * macro;
    int            bucket;

    bucket = vstring_hash(name) % NR_BUCKETS;
    for (macro = buckets[bucket]; macro; macro = macro->link) 
        if (vstring_equal(macro->name, name)) {
            if (macro->predefined) macro_update(macro);
            return macro;
        }

    if (mode != MACRO_LOOKUP_CREATE) return NULL;

    macro = (struct macro *) safe_malloc(sizeof(struct macro));
    macro->name = vstring_copy(name);
    macro->replacement = NULL;
    macro->arguments = NULL;
    macro->predefined = 0;
    macro->link = buckets[bucket];
    buckets[bucket] = macro;

    return macro;
}

/* put a macro in the table. if this is a redefinition, make sure the new
   definition matches the old, or it's an error. 'arguments' is a list of 
   TOKEN_NAME tokens that give the names of the macro arguments. this may be
   empty (indicating a function-like macro with no arguments) or it may be 
   NULL, indicating an object-like macro. the caller yields ownership of the 
   'replacement' and 'argument' (if applicable) token lists. */

macro_define(name, arguments, replacement)
    struct vstring * name;
    struct list    * arguments;
    struct list    * replacement;
{
    struct macro * macro;
    int            argument_no;
    struct token * argument;
    struct token * cursor;

    /* normalize the replacement list: 
       1. remove leading or trailing spaces
       2. consolidate consecutive internal whitespaces into one space
       3. convert TOKEN_HASHHASH (inert token) to the TOKEN_PASTE operator
       4. remove whitespace between TOKEN_HASH and TOKEN_PASTE operators 
          and their operands (this simplifies replacement later) */

    list_trim(replacement, LIST_TRIM_EDGES | LIST_TRIM_FOLD);
    cursor = replacement->first;
    while (cursor) {
        if ((cursor->class == TOKEN_HASH) || (cursor->class == TOKEN_HASHHASH))
            while (cursor->next && (cursor->next->class == TOKEN_SPACE))
                list_delete(replacement, cursor->next);

        if (cursor->class == TOKEN_HASHHASH) {
            cursor->class = TOKEN_PASTE;

            while (cursor->previous && (cursor->previous->class == TOKEN_SPACE))
                list_delete(replacement, cursor->previous);

        }
        cursor = cursor->next;
    }

    /* replace all occurrences of argument identifiers in 
       the replacement list with a properly-numbered TOKEN_ARG */

    if (arguments) {
        argument_no = 0;
        argument = arguments->first;

        while (argument) {
            for (cursor = replacement->first; cursor; cursor = cursor->next)
                if ((cursor->class == TOKEN_NAME) && (vstring_equal(cursor->u.text, argument->u.text))) {
                    cursor->class = TOKEN_ARG;
                    cursor->u.argument_no = argument_no;
                }

            argument = argument->next;
            argument_no++;
        }
    }

    /* now we're ready to put it in the table */

    macro = macro_lookup(name, MACRO_LOOKUP_CREATE);
    if (macro->predefined) fail("macro name is reserved");

    if (macro->replacement == NULL) {
        macro->arguments = arguments;
        macro->replacement = replacement;
    } else {
        /* the macro is already defined. compare the number of arguments and 
           the now-normalized replacement list to determine if the redefinition is legal. */

        if (list_equal(arguments, macro->arguments) && list_equal(macro->replacement, replacement)) {
            if (arguments) list_free(arguments);
            list_free(replacement);
        } else
            fail("incompatible redefinition of '%V'", name);
    }
}

/* remove a macro from the hash table. */

macro_undef(name)
    struct vstring * name;
{
    struct macro *  macro;
    struct macro ** ptr;
    int             bucket;

    bucket = vstring_hash(name) % NR_BUCKETS;
    ptr = &(buckets[bucket]);

    while (*ptr) {
        if (vstring_equal((*ptr)->name, name)) {
            macro = *ptr;
            if (macro->predefined) fail("can't do that to predefined macro");
            *ptr = macro->link;
            vstring_free(macro->name);
            list_free(macro->replacement);
            if (macro->arguments) list_free(macro->arguments);
            free(macro);
            return 0;
        }
        ptr = &((*ptr)->link);
    }
}

/* take a string of the form <macro_name>[=<replacement>] (from
   a command-line option) and define it in the macro table. */

macro_option(option)
    char * option;
{
    struct list *    replacement;
    struct vstring * vstring;
    struct vstring * name;
    struct token *   token;

    replacement = list_new();

    vstring = vstring_new(option);
    tokenize(vstring, replacement);
    vstring_free(vstring);
    list_trim(replacement, LIST_TRIM_TRAILING);

    if (replacement->count == 0) fail("missing macro name");
    if (replacement->first->class != TOKEN_NAME) fail("invalid macro name '%T'", replacement->first);
    name = vstring_copy(replacement->first->u.text);
    list_delete(replacement, replacement->first);

    if (replacement->count == 0) {
        token = token_new(TOKEN_NUMBER);
        token->u.text = vstring_new("1");
        list_insert(replacement, token, NULL);
    } else {
        if (replacement->first->class != TOKEN_EQ) fail("malformed macro option");
        list_delete(replacement, replacement->first);
    }

    macro_define(name, NULL, replacement);
    vstring_free(name);
}

/* 'source' begins with a left parenthesis; destructively parse exactly nr_arguments
   actual arguments to a macro invocation into arguments[]. */

static 
actual_arguments(arguments, nr_arguments, source)
    struct list ** arguments;
    struct list * source;
{
    int argument_no = 0;
    int parentheses;

    if (source->first->class != TOKEN_LPAREN) fail("whoops");
    list_delete(source, source->first); 

    while (argument_no < nr_arguments) {
        parentheses = 0;

        while (source->first) {
            if (parentheses == 0) {
                if (source->first->class == TOKEN_RPAREN) break;
                if (source->first->class == TOKEN_COMMA) break;
            }

            if (source->first->class == TOKEN_LPAREN) parentheses++;
            if (source->first->class == TOKEN_RPAREN) parentheses--;
            list_move(arguments[argument_no], source, 1, NULL);
        }

        list_trim(arguments[argument_no++], LIST_TRIM_EDGES);
        if (!source->first || (source->first->class == TOKEN_RPAREN)) break;
        if (argument_no < nr_arguments) list_delete(source, source->first); 
    }
    
    list_trim(source, LIST_TRIM_EDGES);
    if (argument_no != nr_arguments) fail("too few macro arguments");
    if (!source->first) fail("unterminated macro argument list");
    if (source->first->class != TOKEN_RPAREN) fail("too many macro arguments");
    list_delete(source, source->first);
}

/* if the source begins with a macro that needs replacement, then replace it.
   returns non-zero if replacement took place, otherwise zero. */

static 
replace1(destination, source)
    struct list * destination;
    struct list * source;
{
    struct macro *   macro;
    struct list **   arguments;
    struct token *   cursor;
    struct token *   token;
    int              argument_no;

    /* if the first element not a macro name [followed by an opening
       parenthesis, if function-like], then bounce */

    if (source->first->class != TOKEN_NAME) return 0;
    macro = macro_lookup(source->first->u.text, MACRO_LOOKUP_NORMAL);
    if (!macro) return 0;

    /* gobble up the macro name and collect its arguments, if any. */

    arguments = NULL;

    if (!macro->arguments) 
        list_delete(source, source->first);
    else {
        cursor = source->first->next;
        SKIP_SPACES(cursor);
        if (!cursor || (cursor->class != TOKEN_LPAREN)) return 0;
        list_cut(source, cursor);

        if (macro->arguments->count) {
            arguments = (struct list **) safe_malloc(sizeof(struct list *) * macro->arguments->count);
            for (argument_no = 0; argument_no < macro->arguments->count; argument_no++)
                arguments[argument_no] = list_new();
        }

        actual_arguments(arguments, macro->arguments->count, source);
    } 

    /* in the first pass, the replacement list is copied to the output, 
       expanding arguments (and submitting them for replacement, too, if not 
       the subject of preprocessor operators). stringize happens here. */

    for (cursor = macro->replacement->first; cursor; cursor = cursor->next) {
        if (cursor->class == TOKEN_HASH) {
            struct vstring * vstring;

            cursor = cursor->next;
            if (!cursor) fail("stringize (#) missing operand");
            if (cursor->class != TOKEN_ARG) fail("invalid operand to stringize (#)");
            vstring = list_glue(arguments[cursor->u.argument_no], LIST_GLUE_STRINGIZE);
            token = token_new(TOKEN_STRING);
            token->u.text = vstring;
            list_insert(destination, token, NULL);
        } else if (cursor->class == TOKEN_ARG) {
            struct list * argument;

            argument = list_copy(arguments[cursor->u.argument_no]);

            if ((!cursor->next || (cursor->next->class != TOKEN_PASTE))
                    && (!destination->last || (destination->last->class != TOKEN_PASTE)))
                macro_replace(argument, MACRO_REPLACE_REPEAT);

            list_move(destination, argument, -1, NULL);
            list_free(argument);
        } else {
            list_insert(destination, token_copy(cursor), NULL);
        }
    }

    /* second pass: process token pasting operators. */

    for (cursor = destination->first; cursor; cursor = cursor->next) {
        if (cursor->class == TOKEN_PASTE) {
            if (!cursor->previous || !cursor->next) fail("missing operand(s) to paste (##)");
            token = token_paste(cursor->previous, cursor->next);
            list_delete(destination, cursor->previous);
            list_delete(destination, cursor->next);
            list_insert(destination, token, cursor);
            list_delete(destination, cursor);
            cursor = token;
        }
    }

    /* free up the arguments, if any. */

    if (arguments) {
        for (argument_no = 0; argument_no < macro->arguments->count; argument_no++)
            list_free(arguments[argument_no]);

        free(arguments);
    }

    /* mark all occurrences of this macro's name in 
       the replacement list as ineligible for replacement */

    for (cursor = destination->first; cursor; cursor = cursor->next) 
        if ((cursor->class == TOKEN_NAME) && vstring_equal(cursor->u.text, macro->name))
            cursor->class = TOKEN_EXEMPT_NAME;

    return 1;
}

/* replace macros in 'list'. if mode is MODE_REPLACE_REPEAT, 
   make multiple passes over the list until no more macro 
   replacements are made. (the main loop calls this with 
   MODE_REPLACE_ONCE because it rescans the input itself). */

macro_replace(list, mode)
    struct list * list;
{
    struct list *  source;
    int            changes;
    
    source = list_new();

    do {
        changes = 0;
        list_move(source, list, -1, NULL);

        while (source->first) {
            if (replace1(list, source)) 
                changes++;
            else
                list_move(list, source, 1, NULL);
        }
    } while (changes && (mode == MACRO_REPLACE_REPEAT));

    list_free(source);
}

