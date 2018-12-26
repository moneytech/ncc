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
#include <stdlib.h>
#include <stddef.h>
#include "ncpp.h"

struct condition
{
    int                saw_true_group;
    int                in_true_group;
    int                saw_else;
    struct condition * link;
};

static struct condition * condition_stack;
static int                compiling = 1;

/* update the compiling flag based on the state of 
   the condition stack. */

static 
check_compiling()
{
    struct condition * condition;

    compiling = 1;

    for (condition = condition_stack; condition; condition = condition->link)
        if (!condition->in_true_group) compiling = 0;
}

/* create a new condition and put it on top of the stack. */

static 
condition_new()
{
    struct condition * condition;

    condition = (struct condition *) safe_malloc(sizeof(struct condition));
    condition->saw_true_group = 0;
    condition->in_true_group = 0;
    condition->saw_else = 0;
    condition->link = condition_stack;
    condition_stack = condition;
}

/* pop the most recent condition off the stack and free it. */

static 
condition_pop()
{
    struct condition * condition;

    condition = condition_stack;
    condition_stack = condition->link;
    free(condition);
}

/* determine the precedence level of a binary operator. */

#define PRECEDENCE_NONE                 0
#define PRECEDENCE_MULTIPLICATIVE       1
#define PRECEDENCE_ADDITIVE             2
#define PRECEDENCE_SHIFT                3
#define PRECEDENCE_RELATIONAL           4
#define PRECEDENCE_EQUALITY             5
#define PRECEDENCE_BIT_AND              6
#define PRECEDENCE_BIT_XOR              7
#define PRECEDENCE_BIT_OR               8

static 
token_precedence(token)
    struct token * token;
{
    switch (token->class) {
    case TOKEN_MUL:
    case TOKEN_DIV:
    case TOKEN_MOD:
        return PRECEDENCE_MULTIPLICATIVE;

    case TOKEN_PLUS:
    case TOKEN_MINUS:
        return PRECEDENCE_ADDITIVE;

    case TOKEN_SHL:
    case TOKEN_SHR:
        return PRECEDENCE_SHIFT;

    case TOKEN_GT:
    case TOKEN_GTEQ:
    case TOKEN_LT:
    case TOKEN_LTEQ:
        return PRECEDENCE_RELATIONAL;

    case TOKEN_EQEQ:
    case TOKEN_NOTEQ:
        return PRECEDENCE_EQUALITY;

    case TOKEN_AND:
        return PRECEDENCE_BIT_AND;

    case TOKEN_XOR:
        return PRECEDENCE_BIT_XOR;

    case TOKEN_OR:
        return PRECEDENCE_BIT_OR;

    default:
        return PRECEDENCE_NONE;
    }
}

#define PARSE_EVALUATE  0
#define PARSE_SKIP      1

static parse_expression();

static 
parse_unary(list, mode)
    struct list * list;
{
    if (list->first) {
        switch (list->first->class) {
        case TOKEN_PLUS:
            list_delete(list, list->first);
            parse_unary(list, mode);
            return 0;

        case TOKEN_MINUS:
            list_delete(list, list->first);
            parse_unary(list, mode);
            list->first->u.int_value = -(list->first->u.int_value);
            return 0;

        case TOKEN_TILDE:
            list_delete(list, list->first);
            parse_unary(list, mode);
            list->first->u.int_value = ~(list->first->u.int_value);
            return 0;

        case TOKEN_NOT:
            list_delete(list, list->first);
            parse_unary(list, mode);
            list->first->u.int_value = !(list->first->u.int_value);
            return 0;

        case TOKEN_LPAREN:
            list_delete(list, list->first);
            parse_expression(list, mode);

            if (!list->first->next || (list->first->next->class != TOKEN_RPAREN))
                fail("missing closing parenthesis in expression");

            list_delete(list, list->first->next);
            return 0;

        case TOKEN_NUMBER:
            token_convert_number(list->first);
        case TOKEN_INT:
        case TOKEN_UNSIGNED:
            return 0;
        }
    }

    fail("expression expected");
}

static 
parse_binary(precedence, list, mode)
    struct list * list;
{
    if (precedence == PRECEDENCE_NONE)
        return parse_unary(list, mode);

