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

/* all lexical strings encountered (both identifiers and string literals) 
   are kept in a hash table for the lifetime of the compilation. this simplifies
   memory management and speeds comparisons. technique from Fraser and Hanson's LCC.

   the buckets are kept in LRU order to mitigate the overhead associated with
   passing every identifier through the string table.

   if 'asm_label' is non-zero, then this is a string literal that must be output 
   to the text section at the end of compilation - see literals().

   if 'token' is not KK_IDENT, then this is a keyword with that token code.

   'data' is guaranteed to be NUL-terminated, so can be used as a C string.
   just keep in mind that string literals may have embedded NULs. */

struct string
{
    unsigned        hash;
    int             length;
    char *          data;
    int             asm_label;
    int             token;
    struct string * link;
};

/* the symbol table is a hash table where each bucket is ordered
   (in decreasing order) by 'scope'. 

   SCOPE_NONE is just an initial value. no one should end up in the
       symbol table at this scope.

   SCOPE_GLOBAL is file scope.

   SCOPE_FUNCTION ... SCOPE_MAX are the (nesting) function block scopes. 

   SCOPE_RETIRED is a temporary home for non-globals that have gone out of
       scope. we keep them until finished generating the current function. */

#define SCOPE_NONE      0  
#define SCOPE_GLOBAL    1        
#define SCOPE_FUNCTION  2          
#define SCOPE_RETIRED   (SCOPE_MAX + 1)

/* the S_* flags encompass "storage classes" and a few other things. the 'ss' field of
   a symbol is a basic indicator of its kind. S_* are defined as bitfields so that a
   simple bitwise-and can be used to determine if two symbols are in the same namespace. 
   
   some explanation of block-level storage classes is in order:

   S_LOCAL is the storage class assigned to block-level symbols without an explicit 
   storage class. during the processing of the function, if the address is taken of an
   S_LOCAL, it is converted to S_AUTO. at the end of a function, before the optimizer 
   is invoked, all S_LOCALs are converted to S_REGISTER. thus the optimizer can assume
   that an S_AUTO is aliased, and S_REGISTER is not. */

#define S_NONE          0

#define S_STRUCT        0x00000001
#define S_UNION         0x00000002

    /* tags live in their own namespace */

#define S_TAG           (S_STRUCT | S_UNION)    
                        
#define S_TYPEDEF       0x00000004
#define S_STATIC        0x00000008
#define S_EXTERN        0x00000010
#define S_AUTO          0x00000020
#define S_REGISTER      0x00000040
#define S_LOCAL         0x00000080

    /* S_BLOCK is shorthand for all block-level symbols */

#define S_BLOCK         (S_AUTO | S_REGISTER | S_LOCAL)

    /* S_NORMAL covers all symbols in the "normal" namespace */

#define S_NORMAL        (S_LOCAL | S_REGISTER | S_AUTO | S_TYPEDEF | S_STATIC | S_EXTERN)

#define S_MEMBER        0x00000100
#define S_LABEL         0x00000200

    /* formal arguments are put in this storage class until 
       their types have been declared in the function header */

#define S_ARG           0x00000400

    /* the S_REFERENCED flag is set on a symbol once it's
       actually used in an expression. we only care about this
       on S_EXTERNs, and that's just to keep from emitting 
       unnecessary directives at the end of compilation. */

#define S_REFERENCED    0x40000000

    /* the S_DEFINED flag is used by labels, struct/union tags, 
       and extern/static objects/functions to indicate a definition
       has already been seen. */

#define S_DEFINED       0x80000000 

struct symbol
{
    struct string * id;         /* can be NULL (anonymous symbols) */
    int             ss;         /* S_* */
    int             scope;      /* SCOPE_* */
    struct type *   type;       /* NULL if not applicable (e.g., S_TAG) */
    int             reg;        /* assigned pseudo-register (or R_NONE) */
    int             align;      /* S_TAG only */
    struct block *  target;     /* S_LABEL only */
    struct symbol * link;       /* table bucket link */

    /* 'i' is vaguely named because it's a general purpose holder.
    
       S_STATIC: for non-global static objects, this is the
       assembler label assigned to the storage.

       S_MEMBER: offset from the struct/union base.

       S_AUTO/S_REGISTER/S_LOCAL: offset from the frame pointer. 
       note that if this is 0, the address hasn't been assigned yet!
       (assignments for S_REGISTER might be deferred until spilled.) 
       
       S_STRUCT/S_UNION: when S_DEFINED, the size in in BYTES.
       When !S_DEFINED, size is a running counter IN BITS. */

    int             i;

    /* S_MEMBERs live in lists threaded through the symbol table: the 
       S_STRUCT or S_UNION container holds the head of the list. formal 
       arguments also use this field, but the head is maintained externally. */

    struct symbol * list;   
};
