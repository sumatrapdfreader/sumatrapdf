/*
 * Copyright © 2018-2019, VideoLAN and dav1d authors
 * Copyright © 2018-2019, Two Orioles, LLC
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
 */

#include "config.h"

#include <stddef.h>
#include <stdint.h>

#include "common/intops.h"

#include "src/itx_1d.h"

#define CLIP(a) iclip(a, min, max)

/*
 * In some places, we use the pattern like this:
 * t2 = ((in1 *  1567         - in3 * (3784 - 4096) + 2048) >> 12) - in3;
 * even though the reference code might use something like:
 * t2 =  (in1 *  1567         - in3 *  3784         + 2048) >> 12;
 *
 * The reason for this is that for 12 bits/component bitstreams (corrupt/
 * invalid ones, but they are codable nonetheless), each coefficient or
 * input can be 19(+sign) bits, and therefore if the combination of the
 * two multipliers (each 12 bits) is >= 4096, the result of the add/sub
 * after the pair of multiplies will exceed the 31+sign bit range. Signed
 * integer overflows are UB in C, and we'd like to prevent that.
 *
 * To workaround this, we invert one of the two coefficients (or, if both are
 * multiples of 2, we reduce their magnitude by one bit). It should be noted
 * that SIMD implementations do not have to follow this exact behaviour. The
 * AV1 spec clearly states that the result of the multiply/add pairs should
 * fit in 31+sign bit intermediates, and that streams violating this convention
 * are not AV1-compliant. So, as long as we don't trigger UB (which some people
 * would consider a security vulnerability), we're fine. So, SIMD can simply
 * use the faster implementation, even if that might in some cases result in
 * integer overflows, since these are not considered valid AV1 anyway, and in
 * e.g. x86 assembly, integer overflows are not considered UB, but they merely
 * wrap around.
 */

static NOINLINE void
inv_dct4_1d_internal_c(int32_t *const c, const ptrdiff_t stride,
                       const int min, const int max, const int tx64)
{
    assert(stride > 0);
    const int in0 = c[0 * stride], in1 = c[1 * stride];

    int t0, t1, t2, t3;
    if (tx64) {
        t0 = t1 = (in0 * 181 + 128) >> 8;
        t2 = (in1 * 1567 + 2048) >> 12;
        t3 = (in1 * 3784 + 2048) >> 12;
    } else {
        const int in2 = c[2 * stride], in3 = c[3 * stride];

        t0 = ((in0 + in2) * 181 + 128) >> 8;
        t1 = ((in0 - in2) * 181 + 128) >> 8;
        t2 = ((in1 *  1567         - in3 * (3784 - 4096) + 2048) >> 12) - in3;
        t3 = ((in1 * (3784 - 4096) + in3 *  1567         + 2048) >> 12) + in1;
    }

    c[0 * stride] = CLIP(t0 + t3);
    c[1 * stride] = CLIP(t1 + t2);
    c[2 * stride] = CLIP(t1 - t2);
    c[3 * stride] = CLIP(t0 - t3);
}

void dav1d_inv_dct4_1d_c(int32_t *const c, const ptrdiff_t stride,
                         const int min, const int max)
{
    inv_dct4_1d_internal_c(c, stride, min, max, 0);
}

static NOINLINE void
inv_dct8_1d_internal_c(int32_t *const c, const ptrdiff_t stride,
                       const int min, const int max, const int tx64)
{
    assert(stride > 0);
    inv_dct4_1d_internal_c(c, stride << 1, min, max, tx64);

    const int in1 = c[1 * stride], in3 = c[3 * stride];

    int t4a, t5a, t6a, t7a;
    if (tx64) {
        t4a = (in1 *   799 + 2048) >> 12;
        t5a = (in3 * -2276 + 2048) >> 12;
        t6a = (in3 *  3406 + 2048) >> 12;
        t7a = (in1 *  4017 + 2048) >> 12;
    } else {
        const int in5 = c[5 * stride], in7 = c[7 * stride];

        t4a = ((in1 *   799         - in7 * (4017 - 4096) + 2048) >> 12) - in7;
        t5a =  (in5 *  1703         - in3 *  1138         + 1024) >> 11;
        t6a =  (in5 *  1138         + in3 *  1703         + 1024) >> 11;
        t7a = ((in1 * (4017 - 4096) + in7 *  799          + 2048) >> 12) + in1;
    }

    const int t4  = CLIP(t4a + t5a);
              t5a = CLIP(t4a - t5a);
    const int t7  = CLIP(t7a + t6a);
              t6a = CLIP(t7a - t6a);

    const int t5  = ((t6a - t5a) * 181 + 128) >> 8;
    const int t6  = ((t6a + t5a) * 181 + 128) >> 8;

    const int t0 = c[0 * stride];
    const int t1 = c[2 * stride];
    const int t2 = c[4 * stride];
    const int t3 = c[6 * stride];

    c[0 * stride] = CLIP(t0 + t7);
    c[1 * stride] = CLIP(t1 + t6);
    c[2 * stride] = CLIP(t2 + t5);
    c[3 * stride] = CLIP(t3 + t4);
    c[4 * stride] = CLIP(t3 - t4);
    c[5 * stride] = CLIP(t2 - t5);
    c[6 * stride] = CLIP(t1 - t6);
    c[7 * stride] = CLIP(t0 - t7);
}

void dav1d_inv_dct8_1d_c(int32_t *const c, const ptrdiff_t stride,
                         const int min, const int max)
{
    inv_dct8_1d_internal_c(c, stride, min, max, 0);
}

