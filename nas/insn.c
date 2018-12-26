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

#include "nas.h"

struct insn insns[] = 
{
    { "mov", 2, { O_REG_CRL | O_I_MIDREG, O_REG_32 | O_I_ENDREG }, 3, { 0x0F, 0x22, 0xC0 }, I_NO_BITS_16 | I_NO_BITS_64 },
    { "mov", 2, { O_REG_CRL | O_I_MIDREG, O_REG_64 | O_I_ENDREG }, 3, { 0x0F, 0x22, 0xC0 }, I_NO_BITS_16 | I_NO_BITS_32 },
    { "mov", 2, { O_REG_32 | O_I_ENDREG, O_REG_CRL | O_I_MIDREG }, 3, { 0x0F, 0x20, 0xC0 }, I_NO_BITS_16 | I_NO_BITS_64 },
    { "mov", 2, { O_REG_64 | O_I_ENDREG, O_REG_CRL | O_I_MIDREG }, 3, { 0x0F, 0x20, 0xC0 }, I_NO_BITS_16 | I_NO_BITS_32 },

    { "mov", 2, { O_REG_8 | O_I_ENDREG, O_IMM_8 }, 1, { 0xB0 }, I_DATA_8 },
    { "mov", 2, { O_REG_16 | O_I_ENDREG, O_IMM_16 }, 1, { 0xB8 }, I_DATA_16 },
    { "mov", 2, { O_REG_32 | O_I_ENDREG, O_IMM_32 }, 1, { 0xB8 }, I_DATA_32 },

    { "mov", 2, { O_MEM_8 | O_I_MODRM, O_IMM_8 }, 2, { 0xC6 }, I_DATA_8 },
    { "mov", 2, { O_MEM_16 | O_I_MODRM, O_IMM_16 }, 2, { 0xC7 }, I_DATA_16 },
    { "mov", 2, { O_MEM_32 | O_I_MODRM, O_IMM_32 }, 2, { 0xC7 }, I_DATA_32 },
    { "mov", 2, { O_MRM_64 | O_I_MODRM, O_IMM_S32 }, 2, { 0xC7 }, I_DATA_64 },

    { "mov", 2, { O_REG_64 | O_I_ENDREG, O_IMM_64 }, 1, { 0xB8 }, I_DATA_64 },

    { "mov", 2, { O_ACC_8, O_MOFS_8 }, 1, { 0xA0 }, I_DATA_8 | I_NO_BITS_64 },
    { "mov", 2, { O_ACC_16, O_MOFS_16 }, 1, { 0xA1 }, I_DATA_16 | I_NO_BITS_64 },
    { "mov", 2, { O_ACC_32, O_MOFS_32 }, 1, { 0xA1 }, I_DATA_32 | I_NO_BITS_64 },
    { "mov", 2, { O_MOFS_8, O_ACC_8 }, 1, { 0xA2 }, I_DATA_8 | I_NO_BITS_64 },
    { "mov", 2, { O_MOFS_16, O_ACC_16 }, 1, { 0xA3 }, I_DATA_16 | I_NO_BITS_64 },
    { "mov", 2, { O_MOFS_32, O_ACC_32 }, 1, { 0xA3 }, I_DATA_32 | I_NO_BITS_64 },

    { "mov", 2, { O_MRM_8 | O_I_MODRM, O_REG_8 | O_I_MIDREG }, 2, { 0x88 }, I_DATA_8 },
    { "mov", 2, { O_MRM_16 | O_I_MODRM, O_REG_16 | O_I_MIDREG }, 2, { 0x89 }, I_DATA_16 },
    { "mov", 2, { O_MRM_32 | O_I_MODRM, O_REG_32 | O_I_MIDREG }, 2, { 0x89 }, I_DATA_32 },
    { "mov", 2, { O_MRM_64 | O_I_MODRM, O_REG_64 | O_I_MIDREG }, 2, { 0x89 }, I_DATA_64 },

    { "mov", 2, { O_REG_8 | O_I_MIDREG, O_MRM_8 | O_I_MODRM }, 2, { 0x8A }, I_DATA_8 },
    { "mov", 2, { O_REG_16 | O_I_MIDREG, O_MRM_16 | O_I_MODRM }, 2, { 0x8B }, I_DATA_16 },
    { "mov", 2, { O_REG_32 | O_I_MIDREG, O_MRM_32 | O_I_MODRM }, 2, { 0x8B }, I_DATA_32 },
    { "mov", 2, { O_REG_64 | O_I_MIDREG, O_MRM_64 | O_I_MODRM }, 2, { 0x8B }, I_DATA_64 },

    { "mov", 2, { O_ACC_8, O_MOFS_8 }, 1, { 0xA0 }, I_DATA_8 },
    { "mov", 2, { O_ACC_16, O_MOFS_16 }, 1, { 0xA1 }, I_DATA_16 },
    { "mov", 2, { O_ACC_32, O_MOFS_32 }, 1, { 0xA1 }, I_DATA_32 },
    { "mov", 2, { O_ACC_64, O_MOFS_64 }, 1, { 0xA1 }, I_DATA_64 },
    { "mov", 2, { O_MOFS_8, O_ACC_8 }, 1, { 0xA2 }, I_DATA_8 },
    { "mov", 2, { O_MOFS_16, O_ACC_16 }, 1, { 0xA3 }, I_DATA_16 },
    { "mov", 2, { O_MOFS_32, O_ACC_32 }, 1, { 0xA3 }, I_DATA_32 },
    { "mov", 2, { O_MOFS_64, O_ACC_64 }, 1, { 0xA3 }, I_DATA_64 },

    { "mov", 2, { O_SREG2 | O_SREG3 | O_I_MIDREG, O_MEM_16 | O_I_MODRM }, 2, { 0x8E }, 0 },
    { "mov", 2, { O_SREG2 | O_SREG3 | O_I_MIDREG, O_REG_16 | O_REG_32 | O_I_MODRM }, 2, { 0x8E }, 0 },
    { "mov", 2, { O_SREG2 | O_SREG3 | O_I_MIDREG, O_REG_16 | O_REG_32 | O_I_MODRM }, 2, { 0x8E }, 0 },
    { "mov", 2, { O_SREG2 | O_SREG3 | O_I_MIDREG, O_REG_64 | O_I_MODRM }, 2, { 0x8E }, I_NO_BITS_16 | I_NO_BITS_32 },

    { "mov", 2, { O_MEM_16 | O_I_MODRM, O_SREG2 | O_SREG3 | O_I_MIDREG }, 2, { 0x8C }, 0 },
    { "mov", 2, { O_REG_16 | O_I_MODRM, O_SREG2 | O_SREG3 | O_I_MIDREG }, 2, { 0x8C }, I_DATA_16 },
    { "mov", 2, { O_REG_32 | O_I_MODRM, O_SREG2 | O_SREG3 | O_I_MIDREG }, 2, { 0x8C }, I_DATA_32 },
    { "mov", 2, { O_REG_64 | O_I_MODRM, O_SREG2 | O_SREG3 | O_I_MIDREG }, 2, { 0x8C }, I_DATA_64 | I_NO_DATA_REX },

    { "push", 1, { O_REG_16 | O_I_ENDREG }, 1, { 0x50 }, I_DATA_16 },
    { "push", 1, { O_REG_32 | O_I_ENDREG }, 1, { 0x50 }, I_DATA_32 | I_NO_BITS_64 },
    { "push", 1, { O_REG_64 | O_I_ENDREG }, 1, { 0x50 }, I_DATA_64 | I_NO_DATA_REX },

    { "push", 1, { O_SREG2 | O_I_MIDREG }, 1, { 0x06 }, I_NO_BITS_64 },
    { "push", 1, { O_SREG3 | O_I_MIDREG }, 2, { 0x0F, 0x80 }, 0 },

    { "push", 1, { O_MRM_16 | O_I_MODRM }, 2, { 0xFF, 0x30 }, I_DATA_16 },
    { "push", 1, { O_MRM_32 | O_I_MODRM }, 2, { 0xFF, 0x30 }, I_DATA_32 | I_NO_BITS_64 },
    { "push", 1, { O_MRM_64 | O_I_MODRM }, 2, { 0xFF, 0x30 }, I_DATA_64 | I_NO_DATA_REX },

    { "push", 1, { O_IMM_S8 }, 1, { 0x6A }, 0 },
    { "push", 1, { O_IMM_16 }, 1, { 0x68 }, I_DATA_16 | I_NO_BITS_32 | I_NO_BITS_64},
    { "push", 1, { O_IMM_32 }, 1, { 0x68 }, I_DATA_32 | I_NO_BITS_64 },
    { "push", 1, { O_IMM_S32 }, 1, { 0x68 }, I_DATA_64 | I_NO_DATA_REX },

    { "pop", 1, { O_REG_16 | O_I_ENDREG }, 1, { 0x58 }, I_DATA_16 },
    { "pop", 1, { O_REG_32 | O_I_ENDREG }, 1, { 0x58 }, I_DATA_32 | I_NO_BITS_64 },
    { "pop", 1, { O_REG_64 | O_I_ENDREG }, 1, { 0x58 }, I_DATA_64 | I_NO_DATA_REX },

    { "pop", 1, { O_MRM_16 | O_I_MODRM }, 2, { 0x8F, 0x00 }, I_DATA_16 },
    { "pop", 1, { O_MRM_32 | O_I_MODRM }, 2, { 0x8F, 0x00 }, I_DATA_32 | I_NO_BITS_64 },
    { "pop", 1, { O_MRM_64 | O_I_MODRM }, 2, { 0x8F, 0x00 }, I_DATA_64 | I_NO_DATA_REX },

    { "pop", 1, { O_SREG2 | O_I_MIDREG }, 1, { 0x07 }, I_NO_BITS_64 },
    { "pop", 1, { O_SREG3 | O_I_MIDREG }, 2, { 0x0F, 0x81 }, 0 },

    { "add", 2, { O_ACC_8, O_IMM_8 }, 1, { 0x04 }, I_DATA_8 }, 

    { "add", 2, { O_MRM_8 | O_I_MODRM, O_IMM_8 }, 2, { 0x80, 0x00 }, I_DATA_8 },
    { "add", 2, { O_MRM_16 | O_I_MODRM, O_IMM_S8 }, 2, { 0x83, 0x00 }, I_DATA_16 },
    { "add", 2, { O_MRM_32 | O_I_MODRM, O_IMM_S8 }, 2, { 0x83, 0x00 }, I_DATA_32 },
    { "add", 2, { O_MRM_64 | O_I_MODRM, O_IMM_S8 }, 2, { 0x83, 0x00 }, I_DATA_64 },

    { "add", 2, { O_ACC_16, O_IMM_16 }, 1, { 0x05 }, I_DATA_16 },
    { "add", 2, { O_ACC_32, O_IMM_32 }, 1, { 0x05 }, I_DATA_32 },
    { "add", 2, { O_ACC_64, O_IMM_S32 }, 1, { 0x05 }, I_DATA_64 },

