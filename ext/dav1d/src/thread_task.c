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

#include "common/frame.h"

#include "src/thread_task.h"
#include "src/fg_apply.h"

// This function resets the cur pointer to the first frame theoretically
// executable after a task completed (ie. each time we update some progress or
// insert some tasks in the queue).
// When frame_idx is set, it can be either from a completed task, or from tasks
// inserted in the queue, in which case we have to make sure the cur pointer
// isn't past this insert.
// The special case where frame_idx is UINT_MAX is to handle the reset after
// completing a task and locklessly signaling progress. In this case we don't
// enter a critical section, which is needed for this function, so we set an
// atomic for a delayed handling, happening here. Meaning we can call this
// function without any actual update other than what's in the atomic, hence
// this special case.
static inline int reset_task_cur(const Dav1dContext *const c,
                                 struct TaskThreadData *const ttd,
                                 unsigned frame_idx)
{
    const unsigned first = atomic_load(&ttd->first);
    if (!ttd->cur && c->fc[first].task_thread.task_cur_prev == NULL)
        return 0;
    unsigned reset_frame_idx = atomic_exchange(&ttd->reset_task_cur, UINT_MAX);
    if (reset_frame_idx != UINT_MAX) {
        if (frame_idx == UINT_MAX) {
            if (reset_frame_idx > first + ttd->cur)
                return 0;
            ttd->cur = reset_frame_idx - first;
            goto cur_found;
        }
    } else if (frame_idx == UINT_MAX)
        return 0;
    if (frame_idx < first) frame_idx += c->n_fc;
    const unsigned min_frame_idx = umin(reset_frame_idx, frame_idx);
    const unsigned cur_frame_idx = first + ttd->cur;
    if (ttd->cur < c->n_fc && cur_frame_idx < min_frame_idx)
        return 0;
    for (ttd->cur = min_frame_idx - first; ttd->cur < c->n_fc; ttd->cur++)
        if (c->fc[(first + ttd->cur) % c->n_fc].task_thread.task_head)
            break;
cur_found:
    for (unsigned i = ttd->cur; i < c->n_fc; i++)
        c->fc[(first + i) % c->n_fc].task_thread.task_cur_prev = NULL;
    return 1;
}

static inline void reset_task_cur_async(struct TaskThreadData *const ttd,
                                        unsigned frame_idx, unsigned n_frames)
{
    if (frame_idx < (unsigned)atomic_load(&ttd->first)) frame_idx += n_frames;
    unsigned last_idx = frame_idx;
    do {
        frame_idx = last_idx;
        last_idx = atomic_exchange(&ttd->reset_task_cur, frame_idx);
    } while (last_idx < frame_idx);
}

static void insert_tasks_between(Dav1dFrameContext *const f,
                                 Dav1dTask *const first, Dav1dTask *const last,
                                 Dav1dTask *const a, Dav1dTask *const b,
                                 const int cond_signal)
{
    struct TaskThreadData *const ttd = f->task_thread.ttd;
    if (atomic_load(f->c->flush)) return;
    assert(!a || a->next == b);
    if (!a) f->task_thread.task_head = first;
    else a->next = first;
    if (!b) f->task_thread.task_tail = last;
    last->next = b;
    reset_task_cur(f->c, ttd, first->frame_idx);
    if (cond_signal && !atomic_fetch_or(&ttd->cond_signaled, 1))
        pthread_cond_signal(&ttd->cond);
}

static void insert_tasks(Dav1dFrameContext *const f,
                         Dav1dTask *const first, Dav1dTask *const last,
                         const int cond_signal)
{
    // insert task back into task queue
    Dav1dTask *t_ptr, *prev_t = NULL;
    for (t_ptr = f->task_thread.task_head;
         t_ptr; prev_t = t_ptr, t_ptr = t_ptr->next)
    {
        // entropy coding precedes other steps
        if (t_ptr->type == DAV1D_TASK_TYPE_TILE_ENTROPY) {
            if (first->type > DAV1D_TASK_TYPE_TILE_ENTROPY) continue;
            // both are entropy
            if (first->sby > t_ptr->sby) continue;
            if (first->sby < t_ptr->sby) {
                insert_tasks_between(f, first, last, prev_t, t_ptr, cond_signal);
                return;
            }
            // same sby
        } else {
            if (first->type == DAV1D_TASK_TYPE_TILE_ENTROPY) {
                insert_tasks_between(f, first, last, prev_t, t_ptr, cond_signal);
                return;
            }
            if (first->sby > t_ptr->sby) continue;
            if (first->sby < t_ptr->sby) {
                insert_tasks_between(f, first, last, prev_t, t_ptr, cond_signal);
                return;
            }
            // same sby
            if (first->type > t_ptr->type) continue;
            if (first->type < t_ptr->type) {
                insert_tasks_between(f, first, last, prev_t, t_ptr, cond_signal);
                return;
            }
            // same task type
        }

        // sort by tile-id
        assert(first->type == DAV1D_TASK_TYPE_TILE_RECONSTRUCTION ||
               first->type == DAV1D_TASK_TYPE_TILE_ENTROPY);
        assert(first->type == t_ptr->type);
        assert(t_ptr->sby == first->sby);
        const int p = first->type == DAV1D_TASK_TYPE_TILE_ENTROPY;
        const int t_tile_idx = (int) (first - f->task_thread.tile_tasks[p]);
        const int p_tile_idx = (int) (t_ptr - f->task_thread.tile_tasks[p]);
        assert(t_tile_idx != p_tile_idx);
        if (t_tile_idx > p_tile_idx) continue;
        insert_tasks_between(f, first, last, prev_t, t_ptr, cond_signal);
        return;
    }
    // append at the end
    insert_tasks_between(f, first, last, prev_t, NULL, cond_signal);
}