static NOINLINE void
inv_dct16_1d_internal_c(int32_t *const c, const ptrdiff_t stride,
                        const int min, const int max, int tx64)
{
    assert(stride > 0);
    inv_dct8_1d_internal_c(c, stride << 1, min, max, tx64);

    const int in1 = c[1 * stride], in3 = c[3 * stride];
    const int in5 = c[5 * stride], in7 = c[7 * stride];

    int t8a, t9a, t10a, t11a, t12a, t13a, t14a, t15a;
    if (tx64) {
        t8a  = (in1 *   401 + 2048) >> 12;
        t9a  = (in7 * -2598 + 2048) >> 12;
        t10a = (in5 *  1931 + 2048) >> 12;
        t11a = (in3 * -1189 + 2048) >> 12;
        t12a = (in3 *  3920 + 2048) >> 12;
        t13a = (in5 *  3612 + 2048) >> 12;
        t14a = (in7 *  3166 + 2048) >> 12;
        t15a = (in1 *  4076 + 2048) >> 12;
    } else {
        const int in9  = c[ 9 * stride], in11 = c[11 * stride];
        const int in13 = c[13 * stride], in15 = c[15 * stride];

        t8a  = ((in1  *   401         - in15 * (4076 - 4096) + 2048) >> 12) - in15;
        t9a  =  (in9  *  1583         - in7  *  1299         + 1024) >> 11;
        t10a = ((in5  *  1931         - in11 * (3612 - 4096) + 2048) >> 12) - in11;
        t11a = ((in13 * (3920 - 4096) - in3  *  1189         + 2048) >> 12) + in13;
        t12a = ((in13 *  1189         + in3  * (3920 - 4096) + 2048) >> 12) + in3;
        t13a = ((in5  * (3612 - 4096) + in11 *  1931         + 2048) >> 12) + in5;
        t14a =  (in9  *  1299         + in7  *  1583         + 1024) >> 11;
        t15a = ((in1  * (4076 - 4096) + in15 *   401         + 2048) >> 12) + in1;
    }

    int t8  = CLIP(t8a  + t9a);
    int t9  = CLIP(t8a  - t9a);
    int t10 = CLIP(t11a - t10a);
    int t11 = CLIP(t11a + t10a);
    int t12 = CLIP(t12a + t13a);
    int t13 = CLIP(t12a - t13a);
    int t14 = CLIP(t15a - t14a);
    int t15 = CLIP(t15a + t14a);

    t9a  = ((  t14 *  1567         - t9  * (3784 - 4096)  + 2048) >> 12) - t9;
    t14a = ((  t14 * (3784 - 4096) + t9  *  1567          + 2048) >> 12) + t14;
    t10a = ((-(t13 * (3784 - 4096) + t10 *  1567)         + 2048) >> 12) - t13;
    t13a = ((  t13 *  1567         - t10 * (3784 - 4096)  + 2048) >> 12) - t10;

    t8a  = CLIP(t8   + t11);
    t9   = CLIP(t9a  + t10a);
    t10  = CLIP(t9a  - t10a);
    t11a = CLIP(t8   - t11);
    t12a = CLIP(t15  - t12);
    t13  = CLIP(t14a - t13a);
    t14  = CLIP(t14a + t13a);
    t15a = CLIP(t15  + t12);

    t10a = ((t13  - t10)  * 181 + 128) >> 8;
    t13a = ((t13  + t10)  * 181 + 128) >> 8;
    t11  = ((t12a - t11a) * 181 + 128) >> 8;
    t12  = ((t12a + t11a) * 181 + 128) >> 8;

    const int t0 = c[ 0 * stride];
    const int t1 = c[ 2 * stride];
    const int t2 = c[ 4 * stride];
    const int t3 = c[ 6 * stride];
    const int t4 = c[ 8 * stride];
    const int t5 = c[10 * stride];
    const int t6 = c[12 * stride];
    const int t7 = c[14 * stride];

    c[ 0 * stride] = CLIP(t0 + t15a);
    c[ 1 * stride] = CLIP(t1 + t14);
    c[ 2 * stride] = CLIP(t2 + t13a);
    c[ 3 * stride] = CLIP(t3 + t12);
    c[ 4 * stride] = CLIP(t4 + t11);
    c[ 5 * stride] = CLIP(t5 + t10a);
    c[ 6 * stride] = CLIP(t6 + t9);
    c[ 7 * stride] = CLIP(t7 + t8a);
    c[ 8 * stride] = CLIP(t7 - t8a);
    c[ 9 * stride] = CLIP(t6 - t9);
    c[10 * stride] = CLIP(t5 - t10a);
    c[11 * stride] = CLIP(t4 - t11);
    c[12 * stride] = CLIP(t3 - t12);
    c[13 * stride] = CLIP(t2 - t13a);
    c[14 * stride] = CLIP(t1 - t14);
    c[15 * stride] = CLIP(t0 - t15a);
}

void dav1d_inv_dct16_1d_c(int32_t *const c, const ptrdiff_t stride,
                          const int min, const int max)
{
    inv_dct16_1d_internal_c(c, stride, min, max, 0);
}

