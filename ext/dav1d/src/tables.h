/*
 * Copyright © 2018-2021, VideoLAN and dav1d authors
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

#ifndef DAV1D_SRC_TABLES_H
#define DAV1D_SRC_TABLES_H

#include <stdint.h>

#include "common/intops.h"

#include "src/levels.h"

extern const uint8_t dav1d_al_part_ctx[2][N_BL_LEVELS][N_PARTITIONS];
extern const uint8_t /* enum BlockSize */
                     dav1d_block_sizes[N_BL_LEVELS][N_PARTITIONS][2];
// width, height (in 4px blocks), log2 versions of these two
extern const uint8_t dav1d_block_dimensions[N_BS_SIZES][4];
typedef struct TxfmInfo {
    // width, height (in 4px blocks), log2 of them, min/max of log2, sub, pad
    uint8_t w, h, lw, lh, min, max, sub, ctx;
} TxfmInfo;
extern const TxfmInfo dav1d_txfm_dimensions[N_RECT_TX_SIZES];
extern const uint8_t /* enum (Rect)TxfmSize */
                     dav1d_max_txfm_size_for_bs[N_BS_SIZES][4 /* y, 420, 422, 444 */];
extern const uint8_t /* enum TxfmType */
                     dav1d_txtp_from_uvmode[N_UV_INTRA_PRED_MODES];

extern const uint8_t /* enum InterPredMode */
                     dav1d_comp_inter_pred_modes[N_COMP_INTER_PRED_MODES][2];

extern const uint8_t dav1d_partition_type_count[N_BL_LEVELS];
extern const uint8_t /* enum TxfmType */ dav1d_tx_types_per_set[40];

extern const uint8_t dav1d_filter_mode_to_y_mode[5];
extern const uint8_t dav1d_ymode_size_context[N_BS_SIZES];
extern const uint8_t dav1d_lo_ctx_offsets[3][5][5];
extern const uint8_t dav1d_skip_ctx[5][5];
extern const uint8_t /* enum TxClass */
                     dav1d_tx_type_class[N_TX_TYPES_PLUS_LL];
extern const uint8_t /* enum Filter2d */
                     dav1d_filter_2d[DAV1D_N_FILTERS /* h */][DAV1D_N_FILTERS /* v */];
extern const uint8_t /* enum Dav1dFilterMode */ dav1d_filter_dir[N_2D_FILTERS][2];
extern const uint8_t dav1d_intra_mode_context[N_INTRA_PRED_MODES];
extern const uint8_t dav1d_wedge_ctx_lut[N_BS_SIZES];

static const unsigned cfl_allowed_mask =
    (1 << BS_32x32) |
    (1 << BS_32x16) |
    (1 << BS_32x8) |
    (1 << BS_16x32) |
    (1 << BS_16x16) |
    (1 << BS_16x8) |
    (1 << BS_16x4) |
    (1 << BS_8x32) |
    (1 << BS_8x16) |
    (1 << BS_8x8) |
    (1 << BS_8x4) |
    (1 << BS_4x16) |
    (1 << BS_4x8) |
    (1 << BS_4x4);

static const unsigned wedge_allowed_mask =
    (1 << BS_32x32) |
    (1 << BS_32x16) |
    (1 << BS_32x8) |
    (1 << BS_16x32) |
    (1 << BS_16x16) |
    (1 << BS_16x8) |
    (1 << BS_8x32) |
    (1 << BS_8x16) |
    (1 << BS_8x8);

static const unsigned interintra_allowed_mask =
    (1 << BS_32x32) |
    (1 << BS_32x16) |
    (1 << BS_16x32) |
    (1 << BS_16x16) |
    (1 << BS_16x8) |
    (1 << BS_8x16) |
    (1 << BS_8x8);

extern const Dav1dWarpedMotionParams dav1d_default_wm_params;

extern const int8_t dav1d_cdef_directions[12][2];

extern const uint16_t dav1d_sgr_params[16][2];
extern const uint8_t dav1d_sgr_x_by_x[256];

extern const int8_t dav1d_mc_subpel_filters[6][15][8];
extern const int8_t dav1d_mc_warp_filter[193][8];
extern const int8_t dav1d_resize_filter[64][8];

extern const uint8_t dav1d_sm_weights[128];
extern const uint16_t dav1d_dr_intra_derivative[44];
extern const int8_t dav1d_filter_intra_taps[5][64];

extern const uint8_t dav1d_obmc_masks[64];

extern const int16_t dav1d_gaussian_sequence[2048]; // for fgs

#endif /* DAV1D_SRC_TABLES_H */
