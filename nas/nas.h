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
#include "../obj.h"

/* don't allow text or data segments larger than 256MB. more than
   7 hex digits will require a change to the listing format. */

#define MAX_BYTES           ((256 * 1024 * 1024) - 1)

/* an impossible address to tell us there isn't one */

#define NO_ADDRESS          -1

/* numbers marking the first and final passes. the distance between 
   them limits the total number of passes that can be made, but any
   more than a few passes indicates a serious problem, anyway. */

#define FIRST_PASS          1
#define FINAL_PASS          100

/* max number of opcode bytes encoded in the instruction table */

#define MAX_INSN_OPCODES    3

/* fully-qualified instructions are of variable length, between 1 and 15 bytes. */

#define MAX_INSN_LENGTH     15

/* maximum number of characters per input line. this is arbitrary, 
   but a fixed length simplifies the scanner considerably. */

#define MAX_INPUT_LINE      1024

/* no instructions with more than 3 operands (yet?) */

#define MAX_OPERANDS        3

/* tokens from scan(). tokens 1..127 represent their ASCII counterparts.
   registers also double as tokens, so their values must be distinct. */

#define NONE                0

#define NAME                128     /* classes */
#define PSEUDO              129
#define NUMBER              130
                                    
#define BYTE                140     /* keywords */
#define WORD                141
#define DWORD               142
#define QWORD               143

    /* each register value serves not only as a distinct token,
       but also as a store of data about that register. */

#define REG                 0x80000000      /* any register */
#define REG_LOW             0x40000000      /* new byte registers SIL, DIL etc. */
#define REG_HIGH            0x20000000      /* legacy high-byte registers AH, CH etc. */

#define REG_GP              0x0F000000      /* any GP register */
#define REG_8               0x08000000
#define REG_16              0x04000000
#define REG_32              0x02000000
#define REG_64              0x01000000
#define REG_SIZE            0x0F000000      /* GP register size */

#define REG_ENCODING        0x00F00000      /* 4 bits for register encoding */
#define REG_ENCODE(x)       (((x) << 20) & REG_ENCODING)
#define REG_DECODE(x)       (((x) & REG_ENCODING) >> 20)

#define REG_SEG2            0x00080000      /* SS, CS, DS, ES */
#define REG_SEG3            0x00040000      /* FS or GS */
#define REG_CRL             0x00020000      /* CR0-CR7 */
#define REG_CRH             0x00010000      /* CR8-CR15 */
#define REG_X               0x00008000      /* XMM0-15 */

#define REG_SEG             (REG_SEG2 | REG_SEG3)

#define REG_RIP             (REG | 150)     /* registers */

#define REG_AL              (REG | REG_8 | REG_ENCODE(0) | 151)
#define REG_AH              (REG | REG_8 | REG_HIGH | REG_ENCODE(4) | 152)
#define REG_AX              (REG | REG_16 | REG_ENCODE(0) | 153)
#define REG_EAX             (REG | REG_32 | REG_ENCODE(0) | 154)
#define REG_RAX             (REG | REG_64 | REG_ENCODE(0) | 155)

#define REG_BL              (REG | REG_8 | REG_ENCODE(3) | 156)
#define REG_BH              (REG | REG_8 | REG_HIGH | REG_ENCODE(7) | 157)
#define REG_BX              (REG | REG_16 | REG_ENCODE(3) | 158)
#define REG_EBX             (REG | REG_32 | REG_ENCODE(3) | 159)
#define REG_RBX             (REG | REG_64 | REG_ENCODE(3) | 160)

#define REG_CL              (REG | REG_8 | REG_ENCODE(1) | 161)
#define REG_CH              (REG | REG_8 | REG_HIGH | REG_ENCODE(5) | 162)
#define REG_CX              (REG | REG_16 | REG_ENCODE(1) | 163)
#define REG_ECX             (REG | REG_32 | REG_ENCODE(1) | 164)
#define REG_RCX             (REG | REG_64 | REG_ENCODE(1) | 165)

