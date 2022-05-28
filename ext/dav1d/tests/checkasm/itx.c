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

#include "tests/checkasm/checkasm.h"

#include <math.h>

#include "src/itx.h"
#include "src/levels.h"
#include "src/scan.h"
#include "src/tables.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#ifndef M_SQRT1_2
#define M_SQRT1_2 0.707106781186547524401
#endif

enum Tx1D { DCT, ADST, FLIPADST, IDENTITY, WHT };

static const uint8_t itx_1d_types[N_TX_TYPES_PLUS_LL][2] = {
    [DCT_DCT]           = { DCT,      DCT      },
    [ADST_DCT]          = { DCT,      ADST     },
    [DCT_ADST]          = { ADST,     DCT      },
    [ADST_ADST]         = { ADST,     ADST     },
    [FLIPADST_DCT]      = { DCT,      FLIPADST },
    [DCT_FLIPADST]      = { FLIPADST, DCT      },
    [FLIPADST_FLIPADST] = { FLIPADST, FLIPADST },
    [ADST_FLIPADST]     = { FLIPADST, ADST     },
    [FLIPADST_ADST]     = { ADST,     FLIPADST },
    [IDTX]              = { IDENTITY, IDENTITY },
    [V_DCT]             = { IDENTITY, DCT      },
    [H_DCT]             = { DCT,      IDENTITY },
    [V_ADST]            = { IDENTITY, ADST     },
    [H_ADST]            = { ADST,     IDENTITY },
    [V_FLIPADST]        = { IDENTITY, FLIPADST },
    [H_FLIPADST]        = { FLIPADST, IDENTITY },
    [WHT_WHT]           = { WHT,      WHT      },
};

static const char *const itx_1d_names[5] = {
    [DCT]      = "dct",
    [ADST]     = "adst",
    [FLIPADST] = "flipadst",
    [IDENTITY] = "identity",
    [WHT]      = "wht"
};

static const double scaling_factors[9] = {
    4.0000,             /*  4x4                          */
    4.0000 * M_SQRT1_2, /*  4x8   8x4                    */
    2.0000,             /*  4x16  8x8  16x4              */
    2.0000 * M_SQRT1_2, /*        8x16 16x8              */
    1.0000,             /*        8x32 16x16 32x8        */
    0.5000 * M_SQRT1_2, /*             16x32 32x16       */
    0.2500,             /*             16x64 32x32 64x16 */
    0.1250 * M_SQRT1_2, /*                   32x64 64x32 */
    0.0625,             /*                         64x64 */
};

/* FIXME: Ensure that those forward transforms are similar to the real AV1
 * transforms. The FLIPADST currently uses the ADST forward transform for
 * example which is obviously "incorrect", but we're just using it for now
 * since it does produce coefficients in the correct range at least. */

/* DCT-II */
static void fdct_1d(double *const out, const double *const in, const int sz) {
    for (int i = 0; i < sz; i++) {
        out[i] = 0.0;
        for (int j = 0; j < sz; j++)
            out[i] += in[j] * cos(M_PI * (2 * j + 1) * i / (sz * 2.0));
    }
    out[0] *= M_SQRT1_2;
}

/* See "Towards jointly optimal spatial prediction and adaptive transform in
 * video/image coding", by J. Han, A. Saxena, and K. Rose
 * IEEE Proc. ICASSP, pp. 726-729, Mar. 2010.
 * and "A Butterfly Structured Design of The Hybrid Transform Coding Scheme",
 * by Jingning Han, Yaowu Xu, and Debargha Mukherjee
 * http://research.google.com/pubs/archive/41418.pdf
 */
static void fadst_1d(double *const out, const double *const in, const int sz) {
    for (int i = 0; i < sz; i++) {
        out[i] = 0.0;
        for (int j = 0; j < sz; j++)
            out[i] += in[j] * sin(M_PI *
            (sz == 4 ? (    j + 1) * (2 * i + 1) / (8.0 + 1.0) :
                       (2 * j + 1) * (2 * i + 1) / (sz * 4.0)));
    }
}

