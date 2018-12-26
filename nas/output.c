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
#include <unistd.h>
#include <stdlib.h>
#include "nas.h"

/* write 'length' bytes from 'data' to the output file at 'position'.
   this is here mainly to check for errors. */

output(position, data, length)
    char * data;
{
    static int current_position = -1;

    if (position != current_position) {
        if (fseek(output_file, (long) position, SEEK_SET))
            error("can't seek output file");

        current_position = position;
    }

    if (fwrite(data, sizeof(char), sizeof(char) * length, output_file) != (sizeof(char) * length))
        error("error writing output");

    current_position += length;
}

/* emit 1, 2, 4, or 8 bytes to the current output segment (text or data).
   on the final pass, actually writes to the file. bumps the counter. */

emit(l, length)
    long l;
{
    int  position;
    char b;

    while (length) {
        b = l & 0xFF;
        if (pass == FINAL_PASS) list_byte(b);

        if (segment == OBJ_SYMBOL_SEG_TEXT) {
            position = OBJ_TEXT_OFFSET(header) + text_bytes;
            if (text_bytes == MAX_BYTES) error("text segment overflow");
            text_bytes++;
        } else {
            position = OBJ_DATA_OFFSET(header) + data_bytes;
            if (data_bytes == MAX_BYTES) error("data segment overflow");
            data_bytes++;
        }

        if (pass == FINAL_PASS) output(position, &b, 1);

        length--;
        l >>= 8;
    }
}

/* peer into the crystal ball and tell us how many immediate bytes are
   going to be emitted with this instruction. needed to properly compute
   rIP-relative operands in the presence of trailing immediate bytes. */

crystal()
{
    long flags;
    int  i;
    int  count = 0;

    for (i = 0; i < nr_operands; i++) {
        flags = insn->operand_flags[i] & O_IMM;
        if (flags && !(insn->operand_flags[i] & O_I_REL)) {
            if (flags & O_IMM_8) count += 1; 
            if (flags & O_IMM_16) count += 2;
            if (flags & O_IMM_32) count += 4;
            if (flags & O_IMM_64) count += 8;
        }
    }

    return count;
}

/* compute an rIP-relative operand, attempting to resolve symbols
   locally. assume rIP is the current location counter plus 'offset'. 
   'flags' is one of O_IMM_* to check that we don't overflow. */

resolve(n, flags, offset)
    long flags;
{
    int bytes = (segment == OBJ_SYMBOL_SEG_TEXT) ? text_bytes : data_bytes;

    operands[n].offset -= offset;

    if (    operands[n].symbol 
        && (operands[n].symbol->flags & OBJ_SYMBOL_DEFINED)
        && (OBJ_SYMBOL_GET_SEG(*operands[n].symbol) == segment) )
    {
        operands[n].offset += operands[n].symbol->value;
        operands[n].symbol = NULL;
    }

    if (operands[n].symbol == NULL) operands[n].offset -= bytes;
    if (!(classify(operands[n].offset) & flags)) error("relative address out of range");
}

/* output the value ('symbol' + 'offset') operands[n] to the current segment,
   and, if on the final pass, issue a relocation if necessary. */

reloc(n, flags, rel)
    long flags;
{
    struct obj_reloc reloc; 
    int              size = 0;
    int              position;

    if (flags & O_IMM_8) size = 1;
    if (flags & O_IMM_16) size = 2;
    if (flags & O_IMM_32) size = 4;
    if (flags & O_IMM_64) size = 8;

    if (rel) resolve(n, flags, size + crystal());

    if (operands[n].symbol) {
        if (pass == FINAL_PASS) {
            reloc.flags = 0;
            reloc.reserved = 0;
            reloc.index = operands[n].symbol->index;
            if (rel) reloc.flags |= OBJ_RELOC_REL;

            if (segment == OBJ_SYMBOL_SEG_TEXT) {
                reloc.flags |= OBJ_RELOC_TEXT;
                reloc.target = text_bytes;
            } else {
                reloc.flags |= OBJ_RELOC_DATA;
                reloc.target = data_bytes;
            }

            switch (size)
            {
            case 1: OBJ_RELOC_SET_SIZE(reloc, OBJ_RELOC_SIZE_8); break;
            case 2: OBJ_RELOC_SET_SIZE(reloc, OBJ_RELOC_SIZE_16); break;
            case 4: OBJ_RELOC_SET_SIZE(reloc, OBJ_RELOC_SIZE_32); break;
            case 8: OBJ_RELOC_SET_SIZE(reloc, OBJ_RELOC_SIZE_64); break;
            }

            position = OBJ_RELOCS_OFFSET(header);
            position += nr_relocs * sizeof(struct obj_reloc);
            output(position, &reloc, sizeof(reloc));
        }

        nr_relocs++;
    }

    emit(operands[n].offset, size);
}


/* encode OPERAND_MEM operand[n] of 'insn' as a mod/rm field.  
   'modrm' is the mod/rm container with the middle bits pre-filled. 

   this is separated from encode() to keep the mess confined. */

