/*
 * Copyright © 2018, VideoLAN and dav1d authors
 * Copyright © 2018, Two Orioles, LLC
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

#include "common/intops.h"

#include "dav1d/headers.h"

#include "input/demuxer.h"
#include "input/parse.h"

// these functions are based on an implementation from FFmpeg, and relicensed
// with author's permission

#define PROBE_SIZE 1024

static int annexb_probe(const uint8_t *data) {
    int ret, cnt = 0;

    size_t temporal_unit_size;
    ret = leb(data + cnt, PROBE_SIZE - cnt, &temporal_unit_size);
    if (ret < 0)
        return 0;
    cnt += ret;

    size_t frame_unit_size;
    ret = leb(data + cnt, PROBE_SIZE - cnt, &frame_unit_size);
    if (ret < 0 || ((uint64_t)frame_unit_size + ret) > temporal_unit_size)
        return 0;
    cnt += ret;

    temporal_unit_size -= ret;

    size_t obu_unit_size;
    ret = leb(data + cnt, PROBE_SIZE - cnt, &obu_unit_size);
    if (ret < 0 || ((uint64_t)obu_unit_size + ret) >= frame_unit_size)
        return 0;
    cnt += ret;

    temporal_unit_size -= obu_unit_size + ret;
    frame_unit_size -= obu_unit_size + ret;

    // Check that the first OBU is a Temporal Delimiter.
    size_t obu_size;
    enum Dav1dObuType type;
    ret = parse_obu_header(data + cnt, imin(PROBE_SIZE - cnt, (int) obu_unit_size),
                           &obu_size, &type, 1);
    if (ret < 0 || type != DAV1D_OBU_TD || obu_size > 0)
        return 0;
    cnt += (int)obu_unit_size;

    // look for first frame and accompanying sequence header
    int seq = 0;
    while (cnt < PROBE_SIZE) {
        ret = leb(data + cnt, PROBE_SIZE - cnt, &obu_unit_size);
        if (ret < 0 || ((uint64_t)obu_unit_size + ret) > frame_unit_size)
            return 0;
        cnt += ret;
        temporal_unit_size -= ret;
        frame_unit_size -= ret;

        ret = parse_obu_header(data + cnt, imin(PROBE_SIZE - cnt, (int) obu_unit_size),
                               &obu_size, &type, 1);
        if (ret < 0)
            return 0;
        cnt += (int)obu_unit_size;

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

        temporal_unit_size -= obu_unit_size;
        frame_unit_size -= obu_unit_size;
        if (frame_unit_size <= 0)
            break;
    }

    return 0;
}

typedef struct DemuxerPriv {
    FILE *f;
    size_t temporal_unit_size;
    size_t frame_unit_size;
} AnnexbInputContext;

static int annexb_open(AnnexbInputContext *const c, const char *const file,
                       unsigned fps[2], unsigned *const num_frames, unsigned timebase[2])
{
    int res;
    size_t len;

    if (!(c->f = fopen(file, "rb"))) {
        fprintf(stderr, "Failed to open %s: %s\n", file, strerror(errno));
        return -1;
    }

    // TODO: Parse sequence header and read timing info if any.
    fps[0] = 25;
    fps[1] = 1;
    timebase[0] = 25;
    timebase[1] = 1;
    for (*num_frames = 0;; (*num_frames)++) {
        res = leb128(c->f, &len);
        if (res < 0)
            break;
        fseeko(c->f, len, SEEK_CUR);
    }
    fseeko(c->f, 0, SEEK_SET);

    return 0;
}

static int annexb_read(AnnexbInputContext *const c, Dav1dData *const data) {
    size_t len;
    int res;

    if (!c->temporal_unit_size) {
        res = leb128(c->f, &c->temporal_unit_size);
        if (res < 0) return -1;
    }
    if (!c->frame_unit_size) {
        res = leb128(c->f, &c->frame_unit_size);
        if (res < 0 || (c->frame_unit_size + res) > c->temporal_unit_size) return -1;
        c->temporal_unit_size -= res;
    }
    res = leb128(c->f, &len);
    if (res < 0 || (len + res) > c->frame_unit_size) return -1;
    uint8_t *ptr = dav1d_data_create(data, len);
    if (!ptr) return -1;
    c->temporal_unit_size -= len + res;
    c->frame_unit_size -= len + res;
    if (fread(ptr, len, 1, c->f) != 1) {
        fprintf(stderr, "Failed to read frame data: %s\n", strerror(errno));
        dav1d_data_unref(data);
        return -1;
    }

    return 0;
}

static void annexb_close(AnnexbInputContext *const c) {
    fclose(c->f);
}

const Demuxer annexb_demuxer = {
    .priv_data_size = sizeof(AnnexbInputContext),
    .name = "annexb",
    .probe = annexb_probe,
    .probe_sz = PROBE_SIZE,
    .open = annexb_open,
    .read = annexb_read,
    .seek = NULL,
    .close = annexb_close,
};