static inline void insert_task(Dav1dFrameContext *const f,
                               Dav1dTask *const t, const int cond_signal)
{
    insert_tasks(f, t, t, cond_signal);
}

static int create_filter_sbrow(Dav1dFrameContext *const f,
                               const int pass, Dav1dTask **res_t)
{
    const int has_deblock = f->frame_hdr->loopfilter.level_y[0] ||
                            f->frame_hdr->loopfilter.level_y[1];
    const int has_cdef = f->seq_hdr->cdef;
    const int has_resize = f->frame_hdr->width[0] != f->frame_hdr->width[1];
    const int has_lr = f->lf.restore_planes;

    Dav1dTask *tasks = f->task_thread.tasks;
    const int uses_2pass = f->c->n_fc > 1;
    int num_tasks = f->sbh * (1 + uses_2pass);
    if (num_tasks > f->task_thread.num_tasks) {
        const size_t size = sizeof(Dav1dTask) * num_tasks;
        tasks = realloc(f->task_thread.tasks, size);
        if (!tasks) return -1;
        memset(tasks, 0, size);
        f->task_thread.tasks = tasks;
        f->task_thread.num_tasks = num_tasks;
    }
    tasks += f->sbh * (pass & 1);

    if (pass & 1) {
        f->frame_thread.entropy_progress = 0;
    } else {
        const int prog_sz = ((f->sbh + 31) & ~31) >> 5;
        if (prog_sz > f->frame_thread.prog_sz) {
            atomic_uint *const prog = realloc(f->frame_thread.frame_progress,
                                              prog_sz * 2 * sizeof(*prog));
            if (!prog) return -1;
            f->frame_thread.frame_progress = prog;
            f->frame_thread.copy_lpf_progress = prog + prog_sz;
            f->frame_thread.prog_sz = prog_sz;
        }
        memset(f->frame_thread.frame_progress, 0, prog_sz * 2 * sizeof(atomic_uint));
        atomic_store(&f->frame_thread.deblock_progress, 0);
    }
    f->frame_thread.next_tile_row[pass & 1] = 0;

    Dav1dTask *t = &tasks[0];
    t->sby = 0;
    t->recon_progress = 1;
    t->deblock_progress = 0;
    t->type = pass == 1 ? DAV1D_TASK_TYPE_ENTROPY_PROGRESS :
              has_deblock ? DAV1D_TASK_TYPE_DEBLOCK_COLS :
              has_cdef || has_lr /* i.e. LR backup */ ? DAV1D_TASK_TYPE_DEBLOCK_ROWS :
              has_resize ? DAV1D_TASK_TYPE_SUPER_RESOLUTION :
              DAV1D_TASK_TYPE_RECONSTRUCTION_PROGRESS;
    t->frame_idx = (int)(f - f->c->fc);

    *res_t = t;
    return 0;
}

