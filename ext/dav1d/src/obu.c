/*
 * Copyright © 2018-2021, VideoLAN and dav1d authors
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
#include <limits.h>
#include <stdio.h>

#include "dav1d/data.h"

#include "common/frame.h"
#include "common/intops.h"
#include "common/validate.h"

#include "src/decode.h"
#include "src/getbits.h"
#include "src/levels.h"
#include "src/log.h"
#include "src/obu.h"
#include "src/ref.h"
#include "src/thread_task.h"

static int check_trailing_bits(GetBits *const gb,
                               const int strict_std_compliance)
{
    const int trailing_one_bit = dav1d_get_bit(gb);

    if (gb->error)
        return DAV1D_ERR(EINVAL);

    if (!strict_std_compliance)
        return 0;

    if (!trailing_one_bit || gb->state)
        return DAV1D_ERR(EINVAL);

    ptrdiff_t size = gb->ptr_end - gb->ptr;
    while (size > 0 && gb->ptr[size - 1] == 0)
        size--;

    if (size)
        return DAV1D_ERR(EINVAL);

    return 0;
}

static NOINLINE int parse_seq_hdr(Dav1dSequenceHeader *const hdr,
                                  GetBits *const gb,
                                  const int strict_std_compliance)
{
#define DEBUG_SEQ_HDR 0

#if DEBUG_SEQ_HDR
    const unsigned init_bit_pos = dav1d_get_bits_pos(gb);
#endif

    memset(hdr, 0, sizeof(*hdr));
    hdr->profile = dav1d_get_bits(gb, 3);
    if (hdr->profile > 2) goto error;
#if DEBUG_SEQ_HDR
    printf("SEQHDR: post-profile: off=%u\n",
           dav1d_get_bits_pos(gb) - init_bit_pos);
#endif

    hdr->still_picture = dav1d_get_bit(gb);
    hdr->reduced_still_picture_header = dav1d_get_bit(gb);
    if (hdr->reduced_still_picture_header && !hdr->still_picture) goto error;
#if DEBUG_SEQ_HDR
    printf("SEQHDR: post-stillpicture_flags: off=%u\n",
           dav1d_get_bits_pos(gb) - init_bit_pos);
#endif

    if (hdr->reduced_still_picture_header) {
        hdr->num_operating_points = 1;
        hdr->operating_points[0].major_level = dav1d_get_bits(gb, 3);
        hdr->operating_points[0].minor_level = dav1d_get_bits(gb, 2);
        hdr->operating_points[0].initial_display_delay = 10;
    } else {
        hdr->timing_info_present = dav1d_get_bit(gb);
        if (hdr->timing_info_present) {
            hdr->num_units_in_tick = dav1d_get_bits(gb, 32);
            hdr->time_scale = dav1d_get_bits(gb, 32);
            if (strict_std_compliance && (!hdr->num_units_in_tick || !hdr->time_scale))
                goto error;
            hdr->equal_picture_interval = dav1d_get_bit(gb);
            if (hdr->equal_picture_interval) {
                const unsigned num_ticks_per_picture = dav1d_get_vlc(gb);
                if (num_ticks_per_picture == UINT32_MAX)
                    goto error;
                hdr->num_ticks_per_picture = num_ticks_per_picture + 1;
            }

            hdr->decoder_model_info_present = dav1d_get_bit(gb);
            if (hdr->decoder_model_info_present) {
                hdr->encoder_decoder_buffer_delay_length = dav1d_get_bits(gb, 5) + 1;
                hdr->num_units_in_decoding_tick = dav1d_get_bits(gb, 32);
                if (strict_std_compliance && !hdr->num_units_in_decoding_tick)
                    goto error;
                hdr->buffer_removal_delay_length = dav1d_get_bits(gb, 5) + 1;
                hdr->frame_presentation_delay_length = dav1d_get_bits(gb, 5) + 1;
            }
        }
#if DEBUG_SEQ_HDR
        printf("SEQHDR: post-timinginfo: off=%u\n",
               dav1d_get_bits_pos(gb) - init_bit_pos);
#endif

        hdr->display_model_info_present = dav1d_get_bit(gb);
        hdr->num_operating_points = dav1d_get_bits(gb, 5) + 1;
        for (int i = 0; i < hdr->num_operating_points; i++) {
            struct Dav1dSequenceHeaderOperatingPoint *const op =
                &hdr->operating_points[i];
            op->idc = dav1d_get_bits(gb, 12);
            if (op->idc && (!(op->idc & 0xff) || !(op->idc & 0xf00)))
                goto error;
            op->major_level = 2 + dav1d_get_bits(gb, 3);
            op->minor_level = dav1d_get_bits(gb, 2);
            if (op->major_level > 3)
                op->tier = dav1d_get_bit(gb);
            if (hdr->decoder_model_info_present) {
                op->decoder_model_param_present = dav1d_get_bit(gb);
                if (op->decoder_model_param_present) {
                    struct Dav1dSequenceHeaderOperatingParameterInfo *const opi =
                        &hdr->operating_parameter_info[i];
                    opi->decoder_buffer_delay =
                        dav1d_get_bits(gb, hdr->encoder_decoder_buffer_delay_length);
                    opi->encoder_buffer_delay =
                        dav1d_get_bits(gb, hdr->encoder_decoder_buffer_delay_length);
                    opi->low_delay_mode = dav1d_get_bit(gb);
                }
            }
            if (hdr->display_model_info_present)
                op->display_model_param_present = dav1d_get_bit(gb);
            op->initial_display_delay =
                op->display_model_param_present ? dav1d_get_bits(gb, 4) + 1 : 10;
        }
#if DEBUG_SEQ_HDR
        printf("SEQHDR: post-operating-points: off=%u\n",
               dav1d_get_bits_pos(gb) - init_bit_pos);
#endif
    }

    hdr->width_n_bits = dav1d_get_bits(gb, 4) + 1;
    hdr->height_n_bits = dav1d_get_bits(gb, 4) + 1;
    hdr->max_width = dav1d_get_bits(gb, hdr->width_n_bits) + 1;
    hdr->max_height = dav1d_get_bits(gb, hdr->height_n_bits) + 1;
#if DEBUG_SEQ_HDR
    printf("SEQHDR: post-size: off=%u\n",
           dav1d_get_bits_pos(gb) - init_bit_pos);
#endif
    if (!hdr->reduced_still_picture_header) {
        hdr->frame_id_numbers_present = dav1d_get_bit(gb);
        if (hdr->frame_id_numbers_present) {
            hdr->delta_frame_id_n_bits = dav1d_get_bits(gb, 4) + 2;
            hdr->frame_id_n_bits = dav1d_get_bits(gb, 3) + hdr->delta_frame_id_n_bits + 1;
        }
    }
#if DEBUG_SEQ_HDR
    printf("SEQHDR: post-frame-id-numbers-present: off=%u\n",
           dav1d_get_bits_pos(gb) - init_bit_pos);
#endif

    hdr->sb128 = dav1d_get_bit(gb);
    hdr->filter_intra = dav1d_get_bit(gb);
    hdr->intra_edge_filter = dav1d_get_bit(gb);
    if (hdr->reduced_still_picture_header) {
        hdr->screen_content_tools = DAV1D_ADAPTIVE;
        hdr->force_integer_mv = DAV1D_ADAPTIVE;
    } else {
        hdr->inter_intra = dav1d_get_bit(gb);
        hdr->masked_compound = dav1d_get_bit(gb);
        hdr->warped_motion = dav1d_get_bit(gb);
        hdr->dual_filter = dav1d_get_bit(gb);
        hdr->order_hint = dav1d_get_bit(gb);
        if (hdr->order_hint) {
            hdr->jnt_comp = dav1d_get_bit(gb);
            hdr->ref_frame_mvs = dav1d_get_bit(gb);
        }
        hdr->screen_content_tools = dav1d_get_bit(gb) ? DAV1D_ADAPTIVE : dav1d_get_bit(gb);
    #if DEBUG_SEQ_HDR
        printf("SEQHDR: post-screentools: off=%u\n",
               dav1d_get_bits_pos(gb) - init_bit_pos);
    #endif
        hdr->force_integer_mv = hdr->screen_content_tools ?
                                dav1d_get_bit(gb) ? DAV1D_ADAPTIVE : dav1d_get_bit(gb) : 2;
        if (hdr->order_hint)
            hdr->order_hint_n_bits = dav1d_get_bits(gb, 3) + 1;
    }
    hdr->super_res = dav1d_get_bit(gb);
    hdr->cdef = dav1d_get_bit(gb);
    hdr->restoration = dav1d_get_bit(gb);
#if DEBUG_SEQ_HDR
    printf("SEQHDR: post-featurebits: off=%u\n",
           dav1d_get_bits_pos(gb) - init_bit_pos);
#endif

    hdr->hbd = dav1d_get_bit(gb);
    if (hdr->profile == 2 && hdr->hbd)
        hdr->hbd += dav1d_get_bit(gb);
    if (hdr->profile != 1)
        hdr->monochrome = dav1d_get_bit(gb);
    hdr->color_description_present = dav1d_get_bit(gb);
    if (hdr->color_description_present) {
        hdr->pri = dav1d_get_bits(gb, 8);
        hdr->trc = dav1d_get_bits(gb, 8);
        hdr->mtrx = dav1d_get_bits(gb, 8);
    } else {
        hdr->pri = DAV1D_COLOR_PRI_UNKNOWN;
        hdr->trc = DAV1D_TRC_UNKNOWN;
        hdr->mtrx = DAV1D_MC_UNKNOWN;
    }
    if (hdr->monochrome) {
        hdr->color_range = dav1d_get_bit(gb);
        hdr->layout = DAV1D_PIXEL_LAYOUT_I400;
        hdr->ss_hor = hdr->ss_ver = 1;
        hdr->chr = DAV1D_CHR_UNKNOWN;
    } else if (hdr->pri == DAV1D_COLOR_PRI_BT709 &&
               hdr->trc == DAV1D_TRC_SRGB &&
               hdr->mtrx == DAV1D_MC_IDENTITY)
    {
        hdr->layout = DAV1D_PIXEL_LAYOUT_I444;
        hdr->color_range = 1;
        if (hdr->profile != 1 && !(hdr->profile == 2 && hdr->hbd == 2))
            goto error;
    } else {
        hdr->color_range = dav1d_get_bit(gb);
        switch (hdr->profile) {
        case 0: hdr->layout = DAV1D_PIXEL_LAYOUT_I420;
                hdr->ss_hor = hdr->ss_ver = 1;
                break;
        case 1: hdr->layout = DAV1D_PIXEL_LAYOUT_I444;
                break;
        case 2:
            if (hdr->hbd == 2) {
                hdr->ss_hor = dav1d_get_bit(gb);
                if (hdr->ss_hor)
                    hdr->ss_ver = dav1d_get_bit(gb);
            } else
                hdr->ss_hor = 1;
            hdr->layout = hdr->ss_hor ?
                          hdr->ss_ver ? DAV1D_PIXEL_LAYOUT_I420 :
                                        DAV1D_PIXEL_LAYOUT_I422 :
                                        DAV1D_PIXEL_LAYOUT_I444;
            break;
        }
        hdr->chr = (hdr->ss_hor & hdr->ss_ver) ?
                   dav1d_get_bits(gb, 2) : DAV1D_CHR_UNKNOWN;
    }
    if (strict_std_compliance &&
        hdr->mtrx == DAV1D_MC_IDENTITY && hdr->layout != DAV1D_PIXEL_LAYOUT_I444)
    {
        goto error;
    }
    if (!hdr->monochrome)
        hdr->separate_uv_delta_q = dav1d_get_bit(gb);
#if DEBUG_SEQ_HDR
    printf("SEQHDR: post-colorinfo: off=%u\n",
           dav1d_get_bits_pos(gb) - init_bit_pos);
#endif

    hdr->film_grain_present = dav1d_get_bit(gb);
#if DEBUG_SEQ_HDR
    printf("SEQHDR: post-filmgrain: off=%u\n",
           dav1d_get_bits_pos(gb) - init_bit_pos);
#endif

    // We needn't bother flushing the OBU here: we'll check we didn't
    // overrun in the caller and will then discard gb, so there's no
    // point in setting its position properly.

    return check_trailing_bits(gb, strict_std_compliance);

error:
    return DAV1D_ERR(EINVAL);
}

int dav1d_parse_sequence_header(Dav1dSequenceHeader *const out,
                                const uint8_t *const ptr, const size_t sz)
{
    validate_input_or_ret(out != NULL, DAV1D_ERR(EINVAL));
    validate_input_or_ret(ptr != NULL, DAV1D_ERR(EINVAL));
    validate_input_or_ret(sz > 0 && sz <= SIZE_MAX / 2, DAV1D_ERR(EINVAL));

    GetBits gb;
    dav1d_init_get_bits(&gb, ptr, sz);
    int res = DAV1D_ERR(ENOENT);

    do {
        dav1d_get_bit(&gb); // obu_forbidden_bit
        const enum Dav1dObuType type = dav1d_get_bits(&gb, 4);
        const int has_extension = dav1d_get_bit(&gb);
        const int has_length_field = dav1d_get_bit(&gb);
        dav1d_get_bits(&gb, 1 + 8 * has_extension); // ignore

        const uint8_t *obu_end = gb.ptr_end;
        if (has_length_field) {
            const size_t len = dav1d_get_uleb128(&gb);
            if (len > (size_t)(obu_end - gb.ptr)) return DAV1D_ERR(EINVAL);
            obu_end = gb.ptr + len;
        }

        if (type == DAV1D_OBU_SEQ_HDR) {
            if ((res = parse_seq_hdr(out, &gb, 0)) < 0) return res;
            if (gb.ptr > obu_end) return DAV1D_ERR(EINVAL);
            dav1d_bytealign_get_bits(&gb);
        }

        if (gb.error) return DAV1D_ERR(EINVAL);
        assert(gb.state == 0 && gb.bits_left == 0);
        gb.ptr = obu_end;
    } while (gb.ptr < gb.ptr_end);

    return res;
}

static int read_frame_size(Dav1dContext *const c, GetBits *const gb,
                           const int use_ref)
{
    const Dav1dSequenceHeader *const seqhdr = c->seq_hdr;
    Dav1dFrameHeader *const hdr = c->frame_hdr;

    if (use_ref) {
        for (int i = 0; i < 7; i++) {
            if (dav1d_get_bit(gb)) {
                const Dav1dThreadPicture *const ref =
                    &c->refs[c->frame_hdr->refidx[i]].p;
                if (!ref->p.frame_hdr) return -1;
                hdr->width[1] = ref->p.frame_hdr->width[1];
                hdr->height = ref->p.frame_hdr->height;
                hdr->render_width = ref->p.frame_hdr->render_width;
                hdr->render_height = ref->p.frame_hdr->render_height;
                hdr->super_res.enabled = seqhdr->super_res && dav1d_get_bit(gb);
                if (hdr->super_res.enabled) {
                    const int d = hdr->super_res.width_scale_denominator =
                        9 + dav1d_get_bits(gb, 3);
                    hdr->width[0] = imax((hdr->width[1] * 8 + (d >> 1)) / d,
                                         imin(16, hdr->width[1]));
                } else {
                    hdr->super_res.width_scale_denominator = 8;
                    hdr->width[0] = hdr->width[1];
                }
                return 0;
            }
        }
    }

    if (hdr->frame_size_override) {
        hdr->width[1] = dav1d_get_bits(gb, seqhdr->width_n_bits) + 1;
        hdr->height = dav1d_get_bits(gb, seqhdr->height_n_bits) + 1;
    } else {
        hdr->width[1] = seqhdr->max_width;
        hdr->height = seqhdr->max_height;
    }
    hdr->super_res.enabled = seqhdr->super_res && dav1d_get_bit(gb);
    if (hdr->super_res.enabled) {
        const int d = hdr->super_res.width_scale_denominator = 9 + dav1d_get_bits(gb, 3);
        hdr->width[0] = imax((hdr->width[1] * 8 + (d >> 1)) / d, imin(16, hdr->width[1]));
    } else {
        hdr->super_res.width_scale_denominator = 8;
        hdr->width[0] = hdr->width[1];
    }
    hdr->have_render_size = dav1d_get_bit(gb);
    if (hdr->have_render_size) {
        hdr->render_width = dav1d_get_bits(gb, 16) + 1;
        hdr->render_height = dav1d_get_bits(gb, 16) + 1;
    } else {
        hdr->render_width = hdr->width[1];
        hdr->render_height = hdr->height;
    }
    return 0;
}

static inline int tile_log2(const int sz, const int tgt) {
    int k;
    for (k = 0; (sz << k) < tgt; k++) ;
    return k;
}

static const Dav1dLoopfilterModeRefDeltas default_mode_ref_deltas = {
    .mode_delta = { 0, 0 },
    .ref_delta = { 1, 0, 0, 0, -1, 0, -1, -1 },
};

static int parse_frame_hdr(Dav1dContext *const c, GetBits *const gb) {
#define DEBUG_FRAME_HDR 0

#if DEBUG_FRAME_HDR
    const uint8_t *const init_ptr = gb->ptr;
#endif
    const Dav1dSequenceHeader *const seqhdr = c->seq_hdr;
    Dav1dFrameHeader *const hdr = c->frame_hdr;

    if (!seqhdr->reduced_still_picture_header)
        hdr->show_existing_frame = dav1d_get_bit(gb);
#if DEBUG_FRAME_HDR
    printf("HDR: post-show_existing_frame: off=%td\n",
           (gb->ptr - init_ptr) * 8 - gb->bits_left);
#endif
    if (hdr->show_existing_frame) {
        hdr->existing_frame_idx = dav1d_get_bits(gb, 3);
        if (seqhdr->decoder_model_info_present && !seqhdr->equal_picture_interval)
            hdr->frame_presentation_delay = dav1d_get_bits(gb, seqhdr->frame_presentation_delay_length);
        if (seqhdr->frame_id_numbers_present) {
            hdr->frame_id = dav1d_get_bits(gb, seqhdr->frame_id_n_bits);
            Dav1dFrameHeader *const ref_frame_hdr = c->refs[hdr->existing_frame_idx].p.p.frame_hdr;
            if (!ref_frame_hdr || ref_frame_hdr->frame_id != hdr->frame_id) goto error;
        }
        return 0;
    }

    if (seqhdr->reduced_still_picture_header) {
        hdr->frame_type = DAV1D_FRAME_TYPE_KEY;
        hdr->show_frame = 1;
    } else {
        hdr->frame_type = dav1d_get_bits(gb, 2);
        hdr->show_frame = dav1d_get_bit(gb);
    }
    if (hdr->show_frame) {
        if (seqhdr->decoder_model_info_present && !seqhdr->equal_picture_interval)
            hdr->frame_presentation_delay = dav1d_get_bits(gb, seqhdr->frame_presentation_delay_length);
        hdr->showable_frame = hdr->frame_type != DAV1D_FRAME_TYPE_KEY;
    } else
        hdr->showable_frame = dav1d_get_bit(gb);
    hdr->error_resilient_mode =
        (hdr->frame_type == DAV1D_FRAME_TYPE_KEY && hdr->show_frame) ||
        hdr->frame_type == DAV1D_FRAME_TYPE_SWITCH ||
        seqhdr->reduced_still_picture_header || dav1d_get_bit(gb);
#if DEBUG_FRAME_HDR
    printf("HDR: post-frametype_bits: off=%td\n",
           (gb->ptr - init_ptr) * 8 - gb->bits_left);
#endif
    hdr->disable_cdf_update = dav1d_get_bit(gb);
    hdr->allow_screen_content_tools = seqhdr->screen_content_tools == DAV1D_ADAPTIVE ?
                                      dav1d_get_bit(gb) : seqhdr->screen_content_tools;
    if (hdr->allow_screen_content_tools)
        hdr->force_integer_mv = seqhdr->force_integer_mv == DAV1D_ADAPTIVE ?
                                dav1d_get_bit(gb) : seqhdr->force_integer_mv;

    if (IS_KEY_OR_INTRA(hdr))
        hdr->force_integer_mv = 1;

    if (seqhdr->frame_id_numbers_present)
        hdr->frame_id = dav1d_get_bits(gb, seqhdr->frame_id_n_bits);

    if (!seqhdr->reduced_still_picture_header)
        hdr->frame_size_override = hdr->frame_type == DAV1D_FRAME_TYPE_SWITCH ? 1 : dav1d_get_bit(gb);
#if DEBUG_FRAME_HDR
    printf("HDR: post-frame_size_override_flag: off=%td\n",
           (gb->ptr - init_ptr) * 8 - gb->bits_left);
#endif
    if (seqhdr->order_hint)
        hdr->frame_offset = dav1d_get_bits(gb, seqhdr->order_hint_n_bits);
    hdr->primary_ref_frame = !hdr->error_resilient_mode && IS_INTER_OR_SWITCH(hdr) ?
                             dav1d_get_bits(gb, 3) : DAV1D_PRIMARY_REF_NONE;

    if (seqhdr->decoder_model_info_present) {
        hdr->buffer_removal_time_present = dav1d_get_bit(gb);
        if (hdr->buffer_removal_time_present) {
            for (int i = 0; i < c->seq_hdr->num_operating_points; i++) {
                const struct Dav1dSequenceHeaderOperatingPoint *const seqop = &seqhdr->operating_points[i];
                struct Dav1dFrameHeaderOperatingPoint *const op = &hdr->operating_points[i];
                if (seqop->decoder_model_param_present) {
                    int in_temporal_layer = (seqop->idc >> hdr->temporal_id) & 1;
                    int in_spatial_layer  = (seqop->idc >> (hdr->spatial_id + 8)) & 1;
                    if (!seqop->idc || (in_temporal_layer && in_spatial_layer))
                        op->buffer_removal_time = dav1d_get_bits(gb, seqhdr->buffer_removal_delay_length);
                }
            }
        }
    }

    if (IS_KEY_OR_INTRA(hdr)) {
        hdr->refresh_frame_flags = (hdr->frame_type == DAV1D_FRAME_TYPE_KEY &&
                                    hdr->show_frame) ? 0xff : dav1d_get_bits(gb, 8);
        if (hdr->refresh_frame_flags != 0xff && hdr->error_resilient_mode && seqhdr->order_hint)
            for (int i = 0; i < 8; i++)
                dav1d_get_bits(gb, seqhdr->order_hint_n_bits);
        if (c->strict_std_compliance &&
            hdr->frame_type == DAV1D_FRAME_TYPE_INTRA && hdr->refresh_frame_flags == 0xff)
        {
            goto error;
        }
        if (read_frame_size(c, gb, 0) < 0) goto error;
        if (hdr->allow_screen_content_tools && !hdr->super_res.enabled)
            hdr->allow_intrabc = dav1d_get_bit(gb);
    } else {
        hdr->refresh_frame_flags = hdr->frame_type == DAV1D_FRAME_TYPE_SWITCH ? 0xff :
                                   dav1d_get_bits(gb, 8);
        if (hdr->error_resilient_mode && seqhdr->order_hint)
            for (int i = 0; i < 8; i++)
                dav1d_get_bits(gb, seqhdr->order_hint_n_bits);
        if (seqhdr->order_hint) {
            hdr->frame_ref_short_signaling = dav1d_get_bit(gb);
            if (hdr->frame_ref_short_signaling) {
                hdr->refidx[0] = dav1d_get_bits(gb, 3);
                hdr->refidx[1] = hdr->refidx[2] = -1;
                hdr->refidx[3] = dav1d_get_bits(gb, 3);

                /* +1 allows for unconditional stores, as unused
                 * values can be dumped into frame_offset[-1]. */
                int frame_offset_mem[8+1];
                int *const frame_offset = &frame_offset_mem[1];
                int earliest_ref = -1;
                for (int i = 0, earliest_offset = INT_MAX; i < 8; i++) {
                    const Dav1dFrameHeader *const refhdr = c->refs[i].p.p.frame_hdr;
                    if (!refhdr) goto error;
                    const int diff = get_poc_diff(seqhdr->order_hint_n_bits,
                                                  refhdr->frame_offset,
                                                  hdr->frame_offset);
                    frame_offset[i] = diff;
                    if (diff < earliest_offset) {
                        earliest_offset = diff;
                        earliest_ref = i;
                    }
                }
                frame_offset[hdr->refidx[0]] = INT_MIN; // = reference frame is used
                frame_offset[hdr->refidx[3]] = INT_MIN;
                assert(earliest_ref >= 0);

                int refidx = -1;
                for (int i = 0, latest_offset = 0; i < 8; i++) {
                    const int hint = frame_offset[i];
                    if (hint >= latest_offset) {
                        latest_offset = hint;
                        refidx = i;
                    }
                }
                frame_offset[refidx] = INT_MIN;
                hdr->refidx[6] = refidx;

                for (int i = 4; i < 6; i++) {
                    /* Unsigned compares to handle negative values. */
                    unsigned earliest_offset = UINT8_MAX;
                    refidx = -1;
                    for (int j = 0; j < 8; j++) {
                        const unsigned hint = frame_offset[j];
                        if (hint < earliest_offset) {
                            earliest_offset = hint;
                            refidx = j;
                        }
                    }
                    frame_offset[refidx] = INT_MIN;
                    hdr->refidx[i] = refidx;
                }

                for (int i = 1; i < 7; i++) {
                    refidx = hdr->refidx[i];
                    if (refidx < 0) {
                        unsigned latest_offset = ~UINT8_MAX;
                        for (int j = 0; j < 8; j++) {
                            const unsigned hint = frame_offset[j];
                            if (hint >= latest_offset) {
                                latest_offset = hint;
                                refidx = j;
                            }
                        }
                        frame_offset[refidx] = INT_MIN;
                        hdr->refidx[i] = refidx >= 0 ? refidx : earliest_ref;
                    }
                }
            }
        }
        for (int i = 0; i < 7; i++) {
            if (!hdr->frame_ref_short_signaling)
                hdr->refidx[i] = dav1d_get_bits(gb, 3);
            if (seqhdr->frame_id_numbers_present) {
                const unsigned delta_ref_frame_id = dav1d_get_bits(gb, seqhdr->delta_frame_id_n_bits) + 1;
                const unsigned ref_frame_id = (hdr->frame_id + (1 << seqhdr->frame_id_n_bits) - delta_ref_frame_id) & ((1 << seqhdr->frame_id_n_bits) - 1);
                Dav1dFrameHeader *const ref_frame_hdr = c->refs[hdr->refidx[i]].p.p.frame_hdr;
                if (!ref_frame_hdr || ref_frame_hdr->frame_id != ref_frame_id) goto error;
            }
        }
        const int use_ref = !hdr->error_resilient_mode &&
                            hdr->frame_size_override;
        if (read_frame_size(c, gb, use_ref) < 0) goto error;
        if (!hdr->force_integer_mv)
            hdr->hp = dav1d_get_bit(gb);
        hdr->subpel_filter_mode = dav1d_get_bit(gb) ? DAV1D_FILTER_SWITCHABLE :
                                                      dav1d_get_bits(gb, 2);
        hdr->switchable_motion_mode = dav1d_get_bit(gb);
        if (!hdr->error_resilient_mode && seqhdr->ref_frame_mvs &&
            seqhdr->order_hint && IS_INTER_OR_SWITCH(hdr))
        {
            hdr->use_ref_frame_mvs = dav1d_get_bit(gb);
        }
    }
