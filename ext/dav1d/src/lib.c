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

#include <errno.h>
#include <string.h>

#if defined(__linux__) && HAVE_DLSYM
#include <dlfcn.h>
#endif

#include "dav1d/dav1d.h"
#include "dav1d/data.h"

#include "common/validate.h"

#include "src/cpu.h"
#include "src/fg_apply.h"
#include "src/internal.h"
#include "src/log.h"
#include "src/obu.h"
#include "src/qm.h"
#include "src/ref.h"
#include "src/thread_task.h"
#include "src/wedge.h"

static COLD void init_internal(void) {
    dav1d_init_cpu();
    dav1d_init_ii_wedge_masks();
    dav1d_init_intra_edge_tree();
    dav1d_init_qm_tables();
    dav1d_init_thread();
}

COLD const char *dav1d_version(void) {
    return DAV1D_VERSION;
}

COLD unsigned dav1d_version_api(void) {
    return (DAV1D_API_VERSION_MAJOR << 16) |
           (DAV1D_API_VERSION_MINOR <<  8) |
           (DAV1D_API_VERSION_PATCH <<  0);
}

COLD void dav1d_default_settings(Dav1dSettings *const s) {
    s->n_threads = 0;
    s->max_frame_delay = 0;
    s->apply_grain = 1;
    s->allocator.cookie = NULL;
    s->allocator.alloc_picture_callback = dav1d_default_picture_alloc;
    s->allocator.release_picture_callback = dav1d_default_picture_release;
    s->logger.cookie = NULL;
    s->logger.callback = dav1d_log_default_callback;
    s->operating_point = 0;
    s->all_layers = 1; // just until the tests are adjusted
    s->frame_size_limit = 0;
    s->strict_std_compliance = 0;
    s->output_invisible_frames = 0;
    s->inloop_filters = DAV1D_INLOOPFILTER_ALL;
    s->decode_frame_type = DAV1D_DECODEFRAMETYPE_ALL;
}

static void close_internal(Dav1dContext **const c_out, int flush);

NO_SANITIZE("cfi-icall") // CFI is broken with dlsym()
static COLD size_t get_stack_size_internal(const pthread_attr_t *const thread_attr) {
#if defined(__linux__) && HAVE_DLSYM && defined(__GLIBC__)
    /* glibc has an issue where the size of the TLS is subtracted from the stack
     * size instead of allocated separately. As a result the specified stack
     * size may be insufficient when used in an application with large amounts
     * of TLS data. The following is a workaround to compensate for that.
     * See https://sourceware.org/bugzilla/show_bug.cgi?id=11787 */
    size_t (*const get_minstack)(const pthread_attr_t*) =
        dlsym(RTLD_DEFAULT, "__pthread_get_minstack");
    if (get_minstack)
        return get_minstack(thread_attr) - PTHREAD_STACK_MIN;
#endif
    return 0;
}

static COLD void get_num_threads(Dav1dContext *const c, const Dav1dSettings *const s,
                                 unsigned *n_tc, unsigned *n_fc)
{
    /* ceil(sqrt(n)) */
    static const uint8_t fc_lut[49] = {
        1,                                     /*     1 */
        2, 2, 2,                               /*  2- 4 */
        3, 3, 3, 3, 3,                         /*  5- 9 */
        4, 4, 4, 4, 4, 4, 4,                   /* 10-16 */
        5, 5, 5, 5, 5, 5, 5, 5, 5,             /* 17-25 */
        6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,       /* 26-36 */
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, /* 37-49 */
    };
    *n_tc = s->n_threads ? s->n_threads :
        iclip(dav1d_num_logical_processors(c), 1, DAV1D_MAX_THREADS);
    *n_fc = s->max_frame_delay ? umin(s->max_frame_delay, *n_tc) :
            *n_tc < 50 ? fc_lut[*n_tc - 1] : 8; // min(8, ceil(sqrt(n)))
}

COLD int dav1d_get_frame_delay(const Dav1dSettings *const s) {
    unsigned n_tc, n_fc;
    validate_input_or_ret(s != NULL, DAV1D_ERR(EINVAL));
    validate_input_or_ret(s->n_threads >= 0 &&
                          s->n_threads <= DAV1D_MAX_THREADS, DAV1D_ERR(EINVAL));
    validate_input_or_ret(s->max_frame_delay >= 0 &&
                          s->max_frame_delay <= DAV1D_MAX_FRAME_DELAY, DAV1D_ERR(EINVAL));

    get_num_threads(NULL, s, &n_tc, &n_fc);
    return n_fc;
}