int dav1d_task_create_tile_sbrow(Dav1dFrameContext *const f, const int pass,
                                 const int cond_signal)
{
    Dav1dTask *tasks = f->task_thread.tile_tasks[0];
    const int uses_2pass = f->c->n_fc > 1;
    const int num_tasks = f->frame_hdr->tiling.cols * f->frame_hdr->tiling.rows;
    int alloc_num_tasks = num_tasks * (1 + uses_2pass);
    if (alloc_num_tasks > f->task_thread.num_tile_tasks) {
        const size_t size = sizeof(Dav1dTask) * alloc_num_tasks;
        tasks = realloc(f->task_thread.tile_tasks[0], size);
        if (!tasks) return -1;
        memset(tasks, 0, size);
        f->task_thread.tile_tasks[0] = tasks;
        f->task_thread.num_tile_tasks = alloc_num_tasks;
    }
    f->task_thread.tile_tasks[1] = tasks + num_tasks;
    tasks += num_tasks * (pass & 1);

    Dav1dTask *pf_t;
    if (create_filter_sbrow(f, pass, &pf_t))
        return -1;

    Dav1dTask *prev_t = NULL;
    for (int tile_idx = 0; tile_idx < num_tasks; tile_idx++) {
        Dav1dTileState *const ts = &f->ts[tile_idx];
        Dav1dTask *t = &tasks[tile_idx];
        t->sby = ts->tiling.row_start >> f->sb_shift;
        if (pf_t && t->sby) {
            prev_t->next = pf_t;
            prev_t = pf_t;
            pf_t = NULL;
        }
        t->recon_progress = 0;
        t->deblock_progress = 0;
        t->deps_skip = 0;
        t->type = pass != 1 ? DAV1D_TASK_TYPE_TILE_RECONSTRUCTION :
                              DAV1D_TASK_TYPE_TILE_ENTROPY;
        t->frame_idx = (int)(f - f->c->fc);
        if (prev_t) prev_t->next = t;
        prev_t = t;
    }
    if (pf_t) {
        prev_t->next = pf_t;
        prev_t = pf_t;
    }
    insert_tasks(f, &tasks[0], prev_t, cond_signal);
    f->task_thread.done[pass & 1] = 0;

    return 0;
}

void dav1d_task_frame_init(Dav1dFrameContext *const f) {
    const Dav1dContext *const c = f->c;

    f->task_thread.init_done = 0;
    // schedule init task, which will schedule the remaining tasks
    Dav1dTask *const t = &f->task_thread.init_task;
    t->type = DAV1D_TASK_TYPE_INIT;
    t->frame_idx = (int)(f - c->fc);
    t->sby = 0;
    t->recon_progress = t->deblock_progress = 0;
    insert_task(f, t, 1);
}

void dav1d_task_delayed_fg(Dav1dContext *const c, Dav1dPicture *const out,
                           const Dav1dPicture *const in)
{
    struct TaskThreadData *const ttd = &c->task_thread;
    ttd->delayed_fg.in = in;
    ttd->delayed_fg.out = out;
    ttd->delayed_fg.type = DAV1D_TASK_TYPE_FG_PREP;
    atomic_init(&ttd->delayed_fg.progress[0], 0);
    atomic_init(&ttd->delayed_fg.progress[1], 0);
    pthread_mutex_lock(&ttd->lock);
    ttd->delayed_fg.exec = 1;
    pthread_cond_signal(&ttd->cond);
    pthread_cond_wait(&ttd->delayed_fg.cond, &ttd->lock);
    pthread_mutex_unlock(&ttd->lock);
}

static inline int ensure_progress(struct TaskThreadData *const ttd,
                                  Dav1dFrameContext *const f,
                                  Dav1dTask *const t, const enum TaskType type,
                                  atomic_int *const state, int *const target)
{
    // deblock_rows (non-LR portion) depends on deblock of previous sbrow,
    // so ensure that completed. if not, re-add to task-queue; else, fall-through
    int p1 = atomic_load(state);
    if (p1 < t->sby) {
        pthread_mutex_lock(&ttd->lock);
        p1 = atomic_load(state);
        if (p1 < t->sby) {
            t->type = type;
            t->recon_progress = t->deblock_progress = 0;
            *target = t->sby;
            insert_task(f, t, 0);
            return 1;
        }
        pthread_mutex_unlock(&ttd->lock);
    }
    return 0;
}