#if DEBUG_FRAME_HDR
    printf("HDR: post-frametype-specific-bits: off=%td\n",
           (gb->ptr - init_ptr) * 8 - gb->bits_left);
#endif

    if (!seqhdr->reduced_still_picture_header && !hdr->disable_cdf_update)
        hdr->refresh_context = !dav1d_get_bit(gb);
#if DEBUG_FRAME_HDR
    printf("HDR: post-refresh_context: off=%td\n",
           (gb->ptr - init_ptr) * 8 - gb->bits_left);
#endif

    // tile data
    hdr->tiling.uniform = dav1d_get_bit(gb);
    const int sbsz_min1 = (64 << seqhdr->sb128) - 1;
    const int sbsz_log2 = 6 + seqhdr->sb128;
    const int sbw = (hdr->width[0] + sbsz_min1) >> sbsz_log2;
    const int sbh = (hdr->height + sbsz_min1) >> sbsz_log2;
    const int max_tile_width_sb = 4096 >> sbsz_log2;
    const int max_tile_area_sb = 4096 * 2304 >> (2 * sbsz_log2);
    hdr->tiling.min_log2_cols = tile_log2(max_tile_width_sb, sbw);
    hdr->tiling.max_log2_cols = tile_log2(1, imin(sbw, DAV1D_MAX_TILE_COLS));
    hdr->tiling.max_log2_rows = tile_log2(1, imin(sbh, DAV1D_MAX_TILE_ROWS));
    const int min_log2_tiles = imax(tile_log2(max_tile_area_sb, sbw * sbh),
                              hdr->tiling.min_log2_cols);
    if (hdr->tiling.uniform) {
        for (hdr->tiling.log2_cols = hdr->tiling.min_log2_cols;
             hdr->tiling.log2_cols < hdr->tiling.max_log2_cols && dav1d_get_bit(gb);
             hdr->tiling.log2_cols++) ;
        const int tile_w = 1 + ((sbw - 1) >> hdr->tiling.log2_cols);
        hdr->tiling.cols = 0;
        for (int sbx = 0; sbx < sbw; sbx += tile_w, hdr->tiling.cols++)
            hdr->tiling.col_start_sb[hdr->tiling.cols] = sbx;
        hdr->tiling.min_log2_rows =
            imax(min_log2_tiles - hdr->tiling.log2_cols, 0);

        for (hdr->tiling.log2_rows = hdr->tiling.min_log2_rows;
             hdr->tiling.log2_rows < hdr->tiling.max_log2_rows && dav1d_get_bit(gb);
             hdr->tiling.log2_rows++) ;
        const int tile_h = 1 + ((sbh - 1) >> hdr->tiling.log2_rows);
        hdr->tiling.rows = 0;
        for (int sby = 0; sby < sbh; sby += tile_h, hdr->tiling.rows++)
            hdr->tiling.row_start_sb[hdr->tiling.rows] = sby;
    } else {
        hdr->tiling.cols = 0;
        int widest_tile = 0, max_tile_area_sb = sbw * sbh;
        for (int sbx = 0; sbx < sbw && hdr->tiling.cols < DAV1D_MAX_TILE_COLS; hdr->tiling.cols++) {
            const int tile_width_sb = imin(sbw - sbx, max_tile_width_sb);
            const int tile_w = (tile_width_sb > 1) ? 1 + dav1d_get_uniform(gb, tile_width_sb) : 1;
            hdr->tiling.col_start_sb[hdr->tiling.cols] = sbx;
            sbx += tile_w;
            widest_tile = imax(widest_tile, tile_w);
        }
        hdr->tiling.log2_cols = tile_log2(1, hdr->tiling.cols);
        if (min_log2_tiles) max_tile_area_sb >>= min_log2_tiles + 1;
        const int max_tile_height_sb = imax(max_tile_area_sb / widest_tile, 1);

        hdr->tiling.rows = 0;
        for (int sby = 0; sby < sbh && hdr->tiling.rows < DAV1D_MAX_TILE_ROWS; hdr->tiling.rows++) {
            const int tile_height_sb = imin(sbh - sby, max_tile_height_sb);
            const int tile_h = (tile_height_sb > 1) ? 1 + dav1d_get_uniform(gb, tile_height_sb) : 1;
            hdr->tiling.row_start_sb[hdr->tiling.rows] = sby;
            sby += tile_h;
        }
        hdr->tiling.log2_rows = tile_log2(1, hdr->tiling.rows);
    }
    hdr->tiling.col_start_sb[hdr->tiling.cols] = sbw;
    hdr->tiling.row_start_sb[hdr->tiling.rows] = sbh;
    if (hdr->tiling.log2_cols || hdr->tiling.log2_rows) {
        hdr->tiling.update = dav1d_get_bits(gb, hdr->tiling.log2_cols + hdr->tiling.log2_rows);
        if (hdr->tiling.update >= hdr->tiling.cols * hdr->tiling.rows)
            goto error;
        hdr->tiling.n_bytes = dav1d_get_bits(gb, 2) + 1;
    }
