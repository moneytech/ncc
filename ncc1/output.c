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
#include <stdarg.h>

/* write a register name to the output file.  the T_* type bits
   are used solely as a hint to the proper size for integer registers. */

static
output_reg(reg, ts)
{
    static char *iregs[NR_REGS][4] = {
        { "al", "ax", "eax", "rax" },
        { "dl", "dx", "edx", "rdx" },
        { "cl", "cx", "ecx", "rcx" },
        { "bl", "bx", "ebx", "rbx" },
        { "sil", "si", "esi", "rsi" },
        { "dil", "di", "edi", "rdi" },
        { "bpl", "bp", "ebp", "rbp" },
        { "spl", "sp", "esp", "rsp" },
        { "r8b", "r8w", "r8d", "r8" },
        { "r9b", "r9w", "r9d", "r9" },
        { "r10b", "r10w", "r10d", "r10" },
        { "r11b", "r11w", "r11d", "r11" },
        { "r12b", "r12w", "r12d", "r12" },
        { "r13b", "r13w", "r13d", "r13" },
        { "r14b", "r14w", "r14d", "r14" },
        { "r15b", "r15w", "r15d", "r15" }
    };

    int i;

    if (R_IS_PSEUDO(reg)) {
        fprintf(output_file, "%c#%d", (reg & R_IS_FLOAT) ? 'f' : 'i', R_IDX(reg));
        if (reg & R_IS_INTEGRAL) {
            if (ts & T_IS_BYTE) fputc('b', output_file);
            if (ts & T_IS_WORD) fputc('w', output_file);
            if (ts & T_IS_DWORD) fputc('d', output_file);
            if (ts & T_IS_QWORD) fputc('q', output_file);
        }
    } else {
        if (reg & R_IS_FLOAT) 
            fprintf(output_file, "xmm%d", R_IDX(reg));
        else {
            if (ts & T_IS_BYTE) i = 0;
            if (ts & T_IS_WORD) i = 1;
            if (ts & T_IS_DWORD) i = 2;
            if (ts & T_IS_QWORD) i = 3;
            fprintf(output_file, "%s", iregs[R_IDX(reg)][i]);
        }
    }
}

/* outputting operands is an easy, if messy, business. */

static
output_operand(tree)
    struct tree * tree;
{
    double     lf;
    float      f;
    int        indexed;

    switch (tree->op) {
    case E_CON:
        if (tree->type->ts & T_FLOAT) {
            f = tree->u.con.f;
            fprintf(output_file, "0x%x", *((unsigned *) &f));
        } else if (tree->type->ts & T_LFLOAT) {
            lf = tree->u.con.f;
            fprintf(output_file, "0x%lx", *((unsigned long *) &lf));
        } else 
            fprintf(output_file, "%ld", tree->u.con.i);

        break;
    case E_REG:
        output_reg(tree->u.reg, tree->type->ts);
        break;
    case E_IMM:
    case E_MEM:
        if (tree->op == E_MEM) {
            if (tree->type->ts & T_IS_BYTE) fprintf(output_file, "byte ");
            if (tree->type->ts & T_IS_WORD) fprintf(output_file, "word ");
            if (tree->type->ts & T_IS_DWORD) fprintf(output_file, "dword ");
            if (tree->type->ts & T_IS_QWORD) fprintf(output_file, "qword ");
            fputc('[', output_file);
        }

        if (tree->u.mi.rip) {
            fprintf(output_file, "rip ");
            output("%G", tree->u.mi.glob);
            if (tree->u.mi.ofs) {
                if (tree->u.mi.ofs > 0) fputc('+', output_file);
                fprintf(output_file, "%ld", tree->u.mi.ofs);
            }
        } else {
            indexed = 0;

            if (tree->u.mi.b != R_NONE) {
                output_reg(tree->u.mi.b, T_PTR);
                indexed++;
            }

            if (tree->u.mi.i != R_NONE) {
                fputc(',', output_file);
                output_reg(tree->u.mi.i, T_PTR);
                if (tree->u.mi.s > 1) fprintf(output_file, "*%d", tree->u.mi.s);
                indexed++;
            }

            if (tree->u.mi.glob) {
                if (indexed) fputc(',', output_file);
                output("%G", tree->u.mi.glob);
            }

            if (tree->u.mi.ofs) {
                if (tree->u.mi.glob && (tree->u.mi.ofs > 0))
                    fputc('+', output_file);
                else if (indexed)
                    fputc(',', output_file);

                fprintf(output_file, "%ld", tree->u.mi.ofs);
            }
        }

        if (tree->op == E_MEM) fputc(']', output_file);
        break;

    default: error(ERROR_INTERNAL);
    }
}

