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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/intops.h"
#include "common/validate.h"

#include "src/internal.h"
#include "src/log.h"
#include "src/picture.h"
#include "src/ref.h"
#include "src/thread.h"
#include "src/thread_task.h"

int dav1d_default_picture_alloc(Dav1dPicture *const p, void *const cookie) {
    assert(sizeof(Dav1dMemPoolBuffer) <= DAV1D_PICTURE_ALIGNMENT);
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
    p->stride[0] = y_stride;
    p->stride[1] = uv_stride;
    const size_t y_sz = y_stride * aligned_h;
    const size_t uv_sz = uv_stride * (aligned_h >> ss_ver);
    const size_t pic_size = y_sz + 2 * uv_sz;

    Dav1dMemPoolBuffer *const buf = dav1d_mem_pool_pop(cookie, pic_size +
                                                       DAV1D_PICTURE_ALIGNMENT -
                                                       sizeof(Dav1dMemPoolBuffer));
    if (!buf) return DAV1D_ERR(ENOMEM);
    p->allocator_data = buf;

    uint8_t *const data = buf->data;
    p->data[0] = data;
    p->data[1] = has_chroma ? data + y_sz : NULL;
    p->data[2] = has_chroma ? data + y_sz + uv_sz : NULL;

    return 0;
}

void dav1d_default_picture_release(Dav1dPicture *const p, void *const cookie) {
    dav1d_mem_pool_push(cookie, p->allocator_data);
}

struct pic_ctx_context {
    Dav1dPicAllocator allocator;
    Dav1dPicture pic;
    Dav1dRef ref;
    void *extra_data[];
};

static void free_buffer(const uint8_t *const data, void *const user_data) {
    Dav1dMemPoolBuffer *buf = (Dav1dMemPoolBuffer *)data;
    struct pic_ctx_context *pic_ctx = buf->data;

    pic_ctx->allocator.release_picture_callback(&pic_ctx->pic,
                                                pic_ctx->allocator.cookie);
    dav1d_mem_pool_push(user_data, buf);
}

void dav1d_picture_free_itut_t35(const uint8_t *const data, void *const user_data) {
    struct itut_t35_ctx_context *itut_t35_ctx = user_data;

    for (size_t i = 0; i < itut_t35_ctx->n_itut_t35; i++)
        dav1d_free(itut_t35_ctx->itut_t35[i].payload);
    dav1d_free(itut_t35_ctx->itut_t35);
    dav1d_free(itut_t35_ctx);
}

static int picture_alloc(Dav1dContext *const c,
                         Dav1dPicture *const p,
                         const int w, const int h,
                         Dav1dSequenceHeader *const seq_hdr, Dav1dRef *const seq_hdr_ref,
                         Dav1dFrameHeader *const frame_hdr, Dav1dRef *const frame_hdr_ref,
                         const int bpc,
                         const Dav1dDataProps *const props,
                         Dav1dPicAllocator *const p_allocator,
                         void **const extra_ptr)
{
    if (p->data[0]) {
        dav1d_log(c, "Picture already allocated!\n");
        return -1;
    }
    assert(bpc > 0 && bpc <= 16);

    size_t extra = c->n_fc > 1 ? sizeof(atomic_int) * 2 : 0;
    Dav1dMemPoolBuffer *buf = dav1d_mem_pool_pop(c->pic_ctx_pool,
                                                 extra + sizeof(struct pic_ctx_context));
    if (buf == NULL)
        return DAV1D_ERR(ENOMEM);

    struct pic_ctx_context *pic_ctx = buf->data;

    p->p.w = w;
    p->p.h = h;
    p->seq_hdr = seq_hdr;
    p->frame_hdr = frame_hdr;
    p->p.layout = seq_hdr->layout;
    p->p.bpc = bpc;
    dav1d_data_props_set_defaults(&p->m);
    const int res = p_allocator->alloc_picture_callback(p, p_allocator->cookie);
    if (res < 0) {
        dav1d_mem_pool_push(c->pic_ctx_pool, buf);
        return res;
    }

    pic_ctx->allocator = *p_allocator;
    pic_ctx->pic = *p;
    p->ref = dav1d_ref_init(&pic_ctx->ref, buf, free_buffer, c->pic_ctx_pool, 0);

    p->seq_hdr_ref = seq_hdr_ref;
    if (seq_hdr_ref) dav1d_ref_inc(seq_hdr_ref);

    p->frame_hdr_ref = frame_hdr_ref;
    if (frame_hdr_ref) dav1d_ref_inc(frame_hdr_ref);

    if (extra && extra_ptr)
        *extra_ptr = &pic_ctx->extra_data;

    return 0;
}