#if DEBUG_FRAME_HDR
    printf("HDR: post-tiling: off=%td\n",
           (gb->ptr - init_ptr) * 8 - gb->bits_left);
#endif

    // quant data
    hdr->quant.yac = dav1d_get_bits(gb, 8);
    if (dav1d_get_bit(gb))
        hdr->quant.ydc_delta = dav1d_get_sbits(gb, 7);
    if (!seqhdr->monochrome) {
        // If the sequence header says that delta_q might be different
        // for U, V, we must check whether it actually is for this
        // frame.
        const int diff_uv_delta = seqhdr->separate_uv_delta_q ? dav1d_get_bit(gb) : 0;
        if (dav1d_get_bit(gb))
            hdr->quant.udc_delta = dav1d_get_sbits(gb, 7);
        if (dav1d_get_bit(gb))
            hdr->quant.uac_delta = dav1d_get_sbits(gb, 7);
        if (diff_uv_delta) {
            if (dav1d_get_bit(gb))
                hdr->quant.vdc_delta = dav1d_get_sbits(gb, 7);
            if (dav1d_get_bit(gb))
                hdr->quant.vac_delta = dav1d_get_sbits(gb, 7);
        } else {
            hdr->quant.vdc_delta = hdr->quant.udc_delta;
            hdr->quant.vac_delta = hdr->quant.uac_delta;
        }
    }
