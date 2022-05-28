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

#ifndef DAV1D_SRC_INTRA_EDGE_H
#define DAV1D_SRC_INTRA_EDGE_H

enum EdgeFlags {
    EDGE_I444_TOP_HAS_RIGHT = 1 << 0,
    EDGE_I422_TOP_HAS_RIGHT = 1 << 1,
    EDGE_I420_TOP_HAS_RIGHT = 1 << 2,
    EDGE_I444_LEFT_HAS_BOTTOM = 1 << 3,
    EDGE_I422_LEFT_HAS_BOTTOM = 1 << 4,
    EDGE_I420_LEFT_HAS_BOTTOM = 1 << 5,
};

typedef struct EdgeNode EdgeNode;
struct EdgeNode {
    enum EdgeFlags o, h[2], v[2];
};
typedef struct EdgeTip {
    EdgeNode node;
    enum EdgeFlags split[4];
} EdgeTip;
typedef struct EdgeBranch {
    EdgeNode node;
    enum EdgeFlags tts[3], tbs[3], tls[3], trs[3], h4[4], v4[4];
    EdgeNode *split[4];
} EdgeBranch;

void dav1d_init_mode_tree(EdgeNode *const root, EdgeTip *const nt,
                          const int allow_sb128);

#endif /* DAV1D_SRC_INTRA_EDGE_H */
