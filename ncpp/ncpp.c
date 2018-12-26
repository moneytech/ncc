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
#include <stdarg.h>
#include "ncpp.h"

struct vstring *   output_path;
FILE *             output_file;

/* specialized printf()-like output, used by fail() and out().

   the recognized format specifiers are:

        %d %s like printf()
        %V    struct vstring * 
        %T    struct token *

   other specifiers are just ignored. */

static 
print(file, fmt, args)
    FILE    * file;
    char    * fmt;
    va_list   args;
{
    struct vstring * vstring;

    while (*fmt) {
        if (*fmt != '%') {
            fputc(*fmt, file);
        } else {
            fmt++;
            switch (*fmt) {
            case 's':
                fprintf(file, "%s", va_arg(args, char *));
                break;

            case 'd':
                fprintf(file, "%d", va_arg(args, int));
                break;

            case 'V':
                vstring = va_arg(args, struct vstring *);
                if (vstring->length) 
                    fprintf(file, "%s", vstring->data);
                else
                    fprintf(file, "[empty]");
                break;

            case 'T':
                token_print(va_arg(args, struct token *), file);
                break;
            }
        }
        fmt++;
    }
}

/* invoke print() on output_file. */

#ifdef __STDC__
void
out(char * fmt, ...)
#else
out(fmt)
    char * fmt;
#endif
{
    va_list args;

    va_start(args, fmt);
    print(output_file, fmt, args);
    va_end(args);
}

/* report an error to the user, clean up, and exit.
   format specifiers and arguments are processed by print(), above. */

#ifdef __STDC__
void
fail(char * fmt, ...)
#else
fail(fmt)
    char * fmt;
#endif
{
    va_list args;

    if (input_stack) {
        fprintf(stderr, "'%s'", input_stack->path->data);
        if (input_stack->line_number) fprintf(stderr, " (%d)", input_stack->line_number);
        fprintf(stderr, ": ");
    }

    fprintf(stderr, "ERROR: ");
    va_start(args, fmt);
    print(stderr, fmt, args);
    va_end(args);
    fputc('\n', stderr);

    if (output_file) {
        fclose(output_file);
        unlink(output_path->data);
    }

    exit(1);
}

/* a simple wrapper to use instead of malloc() */

char *
safe_malloc(sz)
{
    char * p = malloc(sz);

    if (p == NULL) fail("out of memory");

    return p;
}

/* synchronize the output file's idea of its path name and line number
   with the input file. if the output is less than SYNC_WINDOW lines behind,
   just rectify with newlines, otherwise issue a #line directive. */

#define SYNC_WINDOW 10

static
sync()
{
    static struct vstring * path;
    static int              line_number;

    if ((path == NULL) 
            || !vstring_equal(path, input_stack->path)
            || (line_number > input_stack->line_number) 
            || (line_number < (input_stack->line_number - SYNC_WINDOW))) 
    {
        if (path != NULL) {
            out("\n");
            vstring_free(path);
        }

        path = vstring_copy(input_stack->path);
        line_number = input_stack->line_number;
        out("# %d \"%V\"\n", line_number, path);
    } 

    while (line_number < input_stack->line_number) {
        out("\n");
        line_number++;
    }
}

/* call input_line() with the given 'mode' and tokenize the line
   onto the end of 'list'. returns non-zero on success, or zero if 
   there is no more input. */

static
fill(mode, list)
    int           mode;
    struct list * list;
{
    struct vstring * line;

    line = input_line(mode);
    if (line == NULL) return 0;
    tokenize(line, list);
    vstring_free(line);

    return 1;
}

/* subject the first 'count' tokens of 'list' to macro replacement. */

replace_tokens(list, count)
    struct list * list;
{
    struct list * replace_list;

    replace_list = list_new();
    list_move(replace_list, list, count, NULL);
    macro_replace(replace_list, MACRO_REPLACE_ONCE);
    list_move(list, replace_list, -1, list->first);
    list_free(replace_list);
}

/* return the number of tokens (including 'start') that comprise
   the parentheses-enclosed actual macro argument list. if no open
   parenthesis is found, the return value is either 0 (no action 
   required) or -1 (additional lines were read, fixup is required). */

static
match_parentheses(list, start)
    struct list  * list;
    struct token * start;
{
    int            no_match = 0;
    struct token * cursor;
    int            parentheses;
    int            count;

    restart:

    for (;;) {
        cursor = start;
        parentheses = 0;
        count = 0;

        while (cursor && (cursor->class == TOKEN_SPACE)) {
            count++;
            cursor = cursor->next;
        }

        if (cursor && (cursor->class != TOKEN_LPAREN)) return no_match;

        if (!cursor) {
            no_match = -1;
            if (!fill(INPUT_LINE_LIMITED, list)) return no_match;
            goto restart;
        }

        do {
            if (cursor->class == TOKEN_LPAREN) parentheses++;
            if (cursor->class == TOKEN_RPAREN) parentheses--;
            cursor = cursor->next;
            count++;
            if (!cursor && parentheses) {
                if (!fill(INPUT_LINE_LIMITED, list)) return count;
                goto restart;
            }
        } while (parentheses);

        return count;
    }
}

/* main() seeds the keyword strings, processes the command line arguments, 
   and then loops copying input to output until there's no more. no surprises here. */

main(argc, argv)
    char ** argv;
{
    struct vstring * input_path;
    struct list *    list;
    int              check_directives;

    macro_predefine();

    ++argv;
    --argc;

    while (*argv && (**argv == '-')) {
        switch ((*argv)[1]) {
        case 'I':
            input_include_directory((*argv) + 2);
            break;
            
        case 'D':
            macro_option((*argv) + 2);
            break;

        default:
            fail("bad argument '%s'", *argv);
        }
        ++argv;
    }

    if (!*argv) fail("no input path specified");
    input_path = vstring_new(*argv);
    ++argv;

    if (!*argv) fail("no output path specified");
    output_path = vstring_new(*argv);
    output_file = fopen(output_path->data, "w");
    if (!output_file) fail("could not open '%V' for writing", output_path);
    ++argv;

    if (*argv) fail("too many arguments");

    input_open(input_path);
    list = list_new();

    for (;;) {
        while (list->count == 0) {
            if (!fill(INPUT_LINE_NORMAL, list)) {
                out("\n");
                fclose(output_file);
                exit(0);
            }
            check_directives = 1;
        }

        if (check_directives) {
            directive(list);
            check_directives = 0;
        }

        if (list->first && (list->first->class == TOKEN_NAME)) {
            struct macro * macro = macro_lookup(list->first->u.text, MACRO_LOOKUP_NORMAL);

            if (macro) {
                if (!macro->arguments) {
                    replace_tokens(list, 1);
                    continue;
                } else {
                    int i = match_parentheses(list, list->first->next);

                    if (i > 0) {
                        replace_tokens(list, 1 + i);
                        continue;
                    }

                    if (i == -1) check_directives = 1;
                }
            }
        }

        sync();

        if (list->count) {
            if (list->first->class != TOKEN_SPACE)
                out("%T ", list->first);

            list_delete(list, list->first);
        }
    }
}
