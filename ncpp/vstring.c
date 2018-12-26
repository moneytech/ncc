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

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include "ncpp.h"

/* the more buckets the better, to a point. for optimal performance,
   NR_STRING BUCKETS should really be a power of two. */

#define NR_STRING_BUCKETS 16

static struct string * buckets[NR_STRING_BUCKETS];

/* the initial capacity of a vstring. there is probably a 'best' value for 
   any given standard library, but 16 seems pretty safe for now. */

#define VSTRING_INITIAL_ALLOC 16

/* interpret the C escape sequence at 'cp' and determine its value. 'c' is
   set to that value. 'cp' is advanced to the next character position. if the
   sequence is invalid, return false, otherwise, return true. */

static char * digits = "0123456789ABCDEF";

#define isodigit(x)     (isdigit(x) && ((x) != '8') && ((x) != '9'))
#define digit_value(x)  (strchr(digits, toupper(x)) - digits)

static 
escape(cp, c)
    char ** cp;
    int   * c;
{
    if (isodigit(**cp)) {
        *c = digit_value(**cp);
        (*cp)++;
        if (isodigit(**cp)) {
            *c <<= 3;
            *c += digit_value(**cp);
            (*cp)++;
            if (isodigit(**cp)) {
                *c <<= 3;
                *c += digit_value(**cp);
                (*cp)++;
                if (*c > UCHAR_MAX) return 0;
            }
        }
    } else if (**cp == 'x') {
        *c = 0;
        (*cp)++;
        if (!isxdigit(**cp)) return 0;
        *c = digit_value(**cp);
        (*cp)++;

        if (isxdigit(**cp)) {
            *c <<= 4;
            *c += digit_value(**cp);
            (*cp)++;
            if (isxdigit(**cp)) return 0;
        }
    } else {
        switch (**cp) {
        case 'a':
            *c = '\a';
            break;

        case 'b':
            *c = '\b';
            break;

        case 'f':
            *c = '\f';
            break;

        case 'n': 
            *c = '\n'; 
            break;

        case 'r':
            *c = '\r';
            break;

        case 't':
            *c = '\t';
            break;

        case 'v':
            *c = '\v';
            break;

        case '\?':
        case '\"':
        case '\\':
        case '\'':
            *c = **cp;
            break;

        default:
            return 0;
        }
        (*cp)++;
    }

    return 1;
}

/* return a new string from the token text of a C string literal, interpreting
   escape sequences. if the literal contains illegal escape sequences, return NULL. */

struct vstring *
vstring_from_literal(literal)
    struct vstring * literal;
{
    struct vstring * raw;
    char           * cp;
    int              c;

    if (literal->length == 0) return NULL;
    cp = literal->data;
    if (*cp == 'L') cp++;
    cp++; /* opening quote */
    raw = vstring_new(NULL);

    while (*cp != '\"') {
        if (*cp == '\\') {
            cp++;
            if (!escape(&cp, &c)) {
                vstring_free(raw);
                return NULL;
            }
        } else
            c = *cp++;

        vstring_putc(raw, c);
    }

    return raw;
}

/* allocate a new struct vstring. note that this only allocates and initializes
   the structure itself- a zero-capacity string has no storage. if 's' is not NULL,
   then the vstring is initialized with the contents of the C-style string 's'. */

struct vstring *
vstring_new(s)
    char * s;
{
    struct vstring * vstring = (struct vstring *) safe_malloc(sizeof(struct vstring));

    vstring->data = NULL;
    vstring->length = 0;
    vstring->capacity = 0;

    if (s) vstring_puts(vstring, s);

    return vstring;
}

/* append the contents of the source string to the destination string.
   the source string is unmodified. */

vstring_concat(destination, source)
    struct vstring * destination;
    struct vstring * source;
{
    int i = 0;

    while (i < source->length) vstring_putc(destination, source->data[i++]);
}

/* create a new vstring initialized with the contents of another vstring. */

struct vstring *
vstring_copy(source)
    struct vstring * source;
{
    struct vstring * vstring;

    vstring = vstring_new(NULL);
    vstring_concat(vstring, source);

    return vstring;
}

/* free a vstring, and its associated storage if present. */

vstring_free(vstring)
    struct vstring * vstring;
{
    if (vstring->data) free(vstring->data);
    free(vstring);
}

/* return true if string contents are equal */

vstring_equal(vstring1, vstring2)
    struct vstring * vstring1;
    struct vstring * vstring2;
{
    if (vstring1->length != vstring2->length) return 0;
    return (memcmp(vstring1->data, vstring2->data, vstring1->length) == 0);
}

vstring_equal_s(vstring, s)
    struct vstring * vstring;
    char           * s;
{
    int length = strlen(s);

    if (vstring->length != length) return 0;
    return (memcmp(vstring->data, s, length) == 0);
}

/* add a char to the end of a vstring, increasing the capacity and
   (re)allocating storage if necessary */

vstring_putc(vstring, c)
    struct vstring * vstring;
{
    if (vstring->length == vstring->capacity) {
        if (vstring->capacity == 0) {
            vstring->data = safe_malloc(VSTRING_INITIAL_ALLOC);
            vstring->capacity = VSTRING_INITIAL_ALLOC - 1;
        } else {
            char *new_data = safe_malloc((vstring->capacity + 1) * 2);
            memcpy(new_data, vstring->data, vstring->capacity);
            free(vstring->data);
            vstring->data = new_data;
            vstring->capacity = ((vstring->capacity + 1) * 2) - 1;
        }
    }

    vstring->data[vstring->length++] = c;
    vstring->data[vstring->length] = 0;
}

/* add a null-terminated string to the end of a vstring. */

vstring_puts(vstring, s)
    struct vstring * vstring;
    char           * s;
{
    while (*s) vstring_putc(vstring, *s++);
}


/* erase the last character from the string (if any) */

vstring_rubout(vstring)
    struct vstring * vstring;
{
    if (vstring->length) {
        vstring->length--;
        vstring->data[vstring->length] = 0;
    }
}

/* return a hash value for the string */

unsigned
vstring_hash(vstring)
    struct vstring * vstring;
{
    unsigned hash = 0;
    int      i;

    for (i = 0; i < vstring->length; i++) {
        hash <<= 3;
        hash ^= vstring->data[i];
    }
    
    return hash;
}

