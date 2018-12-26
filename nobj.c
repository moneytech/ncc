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
#include <unistd.h>
#include <stdlib.h>

#include "obj.h"
#include "a.out.h"

struct exec         exec;
struct obj_header   hdr;
FILE              * fp;
char              * path;
int                 s_flag = 0;
int                 r_flag = 0;

error(msg)
    char * msg;
{
    fprintf(stderr, "obj: ");
    if (path) fprintf(stderr, "'%s': ", path);
    fprintf(stderr, "%s\n", msg);
    exit(1);
}

/* read 'nr_bytes' bytes into 'buffer' from 'position' in 'fp',
   and make sure no errors occur in the process. */

input(position, buffer, nr_bytes)
    char * buffer;
{
    if (fseek(fp, (long) position, SEEK_SET))
        error("seek error");

    if (fread(buffer, sizeof(char), sizeof(char) * nr_bytes, fp) != (sizeof(char) * nr_bytes))
        error("read error");
}

/* print out a NUL-terminated string starting at 'position'.
   returns the size of the string printed (excluding terminator). */

string(position)
{
    int  i;
    char c;

    for (i = 0; ; ++i) {
        input(position + i, &c, 1);

        if (c)
            putchar(c);
        else
            return i;
    }
}

/* print out the name of a symbol, given the symbol's index. */

obj_name(index)
{
    struct obj_symbol symbol;

    input(OBJ_SYMBOL_OFFSET(hdr, index), &symbol, sizeof(symbol));
    string(OBJ_NAME_OFFSET(hdr, symbol.index));
}

/* print out a table of all the symbols in the object */

obj_symbols()
{
    struct obj_symbol symbol;
    int               i;

    putchar('\n');

    if (hdr.nr_symbols) {
        for (i = 0; i < hdr.nr_symbols; i++) {
            input(OBJ_SYMBOL_OFFSET(hdr, i), &symbol, sizeof(symbol));
            putchar((symbol.flags & OBJ_SYMBOL_GLOBAL) ? '+' : ' ');

            if (symbol.flags & OBJ_SYMBOL_DEFINED) {
                switch (OBJ_SYMBOL_GET_SEG(symbol)) 
                {
                case OBJ_SYMBOL_SEG_TEXT:
                    printf("text %08lx       ", symbol.value);
                    break;
                case OBJ_SYMBOL_SEG_DATA:
                    printf("data %08lx       ", symbol.value);
                    break;
                case OBJ_SYMBOL_SEG_ABS:
                    printf("abs  %08lx       ", symbol.value);
                    break;
                case OBJ_SYMBOL_SEG_BSS:
                    printf("bss  %08lx,%-5d ", symbol.value, 1 << OBJ_SYMBOL_GET_ALIGN(symbol));
                }
            } else
                printf("?                   ");

            obj_name(i);
            putchar('\n');
        }
    } else
        puts("no symbols");

    putchar('\n');
}

/* print out all the relocation records in the file */

obj_relocs()
{
    struct obj_reloc reloc;
    int              i;

    putchar('\n');
    
    if (hdr.nr_relocs) {
        for (i = 0; i < hdr.nr_relocs; i++) {
            input(OBJ_RELOC_OFFSET(hdr, i), &reloc, sizeof(reloc));
            putchar((reloc.flags & OBJ_RELOC_REL) ? 'R' : ' ');
            switch (OBJ_RELOC_GET_SIZE(reloc))
            {
            case OBJ_RELOC_SIZE_8:  printf("8  "); break;
            case OBJ_RELOC_SIZE_16: printf("16 "); break;
            case OBJ_RELOC_SIZE_32: printf("32 "); break;
            case OBJ_RELOC_SIZE_64: printf("64 "); break;
            }
            printf(" @ %s %08lx (", (reloc.flags & OBJ_RELOC_TEXT) ? "text" : "data", reloc.target);
            obj_name(reloc.index);
            puts(")");
        }
    } else 
        puts("no relocation records");
    
    putchar('\n');
}

exec_symbols()
{
    int  symofs;
    int  length;
    long value;
    int  position;

    putchar('\n');
    if (exec.a_syms) {
        symofs = exec.a_text + exec.a_data;
        for (position = 0; position < exec.a_syms; ) {
            length = string(symofs + position);
            position += length + 1;
            position += sizeof(value) - 1;
            position &= ~(sizeof(value) - 1);
            input(symofs + position, &value, sizeof(value));
            while (length < 32) {
                length++;
                putchar(' ');
            }
            printf(" %016lx\n", value);
            position += sizeof(value);
        }
    } else 
        puts("no symbols");

    putchar('\n');
}

exec_relocs()
{
}

main(argc, argv)
    char * argv[];
{
    int opt;
    int magic;

    while ((opt = getopt(argc, argv, "rs")) != -1) {
        switch (opt)
        {
        case 'r': r_flag++; break;
        case 's': s_flag++; break;

        default:    
            exit(1);
        }
    }

    argv = &argv[optind];

    while (*argv) {
        path = *argv;
        if (!(fp = fopen(path, "r"))) error("can't open");

        input(0, &magic, sizeof(magic));

        if (magic == OBJ_MAGIC) {
            input(0, &hdr, sizeof(hdr));
            printf("object file: %s\n", path);
            printf("  text size: %d\n", hdr.text_bytes);
            printf("  data size: %d\n", hdr.data_bytes);
            printf("  # symbols: %d\n", hdr.nr_symbols);
            printf("   # relocs: %d\n", hdr.nr_relocs);
            printf("  name size: %d\n", hdr.name_bytes);
            if (s_flag) obj_symbols();
            if (r_flag) obj_relocs();
        } else if (magic == A_MAGIC) {
            input(0, &exec, sizeof(exec));
            printf(" a.out file: %s\n", path);
            printf("  text size: %d\n", exec.a_text);
            printf("  data size: %d\n", exec.a_data);
            printf("   bss size: %d\n", exec.a_bss);
            printf("  syms size: %d\n", exec.a_syms);
            if (s_flag) exec_symbols();
            if (r_flag) exec_relocs();
        } else error("unknown file format");

        fclose(fp);
        argv++;
    }

    return 0;
}
