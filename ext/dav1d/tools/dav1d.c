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
#include "vcs_version.h"
#include "cli_config.h"

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef HAVE_IO_H
# include <io.h>
#endif
#ifdef _WIN32
# include <windows.h>
#endif
#ifdef __APPLE__
#include <mach/mach_time.h>
#endif

#include "dav1d/dav1d.h"

#include "input/input.h"

#include "output/output.h"

#include "dav1d_cli_parse.h"

static uint64_t get_time_nanos(void) {
#ifdef _WIN32
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    uint64_t seconds = t.QuadPart / frequency.QuadPart;
    uint64_t fractions = t.QuadPart % frequency.QuadPart;
    return 1000000000 * seconds + 1000000000 * fractions / frequency.QuadPart;
#elif defined(HAVE_CLOCK_GETTIME)
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return 1000000000ULL * ts.tv_sec + ts.tv_nsec;
#elif defined(__APPLE__)
    mach_timebase_info_data_t info;
    mach_timebase_info(&info);
    return mach_absolute_time() * info.numer / info.denom;
#endif
}

static void sleep_nanos(uint64_t d) {
#ifdef _WIN32
    Sleep((unsigned)(d / 1000000));
#else
    const struct timespec ts = {
        .tv_sec = (time_t)(d / 1000000000),
        .tv_nsec = d % 1000000000,
    };
    nanosleep(&ts, NULL);
#endif
}

static void synchronize(const int realtime, const unsigned cache,
                        const unsigned n_out, const uint64_t nspf,
                        const uint64_t tfirst, uint64_t *const elapsed,
                        FILE *const frametimes)
{
    const uint64_t tcurr = get_time_nanos();
    const uint64_t last = *elapsed;
    *elapsed = tcurr - tfirst;
    if (realtime) {
        const uint64_t deadline = nspf * n_out;
        if (*elapsed < deadline) {
            const uint64_t remaining = deadline - *elapsed;
            if (remaining > nspf * cache) sleep_nanos(remaining - nspf * cache);
            *elapsed = deadline;
        }
    }
    if (frametimes) {
        const uint64_t frametime = *elapsed - last;
        fprintf(frametimes, "%" PRIu64 "\n", frametime);
        fflush(frametimes);
    }
}

static void print_stats(const int istty, const unsigned n, const unsigned num,
                        const uint64_t elapsed, const double i_fps)
{
    char buf[80], *b = buf, *const end = buf + 80;

    if (istty)
        *b++ = '\r';
    if (num == 0xFFFFFFFF)
        b += snprintf(b, end - b, "Decoded %u frames", n);
    else
        b += snprintf(b, end - b, "Decoded %u/%u frames (%.1lf%%)",
                      n, num, 100.0 * n / num);
    if (b < end) {
        const double d_fps = 1e9 * n / elapsed;
        if (i_fps) {
            const double speed = d_fps / i_fps;
            b += snprintf(b, end - b, " - %.2lf/%.2lf fps (%.2lfx)",
                          d_fps, i_fps, speed);
        } else {
            b += snprintf(b, end - b, " - %.2lf fps", d_fps);
        }
    }
    if (!istty)
        strcpy(b > end - 2 ? end - 2 : b, "\n");
    fputs(buf, stderr);
}