static void fwht4_1d(double *const out, const double *const in)
{
    const double t0 = in[0] + in[1];
    const double t3 = in[3] - in[2];
    const double t4 = (t0 - t3) * 0.5;
    const double t1 = t4 - in[1];
    const double t2 = t4 - in[2];
    out[0] = t0 - t2;
    out[1] = t2;
    out[2] = t3 + t1;
    out[3] = t1;
}

static int copy_subcoefs(coef *coeff,
                         const enum RectTxfmSize tx, const enum TxfmType txtp,
                         const int sw, const int sh, const int subsh)
{
    /* copy the topleft coefficients such that the return value (being the
     * coefficient scantable index for the eob token) guarantees that only
     * the topleft $sub out of $sz (where $sz >= $sub) coefficients in both
     * dimensions are non-zero. This leads to braching to specific optimized
     * simd versions (e.g. dc-only) so that we get full asm coverage in this
     * test */

    const enum TxClass tx_class = dav1d_tx_type_class[txtp];
    const uint16_t *const scan = dav1d_scans[tx];
    const int sub_high = subsh > 0 ? subsh * 8 - 1 : 0;
    const int sub_low  = subsh > 1 ? sub_high - 8 : 0;
    int n, eob;

    for (n = 0, eob = 0; n < sw * sh; n++) {
        int rc, rcx, rcy;
        if (tx_class == TX_CLASS_2D)
            rc = scan[n], rcx = rc % sh, rcy = rc / sh;
        else if (tx_class == TX_CLASS_H)
            rcx = n % sh, rcy = n / sh, rc = n;
        else /* tx_class == TX_CLASS_V */
            rcx = n / sw, rcy = n % sw, rc = rcy * sh + rcx;

        /* Pick a random eob within this sub-itx */
        if (rcx > sub_high || rcy > sub_high) {
            break; /* upper boundary */
        } else if (!eob && (rcx > sub_low || rcy > sub_low))
            eob = n; /* lower boundary */
    }

    if (eob)
        eob += rnd() % (n - eob - 1);
    if (tx_class == TX_CLASS_2D)
        for (n = eob + 1; n < sw * sh; n++)
            coeff[scan[n]] = 0;
    else if (tx_class == TX_CLASS_H)
        for (n = eob + 1; n < sw * sh; n++)
            coeff[n] = 0;
    else /* tx_class == TX_CLASS_V */ {
        for (int rcx = eob / sw, rcy = eob % sw; rcx < sh; rcx++, rcy = -1)
            while (++rcy < sw)
                coeff[rcy * sh + rcx] = 0;
        n = sw * sh;
    }
    for (; n < 32 * 32; n++)
        coeff[n] = rnd();
    return eob;
}

static int ftx(coef *const buf, const enum RectTxfmSize tx,
               const enum TxfmType txtp, const int w, const int h,
               const int subsh, const int bitdepth_max)
{
    double out[64 * 64], temp[64 * 64];
    const double scale = scaling_factors[ctz(w * h) - 4];
    const int sw = imin(w, 32), sh = imin(h, 32);

    for (int i = 0; i < h; i++) {
        double in[64], temp_out[64];

        for (int i = 0; i < w; i++)
            in[i] = (rnd() & (2 * bitdepth_max + 1)) - bitdepth_max;

        switch (itx_1d_types[txtp][0]) {
        case DCT:
            fdct_1d(temp_out, in, w);
            break;
        case ADST:
        case FLIPADST:
            fadst_1d(temp_out, in, w);
            break;
        case WHT:
            fwht4_1d(temp_out, in);
            break;
        case IDENTITY:
            memcpy(temp_out, in, w * sizeof(*temp_out));
            break;
        }

        for (int j = 0; j < w; j++)
            temp[j * h + i] = temp_out[j] * scale;
    }

    for (int i = 0; i < w; i++) {
        switch (itx_1d_types[txtp][0]) {
        case DCT:
            fdct_1d(&out[i * h], &temp[i * h], h);
            break;
        case ADST:
        case FLIPADST:
            fadst_1d(&out[i * h], &temp[i * h], h);
            break;
        case WHT:
            fwht4_1d(&out[i * h], &temp[i * h]);
            break;
        case IDENTITY:
            memcpy(&out[i * h], &temp[i * h], h * sizeof(*out));
            break;
        }
    }

    for (int y = 0; y < sh; y++)
        for (int x = 0; x < sw; x++)
            buf[y * sw + x] = (coef) (out[y * w + x] + 0.5);

    return copy_subcoefs(buf, tx, txtp, sw, sh, subsh);
}

