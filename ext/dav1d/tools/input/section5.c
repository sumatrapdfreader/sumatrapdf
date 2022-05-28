/*
 * Copyright © 2019, VideoLAN and dav1d authors
 * Copyright © 2019, Two Orioles, LLC
 * Copyright © 2019, James Almer <jamrial@gmail.com>
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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "dav1d/headers.h"

#include "input/demuxer.h"
#include "input/parse.h"

#define PROBE_SIZE 1024

static int section5_probe(const uint8_t *data) {
    int ret, cnt = 0;

    // Check that the first OBU is a Temporal Delimiter.
    size_t obu_size;
    enum Dav1dObuType type;
    ret = parse_obu_header(data + cnt, PROBE_SIZE - cnt,
                           &obu_size, &type, 0);
    if (ret < 0 || type != DAV1D_OBU_TD || obu_size > 0)
        return 0;
    cnt += ret;

    // look for first frame and accompanying sequence header
    int seq = 0;
    while (cnt < PROBE_SIZE) {
        ret = parse_obu_header(data + cnt, PROBE_SIZE - cnt,
                               &obu_size, &type, 0);
        if (ret < 0)
            return 0;
        cnt += ret;

        switch (type) {
        case DAV1D_OBU_SEQ_HDR:
            seq = 1;
            break;
        case DAV1D_OBU_FRAME:
        case DAV1D_OBU_FRAME_HDR:
            return seq;
        case DAV1D_OBU_TD:
        case DAV1D_OBU_TILE_GRP:
            return 0;
        default:
            break;
        }
    }

    return 0;
}

typedef struct DemuxerPriv {
    FILE *f;
} Section5InputContext;

static int section5_open(Section5InputContext *const c, const char *const file,
                         unsigned fps[2], unsigned *const num_frames, unsigned timebase[2])
{
    if (!(c->f = fopen(file, "rb"))) {
        fprintf(stderr, "Failed to open %s: %s\n", file, strerror(errno));
        return -1;
    }

    // TODO: Parse sequence header and read timing info if any.
    fps[0] = 25;
    fps[1] = 1;
    timebase[0] = 25;
    timebase[1] = 1;
    *num_frames = 0;
    for (;;) {
        uint8_t byte[2];

        if (fread(&byte[0], 1, 1, c->f) < 1)
            break;
        const enum Dav1dObuType obu_type = (byte[0] >> 3) & 0xf;
        if (obu_type == DAV1D_OBU_TD)
            (*num_frames)++;
        const int has_length_field = byte[0] & 0x2;
        if (!has_length_field)
            return -1;
        const int has_extension = byte[0] & 0x4;
        if (has_extension && fread(&byte[1], 1, 1, c->f) < 1)
            return -1;
        size_t len;
        const int res = leb128(c->f, &len);
        if (res < 0)
            return -1;
        fseeko(c->f, len, SEEK_CUR); // skip packet
    }
    fseeko(c->f, 0, SEEK_SET);

    return 0;
}

static int section5_read(Section5InputContext *const c, Dav1dData *const data) {
    size_t total_bytes = 0;

    for (int first = 1;; first = 0) {
        uint8_t byte[2];

        if (fread(&byte[0], 1, 1, c->f) < 1) {
            if (!first && feof(c->f)) break;
            return -1;
        }
        const enum Dav1dObuType obu_type = (byte[0] >> 3) & 0xf;
        if (first) {
            if (obu_type != DAV1D_OBU_TD)
                return -1;
        } else {
            if (obu_type == DAV1D_OBU_TD) {
                // include TD in next packet
                fseeko(c->f, -1, SEEK_CUR);
                break;
            }
        }
        const int has_length_field = byte[0] & 0x2;
        if (!has_length_field)
            return -1;
        const int has_extension = !!(byte[0] & 0x4);
        if (has_extension && fread(&byte[1], 1, 1, c->f) < 1)
            return -1;
        size_t len;
        const int res = leb128(c->f, &len);
        if (res < 0)
            return -1;
        total_bytes += 1 + has_extension + res + len;
        fseeko(c->f, len, SEEK_CUR); // skip packet, we'll read it below
    }

    fseeko(c->f, -(off_t)total_bytes, SEEK_CUR);
    uint8_t *ptr = dav1d_data_create(data, total_bytes);
    if (!ptr) return -1;
    if (fread(ptr, total_bytes, 1, c->f) != 1) {
        fprintf(stderr, "Failed to read frame data: %s\n", strerror(errno));
        dav1d_data_unref(data);
        return -1;
    }

    return 0;
}

static void section5_close(Section5InputContext *const c) {
    fclose(c->f);
}

const Demuxer section5_demuxer = {
    .priv_data_size = sizeof(Section5InputContext),
    .name = "section5",
    .probe = section5_probe,
    .probe_sz = PROBE_SIZE,
    .open = section5_open,
    .read = section5_read,
    .seek = NULL,
    .close = section5_close,
};
