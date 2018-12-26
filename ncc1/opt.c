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

/* simple jump optimization -- replace jumps to empty blocks with
   unconditional successors with direct jumps to those successors.
   this is really a requirement for half-decent code, given the
   abandon with which the parser generates empty blocks. as such,
   it's run whether -O is given or not. */

static
jumps()
{
    struct block * block;
    struct block * successor;
    struct block * successor_successor;
    int            cc;
    int            n;
    int            changes;

    do {
        changes = 0;
        for (block = first_block; block; block = block->next) {
            for (n = 0; successor = block_successor(block, n); ++n) {
                if ((successor->nr_insns == 0) && (successor->nr_successors == 1)) {
                    successor_successor = block_successor(successor, 0);
                    cc = block_successor_cc(successor, 0);

                    if ((cc == CC_ALWAYS) && (successor != successor_successor)) {
                        cc = block_successor_cc(block, n);
                        unsucceed_block(block, n);
                        succeed_block(block, cc, successor_successor);
                        ++changes;
                    }
                }
            }
        }
    } while (changes);
}

/* remove unreachable code, i.e., blocks with no predecessors. 
   we never remove the entry or exit blocks. like jumps(), this
   is run whether -O is given or not, and for the same reason. */

static
unreachable()
{
    struct block * block;
    struct block * successor;

    again:
    for (block = first_block; block; block = block->next) {
        if (block == entry_block) continue;
        if (block == exit_block) continue;

        if (block->nr_predecessors == 0) {
            while (successor = block_successor(block, 0))
                unsucceed_block(block, 0);

            free_block(block);
            goto again; 
        }
    }
}


/* convert S_LOCALs to S_REGISTER. error on any undefined labels. */

static
walk1(symbol)
    struct symbol * symbol;
{
    if (symbol->ss & S_LOCAL) {
        symbol->ss &= ~S_LOCAL;
        symbol->ss |= S_REGISTER;
    }

    if ((symbol->ss & S_LABEL) && !(symbol->ss & S_DEFINED))
        error(ERROR_DANGLING);
}

/* generate function prologue and epilogue in entry and exit blocks */

static
logues()
{
    struct tree   * reg;
    int             i;
    long            locals;
    struct tree   * floats[NR_REGS];
    struct symbol * tmp;

    for (i = 0; i < NR_REGS; i++) {
        if (save_fregs & (1 << i)) {
            tmp = temporary_symbol(new_type(T_LFLOAT));
            floats[i] = memory_tree(tmp);
        } else
            floats[i] = NULL;
    }

    frame_offset = ROUND_UP(frame_offset, FRAME_ALIGN);
    locals = frame_offset;

    put_insn(entry_block, new_insn(I_PUSH, reg_tree(R_BP, new_type(T_LONG))), NULL);
    put_insn(entry_block, new_insn(I_MOV, reg_tree(R_BP, new_type(T_LONG)), reg_tree(R_SP, new_type(T_LONG))), NULL);
    if (locals) put_insn(entry_block, new_insn(I_SUB, reg_tree(R_SP, new_type(T_LONG)), int_tree(T_LONG, locals)), NULL);
    
    for (i = 0; i < NR_REGS; i++) {
        if (save_iregs & (1 << i)) {
            reg = reg_tree(R_AX + i, new_type(T_LONG));
            put_insn(entry_block, new_insn(I_PUSH, copy_tree(reg)), NULL);
            put_insn(exit_block, new_insn(I_POP, reg), exit_block->first_insn);
        }

        if (save_fregs & (1 << i)) {
            reg = reg_tree(R_XMM0 + i, new_type(T_LFLOAT));
            put_insn(entry_block, new_insn(I_MOVSD, copy_tree(floats[i]), copy_tree(reg)), NULL);
            put_insn(exit_block, new_insn(I_MOVSD, reg, floats[i]), exit_block->first_insn);
        }
    }

    if (locals) put_insn(exit_block, new_insn(I_ADD, reg_tree(R_SP, new_type(T_LONG)), int_tree(T_LONG, locals)), NULL);
    put_insn(exit_block, new_insn(I_POP, reg_tree(R_BP, new_type(T_LONG))), NULL);
    put_insn(exit_block, new_insn(I_RET), NULL);
}

/* early substitutions. */

static
early_subs(block)
    struct block * block;
{
    struct insn * insn;
    int           ret = 0;

    for (insn = block->first_insn; insn; insn = insn->next) {
        switch (insn->opcode)
        {
        case I_AND:
            /* AND <reg>, <??> -> TEST <reg>, <??> (if <reg> is dead) */

            if (    (insn->operand[0]->op == E_REG) 
                &&  reg_is_dead(block, insn, insn->operand[0]->u.reg) )
            {
                insn->opcode = I_TEST;
                ++ret;
            }
            break;
        }
    }

    return ret;
}


/* returns the log base 2 of 'x', or -1 
   if it's not an integer or "out of range" */

#define LOG2_MAX 30