    parse_binary(precedence - 1, list, mode);
    while ((list->count >= 3) && (token_precedence(list->first->next) == precedence)) {
        struct token * left;
        struct token * right;
        struct token * operator;
        struct token * result;

        left = list->first;
        list_unlink(list, left);
        operator = list->first;
        list_unlink(list, operator);

        parse_binary(precedence - 1, list, mode);
        right = list->first;
        list_unlink(list, right);
        result = token_new(TOKEN_INT);

        if ((left->class == TOKEN_UNSIGNED) || (right->class == TOKEN_UNSIGNED)) 
            result->class = TOKEN_UNSIGNED;

        if (mode == PARSE_EVALUATE) {
            switch (operator->class) {
            case TOKEN_PLUS:
                result->u.int_value = left->u.int_value + right->u.int_value;
                break;

            case TOKEN_MINUS:
                result->u.int_value = left->u.int_value - right->u.int_value;
                break;

            case TOKEN_MUL:
                result->u.int_value = left->u.int_value * right->u.int_value;
                break;

            case TOKEN_AND:
                result->u.int_value = left->u.int_value & right->u.int_value;
                break;

            case TOKEN_OR:
                result->u.int_value = left->u.int_value | right->u.int_value;
                break;

            case TOKEN_XOR:
                result->u.int_value = left->u.int_value ^ right->u.int_value;
                break;

            case TOKEN_SHL:
                result->u.int_value = left->u.int_value << right->u.int_value;
                break;

            case TOKEN_SHR:
                if (result->class == TOKEN_UNSIGNED) 
                    result->u.int_value = left->u.unsigned_value >> right->u.int_value;
                else
                    result->u.int_value = left->u.int_value >> right->u.int_value;
                break;

            case TOKEN_MOD:
                if (right->u.int_value == 0) fail("division by zero");

                if (result->class == TOKEN_UNSIGNED) 
                    result->u.int_value = left->u.unsigned_value % right->u.int_value;
                else
                    result->u.int_value = left->u.int_value % right->u.int_value;
                break;

            case TOKEN_DIV:
                if (right->u.int_value == 0) fail("division by zero");

                if (result->class == TOKEN_UNSIGNED) 
                    result->u.int_value = left->u.unsigned_value / right->u.int_value;
                else
                    result->u.int_value = left->u.int_value / right->u.int_value;
                break;

            case TOKEN_EQEQ:
                result->class = TOKEN_INT;
                result->u.int_value = (left->u.int_value == right->u.int_value);
                break;

            case TOKEN_NOTEQ:
                result->class = TOKEN_INT;
                result->u.int_value = (left->u.int_value != right->u.int_value);
                break;

            case TOKEN_GTEQ:
                if (result->class == TOKEN_UNSIGNED) 
                    result->u.int_value = (left->u.unsigned_value >= right->u.unsigned_value);
                else
                    result->u.int_value = (left->u.int_value >= right->u.int_value);

                result->class = TOKEN_INT;
                break;
                
            case TOKEN_GT:
                if (result->class == TOKEN_UNSIGNED) 
                    result->u.int_value = (left->u.unsigned_value > right->u.unsigned_value);
                else
                    result->u.int_value = (left->u.int_value > right->u.int_value);

                result->class = TOKEN_INT;
                break;

            case TOKEN_LT:
                if (result->class == TOKEN_UNSIGNED) 
                    result->u.int_value = (left->u.unsigned_value < right->u.unsigned_value);
                else
                    result->u.int_value = (left->u.int_value < right->u.int_value);

                result->class = TOKEN_INT;
                break;

            case TOKEN_LTEQ:
                if (result->class == TOKEN_UNSIGNED) 
                    result->u.int_value = (left->u.unsigned_value <= right->u.unsigned_value);
                else
                    result->u.int_value = (left->u.int_value <= right->u.int_value);

                result->class = TOKEN_INT;
                break;
            }
        } else {
            result->u.int_value = 0;
        }

        list_insert(list, result, list->first);

        token_free(left);
        token_free(right);
        token_free(operator);
    }
}

static
parse_and(list, mode)
    struct list * list;
{
    int left;