static NOINLINE void
inv_dct32_1d_internal_c(int32_t *const c, const ptrdiff_t stride,
                        const int min, const int max, const int tx64)
{
    assert(stride > 0);
    inv_dct16_1d_internal_c(c, stride << 1, min, max, tx64);

    const int in1  = c[ 1 * stride], in3  = c[ 3 * stride];
    const int in5  = c[ 5 * stride], in7  = c[ 7 * stride];
    const int in9  = c[ 9 * stride], in11 = c[11 * stride];
    const int in13 = c[13 * stride], in15 = c[15 * stride];

    int t16a, t17a, t18a, t19a, t20a, t21a, t22a, t23a;
    int t24a, t25a, t26a, t27a, t28a, t29a, t30a, t31a;
    if (tx64) {
        t16a = (in1  *   201 + 2048) >> 12;
        t17a = (in15 * -2751 + 2048) >> 12;
        t18a = (in9  *  1751 + 2048) >> 12;
        t19a = (in7  * -1380 + 2048) >> 12;
        t20a = (in5  *   995 + 2048) >> 12;
        t21a = (in11 * -2106 + 2048) >> 12;
        t22a = (in13 *  2440 + 2048) >> 12;
        t23a = (in3  *  -601 + 2048) >> 12;
        t24a = (in3  *  4052 + 2048) >> 12;
        t25a = (in13 *  3290 + 2048) >> 12;
        t26a = (in11 *  3513 + 2048) >> 12;
        t27a = (in5  *  3973 + 2048) >> 12;
        t28a = (in7  *  3857 + 2048) >> 12;
        t29a = (in9  *  3703 + 2048) >> 12;
        t30a = (in15 *  3035 + 2048) >> 12;
        t31a = (in1  *  4091 + 2048) >> 12;
    } else {
        const int in17 = c[17 * stride], in19 = c[19 * stride];
        const int in21 = c[21 * stride], in23 = c[23 * stride];
        const int in25 = c[25 * stride], in27 = c[27 * stride];
        const int in29 = c[29 * stride], in31 = c[31 * stride];

        t16a = ((in1  *   201         - in31 * (4091 - 4096) + 2048) >> 12) - in31;
        t17a = ((in17 * (3035 - 4096) - in15 *  2751         + 2048) >> 12) + in17;
        t18a = ((in9  *  1751         - in23 * (3703 - 4096) + 2048) >> 12) - in23;
        t19a = ((in25 * (3857 - 4096) - in7  *  1380         + 2048) >> 12) + in25;
        t20a = ((in5  *   995         - in27 * (3973 - 4096) + 2048) >> 12) - in27;
        t21a = ((in21 * (3513 - 4096) - in11 *  2106         + 2048) >> 12) + in21;
        t22a =  (in13 *  1220         - in19 *  1645         + 1024) >> 11;
        t23a = ((in29 * (4052 - 4096) - in3  *   601         + 2048) >> 12) + in29;
        t24a = ((in29 *   601         + in3  * (4052 - 4096) + 2048) >> 12) + in3;
        t25a =  (in13 *  1645         + in19 *  1220         + 1024) >> 11;
        t26a = ((in21 *  2106         + in11 * (3513 - 4096) + 2048) >> 12) + in11;
        t27a = ((in5  * (3973 - 4096) + in27 *   995         + 2048) >> 12) + in5;
        t28a = ((in25 *  1380         + in7  * (3857 - 4096) + 2048) >> 12) + in7;
        t29a = ((in9  * (3703 - 4096) + in23 *  1751         + 2048) >> 12) + in9;
        t30a = ((in17 *  2751         + in15 * (3035 - 4096) + 2048) >> 12) + in15;
        t31a = ((in1  * (4091 - 4096) + in31 *   201         + 2048) >> 12) + in1;
    }

    int t16 = CLIP(t16a + t17a);
    int t17 = CLIP(t16a - t17a);
    int t18 = CLIP(t19a - t18a);
    int t19 = CLIP(t19a + t18a);
    int t20 = CLIP(t20a + t21a);
    int t21 = CLIP(t20a - t21a);
    int t22 = CLIP(t23a - t22a);
    int t23 = CLIP(t23a + t22a);
    int t24 = CLIP(t24a + t25a);
    int t25 = CLIP(t24a - t25a);
    int t26 = CLIP(t27a - t26a);
    int t27 = CLIP(t27a + t26a);
    int t28 = CLIP(t28a + t29a);
    int t29 = CLIP(t28a - t29a);
    int t30 = CLIP(t31a - t30a);
    int t31 = CLIP(t31a + t30a);

    t17a = ((  t30 *   799         - t17 * (4017 - 4096)  + 2048) >> 12) - t17;
    t30a = ((  t30 * (4017 - 4096) + t17 *   799          + 2048) >> 12) + t30;
    t18a = ((-(t29 * (4017 - 4096) + t18 *   799)         + 2048) >> 12) - t29;
    t29a = ((  t29 *   799         - t18 * (4017 - 4096)  + 2048) >> 12) - t18;
    t21a =  (  t26 *  1703         - t21 *  1138          + 1024) >> 11;
    t26a =  (  t26 *  1138         + t21 *  1703          + 1024) >> 11;
    t22a =  (-(t25 *  1138         + t22 *  1703        ) + 1024) >> 11;
    t25a =  (  t25 *  1703         - t22 *  1138          + 1024) >> 11;

    t16a = CLIP(t16  + t19);
    t17  = CLIP(t17a + t18a);
    t18  = CLIP(t17a - t18a);
    t19a = CLIP(t16  - t19);
    t20a = CLIP(t23  - t20);
    t21  = CLIP(t22a - t21a);
    t22  = CLIP(t22a + t21a);
    t23a = CLIP(t23  + t20);
    t24a = CLIP(t24  + t27);
    t25  = CLIP(t25a + t26a);
    t26  = CLIP(t25a - t26a);
    t27a = CLIP(t24  - t27);
    t28a = CLIP(t31  - t28);
    t29  = CLIP(t30a - t29a);
    t30  = CLIP(t30a + t29a);
    t31a = CLIP(t31  + t28);

    t18a = ((  t29  *  1567         - t18  * (3784 - 4096)  + 2048) >> 12) - t18;
    t29a = ((  t29  * (3784 - 4096) + t18  *  1567          + 2048) >> 12) + t29;
    t19  = ((  t28a *  1567         - t19a * (3784 - 4096)  + 2048) >> 12) - t19a;
    t28  = ((  t28a * (3784 - 4096) + t19a *  1567          + 2048) >> 12) + t28a;
    t20  = ((-(t27a * (3784 - 4096) + t20a *  1567)         + 2048) >> 12) - t27a;
    t27  = ((  t27a *  1567         - t20a * (3784 - 4096)  + 2048) >> 12) - t20a;
    t21a = ((-(t26  * (3784 - 4096) + t21  *  1567)         + 2048) >> 12) - t26;
    t26a = ((  t26  *  1567         - t21  * (3784 - 4096)  + 2048) >> 12) - t21;

    t16  = CLIP(t16a + t23a);
    t17a = CLIP(t17  + t22);
    t18  = CLIP(t18a + t21a);
    t19a = CLIP(t19  + t20);
    t20a = CLIP(t19  - t20);
    t21  = CLIP(t18a - t21a);
    t22a = CLIP(t17  - t22);
    t23  = CLIP(t16a - t23a);
    t24  = CLIP(t31a - t24a);
    t25a = CLIP(t30  - t25);
    t26  = CLIP(t29a - t26a);
    t27a = CLIP(t28  - t27);
    t28a = CLIP(t28  + t27);
    t29  = CLIP(t29a + t26a);
    t30a = CLIP(t30  + t25);
    t31  = CLIP(t31a + t24a);

    t20  = ((t27a - t20a) * 181 + 128) >> 8;
    t27  = ((t27a + t20a) * 181 + 128) >> 8;
    t21a = ((t26  - t21 ) * 181 + 128) >> 8;
    t26a = ((t26  + t21 ) * 181 + 128) >> 8;
    t22  = ((t25a - t22a) * 181 + 128) >> 8;
    t25  = ((t25a + t22a) * 181 + 128) >> 8;
    t23a = ((t24  - t23 ) * 181 + 128) >> 8;
    t24a = ((t24  + t23 ) * 181 + 128) >> 8;

    const int t0  = c[ 0 * stride];
    const int t1  = c[ 2 * stride];
    const int t2  = c[ 4 * stride];
    const int t3  = c[ 6 * stride];
    const int t4  = c[ 8 * stride];
    const int t5  = c[10 * stride];
    const int t6  = c[12 * stride];
    const int t7  = c[14 * stride];
    const int t8  = c[16 * stride];
    const int t9  = c[18 * stride];
    const int t10 = c[20 * stride];
    const int t11 = c[22 * stride];
    const int t12 = c[24 * stride];
    const int t13 = c[26 * stride];
    const int t14 = c[28 * stride];
    const int t15 = c[30 * stride];

    c[ 0 * stride] = CLIP(t0  + t31);
    c[ 1 * stride] = CLIP(t1  + t30a);
    c[ 2 * stride] = CLIP(t2  + t29);
    c[ 3 * stride] = CLIP(t3  + t28a);
    c[ 4 * stride] = CLIP(t4  + t27);
    c[ 5 * stride] = CLIP(t5  + t26a);
    c[ 6 * stride] = CLIP(t6  + t25);
    c[ 7 * stride] = CLIP(t7  + t24a);
    c[ 8 * stride] = CLIP(t8  + t23a);
    c[ 9 * stride] = CLIP(t9  + t22);
    c[10 * stride] = CLIP(t10 + t21a);
    c[11 * stride] = CLIP(t11 + t20);
    c[12 * stride] = CLIP(t12 + t19a);
    c[13 * stride] = CLIP(t13 + t18);
    c[14 * stride] = CLIP(t14 + t17a);
    c[15 * stride] = CLIP(t15 + t16);
    c[16 * stride] = CLIP(t15 - t16);
    c[17 * stride] = CLIP(t14 - t17a);
    c[18 * stride] = CLIP(t13 - t18);
    c[19 * stride] = CLIP(t12 - t19a);
    c[20 * stride] = CLIP(t11 - t20);
    c[21 * stride] = CLIP(t10 - t21a);
    c[22 * stride] = CLIP(t9  - t22);
    c[23 * stride] = CLIP(t8  - t23a);
    c[24 * stride] = CLIP(t7  - t24a);
    c[25 * stride] = CLIP(t6  - t25);
    c[26 * stride] = CLIP(t5  - t26a);
    c[27 * stride] = CLIP(t4  - t27);
    c[28 * stride] = CLIP(t3  - t28a);
    c[29 * stride] = CLIP(t2  - t29);
    c[30 * stride] = CLIP(t1  - t30a);
    c[31 * stride] = CLIP(t0  - t31);
}

