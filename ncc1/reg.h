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

/* registers are divided into two classes, integral and float.
   in those classes each register has an index, starting from 0.
   the first 16 registers in each class are the real registers,
   and the remainder are pseudo registers. */

#define NR_REGS         16  /* in each class */

#define R_IDX(x)        ((x) & 0x0FFFFFFF)
#define R_IS_PSEUDO(x)  (R_IDX(x) >= NR_REGS)

#define R_NONE          0
#define R_IS_INTEGRAL   0x10000000
#define R_IS_FLOAT      0x20000000

#define R_AX            (R_IS_INTEGRAL | 0)
#define R_DX            (R_IS_INTEGRAL | 1)
#define R_CX            (R_IS_INTEGRAL | 2)
#define R_BX            (R_IS_INTEGRAL | 3)
#define R_SI            (R_IS_INTEGRAL | 4)
#define R_DI            (R_IS_INTEGRAL | 5)
#define R_BP            (R_IS_INTEGRAL | 6)
#define R_SP            (R_IS_INTEGRAL | 7)
#define R_8             (R_IS_INTEGRAL | 8)
#define R_9             (R_IS_INTEGRAL | 9)
#define R_10            (R_IS_INTEGRAL | 10)
#define R_11            (R_IS_INTEGRAL | 11)
#define R_12            (R_IS_INTEGRAL | 12)
#define R_13            (R_IS_INTEGRAL | 13)
#define R_14            (R_IS_INTEGRAL | 14)
#define R_15            (R_IS_INTEGRAL | 15)
#define R_IPSEUDO       (R_IS_INTEGRAL | NR_REGS)

#define R_XMM0          (R_IS_FLOAT | 0)
#define R_XMM1          (R_IS_FLOAT | 1)
#define R_XMM2          (R_IS_FLOAT | 2)
#define R_XMM3          (R_IS_FLOAT | 3)
#define R_XMM4          (R_IS_FLOAT | 4)
#define R_XMM5          (R_IS_FLOAT | 5)
#define R_XMM6          (R_IS_FLOAT | 6)
#define R_XMM7          (R_IS_FLOAT | 7)
#define R_XMM8          (R_IS_FLOAT | 8)
#define R_XMM9          (R_IS_FLOAT | 9)
#define R_XMM10         (R_IS_FLOAT | 10)
#define R_XMM11         (R_IS_FLOAT | 11)
#define R_XMM12         (R_IS_FLOAT | 12)
#define R_XMM13         (R_IS_FLOAT | 13)
#define R_XMM14         (R_IS_FLOAT | 14)
#define R_XMM15         (R_IS_FLOAT | 15)
#define R_FPSEUDO       (R_IS_FLOAT | NR_REGS)