static void check_itxfm_add(Dav1dInvTxfmDSPContext *const c,
                            const enum RectTxfmSize tx)
{
    ALIGN_STK_64(coef, coeff, 2, [32 * 32]);
    PIXEL_RECT(c_dst, 64, 64);
    PIXEL_RECT(a_dst, 64, 64);

    static const uint8_t subsh_iters[5] = { 2, 2, 3, 5, 5 };

    const int w = dav1d_txfm_dimensions[tx].w * 4;
    const int h = dav1d_txfm_dimensions[tx].h * 4;
    const int subsh_max = subsh_iters[imax(dav1d_txfm_dimensions[tx].lw,
                                           dav1d_txfm_dimensions[tx].lh)];
#if BITDEPTH == 16
    const int bpc_min = 10, bpc_max = 12;
#else
    const int bpc_min = 8, bpc_max = 8;
#endif

    declare_func(void, pixel *dst, ptrdiff_t dst_stride, coef *coeff,
                 int eob HIGHBD_DECL_SUFFIX);

    for (int bpc = bpc_min; bpc <= bpc_max; bpc += 2) {
        bitfn(dav1d_itx_dsp_init)(c, bpc);
        for (enum TxfmType txtp = 0; txtp < N_TX_TYPES_PLUS_LL; txtp++)
            for (int subsh = 0; subsh < subsh_max; subsh++)
                if (check_func(c->itxfm_add[tx][txtp],
                               "inv_txfm_add_%dx%d_%s_%s_%d_%dbpc",
                               w, h, itx_1d_names[itx_1d_types[txtp][0]],
                               itx_1d_names[itx_1d_types[txtp][1]], subsh,
                               bpc))
                {
                    const int bitdepth_max = (1 << bpc) - 1;
                    const int eob = ftx(coeff[0], tx, txtp, w, h, subsh, bitdepth_max);
                    memcpy(coeff[1], coeff[0], sizeof(*coeff));

                    CLEAR_PIXEL_RECT(c_dst);
                    CLEAR_PIXEL_RECT(a_dst);

                    for (int y = 0; y < h; y++)
                        for (int x = 0; x < w; x++)
                            c_dst[y*PXSTRIDE(c_dst_stride) + x] =
                            a_dst[y*PXSTRIDE(a_dst_stride) + x] = rnd() & bitdepth_max;

                    call_ref(c_dst, c_dst_stride, coeff[0], eob
                             HIGHBD_TAIL_SUFFIX);
                    call_new(a_dst, a_dst_stride, coeff[1], eob
                             HIGHBD_TAIL_SUFFIX);

                    checkasm_check_pixel_padded(c_dst, c_dst_stride,
                                                a_dst, a_dst_stride,
                                                w, h, "dst");
                    if (memcmp(coeff[0], coeff[1], sizeof(*coeff)))
                        fail();

                    bench_new(alternate(c_dst, a_dst), a_dst_stride,
                              alternate(coeff[0], coeff[1]), eob HIGHBD_TAIL_SUFFIX);
                }
    }
    report("add_%dx%d", w, h);
}

void bitfn(checkasm_check_itx)(void) {
    static const uint8_t txfm_size_order[N_RECT_TX_SIZES] = {
        TX_4X4,   RTX_4X8,  RTX_4X16,
        RTX_8X4,  TX_8X8,   RTX_8X16,  RTX_8X32,
        RTX_16X4, RTX_16X8, TX_16X16,  RTX_16X32, RTX_16X64,
                  RTX_32X8, RTX_32X16, TX_32X32,  RTX_32X64,
                            RTX_64X16, RTX_64X32, TX_64X64
    };

    /* Zero unused function pointer elements. */
    Dav1dInvTxfmDSPContext c = { { { 0 } } };

    for (int i = 0; i < N_RECT_TX_SIZES; i++)
        check_itxfm_add(&c, txfm_size_order[i]);
}