COLD int dav1d_open(Dav1dContext **const c_out, const Dav1dSettings *const s) {
    static pthread_once_t initted = PTHREAD_ONCE_INIT;
    pthread_once(&initted, init_internal);

    validate_input_or_ret(c_out != NULL, DAV1D_ERR(EINVAL));
    validate_input_or_ret(s != NULL, DAV1D_ERR(EINVAL));
    validate_input_or_ret(s->n_threads >= 0 &&
                          s->n_threads <= DAV1D_MAX_THREADS, DAV1D_ERR(EINVAL));
    validate_input_or_ret(s->max_frame_delay >= 0 &&
                          s->max_frame_delay <= DAV1D_MAX_FRAME_DELAY, DAV1D_ERR(EINVAL));
    validate_input_or_ret(s->allocator.alloc_picture_callback != NULL,
                          DAV1D_ERR(EINVAL));
    validate_input_or_ret(s->allocator.release_picture_callback != NULL,
                          DAV1D_ERR(EINVAL));
    validate_input_or_ret(s->operating_point >= 0 &&
                          s->operating_point <= 31, DAV1D_ERR(EINVAL));
    validate_input_or_ret(s->decode_frame_type >= DAV1D_DECODEFRAMETYPE_ALL &&
                          s->decode_frame_type <= DAV1D_DECODEFRAMETYPE_KEY, DAV1D_ERR(EINVAL));

    pthread_attr_t thread_attr;
    if (pthread_attr_init(&thread_attr)) return DAV1D_ERR(ENOMEM);
    size_t stack_size = 1024 * 1024 + get_stack_size_internal(&thread_attr);

    pthread_attr_setstacksize(&thread_attr, stack_size);

    Dav1dContext *const c = *c_out = dav1d_alloc_aligned(ALLOC_COMMON_CTX, sizeof(*c), 64);
    if (!c) goto error;
    memset(c, 0, sizeof(*c));

    c->allocator = s->allocator;
    c->logger = s->logger;
    c->apply_grain = s->apply_grain;
    c->operating_point = s->operating_point;
    c->all_layers = s->all_layers;
    c->frame_size_limit = s->frame_size_limit;
    c->strict_std_compliance = s->strict_std_compliance;
    c->output_invisible_frames = s->output_invisible_frames;
    c->inloop_filters = s->inloop_filters;
    c->decode_frame_type = s->decode_frame_type;

    dav1d_data_props_set_defaults(&c->cached_error_props);

    if (dav1d_mem_pool_init(ALLOC_OBU_HDR, &c->seq_hdr_pool) ||
        dav1d_mem_pool_init(ALLOC_OBU_HDR, &c->frame_hdr_pool) ||
        dav1d_mem_pool_init(ALLOC_SEGMAP, &c->segmap_pool) ||
        dav1d_mem_pool_init(ALLOC_REFMVS, &c->refmvs_pool) ||
        dav1d_mem_pool_init(ALLOC_PIC_CTX, &c->pic_ctx_pool) ||
        dav1d_mem_pool_init(ALLOC_CDF, &c->cdf_pool))
    {
        goto error;
    }

    if (c->allocator.alloc_picture_callback   == dav1d_default_picture_alloc &&
        c->allocator.release_picture_callback == dav1d_default_picture_release)
    {
        if (c->allocator.cookie) goto error;
        if (dav1d_mem_pool_init(ALLOC_PIC, &c->picture_pool)) goto error;
        c->allocator.cookie = c->picture_pool;
    } else if (c->allocator.alloc_picture_callback   == dav1d_default_picture_alloc ||
               c->allocator.release_picture_callback == dav1d_default_picture_release)
    {
        goto error;
    }

    /* On 32-bit systems extremely large frame sizes can cause overflows in
     * dav1d_decode_frame() malloc size calculations. Prevent that from occuring
     * by enforcing a maximum frame size limit, chosen to roughly correspond to
     * the largest size possible to decode without exhausting virtual memory. */
    if (sizeof(size_t) < 8 && s->frame_size_limit - 1 >= 8192 * 8192) {
        c->frame_size_limit = 8192 * 8192;
        if (s->frame_size_limit)
            dav1d_log(c, "Frame size limit reduced from %u to %u.\n",
                      s->frame_size_limit, c->frame_size_limit);
    }

    c->flush = &c->flush_mem;
    atomic_init(c->flush, 0);

    get_num_threads(c, s, &c->n_tc, &c->n_fc);

    c->fc = dav1d_alloc_aligned(ALLOC_THREAD_CTX, sizeof(*c->fc) * c->n_fc, 32);
    if (!c->fc) goto error;
    memset(c->fc, 0, sizeof(*c->fc) * c->n_fc);

    c->tc = dav1d_alloc_aligned(ALLOC_THREAD_CTX, sizeof(*c->tc) * c->n_tc, 64);
    if (!c->tc) goto error;
    memset(c->tc, 0, sizeof(*c->tc) * c->n_tc);
    if (c->n_tc > 1) {
        if (pthread_mutex_init(&c->task_thread.lock, NULL)) goto error;
        if (pthread_cond_init(&c->task_thread.cond, NULL)) {
            pthread_mutex_destroy(&c->task_thread.lock);
            goto error;
        }
        if (pthread_cond_init(&c->task_thread.delayed_fg.cond, NULL)) {
            pthread_cond_destroy(&c->task_thread.cond);
            pthread_mutex_destroy(&c->task_thread.lock);
            goto error;
        }
        c->task_thread.cur = c->n_fc;
        atomic_init(&c->task_thread.reset_task_cur, UINT_MAX);
        atomic_init(&c->task_thread.cond_signaled, 0);
        c->task_thread.inited = 1;
    }

    if (c->n_fc > 1) {
        const size_t out_delayed_sz = sizeof(*c->frame_thread.out_delayed) * c->n_fc;
        c->frame_thread.out_delayed =
            dav1d_malloc(ALLOC_THREAD_CTX, out_delayed_sz);
        if (!c->frame_thread.out_delayed) goto error;
        memset(c->frame_thread.out_delayed, 0, out_delayed_sz);
    }
    for (unsigned n = 0; n < c->n_fc; n++) {
        Dav1dFrameContext *const f = &c->fc[n];
        if (c->n_tc > 1) {
            if (pthread_mutex_init(&f->task_thread.lock, NULL)) goto error;
            if (pthread_cond_init(&f->task_thread.cond, NULL)) {
                pthread_mutex_destroy(&f->task_thread.lock);
                goto error;
            }
            if (pthread_mutex_init(&f->task_thread.pending_tasks.lock, NULL)) {
                pthread_cond_destroy(&f->task_thread.cond);
                pthread_mutex_destroy(&f->task_thread.lock);
                goto error;
            }
        }
        f->c = c;
        f->task_thread.ttd = &c->task_thread;
        f->lf.last_sharpness = -1;
    }

    for (unsigned m = 0; m < c->n_tc; m++) {
        Dav1dTaskContext *const t = &c->tc[m];
        t->f = &c->fc[0];
        t->task_thread.ttd = &c->task_thread;
        t->c = c;
        memset(t->cf_16bpc, 0, sizeof(t->cf_16bpc));
        if (c->n_tc > 1) {
            if (pthread_mutex_init(&t->task_thread.td.lock, NULL)) goto error;
            if (pthread_cond_init(&t->task_thread.td.cond, NULL)) {
                pthread_mutex_destroy(&t->task_thread.td.lock);
                goto error;
            }
            if (pthread_create(&t->task_thread.td.thread, &thread_attr, dav1d_worker_task, t)) {
                pthread_cond_destroy(&t->task_thread.td.cond);
                pthread_mutex_destroy(&t->task_thread.td.lock);
                goto error;
            }
            t->task_thread.td.inited = 1;
        }
    }
    dav1d_pal_dsp_init(&c->pal_dsp);
    dav1d_refmvs_dsp_init(&c->refmvs_dsp);

    pthread_attr_destroy(&thread_attr);

    return 0;

error:
    if (c) close_internal(c_out, 0);
    pthread_attr_destroy(&thread_attr);
    return DAV1D_ERR(ENOMEM);
}

