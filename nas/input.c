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

#include <limits.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "nas.h"

/* the listing facility is essentially a coroutine, with entry points
   at list_byte() and list_line(), with a collection of state variables. */

static char * list_name;
static int    list_address = NO_ADDRESS;            /* address this line starts at */
static int    nr_list_bytes;                        /* number of bytes emitted this insn */
static char   list_bytes[MAX_INSN_LENGTH];          /* buffer */

list_byte(b)
{
    if (list_address == NO_ADDRESS) 
        list_address = (segment == OBJ_SYMBOL_SEG_TEXT) ? text_bytes : data_bytes;

    if (nr_list_bytes < MAX_INSN_LENGTH) 
        list_bytes[nr_list_bytes] = b;

    nr_list_bytes++;
}

/* two simple helper functions for list_line() */

static
pad(n)
{
    while (n--) fputc(' ', list_file);
}


static 
bytes(n, idx)
    int * idx;
{
    int i;

    for (i = 0; i < n; i++) {
        if (*idx < nr_list_bytes) {
            fprintf(list_file, "%02X", list_bytes[*idx] & 0xFF);
            (*idx)++;
        } else
            pad(2);
    }
}

/* the first thing next_line() does is invoke list_line() after every line 
   processed (actually, right before reading any new line, except the first).
   provided it's the final pass and we're generating a listing, that is. */

static 
list_line()
{
    int i = 0;

    if (input_paths[input_index] != list_name) {
        list_name = input_paths[input_index];
        fprintf(list_file, "\n%s\n\n", list_name);
    }

    fprintf(list_file, "%5d ", line_number);

    if (list_address != NO_ADDRESS) {
        fprintf(list_file, "%07X ", list_address);
        bytes(8, &i);
        fprintf(list_file, "  %s", input_line);

        if (i < nr_list_bytes) {
            pad(14);
            bytes(7, &i);
            if (i < nr_list_bytes) fprintf(list_file, "..");
            fputc('\n', list_file);
        }
    } else {
        pad(26);
        fprintf(list_file, "%s", input_line);
    }

    nr_list_bytes = 0;
    list_address = NO_ADDRESS;
}

/* return the next line of input, or return zero if there isn't another. */

static
next_line()
{
    static FILE * file;

    if ((input_index >= 0) && list_file && (pass == FINAL_PASS)) list_line();

    for (;;) {
        if (file == NULL) {
            input_index++;
            if (input_paths[input_index] != NULL) {
                file = fopen(input_paths[input_index], "r");
                if (file == NULL) error("can't open input file");
            } else
                return 0;
        }

        if (fgets(input_line, MAX_INPUT_LINE, file) == NULL)  {
            fclose(file);
            file = NULL;
            line_number = 0;
        } else {
            if (strchr(input_line, '\n') == NULL) error("line unterminated or too long");
            line_number++;
            input_pos = input_line;
            return 1;
        }
    }
}

/* return the next token from the input stream, or NONE if no more. also
   sets the global variables 'token', 'name_token' and 'number_token' as applicable. */

#define ISALPHA(x)  (isalpha(x) || ((x) == '_') || ((x) == '$'))

scan()
{
    char * start;
    char * end;

    if ((*input_pos == 0) && (next_line() == 0)) 
        return token = NONE;

    while (isspace(*input_pos) && (*input_pos != '\n')) 
        input_pos++;

    if (*input_pos == ';')                              
        while (*input_pos != '\n') input_pos++;

    start = input_pos;

    if ((*input_pos == '.') && ISALPHA(input_pos[1])) input_pos++;

    if (isdigit(*input_pos) || ISALPHA(*input_pos)) {
        while (ISALPHA(*input_pos) || isdigit(*input_pos)) input_pos++;

        if (isdigit(*start)) {
            errno = 0;
            number_token = strtoul(start, &end, 0);
            if (errno || (end != input_pos)) {
                *input_pos = 0;
                error("malformed number '%s'", start);
            }
            return token = NUMBER;
        }

        if (*start == '.') {
            start++;
            token = PSEUDO;
        } else
            token = NAME;

        name_token = lookup_name(start, input_pos - start);
        if ((token == NAME) && (name_token->token)) token = name_token->token;
        return token;
    }
    
    return token = *input_pos++;
}

/* parse an expression into the 'symbol' and 'offset' fields of
   operands[n]. only supports +/- for now, but can (and will) be 
   expanded with a few primitive operators later. */