    { "add", 2, { O_MRM_16 | O_I_MODRM, O_IMM_16 }, 2, { 0x81, 0x00 }, I_DATA_16 },
    { "add", 2, { O_MRM_32 | O_I_MODRM, O_IMM_32 }, 2, { 0x81, 0x00 }, I_DATA_32 },
    { "add", 2, { O_MRM_64 | O_I_MODRM, O_IMM_S32 }, 2, { 0x81, 0x00 }, I_DATA_64 },

    { "add", 2, { O_MRM_8 | O_I_MODRM, O_REG_8 | O_I_MIDREG }, 2, { 0x00, 0x00 }, I_DATA_8 },
    { "add", 2, { O_MRM_16 | O_I_MODRM, O_REG_16 | O_I_MIDREG }, 2, { 0x01, 0x00 }, I_DATA_16 },
    { "add", 2, { O_MRM_32 | O_I_MODRM, O_REG_32 | O_I_MIDREG }, 2, { 0x01, 0x00 }, I_DATA_32 },
    { "add", 2, { O_MRM_64 | O_I_MODRM, O_REG_64 | O_I_MIDREG }, 2, { 0x01, 0x00 }, I_DATA_64 },

    { "add", 2, { O_REG_8 | O_I_MIDREG, O_MRM_8 | O_I_MODRM }, 2, { 0x02, 0x00 }, I_DATA_8 },
    { "add", 2, { O_REG_16 | O_I_MIDREG, O_MRM_16 | O_I_MODRM }, 2, { 0x03, 0x00 }, I_DATA_16 },
    { "add", 2, { O_REG_32 | O_I_MIDREG, O_MRM_32 | O_I_MODRM }, 2, { 0x03, 0x00 }, I_DATA_32 },
    { "add", 2, { O_REG_64 | O_I_MIDREG, O_MRM_64 | O_I_MODRM }, 2, { 0x03, 0x00 }, I_DATA_64 },

    { "adc", 2, { O_ACC_8, O_IMM_8 }, 1, { 0x14 }, I_DATA_8 }, 

    { "adc", 2, { O_MRM_8 | O_I_MODRM, O_IMM_8 }, 2, { 0x80, 0x10 }, I_DATA_8 },
    { "adc", 2, { O_MRM_16 | O_I_MODRM, O_IMM_S8 }, 2, { 0x83, 0x10 }, I_DATA_16 },
    { "adc", 2, { O_MRM_32 | O_I_MODRM, O_IMM_S8 }, 2, { 0x83, 0x10 }, I_DATA_32 },
    { "adc", 2, { O_MRM_64 | O_I_MODRM, O_IMM_S8 }, 2, { 0x83, 0x10 }, I_DATA_64 },

    { "adc", 2, { O_ACC_16, O_IMM_16 }, 1, { 0x15 }, I_DATA_16 },
    { "adc", 2, { O_ACC_32, O_IMM_32 }, 1, { 0x15 }, I_DATA_32 },
    { "adc", 2, { O_ACC_64, O_IMM_S32 }, 1, { 0x15 }, I_DATA_64 },

    { "adc", 2, { O_MRM_16 | O_I_MODRM, O_IMM_16 }, 2, { 0x81, 0x10 }, I_DATA_16 },
    { "adc", 2, { O_MRM_32 | O_I_MODRM, O_IMM_32 }, 2, { 0x81, 0x10 }, I_DATA_32 },
    { "adc", 2, { O_MRM_64 | O_I_MODRM, O_IMM_S32 }, 2, { 0x81, 0x10 }, I_DATA_64 },

    { "adc", 2, { O_MRM_8 | O_I_MODRM, O_REG_8 | O_I_MIDREG }, 2, { 0x10, 0x00 }, I_DATA_8 },
    { "adc", 2, { O_MRM_16 | O_I_MODRM, O_REG_16 | O_I_MIDREG }, 2, { 0x11, 0x00 }, I_DATA_16 },
    { "adc", 2, { O_MRM_32 | O_I_MODRM, O_REG_32 | O_I_MIDREG }, 2, { 0x11, 0x00 }, I_DATA_32 },
    { "adc", 2, { O_MRM_64 | O_I_MODRM, O_REG_64 | O_I_MIDREG }, 2, { 0x11, 0x00 }, I_DATA_64 },

    { "adc", 2, { O_REG_8 | O_I_MIDREG, O_MRM_8 | O_I_MODRM }, 2, { 0x12, 0x00 }, I_DATA_8 },
    { "adc", 2, { O_REG_16 | O_I_MIDREG, O_MRM_16 | O_I_MODRM }, 2, { 0x13, 0x00 }, I_DATA_16 },
    { "adc", 2, { O_REG_32 | O_I_MIDREG, O_MRM_32 | O_I_MODRM }, 2, { 0x13, 0x00 }, I_DATA_32 },
    { "adc", 2, { O_REG_64 | O_I_MIDREG, O_MRM_64 | O_I_MODRM }, 2, { 0x13, 0x00 }, I_DATA_64 },

    { "sub", 2, { O_ACC_8, O_IMM_8 }, 1, { 0x2C }, I_DATA_8 }, 

    { "sub", 2, { O_MRM_8 | O_I_MODRM, O_IMM_8 }, 2, { 0x80, 0x28 }, I_DATA_8 },
    { "sub", 2, { O_MRM_16 | O_I_MODRM, O_IMM_S8 }, 2, { 0x83, 0x28 }, I_DATA_16 },
    { "sub", 2, { O_MRM_32 | O_I_MODRM, O_IMM_S8 }, 2, { 0x83, 0x28 }, I_DATA_32 },
    { "sub", 2, { O_MRM_64 | O_I_MODRM, O_IMM_S8 }, 2, { 0x83, 0x28 }, I_DATA_64 },

    { "sub", 2, { O_ACC_16, O_IMM_16 }, 1, { 0x2D }, I_DATA_16 },
    { "sub", 2, { O_ACC_32, O_IMM_32 }, 1, { 0x2D }, I_DATA_32 },
    { "sub", 2, { O_ACC_64, O_IMM_S32 }, 1, { 0x2D }, I_DATA_64 },

    { "sub", 2, { O_MRM_16 | O_I_MODRM, O_IMM_16 }, 2, { 0x81, 0x28 }, I_DATA_16 },
    { "sub", 2, { O_MRM_32 | O_I_MODRM, O_IMM_32 }, 2, { 0x81, 0x28 }, I_DATA_32 },
    { "sub", 2, { O_MRM_64 | O_I_MODRM, O_IMM_S32 }, 2, { 0x81, 0x28 }, I_DATA_64 },

    { "sub", 2, { O_MRM_8 | O_I_MODRM, O_REG_8 | O_I_MIDREG }, 2, { 0x28, 0x00 }, I_DATA_8 },
    { "sub", 2, { O_MRM_16 | O_I_MODRM, O_REG_16 | O_I_MIDREG }, 2, { 0x29, 0x00 }, I_DATA_16 },
    { "sub", 2, { O_MRM_32 | O_I_MODRM, O_REG_32 | O_I_MIDREG }, 2, { 0x29, 0x00 }, I_DATA_32 },
    { "sub", 2, { O_MRM_64 | O_I_MODRM, O_REG_64 | O_I_MIDREG }, 2, { 0x29, 0x00 }, I_DATA_64 },

    { "sub", 2, { O_REG_8 | O_I_MIDREG, O_MRM_8 | O_I_MODRM }, 2, { 0x2A, 0x00 }, I_DATA_8 },
    { "sub", 2, { O_REG_16 | O_I_MIDREG, O_MRM_16 | O_I_MODRM }, 2, { 0x2B, 0x00 }, I_DATA_16 },
    { "sub", 2, { O_REG_32 | O_I_MIDREG, O_MRM_32 | O_I_MODRM }, 2, { 0x2B, 0x00 }, I_DATA_32 },
    { "sub", 2, { O_REG_64 | O_I_MIDREG, O_MRM_64 | O_I_MODRM }, 2, { 0x2B, 0x00 }, I_DATA_64 },

    { "sbb", 2, { O_ACC_8, O_IMM_8 }, 1, { 0x1C }, I_DATA_8 }, 

    { "sbb", 2, { O_MRM_8 | O_I_MODRM, O_IMM_8 }, 2, { 0x80, 0x18 }, I_DATA_8 },
    { "sbb", 2, { O_MRM_16 | O_I_MODRM, O_IMM_S8 }, 2, { 0x83, 0x18 }, I_DATA_16 },
    { "sbb", 2, { O_MRM_32 | O_I_MODRM, O_IMM_S8 }, 2, { 0x83, 0x18 }, I_DATA_32 },
    { "sbb", 2, { O_MRM_64 | O_I_MODRM, O_IMM_S8 }, 2, { 0x83, 0x18 }, I_DATA_64 },

    { "sbb", 2, { O_ACC_16, O_IMM_16 }, 1, { 0x1D }, I_DATA_16 },
    { "sbb", 2, { O_ACC_32, O_IMM_32 }, 1, { 0x1D }, I_DATA_32 },
    { "sbb", 2, { O_ACC_64, O_IMM_S32 }, 1, { 0x1D }, I_DATA_64 },

    { "sbb", 2, { O_MRM_16 | O_I_MODRM, O_IMM_16 }, 2, { 0x81, 0x18 }, I_DATA_16 },
    { "sbb", 2, { O_MRM_32 | O_I_MODRM, O_IMM_32 }, 2, { 0x81, 0x18 }, I_DATA_32 },
    { "sbb", 2, { O_MRM_64 | O_I_MODRM, O_IMM_S32 }, 2, { 0x81, 0x18 }, I_DATA_64 },

    { "sbb", 2, { O_MRM_8 | O_I_MODRM, O_REG_8 | O_I_MIDREG }, 2, { 0x18, 0x00 }, I_DATA_8 },
    { "sbb", 2, { O_MRM_16 | O_I_MODRM, O_REG_16 | O_I_MIDREG }, 2, { 0x19, 0x00 }, I_DATA_16 },
    { "sbb", 2, { O_MRM_32 | O_I_MODRM, O_REG_32 | O_I_MIDREG }, 2, { 0x19, 0x00 }, I_DATA_32 },
    { "sbb", 2, { O_MRM_64 | O_I_MODRM, O_REG_64 | O_I_MIDREG }, 2, { 0x19, 0x00 }, I_DATA_64 },

    { "sbb", 2, { O_REG_8 | O_I_MIDREG, O_MRM_8 | O_I_MODRM }, 2, { 0x1A, 0x00 }, I_DATA_8 },
    { "sbb", 2, { O_REG_16 | O_I_MIDREG, O_MRM_16 | O_I_MODRM }, 2, { 0x1B, 0x00 }, I_DATA_16 },
    { "sbb", 2, { O_REG_32 | O_I_MIDREG, O_MRM_32 | O_I_MODRM }, 2, { 0x1B, 0x00 }, I_DATA_32 },
    { "sbb", 2, { O_REG_64 | O_I_MIDREG, O_MRM_64 | O_I_MODRM }, 2, { 0x1B, 0x00 }, I_DATA_64 },

