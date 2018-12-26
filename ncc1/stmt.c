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

#include <stdlib.h>
#include "ncc1.h"

/* switch cases are kept in a linked list */

struct switchcase
{
    struct tree       * value;
    struct block      * target;
    struct switchcase * next;
};

/* these variables are associated with the closest-enclosing 
   block of appropriate type. the parser routines deal with 
   saving/restoring their contents in their activation records. */

static struct switchcase  * switchcases;
static struct type        * switch_type;
static struct block       * break_block;
static struct block       * continue_block;
static struct block       * default_block;

static statement();

static
if_statement()
{
    struct block * true_block;
    struct block * else_block;
    struct block * join_block;
    struct tree  * test;
    int            cc;

    true_block = new_block();
    else_block = new_block();
    join_block = new_block();
    
    lex();
    match(KK_LPAREN);
    test = expression();
    test = scalar_expression(test);
    match(KK_RPAREN);
    generate(test, GOAL_CC, &cc);
    succeed_block(current_block, cc, true_block);
    succeed_block(current_block, CC_INVERT(cc), else_block);

    current_block = true_block;
    statement();
    true_block = current_block;
    succeed_block(true_block, CC_ALWAYS, join_block);

    if (token.kk == KK_ELSE) {
        lex();
        current_block = else_block;
        statement();
        else_block = current_block;
    }

    succeed_block(else_block, CC_ALWAYS, join_block);
    current_block = join_block;
}

static
while_statement()
{
    struct block * test_block;
    struct block * body_block;
    struct block * saved_break_block;
    struct block * saved_continue_block;
    struct tree *  test;
    int            cc;

    saved_continue_block = continue_block;
    saved_break_block = break_block;

    test_block = new_block();
    body_block = new_block();
    break_block = new_block();
    continue_block = test_block;
    succeed_block(current_block, CC_ALWAYS, test_block);
    current_block = test_block;

    lex();
    match(KK_LPAREN);
    test = expression();
    test = scalar_expression(test);
    match(KK_RPAREN);
    generate(test, GOAL_CC, &cc);
    succeed_block(current_block, cc, body_block);
    succeed_block(current_block, CC_INVERT(cc), break_block);

    current_block = body_block;
    statement();
    body_block = current_block;
    succeed_block(body_block, CC_ALWAYS, test_block);

    current_block = break_block;
    continue_block = saved_continue_block;
    break_block = saved_break_block;
}

static
do_statement()
{
    struct block * saved_continue_block;
    struct block * saved_break_block;
    struct block * body_block;
    struct tree *  test;
    int            cc;

    saved_continue_block = continue_block;
    saved_break_block = break_block;
    continue_block = new_block();
    break_block = new_block();
    body_block = new_block();
    succeed_block(current_block, CC_ALWAYS, body_block);
    current_block = body_block;

    lex();
    statement();
    match(KK_WHILE);
    match(KK_LPAREN);
    test = expression();
    test = scalar_expression(test);
    match(KK_RPAREN);
    match(KK_SEMI);
    succeed_block(current_block, CC_ALWAYS, continue_block);
    current_block = continue_block;
    generate(test, GOAL_CC, &cc);
    succeed_block(current_block, cc, body_block);
    succeed_block(current_block, CC_INVERT(cc), break_block);

    current_block = break_block;
    continue_block = saved_continue_block;
    break_block = saved_break_block;
}

static
return_statement()
{
    struct type * return_type;
    struct tree * tree;

    match(KK_RETURN);
    return_type = current_function->type->next;

    if (token.kk != KK_SEMI) {
        if (return_type->ts & T_IS_FLOAT) 
            tree = reg_tree(R_XMM0, copy_type(return_type));
        else
            tree = reg_tree(R_AX, copy_type(return_type));

        tree = assignment_expression(tree);
        generate(tree, GOAL_EFFECT, NULL);
    }

    succeed_block(current_block, CC_ALWAYS, exit_block);
    current_block = new_block();
    match(KK_SEMI);
}

static 
for_statement()
{
    struct block * saved_continue_block;
    struct block * saved_break_block;
    struct block * test_block;
    struct block * body_block;
    struct tree *  initial = NULL;
    struct tree *  test = NULL;
    struct tree *  step = NULL;
    int            cc;

    saved_continue_block = continue_block;
    saved_break_block = break_block;
    test_block = new_block();
    body_block = new_block();
    continue_block = new_block();
    break_block = new_block();

    lex();
    match(KK_LPAREN);
    if (token.kk != KK_SEMI) initial = expression();
    match(KK_SEMI);
    if (token.kk != KK_SEMI) test = expression();
    match(KK_SEMI);
    if (token.kk != KK_RPAREN) step = expression();
    match(KK_RPAREN);

    if (initial) generate(initial, GOAL_EFFECT, NULL);
    succeed_block(current_block, CC_ALWAYS, test_block);
    current_block = test_block;

    if (test) {
        generate(test, GOAL_CC, &cc);
        succeed_block(current_block, cc, body_block);
        succeed_block(current_block, CC_INVERT(cc), break_block);
    } else
        succeed_block(current_block, CC_ALWAYS, body_block);

    current_block = body_block;
    statement();
    succeed_block(current_block, CC_ALWAYS, continue_block);

    current_block = continue_block;
    if (step) generate(step, GOAL_EFFECT, NULL);
    succeed_block(current_block, CC_ALWAYS, test_block);
    
    current_block = break_block;
    continue_block = saved_continue_block;
    break_block = saved_break_block;
}