    parse_binary(PRECEDENCE_BIT_OR, list, mode);
    while ((list->count >= 3) && (list->first->next->class == TOKEN_ANDAND)) {
        left = list->first->u.int_value;
        if ((mode == PARSE_EVALUATE) && !left) mode = PARSE_SKIP;
        list_delete(list, list->first);
        list_delete(list, list->first);
        parse_binary(PRECEDENCE_BIT_OR, list, mode);
        list->first->u.int_value = left && list->first->u.int_value;
        list->first->class = TOKEN_INT;
    }
}

static
parse_or(list, mode)
    struct list * list;
{
    int left;

    parse_and(list, mode);
    while ((list->count >= 3) && (list->first->next->class == TOKEN_OROR)) {
        left = list->first->u.int_value;
        if ((mode == PARSE_EVALUATE) && left) mode = PARSE_SKIP;
        list_delete(list, list->first);
        list_delete(list, list->first);
        parse_and(list, mode);
        list->first->u.int_value = left || list->first->u.int_value;
        list->first->class = TOKEN_INT;
    }
}

static
parse_expression(list, mode)
    struct list * list;
{   
    struct token * result;
    int            control;

    parse_or(list, mode);
    if (list->first->next && (list->first->next->class == TOKEN_QUEST)) {
        control = list->first->u.int_value;
        list_delete(list, list->first);
        list_delete(list, list->first);

        parse_expression(list, (control && (mode == PARSE_EVALUATE)) ? PARSE_EVALUATE : PARSE_SKIP);

        if (control) {
            result = list->first;
            list_unlink(list, result);
        } else 
            list_delete(list, list->first);

        if (!list->first || (list->first->class != TOKEN_COLON)) fail("expected ':'");
        list_delete(list, list->first);
        parse_expression(list, (!control && (mode == PARSE_EVALUATE)) ? PARSE_EVALUATE : PARSE_SKIP);

        if (control) {
            list_delete(list, list->first);
            list_insert(list, result, list->first);
        }
    }
}

/* evaluate an expression and returns true if the expression
   is true, or false otherwise. */

static 
expression(list)
    struct list * list;
{
    struct token * cursor;
    int            parentheses; 
    struct macro * macro;
    struct token * value;

    list_trim(list, LIST_TRIM_STRIP);

    /* evaluate all 'defined' operators */

    cursor = list->first;
    while (cursor) {
        if ((cursor->class == TOKEN_NAME) && vstring_equal_s(cursor->u.text, "defined")) {
            cursor = list_delete(list, cursor);
            parentheses = 0;
            if (cursor && (cursor->class == TOKEN_LPAREN)) {
                cursor = list_delete(list, cursor);
                parentheses++;
            }

            if (!cursor || (cursor->class != TOKEN_NAME))
                fail("illegal/missing operator to 'defined'");

            macro = macro_lookup(cursor->u.text, MACRO_LOOKUP_NORMAL);
            cursor = list_delete(list, cursor);

            if (parentheses) {
                if (!cursor || (cursor->class != TOKEN_RPAREN))
                    fail("missing closing parenthesis");

                cursor = list_delete(list, cursor);
            }

            value = token_new(TOKEN_INT);
            value->u.int_value = (macro != NULL);
            list_insert(list, value, cursor);
        } else
            cursor = cursor->next;
    }

    /* now we can do macro replacement, and then replace all remaining
       identifiers with integer value 0 */

    macro_replace(list, MACRO_REPLACE_REPEAT);
    list_trim(list, LIST_TRIM_STRIP);
    for (cursor = list->first; cursor; cursor = cursor->next) {
        if ((cursor->class == TOKEN_NAME) || (cursor->class == TOKEN_EXEMPT_NAME)) {
            cursor->class = TOKEN_INT;
            cursor->u.int_value = 0;
        }
    }

    parse_expression(list, PARSE_EVALUATE);
    if (list->count != 1) fail("trailing garbage after expression");

    return list->first->u.int_value;
}

/* when fresh lines are read from input, they are first fed through
   directive() to check for, and act on, directives. this function 
   also deletes tokens when in a region that is excluded by #if/#ifdef etc. */

