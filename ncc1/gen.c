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
#include "ncc1.h"

/* normalize a node after we've messed with its contents. 
   at present, that means after folding integral constants
   in E_CONs or removing elements of E_IMMs. */

static
normalize(tree)
    struct tree * tree;
{
    long  i;
    int   reg;

    if (tree->op == E_CON) {
        /* integral constants are sign- or zero-extended so that,
           thanks to the magic of 2's complement, most operations
           can be type-agnostic */

        switch (tree->type->ts & T_BASE)
        {
        case T_CHAR:    tree->u.con.i = (char) tree->u.con.i; break;
        case T_UCHAR:   tree->u.con.i = (unsigned char) tree->u.con.i; break;
        case T_SHORT:   tree->u.con.i = (short) tree->u.con.i; break;
        case T_USHORT:  tree->u.con.i = (unsigned short) tree->u.con.i; break;
        case T_INT:     tree->u.con.i = (int) tree->u.con.i; break;
        case T_UINT:    tree->u.con.i = (unsigned) tree->u.con.i; break;
        }
    } else if (tree->op == E_IMM) {
        /* a single unscaled register is always the base. */
        if ((tree->u.mi.b == R_NONE) && (tree->u.mi.i != R_NONE) && (tree->u.mi.s == 1)) {
            tree->u.mi.b = tree->u.mi.i;
            tree->u.mi.i = R_NONE;
        }

        if (!tree->u.mi.rip && !tree->u.mi.glob) {
            if ((tree->u.mi.b == R_NONE) && (tree->u.mi.i == R_NONE)) {
                /* an E_IMM with only an offset is actually an E_CON */
                i = tree->u.mi.ofs;
                tree->op = E_CON;
                tree->u.con.i = i;
            } else if ((tree->u.mi.i == R_NONE) && !tree->u.mi.ofs) {
                /* just a base register is an E_REG */
                reg = tree->u.mi.b;
                tree->op = E_REG;
                tree->u.reg = reg;
            }
        }
    }
}

/* attempt to combine two E_IMM trees into the left tree. on success, the 
   left tree contains the combination, the right tree is an E_NOP, and true
   is returned. on failure, false is returned and neither tree is disturbed.
   this function is only called during generate_binary() but it's separated 
   because it's so messy. */

static
combine_imm(left, right)
    struct tree * left;
    struct tree * right;
{
    int nr_regs = 0;
    int nr_scales = 0;

    if (left->u.mi.glob && right->u.mi.glob) return 0;
    if (left->u.mi.rip || right->u.mi.rip) return 0;
    if (left->u.mi.b != R_NONE) nr_regs++;
    if (left->u.mi.i != R_NONE) nr_regs++;
    if (left->u.mi.s != 1) nr_scales++;
    if (right->u.mi.b != R_NONE) nr_regs++;
    if (right->u.mi.i != R_NONE) nr_regs++;
    if (right->u.mi.s != 1) nr_scales++;
    if ((nr_regs > 2) || (nr_scales > 1)) return 0;

    if (!left->u.mi.glob) left->u.mi.glob = right->u.mi.glob;

    if ((left->u.mi.b != R_NONE) && (right->u.mi.b != R_NONE)) 
        left->u.mi.i = right->u.mi.b;
    else {
        left->u.mi.b = (left->u.mi.b == R_NONE) ? right->u.mi.b : left->u.mi.b;
        left->u.mi.i = (left->u.mi.i == R_NONE) ? right->u.mi.i : left->u.mi.i;
        left->u.mi.s = (left->u.mi.i == 1) ? right->u.mi.s : left->u.mi.s;
    }

    left->u.mi.ofs += right->u.mi.ofs;
    right->op = E_NOP;
    return 1;
}

/* emit an instruction, i.e., place the instruction at the end of the
   current block. used instead of put_insn() directly because it:

   1. saves a bit of typing,
   2. makes sure we actually have a block to emit to, that is,
      we're not trying to evaluate a constant expression */

emit(insn)
    struct insn * insn;
{
    if (current_block == NULL) error(ERROR_CONEXPR);
    put_insn(current_block, insn, NULL);
}

/* return an E_REG tree for a temporary of the given 'type'.
   caller yields ownership of 'type'. */

static struct tree *
temporary(type)
    struct type * type;
{
    struct symbol * temp;

    temp = temporary_symbol(copy_type(type));
    return reg_tree(symbol_reg(temp), type);
}

/* the expression is going to be used as an instruction operand,
   so make sure it's suitable. by the time a tree gets to this 
   point, it's already a leaf - and not an E_SYM - but leaves
   aren't quite as restricted as AMD64 operands. 

   E_CON may change to E_REG or E_MEM.
   E_IMM may change to E_REG.
   E_REG and E_MEM are guaranteed to remain E_REG and E_MEM. */

static struct tree *
operand(tree)
    struct tree * tree;
{
    struct tree * temp1;
    struct tree * temp2;
    struct tree * temp3;

    switch (tree->op) {
    case E_CON:
        /* there is no such thing as an immediate floating-point constant.
           such a constant is written out to the text segment as a static
           anonymous symbol (like a string literal) and turned into an E_MEM. */

        if (tree->type->ts & T_IS_FLOAT) 
            return float_literal(tree);

        /* integral constants can be most 32 bits and are sign-extended. values
           outside this range must be loaded directly with MOV. [if this operand is
           already the source operand of a MOV instruction, then operand(), not
           knowing this, will generate an unnecessary temporary and move. this is 
           fairly infrequent, and the optimizer will remove them anyway.] */

        if ((tree->u.con.i >= INT_MIN) && (tree->u.con.i <= INT_MAX)) return tree;

        temp1 = temporary(copy_type(tree->type));
        emit(new_insn(I_MOV, copy_tree(temp1), tree));
        return temp1;

    case E_MEM:
    case E_IMM:
        /* offsets in E_IMM and E_MEM must fit into 32 bits (signed). if not, 
           split off 'glob' and 'ofs' into a separate register. if we're lucky,
           we can just stuff that back into the E_IMM or E_MEM. if not, do side
           computations and still end up with the same node kind we started with.

           we don't account for [REL glob+big offset]. that would imply
           the existence of a massive object in the text segment, or the
           programmer doing something unusal/bizarre, so don't bother. 

           N.B.: we might just want to prohibit ANY but the simplest of cases.
           are there any legitimate cases where the more complex ones arise? */

        if ((tree->u.mi.ofs < INT_MIN) || (tree->u.mi.ofs > INT_MAX)) {
            if (tree->u.mi.rip) error(ERROR_INTERNAL);

            temp3 = new_tree(E_IMM, new_type(T_LONG));
            temp3->u.mi.glob = tree->u.mi.glob;
            temp3->u.mi.ofs = tree->u.mi.ofs;
            tree->u.mi.glob = NULL;
            tree->u.mi.ofs = 0;
            temp1 = temporary(new_type(T_LONG));
            emit(new_insn(I_MOV, copy_tree(temp1), temp3));

            if (tree->u.mi.b == R_NONE) 
                tree->u.mi.b = temp1->u.reg;
            else if (tree->u.mi.i == R_NONE) 
                tree->u.mi.i = temp1->u.reg;
            else {
                temp2 = temporary(new_type(T_LONG));
                temp3 = copy_tree(tree);
                temp3->op = E_MEM; /* force E_IMM to E_MEM for asm syntax */
                emit(new_insn(I_LEA, copy_tree(temp2), temp3));
                tree->u.mi.b = temp1->u.reg;
                tree->u.mi.i = temp2->u.reg;
                tree->u.mi.s = 1;
                free_tree(temp2);
            } 

            free_tree(temp1);
            normalize(tree);    /* E_IMM might decay */
        }

        /* E_IMM fields that aren't simply glob+ofs can't be used as operands. */

        if (    (tree->op == E_IMM) 
            &&  ((tree->u.mi.b != R_NONE) || (tree->u.mi.i != R_NONE) || (tree->u.mi.rip)) )
        {
            temp1 = temporary(copy_type(tree->type));
            tree->op = E_MEM; /* syntax again */
            emit(new_insn(I_LEA, copy_tree(temp1), tree));
            tree = temp1;
        }

        return tree;

    case E_REG:
        break;

    default: error(ERROR_INTERNAL);
    }
    return tree;
}

