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
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "input/demuxer.h"

typedef struct DemuxerPriv {
    FILE *f;
    int broken;
    double timebase;
    uint64_t last_ts;
    uint64_t step;
} IvfInputContext;

static const uint8_t probe_data[] = {
    'D', 'K', 'I', 'F',
    0, 0, 0x20, 0,
    'A', 'V', '0', '1',
};

static int ivf_probe(const uint8_t *const data) {
    return !memcmp(data, probe_data, sizeof(probe_data));
}

static unsigned rl32(const uint8_t *const p) {
    return ((uint32_t)p[3] << 24U) | (p[2] << 16U) | (p[1] << 8U) | p[0];
}

static int64_t rl64(const uint8_t *const p) {
    return (((uint64_t) rl32(&p[4])) << 32) | rl32(p);
}

static int ivf_open(IvfInputContext *const c, const char *const file,
                    unsigned fps[2], unsigned *const num_frames, unsigned timebase[2])
{
    uint8_t hdr[32];

    if (!(c->f = fopen(file, "rb"))) {
        fprintf(stderr, "Failed to open %s: %s\n", file, strerror(errno));
        return -1;
    } else if (fread(hdr, 32, 1, c->f) != 1) {
        fprintf(stderr, "Failed to read stream header: %s\n", strerror(errno));
        fclose(c->f);
        return -1;
    } else if (memcmp(hdr, "DKIF", 4)) {
        fprintf(stderr, "%s is not an IVF file [tag=%.4s|0x%02x%02x%02x%02x]\n",
                file, hdr, hdr[0], hdr[1], hdr[2], hdr[3]);
        fclose(c->f);
        return -1;
    } else if (memcmp(&hdr[8], "AV01", 4)) {
        fprintf(stderr, "%s is not an AV1 file [tag=%.4s|0x%02x%02x%02x%02x]\n",
                file, &hdr[8], hdr[8], hdr[9], hdr[10], hdr[11]);
        fclose(c->f);
        return -1;
    }

    timebase[0] = rl32(&hdr[16]);
    timebase[1] = rl32(&hdr[20]);
    const unsigned duration = rl32(&hdr[24]);

    uint8_t data[8];
    c->broken = 0;
    for (*num_frames = 0;; (*num_frames)++) {
        if (fread(data, 4, 1, c->f) != 1) break; // EOF
        size_t sz = rl32(data);
        if (fread(data, 8, 1, c->f) != 1) break; // EOF
        const uint64_t ts = rl64(data);
        if (*num_frames && ts <= c->last_ts)
            c->broken = 1;
        c->last_ts = ts;
        fseeko(c->f, sz, SEEK_CUR);
    }

    uint64_t fps_num = (uint64_t) timebase[0] * *num_frames;
    uint64_t fps_den = (uint64_t) timebase[1] * duration;
    if (fps_num && fps_den) { /* Reduce fraction */
        uint64_t gcd = fps_num;
        for (uint64_t a = fps_den, b; (b = a % gcd); a = gcd, gcd = b);
        fps_num /= gcd;
        fps_den /= gcd;

        while ((fps_num | fps_den) > UINT_MAX) {
            fps_num >>= 1;
            fps_den >>= 1;
        }
    }
    if (fps_num && fps_den) {
        fps[0] = (unsigned) fps_num;
        fps[1] = (unsigned) fps_den;
    } else {
        fps[0] = fps[1] = 0;
    }
    c->timebase = (double)timebase[0] / timebase[1];
    c->step = duration / *num_frames;

    fseeko(c->f, 32, SEEK_SET);
    c->last_ts = 0;

    return 0;
}

static inline int ivf_read_header(IvfInputContext *const c, ptrdiff_t *const sz,
                                  int64_t *const off_, uint64_t *const ts)
{
    uint8_t data[8];
    int64_t const off = ftello(c->f);
    if (off_) *off_ = off;
    if (fread(data, 4, 1, c->f) != 1) return -1; // EOF
    *sz = rl32(data);
    if (!c->broken) {
        if (fread(data, 8, 1, c->f) != 1) return -1;
        *ts = rl64(data);
    } else {
        if (fseeko(c->f, 8, SEEK_CUR)) return -1;
        *ts = off > 32 ? c->last_ts + c->step : 0;
    }
    return 0;
}

static int ivf_read(IvfInputContext *const c, Dav1dData *const buf) {
    uint8_t *ptr;
    ptrdiff_t sz;
    int64_t off;
    uint64_t ts;
    if (ivf_read_header(c, &sz, &off, &ts)) return -1;
    if (!(ptr = dav1d_data_create(buf, sz))) return -1;
    if (fread(ptr, sz, 1, c->f) != 1) {
        fprintf(stderr, "Failed to read frame data: %s\n", strerror(errno));
        dav1d_data_unref(buf);
        return -1;
    }
    buf->m.offset = off;
    buf->m.timestamp = ts;
    c->last_ts = ts;
    return 0;
}

static int ivf_seek(IvfInputContext *const c, const uint64_t pts) {
    uint64_t cur;
    const uint64_t ts = llround((pts * c->timebase) / 1000000000.0);
    if (ts <= c->last_ts)
        if (fseeko(c->f, 32, SEEK_SET)) goto error;
    while (1) {
        ptrdiff_t sz;
        if (ivf_read_header(c, &sz, NULL, &cur)) goto error;
        if (cur >= ts) break;
        if (fseeko(c->f, sz, SEEK_CUR)) goto error;
        c->last_ts = cur;
    }
    if (fseeko(c->f, -12, SEEK_CUR)) goto error;
    return 0;
error:
    fprintf(stderr, "Failed to seek: %s\n", strerror(errno));
    return -1;
}

static void ivf_close(IvfInputContext *const c) {
    fclose(c->f);
}

const Demuxer ivf_demuxer = {
    .priv_data_size = sizeof(IvfInputContext),
    .name = "ivf",
    .probe = ivf_probe,
    .probe_sz = sizeof(probe_data),
    .open = ivf_open,
    .read = ivf_read,
    .seek = ivf_seek,
    .close = ivf_close,
};
