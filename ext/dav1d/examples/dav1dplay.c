/*
 * Copyright © 2019, VideoLAN and dav1d authors
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

#include <getopt.h>
#include <stdbool.h>

#include <SDL.h>

#include "dav1d/dav1d.h"

#include "common/attributes.h"
#include "tools/input/input.h"
#include "dp_fifo.h"
#include "dp_renderer.h"

#define FRAME_OFFSET_TO_PTS(foff) \
    (uint64_t)(((foff) * rd_ctx->spf) * 1000000000.0 + .5)
#define TS_TO_PTS(ts) \
    (uint64_t)(((ts) * rd_ctx->timebase) * 1000000000.0 + .5)

// Selected renderer callbacks and cookie
static const Dav1dPlayRenderInfo *renderer_info = { NULL };

/**
 * Render context structure
 * This structure contains informations necessary
 * to be shared between the decoder and the renderer
 * threads.
 */
typedef struct render_context
{
    Dav1dPlaySettings settings;
    Dav1dSettings lib_settings;

    // Renderer private data (passed to callbacks)
    void *rd_priv;

    // Lock to protect access to the context structure
    SDL_mutex *lock;

    // Timestamp of last displayed frame (in timebase unit)
    int64_t last_ts;
    // Timestamp of last decoded frame (in timebase unit)
    int64_t current_ts;
    // Ticks when last frame was received
    uint32_t last_ticks;
    // PTS time base
    double timebase;
    // Seconds per frame
    double spf;
    // Number of frames
    uint32_t total;

    // Fifo
    Dav1dPlayPtrFifo *fifo;

    // Custom SDL2 event types
    uint32_t event_types;

    // User pause state
    uint8_t user_paused;
    // Internal pause state
    uint8_t paused;
    // Start of internal pause state
    uint32_t pause_start;
    // Duration of internal pause state
    uint32_t pause_time;

    // Seek accumulator
    int seek;

    // Indicates if termination of the decoder thread was requested
    uint8_t dec_should_terminate;
} Dav1dPlayRenderContext;

static void dp_settings_print_usage(const char *const app,
                                    const char *const reason, ...)
{
    if (reason) {
        va_list args;

        va_start(args, reason);
        vfprintf(stderr, reason, args);
        va_end(args);
        fprintf(stderr, "\n\n");
    }
    fprintf(stderr, "Usage: %s [options]\n\n", app);
    fprintf(stderr, "Supported options:\n"
            " --input/-i  $file:    input file\n"
            " --untimed/-u:         ignore PTS, render as fast as possible\n"
            " --threads $num:       number of threads (default: 0)\n"
            " --framedelay $num:    maximum frame delay, capped at $threads (default: 0);\n"
            "                       set to 1 for low-latency decoding\n"
            " --highquality:        enable high quality rendering\n"
            " --zerocopy/-z:        enable zero copy upload path\n"
            " --gpugrain/-g:        enable GPU grain synthesis\n"
            " --version/-v:         print version and exit\n"
            " --renderer/-r:        select renderer backend (default: auto)\n");
    exit(1);
}

static unsigned parse_unsigned(const char *const optarg, const int option,
                               const char *const app)
{
    char *end;
    const unsigned res = (unsigned) strtoul(optarg, &end, 0);
    if (*end || end == optarg)
        dp_settings_print_usage(app, "Invalid argument \"%s\" for option %s; should be an integer",
          optarg, option);
    return res;
}