/* given a tree 'op' and two operand trees, emit the first match in the
   table. an entry is considered a match when the 'op' is the same and the 
   operands' type bits are "covered" by the corresponding masks. 
   
   as a special case, an opcode of I_NOP results in no emission at all.
   returns non-zero if an instruction was emitted, zero otherwise.
   ownership of 'left' and 'right' trees is given to choose(). */
   
static struct
{
    int     op;
    int     left_ts;
    int     right_ts;
    int     opcode;
} choices[] = 
{
    { E_ASSIGN, T_IS_INTEGRAL | T_PTR, T_IS_INTEGRAL | T_PTR, I_MOV },
    { E_ASSIGN, T_FLOAT, T_FLOAT, I_MOVSS },
    { E_ASSIGN, T_LFLOAT, T_LFLOAT, I_MOVSD },

    { E_EQ, T_IS_INTEGRAL | T_PTR, T_IS_INTEGRAL | T_PTR, I_CMP },
    { E_EQ, T_FLOAT, T_FLOAT, I_UCOMISS },
    { E_EQ, T_LFLOAT, T_LFLOAT, I_UCOMISD },

    { E_SHL, T_IS_INTEGRAL, T_IS_INTEGRAL, I_SHL },
    { E_SHR, T_IS_SIGNED, T_IS_INTEGRAL, I_SAR },
    { E_SHR, T_IS_UNSIGNED, T_IS_INTEGRAL, I_SHR },

    { E_OR, T_IS_INTEGRAL | T_PTR, T_IS_INTEGRAL | T_PTR, I_OR },
    { E_AND, T_IS_INTEGRAL | T_PTR, T_IS_INTEGRAL | T_PTR, I_AND },
    { E_XOR, T_IS_INTEGRAL | T_PTR, T_IS_INTEGRAL | T_PTR, I_XOR },

    { E_ADD, T_FLOAT, T_FLOAT, I_ADDSS },
    { E_ADD, T_LFLOAT, T_LFLOAT, I_ADDSD },
    { E_ADD, T_IS_INTEGRAL | T_PTR, T_IS_INTEGRAL | T_PTR, I_ADD },

    { E_SUB, T_FLOAT, T_FLOAT, I_SUBSS },
    { E_SUB, T_LFLOAT, T_LFLOAT, I_SUBSD },
    { E_SUB, T_IS_INTEGRAL | T_PTR, T_IS_INTEGRAL | T_PTR, I_SUB },

    { E_DIV, T_FLOAT, T_FLOAT, I_DIVSS },
    { E_DIV, T_LFLOAT, T_LFLOAT, I_DIVSD },
    { E_MUL, T_FLOAT, T_FLOAT, I_MULSS },
    { E_MUL, T_LFLOAT, T_LFLOAT, I_MULSD },
    { E_MUL, T_IS_INTEGRAL | T_PTR, T_IS_INTEGRAL | T_PTR, I_IMUL },

    { E_CAST, T_IS_CHAR, T_IS_CHAR, I_NOP }, 
    { E_CAST, T_IS_SHORT, T_IS_SHORT, I_NOP },
    { E_CAST, T_IS_INT, T_IS_INT, I_NOP },
    { E_CAST, T_IS_LONG | T_PTR, T_IS_LONG | T_PTR, I_NOP },
    { E_CAST, T_FLOAT, T_FLOAT, I_NOP },
    { E_CAST, T_LFLOAT, T_LFLOAT, I_NOP },
    { E_CAST, T_PTR | T_IS_LONG | T_IS_INT | T_IS_SHORT, T_CHAR, I_MOVSX },  
    { E_CAST, T_PTR | T_IS_LONG | T_IS_INT | T_IS_SHORT, T_UCHAR, I_MOVZX },  
    { E_CAST, T_PTR | T_IS_LONG | T_IS_INT, T_SHORT, I_MOVSX },
    { E_CAST, T_PTR | T_IS_LONG | T_IS_INT, T_USHORT, I_MOVZX },
    { E_CAST, T_PTR | T_IS_LONG, T_INT, I_MOVSX },
    { E_CAST, T_PTR | T_IS_LONG, T_UINT, I_MOVZX },
    { E_CAST, T_PTR | T_IS_INTEGRAL, T_PTR | T_IS_INTEGRAL, I_MOV }, 
    { E_CAST, T_FLOAT, T_LFLOAT, I_CVTSD2SS }, 
    { E_CAST, T_LFLOAT, T_FLOAT, I_CVTSS2SD },  
    { E_CAST, T_IS_INT | T_IS_LONG, T_FLOAT, I_CVTSS2SI }, 
    { E_CAST, T_IS_INT | T_IS_LONG, T_LFLOAT, I_CVTSD2SI }, 
    { E_CAST, T_FLOAT, T_IS_INT | T_IS_LONG, I_CVTSI2SS }, 
    { E_CAST, T_LFLOAT, T_IS_INT | T_IS_LONG, I_CVTSI2SD }
};

#define NR_CHOICES (sizeof(choices)/sizeof(*choices))

choose(op, left, right)
    struct tree * left;
    struct tree * right;
{
    int i;

    for (i = 0; i < NR_CHOICES; i++) {
        if (choices[i].op != op) continue;
        if (!(choices[i].left_ts & left->type->ts)) continue;
        if (!(choices[i].right_ts & right->type->ts)) continue;

        if (choices[i].opcode != I_NOP) {
            left = operand(left);
            right = operand(right);

            /* we let the callers be lazy by making sure the source size
               of MOV instructions matches the destination size */

            if ((choices[i].opcode == I_MOV) && (left->type->ts != right->type->ts)) {
                free_type(right->type);
                right->type = copy_type(left->type);
            }

            emit(new_insn(choices[i].opcode, left, right));
            return 1;
        } else {
            free_tree(left);
            free_tree(right);
            return 0;
        }
    }

    fprintf(stderr, "op = %d, left = %x, right = %x\n", op, left->type->ts, right->type->ts);

    error(ERROR_INTERNAL);
}

/* examines the tree to see if it's an E_CON and a power of two.
   if not, the tree is unmodified and zero is returned. otherwise,
   the constant is rewritten either as log_2(constant) if mask is
   zero, or as (2^log_2(constant))-1 is mask is non-zero. (the 
   latter value is useful for constant modulo operations.) */

static
log_2(tree, mask)
    struct tree * tree;
{
    long bit;
    int  i;

