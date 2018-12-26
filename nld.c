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
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>

#include "obj.h"
#include "a.out.h"

#define ALIGN(a,b)      (((a) % (b)) ? ((a) + ((b) - ((a) % (b)))) : (a))

/* size of buffer for copying from object files to output */

#define BUFFER_SIZE 4096

/* global symbol names are referenced from a master table
   hashed into NR_BUCKETS */

#define NR_BUCKETS 64

struct global
{
    char              * name;
    unsigned            hash;
    int                 length;
    struct obj_symbol * symbol;
    struct global     * link;
};

/* all object sources are kept in a linked list, 
   ordered as they appear on the command line. */

#define OBJECT_REFERENCED   0x00000001      /* this object must be linked in the output */

struct object
{
    char              * path;
    FILE              * fp;
    int                 flags;
    struct obj_header   header;
    struct obj_symbol * symbols;
    struct obj_reloc  * relocs;
    char              * names;
    struct object     * next;
};

struct exec              exec;
unsigned long            base_address;
unsigned long            current_address;
FILE                   * out_fp;
char                   * out_path;
char                   * entry;
struct object          * first_object;
struct object          * last_object;
struct global          * buckets[NR_BUCKETS];
char                     buffer[BUFFER_SIZE];
int                      type = -1;
int                      raw_flag;

/* output an error message, clean up, and abort */

#ifdef __STDC__
void
error(char * fmt, ...)
#else
error(fmt)
    char *fmt;
#endif
{
    va_list args;

    fprintf(stderr, "ld: ");

    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fputc('\n', stderr);

    if (out_fp) {
        fclose(out_fp);
        unlink(out_path);
    }

    exit(1);
}

/* allocate or bust */

char *
allocate(bytes)
    int bytes;
{
    char * p = malloc(bytes);

    if (p == NULL) error("out of memory");
    return p;
}

/* return the hash of a NUL-terminated string */

unsigned
compute_hash(name)
    char *name;
{
    unsigned hash;
    int      i;

    for (i = 0, hash = 0; name[i]; i++) {
        hash <<= 4;
        hash ^= (name[i] & 0xFF);
    }

    return hash;
}

/* look up a global symbol. returns NULL if not found. */

struct global *
find_global(name)
    char * name;
{
    struct global * global;
    unsigned        hash;
    int             i; 
    int             length;

    length = strlen(name);
    hash = compute_hash(name);
    i = hash % NR_BUCKETS;

    for (global = buckets[i]; global; global = global->link) {
        if (global->length != length) continue;
        if (global->hash != hash) continue;
        if (memcmp(global->name, name, length)) continue;

        break;
    }

    return global;
}

/* export a global symbol from an object. */

struct global *
export(name, symbol)
    char              * name;
    struct obj_symbol * symbol;
{
    struct global * global;
    unsigned        hash;
    int             i;

    if (find_global(name)) error("multiple definitions for '%s'", name);
    hash = compute_hash(name);
    i = hash % NR_BUCKETS;
    global = (struct global *) allocate(sizeof(struct global));
    global->link = buckets[i];
    buckets[i] = global;
    global->name = name;
    global->hash = hash;
    global->length = strlen(name);
    global->symbol = symbol;

    return global;
}

/* write global symbols out (debugging data) */

debug_info()
{
    struct global * global;
    int             i;
    long            value;

    for (i = 0; i < NR_BUCKETS; i++) {
        for (global = buckets[i]; global; global = global->link) {
            value = global->symbol->value;
            output(current_address - base_address, global->name, global->length + 1);
            current_address += global->length + 1;
            pad(0, 8);
            exec.a_syms += global->length + 1;
            exec.a_syms = ALIGN(exec.a_syms, 8);
            output(current_address - base_address, &value, 8);
            current_address += 8;
            exec.a_syms += 8;
        }
    }
}

/* error-free I/O with output file */

output(position, buffer, length)
    char * buffer;
{
    if (fseek(out_fp, (long) position, SEEK_SET))
        error("can't seek output file '%s'", out_fp);

    if (fwrite(buffer, sizeof(char), sizeof(char) * length, out_fp) != (sizeof(char) * length))
        error("error writing output '%s'", out_path);
}

input(position, buffer, length)
    char * buffer;
{
    if (fseek(out_fp, (long) position, SEEK_SET))
        error("can't seek output file '%s'", out_fp);

    if (fread(buffer, sizeof(char), sizeof(char) * length, out_fp) != (sizeof(char) * length))
        error("error reading output '%s'", out_path, length);
}

/* copy segment from object to output file */

#define MIN(a,b)        (((a) < (b)) ? (a) : (b))

copy_segment(object, from, to, length)
    struct object * object;
{
    int size;

    while (length) {
        size = MIN(length, BUFFER_SIZE);
        read_object(object, from, buffer, size);
        output(to, buffer, size);
        length -= size;
        from += size;
        to += size;
    }
}

