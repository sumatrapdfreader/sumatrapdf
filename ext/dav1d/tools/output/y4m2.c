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

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "output/muxer.h"

typedef struct MuxerPriv {
    FILE *f;
    int first;
    unsigned fps[2];
} Y4m2OutputContext;

static int y4m2_open(Y4m2OutputContext *const c, const char *const file,
                     const Dav1dPictureParameters *p, const unsigned fps[2])
{
    if (!strcmp(file, "-")) {
        c->f = stdout;
    } else if (!(c->f = fopen(file, "wb"))) {
        fprintf(stderr, "Failed to open %s: %s\n", file, strerror(errno));
        return -1;
    }

    c->first = 1;
    c->fps[0] = fps[0];
    c->fps[1] = fps[1];

    return 0;
}

static int write_header(Y4m2OutputContext *const c, const Dav1dPicture *const p) {
    static const char *const ss_names[][3] = {
        [DAV1D_PIXEL_LAYOUT_I400] = { "mono", "mono10", "mono12" },
        [DAV1D_PIXEL_LAYOUT_I420] = { NULL,   "420p10", "420p12" },
        [DAV1D_PIXEL_LAYOUT_I422] = { "422",  "422p10", "422p12" },
        [DAV1D_PIXEL_LAYOUT_I444] = { "444",  "444p10", "444p12" }
    };

    static const char *const chr_names_8bpc_i420[] = {
        [DAV1D_CHR_UNKNOWN] = "420jpeg",
        [DAV1D_CHR_VERTICAL] = "420mpeg2",
        [DAV1D_CHR_COLOCATED] = "420"
    };

    const char *const ss_name =
        p->p.layout == DAV1D_PIXEL_LAYOUT_I420 && p->p.bpc == 8 ?
        chr_names_8bpc_i420[p->seq_hdr->chr > 2 ? DAV1D_CHR_UNKNOWN : p->seq_hdr->chr] :
        ss_names[p->p.layout][p->seq_hdr->hbd];

    const unsigned fw = p->p.w;
    const unsigned fh = p->p.h;
    uint64_t aw = (uint64_t)fh * p->frame_hdr->render_width;
    uint64_t ah = (uint64_t)fw * p->frame_hdr->render_height;
    uint64_t gcd = ah;
    for (uint64_t a = aw, b; (b = a % gcd); a = gcd, gcd = b);
    aw /= gcd;
    ah /= gcd;

    fprintf(c->f, "YUV4MPEG2 W%u H%u F%u:%u Ip A%"PRIu64":%"PRIu64" C%s\n",
            fw, fh, c->fps[0], c->fps[1], aw, ah, ss_name);

    return 0;
}

static int y4m2_write(Y4m2OutputContext *const c, Dav1dPicture *const p) {
    if (c->first) {
        c->first = 0;
        const int res = write_header(c, p);
        if (res < 0) return res;
    }
    fprintf(c->f, "FRAME\n");

    uint8_t *ptr;
    const int hbd = p->p.bpc > 8;

    ptr = p->data[0];
    for (int y = 0; y < p->p.h; y++) {
        if (fwrite(ptr, p->p.w << hbd, 1, c->f) != 1)
            goto error;
        ptr += p->stride[0];
    }

    if (p->p.layout != DAV1D_PIXEL_LAYOUT_I400) {
        // u/v
        const int ss_ver = p->p.layout == DAV1D_PIXEL_LAYOUT_I420;
        const int ss_hor = p->p.layout != DAV1D_PIXEL_LAYOUT_I444;
        const int cw = (p->p.w + ss_hor) >> ss_hor;
        const int ch = (p->p.h + ss_ver) >> ss_ver;
        for (int pl = 1; pl <= 2; pl++) {
            ptr = p->data[pl];
            for (int y = 0; y < ch; y++) {
                if (fwrite(ptr, cw << hbd, 1, c->f) != 1)
                    goto error;
                ptr += p->stride[1];
            }
        }
    }

    dav1d_picture_unref(p);
    return 0;

error:
    dav1d_picture_unref(p);
    fprintf(stderr, "Failed to write frame data: %s\n", strerror(errno));
    return -1;
}

static void y4m2_close(Y4m2OutputContext *const c) {
    if (c->f != stdout)
        fclose(c->f);
}

const Muxer y4m2_muxer = {
    .priv_data_size = sizeof(Y4m2OutputContext),
    .name = "yuv4mpeg2",
    .extension = "y4m",
    .write_header = y4m2_open,
    .write_picture = y4m2_write,
    .write_trailer = y4m2_close,
};