#define REG_DL              (REG | REG_8 | REG_ENCODE(2) | 166)
#define REG_DH              (REG | REG_8 | REG_HIGH | REG_ENCODE(6) | 167)
#define REG_DX              (REG | REG_16 | REG_ENCODE(2) | 168)
#define REG_EDX             (REG | REG_32 | REG_ENCODE(2) | 169)
#define REG_RDX             (REG | REG_64 | REG_ENCODE(2) | 170)

#define REG_BPL             (REG | REG_8 | REG_LOW | REG_ENCODE(5) | 171)
#define REG_BP              (REG | REG_16 | REG_ENCODE(5) | 172)
#define REG_EBP             (REG | REG_32 | REG_ENCODE(5) | 173)
#define REG_RBP             (REG | REG_64 | REG_ENCODE(5) | 174)

#define REG_SPL             (REG | REG_8 | REG_LOW | REG_ENCODE(4) | 175)
#define REG_SP              (REG | REG_16 | REG_ENCODE(4) | 176)
#define REG_ESP             (REG | REG_32 | REG_ENCODE(4) | 177)
#define REG_RSP             (REG | REG_64 | REG_ENCODE(4) | 178)

#define REG_SIL             (REG | REG_8 | REG_LOW | REG_ENCODE(6) | 179)
#define REG_SI              (REG | REG_16 | REG_ENCODE(6) | 180)
#define REG_ESI             (REG | REG_32 | REG_ENCODE(6) | 181)
#define REG_RSI             (REG | REG_64 | REG_ENCODE(6) | 182)

#define REG_DIL             (REG | REG_8 | REG_LOW | REG_ENCODE(7) | 183)
#define REG_DI              (REG | REG_16 | REG_ENCODE(7) | 184)
#define REG_EDI             (REG | REG_32 | REG_ENCODE(7) | 185)
#define REG_RDI             (REG | REG_64 | REG_ENCODE(7) | 186)

#define REG_XMM0            (REG | REG_X | REG_ENCODE(0) | 187)
#define REG_XMM1            (REG | REG_X | REG_ENCODE(1) | 188)
#define REG_XMM2            (REG | REG_X | REG_ENCODE(2) | 189)
#define REG_XMM3            (REG | REG_X | REG_ENCODE(3) | 190)
#define REG_XMM4            (REG | REG_X | REG_ENCODE(4) | 191)
#define REG_XMM5            (REG | REG_X | REG_ENCODE(5) | 192)
#define REG_XMM6            (REG | REG_X | REG_ENCODE(6) | 193)
#define REG_XMM7            (REG | REG_X | REG_ENCODE(7) | 194)
#define REG_XMM8            (REG | REG_X | REG_ENCODE(8) | 195)
#define REG_XMM9            (REG | REG_X | REG_ENCODE(9) | 196)
#define REG_XMM10           (REG | REG_X | REG_ENCODE(10) | 197)
#define REG_XMM11           (REG | REG_X | REG_ENCODE(11) | 198)
#define REG_XMM12           (REG | REG_X | REG_ENCODE(12) | 199)
#define REG_XMM13           (REG | REG_X | REG_ENCODE(13) | 200)
#define REG_XMM14           (REG | REG_X | REG_ENCODE(14) | 201)
#define REG_XMM15           (REG | REG_X | REG_ENCODE(15) | 202)

#define REG_R8B             (REG | REG_8 | REG_ENCODE(8) | 203)
#define REG_R8W             (REG | REG_16 | REG_ENCODE(8) | 204)
#define REG_R8D             (REG | REG_32 | REG_ENCODE(8) | 205)
#define REG_R8              (REG | REG_64 | REG_ENCODE(8) | 206)

#define REG_R9B             (REG | REG_8 | REG_ENCODE(9) | 207)
#define REG_R9W             (REG | REG_16 | REG_ENCODE(9) | 208)
#define REG_R9D             (REG | REG_32 | REG_ENCODE(9) | 209)
#define REG_R9              (REG | REG_64 | REG_ENCODE(9) | 210)