#if DEBUG_FRAME_HDR
    printf("HDR: post-quant: off=%td\n",
           (gb->ptr - init_ptr) * 8 - gb->bits_left);
#endif
    hdr->quant.qm = dav1d_get_bit(gb);
    if (hdr->quant.qm) {
        hdr->quant.qm_y = dav1d_get_bits(gb, 4);
        hdr->quant.qm_u = dav1d_get_bits(gb, 4);
        hdr->quant.qm_v = seqhdr->separate_uv_delta_q ? dav1d_get_bits(gb, 4) :
                                                        hdr->quant.qm_u;
    }
#if DEBUG_FRAME_HDR
    printf("HDR: post-qm: off=%td\n",
           (gb->ptr - init_ptr) * 8 - gb->bits_left);
#endif

    // segmentation data
    hdr->segmentation.enabled = dav1d_get_bit(gb);
    if (hdr->segmentation.enabled) {
        if (hdr->primary_ref_frame == DAV1D_PRIMARY_REF_NONE) {
            hdr->segmentation.update_map = 1;
            hdr->segmentation.update_data = 1;
        } else {
            hdr->segmentation.update_map = dav1d_get_bit(gb);
            if (hdr->segmentation.update_map)
                hdr->segmentation.temporal = dav1d_get_bit(gb);
            hdr->segmentation.update_data = dav1d_get_bit(gb);
        }

        if (hdr->segmentation.update_data) {
            hdr->segmentation.seg_data.last_active_segid = -1;
            for (int i = 0; i < DAV1D_MAX_SEGMENTS; i++) {
                Dav1dSegmentationData *const seg =
                    &hdr->segmentation.seg_data.d[i];
                if (dav1d_get_bit(gb)) {
                    seg->delta_q = dav1d_get_sbits(gb, 9);
                    hdr->segmentation.seg_data.last_active_segid = i;
                }
                if (dav1d_get_bit(gb)) {
                    seg->delta_lf_y_v = dav1d_get_sbits(gb, 7);
                    hdr->segmentation.seg_data.last_active_segid = i;
                }
                if (dav1d_get_bit(gb)) {
                    seg->delta_lf_y_h = dav1d_get_sbits(gb, 7);
                    hdr->segmentation.seg_data.last_active_segid = i;
                }
                if (dav1d_get_bit(gb)) {
                    seg->delta_lf_u = dav1d_get_sbits(gb, 7);
                    hdr->segmentation.seg_data.last_active_segid = i;
                }
                if (dav1d_get_bit(gb)) {
                    seg->delta_lf_v = dav1d_get_sbits(gb, 7);
                    hdr->segmentation.seg_data.last_active_segid = i;
                }
                if (dav1d_get_bit(gb)) {
                    seg->ref = dav1d_get_bits(gb, 3);
                    hdr->segmentation.seg_data.last_active_segid = i;
                    hdr->segmentation.seg_data.preskip = 1;
                } else {
                    seg->ref = -1;
                }
                if ((seg->skip = dav1d_get_bit(gb))) {
                    hdr->segmentation.seg_data.last_active_segid = i;
                    hdr->segmentation.seg_data.preskip = 1;
                }
                if ((seg->globalmv = dav1d_get_bit(gb))) {
                    hdr->segmentation.seg_data.last_active_segid = i;
                    hdr->segmentation.seg_data.preskip = 1;
                }
            }
        } else {
            // segmentation.update_data was false so we should copy
            // segmentation data from the reference frame.
            assert(hdr->primary_ref_frame != DAV1D_PRIMARY_REF_NONE);
            const int pri_ref = hdr->refidx[hdr->primary_ref_frame];
            if (!c->refs[pri_ref].p.p.frame_hdr) goto error;
            hdr->segmentation.seg_data =
                c->refs[pri_ref].p.p.frame_hdr->segmentation.seg_data;
        }
    } else {
        for (int i = 0; i < DAV1D_MAX_SEGMENTS; i++)
            hdr->segmentation.seg_data.d[i].ref = -1;
    }
#if DEBUG_FRAME_HDR
    printf("HDR: post-segmentation: off=%td\n",
           (gb->ptr - init_ptr) * 8 - gb->bits_left);
#endif

    // delta q
    if (hdr->quant.yac) {
        hdr->delta.q.present = dav1d_get_bit(gb);
        if (hdr->delta.q.present) {
            hdr->delta.q.res_log2 = dav1d_get_bits(gb, 2);
            if (!hdr->allow_intrabc) {
                hdr->delta.lf.present = dav1d_get_bit(gb);
                if (hdr->delta.lf.present) {
                    hdr->delta.lf.res_log2 = dav1d_get_bits(gb, 2);
                    hdr->delta.lf.multi = dav1d_get_bit(gb);
                }
            }
        }
    }
#if DEBUG_FRAME_HDR
    printf("HDR: post-delta_q_lf_flags: off=%td\n",
           (gb->ptr - init_ptr) * 8 - gb->bits_left);
