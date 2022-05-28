/*
 * Copyright © 2018, VideoLAN and dav1d authors
 * Copyright © 2018, Two Orioles, LLC
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

#include <stdlib.h>

#include "common/intops.h"

#include "src/warpmv.h"

static const uint16_t div_lut[257] = {
    16384, 16320, 16257, 16194, 16132, 16070, 16009, 15948, 15888, 15828, 15768,
    15709, 15650, 15592, 15534, 15477, 15420, 15364, 15308, 15252, 15197, 15142,
    15087, 15033, 14980, 14926, 14873, 14821, 14769, 14717, 14665, 14614, 14564,
    14513, 14463, 14413, 14364, 14315, 14266, 14218, 14170, 14122, 14075, 14028,
    13981, 13935, 13888, 13843, 13797, 13752, 13707, 13662, 13618, 13574, 13530,
    13487, 13443, 13400, 13358, 13315, 13273, 13231, 13190, 13148, 13107, 13066,
    13026, 12985, 12945, 12906, 12866, 12827, 12788, 12749, 12710, 12672, 12633,
    12596, 12558, 12520, 12483, 12446, 12409, 12373, 12336, 12300, 12264, 12228,
    12193, 12157, 12122, 12087, 12053, 12018, 11984, 11950, 11916, 11882, 11848,
    11815, 11782, 11749, 11716, 11683, 11651, 11619, 11586, 11555, 11523, 11491,
    11460, 11429, 11398, 11367, 11336, 11305, 11275, 11245, 11215, 11185, 11155,
    11125, 11096, 11067, 11038, 11009, 10980, 10951, 10923, 10894, 10866, 10838,
    10810, 10782, 10755, 10727, 10700, 10673, 10645, 10618, 10592, 10565, 10538,
    10512, 10486, 10460, 10434, 10408, 10382, 10356, 10331, 10305, 10280, 10255,
    10230, 10205, 10180, 10156, 10131, 10107, 10082, 10058, 10034, 10010,  9986,
     9963,  9939,  9916,  9892,  9869,  9846,  9823,  9800,  9777,  9754,  9732,
     9709,  9687,  9664,  9642,  9620,  9598,  9576,  9554,  9533,  9511,  9489,
     9468,  9447,  9425,  9404,  9383,  9362,  9341,  9321,  9300,  9279,  9259,
     9239,  9218,  9198,  9178,  9158,  9138,  9118,  9098,  9079,  9059,  9039,
     9020,  9001,  8981,  8962,  8943,  8924,  8905,  8886,  8867,  8849,  8830,
     8812,  8793,  8775,  8756,  8738,  8720,  8702,  8684,  8666,  8648,  8630,
     8613,  8595,  8577,  8560,  8542,  8525,  8508,  8490,  8473,  8456,  8439,
     8422,  8405,  8389,  8372,  8355,  8339,  8322,  8306,  8289,  8273,  8257,
     8240,  8224,  8208,  8192,
};

static inline int iclip_wmp(const int v) {
    const int cv = iclip(v, INT16_MIN, INT16_MAX);

    return apply_sign((abs(cv) + 32) >> 6, cv) * (1 << 6);
}

static inline int resolve_divisor_32(const unsigned d, int *const shift) {
    *shift = ulog2(d);
    const int e = d - (1 << *shift);
    const int f = *shift > 8 ? (e + (1 << (*shift - 9))) >> (*shift - 8) :
                               e << (8 - *shift);
    assert(f <= 256);
    *shift += 14;
    // Use f as lookup into the precomputed table of multipliers
    return div_lut[f];
}

int dav1d_get_shear_params(Dav1dWarpedMotionParams *const wm) {
    const int32_t *const mat = wm->matrix;

    if (mat[2] <= 0) return 1;

    wm->u.p.alpha = iclip_wmp(mat[2] - 0x10000);
    wm->u.p.beta = iclip_wmp(mat[3]);

    int shift;
    const int y = apply_sign(resolve_divisor_32(abs(mat[2]), &shift), mat[2]);
    const int64_t v1 = ((int64_t) mat[4] * 0x10000) * y;
    const int rnd = (1 << shift) >> 1;
    wm->u.p.gamma = iclip_wmp(apply_sign64((int) ((llabs(v1) + rnd) >> shift), v1));
    const int64_t v2 = ((int64_t) mat[3] * mat[4]) * y;
    wm->u.p.delta = iclip_wmp(mat[5] -
                          apply_sign64((int) ((llabs(v2) + rnd) >> shift), v2) -
                          0x10000);

    return (4 * abs(wm->u.p.alpha) + 7 * abs(wm->u.p.beta) >= 0x10000) ||
           (4 * abs(wm->u.p.gamma) + 4 * abs(wm->u.p.delta) >= 0x10000);
}

static int resolve_divisor_64(const uint64_t d, int *const shift) {
    *shift = u64log2(d);
    const int64_t e = d - (1LL << *shift);
    const int64_t f = *shift > 8 ? (e + (1LL << (*shift - 9))) >> (*shift - 8) :
                                   e << (8 - *shift);
    assert(f <= 256);
    *shift += 14;
    // Use f as lookup into the precomputed table of multipliers
    return div_lut[f];
}

static int get_mult_shift_ndiag(const int64_t px,
                                const int idet, const int shift)
{
    const int64_t v1 = px * idet;
    const int v2 = apply_sign64((int) ((llabs(v1) +
                                        ((1LL << shift) >> 1)) >> shift),
                                v1);
    return iclip(v2, -0x1fff, 0x1fff);
}

static int get_mult_shift_diag(const int64_t px,
                               const int idet, const int shift)
{
    const int64_t v1 = px * idet;
    const int v2 = apply_sign64((int) ((llabs(v1) +
                                        ((1LL << shift) >> 1)) >> shift),
                                v1);
    return iclip(v2, 0xe001, 0x11fff);
}

void dav1d_set_affine_mv2d(const int bw4, const int bh4,
                           const mv mv, Dav1dWarpedMotionParams *const wm,
                           const int bx4, const int by4)
{
    int32_t *const mat = wm->matrix;
    const int rsuy = 2 * bh4 - 1;
    const int rsux = 2 * bw4 - 1;
    const int isuy = by4 * 4 + rsuy;
    const int isux = bx4 * 4 + rsux;

    mat[0] = iclip(mv.x * 0x2000 - (isux * (mat[2] - 0x10000) + isuy * mat[3]),
                   -0x800000, 0x7fffff);
    mat[1] = iclip(mv.y * 0x2000 - (isux * mat[4] + isuy * (mat[5] - 0x10000)),
                   -0x800000, 0x7fffff);
}

int dav1d_find_affine_int(const int (*pts)[2][2], const int np,
                          const int bw4, const int bh4,
                          const mv mv, Dav1dWarpedMotionParams *const wm,
                          const int bx4, const int by4)
{
    int32_t *const mat = wm->matrix;
    int a[2][2] = { { 0, 0 }, { 0, 0 } };
    int bx[2] = { 0, 0 };
    int by[2] = { 0, 0 };
    const int rsuy = 2 * bh4 - 1;
    const int rsux = 2 * bw4 - 1;
    const int suy = rsuy * 8;
    const int sux = rsux * 8;
    const int duy = suy + mv.y;
    const int dux = sux + mv.x;
    const int isuy = by4 * 4 + rsuy;
    const int isux = bx4 * 4 + rsux;

    for (int i = 0; i < np; i++) {
        const int dx = pts[i][1][0] - dux;
        const int dy = pts[i][1][1] - duy;
        const int sx = pts[i][0][0] - sux;
        const int sy = pts[i][0][1] - suy;
        if (abs(sx - dx) < 256 && abs(sy - dy) < 256) {
            a[0][0] += ((sx * sx) >> 2) + sx * 2 + 8;
            a[0][1] += ((sx * sy) >> 2) + sx + sy + 4;
            a[1][1] += ((sy * sy) >> 2) + sy * 2 + 8;
            bx[0] += ((sx * dx) >> 2) + sx + dx + 8;
            bx[1] += ((sy * dx) >> 2) + sy + dx + 4;
            by[0] += ((sx * dy) >> 2) + sx + dy + 4;
            by[1] += ((sy * dy) >> 2) + sy + dy + 8;
        }
    }

    // compute determinant of a
    const int64_t det = (int64_t) a[0][0] * a[1][1] - (int64_t) a[0][1] * a[0][1];
    if (det == 0) return 1;
    int shift, idet = apply_sign64(resolve_divisor_64(llabs(det), &shift), det);
    shift -= 16;
    if (shift < 0) {
        idet <<= -shift;
        shift = 0;
    }

    // solve the least-squares
    mat[2] = get_mult_shift_diag((int64_t) a[1][1] * bx[0] -
                                 (int64_t) a[0][1] * bx[1], idet, shift);
    mat[3] = get_mult_shift_ndiag((int64_t) a[0][0] * bx[1] -
                                  (int64_t) a[0][1] * bx[0], idet, shift);
    mat[4] = get_mult_shift_ndiag((int64_t) a[1][1] * by[0] -
                                  (int64_t) a[0][1] * by[1], idet, shift);
    mat[5] = get_mult_shift_diag((int64_t) a[0][0] * by[1] -
                                 (int64_t) a[0][1] * by[0], idet, shift);

    mat[0] = iclip(mv.x * 0x2000 - (isux * (mat[2] - 0x10000) + isuy * mat[3]),
                   -0x800000, 0x7fffff);
    mat[1] = iclip(mv.y * 0x2000 - (isux * mat[4] + isuy * (mat[5] - 0x10000)),
                   -0x800000, 0x7fffff);

    return 0;
}
