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

/* types are represented by lists of type nodes. */

struct type
{
    int             ts;           /* T_* */
    struct type *   next;
    int             nr_elements;  /* T_ARRAY */
    struct symbol * tag;          /* T_TAG */
};

/* type nodes will have exactly one of the following bits set. they're defined
   as a powerset so that testing for groups of types is a simple operation. 
   these types are also ordered to make the usual arithmetic conversions easy -
   see usuals() in tree.c */

#define T_CHAR      0x00000001      
#define T_UCHAR     0x00000002
#define T_SHORT     0x00000004
#define T_USHORT    0x00000008
#define T_INT       0x00000010
#define T_UINT      0x00000020
#define T_LONG      0x00000040
#define T_ULONG     0x00000080
#define T_FLOAT     0x00000100
#define T_LFLOAT    0x00000200
#define T_TAG       0x00000400      /* struct/union 'u.tag' */
#define T_ARRAY     0x00000800      /* array[nr_elements] of ... */
#define T_PTR       0x00001000      /* ptr to ... */
#define T_FUNC      0x00002000      /* function returning ... */

#define T_BASE      0x00002FFF      /* bits that must be exclusive */

/* bit fields must have one of the integral T_* bits set, as well as T_FIELD.
   the size and shift are then encoded using the macros below. these types
   will only appear in the symbol table as a struct/union member, and will only 
   appear in expression trees under an E_FETCH or E_MEM referencing those members. */

#define T_FIELD                 0x80000000

#define T_GET_SIZE(ts)          (((ts) & 0x3F000000) >> 24)
#define T_SET_SIZE(ts, bits)    ((ts) |= (((bits) & 0x3F) << 24))
#define T_GET_SHIFT(ts)         (((ts) & 0x003F0000) >> 16)
#define T_SET_SHIFT(ts, bits)   ((ts) |= (((bits) & 0x3F) << 16))

/* useful sets of types */

#define T_IS_CHAR       (T_CHAR | T_UCHAR)
#define T_IS_SHORT      (T_SHORT | T_USHORT)
#define T_IS_INT        (T_INT | T_UINT)
#define T_IS_LONG       (T_LONG | T_ULONG)
#define T_IS_FLOAT      (T_FLOAT | T_LFLOAT)

#define T_IS_SIGNED     (T_CHAR | T_SHORT | T_INT | T_LONG)
#define T_IS_UNSIGNED   (T_UCHAR | T_USHORT | T_UINT | T_ULONG)
#define T_IS_INTEGRAL   (T_IS_SIGNED | T_IS_UNSIGNED)
#define T_IS_ARITH      (T_IS_FLOAT | T_IS_INTEGRAL)

#define T_IS_SCALAR     (T_IS_ARITH | T_PTR)

#define T_IS_BYTE       T_IS_CHAR
#define T_IS_WORD       T_IS_SHORT
#define T_IS_DWORD      (T_IS_INT | T_FLOAT)
#define T_IS_QWORD      (T_IS_LONG | T_PTR | T_LFLOAT)