/* error-free input from the object file */

read_object(object, position, buffer, length)
    struct object * object;
    char          * buffer;
{
    if (fseek(object->fp, (long) position, SEEK_SET))
        error("can't seek input file '%s'", object->path);

    if (fread(buffer, sizeof(char), sizeof(char) * length, object->fp) != (sizeof(char) * length))
        error("error reading object '%s'", object->path);
}

/* at some point it may make more sense to simply keep 
   all the files open, but for now we open and close them
   to keep the number of file handles to a minimum. */

open_object(object)
    struct object * object;
{
    object->fp = fopen(object->path, "r");
    if (object->fp == NULL) error("'%s': can't open", object->path);
}

close_object(object)
    struct object * object;
{
    fclose(object->fp);
    object->fp = NULL;
}

/* create a new object, and read in its metadata */

struct object *
new_object(path)
    char * path;
{
    struct object * object;
    FILE          * fp;
    int             size;

    object = (struct object *) allocate(sizeof(struct object));
    object->path = path;
    object->flags = 0;
    object->symbols = NULL;
    object->relocs = NULL;
    object->names = NULL;
    object->next = NULL;
    open_object(object);
    read_object(object, 0, &object->header, sizeof(object->header));
    if (object->header.magic != OBJ_MAGIC) error("'%s' is not an object file", path);

    if (object->header.nr_symbols) {
        size = sizeof(struct obj_symbol) * object->header.nr_symbols;
        object->symbols = (struct obj_symbol *) allocate(size);
        read_object(object, OBJ_SYMBOLS_OFFSET(object->header), object->symbols, size);
    }

    if (object->header.nr_relocs) {
        size = sizeof(struct obj_reloc) * object->header.nr_relocs;
        object->relocs = (struct obj_reloc *) allocate(size);
        read_object(object, OBJ_RELOCS_OFFSET(object->header), object->relocs, size);
    }

    if (object->header.name_bytes) {
        object->names = allocate(object->header.name_bytes);
        read_object(object, OBJ_NAMES_OFFSET(object->header), object->names, object->header.name_bytes);
    }

    if (last_object == NULL) {
        first_object = object;
        last_object = object;
    } else {
        last_object->next = object;
        last_object = object;
    }

    close_object(object);
}

/* find the object that exports 'name' and reference it.
   returns non-zero on success, or zero if not found. */

import(name)
    char          * name;
{
    struct object     * object;
    struct obj_symbol * symbol;
    int                 i;
    
    for (object = first_object; object; object = object->next) {
        if (object->flags & OBJECT_REFERENCED) continue;
        symbol = object->symbols;

        for (i = 0; i < object->header.nr_symbols; ++i, ++symbol) {
            if (!(symbol->flags & OBJ_SYMBOL_DEFINED) || !(symbol->flags & OBJ_SYMBOL_GLOBAL)) continue;
            if (!strcmp(object->names + symbol->index, name)) {
                reference(object);
                return 1;
            }
        }
    }

    return 0;
}

/* the object file has a symbol we need in the output, so mark
   it referenced, and export all its global symbols to our table.
   then, attempt to import the object's undefined globals. */

reference(object)
    struct object * object;
{
    struct obj_symbol * symbol;
    char              * name;
    int                 i;

    object->flags |= OBJECT_REFERENCED;

    for (i = 0, symbol = object->symbols; i < object->header.nr_symbols; ++i, ++symbol) {
        name = object->names + symbol->index;

        if ((symbol->flags & OBJ_SYMBOL_DEFINED) && (symbol->flags & OBJ_SYMBOL_GLOBAL))
            export(name, symbol);
    }

    for (i = 0, symbol = object->symbols; i < object->header.nr_symbols; ++i, ++symbol) {
        name = object->names + symbol->index;

        if (!(symbol->flags & OBJ_SYMBOL_DEFINED) && !find_global(name) && !import(name)) 
                error("unresolved reference to '%s' in '%s'", name, object->path);
    }
}

/* bump the symbols/relocations in an object by 'offset' bytes.
   'segment' is one of OBJ_SYMBOL_SEG_TEXT or OBJ_SYMBOL_SEG_DATA.
   'flags' is one of OBJ_RELOC_TEXT or OBJ_RELOC_DATA. */

offset_symbols(object, segment, offset)
    struct object * object;
    long            offset;
{
    struct obj_symbol * symbol;
    int                 i;

    for (i = 0, symbol = object->symbols; i < object->header.nr_symbols; ++i, ++symbol) {
        if (!(symbol->flags & OBJ_SYMBOL_DEFINED)) continue;
        if (OBJ_SYMBOL_GET_SEG(*symbol) != segment) continue;
        symbol->value += offset;
    }
}

offset_relocs(object, flags, offset)
    struct object * object;
{
    struct obj_reloc * reloc;
    int                i;

