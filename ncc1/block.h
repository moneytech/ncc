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

#define B_SEQ           0x00000001          /* sequenced */
#define B_REG           0x00000002          /* registers allocated */
#define B_RECON         0x00000004          /* reconciliation block */

struct block
{
    int                 asm_label;
    int                 bs;
    int                 loop_level;
    int                 nr_insns;
    struct block      * previous;   
    struct block      * next;
    struct insn       * first_insn; 
    struct insn       * last_insn;
    struct block_list * successors;
    struct block_list * predecessors;
    int                 nr_successors;
    int                 nr_predecessors;
    struct defuse     * defuses;
    struct symbol     * iregs[NR_REGS];
    struct symbol     * fregs[NR_REGS];

    /* the prohibit_* fields are (real) register bitmasks (1 << R_IDX(x)) 
       determined in analyze_block() that tell the allocator which registers
       aren't available in this block.  similarly, the temponly_* bitmasks
       indicate which registers are only available for temporaries (DU_TEMP). */

    int                 prohibit_iregs;  
    int                 prohibit_fregs;
    int                 temponly_iregs;  
    int                 temponly_fregs;
};

/* for successors, 'cc' is the branch condition that leads to
   the successor. (predecessors have 'cc' = CC_NONE.)

   rules regarding block successors:

   0 successors: only the exit_block has no successors.
   1 successor: 'cc' for the one successor is ALWAYS.
   2 successors: the 'cc's are opposite conditions. */

struct block_list
{
    int                 cc;
    struct block *      block;
    struct block_list * link;
};

/* CC_* represent AMD64 branch conditions. their values are 
   important for two reasons:

   1. they're used as indices to select instructions,
      (e.g., I_SETZ + CC_LE = I_SETLE, etc.) and
   2. they're used as table indexes in output.c,
   3. opposite conditions differ in the LSB only, so 
      CC_INVERT can toggle between them easily.

   CC_NEVER obviously never makes sense for an actual
   successor. */
  

#define CC_INVERT(x)    ((x) ^ 1)

#define CC_Z            0
#define CC_NZ           1
#define CC_G            2
#define CC_LE           3
#define CC_GE           4
#define CC_L            5
#define CC_A            6
#define CC_BE           7
#define CC_AE           8 
#define CC_B            9 
#define CC_ALWAYS       10
#define CC_NEVER        11

#define CC_NONE         12

/* def/use, live variable information tracking and other sundries. */

struct defuse
{
    struct symbol * symbol;        
    struct defuse * link;
    int             dus;        /* DU_* */
    int             reg;
    int             cache;      /* DU_CACHE */
    int             con;        /* if DU_CON; see con_prop() [opt.c] */
            
    /* first_n and last_n give the insn indexes (insn->n) of the first
       and last appearances of the symbol in the block.  if the symbol 
       doesn't actually appear in the block, these are 0. */

    int             first_n, last_n;    

    /* for transit variables, 'distance' indicates how far to the 
       next reference of the variable. it's a heuristic value, but  
       smaller means closer. */

    int             distance;
};

#define DU_IN           0x00000001      /* live in and/or out */
#define DU_OUT          0x00000002      
#define DU_DEF          0x00000004      /* def or use */
#define DU_USE          0x00000008
#define DU_CON          0x00000010      /* for const_prop() [opt.c] */

    /* the register allocator uses these fields to track the coherency
       between the ->reg and memory, for aliased (i.e., non S_REGISTER) */

#define DU_CACHE_CLEAN      0           /* register and memory agree */
#define DU_CACHE_DIRTY      1           /* register valid, memory out-of-date */
#define DU_CACHE_INVALID    2           /* register invalid, memory up-to-date */

    /* a variable is deemed a 'temporary' if it is neither live in nor 
       live out, and its storage class is S_REGISTER. its entire lifespan 
       is [first_n..last_n] in this block. */

#define DU_TEMP(x)      (!(((x).dus) & (DU_IN | DU_OUT)) && ((x).symbol->ss & S_REGISTER))

    /* a transit variable is not referenced in this block. */

#define DU_TRANSIT(x)   (!(((x).dus) & (DU_DEF | DU_USE)))

/* an instruction is an opcode with up to 3 operands, which are 
   expression trees, but limited to E_REG, E_MEM, E_IMM, E_CON. */

#define NR_INSN_OPERANDS    3
#define NR_INSN_REGS        (NR_INSN_OPERANDS * 2)  /* maximum # of registers referenced in one insn */

#define INSN_FLAG_CC    0x00000001  /* condition codes from this insn used */

struct insn
{
    struct insn * previous;
    struct insn * next;