#define REG_R10B            (REG | REG_8 | REG_ENCODE(10) | 211)
#define REG_R10W            (REG | REG_16 | REG_ENCODE(10) | 212)
#define REG_R10D            (REG | REG_32 | REG_ENCODE(10) | 213)
#define REG_R10             (REG | REG_64 | REG_ENCODE(10) | 214)

#define REG_R11B            (REG | REG_8 | REG_ENCODE(11) | 215)
#define REG_R11W            (REG | REG_16 | REG_ENCODE(11) | 216)
#define REG_R11D            (REG | REG_32 | REG_ENCODE(11) | 217)
#define REG_R11             (REG | REG_64 | REG_ENCODE(11) | 218)

#define REG_R12B            (REG | REG_8 | REG_ENCODE(12) | 219)
#define REG_R12W            (REG | REG_16 | REG_ENCODE(12) | 220)
#define REG_R12D            (REG | REG_32 | REG_ENCODE(12) | 221)
#define REG_R12             (REG | REG_64 | REG_ENCODE(12) | 222)

#define REG_R13B            (REG | REG_8 | REG_ENCODE(13) | 223)
#define REG_R13W            (REG | REG_16 | REG_ENCODE(13) | 224)
#define REG_R13D            (REG | REG_32 | REG_ENCODE(13) | 225)
#define REG_R13             (REG | REG_64 | REG_ENCODE(13) | 226)

#define REG_R14B            (REG | REG_8 | REG_ENCODE(14) | 227)
#define REG_R14W            (REG | REG_16 | REG_ENCODE(14) | 228)
#define REG_R14D            (REG | REG_32 | REG_ENCODE(14) | 229)
#define REG_R14             (REG | REG_64 | REG_ENCODE(14) | 230)

#define REG_R15B            (REG | REG_8 | REG_ENCODE(15) | 231)
#define REG_R15W            (REG | REG_16 | REG_ENCODE(15) | 232)
#define REG_R15D            (REG | REG_32 | REG_ENCODE(15) | 233)
#define REG_R15             (REG | REG_64 | REG_ENCODE(15) | 234)

#define REG_CS              (REG | REG_SEG2 | REG_ENCODE(1) | 235)
#define REG_ES              (REG | REG_SEG2 | REG_ENCODE(0) | 236)
#define REG_DS              (REG | REG_SEG2 | REG_ENCODE(3) | 237)
#define REG_SS              (REG | REG_SEG2 | REG_ENCODE(2) | 238)
#define REG_FS              (REG | REG_SEG3 | REG_ENCODE(4) | 239)
#define REG_GS              (REG | REG_SEG3 | REG_ENCODE(5) | 240)

#define REG_CR0             (REG | REG_CRL | REG_ENCODE(0) | 241)
#define REG_CR1             (REG | REG_CRL | REG_ENCODE(1) | 242)
#define REG_CR2             (REG | REG_CRL | REG_ENCODE(2) | 243)
#define REG_CR3             (REG | REG_CRL | REG_ENCODE(3) | 244)
#define REG_CR4             (REG | REG_CRL | REG_ENCODE(4) | 245)
#define REG_CR5             (REG | REG_CRL | REG_ENCODE(5) | 246)
#define REG_CR6             (REG | REG_CRL | REG_ENCODE(6) | 247)
#define REG_CR7             (REG | REG_CRL | REG_ENCODE(7) | 248)
#define REG_CR8             (REG | REG_CRH | REG_ENCODE(0) | 249)
#define REG_CR9             (REG | REG_CRH | REG_ENCODE(1) | 250)
#define REG_CR10            (REG | REG_CRH | REG_ENCODE(2) | 251)
#define REG_CR11            (REG | REG_CRH | REG_ENCODE(3) | 252)
#define REG_CR12            (REG | REG_CRH | REG_ENCODE(4) | 253)
#define REG_CR13            (REG | REG_CRH | REG_ENCODE(5) | 254)
#define REG_CR14            (REG | REG_CRH | REG_ENCODE(6) | 255)
#define REG_CR15            (REG | REG_CRH | REG_ENCODE(7) | 256)

