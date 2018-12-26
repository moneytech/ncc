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
#include <stdlib.h>
#include <string.h>
#include "nas.h"

/* guaranteed-not-to-fail allocation, sort of */

static char *
allocate(bytes)
    int bytes;
{
    char * p = malloc(bytes);

    if (p == NULL) error("out of memory");
    return p;
}

static struct name * buckets[NR_NAME_BUCKETS];

/* return the name entry for the string given, creating one if necessary.
   this is basically a copy of stringize() from symbol.c in the C compiler. */

struct name *
lookup_name(data, length)
    char * data;
    int    length;
{
    struct name ** namep;
    struct name  * name;
    unsigned       hash;
    int            i; 

    for (i = 0, hash = 0; i < length; i++) {
        hash <<= 4;
        hash ^= (data[i] & 0xFF);
    }

    i = hash % NR_NAME_BUCKETS;
    for (namep = &(buckets[i]); (name = *namep); namep = &((*namep)->link)) {
        if (name->length != length) continue;
        if (name->hash != hash) continue;
        if (memcmp(name->data, data, length)) continue;

        *namep = name->link;        /* move to top for LRU */
        name->link = buckets[i];
        buckets[i] = name;

        return name;
    }

    name = (struct name *) allocate(sizeof(struct name));
    name->link = buckets[i];
    buckets[i] = name;
    name->hash = hash;
    name->length = length;
    name->symbol = NULL;
    name->insn_entries = NULL;
    name->pseudo = NULL;
    name->token = 0;
    name->data = allocate(length + 1);
    memcpy(name->data, data, length);
    name->data[length] = 0;

    return name;
}

/* this function is called when a symbol is referenced, and
   does the necessary housekeeping. on return, name->symbol
   has valid data for the caller.

   NOTE: inside the assembler, obj_symbol.index refers to the
   symbol index that will be assigned to the symbol, NOT the
   index into the name table as it means on disk! */

reference(name)
    struct name * name;
{
    int position;

    if (pass == FIRST_PASS) {
        if (name->symbol == NULL) {
            name->symbol = (struct obj_symbol *) allocate(sizeof(struct obj_symbol));
            name->symbol->index = nr_symbols;
            name->symbol->flags = 0;
            name->symbol->value = 0;
        }
    } else if (pass == FINAL_PASS) {
        /* on first encounter, write the symbol and its name to the output file */

        if (name->symbol->index == nr_symbols) {
            position = OBJ_SYMBOLS_OFFSET(header);
            position += name->symbol->index * sizeof(struct obj_symbol);
            name->symbol->index = name_bytes;
            output(position, name->symbol, sizeof(struct obj_symbol));
            name->symbol->index = nr_symbols;

            position = OBJ_NAMES_OFFSET(header);
            position += name_bytes;
            output(position, name->data, name->length + 1);
            name_bytes += name->length + 1;
        }
    } else { 
        /* the first intermediate pass catch undefined symbols */

        if (!(name->symbol->flags & (OBJ_SYMBOL_GLOBAL | OBJ_SYMBOL_DEFINED)))
            error("undefined symbol");
    }

    /* regardless of which pass, bump the symbol counter after the
       first appearance of any symbol. */

    if (name->symbol->index == nr_symbols) nr_symbols++;
}

/* assign the given symbol the specified value, and mark it defined. 
   like reference(), there's a fair amount of bookkeeping done here. */

define(name, value)
    struct name * name;
    long          value;
{
    reference(name);

    if (pass == FIRST_PASS) {
        if (name->symbol->flags & OBJ_SYMBOL_DEFINED) error("multiple definition");
    } else if (pass == FINAL_PASS) {
        if (name->symbol->value != value) error("internal error: symbol changed");
    } else /* intermediate pass */ {
        if (name->symbol->value != value) 
            nr_symbol_changes++;
    }

    name->symbol->flags |= OBJ_SYMBOL_DEFINED;
    name->symbol->value = value;
}

/* at start up, names are loaded into the name table. most of the pre-load tables 
   are here, but mnemonics from the instruction table are also installed. [given
   the sheer number of mnemonics for AMD64, these should probably be precomputed.] */