static
log_2(x)
    long x;
{
    long i = 1;
    int  log_2 = 0;

    while (log_2 < LOG2_MAX) {
        if (x == i) return log_2;
        i <<= 1;
        ++log_2;
    }

    return -1;
}

/* late substitutions. */

static
late_subs(block)
    struct block * block;
{
    struct insn * insn;
    int           i;

    for (insn = block->first_insn; insn; insn = insn->next) {
        switch (insn->opcode) {
        case I_ADD:
            /* ADD <??>, 1 -> INC <??>
               ADD <??>, -1 -> DEC <??> */

            if (    (insn->operand[1]->op == E_CON) 
                &&  !(insn->flags & INSN_FLAG_CC) ) 
            {
                if (insn->operand[1]->u.con.i == 1) {
                    insn->opcode = I_INC;
                    free_tree(insn->operand[1]);
                    insn->operand[1] = NULL;
                } else if (insn->operand[1]->u.con.i == -1) {
                    insn->opcode = I_DEC;
                    free_tree(insn->operand[1]);
                    insn->operand[1] = NULL;
                }
            }

            break;

        case I_SUB:
            /* SUB <??>, 1 -> DEC <??> */

            if (    (insn->operand[1]->op == E_CON)
                &&  (insn->operand[1]->u.con.i == 1)
                &&  !(insn->flags & INSN_FLAG_CC) )
            {
                insn->opcode = I_DEC;
                free_tree(insn->operand[1]);
                insn->operand[1] = NULL;
            }

            break;

        case I_IMUL:
            /* IMUL <??>, x (power of 2) -> SHL <??>, log2(x) */

            if (    (insn->operand[1]->op == E_CON)
                &&  ((i = log_2(insn->operand[1]->u.con.i)) != -1)
                &&  !(insn->flags & INSN_FLAG_CC) )
            {
                insn->opcode = I_SHL;
                insn->operand[1]->u.con.i = i;
            }
            
            break;

#if 0
        case I_MOV:

            /* MOV <reg>, 0 -> XOR <reg>, <reg> */

            if (    (insn->operand[1]->op == E_CON)
                &&  (insn->operand[1]->u.con.i == 0)
                &&  !(insn->operand[1]->type->tts & T_IS_CHAR)
                &&  ccs_are_dead(block, insn) ) /* XXX: don't have this yet */
            {
                insn->opcode = I_XOR;
                free_tree(insn->operand[1]);
                insn->operand[1] = copy_tree(insn->operand[0]);
            }
            
            break;
#endif
        }

    }
}

/* temp_peep() deals with a number of related constructs that the
   code generator emits due to its [over-]use of temporaries. these
   are unsophisticated transforms that [almost] qualify as peephole.

    [A] 

        (1) <op> REG_A, <???>
        (2) MOV REG_B, REG_A

        replace REG_A with REG_B in (1) and eliminate (2). */
 
static
temp_peep(block)
    struct block * block;
{
    struct insn * insn;
    struct insn * next;
    int           kills = 0;
    int           reg_a;
    int           reg_b;

    for (insn = block->first_insn; insn; insn = next) {
        next = insn->next;
        if (next == NULL) break;

        /* (2) must be a MOV REG_B, REG_A */
            
        if (next->opcode != I_MOV) continue;
        if (next->operand[0]->op != E_REG) continue;
        if (next->operand[1]->op != E_REG) continue;
        reg_a = next->operand[1]->u.reg;
        reg_b = next->operand[0]->u.reg;
        if (reg_a == reg_b) continue;  /* different optimization */
  
        /* (1) must look like <op> REG_A, ...
           it must DEF, but not USE, REG_A.
           REG_A must be the same form as in (2)
           REG_A must be dead after (2) */

        if (insn->operand[0]->op != E_REG) continue;
        if (insn->operand[0]->u.reg != reg_a) continue;     

        if (    size_of(insn->operand[0]->type)  /* e.g., AL != EAX */
             != size_of(next->operand[1]->type) ) continue;

        if (!insn_defs_reg(insn, reg_a)) continue;
        if (insn_uses_reg(insn, reg_a)) continue;
        if (!reg_is_dead(block, next, reg_a)) continue;

        /* looks good. make the replacement in (1) and kill (2).
           alter the 'next' pointer to reexamine the result anew. */
   
        insn_replace_reg(insn, reg_a, reg_b);
        kill_insn(block, next);  
        next = insn; 
        ++kills;        
    }

    return -kills; 
}

/* remove dead code (dead stores): any instruction that
   only DEFs a register whose value is never used, and
   has no other side effects, is dead code. */

static 
dead_stores(block)
    struct block * block;
{
    struct insn * insn;
    struct insn * next;
    int           kills = 0;

    for (insn = block->first_insn; insn; insn = next) {
        next = insn->next;

        /* no side effects:
           1. can't set condition codes that are inspected
           2. no memory reads
           3. no memory writes
           4. only one register DEFd
           5. reg is dead after insn */

        if (insn->flags & INSN_FLAG_CC) continue; 
        if (insn->mem_used) continue;
        if (insn->mem_defd) continue;      
        if (insn_nr_defs(insn) != 1) continue; 
        if (!reg_is_dead(block, insn, insn->regs_defd[0])) continue;

        kill_insn(block, insn);
        ++kills;
    }

