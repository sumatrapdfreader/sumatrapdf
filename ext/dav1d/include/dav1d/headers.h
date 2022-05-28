/*
 * Copyright © 2018-2020, VideoLAN and dav1d authors
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

#ifndef DAV1D_HEADERS_H
#define DAV1D_HEADERS_H

#include <stdint.h>
#include <stddef.h>

// Constants from Section 3. "Symbols and abbreviated terms"
#define DAV1D_MAX_CDEF_STRENGTHS 8
#define DAV1D_MAX_OPERATING_POINTS 32
#define DAV1D_MAX_TILE_COLS 64
#define DAV1D_MAX_TILE_ROWS 64
#define DAV1D_MAX_SEGMENTS 8
#define DAV1D_NUM_REF_FRAMES 8
#define DAV1D_PRIMARY_REF_NONE 7
#define DAV1D_REFS_PER_FRAME 7
#define DAV1D_TOTAL_REFS_PER_FRAME (DAV1D_REFS_PER_FRAME + 1)

enum Dav1dObuType {
    DAV1D_OBU_SEQ_HDR   = 1,
    DAV1D_OBU_TD        = 2,
    DAV1D_OBU_FRAME_HDR = 3,
    DAV1D_OBU_TILE_GRP  = 4,
    DAV1D_OBU_METADATA  = 5,
    DAV1D_OBU_FRAME     = 6,
    DAV1D_OBU_REDUNDANT_FRAME_HDR = 7,
    DAV1D_OBU_PADDING   = 15,
};

enum Dav1dTxfmMode {
    DAV1D_TX_4X4_ONLY,
    DAV1D_TX_LARGEST,
    DAV1D_TX_SWITCHABLE,
    DAV1D_N_TX_MODES,
};

enum Dav1dFilterMode {
    DAV1D_FILTER_8TAP_REGULAR,
    DAV1D_FILTER_8TAP_SMOOTH,
    DAV1D_FILTER_8TAP_SHARP,
    DAV1D_N_SWITCHABLE_FILTERS,
    DAV1D_FILTER_BILINEAR = DAV1D_N_SWITCHABLE_FILTERS,
    DAV1D_N_FILTERS,
    DAV1D_FILTER_SWITCHABLE = DAV1D_N_FILTERS,
};

enum Dav1dAdaptiveBoolean {
    DAV1D_OFF = 0,
    DAV1D_ON = 1,
    DAV1D_ADAPTIVE = 2,
};

enum Dav1dRestorationType {
    DAV1D_RESTORATION_NONE,
    DAV1D_RESTORATION_SWITCHABLE,
    DAV1D_RESTORATION_WIENER,
    DAV1D_RESTORATION_SGRPROJ,
};

enum Dav1dWarpedMotionType {
    DAV1D_WM_TYPE_IDENTITY,
    DAV1D_WM_TYPE_TRANSLATION,
    DAV1D_WM_TYPE_ROT_ZOOM,
    DAV1D_WM_TYPE_AFFINE,
};

typedef struct Dav1dWarpedMotionParams {
    enum Dav1dWarpedMotionType type;
    int32_t matrix[6];
    union {
        struct {
            int16_t alpha, beta, gamma, delta;
        } p;
        int16_t abcd[4];
    } u;
} Dav1dWarpedMotionParams;

enum Dav1dPixelLayout {
    DAV1D_PIXEL_LAYOUT_I400, ///< monochrome
    DAV1D_PIXEL_LAYOUT_I420, ///< 4:2:0 planar
    DAV1D_PIXEL_LAYOUT_I422, ///< 4:2:2 planar
    DAV1D_PIXEL_LAYOUT_I444, ///< 4:4:4 planar
};

enum Dav1dFrameType {
    DAV1D_FRAME_TYPE_KEY = 0,    ///< Key Intra frame
    DAV1D_FRAME_TYPE_INTER = 1,  ///< Inter frame
    DAV1D_FRAME_TYPE_INTRA = 2,  ///< Non key Intra frame
    DAV1D_FRAME_TYPE_SWITCH = 3, ///< Switch Inter frame
};

enum Dav1dColorPrimaries {
    DAV1D_COLOR_PRI_BT709 = 1,
    DAV1D_COLOR_PRI_UNKNOWN = 2,
    DAV1D_COLOR_PRI_BT470M = 4,
    DAV1D_COLOR_PRI_BT470BG = 5,
    DAV1D_COLOR_PRI_BT601 = 6,
    DAV1D_COLOR_PRI_SMPTE240 = 7,
    DAV1D_COLOR_PRI_FILM = 8,
    DAV1D_COLOR_PRI_BT2020 = 9,
    DAV1D_COLOR_PRI_XYZ = 10,
    DAV1D_COLOR_PRI_SMPTE431 = 11,
    DAV1D_COLOR_PRI_SMPTE432 = 12,
    DAV1D_COLOR_PRI_EBU3213 = 22,
    DAV1D_COLOR_PRI_RESERVED = 255,
};

enum Dav1dTransferCharacteristics {
    DAV1D_TRC_BT709 = 1,
    DAV1D_TRC_UNKNOWN = 2,
    DAV1D_TRC_BT470M = 4,
    DAV1D_TRC_BT470BG = 5,
    DAV1D_TRC_BT601 = 6,
    DAV1D_TRC_SMPTE240 = 7,
    DAV1D_TRC_LINEAR = 8,
    DAV1D_TRC_LOG100 = 9,         ///< logarithmic (100:1 range)
    DAV1D_TRC_LOG100_SQRT10 = 10, ///< lograithmic (100*sqrt(10):1 range)
    DAV1D_TRC_IEC61966 = 11,
    DAV1D_TRC_BT1361 = 12,
    DAV1D_TRC_SRGB = 13,
    DAV1D_TRC_BT2020_10BIT = 14,
    DAV1D_TRC_BT2020_12BIT = 15,
    DAV1D_TRC_SMPTE2084 = 16,     ///< PQ
    DAV1D_TRC_SMPTE428 = 17,
    DAV1D_TRC_HLG = 18,           ///< hybrid log/gamma (BT.2100 / ARIB STD-B67)
    DAV1D_TRC_RESERVED = 255,
};

enum Dav1dMatrixCoefficients {
    DAV1D_MC_IDENTITY = 0,
    DAV1D_MC_BT709 = 1,
    DAV1D_MC_UNKNOWN = 2,
    DAV1D_MC_FCC = 4,
    DAV1D_MC_BT470BG = 5,
    DAV1D_MC_BT601 = 6,
    DAV1D_MC_SMPTE240 = 7,
    DAV1D_MC_SMPTE_YCGCO = 8,
    DAV1D_MC_BT2020_NCL = 9,
    DAV1D_MC_BT2020_CL = 10,
    DAV1D_MC_SMPTE2085 = 11,
    DAV1D_MC_CHROMAT_NCL = 12, ///< Chromaticity-derived
    DAV1D_MC_CHROMAT_CL = 13,
    DAV1D_MC_ICTCP = 14,
    DAV1D_MC_RESERVED = 255,
};

enum Dav1dChromaSamplePosition {
    DAV1D_CHR_UNKNOWN = 0,
    DAV1D_CHR_VERTICAL = 1,  ///< Horizontally co-located with luma(0, 0)
                           ///< sample, between two vertical samples
    DAV1D_CHR_COLOCATED = 2, ///< Co-located with luma(0, 0) sample
};

typedef struct Dav1dContentLightLevel {
    int max_content_light_level;
    int max_frame_average_light_level;
} Dav1dContentLightLevel;

typedef struct Dav1dMasteringDisplay {
    ///< 0.16 fixed point
    uint16_t primaries[3][2];
    ///< 0.16 fixed point
    uint16_t white_point[2];
    ///< 24.8 fixed point
    uint32_t max_luminance;
    ///< 18.14 fixed point
    uint32_t min_luminance;
} Dav1dMasteringDisplay;

typedef struct Dav1dITUTT35 {
    uint8_t  country_code;
    uint8_t  country_code_extension_byte;
    size_t   payload_size;
    uint8_t *payload;
} Dav1dITUTT35;

typedef struct Dav1dSequenceHeader {
    /**
     * Stream profile, 0 for 8-10 bits/component 4:2:0 or monochrome;
     * 1 for 8-10 bits/component 4:4:4; 2 for 4:2:2 at any bits/component,
     * or 12 bits/component at any chroma subsampling.
     */
    int profile;
    /**
     * Maximum dimensions for this stream. In non-scalable streams, these
     * are often the actual dimensions of the stream, although that is not
     * a normative requirement.
     */
    int max_width, max_height;
    enum Dav1dPixelLayout layout; ///< format of the picture
    enum Dav1dColorPrimaries pri; ///< color primaries (av1)
    enum Dav1dTransferCharacteristics trc; ///< transfer characteristics (av1)
    enum Dav1dMatrixCoefficients mtrx; ///< matrix coefficients (av1)
    enum Dav1dChromaSamplePosition chr; ///< chroma sample position (av1)
    /**
     * 0, 1 and 2 mean 8, 10 or 12 bits/component, respectively. This is not
     * exactly the same as 'hbd' from the spec; the spec's hbd distinguishes
     * between 8 (0) and 10-12 (1) bits/component, and another element
     * (twelve_bit) to distinguish between 10 and 12 bits/component. To get
     * the spec's hbd, use !!our_hbd, and to get twelve_bit, use hbd == 2.
     */
    int hbd;
    /**
     * Pixel data uses JPEG pixel range ([0,255] for 8bits) instead of
     * MPEG pixel range ([16,235] for 8bits luma, [16,240] for 8bits chroma).
     */
    int color_range;

    int num_operating_points;
    struct Dav1dSequenceHeaderOperatingPoint {
        int major_level, minor_level;
        int initial_display_delay;
        int idc;
        int tier;
        int decoder_model_param_present;
        int display_model_param_present;
    } operating_points[DAV1D_MAX_OPERATING_POINTS];

    int still_picture;
    int reduced_still_picture_header;
    int timing_info_present;
    int num_units_in_tick;
    int time_scale;
    int equal_picture_interval;
    unsigned num_ticks_per_picture;
    int decoder_model_info_present;
    int encoder_decoder_buffer_delay_length;
    int num_units_in_decoding_tick;
    int buffer_removal_delay_length;
    int frame_presentation_delay_length;
    int display_model_info_present;
    int width_n_bits, height_n_bits;
    int frame_id_numbers_present;
    int delta_frame_id_n_bits;
    int frame_id_n_bits;
    int sb128;
    int filter_intra;
    int intra_edge_filter;
    int inter_intra;
    int masked_compound;
    int warped_motion;
    int dual_filter;
    int order_hint;
    int jnt_comp;
    int ref_frame_mvs;
    enum Dav1dAdaptiveBoolean screen_content_tools;
    enum Dav1dAdaptiveBoolean force_integer_mv;
    int order_hint_n_bits;
    int super_res;
    int cdef;
    int restoration;
    int ss_hor, ss_ver, monochrome;
    int color_description_present;
    int separate_uv_delta_q;
    int film_grain_present;

    // Dav1dSequenceHeaders of the same sequence are required to be
    // bit-identical until this offset. See 7.5 "Ordering of OBUs":
    //   Within a particular coded video sequence, the contents of
    //   sequence_header_obu must be bit-identical each time the
    //   sequence header appears except for the contents of
    //   operating_parameters_info.
    struct Dav1dSequenceHeaderOperatingParameterInfo {
        int decoder_buffer_delay;
        int encoder_buffer_delay;
        int low_delay_mode;
    } operating_parameter_info[DAV1D_MAX_OPERATING_POINTS];
} Dav1dSequenceHeader;

