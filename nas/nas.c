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
#include <stdarg.h>
#include <unistd.h>
#include "nas.h"

int                 bits = 64;                          /* .bits <x> */
int                 segment = OBJ_SYMBOL_SEG_TEXT;      /* .text or .data? */
int                 text_bytes;                         /* current text segment position */
int                 data_bytes;                         /* current data segment position */
int                 name_bytes;                         /* current name bytes (last pass only) */
int                 nr_relocs;                          /* number of relocations */
int                 nr_symbols;                         /* number of symbols */
int                 pass;                               /* between FIRST_PASS .. FINAL_PASS */
char             ** input_paths;                        /* array of input path names */
int                 input_index = -1;                   /* current index, -1 means "the beginning" */
FILE              * input_file;                         /* current input file */
char                input_line[MAX_INPUT_LINE];         /* current input line */
char              * input_pos = input_line;             /* position on that line */
int                 line_number;                        /* which line number input_line is */
char              * output_path;                        /* output path ... */
FILE              * output_file;                        /* ... and file */
char              * list_path;                          /* these are NULL unless the ... */
FILE              * list_file;                          /* ... user requested a listing file */
int                 token;                              /* current token */
struct name       * name_token;                         /* name of most recent NAME or PSEUDO */
long                number_token;                       /* value of most recent numeric token */
int                 nr_symbol_changes;                  /* symbols defined/changed this pass */
struct obj_header   header;                             /* header to use for final pass output */
struct insn       * insn;                               /* instruction being encoded */
int                 nr_operands;                        /* number of operands to current instruction */
struct operand      operands[MAX_OPERANDS];             /* the operands themselves */

/* report an error, clean up the output(s), and exit */

#ifdef __STDC__
void
error(char * fmt, ...)
#else
error(fmt)
    char * fmt;
#endif
{
    va_list args;

    fprintf(stderr, "as: ");

    if (input_index >= 0) {
        fprintf(stderr, "'%s' ", input_paths[input_index]);
        if (line_number) fprintf(stderr, "(%d) ", line_number);
    }
    
    fprintf(stderr,"ERROR: ");
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fputc('\n', stderr);
    
    if (output_file) {
        fclose(output_file);
        unlink(output_path);
    }

    if (list_file) {
        fclose(list_file);
        unlink(list_path);
    }

    exit(1);
}


assemble()
{
    struct name * name;
    int           i;

    input_index = -1;      
    nr_symbol_changes = 0;
    nr_symbols = 0;
    nr_relocs = 0;
    text_bytes = 0;
    data_bytes = 0;
    segment = OBJ_SYMBOL_SEG_TEXT;

    scan();
    while (token != NONE) {
        if (token == '\n') goto end_of_line;

        if (token == NAME && (*input_pos == '=')) {
            name = name_token;
            scan();
            scan();
            define(name, constant_expression());
            OBJ_SYMBOL_SET_SEG(*(name->symbol), OBJ_SYMBOL_SEG_ABS);
            goto end_of_line;
        }

        if (token == NAME && (*input_pos == ':')) {
            name = name_token;
            scan();
            scan();
            define(name, (segment == OBJ_SYMBOL_SEG_TEXT) ? text_bytes : data_bytes);
            OBJ_SYMBOL_SET_SEG(*(name->symbol), segment);
            if (token == '\n') goto end_of_line;
        }

        if (token == PSEUDO) {
            name = name_token;
            scan();

            if (name->pseudo == NULL) 
                error("unknown pseudo-op");
            else {
                name->pseudo();
                goto end_of_line;
            }
        }

        if ((token != NAME) || !(name_token->insn_entries)) error("instruction or pseudo-op expected");
        name = name_token;
        scan();
        nr_operands = 0;

        if (token != '\n') {
            for (;;) {
                if (nr_operands == MAX_OPERANDS) error("too many operands");
                operand(nr_operands);
                nr_operands++;

                if (token == ',')
                    scan();
                else
                    break;
            }
        }

        for (insn = name->insn_entries; insn->name == name; insn++) {
            if (insn->nr_operands != nr_operands) goto mismatch;
            if ((bits == 16) && (insn->insn_flags & I_NO_BITS_16)) goto mismatch;
            if ((bits == 32) && (insn->insn_flags & I_NO_BITS_32)) goto mismatch;
            if ((bits == 64) && (insn->insn_flags & I_NO_BITS_64)) goto mismatch;

            for (i = 0; i < insn->nr_operands; i++) 
                if (!(insn->operand_flags[i] & operands[i].flags)) goto mismatch;

            break;

          mismatch: ;
        }

        if (insn->name == name) 
            encode(insn);
        else
            error("invalid instruction/operand combination");

      end_of_line:
        if (token != '\n') error("trailing garbage - end of line expected");
        scan();
    }
}

main(argc, argv)
    char * argv[];
{
    int opt;

    while ((opt = getopt(argc, argv, "o:l:")) != -1) {
        switch (opt)
        {
        case 'o':
            output_path = optarg;
            break;

        case 'l':
            list_path = optarg;
            break;

        default:
            exit(1);
        }
    }

    input_paths = &argv[optind];
    if (*input_paths == NULL) error("no input file(s) specified");

    if (output_path == NULL) error("no output file (-o) specified");
    output_file = fopen(output_path, "w");
    if (output_file == NULL) error("can't open output file '%s'", output_path);

    if (list_path) {
        list_file = fopen(list_path, "w");
        if (list_file == NULL) error("can't open list file '%s'", list_path);
    }

    /* assembly is in minimum three passes:
       FIRST_PASS:      collect symbol data
       FIRST_PASS + n:  repeat until symbols stabilize
       FINAL_PASS:      assemble to output file */

    load_names();
    pass = FIRST_PASS;
    assemble();

    do {
        pass++;
        if (pass == FINAL_PASS) error("too many passes");
        assemble();
    } while (nr_symbol_changes);

    header.magic = OBJ_MAGIC;
    header.text_bytes = text_bytes;
    header.data_bytes = data_bytes;
    header.nr_symbols = nr_symbols;
    header.nr_relocs = nr_relocs;

    pass = FINAL_PASS;
    assemble();

    header.name_bytes = name_bytes;
    output(0, &header, sizeof(header));

    fclose(output_file);
    if (list_file) fclose(list_file);
    return 0;
}
