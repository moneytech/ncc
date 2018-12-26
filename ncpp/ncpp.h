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

extern FILE * output_file;

extern char * safe_malloc();

#ifdef __STDC__
extern void   fail(char *, ...);
extern void   output(char *, ...);
#endif

/* struct vstring represents a variable-length string */

struct vstring
{
    char * data;
    int    length;
    int    capacity;
};

extern struct vstring * vstring_new();
extern struct vstring * vstring_copy();
extern struct vstring * vstring_from_literal();
extern int              vstring_equal();
extern int              vstring_equal_s();
extern unsigned         vstring_hash();

struct macro
{
    struct vstring * name;
    struct list *    arguments;
    struct list *    replacement;
    struct macro *   link;
    int              predefined;
};

#define MACRO_LOOKUP_NORMAL 0
#define MACRO_LOOKUP_CREATE 1

extern struct macro * macro_lookup();

#define MACRO_REPLACE_ONCE   0
#define MACRO_REPLACE_REPEAT 1

struct input
{
    struct vstring * path;
    FILE *           file;
    int              line_number;
    struct input *   stack_link;
};

extern struct input * input_stack;

#define INPUT_LINE_NORMAL  0
#define INPUT_LINE_LIMITED 1

extern struct vstring * input_line();

#define INPUT_INCLUDE_LOCAL  0   
#define INPUT_INCLUDE_SYSTEM 1

struct token
{
    int            class;

    struct token * previous;
    struct token * next;

    union
    {
        struct vstring * text;
        int              argument_no;
        int              ascii;
        long             int_value;
        unsigned long    unsigned_value;
    } u;
};

struct list
{
    int             count;
    struct token *  first;
    struct token *  last;
};

/* token classes. be careful when changing this list- the values 
   must match the indices into the token_text[] array in token.c */

#define TOKEN_SPACE         0       /* u.ascii: whitespace (except newline) */
#define TOKEN_INT           1       /* u.int_value: integer (in expression) */
#define TOKEN_UNSIGNED      2       /* u.unsigned_value: unsigned (in expression) */ 
#define TOKEN_ARG           3       /* u.argument_no: placeholder for function-like macro argument */
#define TOKEN_UNKNOWN       4       /* u.ascii: any char in input not otherwise accounted for */
#define TOKEN_STRING        5       /* u.text: string literal */
#define TOKEN_CHAR          6       /* u.text: char constant */
#define TOKEN_NUMBER        7       /* u.text: preprocessing number */
#define TOKEN_NAME          8       /* u.text: an identifier subject to macro replacement */
#define TOKEN_EXEMPT_NAME   9       /* u.text: an identifier NOT subject to macro replacement */

#define TOKEN_GT            10      /* > */        
#define TOKEN_LT            11      /* < */         
#define TOKEN_GTEQ          12      /* >= */
#define TOKEN_LTEQ          13      /* <= */
#define TOKEN_SHL           14      /* << */
#define TOKEN_SHLEQ         15      /* <<= */
#define TOKEN_SHR           16      /* >> */
#define TOKEN_SHREQ         17      /* >>= */
#define TOKEN_EQ            18      /* = */
#define TOKEN_EQEQ          19      /* == */

#define TOKEN_NOTEQ         20      /* != */    
#define TOKEN_PLUS          21      /* + */         
#define TOKEN_PLUSEQ        22      /* += */
#define TOKEN_INC           23      /* ++ */
#define TOKEN_MINUS         24      /* - */
#define TOKEN_MINUSEQ       25      /* -= */
#define TOKEN_DEC           26      /* -- */
#define TOKEN_ARROW         27      /* -> */
#define TOKEN_LPAREN        28      /* ( */
#define TOKEN_RPAREN        29      /* ) */

#define TOKEN_LBRACK        30      /* [ */       
#define TOKEN_RBRACK        31      /* ] */         
#define TOKEN_LBRACE        32      /* { */
#define TOKEN_RBRACE        33      /* } */
#define TOKEN_COMMA         34      /* , */
#define TOKEN_DOT           35      /* . */
#define TOKEN_QUEST         36      /* ? */
#define TOKEN_COLON         37      /* : */
#define TOKEN_SEMI          38      /* ; */
#define TOKEN_OR            39      /* | */

#define TOKEN_OROR          40      /* || */      
#define TOKEN_OREQ          41      /* |= */      
#define TOKEN_AND           42      /* & */
#define TOKEN_ANDAND        43      /* && */
#define TOKEN_ANDEQ         44      /* &= */
#define TOKEN_MUL           45      /* * */
#define TOKEN_MULEQ         46      /* *= */
#define TOKEN_MOD           47      /* % */
#define TOKEN_MODEQ         48      /* %= */
#define TOKEN_XOR           49      /* ^ */

#define TOKEN_XOREQ         50      /* ^= */   
#define TOKEN_NOT           51      /* ! */         
#define TOKEN_HASH          52      /* # */
#define TOKEN_HASHHASH      53      /* ## (impotent) */
#define TOKEN_DIV           54      /* / */
#define TOKEN_DIVEQ         55      /* /= */
#define TOKEN_TILDE         56      /* ~ */
#define TOKEN_ELLIPSIS      57      /* ... */
#define TOKEN_PASTE         58      /* ## (in macro replacement list) */

extern struct token * token_new();
extern struct token * token_copy();
extern struct token * token_paste();
extern int            token_equal();
extern struct list  * list_new();
extern struct token * list_unlink();
extern struct token * list_delete();
struct list         * list_copy();
extern int            list_equal();

#define LIST_TRIM_LEADING       0x00000001
#define LIST_TRIM_TRAILING      0x00000002
#define LIST_TRIM_EDGES         (LIST_TRIM_LEADING | LIST_TRIM_TRAILING)
#define LIST_TRIM_FOLD          0x00000004
#define LIST_TRIM_STRIP         0x00000008

#define LIST_GLUE_RAW       0
#define LIST_GLUE_STRINGIZE 1

extern struct vstring * list_glue();

#define SKIP_SPACES(t) while ((t) && ((t)->class == TOKEN_SPACE)) ((t) = (t)->next)