static inline int check_tile(Dav1dTask *const t, Dav1dFrameContext *const f,
                             const int frame_mt)
{
    const int tp = t->type == DAV1D_TASK_TYPE_TILE_ENTROPY;
    const int tile_idx = (int)(t - f->task_thread.tile_tasks[tp]);
    Dav1dTileState *const ts = &f->ts[tile_idx];
    const int p1 = atomic_load(&ts->progress[tp]);
    if (p1 < t->sby) return 1;
    int error = p1 == TILE_ERROR;
    error |= atomic_fetch_or(&f->task_thread.error, error);
    if (!error && frame_mt && !tp) {
        const int p2 = atomic_load(&ts->progress[1]);
        if (p2 <= t->sby) return 1;
        error = p2 == TILE_ERROR;
        error |= atomic_fetch_or(&f->task_thread.error, error);
    }
    if (!error && frame_mt && !IS_KEY_OR_INTRA(f->frame_hdr)) {
        // check reference state
        const Dav1dThreadPicture *p = &f->sr_cur;
        const int ss_ver = p->p.p.layout == DAV1D_PIXEL_LAYOUT_I420;
        const unsigned p_b = (t->sby + 1) << (f->sb_shift + 2);
        const int tile_sby = t->sby - (ts->tiling.row_start >> f->sb_shift);
        const int (*const lowest_px)[2] = ts->lowest_pixel[tile_sby];
        for (int n = t->deps_skip; n < 7; n++, t->deps_skip++) {
            unsigned lowest;
            if (tp) {
                // if temporal mv refs are disabled, we only need this
                // for the primary ref; if segmentation is disabled, we
                // don't even need that
                lowest = p_b;
            } else {
                // +8 is postfilter-induced delay
                const int y = lowest_px[n][0] == INT_MIN ? INT_MIN :
                              lowest_px[n][0] + 8;
                const int uv = lowest_px[n][1] == INT_MIN ? INT_MIN :
                               lowest_px[n][1] * (1 << ss_ver) + 8;
                const int max = imax(y, uv);
                if (max == INT_MIN) continue;
                lowest = iclip(max, 1, f->refp[n].p.p.h);
            }
            const unsigned p3 = atomic_load(&f->refp[n].progress[!tp]);
            if (p3 < lowest) return 1;
            atomic_fetch_or(&f->task_thread.error, p3 == FRAME_ERROR);
        }
    }
    return 0;
}

static inline void abort_frame(Dav1dFrameContext *const f, const int error) {
    atomic_store(&f->task_thread.error, error == DAV1D_ERR(EINVAL) ? 1 : -1);
    f->task_thread.task_counter = 0;
    f->task_thread.done[0] = 1;
    f->task_thread.done[1] = 1;
    atomic_store(&f->sr_cur.progress[0], FRAME_ERROR);
    atomic_store(&f->sr_cur.progress[1], FRAME_ERROR);
    dav1d_decode_frame_exit(f, error);
    f->n_tile_data = 0;
    pthread_cond_signal(&f->task_thread.cond);
}

static inline void delayed_fg_task(const Dav1dContext *const c,
                                   struct TaskThreadData *const ttd)
{
    const Dav1dPicture *const in = ttd->delayed_fg.in;
    Dav1dPicture *const out = ttd->delayed_fg.out;
#if CONFIG_16BPC
    int off;
    if (out->p.bpc != 8)
        off = (out->p.bpc >> 1) - 4;
#endif
    switch (ttd->delayed_fg.type) {
    case DAV1D_TASK_TYPE_FG_PREP:
        ttd->delayed_fg.exec = 0;
        if (atomic_load(&ttd->cond_signaled))
            pthread_cond_signal(&ttd->cond);
        pthread_mutex_unlock(&ttd->lock);
        switch (out->p.bpc) {
#if CONFIG_8BPC
        case 8:
            dav1d_prep_grain_8bpc(&c->dsp[0].fg, out, in,
                                  ttd->delayed_fg.scaling_8bpc,
                                  ttd->delayed_fg.grain_lut_8bpc);
            break;
#endif
#if CONFIG_16BPC
        case 10:
        case 12:
            dav1d_prep_grain_16bpc(&c->dsp[off].fg, out, in,
                                   ttd->delayed_fg.scaling_16bpc,
                                   ttd->delayed_fg.grain_lut_16bpc);
            break;
#endif
        default: abort();
        }
        ttd->delayed_fg.type = DAV1D_TASK_TYPE_FG_APPLY;
        pthread_mutex_lock(&ttd->lock);
        ttd->delayed_fg.exec = 1;
        // fall-through
    case DAV1D_TASK_TYPE_FG_APPLY:;
        int row = atomic_fetch_add(&ttd->delayed_fg.progress[0], 1);
        pthread_mutex_unlock(&ttd->lock);
        int progmax = (out->p.h + 31) >> 5;
    fg_apply_loop:
        if (row + 1 < progmax)
            pthread_cond_signal(&ttd->cond);
        else if (row + 1 >= progmax) {
            pthread_mutex_lock(&ttd->lock);
            ttd->delayed_fg.exec = 0;
            if (row >= progmax) goto end_add;
            pthread_mutex_unlock(&ttd->lock);
        }
        switch (out->p.bpc) {
#if CONFIG_8BPC
        case 8:
            dav1d_apply_grain_row_8bpc(&c->dsp[0].fg, out, in,
                                       ttd->delayed_fg.scaling_8bpc,
                                       ttd->delayed_fg.grain_lut_8bpc, row);
            break;
#endif
#if CONFIG_16BPC
        case 10:
        case 12:
            dav1d_apply_grain_row_16bpc(&c->dsp[off].fg, out, in,
                                        ttd->delayed_fg.scaling_16bpc,
                                        ttd->delayed_fg.grain_lut_16bpc, row);
            break;
#endif
        default: abort();
        }
        row = atomic_fetch_add(&ttd->delayed_fg.progress[0], 1);
        int done = atomic_fetch_add(&ttd->delayed_fg.progress[1], 1) + 1;
        if (row < progmax) goto fg_apply_loop;
        pthread_mutex_lock(&ttd->lock);
        ttd->delayed_fg.exec = 0;
    end_add:
        done = atomic_fetch_add(&ttd->delayed_fg.progress[1], 1) + 1;
        progmax = atomic_load(&ttd->delayed_fg.progress[0]);
        // signal for completion only once the last runner reaches this
        if (done < progmax)
            break;
        pthread_cond_signal(&ttd->delayed_fg.cond);
        break;
    default: abort();
    }
}

