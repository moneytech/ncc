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

#include <string.h>
#include "ncc1.h"

/* we need a register for the symbol in this block referenced in the defuse.
   updates iregs[] and fregs[] (local and block) and the defuse as required. 
   returns R_NONE if allocation fails. */

static
map_reg(block, defuse, iregs, fregs)
    struct block  * block;
    struct defuse * defuse;
    struct symbol * iregs[];
    struct symbol * fregs[];
{
    struct symbol ** regs;
    struct symbol ** block_regs;
    struct defuse  * transit_defuse;
    int              prohibit;
    int              temponly;
    int              base_reg;
    int              i;
    int              distance;
    int              reg = R_NONE;

    if (defuse->reg != R_NONE) return defuse->reg;

    if (defuse->symbol->reg & R_IS_FLOAT) { 
        base_reg = R_XMM0;
        regs = fregs;
        block_regs = block->fregs;
        prohibit = block->prohibit_fregs;
        temponly = block->temponly_fregs;
    } else {
        base_reg = R_AX;
        regs = iregs;
        block_regs = block->iregs;
        prohibit = block->prohibit_iregs;
        temponly = block->temponly_iregs;
    }

    /* first, the symbol may already be mapped: if it's in regs[], 
       then it's been inherited from a predecessor. */

    for (i = 0; i < NR_REGS; i++) {
        if (regs[i] == defuse->symbol) {
            reg = base_reg + i;
            break;
        }
    }

    /* if the symbol is a temporary, we try 
       to find an empty temporary for it first */

    if ((reg == R_NONE) && DU_TEMP(*defuse)) {
        for (i = 0; i < NR_REGS; i++) {
            if (!(temponly & (1 << i))) continue;
            if (prohibit & (1 << i)) continue;
            if (regs[i]) continue;
            reg = base_reg + i;
            break;
        }
    }

    /* find any empty register */

    if (reg == R_NONE) {
        for (i = 0; i < NR_REGS; i++) {
            if (regs[i]) continue;
            if (temponly & (1 << i)) continue;
            if (prohibit & (1 << i)) continue;
            reg = base_reg + i;
            break;
        }
    }

    /* nothing free, try to evict a transit variable */

    if (reg == R_NONE) {
        distance = -1;
        for (i = 0; i < NR_REGS; i++) {
            if (temponly & (1 << i)) continue;
            if (prohibit & (1 << i)) continue;
            transit_defuse = find_defuse_by_symbol(block, regs[i]);
            if (DU_TRANSIT(*transit_defuse) && (transit_defuse->distance > distance)) {
                reg = base_reg + i;
                distance = transit_defuse->distance;
                break;
            }
        }
    }

    if (reg != R_NONE) {
        regs[R_IDX(reg)] = defuse->symbol;

        if (DU_TEMP(*defuse)) 
            block_regs[R_IDX(reg)] = NULL;
        else
            block_regs[R_IDX(reg)] = defuse->symbol;

        defuse->reg = reg;
    }

    return reg;
}

/* remove symbols (allocated) from the register set 'regs' if:

   1. the symbol isn't in this block 
   2. the symbol is a temporary and its life is over */

static 
cull(block, n, regs)
    struct block  * block;
    struct symbol * regs[];
{
    struct defuse * defuse;
    int             i;

    for (i = 0; i < NR_REGS; i++) {
        defuse = find_defuse_by_symbol(block, regs[i]);

        if (!defuse || (DU_TEMP(*defuse) && (defuse->last_n <= n))) 
            regs[i] = NULL;
    }
}

/* first pass - returns non-zero on success, or zero if a split is required. */

static
select_regs(block)
    struct block * block;
{
    struct defuse    * defuse;
    struct insn      * insn;
    int                n;
    struct block     * successor;
    struct symbol    * iregs[NR_REGS];
    struct symbol    * fregs[NR_REGS];
    
    block->bs |= B_REG;

    /* eliminate any symbols that don't exist in this block.
       we have inherited them from a predecessor, or they 
       disappeared when we split. */

    cull(block, 0, block->iregs);
    cull(block, 0, block->fregs);

    /* the difference between the block regs and the local regs concerns
       temporaries (DU_TEMPs): they're never in the block regs. */

    memcpy(iregs, block->iregs, sizeof(iregs));
    memcpy(fregs, block->fregs, sizeof(fregs));

    /* first, allocate all the non-DU_TEMPS. they must have the same
       assignment for the whole block. */