#endif

    // derive lossless flags
    const int delta_lossless = !hdr->quant.ydc_delta && !hdr->quant.udc_delta &&
        !hdr->quant.uac_delta && !hdr->quant.vdc_delta && !hdr->quant.vac_delta;
    hdr->all_lossless = 1;
    for (int i = 0; i < DAV1D_MAX_SEGMENTS; i++) {
        hdr->segmentation.qidx[i] = hdr->segmentation.enabled ?
            iclip_u8(hdr->quant.yac + hdr->segmentation.seg_data.d[i].delta_q) :
            hdr->quant.yac;
        hdr->segmentation.lossless[i] =
            !hdr->segmentation.qidx[i] && delta_lossless;
        hdr->all_lossless &= hdr->segmentation.lossless[i];
    }

    // loopfilter
    if (hdr->all_lossless || hdr->allow_intrabc) {
        hdr->loopfilter.mode_ref_delta_enabled = 1;
        hdr->loopfilter.mode_ref_delta_update = 1;
        hdr->loopfilter.mode_ref_deltas = default_mode_ref_deltas;
    } else {
        hdr->loopfilter.level_y[0] = dav1d_get_bits(gb, 6);
        hdr->loopfilter.level_y[1] = dav1d_get_bits(gb, 6);
        if (!seqhdr->monochrome &&
            (hdr->loopfilter.level_y[0] || hdr->loopfilter.level_y[1]))
        {
            hdr->loopfilter.level_u = dav1d_get_bits(gb, 6);
            hdr->loopfilter.level_v = dav1d_get_bits(gb, 6);
        }
        hdr->loopfilter.sharpness = dav1d_get_bits(gb, 3);

        if (hdr->primary_ref_frame == DAV1D_PRIMARY_REF_NONE) {
            hdr->loopfilter.mode_ref_deltas = default_mode_ref_deltas;
        } else {
            const int ref = hdr->refidx[hdr->primary_ref_frame];
            if (!c->refs[ref].p.p.frame_hdr) goto error;
            hdr->loopfilter.mode_ref_deltas =
                c->refs[ref].p.p.frame_hdr->loopfilter.mode_ref_deltas;
        }
        hdr->loopfilter.mode_ref_delta_enabled = dav1d_get_bit(gb);
        if (hdr->loopfilter.mode_ref_delta_enabled) {
            hdr->loopfilter.mode_ref_delta_update = dav1d_get_bit(gb);
            if (hdr->loopfilter.mode_ref_delta_update) {
                for (int i = 0; i < 8; i++)
                    if (dav1d_get_bit(gb))
                        hdr->loopfilter.mode_ref_deltas.ref_delta[i] =
                            dav1d_get_sbits(gb, 7);
                for (int i = 0; i < 2; i++)
                    if (dav1d_get_bit(gb))
                        hdr->loopfilter.mode_ref_deltas.mode_delta[i] =
                            dav1d_get_sbits(gb, 7);
            }
        }
    }
#if DEBUG_FRAME_HDR
    printf("HDR: post-lpf: off=%td\n",
           (gb->ptr - init_ptr) * 8 - gb->bits_left);
#endif

    // cdef
    if (!hdr->all_lossless && seqhdr->cdef && !hdr->allow_intrabc) {
        hdr->cdef.damping = dav1d_get_bits(gb, 2) + 3;
        hdr->cdef.n_bits = dav1d_get_bits(gb, 2);
        for (int i = 0; i < (1 << hdr->cdef.n_bits); i++) {
            hdr->cdef.y_strength[i] = dav1d_get_bits(gb, 6);
            if (!seqhdr->monochrome)
                hdr->cdef.uv_strength[i] = dav1d_get_bits(gb, 6);
        }
    }
#if DEBUG_FRAME_HDR
    printf("HDR: post-cdef: off=%td\n",
           (gb->ptr - init_ptr) * 8 - gb->bits_left);
#endif

    // restoration
    if ((!hdr->all_lossless || hdr->super_res.enabled) &&
        seqhdr->restoration && !hdr->allow_intrabc)
    {
        hdr->restoration.type[0] = dav1d_get_bits(gb, 2);
        if (!seqhdr->monochrome) {
            hdr->restoration.type[1] = dav1d_get_bits(gb, 2);
            hdr->restoration.type[2] = dav1d_get_bits(gb, 2);
        }

        if (hdr->restoration.type[0] || hdr->restoration.type[1] ||
            hdr->restoration.type[2])
        {
            // Log2 of the restoration unit size.
            hdr->restoration.unit_size[0] = 6 + seqhdr->sb128;
            if (dav1d_get_bit(gb)) {
                hdr->restoration.unit_size[0]++;
                if (!seqhdr->sb128)
                    hdr->restoration.unit_size[0] += dav1d_get_bit(gb);
            }
            hdr->restoration.unit_size[1] = hdr->restoration.unit_size[0];
            if ((hdr->restoration.type[1] || hdr->restoration.type[2]) &&
                seqhdr->ss_hor == 1 && seqhdr->ss_ver == 1)
            {
                hdr->restoration.unit_size[1] -= dav1d_get_bit(gb);
            }
        } else {
            hdr->restoration.unit_size[0] = 8;
        }
    }
#if DEBUG_FRAME_HDR
    printf("HDR: post-restoration: off=%td\n",
           (gb->ptr - init_ptr) * 8 - gb->bits_left);
#endif

    if (!hdr->all_lossless)
        hdr->txfm_mode = dav1d_get_bit(gb) ? DAV1D_TX_SWITCHABLE : DAV1D_TX_LARGEST;
#if DEBUG_FRAME_HDR
    printf("HDR: post-txfmmode: off=%td\n",
           (gb->ptr - init_ptr) * 8 - gb->bits_left);
#endif
    if (IS_INTER_OR_SWITCH(hdr))
        hdr->switchable_comp_refs = dav1d_get_bit(gb);
#if DEBUG_FRAME_HDR
    printf("HDR: post-refmode: off=%td\n",
           (gb->ptr - init_ptr) * 8 - gb->bits_left);
#endif
    if (hdr->switchable_comp_refs && IS_INTER_OR_SWITCH(hdr) && seqhdr->order_hint) {
        const int poc = hdr->frame_offset;
        int off_before = -1, off_after = -1;
        int off_before_idx, off_after_idx;
        for (int i = 0; i < 7; i++) {
            if (!c->refs[hdr->refidx[i]].p.p.frame_hdr) goto error;
            const int refpoc = c->refs[hdr->refidx[i]].p.p.frame_hdr->frame_offset;

            const int diff = get_poc_diff(seqhdr->order_hint_n_bits, refpoc, poc);
            if (diff > 0) {
                if (off_after < 0 || get_poc_diff(seqhdr->order_hint_n_bits,
                                                  off_after, refpoc) > 0)
                {
                    off_after = refpoc;
                    off_after_idx = i;
                }
            } else if (diff < 0 && (off_before < 0 ||
                                    get_poc_diff(seqhdr->order_hint_n_bits,
                                                 refpoc, off_before) > 0))
            {
                off_before = refpoc;
                off_before_idx = i;
            }
        }

        if ((off_before | off_after) >= 0) {
            hdr->skip_mode_refs[0] = imin(off_before_idx, off_after_idx);
            hdr->skip_mode_refs[1] = imax(off_before_idx, off_after_idx);
            hdr->skip_mode_allowed = 1;
        } else if (off_before >= 0) {
            int off_before2 = -1;
            int off_before2_idx;
            for (int i = 0; i < 7; i++) {
                if (!c->refs[hdr->refidx[i]].p.p.frame_hdr) goto error;
                const int refpoc = c->refs[hdr->refidx[i]].p.p.frame_hdr->frame_offset;
                if (get_poc_diff(seqhdr->order_hint_n_bits,
                                 refpoc, off_before) < 0) {
                    if (off_before2 < 0 || get_poc_diff(seqhdr->order_hint_n_bits,
                                                        refpoc, off_before2) > 0)
                    {
                        off_before2 = refpoc;
                        off_before2_idx = i;
                    }
                }
            }

            if (off_before2 >= 0) {
                hdr->skip_mode_refs[0] = imin(off_before_idx, off_before2_idx);
                hdr->skip_mode_refs[1] = imax(off_before_idx, off_before2_idx);
                hdr->skip_mode_allowed = 1;
            }
        }
    }
    if (hdr->skip_mode_allowed)
        hdr->skip_mode_enabled = dav1d_get_bit(gb);
#if DEBUG_FRAME_HDR
    printf("HDR: post-extskip: off=%td\n",
           (gb->ptr - init_ptr) * 8 - gb->bits_left);
#endif
    if (!hdr->error_resilient_mode && IS_INTER_OR_SWITCH(hdr) && seqhdr->warped_motion)
        hdr->warp_motion = dav1d_get_bit(gb);
#if DEBUG_FRAME_HDR
    printf("HDR: post-warpmotionbit: off=%td\n",
           (gb->ptr - init_ptr) * 8 - gb->bits_left);
#endif
    hdr->reduced_txtp_set = dav1d_get_bit(gb);
#if DEBUG_FRAME_HDR
    printf("HDR: post-reducedtxtpset: off=%td\n",
           (gb->ptr - init_ptr) * 8 - gb->bits_left);
#endif

    for (int i = 0; i < 7; i++)
        hdr->gmv[i] = dav1d_default_wm_params;

    if (IS_INTER_OR_SWITCH(hdr)) {
        for (int i = 0; i < 7; i++) {
            hdr->gmv[i].type = !dav1d_get_bit(gb) ? DAV1D_WM_TYPE_IDENTITY :
                                dav1d_get_bit(gb) ? DAV1D_WM_TYPE_ROT_ZOOM :
                                dav1d_get_bit(gb) ? DAV1D_WM_TYPE_TRANSLATION :
                                                    DAV1D_WM_TYPE_AFFINE;

            if (hdr->gmv[i].type == DAV1D_WM_TYPE_IDENTITY) continue;

            const Dav1dWarpedMotionParams *ref_gmv;
            if (hdr->primary_ref_frame == DAV1D_PRIMARY_REF_NONE) {
                ref_gmv = &dav1d_default_wm_params;
            } else {
                const int pri_ref = hdr->refidx[hdr->primary_ref_frame];
                if (!c->refs[pri_ref].p.p.frame_hdr) goto error;
                ref_gmv = &c->refs[pri_ref].p.p.frame_hdr->gmv[i];
            }
            int32_t *const mat = hdr->gmv[i].matrix;
            const int32_t *const ref_mat = ref_gmv->matrix;
            int bits, shift;

            if (hdr->gmv[i].type >= DAV1D_WM_TYPE_ROT_ZOOM) {
                mat[2] = (1 << 16) + 2 *
                    dav1d_get_bits_subexp(gb, (ref_mat[2] - (1 << 16)) >> 1, 12);
                mat[3] = 2 * dav1d_get_bits_subexp(gb, ref_mat[3] >> 1, 12);

                bits = 12;
                shift = 10;
            } else {
                bits = 9 - !hdr->hp;
                shift = 13 + !hdr->hp;
            }

            if (hdr->gmv[i].type == DAV1D_WM_TYPE_AFFINE) {
                mat[4] = 2 * dav1d_get_bits_subexp(gb, ref_mat[4] >> 1, 12);
                mat[5] = (1 << 16) + 2 *
                    dav1d_get_bits_subexp(gb, (ref_mat[5] - (1 << 16)) >> 1, 12);
            } else {
                mat[4] = -mat[3];
                mat[5] = mat[2];
            }

            mat[0] = dav1d_get_bits_subexp(gb, ref_mat[0] >> shift, bits) * (1 << shift);
            mat[1] = dav1d_get_bits_subexp(gb, ref_mat[1] >> shift, bits) * (1 << shift);
        }
    }