static int picture_alloc(Dav1dPicture *const p, void *const _) {
    const int hbd = p->p.bpc > 8;
    const int aligned_w = (p->p.w + 127) & ~127;
    const int aligned_h = (p->p.h + 127) & ~127;
    const int has_chroma = p->p.layout != DAV1D_PIXEL_LAYOUT_I400;
    const int ss_ver = p->p.layout == DAV1D_PIXEL_LAYOUT_I420;
    const int ss_hor = p->p.layout != DAV1D_PIXEL_LAYOUT_I444;
    ptrdiff_t y_stride = aligned_w << hbd;
    ptrdiff_t uv_stride = has_chroma ? y_stride >> ss_hor : 0;
    /* Due to how mapping of addresses to sets works in most L1 and L2 cache
     * implementations, strides of multiples of certain power-of-two numbers
     * may cause multiple rows of the same superblock to map to the same set,
     * causing evictions of previous rows resulting in a reduction in cache
     * hit rate. Avoid that by slightly padding the stride when necessary. */
    if (!(y_stride & 1023))
        y_stride += DAV1D_PICTURE_ALIGNMENT;
    if (!(uv_stride & 1023) && has_chroma)
        uv_stride += DAV1D_PICTURE_ALIGNMENT;
    p->stride[0] = -y_stride;
    p->stride[1] = -uv_stride;
    const size_t y_sz = y_stride * aligned_h;
    const size_t uv_sz = uv_stride * (aligned_h >> ss_ver);
    const size_t pic_size = y_sz + 2 * uv_sz;

    uint8_t *const buf = malloc(pic_size + DAV1D_PICTURE_ALIGNMENT * 2);
    if (!buf) return DAV1D_ERR(ENOMEM);
    p->allocator_data = buf;

    const ptrdiff_t align_m1 = DAV1D_PICTURE_ALIGNMENT - 1;
    uint8_t *const data = (uint8_t *)(((ptrdiff_t)buf + align_m1) & ~align_m1);
    p->data[0] = data + y_sz - y_stride;
    p->data[1] = has_chroma ? data + y_sz + uv_sz * 1 - uv_stride : NULL;
    p->data[2] = has_chroma ? data + y_sz + uv_sz * 2 - uv_stride : NULL;

    return 0;
}

static void picture_release(Dav1dPicture *const p, void *const _) {
    free(p->allocator_data);
}