    for (defuse = block->defuses; defuse; defuse = defuse->link) {
        if (DU_TEMP(*defuse)) continue;
        if (map_reg(block, defuse, iregs, fregs) == R_NONE) return 0;
    }

    /* iterate over instructions, allocating and culling temporaries as 
       needed. if we run out of registers, split the block and start over. */

    for (insn = block->first_insn; insn; insn = insn->next) {
        for (defuse = block->defuses; defuse; defuse = defuse->link) {
            if (DU_TRANSIT(*defuse)) continue;
            if (!DU_TEMP(*defuse)) continue;
            if (defuse->reg != R_NONE) continue;
            if (!insn_touches_reg(insn, defuse->symbol->reg)) continue;
            if (map_reg(block, defuse, iregs, fregs) == R_NONE) return 0; 
        }

        cull(block, insn->n, iregs);
        cull(block, insn->n, fregs);
    }
    
    /* push register state down to any successors who've not had
       the allocator run on them yet. */

    for (n = 0; successor = block_successor(block, n); ++n) 
        if (!(successor->bs & B_REG)) {
            memcpy(successor->iregs, block->iregs, sizeof(iregs));
            memcpy(successor->fregs, block->fregs, sizeof(fregs));
        }

    return 1; /* success */
}

/* insert an insn in 'block' (before insn 'before') 
   to spill or restore a register to or from memory. */

#define SPILL_IN    0       /* memory -> reg */
#define SPILL_OUT   1       /* reg -> memory */

spill(block, defuse, before, dir)
    struct block  * block;
    struct defuse * defuse;
    struct insn   * before;
{
    struct tree * memory;
    struct tree * reg;
    struct insn * insn;
    int           opcode;

    memory = memory_tree(defuse->symbol);
    reg = reg_tree(defuse->reg, copy_type(memory->type));
    opcode = I_MOV;
    if (reg->type->ts & T_FLOAT) opcode = I_MOVSS;
    if (reg->type->ts & T_LFLOAT) opcode = I_MOVSD;

    if (dir == SPILL_IN) 
        insn = new_insn(opcode, reg, memory);
    else
        insn = new_insn(opcode, memory, reg);

    put_insn(block, insn, before);
}

/* the second pass has three main responsibilities:

   1. to rewrite the pseudo-registers in each instruction with 
      the real registers allocated to them in the first pass,
   2. track which registers are used in the function, so we can
      save/restore the callee-save registers, and
   3. insert appropriate memory accesses for aliased variables.

   there's a lot of room for optimization here w/r/t #3. in particular,
   in many cases a load or store could be fused with another instruction
   (since AMD64 supports memory operands). TO-DO. */

static 
rewrite(block)
    struct block * block;
{
    struct insn   * insn;
    struct defuse * defuse;
    int             i;

    for (insn = block->first_insn; insn; insn = insn->next) {
        for (defuse = block->defuses; defuse; defuse = defuse->link) {
            if (defuse->reg == R_NONE) continue;

            if (!(defuse->symbol->ss & S_REGISTER)) {
                /* rules for aliased variables .. */

                /* before a memory read or write (or I_CALL): DIRTY -> spill -> CLEAN */
                if ((insn->mem_used || insn->mem_defd) && (defuse->cache == DU_CACHE_DIRTY)) {
                    spill(block, defuse, insn, SPILL_OUT);
                    defuse->cache = DU_CACHE_CLEAN;
                }

                /* before the reg is USEd, INVALID -> restore -> CLEAN */
                if (insn_uses_reg(insn, defuse->symbol->reg) && (defuse->cache == DU_CACHE_INVALID)) {
                    spill(block, defuse, insn, SPILL_IN);
                    defuse->cache = DU_CACHE_CLEAN;
                }

                /* after the reg is DEFd, * -> DIRTY */
                if (insn_defs_reg(insn, defuse->symbol->reg)) defuse->cache = DU_CACHE_DIRTY;

                /* after a memory write (or I_CALL), * -> INVALID */
                if (insn->mem_defd) defuse->cache = DU_CACHE_INVALID;
            } 

            /* if the register appears in this instruction, substitute
               the real register for the pseudo register and mark the 
               real register as used in this function */

            if (insn_touches_reg(insn, defuse->symbol->reg)) {
                insn_replace_reg(insn, defuse->symbol->reg, defuse->reg);

                if (defuse->reg & R_IS_FLOAT) 
                    save_fregs |= 1 << R_IDX(defuse->reg);
                else
                    save_iregs |= 1 << R_IDX(defuse->reg);
            }
        }
    }