void dav1d_picture_copy_props(Dav1dPicture *const p,
                              Dav1dContentLightLevel *const content_light, Dav1dRef *const content_light_ref,
                              Dav1dMasteringDisplay *const mastering_display, Dav1dRef *const mastering_display_ref,
                              Dav1dITUTT35 *const itut_t35, Dav1dRef *itut_t35_ref, size_t n_itut_t35,
                              const Dav1dDataProps *const props)
{
    dav1d_data_props_copy(&p->m, props);

    dav1d_ref_dec(&p->content_light_ref);
    p->content_light_ref = content_light_ref;
    p->content_light = content_light;
    if (content_light_ref) dav1d_ref_inc(content_light_ref);

    dav1d_ref_dec(&p->mastering_display_ref);
    p->mastering_display_ref = mastering_display_ref;
    p->mastering_display = mastering_display;
    if (mastering_display_ref) dav1d_ref_inc(mastering_display_ref);

    dav1d_ref_dec(&p->itut_t35_ref);
    p->itut_t35_ref = itut_t35_ref;
    p->itut_t35 = itut_t35;
    p->n_itut_t35 = n_itut_t35;
    if (itut_t35_ref) dav1d_ref_inc(itut_t35_ref);
}

int dav1d_thread_picture_alloc(Dav1dContext *const c, Dav1dFrameContext *const f,
                               const int bpc)
{
    Dav1dThreadPicture *const p = &f->sr_cur;

    const int res = picture_alloc(c, &p->p, f->frame_hdr->width[1], f->frame_hdr->height,
                                  f->seq_hdr, f->seq_hdr_ref,
                                  f->frame_hdr, f->frame_hdr_ref,
                                  bpc, &f->tile[0].data.m, &c->allocator,
                                  (void **) &p->progress);
    if (res) return res;

    // Don't clear these flags from c->frame_flags if the frame is not going to be output.
    // This way they will be added to the next visible frame too.
    const int flags_mask = ((f->frame_hdr->show_frame || c->output_invisible_frames) &&
                            c->max_spatial_id == f->frame_hdr->spatial_id)
                           ? 0 : (PICTURE_FLAG_NEW_SEQUENCE | PICTURE_FLAG_NEW_OP_PARAMS_INFO);
    p->flags = c->frame_flags;
    c->frame_flags &= flags_mask;

    p->visible = f->frame_hdr->show_frame;
    p->showable = f->frame_hdr->showable_frame;

    if (p->visible) {
        // Only add HDR10+ and T35 metadata when show frame flag is enabled
        dav1d_picture_copy_props(&p->p, c->content_light, c->content_light_ref,
                                 c->mastering_display, c->mastering_display_ref,
                                 c->itut_t35, c->itut_t35_ref, c->n_itut_t35,
                                 &f->tile[0].data.m);

        // Must be removed from the context after being attached to the frame
        dav1d_ref_dec(&c->itut_t35_ref);
        c->itut_t35 = NULL;
        c->n_itut_t35 = 0;
    } else {
        dav1d_data_props_copy(&p->p.m, &f->tile[0].data.m);
    }

    if (c->n_fc > 1) {
        atomic_init(&p->progress[0], 0);
        atomic_init(&p->progress[1], 0);
    }
    return res;
}