static void dp_rd_ctx_parse_args(Dav1dPlayRenderContext *rd_ctx,
                                 const int argc, char *const *const argv)
{
    int o;
    Dav1dPlaySettings *settings = &rd_ctx->settings;
    Dav1dSettings *lib_settings = &rd_ctx->lib_settings;

    // Short options
    static const char short_opts[] = "i:vuzgr:";

    enum {
        ARG_THREADS = 256,
        ARG_FRAME_DELAY,
        ARG_HIGH_QUALITY,
    };

    // Long options
    static const struct option long_opts[] = {
        { "input",          1, NULL, 'i' },
        { "version",        0, NULL, 'v' },
        { "untimed",        0, NULL, 'u' },
        { "threads",        1, NULL, ARG_THREADS },
        { "framedelay",     1, NULL, ARG_FRAME_DELAY },
        { "highquality",    0, NULL, ARG_HIGH_QUALITY },
        { "zerocopy",       0, NULL, 'z' },
        { "gpugrain",       0, NULL, 'g' },
        { "renderer",       0, NULL, 'r'},
        { NULL,             0, NULL, 0 },
    };

    while ((o = getopt_long(argc, argv, short_opts, long_opts, NULL)) != -1) {
        switch (o) {
            case 'i':
                settings->inputfile = optarg;
                break;
            case 'v':
                fprintf(stderr, "%s\n", dav1d_version());
                exit(0);
            case 'u':
                settings->untimed = true;
                break;
            case ARG_HIGH_QUALITY:
                settings->highquality = true;
                break;
            case 'z':
                settings->zerocopy = true;
                break;
            case 'g':
                settings->gpugrain = true;
                break;
            case 'r':
                settings->renderer_name = optarg;
                break;
            case ARG_THREADS:
                lib_settings->n_threads =
                    parse_unsigned(optarg, ARG_THREADS, argv[0]);
                break;
            case ARG_FRAME_DELAY:
                lib_settings->max_frame_delay =
                    parse_unsigned(optarg, ARG_FRAME_DELAY, argv[0]);
                break;
            default:
                dp_settings_print_usage(argv[0], NULL);
        }
    }

    if (optind < argc)
        dp_settings_print_usage(argv[0],
            "Extra/unused arguments found, e.g. '%s'\n", argv[optind]);
    if (!settings->inputfile)
        dp_settings_print_usage(argv[0], "Input file (-i/--input) is required");
    if (settings->renderer_name && strcmp(settings->renderer_name, "auto") == 0)
        settings->renderer_name = NULL;
}

/**
 * Destroy a Dav1dPlayRenderContext
 */
static void dp_rd_ctx_destroy(Dav1dPlayRenderContext *rd_ctx)
{
    assert(rd_ctx != NULL);

    renderer_info->destroy_renderer(rd_ctx->rd_priv);
    dp_fifo_destroy(rd_ctx->fifo);
    SDL_DestroyMutex(rd_ctx->lock);
    free(rd_ctx);
}

/**
 * Create a Dav1dPlayRenderContext
 *
 * \note  The Dav1dPlayRenderContext must be destroyed
 *        again by using dp_rd_ctx_destroy.
 */
static Dav1dPlayRenderContext *dp_rd_ctx_create(int argc, char **argv)
{
    Dav1dPlayRenderContext *rd_ctx;

    // Alloc
    rd_ctx = calloc(1, sizeof(Dav1dPlayRenderContext));
    if (rd_ctx == NULL) {
        return NULL;
    }

    // Register a custom event to notify our SDL main thread
    // about new frames
    rd_ctx->event_types = SDL_RegisterEvents(3);
    if (rd_ctx->event_types == UINT32_MAX) {
        fprintf(stderr, "Failure to create custom SDL event types!\n");
        free(rd_ctx);
        return NULL;
    }

    rd_ctx->fifo = dp_fifo_create(5);
    if (rd_ctx->fifo == NULL) {
        fprintf(stderr, "Failed to create FIFO for output pictures!\n");
        free(rd_ctx);
        return NULL;
    }

    rd_ctx->lock = SDL_CreateMutex();
    if (rd_ctx->lock == NULL) {
        fprintf(stderr, "SDL_CreateMutex failed: %s\n", SDL_GetError());
        dp_fifo_destroy(rd_ctx->fifo);
        free(rd_ctx);
        return NULL;
    }

    // Parse and validate arguments
    dav1d_default_settings(&rd_ctx->lib_settings);
    memset(&rd_ctx->settings, 0, sizeof(rd_ctx->settings));
    dp_rd_ctx_parse_args(rd_ctx, argc, argv);

    // Select renderer
    renderer_info = dp_get_renderer(rd_ctx->settings.renderer_name);

    if (renderer_info == NULL) {
        printf("No suitable renderer matching %s found.\n",
            (rd_ctx->settings.renderer_name) ? rd_ctx->settings.renderer_name : "auto");
    } else {
        printf("Using %s renderer\n", renderer_info->name);
    }

    rd_ctx->rd_priv = (renderer_info) ? renderer_info->create_renderer() : NULL;
    if (rd_ctx->rd_priv == NULL) {
        SDL_DestroyMutex(rd_ctx->lock);
        dp_fifo_destroy(rd_ctx->fifo);
        free(rd_ctx);
        return NULL;
    }

    return rd_ctx;
}

