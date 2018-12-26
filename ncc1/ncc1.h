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

#ifndef __STDC__
typedef long float double;
#define strtod strtolf
#else
extern void output(char * fmt, ...);
#endif

/* let's start off with the basics. */

#define BITS                8

/* some useful generic macros. */

#define MIN(a,b)            (((a) < (b)) ? (a) : (b))
#define MAX(a,b)            (((a) > (b)) ? (a) : (b))
#define ROUND_UP(a,b)       (((a) % (b)) ? ((a) + ((b) - ((a) % (b)))) : (a))
#define ROUND_DOWN(a,b)     ((a) - ((a) % (b)))

/* same basic data about the activation records. probably
   shouldn't be changed, at least not without serious consideration. */

#define FRAME_ARGUMENTS     16      /* start of arguments in frame */
#define FRAME_ALIGN         8       /* always 8-byte aligned */

/* number of buckets in the hash tables. a power of two is preferable.
   more buckets can improve performance, but with NR_SYMBOL_BUCKETS in 
   particular, larger numbers can have a negative impact, as every bucket 
   must be scanned when exiting a scope. */

#define NR_STRING_BUCKETS   64
#define NR_SYMBOL_BUCKETS   32

/* limits the level of block nesting. the number is arbitrary, but
   it must be at least "a few less" than INT_MAX at most. */

#define SCOPE_MAX           1000

/*
 * MAX_SIZE limits the number of bytes specified by a type. 256MB - 1
 * is currently the largest safe value, due to the use of 'int' to 
 * store type sizes and the expansion from byte counts to bit counts
 * in certain places (notably the struct type code). increasing this
 * maximum doesn't seem to be worth the penalty in speed and complexity.
 */

#define MAX_SIZE            ((256 * 1024 * 1024) - 1)

#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include "token.h"
#include "tree.h"
#include "symbol.h"
#include "type.h"
#include "reg.h"
#include "block.h"

extern int              g_flag;
extern int              O_flag;
extern FILE *           yyin;
extern struct token     token;
extern int              line_number;
extern struct string *  input_name;
extern struct string *  output_name;
extern FILE *           output_file;
extern int              current_scope;
extern int              next_asm_label;
extern int              next_iregister;
extern int              next_fregister;
extern int              loop_level;
extern struct symbol *  current_function;
extern int              frame_offset;
extern int              save_iregs;
extern int              save_fregs;
extern struct block *   entry_block;
extern struct block *   current_block;
extern struct block *   exit_block;
extern struct block *   first_block;
extern struct block *   last_block;

extern char *           allocate();
extern struct string *  stringize();
extern struct symbol *  string_symbol();
extern struct symbol *  new_symbol();
extern struct symbol *  find_symbol();
extern struct symbol *  find_symbol_by_reg();
extern struct symbol *  find_typedef();
extern struct symbol *  find_symbol_list();
extern struct symbol *  temporary_symbol();
extern struct symbol *  find_label();
extern struct type *    new_type();
extern struct type *    copy_type();
extern struct type *    splice_types();
extern struct type *    argument_type();
extern struct tree *    new_tree();
extern struct tree *    copy_tree();
extern struct tree *    expression();
extern struct tree *    assignment_expression();
extern struct tree *    conditional_expression();
extern struct tree *    scalar_expression();
extern struct tree *    null_pointer();
extern struct tree *    reg_tree();
extern struct tree *    stack_tree();
extern struct tree *    memory_tree();
extern struct tree *    symbol_tree();
extern struct tree *    int_tree();
extern struct tree *    float_tree();
extern struct tree *    addr_tree();
extern struct type *    abstract_type();
extern struct block *   new_block();
extern struct block *   block_successor();
extern struct block *   block_predecessor();
extern struct insn *    new_insn();
extern struct tree *    generate();
extern struct tree *    float_literal();
extern struct defuse *  find_defuse();
extern struct defuse *  find_defuse_by_symbol();

/* goals for generate() */

#define GOAL_EFFECT             0
#define GOAL_CC                 1
#define GOAL_VALUE              2

/* modes for find_defuse() */

#define FIND_DEFUSE_NORMAL      0
#define FIND_DEFUSE_CREATE      1

/* flags for generate() */