static int has_grain(const Dav1dPicture *const pic)
{
    const Dav1dFilmGrainData *fgdata = &pic->frame_hdr->film_grain.data;
    return fgdata->num_y_points || fgdata->num_uv_points[0] ||
           fgdata->num_uv_points[1] || (fgdata->clip_to_restricted_range &&
                                        fgdata->chroma_scaling_from_luma);
}

static int output_image(Dav1dContext *const c, Dav1dPicture *const out)
{
    int res = 0;

    Dav1dThreadPicture *const in = (c->all_layers || !c->max_spatial_id)
                                   ? &c->out : &c->cache;
    if (!c->apply_grain || !has_grain(&in->p)) {
        dav1d_picture_move_ref(out, &in->p);
        dav1d_thread_picture_unref(in);
        goto end;
    }

    res = dav1d_apply_grain(c, out, &in->p);
    dav1d_thread_picture_unref(in);
end:
    if (!c->all_layers && c->max_spatial_id && c->out.p.data[0]) {
        dav1d_thread_picture_move_ref(in, &c->out);
    }
    return res;
}

static int output_picture_ready(Dav1dContext *const c, const int drain) {
    if (c->cached_error) return 1;
    if (!c->all_layers && c->max_spatial_id) {
        if (c->out.p.data[0] && c->cache.p.data[0]) {
            if (c->max_spatial_id == c->cache.p.frame_hdr->spatial_id ||
                c->out.flags & PICTURE_FLAG_NEW_TEMPORAL_UNIT)
                return 1;
            dav1d_thread_picture_unref(&c->cache);
            dav1d_thread_picture_move_ref(&c->cache, &c->out);
            return 0;
        } else if (c->cache.p.data[0] && drain) {
            return 1;
        } else if (c->out.p.data[0]) {
            dav1d_thread_picture_move_ref(&c->cache, &c->out);
            return 0;
        }
    }

    return !!c->out.p.data[0];
}