/**
 * Notify about new event
 */
static void dp_rd_ctx_post_event(Dav1dPlayRenderContext *rd_ctx, uint32_t type)
{
    SDL_Event event;
    SDL_zero(event);
    event.type = type;
    SDL_PushEvent(&event);
}

/**
 * Update the decoder context with a new dav1d picture
 *
 * Once the decoder decoded a new picture, this call can be used
 * to update the internal texture of the render context with the
 * new picture.
 */
static void dp_rd_ctx_update_with_dav1d_picture(Dav1dPlayRenderContext *rd_ctx,
                                                Dav1dPicture *dav1d_pic)
{
    rd_ctx->current_ts = dav1d_pic->m.timestamp;
    renderer_info->update_frame(rd_ctx->rd_priv, dav1d_pic, &rd_ctx->settings);
}

/**
 * Toggle pause state
 */
static void dp_rd_ctx_toggle_pause(Dav1dPlayRenderContext *rd_ctx)
{
    SDL_LockMutex(rd_ctx->lock);
    rd_ctx->user_paused = !rd_ctx->user_paused;
    if (rd_ctx->seek)
        goto out;
    rd_ctx->paused = rd_ctx->user_paused;
    uint32_t now = SDL_GetTicks();
    if (rd_ctx->paused)
        rd_ctx->pause_start = now;
    else {
        rd_ctx->pause_time += now - rd_ctx->pause_start;
        rd_ctx->pause_start = 0;
        rd_ctx->last_ticks = now;
    }
out:
    SDL_UnlockMutex(rd_ctx->lock);
}

/**
 * Query pause state
 */
static int dp_rd_ctx_is_paused(Dav1dPlayRenderContext *rd_ctx)
{
    int ret;
    SDL_LockMutex(rd_ctx->lock);
    ret = rd_ctx->paused;
    SDL_UnlockMutex(rd_ctx->lock);
    return ret;
}

/**
 * Request seeking, in seconds
 */
static void dp_rd_ctx_seek(Dav1dPlayRenderContext *rd_ctx, int sec)
{
    SDL_LockMutex(rd_ctx->lock);
    rd_ctx->seek += sec;
    if (!rd_ctx->paused)
        rd_ctx->pause_start = SDL_GetTicks();
    rd_ctx->paused = 1;
    SDL_UnlockMutex(rd_ctx->lock);
}

static int decode_frame(Dav1dPicture **p, Dav1dContext *c,
                        Dav1dData *data, DemuxerContext *in_ctx);
static inline void destroy_pic(void *a);

/**
 * Seek the stream, if requested
 */