#if DEBUG_FRAME_HDR
    printf("HDR: post-gmv: off=%td\n",
           (gb->ptr - init_ptr) * 8 - gb->bits_left);
#endif

    if (seqhdr->film_grain_present && (hdr->show_frame || hdr->showable_frame)) {
        hdr->film_grain.present = dav1d_get_bit(gb);
        if (hdr->film_grain.present) {
            const unsigned seed = dav1d_get_bits(gb, 16);
            hdr->film_grain.update = hdr->frame_type != DAV1D_FRAME_TYPE_INTER || dav1d_get_bit(gb);
            if (!hdr->film_grain.update) {
                const int refidx = dav1d_get_bits(gb, 3);
                int i;
                for (i = 0; i < 7; i++)
                    if (hdr->refidx[i] == refidx)
                        break;
                if (i == 7 || !c->refs[refidx].p.p.frame_hdr) goto error;
                hdr->film_grain.data = c->refs[refidx].p.p.frame_hdr->film_grain.data;
                hdr->film_grain.data.seed = seed;
            } else {
                Dav1dFilmGrainData *const fgd = &hdr->film_grain.data;
                fgd->seed = seed;

                fgd->num_y_points = dav1d_get_bits(gb, 4);
                if (fgd->num_y_points > 14) goto error;
                for (int i = 0; i < fgd->num_y_points; i++) {
                    fgd->y_points[i][0] = dav1d_get_bits(gb, 8);
                    if (i && fgd->y_points[i - 1][0] >= fgd->y_points[i][0])
                        goto error;
                    fgd->y_points[i][1] = dav1d_get_bits(gb, 8);
                }

                if (!seqhdr->monochrome)
                    fgd->chroma_scaling_from_luma = dav1d_get_bit(gb);
                if (seqhdr->monochrome || fgd->chroma_scaling_from_luma ||
                    (seqhdr->ss_ver == 1 && seqhdr->ss_hor == 1 && !fgd->num_y_points))
                {
                    fgd->num_uv_points[0] = fgd->num_uv_points[1] = 0;
                } else for (int pl = 0; pl < 2; pl++) {
                    fgd->num_uv_points[pl] = dav1d_get_bits(gb, 4);
                    if (fgd->num_uv_points[pl] > 10) goto error;
                    for (int i = 0; i < fgd->num_uv_points[pl]; i++) {
                        fgd->uv_points[pl][i][0] = dav1d_get_bits(gb, 8);
                        if (i && fgd->uv_points[pl][i - 1][0] >= fgd->uv_points[pl][i][0])
                            goto error;
                        fgd->uv_points[pl][i][1] = dav1d_get_bits(gb, 8);
                    }
                }

                if (seqhdr->ss_hor == 1 && seqhdr->ss_ver == 1 &&
                    !!fgd->num_uv_points[0] != !!fgd->num_uv_points[1])
                {
                    goto error;
                }

                fgd->scaling_shift = dav1d_get_bits(gb, 2) + 8;
                fgd->ar_coeff_lag = dav1d_get_bits(gb, 2);
                const int num_y_pos = 2 * fgd->ar_coeff_lag * (fgd->ar_coeff_lag + 1);
                if (fgd->num_y_points)
                    for (int i = 0; i < num_y_pos; i++)
                        fgd->ar_coeffs_y[i] = dav1d_get_bits(gb, 8) - 128;
                for (int pl = 0; pl < 2; pl++)
                    if (fgd->num_uv_points[pl] || fgd->chroma_scaling_from_luma) {
                        const int num_uv_pos = num_y_pos + !!fgd->num_y_points;
                        for (int i = 0; i < num_uv_pos; i++)
                            fgd->ar_coeffs_uv[pl][i] = dav1d_get_bits(gb, 8) - 128;
                        if (!fgd->num_y_points)
                            fgd->ar_coeffs_uv[pl][num_uv_pos] = 0;
                    }
                fgd->ar_coeff_shift = dav1d_get_bits(gb, 2) + 6;
                fgd->grain_scale_shift = dav1d_get_bits(gb, 2);
                for (int pl = 0; pl < 2; pl++)
                    if (fgd->num_uv_points[pl]) {
                        fgd->uv_mult[pl] = dav1d_get_bits(gb, 8) - 128;
                        fgd->uv_luma_mult[pl] = dav1d_get_bits(gb, 8) - 128;
                        fgd->uv_offset[pl] = dav1d_get_bits(gb, 9) - 256;
                    }
                fgd->overlap_flag = dav1d_get_bit(gb);
                fgd->clip_to_restricted_range = dav1d_get_bit(gb);
            }
        }
    }
#if DEBUG_FRAME_HDR
    printf("HDR: post-filmgrain: off=%td\n",
           (gb->ptr - init_ptr) * 8 - gb->bits_left);
#endif

    return 0;

error:
    dav1d_log(c, "Error parsing frame header\n");
    return DAV1D_ERR(EINVAL);
}

static void parse_tile_hdr(Dav1dContext *const c, GetBits *const gb) {
    const int n_tiles = c->frame_hdr->tiling.cols * c->frame_hdr->tiling.rows;
    const int have_tile_pos = n_tiles > 1 ? dav1d_get_bit(gb) : 0;

    if (have_tile_pos) {
        const int n_bits = c->frame_hdr->tiling.log2_cols +
                           c->frame_hdr->tiling.log2_rows;
        c->tile[c->n_tile_data].start = dav1d_get_bits(gb, n_bits);
        c->tile[c->n_tile_data].end = dav1d_get_bits(gb, n_bits);
    } else {
        c->tile[c->n_tile_data].start = 0;
        c->tile[c->n_tile_data].end = n_tiles - 1;
    }
}