    /* the opcode and operands are sufficient to describe the
       instruction for output to the assembler. */

    int           opcode; /* I_* */
    struct tree * operand[NR_INSN_OPERANDS];

    /* these fields aren't valid until we begin analyis
       before optimization and register allocation */

    int           flags;                        /* INSN_FLAG_* */
    int           n;                            /* insn # in block (1..2..3..) */
    char          mem_used;                     /* memory read? */
    char          mem_defd;                     /* memory written? */
    int           regs_used[NR_INSN_REGS];      /* USEd regs */
    int           regs_defd[NR_INSN_REGS];      /* DEFd regs */
};

    /* I[7:0] are unique, and serve as 
       indices into insns[] in output.c */

#define I_IDX(x)                ((x) & 0xFF)

    /* I[9:8] encode the number of operands */

#define I_NR_OPERANDS(x)        (((x) >> 8) & 0x03)
#define I_0_OPERANDS            (0 << 8)
#define I_1_OPERANDS            (1 << 8)
#define I_2_OPERANDS            (2 << 8)
#define I_3_OPERANDS            (3 << 8) /* none yet */

    /* I[12:10] are the def bits, I[15:13] are the use bits */

#define I_DEF(i)                (1 << (10 + (i)))
#define I_USE(i)                (1 << (13 + (i)))

    /* I[23:16] are for implicit def/use */

#define I_DEF_AX                (1 << 16)
#define I_DEF_DX                (1 << 17)
#define I_DEF_CX                (1 << 18)
#define I_DEF_XMM0              (1 << 19)
#define I_USE_AX                (1 << 20)
#define I_USE_DX                (1 << 21)
#define I_DEF_MEM               (1 << 22)
#define I_USE_MEM               (1 << 23)

    /* I[24] indicates if this instruction modifies CCs */
    /* I[25] indicates if this instruction uses CCs */

#define I_DEF_CC                (1 << 24)
#define I_USE_CC                (1 << 25)

    /* the instructions */