/* write to the output file. the recognized specifiers are:

   %%   (no argument)       the literal '%'
   %L   (int)               an asm label
   %R   (int, int)          a register (with type bits)
   %G   (struct symbol *)   the assembler name of a global
   %O   (struct tree *)     an operand expression

   %s, %d, %x               like printf()
   %X                       like %x, but for long

   any unrecognized specifiers will bomb */

#ifdef __STDC__
void
output(char * fmt, ...)
#else
output(fmt)
    char * fmt;
#endif
{
    va_list         args;
    struct symbol * symbol;
    int             reg;
    int             ts;

    va_start(args, fmt);

    while (*fmt) {
        if (*fmt == '%') {
            fmt++;
            switch (*fmt) {
            case '%':
                fputc('%', output_file);
                break;
            case 'd':
                fprintf(output_file, "%d", va_arg(args, int));
                break;
            case 'L':
                fprintf(output_file, "L%d", va_arg(args, int));
                break;
            case 's':
                fprintf(output_file, "%s", va_arg(args, char *));
                break;
            case 'x':
                fprintf(output_file, "%x", va_arg(args, int));
                break;
            case 'X':
                fprintf(output_file, "%lx", va_arg(args, long));
                break;
            case 'G':
                symbol = va_arg(args, struct symbol *);

                if ((symbol->ss & (S_EXTERN | S_STATIC)) && (symbol->scope == SCOPE_GLOBAL) && symbol->id)
                    fprintf(output_file, "_%s", symbol->id->data);
                else if (symbol->ss & (S_STATIC | S_EXTERN)) 
                    fprintf(output_file, "L%d", symbol->i);
                else 
                    error(ERROR_INTERNAL);

                break;
            case 'O':
                output_operand(va_arg(args, struct tree *));
                break;
            case 'R':
                reg = va_arg(args, int);
                ts = va_arg(args, int);
                output_reg(reg, ts);
                break;
            default:
                error(ERROR_INTERNAL);
            }
        } else
            fputc(*fmt, output_file);

        fmt++;
    }

    va_end(args);
}

/* emit assembler directive to select the appropriate
   SEGMENT_*, if not already selected */

segment(new)
{
    static int    current = -1;

    if (new != current) {
        output("%s\n", (new == SEGMENT_TEXT) ? ".text" : ".data");
        current = new;
    }
}

/* output 'length' bytes of 'string' to the assembler output.
   the caller is assumed to have selected the appropriate segment
   and emitted a label, if necessary. if 'length' exceeds the 
   length of the string, the output is padded with zeroes. */

output_string(string, length)
    struct string * string;
{
    int i = 0;

    while (i < length) {
        if (i % 16) 
            output(",");
        else {
            if (i) output("\n");
            output(" .byte ");
        }
        output("%d", (i >= string->length) ? 0 : string->data[i]);
        i++;
    }
    output("\n");
}

/* conditional jump mnemonics. these must match the CC_* values in block.h. */

static char * jmps[] = { 
    "jz", "jnz", "jg", "jle", "jge", "jl", "ja", 
    "jbe", "jae", "jb", "jmp", "NEVER"
};

/* instruction mnemonics. keyed to I_IDX() from insn.h */

static char *insns[] =
{
        /*   0 */   "nop", "mov", "movsx", "movzx", "movss",
        /*   5 */   "movsd", "lea", "cmp", "ucomiss", "ucomisd",
        /*  10 */   "pxor", "cvtss2si", "cvtsd2si", "cvtsi2ss", "cvtsi2sd",
        /*  15 */   "cvtss2sd", "cvtsd2ss", "shl", "shr", "sar",
        /*  20 */   "add", "addss", "addsd", "sub", "subss",
        /*  25 */   "subsd", "imul", "mulss", "mulsd", "or",
        /*  30 */   "xor", "and", "cdq", "cqo", "div",
        /*  35 */   "idiv", "divss", "divsd", "cbw", "cwd",
        /*  40 */   "setz", "setnz", "setg", "setle", "setge",
        /*  45 */   "setl", "seta", "setbe", "setae", "setb",
        /*  50 */   "not", "neg", "push", "pop", "call",
        /*  55 */   "test", "ret", "inc", "dec"
};

