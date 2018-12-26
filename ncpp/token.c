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
#include <memory.h>
#include <errno.h>
#include <ctype.h>
#include <limits.h>
#include "ncpp.h"

/* typical allocation/free functions. care must be taken with the
   u.text field: setting this field grants ownership of the
   vstring to the token. */

struct token * 
token_new(class)
{
    struct token * token;

    token = (struct token *) safe_malloc(sizeof(struct token));

    token->class = class;
    token->previous = NULL;
    token->next = NULL;
    token->u.text = NULL;
}

token_free(token)
    struct token * token;
{
    switch (token->class) {
    case TOKEN_NAME:
    case TOKEN_EXEMPT_NAME:
    case TOKEN_STRING:
    case TOKEN_CHAR:
    case TOKEN_NUMBER:
        if (token->u.text) vstring_free(token->u.text);
    }

    free(token);
}

/* return a new copy of a token. */

struct token *
token_copy(source)
    struct token * source;
{
    struct token * token;

    token = (struct token *) safe_malloc(sizeof(struct token));
    memcpy(token, source, sizeof(*token));

    switch (token->class) {
    case TOKEN_NAME:
    case TOKEN_EXEMPT_NAME:
    case TOKEN_STRING:
    case TOKEN_CHAR:
    case TOKEN_NUMBER:
        token->u.text = vstring_copy(token->u.text);
    }

    return token;
}

/* returns true if these two tokens are identical. */

token_equal(token1, token2)
    struct token * token1;
    struct token * token2;
{
    if (token1->class != token2->class) return 0;

    switch (token1->class) {
    case TOKEN_ARG:
        return (token1->u.argument_no == token2->u.argument_no);

    case TOKEN_UNKNOWN:
        return (token1->u.ascii == token2->u.ascii);

    case TOKEN_NAME:
    case TOKEN_EXEMPT_NAME:
    case TOKEN_STRING:
    case TOKEN_CHAR:
    case TOKEN_NUMBER:
        return vstring_equal(token1->u.text, token2->u.text);

    default:
        return 1;
    }
}

/* convert a TOKEN_NUMBER to an TOKEN_INT or TOKEN_UNSIGNED, as appropriate */

token_convert_number(token)
    struct token * token;
{
    unsigned long value;
    char *        end_ptr;

    errno = 0;
    value = strtoul(token->u.text->data, &end_ptr, 0);
    if (errno) fail("integral constant out of range");

    token->class = TOKEN_INT;
    if (value > LONG_MAX) token->class = TOKEN_UNSIGNED;

    if (toupper(*end_ptr) == 'L') {
        end_ptr++;
        if (toupper(*end_ptr) == 'U') {
            end_ptr++;
            token->class = TOKEN_UNSIGNED;
        }
    } else if (toupper(*end_ptr) == 'U') {
        token->class = TOKEN_UNSIGNED;
        if (toupper(*end_ptr) == 'L') end_ptr++;
    }

    if (*end_ptr) fail("malformed integral constant");
    vstring_free(token->u.text);
    token->u.unsigned_value = value;
}

/* this collection of code handles reproducing the textual representation 
   of tokens. in addition to doing this for producing the output file, 
   this is needed to evaluate the stringize (#) and pasting (##) operators. */

static char *token_text[] = /* indices keyed to enumeration in token.h! */
{
    /*  0 */ NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    /* 10 */ ">", "<", ">=", "<=", "<<", "<<=", ">>", ">>=", "=", "==", 
    /* 20 */ "!=", "+", "+=", "++", "-", "-=", "--", "->", "(", ")", 
    /* 30 */ "[", "]", "{", "}", ",", ".", "?", ":", ";", "|", 
    /* 40 */ "||", "|=", "&", "&&", "&=", "*", "*=", "%", "%=", "^", 
    /* 50 */ "^=", "!", "#", "##", "/", "/=", "~", "...", "##"
};

static file_helper(file, c) FILE *file; { fputc(c, file); }
static vstring_helper(vstring, c) struct vstring * vstring; { vstring_putc(vstring, c); }

#define TOKEN_PRINT_RAW    0     /* print unmodified */
#define TOKEN_PRINT_ESCAPE 1     /* print escaped (for stringize) */

token_print_internal(token, mode, helper, helper_data)
    struct token   * token;
    int          ( * helper )();
    char           * helper_data;
{
    char * text;

    switch (token->class) {
    case TOKEN_SPACE:
    case TOKEN_UNKNOWN:
        helper(helper_data, token->u.ascii);
        return 0;

    case TOKEN_NAME:
    case TOKEN_EXEMPT_NAME:
    case TOKEN_STRING:
    case TOKEN_CHAR:
    case TOKEN_NUMBER:
        text = token->u.text->data;
        break;

    default:
        text = token_text[token->class];
    }

    while (*text) {
        if ((mode == TOKEN_PRINT_ESCAPE) && ((*text == '\\') || (*text == '\"')))
            helper(helper_data, '\\');

        helper(helper_data, *text);
        text++;
    }
}