    { "and", 2, { O_ACC_8, O_IMM_8 }, 1, { 0x24 }, I_DATA_8 }, 

    { "and", 2, { O_MRM_8 | O_I_MODRM, O_IMM_8 }, 2, { 0x80, 0x20 }, I_DATA_8 },
    { "and", 2, { O_MRM_16 | O_I_MODRM, O_IMM_S8 }, 2, { 0x83, 0x20 }, I_DATA_16 },
    { "and", 2, { O_MRM_32 | O_I_MODRM, O_IMM_S8 }, 2, { 0x83, 0x20 }, I_DATA_32 },
    { "and", 2, { O_MRM_64 | O_I_MODRM, O_IMM_S8 }, 2, { 0x83, 0x20 }, I_DATA_64 },

    { "and", 2, { O_ACC_16, O_IMM_16 }, 1, { 0x25 }, I_DATA_16 },
    { "and", 2, { O_ACC_32, O_IMM_32 }, 1, { 0x25 }, I_DATA_32 },
    { "and", 2, { O_ACC_64, O_IMM_S32 }, 1, { 0x25 }, I_DATA_64 },

    { "and", 2, { O_MRM_16 | O_I_MODRM, O_IMM_16 }, 2, { 0x81, 0x20 }, I_DATA_16 },
    { "and", 2, { O_MRM_32 | O_I_MODRM, O_IMM_32 }, 2, { 0x81, 0x20 }, I_DATA_32 },
    { "and", 2, { O_MRM_64 | O_I_MODRM, O_IMM_S32 }, 2, { 0x81, 0x20 }, I_DATA_64 },

    { "and", 2, { O_MRM_8 | O_I_MODRM, O_REG_8 | O_I_MIDREG }, 2, { 0x20, 0x00 }, I_DATA_8 },
    { "and", 2, { O_MRM_16 | O_I_MODRM, O_REG_16 | O_I_MIDREG }, 2, { 0x21, 0x00 }, I_DATA_16 },
    { "and", 2, { O_MRM_32 | O_I_MODRM, O_REG_32 | O_I_MIDREG }, 2, { 0x21, 0x00 }, I_DATA_32 },
    { "and", 2, { O_MRM_64 | O_I_MODRM, O_REG_64 | O_I_MIDREG }, 2, { 0x21, 0x00 }, I_DATA_64 },

    { "and", 2, { O_REG_8 | O_I_MIDREG, O_MRM_8 | O_I_MODRM }, 2, { 0x22, 0x00 }, I_DATA_8 },
    { "and", 2, { O_REG_16 | O_I_MIDREG, O_MRM_16 | O_I_MODRM }, 2, { 0x23, 0x00 }, I_DATA_16 },
    { "and", 2, { O_REG_32 | O_I_MIDREG, O_MRM_32 | O_I_MODRM }, 2, { 0x23, 0x00 }, I_DATA_32 },
    { "and", 2, { O_REG_64 | O_I_MIDREG, O_MRM_64 | O_I_MODRM }, 2, { 0x23, 0x00 }, I_DATA_64 },

    { "cmp", 2, { O_ACC_8, O_IMM_8 }, 1, { 0x3C }, I_DATA_8 }, 

    { "cmp", 2, { O_MRM_8 | O_I_MODRM, O_IMM_8 }, 2, { 0x80, 0x38 }, I_DATA_8 },
    { "cmp", 2, { O_MRM_16 | O_I_MODRM, O_IMM_S8 }, 2, { 0x83, 0x38 }, I_DATA_16 },
    { "cmp", 2, { O_MRM_32 | O_I_MODRM, O_IMM_S8 }, 2, { 0x83, 0x38 }, I_DATA_32 },
    { "cmp", 2, { O_MRM_64 | O_I_MODRM, O_IMM_S8 }, 2, { 0x83, 0x38 }, I_DATA_64 },

    { "cmp", 2, { O_ACC_16, O_IMM_16 }, 1, { 0x3D }, I_DATA_16 },
    { "cmp", 2, { O_ACC_32, O_IMM_32 }, 1, { 0x3D }, I_DATA_32 },
    { "cmp", 2, { O_ACC_64, O_IMM_S32 }, 1, { 0x3D }, I_DATA_64 },

    { "cmp", 2, { O_MRM_16 | O_I_MODRM, O_IMM_16 }, 2, { 0x81, 0x38 }, I_DATA_16 },
    { "cmp", 2, { O_MRM_32 | O_I_MODRM, O_IMM_32 }, 2, { 0x81, 0x38 }, I_DATA_32 },
    { "cmp", 2, { O_MRM_64 | O_I_MODRM, O_IMM_S32 }, 2, { 0x81, 0x38 }, I_DATA_64 },

    { "cmp", 2, { O_MRM_8 | O_I_MODRM, O_REG_8 | O_I_MIDREG }, 2, { 0x38, 0x00 }, I_DATA_8 },
    { "cmp", 2, { O_MRM_16 | O_I_MODRM, O_REG_16 | O_I_MIDREG }, 2, { 0x39, 0x00 }, I_DATA_16 },
    { "cmp", 2, { O_MRM_32 | O_I_MODRM, O_REG_32 | O_I_MIDREG }, 2, { 0x39, 0x00 }, I_DATA_32 },
    { "cmp", 2, { O_MRM_64 | O_I_MODRM, O_REG_64 | O_I_MIDREG }, 2, { 0x39, 0x00 }, I_DATA_64 },

    { "cmp", 2, { O_REG_8 | O_I_MIDREG, O_MRM_8 | O_I_MODRM }, 2, { 0x3A, 0x00 }, I_DATA_8 },
    { "cmp", 2, { O_REG_16 | O_I_MIDREG, O_MRM_16 | O_I_MODRM }, 2, { 0x3B, 0x00 }, I_DATA_16 },
    { "cmp", 2, { O_REG_32 | O_I_MIDREG, O_MRM_32 | O_I_MODRM }, 2, { 0x3B, 0x00 }, I_DATA_32 },
    { "cmp", 2, { O_REG_64 | O_I_MIDREG, O_MRM_64 | O_I_MODRM }, 2, { 0x3B, 0x00 }, I_DATA_64 },

    { "test", 2, { O_ACC_8, O_IMM_8 }, 1, { 0xA8 }, I_DATA_8 }, 
    { "test", 2, { O_ACC_16, O_IMM_16 }, 1, { 0xA9 }, I_DATA_16 },
    { "test", 2, { O_ACC_32, O_IMM_32 }, 1, { 0xA9 }, I_DATA_32 },
    { "test", 2, { O_ACC_64, O_IMM_S32 }, 1, { 0xA9 }, I_DATA_64 },

    { "test", 2, { O_MRM_8 | O_I_MODRM, O_REG_8 | O_I_MIDREG }, 2, { 0x84, 0x00 }, I_DATA_8 },
    { "test", 2, { O_MRM_16 | O_I_MODRM, O_REG_16 | O_I_MIDREG }, 2, { 0x85, 0x00 }, I_DATA_16 },
    { "test", 2, { O_MRM_32 | O_I_MODRM, O_REG_32 | O_I_MIDREG }, 2, { 0x85, 0x00 }, I_DATA_32 },
    { "test", 2, { O_MRM_64 | O_I_MODRM, O_REG_64 | O_I_MIDREG }, 2, { 0x85, 0x00 }, I_DATA_64 },

    { "test", 2, { O_MRM_8 | O_I_MODRM, O_IMM_8 }, 2, { 0xF6, 0x00 }, I_DATA_8 },
    { "test", 2, { O_MRM_16 | O_I_MODRM, O_IMM_16 }, 2, { 0xF7, 0x00 }, I_DATA_16 },
    { "test", 2, { O_MRM_32 | O_I_MODRM, O_IMM_32 }, 2, { 0xF7, 0x00 }, I_DATA_32 },
    { "test", 2, { O_MRM_64 | O_I_MODRM, O_IMM_S32 }, 2, { 0xF7, 0x00 }, I_DATA_64 },

    { "test", 2, { O_REG_8 | O_I_MIDREG, O_MRM_8 | O_I_MODRM }, 2, { 0x84, 0x00 }, I_DATA_8 },
    { "test", 2, { O_REG_16 | O_I_MIDREG, O_MRM_16 | O_I_MODRM }, 2, { 0x85, 0x00 }, I_DATA_16 },
    { "test", 2, { O_REG_32 | O_I_MIDREG, O_MRM_32 | O_I_MODRM }, 2, { 0x85, 0x00 }, I_DATA_32 },
    { "test", 2, { O_REG_64 | O_I_MIDREG, O_MRM_64 | O_I_MODRM }, 2, { 0x85, 0x00 }, I_DATA_64 },

    { "or", 2, { O_ACC_8, O_IMM_8 }, 1, { 0x0C }, I_DATA_8 }, 

    { "or", 2, { O_MRM_8 | O_I_MODRM, O_IMM_8 }, 2, { 0x80, 0x08 }, I_DATA_8 },
    { "or", 2, { O_MRM_16 | O_I_MODRM, O_IMM_S8 }, 2, { 0x83, 0x08 }, I_DATA_16 },
    { "or", 2, { O_MRM_32 | O_I_MODRM, O_IMM_S8 }, 2, { 0x83, 0x08 }, I_DATA_32 },
    { "or", 2, { O_MRM_64 | O_I_MODRM, O_IMM_S8 }, 2, { 0x83, 0x08 }, I_DATA_64 },

    { "or", 2, { O_ACC_16, O_IMM_16 }, 1, { 0x0D }, I_DATA_16 },
    { "or", 2, { O_ACC_32, O_IMM_32 }, 1, { 0x0D }, I_DATA_32 },
    { "or", 2, { O_ACC_64, O_IMM_S32 }, 1, { 0x0D }, I_DATA_64 },

    { "or", 2, { O_MRM_16 | O_I_MODRM, O_IMM_16 }, 2, { 0x81, 0x08 }, I_DATA_16 },
    { "or", 2, { O_MRM_32 | O_I_MODRM, O_IMM_32 }, 2, { 0x81, 0x08 }, I_DATA_32 },
    { "or", 2, { O_MRM_64 | O_I_MODRM, O_IMM_S32 }, 2, { 0x81, 0x08 }, I_DATA_64 },

    { "or", 2, { O_MRM_8 | O_I_MODRM, O_REG_8 | O_I_MIDREG }, 2, { 0x08, 0x00 }, I_DATA_8 },
    { "or", 2, { O_MRM_16 | O_I_MODRM, O_REG_16 | O_I_MIDREG }, 2, { 0x09, 0x00 }, I_DATA_16 },
    { "or", 2, { O_MRM_32 | O_I_MODRM, O_REG_32 | O_I_MIDREG }, 2, { 0x09, 0x00 }, I_DATA_32 },
    { "or", 2, { O_MRM_64 | O_I_MODRM, O_REG_64 | O_I_MIDREG }, 2, { 0x09, 0x00 }, I_DATA_64 },

