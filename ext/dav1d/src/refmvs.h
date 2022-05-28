/*
 * Copyright © 2020, VideoLAN and dav1d authors
 * Copyright © 2020, Two Orioles, LLC
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

#ifndef DAV1D_SRC_REF_MVS_H
#define DAV1D_SRC_REF_MVS_H

#include <stdint.h>

#include "dav1d/headers.h"

#include "common/intops.h"

#include "src/intra_edge.h"
#include "src/tables.h"

#define INVALID_MV 0x80008000

typedef struct refmvs_temporal_block {
    mv mv;
    int8_t ref;
} refmvs_temporal_block;

typedef union refmvs_refpair {
    int8_t ref[2]; // [0] = 0: intra=1, [1] = -1: comp=0
    uint16_t pair;
} refmvs_refpair;

typedef union refmvs_mvpair {
    mv mv[2];
    uint64_t n;
} refmvs_mvpair;

PACKED(typedef struct refmvs_block {
    refmvs_mvpair mv;
    refmvs_refpair ref;
    uint8_t bs, mf; // 1 = globalmv+affine, 2 = newmv
}) ALIGN(refmvs_block, 4);

typedef struct refmvs_frame {
    const Dav1dFrameHeader *frm_hdr;
    int iw4, ih4, iw8, ih8;
    int sbsz;
    int use_ref_frame_mvs;
    uint8_t sign_bias[7], mfmv_sign[7];
    int8_t pocdiff[7];
    uint8_t mfmv_ref[3];
    int mfmv_ref2cur[3];
    int mfmv_ref2ref[3][7];
    int n_mfmvs;

    refmvs_temporal_block *rp;
    /*const*/ refmvs_temporal_block *const *rp_ref;
    refmvs_temporal_block *rp_proj;
    ptrdiff_t rp_stride;

    refmvs_block *r; // 35 x r_stride memory
    ptrdiff_t r_stride;
    int n_tile_rows, n_tile_threads, n_frame_threads;
} refmvs_frame;

typedef struct refmvs_tile {
    const refmvs_frame *rf;
    refmvs_block *r[32 + 5];
    refmvs_temporal_block *rp_proj;
    struct {
        int start, end;
    } tile_col, tile_row;
} refmvs_tile;

typedef struct refmvs_candidate {
    refmvs_mvpair mv;
    int weight;
} refmvs_candidate;

#define decl_splat_mv_fn(name) \
void (name)(refmvs_block **rr, const refmvs_block *rmv, int bx4, int bw4, int bh4)
typedef decl_splat_mv_fn(*splat_mv_fn);

typedef struct Dav1dRefmvsDSPContext {
    splat_mv_fn splat_mv;
} Dav1dRefmvsDSPContext;

// call once per frame thread
void dav1d_refmvs_init(refmvs_frame *rf);
void dav1d_refmvs_clear(refmvs_frame *rf);

// call once per frame
int dav1d_refmvs_init_frame(refmvs_frame *rf,
                            const Dav1dSequenceHeader *seq_hdr,
                            const Dav1dFrameHeader *frm_hdr,
                            const unsigned ref_poc[7],
                            refmvs_temporal_block *rp,
                            const unsigned ref_ref_poc[7][7],
                            /*const*/ refmvs_temporal_block *const rp_ref[7],
                            int n_tile_threads, int n_frame_threads);

// initialize temporal MVs; this can be done in any configuration, e.g. one
// tile/sbrow at a time, where col_{start,end}8 are the tile boundaries; or
// it can just be for the whole frame's sbrow, where col_{start,end}8 are the
// frame boundaries. row_{start,end}8 are the superblock row boundaries.
void dav1d_refmvs_load_tmvs(const refmvs_frame *rf, int tile_row_idx,
                            int col_start8, int col_end8,
                            int row_start8, int row_end8);

// cache the current tile/sbrow (or frame/sbrow)'s projectable motion vectors
// into buffers for use in future frame's temporal MV prediction
void dav1d_refmvs_save_tmvs(const refmvs_tile *rt,
                            int col_start8, int col_end8,
                            int row_start8, int row_end8);

// initialize tile boundaries and refmvs_block pointers for one tile/sbrow
void dav1d_refmvs_tile_sbrow_init(refmvs_tile *rt, const refmvs_frame *rf,
                                  int tile_col_start4, int tile_col_end4,
                                  int tile_row_start4, int tile_row_end4,
                                  int sby, int tile_row_idx, int pass);

// call for each block
void dav1d_refmvs_find(const refmvs_tile *rt,
                       refmvs_candidate mvstack[8], int *cnt,
                       int *ctx, const refmvs_refpair ref, enum BlockSize bs,
                       enum EdgeFlags edge_flags, int by4, int bx4);

void dav1d_refmvs_dsp_init(Dav1dRefmvsDSPContext *dsp);
void dav1d_refmvs_dsp_init_arm(Dav1dRefmvsDSPContext *dsp);
void dav1d_refmvs_dsp_init_x86(Dav1dRefmvsDSPContext *dsp);

#endif /* DAV1D_SRC_REF_MVS_H */