    if (tree->op == E_CON) {
        for (bit = 2, i = 1; bit > 0; i++, bit <<= 1) { 
            if ((tree->u.con.i & bit) == tree->u.con.i) {
                if (mask) 
                    tree->u.con.i = bit - 1;
                else
                    tree->u.con.i = i;

                return 1;
            }
        }
    }

    return 0;
}

/* process a leaf node (except E_SYM). also called by other lazy handlers
   that leave a leaf on the tree and need it to be processed for 'goal'. */

static struct tree *
generate_leaf(tree, goal, cc)       /* E_REG E_CON E_IMM E_MEM */
    struct tree * tree;
    int *         cc;
{
    struct tree * zero;

    if (!E_IS_LEAF(tree->op) || (tree->op == E_SYM)) error(ERROR_INTERNAL);
    
    if (goal == GOAL_CC) {
        if (tree->op == E_CON) {
            *cc = CC_NEVER;

            if (    ((tree->type->ts & T_IS_FLOAT) && (tree->u.con.f)) 
                ||  ((tree->type->ts & T_IS_INTEGRAL) && (tree->u.con.i)) )
            {
                *cc = CC_ALWAYS;
            }
        } else {
            if (!(tree->type->ts & T_IS_SCALAR)) error(ERROR_INTERNAL);

            tree = operand(tree);

            if (tree->type->ts & T_IS_FLOAT) {
                zero = temporary(copy_type(tree->type));
                emit(new_insn(I_PXOR, copy_tree(zero), copy_tree(zero)));
                choose(E_EQ, zero, tree);
            } else {
                if (tree->op == E_REG) 
                    emit(new_insn(I_TEST, copy_tree(tree), tree));
                else {
                    zero = new_tree(E_CON, copy_type(tree->type));
                    zero->u.con.i = 0;
                    choose(E_EQ, tree, zero);
                }
            }

            *cc = CC_NZ;
            tree = NULL;
        }
    }

    if (goal != GOAL_VALUE) {
        free_tree(tree);
        tree = NULL;
    }

    return tree;
}

/* extract the value of a bitfield into a temporary. 
   ownership of 'tree' is yielded by the caller. */

static struct tree *
extract_field(tree)
    struct tree * tree;
{
    struct tree * temp;
    int           shift = T_GET_SHIFT(tree->type->ts);
    int           size = T_GET_SIZE(tree->type->ts);
    long          temp_bits;

    if ((tree->op != E_MEM) || !(tree->type->ts & T_FIELD)) error(ERROR_INTERNAL);

    tree->type->ts &= T_BASE;
    temp = temporary(copy_type(tree->type));
    temp_bits = size_of(temp->type) * BITS;

    choose(E_ASSIGN, copy_tree(temp), tree);
    choose(E_SHL, copy_tree(temp), int_tree(T_INT, temp_bits - shift - size));
    choose(E_SHR, copy_tree(temp), int_tree(T_INT, temp_bits - size)); 

    return temp;
}

/* insert a value into a bitfield. both 'tree' and 'value' are yielded by the
   caller. this is meant to be called in place of generate_leaf() when storing
   a bitfield - it attempts to "optimize" based on GOAL_CC or GOAL_VALUE. */

struct tree *
insert_field(tree, value, goal, cc)
    struct tree * tree;
    struct tree * value;
    int *         cc;
{
    struct tree * temp;
    struct tree * source;
    struct tree * original_tree;
    long          source_mask;
    long          target_mask;
    long          shift = T_GET_SHIFT(tree->type->ts);
    long          size = T_GET_SIZE(tree->type->ts);
    long          value_bits;

    if ((tree->op != E_MEM) || !(tree->type->ts & T_FIELD)) error(ERROR_INTERNAL);

    value_bits = size_of(value->type) * BITS;
    source_mask = ~(-1L << size) & ~(-1L << value_bits);
    target_mask = ~(source_mask << shift) & ~(-1L << value_bits);

    if (value->op == E_CON) {
        value->u.con.i &= source_mask;
        value->u.con.i <<= shift;
        source = value;
    } else {
        temp = temporary(copy_type(value->type));
        choose(E_ASSIGN, copy_tree(temp), value);
        choose(E_AND, copy_tree(temp), int_tree(temp->type->ts, source_mask));
        choose(E_SHL, copy_tree(temp), int_tree(temp->type->ts, shift));
        source = temp;
    } 

    original_tree = copy_tree(tree);
    tree->type->ts &= T_BASE;
    choose(E_AND, copy_tree(tree), int_tree(tree->type->ts, target_mask));
    choose(E_OR, tree, copy_tree(source));

    switch (goal)
    {
    case GOAL_VALUE:
        free_tree(source);
        return extract_field(original_tree);
    default:
        free_tree(original_tree);
        return generate_leaf(source, goal, cc);
    }
}

/* load into a temporary register. tree is yielded by caller. */

static struct tree *
load(tree)
    struct tree * tree;
{
    struct tree * temp;

    if (tree->op == E_REG) error(ERROR_INTERNAL);

    temp = temporary(copy_type(tree->type));
    choose(E_ASSIGN, copy_tree(temp), tree);
    return temp;
}

/* returns a temporary register that is set to 0 or 1,
   depending on whether the given CC is true or not. */

static struct tree *
setcc(cc)
{
    struct tree * temp;

    temp = temporary(new_type(T_INT));
    choose(E_ASSIGN, copy_tree(temp), int_tree(T_INT, 0L));
    emit(new_insn(I_SETZ + cc, reg_tree(temp->u.reg, new_type(T_CHAR))));
    return temp;
}

/* the terminology is a bit confusing. in an E_COMMA node,
   the _right_ is evaluated for side effects and tossed away,
   and the _left_ is the value of the expression. */

static struct tree *
generate_comma(tree, goal, cc)      /* E_COMMA */
    struct tree * tree;
    int *         cc;
{
    struct tree * left;
    struct tree * right;

    decap_tree(tree, NULL, &left, &right, NULL);
    right = generate(right, GOAL_EFFECT, NULL);
    return generate(left, goal, cc);
}