static int drain_picture(Dav1dContext *const c, Dav1dPicture *const out) {
    unsigned drain_count = 0;
    int drained = 0;
    do {
        const unsigned next = c->frame_thread.next;
        Dav1dFrameContext *const f = &c->fc[next];
        pthread_mutex_lock(&c->task_thread.lock);
        while (f->n_tile_data > 0)
            pthread_cond_wait(&f->task_thread.cond,
                              &f->task_thread.ttd->lock);
        Dav1dThreadPicture *const out_delayed =
            &c->frame_thread.out_delayed[next];
        if (out_delayed->p.data[0] || atomic_load(&f->task_thread.error)) {
            unsigned first = atomic_load(&c->task_thread.first);
            if (first + 1U < c->n_fc)
                atomic_fetch_add(&c->task_thread.first, 1U);
            else
                atomic_store(&c->task_thread.first, 0);
            atomic_compare_exchange_strong(&c->task_thread.reset_task_cur,
                                           &first, UINT_MAX);
            if (c->task_thread.cur && c->task_thread.cur < c->n_fc)
                c->task_thread.cur--;
            drained = 1;
        } else if (drained) {
            pthread_mutex_unlock(&c->task_thread.lock);
            break;
        }
        if (++c->frame_thread.next == c->n_fc)
            c->frame_thread.next = 0;
        pthread_mutex_unlock(&c->task_thread.lock);
        const int error = f->task_thread.retval;
        if (error) {
            f->task_thread.retval = 0;
            dav1d_data_props_copy(&c->cached_error_props, &out_delayed->p.m);
            dav1d_thread_picture_unref(out_delayed);
            return error;
        }
        if (out_delayed->p.data[0]) {
            const unsigned progress =
                atomic_load_explicit(&out_delayed->progress[1],
                                     memory_order_relaxed);
            if ((out_delayed->visible || c->output_invisible_frames) &&
                progress != FRAME_ERROR)
            {
                dav1d_thread_picture_ref(&c->out, out_delayed);
                c->event_flags |= dav1d_picture_get_event_flags(out_delayed);
            }
            dav1d_thread_picture_unref(out_delayed);
            if (output_picture_ready(c, 0))
                return output_image(c, out);
        }
    } while (++drain_count < c->n_fc);

    if (output_picture_ready(c, 1))
        return output_image(c, out);

    return DAV1D_ERR(EAGAIN);
}

static int gen_picture(Dav1dContext *const c)
{
    Dav1dData *const in = &c->in;

    if (output_picture_ready(c, 0))
        return 0;

    while (in->sz > 0) {
        const ptrdiff_t res = dav1d_parse_obus(c, in);
        if (res < 0) {
            dav1d_data_unref_internal(in);
        } else {
            assert((size_t)res <= in->sz);
            in->sz -= res;
            in->data += res;
            if (!in->sz) dav1d_data_unref_internal(in);
        }
        if (output_picture_ready(c, 0))
            break;
        if (res < 0)
            return (int)res;
    }

    return 0;
}