void dav1d_inv_dct32_1d_c(int32_t *const c, const ptrdiff_t stride,
                          const int min, const int max)
{
    inv_dct32_1d_internal_c(c, stride, min, max, 0);
}

void dav1d_inv_dct64_1d_c(int32_t *const c, const ptrdiff_t stride,
                          const int min, const int max)
{
    assert(stride > 0);
    inv_dct32_1d_internal_c(c, stride << 1, min, max, 1);

    const int in1  = c[ 1 * stride], in3  = c[ 3 * stride];
    const int in5  = c[ 5 * stride], in7  = c[ 7 * stride];
    const int in9  = c[ 9 * stride], in11 = c[11 * stride];
    const int in13 = c[13 * stride], in15 = c[15 * stride];
    const int in17 = c[17 * stride], in19 = c[19 * stride];
    const int in21 = c[21 * stride], in23 = c[23 * stride];
    const int in25 = c[25 * stride], in27 = c[27 * stride];
    const int in29 = c[29 * stride], in31 = c[31 * stride];

    int t32a = (in1  *   101 + 2048) >> 12;
    int t33a = (in31 * -2824 + 2048) >> 12;
    int t34a = (in17 *  1660 + 2048) >> 12;
    int t35a = (in15 * -1474 + 2048) >> 12;
    int t36a = (in9  *   897 + 2048) >> 12;
    int t37a = (in23 * -2191 + 2048) >> 12;
    int t38a = (in25 *  2359 + 2048) >> 12;
    int t39a = (in7  *  -700 + 2048) >> 12;
    int t40a = (in5  *   501 + 2048) >> 12;
    int t41a = (in27 * -2520 + 2048) >> 12;
    int t42a = (in21 *  2019 + 2048) >> 12;
    int t43a = (in11 * -1092 + 2048) >> 12;
    int t44a = (in13 *  1285 + 2048) >> 12;
    int t45a = (in19 * -1842 + 2048) >> 12;
    int t46a = (in29 *  2675 + 2048) >> 12;
    int t47a = (in3  *  -301 + 2048) >> 12;
    int t48a = (in3  *  4085 + 2048) >> 12;
    int t49a = (in29 *  3102 + 2048) >> 12;
    int t50a = (in19 *  3659 + 2048) >> 12;
    int t51a = (in13 *  3889 + 2048) >> 12;
    int t52a = (in11 *  3948 + 2048) >> 12;
    int t53a = (in21 *  3564 + 2048) >> 12;
    int t54a = (in27 *  3229 + 2048) >> 12;
    int t55a = (in5  *  4065 + 2048) >> 12;
    int t56a = (in7  *  4036 + 2048) >> 12;
    int t57a = (in25 *  3349 + 2048) >> 12;
    int t58a = (in23 *  3461 + 2048) >> 12;
    int t59a = (in9  *  3996 + 2048) >> 12;
    int t60a = (in15 *  3822 + 2048) >> 12;
    int t61a = (in17 *  3745 + 2048) >> 12;
    int t62a = (in31 *  2967 + 2048) >> 12;
    int t63a = (in1  *  4095 + 2048) >> 12;

    int t32 = CLIP(t32a + t33a);
    int t33 = CLIP(t32a - t33a);
    int t34 = CLIP(t35a - t34a);
    int t35 = CLIP(t35a + t34a);
    int t36 = CLIP(t36a + t37a);
    int t37 = CLIP(t36a - t37a);
    int t38 = CLIP(t39a - t38a);
    int t39 = CLIP(t39a + t38a);
    int t40 = CLIP(t40a + t41a);
    int t41 = CLIP(t40a - t41a);
    int t42 = CLIP(t43a - t42a);
    int t43 = CLIP(t43a + t42a);
    int t44 = CLIP(t44a + t45a);
    int t45 = CLIP(t44a - t45a);
    int t46 = CLIP(t47a - t46a);
    int t47 = CLIP(t47a + t46a);
    int t48 = CLIP(t48a + t49a);
    int t49 = CLIP(t48a - t49a);
    int t50 = CLIP(t51a - t50a);
    int t51 = CLIP(t51a + t50a);
    int t52 = CLIP(t52a + t53a);
    int t53 = CLIP(t52a - t53a);
    int t54 = CLIP(t55a - t54a);
    int t55 = CLIP(t55a + t54a);
    int t56 = CLIP(t56a + t57a);
    int t57 = CLIP(t56a - t57a);
    int t58 = CLIP(t59a - t58a);
    int t59 = CLIP(t59a + t58a);
    int t60 = CLIP(t60a + t61a);
    int t61 = CLIP(t60a - t61a);
    int t62 = CLIP(t63a - t62a);
    int t63 = CLIP(t63a + t62a);

    t33a = ((t33 * (4096 - 4076) + t62 *   401         + 2048) >> 12) - t33;
    t34a = ((t34 *  -401         + t61 * (4096 - 4076) + 2048) >> 12) - t61;
    t37a =  (t37 * -1299         + t58 *  1583         + 1024) >> 11;
    t38a =  (t38 * -1583         + t57 * -1299         + 1024) >> 11;
    t41a = ((t41 * (4096 - 3612) + t54 *  1931         + 2048) >> 12) - t41;
    t42a = ((t42 * -1931         + t53 * (4096 - 3612) + 2048) >> 12) - t53;
    t45a = ((t45 * -1189         + t50 * (3920 - 4096) + 2048) >> 12) + t50;
    t46a = ((t46 * (4096 - 3920) + t49 * -1189         + 2048) >> 12) - t46;
    t49a = ((t46 * -1189         + t49 * (3920 - 4096) + 2048) >> 12) + t49;
    t50a = ((t45 * (3920 - 4096) + t50 *  1189         + 2048) >> 12) + t45;
    t53a = ((t42 * (4096 - 3612) + t53 *  1931         + 2048) >> 12) - t42;
    t54a = ((t41 *  1931         + t54 * (3612 - 4096) + 2048) >> 12) + t54;
    t57a =  (t38 * -1299         + t57 *  1583         + 1024) >> 11;
    t58a =  (t37 *  1583         + t58 *  1299         + 1024) >> 11;
    t61a = ((t34 * (4096 - 4076) + t61 *   401         + 2048) >> 12) - t34;
    t62a = ((t33 *   401         + t62 * (4076 - 4096) + 2048) >> 12) + t62;

    t32a = CLIP(t32  + t35);
    t33  = CLIP(t33a + t34a);
    t34  = CLIP(t33a - t34a);
    t35a = CLIP(t32  - t35);
    t36a = CLIP(t39  - t36);
    t37  = CLIP(t38a - t37a);
    t38  = CLIP(t38a + t37a);
    t39a = CLIP(t39  + t36);
    t40a = CLIP(t40  + t43);
    t41  = CLIP(t41a + t42a);
    t42  = CLIP(t41a - t42a);
    t43a = CLIP(t40  - t43);
    t44a = CLIP(t47  - t44);
    t45  = CLIP(t46a - t45a);
    t46  = CLIP(t46a + t45a);
    t47a = CLIP(t47  + t44);
    t48a = CLIP(t48  + t51);
    t49  = CLIP(t49a + t50a);
    t50  = CLIP(t49a - t50a);
    t51a = CLIP(t48  - t51);
    t52a = CLIP(t55  - t52);
    t53  = CLIP(t54a - t53a);
    t54  = CLIP(t54a + t53a);
    t55a = CLIP(t55  + t52);
    t56a = CLIP(t56  + t59);
    t57  = CLIP(t57a + t58a);
    t58  = CLIP(t57a - t58a);
    t59a = CLIP(t56  - t59);
    t60a = CLIP(t63  - t60);
    t61  = CLIP(t62a - t61a);
    t62  = CLIP(t62a + t61a);
    t63a = CLIP(t63  + t60);

    t34a = ((t34  * (4096 - 4017) + t61  *   799         + 2048) >> 12) - t34;
    t35  = ((t35a * (4096 - 4017) + t60a *   799         + 2048) >> 12) - t35a;
    t36  = ((t36a *  -799         + t59a * (4096 - 4017) + 2048) >> 12) - t59a;
    t37a = ((t37  *  -799         + t58  * (4096 - 4017) + 2048) >> 12) - t58;
    t42a =  (t42  * -1138         + t53  *  1703         + 1024) >> 11;
    t43  =  (t43a * -1138         + t52a *  1703         + 1024) >> 11;
    t44  =  (t44a * -1703         + t51a * -1138         + 1024) >> 11;
    t45a =  (t45  * -1703         + t50  * -1138         + 1024) >> 11;
    t50a =  (t45  * -1138         + t50  *  1703         + 1024) >> 11;
    t51  =  (t44a * -1138         + t51a *  1703         + 1024) >> 11;
    t52  =  (t43a *  1703         + t52a *  1138         + 1024) >> 11;
    t53a =  (t42  *  1703         + t53  *  1138         + 1024) >> 11;
    t58a = ((t37  * (4096 - 4017) + t58  *   799         + 2048) >> 12) - t37;
    t59  = ((t36a * (4096 - 4017) + t59a *   799         + 2048) >> 12) - t36a;
    t60  = ((t35a *   799         + t60a * (4017 - 4096) + 2048) >> 12) + t60a;
    t61a = ((t34  *   799         + t61  * (4017 - 4096) + 2048) >> 12) + t61;

    t32  = CLIP(t32a + t39a);
    t33a = CLIP(t33  + t38);
    t34  = CLIP(t34a + t37a);
    t35a = CLIP(t35  + t36);
    t36a = CLIP(t35  - t36);
    t37  = CLIP(t34a - t37a);
    t38a = CLIP(t33  - t38);
    t39  = CLIP(t32a - t39a);
    t40  = CLIP(t47a - t40a);
    t41a = CLIP(t46  - t41);
    t42  = CLIP(t45a - t42a);
    t43a = CLIP(t44  - t43);
    t44a = CLIP(t44  + t43);
    t45  = CLIP(t45a + t42a);
    t46a = CLIP(t46  + t41);
    t47  = CLIP(t47a + t40a);
    t48  = CLIP(t48a + t55a);
    t49a = CLIP(t49  + t54);
    t50  = CLIP(t50a + t53a);
    t51a = CLIP(t51  + t52);
    t52a = CLIP(t51  - t52);
    t53  = CLIP(t50a - t53a);
    t54a = CLIP(t49  - t54);
    t55  = CLIP(t48a - t55a);
    t56  = CLIP(t63a - t56a);
    t57a = CLIP(t62  - t57);
    t58  = CLIP(t61a - t58a);
    t59a = CLIP(t60  - t59);
    t60a = CLIP(t60  + t59);
    t61  = CLIP(t61a + t58a);
    t62a = CLIP(t62  + t57);
    t63  = CLIP(t63a + t56a);

    t36  = ((t36a * (4096 - 3784) + t59a *  1567         + 2048) >> 12) - t36a;
    t37a = ((t37  * (4096 - 3784) + t58  *  1567         + 2048) >> 12) - t37;
    t38  = ((t38a * (4096 - 3784) + t57a *  1567         + 2048) >> 12) - t38a;
    t39a = ((t39  * (4096 - 3784) + t56  *  1567         + 2048) >> 12) - t39;
    t40a = ((t40  * -1567         + t55  * (4096 - 3784) + 2048) >> 12) - t55;
    t41  = ((t41a * -1567         + t54a * (4096 - 3784) + 2048) >> 12) - t54a;
    t42a = ((t42  * -1567         + t53  * (4096 - 3784) + 2048) >> 12) - t53;
    t43  = ((t43a * -1567         + t52a * (4096 - 3784) + 2048) >> 12) - t52a;
    t52  = ((t43a * (4096 - 3784) + t52a *  1567         + 2048) >> 12) - t43a;
    t53a = ((t42  * (4096 - 3784) + t53  *  1567         + 2048) >> 12) - t42;
    t54  = ((t41a * (4096 - 3784) + t54a *  1567         + 2048) >> 12) - t41a;
    t55a = ((t40  * (4096 - 3784) + t55  *  1567         + 2048) >> 12) - t40;
    t56a = ((t39  *  1567         + t56  * (3784 - 4096) + 2048) >> 12) + t56;
    t57  = ((t38a *  1567         + t57a * (3784 - 4096) + 2048) >> 12) + t57a;
    t58a = ((t37  *  1567         + t58  * (3784 - 4096) + 2048) >> 12) + t58;
    t59  = ((t36a *  1567         + t59a * (3784 - 4096) + 2048) >> 12) + t59a;

    t32a = CLIP(t32  + t47);
    t33  = CLIP(t33a + t46a);
    t34a = CLIP(t34  + t45);
    t35  = CLIP(t35a + t44a);
    t36a = CLIP(t36  + t43);
    t37  = CLIP(t37a + t42a);
    t38a = CLIP(t38  + t41);
    t39  = CLIP(t39a + t40a);
    t40  = CLIP(t39a - t40a);
    t41a = CLIP(t38  - t41);
    t42  = CLIP(t37a - t42a);
    t43a = CLIP(t36  - t43);
    t44  = CLIP(t35a - t44a);
    t45a = CLIP(t34  - t45);
    t46  = CLIP(t33a - t46a);
    t47a = CLIP(t32  - t47);
    t48a = CLIP(t63  - t48);
    t49  = CLIP(t62a - t49a);
    t50a = CLIP(t61  - t50);
    t51  = CLIP(t60a - t51a);
    t52a = CLIP(t59  - t52);
    t53  = CLIP(t58a - t53a);
    t54a = CLIP(t57  - t54);
    t55  = CLIP(t56a - t55a);
    t56  = CLIP(t56a + t55a);
    t57a = CLIP(t57  + t54);
    t58  = CLIP(t58a + t53a);
    t59a = CLIP(t59  + t52);
    t60  = CLIP(t60a + t51a);
    t61a = CLIP(t61  + t50);
    t62  = CLIP(t62a + t49a);
    t63a = CLIP(t63  + t48);

    t40a = ((t55  - t40 ) * 181 + 128) >> 8;
    t41  = ((t54a - t41a) * 181 + 128) >> 8;
    t42a = ((t53  - t42 ) * 181 + 128) >> 8;
    t43  = ((t52a - t43a) * 181 + 128) >> 8;
    t44a = ((t51  - t44 ) * 181 + 128) >> 8;
    t45  = ((t50a - t45a) * 181 + 128) >> 8;
    t46a = ((t49  - t46 ) * 181 + 128) >> 8;
    t47  = ((t48a - t47a) * 181 + 128) >> 8;
    t48  = ((t47a + t48a) * 181 + 128) >> 8;
    t49a = ((t46  + t49 ) * 181 + 128) >> 8;
    t50  = ((t45a + t50a) * 181 + 128) >> 8;
    t51a = ((t44  + t51 ) * 181 + 128) >> 8;
    t52  = ((t43a + t52a) * 181 + 128) >> 8;
    t53a = ((t42  + t53 ) * 181 + 128) >> 8;
    t54  = ((t41a + t54a) * 181 + 128) >> 8;
    t55a = ((t40  + t55 ) * 181 + 128) >> 8;

    const int t0  = c[ 0 * stride];
    const int t1  = c[ 2 * stride];
    const int t2  = c[ 4 * stride];
    const int t3  = c[ 6 * stride];
    const int t4  = c[ 8 * stride];
    const int t5  = c[10 * stride];
    const int t6  = c[12 * stride];
    const int t7  = c[14 * stride];
    const int t8  = c[16 * stride];
    const int t9  = c[18 * stride];
    const int t10 = c[20 * stride];
    const int t11 = c[22 * stride];
    const int t12 = c[24 * stride];
    const int t13 = c[26 * stride];
    const int t14 = c[28 * stride];
    const int t15 = c[30 * stride];
    const int t16 = c[32 * stride];
    const int t17 = c[34 * stride];
    const int t18 = c[36 * stride];
    const int t19 = c[38 * stride];
    const int t20 = c[40 * stride];
    const int t21 = c[42 * stride];
    const int t22 = c[44 * stride];
    const int t23 = c[46 * stride];
    const int t24 = c[48 * stride];
    const int t25 = c[50 * stride];
    const int t26 = c[52 * stride];
    const int t27 = c[54 * stride];
    const int t28 = c[56 * stride];
    const int t29 = c[58 * stride];
    const int t30 = c[60 * stride];
    const int t31 = c[62 * stride];

    c[ 0 * stride] = CLIP(t0  + t63a);
    c[ 1 * stride] = CLIP(t1  + t62);
    c[ 2 * stride] = CLIP(t2  + t61a);
    c[ 3 * stride] = CLIP(t3  + t60);
    c[ 4 * stride] = CLIP(t4  + t59a);
    c[ 5 * stride] = CLIP(t5  + t58);
    c[ 6 * stride] = CLIP(t6  + t57a);
    c[ 7 * stride] = CLIP(t7  + t56);
    c[ 8 * stride] = CLIP(t8  + t55a);
    c[ 9 * stride] = CLIP(t9  + t54);
    c[10 * stride] = CLIP(t10 + t53a);
    c[11 * stride] = CLIP(t11 + t52);
    c[12 * stride] = CLIP(t12 + t51a);
    c[13 * stride] = CLIP(t13 + t50);
    c[14 * stride] = CLIP(t14 + t49a);
    c[15 * stride] = CLIP(t15 + t48);
    c[16 * stride] = CLIP(t16 + t47);
    c[17 * stride] = CLIP(t17 + t46a);
    c[18 * stride] = CLIP(t18 + t45);
    c[19 * stride] = CLIP(t19 + t44a);
    c[20 * stride] = CLIP(t20 + t43);
    c[21 * stride] = CLIP(t21 + t42a);
    c[22 * stride] = CLIP(t22 + t41);
    c[23 * stride] = CLIP(t23 + t40a);
    c[24 * stride] = CLIP(t24 + t39);
    c[25 * stride] = CLIP(t25 + t38a);
    c[26 * stride] = CLIP(t26 + t37);
    c[27 * stride] = CLIP(t27 + t36a);
    c[28 * stride] = CLIP(t28 + t35);
    c[29 * stride] = CLIP(t29 + t34a);
    c[30 * stride] = CLIP(t30 + t33);
    c[31 * stride] = CLIP(t31 + t32a);
    c[32 * stride] = CLIP(t31 - t32a);
    c[33 * stride] = CLIP(t30 - t33);
    c[34 * stride] = CLIP(t29 - t34a);
    c[35 * stride] = CLIP(t28 - t35);
    c[36 * stride] = CLIP(t27 - t36a);
    c[37 * stride] = CLIP(t26 - t37);
    c[38 * stride] = CLIP(t25 - t38a);
    c[39 * stride] = CLIP(t24 - t39);
    c[40 * stride] = CLIP(t23 - t40a);
    c[41 * stride] = CLIP(t22 - t41);
    c[42 * stride] = CLIP(t21 - t42a);
    c[43 * stride] = CLIP(t20 - t43);
    c[44 * stride] = CLIP(t19 - t44a);
    c[45 * stride] = CLIP(t18 - t45);
    c[46 * stride] = CLIP(t17 - t46a);
    c[47 * stride] = CLIP(t16 - t47);
    c[48 * stride] = CLIP(t15 - t48);
    c[49 * stride] = CLIP(t14 - t49a);
    c[50 * stride] = CLIP(t13 - t50);
    c[51 * stride] = CLIP(t12 - t51a);
    c[52 * stride] = CLIP(t11 - t52);
    c[53 * stride] = CLIP(t10 - t53a);
    c[54 * stride] = CLIP(t9  - t54);
    c[55 * stride] = CLIP(t8  - t55a);
    c[56 * stride] = CLIP(t7  - t56);
    c[57 * stride] = CLIP(t6  - t57a);
    c[58 * stride] = CLIP(t5  - t58);
    c[59 * stride] = CLIP(t4  - t59a);
    c[60 * stride] = CLIP(t3  - t60);
    c[61 * stride] = CLIP(t2  - t61a);
    c[62 * stride] = CLIP(t1  - t62);
    c[63 * stride] = CLIP(t0  - t63a);
}