/* token_print() prints an unmodified token to a file. 
   (this is the only print function exported publicly.) */

token_print(token, file)
    struct token * token;
    FILE         * file;
{
    token_print_internal(token, TOKEN_PRINT_RAW, file_helper, file);
}

/* allocate and initialize a new list */

struct list *
list_new()
{
    struct list * list;

    list = (struct list *) safe_malloc(sizeof(struct list));
    list->first = NULL;
    list->last = NULL;
    list->count = 0;

    return list;
}

/* insert a token into 'list' before 'before' (NULL means end of list).
   list takes ownership of the token. */

list_insert(list, token, before)
    struct list  * list;
    struct token * token;
    struct token * before;
{
    token->next = before;

    if (token->next) {
        token->previous = token->next->previous;
        token->next->previous = token;
    } else {
        token->previous = list->last;
        list->last = token;
    }

    if (token->previous) 
        token->previous->next = token;
    else
        list->first = token;

    list->count++;
}

/* remove a token from the list. the caller takes ownership of the token.
   for convenience, returns the token that followed the unlinked token. */

struct token *
list_unlink(list, token)
    struct list  * list;
    struct token * token;
{
    if (token->previous)
        token->previous->next = token->next;
    else
        list->first = token->next;

    if (token->next)
        token->next->previous = token->previous;
    else
        list->last = token->previous;

    list->count--;
    return token->next;
}

/* unlink a token from the list and destroy it. like list_unlink,
   returns the token that used to follow the deleted token. */

struct token *
list_delete(list, token)
    struct list  * list;
    struct token * token;
{
    struct token * next;

    next = list_unlink(list, token);
    token_free(token);
    return next;
}

/* move at most 'count' tokens from the source list to the destination
   list before 'before'. if count is -1, then all tokens are moved. */

list_move(destination, source, count, before)
    struct list  * destination;
    struct list  * source;
    struct token * before;
{
    struct token * token;

    while (source->first && count) {
        token = source->first;
        list_unlink(source, token);
        list_insert(destination, token, before);
        before = token->next;
        if (count > 0) count--;
    }
}

/* return a new copy of a list. */

struct list *
list_copy(source)
    struct list * source;
{
    struct list *  destination;
    struct token * cursor;

    destination = list_new();

    for (cursor = source->first; cursor; cursor = cursor->next)
        list_insert(destination, token_copy(cursor), NULL);

    return destination;
}

/* remove all tokens from the front of the list before 'token' */

list_cut(list, token)
    struct list * list;
    struct token * token;
{
    while (list->first != token) list_delete(list, list->first);
}

/* massage whitespace found inside a token list. the flags
   may be combined and are as follows:

   LIST_TRIM_LEADING: remove leading whitespaces
   LIST_TRIM_TRAILING: remove trailing whitespaces
   LIST_TRIM_FOLD: fold consecutive whitespaces into one 
   LIST_TRIM_STRIP: remove all spaces, wherever they are */

list_trim(list, mode)
    struct list * list;
{
    struct token * cursor;
    struct token * space;

    if (mode & LIST_TRIM_LEADING) 
        while (list->count && (list->first->class == TOKEN_SPACE))
            list_delete(list, list->first);

    if (mode & LIST_TRIM_TRAILING) 
        while (list->count && (list->last->class == TOKEN_SPACE))
            list_delete(list, list->last);

    if (mode & (LIST_TRIM_FOLD | LIST_TRIM_STRIP)) {
        cursor = list->first; 
        while (cursor) {
            if (cursor->class == TOKEN_SPACE) {
                while (cursor && (cursor->class == TOKEN_SPACE)) 
                    cursor = list_delete(list, cursor);

                if (mode & LIST_TRIM_FOLD) {
                    space = token_new(TOKEN_SPACE);
                    space->u.ascii = ' ';
                    list_insert(list, space, cursor);
                }
            } else
                cursor = cursor->next;
        }
    }
}

/* empty the contents of the list. */

list_clear(list)
    struct list * list;
{
    while (list->count) list_delete(list, list->first);
}

/* free a list (and all of its contents) */

list_free(list)
    struct list * list;
{
    list_clear(list);
    free(list);
}

/* returns non-zero if the lists contain the same exact tokens.
   either list1 or list2 (or both) may be NULL. */

list_equal(list1, list2)
    struct list * list1;
    struct list * list2;
{
    struct token * token1;
    struct token * token2;

    if (list1 == list2) return 1;
    if (!list1 || !list2) return 0;