static struct tree * 
generate_logical(tree, goal, cc)    /* E_LOR E_LAND */
    struct tree * tree;
    int *         cc;
{
    struct tree *  left;
    struct tree *  right;
    struct tree *  temp;
    int            op;
    int            result_cc;
    struct block * right_block;
    struct block * join_block;
    struct block * true_block;
    struct block * false_block;

    op = tree->op;
    decap_tree(tree, NULL, &left, &right, NULL);
    generate(left, GOAL_CC, &result_cc);

    if ((result_cc == CC_ALWAYS) || (result_cc == CC_NEVER)) {
        if (    ((result_cc == CC_ALWAYS) && (op == E_LOR))
            ||  ((result_cc == CC_NEVER) && (op == E_LAND)) )
        {
            /* short-circuit: right side is irrelevant */
            free_tree(right);
            temp = int_tree(T_INT, (op == E_LOR) ? 1L : 0L);
            return generate_leaf(temp, goal, cc);
        } else {
            /* left side ambiguous: right side all that matters */
            right = generate(right, GOAL_CC, &result_cc);

            if ((result_cc == CC_ALWAYS) || (result_cc == CC_NEVER)) {
                temp = int_tree(T_INT, (result_cc == CC_ALWAYS) ? 1L : 0L);
                return generate_leaf(temp, goal, cc);
            }

            switch (goal)
            {
            case GOAL_VALUE:
                return setcc(result_cc);
            case GOAL_CC:
                *cc = result_cc;
            case GOAL_EFFECT:
                return NULL;
            }
        }
    }

    true_block = new_block();
    false_block = new_block();
    join_block = new_block();
    right_block = new_block();

    temp = temporary(new_type(T_INT));
    put_insn(true_block, new_insn(I_MOV, copy_tree(temp), int_tree(T_INT, 1L)), NULL);
    put_insn(false_block, new_insn(I_MOV, copy_tree(temp), int_tree(T_INT, 0L)), NULL);
    succeed_block(true_block, CC_ALWAYS, join_block);
    succeed_block(false_block, CC_ALWAYS, join_block);
    
    if (op == E_LOR) {
        succeed_block(current_block, result_cc, true_block);
        succeed_block(current_block, CC_INVERT(result_cc), right_block);
    } else {
        succeed_block(current_block, result_cc, right_block);
        succeed_block(current_block, CC_INVERT(result_cc), false_block);
    }

    current_block = right_block;
    right = generate(right, GOAL_CC, &result_cc);
    succeed_block(current_block, result_cc, true_block);
    succeed_block(current_block, CC_INVERT(result_cc), false_block);
    current_block = join_block;
    return generate_leaf(temp, goal, cc);
}

static struct tree *
generate_ternary(tree, goal, cc)    /* E_TERN */
    struct tree * tree;
    int *         cc;
{
    struct tree *  left;
    struct tree *  right;
    struct tree *  temp;
    struct block * true_block;
    struct block * false_block;
    struct block * join_block;
    struct type *  type;
    int            result_cc;

    decap_tree(tree, &type, &left, &right, &tree);
    generate(tree, GOAL_CC, &result_cc);

    if (result_cc == CC_ALWAYS) {
        free_tree(right);
        free_type(type);
        return generate(left, goal, cc);
    }

    if (result_cc == CC_NEVER) {
        free_tree(left);
        free_type(type);
        return generate(right, goal, cc);
    }
    
    if (goal != GOAL_EFFECT)
        temp = temporary(type);
    else
        temp = NULL;

    true_block = new_block();
    false_block = new_block();
    join_block = new_block();

    succeed_block(current_block, result_cc, true_block);
    succeed_block(current_block, CC_INVERT(result_cc), false_block);

    current_block = true_block;
    left = generate(left, GOAL_VALUE, NULL);
    if (goal != GOAL_EFFECT) choose(E_ASSIGN, copy_tree(temp), left);
    true_block = current_block;
    succeed_block(true_block, CC_ALWAYS, join_block);

    current_block = false_block;
    right = generate(right, GOAL_VALUE, NULL);
    if (goal != GOAL_EFFECT) choose(E_ASSIGN, copy_tree(temp), right);
    false_block = current_block;
    succeed_block(false_block, CC_ALWAYS, join_block);

    current_block = join_block;

    if (goal != GOAL_EFFECT)
        return generate_leaf(temp, goal, cc);
    else
        return NULL;
}

static struct tree * 
generate_call(tree, goal, cc)       /* E_CALL */
    struct tree * tree;
    int *         cc;
{
    struct tree * function;
    struct tree * arguments;
    struct tree * argument;
    struct type * type;
    int           reg;
    int           stack_adjust = 0;

    decap_tree(tree, &type, &function, &arguments, NULL);
    function = generate(function, GOAL_VALUE, NULL);

    /* this could use some optimization, especially w/r/t float arguments */

    while (argument = arguments)
    {
        arguments = argument->list;
        argument->list = NULL;
        argument = generate(argument, GOAL_VALUE, 0);
        argument = operand(argument);

        /* non-quadwords in memory must go through a temporary */

        if ((argument->op == E_MEM) && !(argument->type->ts & T_IS_QWORD))
            argument = load(argument);

        /* integer registers must be quadwords */

        if ((argument->op == E_REG) && (argument->type->ts & T_IS_INTEGRAL)) {
            free_type(argument->type);
            argument->type = new_type(T_LONG);
        }

        /* can't push floats directly from float registers, so fake it. push everyone else. */

        if ((argument->op == E_REG) && (argument->type->ts & T_IS_FLOAT)) {
            emit(new_insn(I_SUB, reg_tree(R_SP, new_type(T_LONG)), int_tree(T_LONG, (long) FRAME_ALIGN)));
            tree = stack_tree(copy_type(argument->type), 0);
            choose(E_ASSIGN, tree, argument);
        } else 
            emit(new_insn(I_PUSH, argument));

        stack_adjust += FRAME_ALIGN;
    }

    /* I_CALL is implicitly REL, so an E_IMM must be REL and we strip it. */

    if (function->op == E_IMM) {
        if (!function->u.mi.rip) error(ERROR_SEGMENT);
        function->u.mi.rip = 0;
        normalize(function);
    }

    emit(new_insn(I_CALL, operand(function)));
    if (stack_adjust) emit(new_insn(I_ADD, reg_tree(R_SP, new_type(T_LONG)), int_tree(T_LONG, (long) stack_adjust)));

    if (goal == GOAL_EFFECT) {
        free_type(type);
        return NULL;
    } else {
        tree = temporary(type);
        reg = (tree->type->ts & T_IS_FLOAT) ? R_XMM0 : R_AX;
        choose(E_ASSIGN, copy_tree(tree), reg_tree(reg, copy_type(tree->type)));
        return generate_leaf(tree, goal, cc);
    }
}

static struct tree *
generate_relational(tree, goal, cc) /* E_GT E_LT E_EQ E_NEQ E_GTEQ E_LTEQ */
    struct tree * tree;
    int *         cc;
{
    struct tree * left;
    struct tree * right;
    struct tree * temp;
    int           op;
    int           eq;
    int           gt;
    int           result_cc;

    op = tree->op;
    decap_tree(tree, NULL, &left, &right, NULL);

    if (goal == GOAL_EFFECT) {
        generate(left, GOAL_EFFECT, NULL);
        generate(right, GOAL_EFFECT, NULL);
        return NULL;
    }

    left = generate(left, GOAL_VALUE, NULL);
    right = generate(right, GOAL_VALUE, NULL);

    if ((left->op == E_CON) && (right->op == E_CON)) {
        if (left->type->ts & T_IS_FLOAT) {
            eq = (left->u.con.f == right->u.con.f);
            gt = (left->u.con.f > right->u.con.f);
        } else if (left->type->ts & T_IS_SIGNED) {
            eq = (left->u.con.i == right->u.con.i);
            gt = (left->u.con.i > right->u.con.i);
        } else {
            eq = (((unsigned long) left->u.con.i) == right->u.con.i);
            gt = (((unsigned long) left->u.con.i) > right->u.con.i);
        }

        free_tree(left);
        free_tree(right);
        temp = int_tree(T_INT, 0L);

        switch (op)
        {
        case E_EQ:      temp->u.con.i = eq; break;
        case E_NEQ:     temp->u.con.i = !eq; break;
        case E_GT:      temp->u.con.i = gt; break;
        case E_GTEQ:    temp->u.con.i = (gt || eq); break;
        case E_LT:      temp->u.con.i = (!eq && !gt); break;
        case E_LTEQ:    temp->u.con.i = (eq || !gt); break;
        }
    
        return generate_leaf(temp, goal, cc);
    }

    switch (op)
    {
    case E_EQ:      result_cc = CC_Z; break;
    case E_NEQ:     result_cc = CC_NZ; break;
    case E_GT:      result_cc = (left->type->ts & T_IS_SIGNED) ? CC_G : CC_A; break;
    case E_LT:      result_cc = (left->type->ts & T_IS_SIGNED) ? CC_L : CC_B; break;
    case E_GTEQ:    result_cc = (left->type->ts & T_IS_SIGNED) ? CC_GE : CC_AE; break;
    case E_LTEQ:    result_cc = (left->type->ts & T_IS_SIGNED) ? CC_LE : CC_BE; break;
    }

    if (left->op == E_CON) left = load(left);
    if ((left->op == E_MEM) && (left->type->ts & T_IS_FLOAT)) left = load(left);
    if ((left->op == E_MEM) && (right->op == E_MEM)) right = load(right);
    choose(E_EQ, left, right);

    if (goal == GOAL_CC) {
        *cc = result_cc;
        return NULL;
    } else 
        return setcc(result_cc);
}