compound()
{
    enter_scope();
    match(KK_LBRACE);
    local_declarations();
    while (token.kk != KK_RBRACE) statement();
    match(KK_RBRACE);
    exit_scope();
}

static
loop_control(block)
    struct block * block;
{
    lex();
    if (block == NULL) error(ERROR_MISPLACED);
    succeed_block(current_block, CC_ALWAYS, block);
    current_block = new_block();
    match(KK_SEMI);
}

static
label_statement()
{
    struct symbol * label;

    label = find_label(token.u.text);
    if (label->ss & S_DEFINED) error(ERROR_DUPDEF);
    label->ss |= S_DEFINED;

    lex();
    match(KK_COLON);

    succeed_block(current_block, CC_ALWAYS, label->target);
    current_block = label->target;

    statement();
}

static
goto_statement()
{
    struct symbol * label;

    match(KK_GOTO);
    expect(KK_IDENT);
    label = find_label(token.u.text);
    match(KK_IDENT);
    match(KK_SEMI);

    succeed_block(current_block, CC_ALWAYS, label->target);
    current_block = new_block();
}

static
switch_statement()
{
    struct switchcase * saved_switchcases;
    struct block      * saved_default_block;
    struct block      * saved_break_block;
    struct type       * saved_switch_type;
    struct tree       * tree;
    struct tree       * reg_ax;
    struct block      * control_block;

    saved_switchcases = switchcases;
    saved_default_block = default_block;
    saved_break_block = break_block;
    saved_switch_type = switch_type;
    switchcases = NULL;
    default_block = NULL;
    break_block = new_block();

    lex();
    match(KK_LPAREN);
    tree = expression();
    tree = generate(tree, GOAL_VALUE, NULL);
    if (!(tree->type->ts & T_IS_INTEGRAL)) error(ERROR_CASE);
    switch_type = copy_type(tree->type);
    reg_ax = reg_tree(R_AX, copy_type(switch_type));
    choose(E_ASSIGN, copy_tree(reg_ax), tree);
    match(KK_RPAREN);
    control_block = current_block;

    current_block = new_block();
    statement();
    succeed_block(current_block, CC_ALWAYS, break_block);
    if (default_block == NULL) default_block = break_block;

    current_block = control_block;

    while (switchcases) {
        struct switchcase * switchcase;
        struct block      * tmp;

        switchcase = switchcases;
        switchcases = switchcases->next;

        put_insn(current_block, new_insn(I_CMP, copy_tree(reg_ax), switchcase->value), NULL);
        succeed_block(current_block, CC_Z, switchcase->target);
        succeed_block(current_block, CC_NZ, tmp = new_block());
        current_block = tmp;

        free(switchcase);
    }

    succeed_block(current_block, CC_ALWAYS, default_block);

    free_tree(reg_ax);
    current_block = break_block;
    switchcases = saved_switchcases;
    default_block = saved_default_block;
    break_block = saved_break_block;
    switch_type = saved_switch_type;
}

static
case_statement()
{
    struct switchcase   * switchcase;
    struct tree         * value;

    if (switch_type == NULL) error(ERROR_MISPLACED);

    if (token.kk == KK_DEFAULT) {
        lex();
        if (default_block) error(ERROR_DUPCASE);
        default_block = new_block();
        succeed_block(current_block, CC_ALWAYS, default_block);
        current_block = default_block;
    } else {
        lex();

        value = expression();
        if (!(value->type->ts & T_IS_INTEGRAL)) error(ERROR_CASE);
        value = new_tree(E_CAST, copy_type(switch_type), value);
        value = generate(value, GOAL_VALUE, NULL);
        if (value->op != E_CON) error(ERROR_CONEXPR);

        for (switchcase = switchcases; switchcase; switchcase = switchcase->next)
            if (switchcase->value->u.con.i == value->u.con.i) error(ERROR_DUPCASE);

        switchcase = (struct switchcase *) allocate(sizeof(struct switchcase));
        switchcase->value = value;
        switchcase->target = new_block();
        switchcase->next = switchcases;
        switchcases = switchcase;
    
        succeed_block(current_block, CC_ALWAYS, switchcase->target);
        current_block = switchcase->target;
    }
    
    match(KK_COLON);
    statement();
}

static
statement()
{
    struct tree * tree;

    switch (token.kk) {
    case KK_LBRACE:
        compound();
        break;
    case KK_BREAK:
        loop_control(break_block);
        break;
    case KK_CONTINUE:
        loop_control(continue_block);
        break;
    case KK_CASE:
    case KK_DEFAULT:
        case_statement();
        break;
    case KK_DO:
        do_statement();
        break;
    case KK_IF:
        if_statement();
        break;
    case KK_FOR:
        for_statement();
        break;
    case KK_GOTO:
        goto_statement();
        break;
    case KK_SWITCH:
        switch_statement();
        break;
    case KK_WHILE:
        while_statement();
        break;
    case KK_RETURN:
        return_statement();
        break;
    case KK_IDENT:
        if (peek(NULL) == KK_COLON) {
            label_statement();
            break;
        }
        /* fall through */
    default: 
        tree = expression();
        tree = generate(tree, GOAL_EFFECT, NULL);
    case KK_SEMI:
        match(KK_SEMI);
    }
}