int dav1d_send_data(Dav1dContext *const c, Dav1dData *const in)
{
    validate_input_or_ret(c != NULL, DAV1D_ERR(EINVAL));
    validate_input_or_ret(in != NULL, DAV1D_ERR(EINVAL));

    if (in->data) {
        validate_input_or_ret(in->sz > 0 && in->sz <= SIZE_MAX / 2, DAV1D_ERR(EINVAL));
        c->drain = 0;
    }
    if (c->in.data)
        return DAV1D_ERR(EAGAIN);
    dav1d_data_ref(&c->in, in);

    int res = gen_picture(c);
    if (!res)
        dav1d_data_unref_internal(in);

    return res;
}

int dav1d_get_picture(Dav1dContext *const c, Dav1dPicture *const out)
{
    validate_input_or_ret(c != NULL, DAV1D_ERR(EINVAL));
    validate_input_or_ret(out != NULL, DAV1D_ERR(EINVAL));

    const int drain = c->drain;
    c->drain = 1;

    int res = gen_picture(c);
    if (res < 0)
        return res;

    if (c->cached_error) {
        const int res = c->cached_error;
        c->cached_error = 0;
        return res;
    }

    if (output_picture_ready(c, c->n_fc == 1))
        return output_image(c, out);

    if (c->n_fc > 1 && drain)
        return drain_picture(c, out);

    return DAV1D_ERR(EAGAIN);
}

int dav1d_apply_grain(Dav1dContext *const c, Dav1dPicture *const out,
                      const Dav1dPicture *const in)
{
    validate_input_or_ret(c != NULL, DAV1D_ERR(EINVAL));
    validate_input_or_ret(out != NULL, DAV1D_ERR(EINVAL));
    validate_input_or_ret(in != NULL, DAV1D_ERR(EINVAL));

    if (!has_grain(in)) {
        dav1d_picture_ref(out, in);
        return 0;
    }

    int res = dav1d_picture_alloc_copy(c, out, in->p.w, in);
    if (res < 0) goto error;

    if (c->n_tc > 1) {
        dav1d_task_delayed_fg(c, out, in);
    } else {
        switch (out->p.bpc) {
#if CONFIG_8BPC
        case 8:
            dav1d_apply_grain_8bpc(&c->dsp[0].fg, out, in);
            break;
#endif
#if CONFIG_16BPC
        case 10:
        case 12:
            dav1d_apply_grain_16bpc(&c->dsp[(out->p.bpc >> 1) - 4].fg, out, in);
            break;
#endif
        default: abort();
        }
    }

    return 0;

error:
    dav1d_picture_unref_internal(out);
    return res;
}