static struct tree *
generate_neg(tree, goal, cc)
    struct tree * tree;
    int *         cc;
{
    struct tree * temp;

    decap_tree(tree, NULL, &tree, NULL, NULL);

    if (goal == GOAL_VALUE) {
        tree = generate(tree, GOAL_VALUE, NULL);
        if (tree->type->ts & (T_IS_INTEGRAL | T_PTR)) {
            if (tree->op == E_CON) {
                tree->u.con.i = -(tree->u.con.i);
                normalize(tree);
            } else {
                temp = temporary(copy_type(tree->type));
                choose(E_ASSIGN, copy_tree(temp), tree);
                emit(new_insn(I_NEG, copy_tree(temp)));
                tree = temp;
            }
        } else {
            if (tree->op == E_CON) 
                tree->u.con.f = -(tree->u.con.f);
            else {
                temp = temporary(copy_type(tree->type));
                choose(E_ASSIGN, copy_tree(temp), float_tree(tree->type->ts, (double) -1));
                choose(E_MUL, copy_tree(temp), tree);
                tree = temp;
            }
        }
    } else 
        tree = generate(tree, goal, cc);

    return tree;
}

static struct tree *
generate_not(tree, goal, cc)        /* E_NOT */
    struct tree * tree;
    int *         cc;
{
    struct tree * temp;
    int           result_cc;

    decap_tree(tree, NULL, &tree, NULL, NULL);

    if (goal == GOAL_EFFECT) {
        tree = generate(tree, GOAL_EFFECT, NULL);
    } else {
        tree = generate(tree, GOAL_CC, &result_cc);
        result_cc = CC_INVERT(result_cc);

        if (goal == GOAL_VALUE) 
            tree = setcc(result_cc);
        else
            *cc = result_cc;
    }

    return tree;
}

static struct tree *
generate_com(tree, goal, cc)        /* E_COM */
    struct tree * tree;
    int *         cc;
{
    struct tree * temp;

    decap_tree(tree, NULL, &tree, NULL, NULL);
    tree = generate(tree, goal, cc);

    if (goal == GOAL_CC) {
        *cc = CC_INVERT(*cc);
    } else if (goal == GOAL_VALUE) {
        if (tree->op == E_CON) {
            tree->u.con.i = ~(tree->u.con.i);
            normalize(tree);
        } else {
            temp = temporary(copy_type(tree->type));
            choose(E_ASSIGN, copy_tree(temp), tree);
            emit(new_insn(I_NOT, copy_tree(temp)));
            tree = temp;
        }
    }

    return tree;
}


static struct tree *
generate_cast(tree, goal, cc)       /* E_CAST */
    struct tree * tree;
    int *         cc;
{
    struct type * target_type;
    double        f;
    long          i;
    struct tree * tree_copy;
    struct tree * destination_reg;
    struct tree * destination_copy;
    struct tree * temp_reg;

    decap_tree(tree, &target_type, &tree, NULL, NULL);

    if (goal == GOAL_EFFECT) {
        free_type(target_type);
        return generate(tree, GOAL_EFFECT, cc);
    }

    tree = generate(tree, GOAL_VALUE, cc);

    if (tree->op == E_CON) {
        i = tree->u.con.i;  /* avoid aliasing issues */
        f = tree->u.con.f;  /* with tree->con.u union */

        if ((tree->type->ts & T_IS_FLOAT) && (target_type->ts & T_IS_INTEGRAL))
            tree->u.con.i = f;

        if ((tree->type->ts & T_IS_INTEGRAL) && (target_type->ts & T_IS_FLOAT))
            tree->u.con.f = i;

        free_type(tree->type);
        tree->type = target_type;
        normalize(tree);
        return generate_leaf(tree, goal, cc);
    }

    /* we use the choices[] table to do most of the work (or not work). 
       the only special cases are casts between floats and short/char, which
       must be done through an intermediary int. */

    destination_reg = temporary(target_type); 
    destination_copy = copy_tree(destination_reg);

    if ((tree->type->ts & T_IS_FLOAT) && (destination_reg->type->ts & (T_IS_CHAR | T_IS_SHORT))) {
        /* fake the size of the target for choose() */
        free_type(destination_reg->type);
        destination_reg->type = new_type(T_INT);
    } else if ((tree->type->ts & (T_IS_CHAR | T_IS_SHORT)) && (destination_reg->type->ts & T_IS_FLOAT)) {
        /* promote into a temporary */
        temp_reg = temporary(new_type(T_INT));
        choose(E_CAST, copy_tree(temp_reg), tree);
        tree = temp_reg;
    } 

    tree_copy = copy_tree(tree);

    /* choose() returns 1 if the cast was anything but conceptual.
       otherwise we just free the temporary we created and go on like
       nothing happened. */

    if (choose(E_CAST, destination_reg, tree)) {
        free_tree(tree_copy);
        return generate(destination_copy, goal, cc);
    } else {
        free_tree(destination_copy);
        return generate(tree_copy, goal, cc);
    }
}

/* basically this turns an E_MEM into an E_IMM.  we need to be careful 
   to call memory_tree on an E_SYM child rather than generate(), so we 
   don't get a register back. */

static struct tree *
generate_addr(tree, goal, cc)       /* E_ADDR */
    struct tree * tree;
    int *         cc;
{
    struct symbol * symbol;
    struct type *   type;

    decap_tree(tree, &type, &tree, NULL, NULL);

    if (tree->op == E_SYM) {
        symbol = tree->u.sym;
        free_tree(tree);
        tree = memory_tree(symbol);
    } else {
        tree = generate(tree, GOAL_VALUE, cc);
    }

    if (tree->op != E_MEM) error(ERROR_INTERNAL);

    tree->op = E_IMM;
    free_type(tree->type);
    tree->type = type;

    return generate_leaf(tree, goal, cc);
}

/* for most operands, we just rewrite the operand to be indirect. 
   the exceptions are:

    1. E_MEM fields, which are loaded into a temporary register
       because they're already indirect, and
    2. bit fields, which are automatically fetched and extracted
       into a temporary. this behavior is suppressed if the 'lvalue' 
       flag is non-zero. */

