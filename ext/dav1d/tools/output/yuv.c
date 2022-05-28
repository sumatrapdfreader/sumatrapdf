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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "output/muxer.h"

typedef struct MuxerPriv {
    FILE *f;
} YuvOutputContext;

static int yuv_open(YuvOutputContext *const c, const char *const file,
                    const Dav1dPictureParameters *const p,
                    const unsigned fps[2])
{
    if (!strcmp(file, "-")) {
        c->f = stdout;
    } else if (!(c->f = fopen(file, "wb"))) {
        fprintf(stderr, "Failed to open %s: %s\n", file, strerror(errno));
        return -1;
    }

    return 0;
}

static int yuv_write(YuvOutputContext *const c, Dav1dPicture *const p) {
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

static void yuv_close(YuvOutputContext *const c) {
    if (c->f != stdout)
        fclose(c->f);
}

const Muxer yuv_muxer = {
    .priv_data_size = sizeof(YuvOutputContext),
    .name = "yuv",
    .extension = "yuv",
    .write_header = yuv_open,
    .write_picture = yuv_write,
    .write_trailer = yuv_close,
};