static int dp_rd_ctx_handle_seek(Dav1dPlayRenderContext *rd_ctx,
                                 DemuxerContext *in_ctx,
                                 Dav1dContext *c, Dav1dData *data)
{
    int res = 0;
    SDL_LockMutex(rd_ctx->lock);
    if (!rd_ctx->seek)
        goto out;
    int64_t seek = rd_ctx->seek * 1000000000ULL;
    uint64_t pts = TS_TO_PTS(rd_ctx->current_ts);
    pts = ((int64_t)pts > -seek) ? pts + seek : 0;
    int end = pts >= FRAME_OFFSET_TO_PTS(rd_ctx->total);
    if (end)
        pts = FRAME_OFFSET_TO_PTS(rd_ctx->total - 1);
    uint64_t target_pts = pts;
    dav1d_flush(c);
    uint64_t shift = FRAME_OFFSET_TO_PTS(5);
    while (1) {
        if (shift > pts)
            shift = pts;
        if ((res = input_seek(in_ctx, pts - shift)))
            goto out;
        Dav1dSequenceHeader seq;
        uint64_t cur_pts;
        do {
            if ((res = input_read(in_ctx, data)))
                break;
            cur_pts = TS_TO_PTS(data->m.timestamp);
            res = dav1d_parse_sequence_header(&seq, data->data, data->sz);
        } while (res && cur_pts < pts);
        if (!res && cur_pts <= pts)
            break;
        if (shift > pts)
            shift = pts;
        pts -= shift;
    }
    if (!res) {
        pts = TS_TO_PTS(data->m.timestamp);
        while (pts < target_pts) {
            Dav1dPicture *p;
            if ((res = decode_frame(&p, c, data, in_ctx)))
                break;
            if (p) {
                pts = TS_TO_PTS(p->m.timestamp);
                if (pts < target_pts)
                    destroy_pic(p);
                else {
                    dp_fifo_push(rd_ctx->fifo, p);
                    uint32_t type = rd_ctx->event_types + DAV1D_EVENT_SEEK_FRAME;
                    dp_rd_ctx_post_event(rd_ctx, type);
                }
            }
        }
        if (!res) {
            rd_ctx->last_ts = data->m.timestamp - rd_ctx->spf / rd_ctx->timebase;
            rd_ctx->current_ts = data->m.timestamp;
        }
    }
out:
    rd_ctx->paused = rd_ctx->user_paused;
    if (!rd_ctx->paused && rd_ctx->seek) {
        uint32_t now = SDL_GetTicks();
        rd_ctx->pause_time += now - rd_ctx->pause_start;
        rd_ctx->pause_start = 0;
        rd_ctx->last_ticks = now;
    }
    rd_ctx->seek = 0;
    SDL_UnlockMutex(rd_ctx->lock);
    if (res)
        fprintf(stderr, "Error seeking, aborting\n");
    return res;
}

/**
 * Terminate decoder thread (async)
 */
static void dp_rd_ctx_request_shutdown(Dav1dPlayRenderContext *rd_ctx)
{
    SDL_LockMutex(rd_ctx->lock);
    rd_ctx->dec_should_terminate = 1;
    SDL_UnlockMutex(rd_ctx->lock);
}

/**
 * Query state of decoder shutdown request
 */
static int dp_rd_ctx_should_terminate(Dav1dPlayRenderContext *rd_ctx)
{
    int ret = 0;
    SDL_LockMutex(rd_ctx->lock);
    ret = rd_ctx->dec_should_terminate;
    SDL_UnlockMutex(rd_ctx->lock);
    return ret;
}

/**
 * Render the currently available texture
 *
 * Renders the currently available texture, if any.
 */