static struct tree *
generate_fetch(tree, goal, cc, lvalue)      /* E_FETCH */
    struct tree * tree;
    int *         cc;
{
    struct type * type;
    struct tree * temp;

    decap_tree(tree, &type, &tree, NULL, NULL);

    if (goal == GOAL_EFFECT) {
        free_type(type);
        return generate(tree, goal, cc);
    } else
        tree = generate(tree, GOAL_VALUE, cc);

    switch (tree->op)
    {
    case E_CON:
        temp = new_tree(E_MEM, type);
        temp->u.mi.ofs = tree->u.con.i;
        free_tree(tree);
        tree = temp;
        break;
    case E_MEM:
        tree = load(tree);
    case E_REG:
        temp = new_tree(E_MEM, type);
        temp->u.mi.b = tree->u.reg;
        free_tree(tree);
        tree = temp;
        break;
    case E_IMM:
        tree->op = E_MEM;
        free_type(tree->type);
        tree->type = type;
        break;
    default: error(ERROR_INTERNAL);
    }

    if ((tree->op == E_MEM) && (tree->type->ts & T_FIELD) && !lvalue)
        tree = extract_field(tree);

    return generate_leaf(tree, goal, cc);
}

static struct tree *
generate_assign(tree, goal, cc)     /* E_ASSIGN */
    struct tree * tree;
    int *         cc;
{
    struct tree * left;
    struct tree * right;

    decap_tree(tree, NULL, &left, &right, NULL);
    right = generate(right, GOAL_VALUE, NULL);

    if (left->type->ts & T_FIELD) {
        left = generate_fetch(left, GOAL_VALUE, NULL, 1);
        return insert_field(left, right, goal, cc);
    } else {
        left = generate(left, GOAL_VALUE, NULL);
        if ((left->op == E_MEM) && (right->op == E_MEM)) right = load(right);
        tree = copy_tree(right);
        choose(E_ASSIGN, left, right);
        return generate_leaf(tree, goal, cc);
    }
}

/* E_SYM nodes are turned into E_REG (scalars) or E_MEM nodes. */

static struct tree *
generate_sym(tree, goal, cc)        /* E_SYM */
    struct tree * tree;
    int *         cc;
{
    struct symbol * symbol;

    symbol = tree->u.sym;
    free_tree(tree);
    tree = NULL;

    if (goal != GOAL_EFFECT) {
        if (symbol->type->ts & T_IS_SCALAR) 
            tree = reg_tree(symbol_reg(symbol), copy_type(symbol->type));
        else 
            tree = memory_tree(symbol);

        tree = generate_leaf(tree, goal, cc);
    } 

    return tree;
}

/* the AMD64 integer division instructions differ enough from the others
   that they require special handling. call this with 'op' = E_DIV or E_MOD.
   the returned tree will be the left tree. 'right' is yielded by the caller.
   if 'result_cc' is not NULL, and divmod() can determine the truth value 
   of the result, it will set 'result_cc' accordingly. otherwise it is untouched. */

static struct tree *
divmod(op, left, right, result_cc)
    struct tree * left;
    struct tree * right;
    int *         result_cc;
{
    struct tree * r_ax;
    struct tree * r_dx;

    if ((op == E_MOD) && (left->type->ts & (T_IS_UNSIGNED | T_PTR)) && log_2(right, 1)) {
        choose(E_AND, copy_tree(left), right);
        if (result_cc) *result_cc = CC_NZ;
        return left;
    }

    if ((op == E_DIV) && (left->type->ts & (T_IS_UNSIGNED | T_PTR)) && log_2(right, 0)) {
        choose(E_SHR, copy_tree(left), right);
        if (result_cc) *result_cc = CC_NZ;
        return left;
    }

    if ((right->op != E_REG) && (right->op != E_MEM)) right = load(right);

    if (left->type->ts & T_IS_CHAR) {
        r_ax = reg_tree(R_AX, copy_type(left->type));

        if (left->type->ts & T_CHAR) {
            choose(E_ASSIGN, copy_tree(r_ax), copy_tree(left));
            emit(new_insn(I_CBW));
            emit(new_insn(I_IDIV, right));
        } else { /* T_UCHAR */
            choose(E_ASSIGN, reg_tree(R_AX, new_type(T_INT)), int_tree(T_LONG, 0L));
            choose(E_ASSIGN, copy_tree(r_ax), copy_tree(left));
            emit(new_insn(I_DIV, right));
        }

        if (op == E_MOD) choose(E_SHR, reg_tree(R_AX, new_type(T_INT)), int_tree(T_INT, 8L));
        choose(E_ASSIGN, copy_tree(left), r_ax);
    } else {
        r_ax = reg_tree(R_AX, copy_type(left->type));
        r_dx = reg_tree(R_DX, copy_type(left->type));

        choose(E_ASSIGN, copy_tree(r_ax), copy_tree(left));

        switch (left->type->ts & T_BASE) 
        {
        case T_SHORT:   emit(new_insn(I_CWD)); break;
        case T_INT:     emit(new_insn(I_CDQ)); break;
        case T_LONG:    emit(new_insn(I_CQO)); break;

        case T_USHORT:
        case T_UINT:
        case T_ULONG:
            choose(E_ASSIGN, copy_tree(r_dx), int_tree(T_LONG, 0L));
            break;
        }

        emit(new_insn((left->type->ts & T_IS_SIGNED) ? I_IDIV : I_DIV, right));

        if (op == E_MOD) {
            choose(E_ASSIGN, copy_tree(left), r_dx);
            free_tree(r_ax);
        } else {
            choose(E_ASSIGN, copy_tree(left), r_ax);
            free_tree(r_dx);
        }
    }

    return left;
}

