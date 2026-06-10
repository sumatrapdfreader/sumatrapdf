/*
 * Copyright © 2020, VideoLAN and dav1d authors
 * Copyright © 2020, Two Orioles, LLC
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

#include <stdint.h>

#include "src/internal.h"

#if TRACK_HEAP_ALLOCATIONS
#include <stdio.h>

#include "src/log.h"

#define DEFAULT_ALIGN 16

typedef struct {
    size_t sz;
    unsigned align;
    enum AllocationType type;
} Dav1dAllocationData;

typedef struct {
    size_t curr_sz;
    size_t peak_sz;
    unsigned num_allocs;
    unsigned num_reuses;
} AllocStats;

static AllocStats tracked_allocs[N_ALLOC_TYPES];
static size_t curr_total_sz;
static size_t peak_total_sz;
static pthread_mutex_t track_alloc_mutex = PTHREAD_MUTEX_INITIALIZER;

static void *track_alloc(const enum AllocationType type, char *ptr,
                         const size_t sz, const size_t align)
{
    assert(align >= sizeof(Dav1dAllocationData));
    if (ptr) {
        ptr += align;
        Dav1dAllocationData *const d = &((Dav1dAllocationData*)ptr)[-1];
        AllocStats *const s = &tracked_allocs[type];

        d->sz = sz;
        d->align = (unsigned)align;
        d->type = type;

        pthread_mutex_lock(&track_alloc_mutex);
        s->num_allocs++;
        s->curr_sz += sz;
        if (s->curr_sz > s->peak_sz)
            s->peak_sz = s->curr_sz;

        curr_total_sz += sz;
        if (curr_total_sz > peak_total_sz)
            peak_total_sz = curr_total_sz;
        pthread_mutex_unlock(&track_alloc_mutex);
    }
    return ptr;
}

static void *track_free(char *const ptr) {
    const Dav1dAllocationData *const d = &((Dav1dAllocationData*)ptr)[-1];
    const size_t sz = d->sz;

    pthread_mutex_lock(&track_alloc_mutex);
    tracked_allocs[d->type].curr_sz -= sz;
    curr_total_sz -= sz;
    pthread_mutex_unlock(&track_alloc_mutex);

    return ptr - d->align;
}

static void dav1d_track_reuse(const enum AllocationType type) {
    pthread_mutex_lock(&track_alloc_mutex);
    tracked_allocs[type].num_reuses++;
    pthread_mutex_unlock(&track_alloc_mutex);
}

void *dav1d_malloc(const enum AllocationType type, const size_t sz) {
    void *const ptr = malloc(sz + DEFAULT_ALIGN);
    return track_alloc(type, ptr, sz, DEFAULT_ALIGN);
}

void *dav1d_alloc_aligned(const enum AllocationType type,
                          const size_t sz, const size_t align)
{
    void *const ptr = dav1d_alloc_aligned_internal(align, sz + align);
    return track_alloc(type, ptr, sz, align);
}

void *dav1d_realloc(const enum AllocationType type,
                    void *ptr, const size_t sz)
{
    if (!ptr)
        return dav1d_malloc(type, sz);
    ptr = realloc((char*)ptr - DEFAULT_ALIGN, sz + DEFAULT_ALIGN);
    if (ptr)
        ptr = track_free((char*)ptr + DEFAULT_ALIGN);
    return track_alloc(type, ptr, sz, DEFAULT_ALIGN);
}

void dav1d_free(void *ptr) {
    if (ptr)
        free(track_free(ptr));
}

void dav1d_free_aligned(void *ptr) {
    if (ptr) {
        dav1d_free_aligned_internal(track_free(ptr));
    }
}

static COLD int cmp_stats(const void *const a, const void *const b) {
    const size_t a_sz = ((const AllocStats*)a)->peak_sz;
    const size_t b_sz = ((const AllocStats*)b)->peak_sz;
    return a_sz < b_sz ? -1 : a_sz > b_sz;
}

/* Insert spaces as thousands separators for better readability */
static COLD int format_tsep(char *const s, const size_t n, const size_t value) {
    if (value < 1000)
        return snprintf(s, n, "%u", (unsigned)value);

    const int len = format_tsep(s, n, value / 1000);
    assert((size_t)len < n);
    return len + snprintf(s + len, n - len, " %03u", (unsigned)(value % 1000));
}