typedef struct Dav1dSegmentationData {
    int delta_q;
    int delta_lf_y_v, delta_lf_y_h, delta_lf_u, delta_lf_v;
    int ref;
    int skip;
    int globalmv;
} Dav1dSegmentationData;

typedef struct Dav1dSegmentationDataSet {
    Dav1dSegmentationData d[DAV1D_MAX_SEGMENTS];
    int preskip;
    int last_active_segid;
} Dav1dSegmentationDataSet;

typedef struct Dav1dLoopfilterModeRefDeltas {
    int mode_delta[2 /* is_zeromv */];
    int ref_delta[DAV1D_TOTAL_REFS_PER_FRAME];
} Dav1dLoopfilterModeRefDeltas;

typedef struct Dav1dFilmGrainData {
    unsigned seed;
    int num_y_points;
    uint8_t y_points[14][2 /* value, scaling */];
    int chroma_scaling_from_luma;
    int num_uv_points[2];
    uint8_t uv_points[2][10][2 /* value, scaling */];
    int scaling_shift;
    int ar_coeff_lag;
    int8_t ar_coeffs_y[24];
    int8_t ar_coeffs_uv[2][25 + 3 /* padding for alignment purposes */];
    uint64_t ar_coeff_shift;
    int grain_scale_shift;
    int uv_mult[2];
    int uv_luma_mult[2];
    int uv_offset[2];
    int overlap_flag;
    int clip_to_restricted_range;
} Dav1dFilmGrainData;

