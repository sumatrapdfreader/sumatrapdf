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

#include "src/ref.h"

static void default_free_callback(const uint8_t *const data, void *const user_data) {
    assert(data == user_data);
    dav1d_free_aligned(user_data);
}

Dav1dRef *dav1d_ref_create(size_t size) {
    size = (size + sizeof(void*) - 1) & ~(sizeof(void*) - 1);

    uint8_t *const data = dav1d_alloc_aligned(size + sizeof(Dav1dRef), 64);
    if (!data) return NULL;

    Dav1dRef *const res = (Dav1dRef*)(data + size);
    res->const_data = res->user_data = res->data = data;
    atomic_init(&res->ref_cnt, 1);
    res->free_ref = 0;
    res->free_callback = default_free_callback;

    return res;
}

static void pool_free_callback(const uint8_t *const data, void *const user_data) {
    dav1d_mem_pool_push((Dav1dMemPool*)data, user_data);
}

Dav1dRef *dav1d_ref_create_using_pool(Dav1dMemPool *const pool, size_t size) {
    size = (size + sizeof(void*) - 1) & ~(sizeof(void*) - 1);

    Dav1dMemPoolBuffer *const buf =
        dav1d_mem_pool_pop(pool, size + sizeof(Dav1dRef));
    if (!buf) return NULL;

    Dav1dRef *const res = &((Dav1dRef*)buf)[-1];
    res->data = buf->data;
    res->const_data = pool;
    atomic_init(&res->ref_cnt, 1);
    res->free_ref = 0;
    res->free_callback = pool_free_callback;
    res->user_data = buf;

    return res;
}

Dav1dRef *dav1d_ref_wrap(const uint8_t *const ptr,
                         void (*free_callback)(const uint8_t *data, void *user_data),
                         void *const user_data)
{
    Dav1dRef *res = malloc(sizeof(Dav1dRef));
    if (!res) return NULL;

    res->data = NULL;
    res->const_data = ptr;
    atomic_init(&res->ref_cnt, 1);
    res->free_ref = 1;
    res->free_callback = free_callback;
    res->user_data = user_data;

    return res;
}

void dav1d_ref_dec(Dav1dRef **const pref) {
    assert(pref != NULL);

    Dav1dRef *const ref = *pref;
    if (!ref) return;

    *pref = NULL;
    if (atomic_fetch_sub(&ref->ref_cnt, 1) == 1) {
        const int free_ref = ref->free_ref;
        ref->free_callback(ref->const_data, ref->user_data);
        if (free_ref) free(ref);
    }
}

int dav1d_ref_is_writable(Dav1dRef *const ref) {
    return atomic_load(&ref->ref_cnt) == 1 && ref->data;
}