    /* any aliased variables that are still DIRTY have to be spilled. */

    for (defuse = block->defuses; defuse; defuse = defuse->link) {
        if (defuse->reg == R_NONE) continue;
        if (defuse->symbol->ss & S_REGISTER) continue;
        if (defuse->cache != DU_CACHE_DIRTY) continue; 
        spill(block, defuse, NULL, SPILL_OUT);
    }
}

/* find registers in the 'from' block that don't match 'to'.
   if they're live in to the 'to' block, we need to spill them. 
   exception: never spill out of the entry block - registers hold junk. */

static
recon1(from, from_regs, to, to_regs, spills)
    struct block  * from;
    struct symbol * from_regs[];
    struct block  * to;
    struct symbol * to_regs[];
    struct block  * spills;
{
    struct defuse * defuse;
    int             i;

    if (from != entry_block) {
        for (i = 0; i < NR_REGS; i++) {
            if (!from_regs[i]) continue;
            if (!(from_regs[i]->ss & S_REGISTER)) continue;
            if (from_regs[i] == to_regs[i]) continue;
            defuse = find_defuse_by_symbol(to, from_regs[i]);

            if (defuse && (defuse->dus & DU_IN)) {
                defuse = find_defuse_by_symbol(from, from_regs[i]);
                spill(spills, defuse, NULL, SPILL_OUT);
            }
        }
    }
}

/* the opposite problem: registers in 'to' that don't match
   'from', must be restored if they're live in.  again, we 
   don't trust the entry block: it has no values loaded in 
   registers, so always spill them in. */

static
recon2(from, from_regs, to, to_regs, spills)
    struct block  * from;
    struct symbol * from_regs[];
    struct block  * to;
    struct symbol * to_regs[];
    struct block  * spills;
{
    struct defuse * defuse;
    int             i;

    for (i = 0; i < NR_REGS; i++) {
        if (!to_regs[i]) continue;
        if (!(to_regs[i]->ss & S_REGISTER)) continue;
        if ((from_regs[i] == to_regs[i]) && (from != entry_block)) continue;
        defuse = find_defuse_by_symbol(to, to_regs[i]);
        if (defuse && (defuse->dus & DU_IN)) spill(spills, defuse, NULL, SPILL_IN);
    }
}

reconcile(from, n)
    struct block * from;
{
    struct block  * spills;
    struct block  * to;
    int             cc;

    to = block_successor(from, n);
    spills = new_block();
    spills->bs |= B_RECON;

    recon1(from, from->iregs, to, to->iregs, spills);   /* spill out */
    recon1(from, from->fregs, to, to->fregs, spills);
    recon2(from, from->iregs, to, to->iregs, spills);   /* spill in */
    recon2(from, from->fregs, to, to->fregs, spills);

    /* if we issued any spill instructions, we need to insert the spill block 
       between 'from' and 'to'. otherwise, just discard it. */

    if (spills->nr_insns) {
        cc = block_successor_cc(from, n);
        unsucceed_block(from, n);
        succeed_block(from, cc, spills);
        succeed_block(spills, CC_ALWAYS, to);
    } else
        free_block(spills);

}

allocate_regs()
{
    struct block * block;
    int            n;

    /* repeat register allocation until it succeeds (no splits required) */

  restart:
    sequence_blocks();
    compute_global_defuses();

    for (block = first_block; block; block = block->next) {
        if (!select_regs(block)) {
            split_block(block);
            goto restart;
        }
    }

    /* rewrite blocks with the chosen registers and record
       which registers the callee needs to save. */

    save_iregs = 0;
    save_fregs = 0;

    for (block = first_block; block; block = block->next) rewrite(block);

    save_iregs &= ~((1 << R_IDX(R_AX)) | (1 << R_IDX(R_CX)) | (1 << R_IDX(R_DX)));
    save_iregs &= ~((1 << R_IDX(R_BP)) | (1 << R_IDX(R_SP)));
    save_fregs &= ~(1 << R_IDX(R_XMM0));

    /* finally, reconcile each block's registers with its successors'. */

    for (block = first_block; block; block = block->next) {
        if (block->bs & B_RECON) continue;
        for (n = 0; block_successor(block, n); ++n) reconcile(block, n);
    }

    sequence_blocks();
}