    if (list1->count != list2->count) return 0;

    token1 = list1->first;
    token2 = list2->first;

    while (token1 && token2) {
        if (!token_equal(token1, token2)) return 0;
        token1 = token1->next;
        token2 = token2->next;
    }
    
    return 1;
}

/* paste the textual representation of all the tokens in the list
   together and return the result as a vstring. if 'mode' is 
   LIST_GLUE_STRINGIZE, then the result is returned as a string literal,
   quoted, with space between the tokens. otherwise, LIST_GLUE_RAW
   returns an unquoted string with spaces removed. */

struct vstring *
list_glue(list, mode)
    struct list * list;
{
    struct vstring * vstring;
    struct token   * token;

    vstring = vstring_new(NULL);
    if (mode == LIST_GLUE_STRINGIZE) vstring_putc(vstring, '\"');

    token = list->first;
    while (token) {
        if (mode == LIST_GLUE_RAW) {
            if (token->class != TOKEN_SPACE) 
                token_print_internal(token, TOKEN_PRINT_RAW, vstring_helper, vstring);

            token = token->next;
        } else {
            if (token->class == TOKEN_SPACE) {
                SKIP_SPACES(token);
                vstring_putc(vstring, ' ');
            } else {
                token_print_internal(token, TOKEN_PRINT_ESCAPE, vstring_helper, vstring);
                token = token->next;
            }
        }
    }

    if (mode == LIST_GLUE_STRINGIZE) vstring_putc(vstring, '\"');
    return vstring;
}

/* return the token that results from pasting 'left' and 'right' together. */

struct token *
token_paste(left, right)
    struct token * left;
    struct token * right;
{
    struct vstring * vstring;
    struct list *    list;
    struct token *   token;

    list = list_new();
    list_insert(list, token_copy(left), NULL);
    list_insert(list, token_copy(right), NULL);
    vstring = list_glue(list, LIST_GLUE_RAW);
    list_clear(list);
    tokenize(vstring, list);
    list_trim(list, LIST_TRIM_EDGES);
    if (list->count != 1) fail("result of paste (%T ## %T) is not a token", left, right);
    token = list->first;
    list_unlink(list, token);
    list_free(list);
    return token;
}

/* convert a string to a collection of tokens - i.e., perform lexical analysis. */

tokenize(vstring, list)
    struct vstring * vstring;
    struct list    * list;
{
    struct token * token;
    char *         cp = vstring->data;
    int            i = 0;

