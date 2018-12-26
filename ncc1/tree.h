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

/* a conventional tree representation is used for expressions. a node
   may be a leaf, or have up to three children. on occasion (that is,
   for function arguments), a forest of trees is created via 'list'.

   the members of this struct have purposely terse names, as this
   (counter-intuitively) makes the code that manipulates trees more
   digestible, once the shorthand is grokked. */

#define NR_TREE_CH 3

struct tree
{
    int           op;           /* E_* */
    struct tree * list;
    struct type * type;

    union
    {
        /* E_SYM represents the value referred to by the symbol.
           these are used during tree construction - early in code
           generation these turn into E_REG or E_MEM nodes. */

        struct symbol *     sym;   

        /* E_REG represents a value held in a register. */

        int                 reg;   

        /* E_CON represents a constant. all floating point constants and 
           arithmetic in the compiler are performed in double precision.
           integers of all sizes are kept normalized (that is, sign- or
           zero-extended to long) to simplify constant folding. */

        union                     
        {
            long            i;
            double          f;
        } con;  

        /* E_MEM represents a value held in memory, at [b+i*s+glob+ofs] or 
           [rip glob+ofs].  E_IMM represents the address of that value. 'glob'
           must be a symbol known to the assembler (S_EXTERN or S_STATIC). */

        struct                      /* E_IMM or E_MEM */
        {
            struct symbol * glob;       /* global */
            long            ofs;        /* offset */
            int             b, i;       /* base and index registers */
            int             s;          /* 1, 2, 4 or 8: scale for index reg */
            int             rip;        /* rIP-relative (flag) */
        } mi;

        struct tree *   ch[NR_TREE_CH]; 
    } u;
};

/* the E_* numbers aren't arbitrary assigned. for starters, they're ordered and
   grouped by the number of children they have. in addition, binary operators are
   a fixed numeric distance from their assignment counterparts (if they exist). 
   finally, the numbers must match the indices into the table for debug_tree() */

#define E_IS_LEAF(x)    ((x) < E_FETCH)
#define E_HAS_CH0(x)    ((x) >= E_FETCH)
#define E_HAS_CH1(x)    ((x) >= E_ADD) 
#define E_HAS_CH2(x)    ((x) == E_TERN)

#define E_TO_ASSIGN(x)      ((x) + (E_ADDASS - E_ADD))
#define E_FROM_ASSIGN(x)    ((x) - (E_ADDASS - E_ADD))

    /* leaves */

#define E_NOP       0
#define E_SYM       1       /* tree.u.sym */
#define E_CON       2       /* tree.u.con */
#define E_IMM       3       /* tree.u.mi */
#define E_MEM       4       /* tree.u.mi */
#define E_REG       5       /* tree.u.reg */

    /* unary operators */

#define E_FETCH     6       /* * */
#define E_ADDR      7       /* & */
#define E_NEG       8       /* - */
#define E_COM       9       /* ~ */
#define E_CAST      10      /* (cast) */
#define E_NOT       11      /* ! */
    
    /* binary operators */

#define E_ADD       12      /* + */
#define E_SUB       13      /* - */
#define E_MUL       14      /* * */
#define E_DIV       15      /* / */
#define E_MOD       16      /* % */
#define E_SHL       17      /* << */
#define E_SHR       18      /* >> */
#define E_AND       19      /* & */
#define E_OR        20      /* | */
#define E_XOR       21      /* ^ */

#define E_ADDASS    22      /* += */
#define E_SUBASS    23      /* -= */
#define E_MULASS    24      /* *= */
#define E_DIVASS    25      /* /= */
#define E_MODASS    26      /* %= */
#define E_SHLASS    27      /* <<= */
#define E_SHRASS    28      /* >>= */
#define E_ANDASS    29      /* &= */
#define E_ORASS     30      /* |= */
#define E_XORASS    31      /* ^= */

#define E_LAND      32      /* && */
#define E_LOR       33      /* || */
#define E_EQ        34      /* == */
#define E_NEQ       35      /* != */
#define E_GT        36      /* > */
#define E_LT        37      /* < */
#define E_GTEQ      38      /* >= */
#define E_LTEQ      39      /* <= */
#define E_COMMA     40      /* , */
#define E_ASSIGN    41      /* = */

    /* the increment/decrement operators are unary in C, but binary in 
       the trees. the second operand specifies the amount to add/subtract,
       so the front end does the scaling. */

#define E_PRE       42      /* prefix -- or ++ */
#define E_POST      43      /* postfix -- or ++ */

    /* function call is considered binary, with ch[0]
       being the function to call, and ch[1] being the
       head of the argument expression forest. */

#define E_CALL      44     

    /* the ternary operator. note that the children aren't
       ordered as one might expect. the condition is last.
       "a ? b : c", ch[0] = b, ch[1] = c, ch[2] = a. */

#define E_TERN      45