int dav1d_picture_alloc_copy(Dav1dContext *const c, Dav1dPicture *const dst, const int w,
                             const Dav1dPicture *const src)
{
    Dav1dMemPoolBuffer *const buf = (Dav1dMemPoolBuffer *)src->ref->const_data;
    struct pic_ctx_context *const pic_ctx = buf->data;
    const int res = picture_alloc(c, dst, w, src->p.h,
                                  src->seq_hdr, src->seq_hdr_ref,
                                  src->frame_hdr, src->frame_hdr_ref,
                                  src->p.bpc, &src->m, &pic_ctx->allocator,
                                  NULL);
    if (res) return res;

    dav1d_picture_copy_props(dst, src->content_light, src->content_light_ref,
                             src->mastering_display, src->mastering_display_ref,
                             src->itut_t35, src->itut_t35_ref, src->n_itut_t35,
                             &src->m);

    return 0;
}

void dav1d_picture_ref(Dav1dPicture *const dst, const Dav1dPicture *const src) {
    assert(dst != NULL);
    assert(dst->data[0] == NULL);
    assert(src != NULL);

    if (src->ref) {
        assert(src->data[0] != NULL);
        dav1d_ref_inc(src->ref);
    }
    if (src->frame_hdr_ref) dav1d_ref_inc(src->frame_hdr_ref);
    if (src->seq_hdr_ref) dav1d_ref_inc(src->seq_hdr_ref);
    if (src->m.user_data.ref) dav1d_ref_inc(src->m.user_data.ref);
    if (src->content_light_ref) dav1d_ref_inc(src->content_light_ref);
    if (src->mastering_display_ref) dav1d_ref_inc(src->mastering_display_ref);
    if (src->itut_t35_ref) dav1d_ref_inc(src->itut_t35_ref);
    *dst = *src;
}

void dav1d_picture_move_ref(Dav1dPicture *const dst, Dav1dPicture *const src) {
    assert(dst != NULL);
    assert(dst->data[0] == NULL);
    assert(src != NULL);

    if (src->ref)
        assert(src->data[0] != NULL);

    *dst = *src;
    memset(src, 0, sizeof(*src));
}

void dav1d_thread_picture_ref(Dav1dThreadPicture *const dst,
                              const Dav1dThreadPicture *const src)
{
    dav1d_picture_ref(&dst->p, &src->p);
    dst->visible = src->visible;
    dst->showable = src->showable;
    dst->progress = src->progress;
    dst->flags = src->flags;
}

void dav1d_thread_picture_move_ref(Dav1dThreadPicture *const dst,
                                   Dav1dThreadPicture *const src)
{
    dav1d_picture_move_ref(&dst->p, &src->p);
    dst->visible = src->visible;
    dst->showable = src->showable;
    dst->progress = src->progress;
    dst->flags = src->flags;
    memset(src, 0, sizeof(*src));
}

void dav1d_picture_unref_internal(Dav1dPicture *const p) {
    validate_input(p != NULL);

    if (p->ref) {
        validate_input(p->data[0] != NULL);
        dav1d_ref_dec(&p->ref);
    }
    dav1d_ref_dec(&p->seq_hdr_ref);
    dav1d_ref_dec(&p->frame_hdr_ref);
    dav1d_ref_dec(&p->m.user_data.ref);
    dav1d_ref_dec(&p->content_light_ref);
    dav1d_ref_dec(&p->mastering_display_ref);
    dav1d_ref_dec(&p->itut_t35_ref);
    memset(p, 0, sizeof(*p));
    dav1d_data_props_set_defaults(&p->m);
}

void dav1d_thread_picture_unref(Dav1dThreadPicture *const p) {
    dav1d_picture_unref_internal(&p->p);

    p->progress = NULL;
}

enum Dav1dEventFlags dav1d_picture_get_event_flags(const Dav1dThreadPicture *const p) {
    if (!p->flags)
        return 0;

    enum Dav1dEventFlags flags = 0;
    if (p->flags & PICTURE_FLAG_NEW_SEQUENCE)
       flags |= DAV1D_EVENT_FLAG_NEW_SEQUENCE;
    if (p->flags & PICTURE_FLAG_NEW_OP_PARAMS_INFO)
       flags |= DAV1D_EVENT_FLAG_NEW_OP_PARAMS_INFO;

    return flags;
}
