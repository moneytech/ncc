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

#include "nas.h"

/* .byte <byte> [ , <byte> ...] 
   .word <word> [ , <word> ...] 
   .dword <dword> [ , <dword> ...] 
   .qword <qword> [ , <qword> ...] */

static
pseudo_bwdq(flags)
    long flags;
{
    for (;;) {
        operand(0);

        if (operands[0].kind != OPERAND_IMM) 
            error("invalid operand");

        if ((operands[0].flags & flags) == 0)
            error("out of range");

        reloc(0, flags, 0);

        if (token == ',') 
            scan();
        else
            break;
    }
}

pseudo_byte() { pseudo_bwdq(O_IMM_8); }
pseudo_word() { pseudo_bwdq(O_IMM_16); }
pseudo_dword() { pseudo_bwdq(O_IMM_32); }
pseudo_qword() { pseudo_bwdq(O_IMM_64); }

/* .align <#> */

pseudo_align()
{
    long    boundary;
    char    fill = 0;
    int     bytes;

    if (segment == OBJ_SYMBOL_SEG_TEXT) {
        fill = 0x90; /* NOP */
        bytes = text_bytes;
    } else
        bytes = data_bytes;

    boundary = constant_expression();

    switch (boundary)
    {
    case 1:
    case 2:
    case 4:
    case 8:
        while (bytes % boundary) {
            emit(fill, 1);
            bytes++;
        }
        break;
        
    default:
        error("bogus alignment");
    }
}

pseudo_skip()
{
    long length;
    char fill = 0;

    length = constant_expression();
    if (segment == OBJ_SYMBOL_SEG_TEXT) fill = 0x90;
    while (length--) emit(fill, 1);
}

pseudo_fill()
{
    long length;
    char fill = 0;

    length = constant_expression();
    if (token != ',') error("missing fill value");
    scan();
    fill = constant_expression();
    if ((fill < -127) || (fill > 255)) error("invalid fill byte");
    while (length--) emit(fill, 1);
}

/* .org <address> */

pseudo_org()
{
    long target;
    char fill = 0;
    int  bytes;

    target = constant_expression();

    if (segment == OBJ_SYMBOL_SEG_TEXT) {
        fill = 0x90; /* NOP */
        bytes = text_bytes;
    } else
        bytes = data_bytes;

    target -= bytes;
    if (target < 0) error("origin goes backwards");
    while (target--) emit(fill, 1);
}

/* .text
   .data

   select output segment */

pseudo_text()
{
    segment = OBJ_SYMBOL_SEG_TEXT;
}

pseudo_data()
{
    segment = OBJ_SYMBOL_SEG_DATA;
}

/* .bss <symbol>, <size> [ , <align> ] */

pseudo_bss()
{
    struct name * name;
    long          number;
    int           bit;
    int           log2 = 0;

    if (token != NAME) error("expected symbol name");
    name = name_token;
    scan();

    if (token != ',') error("missing size");
    scan();
    number = constant_expression();
    define(name, number);
    OBJ_SYMBOL_SET_SEG(*name->symbol, OBJ_SYMBOL_SEG_BSS);

    if (token == ',') {
        scan();
        number = constant_expression();
        bit = 1;

        while ((number & bit) != number) {
            log2++;
            bit <<= 1;
            if (!OBJ_SYMBOL_VALID_ALIGN(log2)) error("invalid alignment");
        }
    }

    OBJ_SYMBOL_SET_ALIGN(*name->symbol, log2);
}

/* .ascii <string>

   a string is any collection of characters (except newline) delimited 
   by single or double quote. the scanner is bypassed since no other part
   of the assembler cares about strings. */

pseudo_ascii()
{
    int delimiter;

    operands[0].kind == OPERAND_IMM;
    operands[0].symbol = NULL;
    operands[0].flags = O_IMM_8;

    delimiter = token;
    if ((delimiter != '\'') && (delimiter != '\"')) error("string expected");
    
    while ((*input_pos != '\n') && (*input_pos != delimiter)) {
        operands[0].offset = *input_pos;
        reloc(0, O_IMM_8, 0);
        input_pos++;
    }

    if (*input_pos == delimiter) 
        input_pos++;
    else
        error("missing closing delimiter");

    scan();
}

/* .global <name> - mark a symbol for external linkage */

pseudo_global()
{
    if (token != NAME) error("name expected");
    reference(name_token);
    name_token->symbol->flags |= OBJ_SYMBOL_GLOBAL;
    scan();
}

/* .bits <#> - set assembly mode */

pseudo_bits()
{
    if (token != NUMBER) error(".bits takes a numeric operand");

    switch (number_token)
    {
    case 16:
    case 32:
    case 64:
        bits = number_token;
        break;
    default:
        error("must be 16, 32 or 64");
    }

    scan();
}