static NOINLINE void
inv_adst4_1d_internal_c(const int32_t *const in, const ptrdiff_t in_s,
                        const int min, const int max,
                        int32_t *const out, const ptrdiff_t out_s)
{
    assert(in_s > 0 && out_s != 0);
    const int in0 = in[0 * in_s], in1 = in[1 * in_s];
    const int in2 = in[2 * in_s], in3 = in[3 * in_s];

    out[0 * out_s] = (( 1321         * in0 + (3803 - 4096) * in2 +
                       (2482 - 4096) * in3 + (3344 - 4096) * in1 + 2048) >> 12) +
                     in2 + in3 + in1;
    out[1 * out_s] = (((2482 - 4096) * in0 -  1321         * in2 -
                       (3803 - 4096) * in3 + (3344 - 4096) * in1 + 2048) >> 12) +
                     in0 - in3 + in1;
    out[2 * out_s] = (209 * (in0 - in2 + in3) + 128) >> 8;
    out[3 * out_s] = (((3803 - 4096) * in0 + (2482 - 4096) * in2 -
                        1321         * in3 - (3344 - 4096) * in1 + 2048) >> 12) +
                     in0 + in2 - in1;
}

static NOINLINE void
inv_adst8_1d_internal_c(const int32_t *const in, const ptrdiff_t in_s,
                        const int min, const int max,
                        int32_t *const out, const ptrdiff_t out_s)
{
    assert(in_s > 0 && out_s != 0);
    const int in0 = in[0 * in_s], in1 = in[1 * in_s];
    const int in2 = in[2 * in_s], in3 = in[3 * in_s];
    const int in4 = in[4 * in_s], in5 = in[5 * in_s];
    const int in6 = in[6 * in_s], in7 = in[7 * in_s];

    const int t0a = (((4076 - 4096) * in7 +   401         * in0 + 2048) >> 12) + in7;
    const int t1a = ((  401         * in7 - (4076 - 4096) * in0 + 2048) >> 12) - in0;
    const int t2a = (((3612 - 4096) * in5 +  1931         * in2 + 2048) >> 12) + in5;
    const int t3a = (( 1931         * in5 - (3612 - 4096) * in2 + 2048) >> 12) - in2;
          int t4a =  ( 1299         * in3 +  1583         * in4 + 1024) >> 11;
          int t5a =  ( 1583         * in3 -  1299         * in4 + 1024) >> 11;
          int t6a = (( 1189         * in1 + (3920 - 4096) * in6 + 2048) >> 12) + in6;
          int t7a = (((3920 - 4096) * in1 -  1189         * in6 + 2048) >> 12) + in1;

    const int t0 = CLIP(t0a + t4a);
    const int t1 = CLIP(t1a + t5a);
          int t2 = CLIP(t2a + t6a);
          int t3 = CLIP(t3a + t7a);
    const int t4 = CLIP(t0a - t4a);
    const int t5 = CLIP(t1a - t5a);
          int t6 = CLIP(t2a - t6a);
          int t7 = CLIP(t3a - t7a);

    t4a = (((3784 - 4096) * t4 +  1567         * t5 + 2048) >> 12) + t4;
    t5a = (( 1567         * t4 - (3784 - 4096) * t5 + 2048) >> 12) - t5;
    t6a = (((3784 - 4096) * t7 -  1567         * t6 + 2048) >> 12) + t7;
    t7a = (( 1567         * t7 + (3784 - 4096) * t6 + 2048) >> 12) + t6;

    out[0 * out_s] =  CLIP(t0  + t2 );
    out[7 * out_s] = -CLIP(t1  + t3 );
    t2             =  CLIP(t0  - t2 );
    t3             =  CLIP(t1  - t3 );
    out[1 * out_s] = -CLIP(t4a + t6a);
    out[6 * out_s] =  CLIP(t5a + t7a);
    t6             =  CLIP(t4a - t6a);
    t7             =  CLIP(t5a - t7a);

    out[3 * out_s] = -(((t2 + t3) * 181 + 128) >> 8);
    out[4 * out_s] =   ((t2 - t3) * 181 + 128) >> 8;
    out[2 * out_s] =   ((t6 + t7) * 181 + 128) >> 8;
    out[5 * out_s] = -(((t6 - t7) * 181 + 128) >> 8);
}

