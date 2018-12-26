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
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include "ncc1.h"

int             g_flag;             /* -g: produce debug info */
int             O_flag;             /* -O: enable optimizations */
FILE          * yyin;               /* lexical input */
struct token    token;          
struct string * input_name;         /* input file name and line number ... */
int             line_number;        /* ... subject to change from # line */
FILE *          output_file;   
struct string * output_name;   
int             next_asm_label = 1;
int             next_iregister = R_IPSEUDO;
int             next_fregister = R_FPSEUDO;
int             current_scope = SCOPE_GLOBAL;
struct symbol * current_function;
int             frame_offset;
int             save_iregs;         /* bitsets (1 << R_IDX(x)) of registers .. */
int             save_fregs;         /* .. used in this function */
int             loop_level;
struct block *  first_block;
struct block *  last_block;
struct block *  current_block;
struct block *  entry_block;
struct block *  exit_block;

/* report an error to the user, clean up, and abort.
   error messages must match the indices (ERROR_*) in cc1.h. */

static char *errors[] = 
{
    "command line syntax",                  /* ERROR_CMDLINE */
    "can't read input file",                /* ERROR_INPUT */
    "syntax",                               /* ERROR_SYNTAX */
    "out of memory",                        /* ERROR_MEMORY */
    "malformed directive",                  /* ERROR_DIRECTIVE */
    "lexical failure",                      /* ERROR_LEXICAL */
    "malformed integral constant",          /* ERROR_BADICON */
    "integral constant out of range",       /* ERROR_IRANGE */
    "malformed floating constant",          /* ERROR_BADFCON */
    "floating constant out of range",       /* ERROR_FRANGE */
    "invalid octal escape sequence",        /* ERROR_ESCAPE */
    "unterminated string literal",          /* ERROR_UNTERM */
    "invalid character constant",           /* ERROR_BADCCON */
    "overflow in multi-character constant", /* ERROR_CRANGE */
    "can't open output file",               /* ERROR_OUTPUT */
    "nesting level too deep",               /* ERROR_NESTING */
    "type too big",                         /* ERROR_TYPESIZE */
    "incomplete type",                      /* ERROR_INCOMPLETE */
    "illegal use of function type",         /* ERROR_ILLFUNC */
    "illegal array specification",          /* ERROR_ILLARRAY */
    "illegal function return type",         /* ERROR_RETURN */
    "illegal use of struct/union type",     /* ERROR_STRUCT */
    "misplaced formal arguments",           /* ERROR_NOARGS */
    "bit field not permitted",              /* ERROR_ILLFIELD */
    "illegal type for bit field",           /* ERROR_FIELDTY */
    "invalid bit field size",               /* ERROR_FIELDSZ */
    "storage class not permitted",          /* ERROR_SCLASS */
    "struct/union already defined",         /* ERROR_TAGREDEF */
    "empty struct/union definition",        /* ERROR_EMPTY */
    "duplicate member declaration",         /* ERROR_REMEMBER */
    "illegal/incompatible redeclaration",   /* ERROR_REDECL */
    "unknown argument identifier",          /* ERROR_NOTARG */
    "declare functions at file scope",      /* ERROR_NOFUNC */
    "can't do that with typedef",           /* ERROR_TYPEDEF */
    "unknown identifier",                   /* ERROR_UNKNOWN */
    "illegal operand(s)",                   /* ERROR_OPERANDS */
    "incompatible type",                    /* ERROR_INCOMPAT */
    "not an lvalue",                        /* ERROR_LVALUE */
    "abstract type required",               /* ERROR_ABSTRACT */
    "declaration missing identifier",       /* ERROR_MISSING */
    "bad type cast",                        /* ERROR_BADCAST */
    "illegal indirection",                  /* ERROR_INDIR */
    "struct or union required",             /* ERROR_NOTSTRUCT */
    "not a member of that struct/union",    /* ERROR_NOTMEMBER */
    "can only call functions",              /* ERROR_NEEDFUNC */
    "compiler bug",                         /* ERROR_INTERNAL */
    "can't take address of register",       /* ERROR_REGISTER */
    "constant expression required",         /* ERROR_CONEXPR */
    "division by 0",                        /* ERROR_DIV0 */
    "illegal constant expression",          /* ERROR_BADEXPR */
    "wrong segment for function call",      /* ERROR_SEGMENT */
    "bad initializer",                      /* ERROR_BADINIT */
    "duplicate definition",                 /* ERROR_DUPDEF */
    "misplaced break, continue or case",    /* ERROR_MISPLACED */
    "dangling goto (undefined label)",      /* ERROR_DANGLING */
    "duplicate case label",                 /* ERROR_DUPCASE */
    "switch/case expression not integral"   /* ERROR_CASE */
};

error(code)
{
    fprintf(stderr, "cc1: ");

    if (input_name) {
        fprintf(stderr, "'%s' ", input_name->data);
        if (line_number) fprintf(stderr, "(%d) ", line_number);
    }

    fprintf(stderr, "ERROR: %s\n", errors[code]);

    if (output_file) {
        fclose(output_file);
        unlink(output_name->data);
    }

    exit(1);
}

/* a general-purpose allocation function. guarantees success. */

char *
allocate(bytes)
    int bytes;
{
    char * p = malloc(bytes);
    
    if (p == NULL) error(ERROR_MEMORY);
    return p;
}


main(argc, argv)
    char *argv[];
{
    int opt;

    while ((opt = getopt(argc, argv, "gO")) != -1)
    {
        switch (opt)
        {
        case 'O':
            ++O_flag;
            break;
        case 'g':
            ++g_flag;
            break;
        default:
            exit(1);
        }
    }

    argc -= optind;
    argv = &argv[optind];

    if (argc != 2) error(ERROR_CMDLINE);

    output_name = stringize(argv[1], strlen(argv[1]));
    input_name = output_name; /* trick error() for a sec */
    output_file = fopen(argv[1], "w");
    if (!output_file) error(ERROR_OUTPUT);

    input_name = stringize(argv[0], strlen(argv[0]));
    yyin = fopen(argv[0], "r");
    if (!yyin) error(ERROR_INPUT);

    yyinit();
    translation_unit();
    literals();
    externs();

    fclose(output_file);
    exit(0);
}