    { "or", 2, { O_REG_8 | O_I_MIDREG, O_MRM_8 | O_I_MODRM }, 2, { 0x0A, 0x00 }, I_DATA_8 },
    { "or", 2, { O_REG_16 | O_I_MIDREG, O_MRM_16 | O_I_MODRM }, 2, { 0x0B, 0x00 }, I_DATA_16 },
    { "or", 2, { O_REG_32 | O_I_MIDREG, O_MRM_32 | O_I_MODRM }, 2, { 0x0B, 0x00 }, I_DATA_32 },
    { "or", 2, { O_REG_64 | O_I_MIDREG, O_MRM_64 | O_I_MODRM }, 2, { 0x0B, 0x00 }, I_DATA_64 },

    { "xor", 2, { O_ACC_8, O_IMM_8 }, 1, { 0x34 }, I_DATA_8 }, 

    { "xor", 2, { O_MRM_8 | O_I_MODRM, O_IMM_8 }, 2, { 0x80, 0x30 }, I_DATA_8 },
    { "xor", 2, { O_MRM_16 | O_I_MODRM, O_IMM_S8 }, 2, { 0x83, 0x30 }, I_DATA_16 },
    { "xor", 2, { O_MRM_32 | O_I_MODRM, O_IMM_S8 }, 2, { 0x83, 0x30 }, I_DATA_32 },
    { "xor", 2, { O_MRM_64 | O_I_MODRM, O_IMM_S8 }, 2, { 0x83, 0x30 }, I_DATA_64 },

    { "xor", 2, { O_ACC_16, O_IMM_16 }, 1, { 0x35 }, I_DATA_16 },
    { "xor", 2, { O_ACC_32, O_IMM_32 }, 1, { 0x35 }, I_DATA_32 },
    { "xor", 2, { O_ACC_64, O_IMM_S32 }, 1, { 0x35 }, I_DATA_64 },

    { "xor", 2, { O_MRM_16 | O_I_MODRM, O_IMM_16 }, 2, { 0x81, 0x30 }, I_DATA_16 },
    { "xor", 2, { O_MRM_32 | O_I_MODRM, O_IMM_32 }, 2, { 0x81, 0x30 }, I_DATA_32 },
    { "xor", 2, { O_MRM_64 | O_I_MODRM, O_IMM_S32 }, 2, { 0x81, 0x30 }, I_DATA_64 },

    { "xor", 2, { O_MRM_8 | O_I_MODRM, O_REG_8 | O_I_MIDREG }, 2, { 0x30, 0x00 }, I_DATA_8 },
    { "xor", 2, { O_MRM_16 | O_I_MODRM, O_REG_16 | O_I_MIDREG }, 2, { 0x31, 0x00 }, I_DATA_16 },
    { "xor", 2, { O_MRM_32 | O_I_MODRM, O_REG_32 | O_I_MIDREG }, 2, { 0x31, 0x00 }, I_DATA_32 },
    { "xor", 2, { O_MRM_64 | O_I_MODRM, O_REG_64 | O_I_MIDREG }, 2, { 0x31, 0x00 }, I_DATA_64 },

    { "xor", 2, { O_REG_8 | O_I_MIDREG, O_MRM_8 | O_I_MODRM }, 2, { 0x32, 0x00 }, I_DATA_8 },
    { "xor", 2, { O_REG_16 | O_I_MIDREG, O_MRM_16 | O_I_MODRM }, 2, { 0x33, 0x00 }, I_DATA_16 },
    { "xor", 2, { O_REG_32 | O_I_MIDREG, O_MRM_32 | O_I_MODRM }, 2, { 0x33, 0x00 }, I_DATA_32 },
    { "xor", 2, { O_REG_64 | O_I_MIDREG, O_MRM_64 | O_I_MODRM }, 2, { 0x33, 0x00 }, I_DATA_64 },

    { "call", 1, { O_IMM_16 | O_I_REL }, 1, { 0xE8 }, I_NO_BITS_32 | I_NO_BITS_64 },
    { "call", 1, { O_IMM_S32 | O_I_REL }, 1, { 0xE8 }, I_NO_BITS_16 },

    { "call", 1, { O_MRM_16 | O_I_MODRM }, 2, { 0xFF, 0x10 }, I_DATA_16 | I_NO_BITS_64 },
    { "call", 1, { O_MRM_32 | O_I_MODRM }, 2, { 0xFF, 0x10 }, I_DATA_32 | I_NO_BITS_64 },
    { "call", 1, { O_MRM_64 | O_I_MODRM }, 2, { 0xFF, 0x10 }, I_NO_BITS_16 | I_NO_BITS_32 },

    { "callf", 2, { O_IMM_16, O_IMM_16 }, 1, { 0x9A }, I_DATA_16 | I_NO_BITS_64 },
    { "callf", 2, { O_IMM_32, O_IMM_16 }, 1, { 0x9A }, I_DATA_32 | I_NO_BITS_64 },

    { "callf", 1, { O_MEM_16 | O_I_MODRM }, 2, { 0xFF, 0x18 }, I_DATA_16 },
    { "callf", 1, { O_MEM_32 | O_I_MODRM }, 2, { 0xFF, 0x18 }, I_DATA_32 },
    { "callf", 1, { O_MEM_64 | O_I_MODRM }, 2, { 0xFF, 0x18 }, I_DATA_64 },

    { "jmp", 1, { O_REL_8 }, 1, { 0xEB }, 0 },
    { "jmp", 1, { O_IMM_16 | O_I_REL }, 1, { 0xE9 }, I_NO_BITS_32 | I_NO_BITS_64 },
    { "jmp", 1, { O_IMM_S32 | O_I_REL }, 1, { 0xE9 }, I_NO_BITS_16 },

    { "jmp", 1, { O_MRM_16 | O_I_MODRM }, 2, { 0xFF, 0x20 }, I_DATA_16 | I_NO_BITS_64 },
    { "jmp", 1, { O_MRM_32 | O_I_MODRM }, 2, { 0xFF, 0x20 }, I_DATA_32 | I_NO_BITS_64 },
    { "jmp", 1, { O_MRM_64 | O_I_MODRM }, 2, { 0xFF, 0x20 }, I_NO_BITS_16 | I_NO_BITS_32 },

    { "jmpf", 2, { O_IMM_16, O_IMM_16 }, 1, { 0xEA }, I_DATA_16 | I_NO_BITS_64 },
    { "jmpf", 2, { O_IMM_32, O_IMM_16 }, 1, { 0xEA }, I_DATA_32 | I_NO_BITS_64 },

    { "jmpf", 1, { O_MEM_16 | O_I_MODRM }, 2, { 0xFF, 0x28 }, I_DATA_16 },
    { "jmpf", 1, { O_MEM_32 | O_I_MODRM }, 2, { 0xFF, 0x28 }, I_DATA_32 },
    { "jmpf", 1, { O_MEM_64 | O_I_MODRM }, 2, { 0xFF, 0x28 }, I_DATA_64 },

    { "ret", 0, { }, 1, { 0xC3 }, 0 },
    { "ret", 1, { O_IMM_16 }, 1, { 0xC2 }, 0 },

    { "retf", 0, { }, 1, { 0xCB }, 0 },
    { "retf", 1, { O_IMM_16 }, 1, { 0xCA }, 0 },

    { "leave", 0, { }, 1, { 0xC9 }, 0 },

    { "jo", 1, { O_REL_8 }, 1, { 0x70 }, 0 },
    { "jo", 1, { O_IMM_16 | O_I_REL }, 2, { 0x0F, 0x80 }, I_NO_BITS_32 | I_NO_BITS_64 },
    { "jo", 1, { O_IMM_S32 | O_I_REL }, 2, { 0x0F, 0x80 }, I_NO_BITS_16 },

    { "jno", 1, { O_REL_8 }, 1, { 0x71 }, 0 },
    { "jno", 1, { O_IMM_16 | O_I_REL }, 2, { 0x0F, 0x81 }, I_NO_BITS_32 | I_NO_BITS_64 },
    { "jno", 1, { O_IMM_S32 | O_I_REL }, 2, { 0x0F, 0x81 }, I_NO_BITS_16 },

    { "jb", 1, { O_REL_8 }, 1, { 0x72 }, 0 },
    { "jb", 1, { O_IMM_16 | O_I_REL }, 2, { 0x0F, 0x82 }, I_NO_BITS_32 | I_NO_BITS_64 },
    { "jb", 1, { O_IMM_S32 | O_I_REL }, 2, { 0x0F, 0x82 }, I_NO_BITS_16 },
    { "jc", 1, { O_REL_8 }, 1, { 0x72 }, 0 },
    { "jc", 1, { O_IMM_16 | O_I_REL }, 2, { 0x0F, 0x82 }, I_NO_BITS_32 | I_NO_BITS_64 },
    { "jc", 1, { O_IMM_S32 | O_I_REL }, 2, { 0x0F, 0x82 }, I_NO_BITS_16 },
    { "jnae", 1, { O_REL_8 }, 1, { 0x72 }, 0 },
    { "jnae", 1, { O_IMM_16 | O_I_REL }, 2, { 0x0F, 0x82 }, I_NO_BITS_32 | I_NO_BITS_64 },
    { "jnae", 1, { O_IMM_S32 | O_I_REL }, 2, { 0x0F, 0x82 }, I_NO_BITS_16 },

    { "jnb", 1, { O_REL_8 }, 1, { 0x73 }, 0 },
    { "jnb", 1, { O_IMM_16 | O_I_REL }, 2, { 0x0F, 0x83 }, I_NO_BITS_32 | I_NO_BITS_64 },
    { "jnb", 1, { O_IMM_S32 | O_I_REL }, 2, { 0x0F, 0x83 }, I_NO_BITS_16 },
    { "jnc", 1, { O_REL_8 }, 1, { 0x73 }, 0 },
    { "jnc", 1, { O_IMM_16 | O_I_REL }, 2, { 0x0F, 0x83 }, I_NO_BITS_32 | I_NO_BITS_64 },
    { "jnc", 1, { O_IMM_S32 | O_I_REL }, 2, { 0x0F, 0x83 }, I_NO_BITS_16 },
    { "jae", 1, { O_REL_8 }, 1, { 0x73 }, 0 },
    { "jae", 1, { O_IMM_16 | O_I_REL }, 2, { 0x0F, 0x83 }, I_NO_BITS_32 | I_NO_BITS_64 },
    { "jae", 1, { O_IMM_S32 | O_I_REL }, 2, { 0x0F, 0x83 }, I_NO_BITS_16 },

    { "je", 1, { O_REL_8 }, 1, { 0x74 }, 0 },
    { "je", 1, { O_IMM_16 | O_I_REL }, 2, { 0x0F, 0x84 }, I_NO_BITS_32 | I_NO_BITS_64 },
    { "je", 1, { O_IMM_S32 | O_I_REL }, 2, { 0x0F, 0x84 }, I_NO_BITS_16 },
    { "jz", 1, { O_REL_8 }, 1, { 0x74 }, 0 },
    { "jz", 1, { O_IMM_16 | O_I_REL }, 2, { 0x0F, 0x84 }, I_NO_BITS_32 | I_NO_BITS_64 },
    { "jz", 1, { O_IMM_S32 | O_I_REL }, 2, { 0x0F, 0x84 }, I_NO_BITS_16 },