static
expression(n)
{
    int sign = 1;

    if (token == '-') {
        sign = -1;
        scan();
    }

    for (;;) {
        switch (token)
        {
        case NAME:
            reference(name_token);

            if (    (name_token->symbol->flags & OBJ_SYMBOL_DEFINED) 
                &&  (OBJ_SYMBOL_GET_SEG(*(name_token->symbol)) == OBJ_SYMBOL_SEG_ABS) ) 
            {
                number_token = name_token->symbol->value; 
                /* and fall through to NUMBER */
            } else if ( operands[n].symbol
                    &&  (sign == -1)
                    &&  (operands[n].symbol->flags & OBJ_SYMBOL_DEFINED)
                    &&  (name_token->symbol->flags & OBJ_SYMBOL_DEFINED)
                    &&  (OBJ_SYMBOL_GET_SEG(*(name_token->symbol)) == OBJ_SYMBOL_GET_SEG(*(operands[n].symbol))) ) 
            {
                /* difference of symbols in the same segment */
                number_token = operands[n].symbol->value - name_token->symbol->value;
                operands[n].symbol = NULL;
                sign = 1;
                /* and fall through to NUMBER */
            } else {
                if ((operands[n].symbol) || (sign == -1)) error("not relocatable");
                operands[n].symbol = name_token->symbol;
                break;
            }

        case NUMBER:
            operands[n].offset += (number_token * sign);
            break;

        default: error("operand/expression expected");
        }

        scan();

        switch (token)
        {
        case '+':   sign = 1; break;
        case '-':   sign = -1; break;
        default:    return 0;
        }

        scan();
    }
}

/* operand() parses an operand into operands[n], and sets applicable flags. */

static
reg_operand(n)
{
    int reg;

    reg = token;
    scan();
    operands[n].kind = OPERAND_REG;
    operands[n].reg = reg;

    if (reg & REG_GP) {
        if (reg & REG_8) {
            operands[n].flags |= O_REG_8 | ((reg == REG_AL) ? O_ACC_8 : 0);
            if (reg == REG_CL) operands[n].flags |= O_REG_CL;
        }
        if (reg & REG_16) operands[n].flags |= O_REG_16 | ((reg == REG_AX) ? O_ACC_16 : 0);
        if (reg & REG_32) operands[n].flags |= O_REG_32 | ((reg == REG_EAX) ? O_ACC_32 : 0);
        if (reg & REG_64) operands[n].flags |= O_REG_64 | ((reg == REG_RAX) ? O_ACC_64 : 0);
    } else if (reg & REG_SEG) {
        if (reg & REG_SEG2) operands[n].flags |= O_SREG2;
        if (reg & REG_SEG3) operands[n].flags |= O_SREG3;
    } else if (reg & REG_CRL) {
        operands[n].flags |= O_REG_CRL;
    } else if (reg & REG_CRH) {
        operands[n].flags |= O_REG_CRH;
    } else if (reg & REG_X) {
        operands[n].flags |= O_REG_XMM;
    } else
        error("illegal use of register");
}

