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

/* create a new type node and give it sane defaults */

struct type *
new_type(ts)
{
    struct type * type;

    type = (struct type *) allocate(sizeof(struct type));
    type->ts = ts;
    type->nr_elements = 0;
    type->tag = NULL;
    type->next = NULL;
    return type;
}

/* free a type - the whole type, not just the node.
   safe to call with NULL 'type'. */

free_type(type)
    struct type * type;
{
    struct type * tmp;

    while (type) {
        tmp = type->next;
        free(type);
        type = tmp;
    }
}

/* return a copy of the type */

struct type *
copy_type(type)
    struct type * type;
{
    struct type *  copy = NULL;
    struct type ** typep = &copy;

    while (type) {
        *typep = new_type(type->ts);
        (*typep)->nr_elements = type->nr_elements;
        (*typep)->tag = type->tag;
        type = type->next;
        typep = &((*typep)->next);
    }

    return copy;
}

/* glue two types into one by appending 'type2' on the end of 'type1'. */

struct type *
splice_types(type1, type2)
    struct type * type1;
    struct type * type2;
{
    struct type * tmp;

    if (type1 == NULL) return type2;
    for (tmp = type1; tmp->next; tmp = tmp->next) ;
    tmp->next = type2;

    return type1;
}

/* check for compatibility between two types. and merge any 
   additional information from 'type2' into 'type1'. */

check_types(type1, type2, mode)
    struct type * type1;
    struct type * type2;
{
    while (type1 && type2) {
        if ((type1->ts & T_BASE) != (type2->ts & T_BASE)) break;
        if (type1->tag != type2->tag) break;
        if ((type1->nr_elements && type2->nr_elements) && (type1->nr_elements != type2->nr_elements)) break;
        if ((mode == CHECK_TYPES_COMPOSE) && (type2->nr_elements)) type1->nr_elements = type2->nr_elements;
        type1 = type1->next;
        type2 = type2->next;
    }

    if (type1 || type2) error(ERROR_INCOMPAT);
}

/* return the size or alignment of the type in bytes.
   abort with an error if the value isn't, or can't be, known. */

size_of(type)
    struct type * type;
{
    long size = 1;

    while (type) {
        if (type->ts & (T_CHAR | T_UCHAR)) 
            /* size *= 1 */ ;
        else if (type->ts & (T_SHORT | T_USHORT)) 
            size *= 2;
        else if (type->ts & (T_INT | T_UINT | T_FLOAT)) 
            size *= 4;
        else if (type->ts & (T_LONG | T_ULONG | T_LFLOAT | T_PTR))
            size *= 8;
        else if (type->ts & T_FUNC)
            error(ERROR_ILLFUNC);
        else if (type->ts & T_TAG) {
            if (type->tag->ss & S_DEFINED)
                size *= type->tag->i;
            else
                error(ERROR_INCOMPLT);
        } else if (type->ts & T_ARRAY) {
            if (type->nr_elements == 0)
                error(ERROR_INCOMPLT);
            else
                size *= type->nr_elements;
        } 

        if (size > MAX_SIZE) error(ERROR_TYPESIZE);
        if (type->ts & T_PTR) break;
        type = type->next;
    }

    return size;
}

align_of(type)
    struct type * type;
{
    int align = 1;

    while (type) {
        if (type->ts & T_ARRAY)
            /* */ ;
        else if (type->ts & (T_CHAR | T_UCHAR)) 
            align = 1;
        else if (type->ts & (T_SHORT | T_USHORT)) 
            align = 2;
        else if (type->ts & (T_INT | T_UINT | T_FLOAT)) 
            align = 4;
        else if (type->ts & (T_LONG | T_ULONG | T_LFLOAT | T_PTR))
            align = 8;
        else if (type->ts & T_FUNC)
            error(ERROR_ILLFUNC);
        else if (type->ts & T_TAG) {
            if (type->tag->ss & S_DEFINED)
                align = type->tag->align;
            else
                error(ERROR_INCOMPLT);
        } 

        if (type->ts & T_PTR) break;
        type = type->next;
    } 

    return align;
}

/* perform some preliminary checks on a type:
     1. functions must return scalars,
     2. array elements can't be functions,
     3. only the first index of an array can be unbounded. */

validate_type(type)
    struct type * type;
{
    while (type) {
        if (type->ts & T_ARRAY) {
            if (type->next->ts & T_ARRAY) {
                if (type->next->nr_elements == 0)
                    error(ERROR_ILLARRAY);
            } else if (type->next->ts & T_FUNC) {
                error(ERROR_ILLFUNC);
            }
        } else if (type->ts & T_FUNC) {
            if (!(type->next->ts & T_IS_SCALAR))
                error(ERROR_RETURN);
        }
        type = type->next;
    }
}

/* adjust and validate the type of a formal argument.
     1. arrays become pointers, and
     2. functions become pointers to function.
     3. structs/unions are prohibited. */

struct type *
argument_type(type)
    struct type * type;
{
    if (type->ts & T_ARRAY) {
        type->ts &= ~T_ARRAY;
        type->ts = T_PTR;
    }

    if (type->ts & T_FUNC) type = splice_types(new_type(T_PTR), type);
    if (type->ts & T_TAG) error(ERROR_STRUCT);

    return type;
}

#ifndef NDEBUG

static struct
{
    int ts;
    char *text;
} ts[] = {
    { T_CHAR, "char" },
    { T_UCHAR, "unsigned char" },
    { T_SHORT, "short" },
    { T_USHORT, "unsigned short" },
    { T_INT, "int" },
    { T_UINT, "unsigned int" },
    { T_LONG, "long" },
    { T_ULONG, "unsigned long" },
    { T_FLOAT, "float" },
    { T_LFLOAT, "long float" },
    { T_PTR, "pointer to " },
    { T_FUNC, "function returning " },
    { T_ARRAY, "array[" }
};

#define NR_TFLAGS (sizeof(ts)/sizeof(*ts))

debug_type(type)
    struct type * type;
{
    struct symbol * tag;
    int             i;

    while (type) {
        for (i = 0; i < NR_TFLAGS; i++)
            if (ts[i].ts & type->ts)
                fprintf(stderr, "%s", ts[i].text);

        if (type->ts & T_ARRAY) {
            if (type->nr_elements) fprintf(stderr, "%d", type->nr_elements);
            fprintf(stderr,"] of ");
        }

        if (type->ts & T_TAG) {
            if (type->tag->ss & S_STRUCT) fprintf(stderr, "struct");
            if (type->tag->ss & S_UNION) fprintf(stderr, "union");
            if (type->tag->id)
                fprintf(stderr, " '%s'", type->tag->id->data);
            else
                fprintf(stderr, " @ %p", type->tag);
        }

        if (type->ts & T_FIELD) 
            fprintf(stderr, "[FIELD size %d shift %d] ", T_GET_SIZE(type->ts), T_GET_SHIFT(type->ts));

        type = type->next;
    }
}

#endif