static void dp_rd_ctx_render(Dav1dPlayRenderContext *rd_ctx)
{
    SDL_LockMutex(rd_ctx->lock);
    // Calculate time since last frame was received
    uint32_t ticks_now = SDL_GetTicks();
    uint32_t ticks_diff = (rd_ctx->last_ticks != 0) ? ticks_now - rd_ctx->last_ticks : 0;

    // Calculate when to display the frame
    int64_t ts_diff = rd_ctx->current_ts - rd_ctx->last_ts;
    int32_t pts_diff = (ts_diff * rd_ctx->timebase) * 1000.0 + .5;
    int32_t wait_time = pts_diff - ticks_diff;

    // In untimed mode, simply don't wait
    if (rd_ctx->settings.untimed)
        wait_time = 0;

    // This way of timing the playback is not accurate, as there is no guarantee
    // that SDL_Delay will wait for exactly the requested amount of time so in a
    // accurate player this would need to be done in a better way.
    if (wait_time > 0) {
        SDL_Delay(wait_time);
    } else if (wait_time < -10 && !rd_ctx->paused) { // Do not warn for minor time drifts
        fprintf(stderr, "Frame displayed %f seconds too late\n", wait_time / 1000.0);
    }

    renderer_info->render(rd_ctx->rd_priv, &rd_ctx->settings);

    rd_ctx->last_ts = rd_ctx->current_ts;
    rd_ctx->last_ticks = SDL_GetTicks();

    SDL_UnlockMutex(rd_ctx->lock);
}

static int decode_frame(Dav1dPicture **p, Dav1dContext *c,
                        Dav1dData *data, DemuxerContext *in_ctx)
{
    int res;
    // Send data packets we got from the demuxer to dav1d
    if ((res = dav1d_send_data(c, data)) < 0) {
        // On EAGAIN, dav1d can not consume more data and
        // dav1d_get_picture needs to be called first, which
        // will happen below, so just keep going in that case
        // and do not error out.
        if (res != DAV1D_ERR(EAGAIN)) {
            dav1d_data_unref(data);
            goto err;
        }
    }
    *p = calloc(1, sizeof(**p));
    // Try to get a decoded frame
    if ((res = dav1d_get_picture(c, *p)) < 0) {
        // In all error cases, even EAGAIN, p needs to be freed as
        // it is never added to the queue and would leak.
        free(*p);
        *p = NULL;
        // On EAGAIN, it means dav1d has not enough data to decode
        // therefore this is not a decoding error but just means
        // we need to feed it more data, which happens in the next
        // run of the decoder loop.
        if (res != DAV1D_ERR(EAGAIN))
            goto err;
    }
    return data->sz == 0 ? input_read(in_ctx, data) : 0;
err:
    fprintf(stderr, "Error decoding frame: %s\n",
            strerror(-res));
    return res;
}

static inline void destroy_pic(void *a)
{
    Dav1dPicture *p = (Dav1dPicture *)a;
    dav1d_picture_unref(p);
    free(p);
}

