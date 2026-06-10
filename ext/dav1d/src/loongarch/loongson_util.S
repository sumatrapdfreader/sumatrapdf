/******************************************************************************
 * Copyright © 2024, VideoLAN and dav1d authors
 * Copyright © 2024, Loongson Technology Corporation Limited
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *****************************************************************************/

#ifndef DAV1D_SRC_LOONGSON_UTIL_S
#define DAV1D_SRC_LOONGSON_UTIL_S

#ifndef DEFAULT_ALIGN
#define DEFAULT_ALIGN 5
#endif

//That l means local defines local functions
.macro functionl name, align=DEFAULT_ALIGN
.macro endfuncl
    jirl    $r0, $r1, 0x0
    .size \name, . - \name
    .purgem endfuncl
.endm
.text ;
.align \align ;
.hidden \name ;
.type \name, @function ;
\name: ;
.endm

.macro TRANSPOSE_4x16B in0, in1 ,in2, in3, in4, in5, in6, in7
    vpackev.b        \in4,  \in1,  \in0
    vpackod.b        \in5,  \in1,  \in0
    vpackev.b        \in6,  \in3,  \in2
    vpackod.b        \in7,  \in3,  \in2

    vpackev.h        \in0,  \in6,  \in4
    vpackod.h        \in2,  \in6,  \in4
    vpackev.h        \in1,  \in7,  \in5
    vpackod.h        \in3,  \in7,  \in5
.endm

.macro TRANSPOSE_8x16B in0, in1, in2, in3, in4, in5, in6, in7, in8, in9
    vpackev.b        \in8,  \in1,  \in0
    vpackod.b        \in9,  \in1,  \in0
    vpackev.b        \in1,  \in3,  \in2
    vpackod.b        \in3,  \in3,  \in2
    vpackev.b        \in0,  \in5,  \in4
    vpackod.b        \in5,  \in5,  \in4
    vpackev.b        \in2,  \in7,  \in6
    vpackod.b        \in7,  \in7,  \in6

    vpackev.h        \in4,  \in2,  \in0
    vpackod.h        \in2,  \in2,  \in0
    vpackev.h        \in6,  \in7,  \in5
    vpackod.h        \in7,  \in7,  \in5
    vpackev.h        \in5,  \in3,  \in9
    vpackod.h        \in9,  \in3,  \in9
    vpackev.h        \in3,  \in1,  \in8
    vpackod.h        \in8,  \in1,  \in8

    vpackev.w        \in0,  \in4,  \in3
    vpackod.w        \in4,  \in4,  \in3
    vpackev.w        \in1,  \in6,  \in5
    vpackod.w        \in5,  \in6,  \in5
    vpackod.w        \in6,  \in2,  \in8
    vpackev.w        \in2,  \in2,  \in8
    vpackev.w        \in3,  \in7,  \in9
    vpackod.w        \in7,  \in7,  \in9
.endm

.macro vld_x8 src, start, stride, in0, in1, in2, in3, in4, in5, in6, in7
    vld           \in0,     \src,     \start
    vld           \in1,     \src,     \start+(\stride*1)
    vld           \in2,     \src,     \start+(\stride*2)
    vld           \in3,     \src,     \start+(\stride*3)
    vld           \in4,     \src,     \start+(\stride*4)
    vld           \in5,     \src,     \start+(\stride*5)
    vld           \in6,     \src,     \start+(\stride*6)
    vld           \in7,     \src,     \start+(\stride*7)
.endm

.macro vst_x8 src, start, stride, in0, in1, in2, in3, in4, in5, in6, in7
    vst           \in0,     \src,     \start
    vst           \in1,     \src,     \start+(\stride*1)
    vst           \in2,     \src,     \start+(\stride*2)
    vst           \in3,     \src,     \start+(\stride*3)
    vst           \in4,     \src,     \start+(\stride*4)
    vst           \in5,     \src,     \start+(\stride*5)
    vst           \in6,     \src,     \start+(\stride*6)
    vst           \in7,     \src,     \start+(\stride*7)
.endm