int main(const int argc, char *const *const argv) {
    const int istty = isatty(fileno(stderr));
    int res = 0;
    CLISettings cli_settings;
    Dav1dSettings lib_settings;
    DemuxerContext *in;
    MuxerContext *out = NULL;
    Dav1dPicture p;
    Dav1dContext *c;
    Dav1dData data;
    unsigned n_out = 0, total, fps[2], timebase[2];
    uint64_t nspf, tfirst, elapsed;
    double i_fps;
    FILE *frametimes = NULL;
    const char *version = dav1d_version();

    if (strcmp(version, DAV1D_VERSION)) {
        fprintf(stderr, "Version mismatch (library: %s, executable: %s)\n",
                version, DAV1D_VERSION);
        return EXIT_FAILURE;
    }

    parse(argc, argv, &cli_settings, &lib_settings);
    if (cli_settings.neg_stride) {
        lib_settings.allocator.alloc_picture_callback = picture_alloc;
        lib_settings.allocator.release_picture_callback = picture_release;
    }

    if ((res = input_open(&in, cli_settings.demuxer,
                          cli_settings.inputfile,
                          fps, &total, timebase)) < 0)
    {
        return EXIT_FAILURE;
    }
    for (unsigned i = 0; i <= cli_settings.skip; i++) {
        if ((res = input_read(in, &data)) < 0) {
            input_close(in);
            return EXIT_FAILURE;
        }
        if (i < cli_settings.skip) dav1d_data_unref(&data);
    }

    if (!cli_settings.quiet)
        fprintf(stderr, "dav1d %s - by VideoLAN\n", dav1d_version());

    // skip frames until a sequence header is found
    if (cli_settings.skip) {
        Dav1dSequenceHeader seq;
        unsigned seq_skip = 0;
        while (dav1d_parse_sequence_header(&seq, data.data, data.sz)) {
            if ((res = input_read(in, &data)) < 0) {
                input_close(in);
                return EXIT_FAILURE;
            }
            seq_skip++;
        }
        if (seq_skip && !cli_settings.quiet)
            fprintf(stderr,
                    "skipped %u packets due to missing sequence header\n",
                    seq_skip);
    }

    if (cli_settings.limit != 0 && cli_settings.limit < total)
        total = cli_settings.limit;

    if ((res = dav1d_open(&c, &lib_settings)))
        return EXIT_FAILURE;

    if (cli_settings.frametimes)
        frametimes = fopen(cli_settings.frametimes, "w");

    if (cli_settings.realtime != REALTIME_CUSTOM) {
        if (fps[1] == 0) {
            i_fps = 0;
            nspf = 0;
        } else {
            i_fps = (double)fps[0] / fps[1];
            nspf = 1000000000ULL * fps[1] / fps[0];
        }
    } else {
        i_fps = cli_settings.realtime_fps;
        nspf = (uint64_t)(1000000000.0 / cli_settings.realtime_fps);
    }
    tfirst = get_time_nanos();

    do {
        memset(&p, 0, sizeof(p));
        if ((res = dav1d_send_data(c, &data)) < 0) {
            if (res != DAV1D_ERR(EAGAIN)) {
                dav1d_data_unref(&data);
                fprintf(stderr, "Error decoding frame: %s\n",
                        strerror(DAV1D_ERR(res)));
                if (res != DAV1D_ERR(EINVAL)) break;
            }
        }

        if ((res = dav1d_get_picture(c, &p)) < 0) {
            if (res != DAV1D_ERR(EAGAIN)) {
                fprintf(stderr, "Error decoding frame: %s\n",
                        strerror(DAV1D_ERR(res)));
                if (res != DAV1D_ERR(EINVAL)) break;
            }
            res = 0;
        } else {
            if (!n_out) {
                if ((res = output_open(&out, cli_settings.muxer,
                                       cli_settings.outputfile,
                                       &p.p, fps)) < 0)
                {
                    if (frametimes) fclose(frametimes);
                    return EXIT_FAILURE;
                }
            }
            if ((res = output_write(out, &p)) < 0)
                break;
            n_out++;
            if (nspf || !cli_settings.quiet) {
                synchronize(cli_settings.realtime, cli_settings.realtime_cache,
                            n_out, nspf, tfirst, &elapsed, frametimes);
            }
            if (!cli_settings.quiet)
                print_stats(istty, n_out, total, elapsed, i_fps);
        }

        if (cli_settings.limit && n_out == cli_settings.limit)
            break;
    } while (data.sz > 0 || !input_read(in, &data));

    if (data.sz > 0) dav1d_data_unref(&data);

    // flush
    if (res == 0) while (!cli_settings.limit || n_out < cli_settings.limit) {
        if ((res = dav1d_get_picture(c, &p)) < 0) {
            if (res != DAV1D_ERR(EAGAIN)) {
                fprintf(stderr, "Error decoding frame: %s\n",
                        strerror(DAV1D_ERR(res)));
                if (res != DAV1D_ERR(EINVAL)) break;
            } else {
                res = 0;
                break;
            }
        } else {
            if (!n_out) {
                if ((res = output_open(&out, cli_settings.muxer,
                                       cli_settings.outputfile,
                                       &p.p, fps)) < 0)
                {
                    if (frametimes) fclose(frametimes);
                    return EXIT_FAILURE;
                }
            }
            if ((res = output_write(out, &p)) < 0)
                break;
            n_out++;
            if (nspf || !cli_settings.quiet) {
                synchronize(cli_settings.realtime, cli_settings.realtime_cache,
                            n_out, nspf, tfirst, &elapsed, frametimes);
            }
            if (!cli_settings.quiet)
                print_stats(istty, n_out, total, elapsed, i_fps);
        }
    }

    if (frametimes) fclose(frametimes);

    input_close(in);
    if (out) {
        if (!cli_settings.quiet && istty)
            fprintf(stderr, "\n");
        if (cli_settings.verify)
            res |= output_verify(out, cli_settings.verify);
        else
            output_close(out);
    } else {
        fprintf(stderr, "No data decoded\n");
        res = 1;
    }
    dav1d_close(&c);

    return (res == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
