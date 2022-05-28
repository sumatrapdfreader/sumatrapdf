/*
 * Copyright © 2018, VideoLAN and dav1d authors
 * Copyright © 2018, Janne Grunau
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
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include <dav1d/dav1d.h>
#include "src/cpu.h"
#include "dav1d_fuzzer.h"

#ifdef DAV1D_ALLOC_FAIL

#include "alloc_fail.h"

static unsigned djb_xor(const uint8_t * c, size_t len) {
    unsigned hash = 5381;
    for(size_t i = 0; i < len; i++)
        hash = hash * 33 ^ c[i];
    return hash;
}
#endif

static unsigned r32le(const uint8_t *const p) {
    return ((uint32_t)p[3] << 24U) | (p[2] << 16U) | (p[1] << 8U) | p[0];
}

#define DAV1D_FUZZ_MAX_SIZE 4096 * 4096

// search for "--cpumask xxx" in argv and remove both parameters
int LLVMFuzzerInitialize(int *argc, char ***argv) {
    int i = 1;
    for (; i < *argc; i++) {
        if (!strcmp((*argv)[i], "--cpumask")) {
            const char * cpumask = (*argv)[i+1];
            if (cpumask) {
                char *end;
                unsigned res;
                if (!strncmp(cpumask, "0x", 2)) {
                    cpumask += 2;
                    res = (unsigned) strtoul(cpumask, &end, 16);
                } else {
                    res = (unsigned) strtoul(cpumask, &end, 0);
                }
                if (end != cpumask && !end[0]) {
                    dav1d_set_cpu_flags_mask(res);
                }
            }
            break;
        }
    }

    for (; i < *argc - 2; i++) {
        (*argv)[i] = (*argv)[i + 2];
    }

    *argc = i;

    return 0;
}


// expects ivf input

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    Dav1dSettings settings = { 0 };
    Dav1dContext * ctx = NULL;
    Dav1dPicture pic;
    const uint8_t *ptr = data;
    int have_seq_hdr = 0;
    int err;

    dav1d_version();

    if (size < 32) goto end;
#ifdef DAV1D_ALLOC_FAIL
    unsigned h = djb_xor(ptr, 32);
    unsigned seed = h;
    unsigned probability = h > (RAND_MAX >> 5) ? RAND_MAX >> 5 : h;
    int max_frame_delay = (h & 0xf) + 1;
    int n_threads = ((h >> 4) & 0x7) + 1;
    if (max_frame_delay > 5) max_frame_delay = 1;
    if (n_threads > 3) n_threads = 1;
#endif
    ptr += 32; // skip ivf header

    dav1d_default_settings(&settings);

#ifdef DAV1D_MT_FUZZING
    settings.max_frame_delay = settings.n_threads = 4;
#elif defined(DAV1D_ALLOC_FAIL)
    settings.max_frame_delay = max_frame_delay;
    settings.n_threads = n_threads;
    dav1d_setup_alloc_fail(seed, probability);
#else
    settings.max_frame_delay = settings.n_threads = 1;
#endif
#if defined(DAV1D_FUZZ_MAX_SIZE)
    settings.frame_size_limit = DAV1D_FUZZ_MAX_SIZE;
#endif

    err = dav1d_open(&ctx, &settings);
    if (err < 0) goto end;

    while (ptr <= data + size - 12) {
        Dav1dData buf;
        uint8_t *p;

        size_t frame_size = r32le(ptr);
        ptr += 12;

        if (frame_size > size || ptr > data + size - frame_size)
            break;

        if (!frame_size) continue;

        if (!have_seq_hdr) {
            Dav1dSequenceHeader seq = { 0 };
            int err = dav1d_parse_sequence_header(&seq, ptr, frame_size);
            // skip frames until we see a sequence header
            if  (err != 0) {
                ptr += frame_size;
                continue;
            }
            have_seq_hdr = 1;
        }

        // copy frame data to a new buffer to catch reads past the end of input
        p = dav1d_data_create(&buf, frame_size);
        if (!p) goto cleanup;
        memcpy(p, ptr, frame_size);
        ptr += frame_size;

        do {
            if ((err = dav1d_send_data(ctx, &buf)) < 0) {
                if (err != DAV1D_ERR(EAGAIN))
                    break;
            }
            memset(&pic, 0, sizeof(pic));
            err = dav1d_get_picture(ctx, &pic);
            if (err == 0) {
                dav1d_picture_unref(&pic);
            } else if (err != DAV1D_ERR(EAGAIN)) {
                break;
            }
        } while (buf.sz > 0);

        if (buf.sz > 0)
            dav1d_data_unref(&buf);
    }

    memset(&pic, 0, sizeof(pic));
    if ((err = dav1d_get_picture(ctx, &pic)) == 0) {
        /* Test calling dav1d_picture_unref() after dav1d_close() */
        do {
            Dav1dPicture pic2 = { 0 };
            if ((err = dav1d_get_picture(ctx, &pic2)) == 0)
                dav1d_picture_unref(&pic2);
        } while (err != DAV1D_ERR(EAGAIN));

        dav1d_close(&ctx);
        dav1d_picture_unref(&pic);
        return 0;
    }

cleanup:
    dav1d_close(&ctx);
end:
    return 0;
}
