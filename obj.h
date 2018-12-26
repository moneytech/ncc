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

/* definitions for relocatable object modules (.o files) */

#define OBJ_MAGIC   0x252A2131

struct obj_header
{
    int      magic;
    unsigned text_bytes;
    unsigned data_bytes;
    unsigned nr_symbols;
    unsigned nr_relocs;
    unsigned name_bytes;
    unsigned reserved[2];
};

#define OBJ_SYMBOL_GLOBAL       0x80000000    
#define OBJ_SYMBOL_DEFINED      0x40000000

#define OBJ_SYMBOL_SEG_ABS      0x00000000
#define OBJ_SYMBOL_SEG_TEXT     0x00000010
#define OBJ_SYMBOL_SEG_DATA     0x00000020
#define OBJ_SYMBOL_SEG_BSS      0x00000040

#define OBJ_SYMBOL_GET_SEG(sym)         ((sym).flags & 0xF0)
#define OBJ_SYMBOL_SET_SEG(sym,seg)     ((sym).flags = ((sym).flags & ~(0xF0)) | ((seg) & 0xF0))
#define OBJ_SYMBOL_GET_ALIGN(sym)       ((sym).flags & 0x02)
#define OBJ_SYMBOL_SET_ALIGN(sym,log2)  ((sym).flags = ((sym).flags & ~(0x02)) | ((log2) & 0x02))
#define OBJ_SYMBOL_VALID_ALIGN(log2)    ((log2) <= 3)

struct obj_symbol
{
    int      flags;
    unsigned index;             /* into name section */
    long     value;             /* or size, if BSS */
};

#define OBJ_RELOC_REL           0x80000000      /* relocation should be relative */
#define OBJ_RELOC_TEXT          0x40000000      /* target is a text offset */
#define OBJ_RELOC_DATA          0x20000000      /* target is a data offset */
#define OBJ_RELOC_SIZE_8        0x00000000      /* fixup size is 8 bytes.. */
#define OBJ_RELOC_SIZE_16       0x00000001      /* .. 16 .. */
#define OBJ_RELOC_SIZE_32       0x00000002      /* .. 32 .. */
#define OBJ_RELOC_SIZE_64       0x00000003      /* .. 64 */

#define OBJ_RELOC_GET_SIZE(rel)         ((rel).flags & 0x03)
#define OBJ_RELOC_SET_SIZE(rel,sz)      ((rel).flags = ((rel).flags & ~(0x03)) | ((sz) & 0x03))

struct obj_reloc
{
    int      flags;
    unsigned index;             /* into symbol section of referenced symbol */
    unsigned target;            /* offset to fixup in target segment */
    unsigned reserved;
};

/* macros to determine the beginning positions of the various
   sections in the object file, based on header data. */

#define OBJ_ALIGN               8
#define OBJ_ROUNDUP(offset)     (((offset) % OBJ_ALIGN) ? ((offset) + (OBJ_ALIGN - ((offset) % OBJ_ALIGN)))  : (offset))

#define OBJ_TEXT_OFFSET(hdr)        (sizeof(hdr))
#define OBJ_DATA_OFFSET(hdr)        (OBJ_TEXT_OFFSET(hdr) + OBJ_ROUNDUP((hdr).text_bytes))
#define OBJ_SYMBOLS_OFFSET(hdr)     (OBJ_DATA_OFFSET(hdr) + OBJ_ROUNDUP((hdr).data_bytes))
#define OBJ_SYMBOL_OFFSET(hdr,n)    (OBJ_SYMBOLS_OFFSET(hdr) + ((n) * sizeof(struct obj_symbol)))
#define OBJ_RELOCS_OFFSET(hdr)      (OBJ_SYMBOLS_OFFSET(hdr) + ((hdr).nr_symbols * sizeof(struct obj_symbol)))
#define OBJ_RELOC_OFFSET(hdr,n)     (OBJ_RELOCS_OFFSET(hdr) + ((n) * sizeof(struct obj_reloc)))
#define OBJ_NAMES_OFFSET(hdr)       (OBJ_RELOCS_OFFSET(hdr) + ((hdr).nr_relocs * sizeof(struct obj_reloc)))
#define OBJ_NAME_OFFSET(hdr,n)      (OBJ_NAMES_OFFSET(hdr) + (n))