    { "jne", 1, { O_REL_8 }, 1, { 0x75 }, 0 },
    { "jne", 1, { O_IMM_16 | O_I_REL }, 2, { 0x0F, 0x85 }, I_NO_BITS_32 | I_NO_BITS_64 },
    { "jne", 1, { O_IMM_S32 | O_I_REL }, 2, { 0x0F, 0x85 }, I_NO_BITS_16 },
    { "jnz", 1, { O_REL_8 }, 1, { 0x75 }, 0 },
    { "jnz", 1, { O_IMM_16 | O_I_REL }, 2, { 0x0F, 0x85 }, I_NO_BITS_32 | I_NO_BITS_64 },
    { "jnz", 1, { O_IMM_S32 | O_I_REL }, 2, { 0x0F, 0x85 }, I_NO_BITS_16 },

    { "jbe", 1, { O_REL_8 }, 1, { 0x76 }, 0 },
    { "jbe", 1, { O_IMM_16 | O_I_REL }, 2, { 0x0F, 0x86 }, I_NO_BITS_32 | I_NO_BITS_64 },
    { "jbe", 1, { O_IMM_S32 | O_I_REL }, 2, { 0x0F, 0x86 }, I_NO_BITS_16 },
    { "jna", 1, { O_REL_8 }, 1, { 0x76 }, 0 },
    { "jna", 1, { O_IMM_16 | O_I_REL }, 2, { 0x0F, 0x86 }, I_NO_BITS_32 | I_NO_BITS_64 },
    { "jna", 1, { O_IMM_S32 | O_I_REL }, 2, { 0x0F, 0x86 }, I_NO_BITS_16 },

    { "jnbe", 1, { O_REL_8 }, 1, { 0x77 }, 0 },
    { "jnbe", 1, { O_IMM_16 | O_I_REL }, 2, { 0x0F, 0x87 }, I_NO_BITS_32 | I_NO_BITS_64 },
    { "jnbe", 1, { O_IMM_S32 | O_I_REL }, 2, { 0x0F, 0x87 }, I_NO_BITS_16 },
    { "ja", 1, { O_REL_8 }, 1, { 0x77 }, 0 },
    { "ja", 1, { O_IMM_16 | O_I_REL }, 2, { 0x0F, 0x87 }, I_NO_BITS_32 | I_NO_BITS_64 },
    { "ja", 1, { O_IMM_S32 | O_I_REL }, 2, { 0x0F, 0x87 }, I_NO_BITS_16 },

    { "js", 1, { O_REL_8 }, 1, { 0x78 }, 0 },
    { "js", 1, { O_IMM_16 | O_I_REL }, 2, { 0x0F, 0x88 }, I_NO_BITS_32 | I_NO_BITS_64 },
    { "js", 1, { O_IMM_S32 | O_I_REL }, 2, { 0x0F, 0x88 }, I_NO_BITS_16 },

    { "jns", 1, { O_REL_8 }, 1, { 0x79 }, 0 },
    { "jns", 1, { O_IMM_16 | O_I_REL }, 2, { 0x0F, 0x89 }, I_NO_BITS_32 | I_NO_BITS_64 },
    { "jns", 1, { O_IMM_S32 | O_I_REL }, 2, { 0x0F, 0x89 }, I_NO_BITS_16 },

    { "jp", 1, { O_REL_8 }, 1, { 0x7A }, 0 },
    { "jp", 1, { O_IMM_16 | O_I_REL }, 2, { 0x0F, 0x8A }, I_NO_BITS_32 | I_NO_BITS_64 },
    { "jp", 1, { O_IMM_S32 | O_I_REL }, 2, { 0x0F, 0x8A }, I_NO_BITS_16 },
    { "jpe", 1, { O_REL_8 }, 1, { 0x7A }, 0 },
    { "jpe", 1, { O_IMM_16 | O_I_REL }, 2, { 0x0F, 0x8A }, I_NO_BITS_32 | I_NO_BITS_64 },
    { "jpe", 1, { O_IMM_S32 | O_I_REL }, 2, { 0x0F, 0x8A }, I_NO_BITS_16 },

    { "jnp", 1, { O_REL_8 }, 1, { 0x7B }, 0 },
    { "jnp", 1, { O_IMM_16 | O_I_REL }, 2, { 0x0F, 0x8B }, I_NO_BITS_32 | I_NO_BITS_64 },
    { "jnp", 1, { O_IMM_S32 | O_I_REL }, 2, { 0x0F, 0x8B }, I_NO_BITS_16 },
    { "jpo", 1, { O_REL_8 }, 1, { 0x7B }, 0 },
    { "jpo", 1, { O_IMM_16 | O_I_REL }, 2, { 0x0F, 0x8B }, I_NO_BITS_32 | I_NO_BITS_64 },
    { "jpo", 1, { O_IMM_S32 | O_I_REL }, 2, { 0x0F, 0x8B }, I_NO_BITS_16 },

    { "jl", 1, { O_REL_8 }, 1, { 0x7C }, 0 },
    { "jl", 1, { O_IMM_16 | O_I_REL }, 2, { 0x0F, 0x8C }, I_NO_BITS_32 | I_NO_BITS_64 },
    { "jl", 1, { O_IMM_S32 | O_I_REL }, 2, { 0x0F, 0x8C }, I_NO_BITS_16 },
    { "jnge", 1, { O_REL_8 }, 1, { 0x7C }, 0 },
    { "jnge", 1, { O_IMM_16 | O_I_REL }, 2, { 0x0F, 0x8C }, I_NO_BITS_32 | I_NO_BITS_64 },
    { "jnge", 1, { O_IMM_S32 | O_I_REL }, 2, { 0x0F, 0x8C }, I_NO_BITS_16 },

    { "jnl", 1, { O_REL_8 }, 1, { 0x7D }, 0 },
    { "jnl", 1, { O_IMM_16 | O_I_REL }, 2, { 0x0F, 0x8D }, I_NO_BITS_32 | I_NO_BITS_64 },
    { "jnl", 1, { O_IMM_S32 | O_I_REL }, 2, { 0x0F, 0x8D }, I_NO_BITS_16 },
    { "jge", 1, { O_REL_8 }, 1, { 0x7D }, 0 },
    { "jge", 1, { O_IMM_16 | O_I_REL }, 2, { 0x0F, 0x8D }, I_NO_BITS_32 | I_NO_BITS_64 },
    { "jge", 1, { O_IMM_S32 | O_I_REL }, 2, { 0x0F, 0x8D }, I_NO_BITS_16 },

    { "jle", 1, { O_REL_8 }, 1, { 0x7E }, 0 },
    { "jle", 1, { O_IMM_16 | O_I_REL }, 2, { 0x0F, 0x8E }, I_NO_BITS_32 | I_NO_BITS_64 },
    { "jle", 1, { O_IMM_S32 | O_I_REL }, 2, { 0x0F, 0x8E }, I_NO_BITS_16 },
    { "jng", 1, { O_REL_8 }, 1, { 0x7E }, 0 },
    { "jng", 1, { O_IMM_16 | O_I_REL }, 2, { 0x0F, 0x8E }, I_NO_BITS_32 | I_NO_BITS_64 },
    { "jng", 1, { O_IMM_S32 | O_I_REL }, 2, { 0x0F, 0x8E }, I_NO_BITS_16 },

    { "jnle", 1, { O_REL_8 }, 1, { 0x7F }, 0 },
    { "jnle", 1, { O_IMM_16 | O_I_REL }, 2, { 0x0F, 0x8F }, I_NO_BITS_32 | I_NO_BITS_64 },
    { "jnle", 1, { O_IMM_S32 | O_I_REL }, 2, { 0x0F, 0x8F }, I_NO_BITS_16 },
    { "jg", 1, { O_REL_8 }, 1, { 0x7F }, 0 },
    { "jg", 1, { O_IMM_16 | O_I_REL }, 2, { 0x0F, 0x8F }, I_NO_BITS_32 | I_NO_BITS_64 },
    { "jg", 1, { O_IMM_S32 | O_I_REL }, 2, { 0x0F, 0x8F }, I_NO_BITS_16 },

    { "seto", 1, { O_MRM_8 | O_I_MODRM }, 3, { 0x0F, 0x90, 0x00 }, 0 },

    { "setno", 1, { O_MRM_8 | O_I_MODRM }, 3, { 0x0F, 0x91, 0x00 }, 0 },

    { "setb", 1, { O_MRM_8 | O_I_MODRM }, 3, { 0x0F, 0x92, 0x00 }, 0 },
    { "setc", 1, { O_MRM_8 | O_I_MODRM }, 3, { 0x0F, 0x92, 0x00 }, 0 },
    { "setnae", 1, { O_MRM_8 | O_I_MODRM }, 3, { 0x0F, 0x92, 0x00 }, 0 },

    { "setnb", 1, { O_MRM_8 | O_I_MODRM }, 3, { 0x0F, 0x93, 0x00 }, 0 },
    { "setnc", 1, { O_MRM_8 | O_I_MODRM }, 3, { 0x0F, 0x93, 0x00 }, 0 },
    { "setae", 1, { O_MRM_8 | O_I_MODRM }, 3, { 0x0F, 0x93, 0x00 }, 0 },

    { "sete", 1, { O_MRM_8 | O_I_MODRM }, 3, { 0x0F, 0x94, 0x00 }, 0 },
    { "setz", 1, { O_MRM_8 | O_I_MODRM }, 3, { 0x0F, 0x94, 0x00 }, 0 },

    { "setne", 1, { O_MRM_8 | O_I_MODRM }, 3, { 0x0F, 0x95, 0x00 }, 0 },
    { "setnz", 1, { O_MRM_8 | O_I_MODRM }, 3, { 0x0F, 0x95, 0x00 }, 0 },

    { "setbe", 1, { O_MRM_8 | O_I_MODRM }, 3, { 0x0F, 0x96, 0x00 }, 0 },
    { "setna", 1, { O_MRM_8 | O_I_MODRM }, 3, { 0x0F, 0x96, 0x00 }, 0 },

    { "setnbe", 1, { O_MRM_8 | O_I_MODRM }, 3, { 0x0F, 0x97, 0x00 }, 0 },
    { "seta", 1, { O_MRM_8 | O_I_MODRM }, 3, { 0x0F, 0x97, 0x00 }, 0 },

    { "sets", 1, { O_MRM_8 | O_I_MODRM }, 3, { 0x0F, 0x98, 0x00 }, 0 },

    { "setns", 1, { O_MRM_8 | O_I_MODRM }, 3, { 0x0F, 0x99, 0x00 }, 0 },