static struct
{
    char * text;
    int    token;
} tokens[] = {
    { "byte", BYTE },       { "cs", REG_CS },       { "ss", REG_SS },       { "fs", REG_FS },
    { "word", WORD },       { "ds", REG_DS },       { "es", REG_ES },       { "gs", REG_GS },

    { "dword", DWORD },   
    { "qword", QWORD },
    { "rip", REG_RIP },

    { "cr0", REG_CR0 },     { "cr1", REG_CR1 },     { "cr2", REG_CR2 },     { "cr3", REG_CR3 },
    { "cr4", REG_CR4 },     { "cr5", REG_CR5 },     { "cr6", REG_CR6 },     { "cr7", REG_CR7 },
                            
    { "rax", REG_RAX },     { "rbx", REG_RBX },     { "rcx", REG_RCX },     { "rdx", REG_RDX },
    { "al", REG_AL },       { "ah", REG_AH },       { "ax", REG_AX },       { "eax", REG_EAX },
    { "bl", REG_BL },       { "bh", REG_BH },       { "bx", REG_BX },       { "ebx", REG_EBX },
    { "cl", REG_CL },       { "ch", REG_CH },       { "cx", REG_CX },       { "ecx", REG_ECX },
    { "dl", REG_DL },       { "dh", REG_DH },       { "dx", REG_DX },       { "edx", REG_EDX },
    { "bpl", REG_BPL },     { "bp", REG_BP },       { "ebp", REG_EBP },     { "rbp", REG_RBP },
    { "spl", REG_SPL },     { "sp", REG_SP },       { "esp", REG_ESP },     { "rsp", REG_RSP },
    { "sil", REG_SIL },     { "si", REG_SI },       { "esi", REG_ESI },     { "rsi", REG_RSI },
    { "dil", REG_DIL },     { "di", REG_DI },       { "edi", REG_EDI },     { "rdi", REG_RDI },
    { "r8b", REG_R8B },     { "r8w", REG_R8W },     { "r8d", REG_R8D },     { "r8", REG_R8 },
    { "r9b", REG_R9B },     { "r9w", REG_R9W },     { "r9d", REG_R9D },     { "r9", REG_R9 },
    { "r10b", REG_R10B },   { "r10w", REG_R10W },   { "r10d", REG_R10D },   { "r10", REG_R10 },
    { "r11b", REG_R11B },   { "r11w", REG_R11W },   { "r11d", REG_R11D },   { "r11", REG_R11 },
    { "r12b", REG_R12B },   { "r12w", REG_R12W },   { "r12d", REG_R12D },   { "r12", REG_R12 },
    { "r13b", REG_R13B },   { "r13w", REG_R13W },   { "r13d", REG_R13D },   { "r13", REG_R13 },
    { "r14b", REG_R14B },   { "r14w", REG_R14W },   { "r14d", REG_R14D },   { "r14", REG_R14 },
    { "r15b", REG_R15B },   { "r15w", REG_R15W },   { "r15d", REG_R15D },   { "r15", REG_R15 },

    { "xmm0", REG_XMM0 },   { "xmm1", REG_XMM1 },   { "xmm2", REG_XMM2 },   { "xmm3", REG_XMM3 },
    { "xmm4", REG_XMM4 },   { "xmm5", REG_XMM5 },   { "xmm6", REG_XMM6 },   { "xmm7", REG_XMM7 },
    { "xmm8", REG_XMM8 },   { "xmm9", REG_XMM9 },   { "xmm10", REG_XMM10 }, { "xmm11", REG_XMM11 },
    { "xmm12", REG_XMM12 }, { "xmm13", REG_XMM13 }, { "xmm14", REG_XMM14 }, { "xmm15", REG_XMM15 }
};

#define NR_TOKENS (sizeof(tokens)/sizeof(*tokens))

struct {
    char   * text;
    int  ( * handler )();
} pseudos[] = {
    { "byte", pseudo_byte },
    { "word", pseudo_word },
    { "dword", pseudo_dword },
    { "qword", pseudo_qword },
    { "ascii", pseudo_ascii },
    { "global", pseudo_global },
    { "align", pseudo_align },
    { "skip", pseudo_skip },
    { "fill", pseudo_fill },
    { "text", pseudo_text },
    { "data", pseudo_data },
    { "bss", pseudo_bss },
    { "org", pseudo_org },
    { "bits", pseudo_bits }
};

#define NR_PSEUDOS (sizeof(pseudos)/sizeof(*pseudos))

load_names()
{
    struct name * name;
    int           i;

    for (i = 0; i < NR_TOKENS; i++) {
        name = lookup_name(tokens[i].text, strlen(tokens[i].text));
        name->token = tokens[i].token;
    }

    for (i = 0; insns[i].mnemonic; i++) {
        name = lookup_name(insns[i].mnemonic, strlen(insns[i].mnemonic));
        if (!name->insn_entries) name->insn_entries = &insns[i];
        insns[i].name = name;
    }

    for (i = 0; i < NR_PSEUDOS; i++) {
        name = lookup_name(pseudos[i].text, strlen(pseudos[i].text));
        name->pseudo = pseudos[i].handler;
    }
}