/* Decoder thread "main" function */
static int decoder_thread_main(void *cookie)
{
    Dav1dPlayRenderContext *rd_ctx = cookie;

    Dav1dPicture *p;
    Dav1dContext *c = NULL;
    Dav1dData data;
    DemuxerContext *in_ctx = NULL;
    int res = 0;
    unsigned total, timebase[2], fps[2];

    Dav1dPlaySettings settings = rd_ctx->settings;

    if ((res = input_open(&in_ctx, "ivf",
                          settings.inputfile,
                          fps, &total, timebase)) < 0)
    {
        fprintf(stderr, "Failed to open demuxer\n");
        res = 1;
        goto cleanup;
    }

    rd_ctx->timebase = (double)timebase[1] / timebase[0];
    rd_ctx->spf = (double)fps[1] / fps[0];
    rd_ctx->total = total;

    if ((res = dav1d_open(&c, &rd_ctx->lib_settings))) {
        fprintf(stderr, "Failed opening dav1d decoder\n");
        res = 1;
        goto cleanup;
    }

    if ((res = input_read(in_ctx, &data)) < 0) {
        fprintf(stderr, "Failed demuxing input\n");
        res = 1;
        goto cleanup;
    }

    // Decoder loop
    while (1) {
        if (dp_rd_ctx_should_terminate(rd_ctx) ||
            (res = dp_rd_ctx_handle_seek(rd_ctx, in_ctx, c, &data)) ||
            (res = decode_frame(&p, c, &data, in_ctx)))
        {
            break;
        }
        else if (p) {
            // Queue frame
            SDL_LockMutex(rd_ctx->lock);
            int seek = rd_ctx->seek;
            SDL_UnlockMutex(rd_ctx->lock);
            if (!seek) {
                dp_fifo_push(rd_ctx->fifo, p);
                uint32_t type = rd_ctx->event_types + DAV1D_EVENT_NEW_FRAME;
                dp_rd_ctx_post_event(rd_ctx, type);
            }
        }
    }

    // Release remaining data
    if (data.sz > 0)
        dav1d_data_unref(&data);
    // Do not drain in case an error occured and caused us to leave the
    // decoding loop early.
    if (res < 0)
        goto cleanup;

    // Drain decoder
    // When there is no more data to feed to the decoder, for example
    // because the file ended, we still need to request pictures, as
    // even though we do not have more data, there can be frames decoded
    // from data we sent before. So we need to call dav1d_get_picture until
    // we get an EAGAIN error.
    do {
        if (dp_rd_ctx_should_terminate(rd_ctx))
            break;
        p = calloc(1, sizeof(*p));
        res = dav1d_get_picture(c, p);
        if (res < 0) {
            free(p);
            if (res != DAV1D_ERR(EAGAIN)) {
                fprintf(stderr, "Error decoding frame: %s\n",
                        strerror(-res));
                break;
            }
        } else {
            // Queue frame
            dp_fifo_push(rd_ctx->fifo, p);
            uint32_t type = rd_ctx->event_types + DAV1D_EVENT_NEW_FRAME;
            dp_rd_ctx_post_event(rd_ctx, type);
        }
    } while (res != DAV1D_ERR(EAGAIN));

cleanup:
    dp_rd_ctx_post_event(rd_ctx, rd_ctx->event_types + DAV1D_EVENT_DEC_QUIT);

    if (in_ctx)
        input_close(in_ctx);
    if (c)
        dav1d_close(&c);

    return (res != DAV1D_ERR(EAGAIN) && res < 0);
}

int main(int argc, char **argv)
{
    SDL_Thread *decoder_thread;

    // Check for version mismatch between library and tool
    const char *version = dav1d_version();
    if (strcmp(version, DAV1D_VERSION)) {
        fprintf(stderr, "Version mismatch (library: %s, executable: %s)\n",
                version, DAV1D_VERSION);
        return 1;
    }

    // Init SDL2 library
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0)
        return 10;

    // Create render context
    Dav1dPlayRenderContext *rd_ctx = dp_rd_ctx_create(argc, argv);
    if (rd_ctx == NULL) {
        fprintf(stderr, "Failed creating render context\n");
        return 5;
    }

    if (rd_ctx->settings.zerocopy) {
        if (renderer_info->alloc_pic) {
            rd_ctx->lib_settings.allocator = (Dav1dPicAllocator) {
                .cookie = rd_ctx->rd_priv,
                .alloc_picture_callback = renderer_info->alloc_pic,
                .release_picture_callback = renderer_info->release_pic,
            };
        } else {
            fprintf(stderr, "--zerocopy unsupported by selected renderer\n");
        }
    }

    if (rd_ctx->settings.gpugrain) {
        if (renderer_info->supports_gpu_grain) {
            rd_ctx->lib_settings.apply_grain = 0;
        } else {
            fprintf(stderr, "--gpugrain unsupported by selected renderer\n");
        }
    }

    // Start decoder thread
    decoder_thread = SDL_CreateThread(decoder_thread_main, "Decoder thread", rd_ctx);

    // Main loop