/* all identifiers end up in the name table: instruction mnemonics, pseudo
   ops, symbol names, register names, etc. the more buckets, the better. */

#define NR_NAME_BUCKETS     32

struct name
{
    int                 length;
    unsigned            hash;
    char              * data;
    struct name       * link;
    struct obj_symbol * symbol;
    struct insn       * insn_entries;
    int             ( * pseudo )();
    int                 token;
};

/* there are three basic kinds of operands: 

   OPERAND_REG: 'reg' 

   OPERAND_IMM: 'symbol' + offset 

   OPERAND_MEM: 
            [ 'rip' 'symbol' + 'offset' ]
      or    [ 'reg' + 'index' * 'scale' + 'symbol' + 'offset'] */

#define OPERAND_REG     0
#define OPERAND_IMM     1
#define OPERAND_MEM     2

    /* immediate operands. all the bits that apply
       to the actual immediate value are set. */

#define O_IMM_S8        0x0000000000000001L  
#define O_IMM_U8        0x0000000000000002L
#define O_IMM_S16       0x0000000000000004L
#define O_IMM_U16       0x0000000000000008L
#define O_IMM_S32       0x0000000000000010L
#define O_IMM_U32       0x0000000000000020L
#define O_IMM_64        0x0000000000000040L

    /* the O_REG_* are mutually exclusive, and refer to 
       the general purpose registers. AX et al are also O_ACC. */

#define O_REG_8         0x0000000000000080L     /* registers */
#define O_REG_16        0x0000000000000100L
#define O_REG_32        0x0000000000000200L
#define O_REG_64        0x0000000000000400L

#define O_ACC_8         0x0000000000000800L     /* AL */
#define O_ACC_16        0x0000000000001000L     /* AX */
#define O_ACC_32        0x0000000000002000L     /* EAX */
#define O_ACC_64        0x0000000000004000L     /* RAX */

    /* O_MEM_* cover memory "standard" memory references that
       can be represented in a mod/rm field. the suffix indicates
       the data size as indicated by the programmer, so they
       are mutually exclusive. */

#define O_MEM_8         0x0000000000008000L     
#define O_MEM_16        0x0000000000010000L     
#define O_MEM_32        0x0000000000020000L  
#define O_MEM_64        0x0000000000040000L

    /* O_MEM and O_MOFS references must have exactly one of O_ADDR_* 
       set, to indicate the address size. */

#define O_ADDR_16       0x0000000000080000L  
#define O_ADDR_32       0x0000000000100000L   
#define O_ADDR_64       0x0000000000200000L

    /* segment registers */

#define O_SREG2         0x0000000000400000L     /* 2-bit seg reg: SS, DS, ES, CS */
#define O_SREG3         0x0000000000800000L     /* 3-bit seg reg: FS, GS */

#define O_REL_8         0x0000000001000000L     /* 8-bit relative/immediate for short jumps */

    /* non-standard memory references (direct displacements). 
       only used to match in legacy MOV to/from accumulator */

#define O_MOFS_8        0x0000000002000000L     
#define O_MOFS_16       0x0000000004000000L     
#define O_MOFS_32       0x0000000008000000L  
#define O_MOFS_64       0x0000000010000000L

    /* high (8-15) and low (0-7) control registers */

#define O_REG_CRL       0x0000000020000000L
#define O_REG_CRH       0x0000000040000000L

    /* specifically CL (for shifts) */

#define O_REG_CL        0x0000000080000000L

    /* XMM registers */

#define O_REG_XMM       0x0000000100000000L

    /* useful classes */

#define O_IMM_8         (O_IMM_S8 | O_IMM_U8)
#define O_IMM_16        (O_IMM_S16 | O_IMM_U16)
#define O_IMM_32        (O_IMM_S32 | O_IMM_U32)
#define O_IMM           (O_IMM_8 | O_IMM_16 | O_IMM_32 | O_IMM_64)   

