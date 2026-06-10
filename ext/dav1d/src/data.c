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

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "dav1d/data.h"

#include "common/attributes.h"
#include "common/validate.h"

#include "src/data.h"
#include "src/ref.h"

uint8_t *dav1d_data_create_internal(Dav1dData *const buf, const size_t sz) {
    validate_input_or_ret(buf != NULL, NULL);

    if (sz > SIZE_MAX / 2) return NULL;
    buf->ref = dav1d_ref_create(ALLOC_DAV1DDATA, sz);
    if (!buf->ref) return NULL;
    buf->data = buf->ref->const_data;
    buf->sz = sz;
    dav1d_data_props_set_defaults(&buf->m);
    buf->m.size = sz;

    return buf->ref->data;
}

int dav1d_data_wrap_internal(Dav1dData *const buf, const uint8_t *const ptr,
                             const size_t sz,
                             void (*const free_callback)(const uint8_t *data,
                                                         void *cookie),
                             void *const cookie)
{
    validate_input_or_ret(buf != NULL, DAV1D_ERR(EINVAL));
    validate_input_or_ret(ptr != NULL, DAV1D_ERR(EINVAL));
    validate_input_or_ret(free_callback != NULL, DAV1D_ERR(EINVAL));

    if (sz > SIZE_MAX / 2) return DAV1D_ERR(EINVAL);
    Dav1dRef *const ref = dav1d_malloc(ALLOC_DAV1DDATA, sizeof(Dav1dRef));
    if (!ref) return DAV1D_ERR(ENOMEM);

    buf->ref = dav1d_ref_init(ref, ptr, free_callback, cookie, 1);
    buf->data = ptr;
    buf->sz = sz;
    dav1d_data_props_set_defaults(&buf->m);
    buf->m.size = sz;

    return 0;
}

int dav1d_data_wrap_user_data_internal(Dav1dData *const buf,
                                       const uint8_t *const user_data,
                                       void (*const free_callback)(const uint8_t *user_data,
                                                                   void *cookie),
                                       void *const cookie)
{
    validate_input_or_ret(buf != NULL, DAV1D_ERR(EINVAL));
    validate_input_or_ret(free_callback != NULL, DAV1D_ERR(EINVAL));

    Dav1dRef *const ref = dav1d_malloc(ALLOC_DAV1DDATA, sizeof(Dav1dRef));
    if (!ref) return DAV1D_ERR(ENOMEM);

    buf->m.user_data.ref = dav1d_ref_init(ref, user_data, free_callback, cookie, 1);
    buf->m.user_data.data = user_data;

    return 0;
}

void dav1d_data_ref(Dav1dData *const dst, const Dav1dData *const src) {
    assert(dst != NULL);
    assert(dst->data == NULL);
    assert(src != NULL);

    if (src->ref) {
        assert(src->data != NULL);
        dav1d_ref_inc(src->ref);
    }
    if (src->m.user_data.ref) dav1d_ref_inc(src->m.user_data.ref);
    *dst = *src;
}

void dav1d_data_props_copy(Dav1dDataProps *const dst,
                           const Dav1dDataProps *const src)
{
    assert(dst != NULL);
    assert(src != NULL);

    dav1d_ref_dec(&dst->user_data.ref);
    *dst = *src;
    if (dst->user_data.ref) dav1d_ref_inc(dst->user_data.ref);
}

void dav1d_data_props_set_defaults(Dav1dDataProps *const props) {
    assert(props != NULL);

    memset(props, 0, sizeof(*props));
    props->timestamp = INT64_MIN;
    props->offset = -1;
}

void dav1d_data_props_unref_internal(Dav1dDataProps *const props) {
    validate_input(props != NULL);

    struct Dav1dRef *user_data_ref = props->user_data.ref;
    dav1d_data_props_set_defaults(props);
    dav1d_ref_dec(&user_data_ref);
}

void dav1d_data_unref_internal(Dav1dData *const buf) {
    validate_input(buf != NULL);

    struct Dav1dRef *user_data_ref = buf->m.user_data.ref;
    if (buf->ref) {
        validate_input(buf->data != NULL);
        dav1d_ref_dec(&buf->ref);
    }
    memset(buf, 0, sizeof(*buf));
    dav1d_data_props_set_defaults(&buf->m);
    dav1d_ref_dec(&user_data_ref);
}