/* output a block. the main task of this function is to output the 
   instructions -- a simple task. the debugging data is most of the work! */

static 
output_block1(block, du)
    struct block * block;
{
    struct defuse * defuse;
    
    output("\n; ");

    switch (du)
    {
    case DU_USE:    output("USE: "); break;
    case DU_DEF:    output("DEF: "); break;
    case DU_IN:     output(" IN: "); break;
    case DU_OUT:    output("OUT: "); break;
    }

    for (defuse = block->defuses; defuse; defuse = defuse->link) {
        if (defuse->dus & du) {
            output("%R", defuse->symbol->reg, defuse->symbol->type->ts);
            if (defuse->reg != R_NONE) output("=%R", defuse->reg, defuse->symbol->type->ts);
            if (defuse->symbol->id) output("(%s)", defuse->symbol->id->data);
            if ((du == DU_OUT) && DU_TRANSIT(*defuse)) output("[dist %d]", defuse->distance);
            output(" ");
        }
    }
}

output_block(block)
    struct block * block;
{
    struct insn * insn;
    int           i;
    struct block  * cessor;
    int             n;

    if (g_flag) {
        output("\n; block %d", block->asm_label);
        if (block == entry_block) output(" ENTRY");
        if (block == exit_block) output(" EXIT");

        if (block->bs & B_RECON) 
            output(" RECON");
        else {
            output_block1(block, DU_IN);
            output_block1(block, DU_USE);
            output_block1(block, DU_DEF);
            output_block1(block, DU_OUT);
        }

        output("\n; %d predecessors:", block->nr_predecessors);
        for (n = 0; cessor = block_predecessor(block, n); ++n) 
            output(" %d", cessor->asm_label);

        output("\n; %d successors:", block->nr_successors);

        for (n = 0; cessor = block_successor(block, n); ++n) 
            output(" %s=%d", 
                jmps[block_successor_cc(block, n)], 
                cessor->asm_label);
    }

    output("\n%L:\n", block->asm_label);

    for (insn = block->first_insn; insn; insn = insn->next) {
        output(" %s ", insns[I_IDX(insn->opcode)]);
        for (i = 0; i < I_NR_OPERANDS(insn->opcode); i++) {
            if (i) output(",");
            output("%O", insn->operand[i]);
        }
        if ((insn->flags & INSN_FLAG_CC) && g_flag) output(" ; FLAG_CC");
        output("\n");
    }
}

/* called after the code generator is complete, to output all the function blocks.
   the main task of this function is to glue the successive blocks together with
   appropriate jump instructions, which is surprisingly tedious. */

output_function()
{
    struct block * block;
    struct block * successor1;
    struct block * successor2;
    int            cc1;

    block = first_block;
    segment(SEGMENT_TEXT);
    if (current_function->ss & S_EXTERN) output(".global %G\n", current_function);
    output("%G:\n", current_function);

    for (block = first_block; block; block = block->next) {
        output_block(block);

        successor1 = block_successor(block, 0);
        if (successor1) cc1 = block_successor_cc(block, 0);

        /* there's no glue if there aren't any successors (exit block) */

        if (!successor1) continue;

        /* if there's only one successor, it should be unconditional,
           so emit a jump unless the target is being output next. */
    
        if (block->nr_successors == 1) {
            if (cc1 != CC_ALWAYS) error(ERROR_INTERNAL);
            if (block->next != successor1) output(" jmp %L\n", successor1->asm_label);
            continue;
        }

        /* the only remaining case is that of two successors, which must have opposite
           condition codes. we make an extra effort here to emit unconditional branches,
           rather than conditional ones, to minimize the impact on the branch predictor. */

        successor2 = block_successor(block, 1);
        if (block_successor_cc(block, 1) != CC_INVERT(cc1)) error(ERROR_INTERNAL);

        if (successor1 == block->next) 
            output(" %s %L\n", jmps[CC_INVERT(cc1)], successor2->asm_label);
        else {
            output(" %s %L\n", jmps[cc1], successor1->asm_label);
            if (successor2 != block->next) output(" jmp %L\n", successor2->asm_label);
        }
    }
}