    for (i = 0, reloc = object->relocs; i < object->header.nr_relocs; ++i, ++reloc) {
        if (!(reloc->flags & flags)) continue;
        reloc->target += offset;
    }
}

/* output 'b' until 'current_address' is aligned at 'align' boundary */

pad(b, align)
{
    while (current_address % align) {
        output(current_address - base_address, &b, 1);
        current_address++;
    }
}

text_phase(object)
    struct object * object;
{
    open_object(object);
    copy_segment(object, OBJ_TEXT_OFFSET(object->header), current_address - base_address, object->header.text_bytes);
    close_object(object);
    offset_symbols(object, OBJ_SYMBOL_SEG_TEXT, current_address);
    offset_relocs(object, OBJ_RELOC_TEXT, current_address - base_address);
    current_address += object->header.text_bytes;
    pad(0x90, 8); /* NOP */
}


data_phase(object)
    struct object * object;
{
    open_object(object);
    copy_segment(object, OBJ_DATA_OFFSET(object->header), current_address - base_address, object->header.data_bytes);
    close_object(object);
    offset_symbols(object, OBJ_SYMBOL_SEG_DATA, current_address);
    offset_relocs(object, OBJ_RELOC_DATA, current_address - base_address);
    current_address += object->header.data_bytes;
    pad(0, 8);
}


bss_phase(object)
    struct object * object;
{
    struct obj_symbol * symbol;
    int                 i;
    long                size;

    for (i = 0, symbol = object->symbols; i < object->header.nr_symbols; ++i, ++symbol) {
        if (!(symbol->flags & OBJ_SYMBOL_DEFINED)) continue;
        if (OBJ_SYMBOL_GET_SEG(*symbol) != OBJ_SYMBOL_SEG_BSS) continue;
        exec.a_bss = ALIGN(exec.a_bss, 1 << OBJ_SYMBOL_GET_ALIGN(*symbol));
        size = symbol->value;
        symbol->value = current_address + exec.a_bss;
        exec.a_bss += size;
    }
}


reloc_phase(object)
    struct object * object;
{
    struct obj_reloc  * reloc;
    struct obj_symbol * symbol;
    struct global     * global;
    int                 i;
    long                value;
    int                 size;

    for (i = 0, reloc = object->relocs; i < object->header.nr_relocs; ++i, ++reloc) {
        symbol = & object->symbols[reloc->index];

        if (!(symbol->flags & OBJ_SYMBOL_DEFINED)) {
            global = find_global(object->names + symbol->index);
            symbol = global->symbol;
        }

        switch (OBJ_RELOC_GET_SIZE(*reloc))
        {
        case OBJ_RELOC_SIZE_8:  size = 1; break;
        case OBJ_RELOC_SIZE_16: size = 2; break;
        case OBJ_RELOC_SIZE_32: size = 4; break;
        case OBJ_RELOC_SIZE_64: size = 8; break;
        }

        input(reloc->target, &value, size);

        if (reloc->flags & OBJ_RELOC_REL) 
            value += symbol->value - (reloc->target + base_address);
        else 
            value += symbol->value;

        output(reloc->target, &value, size);
    }
}

/* call 'f' on all referenced objects */

walk_objects(f)
    int ( * f ) ();
{
    struct object * object;

    for (object = first_object; object; object = object->next) 
        if ((object->flags & OBJECT_REFERENCED)) f(object);
}

main(argc, argv)
    char *argv[];
{
    struct global * global;
    int             opt;

    while ((opt = getopt(argc, argv, "b:e:o:r")) != -1) {
        switch (opt)
        {
        case 'b':
            base_address = strtoul(optarg, NULL, 0); 
            break;

        case 'e':
            entry = optarg;
            break;

        case 'o':
            out_path = optarg;
            break;

        case 'r':
            raw_flag++;
            break;

        default:
            exit(1);
        }
    }

    if (!out_path) error("no output file (-o) specified");
    out_fp = fopen(out_path, "w+");
    if (out_fp == NULL) error("can't open output '%s'", out_path);

    argv = &argv[optind];
    if (!*argv) error("no object file(s) specified");
    while (*argv) new_object(*argv++);

    reference(first_object);

    exec.a_magic = A_MAGIC;

    current_address = base_address;
    if (!raw_flag) current_address += sizeof(struct exec);

    walk_objects(text_phase); 
    if (!raw_flag) pad(0x90, 4096);   
    exec.a_text = current_address - base_address;

    walk_objects(data_phase);                                        
    exec.a_data = current_address - base_address - exec.a_text;

    walk_objects(bss_phase); 
    walk_objects(reloc_phase);

    /* write a.out header */

    if (!raw_flag) {
        debug_info();
        if (!entry) error("no entry point (-e) specified");
        global = find_global(entry);
        if (!global) error("can't find global entry point '%s'", entry);
        exec.a_entry = global->symbol->value;
        output(0, &exec, sizeof(exec));
    }

    fclose(out_fp);
    chmod(out_path, 0755);
    exit(0);
}