void dav1d_flush(Dav1dContext *const c) {
    dav1d_data_unref_internal(&c->in);
    if (c->out.p.frame_hdr)
        dav1d_thread_picture_unref(&c->out);
    if (c->cache.p.frame_hdr)
        dav1d_thread_picture_unref(&c->cache);

    c->drain = 0;
    c->cached_error = 0;

    for (int i = 0; i < 8; i++) {
        if (c->refs[i].p.p.frame_hdr)
            dav1d_thread_picture_unref(&c->refs[i].p);
        dav1d_ref_dec(&c->refs[i].segmap);
        dav1d_ref_dec(&c->refs[i].refmvs);
        dav1d_cdf_thread_unref(&c->cdf[i]);
    }
    c->frame_hdr = NULL;
    c->seq_hdr = NULL;
    dav1d_ref_dec(&c->seq_hdr_ref);

    c->mastering_display = NULL;
    c->content_light = NULL;
    c->itut_t35 = NULL;
    c->n_itut_t35 = 0;
    dav1d_ref_dec(&c->mastering_display_ref);
    dav1d_ref_dec(&c->content_light_ref);
    dav1d_ref_dec(&c->itut_t35_ref);

    dav1d_data_props_unref_internal(&c->cached_error_props);

    if (c->n_fc == 1 && c->n_tc == 1) return;
    atomic_store(c->flush, 1);

    if (c->n_tc > 1) {
        pthread_mutex_lock(&c->task_thread.lock);
        // stop running tasks in worker threads
        for (unsigned i = 0; i < c->n_tc; i++) {
            Dav1dTaskContext *const tc = &c->tc[i];
            while (!tc->task_thread.flushed) {
                pthread_cond_wait(&tc->task_thread.td.cond, &c->task_thread.lock);
            }
        }
        for (unsigned i = 0; i < c->n_fc; i++) {
            c->fc[i].task_thread.task_head = NULL;
            c->fc[i].task_thread.task_tail = NULL;
            c->fc[i].task_thread.task_cur_prev = NULL;
            c->fc[i].task_thread.pending_tasks.head = NULL;
            c->fc[i].task_thread.pending_tasks.tail = NULL;
            atomic_init(&c->fc[i].task_thread.pending_tasks.merge, 0);
        }
        atomic_init(&c->task_thread.first, 0);
        c->task_thread.cur = c->n_fc;
        atomic_store(&c->task_thread.reset_task_cur, UINT_MAX);
        atomic_store(&c->task_thread.cond_signaled, 0);
        pthread_mutex_unlock(&c->task_thread.lock);
    }

    if (c->n_fc > 1) {
        for (unsigned n = 0, next = c->frame_thread.next; n < c->n_fc; n++, next++) {
            if (next == c->n_fc) next = 0;
            Dav1dFrameContext *const f = &c->fc[next];
            dav1d_decode_frame_exit(f, -1);
            f->n_tile_data = 0;
            f->task_thread.retval = 0;
            f->task_thread.error = 0;
            Dav1dThreadPicture *out_delayed = &c->frame_thread.out_delayed[next];
            if (out_delayed->p.frame_hdr) {
                dav1d_thread_picture_unref(out_delayed);
            }
        }
        c->frame_thread.next = 0;
    }
    atomic_store(c->flush, 0);
}

COLD void dav1d_close(Dav1dContext **const c_out) {
    validate_input(c_out != NULL);
#if TRACK_HEAP_ALLOCATIONS
    dav1d_log_alloc_stats(*c_out);
#endif
    close_internal(c_out, 1);
}