static NOINLINE void
inv_adst16_1d_internal_c(const int32_t *const in, const ptrdiff_t in_s,
                         const int min, const int max,
                         int32_t *const out, const ptrdiff_t out_s)
{
    assert(in_s > 0 && out_s != 0);
    const int in0  = in[ 0 * in_s], in1  = in[ 1 * in_s];
    const int in2  = in[ 2 * in_s], in3  = in[ 3 * in_s];
    const int in4  = in[ 4 * in_s], in5  = in[ 5 * in_s];
    const int in6  = in[ 6 * in_s], in7  = in[ 7 * in_s];
    const int in8  = in[ 8 * in_s], in9  = in[ 9 * in_s];
    const int in10 = in[10 * in_s], in11 = in[11 * in_s];
    const int in12 = in[12 * in_s], in13 = in[13 * in_s];
    const int in14 = in[14 * in_s], in15 = in[15 * in_s];

    int t0  = ((in15 * (4091 - 4096) + in0  *   201         + 2048) >> 12) + in15;
    int t1  = ((in15 *   201         - in0  * (4091 - 4096) + 2048) >> 12) - in0;
    int t2  = ((in13 * (3973 - 4096) + in2  *   995         + 2048) >> 12) + in13;
    int t3  = ((in13 *   995         - in2  * (3973 - 4096) + 2048) >> 12) - in2;
    int t4  = ((in11 * (3703 - 4096) + in4  *  1751         + 2048) >> 12) + in11;
    int t5  = ((in11 *  1751         - in4  * (3703 - 4096) + 2048) >> 12) - in4;
    int t6  =  (in9  *  1645         + in6  *  1220         + 1024) >> 11;
    int t7  =  (in9  *  1220         - in6  *  1645         + 1024) >> 11;
    int t8  = ((in7  *  2751         + in8  * (3035 - 4096) + 2048) >> 12) + in8;
    int t9  = ((in7  * (3035 - 4096) - in8  *  2751         + 2048) >> 12) + in7;
    int t10 = ((in5  *  2106         + in10 * (3513 - 4096) + 2048) >> 12) + in10;
    int t11 = ((in5  * (3513 - 4096) - in10 *  2106         + 2048) >> 12) + in5;
    int t12 = ((in3  *  1380         + in12 * (3857 - 4096) + 2048) >> 12) + in12;
    int t13 = ((in3  * (3857 - 4096) - in12 *  1380         + 2048) >> 12) + in3;
    int t14 = ((in1  *   601         + in14 * (4052 - 4096) + 2048) >> 12) + in14;
    int t15 = ((in1  * (4052 - 4096) - in14 *   601         + 2048) >> 12) + in1;

    int t0a  = CLIP(t0 + t8 );
    int t1a  = CLIP(t1 + t9 );
    int t2a  = CLIP(t2 + t10);
    int t3a  = CLIP(t3 + t11);
    int t4a  = CLIP(t4 + t12);
    int t5a  = CLIP(t5 + t13);
    int t6a  = CLIP(t6 + t14);
    int t7a  = CLIP(t7 + t15);
    int t8a  = CLIP(t0 - t8 );
    int t9a  = CLIP(t1 - t9 );
    int t10a = CLIP(t2 - t10);
    int t11a = CLIP(t3 - t11);
    int t12a = CLIP(t4 - t12);
    int t13a = CLIP(t5 - t13);
    int t14a = CLIP(t6 - t14);
    int t15a = CLIP(t7 - t15);

    t8   = ((t8a  * (4017 - 4096) + t9a  *   799         + 2048) >> 12) + t8a;
    t9   = ((t8a  *   799         - t9a  * (4017 - 4096) + 2048) >> 12) - t9a;
    t10  = ((t10a *  2276         + t11a * (3406 - 4096) + 2048) >> 12) + t11a;
    t11  = ((t10a * (3406 - 4096) - t11a *  2276         + 2048) >> 12) + t10a;
    t12  = ((t13a * (4017 - 4096) - t12a *   799         + 2048) >> 12) + t13a;
    t13  = ((t13a *   799         + t12a * (4017 - 4096) + 2048) >> 12) + t12a;
    t14  = ((t15a *  2276         - t14a * (3406 - 4096) + 2048) >> 12) - t14a;
    t15  = ((t15a * (3406 - 4096) + t14a *  2276         + 2048) >> 12) + t15a;

    t0   = CLIP(t0a + t4a);
    t1   = CLIP(t1a + t5a);
    t2   = CLIP(t2a + t6a);
    t3   = CLIP(t3a + t7a);
    t4   = CLIP(t0a - t4a);
    t5   = CLIP(t1a - t5a);
    t6   = CLIP(t2a - t6a);
    t7   = CLIP(t3a - t7a);
    t8a  = CLIP(t8  + t12);
    t9a  = CLIP(t9  + t13);
    t10a = CLIP(t10 + t14);
    t11a = CLIP(t11 + t15);
    t12a = CLIP(t8  - t12);
    t13a = CLIP(t9  - t13);
    t14a = CLIP(t10 - t14);
    t15a = CLIP(t11 - t15);

    t4a  = ((t4   * (3784 - 4096) + t5   *  1567         + 2048) >> 12) + t4;
    t5a  = ((t4   *  1567         - t5   * (3784 - 4096) + 2048) >> 12) - t5;
    t6a  = ((t7   * (3784 - 4096) - t6   *  1567         + 2048) >> 12) + t7;
    t7a  = ((t7   *  1567         + t6   * (3784 - 4096) + 2048) >> 12) + t6;
    t12  = ((t12a * (3784 - 4096) + t13a *  1567         + 2048) >> 12) + t12a;
    t13  = ((t12a *  1567         - t13a * (3784 - 4096) + 2048) >> 12) - t13a;
    t14  = ((t15a * (3784 - 4096) - t14a *  1567         + 2048) >> 12) + t15a;
    t15  = ((t15a *  1567         + t14a * (3784 - 4096) + 2048) >> 12) + t14a;

    out[ 0 * out_s] =  CLIP(t0  + t2  );
    out[15 * out_s] = -CLIP(t1  + t3  );
    t2a             =  CLIP(t0  - t2  );
    t3a             =  CLIP(t1  - t3  );
    out[ 3 * out_s] = -CLIP(t4a + t6a );
    out[12 * out_s] =  CLIP(t5a + t7a );
    t6              =  CLIP(t4a - t6a );
    t7              =  CLIP(t5a - t7a );
    out[ 1 * out_s] = -CLIP(t8a + t10a);
    out[14 * out_s] =  CLIP(t9a + t11a);
    t10             =  CLIP(t8a - t10a);
    t11             =  CLIP(t9a - t11a);
    out[ 2 * out_s] =  CLIP(t12 + t14 );
    out[13 * out_s] = -CLIP(t13 + t15 );
    t14a            =  CLIP(t12 - t14 );
    t15a            =  CLIP(t13 - t15 );

    out[ 7 * out_s] = -(((t2a  + t3a)  * 181 + 128) >> 8);
    out[ 8 * out_s] =   ((t2a  - t3a)  * 181 + 128) >> 8;
    out[ 4 * out_s] =   ((t6   + t7)   * 181 + 128) >> 8;
    out[11 * out_s] = -(((t6   - t7)   * 181 + 128) >> 8);
    out[ 6 * out_s] =   ((t10  + t11)  * 181 + 128) >> 8;
    out[ 9 * out_s] = -(((t10  - t11)  * 181 + 128) >> 8);
    out[ 5 * out_s] = -(((t14a + t15a) * 181 + 128) >> 8);
    out[10 * out_s] =   ((t14a - t15a) * 181 + 128) >> 8;
}