directive(list)
    struct list * list;
{
    struct token *   cursor;
    struct vstring * directive_name = NULL;

    cursor = list->first;
    SKIP_SPACES(cursor);

    if (cursor && (cursor->class == TOKEN_HASH)) {
        cursor = cursor->next;
        SKIP_SPACES(cursor);
        if (cursor && (cursor->class == TOKEN_NAME)) {
            directive_name = cursor->u.text;
            cursor = cursor->next;
        }

        if (vstring_equal_s(directive_name, "include") && compiling) {
            /* #include "local_path"
               #include <system_path>
             
               if the directive doesn't match either form outright,
               macros are expanded and we check again. */

            struct vstring * path;

            list_cut(list, cursor);
            list_trim(list, LIST_TRIM_EDGES);

            if (list->count) 
                if ((list->first->class != TOKEN_STRING) && (list->first->class != TOKEN_LT))
                    macro_replace(list, MACRO_REPLACE_REPEAT);

            if (!list->count) fail("missing file name for #include");

            if (list->first->class == TOKEN_STRING) {
                path = vstring_from_literal(list->first->u.text);
                list_delete(list, list->first);
                if (list->count) fail("trailing garbage after #include directive");
                input_include(path, INPUT_INCLUDE_LOCAL);
            } else if ((list->first->class == TOKEN_LT) && (list->last->class == TOKEN_GT)) {
                list_delete(list, list->first);
                list_delete(list, list->last);
                path = list_glue(list, LIST_GLUE_RAW);
                input_include(path, INPUT_INCLUDE_SYSTEM);
                list_clear(list);
            } else
                fail("malformed #include directive");
        } else if (vstring_equal_s(directive_name, "line")) {
            /* #line <line_number> [ <path> ]
             
               care is taken not to change our idea of the location
               until we're sure there are no errors to report, for
               whatever that's worth. (there really isn't any good way 
               to report the location of errors in #line directives). */

            struct vstring * new_path = NULL;
            int              new_line_number;

            list_cut(list, cursor);
            macro_replace(list, MACRO_REPLACE_REPEAT);
            cursor = list->first;
            SKIP_SPACES(cursor);
            if (!cursor) fail("empty #line directive");

            if (cursor->class == TOKEN_NUMBER) {
                token_convert_number(cursor);
                new_line_number = cursor->u.int_value;

                if ((cursor->class == TOKEN_INT) && (new_line_number > 0)) {
                    cursor = cursor->next;
                    SKIP_SPACES(cursor);
                    if (cursor && (cursor->class == TOKEN_STRING)) {
                        new_path = vstring_from_literal(cursor->u.text);
                        cursor = cursor->next;
                    }
                }
            }

            SKIP_SPACES(cursor);
            list_cut(list, cursor);
            if (list->count) fail("malformed #line directive");
            input_stack->line_number = new_line_number - 1;
            if (new_path) input_stack->path = new_path;
        } else if (vstring_equal_s(directive_name, "pragma") && compiling) {
            /* #pragma { <pptoken> }
              
               these are passed along to the compiler, verbatim, so
               mark all identifiers exempt from macro expansion. */
               
            for (cursor = list->first; cursor; cursor = cursor->next)
                if (cursor->class == TOKEN_NAME) cursor->class = TOKEN_EXEMPT_NAME;
        } else if (vstring_equal_s(directive_name, "error") && compiling) {
            /* #error { <pptoken> } */

            struct vstring * message;

            SKIP_SPACES(cursor);
            list_cut(list, cursor);
            message = list_glue(list, LIST_GLUE_STRINGIZE);
            fail("user abort %V", message);
        } else if (vstring_equal_s(directive_name, "define") && compiling) {
            /* #define macro replacement 
               #define macro(a,b) replacement */

            struct vstring * name;
            struct list *    arguments = NULL;
            struct list *    replacement;
            struct token *   tmp;

            SKIP_SPACES(cursor);
            if (!cursor || (cursor->class != TOKEN_NAME)) fail("missing macro name");
            name = vstring_copy(cursor->u.text);
            cursor = cursor->next;

            if (cursor && (cursor->class == TOKEN_LPAREN)) {
                arguments = list_new();
                cursor = cursor->next;

                while (cursor) {
                    SKIP_SPACES(cursor);
                    if (!cursor) break;
                    if (cursor->class == TOKEN_RPAREN) break;

                    if (arguments->count) {
                        if ((cursor->class != TOKEN_COMMA) || !(cursor->next))
                            fail("syntax error (argument list)");

                        cursor = cursor->next;
                        SKIP_SPACES(cursor);
                        if (!cursor) break;
                    }

                    if (cursor->class != TOKEN_NAME) fail("identifier expected (argument list)");
                    tmp = cursor;
                    cursor = list_unlink(list, tmp);
                    list_insert(arguments, tmp, NULL);
                }

                if (!cursor || (cursor->class != TOKEN_RPAREN)) fail("unclosed macro argument list");
                cursor = cursor->next;
            }

            replacement = list_new();
            list_cut(list, cursor);
            list_move(replacement, list, -1, NULL);
            macro_define(name, arguments, replacement);
            vstring_free(name);
        } else if (vstring_equal_s(directive_name, "undef") && compiling) {
            cursor = cursor->next;
            SKIP_SPACES(cursor);

            if (!cursor) fail("missing argument to #undef");

            if (cursor->class == TOKEN_NAME) {
                macro_undef(cursor->u.text);
                cursor = cursor->next;
            }

            SKIP_SPACES(cursor);
            if (cursor) fail("malformed #undef");
            list_clear(list);
        } else if (vstring_equal_s(directive_name, "ifdef") || vstring_equal_s(directive_name, "ifndef")) {
            int found = vstring_equal_s(directive_name, "ifdef");

            condition_new();

            if (compiling) {
                cursor = cursor->next;
                SKIP_SPACES(cursor);
                if (!cursor) fail("missing argument to #ifdef/#ifndef");

                if (cursor->class == TOKEN_NAME) {
                    if (macro_lookup(cursor->u.text, MACRO_LOOKUP_NORMAL)) {
                        condition_stack->in_true_group = found;
                        condition_stack->saw_true_group = found;
                    } else {
                        condition_stack->in_true_group = !found;
                        condition_stack->saw_true_group = !found;
                    }

                    cursor = cursor->next;
                }

                SKIP_SPACES(cursor);
                if (cursor) fail("trailing garbage after #ifdef/#ifndef");
            }

            list_clear(list);
            check_compiling();
        } else if (vstring_equal_s(directive_name, "if")) {
            condition_new();

            if (compiling) {
                cursor = cursor->next;
                list_cut(list, cursor);

                if (expression(list)) {
                    condition_stack->saw_true_group = 1;
                    condition_stack->in_true_group = 1;
                }
            }
            list_clear(list);
            check_compiling();
        } else if (vstring_equal_s(directive_name, "elif")) {
            if (!condition_stack) fail("#elif without #if");
            if (condition_stack->saw_else) fail("#elif after #else");

            if (compiling) {
                cursor = cursor->next;
                list_cut(list, cursor);

                if (expression(list)) {
                    condition_stack->in_true_group = !condition_stack->saw_true_group;
                    condition_stack->saw_true_group = 1;
                } else 
                    condition_stack->in_true_group = 0;
            }
            list_clear(list);
            check_compiling();
        } else if (vstring_equal_s(directive_name, "else")) {
            cursor = cursor->next;
            SKIP_SPACES(cursor);
            if (!condition_stack) fail("#else without #if");
            if (condition_stack->saw_else) fail("duplicate #else");
            condition_stack->saw_else = 1;
            condition_stack->in_true_group = !(condition_stack->saw_true_group);
            if (cursor && compiling) fail("trailing garbage after #else");
            list_clear(list);
            check_compiling();
        } else if (vstring_equal_s(directive_name, "endif")) {
            cursor = cursor->next;
            SKIP_SPACES(cursor);
            if (!condition_stack) fail("#endif without #if");
            if (cursor && compiling) fail("trailing garbage after #endif");
            list_clear(list);
            condition_pop();
            check_compiling();
        } else if (directive_name && compiling) 
            fail("unrecognized directive '%V'", directive_name);
    }

    if (!compiling) list_clear(list);
}