static COLD void close_internal(Dav1dContext **const c_out, int flush) {
    Dav1dContext *const c = *c_out;
    if (!c) return;

    if (flush) dav1d_flush(c);

    if (c->tc) {
        struct TaskThreadData *ttd = &c->task_thread;
        if (ttd->inited) {
            pthread_mutex_lock(&ttd->lock);
            for (unsigned n = 0; n < c->n_tc && c->tc[n].task_thread.td.inited; n++)
                c->tc[n].task_thread.die = 1;
            pthread_cond_broadcast(&ttd->cond);
            pthread_mutex_unlock(&ttd->lock);
            for (unsigned n = 0; n < c->n_tc; n++) {
                Dav1dTaskContext *const pf = &c->tc[n];
                if (!pf->task_thread.td.inited) break;
                pthread_join(pf->task_thread.td.thread, NULL);
                pthread_cond_destroy(&pf->task_thread.td.cond);
                pthread_mutex_destroy(&pf->task_thread.td.lock);
            }
            pthread_cond_destroy(&ttd->delayed_fg.cond);
            pthread_cond_destroy(&ttd->cond);
            pthread_mutex_destroy(&ttd->lock);
        }
        dav1d_free_aligned(c->tc);
    }

    for (unsigned n = 0; c->fc && n < c->n_fc; n++) {
        Dav1dFrameContext *const f = &c->fc[n];

        // clean-up threading stuff
        if (c->n_fc > 1) {
            dav1d_free(f->tile_thread.lowest_pixel_mem);
            dav1d_free(f->frame_thread.b);
            dav1d_free_aligned(f->frame_thread.cbi);
            dav1d_free_aligned(f->frame_thread.pal_idx);
            dav1d_free_aligned(f->frame_thread.cf);
            dav1d_free(f->frame_thread.tile_start_off);
            dav1d_free_aligned(f->frame_thread.pal);
        }
        if (c->n_tc > 1) {
            pthread_mutex_destroy(&f->task_thread.pending_tasks.lock);
            pthread_cond_destroy(&f->task_thread.cond);
            pthread_mutex_destroy(&f->task_thread.lock);
        }
        dav1d_free(f->frame_thread.frame_progress);
        dav1d_free(f->task_thread.tasks);
        dav1d_free(f->task_thread.tile_tasks[0]);
        dav1d_free_aligned(f->ts);
        dav1d_free_aligned(f->ipred_edge[0]);
        dav1d_free(f->a);
        dav1d_free(f->tile);
        dav1d_free(f->lf.mask);
        dav1d_free(f->lf.level);
        dav1d_free(f->lf.lr_mask);
        dav1d_free(f->lf.tx_lpf_right_edge[0]);
        dav1d_free(f->lf.start_of_tile_row);
        dav1d_free_aligned(f->rf.r);
        dav1d_free_aligned(f->lf.cdef_line_buf);
        dav1d_free_aligned(f->lf.lr_line_buf);
    }
    dav1d_free_aligned(c->fc);
    if (c->n_fc > 1 && c->frame_thread.out_delayed) {
        for (unsigned n = 0; n < c->n_fc; n++)
            if (c->frame_thread.out_delayed[n].p.frame_hdr)
                dav1d_thread_picture_unref(&c->frame_thread.out_delayed[n]);
        dav1d_free(c->frame_thread.out_delayed);
    }
    for (int n = 0; n < c->n_tile_data; n++)
        dav1d_data_unref_internal(&c->tile[n].data);
    dav1d_free(c->tile);
    for (int n = 0; n < 8; n++) {
        dav1d_cdf_thread_unref(&c->cdf[n]);
        if (c->refs[n].p.p.frame_hdr)
            dav1d_thread_picture_unref(&c->refs[n].p);
        dav1d_ref_dec(&c->refs[n].refmvs);
        dav1d_ref_dec(&c->refs[n].segmap);
    }
    dav1d_ref_dec(&c->seq_hdr_ref);
    dav1d_ref_dec(&c->frame_hdr_ref);

    dav1d_ref_dec(&c->mastering_display_ref);
    dav1d_ref_dec(&c->content_light_ref);
    dav1d_ref_dec(&c->itut_t35_ref);

    dav1d_mem_pool_end(c->seq_hdr_pool);
    dav1d_mem_pool_end(c->frame_hdr_pool);
    dav1d_mem_pool_end(c->segmap_pool);
    dav1d_mem_pool_end(c->refmvs_pool);
    dav1d_mem_pool_end(c->cdf_pool);
    dav1d_mem_pool_end(c->picture_pool);
    dav1d_mem_pool_end(c->pic_ctx_pool);

    dav1d_freep_aligned(c_out);
}

int dav1d_get_event_flags(Dav1dContext *const c, enum Dav1dEventFlags *const flags) {
    validate_input_or_ret(c != NULL, DAV1D_ERR(EINVAL));
    validate_input_or_ret(flags != NULL, DAV1D_ERR(EINVAL));

    *flags = c->event_flags;
    c->event_flags = 0;
    return 0;
}

int dav1d_get_decode_error_data_props(Dav1dContext *const c, Dav1dDataProps *const out) {
    validate_input_or_ret(c != NULL, DAV1D_ERR(EINVAL));
    validate_input_or_ret(out != NULL, DAV1D_ERR(EINVAL));

    dav1d_data_props_unref_internal(out);
    *out = c->cached_error_props;
    dav1d_data_props_set_defaults(&c->cached_error_props);

    return 0;
}

void dav1d_picture_unref(Dav1dPicture *const p) {
    dav1d_picture_unref_internal(p);
}

uint8_t *dav1d_data_create(Dav1dData *const buf, const size_t sz) {
    return dav1d_data_create_internal(buf, sz);
}

int dav1d_data_wrap(Dav1dData *const buf, const uint8_t *const ptr,
                    const size_t sz,
                    void (*const free_callback)(const uint8_t *data,
                                                void *user_data),
                    void *const user_data)
{
    return dav1d_data_wrap_internal(buf, ptr, sz, free_callback, user_data);
}

int dav1d_data_wrap_user_data(Dav1dData *const buf,
                              const uint8_t *const user_data,
                              void (*const free_callback)(const uint8_t *user_data,
                                                          void *cookie),
                              void *const cookie)
{
    return dav1d_data_wrap_user_data_internal(buf,
                                              user_data,
                                              free_callback,
                                              cookie);
}

void dav1d_data_unref(Dav1dData *const buf) {
    dav1d_data_unref_internal(buf);
}

void dav1d_data_props_unref(Dav1dDataProps *const props) {
    dav1d_data_props_unref_internal(props);
}