    while (i < vstring->length) {
        if (isspace(cp[i])) { 
            token = token_new(TOKEN_SPACE);
            token->u.ascii = cp[i++];
            list_insert(list, token, NULL);
            continue;
        }

        if (isdigit(cp[i]) || ((cp[i] == '.') && isdigit(cp[i+1]))) {
            token = token_new(TOKEN_NUMBER);
            token->u.text = vstring_new(NULL);

            while (isalnum(cp[i]) || (cp[i] == '.') || (cp[i] == '_')) {
                vstring_putc(token->u.text, cp[i++]);
                if ((toupper(cp[i - 1]) == 'E') && ((cp[i] == '+') || (cp[i] == '-'))) 
                    vstring_putc(token->u.text, cp[i++]);
            }

            list_insert(list, token, NULL);
            continue;
        }

        if (isalpha(cp[i]) || (cp[i] == '_')) {
            token = token_new(TOKEN_NAME);
            token->u.text = vstring_new(NULL);

            while (isalnum(cp[i]) || (cp[i] == '_')) 
                vstring_putc(token->u.text, cp[i++]);

            list_insert(list, token, NULL);
            continue;
        }

        if ((cp[i] == '\'') || (cp[i] == '\"')) {
            int terminator;
            int last_was_backslash;

            if (cp[i] == '\'') 
                token = token_new(TOKEN_CHAR);
            else
                token = token_new(TOKEN_STRING);

            token->u.text = vstring_new(NULL);
            terminator = cp[i];
            last_was_backslash = 1; /* fib so first iteration won't exit */

            for (;;) {
                if (i > vstring->length) {
                    if (terminator == '\'')
                        fail("unterminated character constant");
                    else
                        fail("unterminated string literal");
                }

                vstring_putc(token->u.text, cp[i++]);
                if ((cp[i-1] == terminator) && !last_was_backslash) break;

                if (last_was_backslash) 
                    last_was_backslash = 0;
                else 
                    last_was_backslash = (cp[i-1] == '\\');
            }
            
            list_insert(list, token, NULL);
            continue;
        }

        switch (cp[i]) {
        case '.':
            i++;
            if ((cp[i] == '.') && (cp[i+1] == '.')) {
                i += 2;
                token = token_new(TOKEN_ELLIPSIS);
            } else
                token = token_new(TOKEN_DOT);

            break;

        case '<':
            i++;
            if (cp[i] == '<') {
                i++;
                if (cp[i] == '=') {
                    i++;
                    token = token_new(TOKEN_SHLEQ);
                } else {
                    token = token_new(TOKEN_SHL);
                }
            } else if (cp[i] == '=') {
                i++;
                token = token_new(TOKEN_LTEQ);
            } else
                token = token_new(TOKEN_LT);

            break;

        case '>':
            i++;
            if (cp[i] == '>') {
                i++;
                if (cp[i] == '=') {
                    i++;
                    token = token_new(TOKEN_SHREQ);
                } else {
                    token = token_new(TOKEN_SHR);
                }
            } else if (cp[i] == '=') {
                i++;
                token = token_new(TOKEN_GTEQ);
            } else
                token = token_new(TOKEN_GT);

            break;

        case '*':
            i++;
            if (cp[i] == '=') {
                i++;
                token = token_new(TOKEN_MULEQ);
            } else
                token = token_new(TOKEN_MUL);

            break;

        case '/':
            i++;
            if (cp[i] == '=') {
                i++;
                token = token_new(TOKEN_DIVEQ);
            } else
                token = token_new(TOKEN_DIV);

            break;

        case '%':
            i++;
            if (cp[i] == '=') {
                i++;
                token = token_new(TOKEN_MODEQ);
            } else
                token = token_new(TOKEN_MOD);

            break;

        case '&':
            i++;
            if (cp[i] == '&') {
                i++;
                token = token_new(TOKEN_ANDAND);
            } else if (cp[i] == '=') {
                i++;
                token = token_new(TOKEN_ANDEQ);
            } else
                token = token_new(TOKEN_AND);

            break;

        case '|':
            i++;
            if (cp[i] == '|') {
                i++;
                token = token_new(TOKEN_OROR);
            } else if (cp[i] == '=') {
                i++;
                token = token_new(TOKEN_OREQ);
            } else
                token = token_new(TOKEN_OR);

            break;

        case '^':
            i++;
            if (cp[i] == '=') {
                i++;
                token = token_new(TOKEN_XOREQ);
            } else
                token = token_new(TOKEN_XOR);

            break;

        case '-':
            i++;

            if (cp[i] == '>') {
                i++;
                token = token_new(TOKEN_ARROW);
            } else if (cp[i] == '=') {
                i++;
                token = token_new(TOKEN_MINUSEQ);
            } else if (cp[i] == '-') {
                i++;
                token = token_new(TOKEN_DEC);
            } else
                token = token_new(TOKEN_MINUS);

            break;

        case '+':
            i++;

            if (cp[i] == '=') {
                i++;
                token = token_new(TOKEN_PLUSEQ);
            } else if (cp[i] == '+') {
                i++;
                token = token_new(TOKEN_INC);
            } else
                token = token_new(TOKEN_PLUS);

            break;

        case '=':
            i++;

            if (cp[i] == '=') {
                i++;
                token = token_new(TOKEN_EQEQ);
            } else
                token = token_new(TOKEN_EQ);

            break;

        case '#':
            i++;

            if (cp[i] == '#') {
                i++;
                token = token_new(TOKEN_HASHHASH);
            } else
                token = token_new(TOKEN_HASH);

            break;

        case '!':
            i++;

            if (cp[i] == '=') {
                i++;
                token = token_new(TOKEN_NOTEQ);
            } else
                token = token_new(TOKEN_NOT);

            break;

        case '?':
            i++;
            token = token_new(TOKEN_QUEST);
            break;

        case ':':
            i++;
            token = token_new(TOKEN_COLON);
            break;

        case ',':
            i++;
            token = token_new(TOKEN_COMMA);
            break;

        case ';':
            i++;
            token = token_new(TOKEN_SEMI);
            break;

        case '(':
            i++;
            token = token_new(TOKEN_LPAREN);
            break;
        
        case ')':
            i++;
            token = token_new(TOKEN_RPAREN);
            break;

        case '[':
            i++;
            token = token_new(TOKEN_LBRACK);
            break;

        case ']':
            i++;
            token = token_new(TOKEN_RBRACK);
            break;

        case '{':
            i++;
            token = token_new(TOKEN_LBRACE);
            break;

        case '}':
            i++;
            token = token_new(TOKEN_RBRACE);
            break;

        case '~':
            i++;
            token = token_new(TOKEN_TILDE);
            break;

        default: 
            token = token_new(TOKEN_UNKNOWN);
            token->u.ascii = cp[i++];
        }

        list_insert(list, token, NULL);
    }

    token = token_new(TOKEN_SPACE);
    token->u.ascii = ' ';
    list_insert(list, token, NULL);
}