    return -kills; 
}

/* constant propogation. step through the block and track if a register
   has a known constant value; set DU_CON and 'con' if so. as is typical,
   the register in question must be unaliased (S_REGISTER).

   we could use this information to replace the register with the
   constant value in subsequent insns, if we wished, but we don't (yet).
   for now, we're more interested in eliminating jumps. see end of loop.
   the function is written to make it easy to add "proper" constant
   propagation here, once it becomes clear when that is a win. */

static
con_prop(block)
    struct block * block;
{
    struct insn    * insn;
    int             i;
    int             j;
    int             reg;
    struct defuse * defuse;
    struct block  * test_block;
    struct block  * succ;
    
    for (insn = block->first_insn; insn; insn = insn->next) {
        /* first, invalidate all the
           registers DEFd in this insn */

        j = insn_nr_defs(insn);

        for (i = 0; i < j; ++i) {
            defuse = find_defuse(block, insn->regs_defd[i], 
                FIND_DEFUSE_NORMAL);

            if (defuse) defuse->dus &= ~DU_CON;
        }

        /* look for a MOV <reg>, <con> 
           that meets our constraints */

        if (insn->opcode != I_MOV) continue;
        if (insn->operand[1]->op != E_CON) continue;
        if (insn->operand[0]->op != E_REG) continue;
        reg = insn->operand[0]->u.reg;
        defuse = find_defuse(block, reg, FIND_DEFUSE_NORMAL);
        if (!defuse) continue;
        if (!(defuse->symbol->ss & S_REGISTER)) continue;
        if (!(defuse->symbol->type->ts & T_INT)) continue;

        /* looks good, remember it */

        defuse->dus |= DU_CON;
        defuse->con = insn->operand[1]->u.con.i;
    }

    /* look to see if we have exactly one successor whose 
       sole purpose is to TEST a value whose value we know
       is constant; we can jump around the test. this, in 
       concert with dead code and jump optimization, makes
       the output for logical operators much, much better.
       the code generator just doesn't have enough context. */

    if (block->nr_successors != 1) return 0;
    test_block = block_successor(block, 0);
    if (test_block->nr_insns != 1) return 0;
    insn = test_block->first_insn;
    if (insn->opcode != I_TEST) return 0;
    if (insn->operand[0]->op != E_REG) return 0;
    if (insn->operand[1]->op != E_REG) return 0;
    reg = insn->operand[0]->u.reg;
    if (insn->operand[1]->u.reg != reg) return 0;
    defuse = find_defuse(block, reg, FIND_DEFUSE_NORMAL);
    if (!defuse) return 0;
    if (!(defuse->dus & DU_CON)) return 0;

    /* ok, looks good: let's try to go around test_block
       by picking the right Z or NZ successor. */

    for (i = 0; succ = block_successor(test_block, i); ++i) {
        j = block_successor_cc(test_block, i);
        if ((defuse->con == 0) && (j == CC_Z)) break;
        if ((defuse->con != 0) && (j == CC_NZ)) break;
    }
  
    if (succ) {
        unsucceed_block(block, 0);
        succeed_block(block, CC_ALWAYS, succ);
        return -1;
    } else
        return 0;
}
 

struct optimizer
{
    int     level;
    int ( * func ) ();
} optimizers[] = {
    { 1, early_subs },    
    { 1, temp_peep },
    { 1, dead_stores },     
    { 1, con_prop }
};

#define NR_OPTIMIZERS (sizeof(optimizers)/sizeof(*optimizers))

/* optimize() is a bit of a misnomer. it doesn't just handle 
   optimization - it drives the whole code generation process. */

optimize()
{   
    struct block * block;
    int            again;
    int            ret;
    int            i;

    succeed_block(current_block, CC_ALWAYS, exit_block);
    walk_symbols(SCOPE_FUNCTION, SCOPE_RETIRED, walk1);
    frame_offset = ROUND_UP(frame_offset, FRAME_ALIGN);

    /* optimize in a loop until no more optimizations are done.
       each local optimization function will return non-zero if
       it made any changes - negative if data flow is invalidated.

       for now, we're extremely conservative about maintaining
       data flow information - if an optimizer reports that the 
       data has been invalidated for the block, we recompute ALL
       the data (globally) and restart from the beginning. this 
       obviously has very poor asymptotic properties, so the
       optimizers should try to incrementally update the data. */
    
    do  
    {
      restart:
        again = 0;
        jumps(); 
        unreachable();
        compute_global_defuses();

        for (block = first_block; block; block = block->next) {
            for (i = 0; i < NR_OPTIMIZERS; ++i) {
                if (optimizers[i].level <= O_flag) {
                    ret = optimizers[i].func(block);
                    again += ret;
                    if (ret < 0) goto restart;
                }
            }
        }
    } while (again);

    if (O_flag) {
        for (block = first_block; block; block = block->next)
            late_subs(block);
    }

    allocate_regs();
    logues();
}