    { "setp", 1, { O_MRM_8 | O_I_MODRM }, 3, { 0x0F, 0x9A, 0x00 }, 0 },
    { "setpe", 1, { O_MRM_8 | O_I_MODRM }, 3, { 0x0F, 0x9A, 0x00 }, 0 },

    { "setnp", 1, { O_MRM_8 | O_I_MODRM }, 3, { 0x0F, 0x9B, 0x00 }, 0 },
    { "setpo", 1, { O_MRM_8 | O_I_MODRM }, 3, { 0x0F, 0x9B, 0x00 }, 0 },

    { "setl", 1, { O_MRM_8 | O_I_MODRM }, 3, { 0x0F, 0x9C, 0x00 }, 0 },
    { "setnge", 1, { O_MRM_8 | O_I_MODRM }, 3, { 0x0F, 0x9C, 0x00 }, 0 },

    { "setnl", 1, { O_MRM_8 | O_I_MODRM }, 3, { 0x0F, 0x9D, 0x00 }, 0 },
    { "setge", 1, { O_MRM_8 | O_I_MODRM }, 3, { 0x0F, 0x9D, 0x00 }, 0 },

    { "setle", 1, { O_MRM_8 | O_I_MODRM }, 3, { 0x0F, 0x9E, 0x00 }, 0 },
    { "setng", 1, { O_MRM_8 | O_I_MODRM }, 3, { 0x0F, 0x9E, 0x00 }, 0 },

    { "setnle", 1, { O_MRM_8 | O_I_MODRM }, 3, { 0x0F, 0x9F, 0x00 }, 0 },
    { "setg", 1, { O_MRM_8 | O_I_MODRM }, 3, { 0x0F, 0x9F, 0x00 }, 0 },

    { "loop", 1, { O_REL_8 }, 1, { 0xE2 }, 0 },

    { "cmpsb", 0, { }, 1, { 0xA6 }, 0 },
    { "cmpsw", 0, { }, 1, { 0xA7 }, I_DATA_16 },
    { "cmpsd", 0, { }, 1, { 0xA7 }, I_DATA_32 },
    { "cmpsq", 0, { }, 1, { 0xA7 }, I_DATA_64 },

    { "insb", 0, { }, 1, { 0x6C }, 0 },
    { "insw", 0, { }, 1, { 0x6D }, I_DATA_16 },
    { "insd", 0, { }, 1, { 0x6D }, I_DATA_32 },

    { "lodsb", 0, { }, 1, { 0xAC }, 0 },
    { "lodsw", 0, { }, 1, { 0xAD }, I_DATA_16 },
    { "lodsd", 0, { }, 1, { 0xAD }, I_DATA_32 },
    { "lodsq", 0, { }, 1, { 0xAD }, I_DATA_64 },

    { "movsb", 0, { }, 1, { 0xA4 }, 0 },
    { "movsw", 0, { }, 1, { 0xA5 }, I_DATA_16 },

    { "movsd", 0, { }, 1, { 0xA5 }, I_DATA_32 },
    
    { "movsd", 2, { O_REG_XMM | O_I_MIDREG, O_REG_XMM | O_MEM_64 | O_I_MODRM }, 3, { 0x0F, 0x10, 0 }, I_PREFIX_F2 },
    { "movsd", 2, { O_MEM_64 | O_I_MODRM, O_REG_XMM | O_I_MIDREG }, 3, { 0x0F, 0x11, 0 }, I_PREFIX_F2 },

    { "movss", 2, { O_REG_XMM | O_I_MIDREG, O_REG_XMM | O_MEM_32 | O_I_MODRM }, 3, { 0x0F, 0x10, 0 }, I_PREFIX_F3 },
    { "movss", 2, { O_MEM_32 | O_I_MODRM, O_REG_XMM | O_I_MIDREG }, 3, { 0x0F, 0x11, 0 }, I_PREFIX_F3 },

    { "cvtsi2sd", 2, { O_REG_XMM | O_I_MIDREG, O_MRM_32 | O_I_MODRM }, 3, { 0x0F, 0x2A, 0 }, I_PREFIX_F2 },
    { "cvtsi2sd", 2, { O_REG_XMM | O_I_MIDREG, O_MRM_64 | O_I_MODRM }, 3, { 0x0F, 0x2A, 0 }, I_PREFIX_F2 | I_DATA_64 },

    { "cvtsi2ss", 2, { O_REG_XMM | O_I_MIDREG, O_MRM_32 | O_I_MODRM }, 3, { 0x0F, 0x2A, 0 }, I_PREFIX_F3 },
    { "cvtsi2ss", 2, { O_REG_XMM | O_I_MIDREG, O_MRM_64 | O_I_MODRM }, 3, { 0x0F, 0x2A, 0 }, I_PREFIX_F3 | I_DATA_64 },

    { "cvtsd2si", 2, { O_REG_32 | O_I_MIDREG, O_REG_XMM | O_MEM_64 | O_I_MODRM }, 3, { 0x0F, 0x2D, 0 }, I_PREFIX_F2 },
    { "cvtsd2si", 2, { O_REG_64 | O_I_MIDREG, O_REG_XMM | O_MEM_64 | O_I_MODRM }, 3, { 0x0F, 0x2D, 0 }, I_PREFIX_F2 | I_DATA_64 },

    { "cvtss2si", 2, { O_REG_32 | O_I_MIDREG, O_REG_XMM | O_MEM_32 | O_I_MODRM }, 3, { 0x0F, 0x2D, 0 }, I_PREFIX_F3 },
    { "cvtss2si", 2, { O_REG_64 | O_I_MIDREG, O_REG_XMM | O_MEM_64 | O_I_MODRM }, 3, { 0x0F, 0x2D, 0 }, I_PREFIX_F3 | I_DATA_64 },

    { "cvtss2sd", 2, { O_REG_XMM | O_I_MIDREG, O_REG_XMM | O_MEM_32 | O_I_MODRM }, 3, { 0x0F, 0x5A, 0 }, I_PREFIX_F3 },
    { "cvtsd2ss", 2, { O_REG_XMM | O_I_MIDREG, O_REG_XMM | O_MEM_64 | O_I_MODRM }, 3, { 0x0F, 0x5A, 0 }, I_PREFIX_F2 },

    { "pxor", 2, { O_REG_XMM | O_I_MIDREG, O_REG_XMM | O_I_MODRM }, 3, { 0x0F, 0xEF, 0 }, I_PREFIX_66 },
    { "ucomisd", 2, { O_REG_XMM | O_I_MIDREG, O_REG_XMM | O_MEM_64 | O_I_MODRM }, 3, { 0x0F, 0x2E, 0 }, I_PREFIX_66 },
    { "ucomiss", 2, { O_REG_XMM | O_I_MIDREG, O_REG_XMM | O_MEM_32 | O_I_MODRM }, 3, { 0x0F, 0x2E, 0 }, 0 },
    { "addsd", 2, { O_REG_XMM | O_I_MIDREG, O_REG_XMM | O_MEM_64 | O_I_MODRM }, 3, { 0x0F, 0x58, 0 }, I_PREFIX_F2 },
    { "addss", 2, { O_REG_XMM | O_I_MIDREG, O_REG_XMM | O_MEM_32 | O_I_MODRM }, 3, { 0x0F, 0x58, 0 }, I_PREFIX_F3 },
    { "subsd", 2, { O_REG_XMM | O_I_MIDREG, O_REG_XMM | O_MEM_64 | O_I_MODRM }, 3, { 0x0F, 0x5C, 0 }, I_PREFIX_F2 },
    { "subss", 2, { O_REG_XMM | O_I_MIDREG, O_REG_XMM | O_MEM_32 | O_I_MODRM }, 3, { 0x0F, 0x5C, 0 }, I_PREFIX_F3 },
    { "mulsd", 2, { O_REG_XMM | O_I_MIDREG, O_REG_XMM | O_MEM_64 | O_I_MODRM }, 3, { 0x0F, 0x59, 0 }, I_PREFIX_F2 },
    { "mulss", 2, { O_REG_XMM | O_I_MIDREG, O_REG_XMM | O_MEM_32 | O_I_MODRM }, 3, { 0x0F, 0x59, 0 }, I_PREFIX_F3 },
    { "divsd", 2, { O_REG_XMM | O_I_MIDREG, O_REG_XMM | O_MEM_64 | O_I_MODRM }, 3, { 0x0F, 0x5E, 0 }, I_PREFIX_F2 },
    { "divss", 2, { O_REG_XMM | O_I_MIDREG, O_REG_XMM | O_MEM_32 | O_I_MODRM }, 3, { 0x0F, 0x5E, 0 }, I_PREFIX_F3 },

    { "movsq", 0, { }, 1, { 0xA5 }, I_DATA_64 },

    { "outsb", 0, { }, 1, { 0x6E }, 0 },
    { "outsw", 0, { }, 1, { 0x6F }, I_DATA_16 },
    { "outsd", 0, { }, 1, { 0x6F }, I_DATA_32 },

    { "scasb", 0, { }, 1, { 0xAE }, 0 },
    { "scasw", 0, { }, 1, { 0xAF }, I_DATA_16 },
    { "scasd", 0, { }, 1, { 0xAF }, I_DATA_32 },
    { "scasq", 0, { }, 1, { 0xAF }, I_DATA_64 },

    { "stosb", 0, { }, 1, { 0xAA }, 0 },
    { "stosw", 0, { }, 1, { 0xAB }, I_DATA_16 },
    { "stosd", 0, { }, 1, { 0xAB }, I_DATA_32 },
    { "stosq", 0, { }, 1, { 0xAB }, I_DATA_64 },

    { "int3", 0, { }, 1, { 0xCC }, 0 },
    { "int", 1, { O_IMM_U8 }, 1, { 0xCD }, 0 },

    { "pusha", 0, { }, 1, { 0x60 }, I_NO_BITS_64 },
    { "popa", 0, { }, 1, { 0x61 }, I_NO_BITS_64 },

    { "pushf", 0, { }, 1, { 0x9C }, I_DATA_16 },
    { "pushfd", 0, { }, 1, { 0x9C }, I_DATA_32 | I_NO_BITS_64 },
    { "pushfq", 0, { }, 1, { 0x9C }, I_DATA_64 | I_NO_DATA_REX },

    { "popf", 0, { }, 1, { 0x9D }, I_DATA_16 },
    { "popfd", 0, { }, 1, { 0x9D }, I_DATA_32 | I_NO_BITS_64 },
    { "popfq", 0, { }, 1, { 0x9D }, I_DATA_64 | I_NO_DATA_REX },

    { "cbw", 0, { }, 1, { 0x98 }, I_DATA_16 },
    { "cwde", 0, { }, 1, { 0x98 }, I_DATA_32 },
    { "cdqe", 0, { }, 1, { 0x98 }, I_DATA_64 },

    { "cwd", 0, { }, 1, { 0x99 }, I_DATA_16 },
    { "cdq", 0, { }, 1, { 0x99 }, I_DATA_32 },
    { "cqo", 0, { }, 1, { 0x99 }, I_DATA_64 },

