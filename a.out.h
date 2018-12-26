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

/* a bsd/64 executable is a fairly conventional a.out format.

   the text segment comes first, which begins with the exec header.
   the header is part of the text segment; thus the first user code 
   in text segment is at offset 0x20 == sizeof(struct exec). the
   text segment is padded to a page boundary, and the data section 
   follows. the data section is padded to an 8-byte boundary.

   the symbol table is last. each symbol consists of a NUL-terminated
   identifier, padded to an 8-byte boundary (with NULs), followed by
   an 8-byte address.

   if the text only makes RIP-relative references, it is position-
   independent (the compiler generates code this way already). if we
   wished to add shared library support, we'd simply need to fix up
   the initialized data area if it references any symbol addresses. 
   extending a.out to hold a fixup table and modifying the linker to 
   generate such a table would be straightforward. */

struct exec
{
    unsigned a_magic;
    unsigned a_text; 
    unsigned a_data;        
    unsigned a_bss;
    unsigned a_entry;
    unsigned a_reserved[2];
    unsigned a_syms;
};

#define A_MAGIC 0x87CD1EEB      /* $87CD SYNC: homage to OS-9/6809 */