static struct tree *
generate_binary(tree, goal, cc)        /* E_ADD E_SUB E_MUL E_MOD E_DIV E_SHR E_SHL E_AND E_OR E_XOR */
    struct tree * tree;
    int *         cc;
{
    struct tree * left;
    struct tree * right;
    struct type * type;
    struct tree * temp;
    int           op;
    int           result_cc = CC_NONE;

    op = tree->op;

    /* normalize operands for commutative operators */

    switch (op)
    {
    case E_ADD:
    case E_MUL:
    case E_AND:
    case E_OR:
    case E_XOR:
        if (tree->u.ch[0]->op == E_CON) commute_tree(tree);
        if (tree->u.ch[1]->op == E_IMM) commute_tree(tree);
    }

    decap_tree(tree, &type, &left, &right, NULL);

    /* binary operators don't have side effects themselves */

    if (goal == GOAL_EFFECT) {
        generate(left, GOAL_EFFECT, NULL);
        generate(right, GOAL_EFFECT, NULL);
        free_type(type);
        return NULL;
    }

    left = generate(left, GOAL_VALUE, NULL);
    right = generate(right, GOAL_VALUE, NULL);

    if (right->op == E_CON) {
        if (    ((type->ts & (T_IS_INTEGRAL | T_PTR)) && (right->u.con.i == 0))
            ||  ((type->ts & T_IS_FLOAT) && (right->u.con.f == 0)) )
        {
            switch (op) {
            case E_AND:     /* x & 0 = 0 */
            case E_MUL:     /* x * 0 = 0 */
                temp = left;
                left = right;
                right = temp;
            case E_ADD:     /* x + 0 = x */
            case E_SUB:     /* x - 0 = x */
            case E_SHL:     /* x << 0 = x */
            case E_SHR:     /* x >> 0 = x */
            case E_OR:      /* x | 0 = x */
            case E_XOR:     /* x ^ 0 = x */
                goto folded;
            case E_MOD:     /* x % 0 bad */
            case E_DIV:     /* x / 0 bad */
                error(ERROR_DIV0);
            }
        }
    }

    switch (op)
    {
    case E_ADD:

        /* E_CON + E_CON = E_CON (always)
           E_REG + E_REG = E_IMM (always, pointer types only)
           E_REG + E_CON = E_IMM (always, pointer types only)
           E_IMM + E_CON = E_IMM (always)
           E_IMM + E_REG = E_IMM (when possible)
           E_IMM + E_IMM = E_IMM (when possible) */

        if ((left->op == E_CON) && (right->op == E_CON)) {
            if (type->ts & T_IS_FLOAT)
                left->u.con.f += right->u.con.f;
            else 
                left->u.con.i += right->u.con.i;

            goto folded;
        }

        if ((type->ts & T_PTR) && (left->op == E_REG) && ((right->op == E_REG) || (right->op == E_CON))) {
            temp = new_tree(E_IMM, type);
            temp->u.mi.b = left->u.reg;

            if (right->op == E_CON) 
                temp->u.mi.ofs = right->u.con.i;
            else 
                temp->u.mi.i = right->u.reg;

            free_tree(right);
            free_tree(left);
            return generate_leaf(temp, goal, cc);
        }

        if (left->op == E_IMM) {
            if (right->op == E_CON) {
                left->u.mi.ofs += right->u.con.i;
                goto folded;
            }

            if (!left->u.mi.rip && (right->op == E_REG)) {
                if (left->u.mi.b == R_NONE) {
                    left->u.mi.b = right->u.reg;
                    goto folded;
                } else if (left->u.mi.i == R_NONE) {
                    left->u.mi.i = right->u.reg;
                    goto folded;
                }
            }

            if ((right->op == E_IMM) && combine_imm(left, right)) goto folded;
        }

        if (type->ts & T_IS_INTEGRAL) result_cc = CC_NZ;
        break;

    case E_SUB:

        /* E_CON - E_CON = E_CON (always)
           E_REG - E_CON = E_IMM (always, pointer types only)
           E_IMM - E_CON = E_IMM (always) */

        if ((left->op == E_CON) && (right->op == E_CON)) {
            if (type->ts & T_IS_FLOAT)
                left->u.con.f -= right->u.con.f;
            else 
                left->u.con.i -= right->u.con.i;

            goto folded;
        }

        if ((type->ts & T_PTR) && (left->op == E_REG) && (right->op == E_CON)) {
            temp = new_tree(E_IMM, type);
            temp->u.mi.b = left->u.reg;
            temp->u.mi.ofs = -(right->u.con.i);
            free_tree(left);
            free_tree(right);
            return generate_leaf(temp, goal, cc);
        }

        if ((left->op == E_IMM) && (right->op == E_CON)) {
            left->u.mi.ofs -= right->u.con.i;
            goto folded;
        }

        if (type->ts & T_IS_INTEGRAL) result_cc = CC_NZ;
        break;

    case E_MUL:
        /*   x   *   1   =   x   (always) 
           E_CON * E_CON = E_CON (always)
           E_REG * 2,4,8 = E_IMM (pointer types only) 
             x   *  2^n  =  x<<n (integers always) */

        if (right->op == E_CON) {
            if (    ((type->ts & (T_IS_INTEGRAL | T_PTR)) && (right->u.con.i == 1))
                ||  ((type->ts & T_IS_FLOAT) && (right->u.con.f == 1)) )
            {
                goto folded;
            }

            if (left->op == E_CON) {
                if (type->ts & T_IS_FLOAT)
                    left->u.con.f *= right->u.con.f;
                else 
                    left->u.con.i *= right->u.con.i;

                goto folded;
            }

            if ((type->ts & T_PTR) && (left->op == E_REG)) {
                if ((right->u.con.i == 2) || (right->u.con.i == 4) || (right->u.con.i == 8)) {
                    temp = new_tree(E_IMM, type);
                    temp->u.mi.i = left->u.reg;
                    temp->u.mi.s = right->u.con.i;
                    free_tree(left);
                    free_tree(right);
                    return generate_leaf(temp, goal, cc);
                }
            }
        }

        break;

    case E_MOD:
        /*   x   %   1   =     0     (always) 
           E_CON % E_CON =   E_CON   (always)
             x   %  2^n  = x&(2^n)-1 (unsigned integers always) */

        if (right->op == E_CON) {
            if (right->u.con.i == 1)
            {
                temp = left;
                left = right;
                right = temp;
                left->u.con.i = 0;
                goto folded;
            }

            if (left->op == E_CON) {
                if (type->ts & T_IS_SIGNED)
                    left->u.con.i %= right->u.con.i;
                else
                    left->u.con.i = ((unsigned long) left->u.con.i) % right->u.con.i;

                goto folded;
            }
        }

        break;

    case E_DIV:
        /*   x   /   1   =   x   (always) 
           E_CON * E_CON = E_CON (always) */

        if (right->op == E_CON) {
            if (    ((type->ts & (T_IS_INTEGRAL | T_PTR)) && (right->u.con.i == 1))
                ||  ((type->ts & T_IS_FLOAT) && (right->u.con.f == 1)) )
            {
                goto folded;
            }

            if (left->op == E_CON) {
                if (type->ts & T_IS_FLOAT)
                    left->u.con.f /= right->u.con.f;
                else if (type->ts & T_IS_SIGNED)
                    left->u.con.i /= right->u.con.i;
                else
                    left->u.con.i = ((unsigned long) left->u.con.i) / right->u.con.i;

                goto folded;
            }
        }

        break;

    case E_AND: 
        /* E_CON & E_CON = E_CON (always) */

        if ((left->op == E_CON) && (right->op == E_CON)) {
            left->u.con.i &= right->u.con.i;
            goto folded;
        }

        result_cc = CC_NZ;
        break;

    case E_OR: 
        /* E_CON | E_CON = E_CON (always) */

        if ((left->op == E_CON) && (right->op == E_CON)) {
            left->u.con.i |= right->u.con.i;
            goto folded;
        }
        result_cc = CC_NZ;
        break;

    case E_XOR:
        /* E_CON ^ E_CON = E_CON (always) */

        if ((left->op == E_CON) && (right->op == E_CON)) {
            left->u.con.i ^= right->u.con.i;
            goto folded;
        }

        result_cc = CC_NZ;
        break;

    case E_SHL:
        if ((left->op == E_CON) && (right->op == E_CON)) {
            left->u.con.i <<= right->u.con.i;
            goto folded;
        }
        /* fall-thru, next statement won't match .. */
    case E_SHR:
        if ((left->op == E_CON) && (right->op == E_CON)) {
            if (type->ts & T_IS_SIGNED) 
                left->u.con.i >>= right->u.con.i;
            else
                left->u.con.i = ((unsigned long) left->u.con.i) >> right->u.con.i;

            goto folded;
        }

        if (right->op == E_CON) 
            right->u.con.i &= 0xFF;
        else {
            temp = reg_tree(R_CX, new_type(T_UCHAR));
            choose(E_ASSIGN, copy_tree(temp), right);
            right = temp;
        }
        break;

    default: error(ERROR_INTERNAL);
    }

    /* generate a target temporary and compute the value into it. */

    tree = temporary(type);
    choose(E_ASSIGN, copy_tree(tree), left);

    if ((tree->type->ts & (T_IS_INTEGRAL | T_PTR)) && ((op == E_DIV) || (op == E_MOD))) 
        tree = divmod(op, tree, right, &result_cc);
    else 
        choose(op, copy_tree(tree), right);

    if ((goal == GOAL_CC) && (result_cc != CC_NONE)) {
        *cc = result_cc;
        free_tree(tree);
        return NULL;
    } 
        
    return generate_leaf(tree, goal, cc);

  folded:
    free_tree(right);
    free_type(left->type);
    left->type = type;
    normalize(left);
    return generate_leaf(left, goal, cc);
}