#define SET_MOD(b,n)    ((b) = (((b) & 0x3F) | (((n) & 0x03) << 6)))
#define SET_RM(b,n)     ((b) = (((b) & 0xF8) | ((n) & 0x07)))

#define SET_SIB_SCALE(b,n)  ((b) |= (((n) & 0x03) << 6))
#define SET_SIB_BASE(b,n)   ((b) |= ((n) & 0x07))
#define SET_SIB_INDEX(b,n)  ((b) |= (((n) & 0x07) << 3))

static struct {     /* 16-bit base/index register combinations. */
    int reg;        /* order is important: index is r/m field value */
    int index;
} rm16[] = {
    { REG_BX, REG_SI }, { REG_BX, REG_DI }, { REG_BP, REG_SI }, { REG_BP, REG_DI },
    { REG_SI, NONE },   { REG_DI, NONE },   { REG_BP, NONE },   { REG_BX, NONE }
};

#define NR_RM16 (sizeof(rm16) / sizeof(*rm16))

modrm(n, modrm)
{
    int     i;
    char    sib = 0;
    int     rel = 0;
    int     need_sib = 0;

    if (operands[n].rip) {                                                          /* 64-bit RIP-relative */
        operands[n].disp = O_IMM_32;
        SET_RM(modrm, 5);
        rel = 1;
    } else if (operands[n].flags & O_ADDR_16) {                                     /* 16-bit */
        if ((operands[n].reg == REG_BP) && (operands[n].index == NONE) && !operands[n].disp) 
            operands[n].disp = O_IMM_8;  /* [BP] -> [BP,0] */

        if ((operands[n].reg == NONE) && (operands[n].index == NONE)) {
            SET_RM(modrm, 6);
            operands[n].disp = O_IMM_16;
        } else {
            for (i = 0; i < NR_RM16; i++) {
                if (rm16[i].reg != operands[n].reg) continue;
                if (rm16[i].index != operands[n].index) continue;

                SET_RM(modrm, i);
                break;
            }

            if (i == NR_RM16) error("invalid base/index register combination");

            switch (operands[n].disp) 
            {
            case O_IMM_8:   SET_MOD(modrm, 1); break;
            case O_IMM_16:  SET_MOD(modrm, 2); break;
            }
        }
    } else {                                                                        /* 32/64-bit */
        if ((operands[n].reg == NONE) && (operands[n].index == NONE)) {
            operands[n].disp = O_IMM_32;

            if (operands[n].flags & O_ADDR_64) {
                need_sib++;
                SET_RM(modrm, 4);
                SET_SIB_BASE(sib, 5);
                SET_SIB_INDEX(sib, 4);
            } else {
                SET_RM(modrm, 5);
            }
        } else {
            if ((operands[n].index == REG_ESP) || (operands[n].index == REG_RSP)) 
                error("ESP/RSP can't be the index");

            /* just as in 16-bit address modes, xBP can't appear without a displacement.
               because the base register is only partially decoded, R13x can't either. */

            if (((REG_DECODE(operands[n].reg) & 0x07) == 5) && !operands[n].disp) operands[n].disp = O_IMM_8;

            switch (operands[n].disp)
            {
            case O_IMM_8:   SET_MOD(modrm, 1); break;
            case O_IMM_S32:
            case O_IMM_32:  SET_MOD(modrm, 2); break;
            }

            if ((operands[n].index == NONE) && ((REG_DECODE(operands[n].reg) & 0x07) != 4))     /* xSP or R12x */
                SET_RM(modrm, REG_DECODE(operands[n].reg));
            else {
                need_sib++;
                SET_RM(modrm, 4);

                if (operands[n].reg) 
                    SET_SIB_BASE(sib, REG_DECODE(operands[n].reg));
                else {
                    SET_MOD(modrm, 0);
                    SET_SIB_BASE(sib, 5);
                    operands[n].disp = O_IMM_32;
                }

                if (operands[n].index)
                    SET_SIB_INDEX(sib, REG_DECODE(operands[n].index));
                else
                    SET_SIB_INDEX(sib, 4);

                switch (operands[n].scale)
                {
                case 2:     SET_SIB_SCALE(sib, 1); break;
                case 4:     SET_SIB_SCALE(sib, 2); break;
                case 8:     SET_SIB_SCALE(sib, 3); break;
                }
            }
        }
    }
    
    emit(modrm, 1);
    if (need_sib) emit(sib, 1);
    reloc(n, operands[n].disp, rel);
}

