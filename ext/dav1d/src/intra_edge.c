/*
 * Copyright © 2018-2023, VideoLAN and dav1d authors
 * Copyright © 2018-2023, Two Orioles, LLC
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

#include "common/attributes.h"

#include "src/intra_edge.h"
#include "src/levels.h"

struct ModeSelMem {
    EdgeBranch *nwc[3 /* 64x64, 32x32, 16x16 */];
    EdgeTip *nt;
};

/* Because we're using 16-bit offsets to refer to other nodes those arrays
 * are placed in a struct to ensure they're consecutive in memory. */
static struct {
    EdgeBranch branch_sb128[1 + 4 + 16 + 64];
    EdgeTip tip_sb128[256];
    EdgeBranch branch_sb64[1 + 4 + 16];
    EdgeTip tip_sb64[64];
} ALIGN(nodes, 16);

const EdgeNode *dav1d_intra_edge_tree[2] = {
    (EdgeNode*)nodes.branch_sb128, (EdgeNode*)nodes.branch_sb64
};

static COLD void init_edges(EdgeNode *const node,
                            const enum BlockLevel bl,
                            const enum EdgeFlags edge_flags)
{
    node->o = edge_flags;
    node->h[0] = edge_flags | EDGE_ALL_LEFT_HAS_BOTTOM;
    node->v[0] = edge_flags | EDGE_ALL_TOP_HAS_RIGHT;

    if (bl == BL_8X8) {
        EdgeTip *const nt = (EdgeTip *) node;

        node->h[1] = edge_flags & (EDGE_ALL_LEFT_HAS_BOTTOM |
                                   EDGE_I420_TOP_HAS_RIGHT);
        node->v[1] = edge_flags & (EDGE_ALL_TOP_HAS_RIGHT |
                                   EDGE_I420_LEFT_HAS_BOTTOM |
                                   EDGE_I422_LEFT_HAS_BOTTOM);

        nt->split[0] = (edge_flags & EDGE_ALL_TOP_HAS_RIGHT) |
                       EDGE_I422_LEFT_HAS_BOTTOM;
        nt->split[1] = edge_flags | EDGE_I444_TOP_HAS_RIGHT;
        nt->split[2] = edge_flags & (EDGE_I420_TOP_HAS_RIGHT |
                                     EDGE_I420_LEFT_HAS_BOTTOM |
                                     EDGE_I422_LEFT_HAS_BOTTOM);
    } else {
        EdgeBranch *const nwc = (EdgeBranch *) node;

        node->h[1] = edge_flags & EDGE_ALL_LEFT_HAS_BOTTOM;
        node->v[1] = edge_flags & EDGE_ALL_TOP_HAS_RIGHT;

        nwc->h4 = EDGE_ALL_LEFT_HAS_BOTTOM;
        nwc->v4 = EDGE_ALL_TOP_HAS_RIGHT;
        if (bl == BL_16X16) {
            nwc->h4 |= edge_flags & EDGE_I420_TOP_HAS_RIGHT;
            nwc->v4 |= edge_flags & (EDGE_I420_LEFT_HAS_BOTTOM |
                                     EDGE_I422_LEFT_HAS_BOTTOM);
        }
    }
}

#define PTR_OFFSET(a, b) ((uint16_t)((uintptr_t)(b) - (uintptr_t)(a)))

static COLD void init_mode_node(EdgeBranch *const nwc,
                               const enum BlockLevel bl,
                               struct ModeSelMem *const mem,
                               const int top_has_right,
                               const int left_has_bottom)
{
    init_edges(&nwc->node, bl,
               (top_has_right ? EDGE_ALL_TOP_HAS_RIGHT : 0) |
               (left_has_bottom ? EDGE_ALL_LEFT_HAS_BOTTOM : 0));
    if (bl == BL_16X16) {
        for (int n = 0; n < 4; n++) {
            EdgeTip *const nt = mem->nt++;
            nwc->split_offset[n] = PTR_OFFSET(nwc, nt);
            init_edges(&nt->node, bl + 1,
                       ((n == 3 || (n == 1 && !top_has_right)) ? 0 :
                        EDGE_ALL_TOP_HAS_RIGHT) |
                       (!(n == 0 || (n == 2 && left_has_bottom)) ? 0 :
                        EDGE_ALL_LEFT_HAS_BOTTOM));
        }
    } else {
        for (int n = 0; n < 4; n++) {
            EdgeBranch *const nwc_child = mem->nwc[bl]++;
            nwc->split_offset[n] = PTR_OFFSET(nwc, nwc_child);
            init_mode_node(nwc_child, bl + 1, mem,
                           !(n == 3 || (n == 1 && !top_has_right)),
                           n == 0 || (n == 2 && left_has_bottom));
        }
    }
}

COLD void dav1d_init_intra_edge_tree(void) {
    // This function is guaranteed to be called only once
    struct ModeSelMem mem;

    mem.nwc[BL_128X128] = &nodes.branch_sb128[1];
    mem.nwc[BL_64X64] = &nodes.branch_sb128[1 + 4];
    mem.nwc[BL_32X32] = &nodes.branch_sb128[1 + 4 + 16];
    mem.nt = nodes.tip_sb128;
    init_mode_node(nodes.branch_sb128, BL_128X128, &mem, 1, 0);
    assert(mem.nwc[BL_128X128] == &nodes.branch_sb128[1 + 4]);
    assert(mem.nwc[BL_64X64] == &nodes.branch_sb128[1 + 4 + 16]);
    assert(mem.nwc[BL_32X32] == &nodes.branch_sb128[1 + 4 + 16 + 64]);
    assert(mem.nt == &nodes.tip_sb128[256]);

    mem.nwc[BL_128X128] = NULL;
    mem.nwc[BL_64X64] = &nodes.branch_sb64[1];
    mem.nwc[BL_32X32] = &nodes.branch_sb64[1 + 4];
    mem.nt = nodes.tip_sb64;
    init_mode_node(nodes.branch_sb64, BL_64X64, &mem, 1, 0);
    assert(mem.nwc[BL_64X64] == &nodes.branch_sb64[1 + 4]);
    assert(mem.nwc[BL_32X32] == &nodes.branch_sb64[1 + 4 + 16]);
    assert(mem.nt == &nodes.tip_sb64[64]);
}