#define I_NOP       (   0 | I_0_OPERANDS )
#define I_MOV       (   1 | I_2_OPERANDS | I_DEF(0) | I_USE(1) )
#define I_MOVSX     (   2 | I_2_OPERANDS | I_DEF(0) | I_USE(1) )
#define I_MOVZX     (   3 | I_2_OPERANDS | I_DEF(0) | I_USE(1) )
#define I_MOVSS     (   4 | I_2_OPERANDS | I_DEF(0) | I_USE(1) )
#define I_MOVSD     (   5 | I_2_OPERANDS | I_DEF(0) | I_USE(1) )
#define I_LEA       (   6 | I_2_OPERANDS | I_DEF(0) | I_USE(1) )
#define I_CMP       (   7 | I_2_OPERANDS | I_USE(0) | I_USE(1) | I_DEF_CC )
#define I_UCOMISS   (   8 | I_2_OPERANDS | I_USE(0) | I_USE(1) | I_DEF_CC )
#define I_UCOMISD   (   9 | I_2_OPERANDS | I_USE(0) | I_USE(1) | I_DEF_CC )
#define I_PXOR      (  10 | I_2_OPERANDS | I_DEF(0) | I_USE(0) | I_USE(1) )
#define I_CVTSS2SI  (  11 | I_2_OPERANDS | I_DEF(0) | I_USE(1) )
#define I_CVTSD2SI  (  12 | I_2_OPERANDS | I_DEF(0) | I_USE(1) )
#define I_CVTSI2SS  (  13 | I_2_OPERANDS | I_DEF(0) | I_USE(1) )
#define I_CVTSI2SD  (  14 | I_2_OPERANDS | I_DEF(0) | I_USE(1) )
#define I_CVTSS2SD  (  15 | I_2_OPERANDS | I_DEF(0) | I_USE(1) )
#define I_CVTSD2SS  (  16 | I_2_OPERANDS | I_DEF(0) | I_USE(1) )
#define I_SHL       (  17 | I_2_OPERANDS | I_DEF(0) | I_USE(0) | I_USE(1) | I_DEF_CC )
#define I_SHR       (  18 | I_2_OPERANDS | I_DEF(0) | I_USE(0) | I_USE(1) | I_DEF_CC )
#define I_SAR       (  19 | I_2_OPERANDS | I_DEF(0) | I_USE(0) | I_USE(1) | I_DEF_CC )
#define I_ADD       (  20 | I_2_OPERANDS | I_DEF(0) | I_USE(0) | I_USE(1) | I_DEF_CC )
#define I_ADDSS     (  21 | I_2_OPERANDS | I_DEF(0) | I_USE(0) | I_USE(1) )
#define I_ADDSD     (  22 | I_2_OPERANDS | I_DEF(0) | I_USE(0) | I_USE(1) )
#define I_SUB       (  23 | I_2_OPERANDS | I_DEF(0) | I_USE(0) | I_USE(1) | I_DEF_CC )
#define I_SUBSS     (  24 | I_2_OPERANDS | I_DEF(0) | I_USE(0) | I_USE(1) )
#define I_SUBSD     (  25 | I_2_OPERANDS | I_DEF(0) | I_USE(0) | I_USE(1) )
#define I_IMUL      (  26 | I_2_OPERANDS | I_DEF(0) | I_USE(0) | I_USE(1) | I_DEF_CC )
#define I_MULSS     (  27 | I_2_OPERANDS | I_DEF(0) | I_USE(0) | I_USE(1) )
#define I_MULSD     (  28 | I_2_OPERANDS | I_DEF(0) | I_USE(0) | I_USE(1) )
#define I_OR        (  29 | I_2_OPERANDS | I_DEF(0) | I_USE(0) | I_USE(1) | I_DEF_CC )
#define I_XOR       (  30 | I_2_OPERANDS | I_DEF(0) | I_USE(0) | I_USE(1) | I_DEF_CC )
#define I_AND       (  31 | I_2_OPERANDS | I_DEF(0) | I_USE(0) | I_USE(1) | I_DEF_CC )
#define I_CDQ       (  32 | I_0_OPERANDS | I_USE_AX | I_DEF_DX ) 
#define I_CQO       (  33 | I_0_OPERANDS | I_USE_AX | I_DEF_DX )
#define I_DIV       (  34 | I_1_OPERANDS | I_USE(0) | I_USE_AX | I_DEF_AX | I_USE_DX | I_DEF_DX | I_DEF_CC )
#define I_IDIV      (  35 | I_1_OPERANDS | I_USE(0) | I_USE_AX | I_DEF_AX | I_USE_DX | I_DEF_DX | I_DEF_CC )
#define I_DIVSS     (  36 | I_2_OPERANDS | I_DEF(0) | I_USE(0) | I_USE(1) )
#define I_DIVSD     (  37 | I_2_OPERANDS | I_DEF(0) | I_USE(0) | I_USE(1) )
#define I_CBW       (  38 | I_0_OPERANDS | I_USE_AX | I_DEF_AX )
#define I_CWD       (  39 | I_0_OPERANDS | I_USE_AX | I_DEF_AX | I_DEF_DX )
#define I_SETZ      (  40 | I_1_OPERANDS | I_DEF(0) | I_USE_CC )
#define I_SETNZ     (  41 | I_1_OPERANDS | I_DEF(0) | I_USE_CC )
#define I_SETG      (  42 | I_1_OPERANDS | I_DEF(0) | I_USE_CC )
#define I_SETLE     (  43 | I_1_OPERANDS | I_DEF(0) | I_USE_CC )
#define I_SETGE     (  44 | I_1_OPERANDS | I_DEF(0) | I_USE_CC )
#define I_SETL      (  45 | I_1_OPERANDS | I_DEF(0) | I_USE_CC )
#define I_SETA      (  46 | I_1_OPERANDS | I_DEF(0) | I_USE_CC )
#define I_SETBE     (  47 | I_1_OPERANDS | I_DEF(0) | I_USE_CC )
#define I_SETAE     (  48 | I_1_OPERANDS | I_DEF(0) | I_USE_CC )
#define I_SETB      (  49 | I_1_OPERANDS | I_DEF(0) | I_USE_CC )
#define I_NOT       (  50 | I_1_OPERANDS | I_DEF(0) | I_USE(0) )
#define I_NEG       (  51 | I_1_OPERANDS | I_DEF(0) | I_USE(0) | I_DEF_CC )
#define I_PUSH      (  52 | I_1_OPERANDS | I_USE(0) )
#define I_POP       (  53 | I_1_OPERANDS | I_DEF(0) )
#define I_CALL      (  54 | I_1_OPERANDS | I_USE(0) | I_DEF_AX | I_DEF_CX | I_DEF_DX | I_DEF_XMM0 | I_DEF_CC )
#define I_TEST      (  55 | I_2_OPERANDS | I_USE(0) | I_USE(1) | I_DEF_CC )
#define I_RET       (  56 | I_0_OPERANDS )
#define I_INC       (  57 | I_1_OPERANDS | I_DEF(0) | I_USE(0) | I_DEF_CC )
#define I_DEC       (  58 | I_1_OPERANDS | I_DEF(0) | I_USE(0) | I_DEF_CC )