encode()
{
    char byte;
    char rex = 0;
    int  i;

    if ((bits != 64) && (insn->insn_flags & I_DATA_64)) 
        error("64-bit operands not available");

    /* 1. operand size override prefix

       the instruction template itself tells us its data size */

    byte = 0;

    if ((bits != 16) && (insn->insn_flags & I_DATA_16)) byte = 0x66;
    if ((bits == 16) && (insn->insn_flags & I_DATA_32)) byte = 0x66;

    if (byte) emit(byte, 1);

    /* 2. address size override prefix 
    
       this relates to memory addressing, so the instruction template 
       doesn't know the address size - find the OPERAND_MEM and use that */

    byte = 0;
    for (i = 0; i < nr_operands; i++) {
        if (operands[i].kind != OPERAND_MEM) continue;

        if ((operands[i].flags & O_ADDR_64) && (bits != 64))
            error("64-bit address mode not available");

        if ((bits == 16) && (operands[i].flags & O_ADDR_32)) byte = 0x67;
        if ((bits == 32) && (operands[i].flags & O_ADDR_16)) byte = 0x67;
        if ((bits == 64) && (operands[i].flags & O_ADDR_32)) byte = 0x67;

        if ((bits == 64) && (operands[i].flags & O_ADDR_16))
            error("16-bit address mode not available");
    }
    if (byte) emit(byte, 1);

    /* 3. other prefixes */

    if (insn->insn_flags & I_PREFIX_F2) emit(0xF2, 1);
    if (insn->insn_flags & I_PREFIX_F3) emit(0xF3, 1);
    if (insn->insn_flags & I_PREFIX_66) emit(0x66, 1);

    /* 4. REX prefix is issued when:

       a. a 64-bit operand size is used, except for a handful of opcodes 
          where it's the default size (I_NO_DATA_REX).
       b. when the SIL, DIL, SPL or BPL registers are accessed (REG_LOW).
       c. when the high registers (R8-R15) are accessed. */

    if ((insn->insn_flags & I_DATA_64) && !(insn->insn_flags & I_NO_DATA_REX)) rex = 0x48; /* REX.W */

    for (i = 0; i < nr_operands; i++) {
        /* doesn't matter where the REG_LOW registers appear. if they appear, issue a REX. */

        if ((operands[i].kind == OPERAND_REG) && (operands[i].reg & REG_LOW)) rex |= 0x40; /* new low-byte regs */

        /* for straight register operands, the corresponding REX
           bit differs depending on how they are to be encoded. */

        if ((insn->operand_flags[i] & O_I_ENDREG) && (REG_DECODE(operands[i].reg) & 0x08)) rex |= 0x41; /* REX.B */
        if ((insn->operand_flags[i] & O_I_MIDREG) && (REG_DECODE(operands[i].reg) & 0x08)) rex |= 0x44; /* REX.R */

        if (insn->operand_flags[i] & O_I_MODRM) { 
            /* operand is OPERAND_REG or OPERAND_MEM- this works for either */
            if (REG_DECODE(operands[i].reg) & 0x08) rex |= 0x41; /* REX.B */
            if (REG_DECODE(operands[i].index) & 0x08) rex |= 0x42; /* REX.X */
        }
    }

    /* the high-byte registers AH, BH, CH, and DH are replaced by
       SPL, BPL, SIL and DIL when a REX prefix is issued. */

    for (i = 0; i < nr_operands; i++) 
        if ((operands[i].kind == OPERAND_REG) && (operands[i].reg & REG_HIGH) && rex)
            error("high-byte register(s) not available");

    if ((bits != 64) && rex) error("register(s) not available");
    if (rex) emit(rex, 1);

    /* 5. output opcode bytes.

       we output all but the last byte in the table, as the last byte
       is subject to modification when the operands are processed. */

    for (i = 0; i < (insn->nr_opcodes - 1); i++) emit(insn->opcodes[i], 1);
    byte = insn->opcodes[i];

    /* 6. process non-immediate operands.

       a. if an operand is marked O_I_ENDREG, its lower bits get 
          stuffed in opcode[2:0] (its high bit, if any, being in REX.B).
       b. if an operand is marked O_I_MIDREG, its lower bits get
          stuffed in opcode[5:3] (its high bit, if any, being in REX.R).
       c. if there's a mod/rm field, go process that out-of-line */

    for (i = 0; i < nr_operands; i++) {
        if (insn->operand_flags[i] & O_I_ENDREG)
            byte |= (REG_DECODE(operands[i].reg) & 0x07);

        if (insn->operand_flags[i] & O_I_MIDREG) 
            byte |= (REG_DECODE(operands[i].reg) & 0x07) << 3;
    }

    for (i = 0; i < nr_operands; i++)
        if (insn->operand_flags[i] & O_I_MODRM) {
            if (operands[i].kind == OPERAND_REG) {                         
                SET_MOD(byte, 3);
                SET_RM(byte, REG_DECODE(operands[i].reg));
                emit(byte, 1);
            } else
                modrm(i, byte);

            break;
        }

    if (i == nr_operands) emit(byte, 1);

    /* 7. "immediate" operands are output last. this includes:

       a. actual immediates, handed to reloc() for processing,
       b. O_MOFS_* operands, also given to reloc(),
       c. 8-byte relative addresses, which we emit directly */

    for (i = 0; i < nr_operands; i++) {
        if (insn->operand_flags[i] & O_IMM) 
            reloc(i, insn->operand_flags[i], (insn->operand_flags[i] & O_I_REL) ? 1 : 0);

        if (insn->operand_flags[i] & O_MOFS) reloc(i, operands[i].disp, 0);

        if (insn->operand_flags[i] & O_REL_8) {
            resolve(i, O_IMM_64, 1);
            emit(operands[i].offset, 1);
        }
        
    }
}