#define GENERATE_LVALUE         0x00000001

/* flags for declarations() */

#define DECLARATIONS_ARGS       0x00000001
#define DECLARATIONS_INTS       0x00000002 
#define DECLARATIONS_FIELDS     0x00000004

/* flags for check_types() */

#define CHECK_TYPES_COMPOSE     0x00000001

/* output segments */

#define SEGMENT_TEXT    0           /* code */
#define SEGMENT_DATA    1           /* initialized data */

/* these codes must match the indices of errors[] in cc1.c */

#define ERROR_CMDLINE       0       /* bad command line */
#define ERROR_INPUT         1       /* input file error */
#define ERROR_SYNTAX        2       /* syntax error */
#define ERROR_MEMORY        3       /* out of memory */
#define ERROR_DIRECTIVE     4       /* bad # directive */
#define ERROR_LEXICAL       5       /* invalid character in input */
#define ERROR_BADICON       6       /* malformed integral constant */
#define ERROR_IRANGE        7       /* integral constant out of range */
#define ERROR_BADFCON       8       /* malformed floating constant */
#define ERROR_FRANGE        9       /* floating constant out of range */
#define ERROR_ESCAPE        10      /* invalid octal escape sequence */
#define ERROR_UNTERM        11      /* unterminated string literal */
#define ERROR_BADCCON       12      /* invalid character constant */
#define ERROR_CRANGE        13      /* multi-character constant out of range */
#define ERROR_OUTPUT        14      /* output file error */
#define ERROR_NESTING       15      /* nesting level too deep */
#define ERROR_TYPESIZE      16      /* type too big */
#define ERROR_INCOMPLT      17      /* incomplete type */
#define ERROR_ILLFUNC       18      /* illegal use of function type */
#define ERROR_ILLARRAY      19      /* illegal array specification */
#define ERROR_RETURN        20      /* illegal function return type */
#define ERROR_STRUCT        21      /* illegal use of struct/union type */
#define ERROR_NOARGS        22      /* misplaced formal arguments */
#define ERROR_ILLFIELD      23      /* illegal use of bit field */
#define ERROR_FIELDTY       24      /* illegal bit field type */
#define ERROR_FIELDSZ       25      /* invalid bit field size */
#define ERROR_SCLASS        26      /* storage class not permitted */
#define ERROR_TAGREDEF      27      /* struct/union already defined */
#define ERROR_EMPTY         28      /* empty struct or union */
#define ERROR_REMEMBER      29      /* duplicate member declaration */
#define ERROR_REDECL        30      /* illegal redeclaration */
#define ERROR_NOTARG        31      /* unknown argument identifier */
#define ERROR_NOFUNC        32      /* functions must be globals */
#define ERROR_TYPEDEF       33      /* can't do that with typedef */
#define ERROR_UNKNOWN       34      /* unknown identifier */
#define ERROR_OPERANDS      35      /* illegal operands */
#define ERROR_INCOMPAT      36      /* incompatible operands */
#define ERROR_LVALUE        37      /* not an lvalue */
#define ERROR_ABSTRACT      38      /* abstract declarator required */
#define ERROR_MISSING       39      /* declarator missing identifier */
#define ERROR_BADCAST       40      /* bad typecast */
#define ERROR_INDIR         41      /* illegal indirection */
#define ERROR_NOTSTRUCT     42      /* left side must be struct */
#define ERROR_NOTMEMBER     43      /* not a member of that struct/union */
#define ERROR_NEEDFUNC      44      /* function type required */
#define ERROR_INTERNAL      45      /* compiler internal error */
#define ERROR_REGISTER      46      /* can't take address of register */
#define ERROR_CONEXPR       47      /* constant expression required */
#define ERROR_DIV0          48      /* division by zero */
#define ERROR_BADEXPR       49      /* illegal constant expression */
#define ERROR_SEGMENT       50      /* wrong segment */
#define ERROR_BADINIT       51      /* bad initializer */
#define ERROR_DUPDEF        52      /* duplicate definition */
#define ERROR_MISPLACED     53      /* misplaced break, continue or case */
#define ERROR_DANGLING      54      /* undefined label */
#define ERROR_DUPCASE       55      /* duplicate case label */
#define ERROR_CASE          56      /* switch/case must be integral */
