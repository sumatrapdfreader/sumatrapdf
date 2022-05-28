/*
 * Copyright © 2018-2021, VideoLAN and dav1d authors
 * Copyright © 2018-2021, Two Orioles, LLC
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

#define XXH_INLINE_ALL
#include "xxhash.h"

#include "output/muxer.h"

typedef struct MuxerPriv {
    XXH3_state_t* state;
    FILE *f;
} xxh3Context;

static int xxh3_open(xxh3Context *const xxh3, const char *const file,
                    const Dav1dPictureParameters *const p,
                    const unsigned fps[2])
{
    xxh3->state = XXH3_createState();
    if (!xxh3->state) return DAV1D_ERR(ENOMEM);
    XXH_errorcode err = XXH3_128bits_reset(xxh3->state);
    if (err != XXH_OK) {
        XXH3_freeState(xxh3->state);
        xxh3->state = NULL;
        return DAV1D_ERR(ENOMEM);
    }

    if (!strcmp(file, "-")) {
        xxh3->f = stdout;
    } else if (!(xxh3->f = fopen(file, "wb"))) {
        XXH3_freeState(xxh3->state);
        xxh3->state = NULL;
        fprintf(stderr, "Failed to open %s: %s\n", file, strerror(errno));
        return -1;
    }

    return 0;
}

static int xxh3_write(xxh3Context *const xxh3, Dav1dPicture *const p) {
    const int hbd = p->p.bpc > 8;
    const int w = p->p.w, h = p->p.h;
    uint8_t *yptr = p->data[0];

    for (int y = 0; y < h; y++) {
        XXH3_128bits_update(xxh3->state, yptr, w << hbd);
        yptr += p->stride[0];
    }

    if (p->p.layout != DAV1D_PIXEL_LAYOUT_I400) {
        const int ss_ver = p->p.layout == DAV1D_PIXEL_LAYOUT_I420;
        const int ss_hor = p->p.layout != DAV1D_PIXEL_LAYOUT_I444;
        const int cw = (w + ss_hor) >> ss_hor;
        const int ch = (h + ss_ver) >> ss_ver;
        for (int pl = 1; pl <= 2; pl++) {
            uint8_t *uvptr = p->data[pl];

            for (int y = 0; y < ch; y++) {
                XXH3_128bits_update(xxh3->state, uvptr, cw << hbd);
                uvptr += p->stride[1];
            }
        }
    }

    dav1d_picture_unref(p);

    return 0;
}

static void xxh3_close(xxh3Context *const xxh3) {
    XXH128_hash_t hash = XXH3_128bits_digest(xxh3->state);
    XXH3_freeState(xxh3->state);
    XXH128_canonical_t c;
    XXH128_canonicalFromHash(&c, hash);

    for (int i = 0; i < 16; i++)
        fprintf(xxh3->f, "%2.2x", c.digest[i]);
    fprintf(xxh3->f, "\n");

    if (xxh3->f != stdout)
        fclose(xxh3->f);
}

static int xxh3_verify(xxh3Context *const xxh3, const char * xxh3_str) {
    XXH128_hash_t hash = XXH3_128bits_digest(xxh3->state);
    XXH3_freeState(xxh3->state);

    if (strlen(xxh3_str) < 32)
        return -1;

    XXH128_canonical_t c;
    char t[3] = { 0 };
    for (int i = 0; i < 16; i++) {
        char *ignore;
        memcpy(t, xxh3_str, 2);
        xxh3_str += 2;
        c.digest[i] = (unsigned char) strtoul(t, &ignore, 16);
    }
    XXH128_hash_t verify = XXH128_hashFromCanonical(&c);

    return !XXH128_isEqual(hash, verify);
}

const Muxer xxh3_muxer = {
    .priv_data_size = sizeof(xxh3Context),
    .name = "xxh3",
    .extension = "xxh3",
    .write_header = xxh3_open,
    .write_picture = xxh3_write,
    .write_trailer = xxh3_close,
    .verify = xxh3_verify,
};