.macro vld_x16 src, start, stride, in0, in1, in2, in3, in4, in5, in6, in7, \
               in8, in9, in10, in11, in12, in13, in14, in15

    vld_x8 \src, \start, \stride, \in0, \in1, \in2, \in3, \in4, \in5, \in6, \in7

    vld           \in8,     \src,     \start+(\stride*8)
    vld           \in9,     \src,     \start+(\stride*9)
    vld           \in10,    \src,     \start+(\stride*10)
    vld           \in11,    \src,     \start+(\stride*11)
    vld           \in12,    \src,     \start+(\stride*12)
    vld           \in13,    \src,     \start+(\stride*13)
    vld           \in14,    \src,     \start+(\stride*14)
    vld           \in15,    \src,     \start+(\stride*15)
.endm

.macro vst_x16 src, start, stride, in0, in1, in2, in3, in4, in5, in6, in7, \
               in8, in9, in10, in11, in12, in13, in14, in15

    vst_x8 \src, \start, \stride, \in0, \in1, \in2, \in3, \in4, \in5, \in6, \in7

    vst           \in8,     \src,     \start+(\stride*8)
    vst           \in9,     \src,     \start+(\stride*9)
    vst           \in10,    \src,     \start+(\stride*10)
    vst           \in11,    \src,     \start+(\stride*11)
    vst           \in12,    \src,     \start+(\stride*12)
    vst           \in13,    \src,     \start+(\stride*13)
    vst           \in14,    \src,     \start+(\stride*14)
    vst           \in15,    \src,     \start+(\stride*15)
.endm

.macro xvld_x8 src, start, stride, in0, in1, in2, in3, in4, in5, in6, in7
    xvld           \in0,     \src,     \start
    xvld           \in1,     \src,     \start+(\stride)
    xvld           \in2,     \src,     \start+(\stride<<1)
    xvld           \in3,     \src,     \start+(\stride<<1)+(\stride)
    xvld           \in4,     \src,     \start+(\stride<<2)
    xvld           \in5,     \src,     \start+(\stride<<2)+(\stride)
    xvld           \in6,     \src,     \start+(\stride*6)
    xvld           \in7,     \src,     \start+(\stride<<3)-(\stride)
.endm

.macro xvst_x8 src, start, stride, in0, in1, in2, in3, in4, in5, in6, in7
    xvst           \in0,     \src,     \start
    xvst           \in1,     \src,     \start+(\stride)
    xvst           \in2,     \src,     \start+(\stride<<1)
    xvst           \in3,     \src,     \start+(\stride<<1)+(\stride)
    xvst           \in4,     \src,     \start+(\stride<<2)
    xvst           \in5,     \src,     \start+(\stride<<2)+(\stride)
    xvst           \in6,     \src,     \start+(\stride*6)
    xvst           \in7,     \src,     \start+(\stride<<3)-(\stride)
.endm

.macro xvld_x16 src, start, stride, in0, in1, in2, in3, in4, in5, in6, in7, \
                in8, in9, in10, in11, in12, in13, in14, in15
    xvld_x8 \src, \start, \stride, \in0, \in1, \in2, \in3, \in4, \in5, \in6, \in7

    xvld           \in8,     \src,     \start+(\stride<<3)
    xvld           \in9,     \src,     \start+(\stride<<3)+(\stride)
    xvld           \in10,    \src,     \start+(\stride*10)
    xvld           \in11,    \src,     \start+(\stride*11)
    xvld           \in12,    \src,     \start+(\stride*12)
    xvld           \in13,    \src,     \start+(\stride*13)
    xvld           \in14,    \src,     \start+(\stride*14)
    xvld           \in15,    \src,     \start+(\stride<<4)-(\stride)
.endm

.macro xvst_x16 src, start, stride, in0, in1, in2, in3, in4, in5, in6, in7, \
               in8, in9, in10, in11, in12, in13, in14, in15
    xvst_x8 \src, \start, \stride, \in0, \in1, \in2, \in3, \in4, \in5, \in6, \in7

    xvst           \in8,     \src,     \start+(\stride<<3)
    xvst           \in9,     \src,     \start+(\stride<<3)+(\stride)
    xvst           \in10,    \src,     \start+(\stride*10)
    xvst           \in11,    \src,     \start+(\stride*11)
    xvst           \in12,    \src,     \start+(\stride*12)
    xvst           \in13,    \src,     \start+(\stride*13)
    xvst           \in14,    \src,     \start+(\stride*14)
    xvst           \in15,    \src,     \start+(\stride<<4)-(\stride)
.endm

#endif /* DAV1D_SRC_LOONGSON_UTIL_S */
