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
    void *extra_ptr; /* MUST BE AT THE END */
};

static void free_buffer(const uint8_t *const data, void *const user_data) {
    struct pic_ctx_context *pic_ctx = user_data;

    pic_ctx->allocator.release_picture_callback(&pic_ctx->pic,
                                                pic_ctx->allocator.cookie);
    free(pic_ctx);
}

static int picture_alloc_with_edges(Dav1dContext *const c,
                                    Dav1dPicture *const p,
                                    const int w, const int h,
                                    Dav1dSequenceHeader *const seq_hdr, Dav1dRef *const seq_hdr_ref,
                                    Dav1dFrameHeader *const frame_hdr, Dav1dRef *const frame_hdr_ref,
                                    Dav1dContentLightLevel *const content_light, Dav1dRef *const content_light_ref,
                                    Dav1dMasteringDisplay *const mastering_display, Dav1dRef *const mastering_display_ref,
                                    Dav1dITUTT35 *const itut_t35, Dav1dRef *const itut_t35_ref,
                                    const int bpc,
                                    const Dav1dDataProps *const props,
                                    Dav1dPicAllocator *const p_allocator,
                                    const size_t extra, void **const extra_ptr)
{
    if (p->data[0]) {
        dav1d_log(c, "Picture already allocated!\n");
        return -1;
    }
    assert(bpc > 0 && bpc <= 16);

    struct pic_ctx_context *pic_ctx = malloc(extra + sizeof(struct pic_ctx_context));
    if (pic_ctx == NULL)
        return DAV1D_ERR(ENOMEM);

    p->p.w = w;
    p->p.h = h;
    p->seq_hdr = seq_hdr;
    p->frame_hdr = frame_hdr;
    p->content_light = content_light;
    p->mastering_display = mastering_display;
    p->itut_t35 = itut_t35;
    p->p.layout = seq_hdr->layout;
    p->p.bpc = bpc;
    dav1d_data_props_set_defaults(&p->m);
    const int res = p_allocator->alloc_picture_callback(p, p_allocator->cookie);
    if (res < 0) {
        free(pic_ctx);
        return res;
    }

    pic_ctx->allocator = *p_allocator;
    pic_ctx->pic = *p;

    if (!(p->ref = dav1d_ref_wrap(p->data[0], free_buffer, pic_ctx))) {
        p_allocator->release_picture_callback(p, p_allocator->cookie);
        free(pic_ctx);
        dav1d_log(c, "Failed to wrap picture: %s\n", strerror(errno));
        return DAV1D_ERR(ENOMEM);
    }

    p->seq_hdr_ref = seq_hdr_ref;
    if (seq_hdr_ref) dav1d_ref_inc(seq_hdr_ref);

    p->frame_hdr_ref = frame_hdr_ref;
    if (frame_hdr_ref) dav1d_ref_inc(frame_hdr_ref);

    dav1d_data_props_copy(&p->m, props);

    if (extra && extra_ptr)
        *extra_ptr = &pic_ctx->extra_ptr;

    p->content_light_ref = content_light_ref;
    if (content_light_ref) dav1d_ref_inc(content_light_ref);

    p->mastering_display_ref = mastering_display_ref;
    if (mastering_display_ref) dav1d_ref_inc(mastering_display_ref);

    p->itut_t35_ref = itut_t35_ref;
    if (itut_t35_ref) dav1d_ref_inc(itut_t35_ref);

    return 0;
}

int dav1d_thread_picture_alloc(Dav1dContext *const c, Dav1dFrameContext *const f,
                               const int bpc)
{
    Dav1dThreadPicture *const p = &f->sr_cur;
    const int have_frame_mt = c->n_fc > 1;

    const int res =
        picture_alloc_with_edges(c, &p->p, f->frame_hdr->width[1], f->frame_hdr->height,
                                 f->seq_hdr, f->seq_hdr_ref,
                                 f->frame_hdr, f->frame_hdr_ref,
                                 c->content_light, c->content_light_ref,
                                 c->mastering_display, c->mastering_display_ref,
                                 c->itut_t35, c->itut_t35_ref,
                                 bpc, &f->tile[0].data.m, &c->allocator,
                                 have_frame_mt ? sizeof(atomic_int) * 2 : 0,
                                 (void **) &p->progress);
    if (res) return res;

    // Must be removed from the context after being attached to the frame
    dav1d_ref_dec(&c->itut_t35_ref);
    c->itut_t35 = NULL;

    // Don't clear these flags from c->frame_flags if the frame is not visible.
    // This way they will be added to the next visible frame too.
    const int flags_mask = (f->frame_hdr->show_frame || c->output_invisible_frames)
                           ? 0 : (PICTURE_FLAG_NEW_SEQUENCE | PICTURE_FLAG_NEW_OP_PARAMS_INFO);
    p->flags = c->frame_flags;
    c->frame_flags &= flags_mask;

    p->visible = f->frame_hdr->show_frame;
    p->showable = f->frame_hdr->showable_frame;
    if (have_frame_mt) {
        atomic_init(&p->progress[0], 0);
        atomic_init(&p->progress[1], 0);
    }
    return res;
}

int dav1d_picture_alloc_copy(Dav1dContext *const c, Dav1dPicture *const dst, const int w,
                             const Dav1dPicture *const src)
{
    struct pic_ctx_context *const pic_ctx = src->ref->user_data;
    const int res = picture_alloc_with_edges(c, dst, w, src->p.h,
                                             src->seq_hdr, src->seq_hdr_ref,
                                             src->frame_hdr, src->frame_hdr_ref,
                                             src->content_light, src->content_light_ref,
                                             src->mastering_display, src->mastering_display_ref,
                                             src->itut_t35, src->itut_t35_ref,
                                             src->p.bpc, &src->m, &pic_ctx->allocator,
                                             0, NULL);
    return res;
}

void dav1d_picture_ref(Dav1dPicture *const dst, const Dav1dPicture *const src) {
    validate_input(dst != NULL);
    validate_input(dst->data[0] == NULL);
    validate_input(src != NULL);

    if (src->ref) {
        validate_input(src->data[0] != NULL);
        dav1d_ref_inc(src->ref);
        if (src->frame_hdr_ref) dav1d_ref_inc(src->frame_hdr_ref);
        if (src->seq_hdr_ref) dav1d_ref_inc(src->seq_hdr_ref);
        if (src->m.user_data.ref) dav1d_ref_inc(src->m.user_data.ref);
        if (src->content_light_ref) dav1d_ref_inc(src->content_light_ref);
        if (src->mastering_display_ref) dav1d_ref_inc(src->mastering_display_ref);
        if (src->itut_t35_ref) dav1d_ref_inc(src->itut_t35_ref);
    }
    *dst = *src;
}

void dav1d_picture_move_ref(Dav1dPicture *const dst, Dav1dPicture *const src) {
    validate_input(dst != NULL);
    validate_input(dst->data[0] == NULL);
    validate_input(src != NULL);

    if (src->ref)
        validate_input(src->data[0] != NULL);

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
        dav1d_ref_dec(&p->seq_hdr_ref);
        dav1d_ref_dec(&p->frame_hdr_ref);
        dav1d_ref_dec(&p->m.user_data.ref);
        dav1d_ref_dec(&p->content_light_ref);
        dav1d_ref_dec(&p->mastering_display_ref);
        dav1d_ref_dec(&p->itut_t35_ref);
    }
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
