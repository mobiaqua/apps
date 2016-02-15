/*
    Copyright (C) 2008 Mans Rullgard

    Permission is hereby granted, free of charge, to any person
    obtaining a copy of this software and associated documentation
    files (the "Software"), to deal in the Software without
    restriction, including without limitation the rights to use, copy,
    modify, merge, publish, distribute, sublicense, and/or sell copies
    of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be
    included in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
    NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
    HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
    WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
 */

        .macro mov32    rd, val
        movw            \rd, #:lower16:\val
        movt            \rd, #:upper16:\val
        .endm

        .fpu neon
        .text

        .global yuv420_to_nv12
        .func   yuv420_to_nv12
yuv420_to_nv12:
        ldm             r4,  {r4-r8}
        add             r4,  r4,  #31
        bic             r4,  r4,  #31
        sub             r6,  r6,  r4
        sub             r7,  r7,  r4,  lsr #1
        sub             r8,  r8,  r4
        mov             r10, r5
1:
        mov             r9,  r4
2:
        pld             [r1, #256]
        vld1.8          {q0-q1},  [r1,:128]!
        vst1.8          {q0-q1},  [r0,:128]!
        subs            r9,  r9,  #32
        bgt             2b
        add             r1,  r1,  r6
        add             r0,  r0,  r8
        subs            r10, r10, #1
        bgt             1b
1:
        lsr             r9,  r4,  #1
2:
        pld             [r2, #256]
        pld             [r3, #256]
        vld1.8          {q0},     [r2,:128]!
        vld1.8          {q1},     [r3,:128]!
        vst2.8          {q0-q1},  [r11,:256]!
        subs            r9,  r9,  #16
        bgt             2b
        add             r2,  r2,  r7
        add             r3,  r3,  r7
        add             r11, r11, r8
        subs            r5,  r5,  #2
        bgt             1b

        pop             {r4-r11,pc}
        .endfunc


        .global yuv420_to_nv12_open
        .func   yuv420_to_nv12_open
yuv420_to_nv12_open:
        push            {r4-r8,lr}
        ldrd            r4,  r5,  [r0, #16]
        ldrd            r6,  r7,  [r0, #24]
        ldr             r8,  [r1, #24]
        mov32           r0,  conv_params
        stm             r0,  {r4-r8}
        adr             r3,  yuv420_to_nv12
        str             r3,  [r0, #20]
        mov             r0,  #0
        pop             {r4-r8,pc}
        .endfunc

        .global yuv420_to_nv12_convert
        .func   yuv420_to_nv12_convert
yuv420_to_nv12_convert:
        push            {r4-r11,lr}
        ldm             r0,  {r0, r11}
        mov32           r4,  conv_params
        ldm             r1,  {r1-r3}
        ldr             pc,  [r4, #20]
        .endfunc

        .section .bss
conv_params:
        .skip           6*4
        .size           conv_params, . - conv_params