    { "clc", 0, { }, 1, { 0xF8 }, 0 },
    { "stc", 0, { }, 1, { 0xF9 }, 0 },
    { "cld", 0, { }, 1, { 0xFC }, 0 },
    { "std", 0, { }, 1, { 0xFD }, 0 },
    { "cli", 0, { }, 1, { 0xFA }, 0 },
    { "sti", 0, { }, 1, { 0xFB }, 0 },
    { "cmc", 0, { }, 1, { 0xF5 }, 0 },
    { "rep", 0, { }, 1, { 0xF3 }, 0 },
    { "hlt", 0, { }, 1, { 0xF4 }, 0 },
    { "lock", 0, { }, 1, { 0xF0 }, 0 },
    { "xlat", 0, { }, 1, { 0xD7 }, 0 },

    { "iret", 0, { }, 1, { 0xCF }, I_DATA_16 },
    { "iretd", 0, { }, 1, { 0xCF }, I_DATA_32 },
    { "iretq", 0, { }, 1, { 0xCF }, I_DATA_64 },

    { "cpuid", 0, { }, 2, { 0x0F, 0xA2 }, 0 },
    { "pause", 0, { }, 2, { 0xF3, 0x90 }, 0 },
    { "rdmsr", 0, { }, 2, { 0x0F, 0x32 }, 0 },
    { "wrmsr", 0, { }, 2, { 0x0F, 0x30 }, 0 },
    { "sfence", 0, { }, 3, { 0x0F, 0xAE, 0xF8 }, 0 },

    { "movnti", 2, { O_MEM_32 | O_I_MODRM, O_REG_32 | O_I_MIDREG }, 3, { 0x0F, 0xC3, 0 }, I_DATA_32 },
    { "movnti", 2, { O_MEM_64 | O_I_MODRM, O_REG_64 | O_I_MIDREG }, 3, { 0x0F, 0xC3, 0 }, I_DATA_64 },

    { "fxsave", 1, { O_MEM_64 | O_I_MODRM }, 3, { 0x0F, 0xAE, 0x00 }, I_DATA_64 | I_NO_DATA_REX },
    { "fxrstor", 1, { O_MEM_64 | O_I_MODRM }, 3, { 0x0F, 0xAE, 0x08 }, I_DATA_64 | I_NO_DATA_REX },

    { "lmsw", 1, { O_MRM_16 | O_I_MODRM }, 3, { 0x0F, 0x01, 0x30 }, I_DATA_16 },

    { "ldmxcsr", 1, { O_MEM_32 | O_I_MODRM }, 3, { 0x0F, 0xAE, 0x10 }, I_DATA_32 },
    { "stmxcsr", 1, { O_MEM_32 | O_I_MODRM }, 3, { 0x0F, 0xAE, 0x18 }, I_DATA_32 },

    { "seg", 1, { O_SREG2 | O_I_MIDREG }, 1, { 0x26 }, 0 },
    { "seg", 1, { O_SREG3 | O_I_ENDREG }, 1, { 0x60 }, 0 },

    { "lgdt", 1, { O_MEM_16 | O_I_MODRM }, 3, { 0x0F, 0x01, 0x10 }, I_DATA_16 },
    { "lgdt", 1, { O_MEM_32 | O_I_MODRM }, 3, { 0x0F, 0x01, 0x10 }, I_DATA_32 },
    { "lgdt", 1, { O_MEM_64 | O_I_MODRM }, 3, { 0x0F, 0x01, 0x10 }, I_DATA_64 },

    { "lidt", 1, { O_MEM_16 | O_I_MODRM }, 3, { 0x0F, 0x01, 0x18 }, I_DATA_16 },
    { "lidt", 1, { O_MEM_32 | O_I_MODRM }, 3, { 0x0F, 0x01, 0x18 }, I_DATA_32 },
    { "lidt", 1, { O_MEM_64 | O_I_MODRM }, 3, { 0x0F, 0x01, 0x18 }, I_DATA_64 },

    { "ltr", 1, { O_MRM_16 | O_I_MODRM }, 3, { 0x0F, 0x00, 0x18 }, 0 },
    { "invlpg", 1, { O_MRM_8 | O_I_MODRM }, 3, { 0x0F, 0x01, 0x38 }, 0 },

    { "bts", 2, { O_MRM_16 | O_I_MODRM, O_REG_16 | O_I_MIDREG }, 3, { 0x0F, 0xAB, 0x00 }, I_DATA_16 },
    { "bts", 2, { O_MRM_32 | O_I_MODRM, O_REG_32 | O_I_MIDREG }, 3, { 0x0F, 0xAB, 0x00 }, I_DATA_32 },
    { "bts", 2, { O_MRM_64 | O_I_MODRM, O_REG_64 | O_I_MIDREG }, 3, { 0x0F, 0xAB, 0x00 }, I_DATA_64 },

    { "bts", 2, { O_MRM_16 | O_I_MODRM, O_IMM_U8 }, 3, { 0x0F, 0xBA, 0x28 }, I_DATA_16 },
    { "bts", 2, { O_MRM_32 | O_I_MODRM, O_IMM_U8 }, 3, { 0x0F, 0xBA, 0x28 }, I_DATA_32 },
    { "bts", 2, { O_MRM_64 | O_I_MODRM, O_IMM_U8 }, 3, { 0x0F, 0xBA, 0x28 }, I_DATA_64 },

    { "in", 1, { O_ACC_8 }, 1, { 0xEC }, I_DATA_8 },
    { "in", 1, { O_ACC_16 }, 1, { 0xED }, I_DATA_16 },
    { "in", 1, { O_ACC_32 }, 1, { 0xED }, I_DATA_32 },
    { "in", 2, { O_ACC_8, O_IMM_8 }, 1, { 0xE4 }, I_DATA_8 },
    { "in", 2, { O_ACC_16, O_IMM_8 }, 1, { 0xE5 }, I_DATA_16 },
    { "in", 2, { O_ACC_32, O_IMM_8 }, 1, { 0xE5 }, I_DATA_32 },

    { "out", 1, { O_ACC_8 }, 1, { 0xEE }, I_DATA_8 },
    { "out", 1, { O_ACC_16 }, 1, { 0xEF }, I_DATA_16 },
    { "out", 1, { O_ACC_32 }, 1, { 0xEF }, I_DATA_32 },
    { "out", 2, { O_IMM_8, O_ACC_8 }, 1, { 0xE6 }, I_DATA_8 },
    { "out", 2, { O_IMM_8, O_ACC_16 }, 1, { 0xE7 }, I_DATA_16 },
    { "out", 2, { O_IMM_8, O_ACC_32 }, 1, { 0xE7 }, I_DATA_32 },

    { "shl", 2, { O_MRM_8 | O_I_MODRM, O_IMM_U8 }, 2, { 0xC0, 0x20 }, I_DATA_8 },
    { "shl", 2, { O_MRM_16 | O_I_MODRM, O_IMM_U8 }, 2, { 0xC1, 0x20 }, I_DATA_16 },
    { "shl", 2, { O_MRM_32 | O_I_MODRM, O_IMM_U8 }, 2, { 0xC1, 0x20 }, I_DATA_32 },
    { "shl", 2, { O_MRM_64 | O_I_MODRM, O_IMM_U8 }, 2, { 0xC1, 0x20 }, I_DATA_64 },

    { "shl", 1, { O_MRM_8 | O_I_MODRM }, 2, { 0xD0, 0x20 }, I_DATA_8 },
    { "shl", 1, { O_MRM_16 | O_I_MODRM }, 2, { 0xD1, 0x20 }, I_DATA_16 },
    { "shl", 1, { O_MRM_32 | O_I_MODRM }, 2, { 0xD1, 0x20 }, I_DATA_32 },
    { "shl", 1, { O_MRM_64 | O_I_MODRM }, 2, { 0xD1, 0x20 }, I_DATA_64 },

    { "shl", 2, { O_MRM_8 | O_I_MODRM, O_REG_CL }, 2, { 0xD2, 0x20 }, I_DATA_8 },
    { "shl", 2, { O_MRM_16 | O_I_MODRM, O_REG_CL }, 2, { 0xD3, 0x20 }, I_DATA_16 },
    { "shl", 2, { O_MRM_32 | O_I_MODRM, O_REG_CL }, 2, { 0xD3, 0x20 }, I_DATA_32 },
    { "shl", 2, { O_MRM_64 | O_I_MODRM, O_REG_CL }, 2, { 0xD3, 0x20 }, I_DATA_64 },

    { "shr", 2, { O_MRM_8 | O_I_MODRM, O_IMM_U8 }, 2, { 0xC0, 0x28 }, I_DATA_8 },
    { "shr", 2, { O_MRM_16 | O_I_MODRM, O_IMM_U8 }, 2, { 0xC1, 0x28 }, I_DATA_16 },
    { "shr", 2, { O_MRM_32 | O_I_MODRM, O_IMM_U8 }, 2, { 0xC1, 0x28 }, I_DATA_32 },
    { "shr", 2, { O_MRM_64 | O_I_MODRM, O_IMM_U8 }, 2, { 0xC1, 0x28 }, I_DATA_64 },

    { "shr", 1, { O_MRM_8 | O_I_MODRM }, 2, { 0xD0, 0x28 }, I_DATA_8 },
    { "shr", 1, { O_MRM_16 | O_I_MODRM }, 2, { 0xD1, 0x28 }, I_DATA_16 },
    { "shr", 1, { O_MRM_32 | O_I_MODRM }, 2, { 0xD1, 0x28 }, I_DATA_32 },
    { "shr", 1, { O_MRM_64 | O_I_MODRM }, 2, { 0xD1, 0x28 }, I_DATA_64 },

    { "shr", 2, { O_MRM_8 | O_I_MODRM, O_REG_CL }, 2, { 0xD2, 0x28 }, I_DATA_8 },
    { "shr", 2, { O_MRM_16 | O_I_MODRM, O_REG_CL }, 2, { 0xD3, 0x28 }, I_DATA_16 },
    { "shr", 2, { O_MRM_32 | O_I_MODRM, O_REG_CL }, 2, { 0xD3, 0x28 }, I_DATA_32 },
    { "shr", 2, { O_MRM_64 | O_I_MODRM, O_REG_CL }, 2, { 0xD3, 0x28 }, I_DATA_64 },

    { "sar", 2, { O_MRM_8 | O_I_MODRM, O_IMM_U8 }, 2, { 0xC0, 0x38 }, I_DATA_8 },
    { "sar", 2, { O_MRM_16 | O_I_MODRM, O_IMM_U8 }, 2, { 0xC1, 0x38 }, I_DATA_16 },
    { "sar", 2, { O_MRM_32 | O_I_MODRM, O_IMM_U8 }, 2, { 0xC1, 0x38 }, I_DATA_32 },
    { "sar", 2, { O_MRM_64 | O_I_MODRM, O_IMM_U8 }, 2, { 0xC1, 0x38 }, I_DATA_64 },