ptrdiff_t dav1d_parse_obus(Dav1dContext *const c, Dav1dData *const in) {
    GetBits gb;
    int res;

    dav1d_init_get_bits(&gb, in->data, in->sz);

    // obu header
    const int obu_forbidden_bit = dav1d_get_bit(&gb);
    if (c->strict_std_compliance && obu_forbidden_bit) goto error;
    const enum Dav1dObuType type = dav1d_get_bits(&gb, 4);
    const int has_extension = dav1d_get_bit(&gb);
    const int has_length_field = dav1d_get_bit(&gb);
    dav1d_get_bit(&gb); // reserved

    int temporal_id = 0, spatial_id = 0;
    if (has_extension) {
        temporal_id = dav1d_get_bits(&gb, 3);
        spatial_id = dav1d_get_bits(&gb, 2);
        dav1d_get_bits(&gb, 3); // reserved
    }

    if (has_length_field) {
        const size_t len = dav1d_get_uleb128(&gb);
        if (len > (size_t)(gb.ptr_end - gb.ptr)) goto error;
        gb.ptr_end = gb.ptr + len;
    }
    if (gb.error) goto error;

    // We must have read a whole number of bytes at this point (1 byte
    // for the header and whole bytes at a time when reading the
    // leb128 length field).
    assert(gb.bits_left == 0);

    // skip obu not belonging to the selected temporal/spatial layer
    if (type != DAV1D_OBU_SEQ_HDR && type != DAV1D_OBU_TD &&
        has_extension && c->operating_point_idc != 0)
    {
        const int in_temporal_layer = (c->operating_point_idc >> temporal_id) & 1;
        const int in_spatial_layer = (c->operating_point_idc >> (spatial_id + 8)) & 1;
        if (!in_temporal_layer || !in_spatial_layer)
            return gb.ptr_end - gb.ptr_start;
    }

    switch (type) {
    case DAV1D_OBU_SEQ_HDR: {
        Dav1dRef *ref = dav1d_ref_create_using_pool(c->seq_hdr_pool,
                                                    sizeof(Dav1dSequenceHeader));
        if (!ref) return DAV1D_ERR(ENOMEM);
        Dav1dSequenceHeader *seq_hdr = ref->data;
        if ((res = parse_seq_hdr(seq_hdr, &gb, c->strict_std_compliance)) < 0) {
            dav1d_log(c, "Error parsing sequence header\n");
            dav1d_ref_dec(&ref);
            goto error;
        }

        const int op_idx =
            c->operating_point < seq_hdr->num_operating_points ? c->operating_point : 0;
        c->operating_point_idc = seq_hdr->operating_points[op_idx].idc;
        const unsigned spatial_mask = c->operating_point_idc >> 8;
        c->max_spatial_id = spatial_mask ? ulog2(spatial_mask) : 0;

        // If we have read a sequence header which is different from
        // the old one, this is a new video sequence and can't use any
        // previous state. Free that state.

        if (!c->seq_hdr) {
            c->frame_hdr = NULL;
            c->frame_flags |= PICTURE_FLAG_NEW_SEQUENCE;
        // see 7.5, operating_parameter_info is allowed to change in
        // sequence headers of a single sequence
        } else if (memcmp(seq_hdr, c->seq_hdr, offsetof(Dav1dSequenceHeader, operating_parameter_info))) {
            c->frame_hdr = NULL;
            c->mastering_display = NULL;
            c->content_light = NULL;
            dav1d_ref_dec(&c->mastering_display_ref);
            dav1d_ref_dec(&c->content_light_ref);
            for (int i = 0; i < 8; i++) {
                if (c->refs[i].p.p.frame_hdr)
                    dav1d_thread_picture_unref(&c->refs[i].p);
                dav1d_ref_dec(&c->refs[i].segmap);
                dav1d_ref_dec(&c->refs[i].refmvs);
                dav1d_cdf_thread_unref(&c->cdf[i]);
            }
            c->frame_flags |= PICTURE_FLAG_NEW_SEQUENCE;
        // If operating_parameter_info changed, signal it
        } else if (memcmp(seq_hdr->operating_parameter_info, c->seq_hdr->operating_parameter_info,
                          sizeof(seq_hdr->operating_parameter_info)))
        {
            c->frame_flags |= PICTURE_FLAG_NEW_OP_PARAMS_INFO;
        }
        dav1d_ref_dec(&c->seq_hdr_ref);
        c->seq_hdr_ref = ref;
        c->seq_hdr = seq_hdr;
        break;
    }
    case DAV1D_OBU_REDUNDANT_FRAME_HDR:
        if (c->frame_hdr) break;
        // fall-through
    case DAV1D_OBU_FRAME:
    case DAV1D_OBU_FRAME_HDR:
        if (!c->seq_hdr) goto error;
        if (!c->frame_hdr_ref) {
            c->frame_hdr_ref = dav1d_ref_create_using_pool(c->frame_hdr_pool,
                                                           sizeof(Dav1dFrameHeader));
            if (!c->frame_hdr_ref) return DAV1D_ERR(ENOMEM);
        }
#ifndef NDEBUG
        // ensure that the reference is writable
        assert(dav1d_ref_is_writable(c->frame_hdr_ref));
#endif
        c->frame_hdr = c->frame_hdr_ref->data;
        memset(c->frame_hdr, 0, sizeof(*c->frame_hdr));
        c->frame_hdr->temporal_id = temporal_id;
        c->frame_hdr->spatial_id = spatial_id;
        if ((res = parse_frame_hdr(c, &gb)) < 0) {
            c->frame_hdr = NULL;
            goto error;
        }
        for (int n = 0; n < c->n_tile_data; n++)
            dav1d_data_unref_internal(&c->tile[n].data);
        c->n_tile_data = 0;
        c->n_tiles = 0;
        if (type != DAV1D_OBU_FRAME) {
            // This is actually a frame header OBU so read the
            // trailing bit and check for overrun.
            if (check_trailing_bits(&gb, c->strict_std_compliance) < 0) {
                c->frame_hdr = NULL;
                goto error;
            }
        }

        if (c->frame_size_limit && (int64_t)c->frame_hdr->width[1] *
            c->frame_hdr->height > c->frame_size_limit)
        {
            dav1d_log(c, "Frame size %dx%d exceeds limit %u\n", c->frame_hdr->width[1],
                      c->frame_hdr->height, c->frame_size_limit);
            c->frame_hdr = NULL;
            return DAV1D_ERR(ERANGE);
        }

        if (type != DAV1D_OBU_FRAME)
            break;
        // OBU_FRAMEs shouldn't be signaled with show_existing_frame
        if (c->frame_hdr->show_existing_frame) {
            c->frame_hdr = NULL;
            goto error;
        }

        // This is the frame header at the start of a frame OBU.
        // There's no trailing bit at the end to skip, but we do need
        // to align to the next byte.
        dav1d_bytealign_get_bits(&gb);
        // fall-through
    case DAV1D_OBU_TILE_GRP: {
        if (!c->frame_hdr) goto error;
        if (c->n_tile_data_alloc < c->n_tile_data + 1) {
            if ((c->n_tile_data + 1) > INT_MAX / (int)sizeof(*c->tile)) goto error;
            struct Dav1dTileGroup *tile = dav1d_realloc(ALLOC_TILE, c->tile,
                                                        (c->n_tile_data + 1) * sizeof(*c->tile));
            if (!tile) goto error;
            c->tile = tile;
            memset(c->tile + c->n_tile_data, 0, sizeof(*c->tile));
            c->n_tile_data_alloc = c->n_tile_data + 1;
        }
        parse_tile_hdr(c, &gb);
        // Align to the next byte boundary and check for overrun.
        dav1d_bytealign_get_bits(&gb);
        if (gb.error) goto error;

        dav1d_data_ref(&c->tile[c->n_tile_data].data, in);
        c->tile[c->n_tile_data].data.data = gb.ptr;
        c->tile[c->n_tile_data].data.sz = (size_t)(gb.ptr_end - gb.ptr);
        // ensure tile groups are in order and sane, see 6.10.1
        if (c->tile[c->n_tile_data].start > c->tile[c->n_tile_data].end ||
            c->tile[c->n_tile_data].start != c->n_tiles)
        {
            for (int i = 0; i <= c->n_tile_data; i++)
                dav1d_data_unref_internal(&c->tile[i].data);
            c->n_tile_data = 0;
            c->n_tiles = 0;
            goto error;
        }
        c->n_tiles += 1 + c->tile[c->n_tile_data].end -
                          c->tile[c->n_tile_data].start;
        c->n_tile_data++;
        break;
    }
    case DAV1D_OBU_METADATA: {
#define DEBUG_OBU_METADATA 0
#if DEBUG_OBU_METADATA
        const uint8_t *const init_ptr = gb.ptr;
#endif
        // obu metadta type field
        const enum ObuMetaType meta_type = dav1d_get_uleb128(&gb);
        if (gb.error) goto error;

        switch (meta_type) {
        case OBU_META_HDR_CLL: {
            Dav1dRef *ref = dav1d_ref_create(ALLOC_OBU_META,
                                             sizeof(Dav1dContentLightLevel));
            if (!ref) return DAV1D_ERR(ENOMEM);
            Dav1dContentLightLevel *const content_light = ref->data;

            content_light->max_content_light_level = dav1d_get_bits(&gb, 16);
#if DEBUG_OBU_METADATA
            printf("CLLOBU: max-content-light-level: %d [off=%td]\n",
                   content_light->max_content_light_level,
                   (gb.ptr - init_ptr) * 8 - gb.bits_left);
#endif
            content_light->max_frame_average_light_level = dav1d_get_bits(&gb, 16);
#if DEBUG_OBU_METADATA
            printf("CLLOBU: max-frame-average-light-level: %d [off=%td]\n",
                   content_light->max_frame_average_light_level,
                   (gb.ptr - init_ptr) * 8 - gb.bits_left);
#endif

            if (check_trailing_bits(&gb, c->strict_std_compliance) < 0) {
                dav1d_ref_dec(&ref);
                goto error;
            }

            dav1d_ref_dec(&c->content_light_ref);
            c->content_light = content_light;
            c->content_light_ref = ref;
            break;
        }
        case OBU_META_HDR_MDCV: {
            Dav1dRef *ref = dav1d_ref_create(ALLOC_OBU_META,
                                             sizeof(Dav1dMasteringDisplay));
            if (!ref) return DAV1D_ERR(ENOMEM);
            Dav1dMasteringDisplay *const mastering_display = ref->data;

            for (int i = 0; i < 3; i++) {
                mastering_display->primaries[i][0] = dav1d_get_bits(&gb, 16);
                mastering_display->primaries[i][1] = dav1d_get_bits(&gb, 16);
#if DEBUG_OBU_METADATA
                printf("MDCVOBU: primaries[%d]: (%d, %d) [off=%td]\n", i,
                       mastering_display->primaries[i][0],
                       mastering_display->primaries[i][1],
                       (gb.ptr - init_ptr) * 8 - gb.bits_left);
#endif
            }
            mastering_display->white_point[0] = dav1d_get_bits(&gb, 16);
#if DEBUG_OBU_METADATA
            printf("MDCVOBU: white-point-x: %d [off=%td]\n",
                   mastering_display->white_point[0],
                   (gb.ptr - init_ptr) * 8 - gb.bits_left);
#endif
            mastering_display->white_point[1] = dav1d_get_bits(&gb, 16);
#if DEBUG_OBU_METADATA
            printf("MDCVOBU: white-point-y: %d [off=%td]\n",
                   mastering_display->white_point[1],
                   (gb.ptr - init_ptr) * 8 - gb.bits_left);
#endif
            mastering_display->max_luminance = dav1d_get_bits(&gb, 32);
#if DEBUG_OBU_METADATA
            printf("MDCVOBU: max-luminance: %d [off=%td]\n",
                   mastering_display->max_luminance,
                   (gb.ptr - init_ptr) * 8 - gb.bits_left);
#endif
            mastering_display->min_luminance = dav1d_get_bits(&gb, 32);
#if DEBUG_OBU_METADATA
            printf("MDCVOBU: min-luminance: %d [off=%td]\n",
                   mastering_display->min_luminance,
                   (gb.ptr - init_ptr) * 8 - gb.bits_left);
#endif
            if (check_trailing_bits(&gb, c->strict_std_compliance) < 0) {
                dav1d_ref_dec(&ref);
                goto error;
            }

            dav1d_ref_dec(&c->mastering_display_ref);
            c->mastering_display = mastering_display;
            c->mastering_display_ref = ref;
            break;
        }
        case OBU_META_ITUT_T35: {
            ptrdiff_t payload_size = gb.ptr_end - gb.ptr;
            // Don't take into account all the trailing bits for payload_size
            while (payload_size > 0 && !gb.ptr[payload_size - 1])
                payload_size--; // trailing_zero_bit x 8
            payload_size--; // trailing_one_bit + trailing_zero_bit x 7

            int country_code_extension_byte = 0;
            const int country_code = dav1d_get_bits(&gb, 8);
            payload_size--;
            if (country_code == 0xFF) {
                country_code_extension_byte = dav1d_get_bits(&gb, 8);
                payload_size--;
            }

            if (payload_size <= 0 || gb.ptr[payload_size] != 0x80) {
                dav1d_log(c, "Malformed ITU-T T.35 metadata message format\n");
                break;
            }

            if ((c->n_itut_t35 + 1) > INT_MAX / (int)sizeof(*c->itut_t35)) goto error;
            struct Dav1dITUTT35 *itut_t35 = dav1d_realloc(ALLOC_OBU_META, c->itut_t35,
                                                          (c->n_itut_t35 + 1) * sizeof(*c->itut_t35));
            if (!itut_t35) goto error;
            c->itut_t35 = itut_t35;
            memset(c->itut_t35 + c->n_itut_t35, 0, sizeof(*c->itut_t35));

            struct itut_t35_ctx_context *itut_t35_ctx;
            if (!c->n_itut_t35) {
                assert(!c->itut_t35_ref);
                itut_t35_ctx = dav1d_malloc(ALLOC_OBU_META, sizeof(struct itut_t35_ctx_context));
                if (!itut_t35_ctx) goto error;
                c->itut_t35_ref = dav1d_ref_init(&itut_t35_ctx->ref, c->itut_t35,
                                                 dav1d_picture_free_itut_t35, itut_t35_ctx, 0);
            } else {
                assert(c->itut_t35_ref && atomic_load(&c->itut_t35_ref->ref_cnt) == 1);
                itut_t35_ctx = c->itut_t35_ref->user_data;
                c->itut_t35_ref->const_data = (uint8_t *)c->itut_t35;
            }
            itut_t35_ctx->itut_t35 = c->itut_t35;
            itut_t35_ctx->n_itut_t35 = c->n_itut_t35 + 1;

            Dav1dITUTT35 *const itut_t35_metadata = &c->itut_t35[c->n_itut_t35];
            itut_t35_metadata->payload = dav1d_malloc(ALLOC_OBU_META, payload_size);
            if (!itut_t35_metadata->payload) goto error;

            itut_t35_metadata->country_code = country_code;
            itut_t35_metadata->country_code_extension_byte = country_code_extension_byte;
            itut_t35_metadata->payload_size = payload_size;

            // We know that we've read a whole number of bytes and that the
            // payload is within the OBU boundaries, so just use memcpy()
            assert(gb.bits_left == 0);
            memcpy(itut_t35_metadata->payload, gb.ptr, payload_size);

            c->n_itut_t35++;
            break;
        }
        case OBU_META_SCALABILITY:
        case OBU_META_TIMECODE:
            // ignore metadata OBUs we don't care about
            break;
        default:
            // print a warning but don't fail for unknown types
            if (meta_type > 31) // Types 6 to 31 are "Unregistered user private", so ignore them.
                dav1d_log(c, "Unknown Metadata OBU type %d\n", meta_type);
            break;
        }

        break;
    }
    case DAV1D_OBU_TD:
        c->frame_flags |= PICTURE_FLAG_NEW_TEMPORAL_UNIT;
        break;
    case DAV1D_OBU_PADDING:
        // ignore OBUs we don't care about
        break;
    default:
        // print a warning but don't fail for unknown types
        dav1d_log(c, "Unknown OBU type %d of size %td\n", type, gb.ptr_end - gb.ptr);
        break;
    }

    if (c->seq_hdr && c->frame_hdr) {
        if (c->frame_hdr->show_existing_frame) {
            if (!c->refs[c->frame_hdr->existing_frame_idx].p.p.frame_hdr) goto error;
            switch (c->refs[c->frame_hdr->existing_frame_idx].p.p.frame_hdr->frame_type) {
            case DAV1D_FRAME_TYPE_INTER:
            case DAV1D_FRAME_TYPE_SWITCH:
                if (c->decode_frame_type > DAV1D_DECODEFRAMETYPE_REFERENCE)
                    goto skip;
                break;
            case DAV1D_FRAME_TYPE_INTRA:
                if (c->decode_frame_type > DAV1D_DECODEFRAMETYPE_INTRA)
                    goto skip;
                // fall-through
            default:
                break;
            }
            if (!c->refs[c->frame_hdr->existing_frame_idx].p.p.data[0]) goto error;
            if (c->strict_std_compliance &&
                !c->refs[c->frame_hdr->existing_frame_idx].p.showable)
            {
                goto error;
            }
            if (c->n_fc == 1) {
                dav1d_thread_picture_ref(&c->out,
                                         &c->refs[c->frame_hdr->existing_frame_idx].p);
                dav1d_picture_copy_props(&c->out.p,
                                         c->content_light, c->content_light_ref,
                                         c->mastering_display, c->mastering_display_ref,
                                         c->itut_t35, c->itut_t35_ref, c->n_itut_t35,
                                         &in->m);
                // Must be removed from the context after being attached to the frame
                dav1d_ref_dec(&c->itut_t35_ref);
                c->itut_t35 = NULL;
                c->n_itut_t35 = 0;
                c->event_flags |= dav1d_picture_get_event_flags(&c->refs[c->frame_hdr->existing_frame_idx].p);
            } else {
                pthread_mutex_lock(&c->task_thread.lock);
                // need to append this to the frame output queue
                const unsigned next = c->frame_thread.next++;
                if (c->frame_thread.next == c->n_fc)
                    c->frame_thread.next = 0;

                Dav1dFrameContext *const f = &c->fc[next];
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
                }
                const int error = f->task_thread.retval;
                if (error) {
                    c->cached_error = error;
                    f->task_thread.retval = 0;
                    dav1d_data_props_copy(&c->cached_error_props, &out_delayed->p.m);
                    dav1d_thread_picture_unref(out_delayed);
                } else if (out_delayed->p.data[0]) {
                    const unsigned progress = atomic_load_explicit(&out_delayed->progress[1],
                                                                   memory_order_relaxed);
                    if ((out_delayed->visible || c->output_invisible_frames) &&
                        progress != FRAME_ERROR)
                    {
                        dav1d_thread_picture_ref(&c->out, out_delayed);
                        c->event_flags |= dav1d_picture_get_event_flags(out_delayed);
                    }
                    dav1d_thread_picture_unref(out_delayed);
                }
                dav1d_thread_picture_ref(out_delayed,
                                         &c->refs[c->frame_hdr->existing_frame_idx].p);
                out_delayed->visible = 1;
                dav1d_picture_copy_props(&out_delayed->p,
                                         c->content_light, c->content_light_ref,
                                         c->mastering_display, c->mastering_display_ref,
                                         c->itut_t35, c->itut_t35_ref, c->n_itut_t35,
                                         &in->m);
                // Must be removed from the context after being attached to the frame
                dav1d_ref_dec(&c->itut_t35_ref);
                c->itut_t35 = NULL;
                c->n_itut_t35 = 0;

                pthread_mutex_unlock(&c->task_thread.lock);
            }
            if (c->refs[c->frame_hdr->existing_frame_idx].p.p.frame_hdr->frame_type == DAV1D_FRAME_TYPE_KEY) {
                const int r = c->frame_hdr->existing_frame_idx;
                c->refs[r].p.showable = 0;
                for (int i = 0; i < 8; i++) {
                    if (i == r) continue;

                    if (c->refs[i].p.p.frame_hdr)
                        dav1d_thread_picture_unref(&c->refs[i].p);
                    dav1d_thread_picture_ref(&c->refs[i].p, &c->refs[r].p);

                    dav1d_cdf_thread_unref(&c->cdf[i]);
                    dav1d_cdf_thread_ref(&c->cdf[i], &c->cdf[r]);

                    dav1d_ref_dec(&c->refs[i].segmap);
                    c->refs[i].segmap = c->refs[r].segmap;
                    if (c->refs[r].segmap)
                        dav1d_ref_inc(c->refs[r].segmap);
                    dav1d_ref_dec(&c->refs[i].refmvs);
                }
            }
            c->frame_hdr = NULL;
        } else if (c->n_tiles == c->frame_hdr->tiling.cols * c->frame_hdr->tiling.rows) {
            switch (c->frame_hdr->frame_type) {
            case DAV1D_FRAME_TYPE_INTER:
            case DAV1D_FRAME_TYPE_SWITCH:
                if (c->decode_frame_type > DAV1D_DECODEFRAMETYPE_REFERENCE ||
                    (c->decode_frame_type == DAV1D_DECODEFRAMETYPE_REFERENCE &&
                     !c->frame_hdr->refresh_frame_flags))
                    goto skip;
                break;
            case DAV1D_FRAME_TYPE_INTRA:
                if (c->decode_frame_type > DAV1D_DECODEFRAMETYPE_INTRA ||
                    (c->decode_frame_type == DAV1D_DECODEFRAMETYPE_REFERENCE &&
                     !c->frame_hdr->refresh_frame_flags))
                    goto skip;
                // fall-through
            default:
                break;
            }
            if (!c->n_tile_data)
                goto error;
            if ((res = dav1d_submit_frame(c)) < 0)
                return res;
            assert(!c->n_tile_data);
            c->frame_hdr = NULL;
            c->n_tiles = 0;
        }
    }

    return gb.ptr_end - gb.ptr_start;

skip:
    // update refs with only the headers in case we skip the frame
    for (int i = 0; i < 8; i++) {
        if (c->frame_hdr->refresh_frame_flags & (1 << i)) {
            dav1d_thread_picture_unref(&c->refs[i].p);
            c->refs[i].p.p.frame_hdr = c->frame_hdr;
            c->refs[i].p.p.seq_hdr = c->seq_hdr;
            c->refs[i].p.p.frame_hdr_ref = c->frame_hdr_ref;
            c->refs[i].p.p.seq_hdr_ref = c->seq_hdr_ref;
            dav1d_ref_inc(c->frame_hdr_ref);
            dav1d_ref_inc(c->seq_hdr_ref);
        }
    }

    dav1d_ref_dec(&c->frame_hdr_ref);
    c->frame_hdr = NULL;
    c->n_tiles = 0;

    return gb.ptr_end - gb.ptr_start;

error:
    dav1d_data_props_copy(&c->cached_error_props, &in->m);
    dav1d_log(c, gb.error ? "Overrun in OBU bit buffer\n" :
                            "Error parsing OBU data\n");
    return DAV1D_ERR(EINVAL);
}