static struct tree *
generate_assop(tree, goal, cc)      /* E_ADDASS E_SUBASS E_MULASS E_DIVASS E_SHLASS E_SHRASS */
    struct tree * tree;             /* E_ANDASS E_ORASS E_XORASS */
    int *         cc;
{
    struct tree * left;
    struct tree * right;
    struct tree * temp;
    struct tree * original = NULL;
    int           op;
    int           result_cc = CC_NONE;

    op = E_FROM_ASSIGN(tree->op);
    decap_tree(tree, NULL, &left, &right, NULL);
    right = generate(right, GOAL_VALUE, NULL);

    /* bit field and memory floating-point targets must be 
       loaded into a temporary. save original in 'original'.
       ditto for integral targets of multiplication.

       non-constant shift operands must be in RCX. 

       memory-to-memory operations aren't permitted, so if
       left and right are both memory, put right in temporary. */

    if (left->type->ts & T_FIELD) {
        left = generate_fetch(left, GOAL_VALUE, NULL, 1);
        original = copy_tree(left);
        left = extract_field(left);
    } else 
        left = generate(left, GOAL_VALUE, NULL);

    if (    ((left->type->ts & T_IS_FLOAT) || (op == E_MUL))
        &&  (left->op != E_REG)) 
    {
        original = copy_tree(left);
        left = load(left);
    }

    if ((op == E_SHL) || (op == E_SHR)) {
        if (right->op == E_CON) 
            right->u.con.i &= 0xFF;
        else {
            temp = reg_tree(R_CX, new_type(T_UCHAR));
            choose(E_ASSIGN, copy_tree(temp), right);
            right = temp;
        }
    }

    if ((left->op == E_MEM) && (right->op == E_MEM)) right = load(right);

    /* we can use the condition codes directly for 
       some (integer) instructions. divmod(), below, might also. */

    switch (op)
    {
    case E_ADD:
    case E_SUB:
        if (left->type->ts & (T_IS_FLOAT)) break;
    case E_AND:
    case E_OR:
    case E_XOR:     
        result_cc = CC_NZ;
    }

    /* choose and emit */

    if ((left->type->ts & (T_IS_INTEGRAL | T_PTR)) && ((op == E_DIV) || (op == E_MOD))) 
        left = divmod(op, left, right, &result_cc);
    else
        choose(op, copy_tree(left), right);

    /* if 'original' is set, store the temporary back on our way out. */

    if (original && (original->type->ts & T_FIELD)) 
        return insert_field(original, left, goal, cc);

    if (original) choose(E_ASSIGN, original, copy_tree(left));

    if ((goal == GOAL_CC) && (result_cc != CC_NONE)) {
        *cc = result_cc;
        free_tree(left);
        return NULL;
    }

    return generate_leaf(left, goal, cc);
}

struct tree *
generate_prepost(tree, goal, cc)        /* E_POST E_PRE */
    struct tree * tree;
    int *         cc;
{
    struct tree * copy;
    struct tree * temp;

    /* E_PRE, and E_POST for effect only, are equivalent to E_ADDASS. */

    if ((tree->op == E_PRE) || (goal == GOAL_EFFECT)) {
        tree->op = E_ADDASS;
        return generate(tree, goal, cc);
    }

    /* the only way an lvalue can have side effects is if it's an
       indirection of some kind. to avoid double evaluation, reach down
       and pre-generate the E_FETCH child now, before we copy it.
       (Can't just generate() on the left child, because it might be
       a bitfield, and that would be a step too far for E_ADDASS.) */

    if (tree->u.ch[0]->op == E_FETCH) 
        tree->u.ch[0]->u.ch[0] = generate(tree->u.ch[0]->u.ch[0], GOAL_VALUE, NULL);
    
    copy = copy_tree(tree->u.ch[0]);
    copy = generate(copy, GOAL_VALUE, NULL);
    temp = temporary(copy_type(copy->type));
    choose(E_ASSIGN, copy_tree(temp), copy);
    tree->op = E_ADDASS;
    generate(tree, GOAL_EFFECT, NULL);
    return generate_leaf(temp, goal, cc);
}

/* generate() is called (recursively) to emit the instructions to evaluate 
   a tree. specify one of the following goals:

   GOAL_CC: 
   the expression is being evaluated for true/false (e.g., if/while).
   returns an empty tree, and '*cc' set to the CC_* correlating to TRUE.

   GOAL_VALUE:
   the expression is being evaluated for its value (typical use). returns
   the tree reduced to a leaf representing that value. 

   GOAL_EFFECT:
   evaluates the tree for side effects. returns an empty tree. 

   if GOAL_CC isn't specified, 'cc' isn't used and can/should be NULL. */

struct tree *
generate(tree, goal, cc)
    struct tree * tree;
    int *         cc;
{
    switch (tree->op)
    {
    case E_ADDR:    return generate_addr(tree, goal, cc);
    case E_FETCH:   return generate_fetch(tree, goal, cc, 0); 
    case E_CAST:    return generate_cast(tree, goal, cc); 
    case E_COM:     return generate_com(tree, goal, cc);
    case E_NOT:     return generate_not(tree, goal, cc);
    case E_NEG:     return generate_neg(tree, goal, cc);
    case E_CALL:    return generate_call(tree, goal, cc);
    case E_TERN:    return generate_ternary(tree, goal, cc);
    case E_COMMA:   return generate_comma(tree, goal, cc);
    case E_SYM:     return generate_sym(tree, goal, cc); 

    case E_CON:
    case E_IMM:
    case E_MEM:
    case E_REG:     return generate_leaf(tree, goal, cc); 

    case E_PRE:  
    case E_POST:    return generate_prepost(tree, goal, cc);

    case E_ADDASS:  
    case E_SUBASS:
    case E_MULASS:
    case E_MODASS:
    case E_DIVASS:  
    case E_SHLASS:
    case E_SHRASS:  
    case E_ANDASS:
    case E_XORASS:
    case E_ORASS:   return generate_assop(tree, goal, cc);

    case E_ASSIGN:  return generate_assign(tree, goal, cc); 

    case E_AND:
    case E_OR:
    case E_XOR:
    case E_SHL:
    case E_SHR:
    case E_ADD:
    case E_SUB:     
    case E_MUL:     
    case E_DIV:
    case E_MOD:     return generate_binary(tree, goal, cc);

    case E_EQ:
    case E_NEQ:
    case E_GT:
    case E_LT:
    case E_GTEQ:
    case E_LTEQ:    return generate_relational(tree, goal, cc);

    case E_LOR:
    case E_LAND:    return generate_logical(tree, goal, cc);

    default: error(ERROR_INTERNAL);
    }
}