#define NUM_MAX_EVENTS 8
    SDL_Event events[NUM_MAX_EVENTS];
    int num_frame_events = 0;
    uint32_t start_time = 0, n_out = 0;
    while (1) {
        int num_events = 0;
        SDL_WaitEvent(NULL);
        while (num_events < NUM_MAX_EVENTS && SDL_PollEvent(&events[num_events++]))
            break;
        for (int i = 0; i < num_events; ++i) {
            SDL_Event *e = &events[i];
            if (e->type == SDL_QUIT) {
                dp_rd_ctx_request_shutdown(rd_ctx);
                dp_fifo_flush(rd_ctx->fifo, destroy_pic);
                SDL_FlushEvent(rd_ctx->event_types + DAV1D_EVENT_NEW_FRAME);
                SDL_FlushEvent(rd_ctx->event_types + DAV1D_EVENT_SEEK_FRAME);
                num_frame_events = 0;
            } else if (e->type == SDL_WINDOWEVENT) {
                if (e->window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                    // TODO: Handle window resizes
                } else if(e->window.event == SDL_WINDOWEVENT_EXPOSED) {
                    dp_rd_ctx_render(rd_ctx);
                }
            } else if (e->type == SDL_KEYDOWN) {
                SDL_KeyboardEvent *kbde = (SDL_KeyboardEvent *)e;
                if (kbde->keysym.sym == SDLK_SPACE) {
                    dp_rd_ctx_toggle_pause(rd_ctx);
                } else if (kbde->keysym.sym == SDLK_LEFT ||
                           kbde->keysym.sym == SDLK_RIGHT)
                {
                    if (kbde->keysym.sym == SDLK_LEFT)
                        dp_rd_ctx_seek(rd_ctx, -5);
                    else if (kbde->keysym.sym == SDLK_RIGHT)
                        dp_rd_ctx_seek(rd_ctx, +5);
                    dp_fifo_flush(rd_ctx->fifo, destroy_pic);
                    SDL_FlushEvent(rd_ctx->event_types + DAV1D_EVENT_NEW_FRAME);
                    num_frame_events = 0;
                }
            } else if (e->type == rd_ctx->event_types + DAV1D_EVENT_NEW_FRAME) {
                num_frame_events++;
                // Store current ticks for stats calculation
                if (start_time == 0)
                    start_time = SDL_GetTicks();
            } else if (e->type == rd_ctx->event_types + DAV1D_EVENT_SEEK_FRAME) {
                // Dequeue frame and update the render context with it
                Dav1dPicture *p = dp_fifo_shift(rd_ctx->fifo);
                // Do not update textures during termination
                if (!dp_rd_ctx_should_terminate(rd_ctx)) {
                    dp_rd_ctx_update_with_dav1d_picture(rd_ctx, p);
                    n_out++;
                }
                destroy_pic(p);
            } else if (e->type == rd_ctx->event_types + DAV1D_EVENT_DEC_QUIT) {
                goto out;
            }
        }
        if (num_frame_events && !dp_rd_ctx_is_paused(rd_ctx)) {
            // Dequeue frame and update the render context with it
            Dav1dPicture *p = dp_fifo_shift(rd_ctx->fifo);
            // Do not update textures during termination
            if (!dp_rd_ctx_should_terminate(rd_ctx)) {
                dp_rd_ctx_update_with_dav1d_picture(rd_ctx, p);
                dp_rd_ctx_render(rd_ctx);
                n_out++;
            }
            destroy_pic(p);
            num_frame_events--;
        }
    }

out:;
    // Print stats
    uint32_t time_ms = SDL_GetTicks() - start_time - rd_ctx->pause_time;
    printf("Decoded %u frames in %d seconds, avg %.02f fps\n",
           n_out, time_ms / 1000, n_out/ (time_ms / 1000.0));

    int decoder_ret = 0;
    SDL_WaitThread(decoder_thread, &decoder_ret);
    dp_rd_ctx_destroy(rd_ctx);
    return decoder_ret;
}
