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
#include "cli_config.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/attributes.h"
#include "common/intops.h"

#include "output/output.h"
#include "output/muxer.h"

struct MuxerContext {
    MuxerPriv *data;
    const Muxer *impl;
    int one_file_per_frame;
    unsigned fps[2];
    const char *filename;
    int framenum;
};

extern const Muxer null_muxer;
extern const Muxer md5_muxer;
extern const Muxer xxh3_muxer;
extern const Muxer yuv_muxer;
extern const Muxer y4m2_muxer;
static const Muxer *muxers[] = {
    &null_muxer,
    &md5_muxer,
#if HAVE_XXHASH_H
    &xxh3_muxer,
#endif
    &yuv_muxer,
    &y4m2_muxer,
    NULL
};

static const char *find_extension(const char *const f) {
    const size_t l = strlen(f);

    if (l == 0) return NULL;

    const char *const end = &f[l - 1], *step = end;
    while ((*step >= 'a' && *step <= 'z') ||
           (*step >= 'A' && *step <= 'Z') ||
           (*step >= '0' && *step <= '9'))
    {
        step--;
    }

    return (step < end && step > f && *step == '.' && step[-1] != '/') ?
           &step[1] : NULL;
}

int output_open(MuxerContext **const c_out,
                const char *const name, const char *const filename,
                const Dav1dPictureParameters *const p, const unsigned fps[2])
{
    const Muxer *impl;
    MuxerContext *c;
    unsigned i;
    int res;
    int name_offset = 0;

    if (name) {
        name_offset = 5 * !strncmp(name, "frame", 5);
        for (i = 0; muxers[i]; i++) {
            if (!strcmp(muxers[i]->name, &name[name_offset])) {
                impl = muxers[i];
                break;
            }
        }
        if (!muxers[i]) {
            fprintf(stderr, "Failed to find muxer named \"%s\"\n", name);
            return DAV1D_ERR(ENOPROTOOPT);
        }
    } else if (!strcmp(filename, "/dev/null")) {
        impl = muxers[0];
    } else {
        const char *const ext = find_extension(filename);
        if (!ext) {
            fprintf(stderr, "No extension found for file %s\n", filename);
            return -1;
        }
        for (i = 0; muxers[i]; i++) {
            if (!strcmp(muxers[i]->extension, ext)) {
                impl = muxers[i];
                break;
            }
        }
        if (!muxers[i]) {
            fprintf(stderr, "Failed to find muxer for extension \"%s\"\n", ext);
            return DAV1D_ERR(ENOPROTOOPT);
        }
    }

    if (!(c = malloc(sizeof(MuxerContext) + impl->priv_data_size))) {
        fprintf(stderr, "Failed to allocate memory\n");
        return DAV1D_ERR(ENOMEM);
    }
    c->impl = impl;
    c->data = (MuxerPriv *) &c[1];
    int have_num_pattern = 0;
    for (const char *ptr = filename ? strchr(filename, '%') : NULL;
         !have_num_pattern && ptr; ptr = strchr(ptr, '%'))
    {
        ptr++; // skip '%'
        while (*ptr >= '0' && *ptr <= '9')
            ptr++; // skip length indicators
        have_num_pattern = *ptr == 'n';
    }
    c->one_file_per_frame = name_offset || (!name && have_num_pattern);

    if (c->one_file_per_frame) {
        c->fps[0] = fps[0];
        c->fps[1] = fps[1];
        c->filename = filename;
        c->framenum = 0;
    } else if (impl->write_header &&
               (res = impl->write_header(c->data, filename, p, fps)) < 0)
    {
        free(c);
        return res;
    }
    *c_out = c;

    return 0;
}

static void safe_strncat(char *const dst, const int dst_len,
                         const char *const src, const int src_len)
{
    if (!src_len) return;
    const int dst_fill = (int) strlen(dst);
    assert(dst_fill < dst_len);
    const int to_copy = imin(src_len, dst_len - dst_fill - 1);
    if (!to_copy) return;
    memcpy(dst + dst_fill, src, to_copy);
    dst[dst_fill + to_copy] = 0;
}

static void assemble_field(char *const dst, const int dst_len,
                           const char *const fmt, const int fmt_len,
                           const int field)
{
    char fmt_copy[32];

    assert(fmt[0] == '%');
    fmt_copy[0] = '%';
    if (fmt[1] >= '1' && fmt[1] <= '9') {
        fmt_copy[1] = '0'; // pad with zeroes, not spaces
        fmt_copy[2] = 0;
    } else {
        fmt_copy[1] = 0;
    }
    safe_strncat(fmt_copy, sizeof(fmt_copy), &fmt[1], fmt_len - 1);
    safe_strncat(fmt_copy, sizeof(fmt_copy), "d", 1);

    char tmp[32];
    snprintf(tmp, sizeof(tmp), fmt_copy, field);

    safe_strncat(dst, dst_len, tmp, (int) strlen(tmp));
}

static void assemble_filename(MuxerContext *const ctx, char *const filename,
                              const int filename_size,
                              const Dav1dPictureParameters *const p)
{
    filename[0] = 0;
    const int framenum = ctx->framenum++;
    assert(ctx->filename);
    const char *ptr = ctx->filename, *iptr;
    while ((iptr = strchr(ptr, '%'))) {
        safe_strncat(filename, filename_size, ptr, (int) (iptr - ptr));
        ptr = iptr;

        const char *iiptr = &iptr[1]; // skip '%'
        while (*iiptr >= '0' && *iiptr <= '9')
            iiptr++; // skip length indicators

        switch (*iiptr) {
        case 'w':
            assemble_field(filename, filename_size, ptr, (int) (iiptr - ptr), p->w);
            break;
        case 'h':
            assemble_field(filename, filename_size, ptr, (int) (iiptr - ptr), p->h);
            break;
        case 'n':
            assemble_field(filename, filename_size, ptr, (int) (iiptr - ptr), framenum);
            break;
        default:
            safe_strncat(filename, filename_size, "%", 1);
            ptr = &iptr[1];
            continue;
        }

        ptr = &iiptr[1];
    }
    safe_strncat(filename, filename_size, ptr, (int) strlen(ptr));
}

int output_write(MuxerContext *const ctx, Dav1dPicture *const p) {
    int res;

    if (ctx->one_file_per_frame && ctx->impl->write_header) {
        char filename[1024];
        assemble_filename(ctx, filename, sizeof(filename), &p->p);
        res = ctx->impl->write_header(ctx->data, filename, &p->p, ctx->fps);
        if (res < 0)
            return res;
    }
    if ((res = ctx->impl->write_picture(ctx->data, p)) < 0)
        return res;
    if (ctx->one_file_per_frame && ctx->impl->write_trailer)
        ctx->impl->write_trailer(ctx->data);

    return 0;
}

void output_close(MuxerContext *const ctx) {
    if (!ctx->one_file_per_frame && ctx->impl->write_trailer)
        ctx->impl->write_trailer(ctx->data);
    free(ctx);
}

int output_verify(MuxerContext *const ctx, const char *const md5_str) {
    const int res = ctx->impl->verify ?
        ctx->impl->verify(ctx->data, md5_str) : 0;
    free(ctx);
    return res;
}