    { "sar", 1, { O_MRM_8 | O_I_MODRM }, 2, { 0xD0, 0x38 }, I_DATA_8 },
    { "sar", 1, { O_MRM_16 | O_I_MODRM }, 2, { 0xD1, 0x38 }, I_DATA_16 },
    { "sar", 1, { O_MRM_32 | O_I_MODRM }, 2, { 0xD1, 0x38 }, I_DATA_32 },
    { "sar", 1, { O_MRM_64 | O_I_MODRM }, 2, { 0xD1, 0x38 }, I_DATA_64 },

    { "sar", 2, { O_MRM_8 | O_I_MODRM, O_REG_CL }, 2, { 0xD2, 0x38 }, I_DATA_8 },
    { "sar", 2, { O_MRM_16 | O_I_MODRM, O_REG_CL }, 2, { 0xD3, 0x38 }, I_DATA_16 },
    { "sar", 2, { O_MRM_32 | O_I_MODRM, O_REG_CL }, 2, { 0xD3, 0x38 }, I_DATA_32 },
    { "sar", 2, { O_MRM_64 | O_I_MODRM, O_REG_CL }, 2, { 0xD3, 0x38 }, I_DATA_64 },

    { "dec", 1, { O_REG_16 | O_I_ENDREG }, 1, { 0x48 }, I_DATA_16 | I_NO_BITS_64 },
    { "dec", 1, { O_REG_32 | O_I_ENDREG }, 1, { 0x48 }, I_DATA_32 | I_NO_BITS_64 },

    { "dec", 1, { O_MRM_8 | O_I_MODRM }, 2, { 0xFE, 0x08 }, I_DATA_8 },
    { "dec", 1, { O_MRM_16 | O_I_MODRM }, 2, { 0xFF, 0x08 }, I_DATA_16 },
    { "dec", 1, { O_MRM_32 | O_I_MODRM }, 2, { 0xFF, 0x08 }, I_DATA_32 },
    { "dec", 1, { O_MRM_64 | O_I_MODRM }, 2, { 0xFF, 0x08 }, I_DATA_64 },

    { "inc", 1, { O_REG_16 | O_I_ENDREG }, 1, { 0x40 }, I_DATA_16 | I_NO_BITS_64 },
    { "inc", 1, { O_REG_32 | O_I_ENDREG }, 1, { 0x40 }, I_DATA_32 | I_NO_BITS_64 },

    { "inc", 1, { O_MRM_8 | O_I_MODRM }, 2, { 0xFE, 0x00 }, I_DATA_8 },
    { "inc", 1, { O_MRM_16 | O_I_MODRM }, 2, { 0xFF, 0x00 }, I_DATA_16 },
    { "inc", 1, { O_MRM_32 | O_I_MODRM }, 2, { 0xFF, 0x00 }, I_DATA_32 },
    { "inc", 1, { O_MRM_64 | O_I_MODRM }, 2, { 0xFF, 0x00 }, I_DATA_64 },

    { "div", 1, { O_MRM_8 | O_I_MODRM }, 2, { 0xF6, 0x30 }, I_DATA_8 },
    { "div", 1, { O_MRM_16 | O_I_MODRM }, 2, { 0xF7, 0x30 }, I_DATA_16 },
    { "div", 1, { O_MRM_32 | O_I_MODRM }, 2, { 0xF7, 0x30 }, I_DATA_32 },
    { "div", 1, { O_MRM_64 | O_I_MODRM }, 2, { 0xF7, 0x30 }, I_DATA_64 },

    { "idiv", 1, { O_MRM_8 | O_I_MODRM }, 2, { 0xF6, 0x38 }, I_DATA_8 },
    { "idiv", 1, { O_MRM_16 | O_I_MODRM }, 2, { 0xF7, 0x38 }, I_DATA_16 },
    { "idiv", 1, { O_MRM_32 | O_I_MODRM }, 2, { 0xF7, 0x38 }, I_DATA_32 },
    { "idiv", 1, { O_MRM_64 | O_I_MODRM }, 2, { 0xF7, 0x38 }, I_DATA_64 },

    { "mul", 1, { O_MRM_8 | O_I_MODRM }, 2, { 0xF6, 0x20 }, I_DATA_8 },
    { "mul", 1, { O_MRM_16 | O_I_MODRM }, 2, { 0xF7, 0x20 }, I_DATA_16 },
    { "mul", 1, { O_MRM_32 | O_I_MODRM }, 2, { 0xF7, 0x20 }, I_DATA_32 },
    { "mul", 1, { O_MRM_64 | O_I_MODRM }, 2, { 0xF7, 0x20 }, I_DATA_64 },

    { "imul", 1, { O_MRM_8 | O_I_MODRM }, 2, { 0xF6, 0x28 }, I_DATA_8 },
    { "imul", 1, { O_MRM_16 | O_I_MODRM }, 2, { 0xF7, 0x28 }, I_DATA_16 },
    { "imul", 1, { O_MRM_32 | O_I_MODRM }, 2, { 0xF7, 0x28 }, I_DATA_32 },
    { "imul", 1, { O_MRM_64 | O_I_MODRM }, 2, { 0xF7, 0x28 }, I_DATA_64 },

    { "imul", 2, { O_REG_16 | O_I_MIDREG, O_MRM_16 | O_I_MODRM }, 3, { 0x0F, 0xAF, 0x00 }, I_DATA_16 }, 
    { "imul", 2, { O_REG_32 | O_I_MIDREG, O_MRM_32 | O_I_MODRM }, 3, { 0x0F, 0xAF, 0x00 }, I_DATA_32 }, 
    { "imul", 2, { O_REG_64 | O_I_MIDREG, O_MRM_64 | O_I_MODRM }, 3, { 0x0F, 0xAF, 0x00 }, I_DATA_64 }, 

    { "imul", 2, { O_REG_16 | O_I_MODRM | O_I_MIDREG, O_IMM_S8 }, 2, { 0x6B, 0x00 }, I_DATA_16 },
    { "imul", 2, { O_REG_32 | O_I_MODRM | O_I_MIDREG, O_IMM_S8 }, 2, { 0x6B, 0x00 }, I_DATA_32 },
    { "imul", 2, { O_REG_64 | O_I_MODRM | O_I_MIDREG, O_IMM_S8 }, 2, { 0x6B, 0x00 }, I_DATA_64 },

    { "imul", 2, { O_REG_16 | O_I_MODRM | O_I_MIDREG, O_IMM_16 }, 2, { 0x69, 0x00 }, I_DATA_16 },
    { "imul", 2, { O_REG_32 | O_I_MODRM | O_I_MIDREG, O_IMM_32 }, 2, { 0x69, 0x00 }, I_DATA_32 },
    { "imul", 2, { O_REG_64 | O_I_MODRM | O_I_MIDREG, O_IMM_S32 }, 2, { 0x69, 0x00 }, I_DATA_64 },

    { "lea", 2, { O_REG_16 | O_I_MIDREG, O_MEM | O_I_MODRM }, 2, { 0x8D, 0x00 }, I_DATA_16 },
    { "lea", 2, { O_REG_32 | O_I_MIDREG, O_MEM | O_I_MODRM }, 2, { 0x8D, 0x00 }, I_DATA_32 },
    { "lea", 2, { O_REG_64 | O_I_MIDREG, O_MEM | O_I_MODRM }, 2, { 0x8D, 0x00 }, I_DATA_64 },

    { "neg", 1, { O_MRM_8 | O_I_MODRM }, 2, { 0xF6, 0x18 }, I_DATA_8 },
    { "neg", 1, { O_MRM_16 | O_I_MODRM }, 2, { 0xF7, 0x18 }, I_DATA_16 },
    { "neg", 1, { O_MRM_32 | O_I_MODRM }, 2, { 0xF7, 0x18 }, I_DATA_32 },
    { "neg", 1, { O_MRM_64 | O_I_MODRM }, 2, { 0xF7, 0x18 }, I_DATA_64 },

    { "not", 1, { O_MRM_8 | O_I_MODRM }, 2, { 0xF6, 0x10 }, I_DATA_8 },
    { "not", 1, { O_MRM_16 | O_I_MODRM }, 2, { 0xF7, 0x10 }, I_DATA_16 },
    { "not", 1, { O_MRM_32 | O_I_MODRM }, 2, { 0xF7, 0x10 }, I_DATA_32 },
    { "not", 1, { O_MRM_64 | O_I_MODRM }, 2, { 0xF7, 0x10 }, I_DATA_64 },

    { "movsx", 2, { O_REG_16 | O_I_MIDREG, O_MRM_8 | O_I_MODRM }, 3, { 0x0F, 0xBE, 0x00 }, I_DATA_16 },
    { "movsx", 2, { O_REG_32 | O_I_MIDREG, O_MRM_8 | O_I_MODRM }, 3, { 0x0F, 0xBE, 0x00 }, I_DATA_32 },
    { "movsx", 2, { O_REG_64 | O_I_MIDREG, O_MRM_8 | O_I_MODRM }, 3, { 0x0F, 0xBE, 0x00 }, I_DATA_64 },
    { "movsx", 2, { O_REG_32 | O_I_MIDREG, O_MRM_16 | O_I_MODRM }, 3, { 0x0F, 0xBF, 0x00 }, I_DATA_32 },
    { "movsx", 2, { O_REG_64 | O_I_MIDREG, O_MRM_16 | O_I_MODRM }, 3, { 0x0F, 0xBF, 0x00 }, I_DATA_64 },

    { "movsx", 2, { O_REG_64 | O_I_MIDREG, O_MRM_32 | O_I_MODRM }, 2, { 0x63, 0x00 }, I_DATA_64 },

    { "movzx", 2, { O_REG_16 | O_I_MIDREG, O_MRM_8 | O_I_MODRM }, 3, { 0x0F, 0xB6, 0x00 }, I_DATA_16 },
    { "movzx", 2, { O_REG_32 | O_I_MIDREG, O_MRM_8 | O_I_MODRM }, 3, { 0x0F, 0xB6, 0x00 }, I_DATA_32 },
    { "movzx", 2, { O_REG_64 | O_I_MIDREG, O_MRM_8 | O_I_MODRM }, 3, { 0x0F, 0xB6, 0x00 }, I_DATA_64 },
    { "movzx", 2, { O_REG_32 | O_I_MIDREG, O_MRM_16 | O_I_MODRM }, 3, { 0x0F, 0xB7, 0x00 }, I_DATA_32 },
    { "movzx", 2, { O_REG_64 | O_I_MIDREG, O_MRM_16 | O_I_MODRM }, 3, { 0x0F, 0xB7, 0x00 }, I_DATA_64 },

    /* MOVZX reg64, mod/reg32 isn't real: MOV reg32, mod/reg32 zero-extends into reg64, so substitute that */

    { "movzx", 2, { O_REG_64 | O_I_MIDREG, O_MRM_32 | O_I_MODRM }, 2, { 0x8B }, I_DATA_32 | I_NO_BITS_16 | I_NO_BITS_32 },

    { NULL }
};