static
mem_operand(n)
{
    long disp;
    int  reg;

    switch (token)
    {
    case BYTE: operands[n].flags |= O_MEM_8 | O_MOFS_8; break;
    case WORD: operands[n].flags |= O_MEM_16 | O_MOFS_16; break;
    case DWORD: operands[n].flags |= O_MEM_32 | O_MOFS_32; break;
    case QWORD: operands[n].flags |= O_MEM_64 | O_MOFS_64; break;
    }

    scan();
    operands[n].kind = OPERAND_MEM;
    if (token != '[') error("expected opening bracket '['");
    scan();

    if (token == REG_RIP) {
        operands[n].rip = 1;
        scan();
        expression(n);
        operands[n].flags |= O_ADDR_64;
    } else if ((token & REG_GP) || (token == ',')) {
        if (token & REG_GP) {
            operands[n].reg = token;
            scan();
        }

        if (token == ',') {
            scan();

            if (token & REG_GP) {
                operands[n].index = token;
                scan();

                if (token == '*') {
                    scan();
                    if (token != NUMBER) error("missing scale factor");

                    switch (number_token)
                    {
                    case 1:
                    case 2:
                    case 4:
                    case 8:
                        operands[n].scale = number_token;
                        break;
                    default:
                        error("invalid scale factor");
                    }

                    scan();
                }

                if (token == ',') {
                    scan();
                    expression(n);
                }
            } else
                expression(n);
        }
    
        operands[n].flags &= ~O_MOFS;
        disp = classify(operands[n].offset);

        if ((operands[n].reg == NONE) && (operands[n].index == NONE))
            error("missing base/index register");
        else if (operands[n].reg == NONE)
            reg = operands[n].index;
        else if (operands[n].index == NONE) 
            reg = operands[n].reg;
        else 
            reg = operands[n].reg & operands[n].index;

        switch (reg & REG_SIZE)
        {
        case REG_16:
            operands[n].flags |= O_ADDR_16;
            operands[n].disp = O_IMM_16;
            break;
        case REG_32:
            operands[n].flags |= O_ADDR_32;
            operands[n].disp = O_IMM_32;
            break;
        case REG_64:
            operands[n].flags |= O_ADDR_64;
            operands[n].disp = O_IMM_S32;
            break;
        default:
            error("mismatched base/index registers");
        }
            
        if (!(operands[n].disp & disp)) error("displacement out of range");
        if (!operands[n].symbol && (disp & O_IMM_S8)) operands[n].disp = O_IMM_8;
        if (!operands[n].symbol && !operands[n].offset) operands[n].disp = 0;
    } else {
        expression(n);
        disp = classify(operands[n].offset);

        switch (bits)
        {
        case 16:
            if (disp & O_IMM_16) {
                operands[n].flags |= O_ADDR_16;
                operands[n].disp = O_IMM_16;
                break;
            }
        case 32: 
            if (disp & O_IMM_32) {
                operands[n].flags |= O_ADDR_32;
                operands[n].disp = O_IMM_32;
                break;
            }
        case 64: 
            operands[n].flags |= O_ADDR_64;
            operands[n].disp = O_IMM_64;
        }

        if (!(disp & O_IMM_32)) operands[n].flags &= ~O_MEM;
    }

    if (!(operands[n].flags & (O_MEM | O_MOFS))) error("illegal addressing mode");
    if (token != ']') error("expected right bracket ']'");
    scan();
}

long
classify(offset)
    long offset;
{
    long flags = O_IMM_8 | O_IMM_16 | O_IMM_32 | O_IMM_64;

    if ( (offset < SCHAR_MIN) || (offset > SCHAR_MAX) ) flags &= ~O_IMM_S8;
    if ( (offset < 0)         || (offset > UCHAR_MAX) ) flags &= ~O_IMM_U8;
    if ( (offset < SHRT_MIN)  || (offset > SHRT_MAX)  ) flags &= ~O_IMM_S16;
    if ( (offset < 0)         || (offset > USHRT_MAX) ) flags &= ~O_IMM_U16;
    if ( (offset < INT_MIN)   || (offset > INT_MAX)   ) flags &= ~O_IMM_S32;
    if ( (offset < 0)         || (offset > UINT_MAX)  ) flags &= ~O_IMM_U32;

    return flags;
}

static
imm_operand(n)
{
    struct operand saved_operand;
    int            rel8 = 0;

    operands[n].kind = OPERAND_IMM;
    expression(n);

    operands[n].flags |= classify(operands[n].offset);
    if (operands[n].symbol) operands[n].flags &= ~O_IMM_8;
    if (operands[n].symbol && (bits > 16)) operands[n].flags &= ~O_IMM_16;

    /* this is a roundabout way of determining if the operand describes a 
       known target that can be reached by a short (8-bit relative) jump. */

    memcpy(&saved_operand, &operands[n], sizeof(struct operand));
    resolve(n, O_IMM_64, 2); 
    if ((operands[n].symbol == NULL) && (operands[n].offset >= SCHAR_MIN) && (operands[n].offset <= SCHAR_MAX)) rel8++;
    memcpy(&operands[n], &saved_operand, sizeof(struct operand));
    if (rel8) operands[n].flags |= O_REL_8;
}

operand(n)
{
    operands[n].reg = NONE;
    operands[n].rip = 0;
    operands[n].index = NONE;
    operands[n].scale = 1;
    operands[n].symbol = NULL;
    operands[n].offset = 0;
    operands[n].flags = 0;

    if (token & REG) 
        reg_operand(n);
    else {
        switch (token) {
        case BYTE:
        case WORD:
        case DWORD:
        case QWORD:
            mem_operand(n);
            break;
        default:
            imm_operand(n);
        }
    }
}

/* convenience function for pseudo-ops that need constant numeric operands */

long
constant_expression()
{
    operand(0);

    if ((operands[0].kind != OPERAND_IMM) || (operands[0].symbol))
        error("constant expression required");

    return operands[0].offset;
}