#define O_MRM_8         (O_MEM_8 | O_REG_8)     /* operands suitable for mod/rm */
#define O_MRM_16        (O_MEM_16 | O_REG_16)
#define O_MRM_32        (O_MEM_32 | O_REG_32)
#define O_MRM_64        (O_MEM_64 | O_REG_64)

#define O_MEM           (O_MEM_8 | O_MEM_16 | O_MEM_32 | O_MEM_64 )
#define O_MOFS          (O_MOFS_8 | O_MOFS_16 | O_MOFS_32 | O_MOFS_64 )

    /* these are in the same bitspace as operand.flags, but only ever 
       appear in insn.operand_flags[] to aid in matching or encoding. */

#define O_I_ENDREG      0x8000000000000000L     /* xxxxxRRR and REX.B */
#define O_I_MIDREG      0x4000000000000000L     /* xxRRRxxx and REX.R */
#define O_I_MODRM       0x2000000000000000L     /* MMxxxRRR and REX.B/REX.X */
#define O_I_REL         0x1000000000000000L     /* emit this immediate as rIP-relative */

struct operand
{
    int                 kind;           /* OPERAND_* */
    int                 reg;       
    int                 rip;
    int                 index;
    int                 scale;
    struct obj_symbol * symbol;
    long                offset;
    long                flags;          /* O_* */
    long                disp;           /* O_IMM_* indicating displacement size */
};

/* the instruction table */

struct insn
{
    char        * mnemonic;
    int           nr_operands;
    long          operand_flags[MAX_OPERANDS];      /* O_* and O_I_* */
    int           nr_opcodes;
    char          opcodes[MAX_INSN_OPCODES];
    long          insn_flags;                       /* I_* */

    struct name * name;
};

    /* instruction flags help with either matching or encoding */

#define I_DATA_8        0x0000000000000001L     /* operand size */
#define I_DATA_16       0x0000000000000002L  
#define I_DATA_32       0x0000000000000004L  
#define I_DATA_64       0x0000000000000008L

#define I_PREFIX_66     0x0200000000000000L     /* 0x66 prefix (precedes REX) */
#define I_PREFIX_F3     0x0400000000000000L     /* 0xF3 prefix (precedes REX) */
#define I_PREFIX_F2     0x0800000000000000L     /* 0xF2 prefix (precedes REX) */

#define I_NO_DATA_REX   0x1000000000000000L     /* 64-bit operand default: no REX for 64-bit data */
#define I_NO_BITS_16    0x2000000000000000L     /* instruction not available in .bits 16 */
#define I_NO_BITS_32    0x4000000000000000L     /* instruction not available in .bits 32 */
#define I_NO_BITS_64    0x8000000000000000L     /* instruction not available in .bits 64 */

extern struct insn       insns[];
extern int               pass;
extern int               line_number;
extern int               input_index;
extern char           ** input_paths;
extern int               token;
extern struct name     * name_token;
extern long              number_token;
extern char              input_line[];
extern char            * input_pos;
extern FILE            * list_file;
extern FILE            * output_file;
extern int               segment;
extern int               text_bytes;
extern int               data_bytes;
extern int               nr_symbols;
extern int               nr_symbol_changes;
extern int               nr_relocs;
extern int               name_bytes;
extern int               base_address;
extern int               bits;
extern struct insn     * insn;
extern struct operand    operands[];
extern int               nr_operands;

extern struct name     * lookup_name();
extern long              constant_expression();
extern long              classify();
extern                   pseudo_byte();
extern                   pseudo_word();
extern                   pseudo_dword();
extern                   pseudo_qword();
extern                   pseudo_align();
extern                   pseudo_skip();
extern                   pseudo_fill();
extern                   pseudo_ascii();
extern                   pseudo_global();
extern                   pseudo_text();
extern                   pseudo_data();
extern                   pseudo_bss();
extern                   pseudo_org();
extern                   pseudo_bits();
extern struct obj_header header;

#ifdef __STDC__
extern void error(char *, ...);
#endif