COLD void dav1d_log_alloc_stats(Dav1dContext *const c) {
    static const char *const type_names[N_ALLOC_TYPES] = {
        [ALLOC_BLOCK     ] = "Block data",
        [ALLOC_CDEF      ] = "CDEF line buffers",
        [ALLOC_CDF       ] = "CDF contexts",
        [ALLOC_COEF      ] = "Coefficient data",
        [ALLOC_COMMON_CTX] = "Common context data",
        [ALLOC_DAV1DDATA ] = "Dav1dData",
        [ALLOC_IPRED     ] = "Intra pred edges",
        [ALLOC_LF        ] = "Loopfilter data",
        [ALLOC_LR        ] = "Looprestoration data",
        [ALLOC_OBU_HDR   ] = "OBU headers",
        [ALLOC_OBU_META  ] = "OBU metadata",
        [ALLOC_PAL       ] = "Palette data",
        [ALLOC_PIC       ] = "Picture buffers",
        [ALLOC_PIC_CTX   ] = "Picture context data",
        [ALLOC_REFMVS    ] = "Reference mv data",
        [ALLOC_SEGMAP    ] = "Segmentation maps",
        [ALLOC_THREAD_CTX] = "Thread context data",
        [ALLOC_TILE      ] = "Tile data",
    };

    struct {
        AllocStats stats;
        enum AllocationType type;
    } data[N_ALLOC_TYPES];
    unsigned total_allocs = 0;
    unsigned total_reuses = 0;

    pthread_mutex_lock(&track_alloc_mutex);
    for (int i = 0; i < N_ALLOC_TYPES; i++) {
        AllocStats *const s = &data[i].stats;
        *s = tracked_allocs[i];
        data[i].type = i;
        total_allocs += s->num_allocs;
        total_reuses += s->num_reuses;
    }
    size_t total_sz = peak_total_sz;
    pthread_mutex_unlock(&track_alloc_mutex);

    /* Sort types by memory usage */
    qsort(&data, N_ALLOC_TYPES, sizeof(*data), cmp_stats);

    const double inv_total_share = 100.0 / total_sz;
    char total_sz_buf[32];
    const int sz_len = 4 + format_tsep(total_sz_buf, sizeof(total_sz_buf), total_sz);

    dav1d_log(c, "\n Type                    Allocs    Reuses    Share    Peak size\n"
                 "---------------------------------------------------------------------\n");
    for (int i = N_ALLOC_TYPES - 1; i >= 0; i--) {
        const AllocStats *const s = &data[i].stats;
        if (s->num_allocs) {
            const double share = s->peak_sz * inv_total_share;
            char sz_buf[32];
            format_tsep(sz_buf, sizeof(sz_buf), s->peak_sz);
            dav1d_log(c, " %-20s%10u%10u%8.1f%%%*s\n", type_names[data[i].type],
                      s->num_allocs, s->num_reuses, share, sz_len, sz_buf);
        }
    }
    dav1d_log(c, "---------------------------------------------------------------------\n"
                 "%31u%10u             %s\n",
                 total_allocs, total_reuses, total_sz_buf);
}
#endif /* TRACK_HEAP_ALLOCATIONS */

static COLD void mem_pool_destroy(Dav1dMemPool *const pool) {
    pthread_mutex_destroy(&pool->lock);
    dav1d_free(pool);
}

void dav1d_mem_pool_push(Dav1dMemPool *const pool, Dav1dMemPoolBuffer *const buf) {
    pthread_mutex_lock(&pool->lock);
    const int ref_cnt = --pool->ref_cnt;
    if (!pool->end) {
        buf->next = pool->buf;
        pool->buf = buf;
        pthread_mutex_unlock(&pool->lock);
        assert(ref_cnt > 0);
    } else {
        pthread_mutex_unlock(&pool->lock);
        dav1d_free_aligned(buf->data);
        if (!ref_cnt) mem_pool_destroy(pool);
    }
}

Dav1dMemPoolBuffer *dav1d_mem_pool_pop(Dav1dMemPool *const pool, const size_t size) {
    assert(!(size & (sizeof(void*) - 1)));
    pthread_mutex_lock(&pool->lock);
    Dav1dMemPoolBuffer *buf = pool->buf;
    pool->ref_cnt++;
    uint8_t *data;
    if (buf) {
        pool->buf = buf->next;
        pthread_mutex_unlock(&pool->lock);
        data = buf->data;
        if ((uintptr_t)buf - (uintptr_t)data != size) {
            /* Reallocate if the size has changed */
            dav1d_free_aligned(data);
            goto alloc;
        }
#if TRACK_HEAP_ALLOCATIONS
        dav1d_track_reuse(pool->type);
#endif
    } else {
        pthread_mutex_unlock(&pool->lock);
alloc:
        data = dav1d_alloc_aligned(pool->type,
                                   size + sizeof(Dav1dMemPoolBuffer), 64);
        if (!data) {
            pthread_mutex_lock(&pool->lock);
            const int ref_cnt = --pool->ref_cnt;
            pthread_mutex_unlock(&pool->lock);
            if (!ref_cnt) mem_pool_destroy(pool);
            return NULL;
        }
        buf = (Dav1dMemPoolBuffer*)(data + size);
        buf->data = data;
    }

    return buf;
}

COLD int dav1d_mem_pool_init(const enum AllocationType type,
                             Dav1dMemPool **const ppool)
{
    Dav1dMemPool *const pool = dav1d_malloc(ALLOC_COMMON_CTX,
                                            sizeof(Dav1dMemPool));
    if (pool) {
        if (!pthread_mutex_init(&pool->lock, NULL)) {
            pool->buf = NULL;
            pool->ref_cnt = 1;
            pool->end = 0;
#if TRACK_HEAP_ALLOCATIONS
            pool->type = type;
#endif
            *ppool = pool;
            return 0;
        }
        dav1d_free(pool);
    }
    *ppool = NULL;
    return DAV1D_ERR(ENOMEM);
}

COLD void dav1d_mem_pool_end(Dav1dMemPool *const pool) {
    if (pool) {
        pthread_mutex_lock(&pool->lock);
        Dav1dMemPoolBuffer *buf = pool->buf;
        const int ref_cnt = --pool->ref_cnt;
        pool->buf = NULL;
        pool->end = 1;
        pthread_mutex_unlock(&pool->lock);

        while (buf) {
            void *const data = buf->data;
            buf = buf->next;
            dav1d_free_aligned(data);
        }
        if (!ref_cnt) mem_pool_destroy(pool);
    }
}