typedef struct Dav1dFrameHeader {
    struct {
        Dav1dFilmGrainData data;
        int present, update;
    } film_grain; ///< film grain parameters
    enum Dav1dFrameType frame_type; ///< type of the picture
    int width[2 /* { coded_width, superresolution_upscaled_width } */], height;
    int frame_offset; ///< frame number
    int temporal_id; ///< temporal id of the frame for SVC
    int spatial_id; ///< spatial id of the frame for SVC

    int show_existing_frame;
    int existing_frame_idx;
    int frame_id;
    int frame_presentation_delay;
    int show_frame;
    int showable_frame;
    int error_resilient_mode;
    int disable_cdf_update;
    int allow_screen_content_tools;
    int force_integer_mv;
    int frame_size_override;
    int primary_ref_frame;
    int buffer_removal_time_present;
    struct Dav1dFrameHeaderOperatingPoint {
        int buffer_removal_time;
    } operating_points[DAV1D_MAX_OPERATING_POINTS];
    int refresh_frame_flags;
    int render_width, render_height;
    struct {
        int width_scale_denominator;
        int enabled;
    } super_res;
    int have_render_size;
    int allow_intrabc;
    int frame_ref_short_signaling;
    int refidx[DAV1D_REFS_PER_FRAME];
    int hp;
    enum Dav1dFilterMode subpel_filter_mode;
    int switchable_motion_mode;
    int use_ref_frame_mvs;
    int refresh_context;
    struct {
        int uniform;
        unsigned n_bytes;
        int min_log2_cols, max_log2_cols, log2_cols, cols;
        int min_log2_rows, max_log2_rows, log2_rows, rows;
        uint16_t col_start_sb[DAV1D_MAX_TILE_COLS + 1];
        uint16_t row_start_sb[DAV1D_MAX_TILE_ROWS + 1];
        int update;
    } tiling;
    struct {
        int yac;
        int ydc_delta;
        int udc_delta, uac_delta, vdc_delta, vac_delta;
        int qm, qm_y, qm_u, qm_v;
    } quant;
    struct {
        int enabled, update_map, temporal, update_data;
        Dav1dSegmentationDataSet seg_data;
        int lossless[DAV1D_MAX_SEGMENTS], qidx[DAV1D_MAX_SEGMENTS];
    } segmentation;
    struct {
        struct {
            int present;
            int res_log2;
        } q;
        struct {
            int present;
            int res_log2;
            int multi;
        } lf;
    } delta;
    int all_lossless;
    struct {
        int level_y[2 /* dir */];
        int level_u, level_v;
        int mode_ref_delta_enabled;
        int mode_ref_delta_update;
        Dav1dLoopfilterModeRefDeltas mode_ref_deltas;
        int sharpness;
    } loopfilter;
    struct {
        int damping;
        int n_bits;
        int y_strength[DAV1D_MAX_CDEF_STRENGTHS];
        int uv_strength[DAV1D_MAX_CDEF_STRENGTHS];
    } cdef;
    struct {
        enum Dav1dRestorationType type[3 /* plane */];
        int unit_size[2 /* y, uv */];
    } restoration;
    enum Dav1dTxfmMode txfm_mode;
    int switchable_comp_refs;
    int skip_mode_allowed, skip_mode_enabled, skip_mode_refs[2];
    int warp_motion;
    int reduced_txtp_set;
    Dav1dWarpedMotionParams gmv[DAV1D_REFS_PER_FRAME];
} Dav1dFrameHeader;

#endif /* DAV1D_HEADERS_H */