#define inv_adst_1d(sz) \
void dav1d_inv_adst##sz##_1d_c(int32_t *const c, const ptrdiff_t stride, \
                               const int min, const int max) \
{ \
    inv_adst##sz##_1d_internal_c(c, stride, min, max, c, stride); \
} \
void dav1d_inv_flipadst##sz##_1d_c(int32_t *const c, const ptrdiff_t stride, \
                                   const int min, const int max) \
{ \
    inv_adst##sz##_1d_internal_c(c, stride, min, max, \
                                 &c[(sz - 1) * stride], -stride); \
}

inv_adst_1d( 4)
inv_adst_1d( 8)
inv_adst_1d(16)

#undef inv_adst_1d

void dav1d_inv_identity4_1d_c(int32_t *const c, const ptrdiff_t stride,
                              const int min, const int max)
{
    assert(stride > 0);
    for (int i = 0; i < 4; i++) {
        const int in = c[stride * i];
        c[stride * i] = in + ((in * 1697 + 2048) >> 12);
    }
}

void dav1d_inv_identity8_1d_c(int32_t *const c, const ptrdiff_t stride,
                              const int min, const int max)
{
    assert(stride > 0);
    for (int i = 0; i < 8; i++)
        c[stride * i] *= 2;
}

void dav1d_inv_identity16_1d_c(int32_t *const c, const ptrdiff_t stride,
                               const int min, const int max)
{
    assert(stride > 0);
    for (int i = 0; i < 16; i++) {
        const int in = c[stride * i];
        c[stride * i] = 2 * in + ((in * 1697 + 1024) >> 11);
    }
}

void dav1d_inv_identity32_1d_c(int32_t *const c, const ptrdiff_t stride,
                               const int min, const int max)
{
    assert(stride > 0);
    for (int i = 0; i < 32; i++)
        c[stride * i] *= 4;
}

void dav1d_inv_wht4_1d_c(int32_t *const c, const ptrdiff_t stride) {
    assert(stride > 0);
    const int in0 = c[0 * stride], in1 = c[1 * stride];
    const int in2 = c[2 * stride], in3 = c[3 * stride];

    const int t0 = in0 + in1;
    const int t2 = in2 - in3;
    const int t4 = (t0 - t2) >> 1;
    const int t3 = t4 - in3;
    const int t1 = t4 - in1;

    c[0 * stride] = t0 - t3;
    c[1 * stride] = t3;
    c[2 * stride] = t1;
    c[3 * stride] = t2 + t1;
}