void *dav1d_worker_task(void *data) {
    Dav1dTaskContext *const tc = data;
    const Dav1dContext *const c = tc->c;
    struct TaskThreadData *const ttd = tc->task_thread.ttd;

    dav1d_set_thread_name("dav1d-worker");

    pthread_mutex_lock(&ttd->lock);
    for (;;) {
        if (tc->task_thread.die) break;
        if (atomic_load(c->flush)) goto park;
        if (ttd->delayed_fg.exec) { // run delayed film grain first
            delayed_fg_task(c, ttd);
            continue;
        }
        Dav1dFrameContext *f;
        Dav1dTask *t, *prev_t = NULL;
        if (c->n_fc > 1) { // run init tasks second
            for (unsigned i = 0; i < c->n_fc; i++) {
                const unsigned first = atomic_load(&ttd->first);
                f = &c->fc[(first + i) % c->n_fc];
                if (f->task_thread.init_done) continue;
                t = f->task_thread.task_head;
                if (!t) continue;
                if (t->type == DAV1D_TASK_TYPE_INIT) goto found;
                if (t->type == DAV1D_TASK_TYPE_INIT_CDF) {
                    const int p1 = f->in_cdf.progress ?
                        atomic_load(f->in_cdf.progress) : 1;
                    if (p1) {
                        atomic_fetch_or(&f->task_thread.error, p1 == TILE_ERROR);
                        goto found;
                    }
                }
            }
        }
        while (ttd->cur < c->n_fc) { // run decoding tasks last
            const unsigned first = atomic_load(&ttd->first);
            f = &c->fc[(first + ttd->cur) % c->n_fc];
            prev_t = f->task_thread.task_cur_prev;
            t = prev_t ? prev_t->next : f->task_thread.task_head;
            while (t) {
                if (t->type == DAV1D_TASK_TYPE_INIT_CDF) goto next;
                else if (t->type == DAV1D_TASK_TYPE_TILE_ENTROPY ||
                         t->type == DAV1D_TASK_TYPE_TILE_RECONSTRUCTION)
                {
                    // if not bottom sbrow of tile, this task will be re-added
                    // after it's finished
                    if (!check_tile(t, f, c->n_fc > 1))
                        goto found;
                } else if (t->recon_progress) {
                    const int p = t->type == DAV1D_TASK_TYPE_ENTROPY_PROGRESS;
                    int error = atomic_load(&f->task_thread.error);
                    assert(!f->task_thread.done[p] || error);
                    const int tile_row_base = f->frame_hdr->tiling.cols *
                                              f->frame_thread.next_tile_row[p];
                    if (p) {
                        const int p1 = f->frame_thread.entropy_progress;
                        if (p1 < t->sby) goto next;
                        atomic_fetch_or(&f->task_thread.error, p1 == TILE_ERROR);
                    }
                    for (int tc = 0; tc < f->frame_hdr->tiling.cols; tc++) {
                        Dav1dTileState *const ts = &f->ts[tile_row_base + tc];
                        const int p2 = atomic_load(&ts->progress[p]);
                        if (p2 < t->recon_progress) goto next;
                        atomic_fetch_or(&f->task_thread.error, p2 == TILE_ERROR);
                    }
                    if (t->sby + 1 < f->sbh) {
                        // add sby+1 to list to replace this one
                        Dav1dTask *next_t = &t[1];
                        *next_t = *t;
                        next_t->sby++;
                        const int ntr = f->frame_thread.next_tile_row[p] + 1;
                        const int start = f->frame_hdr->tiling.row_start_sb[ntr];
                        if (next_t->sby == start)
                            f->frame_thread.next_tile_row[p] = ntr;
                        next_t->recon_progress = next_t->sby + 1;
                        insert_task(f, next_t, 0);
                    }
                    goto found;
                } else if (t->type == DAV1D_TASK_TYPE_CDEF) {
                    atomic_uint *prog = f->frame_thread.copy_lpf_progress;
                    const int p1 = atomic_load(&prog[(t->sby - 1) >> 5]);
                    if (p1 & (1U << ((t->sby - 1) & 31)))
                        goto found;
                } else {
                    assert(t->deblock_progress);
                    const int p1 = atomic_load(&f->frame_thread.deblock_progress);
                    if (p1 >= t->deblock_progress) {
                        atomic_fetch_or(&f->task_thread.error, p1 == TILE_ERROR);
                        goto found;
                    }
                }
            next:
                prev_t = t;
                t = t->next;
                f->task_thread.task_cur_prev = prev_t;
            }
            ttd->cur++;
        }
        if (reset_task_cur(c, ttd, UINT_MAX)) continue;
    park:
        tc->task_thread.flushed = 1;
        pthread_cond_signal(&tc->task_thread.td.cond);
        // we want to be woken up next time progress is signaled
        atomic_store(&ttd->cond_signaled, 0);
        pthread_cond_wait(&ttd->cond, &ttd->lock);
        tc->task_thread.flushed = 0;
        reset_task_cur(c, ttd, UINT_MAX);
        continue;

    found:
        // remove t from list
        if (prev_t) prev_t->next = t->next;
        else f->task_thread.task_head = t->next;
        if (!t->next) f->task_thread.task_tail = prev_t;
        if (t->type > DAV1D_TASK_TYPE_INIT_CDF && !f->task_thread.task_head)
            ttd->cur++;
        // we don't need to check cond_signaled here, since we found a task
        // after the last signal so we want to re-signal the next waiting thread
        // and again won't need to signal after that
        atomic_store(&ttd->cond_signaled, 1);
        pthread_cond_signal(&ttd->cond);
        pthread_mutex_unlock(&ttd->lock);
    found_unlocked:;
        const int flush = atomic_load(c->flush);
        int error = atomic_fetch_or(&f->task_thread.error, flush) | flush;

        // run it
        tc->f = f;
        int sby = t->sby;
        switch (t->type) {
        case DAV1D_TASK_TYPE_INIT: {
            assert(c->n_fc > 1);
            int res = dav1d_decode_frame_init(f);
            int p1 = f->in_cdf.progress ? atomic_load(f->in_cdf.progress) : 1;
            if (res || p1 == TILE_ERROR) {
                pthread_mutex_lock(&ttd->lock);
                abort_frame(f, res ? res : DAV1D_ERR(EINVAL));
            } else if (!res) {
                t->type = DAV1D_TASK_TYPE_INIT_CDF;
                if (p1) goto found_unlocked;
                pthread_mutex_lock(&ttd->lock);
                insert_task(f, t, 0);
            }
            reset_task_cur(c, ttd, t->frame_idx);
            continue;
        }
        case DAV1D_TASK_TYPE_INIT_CDF: {
            assert(c->n_fc > 1);
            int res = DAV1D_ERR(EINVAL);
            if (!atomic_load(&f->task_thread.error))
                res = dav1d_decode_frame_init_cdf(f);
            pthread_mutex_lock(&ttd->lock);
            if (f->frame_hdr->refresh_context && !f->task_thread.update_set) {
                atomic_store(f->out_cdf.progress, res < 0 ? TILE_ERROR : 1);
            }
            if (!res) {
                assert(c->n_fc > 1);
                for (int p = 1; p <= 2; p++) {
                    const int res = dav1d_task_create_tile_sbrow(f, p, 0);
                    if (res) {
                        // memory allocation failed
                        f->task_thread.done[2 - p] = 1;
                        atomic_store(&f->task_thread.error, -1);
                        f->task_thread.task_counter -= f->sbh +
                            f->frame_hdr->tiling.cols * f->frame_hdr->tiling.rows;
                        atomic_store(&f->sr_cur.progress[p - 1], FRAME_ERROR);
                        if (p == 2 && f->task_thread.done[1]) {
                            assert(!f->task_thread.task_counter);
                            dav1d_decode_frame_exit(f, DAV1D_ERR(ENOMEM));
                            f->n_tile_data = 0;
                            pthread_cond_signal(&f->task_thread.cond);
                        }
                    }
                }
            } else abort_frame(f, res);
            reset_task_cur(c, ttd, t->frame_idx);
            f->task_thread.init_done = 1;
            continue;
        }
        case DAV1D_TASK_TYPE_TILE_ENTROPY:
        case DAV1D_TASK_TYPE_TILE_RECONSTRUCTION: {
            const int p = t->type == DAV1D_TASK_TYPE_TILE_ENTROPY;
            const int tile_idx = (int)(t - f->task_thread.tile_tasks[p]);
            Dav1dTileState *const ts = &f->ts[tile_idx];

            tc->ts = ts;
            tc->by = sby << f->sb_shift;
            const int uses_2pass = c->n_fc > 1;
            tc->frame_thread.pass = !uses_2pass ? 0 :
                1 + (t->type == DAV1D_TASK_TYPE_TILE_RECONSTRUCTION);
            if (!error) error = dav1d_decode_tile_sbrow(tc);
            const int progress = error ? TILE_ERROR : 1 + sby;

            // signal progress
            atomic_fetch_or(&f->task_thread.error, error);
            if (((sby + 1) << f->sb_shift) < ts->tiling.row_end) {
                t->sby++;
                t->deps_skip = 0;
                if (!check_tile(t, f, uses_2pass)) {
                    atomic_store(&ts->progress[p], progress);
                    reset_task_cur_async(ttd, t->frame_idx, c->n_fc);
                    if (!atomic_fetch_or(&ttd->cond_signaled, 1))
                        pthread_cond_signal(&ttd->cond);
                    goto found_unlocked;
                }
                pthread_mutex_lock(&ttd->lock);
                atomic_store(&ts->progress[p], progress);
                reset_task_cur(c, ttd, t->frame_idx);
                insert_task(f, t, 0);
            } else {
                pthread_mutex_lock(&ttd->lock);
                atomic_store(&ts->progress[p], progress);
                reset_task_cur(c, ttd, t->frame_idx);
                error = atomic_load(&f->task_thread.error);
                if (f->frame_hdr->refresh_context &&
                    tc->frame_thread.pass <= 1 && f->task_thread.update_set &&
                    f->frame_hdr->tiling.update == tile_idx)
                {
                    if (!error)
                        dav1d_cdf_thread_update(f->frame_hdr, f->out_cdf.data.cdf,
                                                &f->ts[f->frame_hdr->tiling.update].cdf);
                    if (c->n_fc > 1)
                        atomic_store(f->out_cdf.progress, error ? TILE_ERROR : 1);
                }
                if (!--f->task_thread.task_counter && f->task_thread.done[0] &&
                    (!uses_2pass || f->task_thread.done[1]))
                {
                    dav1d_decode_frame_exit(f, error == 1 ? DAV1D_ERR(EINVAL) :
                                            error ? DAV1D_ERR(ENOMEM) : 0);
                    f->n_tile_data = 0;
                    pthread_cond_signal(&f->task_thread.cond);
                }
                assert(f->task_thread.task_counter >= 0);
                if (!atomic_fetch_or(&ttd->cond_signaled, 1))
                    pthread_cond_signal(&ttd->cond);
            }
            continue;
        }
        case DAV1D_TASK_TYPE_DEBLOCK_COLS:
            if (!atomic_load(&f->task_thread.error))
                f->bd_fn.filter_sbrow_deblock_cols(f, sby);
            if (ensure_progress(ttd, f, t, DAV1D_TASK_TYPE_DEBLOCK_ROWS,
                                &f->frame_thread.deblock_progress,
                                &t->deblock_progress)) continue;
            // fall-through
        case DAV1D_TASK_TYPE_DEBLOCK_ROWS:
            if (!atomic_load(&f->task_thread.error))
                f->bd_fn.filter_sbrow_deblock_rows(f, sby);
            // signal deblock progress
            if (f->frame_hdr->loopfilter.level_y[0] ||
                f->frame_hdr->loopfilter.level_y[1])
            {
                error = atomic_load(&f->task_thread.error);
                atomic_store(&f->frame_thread.deblock_progress,
                             error ? TILE_ERROR : sby + 1);
                reset_task_cur_async(ttd, t->frame_idx, c->n_fc);
                if (!atomic_fetch_or(&ttd->cond_signaled, 1))
                    pthread_cond_signal(&ttd->cond);
            } else if (f->seq_hdr->cdef || f->lf.restore_planes) {
                atomic_fetch_or(&f->frame_thread.copy_lpf_progress[sby >> 5],
                                1U << (sby & 31));
                // CDEF needs the top buffer to be saved by lr_copy_lpf of the
                // previous sbrow
                if (sby) {
                    int prog = atomic_load(&f->frame_thread.copy_lpf_progress[(sby - 1) >> 5]);
                    if (~prog & (1U << ((sby - 1) & 31))) {
                        pthread_mutex_lock(&ttd->lock);
                        prog = atomic_load(&f->frame_thread.copy_lpf_progress[(sby - 1) >> 5]);
                        if (~prog & (1U << ((sby - 1) & 31))) {
                            t->type = DAV1D_TASK_TYPE_CDEF;
                            t->recon_progress = t->deblock_progress = 0;
                            insert_task(f, t, 0);
                            continue;
                        }
                        pthread_mutex_unlock(&ttd->lock);
                    }
                }
            }
            // fall-through
        case DAV1D_TASK_TYPE_CDEF:
            if (f->seq_hdr->cdef) {
                if (!atomic_load(&f->task_thread.error))
                    f->bd_fn.filter_sbrow_cdef(tc, sby);
                reset_task_cur_async(ttd, t->frame_idx, c->n_fc);
                if (!atomic_fetch_or(&ttd->cond_signaled, 1))
                    pthread_cond_signal(&ttd->cond);
            }
            // fall-through
        case DAV1D_TASK_TYPE_SUPER_RESOLUTION:
            if (f->frame_hdr->width[0] != f->frame_hdr->width[1])
                if (!atomic_load(&f->task_thread.error))
                    f->bd_fn.filter_sbrow_resize(f, sby);
            // fall-through
        case DAV1D_TASK_TYPE_LOOP_RESTORATION:
            if (!atomic_load(&f->task_thread.error) && f->lf.restore_planes)
                f->bd_fn.filter_sbrow_lr(f, sby);
            // fall-through
        case DAV1D_TASK_TYPE_RECONSTRUCTION_PROGRESS:
            // dummy to cover for no post-filters
        case DAV1D_TASK_TYPE_ENTROPY_PROGRESS:
            // dummy to convert tile progress to frame
            break;
        default: abort();
        }
        // if task completed [typically LR], signal picture progress as per below
        const int uses_2pass = c->n_fc > 1;
        const int sbh = f->sbh;
        const int sbsz = f->sb_step * 4;
        const enum PlaneType progress_plane_type =
            t->type == DAV1D_TASK_TYPE_ENTROPY_PROGRESS ? PLANE_TYPE_BLOCK :
            c->n_fc > 1 ? PLANE_TYPE_Y : PLANE_TYPE_ALL;
        if (t->type != DAV1D_TASK_TYPE_ENTROPY_PROGRESS)
            atomic_fetch_or(&f->frame_thread.frame_progress[sby >> 5],
                            1U << (sby & 31));
        pthread_mutex_lock(&ttd->lock);
        if (t->type != DAV1D_TASK_TYPE_ENTROPY_PROGRESS) {
            unsigned frame_prog = c->n_fc > 1 ? atomic_load(&f->sr_cur.progress[1]) : 0;
            if (frame_prog < FRAME_ERROR) {
                int idx = frame_prog >> (f->sb_shift + 7);
                int prog;
                do {
                    atomic_uint *state = &f->frame_thread.frame_progress[idx];
                    const unsigned val = ~atomic_load(state);
                    prog = val ? ctz(val) : 32;
                    if (prog != 32) break;
                    prog = 0;
                } while (++idx < f->frame_thread.prog_sz);
                sby = ((idx << 5) | prog) - 1;
            } else sby = sbh - 1;
        }
        error = atomic_load(&f->task_thread.error);
        const unsigned y = sby + 1 == sbh ? UINT_MAX : (unsigned)(sby + 1) * sbsz;
        if (c->n_fc > 1 && f->sr_cur.p.data[0] /* upon flush, this can be free'ed already */) {
            const int idx = t->type != DAV1D_TASK_TYPE_ENTROPY_PROGRESS;
            atomic_store(&f->sr_cur.progress[idx], error ? FRAME_ERROR : y);
        }
        if (progress_plane_type == PLANE_TYPE_BLOCK)
            f->frame_thread.entropy_progress = error ? TILE_ERROR : sby + 1;
        if (sby + 1 == sbh)
            f->task_thread.done[progress_plane_type == PLANE_TYPE_BLOCK] = 1;
        if (!--f->task_thread.task_counter &&
            f->task_thread.done[0] && (!uses_2pass || f->task_thread.done[1]))
        {
            dav1d_decode_frame_exit(f, error == 1 ? DAV1D_ERR(EINVAL) :
                                    error ? DAV1D_ERR(ENOMEM) : 0);
            f->n_tile_data = 0;
            pthread_cond_signal(&f->task_thread.cond);
        }
        reset_task_cur(c, ttd, t->frame_idx);
    }
    pthread_mutex_unlock(&ttd->lock);

    return NULL;
}
