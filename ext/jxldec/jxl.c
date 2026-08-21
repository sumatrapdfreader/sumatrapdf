#ifndef JXLDEC_H
#define JXLDEC_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void *(*jxl_alloc_cb)(void *user, void *ctx, size_t size);
typedef void  (*jxl_free_cb)(void *user, void *ctx, void *ptr);

typedef enum {
    JXLDEC_SEVERITY_DEBUG,
    JXLDEC_SEVERITY_INFO,
    JXLDEC_SEVERITY_WARNING,
    JXLDEC_SEVERITY_ERROR,
    JXLDEC_SEVERITY_FATAL
} jxl_severity;

typedef void (*jxl_error_cb)(void *user, jxl_severity sev, const char *msg);

typedef struct jxl_ctx jxl_ctx;
typedef struct jxl_doc jxl_doc;

jxl_ctx *jxl_ctx_new(jxl_alloc_cb alloc, jxl_free_cb free_cb,
                     jxl_error_cb error, void *user);
void jxl_ctx_free(jxl_ctx *ctx);

void jxl_ctx_set_bgr(jxl_ctx *ctx, int enable);

void jxl_ctx_set_keep_orientation(jxl_ctx *ctx, int enable);

void jxl_ctx_set_srgb_output(jxl_ctx *ctx, int enable);

void jxl_request_abort(jxl_ctx *ctx);

typedef enum {
    JXLDEC_SIG_INVALID = 0,
    JXLDEC_SIG_NOT_ENOUGH_BYTES = 1,
    JXLDEC_SIG_CODESTREAM = 2,
    JXLDEC_SIG_CONTAINER = 3
} jxl_signature;

jxl_signature jxl_signature_check(const uint8_t *data, size_t len);

jxl_doc *jxl_doc_open(jxl_ctx *ctx, const uint8_t *data, size_t len);
void jxl_doc_close(jxl_doc *doc);

typedef enum {
    JXLDEC_CS_RGB   = 0,
    JXLDEC_CS_GRAY  = 1,
    JXLDEC_CS_XYB   = 2,
    JXLDEC_CS_UNKNOWN = 3
} jxl_color_space;

typedef struct {
    int width;
    int height;
    int bits_per_sample;
    int exponent_bits;
    int num_color_channels;
    int num_extra_channels;
    int alpha_bits;
    int alpha_premultiplied;
    int have_animation;
    int num_frames;
    int orientation;
    int have_preview;
    int uses_original_profile;
    jxl_color_space color_space;
    int intrinsic_width;
    int intrinsic_height;
} jxl_image_info;

int jxl_doc_info(jxl_doc *doc, jxl_image_info *info);

int jxl_doc_frame_count(jxl_doc *doc);

const uint8_t *jxl_doc_icc_profile(jxl_doc *doc, size_t *len);

typedef enum {
    JXLDEC_FORMAT_NATIVE  = 0,
    JXLDEC_FORMAT_GRAY8   = 1,
    JXLDEC_FORMAT_GRAYA8  = 2,
    JXLDEC_FORMAT_RGB24   = 3,
    JXLDEC_FORMAT_RGBA32  = 4,
    JXLDEC_FORMAT_GRAY16  = 5,
    JXLDEC_FORMAT_GRAYA16 = 6,
    JXLDEC_FORMAT_RGB48   = 7,
    JXLDEC_FORMAT_RGBA64  = 8
} jxl_format;

int jxl_format_bpp(jxl_format fmt);

typedef struct {
    int width;
    int height;
    jxl_format format;
    int stride;
    uint8_t *data;
} jxl_image;

jxl_image *jxl_frame_render(jxl_doc *doc, int frame_no, jxl_format fmt);
void jxl_image_destroy(jxl_ctx *ctx, jxl_image *img);

typedef struct {
    int width;
    int height;
    jxl_format format;
} jxl_render_info;

int jxl_frame_render_info(jxl_doc *doc, int frame_no, jxl_format fmt,
                          jxl_render_info *info);

int jxl_frame_render_into(jxl_doc *doc, int frame_no, jxl_format fmt,
                          uint8_t *dst, int stride);

typedef struct {
    int duration_ticks;
    int tps_numerator;
    int tps_denominator;
    int is_last;
} jxl_frame_info;

int jxl_doc_frame_info(jxl_doc *doc, int frame_no, jxl_frame_info *info);

jxl_image *jxl_decode(jxl_ctx *ctx, const uint8_t *data, size_t len,
                      jxl_format fmt);

int jxl_decode_size(jxl_ctx *ctx, const uint8_t *data, size_t len,
                    int *width, int *height);

#ifdef __cplusplus
}
#endif
#endif

#ifndef JXL_INTERNAL_H
#define JXL_INTERNAL_H

#include <stdarg.h>
#include <string.h>

struct jxl_ctx {
    jxl_alloc_cb alloc;
    jxl_free_cb free_cb;
    jxl_error_cb error;
    void *user;

    int bgr;
    int keep_orientation;
    int srgb_output;
    volatile int abort_epoch;
};

void *jxl_malloc(jxl_ctx *ctx, size_t size);
void *jxl_calloc(jxl_ctx *ctx, size_t count, size_t size);
void *jxl_realloc_array(jxl_ctx *ctx, void *ptr, size_t old_count,
                        size_t new_count, size_t size);
void jxl_free(jxl_ctx *ctx, void *ptr);

void jxl_errorf(jxl_ctx *ctx, jxl_severity sev, const char *fmt, ...);

#define JXL_ERR(ctx, ...)  jxl_errorf((ctx), JXLDEC_SEVERITY_ERROR, __VA_ARGS__)
#define JXL_WARN(ctx, ...) jxl_errorf((ctx), JXLDEC_SEVERITY_WARNING, __VA_ARGS__)

int jxl_size_mul(size_t a, size_t b, size_t *out);

int jxl_has_avx2(void);

int jxl_has_avx2_fma(void);

#if defined(__clang__) || defined(__GNUC__)

#define JXL_TARGET_AVX2 __attribute__((target("avx2")))
#define JXL_TARGET_AVX2_FMA __attribute__((target("avx2,fma")))
#else
#define JXL_TARGET_AVX2
#define JXL_TARGET_AVX2_FMA
#endif

#if defined(_MSC_VER) && !defined(__clang__)
#define JXL_ALIGN32 __declspec(align(32))
#else
#define JXL_ALIGN32 __attribute__((aligned(32)))
#endif

#define JXL_MIN(a, b) ((a) < (b) ? (a) : (b))
#define JXL_MAX(a, b) ((a) > (b) ? (a) : (b))

typedef struct {
    const uint8_t *data;
    size_t len;
    size_t byte_pos;
    uint64_t buf;
    int nbits;
    int err;
} jxl_br;

static inline size_t jxl_br_bits_read(const jxl_br *br) {
    return br->byte_pos * 8 - (size_t)br->nbits;
}

void jxl_br_init(jxl_br *br, const uint8_t *data, size_t len);
void jxl_br_refill(jxl_br *br);

void jxl_br_skip(jxl_br *br, size_t n);
int jxl_br_bool(jxl_br *br);
uint32_t jxl_br_u32(jxl_br *br, uint32_t c0, int n0, uint32_t c1, int n1,
                    uint32_t c2, int n2, uint32_t c3, int n3);
uint64_t jxl_br_u64(jxl_br *br);
float jxl_br_f16(jxl_br *br);
uint32_t jxl_br_enum(jxl_br *br);
void jxl_br_zero_pad_to_byte(jxl_br *br);

size_t jxl_br_byte_pos(const jxl_br *br);

void jxl_br_seek_byte(jxl_br *br, size_t byte_off);

#if defined(_MSC_VER) && !defined(__clang__)
#include <intrin.h>
#endif

#if defined(_MSC_VER) && !defined(__clang__)
#define JXL_INLINE_HINT __forceinline
#elif defined(__clang__) || defined(__GNUC__)
#define JXL_INLINE_HINT inline __attribute__((always_inline))
#else
#define JXL_INLINE_HINT
#endif

static JXL_INLINE_HINT uint32_t jxl_br_peek(jxl_br *br, int n) {
    if (br->nbits < n) jxl_br_refill(br);
    if (n == 0) return 0;
    return (uint32_t)(br->buf & ((n == 64) ? ~(uint64_t)0
                                           : (((uint64_t)1 << n) - 1)));
}

static JXL_INLINE_HINT void jxl_br_consume(jxl_br *br, int n) {
    if (br->nbits < n) {

        br->err = 1;
        br->buf = 0;
        br->nbits = 0;
        return;
    }
    br->nbits -= n;
    br->buf >>= n;
}

static JXL_INLINE_HINT uint32_t jxl_br_read(jxl_br *br, int n) {
    uint32_t v;
    if (n <= 0) return 0;
    v = jxl_br_peek(br, n);
    jxl_br_consume(br, n);
    return br->err ? 0 : v;
}

static inline uint32_t jxl_floor_log2_u64(uint64_t v) {
#if defined(_MSC_VER) && !defined(__clang__) && defined(_WIN64)
    unsigned long idx;
    _BitScanReverse64(&idx, v);
    return (uint32_t)idx;
#elif defined(_MSC_VER) && !defined(__clang__)

    unsigned long idx;
    if ((uint32_t)(v >> 32)) {
        _BitScanReverse(&idx, (unsigned long)(v >> 32));
        return (uint32_t)idx + 32u;
    }
    _BitScanReverse(&idx, (unsigned long)v);
    return (uint32_t)idx;
#elif defined(__clang__) || defined(__GNUC__)
    return 63u - (uint32_t)__builtin_clzll(v);
#else
    uint32_t n = 0;
    while (v > 1) { v >>= 1; n++; }
    return n;
#endif
}

static inline int32_t jxl_unpack_signed(uint32_t u) {
    return (int32_t)((u >> 1) ^ (~(u & 1) + 1));
}

typedef struct {
    uint8_t *cs;
    size_t cs_len;
    int cs_owned;
    const uint8_t *exif; size_t exif_len;
    const uint8_t *xmp;  size_t xmp_len;
    const uint8_t *jbrd; size_t jbrd_len;
} jxl_container;

int jxl_container_parse(jxl_ctx *ctx, const uint8_t *data, size_t len,
                        jxl_container *out);
void jxl_container_free(jxl_ctx *ctx, jxl_container *c);

typedef enum {
    JXL_EC_ALPHA = 0,
    JXL_EC_DEPTH = 1,
    JXL_EC_SPOT = 2,
    JXL_EC_SELECTION_MASK = 3,
    JXL_EC_BLACK = 4,
    JXL_EC_CFA = 5,
    JXL_EC_THERMAL = 6,
    JXL_EC_NON_OPTIONAL = 15,
    JXL_EC_OPTIONAL = 16
} jxl_ec_type;

typedef struct {
    int float_sample;
    uint32_t bits_per_sample;
    uint32_t exp_bits;
} jxl_bit_depth;

typedef struct {
    jxl_ec_type type;
    jxl_bit_depth bit_depth;
    uint32_t dim_shift;
    char *name;
    int alpha_associated;
    float spot[4];
    uint32_t cfa_channel;
} jxl_ec_info;

typedef enum {
    JXL_WP_D65 = 1,
    JXL_WP_CUSTOM = 2,
    JXL_WP_E = 10,
    JXL_WP_DCI = 11
} jxl_white_point;

typedef enum {
    JXL_PRIMARIES_SRGB = 1,
    JXL_PRIMARIES_CUSTOM = 2,
    JXL_PRIMARIES_2100 = 9,
    JXL_PRIMARIES_P3 = 11
} jxl_primaries;

typedef enum {
    JXL_TF_709 = 1,
    JXL_TF_UNKNOWN = 2,
    JXL_TF_LINEAR = 8,
    JXL_TF_SRGB = 13,
    JXL_TF_PQ = 16,
    JXL_TF_DCI = 17,
    JXL_TF_HLG = 18
} jxl_transfer_function;

typedef struct {
    int want_icc;
    jxl_color_space colour_space;
    jxl_white_point white_point;
    float white_xy[2];
    jxl_primaries primaries;
    float prim_xy[6];
    int tf_have_gamma;
    uint32_t tf_gamma;
    jxl_transfer_function tf;
    uint32_t rendering_intent;
} jxl_colour_encoding;

typedef struct {
    float intensity_target;
    float min_nits;
    int relative_to_max_display;
    float linear_below;
} jxl_tone_mapping;

typedef struct {
    uint32_t tps_numerator;
    uint32_t tps_denominator;
    uint32_t num_loops;
    int have_timecodes;
} jxl_animation_header;

typedef struct {
    uint32_t width, height;
} jxl_size_header;

typedef struct {
    uint32_t orientation;
    int have_intr_size;
    jxl_size_header intrinsic;
    int have_preview;
    jxl_size_header preview;
    int have_animation;
    jxl_animation_header animation;

    jxl_bit_depth bit_depth;
    int modular_16bit_buffers;
    uint32_t num_extra;
    jxl_ec_info *ec_info;
    int alpha_index;

    int xyb_encoded;
    jxl_colour_encoding colour;
    jxl_tone_mapping tone_mapping;

    float opsin_inv[9];
    float opsin_bias[3];
    float quant_bias[3];
    float quant_bias_numerator;

    float up2[15], up4[55], up8[210];
} jxl_image_metadata;

char *jxl_read_name(jxl_ctx *ctx, jxl_br *br);
int jxl_read_image_header(jxl_ctx *ctx, jxl_br *br, jxl_size_header *size,
                          jxl_image_metadata *meta);
void jxl_image_metadata_free(jxl_ctx *ctx, jxl_image_metadata *meta);

void jxl_apply_orientation_dims(uint32_t orientation, uint32_t w, uint32_t h,
                                uint32_t *ow, uint32_t *oh);

extern const float jxl_default_up2[15];
extern const float jxl_default_up4[55];
extern const float jxl_default_up8[210];

int jxl_read_icc(jxl_ctx *ctx, jxl_br *br, uint8_t **out, size_t *out_len);

uint32_t jxl_bitlen(uint32_t x);

typedef struct {

    uint32_t bits;
} jxl_pfx_entry;

typedef struct {
    jxl_pfx_entry *root;
    jxl_pfx_entry *sub;
    uint32_t nsub;
    int root_bits;
    uint32_t root_mask;
    int single_symbol;
} jxl_pfx_hist;

typedef struct {
    uint8_t alias_symbol;
    uint8_t alias_cutoff;
    uint16_t dist;
    uint16_t alias_offset;
    uint16_t alias_dist_xor;
} jxl_ans_bucket;

typedef struct {
    jxl_ans_bucket *buckets;
    uint32_t log_bucket_size;
    uint32_t bucket_mask;
    int single_symbol;
} jxl_ans_hist;

typedef struct {
    uint32_t split_exponent;
    uint32_t split;
    uint32_t msb_in_token;
    uint32_t lsb_in_token;
} jxl_int_config;

typedef struct {
    jxl_ctx *ctx;
    uint8_t *clusters;
    uint32_t num_dist;
    uint32_t num_clusters;
    jxl_int_config *configs;
    int use_prefix;
    jxl_pfx_hist *pfx;
    jxl_ans_hist *ans;
    uint32_t state;

    int lz77_enabled;
    uint32_t min_symbol;
    uint32_t min_length;
    jxl_int_config lz_len_conf;
    uint32_t *window;
    uint32_t num_to_copy;
    uint32_t copy_pos;
    uint32_t num_decoded;

    int err;
} jxl_dec;

int jxl_dec_init(jxl_ctx *ctx, jxl_dec *dec, jxl_br *br, uint32_t num_dist);
void jxl_dec_begin(jxl_dec *dec, jxl_br *br);
void jxl_dec_free(jxl_dec *dec);
uint32_t jxl_dec_read(jxl_dec *dec, jxl_br *br, uint32_t ctx_idx);
uint32_t jxl_dec_read_mult(jxl_dec *dec, jxl_br *br, uint32_t ctx_idx,
                           uint32_t dist_multiplier);

uint32_t jxl_dec_read_clustered(jxl_dec *dec, jxl_br *br, uint32_t cluster,
                                uint32_t dist_multiplier);

uint32_t jxl_dec_read_clustered_no_lz77(jxl_dec *dec, jxl_br *br,
                                        uint32_t cluster);

int jxl_dec_is_prefix_rle1(const jxl_dec *dec);
uint32_t jxl_dec_read_prefix_rle1(jxl_dec *dec, jxl_br *br, uint32_t cluster,
                                  uint32_t *last, uint32_t *run, int *have);
int jxl_dec_finalize(jxl_dec *dec);

int jxl_read_clusters(jxl_ctx *ctx, jxl_br *br, uint32_t num_dist,
                      uint8_t *clusters, uint32_t *num_clusters_out);
int jxl_read_permutation(jxl_ctx *ctx, jxl_dec *dec, jxl_br *br, uint32_t size,
                         uint32_t skip, uint32_t *out);

typedef struct {
    uint32_t p1, p2, p3a, p3b, p3c, p3d, p3e;
    uint32_t w0, w1, w2, w3;
} jxl_wp_header;

typedef enum {
    JXL_PRED_ZERO = 0,
    JXL_PRED_WEST,
    JXL_PRED_NORTH,
    JXL_PRED_AVG_W_N,
    JXL_PRED_SELECT,
    JXL_PRED_GRADIENT,
    JXL_PRED_SELF_CORRECTING,
    JXL_PRED_NORTH_EAST,
    JXL_PRED_NORTH_WEST,
    JXL_PRED_WEST_WEST,
    JXL_PRED_AVG_W_NW,
    JXL_PRED_AVG_N_NW,
    JXL_PRED_AVG_N_NE,
    JXL_PRED_AVG_ALL
} jxl_predictor;

typedef struct {
    int32_t property;
    int32_t value;
    uint32_t child;
} jxl_ma_node;

typedef struct {
    int32_t offset;
    uint32_t multiplier;
    uint8_t cluster;
    uint8_t predictor;
} jxl_ma_leaf;

typedef struct {
    int32_t property;
    union {
        struct {
            int32_t split0;
            int32_t split1, split2;
            uint32_t child;
            int16_t prop1, prop2;
        } dec;
        jxl_ma_leaf leaf;
    } u;
} jxl_ma_flat;

typedef struct {
    jxl_ma_flat *flat;
    uint32_t nflat;

    jxl_ma_node *raw;
    uint32_t nraw, root;
    jxl_ma_leaf *leaves;

    const jxl_ma_leaf **wp_lut;
    uint8_t *wp_cluster_lut;
    jxl_dec dec;
    int valid;
} jxl_ma_config;

int jxl_ma_config_read(jxl_ctx *ctx, jxl_br *br, jxl_ma_config *ma,
                       size_t node_limit);
void jxl_ma_config_free(jxl_ctx *ctx, jxl_ma_config *ma);

typedef struct {
    int32_t *data;
    size_t stride;
    uint32_t w, h;
    int hshift, vshift;
    uint32_t ow, oh;
} jxl_mchan;

typedef struct {
    int horizontal, in_place;
    uint32_t begin_c, num_c;
} jxl_sq_param;

typedef struct {
    int kind;
    uint32_t begin_c;
    uint32_t rct_type;
    uint32_t num_c, nb_colours, nb_deltas;
    uint8_t d_pred;
    jxl_sq_param *sp;
    uint32_t nsp;

    int32_t *pal_buf;
    jxl_mchan pal;
    jxl_mchan *saved;
    uint32_t nsaved;
} jxl_transform;

typedef struct {
    jxl_mchan *chans;
    uint32_t n;
    uint32_t cap;
    uint32_t nb_meta;
} jxl_chanlist;

typedef struct {
    int use_global_tree;
    jxl_wp_header wp;
    uint32_t ntransforms;
    jxl_transform *transforms;
} jxl_modular_header;

typedef struct {
    uint32_t w, h;
    int hshift, vshift;
} jxl_mchan_spec;

typedef struct {
    jxl_ctx *ctx;
    jxl_modular_header header;
    jxl_ma_config *ma;
    jxl_ma_config local;
    int has_local;
    uint32_t group_dim;
    uint32_t bit_depth;

    jxl_mchan *base;
    uint32_t nbase;
    int32_t **bufs;
    uint32_t nbufs;
} jxl_modular;

int jxl_modular_init(jxl_ctx *ctx, jxl_modular *m, jxl_br *br,
                     const jxl_mchan_spec *specs, uint32_t nspecs,
                     jxl_ma_config *global_ma, uint32_t group_dim,
                     uint32_t bit_depth);

int jxl_modular_init_over(jxl_ctx *ctx, jxl_modular *m, jxl_br *br,
                          const jxl_mchan *chans, uint32_t nchans,
                          jxl_ma_config *global_ma, uint32_t group_dim,
                          uint32_t bit_depth);
void jxl_modular_free(jxl_ctx *ctx, jxl_modular *m);

int jxl_modular_transform_channels(jxl_ctx *ctx, jxl_modular *m,
                                   jxl_chanlist *cl);

int jxl_modular_inverse(jxl_ctx *ctx, jxl_modular *m, jxl_chanlist *cl);

int jxl_modular_decode(jxl_ctx *ctx, jxl_modular *m, jxl_chanlist *cl,
                       jxl_br *br, uint32_t stream_idx);

void jxl_chanlist_free(jxl_ctx *ctx, jxl_chanlist *cl);
int jxl_chanlist_push(jxl_ctx *ctx, jxl_chanlist *cl, const jxl_mchan *ch);

typedef enum {
    JXL_FRAME_REGULAR = 0,
    JXL_FRAME_LF = 1,
    JXL_FRAME_REFERENCE_ONLY = 2,
    JXL_FRAME_SKIP_PROGRESSIVE = 3
} jxl_frame_type;

typedef enum {
    JXL_ENC_VARDCT = 0,
    JXL_ENC_MODULAR = 1
} jxl_encoding;

#define JXL_FF_NOISE                    0x01
#define JXL_FF_PATCHES                  0x02
#define JXL_FF_SPLINES                  0x10
#define JXL_FF_USE_LF_FRAME             0x20
#define JXL_FF_SKIP_ADAPTIVE_LF_SMOOTH  0x80

typedef enum {
    JXL_BLEND_REPLACE = 0,
    JXL_BLEND_ADD = 1,
    JXL_BLEND_BLEND = 2,
    JXL_BLEND_MULADD = 3,
    JXL_BLEND_MUL = 4
} jxl_blend_mode;

typedef struct {
    jxl_blend_mode mode;
    uint32_t alpha_channel;
    int clamp;
    uint32_t source;
} jxl_blending_info;

typedef struct {
    int enabled;
    float weights[3][2];
} jxl_gabor;

typedef struct {
    int enabled;
    uint32_t iters;
    float sharp_lut[8];
    float channel_scale[3];
    float quant_mul;
    float pass0_sigma_scale;
    float pass2_sigma_scale;
    float border_sad_mul;
    float sigma_for_modular;
} jxl_epf;

#define JXL_EPF_INV_SIGMA_NUM \
    (6.6f * (0.70710678118654752f - 1.0f))

typedef struct {
    uint32_t num_passes;
    uint32_t num_ds;
    uint32_t shift[16];
    uint32_t downsample[8];
    uint32_t last_pass[8];
} jxl_passes;

typedef struct {
    jxl_frame_type frame_type;
    jxl_encoding encoding;
    uint64_t flags;
    int do_ycbcr;
    int encoded_color_channels;
    uint32_t jpeg_upsampling[3];
    uint32_t upsampling;
    uint32_t *ec_upsampling;
    uint32_t group_size_shift;
    uint32_t x_qm_scale, b_qm_scale;
    jxl_passes passes;
    uint32_t lf_level;
    int have_crop;
    int32_t x0, y0;
    uint32_t width, height;
    jxl_blending_info blending;
    jxl_blending_info *ec_blending;
    uint32_t duration;
    uint32_t timecode;
    int is_last;
    uint32_t save_as_reference;
    int resets_canvas;
    int save_before_ct;
    char *name;
    jxl_gabor gab;
    jxl_epf epf;
} jxl_frame_header;

typedef enum {
    JXL_TOC_ALL = 0,
    JXL_TOC_LF_GLOBAL,
    JXL_TOC_LF_GROUP,
    JXL_TOC_HF_GLOBAL,
    JXL_TOC_GROUP_PASS
} jxl_toc_kind;

typedef struct {
    size_t offset;
    uint32_t size;
} jxl_toc_entry;

typedef struct {
    jxl_toc_entry *entries;
    uint32_t count;
    uint32_t num_lf_groups;
    uint32_t num_groups;
    uint32_t num_passes;
    size_t total_size;
    size_t end_off;
} jxl_toc;

int jxl_read_frame_header(jxl_ctx *ctx, jxl_br *br, const jxl_size_header *size,
                          const jxl_image_metadata *meta, jxl_frame_header *fh);
void jxl_frame_header_free(jxl_ctx *ctx, jxl_frame_header *fh);
int jxl_read_toc(jxl_ctx *ctx, jxl_br *br, const jxl_frame_header *fh,
                 jxl_toc *toc);
void jxl_toc_free(jxl_ctx *ctx, jxl_toc *toc);

uint32_t jxl_toc_index(const jxl_toc *toc, jxl_toc_kind kind, uint32_t pass_idx,
                       uint32_t group_idx);

uint32_t jxl_frame_sample_width(const jxl_frame_header *fh, uint32_t upsampling);
uint32_t jxl_frame_sample_height(const jxl_frame_header *fh, uint32_t upsampling);
uint32_t jxl_frame_color_width(const jxl_frame_header *fh);
uint32_t jxl_frame_color_height(const jxl_frame_header *fh);
uint32_t jxl_frame_group_dim(const jxl_frame_header *fh);
uint32_t jxl_frame_num_groups(const jxl_frame_header *fh);
uint32_t jxl_frame_num_lf_groups(const jxl_frame_header *fh);
uint32_t jxl_frame_groups_per_row(const jxl_frame_header *fh);
uint32_t jxl_frame_lf_groups_per_row(const jxl_frame_header *fh);

uint32_t jxl_frame_blocks_w(const jxl_frame_header *fh);
uint32_t jxl_frame_blocks_h(const jxl_frame_header *fh);

void jxl_dct_2d(float *data, size_t stride, int w, int h, int inverse);
float jxl_scale_f(int c, int logb);

typedef enum {
    JXL_TR_DCT8 = 0, JXL_TR_HORNUSS, JXL_TR_DCT2, JXL_TR_DCT4, JXL_TR_DCT16,
    JXL_TR_DCT32, JXL_TR_DCT16X8, JXL_TR_DCT8X16, JXL_TR_DCT32X8,
    JXL_TR_DCT8X32, JXL_TR_DCT32X16, JXL_TR_DCT16X32, JXL_TR_DCT4X8,
    JXL_TR_DCT8X4, JXL_TR_AFV0, JXL_TR_AFV1, JXL_TR_AFV2, JXL_TR_AFV3,
    JXL_TR_DCT64, JXL_TR_DCT64X32, JXL_TR_DCT32X64, JXL_TR_DCT128,
    JXL_TR_DCT128X64, JXL_TR_DCT64X128, JXL_TR_DCT256, JXL_TR_DCT256X128,
    JXL_TR_DCT128X256, JXL_TR_COUNT
} jxl_transform_type;

void jxl_tr_select_size(int tr, uint32_t *bw, uint32_t *bh);
void jxl_tr_matrix_size(int tr, uint32_t *w, uint32_t *h);
int jxl_tr_matrix_index(int tr);
int jxl_tr_order_id(int tr);
int jxl_tr_need_transpose(int tr);

typedef struct {
    uint32_t global_scale;
    uint32_t quant_lf;
} jxl_quantizer;

typedef struct {
    uint32_t colour_factor;
    float base_correlation_x, base_correlation_b;
    uint32_t x_factor_lf, b_factor_lf;
} jxl_lf_chan_corr;

typedef struct {
    uint32_t nqf;
    uint32_t qf_thresholds[16];
    uint32_t nlf[3];
    int32_t lf_thresholds[3][16];
    uint8_t *block_ctx_map;
    uint32_t block_ctx_map_len;
    uint32_t num_block_clusters;
} jxl_hf_block_ctx;

struct jxl_dq_encoding;
typedef struct {
    float *matrix[17][3];
    float *matrix_tr[17][3];
    struct jxl_dq_encoding *enc;
} jxl_dequant_matrices;

typedef struct {
    uint16_t *order[13][3];
    jxl_dec dist;
    int have_dist;
} jxl_hf_pass;

#define JXL_BLK_UNINIT   0xff
#define JXL_BLK_OCCUPIED 0xfe

typedef struct {
    uint8_t dct_select;
    uint8_t hf_nonzero_mask;
    uint8_t hf_transform_mask;
    int32_t hf_mul;
} jxl_block_info;

typedef struct {
    int32_t *x_from_y, *b_from_y;
    uint32_t cfl_w, cfl_h;
    jxl_block_info *block_info;
    uint32_t bw, bh;
    float *epf_sigma;
    int have;
} jxl_hf_meta;

typedef struct {
    uint16_t *order[13];
} jxl_natural_orders;

const uint16_t *jxl_natural_order(jxl_ctx *ctx, jxl_natural_orders *no,
                                  int order_id);
void jxl_natural_orders_free(jxl_ctx *ctx, jxl_natural_orders *no);

void jxl_quantizer_read(jxl_br *br, jxl_quantizer *q);

void jxl_lf_chan_corr_read(jxl_br *br, jxl_lf_chan_corr *c, int xyb);

void jxl_jpeg_upsampling_shifts(const uint32_t ju[3], int idx, int *hs,
                                int *vs);

void jxl_chroma_upsample(float *p, uint32_t w, uint32_t h, size_t stride,
                         int hs, int vs, uint32_t out_w, uint32_t out_h);

void jxl_ycbcr_to_rgb(float *cb, float *y, float *cr, size_t n);

void jxl_ycbcr_to_gray(float *gray, const float *y, const float *cr, size_t n);
int jxl_hf_block_ctx_read(jxl_ctx *ctx, jxl_br *br, jxl_hf_block_ctx *bc);
void jxl_hf_block_ctx_free(jxl_ctx *ctx, jxl_hf_block_ctx *bc);

int jxl_dequant_matrices_read(jxl_ctx *ctx, jxl_br *br,
                              jxl_dequant_matrices *dm, uint32_t bit_depth,
                              uint32_t num_lf_groups,
                              jxl_ma_config *global_ma);
void jxl_dequant_matrices_free(jxl_ctx *ctx, jxl_dequant_matrices *dm);

int jxl_dequant_matrices_ensure(jxl_ctx *ctx, jxl_dequant_matrices *dm, int tr);

int jxl_hf_pass_read(jxl_ctx *ctx, jxl_br *br, jxl_hf_pass *hp,
                     const jxl_hf_block_ctx *bc, uint32_t num_hf_presets,
                     jxl_natural_orders *no);
void jxl_hf_pass_free(jxl_ctx *ctx, jxl_hf_pass *hp);

typedef struct {
    uint32_t num_hf_presets;
    const jxl_hf_block_ctx *bc;
    jxl_block_info *block_info;
    uint32_t bi_w, bi_h;
    size_t bi_stride;
    uint32_t jpeg_upsampling[3];
    const jxl_mchan *lf_quant[3];
    jxl_hf_pass *pass;
    uint32_t coeff_shift;
    jxl_natural_orders *no;
    uint8_t discard_mask;
} jxl_hf_coeff_params;

int jxl_write_hf_coeff(jxl_ctx *ctx, jxl_br *br,
                       const jxl_hf_coeff_params *params, float *out[3],
                       size_t stride[3]);

void jxl_hf_meta_free(jxl_ctx *ctx, jxl_hf_meta *m);
int jxl_hf_meta_read(jxl_ctx *ctx, jxl_br *br, jxl_hf_meta *m,
                     uint32_t num_lf_groups, uint32_t lf_group_idx,
                     uint32_t lf_width, uint32_t lf_height,
                     const uint32_t jpeg_upsampling[3], uint32_t bit_depth,
                     jxl_ma_config *global_ma, const jxl_epf *epf,
                     uint32_t quantizer_global_scale);

void jxl_copy_lf_dequant(float *dst, size_t dstride, const jxl_mchan *src,
                         const jxl_quantizer *q, float m_lf,
                         int extra_precision);
int jxl_adaptive_lf_smoothing(jxl_ctx *ctx, float *plane[3], uint32_t width,
                              uint32_t height, size_t stride,
                              const float m_lf[3], const jxl_quantizer *q);
void jxl_cfl_lf(float *x, float *y, float *b, uint32_t w, uint32_t h,
                size_t stride, const jxl_lf_chan_corr *corr);
void jxl_cfl_hf(float *cx, float *cy, float *cb, size_t stride, uint32_t gw,
                uint32_t gh, const int32_t *x_from_y, const int32_t *b_from_y,
                uint32_t cfl_stride, const jxl_lf_chan_corr *corr, int skip_x);
void jxl_dequant_varblock(float *coeff, size_t stride, int tr, int32_t hf_mul,
                          int channel, const jxl_dequant_matrices *dm,
                          const jxl_quantizer *q, float qm_scale,
                          float quant_bias, float quant_bias_numerator);
void jxl_dequant_dct8_plane(float *coeff, size_t stride,
                            const jxl_block_info *blocks,
                            uint32_t blocks_w, uint32_t blocks_h,
                            int channel, const jxl_dequant_matrices *dm,
                            const jxl_quantizer *q, float qm_scale,
                            float quant_bias, float quant_bias_numerator);
void jxl_transform_varblock(float *coeff, size_t stride, int tr);
void jxl_idct8x8_plane(float *data, size_t stride,
                       const jxl_block_info *blocks, int channel,
                       uint32_t blocks_w, uint32_t blocks_h);
void jxl_fill_varblock_lf(float *coeff, size_t stride, int tr,
                          const float *lf, size_t lf_stride, uint32_t lf_x,
                          uint32_t lf_y);

int jxl_apply_gabor(jxl_ctx *ctx, float *plane[3], uint32_t w, uint32_t h,
                    size_t stride, const float weights[3][2]);
int jxl_apply_epf(jxl_ctx *ctx, float *plane[3], uint32_t w, uint32_t h,
                  size_t stride, const float *sigma, uint32_t sigma_stride,
                  const jxl_epf *epf);

void jxl_xyb_to_linear(float *x, float *y, float *b, size_t n,
                       const float opsin_inv[9], const float opsin_bias[3],
                       float intensity_target);
void jxl_linear_to_tf(float *v, size_t n, const jxl_colour_encoding *enc,
                      float intensity_target);

void jxl_opsin_matrix_for(const jxl_image_metadata *meta, float out[9]);

typedef enum {
    JXL_PATCH_NONE = 0,
    JXL_PATCH_REPLACE,
    JXL_PATCH_ADD,
    JXL_PATCH_MUL,
    JXL_PATCH_BLEND_ABOVE,
    JXL_PATCH_BLEND_BELOW,
    JXL_PATCH_MULADD_ABOVE,
    JXL_PATCH_MULADD_BELOW
} jxl_patch_mode;

typedef struct {
    uint8_t mode;
    uint32_t alpha_channel;
    int clamp;
} jxl_patch_blend;

typedef struct {
    int32_t x, y;
    jxl_patch_blend *blending;
} jxl_patch_target;

typedef struct {
    uint32_t ref_idx;
    uint32_t x0, y0, width, height;
    jxl_patch_target *targets;
    uint32_t ntargets;
} jxl_patch_ref;

typedef struct {
    jxl_patch_ref *refs;
    uint32_t n;
    uint32_t nblend;
} jxl_patches;

int jxl_patches_read(jxl_ctx *ctx, jxl_br *br, const jxl_image_metadata *meta,
                     const jxl_frame_header *fh, jxl_patches *out);
void jxl_patches_free(jxl_ctx *ctx, jxl_patches *p);

typedef struct {
    int64_t *px, *py;
    uint32_t npoints;
    int32_t xyb_dct[3][32];
    int32_t sigma_dct[32];
} jxl_quant_spline;

typedef struct {
    jxl_quant_spline *splines;
    uint32_t n;
    int32_t quant_adjust;
} jxl_splines;

int jxl_splines_read(jxl_ctx *ctx, jxl_br *br, const jxl_frame_header *fh,
                     jxl_splines *out);
void jxl_splines_free(jxl_ctx *ctx, jxl_splines *sp);

typedef struct {
    float lut[8];
} jxl_noise_params;

int jxl_noise_params_read(jxl_br *br, jxl_noise_params *np);

typedef struct {
    float *data;
    uint32_t w, h;
    size_t stride;
} jxl_fplane;

typedef struct {
    jxl_ctx *ctx;
    jxl_fplane *plane;
    uint32_t nplane;
    uint32_t ncolor;
    uint32_t w, h;
} jxl_fimage;

int jxl_fimage_alloc(jxl_ctx *ctx, jxl_fimage *img, uint32_t nplane);
int jxl_fplane_alloc(jxl_ctx *ctx, jxl_fplane *p, uint32_t w, uint32_t h);

int jxl_fplane_alloc_uninit(jxl_ctx *ctx, jxl_fplane *p, uint32_t w, uint32_t h);
void jxl_fimage_free(jxl_ctx *ctx, jxl_fimage *img);

typedef struct {
    jxl_fimage refs[4];
    int refs_valid[4];
    jxl_fimage lf_image;
    int lf_valid;

    uint32_t visible_frames;
    uint32_t invisible_frames;
} jxl_frame_state;

void jxl_frame_state_free(jxl_ctx *ctx, jxl_frame_state *st);

int jxl_apply_patches(jxl_ctx *ctx, jxl_fimage *img, const jxl_patches *p,
                      const jxl_image_metadata *meta, jxl_fimage refs[4],
                      const int refs_valid[4]);
int jxl_blend_frame(jxl_ctx *ctx, jxl_fimage *canvas, const jxl_fimage *frame,
                    const jxl_frame_header *fh, const jxl_image_metadata *meta);
int jxl_fimage_blank_like(jxl_ctx *ctx, jxl_fimage *out, const jxl_fimage *like,
                          uint32_t w, uint32_t h);
int jxl_fimage_copy(jxl_ctx *ctx, jxl_fimage *dst, const jxl_fimage *src);
int jxl_render_splines(jxl_ctx *ctx, jxl_fimage *img, const jxl_splines *sp,
                       const jxl_frame_header *fh, float corr_x, float corr_b);
int jxl_render_noise(jxl_ctx *ctx, jxl_fimage *img, const jxl_noise_params *np,
                     const jxl_frame_header *fh, uint32_t visible_frames,
                     uint32_t invisible_frames, float corr_x, float corr_b);

int jxl_upsample_plane(jxl_ctx *ctx, jxl_fplane *p, uint32_t shift,
                       const jxl_image_metadata *meta, uint32_t out_w,
                       uint32_t out_h);

int jxl_frame_decode(jxl_ctx *ctx, jxl_doc *doc, const jxl_frame_header *fh,
                     const jxl_toc *toc, jxl_frame_state *st, int apply_ct,
                     jxl_fimage *out);

struct jxl_doc {
    jxl_ctx *ctx;
    const uint8_t *data;
    size_t len;
    jxl_container container;

    jxl_size_header size;
    jxl_image_metadata meta;

    uint8_t *icc;
    size_t icc_len;

    size_t first_frame_off;
    size_t first_frame_bitpos;
    int frame_count;
};

#endif

#include <stdio.h>
#include <stdlib.h>

#if defined(_WIN32)
#include <intrin.h>
#elif defined(__x86_64__) || defined(__i386__)
#include <cpuid.h>
#endif

static int jxl_avx2_cached = -1;
static int jxl_avx2_fma_cached = -1;

static int jxl_detect_avx2(void) {
#if defined(JXL_NO_AVX2)
    return 0;
#elif defined(_WIN32) && (defined(_M_X64) || defined(_M_AMD64) || defined(_M_IX86))
    int r[4];
    __cpuid(r, 0);
    if (r[0] < 7) return 0;
    __cpuidex(r, 1, 0);
    if (!(r[2] & (1 << 27))) return 0;
    if (!(r[2] & (1 << 28))) return 0;

#if defined(__GNUC__) || defined(__clang__)
    {
        unsigned lo, hi;
        __asm__ volatile("xgetbv" : "=a"(lo), "=d"(hi) : "c"(0));
        (void)hi;
        if ((lo & 0x6u) != 0x6u) return 0;
    }
#else
    if ((_xgetbv(0) & 0x6u) != 0x6u) return 0;
#endif
    __cpuidex(r, 7, 0);
    return (r[1] & (1 << 5)) != 0;
#elif defined(__x86_64__) || defined(__i386__)
    unsigned a, b, c, d;
    if (!__get_cpuid_max(0, 0)) return 0;
    if (!__get_cpuid_count(1, 0, &a, &b, &c, &d)) return 0;
    if (!(c & (1u << 27)) || !(c & (1u << 28))) return 0;
    {
        unsigned lo, hi;
        __asm__ volatile("xgetbv" : "=a"(lo), "=d"(hi) : "c"(0));
        (void)hi;
        if ((lo & 0x6u) != 0x6u) return 0;
    }
    if (!__get_cpuid_count(7, 0, &a, &b, &c, &d)) return 0;
    return (b & (1u << 5)) != 0;
#else
    return 0;
#endif
}

int jxl_has_avx2(void) {
    int v = jxl_avx2_cached;
    if (v < 0) {
        v = jxl_detect_avx2();
        jxl_avx2_cached = v;
    }
    return v;
}

static int jxl_detect_avx2_fma(void) {
    if (!jxl_has_avx2()) return 0;
#if defined(_WIN32) && (defined(_M_X64) || defined(_M_AMD64) || defined(_M_IX86))
    int r[4];
    __cpuidex(r, 1, 0);
    return (r[2] & (1 << 12)) != 0;
#elif defined(__x86_64__) || defined(__i386__)
    unsigned a, b, c, d;
    if (!__get_cpuid_count(1, 0, &a, &b, &c, &d)) return 0;
    return (c & (1u << 12)) != 0;
#else
    return 0;
#endif
}

int jxl_has_avx2_fma(void) {
    int v = jxl_avx2_fma_cached;
    if (v < 0) {
        v = jxl_detect_avx2_fma();
        jxl_avx2_fma_cached = v;
    }
    return v;
}

static void *default_alloc(void *user, void *ctx, size_t size) {
    (void)user;
    (void)ctx;
    return malloc(size);
}

static void default_free(void *user, void *ctx, void *ptr) {
    (void)user;
    (void)ctx;
    free(ptr);
}

jxl_ctx *jxl_ctx_new(jxl_alloc_cb alloc, jxl_free_cb free_cb,
                     jxl_error_cb error, void *user) {
    jxl_ctx *ctx;
    jxl_alloc_cb a = alloc ? alloc : default_alloc;
    jxl_free_cb f = free_cb ? free_cb : default_free;

    ctx = (jxl_ctx *)a(user, NULL, sizeof(*ctx));
    if (!ctx) return NULL;
    memset(ctx, 0, sizeof(*ctx));
    ctx->alloc = a;
    ctx->free_cb = f;
    ctx->error = error;
    ctx->user = user;
    return ctx;
}

void jxl_ctx_free(jxl_ctx *ctx) {
    jxl_free_cb f;
    void *user;
    if (!ctx) return;
    f = ctx->free_cb;
    user = ctx->user;
    f(user, NULL, ctx);
}

void jxl_ctx_set_bgr(jxl_ctx *ctx, int enable) {
    if (ctx) ctx->bgr = enable ? 1 : 0;
}

void jxl_ctx_set_keep_orientation(jxl_ctx *ctx, int enable) {
    if (ctx) ctx->keep_orientation = enable ? 1 : 0;
}

void jxl_ctx_set_srgb_output(jxl_ctx *ctx, int enable) {
    if (ctx) ctx->srgb_output = enable ? 1 : 0;
}

void jxl_request_abort(jxl_ctx *ctx) {
    if (ctx) ctx->abort_epoch++;
}

void *jxl_malloc(jxl_ctx *ctx, size_t size) {
    if (size == 0) size = 1;
    return ctx->alloc(ctx->user, ctx, size);
}

void *jxl_calloc(jxl_ctx *ctx, size_t count, size_t size) {
    size_t total;
    void *p;
    if (!jxl_size_mul(count, size, &total)) return NULL;
    if (total == 0) total = 1;
    p = ctx->alloc(ctx->user, ctx, total);
    if (p) memset(p, 0, total);
    return p;
}

void *jxl_realloc_array(jxl_ctx *ctx, void *ptr, size_t old_count,
                        size_t new_count, size_t size) {
    size_t old_total, new_total;
    void *p;
    if (!jxl_size_mul(new_count, size, &new_total)) return NULL;
    if (!jxl_size_mul(old_count, size, &old_total)) return NULL;
    p = ctx->alloc(ctx->user, ctx, new_total ? new_total : 1);
    if (!p) return NULL;
    if (ptr && old_total) memcpy(p, ptr, JXL_MIN(old_total, new_total));
    if (new_total > old_total) {
        memset((uint8_t *)p + old_total, 0, new_total - old_total);
    }
    if (ptr) ctx->free_cb(ctx->user, ctx, ptr);
    return p;
}

void jxl_free(jxl_ctx *ctx, void *ptr) {
    if (ptr) ctx->free_cb(ctx->user, ctx, ptr);
}

int jxl_size_mul(size_t a, size_t b, size_t *out) {
    if (a != 0 && b > (size_t)-1 / a) return 0;
    *out = a * b;
    return 1;
}

void jxl_errorf(jxl_ctx *ctx, jxl_severity sev, const char *fmt, ...) {
    char buf[512];
    va_list ap;
    if (!ctx || !ctx->error) return;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    buf[sizeof(buf) - 1] = 0;
    ctx->error(ctx->user, sev, buf);
}

int jxl_format_bpp(jxl_format fmt) {
    switch (fmt) {
        case JXLDEC_FORMAT_GRAY8: return 1;
        case JXLDEC_FORMAT_GRAYA8: return 2;
        case JXLDEC_FORMAT_RGB24: return 3;
        case JXLDEC_FORMAT_RGBA32: return 4;
        case JXLDEC_FORMAT_GRAY16: return 2;
        case JXLDEC_FORMAT_GRAYA16: return 4;
        case JXLDEC_FORMAT_RGB48: return 6;
        case JXLDEC_FORMAT_RGBA64: return 8;
        default: return 0;
    }
}

void jxl_image_destroy(jxl_ctx *ctx, jxl_image *img) {
    if (!img) return;
    jxl_free(ctx, img->data);
    jxl_free(ctx, img);
}

const float jxl_default_up2[15] = {
    -0.01716200f, -0.03452303f, -0.04022174f, -0.02921014f, -0.00624645f,
    0.14111091f,  0.28896755f,  0.00278718f,  -0.01610267f, 0.56661550f,
    0.03777607f,  -0.01986694f, -0.03144731f, -0.01185068f, -0.00213539f
};

const float jxl_default_up4[55] = {
    -0.02419067f, -0.03491987f, -0.03693351f, -0.03094285f, -0.00529785f,
    -0.01663432f, -0.03556863f, -0.03888905f, -0.03516850f, -0.00989469f,
    0.23651958f,  0.33392945f,  -0.01073543f, -0.01313181f, -0.03556694f,
    0.13048175f,  0.40103025f,  0.03951150f,  -0.02077584f, 0.46914198f,
    -0.00209270f, -0.01484589f, -0.04064806f, 0.18942530f,  0.56279892f,
    0.06674400f,  -0.02335494f, -0.03551682f, -0.00754830f, -0.02267919f,
    -0.02363578f, 0.00315804f,  -0.03399098f, -0.01359519f, -0.00091653f,
    -0.00335467f, -0.01163294f, -0.01610294f, -0.00974088f, -0.00191622f,
    -0.01095446f, -0.03198464f, -0.04455121f, -0.02799790f, -0.00645912f,
    0.06390599f,  0.22963888f,  0.00630981f,  -0.01897349f, 0.67537268f,
    0.08483369f,  -0.02534994f, -0.02205197f, -0.01667999f, -0.00384443f
};

const float jxl_default_up8[210] = {
    -0.02928613f, -0.03706353f, -0.03783812f, -0.03324558f, -0.00447632f,
    -0.02519406f, -0.03752601f, -0.03901508f, -0.03663285f, -0.00646649f,
    -0.02066407f, -0.03838633f, -0.04002101f, -0.03900035f, -0.00901973f,
    -0.01626393f, -0.03954148f, -0.04046620f, -0.03979621f, -0.01224485f,
    0.29895328f,  0.35757708f,  -0.02447552f, -0.01081748f, -0.04314594f,
    0.23903219f,  0.41119301f,  -0.00573046f, -0.01450239f, -0.04246845f,
    0.17567618f,  0.45220643f,  0.02287757f,  -0.01936783f, -0.03583255f,
    0.11572472f,  0.47416733f,  0.06284440f,  -0.02685066f, 0.42720050f,
    -0.02248939f, -0.01155273f, -0.04562755f, 0.28689496f,  0.49093869f,
    -0.00007891f, -0.01545926f, -0.04562659f, 0.21238920f,  0.53980934f,
    0.03369474f,  -0.02070211f, -0.03866988f, 0.14229550f,  0.56593398f,
    0.08045181f,  -0.02888298f, -0.03680918f, -0.00542229f, -0.02920477f,
    -0.02788574f, -0.02118180f, -0.03942402f, -0.00775547f, -0.02433614f,
    -0.03193943f, -0.02030828f, -0.04044014f, -0.01074016f, -0.01930822f,
    -0.03620399f, -0.01974125f, -0.03919545f, -0.01456093f, -0.00045072f,
    -0.00360110f, -0.01020207f, -0.01231907f, -0.00638988f, -0.00071592f,
    -0.00279122f, -0.00957115f, -0.01288327f, -0.00730937f, -0.00107783f,
    -0.00210156f, -0.00890705f, -0.01317668f, -0.00813895f, -0.00153491f,
    -0.02128481f, -0.04173044f, -0.04831487f, -0.03293190f, -0.00525260f,
    -0.01720322f, -0.04052736f, -0.05045706f, -0.03607317f, -0.00738030f,
    -0.01341764f, -0.03965629f, -0.05151616f, -0.03814886f, -0.01005819f,
    0.18968273f,  0.33063684f,  -0.01300105f, -0.01372950f, -0.04017465f,
    0.13727832f,  0.36402234f,  0.01027890f,  -0.01832107f, -0.03365072f,
    0.08734506f,  0.38194295f,  0.04338228f,  -0.02525993f, 0.56408126f,
    0.00458352f,  -0.01648227f, -0.04887868f, 0.24585519f,  0.62026135f,
    0.04314807f,  -0.02213737f, -0.04158014f, 0.16637289f,  0.65027023f,
    0.09621636f,  -0.03101388f, -0.04082742f, -0.00904519f, -0.02790922f,
    -0.02117818f, 0.00798662f,  -0.03995711f, -0.01243427f, -0.02231705f,
    -0.02946266f, 0.00992055f,  -0.03600283f, -0.01684920f, -0.00111684f,
    -0.00411204f, -0.01297130f, -0.01723725f, -0.01022545f, -0.00165306f,
    -0.00313110f, -0.01218016f, -0.01763266f, -0.01125620f, -0.00231663f,
    -0.01374149f, -0.03797620f, -0.05142937f, -0.03117307f, -0.00581914f,
    -0.01064003f, -0.03608089f, -0.05272168f, -0.03375670f, -0.00795586f,
    0.09628104f,  0.27129991f,  -0.00353779f, -0.01734151f, -0.03153981f,
    0.05686230f,  0.28500998f,  0.02230594f,  -0.02374955f, 0.68214326f,
    0.05018048f,  -0.02320852f, -0.04383616f, 0.18459474f,  0.71517975f,
    0.10805613f,  -0.03263677f, -0.03637639f, -0.01394373f, -0.02511203f,
    -0.01728636f, 0.05407331f,  -0.02867568f, -0.01893131f, -0.00240854f,
    -0.00446511f, -0.01636187f, -0.02377053f, -0.01522848f, -0.00333334f,
    -0.00819975f, -0.02964169f, -0.04499287f, -0.02745350f, -0.00612408f,
    0.02727416f,  0.19446600f,  0.00159832f,  -0.02232473f, 0.74982506f,
    0.11452620f,  -0.03348048f, -0.01605681f, -0.02070339f, -0.00458223f
};

const float jxl_afv_basis[16][16] = {
    {
        0.25f, 0.25f, 0.25f, 0.25f,
        0.25f, 0.25f, 0.25f, 0.25f,
        0.25f, 0.25f, 0.25f, 0.25f,
        0.25f, 0.25f, 0.25f, 0.25f,
    },
    {
        0.876902929799142f, 0.2206518106944235f, -0.10140050393753763f, -0.1014005039375375f,
        0.2206518106944236f, -0.10140050393753777f, -0.10140050393753772f, -0.10140050393753763f,
        -0.10140050393753758f, -0.10140050393753769f, -0.1014005039375375f, -0.10140050393753768f,
        -0.10140050393753768f, -0.10140050393753759f, -0.10140050393753763f, -0.10140050393753741f,
    },
    {
        0.0f, 0.0f, 0.40670075830260755f, 0.44444816619734445f,
        0.0f, 0.0f, 0.19574399372042936f, 0.2929100136981264f,
        -0.40670075830260716f, -0.19574399372042872f, 0.0f, 0.11379074460448091f,
        -0.44444816619734384f, -0.29291001369812636f, -0.1137907446044814f, 0.0f,
    },
    {
        0.0f, 0.0f, -0.21255748058288748f, 0.3085497062849767f,
        0.0f, 0.4706702258572536f, -0.1621205195722993f, 0.0f,
        -0.21255748058287047f, -0.16212051957228327f, -0.47067022585725277f, -0.1464291867126764f,
        0.3085497062849487f, 0.0f, -0.14642918671266536f, 0.4251149611657548f,
    },
    {
        0.0f, -0.7071067811865474f, 0.0f, 0.0f,
        0.7071067811865476f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f,
    },
    {
        -0.4105377591765233f, 0.6235485373547691f, -0.06435071657946274f, -0.06435071657946266f,
        0.6235485373547694f, -0.06435071657946284f, -0.0643507165794628f, -0.06435071657946274f,
        -0.06435071657946272f, -0.06435071657946279f, -0.06435071657946266f, -0.06435071657946277f,
        -0.06435071657946277f, -0.06435071657946273f, -0.06435071657946274f, -0.0643507165794626f,
    },
    {
        0.0f, 0.0f, -0.4517556589999482f, 0.15854503551840063f,
        0.0f, -0.04038515160822202f, 0.0074182263792423875f, 0.39351034269210167f,
        -0.45175565899994635f, 0.007418226379244351f, 0.1107416575309343f, 0.08298163094882051f,
        0.15854503551839705f, 0.3935103426921022f, 0.0829816309488214f, -0.45175565899994796f,
    },
    {
        0.0f, 0.0f, -0.304684750724869f, 0.5112616136591823f,
        0.0f, 0.0f, -0.290480129728998f, -0.06578701549142804f,
        0.304684750724884f, 0.2904801297290076f, 0.0f, -0.23889773523344604f,
        -0.5112616136592012f, 0.06578701549142545f, 0.23889773523345467f, 0.0f,
    },
    {
        0.0f, 0.0f, 0.3017929516615495f, 0.25792362796341184f,
        0.0f, 0.16272340142866204f, 0.09520022653475037f, 0.0f,
        0.3017929516615503f, 0.09520022653475055f, -0.16272340142866173f, -0.35312385449816297f,
        0.25792362796341295f, 0.0f, -0.3531238544981624f, -0.6035859033230976f,
    },
    {
        0.0f, 0.0f, 0.40824829046386274f, 0.0f,
        0.0f, 0.0f, 0.0f, -0.4082482904638628f,
        -0.4082482904638635f, 0.0f, 0.0f, -0.40824829046386296f,
        0.0f, 0.4082482904638634f, 0.408248290463863f, 0.0f,
    },
    {
        0.0f, 0.0f, 0.1747866975480809f, 0.0812611176717539f,
        0.0f, 0.0f, -0.3675398009862027f, -0.307882213957909f,
        -0.17478669754808135f, 0.3675398009862011f, 0.0f, 0.4826689115059883f,
        -0.08126111767175039f, 0.30788221395790305f, -0.48266891150598584f, 0.0f,
    },
    {
        0.0f, 0.0f, -0.21105601049335784f, 0.18567180916109802f,
        0.0f, 0.0f, 0.49215859013738733f, -0.38525013709251915f,
        0.21105601049335806f, -0.49215859013738905f, 0.0f, 0.17419412659916217f,
        -0.18567180916109904f, 0.3852501370925211f, -0.1741941265991621f, 0.0f,
    },
    {
        0.0f, 0.0f, -0.14266084808807264f, -0.3416446842253372f,
        0.0f, 0.7367497537172237f, 0.24627107722075148f, -0.08574019035519306f,
        -0.14266084808807344f, 0.24627107722075137f, 0.14883399227113567f, -0.04768680350229251f,
        -0.3416446842253373f, -0.08574019035519267f, -0.047686803502292804f, -0.14266084808807242f,
    },
    {
        0.0f, 0.0f, -0.13813540350758585f, 0.3302282550303788f,
        0.0f, 0.08755115000587084f, -0.07946706605909573f, -0.4613374887461511f,
        -0.13813540350758294f, -0.07946706605910261f, 0.49724647109535086f, 0.12538059448563663f,
        0.3302282550303805f, -0.4613374887461554f, 0.12538059448564315f, -0.13813540350758452f,
    },
    {
        0.0f, 0.0f, -0.17437602599651067f, 0.0702790691196284f,
        0.0f, -0.2921026642334881f, 0.3623817333531167f, 0.0f,
        -0.1743760259965108f, 0.36238173335311646f, 0.29210266423348785f, -0.4326608024727445f,
        0.07027906911962818f, 0.0f, -0.4326608024727457f, 0.34875205199302267f,
    },
    {
        0.0f, 0.0f, 0.11354987314994337f, -0.07417504595810355f,
        0.0f, 0.19402893032594343f, -0.435190496523228f, 0.21918684838857466f,
        0.11354987314994257f, -0.4351904965232251f, 0.5550443808910661f, -0.25468277124066463f,
        -0.07417504595810233f, 0.2191868483885728f, -0.25468277124066413f, 0.1135498731499429f,
    },
};

void jxl_br_init(jxl_br *br, const uint8_t *data, size_t len) {
    br->data = data;
    br->len = len;
    br->byte_pos = 0;
    br->buf = 0;
    br->nbits = 0;
    br->err = 0;
}

void jxl_br_refill(jxl_br *br) {
    if (br->byte_pos + 8 <= br->len) {
        uint64_t bits;
        int read_bytes;
        const uint8_t *p = br->data + br->byte_pos;
        memcpy(&bits, p, sizeof(bits));
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        bits = __builtin_bswap64(bits);
#endif
        br->buf |= bits << br->nbits;
        read_bytes = (63 - br->nbits) >> 3;
        br->nbits |= 56;
        br->byte_pos += (size_t)read_bytes;
        return;
    }
    while (br->nbits < 56 && br->byte_pos < br->len) {
        br->buf |= (uint64_t)br->data[br->byte_pos] << br->nbits;
        br->nbits += 8;
        br->byte_pos++;
    }
}

void jxl_br_skip(jxl_br *br, size_t n) {
    if ((size_t)br->nbits >= n) {
        br->nbits -= (int)n;
        br->buf >>= n;
        return;
    }
    n -= (size_t)br->nbits;
    br->buf = 0;
    br->nbits = 0;
    if (n > (br->len - br->byte_pos) * 8) {
        br->byte_pos = br->len;
        br->err = 1;
        return;
    }
    br->byte_pos += n / 8;
    n %= 8;
    jxl_br_refill(br);
    if ((size_t)br->nbits < n) {
        br->err = 1;
        br->nbits = 0;
        br->buf = 0;
        return;
    }
    br->nbits -= (int)n;
    br->buf >>= n;
}

int jxl_br_bool(jxl_br *br) {
    return (int)jxl_br_read(br, 1);
}

uint32_t jxl_br_u32(jxl_br *br, uint32_t c0, int n0, uint32_t c1, int n1,
                    uint32_t c2, int n2, uint32_t c3, int n3) {
    uint32_t sel = jxl_br_read(br, 2);
    uint32_t c;
    int n;
    switch (sel) {
        case 0: c = c0; n = n0; break;
        case 1: c = c1; n = n1; break;
        case 2: c = c2; n = n2; break;
        default: c = c3; n = n3; break;
    }
    if (n == 0) return c;
    return c + jxl_br_read(br, n);
}

uint64_t jxl_br_u64(jxl_br *br) {
    uint32_t sel = jxl_br_read(br, 2);
    switch (sel) {
        case 0: return 0;
        case 1: return (uint64_t)jxl_br_read(br, 4) + 1;
        case 2: return (uint64_t)jxl_br_read(br, 8) + 17;
        default: {
            uint64_t value = jxl_br_read(br, 12);
            int shift = 12;
            while (jxl_br_read(br, 1) == 1) {
                if (br->err) break;
                if (shift == 60) {
                    value |= (uint64_t)jxl_br_read(br, 4) << shift;
                    break;
                }
                value |= (uint64_t)jxl_br_read(br, 8) << shift;
                shift += 8;
            }
            return value;
        }
    }
}

float jxl_br_f16(jxl_br *br) {
    uint32_t v = jxl_br_read(br, 16);
    uint32_t neg = (v & 0x8000u) << 16;
    uint32_t mantissa = v & 0x3ff;
    uint32_t exponent = (v >> 10) & 0x1f;
    uint32_t bits;
    float out;

    if ((v & 0x7fff) == 0) {
        memcpy(&out, &neg, sizeof(out));
        return out;
    }
    if (exponent == 0x1f) {

        br->err = 1;
        return 0.0f;
    }
    if (exponent == 0) {
        float val = (1.0f / 16384.0f) * ((float)mantissa / 1024.0f);
        return neg ? -val : val;
    }
    bits = (mantissa << 13) | ((exponent + 112) << 23) | neg;
    memcpy(&out, &bits, sizeof(out));
    return out;
}

uint32_t jxl_br_enum(jxl_br *br) {
    return jxl_br_u32(br, 0, 0, 1, 0, 2, 4, 18, 6);
}

void jxl_br_zero_pad_to_byte(jxl_br *br) {
    size_t rem = jxl_br_bits_read(br) & 7;
    if (rem == 0) return;
    if (jxl_br_read(br, (int)(8 - rem)) != 0) {
        br->err = 1;
    }
}

size_t jxl_br_byte_pos(const jxl_br *br) {
    return (jxl_br_bits_read(br) + 7) / 8;
}

void jxl_br_seek_byte(jxl_br *br, size_t byte_off) {
    if (byte_off > br->len) {
        br->err = 1;
        byte_off = br->len;
    }
    br->byte_pos = byte_off;
    br->buf = 0;
    br->nbits = 0;
}

static const uint8_t jxl_sig_container[12] = {
    0x00, 0x00, 0x00, 0x0c, 0x4a, 0x58, 0x4c, 0x20, 0x0d, 0x0a, 0x87, 0x0a
};

jxl_signature jxl_signature_check(const uint8_t *data, size_t len) {
    if (!data) return JXLDEC_SIG_INVALID;
    if (len == 0) return JXLDEC_SIG_NOT_ENOUGH_BYTES;
    if (data[0] == 0xff) {
        if (len < 2) return JXLDEC_SIG_NOT_ENOUGH_BYTES;
        return data[1] == 0x0a ? JXLDEC_SIG_CODESTREAM : JXLDEC_SIG_INVALID;
    }
    if (data[0] == 0x00) {
        size_t n = JXL_MIN(len, sizeof(jxl_sig_container));
        if (memcmp(data, jxl_sig_container, n) != 0) return JXLDEC_SIG_INVALID;
        if (len < sizeof(jxl_sig_container)) return JXLDEC_SIG_NOT_ENOUGH_BYTES;
        return JXLDEC_SIG_CONTAINER;
    }
    return JXLDEC_SIG_INVALID;
}

static uint32_t rd32be(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static uint64_t rd64be(const uint8_t *p) {
    return ((uint64_t)rd32be(p) << 32) | rd32be(p + 4);
}

typedef struct {
    const uint8_t *p;
    size_t len;
} jxl_span;

int jxl_container_parse(jxl_ctx *ctx, const uint8_t *data, size_t len,
                        jxl_container *out) {
    jxl_span *spans = NULL;
    size_t nspans = 0, cap = 0;
    size_t pos = 0;
    size_t total = 0;
    int rc = -1;

    memset(out, 0, sizeof(*out));

    if (jxl_signature_check(data, len) == JXLDEC_SIG_CODESTREAM) {
        out->cs = (uint8_t *)data;
        out->cs_len = len;
        out->cs_owned = 0;
        return 0;
    }
    if (len < sizeof(jxl_sig_container) ||
        memcmp(data, jxl_sig_container, sizeof(jxl_sig_container)) != 0) {
        JXL_ERR(ctx, "not a JPEG XL file");
        return -1;
    }
    pos = sizeof(jxl_sig_container);

    while (pos + 8 <= len) {
        uint64_t box_size = rd32be(data + pos);
        const uint8_t *type = data + pos + 4;
        size_t hdr = 8;
        const uint8_t *payload;
        size_t payload_len;

        if (box_size == 1) {
            if (pos + 16 > len) break;
            box_size = rd64be(data + pos + 8);
            hdr = 16;
            if (box_size < 16) {
                JXL_ERR(ctx, "container: bad extended box size");
                goto done;
            }
        } else if (box_size == 0) {
            box_size = len - pos;
        } else if (box_size < 8) {
            JXL_ERR(ctx, "container: bad box size %u", (unsigned)box_size);
            goto done;
        }
        if (box_size > len - pos) {

            box_size = len - pos;
        }
        payload = data + pos + hdr;
        payload_len = (size_t)box_size - hdr;

        if (memcmp(type, "jxlc", 4) == 0) {
            if (nspans == cap) {
                size_t ncap = cap ? cap * 2 : 8;
                jxl_span *ns = (jxl_span *)jxl_realloc_array(
                    ctx, spans, cap, ncap, sizeof(*spans));
                if (!ns) goto done;
                spans = ns;
                cap = ncap;
            }
            spans[nspans].p = payload;
            spans[nspans].len = payload_len;
            nspans++;
            total += payload_len;
        } else if (memcmp(type, "jxlp", 4) == 0) {
            if (payload_len < 4) {
                JXL_ERR(ctx, "container: short jxlp box");
                goto done;
            }
            if (nspans == cap) {
                size_t ncap = cap ? cap * 2 : 8;
                jxl_span *ns = (jxl_span *)jxl_realloc_array(
                    ctx, spans, cap, ncap, sizeof(*spans));
                if (!ns) goto done;
                spans = ns;
                cap = ncap;
            }
            spans[nspans].p = payload + 4;
            spans[nspans].len = payload_len - 4;
            nspans++;
            total += payload_len - 4;
        } else if (memcmp(type, "Exif", 4) == 0) {
            out->exif = payload;
            out->exif_len = payload_len;
        } else if (memcmp(type, "xml ", 4) == 0) {
            out->xmp = payload;
            out->xmp_len = payload_len;
        } else if (memcmp(type, "jbrd", 4) == 0) {
            out->jbrd = payload;
            out->jbrd_len = payload_len;
        }

        pos += (size_t)box_size;
    }

    if (nspans == 0) {
        JXL_ERR(ctx, "container: no codestream box");
        goto done;
    }
    if (nspans == 1) {
        out->cs = (uint8_t *)spans[0].p;
        out->cs_len = spans[0].len;
        out->cs_owned = 0;
    } else {
        uint8_t *buf = (uint8_t *)jxl_malloc(ctx, total ? total : 1);
        size_t off = 0, i;
        if (!buf) goto done;
        for (i = 0; i < nspans; i++) {
            memcpy(buf + off, spans[i].p, spans[i].len);
            off += spans[i].len;
        }
        out->cs = buf;
        out->cs_len = total;
        out->cs_owned = 1;
    }
    rc = 0;

done:
    jxl_free(ctx, spans);
    return rc;
}

void jxl_container_free(jxl_ctx *ctx, jxl_container *c) {
    if (!c) return;
    if (c->cs_owned) jxl_free(ctx, c->cs);
    c->cs = NULL;
    c->cs_len = 0;
    c->cs_owned = 0;
}

char *jxl_read_name(jxl_ctx *ctx, jxl_br *br) {
    uint32_t len = jxl_br_u32(br, 0, 0, 0, 4, 16, 5, 48, 10);
    char *s;
    uint32_t i;
    if (len == 0 || br->err) return NULL;
    s = (char *)jxl_malloc(ctx, len + 1);
    if (!s) return NULL;
    for (i = 0; i < len; i++) s[i] = (char)jxl_br_read(br, 8);
    s[len] = 0;
    return s;
}

static void read_bit_depth(jxl_br *br, jxl_bit_depth *bd) {
    if (jxl_br_bool(br)) {
        bd->float_sample = 1;
        bd->bits_per_sample = jxl_br_u32(br, 32, 0, 16, 0, 24, 0, 1, 6);
        bd->exp_bits = jxl_br_read(br, 4) + 1;
    } else {
        bd->float_sample = 0;
        bd->bits_per_sample = jxl_br_u32(br, 8, 0, 10, 0, 12, 0, 1, 6);
        bd->exp_bits = 0;
    }
}

static void bit_depth_default(jxl_bit_depth *bd) {
    bd->float_sample = 0;
    bd->bits_per_sample = 8;
    bd->exp_bits = 0;
}

static float read_customxy(jxl_br *br) {
    uint32_t u = jxl_br_u32(br, 0, 19, 524288, 19, 1048576, 20, 2097152, 21);
    return (float)jxl_unpack_signed(u) / 1e6f;
}

static void size_header_read(jxl_br *br, jxl_size_header *sz) {
    int div8 = jxl_br_bool(br);
    uint32_t h_div8 = 0, ratio, w_div8 = 0;

    if (div8) {
        h_div8 = jxl_br_read(br, 5) + 1;
        sz->height = 8 * h_div8;
    } else {
        sz->height = jxl_br_u32(br, 1, 9, 1, 13, 1, 18, 1, 30);
    }
    ratio = jxl_br_read(br, 3);
    if (ratio == 0) {
        if (div8) {
            w_div8 = jxl_br_read(br, 5) + 1;
            sz->width = 8 * w_div8;
        } else {
            sz->width = jxl_br_u32(br, 1, 9, 1, 13, 1, 18, 1, 30);
        }
    } else {
        uint64_t h = sz->height;
        uint64_t w;
        switch (ratio) {
            case 1: w = h; break;
            case 2: w = h * 12 / 10; break;
            case 3: w = h * 4 / 3; break;
            case 4: w = h * 3 / 2; break;
            case 5: w = h * 16 / 9; break;
            case 6: w = h * 5 / 4; break;
            default: w = h * 2; break;
        }
        sz->width = (uint32_t)w;
    }
}

static void preview_header_read(jxl_br *br, jxl_size_header *sz) {
    int div8 = jxl_br_bool(br);
    uint32_t ratio, w_div8 = 1;

    if (div8) {
        uint32_t h_div8 = jxl_br_u32(br, 16, 0, 32, 0, 1, 5, 33, 9);
        sz->height = 8 * h_div8;
    } else {
        sz->height = jxl_br_u32(br, 1, 6, 65, 8, 321, 10, 1345, 12);
    }
    ratio = jxl_br_read(br, 3);
    if (ratio == 0) {
        if (div8) {
            w_div8 = jxl_br_u32(br, 16, 0, 32, 0, 1, 5, 33, 9);
            sz->width = 8 * w_div8;
        } else {
            sz->width = jxl_br_u32(br, 1, 6, 65, 8, 321, 10, 1345, 12);
        }
    } else {
        uint64_t h = sz->height;
        uint64_t w;
        switch (ratio) {
            case 1: w = h; break;
            case 2: w = h * 12 / 10; break;
            case 3: w = h * 4 / 3; break;
            case 4: w = h * 3 / 2; break;
            case 5: w = h * 16 / 9; break;
            case 6: w = h * 5 / 4; break;
            default: w = h * 2; break;
        }
        sz->width = (uint32_t)w;
    }
}

static void read_extensions(jxl_br *br) {
    uint64_t bits = jxl_br_u64(br);
    uint64_t lens[64];
    int idx[64];
    int n = 0, i;

    for (i = 0; i < 64; i++) {
        if ((bits >> i) & 1) {
            idx[n] = i;
            lens[n] = jxl_br_u64(br);
            n++;
        }
    }
    for (i = 0; i < n; i++) jxl_br_skip(br, (size_t)lens[i]);
}

static void colour_encoding_default(jxl_colour_encoding *c) {
    memset(c, 0, sizeof(*c));
    c->want_icc = 0;
    c->colour_space = JXLDEC_CS_RGB;
    c->white_point = JXL_WP_D65;
    c->primaries = JXL_PRIMARIES_SRGB;
    c->tf = JXL_TF_SRGB;
    c->tf_have_gamma = 0;
    c->rendering_intent = 1;
}

static void colour_encoding_read(jxl_ctx *ctx, jxl_br *br,
                                 jxl_colour_encoding *c) {
    colour_encoding_default(c);
    if (jxl_br_bool(br)) return;

    c->want_icc = jxl_br_bool(br);
    c->colour_space = (jxl_color_space)jxl_br_enum(br);
    if ((uint32_t)c->colour_space > 3) {
        JXL_ERR(ctx, "header: invalid colour space %u", (unsigned)c->colour_space);
        br->err = 1;
        return;
    }
    if (c->want_icc) return;

    if (c->colour_space != JXLDEC_CS_XYB) {
        c->white_point = (jxl_white_point)jxl_br_enum(br);
        if (c->white_point == JXL_WP_CUSTOM) {
            c->white_xy[0] = read_customxy(br);
            c->white_xy[1] = read_customxy(br);
        }
    }
    if (c->colour_space != JXLDEC_CS_XYB && c->colour_space != JXLDEC_CS_GRAY) {
        c->primaries = (jxl_primaries)jxl_br_enum(br);
        if (c->primaries == JXL_PRIMARIES_CUSTOM) {
            int i;
            for (i = 0; i < 6; i++) c->prim_xy[i] = read_customxy(br);
        }
    }
    if (jxl_br_bool(br)) {
        c->tf_have_gamma = 1;
        c->tf_gamma = jxl_br_read(br, 24);
    } else {
        c->tf = (jxl_transfer_function)jxl_br_enum(br);
    }
    c->rendering_intent = jxl_br_enum(br);
}

static void ec_info_default(jxl_ec_info *ec) {
    memset(ec, 0, sizeof(*ec));
    ec->type = JXL_EC_ALPHA;
    bit_depth_default(&ec->bit_depth);
    ec->dim_shift = 0;
    ec->alpha_associated = 0;
}

static void ec_info_read(jxl_ctx *ctx, jxl_br *br, jxl_ec_info *ec) {
    uint32_t ty;
    ec_info_default(ec);
    if (jxl_br_bool(br)) return;

    ty = jxl_br_enum(br);
    read_bit_depth(br, &ec->bit_depth);
    ec->dim_shift = jxl_br_u32(br, 0, 0, 3, 0, 4, 0, 1, 3);
    ec->name = jxl_read_name(ctx, br);
    ec->type = (jxl_ec_type)ty;

    switch (ty) {
        case JXL_EC_ALPHA:
            ec->alpha_associated = jxl_br_bool(br);
            break;
        case JXL_EC_SPOT:
            ec->spot[0] = jxl_br_f16(br);
            ec->spot[1] = jxl_br_f16(br);
            ec->spot[2] = jxl_br_f16(br);
            ec->spot[3] = jxl_br_f16(br);
            break;
        case JXL_EC_CFA:
            ec->cfa_channel = jxl_br_u32(br, 1, 0, 0, 2, 3, 4, 19, 8);
            break;
        case JXL_EC_DEPTH:
        case JXL_EC_SELECTION_MASK:
        case JXL_EC_BLACK:
        case JXL_EC_THERMAL:
        case JXL_EC_NON_OPTIONAL:
        case JXL_EC_OPTIONAL:
            break;
        default:
            JXL_ERR(ctx, "header: unknown extra channel type %u", (unsigned)ty);
            br->err = 1;
            break;
    }
}

static const float default_opsin_inv[9] = {
    11.031566901960783f, -9.866943921568629f, -0.16462299647058826f,
    -3.254147380392157f, 4.418770392156863f,  -0.16462299647058826f,
    -3.6588512862745097f, 2.7129230470588235f, 1.9459282392156863f
};

static void custom_transform_data_read(jxl_br *br, jxl_image_metadata *meta) {
    uint32_t cw_mask = 0;
    int i;

    memcpy(meta->opsin_inv, default_opsin_inv, sizeof(meta->opsin_inv));
    for (i = 0; i < 3; i++) meta->opsin_bias[i] = -0.0037930732552754493f;
    meta->quant_bias[0] = 1.0f - 0.05465007330715401f;
    meta->quant_bias[1] = 1.0f - 0.07005449891748593f;
    meta->quant_bias[2] = 1.0f - 0.049935103337343655f;
    meta->quant_bias_numerator = 0.145f;
    memcpy(meta->up2, jxl_default_up2, sizeof(meta->up2));
    memcpy(meta->up4, jxl_default_up4, sizeof(meta->up4));
    memcpy(meta->up8, jxl_default_up8, sizeof(meta->up8));

    if (jxl_br_bool(br)) return;

    if (meta->xyb_encoded) {

        if (!jxl_br_bool(br)) {
            for (i = 0; i < 9; i++) meta->opsin_inv[i] = jxl_br_f16(br);
            for (i = 0; i < 3; i++) meta->opsin_bias[i] = jxl_br_f16(br);
            for (i = 0; i < 3; i++) meta->quant_bias[i] = jxl_br_f16(br);
            meta->quant_bias_numerator = jxl_br_f16(br);
        }
    }
    cw_mask = jxl_br_read(br, 3);
    if (cw_mask & 1) for (i = 0; i < 15; i++) meta->up2[i] = jxl_br_f16(br);
    if (cw_mask & 2) for (i = 0; i < 55; i++) meta->up4[i] = jxl_br_f16(br);
    if (cw_mask & 4) for (i = 0; i < 210; i++) meta->up8[i] = jxl_br_f16(br);
}

static void image_metadata_defaults(jxl_image_metadata *meta) {
    memset(meta, 0, sizeof(*meta));
    meta->orientation = 1;
    bit_depth_default(&meta->bit_depth);
    meta->modular_16bit_buffers = 1;
    meta->num_extra = 0;
    meta->alpha_index = -1;
    meta->xyb_encoded = 1;
    colour_encoding_default(&meta->colour);
    meta->tone_mapping.intensity_target = 255.0f;
    meta->tone_mapping.min_nits = 0.0f;
    meta->tone_mapping.relative_to_max_display = 0;
    meta->tone_mapping.linear_below = 0.0f;
}

static int image_metadata_read(jxl_ctx *ctx, jxl_br *br,
                               jxl_image_metadata *meta) {
    int all_default, extra_fields = 0;
    uint32_t i;

    image_metadata_defaults(meta);
    all_default = jxl_br_bool(br);
    if (!all_default) {
        extra_fields = jxl_br_bool(br);
        if (extra_fields) {
            meta->orientation = jxl_br_read(br, 3) + 1;
            meta->have_intr_size = jxl_br_bool(br);
            if (meta->have_intr_size) size_header_read(br, &meta->intrinsic);
            meta->have_preview = jxl_br_bool(br);
            if (meta->have_preview) preview_header_read(br, &meta->preview);
            meta->have_animation = jxl_br_bool(br);
            if (meta->have_animation) {
                meta->animation.tps_numerator =
                    jxl_br_u32(br, 100, 0, 1000, 0, 1, 10, 1, 30);
                meta->animation.tps_denominator =
                    jxl_br_u32(br, 1, 0, 1001, 0, 1, 8, 1, 10);
                meta->animation.num_loops =
                    jxl_br_u32(br, 0, 0, 0, 3, 0, 16, 0, 32);
                meta->animation.have_timecodes = jxl_br_bool(br);
            }
        }
        read_bit_depth(br, &meta->bit_depth);
        meta->modular_16bit_buffers = jxl_br_bool(br);

        uint32_t n_extra = jxl_br_u32(br, 0, 0, 1, 0, 2, 4, 1, 12);
        if (n_extra > 256) {
            JXL_ERR(ctx, "header: too many extra channels (%u)",
                    (unsigned)n_extra);
            return -1;
        }
        if (n_extra) {
            meta->ec_info = (jxl_ec_info *)jxl_calloc(ctx, n_extra,
                                                      sizeof(jxl_ec_info));
            if (!meta->ec_info) return -1;
            meta->num_extra = n_extra;
            for (i = 0; i < meta->num_extra; i++) {
                ec_info_read(ctx, br, &meta->ec_info[i]);
                if (br->err) return -1;
            }
        }
        meta->xyb_encoded = jxl_br_bool(br);
        colour_encoding_read(ctx, br, &meta->colour);
        if (extra_fields) {
            if (!jxl_br_bool(br)) {
                meta->tone_mapping.intensity_target = jxl_br_f16(br);
                meta->tone_mapping.min_nits = jxl_br_f16(br);
                meta->tone_mapping.relative_to_max_display = jxl_br_bool(br);
                meta->tone_mapping.linear_below = jxl_br_f16(br);
            }
        }
        read_extensions(br);
    }

    custom_transform_data_read(br, meta);

    for (i = 0; i < meta->num_extra; i++) {
        if (meta->ec_info[i].type == JXL_EC_ALPHA) {
            meta->alpha_index = (int)i;
            break;
        }
    }

    if (br->err) {
        JXL_ERR(ctx, "header: truncated image metadata");
        return -1;
    }
    if (!(meta->tone_mapping.intensity_target > 0.0f)) {
        JXL_ERR(ctx, "header: invalid intensity target");
        return -1;
    }
    return 0;
}

int jxl_read_image_header(jxl_ctx *ctx, jxl_br *br, jxl_size_header *size,
                          jxl_image_metadata *meta) {
    uint32_t sig = jxl_br_read(br, 16);
    if (sig != 0x0aff) {
        JXL_ERR(ctx, "codestream: bad signature 0x%04x", (unsigned)sig);
        return -1;
    }
    size_header_read(br, size);
    if (br->err || size->width == 0 || size->height == 0) {
        JXL_ERR(ctx, "codestream: bad image size");
        return -1;
    }
    return image_metadata_read(ctx, br, meta);
}

void jxl_image_metadata_free(jxl_ctx *ctx, jxl_image_metadata *meta) {
    uint32_t i;
    if (!meta) return;

    if (!meta->ec_info) meta->num_extra = 0;
    for (i = 0; i < meta->num_extra; i++) jxl_free(ctx, meta->ec_info[i].name);
    jxl_free(ctx, meta->ec_info);
    meta->ec_info = NULL;
    meta->num_extra = 0;
}

void jxl_apply_orientation_dims(uint32_t orientation, uint32_t w, uint32_t h,
                                uint32_t *ow, uint32_t *oh) {
    if (orientation >= 5 && orientation <= 8) {
        *ow = h;
        *oh = w;
    } else {
        *ow = w;
        *oh = h;
    }
}

uint32_t jxl_bitlen(uint32_t x) {
    uint32_t n = 0;
    while (x) {
        n++;
        x >>= 1;
    }
    return n;
}

#define PFX_MAX_BITS 15
#define PFX_ROOT_BITS 8

static uint32_t pfx_entry(uint32_t sym, uint32_t len, uint32_t nested) {
    return sym | (len << 16) | (nested << 24);
}

static uint32_t reverse_bits(uint32_t v, int n) {
    uint32_t r = 0;
    int i;
    for (i = 0; i < n; i++) {
        r = (r << 1) | ((v >> i) & 1);
    }
    return r;
}

static void pfx_free(jxl_ctx *ctx, jxl_pfx_hist *h) {
    jxl_free(ctx, h->root);
    jxl_free(ctx, h->sub);
    h->root = NULL;
    h->sub = NULL;
}

static void pfx_single_into(jxl_pfx_hist *h, uint16_t sym) {
    h->root_bits = 0;
    h->root_mask = 0;
    h->single_symbol = (int)sym;
    h->root[0].bits = pfx_entry(sym, 0, 0);
    h->nsub = 0;
    h->sub = NULL;
}

static int pfx_single(jxl_ctx *ctx, jxl_pfx_hist *h, uint16_t sym) {
    h->root = (jxl_pfx_entry *)jxl_calloc(ctx, 1, sizeof(jxl_pfx_entry));
    if (!h->root) return -1;
    pfx_single_into(h, sym);
    return 0;
}

static int pfx_build(jxl_ctx *ctx, jxl_pfx_hist *h, const uint8_t *lens,
                     uint32_t n) {
    uint32_t count[PFX_MAX_BITS + 1];
    uint32_t next_code[PFX_MAX_BITS + 2];
    uint32_t sub_off[1 << PFX_ROOT_BITS];
    uint8_t sub_maxlen[1 << PFX_ROOT_BITS];
    uint32_t total = 0, sub_total = 0;
    uint32_t code, i, sym;
    int len, max_len = 0, root_bits;
    uint32_t root_size, root_mask;

    memset(count, 0, sizeof(count));
    for (sym = 0; sym < n; sym++) {
        if (lens[sym] > PFX_MAX_BITS) return -1;
        if (lens[sym]) {
            count[lens[sym]]++;
            if (lens[sym] > max_len) max_len = lens[sym];
        }
    }
    if (max_len == 0) return -1;

    for (len = 1; len <= max_len; len++) total += count[len] << (PFX_MAX_BITS - len);
    if (total != (1u << PFX_MAX_BITS)) return -1;

    code = 0;
    for (len = 1; len <= max_len; len++) {
        next_code[len] = code;
        code = (code + count[len]) << 1;
    }

    root_bits = max_len < PFX_ROOT_BITS ? max_len : PFX_ROOT_BITS;
    root_size = 1u << root_bits;
    root_mask = root_size - 1;

    memset(sub_maxlen, 0, sizeof(sub_maxlen));
    {
        uint32_t nc[PFX_MAX_BITS + 2];
        memcpy(nc, next_code, sizeof(nc));
        for (len = 1; len <= max_len; len++) {
            for (sym = 0; sym < n; sym++) {
                uint32_t rev, prefix;
                if (lens[sym] != len) continue;
                rev = reverse_bits(nc[len]++, len);
                if (len <= root_bits) continue;
                prefix = rev & root_mask;
                if ((uint32_t)len > sub_maxlen[prefix]) sub_maxlen[prefix] = (uint8_t)len;
            }
        }
    }
    for (i = 0; i < root_size; i++) {
        sub_off[i] = sub_total;
        if (sub_maxlen[i]) sub_total += 1u << (sub_maxlen[i] - root_bits);
    }

    h->root = (jxl_pfx_entry *)jxl_calloc(ctx, root_size, sizeof(jxl_pfx_entry));
    if (!h->root) return -1;
    if (sub_total) {
        h->sub = (jxl_pfx_entry *)jxl_calloc(ctx, sub_total, sizeof(jxl_pfx_entry));
        if (!h->sub) return -1;
    }
    h->nsub = sub_total;
    h->root_bits = root_bits;
    h->root_mask = root_mask;
    h->single_symbol = -1;

    for (i = 0; i < root_size; i++) {
        if (!sub_maxlen[i]) continue;
        h->root[i].bits = pfx_entry(
            sub_off[i], (1u << (sub_maxlen[i] - root_bits)) - 1, 1);
    }

    for (len = 1; len <= max_len; len++) {
        for (sym = 0; sym < n; sym++) {
            uint32_t rev, k, step;
            if (lens[sym] != len) continue;
            rev = reverse_bits(next_code[len]++, len);
            if (len <= root_bits) {
                step = 1u << len;
                for (k = rev; k < root_size; k += step) {
                    h->root[k].bits = pfx_entry(sym, (uint32_t)len, 0);
                }
            } else {
                uint32_t prefix = rev & root_mask;
                uint32_t hi = rev >> root_bits;
                uint32_t sub_size = 1u << (sub_maxlen[prefix] - root_bits);
                step = 1u << (len - root_bits);
                for (k = hi; k < sub_size; k += step) {
                    jxl_pfx_entry *e = &h->sub[sub_off[prefix] + k];
                    e->bits = pfx_entry(sym, (uint32_t)len, 0);
                }
            }
        }
    }
    return 0;
}

static uint32_t pfx_read(const jxl_pfx_hist *h, jxl_br *br) {
    uint32_t peeked = jxl_br_peek(br, PFX_MAX_BITS);
    uint32_t e = h->root[peeked & h->root_mask].bits;
    if (e >> 24) {
        uint32_t sym = e & 0xffff;
        uint32_t mask = (e >> 16) & 0xff;
        e = h->sub[sym + ((peeked >> h->root_bits) & mask)].bits;
    }
    jxl_br_consume(br, (int)((e >> 16) & 0xff));
    return e & 0xffff;
}

static int pfx_parse_simple(jxl_ctx *ctx, jxl_br *br, jxl_pfx_hist *h,
                            uint32_t alphabet_size) {
    int alphabet_bits = (int)jxl_bitlen(alphabet_size - 1);
    uint32_t nsym = jxl_br_read(br, 2) + 1;
    uint32_t syms[4];
    uint8_t code_len[4];
    uint8_t *lens;
    uint32_t i;
    int rc;

    if (nsym == 1) {
        uint32_t s = jxl_br_read(br, alphabet_bits);
        if (s >= alphabet_size) return -1;
        return pfx_single(ctx, h, (uint16_t)s);
    }
    for (i = 0; i < nsym; i++) syms[i] = jxl_br_read(br, alphabet_bits);
    if (nsym == 2) {
        code_len[0] = 1; code_len[1] = 1;
    } else if (nsym == 3) {
        code_len[0] = 1; code_len[1] = 2; code_len[2] = 2;
    } else {
        int tree_selector = jxl_br_bool(br);
        if (tree_selector) {
            code_len[0] = 1; code_len[1] = 2; code_len[2] = 3; code_len[3] = 3;
        } else {
            code_len[0] = 2; code_len[1] = 2; code_len[2] = 2; code_len[3] = 2;
        }
    }
    lens = (uint8_t *)jxl_calloc(ctx, alphabet_size, 1);
    if (!lens) return -1;
    for (i = 0; i < nsym; i++) {
        if (syms[i] >= alphabet_size) {
            jxl_free(ctx, lens);
            return -1;
        }
        lens[syms[i]] = code_len[i];
    }
    rc = pfx_build(ctx, h, lens, alphabet_size);
    jxl_free(ctx, lens);
    return rc;
}

static const uint8_t pfx_code_length_order[18] = {
    1, 2, 3, 4, 0, 5, 17, 6, 16, 7, 8, 9, 10, 11, 12, 13, 14, 15
};

static int pfx_parse_complex(jxl_ctx *ctx, jxl_br *br, jxl_pfx_hist *h,
                             uint32_t alphabet_size, uint32_t hskip) {
    uint8_t clcl[18];
    jxl_pfx_entry cl_root[1 << PFX_ROOT_BITS];
    jxl_pfx_hist clh;
    uint8_t *lens = NULL;
    uint32_t bitacc = 0;
    int nonzero_count = 0, nonzero_sym = 0;
    uint32_t i;
    uint32_t repeat_count = 0;
    uint8_t repeat_sym = 0, prev_sym = 8, last_nonzero_sym = 8;
    uint32_t last_repeat_count = 0;
    int rc = -1;

    memset(clcl, 0, sizeof(clcl));
    memset(&clh, 0, sizeof(clh));
    clh.root = cl_root;

    for (i = hskip; i < 18; i++) {
        uint32_t idx = pfx_code_length_order[i];
        uint32_t base = jxl_br_u32(br, 0, 0, 4, 0, 3, 0, 8, 0);
        uint32_t len;
        if (base == 8) {
            if (jxl_br_bool(br)) {
                len = jxl_br_bool(br) ? 5 : 1;
            } else {
                len = 2;
            }
        } else {
            len = base;
        }
        clcl[idx] = (uint8_t)len;
        if (len != 0) {
            nonzero_count++;
            nonzero_sym = (int)idx;
            bitacc += 32u >> len;
            if (bitacc == 32) break;
            if (bitacc > 32) return -1;
        }
    }

    if (nonzero_count == 1) {
        pfx_single_into(&clh, (uint16_t)nonzero_sym);
    } else if (bitacc != 32) {
        return -1;
    } else {

        uint32_t count[PFX_MAX_BITS + 1];
        int len, max_len = 0;
        uint32_t next_code[PFX_MAX_BITS + 2], code = 0, sym, k, step;
        memset(count, 0, sizeof(count));
        for (sym = 0; sym < 18; sym++) {
            if (clcl[sym]) {
                count[clcl[sym]]++;
                if (clcl[sym] > max_len) max_len = clcl[sym];
            }
        }
        for (len = 1; len <= max_len; len++) {
            next_code[len] = code;
            code = (code + count[len]) << 1;
        }
        clh.root_bits = max_len;
        clh.root_mask = (1u << max_len) - 1;
        clh.single_symbol = -1;
        clh.nsub = 0;
        clh.sub = NULL;
        memset(cl_root, 0, sizeof(jxl_pfx_entry) * ((size_t)1 << max_len));
        for (len = 1; len <= max_len; len++) {
            for (sym = 0; sym < 18; sym++) {
                uint32_t rev;
                if (clcl[sym] != len) continue;
                rev = reverse_bits(next_code[len]++, len);
                step = 1u << len;
                for (k = rev; k < (1u << max_len); k += step) {
                    cl_root[k].bits = pfx_entry(sym, (uint32_t)len, 0);
                }
            }
        }
    }

    lens = (uint8_t *)jxl_calloc(ctx, alphabet_size, 1);
    if (!lens) return -1;

    bitacc = 0;
    for (i = 0; i < alphabet_size; i++) {
        if (repeat_count > 0) {
            lens[i] = repeat_sym;
            repeat_count--;
        } else {
            uint32_t sym = pfx_read(&clh, br);
            if (br->err) goto done;
            if (sym == 0) {

            } else if (sym <= 15) {
                lens[i] = (uint8_t)sym;
                last_nonzero_sym = (uint8_t)sym;
            } else if (sym == 16) {
                repeat_count = jxl_br_read(br, 2) + 3;
                if (prev_sym == 16) {
                    repeat_count += last_repeat_count * 3 - 8;
                    last_repeat_count += repeat_count;
                } else {
                    last_repeat_count = repeat_count;
                }
                repeat_sym = last_nonzero_sym;
                lens[i] = repeat_sym;
                repeat_count--;
            } else {
                repeat_count = jxl_br_read(br, 3) + 3;
                if (prev_sym == 17) {
                    repeat_count += last_repeat_count * 7 - 16;
                    last_repeat_count += repeat_count;
                } else {
                    last_repeat_count = repeat_count;
                }
                repeat_sym = 0;
                lens[i] = repeat_sym;
                repeat_count--;
            }
            prev_sym = (uint8_t)sym;
        }
        if (lens[i] != 0) {
            uint32_t shift = lens[i] >= PFX_MAX_BITS ? 0 : (PFX_MAX_BITS - lens[i]);
            bitacc += 1u << shift;
            if (bitacc > (1u << PFX_MAX_BITS)) goto done;
            if (bitacc == (1u << PFX_MAX_BITS) && repeat_count == 0) break;
        }
    }
    if (bitacc != (1u << PFX_MAX_BITS) || repeat_count > 0) goto done;
    rc = pfx_build(ctx, h, lens, alphabet_size);

done:
    jxl_free(ctx, lens);
    return rc;
}

static int pfx_parse(jxl_ctx *ctx, jxl_br *br, jxl_pfx_hist *h,
                     uint32_t alphabet_size) {
    uint32_t hskip;

    h->root = NULL;
    h->sub = NULL;
    h->nsub = 0;
    h->single_symbol = -1;

    if (alphabet_size == 1) return pfx_single(ctx, h, 0);
    if (alphabet_size > (1u << PFX_MAX_BITS)) return -1;

    hskip = jxl_br_read(br, 2);
    if (hskip == 1) return pfx_parse_simple(ctx, br, h, alphabet_size);
    return pfx_parse_complex(ctx, br, h, alphabet_size, hskip);
}

#define ANS_LOG_TAB_SIZE 12
#define ANS_TAB_SIZE (1 << ANS_LOG_TAB_SIZE)

#define ANS_SIGNATURE 0x130000

static uint32_t ans_read_u8(jxl_br *br) {
    if (jxl_br_bool(br)) {
        int n = (int)jxl_br_read(br, 3);
        return (1u << n) + jxl_br_read(br, n);
    }
    return 0;
}

static uint16_t ans_read_prefix(jxl_br *br) {
    switch (jxl_br_read(br, 3)) {
        case 0: return 10;
        case 1: {
            static const uint16_t vals[4] = {4, 0, 11, 13};
            int i;
            for (i = 0; i < 4; i++) {
                if (jxl_br_bool(br)) return vals[i];
            }
            return 12;
        }
        case 2: return 7;
        case 3: return jxl_br_bool(br) ? 1 : 3;
        case 4: return 6;
        case 5: return 8;
        case 6: return 9;
        default: return jxl_br_bool(br) ? 2 : 5;
    }
}

static void ans_free(jxl_ctx *ctx, jxl_ans_hist *h) {
    jxl_free(ctx, h->buckets);
    h->buckets = NULL;
}

static int ans_parse(jxl_ctx *ctx, jxl_br *br, jxl_ans_hist *h,
                     uint32_t log_alphabet_size) {
    uint32_t table_size = 1u << log_alphabet_size;
    uint32_t log_bucket_size = 12 - log_alphabet_size;
    uint32_t bucket_size = 1u << log_bucket_size;
    uint16_t *dist = NULL;
    uint32_t alphabet_size = 0;
    uint32_t i;
    int rc = -1;
    int32_t single_sym = -1;

    uint16_t *cutoff = NULL, *alias_sym = NULL, *alias_off = NULL;
    uint32_t *underfull = NULL, *overfull = NULL;
    uint32_t nunder = 0, nover = 0;

    h->buckets = NULL;
    dist = (uint16_t *)jxl_calloc(ctx, table_size, sizeof(uint16_t));
    if (!dist) return -1;

    if (jxl_br_bool(br)) {
        if (jxl_br_bool(br)) {

            uint32_t v0 = ans_read_u8(br);
            uint32_t v1 = ans_read_u8(br);
            uint32_t prob;
            if (v0 == v1) goto done;
            alphabet_size = (v0 > v1 ? v0 : v1) + 1;
            if (alphabet_size > table_size) goto done;
            prob = jxl_br_read(br, 12);
            dist[v0] = (uint16_t)prob;
            dist[v1] = (uint16_t)(ANS_TAB_SIZE - prob);
        } else {

            uint32_t val = ans_read_u8(br);
            alphabet_size = val + 1;
            if (alphabet_size > table_size) goto done;
            dist[val] = ANS_TAB_SIZE;
        }
    } else if (jxl_br_bool(br)) {

        uint32_t base, leftover;
        alphabet_size = ans_read_u8(br) + 1;
        if (alphabet_size > table_size) goto done;
        base = ANS_TAB_SIZE / alphabet_size;
        leftover = ANS_TAB_SIZE % alphabet_size;
        for (i = 0; i < leftover; i++) dist[i] = (uint16_t)(base + 1);
        for (; i < alphabet_size; i++) dist[i] = (uint16_t)base;
    } else {

        int len = 0;
        int16_t shift;
        uint32_t idx = 0, acc = 0;
        uint16_t prev_dist = 0;
        int32_t omit_log = -1, omit_pos = -1;
        uint32_t *rep_start = NULL, *rep_end = NULL;
        uint32_t nrep = 0, rep_cap = 0, rep_idx = 0;

        while (len < 3 && jxl_br_bool(br)) len++;
        shift = (int16_t)(jxl_br_read(br, len) + (1u << len) - 1);
        if (shift > 13) goto done;
        alphabet_size = ans_read_u8(br) + 3;
        if (alphabet_size > table_size) goto done;

        while (idx < alphabet_size) {
            dist[idx] = ans_read_prefix(br);
            if (br->err) { jxl_free(ctx, rep_start); jxl_free(ctx, rep_end); goto done; }
            if (dist[idx] == 13) {
                uint32_t repeat_count = ans_read_u8(br) + 4;
                if (idx + repeat_count > alphabet_size) {
                    jxl_free(ctx, rep_start);
                    jxl_free(ctx, rep_end);
                    goto done;
                }
                if (nrep == rep_cap) {
                    uint32_t ncap = rep_cap ? rep_cap * 2 : 8;
                    uint32_t *a = (uint32_t *)jxl_realloc_array(ctx, rep_start, rep_cap, ncap, sizeof(uint32_t));
                    uint32_t *b = (uint32_t *)jxl_realloc_array(ctx, rep_end, rep_cap, ncap, sizeof(uint32_t));
                    if (!a || !b) { jxl_free(ctx, a); jxl_free(ctx, b); goto done; }
                    rep_start = a;
                    rep_end = b;
                    rep_cap = ncap;
                }
                rep_start[nrep] = idx;
                rep_end[nrep] = idx + repeat_count;
                nrep++;
                idx += repeat_count;
                continue;
            }
            if (omit_pos < 0 || (int32_t)dist[idx] > omit_log) {
                omit_log = dist[idx];
                omit_pos = (int32_t)idx;
            }
            idx++;
        }
        if (omit_pos < 0 ||
            ((uint32_t)omit_pos + 1 < table_size && dist[omit_pos + 1] == 13)) {
            jxl_free(ctx, rep_start);
            jxl_free(ctx, rep_end);
            goto done;
        }

        for (i = 0; i < table_size; i++) {
            uint16_t code = dist[i];
            if (rep_idx < nrep && rep_start[rep_idx] <= i) {
                if (rep_end[rep_idx] == i) {
                    rep_idx++;
                } else {
                    dist[i] = prev_dist;
                    acc += dist[i];
                    if (acc > ANS_TAB_SIZE) break;
                    continue;
                }
                code = dist[i];
            }
            if (code == 0) { prev_dist = 0; continue; }
            if ((int32_t)i == omit_pos) { prev_dist = 0; continue; }
            if (code > 1) {
                int16_t zeros = (int16_t)(code - 1);
                int16_t bitcount = (int16_t)(shift - ((12 - zeros) >> 1));
                if (bitcount < 0) bitcount = 0;
                if (bitcount > zeros) bitcount = zeros;
                code = (uint16_t)((1u << zeros) +
                                  (jxl_br_read(br, bitcount) << (zeros - bitcount)));
                dist[i] = code;
            }
            prev_dist = code;
            acc += code;
            if (acc > ANS_TAB_SIZE) break;
        }
        jxl_free(ctx, rep_start);
        jxl_free(ctx, rep_end);
        if (acc > ANS_TAB_SIZE) goto done;
        dist[omit_pos] = (uint16_t)(ANS_TAB_SIZE - acc);
    }
    if (br->err) goto done;

    h->log_bucket_size = log_bucket_size;
    h->bucket_mask = bucket_size - 1;
    h->buckets = (jxl_ans_bucket *)jxl_calloc(ctx, table_size,
                                              sizeof(jxl_ans_bucket));
    if (!h->buckets) goto done;

    for (i = 0; i < table_size; i++) {
        if (dist[i] == ANS_TAB_SIZE) { single_sym = (int32_t)i; break; }
    }
    if (single_sym >= 0) {

        for (i = 0; i < table_size; i++) {
            h->buckets[i].dist = dist[i];
            h->buckets[i].alias_symbol = (uint8_t)single_sym;
            h->buckets[i].alias_offset = (uint16_t)(bucket_size * i);
            h->buckets[i].alias_cutoff = 0;
            h->buckets[i].alias_dist_xor = (uint16_t)(dist[i] ^ ANS_TAB_SIZE);
        }
        h->single_symbol = single_sym;
        rc = 0;
        goto done;
    }
    h->single_symbol = -1;

    cutoff = (uint16_t *)jxl_calloc(ctx, table_size, sizeof(uint16_t));
    alias_sym = (uint16_t *)jxl_calloc(ctx, table_size, sizeof(uint16_t));
    alias_off = (uint16_t *)jxl_calloc(ctx, table_size, sizeof(uint16_t));
    underfull = (uint32_t *)jxl_calloc(ctx, table_size, sizeof(uint32_t));
    overfull = (uint32_t *)jxl_calloc(ctx, table_size, sizeof(uint32_t));
    if (!cutoff || !alias_sym || !alias_off || !underfull || !overfull) goto done;

    for (i = 0; i < table_size; i++) {
        cutoff[i] = dist[i];
        alias_sym[i] = (uint16_t)(i < alphabet_size ? i : 0);
        alias_off[i] = 0;
        if (dist[i] < bucket_size) underfull[nunder++] = i;
        else if (dist[i] > bucket_size) overfull[nover++] = i;
    }
    while (nover > 0 && nunder > 0) {
        uint32_t o = overfull[--nover];
        uint32_t u = underfull[--nunder];
        uint16_t by = (uint16_t)(bucket_size - cutoff[u]);
        cutoff[o] = (uint16_t)(cutoff[o] - by);
        alias_sym[u] = (uint16_t)o;
        alias_off[u] = cutoff[o];
        if (cutoff[o] < bucket_size) underfull[nunder++] = o;
        else if (cutoff[o] > bucket_size) overfull[nover++] = o;
    }

    for (i = 0; i < table_size; i++) {
        h->buckets[i].dist = dist[i];
        if (cutoff[i] == bucket_size) {
            h->buckets[i].alias_symbol = (uint8_t)i;
            h->buckets[i].alias_offset = 0;
            h->buckets[i].alias_cutoff = 0;
            h->buckets[i].alias_dist_xor = 0;
        } else {
            h->buckets[i].alias_symbol = (uint8_t)alias_sym[i];
            h->buckets[i].alias_offset = (uint16_t)(alias_off[i] - cutoff[i]);
            h->buckets[i].alias_cutoff = (uint8_t)cutoff[i];
            h->buckets[i].alias_dist_xor =
                (uint16_t)(dist[i] ^ dist[alias_sym[i]]);
        }
    }
    rc = 0;

done:
    jxl_free(ctx, dist);
    jxl_free(ctx, cutoff);
    jxl_free(ctx, alias_sym);
    jxl_free(ctx, alias_off);
    jxl_free(ctx, underfull);
    jxl_free(ctx, overfull);
    return rc;
}

static uint32_t ans_read_symbol(const jxl_ans_hist *h, jxl_br *br,
                                uint32_t *state) {
    uint32_t idx = *state & 0xfff;
    uint32_t i = idx >> h->log_bucket_size;
    uint32_t pos = idx & h->bucket_mask;
    const jxl_ans_bucket *b = &h->buckets[i];
    int map_to_alias = pos >= b->alias_cutoff;
    uint32_t symbol = map_to_alias ? b->alias_symbol : i;
    uint32_t offset = (map_to_alias ? b->alias_offset : 0) + pos;
    uint32_t dist = b->dist ^ (map_to_alias ? b->alias_dist_xor : 0);
    uint32_t next_state = (*state >> 12) * dist + offset;

    if (next_state < (1u << 16)) {
        next_state = (next_state << 16) | jxl_br_peek(br, 16);
        jxl_br_consume(br, 16);
    }
    *state = next_state;
    return symbol;
}

static int int_config_parse(jxl_br *br, jxl_int_config *cfg,
                            uint32_t log_alphabet_size) {
    uint32_t split_exponent_bits = jxl_bitlen(log_alphabet_size);
    uint32_t split_exponent = jxl_br_read(br, (int)split_exponent_bits);
    uint32_t msb = 0, lsb = 0;

    if (split_exponent != log_alphabet_size) {
        uint32_t msb_bits = jxl_bitlen(split_exponent);
        msb = jxl_br_read(br, (int)msb_bits);
        if (msb > split_exponent) return -1;
        lsb = jxl_br_read(br, (int)jxl_bitlen(split_exponent - msb));
    }
    if (lsb + msb > split_exponent) return -1;
    cfg->split_exponent = split_exponent;
    cfg->split = 1u << split_exponent;
    cfg->msb_in_token = msb;
    cfg->lsb_in_token = lsb;
    return 0;
}

static uint32_t read_uint(jxl_br *br, const jxl_int_config *cfg,
                          uint32_t token) {
    uint32_t n, low_bits, msb, lsb;
    uint64_t rest, result;

    if (token < cfg->split) return token;
    msb = cfg->msb_in_token;
    lsb = cfg->lsb_in_token;
    n = (cfg->split_exponent - (msb + lsb) + ((token - cfg->split) >> (msb + lsb))) & 31;
    rest = jxl_br_read(br, (int)n);
    low_bits = token & ((1u << lsb) - 1);
    token >>= lsb;
    token &= (1u << msb) - 1;
    token |= 1u << msb;
    result = (((uint64_t)token << n) | rest) << lsb | low_bits;
    return (uint32_t)result;
}

static JXL_INLINE_HINT uint32_t read_uint_00(jxl_br *br,
                                             const jxl_int_config *cfg,
                                             uint32_t token) {
    uint32_t n;
    if (token < cfg->split) return token;
    n = (cfg->split_exponent + token - cfg->split) & 31;
    return (1u << n) | jxl_br_read(br, (int)n);
}

static const int8_t lz77_special_distances[120][2] = {
    {0,1},{1,0},{1,1},{-1,1},{0,2},{2,0},{1,2},{-1,2},{2,1},{-2,1},
    {2,2},{-2,2},{0,3},{3,0},{1,3},{-1,3},{3,1},{-3,1},{2,3},{-2,3},
    {3,2},{-3,2},{0,4},{4,0},{1,4},{-1,4},{4,1},{-4,1},{3,3},{-3,3},
    {2,4},{-2,4},{4,2},{-4,2},{0,5},{3,4},{-3,4},{4,3},{-4,3},{5,0},
    {1,5},{-1,5},{5,1},{-5,1},{2,5},{-2,5},{5,2},{-5,2},{4,4},{-4,4},
    {3,5},{-3,5},{5,3},{-5,3},{0,6},{6,0},{1,6},{-1,6},{6,1},{-6,1},
    {2,6},{-2,6},{6,2},{-6,2},{4,5},{-4,5},{5,4},{-5,4},{3,6},{-3,6},
    {6,3},{-6,3},{0,7},{7,0},{1,7},{-1,7},{5,5},{-5,5},{7,1},{-7,1},
    {4,6},{-4,6},{6,4},{-6,4},{2,7},{-2,7},{7,2},{-7,2},{3,7},{-3,7},
    {7,3},{-7,3},{5,6},{-5,6},{6,5},{-6,5},{8,0},{4,7},{-4,7},{7,4},
    {-7,4},{8,1},{8,2},{6,6},{-6,6},{8,3},{5,7},{-5,7},{7,5},{-7,5},
    {8,4},{6,7},{-6,7},{7,6},{-7,6},{8,5},{7,7},{-7,7},{8,6},{8,7}
};

#define LZ77_WINDOW_SIZE (1u << 20)
#define LZ77_WINDOW_MASK (LZ77_WINDOW_SIZE - 1)

void jxl_dec_free(jxl_dec *dec) {
    uint32_t i;
    if (!dec || !dec->ctx) return;
    if (dec->pfx) {
        for (i = 0; i < dec->num_clusters; i++) pfx_free(dec->ctx, &dec->pfx[i]);
        jxl_free(dec->ctx, dec->pfx);
    }
    if (dec->ans) {
        for (i = 0; i < dec->num_clusters; i++) ans_free(dec->ctx, &dec->ans[i]);
        jxl_free(dec->ctx, dec->ans);
    }
    jxl_free(dec->ctx, dec->configs);
    jxl_free(dec->ctx, dec->clusters);
    jxl_free(dec->ctx, dec->window);
    memset(dec, 0, sizeof(*dec));
}

static int dec_parse_inner(jxl_ctx *ctx, jxl_dec *dec, jxl_br *br,
                           uint32_t num_dist);

int jxl_read_clusters(jxl_ctx *ctx, jxl_br *br, uint32_t num_dist,
                      uint8_t *clusters, uint32_t *num_clusters_out) {
    uint32_t i;
    uint32_t num_clusters = 0;
    uint8_t seen[256];

    if (num_dist == 1) {
        clusters[0] = 0;
        *num_clusters_out = 1;
        return 0;
    }
    if (jxl_br_bool(br)) {
        int nbits = (int)jxl_br_read(br, 2);
        for (i = 0; i < num_dist; i++) clusters[i] = (uint8_t)jxl_br_read(br, nbits);
    } else {
        int use_mtf = jxl_br_bool(br);
        jxl_dec sub;
        memset(&sub, 0, sizeof(sub));
        if (num_dist <= 2) {
            if (jxl_br_bool(br)) {
                JXL_ERR(ctx, "context map: LZ77 not allowed");
                return -1;
            }
            if (dec_parse_inner(ctx, &sub, br, 1) != 0) return -1;
        } else {
            if (jxl_dec_init(ctx, &sub, br, 1) != 0) return -1;
        }
        jxl_dec_begin(&sub, br);
        for (i = 0; i < num_dist; i++) {
            uint32_t v = jxl_dec_read(&sub, br, 0);
            if (v > 255 || br->err) {
                JXL_ERR(ctx, "context map: invalid cluster %u", (unsigned)v);
                jxl_dec_free(&sub);
                return -1;
            }
            clusters[i] = (uint8_t)v;
        }
        if (jxl_dec_finalize(&sub) != 0) {
            JXL_ERR(ctx, "context map: bad ANS final state");
            jxl_dec_free(&sub);
            return -1;
        }
        jxl_dec_free(&sub);
        if (use_mtf) {
            uint8_t mtf[256];
            for (i = 0; i < 256; i++) mtf[i] = (uint8_t)i;
            for (i = 0; i < num_dist; i++) {
                uint32_t idx = clusters[i];
                uint8_t v = mtf[idx];
                clusters[i] = v;
                memmove(mtf + 1, mtf, idx);
                mtf[0] = v;
            }
        }
    }

    memset(seen, 0, sizeof(seen));
    for (i = 0; i < num_dist; i++) {
        if (clusters[i] + 1u > num_clusters) num_clusters = clusters[i] + 1u;
        seen[clusters[i]] = 1;
    }
    for (i = 0; i < num_clusters; i++) {
        if (!seen[i]) {
            JXL_ERR(ctx, "context map has a hole at cluster %u", (unsigned)i);
            return -1;
        }
    }
    *num_clusters_out = num_clusters;
    return 0;
}

static int dec_parse_inner(jxl_ctx *ctx, jxl_dec *dec, jxl_br *br,
                           uint32_t num_dist) {
    uint32_t log_alphabet_size;
    uint32_t i;

    dec->ctx = ctx;
    dec->num_dist = num_dist;
    dec->clusters = (uint8_t *)jxl_calloc(ctx, num_dist, 1);
    if (!dec->clusters) return -1;
    if (jxl_read_clusters(ctx, br, num_dist, dec->clusters, &dec->num_clusters) != 0)
        return -1;

    dec->use_prefix = jxl_br_bool(br);
    log_alphabet_size = dec->use_prefix ? 15 : jxl_br_read(br, 2) + 5;

    dec->configs = (jxl_int_config *)jxl_calloc(ctx, dec->num_clusters,
                                                sizeof(jxl_int_config));
    if (!dec->configs) return -1;
    for (i = 0; i < dec->num_clusters; i++) {
        if (int_config_parse(br, &dec->configs[i], log_alphabet_size) != 0) {
            JXL_ERR(ctx, "invalid hybrid uint config");
            return -1;
        }
    }

    if (dec->use_prefix) {
        uint32_t *counts = (uint32_t *)jxl_calloc(ctx, dec->num_clusters,
                                                  sizeof(uint32_t));
        if (!counts) return -1;
        for (i = 0; i < dec->num_clusters; i++) {
            uint32_t count = 1;
            if (jxl_br_bool(br)) {
                int n = (int)jxl_br_read(br, 4);
                count = 1 + (1u << n) + jxl_br_read(br, n);
            }
            if (count > (1u << 15)) {
                jxl_free(ctx, counts);
                JXL_ERR(ctx, "prefix alphabet too large");
                return -1;
            }
            counts[i] = count;
        }
        dec->pfx = (jxl_pfx_hist *)jxl_calloc(ctx, dec->num_clusters,
                                              sizeof(jxl_pfx_hist));
        if (!dec->pfx) { jxl_free(ctx, counts); return -1; }
        for (i = 0; i < dec->num_clusters; i++) {
            if (pfx_parse(ctx, br, &dec->pfx[i], counts[i]) != 0) {
                jxl_free(ctx, counts);
                JXL_ERR(ctx, "invalid prefix histogram");
                return -1;
            }
        }
        jxl_free(ctx, counts);
    } else {
        dec->ans = (jxl_ans_hist *)jxl_calloc(ctx, dec->num_clusters,
                                              sizeof(jxl_ans_hist));
        if (!dec->ans) return -1;
        for (i = 0; i < dec->num_clusters; i++) {
            if (ans_parse(ctx, br, &dec->ans[i], log_alphabet_size) != 0) {
                JXL_ERR(ctx, "invalid ANS histogram");
                return -1;
            }
        }
    }
    return br->err ? -1 : 0;
}

void jxl_dec_begin(jxl_dec *dec, jxl_br *br) {
    if (!dec->use_prefix) dec->state = jxl_br_read(br, 32);
    dec->num_to_copy = 0;
    dec->copy_pos = 0;
    dec->num_decoded = 0;
    dec->err = 0;
}

int jxl_dec_init(jxl_ctx *ctx, jxl_dec *dec, jxl_br *br, uint32_t num_dist) {
    memset(dec, 0, sizeof(*dec));
    dec->ctx = ctx;
    dec->lz77_enabled = jxl_br_bool(br);
    if (dec->lz77_enabled) {
        dec->min_symbol = jxl_br_u32(br, 224, 0, 512, 0, 4096, 0, 8, 15);
        dec->min_length = jxl_br_u32(br, 3, 0, 4, 0, 5, 2, 9, 8);
        if (int_config_parse(br, &dec->lz_len_conf, 8) != 0) {
            JXL_ERR(ctx, "invalid LZ77 length config");
            return -1;
        }
        num_dist += 1;
    }
    return dec_parse_inner(ctx, dec, br, num_dist);
}

static int lz77_ensure_window(jxl_dec *dec) {
    if (dec->window) return 0;
    dec->window = (uint32_t *)jxl_malloc(
        dec->ctx, (size_t)LZ77_WINDOW_SIZE * sizeof(uint32_t));
#ifdef JXL_POISON_UNINIT
    if (dec->window) {
        memset(dec->window, 0xCD,
               (size_t)LZ77_WINDOW_SIZE * sizeof(uint32_t));
    }
#endif
    return dec->window ? 0 : -1;
}

static uint32_t dec_read_symbol(jxl_dec *dec, jxl_br *br, uint32_t cluster) {
    if (dec->use_prefix) return pfx_read(&dec->pfx[cluster], br);
    return ans_read_symbol(&dec->ans[cluster], br, &dec->state);
}

uint32_t jxl_dec_read_clustered(jxl_dec *dec, jxl_br *br, uint32_t cluster,
                                uint32_t dist_multiplier) {
    uint32_t r;

    if (cluster >= dec->num_clusters) { dec->err = 1; return 0; }

    if (!dec->lz77_enabled) {
        uint32_t token = dec_read_symbol(dec, br, cluster);
        return read_uint(br, &dec->configs[cluster], token);
    }

    if (lz77_ensure_window(dec) != 0) { dec->err = 1; return 0; }

    if (dec->num_to_copy > 0) {
        r = dec->window[dec->copy_pos & LZ77_WINDOW_MASK];
        dec->copy_pos++;
        dec->num_to_copy--;
    } else {
        uint32_t token = dec_read_symbol(dec, br, cluster);
        if (token >= dec->min_symbol) {
            uint32_t lz_cluster = dec->clusters[dec->num_dist - 1];
            uint32_t num_to_copy, distance;
            if (dec->num_decoded == 0) {
                JXL_ERR(dec->ctx, "LZ77 repeat before any symbol");
                dec->err = 1;
                return 0;
            }
            num_to_copy = read_uint(br, &dec->lz_len_conf, token - dec->min_symbol);
            if (num_to_copy > 0xffffffffu - dec->min_length) {
                dec->err = 1;
                return 0;
            }
            dec->num_to_copy = num_to_copy + dec->min_length;

            token = dec_read_symbol(dec, br, lz_cluster);
            distance = read_uint(br, &dec->configs[lz_cluster], token);
            if (dist_multiplier == 0) {

            } else if (distance < 120) {
                int32_t offset = lz77_special_distances[distance][0];
                int32_t d = lz77_special_distances[distance][1];
                int32_t v = offset + (int32_t)dist_multiplier * d - 1;
                distance = v < 0 ? 0 : (uint32_t)v;
            } else {
                distance -= 120;
            }
            if (distance > LZ77_WINDOW_MASK) distance = LZ77_WINDOW_MASK;
            distance += 1;
            if (distance > dec->num_decoded) distance = dec->num_decoded;
            dec->copy_pos = dec->num_decoded - distance;

            r = dec->window[dec->copy_pos & LZ77_WINDOW_MASK];
            dec->copy_pos++;
            dec->num_to_copy--;
        } else {
            r = read_uint(br, &dec->configs[cluster], token);
        }
    }
    dec->window[dec->num_decoded & LZ77_WINDOW_MASK] = r;
    dec->num_decoded++;
    return r;
}

uint32_t jxl_dec_read_clustered_no_lz77(jxl_dec *dec, jxl_br *br,
                                        uint32_t cluster) {
    uint32_t token;

    token = dec_read_symbol(dec, br, cluster);
    return read_uint(br, &dec->configs[cluster], token);
}

uint32_t jxl_dec_read_mult(jxl_dec *dec, jxl_br *br, uint32_t ctx_idx,
                           uint32_t dist_multiplier) {
    if (ctx_idx >= dec->num_dist) { dec->err = 1; return 0; }
    return jxl_dec_read_clustered(dec, br, dec->clusters[ctx_idx],
                                  dist_multiplier);
}

uint32_t jxl_dec_read(jxl_dec *dec, jxl_br *br, uint32_t ctx_idx) {
    return jxl_dec_read_mult(dec, br, ctx_idx, 0);
}

int jxl_dec_is_prefix_rle1(const jxl_dec *dec) {
    uint32_t cluster, i;
    if (!dec->lz77_enabled || !dec->use_prefix || dec->num_dist == 0)
        return 0;
    cluster = dec->clusters[dec->num_dist - 1];
    if (cluster >= dec->num_clusters) return 0;
    if (dec->pfx[cluster].single_symbol != 1 ||
        dec->configs[cluster].split > 1 ||
        (dec->lz_len_conf.msb_in_token |
         dec->lz_len_conf.lsb_in_token) != 0) {
        return 0;
    }
    for (i = 0; i < dec->num_clusters; i++) {
        if (dec->configs[i].split != 1 ||
            dec->configs[i].split_exponent != 0 ||
            (dec->configs[i].msb_in_token |
             dec->configs[i].lsb_in_token) != 0) {
            return 0;
        }
    }
    return 1;
}

uint32_t jxl_dec_read_prefix_rle1(jxl_dec *dec, jxl_br *br, uint32_t cluster,
                                  uint32_t *last, uint32_t *run, int *have) {
    uint32_t token;

    token = pfx_read(&dec->pfx[cluster], br);
    if (token >= dec->min_symbol) {
        uint32_t n;
        const jxl_int_config *cfg = &dec->lz_len_conf;
        if (!*have) {
            dec->err = 1;
            return 0;
        }
        token -= dec->min_symbol;
        n = read_uint_00(br, cfg, token);
        if (n > 0xffffffffu - dec->min_length) {
            dec->err = 1;
            return 0;
        }
        *run = n + dec->min_length - 1;
    } else {
        if (token == 0) {
            *last = 0;
        } else {
            uint32_t n = (token - 1) & 31;
            *last = (1u << n) | jxl_br_read(br, (int)n);
        }
        *have = 1;
    }
    return *last;
}

int jxl_dec_finalize(jxl_dec *dec) {
    if (dec->err) return -1;
    if (dec->use_prefix) return 0;
    return dec->state == ANS_SIGNATURE ? 0 : -1;
}

static uint32_t perm_context(uint32_t x) {
    uint32_t b = jxl_bitlen(x);
    return b < 7 ? b : 7;
}

int jxl_read_permutation(jxl_ctx *ctx, jxl_dec *dec, jxl_br *br, uint32_t size,
                         uint32_t skip, uint32_t *out) {
    uint32_t *lehmer = NULL;
    uint32_t *temp = NULL;
    uint32_t end, i, ntemp;
    uint32_t prev = 0;
    int rc = -1;

    end = jxl_dec_read(dec, br, perm_context(size));
    if (end > size - skip || br->err) {
        JXL_ERR(ctx, "invalid permutation length");
        return -1;
    }
    if (end) {
        lehmer = (uint32_t *)jxl_calloc(ctx, end, sizeof(uint32_t));
        if (!lehmer) return -1;
    }
    for (i = 0; i < end; i++) {
        lehmer[i] = jxl_dec_read(dec, br, perm_context(prev));
        if (lehmer[i] >= size - skip - i || br->err) {
            JXL_ERR(ctx, "invalid permutation entry");
            goto done;
        }
        prev = lehmer[i];
    }

    ntemp = size - skip;
    temp = (uint32_t *)jxl_calloc(ctx, ntemp ? ntemp : 1, sizeof(uint32_t));
    if (!temp) goto done;
    for (i = 0; i < ntemp; i++) temp[i] = skip + i;

    for (i = 0; i < skip; i++) out[i] = i;
    for (i = 0; i < end; i++) {
        uint32_t idx = lehmer[i];
        out[skip + i] = temp[idx];
        memmove(temp + idx, temp + idx + 1, (ntemp - idx - 1) * sizeof(uint32_t));
        ntemp--;
    }
    for (i = 0; i < ntemp; i++) out[skip + end + i] = temp[i];
    rc = 0;

done:
    jxl_free(ctx, lehmer);
    jxl_free(ctx, temp);
    return rc;
}

typedef struct {
    jxl_ctx *ctx;
    uint8_t *p;
    size_t len;
    size_t cap;
    int err;
} jxl_bytebuf;

static void bb_init(jxl_bytebuf *b, jxl_ctx *ctx) {
    memset(b, 0, sizeof(*b));
    b->ctx = ctx;
}

static void bb_free(jxl_bytebuf *b) {
    jxl_free(b->ctx, b->p);
    b->p = NULL;
    b->len = b->cap = 0;
}

static int bb_reserve(jxl_bytebuf *b, size_t need) {
    size_t cap;
    uint8_t *np;
    if (b->err) return -1;
    if (b->len + need <= b->cap) return 0;
    cap = b->cap ? b->cap : 256;
    while (cap < b->len + need) {
        if (cap > ((size_t)-1) / 2) { b->err = 1; return -1; }
        cap *= 2;
    }
    np = (uint8_t *)jxl_realloc_array(b->ctx, b->p, b->cap, cap, 1);
    if (!np) { b->err = 1; return -1; }
    b->p = np;
    b->cap = cap;
    return 0;
}

static void bb_push(jxl_bytebuf *b, uint8_t v) {
    if (bb_reserve(b, 1) != 0) return;
    b->p[b->len++] = v;
}

static void bb_append(jxl_bytebuf *b, const void *src, size_t n) {
    if (bb_reserve(b, n) != 0) return;
    memcpy(b->p + b->len, src, n);
    b->len += n;
}

static void bb_push_be32(jxl_bytebuf *b, uint32_t v) {
    uint8_t t[4];
    t[0] = (uint8_t)(v >> 24);
    t[1] = (uint8_t)(v >> 16);
    t[2] = (uint8_t)(v >> 8);
    t[3] = (uint8_t)v;
    bb_append(b, t, 4);
}

typedef struct {
    const uint8_t *p;
    size_t len;
    size_t pos;
    int err;
} jxl_bytecur;

static int bc_u8(jxl_bytecur *c, uint8_t *out) {
    if (c->pos >= c->len) { c->err = 1; return -1; }
    *out = c->p[c->pos++];
    return 0;
}

static uint64_t bc_varint(jxl_bytecur *c) {
    uint64_t value = 0;
    int shift = 0;
    while (shift < 63) {
        uint8_t b;
        if (bc_u8(c, &b) != 0) return 0;
        value |= (uint64_t)(b & 0x7f) << shift;
        if ((b & 0x80) == 0) break;
        shift += 7;
    }
    return value;
}

static uint32_t icc_ctx_for(size_t idx, uint8_t b1, uint8_t b2) {
    uint32_t p1, p2;
    if (idx <= 128) return 0;

    if ((b1 >= 'a' && b1 <= 'z') || (b1 >= 'A' && b1 <= 'Z')) p1 = 0;
    else if ((b1 >= '0' && b1 <= '9') || b1 == '.' || b1 == ',') p1 = 1;
    else if (b1 <= 1) p1 = 2 + b1;
    else if (b1 <= 15) p1 = 4;
    else if (b1 >= 241 && b1 <= 254) p1 = 5;
    else if (b1 == 255) p1 = 6;
    else p1 = 7;

    if ((b2 >= 'a' && b2 <= 'z') || (b2 >= 'A' && b2 <= 'Z')) p2 = 0;
    else if ((b2 >= '0' && b2 <= '9') || b2 == '.' || b2 == ',') p2 = 1;
    else if (b2 <= 15) p2 = 2;
    else if (b2 >= 241) p2 = 3;
    else p2 = 4;

    return 1 + p1 + 8 * p2;
}

static int read_icc_stream(jxl_ctx *ctx, jxl_br *br, uint8_t **out,
                           size_t *out_len) {
    uint64_t enc_size = jxl_br_u64(br);
    jxl_dec dec;
    uint8_t *enc = NULL;
    uint8_t b1 = 0, b2 = 0;
    size_t i, header_len, bits_at_start;
    int rc = -1;

    if (enc_size > (1u << 28)) {
        JXL_ERR(ctx, "icc: encoded profile too large");
        return -1;
    }
    if (jxl_dec_init(ctx, &dec, br, 41) != 0) return -1;
    jxl_dec_begin(&dec, br);
    bits_at_start = jxl_br_bits_read(br);

    header_len = (size_t)enc_size < 18 ? (size_t)enc_size : 18;
    enc = (uint8_t *)jxl_malloc(ctx, header_len ? header_len : 1);
    if (!enc) goto done;

    for (i = 0; i < header_len; i++) {
        uint32_t sym = jxl_dec_read(&dec, br, icc_ctx_for(i, b1, b2));
        if (sym >= 256 || br->err || dec.err) {
            JXL_ERR(ctx, "icc: bad encoded byte");
            goto done;
        }
        enc[i] = (uint8_t)sym;
        b2 = b1;
        b1 = enc[i];
    }

    if (header_len > 0) {
        jxl_bytecur hdr = {enc, header_len, 0, 0};
        uint64_t output_size = bc_varint(&hdr);
        uint64_t commands_size = bc_varint(&hdr);
        size_t stream_offset = hdr.pos;
        if (hdr.err || stream_offset + commands_size > enc_size) {
            JXL_ERR(ctx, "icc: invalid commands_size");
            goto done;
        }
        if (output_size > (1u << 28)) {
            JXL_ERR(ctx, "icc: output too large");
            goto done;
        }

        if (output_size + 65536ull < enc_size) {
            JXL_ERR(ctx, "icc: encoded profile far larger than output");
            goto done;
        }
    }

    if (enc_size > header_len) {
        uint8_t *np = (uint8_t *)jxl_realloc_array(
            ctx, enc, header_len, (size_t)enc_size, 1);
        if (!np) goto done;
        enc = np;
        for (i = header_len; i < (size_t)enc_size; i++) {
            uint32_t sym = jxl_dec_read(&dec, br, icc_ctx_for(i, b1, b2));
            if (sym >= 256 || br->err || dec.err) {
                JXL_ERR(ctx, "icc: bad encoded byte");
                goto done;
            }
            enc[i] = (uint8_t)sym;
            b2 = b1;
            b1 = enc[i];

            if ((i & 0xFFFF) == 0 && i > 0) {
                size_t used_bits = jxl_br_bits_read(br) - bits_at_start;
                size_t used_bytes = (used_bits + 7) / 8;
                if (used_bytes == 0 || i > used_bytes * 256) {
                    JXL_ERR(ctx, "icc: corrupted stream");
                    goto done;
                }
            }
        }
    }
    if (jxl_dec_finalize(&dec) != 0) {
        JXL_ERR(ctx, "icc: bad ANS final state");
        goto done;
    }
    *out = enc;
    *out_len = (size_t)enc_size;
    enc = NULL;
    rc = 0;

done:
    jxl_free(ctx, enc);
    jxl_dec_free(&dec);
    return rc;
}

static const char *const icc_common_tags[19] = {
    "rTRC", "rXYZ", "cprt", "wtpt", "bkpt", "rXYZ", "gXYZ", "bXYZ", "kXYZ",
    "rTRC", "gTRC", "bTRC", "kTRC", "chad", "desc", "chrm", "dmnd", "dmdd",
    "lumi"
};

static const char *const icc_common_data[8] = {
    "XYZ ", "desc", "text", "mluc", "para", "curv", "sf32", "gbd "
};

static uint8_t predict_header(size_t idx, uint32_t output_size,
                              const uint8_t *header) {
    static const char mntr[] = "mntrRGB XYZ ";
    if (idx <= 3) return (uint8_t)(output_size >> (8 * (3 - idx)));
    if (idx == 8) return 4;
    if (idx >= 12 && idx <= 23) return (uint8_t)mntr[idx - 12];
    if (idx >= 36 && idx <= 39) return (uint8_t)"acsp"[idx - 36];
    if ((idx == 41 || idx == 42) && header[40] == 'A') return 'P';
    if (idx == 43 && header[40] == 'A') return 'L';
    if (idx == 41 && header[40] == 'M') return 'S';
    if (idx == 42 && header[40] == 'M') return 'F';
    if (idx == 43 && header[40] == 'M') return 'T';
    if (idx == 42 && header[40] == 'S' && header[41] == 'G') return 'I';
    if (idx == 43 && header[40] == 'S' && header[41] == 'G') return ' ';
    if (idx == 42 && header[40] == 'S' && header[41] == 'U') return 'N';
    if (idx == 43 && header[40] == 'S' && header[41] == 'U') return 'W';
    if (idx == 70) return 246;
    if (idx == 71) return 214;
    if (idx == 73) return 1;
    if (idx == 78) return 211;
    if (idx == 79) return 45;
    if (idx >= 80 && idx <= 83) return header[4 + idx - 80];
    return 0;
}

static void shuffle2(const uint8_t *in, size_t len, uint8_t *out) {
    size_t height = len / 2, odd = len % 2, i;
    for (i = 0; i < height; i++) {
        out[2 * i] = in[i];
        out[2 * i + 1] = in[i + height + odd];
    }
    if (odd) out[len - 1] = in[height];
}

static void shuffle4(const uint8_t *in, size_t len, uint8_t *out) {
    size_t step = len / 4, wide = len % 4, i, j, o = 0;
    for (i = 0; i < step; i++) {
        size_t base = i;
        for (j = 0; j < wide; j++) {
            out[o++] = in[base];
            base += step + 1;
        }
        for (j = wide; j < 4; j++) {
            out[o++] = in[base];
            base += step;
        }
    }
    for (i = 1; i <= wide; i++) out[o++] = in[(step + 1) * i - 1];
}

static int decode_icc(jxl_ctx *ctx, const uint8_t *stream, size_t stream_len,
                      uint8_t **out_p, size_t *out_n) {
    jxl_bytecur hdr = {stream, stream_len, 0, 0};
    jxl_bytecur cmds;
    uint64_t output_size, commands_size;
    size_t stream_offset;
    const uint8_t *data;
    size_t data_len;
    size_t header_size;
    jxl_bytebuf out;
    uint8_t *shuf = NULL;
    size_t i;
    int rc = -1;

    output_size = bc_varint(&hdr);
    commands_size = bc_varint(&hdr);
    stream_offset = hdr.pos;
    if (hdr.err || stream_offset + commands_size > stream_len) {
        JXL_ERR(ctx, "icc: invalid commands_size");
        return -1;
    }
    if (output_size > (1u << 28)) {
        JXL_ERR(ctx, "icc: output too large");
        return -1;
    }

    cmds.p = stream + stream_offset;
    cmds.len = (size_t)commands_size;
    cmds.pos = 0;
    cmds.err = 0;

    data = stream + stream_offset + commands_size;
    data_len = stream_len - stream_offset - (size_t)commands_size;

    header_size = output_size < 128 ? (size_t)output_size : 128;
    if (data_len < header_size) {
        JXL_ERR(ctx, "icc: invalid output_size");
        return -1;
    }

    bb_init(&out, ctx);
    for (i = 0; i < header_size; i++) {
        uint8_t p = predict_header(i, (uint32_t)output_size, data);
        bb_push(&out, (uint8_t)(p + data[i]));
    }
    data += header_size;
    data_len -= header_size;
    if (output_size <= 128) goto finish;

    {
        uint64_t v = bc_varint(&cmds);
        if (v >= 1) {
            uint32_t num_tags = (uint32_t)(v - 1);
            uint32_t prev_tagstart, prev_tagsize = 0;
            if ((output_size - 128) / 12 < num_tags) {
                JXL_ERR(ctx, "icc: num_tags too large");
                goto done;
            }
            bb_push_be32(&out, num_tags);
            prev_tagstart = num_tags * 12 + 128;

            for (;;) {
                uint8_t command;
                uint8_t tagcode;
                const char *tag;
                char tagbuf[4];
                uint32_t tagstart, tagsize;

                if (bc_u8(&cmds, &command) != 0) goto finish;
                tagcode = command & 63;
                if (tagcode == 0) break;
                if (tagcode == 1) {
                    if (data_len < 4) {
                        JXL_ERR(ctx, "icc: short data stream");
                        goto done;
                    }
                    memcpy(tagbuf, data, 4);
                    data += 4;
                    data_len -= 4;
                    tag = tagbuf;
                } else if (tagcode <= 20) {
                    tag = icc_common_tags[tagcode - 2];
                } else {
                    JXL_ERR(ctx, "icc: invalid tagcode");
                    goto done;
                }

                if (command & 64) tagstart = (uint32_t)bc_varint(&cmds);
                else tagstart = prev_tagstart + prev_tagsize;

                if (command & 128) {
                    tagsize = (uint32_t)bc_varint(&cmds);
                } else if (memcmp(tag, "rXYZ", 4) == 0 ||
                           memcmp(tag, "gXYZ", 4) == 0 ||
                           memcmp(tag, "bXYZ", 4) == 0 ||
                           memcmp(tag, "kXYZ", 4) == 0 ||
                           memcmp(tag, "wtpt", 4) == 0 ||
                           memcmp(tag, "bkpt", 4) == 0 ||
                           memcmp(tag, "lumi", 4) == 0) {
                    tagsize = 20;
                } else {
                    tagsize = prev_tagsize;
                }
                if ((uint64_t)tagstart + tagsize > output_size) {
                    JXL_ERR(ctx, "icc: tag out of range");
                    goto done;
                }
                prev_tagstart = tagstart;
                prev_tagsize = tagsize;

                bb_append(&out, tag, 4);
                bb_push_be32(&out, tagstart);
                bb_push_be32(&out, tagsize);
                if (tagcode == 2) {
                    bb_append(&out, "gTRC", 4);
                    bb_push_be32(&out, tagstart);
                    bb_push_be32(&out, tagsize);
                    bb_append(&out, "bTRC", 4);
                    bb_push_be32(&out, tagstart);
                    bb_push_be32(&out, tagsize);
                } else if (tagcode == 3) {
                    bb_append(&out, "gXYZ", 4);
                    bb_push_be32(&out, tagstart + tagsize);
                    bb_push_be32(&out, tagsize);
                    bb_append(&out, "bXYZ", 4);
                    bb_push_be32(&out, tagstart + tagsize * 2);
                    bb_push_be32(&out, tagsize);
                }
                if (cmds.err) goto finish;
            }
        }
    }

    for (;;) {
        uint8_t command;
        if (bc_u8(&cmds, &command) != 0) break;
        if (command == 1) {
            size_t num = (size_t)bc_varint(&cmds);
            if (num > data_len) { JXL_ERR(ctx, "icc: short stream"); goto done; }
            bb_append(&out, data, num);
            data += num;
            data_len -= num;
        } else if (command == 2 || command == 3) {
            size_t num = (size_t)bc_varint(&cmds);
            if (num > data_len) { JXL_ERR(ctx, "icc: short stream"); goto done; }
            shuf = (uint8_t *)jxl_malloc(ctx, num ? num : 1);
            if (!shuf) goto done;
            if (command == 2) shuffle2(data, num, shuf);
            else shuffle4(data, num, shuf);
            bb_append(&out, shuf, num);
            jxl_free(ctx, shuf);
            shuf = NULL;
            data += num;
            data_len -= num;
        } else if (command == 4) {
            uint8_t flags;
            size_t width, order, stride, num, k;
            const uint8_t *src;
            if (bc_u8(&cmds, &flags) != 0) { JXL_ERR(ctx, "icc: short stream"); goto done; }
            width = (size_t)((flags & 3) + 1);
            order = (size_t)((flags >> 2) & 3);
            if (width == 3 || order == 3) {
                JXL_ERR(ctx, "icc: bad predictor flags");
                goto done;
            }
            if (flags & 16) {
                stride = (size_t)bc_varint(&cmds);
                if (stride < width) { JXL_ERR(ctx, "icc: stride < width"); goto done; }
            } else {
                stride = width;
            }
            if (stride * 4 >= out.len) {
                JXL_ERR(ctx, "icc: stride too large");
                goto done;
            }
            num = (size_t)bc_varint(&cmds);
            if (num > data_len) { JXL_ERR(ctx, "icc: short stream"); goto done; }
            if (width == 1) {
                src = data;
            } else {
                shuf = (uint8_t *)jxl_malloc(ctx, num ? num : 1);
                if (!shuf) goto done;
                if (width == 2) shuffle2(data, num, shuf);
                else shuffle4(data, num, shuf);
                src = shuf;
            }
            for (k = 0; k < num; k += width) {
                uint32_t prev[3] = {0, 0, 0};
                uint32_t p;
                size_t j;
                for (j = 0; j <= order; j++) {
                    size_t offset = out.len - stride * (j + 1);
                    uint8_t t[4] = {0, 0, 0, 0};
                    memcpy(t + (4 - width), out.p + offset, width);
                    prev[j] = ((uint32_t)t[0] << 24) | ((uint32_t)t[1] << 16) |
                              ((uint32_t)t[2] << 8) | t[3];
                }
                if (order == 0) p = prev[0];
                else if (order == 1) p = 2 * prev[0] - prev[1];
                else p = 3 * (prev[0] - prev[1]) + prev[2];

                for (j = 0; j < width && j < num - k; j++) {
                    uint32_t val = (uint32_t)src[k + j] + (p >> (8 * (width - 1 - j)));
                    bb_push(&out, (uint8_t)val);
                }
            }
            jxl_free(ctx, shuf);
            shuf = NULL;
            data += num;
            data_len -= num;
        } else if (command == 10) {
            static const uint8_t xyz[8] = {'X', 'Y', 'Z', ' ', 0, 0, 0, 0};
            if (data_len < 12) { JXL_ERR(ctx, "icc: short stream"); goto done; }
            bb_append(&out, xyz, 8);
            bb_append(&out, data, 12);
            data += 12;
            data_len -= 12;
        } else if (command >= 16 && command <= 23) {
            static const uint8_t zeros[4] = {0, 0, 0, 0};
            bb_append(&out, icc_common_data[command - 16], 4);
            bb_append(&out, zeros, 4);
        } else {
            JXL_ERR(ctx, "icc: invalid command %u", (unsigned)command);
            goto done;
        }
        if (cmds.err || out.err) goto done;
    }

    if (out.len != (size_t)output_size) {
        JXL_ERR(ctx, "icc: size mismatch (%u vs %u)", (unsigned)out.len,
                (unsigned)output_size);
        goto done;
    }

finish:
    if (out.err) goto done;
    *out_p = out.p;
    *out_n = out.len;
    out.p = NULL;
    rc = 0;

done:
    jxl_free(ctx, shuf);
    bb_free(&out);
    return rc;
}

int jxl_read_icc(jxl_ctx *ctx, jxl_br *br, uint8_t **out, size_t *out_len) {
    uint8_t *enc = NULL;
    size_t enc_len = 0;
    int rc;

    *out = NULL;
    *out_len = 0;
    if (read_icc_stream(ctx, br, &enc, &enc_len) != 0) return -1;
    rc = decode_icc(ctx, enc, enc_len, out, out_len);
    jxl_free(ctx, enc);
    return rc;
}

static uint32_t div_ceil(uint32_t a, uint32_t b) {
    return (a + b - 1) / b;
}

uint32_t jxl_frame_sample_width(const jxl_frame_header *fh, uint32_t upsampling) {
    uint32_t w = fh->width;
    if (upsampling > 1) w = div_ceil(w, upsampling);
    if (fh->lf_level > 0) {
        uint32_t sh = 3 * fh->lf_level;
        w = (w + (1u << sh) - 1) >> sh;
    }
    return w;
}

uint32_t jxl_frame_sample_height(const jxl_frame_header *fh, uint32_t upsampling) {
    uint32_t h = fh->height;
    if (upsampling > 1) h = div_ceil(h, upsampling);
    if (fh->lf_level > 0) {
        uint32_t sh = 3 * fh->lf_level;
        h = (h + (1u << sh) - 1) >> sh;
    }
    return h;
}

uint32_t jxl_frame_color_width(const jxl_frame_header *fh) {
    return jxl_frame_sample_width(fh, fh->upsampling);
}

uint32_t jxl_frame_color_height(const jxl_frame_header *fh) {
    return jxl_frame_sample_height(fh, fh->upsampling);
}

uint32_t jxl_frame_group_dim(const jxl_frame_header *fh) {
    return 128u << fh->group_size_shift;
}

uint32_t jxl_frame_groups_per_row(const jxl_frame_header *fh) {
    return div_ceil(jxl_frame_color_width(fh), jxl_frame_group_dim(fh));
}

static uint32_t max_shift(const jxl_frame_header *fh, int vertical) {
    uint32_t m = 0;
    int i;
    for (i = 0; i < 3; i++) {
        uint32_t mode = fh->jpeg_upsampling[i];
        uint32_t s = vertical ? (mode == 1 || mode == 3) : (mode == 1 || mode == 2);
        if (s > m) m = s;
    }
    return m;
}

uint32_t jxl_frame_blocks_w(const jxl_frame_header *fh) {
    uint32_t sh = max_shift(fh, 0);
    return div_ceil(jxl_frame_color_width(fh), 8u << sh) << sh;
}

uint32_t jxl_frame_blocks_h(const jxl_frame_header *fh) {
    uint32_t sh = max_shift(fh, 1);
    return div_ceil(jxl_frame_color_height(fh), 8u << sh) << sh;
}

uint32_t jxl_frame_lf_groups_per_row(const jxl_frame_header *fh) {
    return div_ceil(jxl_frame_blocks_w(fh), jxl_frame_group_dim(fh));
}

uint32_t jxl_frame_num_groups(const jxl_frame_header *fh) {
    uint32_t dim = jxl_frame_group_dim(fh);
    return div_ceil(jxl_frame_color_width(fh), dim) *
           div_ceil(jxl_frame_color_height(fh), dim);
}

uint32_t jxl_frame_num_lf_groups(const jxl_frame_header *fh) {
    uint32_t dim = jxl_frame_group_dim(fh);
    return div_ceil(jxl_frame_blocks_w(fh), dim) *
           div_ceil(jxl_frame_blocks_h(fh), dim);
}

static int frame_type_is_normal(jxl_frame_type t) {
    return t == JXL_FRAME_REGULAR || t == JXL_FRAME_SKIP_PROGRESSIVE;
}

static int test_full_image(const jxl_frame_header *fh, const jxl_size_header *sz) {
    int64_t right, bottom;
    if (fh->x0 > 0 || fh->y0 > 0) return 0;
    right = (int64_t)fh->x0 + fh->width;
    bottom = (int64_t)fh->y0 + fh->height;
    return right >= (int64_t)sz->width && bottom >= (int64_t)sz->height;
}

static int computes_resets_canvas(jxl_blend_mode mode,
                                  const jxl_frame_header *fh,
                                  const jxl_size_header *sz) {
    return mode == JXL_BLEND_REPLACE && (!fh->have_crop || test_full_image(fh, sz));
}

static void read_blending_info(jxl_br *br, jxl_blending_info *bi,
                               int have_extra, const jxl_blend_mode *first_mode,
                               const jxl_frame_header *fh,
                               const jxl_size_header *sz) {
    uint32_t m = jxl_br_u32(br, 0, 0, 1, 0, 2, 0, 3, 2);
    bi->mode = (jxl_blend_mode)m;
    bi->alpha_channel = 0;
    bi->clamp = 0;
    bi->source = 0;
    if (m > JXL_BLEND_MUL) {
        br->err = 1;
        return;
    }
    if (have_extra && (m == JXL_BLEND_BLEND || m == JXL_BLEND_MULADD)) {
        bi->alpha_channel = jxl_br_u32(br, 0, 0, 1, 0, 2, 0, 3, 3);
    }
    if ((have_extra && (m == JXL_BLEND_BLEND || m == JXL_BLEND_MULADD)) ||
        m == JXL_BLEND_MUL) {
        bi->clamp = jxl_br_bool(br);
    }
    if (!computes_resets_canvas(first_mode ? *first_mode : bi->mode, fh, sz)) {
        bi->source = jxl_br_read(br, 2);
    }
}

static void read_gabor(jxl_br *br, jxl_gabor *g) {
    int i, j;
    g->enabled = jxl_br_bool(br);
    for (i = 0; i < 3; i++) {
        g->weights[i][0] = 0.115169525f;
        g->weights[i][1] = 0.061248592f;
    }
    if (!g->enabled) return;
    if (!jxl_br_bool(br)) return;
    for (i = 0; i < 3; i++) {
        for (j = 0; j < 2; j++) g->weights[i][j] = jxl_br_f16(br);
    }
}

static const float epf_sharp_lut_default[8] = {
    0.0f, 1.0f / 7.0f, 2.0f / 7.0f, 3.0f / 7.0f,
    4.0f / 7.0f, 5.0f / 7.0f, 6.0f / 7.0f, 1.0f
};

static void read_epf(jxl_br *br, jxl_epf *e, jxl_encoding encoding) {
    int i;
    memcpy(e->sharp_lut, epf_sharp_lut_default, sizeof(e->sharp_lut));
    e->channel_scale[0] = 40.0f;
    e->channel_scale[1] = 5.0f;
    e->channel_scale[2] = 3.5f;
    e->quant_mul = 0.46f;
    e->pass0_sigma_scale = 0.9f;
    e->pass2_sigma_scale = 6.5f;
    e->border_sad_mul = 2.0f / 3.0f;
    e->sigma_for_modular = 1.0f;

    e->iters = jxl_br_read(br, 2);
    e->enabled = e->iters != 0;
    if (!e->enabled) return;

    if (encoding == JXL_ENC_VARDCT && jxl_br_bool(br)) {
        for (i = 0; i < 8; i++) e->sharp_lut[i] = jxl_br_f16(br);
    }
    if (jxl_br_bool(br)) {
        for (i = 0; i < 3; i++) e->channel_scale[i] = jxl_br_f16(br);
        jxl_br_read(br, 32);
    }
    if (jxl_br_bool(br)) {
        if (encoding == JXL_ENC_VARDCT) e->quant_mul = jxl_br_f16(br);
        e->pass0_sigma_scale = jxl_br_f16(br);
        e->pass2_sigma_scale = jxl_br_f16(br);
        e->border_sad_mul = jxl_br_f16(br);
    }
    if (encoding == JXL_ENC_MODULAR) e->sigma_for_modular = jxl_br_f16(br);
}

static void read_extensions_frame(jxl_br *br) {
    uint64_t bits = jxl_br_u64(br);
    uint64_t lens[64];
    int n = 0, i;
    for (i = 0; i < 64; i++) {
        if ((bits >> i) & 1) lens[n++] = jxl_br_u64(br);
    }
    for (i = 0; i < n; i++) jxl_br_skip(br, (size_t)lens[i]);
}

int jxl_read_frame_header(jxl_ctx *ctx, jxl_br *br, const jxl_size_header *size,
                          const jxl_image_metadata *meta, jxl_frame_header *fh) {
    int all_default;
    uint32_t i;
    uint32_t nec = meta->num_extra;

    memset(fh, 0, sizeof(*fh));
    fh->frame_type = JXL_FRAME_REGULAR;
    fh->encoding = JXL_ENC_VARDCT;
    fh->upsampling = 1;
    fh->group_size_shift = 1;
    fh->passes.num_passes = 1;
    fh->width = size->width;
    fh->height = size->height;
    fh->is_last = 1;
    fh->x_qm_scale = 3;
    fh->b_qm_scale = 2;
    fh->gab.enabled = 1;
    for (i = 0; i < 3; i++) {
        fh->gab.weights[i][0] = 0.115169525f;
        fh->gab.weights[i][1] = 0.061248592f;
    }
    fh->epf.enabled = 1;
    fh->epf.iters = 2;
    memcpy(fh->epf.sharp_lut, epf_sharp_lut_default, sizeof(fh->epf.sharp_lut));
    fh->epf.channel_scale[0] = 40.0f;
    fh->epf.channel_scale[1] = 5.0f;
    fh->epf.channel_scale[2] = 3.5f;
    fh->epf.quant_mul = 0.46f;
    fh->epf.pass0_sigma_scale = 0.9f;
    fh->epf.pass2_sigma_scale = 6.5f;
    fh->epf.border_sad_mul = 2.0f / 3.0f;
    fh->epf.sigma_for_modular = 1.0f;

    if (nec) {
        fh->ec_upsampling = (uint32_t *)jxl_calloc(ctx, nec, sizeof(uint32_t));
        fh->ec_blending = (jxl_blending_info *)jxl_calloc(ctx, nec,
                                                          sizeof(jxl_blending_info));
        if (!fh->ec_upsampling || !fh->ec_blending) return -1;
        for (i = 0; i < nec; i++) fh->ec_upsampling[i] = 1;
    }

    all_default = jxl_br_bool(br);
    if (!all_default) {
        fh->frame_type = (jxl_frame_type)jxl_br_read(br, 2);
        fh->encoding = (jxl_encoding)jxl_br_read(br, 1);
        fh->flags = jxl_br_u64(br);
        if (!meta->xyb_encoded) fh->do_ycbcr = jxl_br_bool(br);
    }

    fh->encoded_color_channels =
        (fh->encoding == JXL_ENC_MODULAR && !fh->do_ycbcr && !meta->xyb_encoded &&
         meta->colour.colour_space == JXLDEC_CS_GRAY) ? 1 : 3;

    if (fh->do_ycbcr && !(fh->flags & JXL_FF_USE_LF_FRAME)) {
        for (i = 0; i < 3; i++) fh->jpeg_upsampling[i] = jxl_br_read(br, 2);
    }
    if (!all_default && !(fh->flags & JXL_FF_USE_LF_FRAME)) {
        fh->upsampling = jxl_br_u32(br, 1, 0, 2, 0, 4, 0, 8, 0);
        for (i = 0; i < nec; i++) {
            fh->ec_upsampling[i] = jxl_br_u32(br, 1, 0, 2, 0, 4, 0, 8, 0);
        }
    }
    if (fh->encoding == JXL_ENC_MODULAR) {
        fh->group_size_shift = jxl_br_read(br, 2);
    }
    if (!all_default && meta->xyb_encoded && fh->encoding == JXL_ENC_VARDCT) {
        fh->x_qm_scale = jxl_br_read(br, 3);
        fh->b_qm_scale = jxl_br_read(br, 3);
    } else if (!(meta->xyb_encoded && fh->encoding == JXL_ENC_VARDCT)) {
        fh->x_qm_scale = 2;
        fh->b_qm_scale = 2;
    }

    if (!all_default && fh->frame_type != JXL_FRAME_REFERENCE_ONLY) {
        fh->passes.num_passes = jxl_br_u32(br, 1, 0, 2, 0, 3, 0, 4, 3);
        if (fh->passes.num_passes > 11) {
            JXL_ERR(ctx, "frame: too many passes (%u)",
                    (unsigned)fh->passes.num_passes);
            return -1;
        }
        if (fh->passes.num_passes != 1) {
            fh->passes.num_ds = jxl_br_u32(br, 0, 0, 1, 0, 2, 0, 3, 1);
            if (fh->passes.num_ds > 4) {
                JXL_ERR(ctx, "frame: too many downsampling levels");
                return -1;
            }
            for (i = 0; i + 1 < fh->passes.num_passes; i++) {
                fh->passes.shift[i] = jxl_br_read(br, 2);
            }
            for (i = 0; i < fh->passes.num_ds; i++) {
                fh->passes.downsample[i] = jxl_br_u32(br, 1, 0, 2, 0, 4, 0, 8, 0);
            }
            for (i = 0; i < fh->passes.num_ds; i++) {
                fh->passes.last_pass[i] = jxl_br_u32(br, 0, 0, 1, 0, 2, 0, 0, 3);
            }
        }
    }

    if (fh->frame_type == JXL_FRAME_LF) {
        fh->lf_level = jxl_br_read(br, 2) + 1;
    } else if (!all_default) {
        fh->have_crop = jxl_br_bool(br);
    }
    if (fh->have_crop) {
        if (fh->frame_type != JXL_FRAME_REFERENCE_ONLY) {
            fh->x0 = jxl_unpack_signed(
                jxl_br_u32(br, 0, 8, 256, 11, 2304, 14, 18688, 30));
            fh->y0 = jxl_unpack_signed(
                jxl_br_u32(br, 0, 8, 256, 11, 2304, 14, 18688, 30));
        }
        fh->width = jxl_br_u32(br, 0, 8, 256, 11, 2304, 14, 18688, 30);
        fh->height = jxl_br_u32(br, 0, 8, 256, 11, 2304, 14, 18688, 30);
    }

    fh->blending.mode = JXL_BLEND_REPLACE;
    if (!all_default && frame_type_is_normal(fh->frame_type)) {
        jxl_blend_mode first;
        read_blending_info(br, &fh->blending, nec != 0, NULL, fh, size);
        first = fh->blending.mode;
        for (i = 0; i < nec; i++) {
            read_blending_info(br, &fh->ec_blending[i], nec != 0, &first, fh, size);
        }
        if (meta->have_animation) {
            fh->duration = jxl_br_u32(br, 0, 0, 1, 0, 0, 8, 0, 32);
            if (meta->animation.have_timecodes) fh->timecode = jxl_br_read(br, 32);
        }
        fh->is_last = jxl_br_bool(br);
    } else {
        fh->is_last = (fh->frame_type == JXL_FRAME_REGULAR);
    }

    if (!all_default && fh->frame_type != JXL_FRAME_LF && !fh->is_last) {
        fh->save_as_reference = jxl_br_read(br, 2);
    }

    fh->resets_canvas = computes_resets_canvas(fh->blending.mode, fh, size);
    fh->save_before_ct = !frame_type_is_normal(fh->frame_type);
    if (!all_default) {
        int cond = (fh->frame_type == JXL_FRAME_REFERENCE_ONLY) ||
                   (fh->resets_canvas && !fh->is_last &&
                    (fh->duration == 0 || fh->save_as_reference != 0) &&
                    fh->frame_type != JXL_FRAME_LF);
        if (cond) fh->save_before_ct = jxl_br_bool(br);
        fh->name = jxl_read_name(ctx, br);
        if (!jxl_br_bool(br)) {
            read_gabor(br, &fh->gab);
            read_epf(br, &fh->epf, fh->encoding);
            read_extensions_frame(br);
        }
        read_extensions_frame(br);
    }

    if (br->err) {
        JXL_ERR(ctx, "frame: truncated header");
        return -1;
    }
    if (fh->width == 0 || fh->height == 0) {
        JXL_ERR(ctx, "frame: zero-sized frame");
        return -1;
    }
    return 0;
}

void jxl_frame_header_free(jxl_ctx *ctx, jxl_frame_header *fh) {
    if (!fh) return;
    jxl_free(ctx, fh->ec_upsampling);
    jxl_free(ctx, fh->ec_blending);
    jxl_free(ctx, fh->name);
    fh->ec_upsampling = NULL;
    fh->ec_blending = NULL;
    fh->name = NULL;
}

uint32_t jxl_toc_index(const jxl_toc *toc, jxl_toc_kind kind, uint32_t pass_idx,
                       uint32_t group_idx) {
    if (toc->count <= 1) return 0;
    switch (kind) {
        case JXL_TOC_LF_GLOBAL: return 0;
        case JXL_TOC_LF_GROUP: return 1 + group_idx;
        case JXL_TOC_HF_GLOBAL: return 1 + toc->num_lf_groups;
        case JXL_TOC_GROUP_PASS:
            return 1 + toc->num_lf_groups + 1 + pass_idx * toc->num_groups +
                   group_idx;
        default: return 0;
    }
}

int jxl_read_toc(jxl_ctx *ctx, jxl_br *br, const jxl_frame_header *fh,
                 jxl_toc *toc) {
    uint32_t num_groups = jxl_frame_num_groups(fh);
    uint32_t num_lf_groups = jxl_frame_num_lf_groups(fh);
    uint32_t num_passes = fh->passes.num_passes;
    uint32_t entry_count;
    uint32_t *perm = NULL;
    uint32_t *sizes = NULL;
    uint32_t i;
    size_t acc;
    int permutated;
    int rc = -1;

    memset(toc, 0, sizeof(*toc));
    entry_count = (num_groups == 1 && num_passes == 1)
                      ? 1
                      : 1 + num_lf_groups + 1 + num_groups * num_passes;
    if (entry_count > 65536) {
        JXL_ERR(ctx, "toc: too many entries (%u)", (unsigned)entry_count);
        return -1;
    }
    toc->count = entry_count;
    toc->num_groups = num_groups;
    toc->num_lf_groups = num_lf_groups;
    toc->num_passes = num_passes;

    permutated = jxl_br_bool(br);
    if (permutated) {
        jxl_dec dec;
        perm = (uint32_t *)jxl_calloc(ctx, entry_count, sizeof(uint32_t));
        if (!perm) return -1;
        if (jxl_dec_init(ctx, &dec, br, 8) != 0) {
            jxl_free(ctx, perm);
            return -1;
        }
        jxl_dec_begin(&dec, br);
        if (jxl_read_permutation(ctx, &dec, br, entry_count, 0, perm) != 0 ||
            jxl_dec_finalize(&dec) != 0) {
            JXL_ERR(ctx, "toc: bad permutation");
            jxl_dec_free(&dec);
            jxl_free(ctx, perm);
            return -1;
        }
        jxl_dec_free(&dec);
    }

    jxl_br_zero_pad_to_byte(br);
    sizes = (uint32_t *)jxl_calloc(ctx, entry_count, sizeof(uint32_t));
    if (!sizes) goto done;
    for (i = 0; i < entry_count; i++) {
        sizes[i] = jxl_br_u32(br, 0, 10, 1024, 14, 17408, 22, 4211712, 30);
    }
    jxl_br_zero_pad_to_byte(br);
    if (br->err) {
        JXL_ERR(ctx, "toc: truncated");
        goto done;
    }

    toc->entries = (jxl_toc_entry *)jxl_calloc(ctx, entry_count,
                                               sizeof(jxl_toc_entry));
    if (!toc->entries) goto done;

    acc = jxl_br_bits_read(br) / 8;
    toc->end_off = acc;
    if (permutated) {
        size_t *offs = (size_t *)jxl_calloc(ctx, entry_count, sizeof(size_t));
        if (!offs) goto done;
        for (i = 0; i < entry_count; i++) {
            offs[i] = acc;
            acc += sizes[i];
            toc->total_size += sizes[i];
        }
        for (i = 0; i < entry_count; i++) {
            uint32_t b = perm[i];
            if (b >= entry_count) {
                jxl_free(ctx, offs);
                JXL_ERR(ctx, "toc: permutation out of range");
                goto done;
            }
            toc->entries[i].offset = offs[b];
            toc->entries[i].size = sizes[b];
        }
        jxl_free(ctx, offs);
    } else {
        for (i = 0; i < entry_count; i++) {
            toc->entries[i].offset = acc;
            toc->entries[i].size = sizes[i];
            acc += sizes[i];
            toc->total_size += sizes[i];
        }
    }
    rc = 0;

done:
    jxl_free(ctx, sizes);
    jxl_free(ctx, perm);
    return rc;
}

void jxl_toc_free(jxl_ctx *ctx, jxl_toc *toc) {
    if (!toc) return;
    jxl_free(ctx, toc->entries);
    toc->entries = NULL;
    toc->count = 0;
}

#include <stdio.h>
#include <stdlib.h>

#if !defined(JXL_NO_AVX2) && \
    (defined(__SSE2__) || defined(_M_X64) || defined(_M_AMD64) || \
     (defined(_M_IX86_FP) && _M_IX86_FP >= 2))
#define JXL_MODULAR_AVX2 1
#include <immintrin.h>
#endif

static uint32_t div_ceil_u32(uint32_t a, uint32_t b) {
    return (a + b - 1) / b;
}

static JXL_INLINE_HINT int32_t grad_clamped(int32_t n, int32_t w, int32_t nw) {
    int32_t lo = n < w ? n : w;
    int32_t hi = n < w ? w : n;
    int32_t grad =
        (int32_t)((uint32_t)n + (uint32_t)w - (uint32_t)nw);
    int32_t grad_hi = nw < lo ? hi : grad;
    return nw > hi ? lo : grad_hi;
}

static int32_t sel_pred(int32_t n, int32_t w, int32_t nw) {
    uint64_t dn = (uint64_t)(n > nw ? (int64_t)n - nw : (int64_t)nw - n);
    uint64_t dw = (uint64_t)(w > nw ? (int64_t)w - nw : (int64_t)nw - w);
    return dn < dw ? w : n;
}

void jxl_chanlist_free(jxl_ctx *ctx, jxl_chanlist *cl) {
    if (!cl) return;
    jxl_free(ctx, cl->chans);
    cl->chans = NULL;
    cl->n = cl->cap = 0;
}

static int chanlist_reserve(jxl_ctx *ctx, jxl_chanlist *cl, uint32_t need) {
    uint32_t cap;
    jxl_mchan *nc;
    if (cl->cap >= need) return 0;
    cap = cl->cap ? cl->cap : 8;
    while (cap < need) cap *= 2;
    nc = (jxl_mchan *)jxl_realloc_array(ctx, cl->chans, cl->cap, cap,
                                        sizeof(jxl_mchan));
    if (!nc) return -1;
    cl->chans = nc;
    cl->cap = cap;
    return 0;
}

int jxl_chanlist_push(jxl_ctx *ctx, jxl_chanlist *cl, const jxl_mchan *ch) {
    if (chanlist_reserve(ctx, cl, cl->n + 1) != 0) return -1;
    cl->chans[cl->n++] = *ch;
    return 0;
}

static int chanlist_insert(jxl_ctx *ctx, jxl_chanlist *cl, uint32_t at,
                           const jxl_mchan *ch) {
    if (chanlist_reserve(ctx, cl, cl->n + 1) != 0) return -1;
    memmove(cl->chans + at + 1, cl->chans + at,
            (cl->n - at) * sizeof(jxl_mchan));
    cl->chans[at] = *ch;
    cl->n++;
    return 0;
}

static void chanlist_remove_range(jxl_chanlist *cl, uint32_t from, uint32_t to) {
    if (to <= from) return;
    memmove(cl->chans + from, cl->chans + to, (cl->n - to) * sizeof(jxl_mchan));
    cl->n -= (to - from);
}

static int ma_flat_grow(jxl_ctx *ctx, jxl_ma_flat **flat, uint32_t **bnode,
                        uint32_t *cap, uint32_t need) {
    uint32_t ncap = *cap ? *cap : 16;
    jxl_ma_flat *nf;
    uint32_t *nb;

    if (need <= *cap) return 0;
    while (ncap < need) ncap *= 2;
    nf = (jxl_ma_flat *)jxl_realloc_array(ctx, *flat, *cap, ncap,
                                          sizeof(jxl_ma_flat));
    if (!nf) return -1;
    *flat = nf;
    nb = (uint32_t *)jxl_realloc_array(ctx, *bnode, *cap, ncap,
                                       sizeof(uint32_t));
    if (!nb) return -1;
    *bnode = nb;
    *cap = ncap;
    return 0;
}

static uint32_t ma_fold(const jxl_ma_node *raw, uint32_t i, int fold,
                        int32_t channel, int32_t stream_idx) {
    if (!fold) return i;
    while (raw[i].property == 0 || raw[i].property == 1) {
        int32_t v = raw[i].property == 0 ? channel : stream_idx;
        i = raw[i].child + (v > raw[i].value ? 0u : 1u);
    }
    return i;
}

static int ma_flatten(jxl_ctx *ctx, const jxl_ma_node *raw,
                      uint32_t count, uint32_t root,
                      const jxl_ma_leaf *leaves, int fold,
                      int32_t channel, int32_t stream_idx,
                      jxl_ma_flat **out_flat, uint32_t *out_n) {
    jxl_ma_flat *flat = NULL;
    uint32_t *bnode = NULL;
    uint32_t cap = 0, n = 0, fi;
    int rc = -1;

    if (ma_flat_grow(ctx, &flat, &bnode, &cap, 1) != 0) goto done;
    bnode[0] = ma_fold(raw, root, fold, channel, stream_idx);
    n = 1;

    for (fi = 0; fi < n; fi++) {
        uint32_t b = bnode[fi], base, side;

        if (raw[b].property < 0) {
            flat[fi].property = -1;
            flat[fi].u.leaf = leaves[raw[b].child];
            continue;
        }

        if (n > 4 * count + 4) {
            JXL_ERR(ctx, "modular: MA tree does not flatten");
            goto done;
        }
        if (ma_flat_grow(ctx, &flat, &bnode, &cap, n + 4) != 0) goto done;
        base = n;
        n += 4;
        flat[fi].property = raw[b].property;
        flat[fi].u.dec.split0 = raw[b].value;
        flat[fi].u.dec.child = base;

        for (side = 0; side < 2; side++) {
            uint32_t c = ma_fold(raw, raw[b].child + side, fold,
                                 channel, stream_idx);
            uint32_t slot = base + 2 * side;
            int32_t p, s;

            if (raw[c].property < 0) {
                p = 0;
                s = 0x7fffffff;
                bnode[slot] = c;
                bnode[slot + 1] = c;
            } else {
                p = raw[c].property;
                s = raw[c].value;
                bnode[slot] = ma_fold(raw, raw[c].child, fold,
                                      channel, stream_idx);
                bnode[slot + 1] = ma_fold(raw, raw[c].child + 1, fold,
                                          channel, stream_idx);
            }
            if (side == 0) {
                flat[fi].u.dec.prop1 = (int16_t)p;
                flat[fi].u.dec.split1 = s;
            } else {
                flat[fi].u.dec.prop2 = (int16_t)p;
                flat[fi].u.dec.split2 = s;
            }
        }
    }

    *out_flat = flat;
    *out_n = n;
    flat = NULL;
    rc = 0;

done:
    jxl_free(ctx, bnode);
    jxl_free(ctx, flat);
    return rc;
}

int jxl_ma_config_read(jxl_ctx *ctx, jxl_br *br, jxl_ma_config *ma,
                       size_t node_limit) {
    jxl_dec tree_dec;
    jxl_ma_node *raw = NULL;
    jxl_ma_leaf *leaves = NULL;
    uint32_t cap = 0, count = 0;
    uint32_t lcap = 0;
    size_t nodes_left = 1;
    uint32_t ctx_count = 0;
    uint32_t *dq = NULL;
    uint32_t dq_head = 0, dq_tail = 0;
    int rc = -1;
    uint32_t i;

    memset(ma, 0, sizeof(*ma));
    memset(&tree_dec, 0, sizeof(tree_dec));

    if (jxl_dec_init(ctx, &tree_dec, br, 6) != 0) return -1;
    jxl_dec_begin(&tree_dec, br);

    while (nodes_left > 0) {
        uint32_t property;
        jxl_ma_node node;

        if (count >= (1u << 26) || count > node_limit) {
            JXL_ERR(ctx, "modular: MA tree too large");
            goto done;
        }
        if (count == cap) {
            uint32_t ncap = cap ? cap * 2 : 16;
            jxl_ma_node *nn = (jxl_ma_node *)jxl_realloc_array(
                ctx, raw, cap, ncap, sizeof(jxl_ma_node));
            if (!nn) goto done;
            raw = nn;
            cap = ncap;
        }
        nodes_left--;

        memset(&node, 0, sizeof(node));
        property = jxl_dec_read(&tree_dec, br, 1);
        if (br->err || tree_dec.err) {
            JXL_ERR(ctx, "modular: truncated MA tree");
            goto done;
        }
        if (property > 256) {
            JXL_ERR(ctx, "modular: bad MA property %u", (unsigned)property);
            goto done;
        }
        if (property > 0) {
            node.property = (int32_t)(property - 1);
            node.value = jxl_unpack_signed(jxl_dec_read(&tree_dec, br, 0));
            nodes_left += 2;
        } else {
            uint32_t pred = jxl_dec_read(&tree_dec, br, 2);
            uint32_t mul_log, mul_bits;
            jxl_ma_leaf *leaf;
            if (pred > JXL_PRED_AVG_ALL) {
                JXL_ERR(ctx, "modular: bad predictor %u", (unsigned)pred);
                goto done;
            }
            if (ctx_count == lcap) {
                uint32_t ncap = lcap ? lcap * 2 : 16;
                jxl_ma_leaf *nl = (jxl_ma_leaf *)jxl_realloc_array(
                    ctx, leaves, lcap, ncap, sizeof(jxl_ma_leaf));
                if (!nl) goto done;
                leaves = nl;
                lcap = ncap;
            }

            leaf = &leaves[ctx_count];
            leaf->predictor = (uint8_t)pred;
            leaf->cluster = 0;
            leaf->offset = jxl_unpack_signed(jxl_dec_read(&tree_dec, br, 3));
            mul_log = jxl_dec_read(&tree_dec, br, 4);
            if (mul_log > 30) {
                JXL_ERR(ctx, "modular: bad MA multiplier");
                goto done;
            }
            mul_bits = jxl_dec_read(&tree_dec, br, 5);
            if (mul_bits > (1u << (31 - mul_log)) - 2) {
                JXL_ERR(ctx, "modular: bad MA multiplier bits");
                goto done;
            }
            leaf->multiplier = (mul_bits + 1) << mul_log;
            node.property = -1;
            node.child = ctx_count;
            ctx_count++;
        }
        raw[count++] = node;
    }
    if (getenv("JXL_DEBUG_TREE")) {
        uint32_t k;
        fprintf(stderr, "tree: %u nodes, %u ctx, prefix=%d nclusters=%u\n",
                (unsigned)count, (unsigned)ctx_count, tree_dec.use_prefix,
                (unsigned)tree_dec.num_clusters);
        for (k = 0; k < count && k < 12; k++) {
            if (raw[k].property >= 0) {
                fprintf(stderr, "  [%u] prop=%d value=%d\n",
                        (unsigned)k, (int)raw[k].property, (int)raw[k].value);
            } else {
                const jxl_ma_leaf *lf = &leaves[raw[k].child];
                fprintf(stderr, "  [%u] leaf ctx=%u pred=%u off=%d mul=%u\n",
                        (unsigned)k, (unsigned)raw[k].child,
                        (unsigned)lf->predictor, (int)lf->offset,
                        (unsigned)lf->multiplier);
            }
        }
    }
    if (jxl_dec_finalize(&tree_dec) != 0) {
        JXL_ERR(ctx, "modular: bad MA tree ANS final state (state=0x%x)",
                (unsigned)tree_dec.state);
        goto done;
    }
    jxl_dec_free(&tree_dec);
    memset(&tree_dec, 0, sizeof(tree_dec));

    if (jxl_dec_init(ctx, &ma->dec, br, ctx_count) != 0) goto done;

    if (ctx_count > ma->dec.num_dist) {
        JXL_ERR(ctx, "modular: MA leaf context out of range");
        goto done;
    }
    for (i = 0; i < ctx_count; i++) leaves[i].cluster = ma->dec.clusters[i];

    dq = (uint32_t *)jxl_calloc(ctx, count + 2, sizeof(uint32_t));
    if (!dq) goto done;
    for (i = count; i > 0; i--) {
        uint32_t idx = i - 1;
        if (raw[idx].property >= 0) {
            uint32_t right, left;
            if (dq_tail - dq_head < 2) {
                JXL_ERR(ctx, "modular: malformed MA tree");
                goto done;
            }
            right = dq[dq_head++];
            left = dq[dq_head++];

            if (right != left + 1) {
                JXL_ERR(ctx, "modular: malformed MA tree (split children)");
                goto done;
            }
            raw[idx].child = left;
        }
        dq[dq_tail++] = idx;
    }
    if (dq_tail - dq_head != 1) {
        JXL_ERR(ctx, "modular: malformed MA tree (%u roots)",
                (unsigned)(dq_tail - dq_head));
        goto done;
    }

    for (i = 0; i < count; i++) {
        if (raw[i].property < 0) continue;
        if (raw[i].child <= i || raw[i].child + 1 >= count) {
            JXL_ERR(ctx, "modular: MA tree has a cycle at node %u",
                    (unsigned)i);
            goto done;
        }
    }
    if (ma_flatten(ctx, raw, count, dq[dq_head], leaves, 0, 0, 0,
                   &ma->flat, &ma->nflat) != 0)
        goto done;

    ma->raw = raw;
    ma->nraw = count;
    ma->root = dq[dq_head];
    ma->leaves = leaves;
    raw = NULL;
    leaves = NULL;
    ma->valid = 1;
    rc = 0;

done:
    jxl_free(ctx, dq);
    jxl_free(ctx, raw);
    jxl_free(ctx, leaves);
    jxl_dec_free(&tree_dec);
    if (rc != 0) jxl_ma_config_free(ctx, ma);
    return rc;
}

void jxl_ma_config_free(jxl_ctx *ctx, jxl_ma_config *ma) {
    if (!ma) return;
    jxl_free(ctx, ma->flat);
    ma->flat = NULL;
    ma->nflat = 0;
    jxl_free(ctx, ma->raw);
    ma->raw = NULL;
    jxl_free(ctx, ma->leaves);
    ma->leaves = NULL;
    jxl_free(ctx, (void *)ma->wp_lut);
    ma->wp_lut = NULL;
    jxl_free(ctx, ma->wp_cluster_lut);
    ma->wp_cluster_lut = NULL;
    ma->nraw = 0;
    ma->root = 0;
    jxl_dec_free(&ma->dec);
    ma->valid = 0;
}

static const uint32_t jxl_div_lookup[65] = {
    0u, 16777216u, 8388608u, 5592405u, 4194304u, 3355443u,
    2796202u, 2396745u, 2097152u, 1864135u, 1677721u, 1525201u,
    1398101u, 1290555u, 1198372u, 1118481u, 1048576u, 986895u,
    932067u, 883011u, 838860u, 798915u, 762600u, 729444u,
    699050u, 671088u, 645277u, 621378u, 599186u, 578524u,
    559240u, 541200u, 524288u, 508400u, 493447u, 479349u,
    466033u, 453438u, 441505u, 430185u, 419430u, 409200u,
    399457u, 390167u, 381300u, 372827u, 364722u, 356962u,
    349525u, 342392u, 335544u, 328965u, 322638u, 316551u,
    310689u, 305040u, 299593u, 294337u, 289262u, 284359u,
    279620u, 275036u, 270600u, 266305u, 262144u
};

typedef struct {
    int64_t prediction;
    int32_t max_error;
    int64_t subpred[4];
} jxl_sc_result;

typedef struct {
    uint32_t width, x, y;
    int32_t *true_err_row;
    uint32_t *subpred_err_row;
    jxl_wp_header wp;
    int default_wp;
    int32_t true_err_w, true_err_nw, true_err_n, true_err_ne;
    uint32_t subpred_err_nw_ww[4], subpred_err_n_w[4], subpred_err_ne[4];
} jxl_sc_pred;

typedef struct {
    uint32_t width;
    int32_t *prev_row;
    int32_t *curr_row;
    uint32_t prev_len, curr_len;
    uint32_t x, y;
    int32_t w, n, nw, prev_grad;

    int use_sc;
    jxl_sc_pred sc;

    const jxl_mchan **prev_chans;
    uint32_t nprev;
} jxl_pred_state;

static void pred_state_free(jxl_ctx *ctx, jxl_pred_state *ps) {
    jxl_free(ctx, ps->prev_row);
    jxl_free(ctx, ps->curr_row);
    jxl_free(ctx, ps->sc.true_err_row);
    jxl_free(ctx, ps->sc.subpred_err_row);
    memset(ps, 0, sizeof(*ps));
}

static int pred_state_reset(jxl_ctx *ctx, jxl_pred_state *ps, uint32_t width,
                            int need_neighbor_rows,
                            const jxl_wp_header *wp,
                            const jxl_mchan **prev_chans, uint32_t nprev) {
    pred_state_free(ctx, ps);

    ps->width = width;
    if (need_neighbor_rows) {
        ps->prev_row =
            (int32_t *)jxl_calloc(ctx, width ? width : 1, sizeof(int32_t));
        ps->curr_row =
            (int32_t *)jxl_calloc(ctx, width ? width : 1, sizeof(int32_t));
        if (!ps->prev_row || !ps->curr_row) return -1;
    }
    ps->prev_chans = prev_chans;
    ps->nprev = nprev;
    if (wp) {
        ps->use_sc = 1;
        ps->sc.width = width;
        ps->sc.wp = *wp;
        ps->sc.default_wp =
            wp->p1 == 16 && wp->p2 == 10 &&
            wp->p3a == 7 && wp->p3b == 7 && wp->p3c == 7 &&
            wp->p3d == 0 && wp->p3e == 0 &&
            wp->w0 == 13 && wp->w1 == 12 &&
            wp->w2 == 12 && wp->w3 == 12;
        ps->sc.true_err_row =
            (int32_t *)jxl_calloc(ctx, width ? width : 1, sizeof(int32_t));
        ps->sc.subpred_err_row =
            (uint32_t *)jxl_calloc(ctx, (size_t)(width ? width : 1) * 4,
                                   sizeof(uint32_t));
        if (!ps->sc.true_err_row || !ps->sc.subpred_err_row) return -1;
    }
    return 0;
}

static int32_t pred_nn(const jxl_pred_state *ps) {
    return ps->x < ps->curr_len ? ps->curr_row[ps->x] : ps->n;
}
static int32_t pred_ne(const jxl_pred_state *ps) {
    if (ps->prev_len == 0 || ps->x + 1 >= ps->width) return ps->n;
    return ps->prev_row[ps->x + 1];
}
static int32_t pred_nee(const jxl_pred_state *ps) {
    if (ps->prev_len == 0 || ps->x + 2 >= ps->width) return pred_ne(ps);
    return ps->prev_row[ps->x + 2];
}
static int32_t pred_ww(const jxl_pred_state *ps) {
    if (ps->x >= 2) return ps->curr_row[ps->x - 2];
    return ps->w;
}

static JXL_INLINE_HINT uint32_t sc_weight_one(uint32_t err_sum, uint32_t wn,
                                              const uint32_t *dl) {
    if (err_sum <= 62)
        return 4 + wn * dl[err_sum + 1];
    uint64_t v = ((uint64_t)err_sum + 1) >> 5;
    uint32_t shift = v > 1 ? jxl_floor_log2_u64(v) : 0;
    uint32_t idx = (err_sum >> shift) + 1;
    if (idx > 64) idx = 64;
    return 4 + ((wn * dl[idx]) >> shift);
}

static void sc_predict(const jxl_sc_pred *sc, int32_t n, int32_t nw, int32_t ne,
                       int32_t w, int32_t nn, jxl_sc_result *out) {
    const uint32_t *dl = jxl_div_lookup;
    int64_t te_w = sc->true_err_w, te_nw = sc->true_err_nw;
    int64_t te_n = sc->true_err_n, te_ne = sc->true_err_ne;
    int64_t n3, nw3, ne3, w3, nn3;

    uint32_t es0, es1, es2, es3;
    uint32_t wt0, wt1, wt2, wt3;
    uint32_t sum_weights;
    int log_weight = 0;
    int64_t s;

    int64_t sp0, sp1, sp2, sp3, pred;

    es0 = sc->subpred_err_nw_ww[0] + sc->subpred_err_n_w[0] + sc->subpred_err_ne[0];
    es1 = sc->subpred_err_nw_ww[1] + sc->subpred_err_n_w[1] + sc->subpred_err_ne[1];
    es2 = sc->subpred_err_nw_ww[2] + sc->subpred_err_n_w[2] + sc->subpred_err_ne[2];
    es3 = sc->subpred_err_nw_ww[3] + sc->subpred_err_n_w[3] + sc->subpred_err_ne[3];

#define JXL_SC_WEIGHT(es, wn) (                                                   sc_weight_one((es), (wn), dl))

    if (sc->default_wp) {
        wt0 = JXL_SC_WEIGHT(es0, 13);
        wt1 = JXL_SC_WEIGHT(es1, 12);
        wt2 = JXL_SC_WEIGHT(es2, 12);
        wt3 = JXL_SC_WEIGHT(es3, 12);
    } else {
        wt0 = JXL_SC_WEIGHT(es0, sc->wp.w0);
        wt1 = JXL_SC_WEIGHT(es1, sc->wp.w1);
        wt2 = JXL_SC_WEIGHT(es2, sc->wp.w2);
        wt3 = JXL_SC_WEIGHT(es3, sc->wp.w3);
    }
#undef JXL_SC_WEIGHT

    sum_weights = wt0 + wt1 + wt2 + wt3;
    {
        uint32_t v = sum_weights >> 4;
        if (v > 1) log_weight = (int)jxl_floor_log2_u64(v);
    }
    wt0 >>= log_weight; wt1 >>= log_weight;
    wt2 >>= log_weight; wt3 >>= log_weight;
    sum_weights = wt0 + wt1 + wt2 + wt3;

    n3 = (int64_t)n << 3;
    nw3 = (int64_t)nw << 3;
    ne3 = (int64_t)ne << 3;
    w3 = (int64_t)w << 3;
    nn3 = (int64_t)nn << 3;
    sp0 = w3 + ne3 - n3;
    if (sc->default_wp) {
        sp1 = n3 - (((te_w + te_n + te_ne) * 16) >> 5);
        sp2 = w3 - (((te_w + te_n + te_nw) * 10) >> 5);
        sp3 = n3 - (((te_nw + te_n + te_ne) * 7) >> 5);
    } else {
        sp1 = n3 - (((te_w + te_n + te_ne) * (int64_t)sc->wp.p1) >> 5);
        sp2 = w3 - (((te_w + te_n + te_nw) * (int64_t)sc->wp.p2) >> 5);
        sp3 = n3 - ((te_nw * (int64_t)sc->wp.p3a +
                     te_n * (int64_t)sc->wp.p3b +
                     te_ne * (int64_t)sc->wp.p3c +
                     (nn3 - n3) * (int64_t)sc->wp.p3d +
                     (nw3 - w3) * (int64_t)sc->wp.p3e) >> 5);
    }

    s = ((int64_t)sum_weights >> 1) - 1;
    s += sp0 * (int64_t)wt0;
    s += sp1 * (int64_t)wt1;
    s += sp2 * (int64_t)wt2;
    s += sp3 * (int64_t)wt3;
    pred = (s * (int64_t)dl[sum_weights > 64 ? 64 : sum_weights]) >> 24;

    if (((te_n ^ te_w) | (te_n ^ te_nw)) <= 0) {
        int64_t lo = n3 < w3 ? n3 : w3;
        int64_t hi = n3 > w3 ? n3 : w3;
        if (ne3 < lo) lo = ne3;
        if (ne3 > hi) hi = ne3;
        if (pred < lo) pred = lo;
        if (pred > hi) pred = hi;
    }

    out->subpred[0] = sp0;
    out->subpred[1] = sp1;
    out->subpred[2] = sp2;
    out->subpred[3] = sp3;
    out->prediction = pred;

    {
        int64_t max_error = te_w;
        int64_t a = te_n < 0 ? -te_n : te_n;
        int64_t b = max_error < 0 ? -max_error : max_error;
        if (a > b) max_error = te_n;
        a = te_nw < 0 ? -te_nw : te_nw;
        b = max_error < 0 ? -max_error : max_error;
        if (a > b) max_error = te_nw;
        a = te_ne < 0 ? -te_ne : te_ne;
        b = max_error < 0 ? -max_error : max_error;
        if (a > b) max_error = te_ne;
        out->max_error = (int32_t)max_error;
    }
}

static void sc_record(jxl_sc_pred *sc, const jxl_sc_result *pred, int32_t sample) {
    int64_t s = (int64_t)sample << 3;
    int64_t true_err = pred->prediction - s;
    uint32_t subpred_err[4];
    uint32_t x = sc->x;
    uint32_t next_x = x + 1;
    uint32_t *subpred_row = sc->subpred_err_row + (size_t)x * 4;
    int i;

    for (i = 0; i < 4; i++) {
        int64_t d = pred->subpred[i] - s;
        if (d < 0) d = -d;
        subpred_err[i] = (uint32_t)((d + 3) >> 3);
    }
    sc->true_err_row[x] = (int32_t)true_err;
    for (i = 0; i < 4; i++) subpred_row[i] = subpred_err[i];
    sc->x = next_x;

    if (next_x >= sc->width) {
        sc->y++;
        sc->x = 0;
        sc->true_err_w = 0;
        sc->true_err_n = sc->true_err_row[0];
        sc->true_err_nw = sc->true_err_n;
        for (i = 0; i < 4; i++) {
            sc->subpred_err_n_w[i] = sc->subpred_err_row[i];
            sc->subpred_err_nw_ww[i] = sc->subpred_err_n_w[i];
        }
        if (sc->width <= 1) {
            sc->true_err_ne = sc->true_err_n;
            for (i = 0; i < 4; i++) sc->subpred_err_ne[i] = sc->subpred_err_n_w[i];
        } else {
            sc->true_err_ne = sc->true_err_row[1];
            for (i = 0; i < 4; i++) sc->subpred_err_ne[i] = sc->subpred_err_row[4 + i];
        }
    } else {
        sc->true_err_w = (int32_t)true_err;
        sc->true_err_nw = sc->true_err_n;
        sc->true_err_n = sc->true_err_ne;
        for (i = 0; i < 4; i++) {
            sc->subpred_err_nw_ww[i] = sc->subpred_err_n_w[i];
            sc->subpred_err_n_w[i] = sc->subpred_err_ne[i];
            sc->subpred_err_n_w[i] += subpred_err[i];
        }
        if (next_x + 1 >= sc->width) {
            sc->true_err_ne = sc->true_err_n;
            for (i = 0; i < 4; i++) sc->subpred_err_ne[i] = sc->subpred_err_n_w[i];
        } else if (sc->y != 0) {
            sc->true_err_ne = sc->true_err_row[next_x + 1];
            for (i = 0; i < 4; i++)
                sc->subpred_err_ne[i] =
                    sc->subpred_err_row[(size_t)(next_x + 1) * 4 + i];
        }
    }
}

static JXL_INLINE_HINT void
sc_record_nec(jxl_sc_pred *sc, const jxl_sc_result *pred, int32_t sample) {
    int64_t s = (int64_t)sample << 3;
    int64_t true_err = pred->prediction - s;
    uint32_t subpred_err0, subpred_err1, subpred_err2, subpred_err3;
    uint32_t x = sc->x;
    uint32_t next_x = x + 1;
    uint32_t *subpred_row = sc->subpred_err_row + (size_t)x * 4;
    int64_t d;

    d = pred->subpred[0] - s;
    if (d < 0) d = -d;
    subpred_err0 = (uint32_t)((d + 3) >> 3);
    d = pred->subpred[1] - s;
    if (d < 0) d = -d;
    subpred_err1 = (uint32_t)((d + 3) >> 3);
    d = pred->subpred[2] - s;
    if (d < 0) d = -d;
    subpred_err2 = (uint32_t)((d + 3) >> 3);
    d = pred->subpred[3] - s;
    if (d < 0) d = -d;
    subpred_err3 = (uint32_t)((d + 3) >> 3);

    sc->true_err_row[x] = (int32_t)true_err;
    subpred_row[0] = subpred_err0;
    subpred_row[1] = subpred_err1;
    subpred_row[2] = subpred_err2;
    subpred_row[3] = subpred_err3;
    sc->x = next_x;
    sc->true_err_w = (int32_t)true_err;
    sc->true_err_nw = sc->true_err_n;
    sc->true_err_n = sc->true_err_ne;
    sc->subpred_err_nw_ww[0] = sc->subpred_err_n_w[0];
    sc->subpred_err_nw_ww[1] = sc->subpred_err_n_w[1];
    sc->subpred_err_nw_ww[2] = sc->subpred_err_n_w[2];
    sc->subpred_err_nw_ww[3] = sc->subpred_err_n_w[3];
    sc->subpred_err_n_w[0] = sc->subpred_err_ne[0] + subpred_err0;
    sc->subpred_err_n_w[1] = sc->subpred_err_ne[1] + subpred_err1;
    sc->subpred_err_n_w[2] = sc->subpred_err_ne[2] + subpred_err2;
    sc->subpred_err_n_w[3] = sc->subpred_err_ne[3] + subpred_err3;
    sc->true_err_ne = sc->true_err_row[next_x + 1];
    sc->subpred_err_ne[0] = sc->subpred_err_row[(size_t)(next_x + 1) * 4];
    sc->subpred_err_ne[1] = sc->subpred_err_row[(size_t)(next_x + 1) * 4 + 1];
    sc->subpred_err_ne[2] = sc->subpred_err_row[(size_t)(next_x + 1) * 4 + 2];
    sc->subpred_err_ne[3] = sc->subpred_err_row[(size_t)(next_x + 1) * 4 + 3];
}

typedef struct {
    int32_t cache[16];
    jxl_sc_result sc;
    int has_sc;
} jxl_props;

static int32_t chan_get(const jxl_mchan *c, uint32_t x, uint32_t y) {
    return c->data[(size_t)y * c->stride + x];
}

static void props_compute(const jxl_pred_state *ps, jxl_props *pr,
                          int32_t channel, int32_t stream_idx) {
    int32_t w_nw = (int32_t)((uint32_t)ps->w - (uint32_t)ps->nw);
    if (ps->use_sc) {
        sc_predict(&ps->sc, ps->n, ps->nw, pred_ne(ps), ps->w, pred_nn(ps),
                   &pr->sc);
        pr->has_sc = 1;
    } else {
        pr->has_sc = 0;
        pr->sc.prediction = 0;
        pr->sc.max_error = 0;
    }

    pr->cache[0] = channel;
    pr->cache[1] = stream_idx;
    pr->cache[2] = (int32_t)ps->y;
    pr->cache[3] = (int32_t)ps->x;
    pr->cache[4] = (int32_t)(ps->n < 0 ? 0u - (uint32_t)ps->n : (uint32_t)ps->n);
    pr->cache[5] = (int32_t)(ps->w < 0 ? 0u - (uint32_t)ps->w : (uint32_t)ps->w);
    pr->cache[6] = ps->n;
    pr->cache[7] = ps->w;
    pr->cache[8] = (int32_t)((uint32_t)ps->w - (uint32_t)ps->prev_grad);
    pr->cache[9] = (int32_t)((uint32_t)w_nw + (uint32_t)ps->n);
    pr->cache[10] = w_nw;
    pr->cache[11] = (int32_t)((uint32_t)ps->nw - (uint32_t)ps->n);
    pr->cache[12] = (int32_t)((uint32_t)ps->n - (uint32_t)pred_ne(ps));
    pr->cache[13] = (int32_t)((uint32_t)ps->n - (uint32_t)pred_nn(ps));
    pr->cache[14] = (int32_t)((uint32_t)ps->w - (uint32_t)pred_ww(ps));
    pr->cache[15] = pr->sc.max_error;
}

static JXL_INLINE_HINT void
props_compute_nec(const jxl_pred_state *ps, jxl_props *pr,
                  int32_t channel, int32_t stream_idx) {
    uint32_t x = ps->x;
    int32_t ne = ps->prev_row[x + 1];
    int32_t nn = ps->curr_row[x];
    int32_t ww = ps->curr_row[x - 2];
    int32_t w_nw = (int32_t)((uint32_t)ps->w - (uint32_t)ps->nw);
    if (ps->use_sc) {
        sc_predict(&ps->sc, ps->n, ps->nw, ne, ps->w, nn, &pr->sc);
        pr->has_sc = 1;
    } else {
        pr->has_sc = 0;
        pr->sc.prediction = 0;
        pr->sc.max_error = 0;
    }
    pr->cache[0] = channel;
    pr->cache[1] = stream_idx;
    pr->cache[2] = (int32_t)ps->y;
    pr->cache[3] = (int32_t)x;
    pr->cache[4] =
        (int32_t)(ps->n < 0 ? 0u - (uint32_t)ps->n : (uint32_t)ps->n);
    pr->cache[5] =
        (int32_t)(ps->w < 0 ? 0u - (uint32_t)ps->w : (uint32_t)ps->w);
    pr->cache[6] = ps->n;
    pr->cache[7] = ps->w;
    pr->cache[8] = (int32_t)((uint32_t)ps->w - (uint32_t)ps->prev_grad);
    pr->cache[9] = (int32_t)((uint32_t)w_nw + (uint32_t)ps->n);
    pr->cache[10] = w_nw;
    pr->cache[11] = (int32_t)((uint32_t)ps->nw - (uint32_t)ps->n);
    pr->cache[12] = (int32_t)((uint32_t)ps->n - (uint32_t)ne);
    pr->cache[13] = (int32_t)((uint32_t)ps->n - (uint32_t)nn);
    pr->cache[14] = (int32_t)((uint32_t)ps->w - (uint32_t)ww);
    pr->cache[15] = pr->sc.max_error;
}

static JXL_INLINE_HINT void
props_compute_nw(const jxl_pred_state *ps, jxl_props *pr,
                 int32_t channel, int32_t stream_idx) {
    int32_t n = ps->n, w = ps->w;
    int32_t w_nw = (int32_t)((uint32_t)w - (uint32_t)ps->nw);
    pr->cache[0] = channel;
    pr->cache[1] = stream_idx;
    pr->cache[4] = (int32_t)(n < 0 ? 0u - (uint32_t)n : (uint32_t)n);
    pr->cache[5] = (int32_t)(w < 0 ? 0u - (uint32_t)w : (uint32_t)w);
    pr->cache[6] = n;
    pr->cache[7] = w;
    pr->cache[8] = (int32_t)((uint32_t)w - (uint32_t)ps->prev_grad);
    pr->cache[9] = (int32_t)((uint32_t)w_nw + (uint32_t)n);
    pr->has_sc = 0;
}

static JXL_INLINE_HINT void
props_compute_grad_wp(const jxl_pred_state *ps, jxl_props *pr,
                      int32_t channel, int32_t stream_idx) {
    int32_t ne = pred_ne(ps);
    int32_t w_nw = (int32_t)((uint32_t)ps->w - (uint32_t)ps->nw);
    if (ps->use_sc) {
        sc_predict(&ps->sc, ps->n, ps->nw, ne, ps->w, pred_nn(ps), &pr->sc);
        pr->has_sc = 1;
    } else {
        pr->has_sc = 0;
        pr->sc.prediction = 0;
        pr->sc.max_error = 0;
    }
    pr->cache[0] = channel;
    pr->cache[1] = stream_idx;
    pr->cache[9] = (int32_t)((uint32_t)w_nw + (uint32_t)ps->n);
    pr->cache[10] = w_nw;
    pr->cache[11] = (int32_t)((uint32_t)ps->nw - (uint32_t)ps->n);
    pr->cache[12] = (int32_t)((uint32_t)ps->n - (uint32_t)ne);
    pr->cache[15] = pr->sc.max_error;
}

static JXL_INLINE_HINT void
props_compute_grad_wp_nec(const jxl_pred_state *ps, jxl_props *pr,
                          int32_t channel, int32_t stream_idx) {
    uint32_t x = ps->x;
    int32_t ne = ps->prev_row[x + 1];
    int32_t w_nw = (int32_t)((uint32_t)ps->w - (uint32_t)ps->nw);
    if (ps->use_sc) {
        sc_predict(&ps->sc, ps->n, ps->nw, ne, ps->w, ps->curr_row[x],
                   &pr->sc);
        pr->has_sc = 1;
    } else {
        pr->has_sc = 0;
        pr->sc.prediction = 0;
        pr->sc.max_error = 0;
    }
    pr->cache[0] = channel;
    pr->cache[1] = stream_idx;
    pr->cache[9] = (int32_t)((uint32_t)w_nw + (uint32_t)ps->n);
    pr->cache[10] = w_nw;
    pr->cache[11] = (int32_t)((uint32_t)ps->nw - (uint32_t)ps->n);
    pr->cache[12] = (int32_t)((uint32_t)ps->n - (uint32_t)ne);
    pr->cache[15] = pr->sc.max_error;
}

static int32_t props_get_extra(const jxl_pred_state *ps, uint32_t prop_extra) {
    uint32_t idx = prop_extra / 4;
    uint32_t sub = prop_extra % 4;
    const jxl_mchan *ch;
    uint32_t x = ps->x, y = ps->y;
    int32_t c, g;

    if (idx >= ps->nprev) return 0;
    ch = ps->prev_chans[idx];
    if (x >= ch->w || y >= ch->h) return 0;
    c = chan_get(ch, x, y);
    if (sub == 0) return (int32_t)(c < 0 ? 0u - (uint32_t)c : (uint32_t)c);
    if (sub == 1) return c;

    if (x == 0 && y == 0) g = 0;
    else if (x == 0) g = chan_get(ch, 0, y - 1);
    else if (y == 0) g = chan_get(ch, x - 1, 0);
    else {
        int32_t nw = chan_get(ch, x - 1, y - 1);
        int32_t n = chan_get(ch, x, y - 1);
        int32_t w = chan_get(ch, x - 1, y);
        g = grad_clamped(n, w, nw);
    }
    if (sub == 2) {
        int64_t d = (int64_t)c - g;
        return (int32_t)(d < 0 ? -d : d);
    }
    return (int32_t)((uint32_t)c - (uint32_t)g);
}

static int32_t predict_sample(const jxl_pred_state *ps, const jxl_props *pr,
                              uint8_t predictor) {
    switch (predictor) {
        case JXL_PRED_ZERO: return 0;
        case JXL_PRED_WEST: return ps->w;
        case JXL_PRED_NORTH: return ps->n;
        case JXL_PRED_AVG_W_N:
            return (int32_t)(((int64_t)ps->w + ps->n) / 2);
        case JXL_PRED_SELECT: {
            int32_t n = ps->n, w = ps->w, nw = ps->nw;
            uint64_t dn = (uint64_t)(n > nw ? (int64_t)n - nw : (int64_t)nw - n);
            uint64_t dw = (uint64_t)(w > nw ? (int64_t)w - nw : (int64_t)nw - w);
            return dn < dw ? w : n;
        }
        case JXL_PRED_GRADIENT: return grad_clamped(ps->n, ps->w, ps->nw);
        case JXL_PRED_SELF_CORRECTING:
            return (int32_t)((pr->sc.prediction + 3) >> 3);
        case JXL_PRED_NORTH_EAST: return pred_ne(ps);
        case JXL_PRED_NORTH_WEST: return ps->nw;
        case JXL_PRED_WEST_WEST: return pred_ww(ps);
        case JXL_PRED_AVG_W_NW:
            return (int32_t)(((int64_t)ps->w + ps->nw) / 2);
        case JXL_PRED_AVG_N_NW:
            return (int32_t)(((int64_t)ps->n + ps->nw) / 2);
        case JXL_PRED_AVG_N_NE:
            return (int32_t)(((int64_t)ps->n + pred_ne(ps)) / 2);
        default: {
            int64_t n = ps->n, w = ps->w;
            int64_t nn = pred_nn(ps), ww = pred_ww(ps);
            int64_t nee = pred_nee(ps), ne = pred_ne(ps);
            return (int32_t)((6 * n - 2 * nn + 7 * w + ww + nee + 3 * ne + 8) / 16);
        }
    }
}

static JXL_INLINE_HINT int32_t
predict_sample_nec(const jxl_pred_state *ps, const jxl_props *pr,
                   uint8_t predictor) {
    uint32_t x = ps->x;
    int32_t ne = ps->prev_row[x + 1];
    switch (predictor) {
        case JXL_PRED_ZERO: return 0;
        case JXL_PRED_WEST: return ps->w;
        case JXL_PRED_NORTH: return ps->n;
        case JXL_PRED_AVG_W_N:
            return (int32_t)(((int64_t)ps->w + ps->n) / 2);
        case JXL_PRED_SELECT: {
            int32_t n = ps->n, w = ps->w, nw = ps->nw;
            uint64_t dn =
                (uint64_t)(n > nw ? (int64_t)n - nw : (int64_t)nw - n);
            uint64_t dw =
                (uint64_t)(w > nw ? (int64_t)w - nw : (int64_t)nw - w);
            return dn < dw ? w : n;
        }
        case JXL_PRED_GRADIENT:
            return grad_clamped(ps->n, ps->w, ps->nw);
        case JXL_PRED_SELF_CORRECTING:
            return (int32_t)((pr->sc.prediction + 3) >> 3);
        case JXL_PRED_NORTH_EAST: return ne;
        case JXL_PRED_NORTH_WEST: return ps->nw;
        case JXL_PRED_WEST_WEST: return ps->curr_row[x - 2];
        case JXL_PRED_AVG_W_NW:
            return (int32_t)(((int64_t)ps->w + ps->nw) / 2);
        case JXL_PRED_AVG_N_NW:
            return (int32_t)(((int64_t)ps->n + ps->nw) / 2);
        case JXL_PRED_AVG_N_NE:
            return (int32_t)(((int64_t)ps->n + ne) / 2);
        default: {
            int64_t n = ps->n, w = ps->w;
            int64_t nn = ps->curr_row[x];
            int64_t ww = ps->curr_row[x - 2];
            int64_t nee = ps->prev_row[x + 2];
            return (int32_t)((6 * n - 2 * nn + 7 * w + ww + nee +
                              3 * (int64_t)ne + 8) / 16);
        }
    }
}

static void pred_record(jxl_pred_state *ps, const jxl_props *pr, int32_t sample) {
    if (ps->use_sc && pr->has_sc) sc_record(&ps->sc, &pr->sc, sample);

    ps->curr_row[ps->x] = sample;
    if (ps->x >= ps->curr_len) ps->curr_len = ps->x + 1;
    ps->x++;

    if (ps->x >= ps->width) {
        int32_t *tmp = ps->prev_row;
        uint32_t tlen = ps->prev_len;
        ps->y++;
        ps->x = 0;
        ps->prev_row = ps->curr_row;
        ps->prev_len = ps->curr_len;
        ps->curr_row = tmp;
        ps->curr_len = tlen;
        ps->prev_grad = 0;
        ps->n = ps->prev_row[0];
        ps->w = ps->n;
        ps->nw = ps->n;
    } else {
        ps->prev_grad = pr->cache[9];
        ps->w = sample;
        if (ps->prev_len == 0) {
            ps->nw = sample;
            ps->n = sample;
        } else {
            ps->nw = ps->n;
            ps->n = ps->prev_row[ps->x];
        }
    }
}

static JXL_INLINE_HINT void
pred_record_nec(jxl_pred_state *ps, const jxl_props *pr, int32_t sample) {
    uint32_t x = ps->x;
    if (ps->use_sc && pr->has_sc) sc_record_nec(&ps->sc, &pr->sc, sample);
    ps->curr_row[x] = sample;
    ps->x = x + 1;
    ps->prev_grad = pr->cache[9];
    ps->w = sample;
    ps->nw = ps->n;
    ps->n = ps->prev_row[x + 1];
}

static void wp_header_read(jxl_br *br, jxl_wp_header *wp) {
    wp->p1 = 16; wp->p2 = 10;
    wp->p3a = 7; wp->p3b = 7; wp->p3c = 7; wp->p3d = 0; wp->p3e = 0;
    wp->w0 = 13; wp->w1 = 12; wp->w2 = 12; wp->w3 = 12;
    if (jxl_br_bool(br)) return;
    wp->p1 = jxl_br_read(br, 5);
    wp->p2 = jxl_br_read(br, 5);
    wp->p3a = jxl_br_read(br, 5);
    wp->p3b = jxl_br_read(br, 5);
    wp->p3c = jxl_br_read(br, 5);
    wp->p3d = jxl_br_read(br, 5);
    wp->p3e = jxl_br_read(br, 5);
    wp->w0 = jxl_br_read(br, 4);
    wp->w1 = jxl_br_read(br, 4);
    wp->w2 = jxl_br_read(br, 4);
    wp->w3 = jxl_br_read(br, 4);
}

static int transform_read(jxl_ctx *ctx, jxl_br *br, jxl_transform *tr) {
    uint32_t id = jxl_br_read(br, 2);
    memset(tr, 0, sizeof(*tr));
    tr->kind = (int)id;
    if (id == 0) {
        tr->begin_c = jxl_br_u32(br, 0, 3, 8, 6, 72, 10, 1096, 13);
        tr->rct_type = jxl_br_u32(br, 6, 0, 0, 2, 2, 4, 10, 6);
        if (tr->rct_type >= 42) {
            JXL_ERR(ctx, "modular: bad RCT type %u", (unsigned)tr->rct_type);
            return -1;
        }
    } else if (id == 1) {
        tr->begin_c = jxl_br_u32(br, 0, 3, 8, 6, 72, 10, 1096, 13);
        tr->num_c = jxl_br_u32(br, 1, 0, 3, 0, 4, 0, 1, 13);
        tr->nb_colours = jxl_br_u32(br, 0, 8, 256, 10, 1280, 12, 5376, 16);
        tr->nb_deltas = jxl_br_u32(br, 0, 0, 1, 8, 257, 10, 1281, 16);
        tr->d_pred = (uint8_t)jxl_br_read(br, 4);
        if (tr->d_pred > JXL_PRED_AVG_ALL) {
            JXL_ERR(ctx, "modular: bad palette predictor");
            return -1;
        }
    } else if (id == 2) {
        uint32_t i;
        tr->nsp = jxl_br_u32(br, 0, 0, 1, 4, 9, 6, 41, 8);
        if (tr->nsp) {
            tr->sp = (jxl_sq_param *)jxl_calloc(ctx, tr->nsp, sizeof(jxl_sq_param));
            if (!tr->sp) return -1;
        }
        for (i = 0; i < tr->nsp; i++) {
            tr->sp[i].horizontal = jxl_br_bool(br);
            tr->sp[i].in_place = jxl_br_bool(br);
            tr->sp[i].begin_c = jxl_br_u32(br, 0, 3, 8, 6, 72, 10, 1096, 13);
            tr->sp[i].num_c = jxl_br_u32(br, 1, 0, 2, 0, 3, 0, 4, 4);
        }
    } else {
        JXL_ERR(ctx, "modular: invalid transform id %u", (unsigned)id);
        return -1;
    }
    return br->err ? -1 : 0;
}

static int squeeze_default_params(jxl_ctx *ctx, jxl_transform *tr,
                                  const jxl_chanlist *cl) {
    uint32_t first = cl->nb_meta;
    uint32_t w, h;
    uint32_t cap = 32, n = 0;
    jxl_sq_param *sp;

    if (tr->nsp != 0) return 0;
    if (first >= cl->n) return -1;
    w = cl->chans[first].w;
    h = cl->chans[first].h;

    sp = (jxl_sq_param *)jxl_calloc(ctx, cap, sizeof(jxl_sq_param));
    if (!sp) return -1;

    if (cl->n - first >= 3) {
        const jxl_mchan *next = &cl->chans[first + 1];
        if (next->w == w && next->h == h) {
            sp[n].horizontal = 1; sp[n].in_place = 0;
            sp[n].begin_c = first + 1; sp[n].num_c = 2;
            n++;
            sp[n].horizontal = 0; sp[n].in_place = 0;
            sp[n].begin_c = first + 1; sp[n].num_c = 2;
            n++;
        }
    }

#define SQ_PUSH(horiz)                                                        \
    do {                                                                      \
        if (n == cap) {                                                       \
            jxl_sq_param *ns = (jxl_sq_param *)jxl_realloc_array(             \
                ctx, sp, cap, cap * 2, sizeof(jxl_sq_param));                 \
            if (!ns) { jxl_free(ctx, sp); return -1; }                        \
            sp = ns; cap *= 2;                                                \
        }                                                                     \
        sp[n].horizontal = (horiz);                                           \
        sp[n].in_place = 1;                                                   \
        sp[n].begin_c = first;                                                \
        sp[n].num_c = cl->n - first;                                          \
        n++;                                                                  \
    } while (0)

    if (h >= w && h > 8) {
        SQ_PUSH(0);
        h = div_ceil_u32(h, 2);
    }
    while (w > 8 || h > 8) {
        if (w > 8) { SQ_PUSH(1); w = div_ceil_u32(w, 2); }
        if (h > 8) { SQ_PUSH(0); h = div_ceil_u32(h, 2); }
    }
#undef SQ_PUSH

    tr->sp = sp;
    tr->nsp = n;
    return 0;
}

static int transform_apply(jxl_ctx *ctx, jxl_transform *tr, jxl_chanlist *cl,
                           int apply_grids) {
    if (tr->kind == 0) {
        uint32_t begin = tr->begin_c, end = tr->begin_c + 3, i;
        uint32_t w, h;
        if (end > cl->n) {
            JXL_ERR(ctx, "modular: RCT channel range out of bounds");
            return -1;
        }
        w = cl->chans[begin].w;
        h = cl->chans[begin].h;
        for (i = begin + 1; i < end; i++) {
            if (cl->chans[i].w != w || cl->chans[i].h != h) {
                JXL_ERR(ctx, "modular: RCT channel size mismatch");
                return -1;
            }
        }
        return 0;
    }

    if (tr->kind == 1) {
        uint32_t begin = tr->begin_c, end = tr->begin_c + tr->num_c, i;
        uint32_t w, h;
        jxl_mchan pal;

        if (tr->num_c == 0 || end > cl->n) {
            JXL_ERR(ctx, "modular: palette channel range out of bounds");
            return -1;
        }
        if (begin < cl->nb_meta) {
            if (end > cl->nb_meta) {
                JXL_ERR(ctx, "modular: palette spans meta channels");
                return -1;
            }
            cl->nb_meta = cl->nb_meta + 2 - tr->num_c;
        } else {
            cl->nb_meta += 1;
        }
        w = cl->chans[begin].w;
        h = cl->chans[begin].h;
        for (i = begin + 1; i < end; i++) {
            if (cl->chans[i].w != w || cl->chans[i].h != h) {
                JXL_ERR(ctx, "modular: palette channel size mismatch");
                return -1;
            }
        }

        if (apply_grids) {
            uint32_t nmem = tr->num_c - 1;
            jxl_free(ctx, tr->saved);
            tr->saved = NULL;
            tr->nsaved = 0;
            if (nmem) {
                tr->saved = (jxl_mchan *)jxl_calloc(ctx, nmem, sizeof(jxl_mchan));
                if (!tr->saved) return -1;
                memcpy(tr->saved, cl->chans + begin + 1, nmem * sizeof(jxl_mchan));
                tr->nsaved = nmem;
            }
        }
        chanlist_remove_range(cl, begin + 1, end);

        memset(&pal, 0, sizeof(pal));
        pal.w = tr->nb_colours;
        pal.h = tr->num_c;
        pal.hshift = -1;
        pal.vshift = -1;
        pal.ow = pal.w;
        pal.oh = pal.h;
        if (apply_grids) {
            if (!tr->pal_buf) {
                size_t total;
                if (!jxl_size_mul(pal.w, pal.h, &total)) return -1;
                tr->pal_buf = (int32_t *)jxl_calloc(ctx, total ? total : 1,
                                                    sizeof(int32_t));
                if (!tr->pal_buf) return -1;
            }
            pal.data = tr->pal_buf;
            pal.stride = pal.w;
            tr->pal = pal;
        }
        return chanlist_insert(ctx, cl, 0, &pal);
    }

    {
        uint32_t s;
        if (squeeze_default_params(ctx, tr, cl) != 0) {
            JXL_ERR(ctx, "modular: bad squeeze parameters");
            return -1;
        }
        for (s = 0; s < tr->nsp; s++) {
            const jxl_sq_param *sp = &tr->sp[s];
            uint32_t begin = sp->begin_c, end = sp->begin_c + sp->num_c;
            uint32_t idx;
            jxl_chanlist residu;

            memset(&residu, 0, sizeof(residu));
            if (end > cl->n) {
                JXL_ERR(ctx, "modular: squeeze range out of bounds");
                return -1;
            }
            if (begin < cl->nb_meta) {
                if (!sp->in_place || end > cl->nb_meta) {
                    JXL_ERR(ctx, "modular: bad squeeze meta range");
                    return -1;
                }
                cl->nb_meta += sp->num_c;
            }

            for (idx = begin; idx < end; idx++) {
                jxl_mchan *ch = &cl->chans[idx];
                jxl_mchan res = *ch;
                uint32_t len;

                if (ch->w == 0 || ch->h == 0) {
                    JXL_ERR(ctx, "modular: cannot squeeze empty channel");
                    jxl_chanlist_free(ctx, &residu);
                    return -1;
                }
                if (ch->hshift > 30 || ch->vshift > 30) {
                    JXL_ERR(ctx, "modular: channel squeezed too far");
                    jxl_chanlist_free(ctx, &residu);
                    return -1;
                }
                if (sp->horizontal) {
                    len = ch->w;
                    ch->w = div_ceil_u32(len, 2);
                    res.w = len / 2;
                    if (ch->hshift >= 0) { ch->hshift++; res.hshift++; }
                    if (apply_grids) res.data = ch->data + ch->w;
                } else {
                    len = ch->h;
                    ch->h = div_ceil_u32(len, 2);
                    res.h = len / 2;
                    if (ch->vshift >= 0) { ch->vshift++; res.vshift++; }
                    if (apply_grids) res.data = ch->data + (size_t)ch->h * ch->stride;
                }
                if (jxl_chanlist_push(ctx, &residu, &res) != 0) {
                    jxl_chanlist_free(ctx, &residu);
                    return -1;
                }
            }

            if (sp->in_place) {
                for (idx = end; idx < cl->n; idx++) {
                    if (jxl_chanlist_push(ctx, &residu, &cl->chans[idx]) != 0) {
                        jxl_chanlist_free(ctx, &residu);
                        return -1;
                    }
                }
                cl->n = end;
            }
            for (idx = 0; idx < residu.n; idx++) {
                if (jxl_chanlist_push(ctx, cl, &residu.chans[idx]) != 0) {
                    jxl_chanlist_free(ctx, &residu);
                    return -1;
                }
            }
            jxl_chanlist_free(ctx, &residu);
        }
    }
    return 0;
}

#ifdef JXL_MODULAR_AVX2
JXL_TARGET_AVX2
static void rct_inverse6_avx2(const jxl_mchan *a, const jxl_mchan *b,
                              const jxl_mchan *c, jxl_mchan *od,
                              jxl_mchan *oe, jxl_mchan *of) {
    uint32_t x, y;
    for (y = 0; y < a->h; y++) {
        const int32_t *ra = a->data + (size_t)y * a->stride;
        const int32_t *rb = b->data + (size_t)y * b->stride;
        const int32_t *rc = c->data + (size_t)y * c->stride;
        int32_t *rd = od->data + (size_t)y * od->stride;
        int32_t *re = oe->data + (size_t)y * oe->stride;
        int32_t *rf = of->data + (size_t)y * of->stride;

        for (x = 0; x + 8 <= a->w; x += 8) {
            __m256i va = _mm256_loadu_si256((const __m256i *)(ra + x));
            __m256i vb = _mm256_loadu_si256((const __m256i *)(rb + x));
            __m256i vc = _mm256_loadu_si256((const __m256i *)(rc + x));
            __m256i tmp =
                _mm256_sub_epi32(va, _mm256_srai_epi32(vc, 1));
            __m256i e = _mm256_add_epi32(vc, tmp);
            __m256i f =
                _mm256_sub_epi32(tmp, _mm256_srai_epi32(vb, 1));
            __m256i d = _mm256_add_epi32(f, vb);

            _mm256_storeu_si256((__m256i *)(rd + x), d);
            _mm256_storeu_si256((__m256i *)(re + x), e);
            _mm256_storeu_si256((__m256i *)(rf + x), f);
        }
        for (; x < a->w; x++) {
            uint32_t va = (uint32_t)ra[x];
            uint32_t vb = (uint32_t)rb[x];
            uint32_t vc = (uint32_t)rc[x];
            uint32_t tmp = va - (uint32_t)(((int32_t)vc) >> 1);
            uint32_t e = vc + tmp;
            uint32_t f = tmp - (uint32_t)(((int32_t)vb) >> 1);
            uint32_t d = f + vb;
            rd[x] = (int32_t)d;
            re[x] = (int32_t)e;
            rf[x] = (int32_t)f;
        }
    }
    _mm256_zeroupper();
}

JXL_TARGET_AVX2
static void rct_inverse_avx2(const jxl_mchan *a, const jxl_mchan *b,
                             const jxl_mchan *c, jxl_mchan *od,
                             jxl_mchan *oe, jxl_mchan *of, uint32_t type) {
    uint32_t x, y;
    for (y = 0; y < a->h; y++) {
        const int32_t *ra = a->data + (size_t)y * a->stride;
        const int32_t *rb = b->data + (size_t)y * b->stride;
        const int32_t *rc = c->data + (size_t)y * c->stride;
        int32_t *rd = od->data + (size_t)y * od->stride;
        int32_t *re = oe->data + (size_t)y * oe->stride;
        int32_t *rf = of->data + (size_t)y * of->stride;

        for (x = 0; x + 8 <= a->w; x += 8) {
            __m256i va = _mm256_loadu_si256((const __m256i *)(ra + x));
            __m256i vb = _mm256_loadu_si256((const __m256i *)(rb + x));
            __m256i vc = _mm256_loadu_si256((const __m256i *)(rc + x));
            __m256i d = va;
            __m256i f = (type & 1) ? _mm256_add_epi32(vc, va) : vc;
            __m256i e;
            if ((type >> 1) == 1) {
                e = _mm256_add_epi32(vb, va);
            } else if ((type >> 1) == 2) {
                e = _mm256_add_epi32(
                    vb, _mm256_srai_epi32(_mm256_add_epi32(va, f), 1));
            } else {
                e = vb;
            }
            _mm256_storeu_si256((__m256i *)(rd + x), d);
            _mm256_storeu_si256((__m256i *)(re + x), e);
            _mm256_storeu_si256((__m256i *)(rf + x), f);
        }
        for (; x < a->w; x++) {
            uint32_t va = (uint32_t)ra[x];
            uint32_t vb = (uint32_t)rb[x];
            uint32_t vc = (uint32_t)rc[x];
            uint32_t d = va;
            uint32_t f = (type & 1) ? vc + va : vc;
            uint32_t e;
            if ((type >> 1) == 1) e = vb + va;
            else if ((type >> 1) == 2)
                e = vb + (uint32_t)(((int32_t)(va + f)) >> 1);
            else e = vb;
            rd[x] = (int32_t)d;
            re[x] = (int32_t)e;
            rf[x] = (int32_t)f;
        }
    }
    _mm256_zeroupper();
}
#endif

static void rct_inverse(jxl_transform *tr, jxl_chanlist *cl) {
    uint32_t permutation = tr->rct_type / 7;
    uint32_t type = tr->rct_type % 7;
    uint32_t begin = tr->begin_c;
    jxl_mchan *a = &cl->chans[begin];
    jxl_mchan *b = &cl->chans[begin + 1];
    jxl_mchan *c = &cl->chans[begin + 2];
    jxl_mchan *od = &cl->chans[begin + permutation % 3];
    jxl_mchan *oe =
        &cl->chans[begin + (permutation + 1 + permutation / 3) % 3];
    jxl_mchan *of =
        &cl->chans[begin + (permutation + 2 - permutation / 3) % 3];
    uint32_t x, y;

    if (type == 0) {
        jxl_mchan ia = *a, ib = *b, ic = *c;
        *od = ia;
        *oe = ib;
        *of = ic;
        return;
    }

#ifdef JXL_MODULAR_AVX2
    if (jxl_has_avx2()) {
        if (type == 6) rct_inverse6_avx2(a, b, c, od, oe, of);
        else rct_inverse_avx2(a, b, c, od, oe, of, type);
        return;
    }
#endif

    for (y = 0; y < a->h; y++) {
        const int32_t *ra = a->data + (size_t)y * a->stride;
        const int32_t *rb = b->data + (size_t)y * b->stride;
        const int32_t *rc = c->data + (size_t)y * c->stride;
        int32_t *rd = od->data + (size_t)y * od->stride;
        int32_t *re = oe->data + (size_t)y * oe->stride;
        int32_t *rf = of->data + (size_t)y * of->stride;
        for (x = 0; x < a->w; x++) {
            uint32_t va = (uint32_t)ra[x];
            uint32_t vb = (uint32_t)rb[x];
            uint32_t vc = (uint32_t)rc[x];
            uint32_t d, e, f;
            if (type == 6) {
                uint32_t tmp = va - (uint32_t)(((int32_t)vc) >> 1);
                e = vc + tmp;
                f = tmp - (uint32_t)(((int32_t)vb) >> 1);
                d = f + vb;
            } else {
                d = va;
                f = (type & 1) ? vc + va : vc;
                if ((type >> 1) == 1) e = vb + va;
                else if ((type >> 1) == 2)
                    e = vb + (uint32_t)(((int32_t)(va + f)) >> 1);
                else e = vb;
            }
            rd[x] = (int32_t)d;
            re[x] = (int32_t)e;
            rf[x] = (int32_t)f;
        }
    }
}

static int32_t squeeze_tendency(int32_t a, int32_t b, int32_t c) {
    if (a >= b && b >= c) {
        int32_t x = (int32_t)((4 * (int64_t)a - 3 * (int64_t)c - b + 6) / 12);
        if (x - (x & 1) > 2 * (a - b)) x = 2 * (a - b) + 1;
        if (x + (x & 1) > 2 * (b - c)) x = 2 * (b - c);
        return x;
    }
    if (a <= b && b <= c) {
        int32_t x = (int32_t)((4 * (int64_t)a - 3 * (int64_t)c - b - 6) / 12);
        if (x + (x & 1) < 2 * (a - b)) x = 2 * (a - b) - 1;
        if (x - (x & 1) < 2 * (b - c)) x = 2 * (b - c);
        return x;
    }
    return 0;
}

#ifdef JXL_MODULAR_AVX2

static JXL_TARGET_AVX2 JXL_INLINE_HINT int squeeze_step8(
    __m256i top, __m256i avg, __m256i next, __m256i residu,
    __m256i *first_out, __m256i *second_out) {
    const __m256i zero = _mm256_setzero_si256();
    const __m256i one = _mm256_set1_epi32(1);
    const __m256i two = _mm256_set1_epi32(2);
    const __m256i all = _mm256_set1_epi32(-1);
    const __m256i limit = _mm256_set1_epi32(0x1fffffff);
    const __m256i neg_limit = _mm256_set1_epi32(-0x1fffffff);
    __m256i bad = _mm256_or_si256(
        _mm256_or_si256(_mm256_cmpgt_epi32(avg, limit),
                        _mm256_cmpgt_epi32(neg_limit, avg)),
        _mm256_or_si256(
            _mm256_or_si256(_mm256_cmpgt_epi32(top, limit),
                            _mm256_cmpgt_epi32(neg_limit, top)),
            _mm256_or_si256(_mm256_cmpgt_epi32(next, limit),
                            _mm256_cmpgt_epi32(neg_limit, next))));
    __m256i ba, an, abs_ba, abs_an, abs_bn;
    __m256i pe, po, div3, absdiff, odd, ba2, repl;
    __m256i tendency, diff;

    if (_mm256_movemask_epi8(bad) != 0) return 0;

    ba = _mm256_sub_epi32(top, avg);
    an = _mm256_sub_epi32(avg, next);
    abs_ba = _mm256_abs_epi32(ba);
    abs_an = _mm256_abs_epi32(an);
    abs_bn = _mm256_abs_epi32(_mm256_sub_epi32(top, next));
    pe = _mm256_mul_epu32(abs_ba, _mm256_set1_epi32(0x55555556));
    po = _mm256_mul_epu32(_mm256_srli_epi64(abs_ba, 32),
                          _mm256_set1_epi32(0x55555556));
    div3 = _mm256_or_si256(
        _mm256_srli_epi64(pe, 32),
        _mm256_slli_epi64(_mm256_srli_epi64(po, 32), 32));
    absdiff = _mm256_srli_epi32(
        _mm256_add_epi32(_mm256_add_epi32(div3, abs_bn), two), 2);
    odd = _mm256_and_si256(absdiff, one);
    ba2 = _mm256_add_epi32(_mm256_slli_epi32(abs_ba, 1), odd);
    repl = _mm256_add_epi32(_mm256_slli_epi32(abs_ba, 1), one);
    absdiff = _mm256_blendv_epi8(
        absdiff, repl, _mm256_cmpgt_epi32(absdiff, ba2));
    odd = _mm256_and_si256(absdiff, one);
    {
        __m256i an2 = _mm256_slli_epi32(abs_an, 1);
        __m256i rounded = _mm256_add_epi32(absdiff, odd);
        absdiff = _mm256_blendv_epi8(
            absdiff, an2, _mm256_cmpgt_epi32(rounded, an2));
    }
    tendency = _mm256_blendv_epi8(
        absdiff, _mm256_sub_epi32(zero, absdiff),
        _mm256_cmpgt_epi32(next, top));
    {
        __m256i neq_ba =
            _mm256_xor_si256(_mm256_cmpeq_epi32(ba, zero), all);
        __m256i neq_an =
            _mm256_xor_si256(_mm256_cmpeq_epi32(an, zero), all);
        __m256i nonmono =
            _mm256_cmpgt_epi32(zero, _mm256_xor_si256(ba, an));
        __m256i skip =
            _mm256_and_si256(_mm256_and_si256(neq_ba, neq_an), nonmono);
        tendency = _mm256_blendv_epi8(tendency, zero, skip);
    }
    diff = _mm256_add_epi32(residu, tendency);
    *first_out = _mm256_add_epi32(
        avg, _mm256_srai_epi32(
                 _mm256_add_epi32(diff, _mm256_srli_epi32(diff, 31)), 1));
    *second_out = _mm256_sub_epi32(*first_out, diff);
    return 1;
}

static JXL_TARGET_AVX2 JXL_INLINE_HINT void squeeze_transpose8(__m256i p[8]) {
    __m256i t0 = _mm256_unpacklo_epi32(p[0], p[1]);
    __m256i t1 = _mm256_unpackhi_epi32(p[0], p[1]);
    __m256i t2 = _mm256_unpacklo_epi32(p[2], p[3]);
    __m256i t3 = _mm256_unpackhi_epi32(p[2], p[3]);
    __m256i t4 = _mm256_unpacklo_epi32(p[4], p[5]);
    __m256i t5 = _mm256_unpackhi_epi32(p[4], p[5]);
    __m256i t6 = _mm256_unpacklo_epi32(p[6], p[7]);
    __m256i t7 = _mm256_unpackhi_epi32(p[6], p[7]);
    __m256i u0 = _mm256_unpacklo_epi64(t0, t2);
    __m256i u1 = _mm256_unpackhi_epi64(t0, t2);
    __m256i u2 = _mm256_unpacklo_epi64(t1, t3);
    __m256i u3 = _mm256_unpackhi_epi64(t1, t3);
    __m256i u4 = _mm256_unpacklo_epi64(t4, t6);
    __m256i u5 = _mm256_unpackhi_epi64(t4, t6);
    __m256i u6 = _mm256_unpacklo_epi64(t5, t7);
    __m256i u7 = _mm256_unpackhi_epi64(t5, t7);
    p[0] = _mm256_permute2x128_si256(u0, u4, 0x20);
    p[1] = _mm256_permute2x128_si256(u1, u5, 0x20);
    p[2] = _mm256_permute2x128_si256(u2, u6, 0x20);
    p[3] = _mm256_permute2x128_si256(u3, u7, 0x20);
    p[4] = _mm256_permute2x128_si256(u0, u4, 0x31);
    p[5] = _mm256_permute2x128_si256(u1, u5, 0x31);
    p[6] = _mm256_permute2x128_si256(u2, u6, 0x31);
    p[7] = _mm256_permute2x128_si256(u3, u7, 0x31);
}

static JXL_TARGET_AVX2 int squeeze_inverse_h_avx2(jxl_ctx *ctx,
                                                  jxl_mchan *merged) {
    uint32_t width = merged->w, height = merged->h;
    uint32_t avg_width = div_ceil_u32(width, 2);
    uint32_t nresidu = width / 2;
    int32_t *scratch;
    uint32_t y, r;

    scratch = (int32_t *)jxl_malloc(
        ctx, (size_t)width * 8 * sizeof(int32_t));
    if (!scratch) return -1;

    for (y = 0; y + 8 <= height; y += 8) {
        uint32_t xi = 0;
        for (r = 0; r < 8; r++) {
            const int32_t *row =
                merged->data + (size_t)(y + r) * merged->stride;
            memcpy(scratch + (size_t)r * width, row,
                   (size_t)width * sizeof(int32_t));
        }

        for (; xi + 8 <= nresidu; xi += 8) {
            __m256i av[8], rv[8], even[8], odd[8];
            __m256i left, ninth;
            uint32_t i;
            uint32_t ni = xi + 8 < avg_width ? xi + 8 : xi + 7;

            for (r = 0; r < 8; r++) {
                const int32_t *srow = scratch + (size_t)r * width;
                av[r] = _mm256_loadu_si256(
                    (const __m256i *)(srow + xi));
                rv[r] = _mm256_loadu_si256(
                    (const __m256i *)(srow + avg_width + xi));
            }
            squeeze_transpose8(av);
            squeeze_transpose8(rv);
            if (xi == 0) {
                left = av[0];
            } else {
                left = _mm256_setr_epi32(
                    merged->data[(size_t)(y + 0) * merged->stride +
                                 2 * xi - 1],
                    merged->data[(size_t)(y + 1) * merged->stride +
                                 2 * xi - 1],
                    merged->data[(size_t)(y + 2) * merged->stride +
                                 2 * xi - 1],
                    merged->data[(size_t)(y + 3) * merged->stride +
                                 2 * xi - 1],
                    merged->data[(size_t)(y + 4) * merged->stride +
                                 2 * xi - 1],
                    merged->data[(size_t)(y + 5) * merged->stride +
                                 2 * xi - 1],
                    merged->data[(size_t)(y + 6) * merged->stride +
                                 2 * xi - 1],
                    merged->data[(size_t)(y + 7) * merged->stride +
                                 2 * xi - 1]);
            }
            ninth = _mm256_setr_epi32(
                scratch[(size_t)0 * width + ni],
                scratch[(size_t)1 * width + ni],
                scratch[(size_t)2 * width + ni],
                scratch[(size_t)3 * width + ni],
                scratch[(size_t)4 * width + ni],
                scratch[(size_t)5 * width + ni],
                scratch[(size_t)6 * width + ni],
                scratch[(size_t)7 * width + ni]);

            for (i = 0; i < 8; i++) {
                __m256i next = i < 7 ? av[i + 1] : ninth;
                if (!squeeze_step8(left, av[i], next, rv[i],
                                   &even[i], &odd[i])) {
                    int32_t lt[8], aa[8], nn[8], rr[8], ee[8], oo[8];
                    uint32_t k;
                    _mm256_storeu_si256((__m256i *)lt, left);
                    _mm256_storeu_si256((__m256i *)aa, av[i]);
                    _mm256_storeu_si256((__m256i *)nn, next);
                    _mm256_storeu_si256((__m256i *)rr, rv[i]);
                    for (k = 0; k < 8; k++) {
                        int32_t diff = (int32_t)(
                            (uint32_t)rr[k] +
                            (uint32_t)squeeze_tendency(
                                lt[k], aa[k], nn[k]));
                        ee[k] = (int32_t)(
                            (uint32_t)aa[k] + (uint32_t)(diff / 2));
                        oo[k] =
                            (int32_t)((uint32_t)ee[k] - (uint32_t)diff);
                    }
                    even[i] = _mm256_loadu_si256((const __m256i *)ee);
                    odd[i] = _mm256_loadu_si256((const __m256i *)oo);
                }
                left = odd[i];
            }

            squeeze_transpose8(even);
            squeeze_transpose8(odd);
            for (r = 0; r < 8; r++) {
                __m256i lo = _mm256_unpacklo_epi32(even[r], odd[r]);
                __m256i hi = _mm256_unpackhi_epi32(even[r], odd[r]);
                __m256i out0 =
                    _mm256_permute2x128_si256(lo, hi, 0x20);
                __m256i out1 =
                    _mm256_permute2x128_si256(lo, hi, 0x31);
                int32_t *row =
                    merged->data + (size_t)(y + r) * merged->stride + 2 * xi;
                _mm256_storeu_si256((__m256i *)row, out0);
                _mm256_storeu_si256((__m256i *)(row + 8), out1);
            }
        }

        for (r = 0; r < 8; r++) {
            const int32_t *avg_row = scratch + (size_t)r * width;
            const int32_t *residu_row = avg_row + avg_width;
            int32_t *row =
                merged->data + (size_t)(y + r) * merged->stride;
            int32_t avg = avg_row[xi];
            int32_t left = xi ? row[2 * xi - 1] : avg;
            uint32_t i;
            for (i = xi; i < nresidu; i++) {
                int32_t next_avg =
                    i + 1 < avg_width ? avg_row[i + 1] : avg;
                int32_t diff = (int32_t)(
                    (uint32_t)residu_row[i] +
                    (uint32_t)squeeze_tendency(left, avg, next_avg));
                int32_t first = (int32_t)(
                    (uint32_t)avg + (uint32_t)(diff / 2));
                int32_t second =
                    (int32_t)((uint32_t)first - (uint32_t)diff);
                row[2 * i] = first;
                row[2 * i + 1] = second;
                avg = next_avg;
                left = second;
            }
            if (width & 1) row[width - 1] = avg_row[avg_width - 1];
        }
    }

    for (; y < height; y++) {
        int32_t *row = merged->data + (size_t)y * merged->stride;
        const int32_t *avg_row, *residu_row;
        int32_t avg, left;
        uint32_t x;
        memcpy(scratch, row, (size_t)width * sizeof(int32_t));
        avg_row = scratch;
        residu_row = scratch + avg_width;
        avg = avg_row[0];
        left = avg;
        for (x = 0; x + 1 < width; x += 2) {
            uint32_t i = x / 2;
            int32_t residu = residu_row[i];
            int32_t next_avg =
                i + 1 < avg_width ? avg_row[i + 1] : avg;
            int32_t diff = (int32_t)(
                (uint32_t)residu +
                (uint32_t)squeeze_tendency(left, avg, next_avg));
            int32_t first = (int32_t)(
                (uint32_t)avg + (uint32_t)(diff / 2));
            int32_t second =
                (int32_t)((uint32_t)first - (uint32_t)diff);
            row[x] = first;
            row[x + 1] = second;
            avg = next_avg;
            left = second;
        }
        if (width & 1) row[width - 1] = avg_row[avg_width - 1];
    }

    jxl_free(ctx, scratch);
    return 0;
}
#endif

static int squeeze_inverse_h(jxl_ctx *ctx, jxl_mchan *merged) {
    uint32_t width = merged->w, height = merged->h;
    uint32_t avg_width = div_ceil_u32(width, 2);
    int32_t *scratch;
    uint32_t x, y;

    if (width == 0 || height == 0) return 0;
#ifdef JXL_MODULAR_AVX2
    if (height >= 8 && width >= 16 && jxl_has_avx2())
        return squeeze_inverse_h_avx2(ctx, merged);
#endif
    scratch = (int32_t *)jxl_malloc(ctx, (size_t)width * sizeof(int32_t));
    if (!scratch) return -1;

    for (y = 0; y < height; y++) {
        int32_t *row = merged->data + (size_t)y * merged->stride;
        const int32_t *avg_row, *residu_row;
        int32_t avg, left;
        memcpy(scratch, row, (size_t)width * sizeof(int32_t));
        avg_row = scratch;
        residu_row = scratch + avg_width;
        avg = avg_row[0];
        left = avg;
        for (x = 0; x + 1 < width; x += 2) {
            uint32_t xi = x / 2;
            int32_t residu = residu_row[xi];
            int32_t next_avg = (xi + 1 < avg_width) ? avg_row[xi + 1] : avg;
            int32_t diff = (int32_t)((uint32_t)residu +
                                     (uint32_t)squeeze_tendency(left, avg, next_avg));
            int32_t first = (int32_t)((uint32_t)avg + (uint32_t)(diff / 2));
            int32_t second = (int32_t)((uint32_t)first - (uint32_t)diff);
            row[x] = first;
            row[x + 1] = second;
            avg = next_avg;
            left = second;
        }
        if (width & 1) row[width - 1] = avg_row[avg_width - 1];
    }
    jxl_free(ctx, scratch);
    return 0;
}

#define JXL_SQ_STRIP 16

#ifdef JXL_MODULAR_AVX2

static JXL_TARGET_AVX2 void squeeze_inverse_v_avx2(
    jxl_mchan *merged, int32_t *scratch, uint32_t width, uint32_t height,
    uint32_t avg_height) {
    uint32_t x, y;

    for (x = 0; x < width; x += JXL_SQ_STRIP) {
        uint32_t nresidu = height / 2;
        uint32_t sw = width - x < JXL_SQ_STRIP ? width - x : JXL_SQ_STRIP;
        int32_t avg[JXL_SQ_STRIP], top[JXL_SQ_STRIP];
        uint32_t dx;

        for (y = 0; y < height; y++) {
            memcpy(scratch + (size_t)y * sw,
                   merged->data + (size_t)y * merged->stride + x,
                   (size_t)sw * sizeof(int32_t));
        }
        for (dx = 0; dx < sw; dx++) {
            avg[dx] = scratch[dx];
            top[dx] = avg[dx];
        }
        for (y = 0; y < nresidu; y++) {
            const int32_t *residu_row =
                scratch + (size_t)(avg_height + y) * sw;
            const int32_t *next_row = (y + 1 < avg_height)
                                          ? scratch + (size_t)(y + 1) * sw
                                          : NULL;
            int32_t *out0 =
                merged->data + (size_t)(2 * y) * merged->stride + x;
            int32_t *out1 = out0 + merged->stride;

            for (dx = 0; dx + 8 <= sw; dx += 8) {
                __m256i av = _mm256_loadu_si256((const __m256i *)(avg + dx));
                __m256i tv = _mm256_loadu_si256((const __m256i *)(top + dx));
                __m256i nv = next_row
                    ? _mm256_loadu_si256((const __m256i *)(next_row + dx))
                    : av;
                __m256i residu = _mm256_loadu_si256(
                    (const __m256i *)(residu_row + dx));
                __m256i first, second;

                if (squeeze_step8(tv, av, nv, residu, &first, &second)) {
                    _mm256_storeu_si256((__m256i *)(out0 + dx), first);
                    _mm256_storeu_si256((__m256i *)(out1 + dx), second);
                    _mm256_storeu_si256((__m256i *)(avg + dx), nv);
                    _mm256_storeu_si256((__m256i *)(top + dx), second);
                } else {
                    uint32_t k;
                    for (k = 0; k < 8; k++) {
                        uint32_t i = dx + k;
                        int32_t next_avg = next_row ? next_row[i] : avg[i];
                        int32_t diff = (int32_t)(
                            (uint32_t)residu_row[i] +
                            (uint32_t)squeeze_tendency(
                                top[i], avg[i], next_avg));
                        int32_t first_s = (int32_t)(
                            (uint32_t)avg[i] + (uint32_t)(diff / 2));
                        int32_t second_s =
                            (int32_t)((uint32_t)first_s - (uint32_t)diff);
                        out0[i] = first_s;
                        out1[i] = second_s;
                        avg[i] = next_avg;
                        top[i] = second_s;
                    }
                }
            }
            for (; dx < sw; dx++) {
                int32_t next_avg = next_row ? next_row[dx] : avg[dx];
                int32_t diff = (int32_t)(
                    (uint32_t)residu_row[dx] +
                    (uint32_t)squeeze_tendency(top[dx], avg[dx], next_avg));
                int32_t first = (int32_t)(
                    (uint32_t)avg[dx] + (uint32_t)(diff / 2));
                int32_t second =
                    (int32_t)((uint32_t)first - (uint32_t)diff);
                out0[dx] = first;
                out1[dx] = second;
                avg[dx] = next_avg;
                top[dx] = second;
            }
        }
        if (height & 1) {
            const int32_t *last = scratch + (size_t)(avg_height - 1) * sw;
            int32_t *out = merged->data +
                           (size_t)(height - 1) * merged->stride + x;
            memcpy(out, last, (size_t)sw * sizeof(int32_t));
        }
    }
}
#endif

static int squeeze_inverse_v(jxl_ctx *ctx, jxl_mchan *merged) {
    uint32_t width = merged->w, height = merged->h;
    uint32_t avg_height = div_ceil_u32(height, 2);
    int32_t *scratch;
    uint32_t x, y;

    if (width == 0 || height == 0) return 0;
    scratch = (int32_t *)jxl_malloc(
        ctx, (size_t)height * JXL_SQ_STRIP * sizeof(int32_t));
    if (!scratch) return -1;

#ifdef JXL_MODULAR_AVX2
    if (jxl_has_avx2()) {
        squeeze_inverse_v_avx2(merged, scratch, width, height, avg_height);
        jxl_free(ctx, scratch);
        return 0;
    }
#endif

    for (x = 0; x < width; x += JXL_SQ_STRIP) {
        uint32_t nresidu = height / 2;
        uint32_t sw = width - x < JXL_SQ_STRIP ? width - x : JXL_SQ_STRIP;
        int32_t avg[JXL_SQ_STRIP], top[JXL_SQ_STRIP];
        uint32_t dx;

        for (y = 0; y < height; y++) {
            memcpy(scratch + (size_t)y * sw,
                   merged->data + (size_t)y * merged->stride + x,
                   (size_t)sw * sizeof(int32_t));
        }
        for (dx = 0; dx < sw; dx++) {
            avg[dx] = scratch[dx];
            top[dx] = avg[dx];
        }
        for (y = 0; y < nresidu; y++) {
            const int32_t *residu_row = scratch + (size_t)(avg_height + y) * sw;
            const int32_t *next_row = (y + 1 < avg_height)
                                          ? scratch + (size_t)(y + 1) * sw
                                          : NULL;
            int32_t *out0 = merged->data + (size_t)(2 * y) * merged->stride + x;
            int32_t *out1 = out0 + merged->stride;
            for (dx = 0; dx < sw; dx++) {
                int32_t next_avg = next_row ? next_row[dx] : avg[dx];
                int32_t diff = (int32_t)((uint32_t)residu_row[dx] +
                        (uint32_t)squeeze_tendency(top[dx], avg[dx], next_avg));
                int32_t first = (int32_t)((uint32_t)avg[dx] +
                                          (uint32_t)(diff / 2));
                int32_t second = (int32_t)((uint32_t)first - (uint32_t)diff);
                out0[dx] = first;
                out1[dx] = second;
                avg[dx] = next_avg;
                top[dx] = second;
            }
        }
        if (height & 1) {
            const int32_t *last = scratch + (size_t)(avg_height - 1) * sw;
            int32_t *out = merged->data +
                           (size_t)(height - 1) * merged->stride + x;
            for (dx = 0; dx < sw; dx++) out[dx] = last[dx];
        }
    }
    jxl_free(ctx, scratch);
    return 0;
}

#undef JXL_SQ_STRIP

static const int16_t delta_palette[72][3] = {
    {0,0,0},{4,4,4},{11,0,0},{0,0,-13},{0,-12,0},{-10,-10,-10},
    {-18,-18,-18},{-27,-27,-27},{-18,-18,0},{0,0,-32},{-32,0,0},{-37,-37,-37},
    {0,-32,-32},{24,24,45},{50,50,50},{-45,-24,-24},{-24,-45,-45},{0,-24,-24},
    {-34,-34,0},{-24,0,-24},{-45,-45,-24},{64,64,64},{-32,0,-32},{0,-32,0},
    {-32,0,32},{-24,-45,-24},{45,24,45},{24,-24,-45},{-45,-24,24},{80,80,80},
    {64,0,0},{0,0,-64},{0,-64,-64},{-24,-24,45},{96,96,96},{64,64,0},
    {45,-24,-24},{34,-34,0},{112,112,112},{24,-45,-45},{45,45,-24},{0,-32,32},
    {24,-24,45},{0,96,96},{45,-24,24},{24,-45,-24},{-24,-45,24},{0,-64,0},
    {96,0,0},{128,128,128},{64,0,64},{144,144,144},{96,96,0},{-36,-36,36},
    {45,-24,-45},{45,-45,-24},{0,0,-96},{0,128,128},{0,96,0},{45,24,-45},
    {-128,0,0},{24,-45,24},{-45,24,-45},{64,0,-64},{64,-64,-64},{96,0,96},
    {45,-45,24},{24,45,-45},{64,64,-64},{128,128,0},{0,0,-128},{-24,45,-45}
};

static int palette_inverse(jxl_ctx *ctx, jxl_transform *tr, jxl_chanlist *cl,
                           uint32_t bit_depth, const jxl_wp_header *wp_header) {
    uint32_t num_c = tr->num_c;
    jxl_mchan pal;
    jxl_mchan *targets = NULL;
    uint32_t begin, x, y, c, i;
    uint32_t width, height;
    int32_t nb_colours = (int32_t)tr->nb_colours;
    int32_t nb_deltas = (int32_t)tr->nb_deltas;
    uint8_t *need_delta = NULL;
    int any_delta = 0;
    int rc = -1;

    if (cl->n == 0) return -1;
    pal = cl->chans[0];
    chanlist_remove_range(cl, 0, 1);
    begin = tr->begin_c;
    if (begin >= cl->n) return -1;

    targets = (jxl_mchan *)jxl_calloc(ctx, num_c, sizeof(jxl_mchan));
    if (!targets) return -1;
    targets[0] = cl->chans[begin];
    for (i = 0; i + 1 < num_c; i++) {
        if (i >= tr->nsaved) goto done;
        targets[i + 1] = tr->saved[i];
    }
    width = targets[0].w;
    height = targets[0].h;

    need_delta = (uint8_t *)jxl_calloc(ctx, (size_t)width * height + 1, 1);
    if (!need_delta) goto done;

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            int32_t index = chan_get(&targets[0], x, y);
            if (index < nb_deltas) {
                need_delta[(size_t)y * width + x] = 1;
                any_delta = 1;
            }
            if (index >= 0 && index < nb_colours) {
                for (c = 0; c < num_c; c++) {
                    targets[c].data[(size_t)y * targets[c].stride + x] =
                        chan_get(&pal, (uint32_t)index, c);
                }
            } else if (index >= nb_colours) {
                int32_t idx = index - nb_colours;
                if (idx < 64) {
                    for (c = 0; c < num_c; c++) {
                        int32_t v = ((idx >> (2 * c)) % 4) *
                                        ((int32_t)(1u << bit_depth) - 1) / 4 +
                                    (int32_t)(1u << (bit_depth >= 3 ? bit_depth - 3 : 0));
                        targets[c].data[(size_t)y * targets[c].stride + x] = v;
                    }
                } else {
                    int32_t k = idx - 64;
                    for (c = 0; c < num_c; c++) {
                        targets[c].data[(size_t)y * targets[c].stride + x] =
                            (k % 5) * ((int32_t)(1u << bit_depth) - 1) / 4;
                        k /= 5;
                    }
                }
            } else {
                for (c = 0; c < num_c; c++) {
                    int32_t v;
                    int32_t di;
                    if (c >= 3) {
                        targets[c].data[(size_t)y * targets[c].stride + x] = 0;
                        continue;
                    }
                    di = -(index + 1);
                    di = di % 143;
                    v = delta_palette[(di + 1) >> 1][c];
                    if ((di & 1) == 0) v = -v;
                    if (bit_depth > 8) {
                        uint32_t sh = bit_depth < 24 ? bit_depth : 24;
                        v = (int32_t)((uint32_t)v << (sh - 8));
                    }
                    targets[c].data[(size_t)y * targets[c].stride + x] = v;
                }
            }
        }
    }

    if (any_delta) {
        jxl_pred_state ps;
        memset(&ps, 0, sizeof(ps));
        for (c = 0; c < num_c; c++) {
            const jxl_wp_header *wp =
                (tr->d_pred == JXL_PRED_SELF_CORRECTING) ? wp_header : NULL;
            if (pred_state_reset(ctx, &ps, width, 1, wp, NULL, 0) != 0) {
                pred_state_free(ctx, &ps);
                goto done;
            }
            for (y = 0; y < height; y++) {
                for (x = 0; x < width; x++) {
                    jxl_props pr;
                    int32_t *sample = &targets[c].data[(size_t)y * targets[c].stride + x];
                    int32_t value = *sample;

                    props_compute(&ps, &pr, 0, 0);
                    if (need_delta[(size_t)y * width + x]) {
                        int32_t diff = predict_sample(&ps, &pr, tr->d_pred);
                        value = (int32_t)((uint32_t)value + (uint32_t)diff);
                        *sample = value;
                    }
                    pred_record(&ps, &pr, value);
                }
            }
        }
        pred_state_free(ctx, &ps);
    }

    for (i = 0; i + 1 < num_c; i++) {
        if (chanlist_insert(ctx, cl, begin + 1 + i, &targets[i + 1]) != 0) goto done;
    }
    cl->chans[begin] = targets[0];
    rc = 0;

done:
    jxl_free(ctx, need_delta);
    jxl_free(ctx, targets);
    return rc;
}

static int squeeze_inverse(jxl_ctx *ctx, jxl_transform *tr, jxl_chanlist *cl) {
    uint32_t s = tr->nsp;
    while (s > 0) {
        const jxl_sq_param *sp = &tr->sp[--s];
        uint32_t begin = sp->begin_c;
        uint32_t count = sp->num_c;
        uint32_t end = begin + count;
        jxl_mchan *residual;
        uint32_t from, i;

        if (end > cl->n) return -1;
        if (sp->in_place) {
            if (end + count > cl->n) return -1;
            from = end;
        } else {
            if (cl->n < count) return -1;
            from = cl->n - count;
        }
        residual = (jxl_mchan *)jxl_calloc(ctx, count, sizeof(jxl_mchan));
        if (!residual) return -1;
        memcpy(residual, cl->chans + from, (size_t)count * sizeof(jxl_mchan));
        chanlist_remove_range(cl, from, from + count);

        for (i = 0; i < count; i++) {
            jxl_mchan *ch = &cl->chans[begin + i];
            if (sp->horizontal) {
                ch->w += residual[i].w;
                if (ch->hshift > 0) ch->hshift--;
                if (squeeze_inverse_h(ctx, ch) != 0) {
                    jxl_free(ctx, residual);
                    return -1;
                }
            } else {
                ch->h += residual[i].h;
                if (ch->vshift > 0) ch->vshift--;
                if (squeeze_inverse_v(ctx, ch) != 0) {
                    jxl_free(ctx, residual);
                    return -1;
                }
            }
        }
        jxl_free(ctx, residual);
    }
    return 0;
}

int jxl_modular_inverse(jxl_ctx *ctx, jxl_modular *m, jxl_chanlist *cl) {
    uint32_t i = m->header.ntransforms;
    while (i > 0) {
        jxl_transform *tr = &m->header.transforms[--i];
        if (tr->kind == 0) {
            if (tr->begin_c + 3 > cl->n) return -1;
            rct_inverse(tr, cl);
        } else if (tr->kind == 1) {
            if (palette_inverse(ctx, tr, cl, m->bit_depth, &m->header.wp) != 0)
                return -1;
        } else {
            if (squeeze_inverse(ctx, tr, cl) != 0) return -1;
        }
    }
    return 0;
}

static void shift_size(int hshift, int vshift, uint32_t w, uint32_t h,
                       uint32_t *ow, uint32_t *oh) {
    uint32_t sh = hshift > 0 ? (uint32_t)hshift : 0;
    uint32_t sv = vshift > 0 ? (uint32_t)vshift : 0;
    *ow = (w + ((1u << sh) - 1)) >> sh;
    *oh = (h + ((1u << sv) - 1)) >> sv;
}

static int modular_init_common(jxl_ctx *ctx, jxl_modular *m, jxl_br *br,
                               jxl_ma_config *global_ma) {
    jxl_chanlist cl;
    uint32_t i;

    memset(&cl, 0, sizeof(cl));
    m->header.use_global_tree = jxl_br_bool(br);
    wp_header_read(br, &m->header.wp);
    m->header.ntransforms = jxl_br_u32(br, 0, 0, 1, 0, 2, 4, 18, 8);
    if (m->header.ntransforms > 512) {
        JXL_ERR(ctx, "modular: too many transforms");
        return -1;
    }
    if (m->header.ntransforms) {
        m->header.transforms = (jxl_transform *)jxl_calloc(
            ctx, m->header.ntransforms, sizeof(jxl_transform));
        if (!m->header.transforms) return -1;
    }
    for (i = 0; i < m->header.ntransforms; i++) {
        if (transform_read(ctx, br, &m->header.transforms[i]) != 0) return -1;
    }
    if (br->err) {
        JXL_ERR(ctx, "modular: truncated header");
        return -1;
    }

    for (i = 0; i < m->nbase; i++) {
        if (jxl_chanlist_push(ctx, &cl, &m->base[i]) != 0) {
            jxl_chanlist_free(ctx, &cl);
            return -1;
        }
    }
    for (i = 0; i < m->header.ntransforms; i++) {
        if (transform_apply(ctx, &m->header.transforms[i], &cl, 0) != 0) {
            jxl_chanlist_free(ctx, &cl);
            return -1;
        }
    }
    if (cl.n > (1u << 16)) {
        JXL_ERR(ctx, "modular: too many channels after transforms");
        jxl_chanlist_free(ctx, &cl);
        return -1;
    }

    if (m->header.use_global_tree) {
        if (!global_ma || !global_ma->valid) {
            JXL_ERR(ctx, "modular: global MA tree not available");
            jxl_chanlist_free(ctx, &cl);
            return -1;
        }
        m->ma = global_ma;
    } else {
        uint64_t samples = 0;
        size_t limit;
        for (i = 0; i < cl.n; i++) {
            samples += (uint64_t)cl.chans[i].w * cl.chans[i].h;
        }
        limit = (size_t)JXL_MIN(1024 + samples, (uint64_t)1 << 20);
        if (jxl_ma_config_read(ctx, br, &m->local, limit) != 0) {
            jxl_chanlist_free(ctx, &cl);
            return -1;
        }
        m->has_local = 1;
        m->ma = &m->local;
    }
    jxl_chanlist_free(ctx, &cl);
    return 0;
}

int jxl_modular_init(jxl_ctx *ctx, jxl_modular *m, jxl_br *br,
                     const jxl_mchan_spec *specs, uint32_t nspecs,
                     jxl_ma_config *global_ma, uint32_t group_dim,
                     uint32_t bit_depth) {
    uint32_t i;

    memset(m, 0, sizeof(*m));
    m->ctx = ctx;
    m->group_dim = group_dim;
    m->bit_depth = bit_depth;
    if (nspecs == 0) return 0;

    m->base = (jxl_mchan *)jxl_calloc(ctx, nspecs, sizeof(jxl_mchan));
    m->bufs = (int32_t **)jxl_calloc(ctx, nspecs, sizeof(int32_t *));
    if (!m->base || !m->bufs) return -1;
    m->nbase = nspecs;
    m->nbufs = nspecs;
    for (i = 0; i < nspecs; i++) {
        uint32_t w, h;
        size_t total;
        shift_size(specs[i].hshift, specs[i].vshift, specs[i].w, specs[i].h, &w, &h);
        m->base[i].w = w;
        m->base[i].h = h;
        m->base[i].ow = specs[i].w;
        m->base[i].oh = specs[i].h;
        m->base[i].hshift = specs[i].hshift;
        m->base[i].vshift = specs[i].vshift;
        m->base[i].stride = w;
        if (!jxl_size_mul(w, h, &total)) return -1;
        m->bufs[i] = (int32_t *)jxl_calloc(ctx, total ? total : 1, sizeof(int32_t));
        if (!m->bufs[i]) return -1;
        m->base[i].data = m->bufs[i];
    }
    return modular_init_common(ctx, m, br, global_ma);
}

int jxl_modular_init_over(jxl_ctx *ctx, jxl_modular *m, jxl_br *br,
                          const jxl_mchan *chans, uint32_t nchans,
                          jxl_ma_config *global_ma, uint32_t group_dim,
                          uint32_t bit_depth) {
    memset(m, 0, sizeof(*m));
    m->ctx = ctx;
    m->group_dim = group_dim;
    m->bit_depth = bit_depth;
    if (nchans == 0) return 0;
    m->base = (jxl_mchan *)jxl_calloc(ctx, nchans, sizeof(jxl_mchan));
    if (!m->base) return -1;
    memcpy(m->base, chans, (size_t)nchans * sizeof(jxl_mchan));
    m->nbase = nchans;
    m->nbufs = 0;
    return modular_init_common(ctx, m, br, global_ma);
}

void jxl_modular_free(jxl_ctx *ctx, jxl_modular *m) {
    uint32_t i;
    if (!m || !m->ctx) return;
    for (i = 0; i < m->header.ntransforms; i++) {
        jxl_free(ctx, m->header.transforms[i].sp);
        jxl_free(ctx, m->header.transforms[i].saved);
        jxl_free(ctx, m->header.transforms[i].pal_buf);
    }
    jxl_free(ctx, m->header.transforms);
    for (i = 0; i < m->nbufs; i++) jxl_free(ctx, m->bufs[i]);
    jxl_free(ctx, m->bufs);
    jxl_free(ctx, m->base);
    if (m->has_local) jxl_ma_config_free(ctx, &m->local);
    memset(m, 0, sizeof(*m));
}

int jxl_modular_transform_channels(jxl_ctx *ctx, jxl_modular *m,
                                   jxl_chanlist *cl) {
    uint32_t i;
    memset(cl, 0, sizeof(*cl));
    for (i = 0; i < m->nbase; i++) {
        if (jxl_chanlist_push(ctx, cl, &m->base[i]) != 0) return -1;
    }
    for (i = 0; i < m->header.ntransforms; i++) {
        if (transform_apply(ctx, &m->header.transforms[i], cl, 1) != 0) return -1;
    }
    return 0;
}

static const jxl_ma_leaf *ma_get_leaf(const jxl_ma_config *ma,
                                      const jxl_pred_state *ps,
                                      const jxl_props *pr) {
    const jxl_ma_flat *flat = ma->flat;
    const jxl_ma_flat *f = flat;
    while (f->property >= 0) {
        int32_t prop0 = f->property;
        int32_t prop1 = f->u.dec.prop1;
        int32_t prop2 = f->u.dec.prop2;
        int32_t v0, v1, v2;
        uint32_t p0, off0, off1;

        v0 = prop0 < 16 ? pr->cache[prop0]
                        : props_get_extra(ps, (uint32_t)prop0 - 16);
        v1 = prop1 < 16 ? pr->cache[prop1]
                        : props_get_extra(ps, (uint32_t)prop1 - 16);
        v2 = prop2 < 16 ? pr->cache[prop2]
                        : props_get_extra(ps, (uint32_t)prop2 - 16);
        p0 = v0 <= f->u.dec.split0;
        off0 = v1 <= f->u.dec.split1;
        off1 = 2u | (uint32_t)(v2 <= f->u.dec.split2);
        f = &flat[f->u.dec.child + (p0 ? off1 : off0)];
    }
    return &f->u.leaf;
}

static const jxl_ma_leaf *ma_get_leaf_local(const jxl_ma_config *ma,
                                            const jxl_props *pr) {
    const jxl_ma_flat *flat = ma->flat;
    const jxl_ma_flat *f = flat;
    while (f->property >= 0) {
        uint32_t p0, off0, off1;
        int32_t v0 = pr->cache[f->property];
        int32_t v1 = pr->cache[f->u.dec.prop1];
        int32_t v2 = pr->cache[f->u.dec.prop2];
        p0 = v0 <= f->u.dec.split0;
        off0 = v1 <= f->u.dec.split1;
        off1 = 2u | (uint32_t)(v2 <= f->u.dec.split2);
        f = &flat[f->u.dec.child + (p0 ? off1 : off0)];
    }
    return &f->u.leaf;
}

static uint32_t ma_props_mask(const jxl_ma_config *ma) {
    uint32_t i, mask = 0;
    for (i = 0; i < ma->nflat; i++) {
        const jxl_ma_flat *f = &ma->flat[i];
        uint32_t p[3];
        int k;
        if (f->property < 0) continue;
        p[0] = (uint32_t)f->property;
        p[1] = f->u.dec.prop1;
        p[2] = f->u.dec.prop2;
        for (k = 0; k < 3; k++) {
            if (p[k] >= 32) return 0xffffffffu;
            mask |= 1u << p[k];
        }
    }
    return mask;
}

static int ma_tests_const_props(const jxl_ma_config *ma) {
    uint32_t i;
    for (i = 0; i < ma->nraw; i++) {
        if (ma->raw[i].property == 0 || ma->raw[i].property == 1) return 1;
    }
    return 0;
}

static int ma_all_gradient_noop(const jxl_ma_config *ma) {
    uint32_t i;
    for (i = 0; i < ma->nraw; i++) {
        const jxl_ma_node *n = &ma->raw[i];
        if (n->property >= 0) {
            if (n->property > 1) return 0;
        } else {
            const jxl_ma_leaf *leaf = &ma->leaves[n->child];
            if (leaf->predictor != JXL_PRED_GRADIENT ||
                leaf->offset != 0 || leaf->multiplier != 1)
                return 0;
        }
    }
    return 1;
}

static int ma_all_wp_noop(const jxl_ma_config *ma) {
    uint32_t i;
    for (i = 0; i < ma->nflat; i++) {
        const jxl_ma_flat *f = &ma->flat[i];
        if (f->property >= 0) continue;
        if (f->u.leaf.predictor != JXL_PRED_SELF_CORRECTING ||
            f->u.leaf.offset != 0 || f->u.leaf.multiplier != 1)
            return 0;
    }
    return 1;
}

#define JXL_WP_LUT_LO (-8192)
#define JXL_WP_LUT_HI (8191)
#define JXL_WP_LUT_N  (JXL_WP_LUT_HI - JXL_WP_LUT_LO + 1)

static int ma_wp_lut_ok(const jxl_ma_config *ma) {
    uint32_t i;
    for (i = 0; i < ma->nflat; i++) {
        const jxl_ma_flat *f = &ma->flat[i];
        if (f->property < 0) continue;
        if (f->property == 15 &&
            !(f->u.dec.split0 >= JXL_WP_LUT_LO && f->u.dec.split0 < JXL_WP_LUT_HI))
            return 0;
        if (f->u.dec.prop1 == 15 &&
            !(f->u.dec.split1 >= JXL_WP_LUT_LO && f->u.dec.split1 < JXL_WP_LUT_HI))
            return 0;
        if (f->u.dec.prop2 == 15 &&
            !(f->u.dec.split2 >= JXL_WP_LUT_LO && f->u.dec.split2 < JXL_WP_LUT_HI))
            return 0;
    }
    return 1;
}

static int ma_needs_self_correcting(const jxl_ma_config *ma) {
    uint32_t i;
    for (i = 0; i < ma->nflat; i++) {
        const jxl_ma_flat *f = &ma->flat[i];
        if (f->property < 0) {
            if (f->u.leaf.predictor == JXL_PRED_SELF_CORRECTING) return 1;
        } else if (f->property == 15 || f->u.dec.prop1 == 15 ||
                   f->u.dec.prop2 == 15) {
            return 1;
        }
    }
    return 0;
}

static uint32_t ma_max_prev_channels(const jxl_ma_config *ma) {
    uint32_t i, max = 0;
    for (i = 0; i < ma->nflat; i++) {
        const jxl_ma_flat *f = &ma->flat[i];
        int32_t props[3];
        int k;

        if (f->property < 0) continue;
        props[0] = f->property;
        props[1] = f->u.dec.prop1;
        props[2] = f->u.dec.prop2;
        for (k = 0; k < 3; k++) {
            if (props[k] >= 16) {
                uint32_t d = ((uint32_t)props[k] - 16) / 4 + 1;
                if (d > max) max = d;
            }
        }
    }
    return max;
}

#if defined(_MSC_VER)
#define JXL_MODULAR_NOINLINE __declspec(noinline)
#elif defined(__clang__) || defined(__GNUC__)
#define JXL_MODULAR_NOINLINE __attribute__((noinline))
#else
#define JXL_MODULAR_NOINLINE
#endif

static JXL_MODULAR_NOINLINE int
modular_decode_general_nec(jxl_mchan *ch, jxl_pred_state *ps,
                           const jxl_ma_config *ma, jxl_dec *dec, jxl_br *br,
                           uint32_t dist_multiplier, int32_t channel,
                           int32_t stream_idx) {
    uint32_t x, y;

#define JXL_NEC_SAMPLE(COMPUTE_PROPS, PREDICT_SAMPLE, RECORD_SAMPLE) do {     \
        jxl_props pr;                                                         \
        const jxl_ma_leaf *leaf;                                              \
        uint32_t token;                                                       \
        int32_t diff, value;                                                  \
                                                                              \
        COMPUTE_PROPS;                                                        \
        leaf = ma_get_leaf_local(ma, &pr);                                    \
        token = jxl_dec_read_clustered(dec, br, leaf->cluster,                \
                                       dist_multiplier);                      \
        diff = jxl_unpack_signed(token);                                      \
        diff = (int32_t)((uint32_t)diff * leaf->multiplier +                  \
                         (uint32_t)leaf->offset);                              \
        value = (int32_t)((uint32_t)diff + (uint32_t)(PREDICT_SAMPLE));       \
        row[x] = value;                                                       \
        RECORD_SAMPLE;                                                        \
    } while (0)

    for (y = 0; y < ch->h; y++) {
        int32_t *row = ch->data + (size_t)y * ch->stride;
        x = 0;
        if (y > 1 && ch->w > 8) {
            for (; x < 2; x++) {
                JXL_NEC_SAMPLE(
                    props_compute(ps, &pr, channel, stream_idx),
                    predict_sample(ps, &pr, leaf->predictor),
                    pred_record(ps, &pr, value));
            }
            for (; x + 2 < ch->w; x++) {
                JXL_NEC_SAMPLE(
                    props_compute_nec(ps, &pr, channel, stream_idx),
                    predict_sample_nec(ps, &pr, leaf->predictor),
                    pred_record_nec(ps, &pr, value));
            }
        }
        for (; x < ch->w; x++) {
            JXL_NEC_SAMPLE(
                props_compute(ps, &pr, channel, stream_idx),
                predict_sample(ps, &pr, leaf->predictor),
                pred_record(ps, &pr, value));
        }
        if (br->err || dec->err) return -1;
    }
#undef JXL_NEC_SAMPLE
    return 0;
}

static JXL_MODULAR_NOINLINE int
modular_decode_nw_nec(jxl_mchan *ch, jxl_pred_state *ps,
                      const jxl_ma_config *ma, jxl_dec *dec, jxl_br *br,
                      int32_t channel, int32_t stream_idx) {
    uint32_t x, y;

#define JXL_NW_SAMPLE(PREDICT_SAMPLE, RECORD_SAMPLE) do {                    \
        jxl_props pr;                                                         \
        const jxl_ma_leaf *leaf;                                              \
        uint32_t token;                                                       \
        int32_t diff, value;                                                  \
                                                                              \
        props_compute_nw(ps, &pr, channel, stream_idx);                       \
        leaf = ma_get_leaf_local(ma, &pr);                                    \
        token = jxl_dec_read_clustered_no_lz77(dec, br, leaf->cluster);       \
        diff = jxl_unpack_signed(token);                                      \
        diff = (int32_t)((uint32_t)diff * leaf->multiplier +                  \
                         (uint32_t)leaf->offset);                              \
        value = (int32_t)((uint32_t)diff + (uint32_t)(PREDICT_SAMPLE));       \
        row[x] = value;                                                       \
        RECORD_SAMPLE;                                                        \
    } while (0)

    for (y = 0; y < ch->h; y++) {
        int32_t *row = ch->data + (size_t)y * ch->stride;
        x = 0;
        if (y > 1 && ch->w > 8) {
            for (; x < 2; x++) {
                JXL_NW_SAMPLE(
                    predict_sample(ps, &pr, leaf->predictor),
                    pred_record(ps, &pr, value));
            }
            for (; x + 2 < ch->w; x++) {
                JXL_NW_SAMPLE(
                    predict_sample_nec(ps, &pr, leaf->predictor),
                    pred_record_nec(ps, &pr, value));
            }
        }
        for (; x < ch->w; x++) {
            JXL_NW_SAMPLE(
                predict_sample(ps, &pr, leaf->predictor),
                pred_record(ps, &pr, value));
        }
        if (br->err || dec->err) return -1;
    }
#undef JXL_NW_SAMPLE
    return 0;
}

static JXL_MODULAR_NOINLINE int
modular_decode_grad_wp_nec(jxl_mchan *ch, jxl_pred_state *ps,
                           const jxl_ma_config *ma, jxl_dec *dec, jxl_br *br,
                           int32_t channel, int32_t stream_idx) {
    uint32_t x, y;

#define JXL_GRAD_WP_SAMPLE(COMPUTE_PROPS, PREDICT_SAMPLE, RECORD_SAMPLE) do { \
        jxl_props pr;                                                         \
        const jxl_ma_leaf *leaf;                                              \
        uint32_t token;                                                       \
        int32_t diff, value;                                                  \
                                                                              \
        COMPUTE_PROPS;                                                        \
        leaf = ma_get_leaf_local(ma, &pr);                                    \
        token = jxl_dec_read_clustered_no_lz77(dec, br, leaf->cluster);       \
        diff = jxl_unpack_signed(token);                                      \
        diff = (int32_t)((uint32_t)diff * leaf->multiplier +                  \
                         (uint32_t)leaf->offset);                              \
        value = (int32_t)((uint32_t)diff + (uint32_t)(PREDICT_SAMPLE));       \
        row[x] = value;                                                       \
        RECORD_SAMPLE;                                                        \
    } while (0)

    for (y = 0; y < ch->h; y++) {
        int32_t *row = ch->data + (size_t)y * ch->stride;
        x = 0;
        if (y > 1 && ch->w > 8) {
            for (; x < 2; x++) {
                JXL_GRAD_WP_SAMPLE(
                    props_compute_grad_wp(
                        ps, &pr, channel, stream_idx),
                    predict_sample(ps, &pr, leaf->predictor),
                    pred_record(ps, &pr, value));
            }
            for (; x + 2 < ch->w; x++) {
                JXL_GRAD_WP_SAMPLE(
                    props_compute_grad_wp_nec(
                        ps, &pr, channel, stream_idx),
                    predict_sample_nec(ps, &pr, leaf->predictor),
                    pred_record_nec(ps, &pr, value));
            }
        }
        for (; x < ch->w; x++) {
            JXL_GRAD_WP_SAMPLE(
                props_compute_grad_wp(ps, &pr, channel, stream_idx),
                predict_sample(ps, &pr, leaf->predictor),
                pred_record(ps, &pr, value));
        }
        if (br->err || dec->err) return -1;
    }
#undef JXL_GRAD_WP_SAMPLE
    return 0;
}

#undef JXL_MODULAR_NOINLINE

int jxl_modular_decode(jxl_ctx *ctx, jxl_modular *m, jxl_chanlist *cl,
                       jxl_br *br, uint32_t stream_idx) {
    jxl_ma_config *ma = m->ma;
    jxl_dec *dec;
    uint32_t dist_multiplier = 0;
    uint32_t i, ci;
    jxl_pred_state ps;
    const jxl_mchan **prev = NULL;
    uint32_t max_prev;
    int need_sc;
    uint32_t props_mask;
    int wp_lut_usable = 0;
    int can_fold;
    int wp_lut_ready = 0;
    int wp_lut_local = 0;
    const jxl_ma_leaf **wp_lut = NULL;
    const uint8_t *wp_cluster_lut = NULL;
    jxl_ma_config spec;
    int spec_ok = 0;
    int rle1_fast;
    uint32_t rle1_value = 0, rle1_run = 0;
    int32_t rle1_diff = 0;
    int rle1_have = 0;
    int rc = -1;

    memset(&spec, 0, sizeof(spec));

    if (!ma || !ma->valid) {
        JXL_ERR(ctx, "modular: no MA tree");
        return -1;
    }
    if (cl->n == 0) return 0;

    dec = &ma->dec;
    for (i = 0; i < cl->n; i++) {
        if (cl->chans[i].w > dist_multiplier) dist_multiplier = cl->chans[i].w;
    }
    jxl_dec_begin(dec, br);
    rle1_fast = jxl_dec_is_prefix_rle1(dec) && ma_all_gradient_noop(ma);

    memset(&ps, 0, sizeof(ps));
    need_sc = ma_needs_self_correcting(ma);
    props_mask = ma_props_mask(ma);

    wp_lut_usable = need_sc && (props_mask & ~(1u | 2u | 0x8000u)) == 0 &&
                    ma_wp_lut_ok(ma);
    can_fold = ma->raw && ma_tests_const_props(ma);
    if (wp_lut_usable) {

        if (!can_fold && ma->wp_lut) {
            wp_lut = ma->wp_lut;
            wp_cluster_lut = ma->wp_cluster_lut;
            wp_lut_ready = 1;
        } else {
            wp_lut = (const jxl_ma_leaf **)jxl_calloc(ctx, JXL_WP_LUT_N,
                                                      sizeof(*wp_lut));
            if (!wp_lut) goto done;
            wp_lut_local = 1;
        }
    }

    if (getenv("JXL_DEBUG_TRACK")) {
        fprintf(stderr, "stream %u: fold=%d mask=0x%x need_sc=%d fixed=%d wplut=%d "
                "nflat=%u nchan=%u w=%u h=%u\n", (unsigned)stream_idx, can_fold,
                (unsigned)props_mask, need_sc,
                (!need_sc && (props_mask & ~3u) == 0), wp_lut_usable,
                (unsigned)ma->nflat, (unsigned)cl->n,
                (unsigned)cl->chans[0].w, (unsigned)cl->chans[0].h);
    }
    max_prev = ma_max_prev_channels(ma);
    if (max_prev) {
        prev = (const jxl_mchan **)jxl_calloc(ctx, cl->n, sizeof(jxl_mchan *));
        if (!prev) return -1;
    }

    for (ci = 0; ci < cl->n; ci++) {
        jxl_mchan *ch = &cl->chans[ci];
        const jxl_ma_config *cma;
        const jxl_ma_leaf *fixed = NULL;
        jxl_props pr0;
        uint32_t nprev = 0;
        uint32_t x, y;
        int channel_need_sc;
        int wp_only_fast;

        jxl_free(ctx, spec.flat);
        spec.flat = NULL;
        spec.nflat = 0;
        spec_ok = 0;

        if (ch->w == 0 || ch->h == 0) continue;

        if (can_fold && (uint64_t)ch->w * ch->h >= ma->nflat &&
            ma_flatten(ctx, ma->raw, ma->nraw, ma->root, ma->leaves, 1,
                       (int32_t)ci, (int32_t)stream_idx,
                       &spec.flat, &spec.nflat) == 0) {
            spec_ok = 1;
        }
        cma = spec_ok ? &spec : ma;

        memset(&pr0, 0, sizeof(pr0));
        if (cma->flat[0].property < 0 ||
            (!need_sc && !spec_ok && (props_mask & ~3u) == 0)) {
            pr0.cache[0] = (int32_t)ci;
            pr0.cache[1] = (int32_t)stream_idx;
            fixed = ma_get_leaf(cma, &ps, &pr0);
            if (fixed->predictor == JXL_PRED_SELF_CORRECTING) fixed = NULL;
        }
        channel_need_sc =
            need_sc && !fixed &&
            (!spec_ok || ma_needs_self_correcting(cma));
        wp_only_fast =
            !fixed && wp_lut && !dec->lz77_enabled && ch->w > 8 &&
            (uint64_t)ch->w * ch->h >= 4 * JXL_WP_LUT_N &&
            ma_all_wp_noop(cma);

        if (max_prev) {

            uint32_t k = ci;
            while (k > 0 && nprev < max_prev) {
                const jxl_mchan *p = &cl->chans[--k];
                if (p->w == ch->w && p->h == ch->h && p->hshift == ch->hshift &&
                    p->vshift == ch->vshift) {
                    prev[nprev++] = p;
                }
            }
        }

        if (pred_state_reset(ctx, &ps, ch->w, !wp_only_fast,
                             channel_need_sc ? &m->header.wp : NULL,
                             prev, nprev) != 0)
            goto done;

        if (!fixed && wp_lut &&
            (uint64_t)ch->w * ch->h >= 4 * JXL_WP_LUT_N) {
            jxl_props pr;
            uint32_t v;
            memset(&pr, 0, sizeof(pr));
            pr.cache[0] = (int32_t)ci;
            pr.cache[1] = (int32_t)stream_idx;
            if (!wp_lut_ready) {
                for (v = 0; v < JXL_WP_LUT_N; v++) {
                    pr.cache[15] = JXL_WP_LUT_LO + (int32_t)v;
                    wp_lut[v] = ma_get_leaf(cma, &ps, &pr);
                }
                if (!can_fold) {

                    ma->wp_lut = wp_lut;
                    wp_lut_local = 0;
                    wp_lut_ready = 1;
                }
            }
            if (wp_only_fast) {
                if (!can_fold && !wp_cluster_lut) {
                    uint8_t *clusters =
                        (uint8_t *)jxl_malloc(ctx, JXL_WP_LUT_N);
                    if (clusters) {
                        for (v = 0; v < JXL_WP_LUT_N; v++)
                            clusters[v] = wp_lut[v]->cluster;
                        ma->wp_cluster_lut = clusters;
                        wp_cluster_lut = clusters;
                    }
                }
#define JXL_DECODE_WP_SAMPLE(n_, nw_, ne_, w_, nn_) do {                   \
                    jxl_sc_result pred;                                    \
                    uint32_t token, lut_idx;                               \
                    uint8_t cluster;                                       \
                    int32_t value, me;                                     \
                    sc_predict(&ps.sc, (n_), (nw_), (ne_), (w_), (nn_),    \
                               &pred);                                     \
                    me = pred.max_error;                                   \
                    if (me < JXL_WP_LUT_LO) me = JXL_WP_LUT_LO;            \
                    else if (me > JXL_WP_LUT_HI) me = JXL_WP_LUT_HI;       \
                    lut_idx = (uint32_t)(me - JXL_WP_LUT_LO);              \
                    cluster = wp_cluster_lut                               \
                                  ? wp_cluster_lut[lut_idx]                 \
                                  : wp_lut[lut_idx]->cluster;               \
                    token = jxl_dec_read_clustered(dec, br, cluster,       \
                                                   dist_multiplier);       \
                    value = (int32_t)(                                     \
                        (uint32_t)jxl_unpack_signed(token) +                \
                        (uint32_t)(int32_t)((pred.prediction + 3) >> 3));   \
                    row[x] = value;                                        \
                    sc_record(&ps.sc, &pred, value);                       \
                } while (0)
                for (y = 0; y < ch->h; y++) {
                    int32_t *row = ch->data + (size_t)y * ch->stride;
                    if (y == 0) {
                        for (x = 0; x < ch->w; x++) {
                            int32_t left = x ? row[x - 1] : 0;
                            JXL_DECODE_WP_SAMPLE(left, left, left, left, left);
                        }
                    } else {
                        const int32_t *top =
                            ch->data + (size_t)(y - 1) * ch->stride;
                        const int32_t *toptop =
                            y > 1 ? top - ch->stride : top;
                        int32_t n = top[0];
                        x = 0;
                        JXL_DECODE_WP_SAMPLE(n, n, top[1], n, toptop[0]);
                        for (x = 1; x + 1 < ch->w; x++) {
                            JXL_DECODE_WP_SAMPLE(
                                top[x], top[x - 1], top[x + 1], row[x - 1],
                                toptop[x]);
                        }
                        x = ch->w - 1;
                        n = top[x];
                        JXL_DECODE_WP_SAMPLE(
                            n, top[x - 1], n, row[x - 1], toptop[x]);
                    }
                    if (br->err || dec->err) {
                        JXL_ERR(ctx, "modular: truncated stream %u",
                                (unsigned)stream_idx);
                        goto done;
                    }
                }
#undef JXL_DECODE_WP_SAMPLE
                continue;
            }
            pr.has_sc = 1;
            for (y = 0; y < ch->h; y++) {
                int32_t *row = ch->data + (size_t)y * ch->stride;
                for (x = 0; x < ch->w; x++) {
                    const jxl_ma_leaf *leaf;
                    uint32_t token;
                    int32_t diff, value, me;
                    sc_predict(&ps.sc, ps.n, ps.nw, pred_ne(&ps), ps.w,
                               pred_nn(&ps), &pr.sc);
                    me = pr.sc.max_error;
                    if (me < JXL_WP_LUT_LO) me = JXL_WP_LUT_LO;
                    else if (me > JXL_WP_LUT_HI) me = JXL_WP_LUT_HI;
                    leaf = wp_lut[me - JXL_WP_LUT_LO];
                    token = jxl_dec_read_clustered(dec, br, leaf->cluster,
                                                   dist_multiplier);
                    diff = jxl_unpack_signed(token);
                    diff = (int32_t)((uint32_t)diff * leaf->multiplier +
                                     (uint32_t)leaf->offset);
                    value = (int32_t)((uint32_t)diff +
                            (uint32_t)predict_sample(&ps, &pr, leaf->predictor));
                    row[x] = value;
                    pred_record(&ps, &pr, value);
                }
                if (br->err || dec->err) {
                    JXL_ERR(ctx, "modular: truncated stream %u",
                            (unsigned)stream_idx);
                    goto done;
                }
            }
            continue;
        }

        {
            if (fixed) {
                uint32_t cluster = fixed->cluster;
                uint32_t mult = fixed->multiplier;
                int32_t off = fixed->offset;
                uint8_t predictor = fixed->predictor;

                if (rle1_fast) {
                    int32_t *row = ch->data;

                    for (x = 0; x < ch->w; x++) {
                        int32_t guess = x ? row[x - 1] : 0;
                        if (rle1_run) {
                            rle1_run--;
                        } else {
                            uint32_t token = jxl_dec_read_prefix_rle1(
                                dec, br, cluster, &rle1_value, &rle1_run,
                                &rle1_have);
                            rle1_diff = jxl_unpack_signed(token);
                        }
                        row[x] = (int32_t)((uint32_t)rle1_diff +
                                           (uint32_t)guess);
                    }
                    if (br->err || dec->err) {
                        JXL_ERR(ctx, "modular: truncated stream %u",
                                (unsigned)stream_idx);
                        goto done;
                    }
                    for (y = 1; y < ch->h; y++) {
                        const int32_t *rtop;
                        int32_t left, topleft;
                        row = ch->data + (size_t)y * ch->stride;
                        rtop = row - ch->stride;
                        if (rle1_run) {
                            rle1_run--;
                        } else {
                            uint32_t token = jxl_dec_read_prefix_rle1(
                                dec, br, cluster, &rle1_value, &rle1_run,
                                &rle1_have);
                            rle1_diff = jxl_unpack_signed(token);
                        }
                        row[0] = (int32_t)((uint32_t)rle1_diff +
                                           (uint32_t)rtop[0]);
                        left = row[0];
                        topleft = rtop[0];
                        for (x = 1; x < ch->w; x++) {
                            int32_t top;
                            if (rle1_run) {
                                rle1_run--;
                            } else {
                                uint32_t token = jxl_dec_read_prefix_rle1(
                                    dec, br, cluster, &rle1_value, &rle1_run,
                                    &rle1_have);
                                rle1_diff = jxl_unpack_signed(token);
                            }
                            top = rtop[x];
                            left = (int32_t)((uint32_t)rle1_diff +
                                (uint32_t)grad_clamped(top, left, topleft));
                            row[x] = left;
                            topleft = top;
                        }
                        if (br->err || dec->err) {
                            JXL_ERR(ctx, "modular: truncated stream %u",
                                    (unsigned)stream_idx);
                            goto done;
                        }
                    }
                    continue;
                }

#define JXL_FIXED_LOOP(GUESS)                                                 \
    for (y = 0; y < ch->h; y++) {                                             \
        int32_t *row = ch->data + (size_t)y * ch->stride;                     \
        const int32_t *rtop = y ? row - ch->stride : row;                     \
        for (x = 0; x < ch->w; x++) {                                         \
            int32_t left = x ? row[x - 1] : (y ? rtop[0] : 0);                \
            int32_t top = y ? rtop[x] : left;                                 \
            int32_t topleft = (x && y) ? rtop[x - 1] : left;                  \
            uint32_t token = jxl_dec_read_clustered(dec, br, cluster,         \
                                                    dist_multiplier);         \
            int32_t diff = jxl_unpack_signed(token);                          \
            (void)top; (void)topleft;                                         \
            diff = (int32_t)((uint32_t)diff * mult + (uint32_t)off);          \
            row[x] = (int32_t)((uint32_t)diff + (uint32_t)(GUESS));           \
        }                                                                     \
        if (br->err || dec->err) {                                            \
            JXL_ERR(ctx, "modular: truncated stream %u",                      \
                    (unsigned)stream_idx);                                    \
            goto done;                                                        \
        }                                                                     \
    }

                switch (predictor) {
                    case JXL_PRED_ZERO:
                        JXL_FIXED_LOOP(0) break;
                    case JXL_PRED_WEST:
                        JXL_FIXED_LOOP(left) break;
                    case JXL_PRED_NORTH:
                        JXL_FIXED_LOOP(top) break;
                    case JXL_PRED_NORTH_WEST:
                        JXL_FIXED_LOOP(topleft) break;
                    case JXL_PRED_GRADIENT:
                        JXL_FIXED_LOOP(grad_clamped(top, left, topleft)) break;
                    case JXL_PRED_AVG_W_N:
                        JXL_FIXED_LOOP((int32_t)(((int64_t)left + top) / 2))
                        break;
                    case JXL_PRED_AVG_W_NW:
                        JXL_FIXED_LOOP((int32_t)(((int64_t)left + topleft) / 2))
                        break;
                    case JXL_PRED_AVG_N_NW:
                        JXL_FIXED_LOOP((int32_t)(((int64_t)top + topleft) / 2))
                        break;
                    case JXL_PRED_SELECT:
                        JXL_FIXED_LOOP(sel_pred(top, left, topleft)) break;
                    default:

                        for (y = 0; y < ch->h; y++) {
                            int32_t *row = ch->data + (size_t)y * ch->stride;
                            for (x = 0; x < ch->w; x++) {
                                uint32_t token = jxl_dec_read_clustered(
                                    dec, br, cluster, dist_multiplier);
                                int32_t diff = jxl_unpack_signed(token);
                                int32_t value;
                                diff = (int32_t)((uint32_t)diff * mult +
                                                 (uint32_t)off);
                                value = (int32_t)((uint32_t)diff +
                                        (uint32_t)predict_sample(&ps, &pr0,
                                                                 predictor));
                                row[x] = value;
                                pred_record(&ps, &pr0, value);
                            }
                            if (br->err || dec->err) {
                                JXL_ERR(ctx, "modular: truncated stream %u",
                                        (unsigned)stream_idx);
                                goto done;
                            }
                        }
                        break;
                }
#undef JXL_FIXED_LOOP
                continue;
            }
        }

#define JXL_GENERAL_LOOP(GET_LEAF, COMPUTE_PROPS)                            \
        for (y = 0; y < ch->h; y++) {                                        \
            int32_t *row = ch->data + (size_t)y * ch->stride;                 \
            for (x = 0; x < ch->w; x++) {                                    \
                jxl_props pr;                                                 \
                const jxl_ma_leaf *leaf;                                      \
                uint32_t token;                                               \
                int32_t diff, value;                                          \
                                                                              \
                COMPUTE_PROPS;                                                \
                leaf = (GET_LEAF);                                            \
                token = jxl_dec_read_clustered(dec, br, leaf->cluster,        \
                                               dist_multiplier);              \
                diff = jxl_unpack_signed(token);                              \
                diff = (int32_t)((uint32_t)diff * leaf->multiplier +          \
                                 (uint32_t)leaf->offset);                      \
                value = (int32_t)((uint32_t)diff +                            \
                    (uint32_t)predict_sample(&ps, &pr, leaf->predictor));      \
                row[x] = value;                                               \
                pred_record(&ps, &pr, value);                                 \
            }                                                                 \
            if (br->err || dec->err) {                                        \
                JXL_ERR(ctx, "modular: truncated stream %u",                  \
                        (unsigned)stream_idx);                                 \
                goto done;                                                    \
            }                                                                 \
        }

        if (max_prev == 0) {
            if (!need_sc && (props_mask & ~0x1f3u) == 0) {
                if (!dec->lz77_enabled) {
                    if (modular_decode_nw_nec(
                            ch, &ps, cma, dec, br, (int32_t)ci,
                            (int32_t)stream_idx) != 0) {
                        JXL_ERR(ctx, "modular: truncated stream %u",
                                (unsigned)stream_idx);
                        goto done;
                    }
                } else {
                    JXL_GENERAL_LOOP(
                        ma_get_leaf_local(cma, &pr),
                        props_compute_nw(
                            &ps, &pr, (int32_t)ci, (int32_t)stream_idx))
                }
            } else if ((props_mask & ~0x9e03u) == 0) {
                if (!dec->lz77_enabled) {
                    if (modular_decode_grad_wp_nec(
                            ch, &ps, cma, dec, br, (int32_t)ci,
                            (int32_t)stream_idx) != 0) {
                        JXL_ERR(ctx, "modular: truncated stream %u",
                                (unsigned)stream_idx);
                        goto done;
                    }
                } else {
                    JXL_GENERAL_LOOP(
                        ma_get_leaf_local(cma, &pr),
                        props_compute_grad_wp(
                            &ps, &pr, (int32_t)ci, (int32_t)stream_idx))
                }
            } else {
                if (modular_decode_general_nec(
                        ch, &ps, cma, dec, br, dist_multiplier, (int32_t)ci,
                        (int32_t)stream_idx) != 0) {
                    JXL_ERR(ctx, "modular: truncated stream %u",
                            (unsigned)stream_idx);
                    goto done;
                }
            }
        } else {
            JXL_GENERAL_LOOP(
                ma_get_leaf(cma, &ps, &pr),
                props_compute(&ps, &pr, (int32_t)ci, (int32_t)stream_idx))
        }
#undef JXL_GENERAL_LOOP
    }

    if (jxl_dec_finalize(dec) != 0) {
        JXL_ERR(ctx, "modular: bad ANS final state (stream %u)",
                (unsigned)stream_idx);
        goto done;
    }
    rc = 0;

done:
    jxl_free(ctx, spec.flat);
    if (wp_lut_local) jxl_free(ctx, wp_lut);
    pred_state_free(ctx, &ps);
    jxl_free(ctx, prev);
    return rc;
}

void jxl_patches_free(jxl_ctx *ctx, jxl_patches *p) {
    uint32_t i, j;
    if (!p || !p->refs) return;
    for (i = 0; i < p->n; i++) {
        for (j = 0; j < p->refs[i].ntargets; j++) {
            jxl_free(ctx, p->refs[i].targets[j].blending);
        }
        jxl_free(ctx, p->refs[i].targets);
    }
    jxl_free(ctx, p->refs);
    memset(p, 0, sizeof(*p));
}

int jxl_patches_read(jxl_ctx *ctx, jxl_br *br, const jxl_image_metadata *meta,
                     const jxl_frame_header *fh, jxl_patches *out) {
    jxl_dec dec;
    uint32_t num_refs, i;
    uint32_t max_refs, max_patches, total = 0;
    uint32_t nblend = meta->num_extra + 1;
    uint32_t first_alpha = 0;
    uint32_t nalpha = 0;
    int rc = -1;

    memset(out, 0, sizeof(*out));
    memset(&dec, 0, sizeof(dec));
    out->nblend = nblend;

    for (i = 0; i < meta->num_extra; i++) {
        if (meta->ec_info[i].type == JXL_EC_ALPHA) {
            if (nalpha == 0) first_alpha = i;
            nalpha++;
        }
    }

    if (jxl_dec_init(ctx, &dec, br, 10) != 0) return -1;
    jxl_dec_begin(&dec, br);

    max_refs = (uint32_t)JXL_MIN((uint64_t)1 << 24,
                                 (uint64_t)fh->width * fh->height / 16);
    max_patches = max_refs > (0xffffffffu / 4) ? 0xffffffffu : max_refs * 4;

    num_refs = jxl_dec_read(&dec, br, 0);
    if (num_refs > max_refs || br->err) {
        JXL_ERR(ctx, "patches: too many patch references (%u)", (unsigned)num_refs);
        goto done;
    }
    if (num_refs) {
        out->refs = (jxl_patch_ref *)jxl_calloc(ctx, num_refs, sizeof(jxl_patch_ref));
        if (!out->refs) goto done;
    }
    out->n = num_refs;

    for (i = 0; i < num_refs; i++) {
        jxl_patch_ref *pr = &out->refs[i];
        uint32_t count, t;
        int32_t px = 0, py = 0;
        int have_prev = 0;

        pr->ref_idx = jxl_dec_read(&dec, br, 1);
        if (pr->ref_idx >= 4) {
            JXL_ERR(ctx, "patches: reference index out of range");
            goto done;
        }
        pr->x0 = jxl_dec_read(&dec, br, 3);
        pr->y0 = jxl_dec_read(&dec, br, 3);
        pr->width = jxl_dec_read(&dec, br, 2) + 1;
        pr->height = jxl_dec_read(&dec, br, 2) + 1;
        count = jxl_dec_read(&dec, br, 7) + 1;
        total += count;
        if (total > max_patches || br->err || dec.err) {
            JXL_ERR(ctx, "patches: too many patches");
            goto done;
        }
        pr->targets = (jxl_patch_target *)jxl_calloc(ctx, count,
                                                     sizeof(jxl_patch_target));
        if (!pr->targets) goto done;
        pr->ntargets = count;

        for (t = 0; t < count; t++) {
            jxl_patch_target *tg = &pr->targets[t];
            uint32_t b;
            if (have_prev) {
                int32_t dx = jxl_unpack_signed(jxl_dec_read(&dec, br, 6));
                int32_t dy = jxl_unpack_signed(jxl_dec_read(&dec, br, 6));
                tg->x = (int32_t)((uint32_t)px + (uint32_t)dx);
                tg->y = (int32_t)((uint32_t)py + (uint32_t)dy);
            } else {
                tg->x = (int32_t)jxl_dec_read(&dec, br, 4);
                tg->y = (int32_t)jxl_dec_read(&dec, br, 4);
                have_prev = 1;
            }
            px = tg->x;
            py = tg->y;

            tg->blending = (jxl_patch_blend *)jxl_calloc(ctx, nblend,
                                                         sizeof(jxl_patch_blend));
            if (!tg->blending) goto done;
            for (b = 0; b < nblend; b++) {
                uint32_t raw = jxl_dec_read(&dec, br, 5);
                if (raw > JXL_PATCH_MULADD_BELOW) {
                    JXL_ERR(ctx, "patches: invalid blend mode %u", (unsigned)raw);
                    goto done;
                }
                tg->blending[b].mode = (uint8_t)raw;
                if (raw >= 4 && nalpha >= 2) {
                    tg->blending[b].alpha_channel = jxl_dec_read(&dec, br, 8);
                } else {
                    tg->blending[b].alpha_channel = first_alpha;
                }
                tg->blending[b].clamp =
                    (raw >= 3) ? (jxl_dec_read(&dec, br, 9) != 0) : 0;
            }
            if (br->err || dec.err) {
                JXL_ERR(ctx, "patches: truncated");
                goto done;
            }
        }
    }

    if (jxl_dec_finalize(&dec) != 0) {
        JXL_ERR(ctx, "patches: bad ANS final state");
        goto done;
    }
    rc = 0;

done:
    jxl_dec_free(&dec);
    if (rc != 0) jxl_patches_free(ctx, out);
    return rc;
}

static float clamp01(float v) {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

static const jxl_fplane *plane_or_null(const jxl_fimage *img, uint32_t idx) {
    if (!img || idx >= img->nplane) return NULL;
    return &img->plane[idx];
}

static float plane_at(const jxl_fplane *p, uint32_t x, uint32_t y) {
    if (!p || !p->data || x >= p->w || y >= p->h) return 0.0f;
    return p->data[(size_t)y * p->stride + x];
}

static void blend_channel(jxl_fplane *base, const jxl_fplane *ref,
                          const jxl_fplane *base_alpha,
                          const jxl_fplane *ref_alpha, int is_alpha_channel,
                          const jxl_patch_blend *bi, int premultiplied,
                          int32_t bx, int32_t by, uint32_t rx, uint32_t ry,
                          uint32_t w, uint32_t h) {
    uint32_t dx, dy;
    int swapped = (bi->mode == JXL_PATCH_BLEND_BELOW ||
                   bi->mode == JXL_PATCH_MULADD_BELOW);
    jxl_patch_blend eff = *bi;

    if (!is_alpha_channel && !ref_alpha) {
        if (eff.mode == JXL_PATCH_BLEND_ABOVE || eff.mode == JXL_PATCH_BLEND_BELOW) {
            eff.mode = JXL_PATCH_REPLACE;
        } else if (eff.mode == JXL_PATCH_MULADD_ABOVE ||
                   eff.mode == JXL_PATCH_MULADD_BELOW) {
            eff.mode = JXL_PATCH_ADD;
        }
    }
    bi = &eff;

    for (dy = 0; dy < h; dy++) {
        float *brow = base->data + (size_t)(by + dy) * base->stride + bx;
        const float *rrow = ref->data + (size_t)(ry + dy) * ref->stride + rx;
        for (dx = 0; dx < w; dx++) {
            float b = brow[dx];
            float n = rrow[dx];
            switch (bi->mode) {
                case JXL_PATCH_NONE:
                    break;
                case JXL_PATCH_REPLACE:
                    brow[dx] = n;
                    break;
                case JXL_PATCH_ADD:
                    brow[dx] = b + n;
                    break;
                case JXL_PATCH_MUL:
                    brow[dx] = b * (bi->clamp ? clamp01(n) : n);
                    break;
                case JXL_PATCH_BLEND_ABOVE:
                case JXL_PATCH_BLEND_BELOW: {
                    float ba, na, bs, ns;
                    if (is_alpha_channel) {

                        float lo = b, hi = n;
                        if (swapped) { lo = n; hi = b; }
                        if (bi->clamp) hi = clamp01(hi);
                        brow[dx] = lo + hi * (1.0f - lo);
                        break;
                    }
                    if (swapped) {
                        bs = n; ns = b;
                        ba = plane_at(ref_alpha, rx + dx, ry + dy);
                        na = plane_at(base_alpha, (uint32_t)bx + dx, (uint32_t)by + dy);
                    } else {
                        bs = b; ns = n;
                        ba = plane_at(base_alpha, (uint32_t)bx + dx, (uint32_t)by + dy);
                        na = plane_at(ref_alpha, rx + dx, ry + dy);
                    }
                    if (bi->clamp) na = clamp01(na);
                    if (premultiplied) {
                        brow[dx] = ns + bs * (1.0f - na);
                    } else {
                        float bar = 1.0f - ba;
                        float nar = 1.0f - na;
                        float mixed = 1.0f - nar * bar;
                        float recip = mixed > 0.0f ? 1.0f / mixed : 0.0f;
                        brow[dx] = (na * ns + ba * bs * nar) * recip;
                    }
                    break;
                }
                case JXL_PATCH_MULADD_ABOVE:
                case JXL_PATCH_MULADD_BELOW: {
                    float bs, ns, na;
                    if (is_alpha_channel) {
                        if (swapped) brow[dx] = n;
                        break;
                    }
                    if (swapped) {
                        bs = n; ns = b;
                        na = plane_at(base_alpha, (uint32_t)bx + dx, (uint32_t)by + dy);
                    } else {
                        bs = b; ns = n;
                        na = plane_at(ref_alpha, rx + dx, ry + dy);
                    }
                    if (bi->clamp) na = clamp01(na);
                    brow[dx] = bs + na * ns;
                    break;
                }
                default:
                    break;
            }
        }
    }
}

int jxl_apply_patches(jxl_ctx *ctx, jxl_fimage *img, const jxl_patches *p,
                      const jxl_image_metadata *meta, jxl_fimage refs[4],
                      const int refs_valid[4]) {
    uint32_t i, t, c;
    uint32_t ncolor = img->ncolor;

    for (i = 0; i < p->n; i++) {
        const jxl_patch_ref *pr = &p->refs[i];
        const jxl_fimage *ref;
        if (pr->ref_idx >= 4 || !refs_valid[pr->ref_idx]) {
            JXL_ERR(ctx, "patches: reference frame %u not available",
                    (unsigned)pr->ref_idx);
            return -1;
        }
        ref = &refs[pr->ref_idx];

        for (t = 0; t < pr->ntargets; t++) {
            const jxl_patch_target *tg = &pr->targets[t];
            for (c = 0; c < img->nplane; c++) {
                const jxl_patch_blend *bi;
                jxl_fplane *base = &img->plane[c];
                const jxl_fplane *rp = plane_or_null(ref, c);
                const jxl_fplane *base_alpha = NULL, *ref_alpha = NULL;
                int is_alpha_channel = 0, premultiplied = 0;
                int64_t bx0, by0, rx0, ry0;
                int64_t w, h;

                if (!rp || !rp->data || !base->data) continue;
                bi = (c < ncolor) ? &tg->blending[0]
                                  : &tg->blending[c - ncolor + 1];
                if (bi->mode == JXL_PATCH_NONE) continue;

                if (bi->mode >= JXL_PATCH_BLEND_ABOVE) {
                    uint32_t ai = ncolor + bi->alpha_channel;
                    is_alpha_channel = (c == ai);
                    base_alpha = plane_or_null(img, ai);
                    ref_alpha = plane_or_null(ref, ai);
                    if (bi->alpha_channel < meta->num_extra) {
                        premultiplied =
                            meta->ec_info[bi->alpha_channel].alpha_associated;
                    }
                }

                bx0 = tg->x;
                by0 = tg->y;
                rx0 = pr->x0;
                ry0 = pr->y0;
                w = pr->width;
                h = pr->height;
                if (bx0 < 0) { rx0 -= bx0; w += bx0; bx0 = 0; }
                if (by0 < 0) { ry0 -= by0; h += by0; by0 = 0; }
                if (bx0 + w > base->w) w = (int64_t)base->w - bx0;
                if (by0 + h > base->h) h = (int64_t)base->h - by0;
                if (rx0 + w > rp->w) w = (int64_t)rp->w - rx0;
                if (ry0 + h > rp->h) h = (int64_t)rp->h - ry0;
                if (w <= 0 || h <= 0 || rx0 < 0 || ry0 < 0) continue;

                blend_channel(base, rp, base_alpha, ref_alpha, is_alpha_channel,
                              bi, premultiplied, (int32_t)bx0, (int32_t)by0,
                              (uint32_t)rx0, (uint32_t)ry0, (uint32_t)w,
                              (uint32_t)h);
            }
        }
    }
    return 0;
}

static uint8_t frame_mode_to_patch(jxl_blend_mode m) {
    switch (m) {
        case JXL_BLEND_REPLACE: return JXL_PATCH_REPLACE;
        case JXL_BLEND_ADD: return JXL_PATCH_ADD;
        case JXL_BLEND_BLEND: return JXL_PATCH_BLEND_ABOVE;
        case JXL_BLEND_MULADD: return JXL_PATCH_MULADD_ABOVE;
        default: return JXL_PATCH_MUL;
    }
}

int jxl_blend_frame(jxl_ctx *ctx, jxl_fimage *canvas, const jxl_fimage *frame,
                    const jxl_frame_header *fh,
                    const jxl_image_metadata *meta) {
    uint32_t c;
    uint32_t ncolor = canvas->ncolor;

    for (c = 0; c < canvas->nplane && c < frame->nplane; c++) {
        const jxl_blending_info *fbi =
            (c < ncolor) ? &fh->blending
                         : (fh->ec_blending ? &fh->ec_blending[c - ncolor]
                                            : &fh->blending);
        jxl_patch_blend bi;
        jxl_fplane *base = &canvas->plane[c];
        const jxl_fplane *nf = &frame->plane[c];
        const jxl_fplane *base_alpha = NULL, *new_alpha = NULL;
        int is_alpha_channel = 0, premultiplied = 0;
        int64_t bx0, by0, rx0 = 0, ry0 = 0, w, h;

        if (!base->data || !nf->data) continue;
        bi.mode = frame_mode_to_patch(fbi->mode);
        bi.alpha_channel = fbi->alpha_channel;
        bi.clamp = fbi->clamp;

        if (bi.mode >= JXL_PATCH_BLEND_ABOVE) {
            uint32_t ai = ncolor + bi.alpha_channel;
            is_alpha_channel = (c == ai);
            base_alpha = plane_or_null(canvas, ai);
            new_alpha = plane_or_null(frame, ai);
            if (bi.alpha_channel < meta->num_extra) {
                premultiplied = meta->ec_info[bi.alpha_channel].alpha_associated;
            }
        }

        bx0 = fh->x0;
        by0 = fh->y0;
        w = nf->w;
        h = nf->h;
        if (bx0 < 0) { rx0 -= bx0; w += bx0; bx0 = 0; }
        if (by0 < 0) { ry0 -= by0; h += by0; by0 = 0; }
        if (bx0 + w > base->w) w = (int64_t)base->w - bx0;
        if (by0 + h > base->h) h = (int64_t)base->h - by0;
        if (rx0 + w > nf->w) w = (int64_t)nf->w - rx0;
        if (ry0 + h > nf->h) h = (int64_t)nf->h - ry0;
        if (w <= 0 || h <= 0) continue;

        blend_channel(base, nf, base_alpha, new_alpha, is_alpha_channel, &bi,
                      premultiplied, (int32_t)bx0, (int32_t)by0, (uint32_t)rx0,
                      (uint32_t)ry0, (uint32_t)w, (uint32_t)h);
    }
    (void)ctx;
    return 0;
}

int jxl_fimage_blank_like(jxl_ctx *ctx, jxl_fimage *out, const jxl_fimage *like,
                          uint32_t w, uint32_t h) {
    uint32_t i;
    if (jxl_fimage_alloc(ctx, out, like->nplane) != 0) return -1;
    out->ncolor = like->ncolor;
    out->w = w;
    out->h = h;
    for (i = 0; i < like->nplane; i++) {
        uint32_t pw = like->plane[i].w ? w : 0;
        uint32_t ph = like->plane[i].h ? h : 0;
        if (jxl_fplane_alloc(ctx, &out->plane[i], pw, ph) != 0) return -1;
    }
    return 0;
}

int jxl_fimage_copy(jxl_ctx *ctx, jxl_fimage *dst, const jxl_fimage *src) {
    uint32_t i, y;
    if (jxl_fimage_alloc(ctx, dst, src->nplane) != 0) return -1;
    dst->ncolor = src->ncolor;
    dst->w = src->w;
    dst->h = src->h;
    for (i = 0; i < src->nplane; i++) {
        const jxl_fplane *s = &src->plane[i];
        if (jxl_fplane_alloc_uninit(ctx, &dst->plane[i], s->w, s->h) != 0) return -1;
        for (y = 0; y < s->h; y++) {
            memcpy(dst->plane[i].data + (size_t)y * dst->plane[i].stride,
                   s->data + (size_t)y * s->stride, (size_t)s->w * sizeof(float));
        }
    }
    return 0;
}

#include <math.h>

#if !defined(JXL_SPLINE_FORCE_SCALAR) && \
    (defined(__SSE2__) || defined(_M_X64) || defined(_M_AMD64) || \
     (defined(_M_IX86_FP) && _M_IX86_FP >= 2))
#define JXL_SPLINE_SSE2 1
#include <emmintrin.h>
#include <immintrin.h>
#endif

#define SPLINE_MAX_POINTS (1u << 20)

void jxl_splines_free(jxl_ctx *ctx, jxl_splines *sp) {
    uint32_t i;
    if (!sp || !sp->splines) return;
    for (i = 0; i < sp->n; i++) {
        jxl_free(ctx, sp->splines[i].px);
        jxl_free(ctx, sp->splines[i].py);
    }
    jxl_free(ctx, sp->splines);
    memset(sp, 0, sizeof(*sp));
}

int jxl_splines_read(jxl_ctx *ctx, jxl_br *br, const jxl_frame_header *fh,
                     jxl_splines *out) {
    jxl_dec dec;
    uint64_t num_pixels = (uint64_t)fh->width * fh->height;
    uint32_t num_splines, i;
    int64_t *sx = NULL, *sy = NULL;
    int64_t prev_x, prev_y;
    size_t acc_points = 0;
    int rc = -1;

    memset(out, 0, sizeof(*out));
    memset(&dec, 0, sizeof(dec));
    if (jxl_dec_init(ctx, &dec, br, 6) != 0) return -1;
    jxl_dec_begin(&dec, br);

    num_splines = jxl_dec_read(&dec, br, 2);
    if ((uint64_t)num_splines >= JXL_MIN((uint64_t)1 << 24, num_pixels / 4)) {
        JXL_ERR(ctx, "splines: too many splines");
        goto done;
    }
    num_splines += 1;

    sx = (int64_t *)jxl_calloc(ctx, num_splines, sizeof(int64_t));
    sy = (int64_t *)jxl_calloc(ctx, num_splines, sizeof(int64_t));
    if (!sx || !sy) goto done;

    prev_x = (int64_t)jxl_dec_read(&dec, br, 1);
    prev_y = (int64_t)jxl_dec_read(&dec, br, 1);
    sx[0] = prev_x;
    sy[0] = prev_y;
    for (i = 1; i < num_splines; i++) {
        int32_t dx = jxl_unpack_signed(jxl_dec_read(&dec, br, 1));
        int32_t dy = jxl_unpack_signed(jxl_dec_read(&dec, br, 1));
        prev_x += dx;
        prev_y += dy;
        sx[i] = prev_x;
        sy[i] = prev_y;
    }

    out->quant_adjust = jxl_unpack_signed(jxl_dec_read(&dec, br, 0));
    out->splines = (jxl_quant_spline *)jxl_calloc(ctx, num_splines,
                                                  sizeof(jxl_quant_spline));
    if (!out->splines) goto done;
    out->n = num_splines;

    for (i = 0; i < num_splines; i++) {
        jxl_quant_spline *qs = &out->splines[i];
        uint32_t num_points = jxl_dec_read(&dec, br, 3);
        int64_t cx = sx[i], cy = sy[i];
        int64_t dx_acc = 0, dy_acc = 0;
        uint32_t k;
        int c, j;

        acc_points += num_points;
        if (acc_points > JXL_MIN((uint64_t)SPLINE_MAX_POINTS, num_pixels / 2)) {
            JXL_ERR(ctx, "splines: too many control points");
            goto done;
        }
        qs->px = (int64_t *)jxl_calloc(ctx, num_points + 1, sizeof(int64_t));
        qs->py = (int64_t *)jxl_calloc(ctx, num_points + 1, sizeof(int64_t));
        if (!qs->px || !qs->py) goto done;
        qs->npoints = num_points + 1;
        qs->px[0] = cx;
        qs->py[0] = cy;

        for (k = 0; k < num_points; k++) {
            int64_t prev_px = cx, prev_py = cy;
            dx_acc += jxl_unpack_signed(jxl_dec_read(&dec, br, 4));
            dy_acc += jxl_unpack_signed(jxl_dec_read(&dec, br, 4));
            cx += dx_acc;
            cy += dy_acc;
            if (cx == prev_px && cy == prev_py) {
                JXL_ERR(ctx, "splines: duplicate control point");
                goto done;
            }
            qs->px[k + 1] = cx;
            qs->py[k + 1] = cy;
        }

        for (c = 0; c < 3; c++) {
            for (j = 0; j < 32; j++) {
                qs->xyb_dct[c][j] = jxl_unpack_signed(jxl_dec_read(&dec, br, 5));
            }
        }
        for (j = 0; j < 32; j++) {
            qs->sigma_dct[j] = jxl_unpack_signed(jxl_dec_read(&dec, br, 5));
        }
        if (br->err || dec.err) {
            JXL_ERR(ctx, "splines: truncated");
            goto done;
        }
    }

    if (jxl_dec_finalize(&dec) != 0) {
        JXL_ERR(ctx, "splines: bad ANS final state");
        goto done;
    }
    rc = 0;

done:
    jxl_free(ctx, sx);
    jxl_free(ctx, sy);
    jxl_dec_free(&dec);
    if (rc != 0) jxl_splines_free(ctx, out);
    return rc;
}

typedef struct { float x, y; } jxl_pt;

static jxl_pt pt_add(jxl_pt a, jxl_pt b) { jxl_pt r; r.x = a.x + b.x; r.y = a.y + b.y; return r; }
static jxl_pt pt_sub(jxl_pt a, jxl_pt b) { jxl_pt r; r.x = a.x - b.x; r.y = a.y - b.y; return r; }
static jxl_pt pt_mul(jxl_pt a, float s) { jxl_pt r; r.x = a.x * s; r.y = a.y * s; return r; }
static float pt_norm2(jxl_pt a) { return a.x * a.x + a.y * a.y; }
static float pt_norm(jxl_pt a) { return sqrtf(pt_norm2(a)); }

static float continuous_idct(const float dct[32], float t) {
    float res = dct[0];
    int i;
    for (i = 1; i < 32; i++) {
        float theta = (float)i * (3.14159265358979323846f / 32.0f) * (t + 0.5f);
        res += 1.4142135623730951f * dct[i] * cosf(theta);
    }
    return res;
}

#ifdef JXL_SPLINE_SSE2

static JXL_INLINE_HINT __m128 spline_erf4(__m128 x) {
    const __m128 sign = _mm_set1_ps(-0.0f);
    const __m128 one = _mm_set1_ps(1.0f);
    __m128 ax = _mm_andnot_ps(sign, x);
    __m128 d = _mm_add_ps(_mm_mul_ps(ax, _mm_set1_ps(7.77394369e-02f)),
                          _mm_set1_ps(2.05260015e-04f));
    __m128 inv, res, neg, lt;
    d = _mm_add_ps(_mm_mul_ps(d, ax), _mm_set1_ps(2.32120216e-01f));
    d = _mm_add_ps(_mm_mul_ps(d, ax), _mm_set1_ps(2.77820801e-01f));
    d = _mm_add_ps(_mm_mul_ps(d, ax), one);
    d = _mm_mul_ps(d, d);
    inv = _mm_div_ps(one, d);
    res = _mm_add_ps(_mm_xor_ps(_mm_mul_ps(inv, inv), sign), one);
    neg = _mm_xor_ps(res, sign);
    lt = _mm_cmplt_ps(x, _mm_setzero_ps());
    return _mm_or_ps(_mm_and_ps(lt, neg), _mm_andnot_ps(lt, res));
}

JXL_TARGET_AVX2
static __m256 spline_erf8(__m256 x) {
    const __m256 sign = _mm256_set1_ps(-0.0f);
    const __m256 one = _mm256_set1_ps(1.0f);
    __m256 ax = _mm256_andnot_ps(sign, x);
    __m256 d = _mm256_add_ps(_mm256_mul_ps(ax, _mm256_set1_ps(7.77394369e-02f)),
                             _mm256_set1_ps(2.05260015e-04f));
    __m256 inv, res, neg, lt;
    d = _mm256_add_ps(_mm256_mul_ps(d, ax), _mm256_set1_ps(2.32120216e-01f));
    d = _mm256_add_ps(_mm256_mul_ps(d, ax), _mm256_set1_ps(2.77820801e-01f));
    d = _mm256_add_ps(_mm256_mul_ps(d, ax), one);
    d = _mm256_mul_ps(d, d);
    inv = _mm256_div_ps(one, d);
    res = _mm256_add_ps(_mm256_xor_ps(_mm256_mul_ps(inv, inv), sign), one);
    neg = _mm256_xor_ps(res, sign);
    lt = _mm256_cmp_ps(x, _mm256_setzero_ps(), _CMP_LT_OQ);
    return _mm256_or_ps(_mm256_and_ps(lt, neg), _mm256_andnot_ps(lt, res));
}

JXL_TARGET_AVX2
static int32_t spline_row_avx2(float *const rows[3], int32_t x, int32_t xe,
                               float px, float dy2, float inv_sigma,
                               const float vs[3]) {
    const __m256 vpx = _mm256_set1_ps(px);
    const __m256 vdy2 = _mm256_set1_ps(dy2);
    const __m256 vinv = _mm256_set1_ps(inv_sigma);
    const __m256 vk = _mm256_set1_ps(0.35355338f);
    const __m256 vhalf = _mm256_set1_ps(0.5f);
    const __m256 vs0 = _mm256_set1_ps(vs[0]);
    const __m256 vs1 = _mm256_set1_ps(vs[1]);
    const __m256 vs2 = _mm256_set1_ps(vs[2]);
    __m256i xi = _mm256_setr_epi32(x, x + 1, x + 2, x + 3,
                                   x + 4, x + 5, x + 6, x + 7);
    for (; x + 8 <= xe; x += 8) {
        __m256 dxv = _mm256_sub_ps(_mm256_cvtepi32_ps(xi), vpx);
        __m256 dist = _mm256_sqrt_ps(
            _mm256_add_ps(_mm256_mul_ps(dxv, dxv), vdy2));
        __m256 h = _mm256_mul_ps(vhalf, dist);
        __m256 f = _mm256_sub_ps(
            spline_erf8(_mm256_mul_ps(_mm256_add_ps(h, vk), vinv)),
            spline_erf8(_mm256_mul_ps(_mm256_sub_ps(h, vk), vinv)));
        __m256 ffv = _mm256_mul_ps(f, f);
        _mm256_storeu_ps(rows[0] + x,
            _mm256_add_ps(_mm256_loadu_ps(rows[0] + x),
                          _mm256_mul_ps(vs0, ffv)));
        _mm256_storeu_ps(rows[1] + x,
            _mm256_add_ps(_mm256_loadu_ps(rows[1] + x),
                          _mm256_mul_ps(vs1, ffv)));
        _mm256_storeu_ps(rows[2] + x,
            _mm256_add_ps(_mm256_loadu_ps(rows[2] + x),
                          _mm256_mul_ps(vs2, ffv)));
        xi = _mm256_add_epi32(xi, _mm256_set1_epi32(8));
    }
    _mm256_zeroupper();
    return x;
}
#endif

static float spline_erf(float x) {
    float ax = fabsf(x);
    float d1 = ax * 7.77394369e-02f + 2.05260015e-04f;
    float d2 = d1 * ax + 2.32120216e-01f;
    float d3 = d2 * ax + 2.77820801e-01f;
    float d4 = d3 * ax + 1.0f;
    float d5 = d4 * d4;
    float inv = 1.0f / d5;
    float result = -inv * inv + 1.0f;
    return x < 0.0f ? -result : result;
}

static int upsample_points(jxl_ctx *ctx, const jxl_quant_spline *qs,
                           jxl_pt **out, uint32_t *out_n) {
    uint32_t n = qs->npoints, ext_n, i, cnt = 0;
    jxl_pt *ext, *up;
    uint32_t cap;

    if (n == 1) {
        up = (jxl_pt *)jxl_calloc(ctx, 1, sizeof(jxl_pt));
        if (!up) return -1;
        up[0].x = (float)qs->px[0];
        up[0].y = (float)qs->py[0];
        *out = up;
        *out_n = 1;
        return 0;
    }

    ext_n = n + 2;
    ext = (jxl_pt *)jxl_calloc(ctx, ext_n, sizeof(jxl_pt));
    if (!ext) return -1;

    ext[0].x = 2.0f * (float)qs->px[0] - (float)qs->px[1];
    ext[0].y = 2.0f * (float)qs->py[0] - (float)qs->py[1];
    for (i = 0; i < n; i++) {
        ext[i + 1].x = (float)qs->px[i];
        ext[i + 1].y = (float)qs->py[i];
    }
    ext[ext_n - 1].x = 2.0f * (float)qs->px[n - 1] - (float)qs->px[n - 2];
    ext[ext_n - 1].y = 2.0f * (float)qs->py[n - 1] - (float)qs->py[n - 2];

    cap = 16 * (ext_n - 3) + 1;
    up = (jxl_pt *)jxl_calloc(ctx, cap, sizeof(jxl_pt));
    if (!up) {
        jxl_free(ctx, ext);
        return -1;
    }

    for (i = 0; i + 3 < ext_n; i++) {
        jxl_pt p[4], a[3], b[2];
        float t[4];
        int k, step;
        for (k = 0; k < 4; k++) p[k] = ext[i + k];
        up[cnt++] = p[1];
        t[0] = 0.0f;
        for (k = 1; k < 4; k++) {
            t[k] = t[k - 1] + powf(pt_norm2(pt_sub(p[k], p[k - 1])), 0.25f);
        }
        for (step = 1; step < 16; step++) {
            float knot = t[1] + ((float)step / 16.0f) * (t[2] - t[1]);
            for (k = 0; k < 3; k++) {
                a[k] = pt_add(p[k], pt_mul(pt_sub(p[k + 1], p[k]),
                                           (knot - t[k]) / (t[k + 1] - t[k])));
            }
            for (k = 0; k < 2; k++) {
                b[k] = pt_add(a[k], pt_mul(pt_sub(a[k + 1], a[k]),
                                           (knot - t[k]) / (t[k + 2] - t[k])));
            }
            up[cnt++] = pt_add(b[0], pt_mul(pt_sub(b[1], b[0]),
                                            (knot - t[1]) / (t[2] - t[1])));
        }
    }
    up[cnt].x = (float)qs->px[n - 1];
    up[cnt].y = (float)qs->py[n - 1];
    cnt++;

    jxl_free(ctx, ext);
    *out = up;
    *out_n = cnt;
    return 0;
}

int jxl_render_splines(jxl_ctx *ctx, jxl_fimage *img, const jxl_splines *sp,
                       const jxl_frame_header *fh, float corr_x, float corr_b) {
    uint32_t si, cw, ch;
    int rc = -1;
    jxl_pt *up = NULL;
    jxl_pt *arc_pt = NULL;
    float *arc_len = NULL;
#ifdef JXL_SPLINE_SSE2
    const int use_avx2 = jxl_has_avx2();
#endif

    if (img->ncolor < 3) return 0;

    cw = fh->width;
    ch = fh->height;
    for (si = 0; si < 3; si++) {
        if (img->plane[si].w < cw) cw = img->plane[si].w;
        if (img->plane[si].h < ch) ch = img->plane[si].h;
    }

    for (si = 0; si < sp->n; si++) {
        const jxl_quant_spline *qs = &sp->splines[si];
        float xyb_dct[3][32], sigma_dct[32];
        float qa = (float)sp->quant_adjust;
        float inv_qa = qa >= 0.0f ? 1.0f / (1.0f + qa / 8.0f) : 1.0f - qa / 8.0f;
        static const float chan_w[4] = {0.0042f, 0.075f, 0.07f, 0.3333f};
        uint32_t up_n = 0, nsamples = 0, cap, i, next_idx;
        float total_arclength;
        jxl_pt current;
        int c, j;

        for (c = 0; c < 3; c++) {
            for (j = 0; j < 32; j++) {
                xyb_dct[c][j] = (float)qs->xyb_dct[c][j] * chan_w[c] * inv_qa;
            }
        }
        for (j = 0; j < 32; j++) {
            xyb_dct[0][j] += corr_x * xyb_dct[1][j];
            xyb_dct[2][j] += corr_b * xyb_dct[1][j];
            sigma_dct[j] = (float)qs->sigma_dct[j] * chan_w[3] * inv_qa;
        }

        jxl_free(ctx, up);
        up = NULL;
        if (upsample_points(ctx, qs, &up, &up_n) != 0) goto done;

        {
            float total = 0.0f;
            uint32_t k;
            for (k = 1; k < up_n; k++) total += pt_norm(pt_sub(up[k], up[k - 1]));
            if (!(total >= 0.0f) || total > 1e8f) total = 0.0f;
            cap = up_n + (uint32_t)total + 4;
        }
        jxl_free(ctx, arc_pt);
        jxl_free(ctx, arc_len);
        arc_pt = (jxl_pt *)jxl_calloc(ctx, cap, sizeof(jxl_pt));
        arc_len = (float *)jxl_calloc(ctx, cap, sizeof(float));
        if (!arc_pt || !arc_len) goto done;

        current = up[0];
        arc_pt[0] = current;
        arc_len[0] = 1.0f;
        nsamples = 1;
        next_idx = 0;
        while (next_idx < up_n) {
            jxl_pt prev = current;
            float acc = 0.0f;
            for (;;) {
                jxl_pt next;
                float to_next;
                if (next_idx >= up_n) {
                    if (nsamples < cap) {
                        arc_pt[nsamples] = prev;
                        arc_len[nsamples] = acc;
                        nsamples++;
                    }
                    break;
                }
                next = up[next_idx];
                to_next = pt_norm(pt_sub(next, prev));
                if (acc + to_next >= 1.0f) {
                    float f = to_next > 0.0f ? (1.0f - acc) / to_next : 0.0f;
                    current = pt_add(prev, pt_mul(pt_sub(up[next_idx], prev), f));
                    if (nsamples < cap) {
                        arc_pt[nsamples] = current;
                        arc_len[nsamples] = 1.0f;
                        nsamples++;
                    }
                    break;
                }
                acc += to_next;
                prev = next;
                next_idx++;
            }
            if (nsamples >= cap) break;
        }
        if (nsamples == 0) continue;

        total_arclength = (float)nsamples - 2.0f + arc_len[nsamples - 1];
        for (i = 0; i < nsamples; i++) {
            float from_start = (float)i / total_arclength;
            float t, sigma, inv_sigma, values[3], max_color, max_distance;
            float vs[3], px, py;
            int32_t xb, xe, yb, ye, x, y;

            if (from_start > 1.0f) from_start = 1.0f;
            t = 31.0f * from_start;
            sigma = continuous_idct(sigma_dct, t);
            if (sigma == 0.0f) continue;
            inv_sigma = 1.0f / sigma;
            for (c = 0; c < 3; c++) {
                values[c] = continuous_idct(xyb_dct[c], t) * arc_len[i];
            }

            max_color = 0.01f;
            for (c = 0; c < 3; c++) {
                float a = fabsf(values[c]);
                if (a > max_color) max_color = a;
            }
            max_distance = -2.0f * sigma * sigma *
                           (logf(0.1f) * 3.0f - logf(max_color));
            max_distance = max_distance > 0.0f ? sqrtf(max_distance) : 0.0f;

            px = arc_pt[i].x;
            py = arc_pt[i].y;
            xb = (int32_t)floorf(px - max_distance + 0.5f);
            xe = (int32_t)floorf(px + max_distance + 1.5f);
            yb = (int32_t)floorf(py - max_distance + 0.5f);
            ye = (int32_t)floorf(py + max_distance + 1.5f);
            if (xb < 0) xb = 0;
            if (yb < 0) yb = 0;
            if (xe > (int32_t)cw) xe = (int32_t)cw;
            if (ye > (int32_t)ch) ye = (int32_t)ch;

            for (c = 0; c < 3; c++) vs[c] = 0.25f * values[c] * sigma;

            for (y = yb; y < ye; y++) {
                float *rows[3];
                float dy = (float)y - py;
                float dy2 = dy * dy;
                for (c = 0; c < 3; c++) {
                    rows[c] = img->plane[c].data +
                              (size_t)y * img->plane[c].stride;
                }
                x = xb;
#ifdef JXL_SPLINE_SSE2
                if (use_avx2) {
                    x = spline_row_avx2(rows, x, xe, px, dy2, inv_sigma, vs);
                }
                {

                    const __m128 vpx = _mm_set1_ps(px);
                    const __m128 vdy2 = _mm_set1_ps(dy2);
                    const __m128 vinv = _mm_set1_ps(inv_sigma);
                    const __m128 vk = _mm_set1_ps(0.35355338f);
                    const __m128 vs0 = _mm_set1_ps(vs[0]);
                    const __m128 vs1 = _mm_set1_ps(vs[1]);
                    const __m128 vs2 = _mm_set1_ps(vs[2]);
                    __m128i xi = _mm_setr_epi32(x, x + 1, x + 2, x + 3);
                    for (; x + 4 <= xe; x += 4) {
                        __m128 dxv = _mm_sub_ps(_mm_cvtepi32_ps(xi), vpx);
                        __m128 dist = _mm_sqrt_ps(
                            _mm_add_ps(_mm_mul_ps(dxv, dxv), vdy2));
                        __m128 h = _mm_mul_ps(_mm_set1_ps(0.5f), dist);
                        __m128 f = _mm_sub_ps(
                            spline_erf4(_mm_mul_ps(_mm_add_ps(h, vk), vinv)),
                            spline_erf4(_mm_mul_ps(_mm_sub_ps(h, vk), vinv)));
                        __m128 ffv = _mm_mul_ps(f, f);
                        _mm_storeu_ps(rows[0] + x,
                            _mm_add_ps(_mm_loadu_ps(rows[0] + x),
                                       _mm_mul_ps(vs0, ffv)));
                        _mm_storeu_ps(rows[1] + x,
                            _mm_add_ps(_mm_loadu_ps(rows[1] + x),
                                       _mm_mul_ps(vs1, ffv)));
                        _mm_storeu_ps(rows[2] + x,
                            _mm_add_ps(_mm_loadu_ps(rows[2] + x),
                                       _mm_mul_ps(vs2, ffv)));
                        xi = _mm_add_epi32(xi, _mm_set1_epi32(4));
                    }
                }
#endif
                for (; x < xe; x++) {
                    float dx = (float)x - px;
                    float distance = sqrtf(dx * dx + dy2);
                    float half = 0.5f * distance;
                    float factor = spline_erf((half + 0.35355338f) * inv_sigma) -
                                   spline_erf((half - 0.35355338f) * inv_sigma);
                    float ff = factor * factor;
                    for (c = 0; c < 3; c++) {
                        rows[c][x] += vs[c] * ff;
                    }
                }
            }
        }
    }
    rc = 0;

done:
    jxl_free(ctx, up);
    jxl_free(ctx, arc_pt);
    jxl_free(ctx, arc_len);
    return rc;
}

#define NOISE_LANES 8

typedef struct {
    uint64_t s0[NOISE_LANES];
    uint64_t s1[NOISE_LANES];
} jxl_xorshift;

static uint64_t split_mix_64(uint64_t z) {
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

static void xorshift_init(jxl_xorshift *r, uint64_t seed0, uint64_t seed1) {
    int i;
    r->s0[0] = split_mix_64(seed0 + 0x9E3779B97F4A7C15ull);
    r->s1[0] = split_mix_64(seed1 + 0x9E3779B97F4A7C15ull);
    for (i = 1; i < NOISE_LANES; i++) {
        r->s0[i] = split_mix_64(r->s0[i - 1]);
        r->s1[i] = split_mix_64(r->s1[i - 1]);
    }
}

static void xorshift_batch(jxl_xorshift *r, uint32_t out[NOISE_LANES * 2]) {
    int i;
    for (i = 0; i < NOISE_LANES; i++) {
        uint64_t s1 = r->s0[i];
        uint64_t s0 = r->s1[i];
        uint64_t ret = s1 + s0;
        r->s0[i] = s0;
        s1 ^= s1 << 23;
        r->s1[i] = s1 ^ (s0 ^ (s1 >> 18) ^ (s0 >> 5));
        out[i * 2] = (uint32_t)ret;
        out[i * 2 + 1] = (uint32_t)(ret >> 32);
    }
}

static float bits_to_unit(uint32_t x) {
    uint32_t b = (x >> 9) | 0x3f800000u;
    float f;
    memcpy(&f, &b, sizeof(f));
    return f;
}

int jxl_noise_params_read(jxl_br *br, jxl_noise_params *np) {
    int i;
    for (i = 0; i < 8; i++) {
        np->lut[i] = (float)jxl_br_read(br, 10) / 1024.0f;
    }
    return br->err ? -1 : 0;
}

static uint32_t noise_mirror(int64_t v, uint32_t len) {
    for (;;) {
        if (v < 0) v = -(v + 1);
        else if ((uint64_t)v >= len) v = (int64_t)len * 2 - 1 - v;
        else return (uint32_t)v;
    }
}

#if defined(__SSE2__) || defined(_M_X64) || defined(_M_AMD64) || \
    (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
#define JXL_NOISE_SSE2 1
#include <immintrin.h>
#endif

#ifdef JXL_NOISE_SSE2

JXL_TARGET_AVX2
static void xorshift_batch_x8(jxl_xorshift *r, float out[NOISE_LANES * 2]) {
    const __m256i exp = _mm256_set1_epi32(0x3f800000u);
    int i;
    for (i = 0; i < NOISE_LANES; i += 4) {
        __m256i s1 = _mm256_loadu_si256((const __m256i *)(r->s0 + i));
        __m256i s0 = _mm256_loadu_si256((const __m256i *)(r->s1 + i));
        __m256i ret = _mm256_add_epi64(s1, s0);
        __m256i next;
        _mm256_storeu_si256((__m256i *)(r->s0 + i), s0);
        s1 = _mm256_xor_si256(s1, _mm256_slli_epi64(s1, 23));
        next = _mm256_xor_si256(
            s1, _mm256_xor_si256(
                s0, _mm256_xor_si256(_mm256_srli_epi64(s1, 18),
                                     _mm256_srli_epi64(s0, 5))));
        _mm256_storeu_si256((__m256i *)(r->s1 + i), next);
        ret = _mm256_or_si256(_mm256_srli_epi32(ret, 9), exp);
        _mm256_storeu_ps(out + i * 2, _mm256_castsi256_ps(ret));
    }
    _mm256_zeroupper();
}
#endif

static void noise_random_row(float *dst, uint32_t width, uint32_t y,
                             uint32_t group_dim, uint32_t groups_per_row,
                             jxl_xorshift *rngs, int use_avx2) {
    uint32_t gx;
    uint32_t gy = y / group_dim;
    for (gx = 0; gx < groups_per_row; gx++) {
        uint32_t x0 = gx * group_dim;
        uint32_t gw = JXL_MIN(group_dim, width - x0);
        uint32_t blocks = (gw + NOISE_LANES * 2 - 1) /
                          (NOISE_LANES * 2);
        jxl_xorshift *rng = rngs + (size_t)gy * groups_per_row + gx;
        uint32_t b;
        for (b = 0; b < blocks; b++) {
            uint32_t bx = b * NOISE_LANES * 2;
            uint32_t valid = JXL_MIN(NOISE_LANES * 2, gw - bx);
            float *out = dst + x0 + bx;
            uint32_t k;
#ifdef JXL_NOISE_SSE2
            if (use_avx2) {
                if (valid == NOISE_LANES * 2) {
                    xorshift_batch_x8(rng, out);
                } else {
                    float values[NOISE_LANES * 2];
                    xorshift_batch_x8(rng, values);
                    memcpy(out, values, valid * sizeof(float));
                }
                continue;
            }
#else
            (void)use_avx2;
#endif
            {
                uint32_t bits[NOISE_LANES * 2];
                xorshift_batch(rng, bits);
                for (k = 0; k < valid; k++) out[k] = bits_to_unit(bits[k]);
            }
        }
    }
}

static void noise_hbox5(const float *src, float *dst, uint32_t w) {
    uint32_t x, lo, hi;
    lo = w < 2 ? w : 2;
    hi = w < 2 ? w : w - 2;
    if (hi < lo) hi = lo;
    for (x = 0; x < lo; x++) {
        dst[x] = src[noise_mirror((int64_t)x - 2, w)] +
                 src[noise_mirror((int64_t)x - 1, w)] +
                 src[noise_mirror((int64_t)x, w)] +
                 src[noise_mirror((int64_t)x + 1, w)] +
                 src[noise_mirror((int64_t)x + 2, w)];
    }
    x = lo;
#ifdef JXL_NOISE_SSE2
    for (; x + 4 <= hi; x += 4) {
        __m128 s = _mm_loadu_ps(src + x - 2);
        s = _mm_add_ps(s, _mm_loadu_ps(src + x - 1));
        s = _mm_add_ps(s, _mm_loadu_ps(src + x));
        s = _mm_add_ps(s, _mm_loadu_ps(src + x + 1));
        s = _mm_add_ps(s, _mm_loadu_ps(src + x + 2));
        _mm_storeu_ps(dst + x, s);
    }
#endif
    for (; x < hi; x++) {
        dst[x] = src[x - 2] + src[x - 1] + src[x] + src[x + 1] + src[x + 2];
    }
    for (x = hi; x < w; x++) {
        dst[x] = src[noise_mirror((int64_t)x - 2, w)] +
                 src[noise_mirror((int64_t)x - 1, w)] +
                 src[noise_mirror((int64_t)x, w)] +
                 src[noise_mirror((int64_t)x + 1, w)] +
                 src[noise_mirror((int64_t)x + 2, w)];
    }
}

#ifdef JXL_NOISE_SSE2
JXL_TARGET_AVX2
static void noise_hbox5_avx2(const float *src, float *dst, uint32_t w) {
    uint32_t x, lo, hi;
    lo = w < 2 ? w : 2;
    hi = w < 2 ? w : w - 2;
    if (hi < lo) hi = lo;
    for (x = 0; x < lo; x++) {
        dst[x] = src[noise_mirror((int64_t)x - 2, w)] +
                 src[noise_mirror((int64_t)x - 1, w)] +
                 src[noise_mirror((int64_t)x, w)] +
                 src[noise_mirror((int64_t)x + 1, w)] +
                 src[noise_mirror((int64_t)x + 2, w)];
    }
    x = lo;
    for (; x + 8 <= hi; x += 8) {
        __m256 s = _mm256_loadu_ps(src + x - 2);
        s = _mm256_add_ps(s, _mm256_loadu_ps(src + x - 1));
        s = _mm256_add_ps(s, _mm256_loadu_ps(src + x));
        s = _mm256_add_ps(s, _mm256_loadu_ps(src + x + 1));
        s = _mm256_add_ps(s, _mm256_loadu_ps(src + x + 2));
        _mm256_storeu_ps(dst + x, s);
    }
    for (; x < hi; x++) {
        dst[x] = src[x - 2] + src[x - 1] + src[x] + src[x + 1] + src[x + 2];
    }
    for (x = hi; x < w; x++) {
        dst[x] = src[noise_mirror((int64_t)x - 2, w)] +
                 src[noise_mirror((int64_t)x - 1, w)] +
                 src[noise_mirror((int64_t)x, w)] +
                 src[noise_mirror((int64_t)x + 1, w)] +
                 src[noise_mirror((int64_t)x + 2, w)];
    }
    _mm256_zeroupper();
}
#endif

static void noise_vbox5(const float *const rows[5], const float *centre,
                        float *dst, uint32_t w) {
    uint32_t x = 0;
#ifdef JXL_NOISE_SSE2
    const __m128 k = _mm_set1_ps(0.16f), m4 = _mm_set1_ps(4.0f);
    for (; x + 4 <= w; x += 4) {
        __m128 s = _mm_loadu_ps(rows[0] + x);
        s = _mm_add_ps(s, _mm_loadu_ps(rows[1] + x));
        s = _mm_add_ps(s, _mm_loadu_ps(rows[2] + x));
        s = _mm_add_ps(s, _mm_loadu_ps(rows[3] + x));
        s = _mm_add_ps(s, _mm_loadu_ps(rows[4] + x));
        s = _mm_sub_ps(_mm_mul_ps(s, k),
                       _mm_mul_ps(_mm_loadu_ps(centre + x), m4));
        _mm_storeu_ps(dst + x, s);
    }
#endif
    for (; x < w; x++) {
        float s = rows[0][x] + rows[1][x] + rows[2][x] + rows[3][x] + rows[4][x];
        dst[x] = s * 0.16f - centre[x] * 4.0f;
    }
}

#ifdef JXL_NOISE_SSE2
JXL_TARGET_AVX2
static void noise_vbox5_avx2(const float *const rows[5], const float *centre,
                             float *dst, uint32_t w) {
    const __m256 k = _mm256_set1_ps(0.16f);
    const __m256 m4 = _mm256_set1_ps(4.0f);
    uint32_t x = 0;
    for (; x + 8 <= w; x += 8) {
        __m256 s = _mm256_loadu_ps(rows[0] + x);
        s = _mm256_add_ps(s, _mm256_loadu_ps(rows[1] + x));
        s = _mm256_add_ps(s, _mm256_loadu_ps(rows[2] + x));
        s = _mm256_add_ps(s, _mm256_loadu_ps(rows[3] + x));
        s = _mm256_add_ps(s, _mm256_loadu_ps(rows[4] + x));
        s = _mm256_sub_ps(_mm256_mul_ps(s, k),
                          _mm256_mul_ps(_mm256_loadu_ps(centre + x), m4));
        _mm256_storeu_ps(dst + x, s);
    }
    for (; x < w; x++) {
        float s = rows[0][x] + rows[1][x] + rows[2][x] + rows[3][x] + rows[4][x];
        dst[x] = s * 0.16f - centre[x] * 4.0f;
    }
    _mm256_zeroupper();
}
#endif

#ifdef JXL_NOISE_SSE2
JXL_TARGET_AVX2
static JXL_INLINE_HINT __m256 noise_strength_x8(__m256 v, __m256 lut) {
    const __m256 zero = _mm256_setzero_ps();
    const __m256 seven = _mm256_set1_ps(7.0f);
    const __m256i seven_i = _mm256_set1_epi32(7);
    __m256i idx;
    __m256 frac, lo, hi;

    v = _mm256_blendv_ps(v, zero,
                         _mm256_cmp_ps(v, zero, _CMP_LT_OQ));

    idx = _mm256_cvttps_epi32(_mm256_min_ps(v, seven));
    frac = _mm256_sub_ps(v, _mm256_cvtepi32_ps(idx));
    lo = _mm256_permutevar8x32_ps(lut, idx);
    hi = _mm256_permutevar8x32_ps(
        lut, _mm256_min_epi32(_mm256_add_epi32(idx, _mm256_set1_epi32(1)),
                              seven_i));
    return _mm256_add_ps(_mm256_mul_ps(_mm256_sub_ps(hi, lo), frac), lo);
}

JXL_TARGET_AVX2
static void noise_apply_x8(float *rx, float *ry, float *rb,
                           const float *nxr, const float *nyr,
                           const float *nbr, const float *lut,
                           float corr_x, float corr_b, uint32_t n,
                           uint32_t *idx) {
    const __m256 three = _mm256_set1_ps(3.0f);
    const __m256 k22 = _mm256_set1_ps(0.22f);
    const __m256 k1 = _mm256_set1_ps(0.0078125f);
    const __m256 k127 = _mm256_set1_ps(0.9921875f);
    const __m256 kx = _mm256_set1_ps(corr_x);
    const __m256 kb = _mm256_set1_ps(corr_b);
    const __m256 lutv = _mm256_loadu_ps(lut);
    uint32_t x = *idx;
    for (; x + 8 <= n; x += 8) {
        __m256 vx = _mm256_loadu_ps(rx + x);
        __m256 vy = _mm256_loadu_ps(ry + x);
        __m256 sx = noise_strength_x8(
            _mm256_mul_ps(_mm256_add_ps(vx, vy), three), lutv);
        __m256 sy = noise_strength_x8(
            _mm256_mul_ps(_mm256_sub_ps(vy, vx), three), lutv);
        __m256 nxmix = _mm256_add_ps(
            _mm256_mul_ps(k1, _mm256_loadu_ps(nxr + x)),
            _mm256_mul_ps(k127, _mm256_loadu_ps(nbr + x)));
        __m256 nymix = _mm256_add_ps(
            _mm256_mul_ps(k1, _mm256_loadu_ps(nyr + x)),
            _mm256_mul_ps(k127, _mm256_loadu_ps(nbr + x)));
        __m256 nx = _mm256_mul_ps(_mm256_mul_ps(k22, sx), nxmix);
        __m256 ny = _mm256_mul_ps(_mm256_mul_ps(k22, sy), nymix);
        __m256 sum = _mm256_add_ps(nx, ny);
        __m256 dx = _mm256_sub_ps(
            _mm256_add_ps(_mm256_mul_ps(kx, sum), nx), ny);
        _mm256_storeu_ps(rx + x, _mm256_add_ps(vx, dx));
        _mm256_storeu_ps(ry + x, _mm256_add_ps(vy, sum));
        _mm256_storeu_ps(rb + x, _mm256_add_ps(
            _mm256_loadu_ps(rb + x), _mm256_mul_ps(kb, sum)));
    }
    _mm256_zeroupper();
    *idx = x;
}
#endif

int jxl_render_noise(jxl_ctx *ctx, jxl_fimage *img, const jxl_noise_params *np,
                     const jxl_frame_header *fh, uint32_t visible_frames,
                     uint32_t invisible_frames, float corr_x, float corr_b) {
    uint32_t width = fh->width, height = fh->height;
    uint32_t group_dim = jxl_frame_group_dim(fh);
    uint32_t groups_per_row, group_rows, gx, gy;
    float *raw[3] = {NULL, NULL, NULL};
    float *hsum = NULL;
    jxl_xorshift *corr_rng = NULL;
    uint32_t hsum_row[5];
    uint64_t seed0 = ((uint64_t)visible_frames << 32) + invisible_frames;
    float lut[9];
    uint32_t x, y;
    int c, rc = -1;
    int use_avx2 = 0;
#ifdef JXL_NOISE_SSE2
    use_avx2 = jxl_has_avx2();
#endif

    if (img->ncolor < 3 || width == 0 || height == 0) return 0;
    for (c = 0; c < 8; c++) lut[c] = np->lut[c];
    lut[8] = np->lut[7];

    groups_per_row = (width + group_dim - 1) / group_dim;
    group_rows = (height + group_dim - 1) / group_dim;

    {
        size_t n;
        if (!jxl_size_mul((size_t)width * height, sizeof(float), &n)) goto done;
        for (c = 0; c < 2; c++) {
            raw[c] = (float *)jxl_malloc(ctx, n);
            if (!raw[c]) goto done;
        }

        if (!jxl_size_mul((size_t)width * 10, sizeof(float), &n)) goto done;
        hsum = (float *)jxl_malloc(ctx, n);
        if (!hsum) goto done;
        if (!jxl_size_mul((size_t)groups_per_row * group_rows,
                          sizeof(*corr_rng), &n)) goto done;
        corr_rng = (jxl_xorshift *)jxl_malloc(ctx, n);
        if (!corr_rng) goto done;
    }

    for (gy = 0; gy < group_rows; gy++) {
        for (gx = 0; gx < groups_per_row; gx++) {
            uint32_t x0 = gx * group_dim, y0 = gy * group_dim;
            uint32_t gw = JXL_MIN(group_dim, width - x0);
            uint32_t gh = JXL_MIN(group_dim, height - y0);
            uint32_t blocks = (gw + NOISE_LANES * 2 - 1) / (NOISE_LANES * 2);
            jxl_xorshift rng;
            xorshift_init(&rng, seed0, ((uint64_t)x0 << 32) + y0);

            for (c = 0; c < 2; c++) {
                uint32_t row, b;
                for (row = 0; row < gh; row++) {
                    for (b = 0; b < blocks; b++) {
                        uint32_t bx = b * NOISE_LANES * 2;
                        uint32_t valid = JXL_MIN(NOISE_LANES * 2, gw - bx);
                        float *dst = raw[c] +
                            (size_t)(y0 + row) * width + x0 + bx;
                        uint32_t k;
#ifdef JXL_NOISE_SSE2
                        if (use_avx2) {
                            if (valid == NOISE_LANES * 2) {
                                xorshift_batch_x8(&rng, dst);
                            } else {
                                float values[NOISE_LANES * 2];
                                xorshift_batch_x8(&rng, values);
                                memcpy(dst, values, valid * sizeof(float));
                            }
                            continue;
                        }
#endif
                        {
                            uint32_t bits[NOISE_LANES * 2];
                            xorshift_batch(&rng, bits);
                            for (k = 0; k < valid; k++)
                                dst[k] = bits_to_unit(bits[k]);
                        }
                    }
                }
            }
            corr_rng[(size_t)gy * groups_per_row + gx] = rng;
        }
    }

    for (c = 0; c < 2; c++) {
        uint32_t slot;
        for (slot = 0; slot < 5; slot++) hsum_row[slot] = (uint32_t)-1;
        for (y = 0; y < height; y++) {
            const float *rows[5];
            int dy;

            for (dy = -2; dy <= 2; dy++) {
                uint32_t r = noise_mirror((int64_t)y + dy, height);
                uint32_t sl = r % 5;
                if (hsum_row[sl] != r) {
#ifdef JXL_NOISE_SSE2
                    if (use_avx2)
                        noise_hbox5_avx2(raw[c] + (size_t)r * width,
                                         hsum + (size_t)sl * width, width);
                    else
#endif
                        noise_hbox5(raw[c] + (size_t)r * width,
                                    hsum + (size_t)sl * width, width);
                    hsum_row[sl] = r;
                }
                rows[dy + 2] = hsum + (size_t)sl * width;
            }

#ifdef JXL_NOISE_SSE2
            if (use_avx2)
                noise_vbox5_avx2(rows, raw[c] + (size_t)y * width,
                                 raw[c] + (size_t)y * width, width);
            else
#endif
                noise_vbox5(rows, raw[c] + (size_t)y * width,
                            raw[c] + (size_t)y * width, width);
        }
    }

    {
        float *corr_raw = hsum + (size_t)width * 5;
        uint32_t next_corr_row = 0;
        uint32_t slot;
        for (slot = 0; slot < 5; slot++) hsum_row[slot] = (uint32_t)-1;
        for (y = 0; y < height && y < img->plane[0].h; y++) {
            const float *rows[5];
            uint32_t last_needed = JXL_MIN(y + 2, height - 1);
            float *rx, *ry, *rb, *nbr;
            const float *nxr, *nyr;
            uint32_t n;
            int dy;
            while (next_corr_row <= last_needed) {
                uint32_t sl = next_corr_row % 5;
                float *src = corr_raw + (size_t)sl * width;
                noise_random_row(src, width, next_corr_row, group_dim,
                                 groups_per_row, corr_rng, use_avx2);
#ifdef JXL_NOISE_SSE2
                if (use_avx2)
                    noise_hbox5_avx2(src, hsum + (size_t)sl * width,
                                     width);
                else
#endif
                    noise_hbox5(src, hsum + (size_t)sl * width, width);
                hsum_row[sl] = next_corr_row++;
            }
            for (dy = -2; dy <= 2; dy++) {
                uint32_t r = noise_mirror((int64_t)y + dy, height);
                rows[dy + 2] = hsum + (size_t)(r % 5) * width;
            }
            nbr = corr_raw + (size_t)(y % 5) * width;
#ifdef JXL_NOISE_SSE2
            if (use_avx2)
                noise_vbox5_avx2(rows, nbr, nbr, width);
            else
#endif
                noise_vbox5(rows, nbr, nbr, width);

            rx = img->plane[0].data + (size_t)y * img->plane[0].stride;
            ry = img->plane[1].data + (size_t)y * img->plane[1].stride;
            rb = img->plane[2].data + (size_t)y * img->plane[2].stride;
            nxr = raw[0] + (size_t)y * width;
            nyr = raw[1] + (size_t)y * width;
            n = JXL_MIN(width, img->plane[0].w);
            x = 0;
#ifdef JXL_NOISE_SSE2
            if (use_avx2)
                noise_apply_x8(rx, ry, rb, nxr, nyr, nbr, lut,
                               corr_x, corr_b, n, &x);
#endif
            for (; x < n; x++) {
                float gx_ = rx[x], gy_ = ry[x];
                float in_x = gx_ + gy_;
                float in_y = gy_ - gx_;
                float sx_ = in_x * 3.0f, sy_ = in_y * 3.0f;
                uint32_t ix, iy;
                float fx, fy, sxv, syv, nx, ny;
                if (sx_ < 0.0f) sx_ = 0.0f;
                if (sy_ < 0.0f) sy_ = 0.0f;
                ix = (uint32_t)sx_;
                iy = (uint32_t)sy_;
                if (ix > 7) ix = 7;
                if (iy > 7) iy = 7;
                fx = sx_ - (float)ix;
                fy = sy_ - (float)iy;
                sxv = (lut[ix + 1] - lut[ix]) * fx + lut[ix];
                syv = (lut[iy + 1] - lut[iy]) * fy + lut[iy];
                nx = 0.22f * sxv *
                     (0.0078125f * nxr[x] + 0.9921875f * nbr[x]);
                ny = 0.22f * syv *
                     (0.0078125f * nyr[x] + 0.9921875f * nbr[x]);
                rx[x] += corr_x * (nx + ny) + nx - ny;
                ry[x] += nx + ny;
                rb[x] += corr_b * (nx + ny);
            }
        }
    }
    rc = 0;

done:
    jxl_free(ctx, corr_rng);
    jxl_free(ctx, hsum);
    for (c = 0; c < 3; c++) {
        jxl_free(ctx, raw[c]);
    }
    return rc;
}

static uint32_t up_mirror(int64_t v, uint32_t len) {
    for (;;) {
        if (v < 0) v = -(v + 1);
        else if ((uint64_t)v >= len) v = (int64_t)len * 2 - 1 - v;
        else return (uint32_t)v;
    }
}

static void build_kernel(float *kernel, uint32_t shift, const float *w) {
    uint32_t N = 1u << shift, H = N / 2;
    uint32_t ky, kx, py, px;
    for (ky = 0; ky < H; ky++) {
        for (kx = 0; kx < H; kx++) {
            size_t o0 = ((size_t)ky * N + kx) * 25;
            size_t o1 = ((size_t)ky * N + (N - 1 - kx)) * 25;
            size_t o2 = ((size_t)(N - 1 - ky) * N + kx) * 25;
            size_t o3 = ((size_t)(N - 1 - ky) * N + (N - 1 - kx)) * 25;
            for (py = 0; py < 5; py++) {
                for (px = 0; px < 5; px++) {
                    uint32_t j = 5 * ky + py;
                    uint32_t i = 5 * kx + px;
                    uint32_t my = i < j ? i : j;
                    uint32_t mx = i < j ? j : i;
                    size_t idx = (size_t)5 * H * my - (size_t)my * (my - 1) / 2
                                 + mx - my;
                    float v = w[idx];
                    kernel[o0 + (size_t)py * 5 + px] = v;
                    kernel[o1 + (size_t)py * 5 + (4 - px)] = v;
                    kernel[o2 + (size_t)(4 - py) * 5 + px] = v;
                    kernel[o3 + (size_t)(4 - py) * 5 + (4 - px)] = v;
                }
            }
        }
    }
}

#if !defined(JXL_UPSAMPLE_FORCE_SCALAR) && \
    (defined(__SSE2__) || defined(_M_X64) || defined(_M_AMD64) || \
     (defined(_M_IX86_FP) && _M_IX86_FP >= 2))
#define JXL_UPSAMPLE_SSE2 1
#include <emmintrin.h>
#include <immintrin.h>
#endif

static void up_minmax25(const float *a, float *out_lo, float *out_hi) {
    float lo = a[0], hi = a[0];
    int n;
    for (n = 1; n < 25; n++) {
        if (a[n] < lo) lo = a[n];
        if (a[n] > hi) hi = a[n];
    }
    *out_lo = lo;
    *out_hi = hi;
}

static float up_dot25(const float *a, const float *b) {
    float sum = 0.0f;
    int n;
    for (n = 0; n < 25; n++) sum += a[n] * b[n];
    return sum;
}

#ifdef JXL_UPSAMPLE_SSE2

static void up_block4(const float *const *srow, uint32_t x, uint32_t N,
                      const float *kernel, float *dst_rows[8], uint32_t ny) {
    __m128 vlo, vhi;
    uint32_t oy, ox;
    int py, px;

    {
        __m128 mn = _mm_loadu_ps(srow[0] + x - 2), mx = mn;
        for (py = 0; py < 5; py++) {
            const float *r = srow[py] + x - 2;
            for (px = (py == 0) ? 1 : 0; px < 5; px++) {
                __m128 v = _mm_loadu_ps(r + px);
                mn = _mm_min_ps(mn, v);
                mx = _mm_max_ps(mx, v);
            }
        }
        vlo = mn;
        vhi = mx;
    }

    for (oy = 0; oy < ny; oy++) {
        float *drow = dst_rows[oy];
        __m128 out[8];
        for (ox = 0; ox < N; ox++) {
            const float *k = kernel + ((size_t)oy * N + ox) * 25;
            __m128 acc = _mm_setzero_ps();
            __m128 m;
            int t = 0;
            for (py = 0; py < 5; py++) {
                const float *r = srow[py] + x - 2;
                for (px = 0; px < 5; px++, t++) {
                    acc = _mm_add_ps(acc, _mm_mul_ps(_mm_loadu_ps(r + px),
                                                     _mm_set1_ps(k[t])));
                }
            }

            m = _mm_cmplt_ps(acc, vlo);
            acc = _mm_or_ps(_mm_and_ps(m, vlo), _mm_andnot_ps(m, acc));
            m = _mm_cmpgt_ps(acc, vhi);
            acc = _mm_or_ps(_mm_and_ps(m, vhi), _mm_andnot_ps(m, acc));
            out[ox] = acc;
        }

        if (N == 2) {
            _mm_storeu_ps(drow + x * 2, _mm_unpacklo_ps(out[0], out[1]));
            _mm_storeu_ps(drow + x * 2 + 4, _mm_unpackhi_ps(out[0], out[1]));
        } else if (N == 4) {
            _MM_TRANSPOSE4_PS(out[0], out[1], out[2], out[3]);
            _mm_storeu_ps(drow + x * 4, out[0]);
            _mm_storeu_ps(drow + x * 4 + 4, out[1]);
            _mm_storeu_ps(drow + x * 4 + 8, out[2]);
            _mm_storeu_ps(drow + x * 4 + 12, out[3]);
        } else {
            for (ox = 0; ox < N; ox++) {
                float tmp[4];
                _mm_storeu_ps(tmp, out[ox]);
                drow[(size_t)(x + 0) * N + ox] = tmp[0];
                drow[(size_t)(x + 1) * N + ox] = tmp[1];
                drow[(size_t)(x + 2) * N + ox] = tmp[2];
                drow[(size_t)(x + 3) * N + ox] = tmp[3];
            }
        }
    }
}

JXL_TARGET_AVX2
static void up_block8(const float *const *srow, uint32_t x, uint32_t N,
                      const float *kernel, float *dst_rows[8], uint32_t ny) {
    __m256 vlo = _mm256_setzero_ps(), vhi = _mm256_setzero_ps();
    uint32_t oy, ox;
    int py, px;

    if (N == 2 && ny == 2) {
        const float *k0 = kernel;
        const float *k1 = kernel + 25;
        const float *k2 = kernel + 50;
        const float *k3 = kernel + 75;
        __m256 a0 = _mm256_setzero_ps();
        __m256 a1 = _mm256_setzero_ps();
        __m256 a2 = _mm256_setzero_ps();
        __m256 a3 = _mm256_setzero_ps();
        __m256 mn = _mm256_setzero_ps(), mx = _mm256_setzero_ps();
        __m256 m;
        int t = 0;

        for (py = 0; py < 5; py++) {
            const float *r = srow[py] + x - 2;
            for (px = 0; px < 5; px++, t++) {
                __m256 v = _mm256_loadu_ps(r + px);
                if (t == 0) {
                    mn = v;
                    mx = v;
                } else {
                    mn = _mm256_min_ps(mn, v);
                    mx = _mm256_max_ps(mx, v);
                }
                a0 = _mm256_add_ps(a0,
                    _mm256_mul_ps(v, _mm256_set1_ps(k0[t])));
                a1 = _mm256_add_ps(a1,
                    _mm256_mul_ps(v, _mm256_set1_ps(k1[t])));
                a2 = _mm256_add_ps(a2,
                    _mm256_mul_ps(v, _mm256_set1_ps(k2[t])));
                a3 = _mm256_add_ps(a3,
                    _mm256_mul_ps(v, _mm256_set1_ps(k3[t])));
            }
        }

#define JXL_UP_CLAMP(v)                                                     \
        m = _mm256_cmp_ps((v), mn, _CMP_LT_OQ);                            \
        (v) = _mm256_or_ps(_mm256_and_ps(m, mn),                           \
                           _mm256_andnot_ps(m, (v)));                       \
        m = _mm256_cmp_ps((v), mx, _CMP_GT_OQ);                            \
        (v) = _mm256_or_ps(_mm256_and_ps(m, mx),                           \
                           _mm256_andnot_ps(m, (v)))
        JXL_UP_CLAMP(a0);
        JXL_UP_CLAMP(a1);
        JXL_UP_CLAMP(a2);
        JXL_UP_CLAMP(a3);
#undef JXL_UP_CLAMP

        {
            __m128 a0lo = _mm256_castps256_ps128(a0);
            __m128 a1lo = _mm256_castps256_ps128(a1);
            __m128 a0hi = _mm256_extractf128_ps(a0, 1);
            __m128 a1hi = _mm256_extractf128_ps(a1, 1);
            __m128 a2lo = _mm256_castps256_ps128(a2);
            __m128 a3lo = _mm256_castps256_ps128(a3);
            __m128 a2hi = _mm256_extractf128_ps(a2, 1);
            __m128 a3hi = _mm256_extractf128_ps(a3, 1);
            float *d0 = dst_rows[0];
            float *d1 = dst_rows[1];
            _mm_storeu_ps(d0 + x * 2, _mm_unpacklo_ps(a0lo, a1lo));
            _mm_storeu_ps(d0 + x * 2 + 4, _mm_unpackhi_ps(a0lo, a1lo));
            _mm_storeu_ps(d0 + (x + 4) * 2, _mm_unpacklo_ps(a0hi, a1hi));
            _mm_storeu_ps(d0 + (x + 4) * 2 + 4,
                          _mm_unpackhi_ps(a0hi, a1hi));
            _mm_storeu_ps(d1 + x * 2, _mm_unpacklo_ps(a2lo, a3lo));
            _mm_storeu_ps(d1 + x * 2 + 4, _mm_unpackhi_ps(a2lo, a3lo));
            _mm_storeu_ps(d1 + (x + 4) * 2, _mm_unpacklo_ps(a2hi, a3hi));
            _mm_storeu_ps(d1 + (x + 4) * 2 + 4,
                          _mm_unpackhi_ps(a2hi, a3hi));
        }
        _mm256_zeroupper();
        return;
    }

    if (!(N == 4 && ny == 4)) {
        __m256 mn = _mm256_loadu_ps(srow[0] + x - 2), mx = mn;
        for (py = 0; py < 5; py++) {
            const float *r = srow[py] + x - 2;
            for (px = (py == 0) ? 1 : 0; px < 5; px++) {
                __m256 v = _mm256_loadu_ps(r + px);
                mn = _mm256_min_ps(mn, v);
                mx = _mm256_max_ps(mx, v);
            }
        }
        vlo = mn;
        vhi = mx;
    }

    if (N == 4 && ny == 4) {
        __m256 mn = _mm256_setzero_ps(), mx = _mm256_setzero_ps();
        for (oy = 0; oy < 4; oy++) {
            const float *k0 = kernel + ((size_t)oy * 4 + 0) * 25;
            const float *k1 = k0 + 25;
            const float *k2 = k1 + 25;
            const float *k3 = k2 + 25;
            __m256 a0 = _mm256_setzero_ps();
            __m256 a1 = _mm256_setzero_ps();
            __m256 a2 = _mm256_setzero_ps();
            __m256 a3 = _mm256_setzero_ps();
            __m256 m;
            int t = 0;

            for (py = 0; py < 5; py++) {
                const float *r = srow[py] + x - 2;
                for (px = 0; px < 5; px++, t++) {
                    __m256 v = _mm256_loadu_ps(r + px);
                    if (oy == 0) {
                        if (t == 0) {
                            mn = v;
                            mx = v;
                        } else {
                            mn = _mm256_min_ps(mn, v);
                            mx = _mm256_max_ps(mx, v);
                        }
                    }
                    a0 = _mm256_add_ps(a0,
                        _mm256_mul_ps(v, _mm256_set1_ps(k0[t])));
                    a1 = _mm256_add_ps(a1,
                        _mm256_mul_ps(v, _mm256_set1_ps(k1[t])));
                    a2 = _mm256_add_ps(a2,
                        _mm256_mul_ps(v, _mm256_set1_ps(k2[t])));
                    a3 = _mm256_add_ps(a3,
                        _mm256_mul_ps(v, _mm256_set1_ps(k3[t])));
                }
            }
            if (oy == 0) {
                vlo = mn;
                vhi = mx;
            }

#define JXL_UP_CLAMP(v)                                                     \
            m = _mm256_cmp_ps((v), vlo, _CMP_LT_OQ);                       \
            (v) = _mm256_or_ps(_mm256_and_ps(m, vlo),                      \
                               _mm256_andnot_ps(m, (v)));                   \
            m = _mm256_cmp_ps((v), vhi, _CMP_GT_OQ);                       \
            (v) = _mm256_or_ps(_mm256_and_ps(m, vhi),                      \
                               _mm256_andnot_ps(m, (v)))
            JXL_UP_CLAMP(a0);
            JXL_UP_CLAMP(a1);
            JXL_UP_CLAMP(a2);
            JXL_UP_CLAMP(a3);
#undef JXL_UP_CLAMP

            {
                __m128 lo0 = _mm256_castps256_ps128(a0);
                __m128 lo1 = _mm256_castps256_ps128(a1);
                __m128 lo2 = _mm256_castps256_ps128(a2);
                __m128 lo3 = _mm256_castps256_ps128(a3);
                __m128 hi0 = _mm256_extractf128_ps(a0, 1);
                __m128 hi1 = _mm256_extractf128_ps(a1, 1);
                __m128 hi2 = _mm256_extractf128_ps(a2, 1);
                __m128 hi3 = _mm256_extractf128_ps(a3, 1);
                float *drow = dst_rows[oy];
                _MM_TRANSPOSE4_PS(lo0, lo1, lo2, lo3);
                _mm_storeu_ps(drow + x * 4, lo0);
                _mm_storeu_ps(drow + x * 4 + 4, lo1);
                _mm_storeu_ps(drow + x * 4 + 8, lo2);
                _mm_storeu_ps(drow + x * 4 + 12, lo3);
                _MM_TRANSPOSE4_PS(hi0, hi1, hi2, hi3);
                _mm_storeu_ps(drow + (x + 4) * 4, hi0);
                _mm_storeu_ps(drow + (x + 4) * 4 + 4, hi1);
                _mm_storeu_ps(drow + (x + 4) * 4 + 8, hi2);
                _mm_storeu_ps(drow + (x + 4) * 4 + 12, hi3);
            }
        }
        _mm256_zeroupper();
        return;
    }

    for (oy = 0; oy < ny; oy++) {
        float *drow = dst_rows[oy];
        __m128 lo[8], hi[8];
        for (ox = 0; ox < N; ox++) {
            const float *k = kernel + ((size_t)oy * N + ox) * 25;
            __m256 acc = _mm256_setzero_ps();
            __m256 m;
            int t = 0;
            for (py = 0; py < 5; py++) {
                const float *r = srow[py] + x - 2;
                for (px = 0; px < 5; px++, t++) {
                    acc = _mm256_add_ps(acc,
                        _mm256_mul_ps(_mm256_loadu_ps(r + px),
                                      _mm256_set1_ps(k[t])));
                }
            }
            m = _mm256_cmp_ps(acc, vlo, _CMP_LT_OQ);
            acc = _mm256_or_ps(_mm256_and_ps(m, vlo),
                               _mm256_andnot_ps(m, acc));
            m = _mm256_cmp_ps(acc, vhi, _CMP_GT_OQ);
            acc = _mm256_or_ps(_mm256_and_ps(m, vhi),
                               _mm256_andnot_ps(m, acc));
            lo[ox] = _mm256_castps256_ps128(acc);
            hi[ox] = _mm256_extractf128_ps(acc, 1);
        }
        if (N == 2) {
            _mm_storeu_ps(drow + x * 2, _mm_unpacklo_ps(lo[0], lo[1]));
            _mm_storeu_ps(drow + x * 2 + 4, _mm_unpackhi_ps(lo[0], lo[1]));
            _mm_storeu_ps(drow + (x + 4) * 2, _mm_unpacklo_ps(hi[0], hi[1]));
            _mm_storeu_ps(drow + (x + 4) * 2 + 4,
                          _mm_unpackhi_ps(hi[0], hi[1]));
        } else if (N == 4) {
            _MM_TRANSPOSE4_PS(lo[0], lo[1], lo[2], lo[3]);
            _mm_storeu_ps(drow + x * 4, lo[0]);
            _mm_storeu_ps(drow + x * 4 + 4, lo[1]);
            _mm_storeu_ps(drow + x * 4 + 8, lo[2]);
            _mm_storeu_ps(drow + x * 4 + 12, lo[3]);
            _MM_TRANSPOSE4_PS(hi[0], hi[1], hi[2], hi[3]);
            _mm_storeu_ps(drow + (x + 4) * 4, hi[0]);
            _mm_storeu_ps(drow + (x + 4) * 4 + 4, hi[1]);
            _mm_storeu_ps(drow + (x + 4) * 4 + 8, hi[2]);
            _mm_storeu_ps(drow + (x + 4) * 4 + 12, hi[3]);
        } else {
            for (ox = 0; ox < N; ox++) {
                float t0[4], t1[4];
                _mm_storeu_ps(t0, lo[ox]);
                _mm_storeu_ps(t1, hi[ox]);
                drow[(size_t)(x + 0) * N + ox] = t0[0];
                drow[(size_t)(x + 1) * N + ox] = t0[1];
                drow[(size_t)(x + 2) * N + ox] = t0[2];
                drow[(size_t)(x + 3) * N + ox] = t0[3];
                drow[(size_t)(x + 4) * N + ox] = t1[0];
                drow[(size_t)(x + 5) * N + ox] = t1[1];
                drow[(size_t)(x + 6) * N + ox] = t1[2];
                drow[(size_t)(x + 7) * N + ox] = t1[3];
            }
        }
    }
    _mm256_zeroupper();
}
#endif

static const float *weights_for(const jxl_image_metadata *meta, uint32_t shift) {
    if (shift == 1) return meta->up2;
    if (shift == 2) return meta->up4;
    return meta->up8;
}

int jxl_upsample_plane(jxl_ctx *ctx, jxl_fplane *p, uint32_t shift,
                       const jxl_image_metadata *meta, uint32_t out_w,
                       uint32_t out_h) {
    uint32_t N = 1u << shift;
    uint32_t w = p->w, h = p->h, x, y;
    float *kernel = NULL;
    jxl_fplane dst;
    int rc = -1;
#ifdef JXL_UPSAMPLE_SSE2
    const int use_avx2 = jxl_has_avx2();
#endif

    if (shift == 0 || shift > 3) return 0;
    if (w == 0 || h == 0) return 0;

    memset(&dst, 0, sizeof(dst));
    kernel = (float *)jxl_calloc(ctx, (size_t)N * N * 25, sizeof(float));
    if (!kernel) goto done;
    build_kernel(kernel, shift, weights_for(meta, shift));

    if (jxl_fplane_alloc_uninit(ctx, &dst, out_w, out_h) != 0) goto done;

    for (y = 0; y < h; y++) {

#ifdef JXL_UPSAMPLE_SSE2
        const int y_full = (uint64_t)(y + 1) * N <= out_h;
#endif

        const float *srow[5];
        int iy;
        for (iy = -2; iy <= 2; iy++) {
            srow[iy + 2] = p->data +
                (size_t)up_mirror((int64_t)y + iy, h) * p->stride;
        }
        for (x = 0; x < w; ) {
            float nb[25];
            float lo, hi;
            uint32_t oy, ox, ny, nx;
            int ix;
            int n = 0;

#ifdef JXL_UPSAMPLE_SSE2

            if (y_full) {
                float *drows[8];
                if (use_avx2 && x >= 2 && x + 10 <= w &&
                    (uint64_t)(x + 8) * N <= out_w) {
                    for (oy = 0; oy < N; oy++) {
                        drows[oy] = dst.data +
                                    (size_t)(y * N + oy) * dst.stride;
                    }
                    up_block8(srow, x, N, kernel, drows, N);
                    x += 8;
                    continue;
                }
                if (x >= 2 && x + 6 <= w && (uint64_t)(x + 4) * N <= out_w) {
                    for (oy = 0; oy < N; oy++) {
                        drows[oy] = dst.data +
                                    (size_t)(y * N + oy) * dst.stride;
                    }
                    up_block4(srow, x, N, kernel, drows, N);
                    x += 4;
                    continue;
                }
            }
#endif

            if (x >= 2 && x + 2 < w) {
                for (iy = 0; iy < 5; iy++) {
                    const float *r = srow[iy] + x - 2;
                    nb[n] = r[0]; nb[n + 1] = r[1]; nb[n + 2] = r[2];
                    nb[n + 3] = r[3]; nb[n + 4] = r[4];
                    n += 5;
                }
            } else {
                for (iy = 0; iy < 5; iy++) {
                    for (ix = -2; ix <= 2; ix++) {
                        nb[n++] = srow[iy][up_mirror((int64_t)x + ix, w)];
                    }
                }
            }
            up_minmax25(nb, &lo, &hi);

            ny = y * N < out_h ? out_h - y * N : 0;
            if (ny > N) ny = N;
            nx = x * N < out_w ? out_w - x * N : 0;
            if (nx > N) nx = N;
            for (oy = 0; oy < ny; oy++) {
                float *drow = dst.data + (size_t)(y * N + oy) * dst.stride;
                const float *krow = kernel + ((size_t)oy * N) * 25;
                uint32_t dx0 = x * N;
                for (ox = 0; ox < nx; ox++) {
                    float sum = up_dot25(nb, krow + (size_t)ox * 25);
                    if (sum < lo) sum = lo;
                    if (sum > hi) sum = hi;
                    drow[dx0 + ox] = sum;
                }
            }
            x++;
        }
    }

    jxl_free(ctx, p->data);
    *p = dst;
    memset(&dst, 0, sizeof(dst));
    rc = 0;

done:
    jxl_free(ctx, kernel);
    jxl_free(ctx, dst.data);
    return rc;
}

#include <math.h>

static uint32_t div_ceil32(uint32_t a, uint32_t b) {
    return (a + b - 1) / b;
}

int jxl_fplane_alloc(jxl_ctx *ctx, jxl_fplane *p, uint32_t w, uint32_t h) {
    size_t total;
    if (!jxl_size_mul(w, h, &total)) return -1;
    p->data = (float *)jxl_calloc(ctx, total ? total : 1, sizeof(float));
    if (!p->data) return -1;
    p->w = w;
    p->h = h;
    p->stride = w;
    return 0;
}

int jxl_fplane_alloc_uninit(jxl_ctx *ctx, jxl_fplane *p, uint32_t w, uint32_t h) {
#ifdef JXL_FPLANE_ALWAYS_ZERO
    return jxl_fplane_alloc(ctx, p, w, h);
#else
    size_t total, bytes;
    if (!jxl_size_mul(w, h, &total)) return -1;
    if (!total) total = 1;
    if (!jxl_size_mul(total, sizeof(float), &bytes)) return -1;
    p->data = (float *)jxl_malloc(ctx, bytes);
    if (!p->data) return -1;
    p->w = w;
    p->h = h;
    p->stride = w;
    return 0;
#endif
}

int jxl_fimage_alloc(jxl_ctx *ctx, jxl_fimage *img, uint32_t nplane) {
    img->ctx = ctx;
    img->plane = (jxl_fplane *)jxl_calloc(ctx, nplane ? nplane : 1,
                                          sizeof(jxl_fplane));
    if (!img->plane) return -1;
    img->nplane = nplane;
    return 0;
}

void jxl_fimage_free(jxl_ctx *ctx, jxl_fimage *img) {
    uint32_t i;
    if (!img || !img->plane) return;
    for (i = 0; i < img->nplane; i++) jxl_free(ctx, img->plane[i].data);
    jxl_free(ctx, img->plane);
    img->plane = NULL;
    img->nplane = 0;
}

static void fimage_keep_gray(jxl_ctx *ctx, jxl_fimage *img) {
    uint32_t k;
    jxl_free(ctx, img->plane[1].data);
    jxl_free(ctx, img->plane[2].data);
    for (k = 3; k < img->nplane; k++) img->plane[k - 2] = img->plane[k];
    img->nplane -= 2;
    img->ncolor = 1;
}

typedef struct {
    const uint8_t *cs;
    size_t cs_len;
    const jxl_toc *toc;
    jxl_br br;
    int single;
} jxl_sections;

static void sections_init(jxl_sections *s, const uint8_t *cs, size_t cs_len,
                          const jxl_toc *toc) {
    s->cs = cs;
    s->cs_len = cs_len;
    s->toc = toc;
    s->single = toc->count <= 1;
    jxl_br_init(&s->br, cs, cs_len);
    if (s->single) jxl_br_seek_byte(&s->br, toc->entries[0].offset);
}

static jxl_br *section_reader(jxl_sections *s, jxl_toc_kind kind,
                              uint32_t pass_idx, uint32_t group_idx) {
    uint32_t idx;
    if (s->single) return &s->br;
    idx = jxl_toc_index(s->toc, kind, pass_idx, group_idx);
    if (idx >= s->toc->count) return NULL;
    jxl_br_init(&s->br, s->cs, s->cs_len);
    jxl_br_seek_byte(&s->br, s->toc->entries[idx].offset);
    return &s->br;
}

typedef struct {
    float m_x_lf, m_y_lf, m_b_lf;
} jxl_lf_dequant;

static void lf_dequant_read(jxl_br *br, jxl_lf_dequant *d) {
    d->m_x_lf = 1.0f / 32.0f;
    d->m_y_lf = 1.0f / 4.0f;
    d->m_b_lf = 1.0f / 2.0f;
    if (jxl_br_bool(br)) return;
    d->m_x_lf = jxl_br_f16(br);
    d->m_y_lf = jxl_br_f16(br);
    d->m_b_lf = jxl_br_f16(br);
}

typedef struct {
    int32_t minshift[16];
    int32_t maxshift[16];
} jxl_pass_shifts;

static void compute_pass_shifts(const jxl_frame_header *fh,
                                jxl_pass_shifts *out) {
    uint32_t i;
    int32_t maxshift = 3;
    for (i = 0; i < 16; i++) {
        out->minshift[i] = 0;
        out->maxshift[i] = 0;
    }
    for (i = 0; i < fh->passes.num_ds && i < 8; i++) {
        uint32_t lp = fh->passes.last_pass[i];
        int32_t minshift = 0;
        uint32_t d = fh->passes.downsample[i];
        while (d > 1) { minshift++; d >>= 1; }
        if (lp < 16) {
            out->minshift[lp] = minshift;
            out->maxshift[lp] = maxshift;
        }
        maxshift = minshift;
    }
    if (fh->passes.num_passes >= 1 && fh->passes.num_passes <= 16) {
        out->minshift[fh->passes.num_passes - 1] = 0;
        out->maxshift[fh->passes.num_passes - 1] = maxshift;
    }
}

static uint32_t pass_for_shift(const jxl_frame_header *fh,
                               const jxl_pass_shifts *ps, int32_t shift) {
    uint32_t i;
    for (i = 0; i < fh->passes.num_passes && i < 16; i++) {
        if (shift >= ps->minshift[i] && shift < ps->maxshift[i]) return i;
    }
    return fh->passes.num_passes - 1;
}

typedef struct {
    jxl_chanlist *lf;
    jxl_chanlist *pass;
    uint32_t num_lf, num_groups, num_passes;
} jxl_group_lists;

static void group_lists_free(jxl_ctx *ctx, jxl_group_lists *gl) {
    uint32_t i;
    if (gl->lf) {
        for (i = 0; i < gl->num_lf; i++) jxl_chanlist_free(ctx, &gl->lf[i]);
        jxl_free(ctx, gl->lf);
    }
    if (gl->pass) {
        for (i = 0; i < gl->num_passes * gl->num_groups; i++) {
            jxl_chanlist_free(ctx, &gl->pass[i]);
        }
        jxl_free(ctx, gl->pass);
    }
    memset(gl, 0, sizeof(*gl));
}

static int split_channel_into_groups(jxl_ctx *ctx, const jxl_mchan *ch,
                                     uint32_t gw, uint32_t gh, uint32_t nx,
                                     uint32_t ny, jxl_chanlist *lists,
                                     uint32_t nlists) {
    uint32_t gx, gy;
    if ((uint64_t)nx * ny > nlists) return -1;
    for (gy = 0; gy < ny; gy++) {
        for (gx = 0; gx < nx; gx++) {
            jxl_mchan tile;
            uint32_t x0 = gx * gw, y0 = gy * gh;
            uint32_t w = 0, h = 0;
            if (x0 < ch->w) w = JXL_MIN(gw, ch->w - x0);
            if (y0 < ch->h) h = JXL_MIN(gh, ch->h - y0);
            if (w == 0 || h == 0) continue;
            tile = *ch;
            tile.w = w;
            tile.h = h;
            tile.ow = w << (ch->hshift > 0 ? ch->hshift : 0);
            tile.oh = h << (ch->vshift > 0 ? ch->vshift : 0);
            tile.data = ch->data + (size_t)y0 * ch->stride + x0;
            if (jxl_chanlist_push(ctx, &lists[gy * nx + gx], &tile) != 0)
                return -1;
        }
    }
    return 0;
}

static int decode_group_modular(jxl_ctx *ctx, jxl_br *br, jxl_chanlist *cl,
                                jxl_ma_config *global_ma, uint32_t group_dim,
                                uint32_t bit_depth, uint32_t stream_idx) {
    jxl_modular gm;
    jxl_chanlist tcl;
    int rc = -1;

    if (cl->n == 0) return 0;
    memset(&tcl, 0, sizeof(tcl));
    if (jxl_modular_init_over(ctx, &gm, br, cl->chans, cl->n, global_ma,
                              group_dim, bit_depth) != 0)
        goto done;
    if (jxl_modular_transform_channels(ctx, &gm, &tcl) != 0) goto done;
    if (jxl_modular_decode(ctx, &gm, &tcl, br, stream_idx) != 0) goto done;
    if (jxl_modular_inverse(ctx, &gm, &tcl) != 0) goto done;
    rc = 0;

done:
    jxl_chanlist_free(ctx, &tcl);
    jxl_modular_free(ctx, &gm);
    return rc;
}

typedef struct {
    jxl_quantizer quantizer;
    jxl_hf_block_ctx block_ctx;
    jxl_lf_chan_corr chan_corr;

    uint32_t bw, bh;
    uint32_t pw, ph;
    float *lf[3];

    int hs[3], vs[3];
    int32_t *lfq[3];
    float *coeff[3];
    jxl_block_info *block_info;
    float *epf_sigma;
    int32_t *x_from_y, *b_from_y;
    uint32_t cfl_w, cfl_h;

    jxl_dequant_matrices dm;
    int have_dm;
    jxl_hf_pass *passes;
    uint32_t num_passes;
    uint32_t num_hf_presets;
    jxl_natural_orders no;
} jxl_vardct_state;

static void vardct_state_free(jxl_ctx *ctx, jxl_vardct_state *v) {
    uint32_t i;
    int c;
    for (c = 0; c < 3; c++) {
        jxl_free(ctx, v->lf[c]);
        jxl_free(ctx, v->lfq[c]);
        jxl_free(ctx, v->coeff[c]);
    }
    jxl_free(ctx, v->block_info);
    jxl_free(ctx, v->epf_sigma);
    jxl_free(ctx, v->x_from_y);
    jxl_free(ctx, v->b_from_y);
    if (v->have_dm) jxl_dequant_matrices_free(ctx, &v->dm);
    if (v->passes) {
        for (i = 0; i < v->num_passes; i++) jxl_hf_pass_free(ctx, &v->passes[i]);
        jxl_free(ctx, v->passes);
    }
    jxl_hf_block_ctx_free(ctx, &v->block_ctx);
    jxl_natural_orders_free(ctx, &v->no);
    memset(v, 0, sizeof(*v));
}

static int vardct_state_alloc(jxl_ctx *ctx, jxl_vardct_state *v, uint32_t bw,
                               uint32_t bh, int skip_coeff0_zero) {
    size_t coeff_count, coeff_bytes;
    int c;
    v->bw = bw;
    v->bh = bh;
    v->pw = bw * 8;
    v->ph = bh * 8;
    v->cfl_w = (v->pw + 63) / 64;
    v->cfl_h = (v->ph + 63) / 64;
    if (!jxl_size_mul(v->pw, v->ph, &coeff_count) ||
        !jxl_size_mul(coeff_count, sizeof(float), &coeff_bytes))
        return -1;
    for (c = 0; c < 3; c++) {
        v->lf[c] = (float *)jxl_calloc(ctx, (size_t)bw * bh, sizeof(float));
        v->lfq[c] = (int32_t *)jxl_calloc(ctx, (size_t)bw * bh, sizeof(int32_t));
        if (c == 0 && skip_coeff0_zero) {
            v->coeff[c] = (float *)jxl_malloc(ctx, coeff_bytes);
        } else {
            v->coeff[c] = (float *)jxl_calloc(ctx, coeff_count, sizeof(float));
        }
        if (!v->lf[c] || !v->lfq[c] || !v->coeff[c]) return -1;
    }
    v->block_info =
        (jxl_block_info *)jxl_calloc(ctx, (size_t)bw * bh, sizeof(jxl_block_info));
    v->epf_sigma = (float *)jxl_calloc(ctx, (size_t)bw * bh, sizeof(float));
    v->x_from_y =
        (int32_t *)jxl_calloc(ctx, (size_t)v->cfl_w * v->cfl_h, sizeof(int32_t));
    v->b_from_y =
        (int32_t *)jxl_calloc(ctx, (size_t)v->cfl_w * v->cfl_h, sizeof(int32_t));
    if (!v->block_info || !v->epf_sigma || !v->x_from_y || !v->b_from_y) return -1;
    {
        size_t i;
        for (i = 0; i < (size_t)bw * bh; i++) {
            v->block_info[i].dct_select = JXL_BLK_UNINIT;
        }
    }
    return 0;
}

static int decode_lf_coeff(jxl_ctx *ctx, jxl_br *br, jxl_vardct_state *v,
                           const jxl_lf_dequant *lfd, uint32_t lf_group_idx,
                           uint32_t lf_width, uint32_t lf_height,
                           uint32_t base_bx, uint32_t base_by,
                           uint32_t bit_depth, jxl_ma_config *global_ma) {
    uint32_t extra_precision = jxl_br_read(br, 2);
    uint32_t w = (lf_width + 7) / 8;
    uint32_t h = (lf_height + 7) / 8;
    jxl_mchan_spec specs[3];
    jxl_modular mod;
    jxl_chanlist cl;

    static const int plane_of[3] = {1, 0, 2};
    float m_lf[3];
    int i, rc = -1;

    memset(&mod, 0, sizeof(mod));
    memset(&cl, 0, sizeof(cl));
    m_lf[0] = lfd->m_x_lf;
    m_lf[1] = lfd->m_y_lf;
    m_lf[2] = lfd->m_b_lf;

    for (i = 0; i < 3; i++) {
        specs[plane_of[i]].w = w >> v->hs[i];
        specs[plane_of[i]].h = h >> v->vs[i];
        specs[plane_of[i]].hshift = 0;
        specs[plane_of[i]].vshift = 0;
    }
    if (jxl_modular_init(ctx, &mod, br, specs, 3, global_ma, 0, bit_depth) != 0)
        goto done;
    if (jxl_modular_transform_channels(ctx, &mod, &cl) != 0) goto done;
    if (jxl_modular_decode(ctx, &mod, &cl, br, 1 + lf_group_idx) != 0) goto done;
    if (jxl_modular_inverse(ctx, &mod, &cl) != 0) goto done;

    for (i = 0; i < 3; i++) {
        const jxl_mchan *src = &mod.base[plane_of[i]];
        uint32_t bx0 = base_bx >> v->hs[i], by0 = base_by >> v->vs[i];
        uint32_t lim_w = v->bw >> v->hs[i], lim_h = v->bh >> v->vs[i];
        float *dst = v->lf[i] + (size_t)by0 * v->bw + bx0;
        uint32_t xx, yy;
        jxl_copy_lf_dequant(dst, v->bw, src, &v->quantizer, m_lf[i],
                            (int)extra_precision);
        for (yy = 0; yy < src->h && by0 + yy < lim_h; yy++) {
            for (xx = 0; xx < src->w && bx0 + xx < lim_w; xx++) {
                v->lfq[i][(size_t)(by0 + yy) * v->bw + bx0 + xx] =
                    src->data[(size_t)yy * src->stride + xx];
            }
        }
    }
    rc = 0;

done:
    jxl_chanlist_free(ctx, &cl);
    jxl_modular_free(ctx, &mod);
    return rc;
}

static void merge_hf_meta(jxl_vardct_state *v, const jxl_hf_meta *m,
                          uint32_t base_bx, uint32_t base_by) {
    uint32_t x, y;
    for (y = 0; y < m->bh && base_by + y < v->bh; y++) {
        for (x = 0; x < m->bw && base_bx + x < v->bw; x++) {
            v->block_info[(size_t)(base_by + y) * v->bw + base_bx + x] =
                m->block_info[(size_t)y * m->bw + x];
            {
                float sigma = m->epf_sigma[(size_t)y * m->bw + x];
                v->epf_sigma[(size_t)(base_by + y) * v->bw + base_bx + x] =
                    sigma >= 0.3f ? JXL_EPF_INV_SIGMA_NUM / sigma : 0.0f;
            }
        }
    }
    for (y = 0; y < m->cfl_h; y++) {
        uint32_t gy = base_by / 8 + y;
        if (gy >= v->cfl_h) break;
        for (x = 0; x < m->cfl_w; x++) {
            uint32_t gx = base_bx / 8 + x;
            if (gx >= v->cfl_w) break;
            v->x_from_y[(size_t)gy * v->cfl_w + gx] = m->x_from_y[(size_t)y * m->cfl_w + x];
            v->b_from_y[(size_t)gy * v->cfl_w + gx] = m->b_from_y[(size_t)y * m->cfl_w + x];
        }
    }
}

static void vardct_finish_blocks(jxl_vardct_state *v,
                                 const jxl_image_metadata *meta,
                                 const jxl_frame_header *fh, int skip_cb) {
    float qm_scale[3];
    uint32_t bx, by;
    int c, batch_dct8 = 0;

    qm_scale[0] = powf(0.8f, (float)fh->x_qm_scale - 2.0f);
    qm_scale[1] = 1.0f;
    qm_scale[2] = powf(0.8f, (float)fh->b_qm_scale - 2.0f);

    if (jxl_has_avx2_fma() &&
        !v->hs[0] && !v->vs[0] && !v->hs[1] && !v->vs[1] &&
        !v->hs[2] && !v->vs[2]) {
        size_t i, nblocks = (size_t)v->bw * v->bh;
        batch_dct8 = 1;
        for (i = 0; i < nblocks; i++) {
            if (v->block_info[i].dct_select != JXL_TR_DCT8) {
                batch_dct8 = 0;
                break;
            }
        }
    }

    if (batch_dct8) {
        for (c = skip_cb ? 1 : 0; c < 3; c++) {
            jxl_dequant_dct8_plane(
                v->coeff[c], v->pw, v->block_info, v->bw, v->bh, c,
                &v->dm, &v->quantizer, qm_scale[c], meta->quant_bias[c],
                meta->quant_bias_numerator);
        }
    } else {
        for (c = skip_cb ? 1 : 0; c < 3; c++) {
            for (by = 0; by < v->bh; by++) {
                uint32_t sby = by >> v->vs[c];
                if ((sby << v->vs[c]) != by) continue;
                for (bx = 0; bx < v->bw; bx++) {
                    const jxl_block_info *bi =
                        &v->block_info[(size_t)by * v->bw + bx];
                    uint32_t sbx = bx >> v->hs[c];
                    if ((sbx << v->hs[c]) != bx) continue;
                    if (bi->dct_select >= JXL_TR_COUNT) continue;
                    jxl_dequant_varblock(
                        v->coeff[c] + (size_t)(sby * 8) * v->pw + sbx * 8,
                        v->pw, bi->dct_select, bi->hf_mul, c, &v->dm,
                        &v->quantizer, qm_scale[c], meta->quant_bias[c],
                        meta->quant_bias_numerator);
                }
            }
        }
    }

    if (!v->hs[0] && !v->vs[0] && !v->hs[2] && !v->vs[2]) {
        jxl_cfl_hf(v->coeff[0], v->coeff[1], v->coeff[2], v->pw, v->pw, v->ph,
                   v->x_from_y, v->b_from_y, v->cfl_w, &v->chan_corr, skip_cb);
    }

    if (batch_dct8) {

        for (by = 0; by < v->bh; by++) {
            for (bx = 0; bx < v->bw; bx++) {
                jxl_block_info *bi =
                    &v->block_info[(size_t)by * v->bw + bx];
                uint8_t mask = bi->hf_nonzero_mask;
                if (mask & (1u << 1)) {
                    size_t corr_idx =
                        (size_t)(by / 8) * v->cfl_w + bx / 8;
                    float kx = v->chan_corr.base_correlation_x +
                        (float)v->x_from_y[corr_idx] /
                            (float)v->chan_corr.colour_factor;
                    float kb = v->chan_corr.base_correlation_b +
                        (float)v->b_from_y[corr_idx] /
                            (float)v->chan_corr.colour_factor;
                    if (kx != 0.0f) mask |= 1u << 0;
                    if (kb != 0.0f) mask |= 1u << 2;
                }
                bi->hf_transform_mask = mask;
            }
        }
        for (c = skip_cb ? 1 : 0; c < 3; c++) {
            for (by = 0; by < v->bh; by++) {
                float *row = v->coeff[c] + (size_t)(by * 8) * v->pw;
                const float *lf_row = v->lf[c] + (size_t)by * v->bw;
                for (bx = 0; bx < v->bw; bx++) row[bx * 8] = lf_row[bx];
            }
            jxl_idct8x8_plane(v->coeff[c], v->pw, v->block_info, c,
                              v->bw, v->bh);
        }
        return;
    }

    for (c = skip_cb ? 1 : 0; c < 3; c++) {
        for (by = 0; by < v->bh; by++) {
            uint32_t sby = by >> v->vs[c];
            if ((sby << v->vs[c]) != by) continue;
            for (bx = 0; bx < v->bw; bx++) {
                const jxl_block_info *bi = &v->block_info[(size_t)by * v->bw + bx];
                uint32_t sbx = bx >> v->hs[c];
                float *blk;
                if ((sbx << v->hs[c]) != bx) continue;
                if (bi->dct_select >= JXL_TR_COUNT) continue;
                blk = v->coeff[c] + (size_t)(sby * 8) * v->pw + sbx * 8;
                jxl_fill_varblock_lf(blk, v->pw, bi->dct_select, v->lf[c], v->bw,
                                     sbx, sby);
                jxl_transform_varblock(blk, v->pw, bi->dct_select);
            }
        }
    }
}

void jxl_frame_state_free(jxl_ctx *ctx, jxl_frame_state *st) {
    int i;
    for (i = 0; i < 4; i++) {
        jxl_fimage_free(ctx, &st->refs[i]);
        st->refs_valid[i] = 0;
    }
    jxl_fimage_free(ctx, &st->lf_image);
    st->lf_valid = 0;
}

int jxl_frame_decode(jxl_ctx *ctx, jxl_doc *doc, const jxl_frame_header *fh,
                     const jxl_toc *toc, jxl_frame_state *st, int apply_ct,
                     jxl_fimage *out) {
    const jxl_image_metadata *meta = &doc->meta;
    jxl_sections sec;
    jxl_br *br;
    jxl_lf_dequant lf_dequant;
    jxl_ma_config global_ma;
    int has_global_ma = 0;
    jxl_modular gmod;
    jxl_chanlist gcl;
    jxl_chanlist prefix;
    jxl_mchan_spec *specs = NULL;
    jxl_group_lists gl;
    jxl_pass_shifts pshifts;
    jxl_vardct_state vd;
    jxl_patches patches;
    jxl_splines splines;
    jxl_noise_params noise;
    int have_patches = 0, have_splines = 0, have_noise = 0;
    int is_vardct, discard_cb;
    uint32_t nspecs = 0, i, split;
    uint32_t color_w, color_h, group_dim, group_dim_shift;
    uint32_t num_lf_groups, num_groups, num_passes;
    uint32_t color_upsampling_shift;
    uint32_t bits;
    uint32_t ncolor;
    int rc = -1;

    memset(&global_ma, 0, sizeof(global_ma));
    memset(&gmod, 0, sizeof(gmod));
    memset(&gcl, 0, sizeof(gcl));
    memset(&prefix, 0, sizeof(prefix));
    memset(&gl, 0, sizeof(gl));
    memset(&vd, 0, sizeof(vd));
    memset(&patches, 0, sizeof(patches));
    memset(&splines, 0, sizeof(splines));
    memset(&noise, 0, sizeof(noise));
    memset(out, 0, sizeof(*out));

    is_vardct = (fh->encoding == JXL_ENC_VARDCT);
    discard_cb = is_vardct && apply_ct && fh->do_ycbcr &&
                 meta->colour.colour_space == JXLDEC_CS_GRAY;
    if ((fh->flags & JXL_FF_USE_LF_FRAME) && !st->lf_valid) {
        JXL_ERR(ctx, "frame: LF frame referenced but not decoded");
        return -1;
    }

    for (i = 0; i < meta->num_extra; i++) {

        if (fh->ec_upsampling[i] != fh->upsampling) {
            JXL_ERR(ctx, "frame: extra channel %u upsamples by %u but the "
                         "colour channels by %u; not supported",
                    (unsigned)i, (unsigned)fh->ec_upsampling[i],
                    (unsigned)fh->upsampling);
            return -1;
        }

        if (meta->ec_info[i].type == JXL_EC_SPOT) {
            JXL_ERR(ctx, "frame: extra channel %u is a spot colour; "
                         "not supported", (unsigned)i);
            return -1;
        }
    }

    color_w = jxl_frame_color_width(fh);
    color_h = jxl_frame_color_height(fh);
    group_dim = jxl_frame_group_dim(fh);
    group_dim_shift = 7 + fh->group_size_shift;
    num_lf_groups = jxl_frame_num_lf_groups(fh);
    num_groups = jxl_frame_num_groups(fh);
    num_passes = fh->passes.num_passes;
    bits = meta->bit_depth.bits_per_sample;
    ncolor = (uint32_t)fh->encoded_color_channels;

    sections_init(&sec, doc->container.cs, doc->container.cs_len, toc);
    br = section_reader(&sec, JXL_TOC_LF_GLOBAL, 0, 0);
    if (!br) return -1;

    if (fh->flags & JXL_FF_PATCHES) {
        if (jxl_patches_read(ctx, br, meta, fh, &patches) != 0) goto done;
        have_patches = 1;
    }
    if (fh->flags & JXL_FF_SPLINES) {
        if (jxl_splines_read(ctx, br, fh, &splines) != 0) goto done;
        have_splines = 1;
    }
    if (fh->flags & JXL_FF_NOISE) {
        if (jxl_noise_params_read(br, &noise) != 0) goto done;
        have_noise = 1;
    }
    lf_dequant_read(br, &lf_dequant);

    if (is_vardct) {
        for (i = 0; i < 3; i++) {
            jxl_jpeg_upsampling_shifts(fh->jpeg_upsampling, i, &vd.hs[i],
                                       &vd.vs[i]);
        }
        if (vardct_state_alloc(
                ctx, &vd, jxl_frame_blocks_w(fh), jxl_frame_blocks_h(fh),
                discard_cb && !fh->gab.enabled && !fh->epf.enabled &&
                    !(fh->flags &
                      (JXL_FF_PATCHES | JXL_FF_SPLINES | JXL_FF_NOISE)) &&
                    fh->upsampling == 1) != 0)
            goto done;
        jxl_quantizer_read(br, &vd.quantizer);
        if (jxl_hf_block_ctx_read(ctx, br, &vd.block_ctx) != 0) goto done;
        jxl_lf_chan_corr_read(br, &vd.chan_corr, meta->xyb_encoded);
        if (vd.quantizer.global_scale == 0 || vd.quantizer.quant_lf == 0) {
            JXL_ERR(ctx, "frame: zero quantizer scale");
            goto done;
        }
    }

    if (jxl_br_bool(br)) {
        uint64_t num_channels = ncolor + meta->num_extra;
        uint64_t limit = 1024 + (uint64_t)fh->width * fh->height * num_channels / 16;
        if (limit > (1u << 22)) limit = 1u << 22;
        if (jxl_ma_config_read(ctx, br, &global_ma, (size_t)limit) != 0) goto done;
        has_global_ma = 1;
    }

    nspecs = (is_vardct ? 0 : ncolor) + meta->num_extra;
    specs = (jxl_mchan_spec *)jxl_calloc(ctx, nspecs ? nspecs : 1,
                                         sizeof(jxl_mchan_spec));
    if (!specs) goto done;
    if (!is_vardct) {
        for (i = 0; i < ncolor; i++) {
            specs[i].w = color_w;
            specs[i].h = color_h;
            specs[i].hshift = 0;
            specs[i].vshift = 0;
        }
    }
    color_upsampling_shift = 0;
    {
        uint32_t u = fh->upsampling;
        while (u > 1) { color_upsampling_shift++; u >>= 1; }
    }
    for (i = 0; i < meta->num_extra; i++) {
        uint32_t base = is_vardct ? 0 : ncolor;
        uint32_t ec_shift = 0;
        uint32_t u = fh->ec_upsampling[i];
        int32_t actual;
        while (u > 1) { ec_shift++; u >>= 1; }
        actual = (int32_t)(ec_shift + meta->ec_info[i].dim_shift) -
                 (int32_t)color_upsampling_shift;
        if (actual < 0) actual = 0;
        specs[base + i].w = color_w;
        specs[base + i].h = color_h;
        specs[base + i].hshift = actual;
        specs[base + i].vshift = actual;
    }

    if (jxl_modular_init(ctx, &gmod, br, specs, nspecs,
                         has_global_ma ? &global_ma : NULL, group_dim,
                         bits) != 0)
        goto done;
    if (nspecs) {
        if (jxl_modular_transform_channels(ctx, &gmod, &gcl) != 0) goto done;
        split = 0;
        while (split < gcl.n) {
            const jxl_mchan *ch = &gcl.chans[split];
            if (split < gcl.nb_meta || (ch->w <= group_dim && ch->h <= group_dim)) {
                split++;
            } else {
                break;
            }
        }
        prefix.chans = gcl.chans;
        prefix.n = split;
        prefix.cap = 0;
        if (jxl_modular_decode(ctx, &gmod, &prefix, br, 0) != 0) goto done;
    } else {
        split = 0;
    }

    compute_pass_shifts(fh, &pshifts);
    gl.num_lf = num_lf_groups;
    gl.num_groups = num_groups;
    gl.num_passes = num_passes;
    gl.lf = (jxl_chanlist *)jxl_calloc(ctx, num_lf_groups ? num_lf_groups : 1,
                                       sizeof(jxl_chanlist));
    gl.pass = (jxl_chanlist *)jxl_calloc(
        ctx, (size_t)(num_passes ? num_passes : 1) * (num_groups ? num_groups : 1),
        sizeof(jxl_chanlist));
    if (!gl.lf || !gl.pass) goto done;

    for (i = split; i < gcl.n; i++) {
        const jxl_mchan *ch = &gcl.chans[i];
        int hshift = ch->hshift, vshift = ch->vshift;
        if (hshift < 0 || vshift < 0) {
            JXL_ERR(ctx, "frame: unshiftable channel outside the global stream");
            goto done;
        }
        if (hshift < 3 || vshift < 3) {
            int32_t shift = hshift < vshift ? hshift : vshift;
            uint32_t pass_idx = pass_for_shift(fh, &pshifts, shift);
            uint32_t gw = group_dim >> hshift;
            uint32_t gh = group_dim >> vshift;
            uint32_t nx = (ch->ow + group_dim - 1) >> group_dim_shift;
            uint32_t ny = (ch->oh + group_dim - 1) >> group_dim_shift;
            if (gw == 0 || gh == 0 || pass_idx >= num_passes) {
                JXL_ERR(ctx, "frame: bad channel shift for the group size");
                goto done;
            }
            if (split_channel_into_groups(ctx, ch, gw, gh, nx, ny,
                                          gl.pass + (size_t)pass_idx * num_groups,
                                          num_groups) != 0)
                goto done;
        } else {
            uint32_t gw = group_dim >> (hshift - 3);
            uint32_t gh = group_dim >> (vshift - 3);
            uint32_t nx = (ch->ow + (group_dim << 3) - 1) >> (group_dim_shift + 3);
            uint32_t ny = (ch->oh + (group_dim << 3) - 1) >> (group_dim_shift + 3);
            if (gw == 0 || gh == 0) {
                JXL_ERR(ctx, "frame: bad channel shift for the LF group size");
                goto done;
            }
            if (split_channel_into_groups(ctx, ch, gw, gh, nx, ny, gl.lf,
                                          num_lf_groups) != 0)
                goto done;
        }
    }

    {
        uint32_t lf_per_row = jxl_frame_lf_groups_per_row(fh);
        for (i = 0; i < num_lf_groups; i++) {
            uint32_t lg_x = lf_per_row ? i % lf_per_row : 0;
            uint32_t lg_y = lf_per_row ? i / lf_per_row : 0;

            uint32_t blocks_w = jxl_frame_blocks_w(fh);
            uint32_t blocks_h = jxl_frame_blocks_h(fh);
            uint32_t base_bx = lg_x * group_dim;
            uint32_t base_by = lg_y * group_dim;
            uint32_t lf_w = JXL_MIN(group_dim, blocks_w - base_bx) * 8;
            uint32_t lf_h = JXL_MIN(group_dim, blocks_h - base_by) * 8;

            br = section_reader(&sec, JXL_TOC_LF_GROUP, 0, i);
            if (!br) goto done;
            if (is_vardct && !(fh->flags & JXL_FF_USE_LF_FRAME)) {
                if (decode_lf_coeff(ctx, br, &vd, &lf_dequant, i, lf_w, lf_h,
                                    base_bx, base_by, bits,
                                    has_global_ma ? &global_ma : NULL) != 0)
                    goto done;
            }
            if (decode_group_modular(ctx, br, &gl.lf[i],
                                     has_global_ma ? &global_ma : NULL, group_dim,
                                     bits, 1 + num_lf_groups + i) != 0)
                goto done;
            if (is_vardct) {
                jxl_hf_meta hm;
                if (jxl_hf_meta_read(ctx, br, &hm, num_lf_groups, i, lf_w, lf_h,
                                     fh->jpeg_upsampling, bits,
                                     has_global_ma ? &global_ma : NULL, &fh->epf,
                                     vd.quantizer.global_scale) != 0)
                    goto done;
                merge_hf_meta(&vd, &hm, base_bx, base_by);
                jxl_hf_meta_free(ctx, &hm);
            }
        }
    }

    if (is_vardct) {
        uint32_t p;
        br = section_reader(&sec, JXL_TOC_HF_GLOBAL, 0, 0);
        if (!br) goto done;
        if (jxl_dequant_matrices_read(ctx, br, &vd.dm, bits, num_lf_groups,
                                      has_global_ma ? &global_ma : NULL) != 0)
            goto done;
        vd.have_dm = 1;
        vd.num_hf_presets =
            jxl_br_read(br, (int)jxl_bitlen(num_groups > 1 ? num_groups - 1 : 0)) + 1;
        vd.passes = (jxl_hf_pass *)jxl_calloc(ctx, num_passes, sizeof(jxl_hf_pass));
        if (!vd.passes) goto done;
        vd.num_passes = num_passes;
        for (p = 0; p < num_passes; p++) {
            if (jxl_hf_pass_read(ctx, br, &vd.passes[p], &vd.block_ctx,
                                 vd.num_hf_presets, &vd.no) != 0)
                goto done;
        }
    }

    {
        uint32_t p, g;
        uint32_t groups_per_row = jxl_frame_groups_per_row(fh);
        uint32_t lf_per_row = jxl_frame_lf_groups_per_row(fh);
        for (p = 0; p < num_passes; p++) {
            for (g = 0; g < num_groups; g++) {
                jxl_chanlist *cl = &gl.pass[(size_t)p * num_groups + g];
                br = section_reader(&sec, JXL_TOC_GROUP_PASS, p, g);
                if (!br) goto done;
                if (is_vardct) {
                    uint32_t gx = groups_per_row ? g % groups_per_row : 0;
                    uint32_t gy = groups_per_row ? g / groups_per_row : 0;
                    uint32_t bx0 = gx * (group_dim / 8);
                    uint32_t by0 = gy * (group_dim / 8);
                    uint32_t bwid = JXL_MIN(group_dim / 8, vd.bw - bx0);
                    uint32_t bhig = JXL_MIN(group_dim / 8, vd.bh - by0);
                    jxl_hf_coeff_params hp;
                    jxl_mchan lfq_view[3];
                    float *outp[3];
                    size_t strides[3];
                    int c;
                    (void)lf_per_row;

                    memset(&hp, 0, sizeof(hp));
                    hp.num_hf_presets = vd.num_hf_presets;
                    hp.bc = &vd.block_ctx;
                    hp.block_info = vd.block_info + (size_t)by0 * vd.bw + bx0;
                    hp.bi_w = bwid;
                    hp.bi_h = bhig;
                    hp.bi_stride = vd.bw;
                    for (c = 0; c < 3; c++) {

                        uint32_t sbx0 = bx0 >> vd.hs[c];
                        uint32_t sby0 = by0 >> vd.vs[c];
                        hp.jpeg_upsampling[c] = fh->jpeg_upsampling[c];
                        memset(&lfq_view[c], 0, sizeof(lfq_view[c]));
                        lfq_view[c].data = vd.lfq[c] + (size_t)sby0 * vd.bw + sbx0;
                        lfq_view[c].stride = vd.bw;
                        lfq_view[c].w = (bwid + ((1u << vd.hs[c]) - 1)) >> vd.hs[c];
                        lfq_view[c].h = (bhig + ((1u << vd.vs[c]) - 1)) >> vd.vs[c];
                        hp.lf_quant[c] = &lfq_view[c];
                        outp[c] = vd.coeff[c] + (size_t)(sby0 * 8) * vd.pw +
                                  sbx0 * 8;
                        strides[c] = vd.pw;
                    }
                    hp.pass = &vd.passes[p];
                    hp.coeff_shift = p < 16 ? fh->passes.shift[p] : 0;
                    hp.no = &vd.no;
                    hp.discard_mask = discard_cb ? 1u : 0u;
                    if (jxl_write_hf_coeff(ctx, br, &hp, outp, strides) != 0)
                        goto done;
                }
                if (decode_group_modular(ctx, br, cl,
                                         has_global_ma ? &global_ma : NULL,
                                         group_dim, bits,
                                         1 + 3 * num_lf_groups + 17 +
                                             p * num_groups + g) != 0)
                    goto done;
            }
        }
    }

    if (nspecs && jxl_modular_inverse(ctx, &gmod, &gcl) != 0) goto done;

    if (is_vardct) {
        float m_lf[3];
        float *planes[3];
        int c;

        m_lf[0] = lf_dequant.m_x_lf;
        m_lf[1] = lf_dequant.m_y_lf;
        m_lf[2] = lf_dequant.m_b_lf;
        if (fh->flags & JXL_FF_USE_LF_FRAME) {

            uint32_t x, y;
            for (c = 0; c < 3; c++) {
                const jxl_fplane *p = &st->lf_image.plane[c];
                for (y = 0; y < vd.bh; y++) {
                    uint32_t sy = y < p->h ? y : (p->h ? p->h - 1 : 0);
                    for (x = 0; x < vd.bw; x++) {
                        uint32_t sx = x < p->w ? x : (p->w ? p->w - 1 : 0);
                        vd.lf[c][(size_t)y * vd.bw + x] =
                            p->data[(size_t)sy * p->stride + sx];
                    }
                }
            }
        } else {
            if (!vd.hs[0] && !vd.vs[0] && !vd.hs[2] && !vd.vs[2]) {
                jxl_cfl_lf(vd.lf[0], vd.lf[1], vd.lf[2], vd.bw, vd.bh, vd.bw,
                           &vd.chan_corr);
            }
            if (!(fh->flags & JXL_FF_SKIP_ADAPTIVE_LF_SMOOTH)) {
                if (jxl_adaptive_lf_smoothing(ctx, vd.lf, vd.bw, vd.bh, vd.bw,
                                              m_lf, &vd.quantizer) != 0)
                    goto done;
            }
        }

        {
            uint8_t used[JXL_TR_COUNT];
            size_t nb = (size_t)vd.bw * vd.bh, k;
            memset(used, 0, sizeof(used));
            for (k = 0; k < nb; k++) {
                uint8_t t = vd.block_info[k].dct_select;
                if (t < JXL_TR_COUNT) used[t] = 1;
            }
            for (k = 0; k < JXL_TR_COUNT; k++) {
                if (!used[k]) continue;
                if (jxl_dequant_matrices_ensure(ctx, &vd.dm, (int)k) != 0) {
                    JXL_ERR(ctx, "vardct: bad dequant matrix");
                    goto done;
                }
            }
        }
        vardct_finish_blocks(&vd, meta, fh, discard_cb);

        for (c = 0; c < 3; c++) {
            if (!vd.hs[c] && !vd.vs[c]) continue;
            jxl_chroma_upsample(vd.coeff[c],
                                div_ceil32(color_w, 1u << vd.hs[c]),
                                div_ceil32(color_h, 1u << vd.vs[c]), vd.pw,
                                vd.hs[c], vd.vs[c], vd.pw, vd.ph);
        }

        for (c = 0; c < 3; c++) planes[c] = vd.coeff[c];
        if (fh->gab.enabled) {
            if (jxl_apply_gabor(ctx, planes, color_w, color_h, vd.pw,
                                fh->gab.weights) != 0)
                goto done;
        }
        if (fh->epf.enabled) {
            if (jxl_apply_epf(ctx, planes, color_w, color_h, vd.pw, vd.epf_sigma,
                              vd.bw, &fh->epf) != 0)
                goto done;
            for (c = 0; c < 3; c++) vd.coeff[c] = planes[c];
        }
    }

    {
        uint32_t nplane = (is_vardct ? 3 : gmod.nbase) +
                          (is_vardct ? meta->num_extra : 0);
        uint32_t base = 0;
        float scale = 1.0f;
        if (jxl_fimage_alloc(ctx, out, nplane) != 0) goto done;
        out->ncolor = is_vardct ? 3 : ncolor;
        out->w = color_w;
        out->h = color_h;
        if (!meta->bit_depth.float_sample) {
            scale = 1.0f / (float)((1u << bits) - 1);
        }
        if (is_vardct) {

            for (i = 0; i < 3; i++) {
                out->plane[i].data = vd.coeff[i];
                out->plane[i].w = color_w;
                out->plane[i].h = color_h;
                out->plane[i].stride = vd.pw;
                vd.coeff[i] = NULL;
            }
            base = 3;
        }

        if (!is_vardct && meta->xyb_encoded && gmod.nbase >= 3) {
            float mul[3];
            uint32_t x, y;
            static const int src_of[3] = {1, 0, 2};
            mul[0] = lf_dequant.m_x_lf / 128.0f;
            mul[1] = lf_dequant.m_y_lf / 128.0f;
            mul[2] = lf_dequant.m_b_lf / 128.0f;
            for (y = 0; y < gmod.base[2].h; y++) {
                int32_t *rb = gmod.base[2].data + (size_t)y * gmod.base[2].stride;
                const int32_t *ry = gmod.base[0].data + (size_t)y * gmod.base[0].stride;
                for (x = 0; x < gmod.base[2].w; x++) {
                    rb[x] = (int32_t)((uint32_t)rb[x] + (uint32_t)ry[x]);
                }
            }
            for (i = 0; i < 3; i++) {
                const jxl_mchan *ch = &gmod.base[src_of[i]];
                if (jxl_fplane_alloc_uninit(ctx, &out->plane[i], ch->w, ch->h) != 0) goto done;
                for (y = 0; y < ch->h; y++) {
                    const int32_t *src = ch->data + (size_t)y * ch->stride;
                    float *dst = out->plane[i].data + (size_t)y * out->plane[i].stride;
                    for (x = 0; x < ch->w; x++) dst[x] = (float)src[x] * mul[i];
                }
            }
        }
        for (i = 0; i < gmod.nbase; i++) {
            const jxl_mchan *ch = &gmod.base[i];
            uint32_t x, y;
            uint32_t pi = base + i;
            if (pi >= nplane) break;
            if (!is_vardct && meta->xyb_encoded && i < 3) continue;
            if (jxl_fplane_alloc_uninit(ctx, &out->plane[pi], ch->w, ch->h) != 0) goto done;
            for (y = 0; y < ch->h; y++) {
                const int32_t *src = ch->data + (size_t)y * ch->stride;
                float *dst = out->plane[pi].data + (size_t)y * out->plane[pi].stride;
                for (x = 0; x < ch->w; x++) dst[x] = (float)src[x] * scale;
            }
        }
    }

    if (have_patches) {
        if (jxl_apply_patches(ctx, out, &patches, meta, st->refs,
                              st->refs_valid) != 0)
            goto done;
    }

    if (have_splines) {
        float cx = is_vardct ? vd.chan_corr.base_correlation_x : 0.0f;
        float cb = is_vardct ? vd.chan_corr.base_correlation_b : 1.0f;
        if (jxl_render_splines(ctx, out, &splines, fh, cx, cb) != 0) goto done;
    }

    if (color_upsampling_shift > 0) {
        uint32_t full_w = jxl_frame_sample_width(fh, 1);
        uint32_t full_h = jxl_frame_sample_height(fh, 1);
        for (i = 0; i < out->nplane; i++) {

            uint32_t tw = out->plane[i].w << color_upsampling_shift;
            uint32_t th = out->plane[i].h << color_upsampling_shift;
            if (tw > full_w) tw = full_w;
            if (th > full_h) th = full_h;
            if (jxl_upsample_plane(ctx, &out->plane[i], color_upsampling_shift,
                                   meta, tw, th) != 0)
                goto done;
        }
        out->w = full_w;
        out->h = full_h;
    }

    if (have_noise) {
        float cx = is_vardct ? vd.chan_corr.base_correlation_x : 0.0f;
        float cb = is_vardct ? vd.chan_corr.base_correlation_b : 1.0f;
        if (jxl_render_noise(ctx, out, &noise, fh, st->visible_frames,
                             st->invisible_frames, cx, cb) != 0)
            goto done;
    }

    if (apply_ct && fh->do_ycbcr && out->ncolor >= 3) {
        uint32_t row, rows = out->plane[0].h, rw = out->plane[0].w;
        if (meta->colour.colour_space == JXLDEC_CS_GRAY) {
            for (row = 0; row < rows; row++) {
                jxl_ycbcr_to_gray(
                    out->plane[0].data + (size_t)row * out->plane[0].stride,
                    out->plane[1].data + (size_t)row * out->plane[1].stride,
                    out->plane[2].data + (size_t)row * out->plane[2].stride,
                    rw);
            }
            fimage_keep_gray(ctx, out);
        } else {
            for (row = 0; row < rows; row++) {
                jxl_ycbcr_to_rgb(
                    out->plane[0].data + (size_t)row * out->plane[0].stride,
                    out->plane[1].data + (size_t)row * out->plane[1].stride,
                    out->plane[2].data + (size_t)row * out->plane[2].stride,
                    rw);
            }
        }
    } else if (apply_ct && meta->xyb_encoded && out->ncolor >= 3) {
        uint32_t row, rows = out->plane[0].h, rw = out->plane[0].w;
        float opsin[9];
        jxl_colour_encoding out_enc = meta->colour;
        jxl_opsin_matrix_for(meta, opsin);

        if (ctx->srgb_output && !out_enc.tf_have_gamma &&
            out_enc.tf == JXL_TF_LINEAR) {
            out_enc.tf = JXL_TF_SRGB;
        }
        for (row = 0; row < rows; row++) {
            jxl_xyb_to_linear(out->plane[0].data + (size_t)row * out->plane[0].stride,
                              out->plane[1].data + (size_t)row * out->plane[1].stride,
                              out->plane[2].data + (size_t)row * out->plane[2].stride,
                              rw, opsin, meta->opsin_bias,
                              meta->tone_mapping.intensity_target);
            for (i = 0; i < 3; i++) {
                jxl_linear_to_tf(out->plane[i].data +
                                     (size_t)row * out->plane[i].stride,
                                 rw, &out_enc,
                                 meta->tone_mapping.intensity_target);
            }
        }

        if (meta->colour.colour_space == JXLDEC_CS_GRAY && out->ncolor == 3) {
            fimage_keep_gray(ctx, out);
        }
    }
    rc = 0;

done:
    jxl_free(ctx, specs);
    group_lists_free(ctx, &gl);
    jxl_chanlist_free(ctx, &gcl);
    jxl_modular_free(ctx, &gmod);
    vardct_state_free(ctx, &vd);
    jxl_patches_free(ctx, &patches);
    jxl_splines_free(ctx, &splines);
    if (has_global_ma) jxl_ma_config_free(ctx, &global_ma);
    if (rc != 0) jxl_fimage_free(ctx, out);
    return rc;
}

#include <math.h>

#if !defined(JXL_VARDCT_FORCE_SCALAR) && \
    (defined(__SSE2__) || defined(_M_X64) || defined(_M_AMD64) || \
     (defined(_M_IX86_FP) && _M_IX86_FP >= 2))
#define JXL_VARDCT_SSE2 1
#include <emmintrin.h>
#include <immintrin.h>
#endif

static const uint8_t tr_size[JXL_TR_COUNT][2] = {
    {1,1},{1,1},{1,1},{1,1},{2,2},{4,4},{1,2},{2,1},{1,4},{4,1},{2,4},{4,2},
    {1,1},{1,1},{1,1},{1,1},{1,1},{1,1},{8,8},{4,8},{8,4},{16,16},{8,16},
    {16,8},{32,32},{16,32},{32,16}
};

static const uint8_t tr_matrix_idx[JXL_TR_COUNT] = {
    0,1,2,3,4,5,6,6,7,7,8,8,9,9,10,10,10,10,11,12,12,13,14,14,15,16,16
};

static const uint8_t tr_order[JXL_TR_COUNT] = {
    0,1,1,1,2,3,4,4,5,5,6,6,1,1,1,1,1,1,7,8,8,9,10,10,11,12,12
};

static const uint16_t tr_matrix_dims[17][2] = {
    {8,8},{8,8},{8,8},{8,8},{16,16},{32,32},{16,8},{32,8},{32,16},{8,8},
    {8,8},{64,64},{64,32},{128,128},{128,64},{256,256},{256,128}
};

void jxl_tr_select_size(int tr, uint32_t *bw, uint32_t *bh) {
    *bw = tr_size[tr][0];
    *bh = tr_size[tr][1];
}

int jxl_tr_matrix_index(int tr) { return tr_matrix_idx[tr]; }
int jxl_tr_order_id(int tr) { return tr_order[tr]; }

void jxl_tr_matrix_size(int tr, uint32_t *w, uint32_t *h) {
    int idx = tr_matrix_idx[tr];
    *w = tr_matrix_dims[idx][0];
    *h = tr_matrix_dims[idx][1];
}

int jxl_tr_need_transpose(int tr) {
    switch (tr) {
        case JXL_TR_HORNUSS: case JXL_TR_DCT2: case JXL_TR_DCT4:
        case JXL_TR_DCT4X8: case JXL_TR_DCT8X4:
        case JXL_TR_AFV0: case JXL_TR_AFV1: case JXL_TR_AFV2: case JXL_TR_AFV3:
            return 0;
        default: return tr_size[tr][1] >= tr_size[tr][0];
    }
}

static const uint16_t order_block_sizes[13][2] = {
    {8,8},{8,8},{16,16},{32,32},{16,8},{32,8},{32,16},{64,64},{64,32},
    {128,128},{128,64},{256,256},{256,128}
};

static void fill_natural_order(uint32_t bw, uint32_t bh, uint16_t *out) {
    uint32_t y_scale = bw / bh;
    uint32_t lbw = bw / 8, lbh = bh / 8;
    uint32_t idx = 0, dist;

    while (idx < lbw * lbh) {
        uint32_t x = idx % lbw;
        uint32_t y = idx / lbw;
        out[idx * 2] = (uint16_t)x;
        out[idx * 2 + 1] = (uint16_t)y;
        idx++;
    }
    for (dist = 1; dist < 2 * bw; dist++) {
        uint32_t margin = dist > bw ? dist - bw : 0;
        uint32_t order;
        for (order = margin; order < dist - margin; order++) {
            uint32_t x, y;
            if (dist % 2 == 1) { x = order; y = dist - 1 - order; }
            else { x = dist - 1 - order; y = order; }
            if (x < lbw && y < lbw) continue;
            if (y % y_scale != 0) continue;
            out[idx * 2] = (uint16_t)x;
            out[idx * 2 + 1] = (uint16_t)(y / y_scale);
            idx++;
        }
    }
}

const uint16_t *jxl_natural_order(jxl_ctx *ctx, jxl_natural_orders *no,
                                  int order_id) {
    uint32_t bw, bh;
    if (order_id < 0 || order_id >= 13) return NULL;
    if (no->order[order_id]) return no->order[order_id];
    bw = order_block_sizes[order_id][0];
    bh = order_block_sizes[order_id][1];
    no->order[order_id] =
        (uint16_t *)jxl_calloc(ctx, (size_t)bw * bh * 2, sizeof(uint16_t));
    if (!no->order[order_id]) return NULL;
    fill_natural_order(bw, bh, no->order[order_id]);
    return no->order[order_id];
}

void jxl_natural_orders_free(jxl_ctx *ctx, jxl_natural_orders *no) {
    int i;
    for (i = 0; i < 13; i++) {
        jxl_free(ctx, no->order[i]);
        no->order[i] = NULL;
    }
}

void jxl_quantizer_read(jxl_br *br, jxl_quantizer *q) {
    q->global_scale = jxl_br_u32(br, 1, 11, 2049, 11, 4097, 12, 8193, 16);
    q->quant_lf = jxl_br_u32(br, 16, 0, 1, 5, 1, 8, 1, 16);
}

void jxl_lf_chan_corr_read(jxl_br *br, jxl_lf_chan_corr *c, int xyb) {
    c->colour_factor = 84;
    c->base_correlation_x = 0.0f;

    c->base_correlation_b = xyb ? 1.0f : 0.0f;
    c->x_factor_lf = 128;
    c->b_factor_lf = 128;
    if (jxl_br_bool(br)) return;
    c->colour_factor = jxl_br_u32(br, 84, 0, 256, 0, 2, 8, 258, 16);
    c->base_correlation_x = jxl_br_f16(br);
    c->base_correlation_b = jxl_br_f16(br);
    c->x_factor_lf = jxl_br_read(br, 8);
    c->b_factor_lf = jxl_br_read(br, 8);
}

static const uint8_t default_block_ctx_map[39] = {
    0, 1, 2, 2, 3, 3, 4, 5, 6, 6, 6, 6, 6, 7, 8, 9, 9, 10, 11, 12, 13, 14, 14,
    14, 14, 14, 7, 8, 9, 9, 10, 11, 12, 13, 14, 14, 14, 14, 14
};

int jxl_hf_block_ctx_read(jxl_ctx *ctx, jxl_br *br, jxl_hf_block_ctx *bc) {
    uint32_t bsize = 1, i, c;

    memset(bc, 0, sizeof(*bc));
    if (jxl_br_bool(br)) {
        bc->num_block_clusters = 15;
        bc->block_ctx_map_len = 39;
        bc->block_ctx_map = (uint8_t *)jxl_malloc(ctx, 39);
        if (!bc->block_ctx_map) return -1;
        memcpy(bc->block_ctx_map, default_block_ctx_map, 39);
        return 0;
    }

    for (c = 0; c < 3; c++) {
        bc->nlf[c] = jxl_br_read(br, 4);
        if (bc->nlf[c] > 15) return -1;
        bsize *= bc->nlf[c] + 1;
        for (i = 0; i < bc->nlf[c]; i++) {
            uint32_t t = jxl_br_u32(br, 0, 4, 16, 8, 272, 16, 65808, 32);
            bc->lf_thresholds[c][i] = jxl_unpack_signed(t);
        }
    }
    bc->nqf = jxl_br_read(br, 4);
    if (bc->nqf > 15) return -1;
    bsize *= bc->nqf + 1;
    for (i = 0; i < bc->nqf; i++) {
        bc->qf_thresholds[i] = 1 + jxl_br_u32(br, 0, 2, 4, 3, 12, 5, 44, 8);
    }
    if (br->err || bsize > 64) {
        JXL_ERR(ctx, "vardct: bad block context sizes");
        return -1;
    }

    bc->block_ctx_map_len = bsize * 39;
    bc->block_ctx_map = (uint8_t *)jxl_calloc(ctx, bc->block_ctx_map_len, 1);
    if (!bc->block_ctx_map) return -1;
    if (jxl_read_clusters(ctx, br, bc->block_ctx_map_len, bc->block_ctx_map,
                          &bc->num_block_clusters) != 0)
        return -1;
    return 0;
}

void jxl_hf_block_ctx_free(jxl_ctx *ctx, jxl_hf_block_ctx *bc) {
    jxl_free(ctx, bc->block_ctx_map);
    bc->block_ctx_map = NULL;
}

static const float dq_seq_a[7] = {
    -1.025f, -0.78f, -0.65012f, -0.19041574f, -0.20819396f, -0.421064f,
    -0.32733846f
};
static const float dq_seq_b[7] = {
    -0.30419582f, -0.36330363f, -0.3566038f, -0.34430745f, -0.33699593f,
    -0.30180866f, -0.27321684f
};
static const float dq_seq_c[7] = {
    -1.2f, -1.2f, -0.8f, -0.7f, -0.7f, -0.4f, -0.5f
};
static const float dct4x8_params[3][4] = {
    {2198.0505f, -0.96269625f, -0.7619425f, -0.65511405f},
    {764.36554f, -0.926302f, -0.967523f, -0.2784529f},
    {527.10754f, -1.4594386f, -1.4500821f, -1.5843723f}
};
static const float dct4_params[3][4] = {
    {2200.0f, 0.0f, 0.0f, 0.0f},
    {392.0f, 0.0f, 0.0f, 0.0f},
    {112.0f, -0.25f, -0.25f, -0.5f}
};

typedef struct {
    float v[32];
    uint32_t n;
} jxl_dq_params;

typedef struct jxl_dq_encoding {
    int mode;
    jxl_dq_params dct[3];
    jxl_dq_params dct4x4[3];
    float fixed[3][9];
    float denominator;
    int32_t *raw[3];
    uint32_t raw_w, raw_h;
} jxl_dq_encoding;

static void dq_seq(jxl_dq_encoding *e, float a, float b, float c) {
    int i;
    e->mode = 0;
    e->dct[0].n = e->dct[1].n = e->dct[2].n = 8;
    e->dct[0].v[0] = a;
    e->dct[1].v[0] = b;
    e->dct[2].v[0] = c;
    for (i = 0; i < 7; i++) {
        e->dct[0].v[i + 1] = dq_seq_a[i];
        e->dct[1].v[i + 1] = dq_seq_b[i];
        e->dct[2].v[i + 1] = dq_seq_c[i];
    }
}

static void dq_set_dct(jxl_dq_encoding *e, const float *c0, uint32_t n0,
                       const float *c1, uint32_t n1, const float *c2,
                       uint32_t n2) {
    e->mode = 0;
    memcpy(e->dct[0].v, c0, n0 * sizeof(float)); e->dct[0].n = n0;
    memcpy(e->dct[1].v, c1, n1 * sizeof(float)); e->dct[1].n = n1;
    memcpy(e->dct[2].v, c2, n2 * sizeof(float)); e->dct[2].n = n2;
}

static void dq_default(jxl_dq_encoding *e, int tr) {
    static const float d8_0[6] = {3150.0f, 0.0f, -0.4f, -0.4f, -0.4f, -2.0f};
    static const float d8_1[6] = {560.0f, 0.0f, -0.3f, -0.3f, -0.3f, -0.3f};
    static const float d8_2[6] = {512.0f, -2.0f, -1.0f, 0.0f, -1.0f, -2.0f};
    static const float d16_0[7] = {8996.873f, -1.3000778f, -0.4942453f,
                                   -0.43909377f, -0.6350102f, -0.9017726f,
                                   -1.6162099f};
    static const float d16_1[7] = {3191.4836f, -0.67424583f, -0.80745816f,
                                   -0.4492584f, -0.3586544f, -0.3132239f,
                                   -0.37615025f};
    static const float d16_2[7] = {1157.504f, -2.0531423f, -1.4f, -0.5068713f,
                                   -0.4270873f, -1.4856834f, -4.920914f};
    static const float d32_0[8] = {15718.408f, -1.025f, -0.98f, -0.9012f,
                                   -0.4f, -0.48819396f, -0.421064f, -0.27f};
    static const float d32_1[8] = {7305.7637f, -0.8041958f, -0.76330364f,
                                   -0.5566038f, -0.49785304f, -0.43699592f,
                                   -0.40180868f, -0.27321684f};
    static const float d32_2[8] = {3803.5317f, -3.0607336f, -2.041327f,
                                   -2.023565f, -0.54953897f, -0.4f, -0.4f,
                                   -0.3f};
    static const float d816_0[7] = {7240.7734f, -0.7f, -0.7f, -0.2f, -0.2f,
                                    -0.2f, -0.5f};
    static const float d816_1[7] = {1448.1547f, -0.5f, -0.5f, -0.5f, -0.2f,
                                    -0.2f, -0.2f};
    static const float d816_2[7] = {506.85413f, -1.4f, -0.2f, -0.5f, -0.5f,
                                    -1.5f, -3.6f};
    static const float d832_0[8] = {16283.249f, -1.7812846f, -1.6309059f,
                                    -1.0382179f, -0.85f, -0.7f, -0.9f,
                                    -1.2360638f};
    static const float d832_1[8] = {5089.1577f, -0.3200494f, -0.3536285f,
                                    -0.3034f, -0.61f, -0.5f, -0.5f, -0.6f};
    static const float d832_2[8] = {3397.7761f, -0.32132736f, -0.3450762f,
                                    -0.7034f, -0.9f, -1.0f, -1.0f, -1.1754606f};
    static const float d1632_0[8] = {13844.971f, -0.971138f, -0.658f, -0.42026f,
                                     -0.22712f, -0.2206f, -0.226f, -0.6f};
    static const float d1632_1[8] = {4798.964f, -0.6112531f, -0.8377079f,
                                     -0.7901486f, -0.26927274f, -0.38272768f,
                                     -0.22924222f, -0.20719099f};
    static const float d1632_2[8] = {1807.2369f, -1.2f, -1.2f, -0.7f, -0.7f,
                                     -0.7f, -0.4f, -0.5f};
    int c, i;

    memset(e, 0, sizeof(*e));
    switch (tr) {
        case JXL_TR_DCT8: dq_set_dct(e, d8_0, 6, d8_1, 6, d8_2, 6); break;
        case JXL_TR_HORNUSS:
            e->mode = 1;
            e->fixed[0][0] = 280.0f; e->fixed[0][1] = 3160.0f; e->fixed[0][2] = 3160.0f;
            e->fixed[1][0] = 60.0f;  e->fixed[1][1] = 864.0f;  e->fixed[1][2] = 864.0f;
            e->fixed[2][0] = 18.0f;  e->fixed[2][1] = 200.0f;  e->fixed[2][2] = 200.0f;
            break;
        case JXL_TR_DCT2: {
            static const float p[3][6] = {
                {3840.0f, 2560.0f, 1280.0f, 640.0f, 480.0f, 300.0f},
                {960.0f, 640.0f, 320.0f, 180.0f, 140.0f, 120.0f},
                {640.0f, 320.0f, 128.0f, 64.0f, 32.0f, 16.0f}
            };
            e->mode = 2;
            for (c = 0; c < 3; c++) for (i = 0; i < 6; i++) e->fixed[c][i] = p[c][i];
            break;
        }
        case JXL_TR_DCT4:
            e->mode = 3;
            for (c = 0; c < 3; c++) {
                e->fixed[c][0] = 1.0f;
                e->fixed[c][1] = 1.0f;
                memcpy(e->dct[c].v, dct4_params[c], 4 * sizeof(float));
                e->dct[c].n = 4;
            }
            break;
        case JXL_TR_DCT16: dq_set_dct(e, d16_0, 7, d16_1, 7, d16_2, 7); break;
        case JXL_TR_DCT32: dq_set_dct(e, d32_0, 8, d32_1, 8, d32_2, 8); break;
        case JXL_TR_DCT16X8:
        case JXL_TR_DCT8X16: dq_set_dct(e, d816_0, 7, d816_1, 7, d816_2, 7); break;
        case JXL_TR_DCT32X8:
        case JXL_TR_DCT8X32: dq_set_dct(e, d832_0, 8, d832_1, 8, d832_2, 8); break;
        case JXL_TR_DCT32X16:
        case JXL_TR_DCT16X32: dq_set_dct(e, d1632_0, 8, d1632_1, 8, d1632_2, 8); break;
        case JXL_TR_DCT4X8:
        case JXL_TR_DCT8X4:
            e->mode = 4;
            for (c = 0; c < 3; c++) {
                e->fixed[c][0] = 1.0f;
                memcpy(e->dct[c].v, dct4x8_params[c], 4 * sizeof(float));
                e->dct[c].n = 4;
            }
            break;
        case JXL_TR_AFV0: case JXL_TR_AFV1: case JXL_TR_AFV2: case JXL_TR_AFV3: {
            static const float p[3][9] = {
                {3072.0f, 3072.0f, 256.0f, 256.0f, 256.0f, 414.0f, 0.0f, 0.0f, 0.0f},
                {1024.0f, 1024.0f, 50.0f, 50.0f, 50.0f, 58.0f, 0.0f, 0.0f, 0.0f},
                {384.0f, 384.0f, 12.0f, 12.0f, 12.0f, 22.0f, -0.25f, -0.25f, -0.25f}
            };
            e->mode = 5;
            for (c = 0; c < 3; c++) {
                for (i = 0; i < 9; i++) e->fixed[c][i] = p[c][i];
                memcpy(e->dct[c].v, dct4x8_params[c], 4 * sizeof(float));
                e->dct[c].n = 4;
                memcpy(e->dct4x4[c].v, dct4_params[c], 4 * sizeof(float));
                e->dct4x4[c].n = 4;
            }
            break;
        }
        case JXL_TR_DCT64: dq_seq(e, 23966.166f, 8380.191f, 4493.024f); break;
        case JXL_TR_DCT32X64:
        case JXL_TR_DCT64X32: dq_seq(e, 15358.898f, 5597.3604f, 2919.9617f); break;
        case JXL_TR_DCT128: dq_seq(e, 47932.332f, 16760.383f, 8986.048f); break;
        case JXL_TR_DCT64X128:
        case JXL_TR_DCT128X64: dq_seq(e, 30717.797f, 11194.721f, 5839.9233f); break;
        case JXL_TR_DCT256: dq_seq(e, 95864.664f, 33520.766f, 17972.096f); break;
        default: dq_seq(e, 61435.594f, 24209.441f, 12979.847f); break;
    }
}

static float dq_interpolate(float pos, float max, const float *bands, int len) {
    float scaled_pos, frac;
    int idx;
    float a, b;
    if (len == 1) return bands[0];
    scaled_pos = pos * (float)(len - 1) / max;
    idx = (int)scaled_pos;
    if (idx < 0) idx = 0;
    if (idx >= len - 1) idx = len - 2;
    frac = scaled_pos - (float)idx;
    a = bands[idx];
    b = bands[idx + 1];
    return a * powf(b / a, frac);
}

static float dq_mult(float x) {
    return x > 0.0f ? 1.0f + x : 1.0f / (1.0f - x);
}

static int dct_quant_weights(const float *params, uint32_t n, uint32_t width,
                             uint32_t height, float *out) {
    float bands[32];
    uint32_t i, x, y;
    float last = params[0];
    bands[0] = last;
    for (i = 1; i < n; i++) {
        float band = last * dq_mult(params[i]);
        if (!(band > 0.0f)) return -1;
        bands[i] = band;
        last = band;
    }
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            float dx = (float)x / (float)(width - 1);
            float dy = (float)y / (float)(height - 1);
            float distance = sqrtf(dx * dx + dy * dy);
            out[y * width + x] =
                dq_interpolate(distance, 1.4142135623730951f + 1e-6f, bands, (int)n);
        }
    }
    return 0;
}

static const float afv_freqs[16] = {
    0.0f, 0.0f, 0.8517779f, 5.3777843f, 0.0f, 0.0f, 4.734748f, 5.4492455f,
    1.659827f, 4.0f, 7.275749f, 10.423227f, 2.6629324f, 7.6306577f, 8.962389f,
    12.971662f
};

static int dq_into_matrix(jxl_ctx *ctx, struct jxl_dq_encoding *e, int tr,
                          float *out[3]) {
    uint32_t width, height, n;
    int c;
    uint32_t i, x, y;

    jxl_tr_matrix_size(tr, &width, &height);
    n = width * height;
    for (c = 0; c < 3; c++) {
        out[c] = (float *)jxl_calloc(ctx, n, sizeof(float));
        if (!out[c]) return -1;
    }

    switch (e->mode) {
        case 0:
            for (c = 0; c < 3; c++) {
                if (dct_quant_weights(e->dct[c].v, e->dct[c].n, width, height,
                                      out[c]) != 0)
                    return -1;
            }
            break;
        case 1:
            for (c = 0; c < 3; c++) {
                for (i = 0; i < 64; i++) out[c][i] = e->fixed[c][0];
                out[c][0] = 1.0f;
                out[c][1] = e->fixed[c][1];
                out[c][8] = e->fixed[c][1];
                out[c][9] = e->fixed[c][2];
            }
            break;
        case 2:
            for (c = 0; c < 3; c++) {
                out[c][0] = 1.0f;
                for (i = 0; i < 6; i++) {
                    uint32_t shift = i / 2;
                    uint32_t dim = 1u << shift;
                    float val = e->fixed[c][i];
                    if (i % 2 == 0) {
                        for (y = 0; y < dim; y++) {
                            for (x = dim; x < dim * 2; x++) {
                                out[c][y * 8 + x] = val;
                                out[c][x * 8 + y] = val;
                            }
                        }
                    } else {
                        for (y = dim; y < dim * 2; y++) {
                            for (x = dim; x < dim * 2; x++) out[c][y * 8 + x] = val;
                        }
                    }
                }
            }
            break;
        case 3:
            for (c = 0; c < 3; c++) {
                float mat[16];
                if (dct_quant_weights(e->dct[c].v, e->dct[c].n, 4, 4, mat) != 0)
                    return -1;
                for (y = 0; y < 4; y++) {
                    for (x = 0; x < 4; x++) {
                        float v = mat[y * 4 + x];
                        out[c][y * 16 + x * 2] = v;
                        out[c][y * 16 + x * 2 + 1] = v;
                        out[c][(y * 2 + 1) * 8 + x * 2] = v;
                        out[c][(y * 2 + 1) * 8 + x * 2 + 1] = v;
                    }
                }
                out[c][1] /= e->fixed[c][0];
                out[c][8] /= e->fixed[c][0];
                out[c][9] /= e->fixed[c][1];
            }
            break;
        case 4:
            for (c = 0; c < 3; c++) {
                float mat[32];
                if (dct_quant_weights(e->dct[c].v, e->dct[c].n, 8, 4, mat) != 0)
                    return -1;
                for (y = 0; y < 4; y++) {
                    memcpy(out[c] + (y * 2) * 8, mat + y * 8, 8 * sizeof(float));
                    memcpy(out[c] + (y * 2 + 1) * 8, mat + y * 8, 8 * sizeof(float));
                }
                out[c][8] /= e->fixed[c][0];
            }
            break;
        case 5:
            for (c = 0; c < 3; c++) {
                float w48[32], w44[16], bands[4];
                float prev;
                if (dct_quant_weights(e->dct[c].v, e->dct[c].n, 8, 4, w48) != 0)
                    return -1;
                if (dct_quant_weights(e->dct4x4[c].v, e->dct4x4[c].n, 4, 4, w44) != 0)
                    return -1;
                bands[0] = e->fixed[c][5];
                prev = bands[0];
                for (i = 1; i < 4; i++) {
                    bands[i] = prev * dq_mult(e->fixed[c][5 + i]);
                    prev = bands[i];
                }
                for (y = 0; y < 4; y++) {
                    for (x = 0; x < 4; x++) {
                        float v;
                        if (x == 0 && y == 0) v = 1.0f;
                        else if (x == 0 && y == 1) v = e->fixed[c][2];
                        else if (x == 1 && y == 0) v = e->fixed[c][3];
                        else if (x == 1 && y == 1) v = e->fixed[c][4];
                        else {
                            v = dq_interpolate(afv_freqs[y * 4 + x] - afv_freqs[2],
                                               afv_freqs[15] - afv_freqs[2] + 1e-6f,
                                               bands, 4);
                        }
                        out[c][16 * y + 2 * x] = v;
                    }
                }
                for (y = 0; y < 4; y++) {
                    float *row0 = out[c] + y * 16;
                    float *row1 = row0 + 8;
                    for (x = 0; x < 8; x++) {
                        row1[x] = (y == 0 && x == 0) ? e->fixed[c][0] : w48[y * 8 + x];
                    }
                    for (x = 0; x < 4; x++) {
                        row0[x * 2 + 1] =
                            (y == 0 && x == 0) ? e->fixed[c][1] : w44[y * 4 + x];
                    }
                }
            }
            break;
        default:
            for (c = 0; c < 3; c++) {
                for (i = 0; i < n; i++) out[c][i] = (float)e->raw[c][i] * e->denominator;
            }
            break;
    }

    if (e->mode != 7) {
        for (c = 0; c < 3; c++) {
            for (i = 0; i < n; i++) out[c][i] = 1.0f / out[c][i];
        }
    }
    for (c = 0; c < 3; c++) {
        for (i = 0; i < n; i++) {
            if (!(out[c][i] < 1e8f) || !(out[c][i] > 0.0f)) {
                JXL_ERR(ctx, "vardct: bad dequant matrix element");
                return -1;
            }
        }
    }
    return 0;
}

static int read_dct_params(jxl_ctx *ctx, jxl_br *br, jxl_dq_params p[3]) {
    uint32_t n = jxl_br_read(br, 4) + 1;
    uint32_t c, i;
    if (n > 32) {
        JXL_ERR(ctx, "vardct: too many dct params");
        return -1;
    }
    for (c = 0; c < 3; c++) p[c].n = n;
    for (c = 0; c < 3; c++) {
        for (i = 0; i < n; i++) p[c].v[i] = jxl_br_f16(br);
    }
    for (c = 0; c < 3; c++) p[c].v[0] *= 64.0f;
    return 0;
}

static void read_fixed(jxl_br *br, float out[3][9], int n) {
    int c, i;
    for (c = 0; c < 3; c++) {
        for (i = 0; i < n; i++) out[c][i] = jxl_br_f16(br);
    }
}

static const uint8_t dq_slot_tr[17] = {
    JXL_TR_DCT8, JXL_TR_HORNUSS, JXL_TR_DCT2, JXL_TR_DCT4, JXL_TR_DCT16,
    JXL_TR_DCT32, JXL_TR_DCT8X16, JXL_TR_DCT8X32, JXL_TR_DCT16X32,
    JXL_TR_DCT4X8, JXL_TR_AFV0, JXL_TR_DCT64, JXL_TR_DCT32X64, JXL_TR_DCT128,
    JXL_TR_DCT64X128, JXL_TR_DCT256, JXL_TR_DCT128X256
};

int jxl_dequant_matrices_read(jxl_ctx *ctx, jxl_br *br,
                              jxl_dequant_matrices *dm, uint32_t bit_depth,
                              uint32_t num_lf_groups,
                              jxl_ma_config *global_ma) {
    int all_default;
    uint32_t slot;
    int rc = -1;
    jxl_dq_encoding *e;
    jxl_modular mod;
    jxl_chanlist cl;

    memset(dm, 0, sizeof(*dm));
    dm->enc = (struct jxl_dq_encoding *)jxl_calloc(ctx, 17, sizeof(*dm->enc));
    if (!dm->enc) return -1;
    all_default = jxl_br_bool(br);

    for (slot = 0; slot < 17; slot++) {
        int tr = dq_slot_tr[slot];
        uint32_t mw, mh;
        int c;

        memset(&mod, 0, sizeof(mod));
        memset(&cl, 0, sizeof(cl));
        jxl_tr_matrix_size(tr, &mw, &mh);
        e = &dm->enc[slot];

        if (all_default) {
            dq_default(e, tr);
        } else {
            uint32_t mode = jxl_br_read(br, 3);
            int midx = jxl_tr_matrix_index(tr);
            if (mode >= 1 && mode <= 5 &&
                !(midx == 0 || midx == 1 || midx == 2 || midx == 3 ||
                  midx == 9 || midx == 10)) {
                JXL_ERR(ctx, "vardct: invalid dequant encoding mode");
                goto done;
            }
            memset(e, 0, sizeof(*e));
            switch (mode) {
                case 0: dq_default(e, tr); break;
                case 1: e->mode = 1; read_fixed(br, e->fixed, 3); break;
                case 2: e->mode = 2; read_fixed(br, e->fixed, 6); break;
                case 3:
                    e->mode = 3;
                    read_fixed(br, e->fixed, 2);
                    if (read_dct_params(ctx, br, e->dct) != 0) goto done;
                    break;
                case 4:
                    e->mode = 4;
                    read_fixed(br, e->fixed, 1);
                    if (read_dct_params(ctx, br, e->dct) != 0) goto done;
                    break;
                case 5: {
                    int ci, i;
                    e->mode = 5;
                    read_fixed(br, e->fixed, 9);
                    for (ci = 0; ci < 3; ci++) {
                        for (i = 0; i < 6; i++) e->fixed[ci][i] *= 64.0f;
                    }
                    if (read_dct_params(ctx, br, e->dct) != 0) goto done;
                    if (read_dct_params(ctx, br, e->dct4x4) != 0) goto done;
                    break;
                }
                case 6:
                    e->mode = 0;
                    if (read_dct_params(ctx, br, e->dct) != 0) goto done;
                    break;
                default: {
                    jxl_mchan_spec specs[3];
                    int k;
                    e->mode = 7;
                    e->denominator = jxl_br_f16(br);
                    for (k = 0; k < 3; k++) {
                        specs[k].w = mw;
                        specs[k].h = mh;
                        specs[k].hshift = 0;
                        specs[k].vshift = 0;
                    }
                    if (jxl_modular_init(ctx, &mod, br, specs, 3, global_ma, 256,
                                         bit_depth) != 0)
                        goto done;
                    if (jxl_modular_transform_channels(ctx, &mod, &cl) != 0) goto done;
                    if (jxl_modular_decode(ctx, &mod, &cl, br,
                                           1 + num_lf_groups * 3 + slot) != 0)
                        goto done;
                    if (jxl_modular_inverse(ctx, &mod, &cl) != 0) goto done;
                    for (k = 0; k < 3; k++) e->raw[k] = mod.base[k].data;
                    e->raw_w = mw;
                    e->raw_h = mh;
                    break;
                }
            }
        }

        if (e->mode == 7) {
            if (jxl_dequant_matrices_ensure(ctx, dm, tr) != 0) goto done;
        }
        (void)c;
        jxl_chanlist_free(ctx, &cl);
        jxl_modular_free(ctx, &mod);
    }
    rc = 0;

done:
    jxl_chanlist_free(ctx, &cl);
    jxl_modular_free(ctx, &mod);
    if (rc != 0) jxl_dequant_matrices_free(ctx, dm);
    return rc;
}

int jxl_dequant_matrices_ensure(jxl_ctx *ctx, jxl_dequant_matrices *dm,
                                int tr) {
    int slot = jxl_tr_matrix_index(tr);
    uint32_t mw, mh, i, x, y;
    int c;

    if (slot < 0 || slot >= 17 || !dm->enc) return -1;
    if (dm->matrix[slot][0]) return 0;

    jxl_tr_matrix_size(tr, &mw, &mh);
    if (dq_into_matrix(ctx, &dm->enc[slot], tr, dm->matrix[slot]) != 0) return -1;

    for (c = 0; c < 3; c++) {
        dm->matrix_tr[slot][c] =
            (float *)jxl_calloc(ctx, (size_t)mw * mh, sizeof(float));
        if (!dm->matrix_tr[slot][c]) return -1;
        for (i = 0; i < mw * mh; i++) {
            x = i % mh;
            y = i / mh;
            dm->matrix_tr[slot][c][i] = dm->matrix[slot][c][x * mw + y];
        }
    }
    return 0;
}

void jxl_dequant_matrices_free(jxl_ctx *ctx, jxl_dequant_matrices *dm) {
    int i, c;
    for (i = 0; i < 17; i++) {
        for (c = 0; c < 3; c++) {
            jxl_free(ctx, dm->matrix[i][c]);
            jxl_free(ctx, dm->matrix_tr[i][c]);
            dm->matrix[i][c] = NULL;
            dm->matrix_tr[i][c] = NULL;
        }
    }
    jxl_free(ctx, dm->enc);
    dm->enc = NULL;
}

int jxl_hf_pass_read(jxl_ctx *ctx, jxl_br *br, jxl_hf_pass *hp,
                     const jxl_hf_block_ctx *bc, uint32_t num_hf_presets,
                     jxl_natural_orders *no) {
    uint32_t used_orders;
    jxl_dec perm_dec;
    int have_perm = 0;
    uint32_t idx;
    int rc = -1;

    memset(hp, 0, sizeof(*hp));
    memset(&perm_dec, 0, sizeof(perm_dec));

    used_orders = jxl_br_u32(br, 0x5F, 0, 0x13, 0, 0x00, 0, 0, 13);
    if (used_orders != 0) {
        if (jxl_dec_init(ctx, &perm_dec, br, 8) != 0) return -1;
        jxl_dec_begin(&perm_dec, br);
        have_perm = 1;
    }

    for (idx = 0; idx < 13; idx++) {
        if (have_perm && (used_orders & 1)) {
            uint32_t bw = order_block_sizes[idx][0];
            uint32_t bh = order_block_sizes[idx][1];
            uint32_t size = bw * bh;
            uint32_t skip = size / 64;
            const uint16_t *nat = jxl_natural_order(ctx, no, (int)idx);
            uint32_t *perm;
            int c;
            if (!nat) goto done;
            perm = (uint32_t *)jxl_calloc(ctx, size, sizeof(uint32_t));
            if (!perm) goto done;
            for (c = 0; c < 3; c++) {
                uint32_t i;
                if (jxl_read_permutation(ctx, &perm_dec, br, size, skip, perm) != 0) {
                    jxl_free(ctx, perm);
                    goto done;
                }
                hp->order[idx][c] =
                    (uint16_t *)jxl_calloc(ctx, (size_t)size * 2, sizeof(uint16_t));
                if (!hp->order[idx][c]) {
                    jxl_free(ctx, perm);
                    goto done;
                }
                for (i = 0; i < size; i++) {
                    hp->order[idx][c][i * 2] = nat[perm[i] * 2];
                    hp->order[idx][c][i * 2 + 1] = nat[perm[i] * 2 + 1];
                }
            }
            jxl_free(ctx, perm);
        }
        used_orders >>= 1;
    }
    if (have_perm && jxl_dec_finalize(&perm_dec) != 0) {
        JXL_ERR(ctx, "vardct: bad coefficient order stream");
        goto done;
    }

    if (jxl_dec_init(ctx, &hp->dist, br,
                     495 * num_hf_presets * bc->num_block_clusters) != 0)
        goto done;
    hp->have_dist = 1;
    rc = 0;

done:
    jxl_dec_free(&perm_dec);
    if (rc != 0) jxl_hf_pass_free(ctx, hp);
    return rc;
}

void jxl_hf_pass_free(jxl_ctx *ctx, jxl_hf_pass *hp) {
    int i, c;
    for (i = 0; i < 13; i++) {
        for (c = 0; c < 3; c++) {
            jxl_free(ctx, hp->order[i][c]);
            hp->order[i][c] = NULL;
        }
    }
    if (hp->have_dist) jxl_dec_free(&hp->dist);
    hp->have_dist = 0;
}

static const uint32_t coeff_freq_context[63] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 15, 16, 16, 17, 17,
    18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 23, 23, 23, 24, 24, 24, 24, 25,
    25, 25, 25, 26, 26, 26, 26, 27, 27, 27, 27, 28, 28, 28, 28, 29, 29, 29, 29,
    30, 30, 30, 30
};
static const uint32_t coeff_num_nonzero_context[63] = {
    0, 31, 62, 62, 93, 93, 93, 93, 123, 123, 123, 123, 152, 152, 152, 152, 152,
    152, 152, 152, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180,
    206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206,
    206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206,
    206
};

void jxl_jpeg_upsampling_shifts(const uint32_t ju[3], int idx, int *hs,
                                int *vs) {
    int hscale = (ju[0] == 1 || ju[0] == 2) || (ju[1] == 1 || ju[1] == 2) ||
                 (ju[2] == 1 || ju[2] == 2);
    int vscale = (ju[0] == 1 || ju[0] == 3) || (ju[1] == 1 || ju[1] == 3) ||
                 (ju[2] == 1 || ju[2] == 3);
    switch (ju[idx]) {
        case 0: *hs = hscale; *vs = vscale; break;
        case 1: *hs = 0; *vs = 0; break;
        case 2: *hs = 0; *vs = vscale; break;
        default: *hs = hscale; *vs = 0; break;
    }
}

int jxl_write_hf_coeff(jxl_ctx *ctx, jxl_br *br,
                       const jxl_hf_coeff_params *p, float *out[3],
                       size_t stride[3]) {
    const jxl_hf_block_ctx *bc = p->bc;
    jxl_dec *dist = &p->pass->dist;
    uint32_t lf_idx_mul = (bc->nlf[0] + 1) * (bc->nlf[1] + 1) * (bc->nlf[2] + 1);
    uint32_t hf_idx_mul = bc->nqf + 1;
    int hshifts[3], vshifts[3];
    uint32_t ctx_size, cluster_base;
    uint32_t hfp_bits, hfp;
    uint32_t *nz_all = NULL;
    uint32_t *nz_row[3];
    uint32_t nz_len[3];
    uint32_t order0_offsets[3][64];
    uint8_t order0_ready[3] = {0, 0, 0};
    uint32_t x, y;
    int c, rc = -1;

    for (c = 0; c < 3; c++) {
        jxl_jpeg_upsampling_shifts(p->jpeg_upsampling, c, &hshifts[c], &vshifts[c]);
    }

    hfp_bits = jxl_bitlen(p->num_hf_presets > 1 ? p->num_hf_presets - 1 : 0);
    hfp = jxl_br_read(br, (int)hfp_bits);
    if (hfp >= p->num_hf_presets) {
        JXL_ERR(ctx, "vardct: HF preset out of range");
        return -1;
    }
    ctx_size = 495 * bc->num_block_clusters;
    cluster_base = ctx_size * hfp;
    if (cluster_base + ctx_size > dist->num_dist) {
        JXL_ERR(ctx, "vardct: HF cluster map too small");
        return -1;
    }

    jxl_dec_begin(dist, br);

    {
        size_t nz_total = 0;
        for (c = 0; c < 3; c++) {
            nz_len[c] =
                (p->bi_w + ((1u << hshifts[c]) - 1)) >> hshifts[c];
            nz_total += nz_len[c] ? nz_len[c] : 1;
        }
        nz_all = (uint32_t *)jxl_calloc(ctx, nz_total, sizeof(uint32_t));
        if (!nz_all) goto done;
        nz_row[0] = nz_all;
        for (c = 1; c < 3; c++) {
            nz_row[c] = nz_row[c - 1] + (nz_len[c - 1] ? nz_len[c - 1] : 1);
        }
    }

    for (y = 0; y < p->bi_h; y++) {
        for (x = 0; x < p->bi_w; x++) {
            jxl_block_info *bi =
                &p->block_info[(size_t)y * p->bi_stride + x];
            uint32_t bw8, bh8, num_blocks, num_blocks_log, order_id;
            uint32_t lf_idx = 0, hf_idx = 0;
            int32_t qf;
            int ci, need_transpose;

            if (bi->dct_select >= JXL_TR_COUNT) continue;
            qf = bi->hf_mul;
            jxl_tr_select_size(bi->dct_select, &bw8, &bh8);
            num_blocks = bw8 * bh8;
            num_blocks_log = jxl_bitlen(num_blocks) - 1;
            order_id = (uint32_t)jxl_tr_order_id(bi->dct_select);
            need_transpose = jxl_tr_need_transpose(bi->dct_select);

            if (p->lf_quant[0]) {
                static const int order[3] = {0, 2, 1};
                int k;
                for (k = 0; k < 3; k++) {
                    int cc = order[k];
                    uint32_t i;
                    uint32_t sx = x >> hshifts[cc];
                    uint32_t sy = y >> vshifts[cc];
                    int32_t q;
                    lf_idx *= bc->nlf[cc] + 1;
                    if (sx >= p->lf_quant[cc]->w || sy >= p->lf_quant[cc]->h) continue;
                    q = p->lf_quant[cc]->data[(size_t)sy * p->lf_quant[cc]->stride + sx];
                    for (i = 0; i < bc->nlf[cc]; i++) {
                        if (q > bc->lf_thresholds[cc][i]) lf_idx++;
                    }
                }
            }
            {
                uint32_t i;
                for (i = 0; i < bc->nqf; i++) {
                    if (qf > (int32_t)bc->qf_thresholds[i]) hf_idx++;
                }
            }

            for (ci = 0; ci < 3; ci++) {
                uint32_t ch_idx = (uint32_t)ci * 13 + order_id;
                int cc = (ci == 0) ? 1 : (ci == 1 ? 0 : 2);
                int hshift = hshifts[cc], vshift = vshifts[cc];
                uint32_t sx = x >> hshift, sy = y >> vshift;
                uint32_t idx, block_ctx, nz_ctx, predicted;
                uint32_t non_zeros, non_zeros_val;
                uint32_t dx8;
                uint32_t is_prev_nonzero;
                const uint16_t *ord;
                const uint32_t *coeff_offsets = NULL;
                int32_t *coeff_out = NULL;
                size_t coeff_stride = 0;
                uint32_t coeff_ctx_base;
                uint32_t k;

                if (hshift != 0 || vshift != 0) {
                    if ((sx << hshift) != x || (sy << vshift) != y) continue;
                    if (p->block_info[(size_t)sy * p->bi_stride + sx].dct_select >=
                        JXL_TR_COUNT)
                        continue;
                }

                idx = (ch_idx * hf_idx_mul + hf_idx) * lf_idx_mul + lf_idx;
                if (idx >= bc->block_ctx_map_len) {
                    JXL_ERR(ctx, "vardct: block context out of range");
                    goto done;
                }
                block_ctx = bc->block_ctx_map[idx];

                if (sy == 0) {
                    predicted = (sx == 0) ? 32 : nz_row[cc][sx - 1];
                } else if (sx == 0) {
                    predicted = nz_row[cc][sx];
                } else {
                    predicted = (nz_row[cc][sx] + nz_row[cc][sx - 1] + 1) >> 1;
                }
                nz_ctx = block_ctx +
                         (predicted >= 8 ? 4 + predicted / 2 : predicted) *
                             bc->num_block_clusters;

                if (dist->lz77_enabled) {
                    non_zeros = jxl_dec_read_clustered(
                        dist, br, dist->clusters[cluster_base + nz_ctx], 0);
                } else {
                    non_zeros = jxl_dec_read_clustered_no_lz77(
                        dist, br, dist->clusters[cluster_base + nz_ctx]);
                }
                if (non_zeros > (63u << num_blocks_log)) {
                    JXL_ERR(ctx, "vardct: non_zeros too large");
                    goto done;
                }
                non_zeros_val = (non_zeros + num_blocks - 1) >> num_blocks_log;
                for (dx8 = 0; dx8 < bw8 && sx + dx8 < nz_len[cc]; dx8++) {
                    nz_row[cc][sx + dx8] = non_zeros_val;
                }
                if (non_zeros == 0) continue;
                bi->hf_nonzero_mask |= (uint8_t)(1u << cc);

                is_prev_nonzero = (non_zeros <= num_blocks * 4) ? 1 : 0;
                ord = NULL;
                if (!(p->discard_mask & (1u << cc))) {
                    ord = p->pass->order[order_id][cc];
                    if (!ord) ord = jxl_natural_order(ctx, p->no, (int)order_id);
                    if (!ord) goto done;
                    coeff_stride = stride[cc];
                    coeff_out = (int32_t *)out[cc] +
                        (size_t)(sy * 8) * coeff_stride + sx * 8;
                    if (num_blocks == 1 && order_id == 0) {
                        if (!order0_ready[cc]) {
                            uint32_t oi;
                            for (oi = 0; oi < 64; oi++) {
                                uint32_t oxy, odx, ody;
                                memcpy(&oxy, ord + oi * 2, sizeof(oxy));
                                odx = oxy & 0xffffu;
                                ody = oxy >> 16;
                                if (need_transpose) {
                                    uint32_t t = odx; odx = ody; ody = t;
                                }
                                order0_offsets[cc][oi] =
                                    (uint32_t)(ody * coeff_stride + odx);
                            }
                            order0_ready[cc] = 1;
                        }
                        coeff_offsets = order0_offsets[cc];
                    }
                }

                coeff_ctx_base = block_ctx * 458 + 37 * bc->num_block_clusters;
                if (coeff_offsets && p->coeff_shift == 0 &&
                    !dist->lz77_enabled) {
                    for (k = 1; k < 64; k++) {
                        uint32_t nzi = non_zeros - 1;
                        uint32_t fi = k - 1;
                        uint32_t coeff_ctx =
                            (coeff_num_nonzero_context[nzi] +
                             coeff_freq_context[fi]) * 2 + is_prev_nonzero;
                        uint32_t ucoeff;
                        int32_t coeff;

                        if (coeff_ctx >= 458) {
                            JXL_ERR(ctx, "vardct: too many zeros in varblock");
                            goto done;
                        }
                        ucoeff = jxl_dec_read_clustered_no_lz77(
                            dist, br,
                            dist->clusters[cluster_base + coeff_ctx_base +
                                           coeff_ctx]);

                        coeff = (int32_t)((ucoeff >> 1) ^
                                          (((~ucoeff) & 1u) - 1u));
                        coeff_out[coeff_offsets[k]] += coeff;
                        is_prev_nonzero = ucoeff != 0;
                        non_zeros -= is_prev_nonzero;
                        if (non_zeros == 0) break;
                    }
                } else if (coeff_offsets) {
                    for (k = 1; k < 64; k++) {
                        uint32_t nzi = non_zeros - 1;
                        uint32_t fi = k - 1;
                        uint32_t coeff_ctx =
                            (coeff_num_nonzero_context[nzi] +
                             coeff_freq_context[fi]) * 2 + is_prev_nonzero;
                        uint32_t ucoeff;
                        int32_t coeff;

                        if (coeff_ctx >= 458) {
                            JXL_ERR(ctx, "vardct: too many zeros in varblock");
                            goto done;
                        }
                        ucoeff = jxl_dec_read_clustered(
                            dist, br,
                            dist->clusters[cluster_base + coeff_ctx_base +
                                           coeff_ctx],
                            0);
                        if (ucoeff == 0) {
                            is_prev_nonzero = 0;
                            continue;
                        }
                        coeff = jxl_unpack_signed(ucoeff) << p->coeff_shift;
                        coeff_out[coeff_offsets[k]] += coeff;
                        is_prev_nonzero = 1;
                        non_zeros--;
                        if (non_zeros == 0) break;
                    }
                } else if (num_blocks == 1) {

                    for (k = 1; k < 64; k++) {
                        uint32_t nzi = non_zeros - 1;
                        uint32_t fi = k - 1;
                        uint32_t coeff_ctx =
                            (coeff_num_nonzero_context[nzi] +
                             coeff_freq_context[fi]) * 2 + is_prev_nonzero;
                        uint32_t ucoeff;
                        int32_t coeff;
                        uint32_t dx, dy, xy;

                        if (coeff_ctx >= 458) {
                            JXL_ERR(ctx, "vardct: too many zeros in varblock");
                            goto done;
                        }
                        ucoeff = jxl_dec_read_clustered(
                            dist, br,
                            dist->clusters[cluster_base + coeff_ctx_base +
                                           coeff_ctx],
                            0);
                        if (ucoeff == 0) {
                            is_prev_nonzero = 0;
                            continue;
                        }
                        coeff = jxl_unpack_signed(ucoeff) << p->coeff_shift;
                        if (ord) {
                            memcpy(&xy, ord + k * 2, sizeof(xy));
                            dx = xy & 0xffffu;
                            dy = xy >> 16;
                            if (need_transpose) {
                                uint32_t t = dx; dx = dy; dy = t;
                            }
                            coeff_out[(size_t)dy * coeff_stride + dx] += coeff;
                        }
                        is_prev_nonzero = 1;
                        non_zeros--;
                        if (non_zeros == 0) break;
                    }
                } else {
                    for (k = num_blocks; k < num_blocks * 64; k++) {
                        uint32_t kk = k - num_blocks;
                        uint32_t nzi = (non_zeros - 1) >> num_blocks_log;
                        uint32_t fi = kk >> num_blocks_log;
                        uint32_t coeff_ctx;
                        uint32_t ucoeff;
                        int32_t coeff;
                        uint32_t dx, dy, xy;

                        if (nzi > 62 || fi > 62) {
                            JXL_ERR(ctx,
                                    "vardct: coefficient context out of range");
                            goto done;
                        }
                        coeff_ctx = (coeff_num_nonzero_context[nzi] +
                                     coeff_freq_context[fi]) * 2 +
                                    is_prev_nonzero;
                        if (coeff_ctx >= 458) {
                            JXL_ERR(ctx, "vardct: too many zeros in varblock");
                            goto done;
                        }
                        ucoeff = jxl_dec_read_clustered(
                            dist, br,
                            dist->clusters[cluster_base + coeff_ctx_base +
                                           coeff_ctx],
                            0);
                        if (ucoeff == 0) {
                            is_prev_nonzero = 0;
                            continue;
                        }
                        coeff = jxl_unpack_signed(ucoeff) << p->coeff_shift;
                        if (ord) {
                            memcpy(&xy, ord + k * 2, sizeof(xy));
                            dx = xy & 0xffffu;
                            dy = xy >> 16;
                            if (need_transpose) {
                                uint32_t t = dx; dx = dy; dy = t;
                            }

                            coeff_out[(size_t)dy * coeff_stride + dx] += coeff;
                        }
                        is_prev_nonzero = 1;
                        non_zeros--;
                        if (non_zeros == 0) break;
                    }
                }
                if (br->err || dist->err) {
                    JXL_ERR(ctx, "vardct: truncated HF coefficients");
                    goto done;
                }
            }
        }
    }

    if (jxl_dec_finalize(dist) != 0) {
        JXL_ERR(ctx, "vardct: bad HF coefficient ANS state");
        goto done;
    }
    rc = 0;

done:
    jxl_free(ctx, nz_all);
    return rc;
}

void jxl_hf_meta_free(jxl_ctx *ctx, jxl_hf_meta *m) {
    if (!m) return;
    jxl_free(ctx, m->x_from_y);
    jxl_free(ctx, m->b_from_y);
    jxl_free(ctx, m->block_info);
    jxl_free(ctx, m->epf_sigma);
    memset(m, 0, sizeof(*m));
}

int jxl_hf_meta_read(jxl_ctx *ctx, jxl_br *br, jxl_hf_meta *m,
                     uint32_t num_lf_groups, uint32_t lf_group_idx,
                     uint32_t lf_width, uint32_t lf_height,
                     const uint32_t jpeg_upsampling[3], uint32_t bit_depth,
                     jxl_ma_config *global_ma, const jxl_epf *epf,
                     uint32_t quantizer_global_scale) {
    uint32_t bw = (lf_width + 7) / 8;
    uint32_t bh = (lf_height + 7) / 8;
    uint32_t cfl_w = (lf_width + 63) / 64;
    uint32_t cfl_h = (lf_height + 63) / 64;
    uint32_t nb_blocks;
    jxl_mchan_spec specs[4];
    jxl_modular mod;
    jxl_chanlist cl;
    int h_up = 0, v_up = 0, i;
    uint32_t x, y, data_idx = 0;
    float epf_quant_mul = 0.0f;
    int rc = -1;

    memset(m, 0, sizeof(*m));
    memset(&mod, 0, sizeof(mod));
    memset(&cl, 0, sizeof(cl));

    for (i = 0; i < 3; i++) {
        if (jpeg_upsampling[i] == 1 || jpeg_upsampling[i] == 2) h_up = 1;
        if (jpeg_upsampling[i] == 1 || jpeg_upsampling[i] == 3) v_up = 1;
    }
    if (h_up) bw = ((bw + 1) / 2) * 2;
    if (v_up) bh = ((bh + 1) / 2) * 2;

    nb_blocks = 1 + jxl_br_read(br, (int)jxl_bitlen(bw * bh > 1 ? bw * bh - 1 : 0));
    if (nb_blocks > bw * bh || br->err) {
        JXL_ERR(ctx, "vardct: bad varblock count");
        return -1;
    }

    specs[0].w = cfl_w; specs[0].h = cfl_h;
    specs[1].w = cfl_w; specs[1].h = cfl_h;
    specs[2].w = nb_blocks; specs[2].h = 2;
    specs[3].w = bw; specs[3].h = bh;
    for (i = 0; i < 4; i++) { specs[i].hshift = 0; specs[i].vshift = 0; }

    if (jxl_modular_init(ctx, &mod, br, specs, 4, global_ma, 0, bit_depth) != 0)
        goto done;
    if (jxl_modular_transform_channels(ctx, &mod, &cl) != 0) goto done;
    if (jxl_modular_decode(ctx, &mod, &cl, br,
                           1 + 2 * num_lf_groups + lf_group_idx) != 0)
        goto done;
    if (jxl_modular_inverse(ctx, &mod, &cl) != 0) goto done;

    m->cfl_w = cfl_w;
    m->cfl_h = cfl_h;
    m->bw = bw;
    m->bh = bh;
    m->x_from_y = (int32_t *)jxl_calloc(ctx, (size_t)cfl_w * cfl_h, sizeof(int32_t));
    m->b_from_y = (int32_t *)jxl_calloc(ctx, (size_t)cfl_w * cfl_h, sizeof(int32_t));
    m->block_info =
        (jxl_block_info *)jxl_calloc(ctx, (size_t)bw * bh, sizeof(jxl_block_info));
    m->epf_sigma = (float *)jxl_calloc(ctx, (size_t)bw * bh, sizeof(float));
    if (!m->x_from_y || !m->b_from_y || !m->block_info || !m->epf_sigma) goto done;

    for (y = 0; y < cfl_h; y++) {
        for (x = 0; x < cfl_w; x++) {
            m->x_from_y[(size_t)y * cfl_w + x] =
                mod.base[0].data[(size_t)y * mod.base[0].stride + x];
            m->b_from_y[(size_t)y * cfl_w + x] =
                mod.base[1].data[(size_t)y * mod.base[1].stride + x];
        }
    }
    for (i = 0; i < (int)(bw * bh); i++) m->block_info[i].dct_select = JXL_BLK_UNINIT;

    if (epf && epf->enabled) {
        epf_quant_mul = epf->quant_mul * 65536.0f / (float)quantizer_global_scale;
    }

    for (y = 0; y < bh; y++) {
        x = 0;
        while (x < bw) {
            jxl_block_info *cur = &m->block_info[(size_t)y * bw + x];
            uint32_t dw, dh, dx, dy;
            int32_t dct_select, hf_mul;
            float sigma_base = 0.0f;

            if (cur->dct_select != JXL_BLK_UNINIT) { x++; continue; }
            if (data_idx >= nb_blocks) {
                JXL_ERR(ctx, "vardct: block info does not fill the LF group");
                goto done;
            }
            dct_select = mod.base[2].data[data_idx];
            hf_mul = mod.base[2].data[mod.base[2].stride + data_idx] + 1;
            if (dct_select < 0 || dct_select >= JXL_TR_COUNT || hf_mul <= 0) {
                JXL_ERR(ctx, "vardct: bad varblock descriptor");
                goto done;
            }
            jxl_tr_select_size(dct_select, &dw, &dh);
            if ((x % 32) + dw > 32 || (y % 32) + dh > 32) {
                JXL_ERR(ctx, "vardct: varblock crosses a pass group border");
                goto done;
            }
            if (epf_quant_mul != 0.0f) sigma_base = epf_quant_mul / (float)hf_mul;

            for (dy = 0; dy < dh; dy++) {
                for (dx = 0; dx < dw; dx++) {
                    jxl_block_info *b;
                    if (x + dx >= bw || y + dy >= bh) {
                        JXL_ERR(ctx, "vardct: varblock does not fit the LF group");
                        goto done;
                    }
                    b = &m->block_info[(size_t)(y + dy) * bw + (x + dx)];
                    if (b->dct_select != JXL_BLK_UNINIT) {
                        JXL_ERR(ctx, "vardct: varblocks overlap");
                        goto done;
                    }
                    if (dx == 0 && dy == 0) {
                        b->dct_select = (uint8_t)dct_select;
                        b->hf_mul = hf_mul;
                    } else {
                        b->dct_select = JXL_BLK_OCCUPIED;
                    }
                    if (sigma_base != 0.0f) {
                        int32_t sharp =
                            mod.base[3].data[(size_t)(y + dy) * mod.base[3].stride + (x + dx)];
                        if (sharp < 0 || sharp >= 8) {
                            JXL_ERR(ctx, "vardct: bad EPF sharpness");
                            goto done;
                        }
                        m->epf_sigma[(size_t)(y + dy) * bw + (x + dx)] =
                            sigma_base * epf->sharp_lut[sharp];
                    }
                }
            }
            data_idx++;
            x += dw;
        }
    }
    m->have = 1;
    rc = 0;

done:
    jxl_chanlist_free(ctx, &cl);
    jxl_modular_free(ctx, &mod);
    if (rc != 0) jxl_hf_meta_free(ctx, m);
    return rc;
}

void jxl_copy_lf_dequant(float *dst, size_t dstride, const jxl_mchan *src,
                         const jxl_quantizer *q, float m_lf,
                         int extra_precision) {
    int precision_scale = 1 << (9 - extra_precision);
    double scale_inv = (double)q->global_scale * (double)q->quant_lf;
    float scale = (float)((double)m_lf * precision_scale / scale_inv);
    uint32_t x, y;
    for (y = 0; y < src->h; y++) {
        const int32_t *row = src->data + (size_t)y * src->stride;
        float *out = dst + (size_t)y * dstride;
        for (x = 0; x < src->w; x++) out[x] = (float)row[x] * scale;
    }
}

int jxl_adaptive_lf_smoothing(jxl_ctx *ctx, float *plane[3], uint32_t width,
                              uint32_t height, size_t stride,
                              const float m_lf[3], const jxl_quantizer *q) {
    const float scale_self = 0.052262735f;
    const float scale_side = 0.2034514f;
    const float scale_diag = 0.03348292f;
    double scale_inv = (double)q->global_scale * (double)q->quant_lf;
    float lf[3];
    float *udsum[3];
    uint32_t x, y;
    int c, rc = -1;

    udsum[0] = udsum[1] = udsum[2] = NULL;
    if (width <= 2 || height <= 2) return 0;
    for (c = 0; c < 3; c++) lf[c] = (float)(512.0 * (double)m_lf[c] / scale_inv);

    for (c = 0; c < 3; c++) {
        udsum[c] = (float *)jxl_calloc(ctx, (size_t)width * (height - 2),
                                       sizeof(float));
        if (!udsum[c]) goto done;
        for (y = 0; y + 2 < height; y++) {
            const float *up = plane[c] + (size_t)y * stride;
            const float *down = plane[c] + (size_t)(y + 2) * stride;
            float *out = udsum[c] + (size_t)y * width;
            for (x = 0; x < width; x++) out[x] = up[x] + down[x];
        }
    }

    for (y = 0; y + 2 < height; y++) {
        float *row[3];
        const float *ud[3];
        float prev[3];
        for (c = 0; c < 3; c++) {
            row[c] = plane[c] + (size_t)(y + 1) * stride;
            ud[c] = udsum[c] + (size_t)y * width;
            prev[c] = row[c][0];
        }
        for (x = 1; x + 1 < width; x++) {
            float self[3], wa[3], gap = 0.5f, gap_scale;
            for (c = 0; c < 3; c++) {
                float side = prev[c] + row[c][x + 1] + ud[c][x];
                float diag = ud[c][x - 1] + ud[c][x + 1];
                float gap_t;
                self[c] = row[c][x];
                wa[c] = self[c] * scale_self + side * scale_side + diag * scale_diag;
                gap_t = fabsf(wa[c] - self[c]) / lf[c];
                if (gap_t > gap) gap = gap_t;
            }
            gap_scale = 3.0f - 4.0f * gap;
            if (gap_scale < 0.0f) gap_scale = 0.0f;
            for (c = 0; c < 3; c++) {
                row[c][x] = (wa[c] - self[c]) * gap_scale + self[c];
                prev[c] = self[c];
            }
        }
    }
    rc = 0;

done:
    for (c = 0; c < 3; c++) jxl_free(ctx, udsum[c]);
    return rc;
}

void jxl_cfl_lf(float *x, float *y, float *b, uint32_t w, uint32_t h,
                size_t stride, const jxl_lf_chan_corr *corr) {
    float kx = corr->base_correlation_x +
               ((float)((int32_t)corr->x_factor_lf - 128) / (float)corr->colour_factor);
    float kb = corr->base_correlation_b +
               ((float)((int32_t)corr->b_factor_lf - 128) / (float)corr->colour_factor);
    uint32_t i, j;
    for (j = 0; j < h; j++) {
        float *rx = x + (size_t)j * stride;
        const float *ry = y + (size_t)j * stride;
        float *rb = b + (size_t)j * stride;
        for (i = 0; i < w; i++) {
            rx[i] += kx * ry[i];
            rb[i] += kb * ry[i];
        }
    }
}

#ifdef JXL_VARDCT_SSE2
JXL_TARGET_AVX2_FMA
static JXL_INLINE_HINT void
dequant_varblock8_core(float *coeff, size_t stride, uint32_t w, uint32_t h,
                       const float *matrix, float mul, float quant_bias,
                       float quant_bias_numerator) {
    const __m256 vbias = _mm256_set1_ps(quant_bias);
    const __m256 vnum = _mm256_set1_ps(quant_bias_numerator);
    const __m256 vmul = _mm256_set1_ps(mul);
    const __m256 one = _mm256_set1_ps(1.0f);
    const __m256 absmask =
        _mm256_castsi256_ps(_mm256_set1_epi32(0x7fffffff));
    uint32_t x, y;

    for (y = 0; y < h; y++) {
        float *row = coeff + (size_t)y * stride;
        const float *mrow = matrix + (size_t)y * w;
        for (x = 0; x < w; x += 8) {
            __m256 v = _mm256_cvtepi32_ps(
                _mm256_loadu_si256((const __m256i *)(row + x)));
            __m256 small = _mm256_mul_ps(v, vbias);

            __m256 large =
                _mm256_fnmadd_ps(vnum, _mm256_rcp_ps(v), v);
            __m256 m = _mm256_cmp_ps(
                _mm256_and_ps(absmask, v), one, _CMP_LE_OQ);
            v = _mm256_or_ps(_mm256_and_ps(m, small),
                             _mm256_andnot_ps(m, large));
            v = _mm256_mul_ps(v, _mm256_loadu_ps(mrow + x));
            v = _mm256_mul_ps(v, vmul);
            _mm256_storeu_ps(row + x, v);
        }
    }
}

JXL_TARGET_AVX2_FMA
static void dequant_varblock8(float *coeff, size_t stride, uint32_t w,
                              uint32_t h, const float *matrix, float mul,
                              float quant_bias,
                              float quant_bias_numerator) {
    dequant_varblock8_core(coeff, stride, w, h, matrix, mul, quant_bias,
                           quant_bias_numerator);
    _mm256_zeroupper();
}

JXL_TARGET_AVX2_FMA
void jxl_dequant_dct8_plane(float *coeff, size_t stride,
                            const jxl_block_info *blocks,
                            uint32_t blocks_w, uint32_t blocks_h,
                            int channel, const jxl_dequant_matrices *dm,
                            const jxl_quantizer *q, float qm_scale,
                            float quant_bias, float quant_bias_numerator) {
    const int slot = jxl_tr_matrix_index(JXL_TR_DCT8);
    const float *matrix = dm->matrix_tr[slot][channel];
    uint32_t bx, by;
    for (by = 0; by < blocks_h; by++) {
        for (bx = 0; bx < blocks_w; bx++) {
            const jxl_block_info *bi =
                blocks + (size_t)by * blocks_w + bx;
            if (!(bi->hf_nonzero_mask & (1u << channel))) continue;
            float mul =
                65536.0f / ((float)q->global_scale * (float)bi->hf_mul) *
                qm_scale;
            dequant_varblock8_core(
                coeff + (size_t)(by * 8) * stride + bx * 8, stride, 8, 8,
                matrix, mul, quant_bias, quant_bias_numerator);
        }
    }
    _mm256_zeroupper();
}
#else

void jxl_dequant_dct8_plane(float *coeff, size_t stride,
                            const jxl_block_info *blocks,
                            uint32_t blocks_w, uint32_t blocks_h,
                            int channel, const jxl_dequant_matrices *dm,
                            const jxl_quantizer *q, float qm_scale,
                            float quant_bias, float quant_bias_numerator) {
    (void)coeff;
    (void)stride;
    (void)blocks;
    (void)blocks_w;
    (void)blocks_h;
    (void)channel;
    (void)dm;
    (void)q;
    (void)qm_scale;
    (void)quant_bias;
    (void)quant_bias_numerator;
}
#endif

void jxl_dequant_varblock(float *coeff, size_t stride, int tr, int32_t hf_mul,
                          int channel, const jxl_dequant_matrices *dm,
                          const jxl_quantizer *q, float qm_scale,
                          float quant_bias, float quant_bias_numerator) {
    uint32_t bw, bh, w, h, x, y;
    const float *matrix;
    float mul;
    int slot = jxl_tr_matrix_index(tr);

    jxl_tr_select_size(tr, &bw, &bh);
    w = bw * 8;
    h = bh * 8;
    mul = 65536.0f / ((float)q->global_scale * (float)hf_mul) * qm_scale;
    matrix = jxl_tr_need_transpose(tr) ? dm->matrix_tr[slot][channel]
                                       : dm->matrix[slot][channel];

#ifdef JXL_VARDCT_SSE2
    if (jxl_has_avx2_fma()) {
        dequant_varblock8(coeff, stride, w, h, matrix, mul, quant_bias,
                          quant_bias_numerator);
        return;
    }
#endif

    for (y = 0; y < h; y++) {
        float *row = coeff + (size_t)y * stride;
        const float *mrow = matrix + (size_t)y * w;
        x = 0;
#ifdef JXL_VARDCT_SSE2
        {
            const __m128 vbias = _mm_set1_ps(quant_bias);
            const __m128 vnum = _mm_set1_ps(quant_bias_numerator);
            const __m128 vmul = _mm_set1_ps(mul);
            const __m128 one = _mm_set1_ps(1.0f);
            const __m128 absmask =
                _mm_castsi128_ps(_mm_set1_epi32(0x7fffffff));
            for (; x + 4 <= w; x += 4) {

                __m128 v = _mm_cvtepi32_ps(
                    _mm_loadu_si128((const __m128i *)(row + x)));
                __m128 small = _mm_mul_ps(v, vbias);

                __m128 large = _mm_sub_ps(v, _mm_div_ps(vnum, v));
                __m128 m = _mm_cmple_ps(_mm_and_ps(absmask, v), one);
                v = _mm_or_ps(_mm_and_ps(m, small), _mm_andnot_ps(m, large));
                v = _mm_mul_ps(v, _mm_loadu_ps(mrow + x));
                v = _mm_mul_ps(v, vmul);
                _mm_storeu_ps(row + x, v);
            }
        }
#endif
        for (; x < w; x++) {
            int32_t qn = *(int32_t *)&row[x];
            float v = (float)qn;
            if (fabsf(v) <= 1.0f) v *= quant_bias;
            else v -= quant_bias_numerator / v;
            v *= mrow[x];
            v *= mul;
            row[x] = v;
        }
    }
}

void jxl_cfl_hf(float *cx, float *cy, float *cb, size_t stride, uint32_t gw,
                uint32_t gh, const int32_t *x_from_y, const int32_t *b_from_y,
                uint32_t cfl_stride, const jxl_lf_chan_corr *corr, int skip_x) {
    uint32_t x, y, x0;

    for (y = 0; y < gh; y++) {
        const int32_t *xr = x_from_y + (size_t)(y / 64) * cfl_stride;
        const int32_t *br_ = b_from_y + (size_t)(y / 64) * cfl_stride;
        float *rx = cx + (size_t)y * stride;
        const float *ry = cy + (size_t)y * stride;
        float *rb = cb + (size_t)y * stride;
        for (x0 = 0; x0 < gw; x0 += 64) {
            uint32_t xe = x0 + 64 < gw ? x0 + 64 : gw;
            float kx = corr->base_correlation_x +
                       ((float)xr[x0 / 64] / (float)corr->colour_factor);
            float kb = corr->base_correlation_b +
                       ((float)br_[x0 / 64] / (float)corr->colour_factor);
            x = x0;
#ifdef JXL_VARDCT_SSE2
            {
                const __m128 vkx = _mm_set1_ps(kx);
                const __m128 vkb = _mm_set1_ps(kb);
                for (; x + 4 <= xe; x += 4) {
                    __m128 vy = _mm_loadu_ps(ry + x);
                    if (!skip_x)
                        _mm_storeu_ps(rx + x, _mm_add_ps(_mm_loadu_ps(rx + x),
                                                         _mm_mul_ps(vkx, vy)));
                    _mm_storeu_ps(rb + x, _mm_add_ps(_mm_loadu_ps(rb + x),
                                                     _mm_mul_ps(vkb, vy)));
                }
            }
#endif
            for (; x < xe; x++) {
                if (!skip_x) rx[x] += kx * ry[x];
                rb[x] += kb * ry[x];
            }
        }
    }
}

static void idct2_in_place(float *block, size_t stride, int size) {
    float scratch[64];
    int num = size / 2;
    int x, y;
    for (y = 0; y < num; y++) {
        for (x = 0; x < num; x++) {
            float c00 = block[(size_t)y * stride + x];
            float c01 = block[(size_t)y * stride + x + num];
            float c10 = block[(size_t)(y + num) * stride + x];
            float c11 = block[(size_t)(y + num) * stride + x + num];
            scratch[(2 * y) * size + 2 * x] = c00 + c01 + c10 + c11;
            scratch[(2 * y) * size + 2 * x + 1] = c00 + c01 - c10 - c11;
            scratch[(2 * y + 1) * size + 2 * x] = c00 - c01 + c10 - c11;
            scratch[(2 * y + 1) * size + 2 * x + 1] = c00 - c01 - c10 + c11;
        }
    }
    for (y = 0; y < size; y++) {
        memcpy(block + (size_t)y * stride, scratch + (size_t)y * size,
               (size_t)size * sizeof(float));
    }
}

static void transform_dct2(float *b, size_t s) {
    idct2_in_place(b, s, 2);
    idct2_in_place(b, s, 4);
    idct2_in_place(b, s, 8);
}

static void transform_dct4(float *b, size_t s) {
    float scratch[64];
    int x, y, ix, iy;
    idct2_in_place(b, s, 2);
    for (y = 0; y < 2; y++) {
        for (x = 0; x < 2; x++) {
            float *sc = scratch + (y * 2 + x) * 16;
            for (iy = 0; iy < 4; iy++) {
                for (ix = 0; ix < 4; ix++) {
                    sc[ix * 4 + iy] = b[(size_t)(y + iy * 2) * s + (x + ix * 2)];
                }
            }
            jxl_dct_2d(sc, 4, 4, 4, 1);
        }
    }
    for (y = 0; y < 2; y++) {
        for (x = 0; x < 2; x++) {
            const float *sc = scratch + (y * 2 + x) * 16;
            for (iy = 0; iy < 4; iy++) {
                for (ix = 0; ix < 4; ix++) {
                    b[(size_t)(y * 4 + iy) * s + (x * 4 + ix)] = sc[iy * 4 + ix];
                }
            }
        }
    }
}

static void transform_hornuss(float *b, size_t s) {
    float scratch[64];
    int x, y, ix, iy, i;
    idct2_in_place(b, s, 2);
    for (y = 0; y < 2; y++) {
        for (x = 0; x < 2; x++) {
            float *sc = scratch + (y * 2 + x) * 16;
            float residual_sum = 0.0f, avg;
            for (iy = 0; iy < 4; iy++) {
                for (ix = 0; ix < 4; ix++) {
                    sc[iy * 4 + ix] = b[(size_t)(y + iy * 2) * s + (x + ix * 2)];
                }
            }
            for (i = 1; i < 16; i++) residual_sum += sc[i];
            avg = sc[0] - residual_sum / 16.0f;
            sc[0] = sc[5];
            sc[5] = 0.0f;
            for (i = 0; i < 16; i++) sc[i] += avg;
        }
    }
    for (y = 0; y < 2; y++) {
        for (x = 0; x < 2; x++) {
            const float *sc = scratch + (y * 2 + x) * 16;
            for (iy = 0; iy < 4; iy++) {
                for (ix = 0; ix < 4; ix++) {
                    b[(size_t)(y * 4 + iy) * s + (x * 4 + ix)] = sc[iy * 4 + ix];
                }
            }
        }
    }
}

static void transform_dct4x8(float *b, size_t s, int transposed) {
    float scratch[64];
    int idx, x, y, ix, iy;
    float c0 = b[0], c1 = b[s];
    b[0] = c0 + c1;
    b[s] = c0 - c1;
    for (idx = 0; idx < 2; idx++) {
        float *sc = scratch + idx * 32;
        for (iy = 0; iy < 4; iy++) {
            for (ix = 0; ix < 8; ix++) {
                sc[iy * 8 + ix] = b[(size_t)(iy * 2 + idx) * s + ix];
            }
        }
        jxl_dct_2d(sc, 8, 8, 4, 1);
    }
    if (transposed) {
        for (y = 0; y < 8; y++) {
            for (x = 0; x < 8; x++) b[(size_t)x * s + y] = scratch[y * 8 + x];
        }
    } else {
        for (y = 0; y < 8; y++) {
            memcpy(b + (size_t)y * s, scratch + y * 8, 8 * sizeof(float));
        }
    }
}

extern const float jxl_afv_basis[16][16];

static void transform_afv(float *b, size_t s, int n) {
    int flip_x = n % 2, flip_y = n / 2;
    float coeff_afv[16], samples_afv[16];
    float scratch_4x4[16], scratch_4x8[32];
    int i, j, ix, iy;

    coeff_afv[0] = (b[0] + b[1] + b[s]) * 4.0f;
    for (i = 1; i < 16; i++) {
        iy = i / 4;
        ix = i % 4;
        coeff_afv[i] = b[(size_t)(2 * iy) * s + 2 * ix];
    }
    for (i = 0; i < 16; i++) samples_afv[i] = 0.0f;
    for (i = 0; i < 16; i++) {
        for (j = 0; j < 16; j++) {
            samples_afv[j] += coeff_afv[i] * jxl_afv_basis[i][j];
        }
    }

    for (i = 0; i < 16; i++) scratch_4x4[i] = 0.0f;
    scratch_4x4[0] = b[0] - b[1] + b[s];
    for (iy = 0; iy < 4; iy++) {
        for (ix = 0; ix < 4; ix++) {
            if ((ix | iy) == 0) continue;
            scratch_4x4[ix * 4 + iy] = b[(size_t)(2 * iy) * s + 2 * ix + 1];
        }
    }
    jxl_dct_2d(scratch_4x4, 4, 4, 4, 1);

    for (i = 0; i < 32; i++) scratch_4x8[i] = 0.0f;
    scratch_4x8[0] = b[0] - b[s];
    for (iy = 0; iy < 4; iy++) {
        for (ix = 0; ix < 8; ix++) {
            if ((ix | iy) == 0) continue;
            scratch_4x8[iy * 8 + ix] = b[(size_t)(2 * iy + 1) * s + ix];
        }
    }
    jxl_dct_2d(scratch_4x8, 8, 8, 4, 1);

    for (iy = 0; iy < 4; iy++) {
        int afv_y = flip_y == 0 ? iy : 3 - iy;
        for (ix = 0; ix < 4; ix++) {
            int afv_x = flip_x == 0 ? ix : 3 - ix;
            b[(size_t)(flip_y * 4 + iy) * s + flip_x * 4 + ix] =
                samples_afv[afv_y * 4 + afv_x];
        }
    }
    for (iy = 0; iy < 4; iy++) {
        for (ix = 0; ix < 4; ix++) {
            b[(size_t)(flip_y * 4 + iy) * s + (1 - flip_x) * 4 + ix] =
                scratch_4x4[iy * 4 + ix];
        }
    }
    for (iy = 0; iy < 4; iy++) {
        memcpy(b + (size_t)((1 - flip_y) * 4 + iy) * s, scratch_4x8 + iy * 8,
               8 * sizeof(float));
    }
}

void jxl_transform_varblock(float *coeff, size_t stride, int tr) {
    uint32_t bw, bh;
    jxl_tr_select_size(tr, &bw, &bh);
    switch (tr) {
        case JXL_TR_DCT2: transform_dct2(coeff, stride); break;
        case JXL_TR_DCT4: transform_dct4(coeff, stride); break;
        case JXL_TR_HORNUSS: transform_hornuss(coeff, stride); break;
        case JXL_TR_DCT4X8: transform_dct4x8(coeff, stride, 0); break;
        case JXL_TR_DCT8X4: transform_dct4x8(coeff, stride, 1); break;
        case JXL_TR_AFV0: transform_afv(coeff, stride, 0); break;
        case JXL_TR_AFV1: transform_afv(coeff, stride, 1); break;
        case JXL_TR_AFV2: transform_afv(coeff, stride, 2); break;
        case JXL_TR_AFV3: transform_afv(coeff, stride, 3); break;
        default:
            jxl_dct_2d(coeff, stride, (int)(bw * 8), (int)(bh * 8), 1);
            break;
    }
}

void jxl_fill_varblock_lf(float *coeff, size_t stride, int tr,
                          const float *lf, size_t lf_stride, uint32_t lf_x,
                          uint32_t lf_y) {
    uint32_t bw, bh, x, y;
    int logbw, logbh;

    jxl_tr_select_size(tr, &bw, &bh);
    if (bw == 1 && bh == 1) {
        coeff[0] = lf[(size_t)lf_y * lf_stride + lf_x];
        return;
    }
    for (y = 0; y < bh; y++) {
        for (x = 0; x < bw; x++) {
            coeff[(size_t)y * stride + x] =
                lf[(size_t)(lf_y + y) * lf_stride + (lf_x + x)];
        }
    }
    logbw = (int)jxl_bitlen(bw) - 1;
    logbh = (int)jxl_bitlen(bh) - 1;
    jxl_dct_2d(coeff, stride, (int)bw, (int)bh, 0);
    for (y = 0; y < bh; y++) {
        for (x = 0; x < bw; x++) {
            coeff[(size_t)y * stride + x] /=
                jxl_scale_f((int)y, 5 - logbh) * jxl_scale_f((int)x, 5 - logbw);
        }
    }
}

#ifdef JXL_VARDCT_SSE2

JXL_TARGET_AVX2
static uint32_t chroma_upsample_h_row_x8(float *row, uint32_t end) {
    const __m256 three_fourths = _mm256_set1_ps(0.75f);
    const __m256 one_fourth = _mm256_set1_ps(0.25f);
    while (end >= 9) {
        uint32_t x = end - 8;
        __m256 cur = _mm256_mul_ps(_mm256_loadu_ps(row + x), three_fourths);
        __m256 lo = _mm256_add_ps(
            cur, _mm256_mul_ps(_mm256_loadu_ps(row + x - 1), one_fourth));
        __m256 hi = _mm256_add_ps(
            cur, _mm256_mul_ps(_mm256_loadu_ps(row + x + 1), one_fourth));
        __m256 il = _mm256_unpacklo_ps(lo, hi);
        __m256 ih = _mm256_unpackhi_ps(lo, hi);
        _mm256_storeu_ps(row + 2 * (size_t)x,
                         _mm256_permute2f128_ps(il, ih, 0x20));
        _mm256_storeu_ps(row + 2 * (size_t)x + 8,
                         _mm256_permute2f128_ps(il, ih, 0x31));
        end = x;
    }
    _mm256_zeroupper();
    return end;
}

static uint32_t chroma_upsample_h_row_x4(float *row, uint32_t end) {
    const __m128 three_fourths = _mm_set1_ps(0.75f);
    const __m128 one_fourth = _mm_set1_ps(0.25f);
    while (end >= 5) {
        uint32_t x = end - 4;
        __m128 cur = _mm_mul_ps(_mm_loadu_ps(row + x), three_fourths);
        __m128 lo = _mm_add_ps(
            cur, _mm_mul_ps(_mm_loadu_ps(row + x - 1), one_fourth));
        __m128 hi = _mm_add_ps(
            cur, _mm_mul_ps(_mm_loadu_ps(row + x + 1), one_fourth));
        _mm_storeu_ps(row + 2 * (size_t)x, _mm_unpacklo_ps(lo, hi));
        _mm_storeu_ps(row + 2 * (size_t)x + 4, _mm_unpackhi_ps(lo, hi));
        end = x;
    }
    return end;
}

JXL_TARGET_AVX2
static uint32_t chroma_upsample_v_row_x8(
    const float *top, const float *mid, const float *bot,
    float *out0, float *out1, uint32_t w, uint32_t x, int write_out1) {
    const __m256 three_fourths = _mm256_set1_ps(0.75f);
    const __m256 one_fourth = _mm256_set1_ps(0.25f);
    for (; x + 8 <= w; x += 8) {
        __m256 m = _mm256_mul_ps(_mm256_loadu_ps(mid + x), three_fourths);
        __m256 lo = _mm256_add_ps(
            m, _mm256_mul_ps(_mm256_loadu_ps(top + x), one_fourth));
        __m256 hi = _mm256_add_ps(
            m, _mm256_mul_ps(_mm256_loadu_ps(bot + x), one_fourth));
        _mm256_storeu_ps(out0 + x, lo);
        if (write_out1) _mm256_storeu_ps(out1 + x, hi);
    }
    _mm256_zeroupper();
    return x;
}

static uint32_t chroma_upsample_v_row_x4(
    const float *top, const float *mid, const float *bot,
    float *out0, float *out1, uint32_t w, uint32_t x, int write_out1) {
    const __m128 three_fourths = _mm_set1_ps(0.75f);
    const __m128 one_fourth = _mm_set1_ps(0.25f);
    for (; x + 4 <= w; x += 4) {
        __m128 m = _mm_mul_ps(_mm_loadu_ps(mid + x), three_fourths);
        __m128 lo = _mm_add_ps(
            m, _mm_mul_ps(_mm_loadu_ps(top + x), one_fourth));
        __m128 hi = _mm_add_ps(
            m, _mm_mul_ps(_mm_loadu_ps(bot + x), one_fourth));
        _mm_storeu_ps(out0 + x, lo);
        if (write_out1) _mm_storeu_ps(out1 + x, hi);
    }
    return x;
}
#endif

static void chroma_upsample_h(float *p, uint32_t w, uint32_t h, size_t stride,
                              uint32_t out_w) {
    uint32_t y;
#ifdef JXL_VARDCT_SSE2
    const int use_avx2 = jxl_has_avx2();
#endif
    for (y = 0; y < h; y++) {
        float *row = p + (size_t)y * stride;
        uint32_t end = w;

        if (end) {
            uint32_t x = --end;
            float cur = row[x] * 0.75f;
            float prev = row[x ? x - 1 : 0];
            float next = row[x + 1 < w ? x + 1 : w - 1];
            if (2 * x + 1 < out_w) row[2 * x + 1] = cur + 0.25f * next;
            row[2 * x] = cur + 0.25f * prev;
        }
#ifdef JXL_VARDCT_SSE2
        if (use_avx2) end = chroma_upsample_h_row_x8(row, end);
        end = chroma_upsample_h_row_x4(row, end);
#endif
        while (end) {
            uint32_t x = --end;
            float cur = row[x] * 0.75f;
            float prev = row[x ? x - 1 : 0];
            float next = row[x + 1 < w ? x + 1 : w - 1];
            if (2 * x + 1 < out_w) row[2 * x + 1] = cur + 0.25f * next;
            row[2 * x] = cur + 0.25f * prev;
        }
    }
}

static void chroma_upsample_v(float *p, uint32_t w, uint32_t h, size_t stride,
                              uint32_t out_h) {
    uint32_t y;
#ifdef JXL_VARDCT_SSE2
    const int use_avx2 = jxl_has_avx2();
#endif
    for (y = h; y-- > 0;) {
        const float *mid = p + (size_t)y * stride;
        const float *top = p + (size_t)(y ? y - 1 : 0) * stride;
        const float *bot = p + (size_t)(y + 1 < h ? y + 1 : h - 1) * stride;
        float *out0 = p + (size_t)(2 * y) * stride;
        float *out1 = p + (size_t)(2 * y + 1) * stride;
        int write_out1 = 2 * y + 1 < out_h;
        uint32_t x = 0;
#ifdef JXL_VARDCT_SSE2
        if (use_avx2) {
            x = chroma_upsample_v_row_x8(top, mid, bot, out0, out1, w, x,
                                         write_out1);
        }
        x = chroma_upsample_v_row_x4(top, mid, bot, out0, out1, w, x,
                                     write_out1);
#endif
        for (; x < w; x++) {
            float m = mid[x] * 0.75f;
            float lo = m + 0.25f * top[x];
            float hi = m + 0.25f * bot[x];
            if (write_out1) out1[x] = hi;
            out0[x] = lo;
        }
    }
}

void jxl_chroma_upsample(float *p, uint32_t w, uint32_t h, size_t stride,
                         int hs, int vs, uint32_t out_w, uint32_t out_h) {
    if (vs) {
        chroma_upsample_v(p, w, h, stride, out_h);
        h = JXL_MIN(2 * h, out_h);
    }
    if (hs) {
        chroma_upsample_h(p, w, h, stride, out_w);
    }
}

#ifdef JXL_VARDCT_SSE2
JXL_TARGET_AVX2
static void ycbcr_to_rgb_x8(float *cb, float *y, float *cr, size_t n,
                            size_t *idx) {
    const __m256 yoff = _mm256_set1_ps(128.0f / 255.0f);
    const __m256 crcr = _mm256_set1_ps(1.402f);
    const __m256 cgcb = _mm256_set1_ps(-0.114f * 1.772f / 0.587f);
    const __m256 cgcr = _mm256_set1_ps(-0.299f * 1.402f / 0.587f);
    const __m256 cbcb = _mm256_set1_ps(1.772f);
    size_t i = *idx;
    for (; i + 8 <= n; i += 8) {
        __m256 b = _mm256_loadu_ps(cb + i);
        __m256 yv = _mm256_add_ps(_mm256_loadu_ps(y + i), yoff);
        __m256 r = _mm256_loadu_ps(cr + i);
        _mm256_storeu_ps(cb + i,
                        _mm256_add_ps(yv, _mm256_mul_ps(crcr, r)));
        _mm256_storeu_ps(y + i,
                        _mm256_add_ps(_mm256_add_ps(
                            yv, _mm256_mul_ps(cgcb, b)),
                            _mm256_mul_ps(cgcr, r)));
        _mm256_storeu_ps(cr + i,
                        _mm256_add_ps(yv, _mm256_mul_ps(cbcb, b)));
    }
    _mm256_zeroupper();
    *idx = i;
}

static void ycbcr_to_rgb_x4(float *cb, float *y, float *cr, size_t n,
                            size_t *idx) {
    const __m128 yoff = _mm_set1_ps(128.0f / 255.0f);
    const __m128 crcr = _mm_set1_ps(1.402f);
    const __m128 cgcb = _mm_set1_ps(-0.114f * 1.772f / 0.587f);
    const __m128 cgcr = _mm_set1_ps(-0.299f * 1.402f / 0.587f);
    const __m128 cbcb = _mm_set1_ps(1.772f);
    size_t i = *idx;
    for (; i + 4 <= n; i += 4) {
        __m128 b = _mm_loadu_ps(cb + i);
        __m128 yv = _mm_add_ps(_mm_loadu_ps(y + i), yoff);
        __m128 r = _mm_loadu_ps(cr + i);
        _mm_storeu_ps(cb + i, _mm_add_ps(yv, _mm_mul_ps(crcr, r)));
        _mm_storeu_ps(y + i,
                      _mm_add_ps(_mm_add_ps(yv, _mm_mul_ps(cgcb, b)),
                                 _mm_mul_ps(cgcr, r)));
        _mm_storeu_ps(cr + i, _mm_add_ps(yv, _mm_mul_ps(cbcb, b)));
    }
    *idx = i;
}

JXL_TARGET_AVX2
static void ycbcr_to_gray_x8(float *gray, const float *y, const float *cr,
                             size_t n, size_t *idx) {
    const __m256 yoff = _mm256_set1_ps(128.0f / 255.0f);
    const __m256 crcr = _mm256_set1_ps(1.402f);
    size_t i = *idx;
    for (; i + 8 <= n; i += 8) {
        __m256 yv = _mm256_add_ps(_mm256_loadu_ps(y + i), yoff);
        __m256 r = _mm256_loadu_ps(cr + i);
        _mm256_storeu_ps(gray + i,
                         _mm256_add_ps(yv, _mm256_mul_ps(crcr, r)));
    }
    _mm256_zeroupper();
    *idx = i;
}

static void ycbcr_to_gray_x4(float *gray, const float *y, const float *cr,
                             size_t n, size_t *idx) {
    const __m128 yoff = _mm_set1_ps(128.0f / 255.0f);
    const __m128 crcr = _mm_set1_ps(1.402f);
    size_t i = *idx;
    for (; i + 4 <= n; i += 4) {
        __m128 yv = _mm_add_ps(_mm_loadu_ps(y + i), yoff);
        __m128 r = _mm_loadu_ps(cr + i);
        _mm_storeu_ps(gray + i, _mm_add_ps(yv, _mm_mul_ps(crcr, r)));
    }
    *idx = i;
}
#endif

void jxl_ycbcr_to_rgb(float *cb, float *y, float *cr, size_t n) {
    const float crcr = 1.402f;
    const float cgcb = -0.114f * 1.772f / 0.587f;
    const float cgcr = -0.299f * 1.402f / 0.587f;
    const float cbcb = 1.772f;
    size_t i = 0;
#ifdef JXL_VARDCT_SSE2
    if (jxl_has_avx2()) ycbcr_to_rgb_x8(cb, y, cr, n, &i);
    ycbcr_to_rgb_x4(cb, y, cr, n, &i);
#endif
    for (; i < n; i++) {
        float yv = y[i] + 128.0f / 255.0f;
        float b_ = cb[i], r_ = cr[i];
        cb[i] = yv + crcr * r_;
        y[i] = yv + cgcb * b_ + cgcr * r_;
        cr[i] = yv + cbcb * b_;
    }
}

void jxl_ycbcr_to_gray(float *gray, const float *y, const float *cr, size_t n) {
    const float crcr = 1.402f;
    size_t i = 0;
#ifdef JXL_VARDCT_SSE2
    if (jxl_has_avx2()) ycbcr_to_gray_x8(gray, y, cr, n, &i);
    ycbcr_to_gray_x4(gray, y, cr, n, &i);
#endif
    for (; i < n; i++) {
        float yv = y[i] + 128.0f / 255.0f;
        gray[i] = yv + crcr * cr[i];
    }
}

#include <math.h>

#if !defined(JXL_DCT_FORCE_SCALAR) &&     (defined(__SSE2__) || defined(_M_X64) || defined(_M_AMD64) ||      (defined(_M_IX86_FP) && _M_IX86_FP >= 2))
#define JXL_DCT_SSE2 1
#include <emmintrin.h>
#include <immintrin.h>
#endif

#define JXL_SQRT2 1.4142135623730951f

static const float sec_half_4[2] = {0.541196100146197f, 1.3065629648763764f};
static const float sec_half_8[4] = {
    0.5097955791041592f, 0.6013448869350453f, 0.8999762231364156f,
    2.5629154477415055f
};
static const float sec_half_16[8] = {
    0.5024192861881557f, 0.5224986149396889f, 0.5669440348163577f,
    0.6468217833599901f, 0.7881546234512502f, 1.060677685990347f,
    1.7224470982383342f, 5.101148618689155f
};
static const float sec_half_32[16] = {
    0.5006029982351963f, 0.5054709598975436f, 0.5154473099226246f,
    0.5310425910897841f, 0.5531038960344445f, 0.5829349682061339f,
    0.6225041230356648f, 0.6748083414550057f, 0.7445362710022984f,
    0.8393496454155268f, 0.9725682378619608f, 1.1694399334328847f,
    1.4841646163141662f, 2.057781009953411f, 3.407608418468719f,
    10.190008123548033f
};

static float sec_half_big[32 + 64 + 128];
static int sec_half_big_init;

static const float *sec_half(int n) {
    switch (n) {
        case 4: return sec_half_4;
        case 8: return sec_half_8;
        case 16: return sec_half_16;
        case 32: return sec_half_32;
        default: break;
    }
    if (!sec_half_big_init) {
        static const int sizes[3] = {64, 128, 256};
        int off = 0, i;
        for (i = 0; i < 3; i++) {
            int m = sizes[i], k;
            for (k = 0; k < m / 2; k++) {
                double theta = (2.0 * k + 1.0) / (2.0 * m) * 3.14159265358979323846;
                sec_half_big[off + k] = (float)(1.0 / cos(theta) / 2.0);
            }
            off += m / 2;
        }
        sec_half_big_init = 1;
    }
    if (n == 64) return sec_half_big;
    if (n == 128) return sec_half_big + 32;
    return sec_half_big + 32 + 64;
}

static const float jxl_scale_f_tbl[32] = {
    1.0000000000000000f, 0.9996047255830407f, 0.9984194528776054f,
    0.9964458326264695f, 0.9936866130906366f, 0.9901456355893141f,
    0.9858278282666936f, 0.9807391980963174f, 0.9748868211368796f,
    0.9682788310563117f, 0.9609244059440204f, 0.9528337534340876f,
    0.9440180941651672f, 0.9344896436056892f, 0.9242615922757944f,
    0.9133480844001980f, 0.9017641950288744f, 0.8895259056651056f,
    0.8766500784429904f, 0.8631544288990163f, 0.8490574973847023f,
    0.8343786191696513f, 0.8191378932865928f, 0.8033561501721485f,
    0.7870549181591013f, 0.7702563888779096f, 0.7529833816270532f,
    0.7352593067735488f, 0.7171081282466044f, 0.6985543251889097f,
    0.6796228528314652f, 0.6603391026591464f
};

float jxl_scale_f(int c, int logb) {
    return jxl_scale_f_tbl[(c << logb) & 31];
}

static void dct4(const float in[4], float out[4], int inverse) {
    const float sec0 = 0.5411961f, sec1 = 1.306563f;
    if (!inverse) {
        float sum03 = in[0] + in[3];
        float sum12 = in[1] + in[2];
        float tmp0 = (in[0] - in[3]) * sec0;
        float tmp1 = (in[1] - in[2]) * sec1;
        float out0 = (tmp0 + tmp1) / 4.0f;
        float out1 = (tmp0 - tmp1) / 4.0f;
        out[0] = (sum03 + sum12) / 4.0f;
        out[1] = out0 * JXL_SQRT2 + out1;
        out[2] = (sum03 - sum12) / 4.0f;
        out[3] = out1;
    } else {
        float tmp0 = in[1] * JXL_SQRT2;
        float tmp1 = in[1] + in[3];
        float out0 = (tmp0 + tmp1) * sec0;
        float out1 = (tmp0 - tmp1) * sec1;
        float sum02 = in[0] + in[2];
        float sub02 = in[0] - in[2];
        out[0] = sum02 + out0;
        out[1] = sub02 + out1;
        out[2] = sub02 - out1;
        out[3] = sum02 - out0;
    }
}

static void dct_1d(float *io, int n, float *scratch, int inverse) {
    int i;
    if (n <= 1) return;
    if (n == 2) {
        float t0 = io[0] + io[1];
        float t1 = io[0] - io[1];
        if (!inverse) { io[0] = t0 / 2.0f; io[1] = t1 / 2.0f; }
        else { io[0] = t0; io[1] = t1; }
        return;
    }
    if (n == 4) {
        float in[4], out[4];
        memcpy(in, io, sizeof(in));
        dct4(in, out, inverse);
        memcpy(io, out, sizeof(out));
        return;
    }
    if (n == 8) {
        const float *sec = sec_half_8;
        float in0[4], in1[4], out0[4], out1[4];
        if (!inverse) {
            for (i = 0; i < 4; i++) {
                in0[i] = (io[i] + io[7 - i]) / 2.0f;
                in1[i] = (io[i] - io[7 - i]) * sec[i] / 2.0f;
            }
            dct4(in0, out0, 0);
            dct4(in1, out1, 0);
            for (i = 0; i < 4; i++) io[i * 2] = out0[i];
            out1[0] *= JXL_SQRT2;
            for (i = 0; i < 3; i++) io[i * 2 + 1] = out1[i] + out1[i + 1];
            io[7] = out1[3];
        } else {
            in0[0] = io[0]; in0[1] = io[2]; in0[2] = io[4]; in0[3] = io[6];
            in1[0] = io[1] * JXL_SQRT2;
            in1[1] = io[3] + io[1];
            in1[2] = io[5] + io[3];
            in1[3] = io[7] + io[5];
            dct4(in0, out0, 1);
            dct4(in1, out1, 1);
            for (i = 0; i < 4; i++) {
                float r = out1[i] * sec[i];
                io[i] = out0[i] + r;
                io[7 - i] = out0[i] - r;
            }
        }
        return;
    }

    {
        const float *sec = sec_half(n);
        int half = n / 2;
        float *in0 = scratch;
        float *in1 = scratch + half;
        if (!inverse) {
            for (i = 0; i < half; i++) {
                in0[i] = (io[i] + io[n - i - 1]) / 2.0f;
                in1[i] = (io[i] - io[n - i - 1]) / 2.0f;
            }
            for (i = 0; i < half; i++) in1[i] *= sec[i];
            dct_1d(in0, half, io, 0);
            dct_1d(in1, half, io + half, 0);
            in1[0] *= JXL_SQRT2;
            for (i = 0; i < half - 1; i++) in1[i] += in1[i + 1];
            for (i = 0; i < half; i++) io[i * 2] = in0[i];
            for (i = 0; i < half; i++) io[i * 2 + 1] = in1[i];
        } else {
            for (i = 0; i < half; i++) {
                in0[i] = io[i * 2];
                in1[i] = io[i * 2 + 1];
            }
            for (i = 1; i < half; i++) in1[half - i] += in1[half - i - 1];
            in1[0] *= JXL_SQRT2;
            dct_1d(in0, half, io, 1);
            dct_1d(in1, half, io + half, 1);
            for (i = 0; i < half; i++) in1[i] *= sec[i];
            for (i = 0; i < half; i++) {
                float a = scratch[i];
                float b = scratch[i + half];
                io[i] = a + b;
                io[n - i - 1] = a - b;
            }
        }
    }
}

#ifdef JXL_DCT_SSE2

static void dct4_v4(const __m128 in[4], __m128 out[4], int inverse) {
    const __m128 sec0 = _mm_set1_ps(0.5411961f);
    const __m128 sec1 = _mm_set1_ps(1.306563f);
    const __m128 sqrt2 = _mm_set1_ps(JXL_SQRT2);
    const __m128 quarter = _mm_set1_ps(0.25f);
    if (!inverse) {
        __m128 sum03 = _mm_add_ps(in[0], in[3]);
        __m128 sum12 = _mm_add_ps(in[1], in[2]);
        __m128 tmp0 = _mm_mul_ps(_mm_sub_ps(in[0], in[3]), sec0);
        __m128 tmp1 = _mm_mul_ps(_mm_sub_ps(in[1], in[2]), sec1);
        __m128 out0 = _mm_mul_ps(_mm_add_ps(tmp0, tmp1), quarter);
        __m128 out1 = _mm_mul_ps(_mm_sub_ps(tmp0, tmp1), quarter);
        out[0] = _mm_mul_ps(_mm_add_ps(sum03, sum12), quarter);
        out[1] = _mm_add_ps(_mm_mul_ps(out0, sqrt2), out1);
        out[2] = _mm_mul_ps(_mm_sub_ps(sum03, sum12), quarter);
        out[3] = out1;
    } else {
        __m128 tmp0 = _mm_mul_ps(in[1], sqrt2);
        __m128 tmp1 = _mm_add_ps(in[1], in[3]);
        __m128 out0 = _mm_mul_ps(_mm_add_ps(tmp0, tmp1), sec0);
        __m128 out1 = _mm_mul_ps(_mm_sub_ps(tmp0, tmp1), sec1);
        __m128 sum02 = _mm_add_ps(in[0], in[2]);
        __m128 sub02 = _mm_sub_ps(in[0], in[2]);
        out[0] = _mm_add_ps(sum02, out0);
        out[1] = _mm_add_ps(sub02, out1);
        out[2] = _mm_sub_ps(sub02, out1);
        out[3] = _mm_sub_ps(sum02, out0);
    }
}

static void dct_1d_v4(__m128 *io, int n, __m128 *scratch, int inverse) {
    const __m128 sqrt2 = _mm_set1_ps(JXL_SQRT2);
    const __m128 half_v = _mm_set1_ps(0.5f);
    int i;
    if (n <= 1) return;
    if (n == 2) {
        __m128 t0 = _mm_add_ps(io[0], io[1]);
        __m128 t1 = _mm_sub_ps(io[0], io[1]);
        if (!inverse) { io[0] = _mm_mul_ps(t0, half_v); io[1] = _mm_mul_ps(t1, half_v); }
        else { io[0] = t0; io[1] = t1; }
        return;
    }
    if (n == 4) {
        __m128 in[4], out[4];
        for (i = 0; i < 4; i++) in[i] = io[i];
        dct4_v4(in, out, inverse);
        for (i = 0; i < 4; i++) io[i] = out[i];
        return;
    }
    if (n == 8) {
        __m128 in0[4], in1[4], out0[4], out1[4];
        if (!inverse) {
            for (i = 0; i < 4; i++) {
                in0[i] = _mm_mul_ps(_mm_add_ps(io[i], io[7 - i]), half_v);
                in1[i] = _mm_mul_ps(
                    _mm_mul_ps(_mm_sub_ps(io[i], io[7 - i]),
                               _mm_set1_ps(sec_half_8[i])),
                    half_v);
            }
            dct4_v4(in0, out0, 0);
            dct4_v4(in1, out1, 0);
            for (i = 0; i < 4; i++) io[i * 2] = out0[i];
            out1[0] = _mm_mul_ps(out1[0], sqrt2);
            for (i = 0; i < 3; i++)
                io[i * 2 + 1] = _mm_add_ps(out1[i], out1[i + 1]);
            io[7] = out1[3];
        } else {
            in0[0] = io[0]; in0[1] = io[2];
            in0[2] = io[4]; in0[3] = io[6];
            in1[0] = _mm_mul_ps(io[1], sqrt2);
            in1[1] = _mm_add_ps(io[3], io[1]);
            in1[2] = _mm_add_ps(io[5], io[3]);
            in1[3] = _mm_add_ps(io[7], io[5]);
            dct4_v4(in0, out0, 1);
            dct4_v4(in1, out1, 1);
            for (i = 0; i < 4; i++) {
                __m128 r = _mm_mul_ps(out1[i], _mm_set1_ps(sec_half_8[i]));
                io[i] = _mm_add_ps(out0[i], r);
                io[7 - i] = _mm_sub_ps(out0[i], r);
            }
        }
        return;
    }
    {
        const float *sec = sec_half(n);
        int hn = n / 2;
        __m128 *in0 = scratch;
        __m128 *in1 = scratch + hn;
        if (!inverse) {
            for (i = 0; i < hn; i++) {
                in0[i] = _mm_mul_ps(_mm_add_ps(io[i], io[n - i - 1]), half_v);
                in1[i] = _mm_mul_ps(_mm_sub_ps(io[i], io[n - i - 1]), half_v);
            }
            for (i = 0; i < hn; i++)
                in1[i] = _mm_mul_ps(in1[i], _mm_set1_ps(sec[i]));
            dct_1d_v4(in0, hn, io, 0);
            dct_1d_v4(in1, hn, io + hn, 0);
            in1[0] = _mm_mul_ps(in1[0], sqrt2);
            for (i = 0; i < hn - 1; i++) in1[i] = _mm_add_ps(in1[i], in1[i + 1]);
            for (i = 0; i < hn; i++) io[i * 2] = in0[i];
            for (i = 0; i < hn; i++) io[i * 2 + 1] = in1[i];
        } else {
            for (i = 0; i < hn; i++) {
                in0[i] = io[i * 2];
                in1[i] = io[i * 2 + 1];
            }
            for (i = 1; i < hn; i++)
                in1[hn - i] = _mm_add_ps(in1[hn - i], in1[hn - i - 1]);
            in1[0] = _mm_mul_ps(in1[0], sqrt2);
            dct_1d_v4(in0, hn, io, 1);
            dct_1d_v4(in1, hn, io + hn, 1);
            for (i = 0; i < hn; i++)
                in1[i] = _mm_mul_ps(in1[i], _mm_set1_ps(sec[i]));
            for (i = 0; i < hn; i++) {
                __m128 a = scratch[i];
                __m128 b = scratch[i + hn];
                io[i] = _mm_add_ps(a, b);
                io[n - i - 1] = _mm_sub_ps(a, b);
            }
        }
    }
}

JXL_TARGET_AVX2
static void dct4_v8(const __m256 in[4], __m256 out[4], int inverse) {
    const __m256 sec0 = _mm256_set1_ps(0.5411961f);
    const __m256 sec1 = _mm256_set1_ps(1.306563f);
    const __m256 sqrt2 = _mm256_set1_ps(JXL_SQRT2);
    const __m256 quarter = _mm256_set1_ps(0.25f);
    if (!inverse) {
        __m256 sum03 = _mm256_add_ps(in[0], in[3]);
        __m256 sum12 = _mm256_add_ps(in[1], in[2]);
        __m256 tmp0 = _mm256_mul_ps(_mm256_sub_ps(in[0], in[3]), sec0);
        __m256 tmp1 = _mm256_mul_ps(_mm256_sub_ps(in[1], in[2]), sec1);
        __m256 out0 = _mm256_mul_ps(_mm256_add_ps(tmp0, tmp1), quarter);
        __m256 out1 = _mm256_mul_ps(_mm256_sub_ps(tmp0, tmp1), quarter);
        out[0] = _mm256_mul_ps(_mm256_add_ps(sum03, sum12), quarter);
        out[1] = _mm256_add_ps(_mm256_mul_ps(out0, sqrt2), out1);
        out[2] = _mm256_mul_ps(_mm256_sub_ps(sum03, sum12), quarter);
        out[3] = out1;
    } else {
        __m256 tmp0 = _mm256_mul_ps(in[1], sqrt2);
        __m256 tmp1 = _mm256_add_ps(in[1], in[3]);
        __m256 out0 = _mm256_mul_ps(_mm256_add_ps(tmp0, tmp1), sec0);
        __m256 out1 = _mm256_mul_ps(_mm256_sub_ps(tmp0, tmp1), sec1);
        __m256 sum02 = _mm256_add_ps(in[0], in[2]);
        __m256 sub02 = _mm256_sub_ps(in[0], in[2]);
        out[0] = _mm256_add_ps(sum02, out0);
        out[1] = _mm256_add_ps(sub02, out1);
        out[2] = _mm256_sub_ps(sub02, out1);
        out[3] = _mm256_sub_ps(sum02, out0);
    }
}

JXL_TARGET_AVX2
static JXL_INLINE_HINT void dct8_v8(__m256 *io, int inverse) {
    const __m256 sqrt2 = _mm256_set1_ps(JXL_SQRT2);
    const __m256 half_v = _mm256_set1_ps(0.5f);
    __m256 in0[4], in1[4], out0[4], out1[4];
    int i;

    if (!inverse) {
        for (i = 0; i < 4; i++) {
            in0[i] = _mm256_mul_ps(
                _mm256_add_ps(io[i], io[7 - i]), half_v);
            in1[i] = _mm256_mul_ps(
                _mm256_mul_ps(_mm256_sub_ps(io[i], io[7 - i]),
                              _mm256_set1_ps(sec_half_8[i])),
                half_v);
        }
        dct4_v8(in0, out0, 0);
        dct4_v8(in1, out1, 0);
        for (i = 0; i < 4; i++) io[i * 2] = out0[i];
        out1[0] = _mm256_mul_ps(out1[0], sqrt2);
        for (i = 0; i < 3; i++)
            io[i * 2 + 1] = _mm256_add_ps(out1[i], out1[i + 1]);
        io[7] = out1[3];
    } else {
        in0[0] = io[0]; in0[1] = io[2];
        in0[2] = io[4]; in0[3] = io[6];
        in1[0] = _mm256_mul_ps(io[1], sqrt2);
        in1[1] = _mm256_add_ps(io[3], io[1]);
        in1[2] = _mm256_add_ps(io[5], io[3]);
        in1[3] = _mm256_add_ps(io[7], io[5]);
        dct4_v8(in0, out0, 1);
        dct4_v8(in1, out1, 1);
        for (i = 0; i < 4; i++) {
            __m256 r = _mm256_mul_ps(
                out1[i], _mm256_set1_ps(sec_half_8[i]));
            io[i] = _mm256_add_ps(out0[i], r);
            io[7 - i] = _mm256_sub_ps(out0[i], r);
        }
    }
}

JXL_TARGET_AVX2_FMA
static JXL_INLINE_HINT void idct4_v8_fma(__m256 *io) {
    const __m256 sqrt2 = _mm256_set1_ps(JXL_SQRT2);
    const __m256 m0 = _mm256_set1_ps(sec_half_4[0]);
    const __m256 m1 = _mm256_set1_ps(sec_half_4[1]);
    __m256 e0 = _mm256_add_ps(io[0], io[2]);
    __m256 e1 = _mm256_sub_ps(io[0], io[2]);
    __m256 o0 = _mm256_mul_ps(io[1], sqrt2);
    __m256 o1 = _mm256_add_ps(io[3], io[1]);
    __m256 b0 = _mm256_add_ps(o0, o1);
    __m256 b1 = _mm256_sub_ps(o0, o1);
    io[0] = _mm256_fmadd_ps(m0, b0, e0);
    io[3] = _mm256_fnmadd_ps(m0, b0, e0);
    io[1] = _mm256_fmadd_ps(m1, b1, e1);
    io[2] = _mm256_fnmadd_ps(m1, b1, e1);
}

JXL_TARGET_AVX2_FMA
static JXL_INLINE_HINT void idct8_v8_fma(__m256 *io) {
    const __m256 sqrt2 = _mm256_set1_ps(JXL_SQRT2);
    const __m256 m0 = _mm256_set1_ps(sec_half_8[0]);
    const __m256 m1 = _mm256_set1_ps(sec_half_8[1]);
    const __m256 m2 = _mm256_set1_ps(sec_half_8[2]);
    const __m256 m3 = _mm256_set1_ps(sec_half_8[3]);
    __m256 even[4], odd[4];

    even[0] = io[0]; even[1] = io[2];
    even[2] = io[4]; even[3] = io[6];
    odd[0] = _mm256_mul_ps(io[1], sqrt2);
    odd[1] = _mm256_add_ps(io[3], io[1]);
    odd[2] = _mm256_add_ps(io[5], io[3]);
    odd[3] = _mm256_add_ps(io[7], io[5]);
    idct4_v8_fma(even);
    idct4_v8_fma(odd);
    io[0] = _mm256_fmadd_ps(m0, odd[0], even[0]);
    io[7] = _mm256_fnmadd_ps(m0, odd[0], even[0]);
    io[1] = _mm256_fmadd_ps(m1, odd[1], even[1]);
    io[6] = _mm256_fnmadd_ps(m1, odd[1], even[1]);
    io[2] = _mm256_fmadd_ps(m2, odd[2], even[2]);
    io[5] = _mm256_fnmadd_ps(m2, odd[2], even[2]);
    io[3] = _mm256_fmadd_ps(m3, odd[3], even[3]);
    io[4] = _mm256_fnmadd_ps(m3, odd[3], even[3]);
}

JXL_TARGET_AVX2
static void dct_1d_v8(__m256 *io, int n, __m256 *scratch, int inverse) {
    const __m256 sqrt2 = _mm256_set1_ps(JXL_SQRT2);
    const __m256 half_v = _mm256_set1_ps(0.5f);
    int i;
    if (n <= 1) return;
    if (n == 2) {
        __m256 t0 = _mm256_add_ps(io[0], io[1]);
        __m256 t1 = _mm256_sub_ps(io[0], io[1]);
        if (!inverse) {
            io[0] = _mm256_mul_ps(t0, half_v);
            io[1] = _mm256_mul_ps(t1, half_v);
        } else { io[0] = t0; io[1] = t1; }
        return;
    }
    if (n == 4) {
        __m256 in[4], out[4];
        for (i = 0; i < 4; i++) in[i] = io[i];
        dct4_v8(in, out, inverse);
        for (i = 0; i < 4; i++) io[i] = out[i];
        return;
    }
    if (n == 8) {
        dct8_v8(io, inverse);
        return;
    }
    if (n == 16) {
        __m256 *in0 = scratch;
        __m256 *in1 = scratch + 8;
        if (!inverse) {
            for (i = 0; i < 8; i++) {
                in0[i] = _mm256_mul_ps(
                    _mm256_add_ps(io[i], io[15 - i]), half_v);
                in1[i] = _mm256_mul_ps(
                    _mm256_sub_ps(io[i], io[15 - i]), half_v);
            }
            for (i = 0; i < 8; i++)
                in1[i] = _mm256_mul_ps(
                    in1[i], _mm256_set1_ps(sec_half_16[i]));
            dct8_v8(in0, 0);
            dct8_v8(in1, 0);
            in1[0] = _mm256_mul_ps(in1[0], sqrt2);
            for (i = 0; i < 7; i++)
                in1[i] = _mm256_add_ps(in1[i], in1[i + 1]);
            for (i = 0; i < 8; i++) io[i * 2] = in0[i];
            for (i = 0; i < 8; i++) io[i * 2 + 1] = in1[i];
        } else {
            for (i = 0; i < 8; i++) {
                in0[i] = io[i * 2];
                in1[i] = io[i * 2 + 1];
            }
            for (i = 1; i < 8; i++)
                in1[8 - i] = _mm256_add_ps(in1[8 - i], in1[7 - i]);
            in1[0] = _mm256_mul_ps(in1[0], sqrt2);
            dct8_v8(in0, 1);
            dct8_v8(in1, 1);
            for (i = 0; i < 8; i++)
                in1[i] = _mm256_mul_ps(
                    in1[i], _mm256_set1_ps(sec_half_16[i]));
            for (i = 0; i < 8; i++) {
                __m256 a = scratch[i];
                __m256 b = scratch[i + 8];
                io[i] = _mm256_add_ps(a, b);
                io[15 - i] = _mm256_sub_ps(a, b);
            }
        }
        return;
    }
    {
        const float *sec = sec_half(n);
        int hn = n / 2;
        __m256 *in0 = scratch;
        __m256 *in1 = scratch + hn;
        if (!inverse) {
            for (i = 0; i < hn; i++) {
                in0[i] = _mm256_mul_ps(_mm256_add_ps(io[i], io[n - i - 1]),
                                       half_v);
                in1[i] = _mm256_mul_ps(_mm256_sub_ps(io[i], io[n - i - 1]),
                                       half_v);
            }
            for (i = 0; i < hn; i++)
                in1[i] = _mm256_mul_ps(in1[i], _mm256_set1_ps(sec[i]));
            dct_1d_v8(in0, hn, io, 0);
            dct_1d_v8(in1, hn, io + hn, 0);
            in1[0] = _mm256_mul_ps(in1[0], sqrt2);
            for (i = 0; i < hn - 1; i++)
                in1[i] = _mm256_add_ps(in1[i], in1[i + 1]);
            for (i = 0; i < hn; i++) io[i * 2] = in0[i];
            for (i = 0; i < hn; i++) io[i * 2 + 1] = in1[i];
        } else {
            for (i = 0; i < hn; i++) {
                in0[i] = io[i * 2];
                in1[i] = io[i * 2 + 1];
            }
            for (i = 1; i < hn; i++)
                in1[hn - i] = _mm256_add_ps(in1[hn - i], in1[hn - i - 1]);
            in1[0] = _mm256_mul_ps(in1[0], sqrt2);
            dct_1d_v8(in0, hn, io, 1);
            dct_1d_v8(in1, hn, io + hn, 1);
            for (i = 0; i < hn; i++)
                in1[i] = _mm256_mul_ps(in1[i], _mm256_set1_ps(sec[i]));
            for (i = 0; i < hn; i++) {
                __m256 a = scratch[i];
                __m256 b = scratch[i + hn];
                io[i] = _mm256_add_ps(a, b);
                io[n - i - 1] = _mm256_sub_ps(a, b);
            }
        }
    }
}

JXL_TARGET_AVX2
static int dct_cols8(float *data, size_t stride, int w, int h, int inverse) {
    __m256 vcol[256], vscratch[256];
    int x = 0, y;
    for (; x + 8 <= w; x += 8) {
        for (y = 0; y < h; y++)
            vcol[y] = _mm256_loadu_ps(data + (size_t)y * stride + x);
        dct_1d_v8(vcol, h, vscratch, inverse);
        for (y = 0; y < h; y++)
            _mm256_storeu_ps(data + (size_t)y * stride + x, vcol[y]);
    }
    _mm256_zeroupper();
    return x;
}

JXL_TARGET_AVX2
static void transpose8_ps(__m256 *r0, __m256 *r1, __m256 *r2, __m256 *r3,
                          __m256 *r4, __m256 *r5, __m256 *r6, __m256 *r7) {
    __m256 t0 = _mm256_unpacklo_ps(*r0, *r1);
    __m256 t1 = _mm256_unpackhi_ps(*r0, *r1);
    __m256 t2 = _mm256_unpacklo_ps(*r2, *r3);
    __m256 t3 = _mm256_unpackhi_ps(*r2, *r3);
    __m256 t4 = _mm256_unpacklo_ps(*r4, *r5);
    __m256 t5 = _mm256_unpackhi_ps(*r4, *r5);
    __m256 t6 = _mm256_unpacklo_ps(*r6, *r7);
    __m256 t7 = _mm256_unpackhi_ps(*r6, *r7);
    __m256 s0 = _mm256_shuffle_ps(t0, t2, 0x44);
    __m256 s1 = _mm256_shuffle_ps(t0, t2, 0xee);
    __m256 s2 = _mm256_shuffle_ps(t1, t3, 0x44);
    __m256 s3 = _mm256_shuffle_ps(t1, t3, 0xee);
    __m256 s4 = _mm256_shuffle_ps(t4, t6, 0x44);
    __m256 s5 = _mm256_shuffle_ps(t4, t6, 0xee);
    __m256 s6 = _mm256_shuffle_ps(t5, t7, 0x44);
    __m256 s7 = _mm256_shuffle_ps(t5, t7, 0xee);
    *r0 = _mm256_permute2f128_ps(s0, s4, 0x20);
    *r1 = _mm256_permute2f128_ps(s1, s5, 0x20);
    *r2 = _mm256_permute2f128_ps(s2, s6, 0x20);
    *r3 = _mm256_permute2f128_ps(s3, s7, 0x20);
    *r4 = _mm256_permute2f128_ps(s0, s4, 0x31);
    *r5 = _mm256_permute2f128_ps(s1, s5, 0x31);
    *r6 = _mm256_permute2f128_ps(s2, s6, 0x31);
    *r7 = _mm256_permute2f128_ps(s3, s7, 0x31);
}

JXL_TARGET_AVX2
static void dct_8x8(float *data, size_t stride, int inverse) {
    __m256 r[8];
    int y;
    for (y = 0; y < 8; y++) {
        r[y] = _mm256_loadu_ps(data + (size_t)y * stride);
    }
    transpose8_ps(&r[0], &r[1], &r[2], &r[3],
                  &r[4], &r[5], &r[6], &r[7]);
    dct8_v8(r, inverse);
    transpose8_ps(&r[0], &r[1], &r[2], &r[3],
                  &r[4], &r[5], &r[6], &r[7]);
    dct8_v8(r, inverse);
    for (y = 0; y < 8; y++) {
        _mm256_storeu_ps(data + (size_t)y * stride, r[y]);
    }
    _mm256_zeroupper();
}

JXL_TARGET_AVX2_FMA
static JXL_INLINE_HINT void idct_8x8_fma_core(float *data, size_t stride) {
    __m256 r[8];
    int y;
    for (y = 0; y < 8; y++) {
        r[y] = _mm256_loadu_ps(data + (size_t)y * stride);
    }
    transpose8_ps(&r[0], &r[1], &r[2], &r[3],
                  &r[4], &r[5], &r[6], &r[7]);
    idct8_v8_fma(r);
    transpose8_ps(&r[0], &r[1], &r[2], &r[3],
                  &r[4], &r[5], &r[6], &r[7]);
    idct8_v8_fma(r);
    for (y = 0; y < 8; y++) {
        _mm256_storeu_ps(data + (size_t)y * stride, r[y]);
    }
}

JXL_TARGET_AVX2_FMA
static void idct_8x8_fma(float *data, size_t stride) {
    idct_8x8_fma_core(data, stride);
    _mm256_zeroupper();
}

JXL_TARGET_AVX2_FMA
void jxl_idct8x8_plane(float *data, size_t stride,
                       const jxl_block_info *blocks, int channel,
                       uint32_t blocks_w, uint32_t blocks_h) {
    uint32_t bx, by;
    for (by = 0; by < blocks_h; by++) {
        for (bx = 0; bx < blocks_w; bx++) {
            float *block =
                data + (size_t)(by * 8) * stride + bx * 8;
            if (blocks[(size_t)by * blocks_w + bx].hf_transform_mask &
                (1u << channel)) {
                idct_8x8_fma_core(block, stride);
            } else {
                __m256 dc = _mm256_set1_ps(block[0]);
                uint32_t y;
                for (y = 0; y < 8; y++)
                    _mm256_storeu_ps(block + (size_t)y * stride, dc);
            }
        }
    }
    _mm256_zeroupper();
}

JXL_TARGET_AVX2
static void dct_rows8(float *data, size_t stride, int w, int inverse) {
    __m256 vrow[256], vscratch[256];
    int j;
    for (j = 0; j + 8 <= w; j += 8) {
        __m256 r0 = _mm256_loadu_ps(data + j);
        __m256 r1 = _mm256_loadu_ps(data + stride + j);
        __m256 r2 = _mm256_loadu_ps(data + 2 * stride + j);
        __m256 r3 = _mm256_loadu_ps(data + 3 * stride + j);
        __m256 r4 = _mm256_loadu_ps(data + 4 * stride + j);
        __m256 r5 = _mm256_loadu_ps(data + 5 * stride + j);
        __m256 r6 = _mm256_loadu_ps(data + 6 * stride + j);
        __m256 r7 = _mm256_loadu_ps(data + 7 * stride + j);
        transpose8_ps(&r0, &r1, &r2, &r3, &r4, &r5, &r6, &r7);
        vrow[j] = r0; vrow[j + 1] = r1; vrow[j + 2] = r2; vrow[j + 3] = r3;
        vrow[j + 4] = r4; vrow[j + 5] = r5; vrow[j + 6] = r6; vrow[j + 7] = r7;
    }
    for (; j < w; j++) {
        vrow[j] = _mm256_setr_ps(data[j], data[stride + j],
                                 data[2 * stride + j], data[3 * stride + j],
                                 data[4 * stride + j], data[5 * stride + j],
                                 data[6 * stride + j], data[7 * stride + j]);
    }
    dct_1d_v8(vrow, w, vscratch, inverse);
    for (j = 0; j + 8 <= w; j += 8) {
        __m256 r0 = vrow[j], r1 = vrow[j + 1];
        __m256 r2 = vrow[j + 2], r3 = vrow[j + 3];
        __m256 r4 = vrow[j + 4], r5 = vrow[j + 5];
        __m256 r6 = vrow[j + 6], r7 = vrow[j + 7];
        transpose8_ps(&r0, &r1, &r2, &r3, &r4, &r5, &r6, &r7);
        _mm256_storeu_ps(data + j, r0);
        _mm256_storeu_ps(data + stride + j, r1);
        _mm256_storeu_ps(data + 2 * stride + j, r2);
        _mm256_storeu_ps(data + 3 * stride + j, r3);
        _mm256_storeu_ps(data + 4 * stride + j, r4);
        _mm256_storeu_ps(data + 5 * stride + j, r5);
        _mm256_storeu_ps(data + 6 * stride + j, r6);
        _mm256_storeu_ps(data + 7 * stride + j, r7);
    }
    for (; j < w; j++) {
        JXL_ALIGN32 float t[8];
        _mm256_store_ps(t, vrow[j]);
        data[j] = t[0];
        data[stride + j] = t[1];
        data[2 * stride + j] = t[2];
        data[3 * stride + j] = t[3];
        data[4 * stride + j] = t[4];
        data[5 * stride + j] = t[5];
        data[6 * stride + j] = t[6];
        data[7 * stride + j] = t[7];
    }
    _mm256_zeroupper();
}

static void dct_rows4(float *data, size_t stride, int w, int inverse) {
    __m128 vrow[256], vscratch[256];
    int j;
    for (j = 0; j + 4 <= w; j += 4) {
        __m128 r0 = _mm_loadu_ps(data + j);
        __m128 r1 = _mm_loadu_ps(data + stride + j);
        __m128 r2 = _mm_loadu_ps(data + 2 * stride + j);
        __m128 r3 = _mm_loadu_ps(data + 3 * stride + j);
        _MM_TRANSPOSE4_PS(r0, r1, r2, r3);
        vrow[j] = r0; vrow[j + 1] = r1; vrow[j + 2] = r2; vrow[j + 3] = r3;
    }
    for (; j < w; j++) {
        vrow[j] = _mm_setr_ps(data[j], data[stride + j],
                              data[2 * stride + j], data[3 * stride + j]);
    }
    dct_1d_v4(vrow, w, vscratch, inverse);
    for (j = 0; j + 4 <= w; j += 4) {
        __m128 r0 = vrow[j], r1 = vrow[j + 1], r2 = vrow[j + 2], r3 = vrow[j + 3];
        _MM_TRANSPOSE4_PS(r0, r1, r2, r3);
        _mm_storeu_ps(data + j, r0);
        _mm_storeu_ps(data + stride + j, r1);
        _mm_storeu_ps(data + 2 * stride + j, r2);
        _mm_storeu_ps(data + 3 * stride + j, r3);
    }
    for (; j < w; j++) {
        float t[4];
        _mm_storeu_ps(t, vrow[j]);
        data[j] = t[0];
        data[stride + j] = t[1];
        data[2 * stride + j] = t[2];
        data[3 * stride + j] = t[3];
    }
}
#else

void jxl_idct8x8_plane(float *data, size_t stride,
                       const jxl_block_info *blocks, int channel,
                       uint32_t blocks_w, uint32_t blocks_h) {
    (void)data;
    (void)stride;
    (void)blocks;
    (void)channel;
    (void)blocks_w;
    (void)blocks_h;
}
#endif

void jxl_dct_2d(float *data, size_t stride, int w, int h, int inverse) {
    float mul = inverse ? 1.0f : 0.5f;
    float scratch[256];
    float col[256];
    int x, y;
#ifdef JXL_DCT_SSE2
    int use_avx2;
#endif

    if (w * h <= 1) return;

    if (w == 2 && h == 1) {
        float v0 = data[0], v1 = data[1];
        data[0] = (v0 + v1) * mul;
        data[1] = (v0 - v1) * mul;
        return;
    }
    if (w == 1 && h == 2) {
        float v0 = data[0], v1 = data[stride];
        data[0] = (v0 + v1) * mul;
        data[stride] = (v0 - v1) * mul;
        return;
    }
    if (w == 2 && h == 2) {
        float v00 = data[0], v01 = data[1];
        float v10 = data[stride], v11 = data[stride + 1];
        data[0] = (v00 + v01 + v10 + v11) * mul * mul;
        data[1] = (v00 - v01 + v10 - v11) * mul * mul;
        data[stride] = (v00 + v01 - v10 - v11) * mul * mul;
        data[stride + 1] = (v00 - v01 - v10 + v11) * mul * mul;
        return;
    }

    if (h == 1) {
        dct_1d(data, w, scratch, inverse);
        return;
    }
    if (w == 1) {
        for (y = 0; y < h; y++) col[y] = data[(size_t)y * stride];
        dct_1d(col, h, scratch, inverse);
        for (y = 0; y < h; y++) data[(size_t)y * stride] = col[y];
        return;
    }

    if (h == 2) {

        float *row0 = data;
        float *row1 = data + stride;
        for (x = 0; x < w; x++) {
            float v0 = row0[x], v1 = row1[x];
            row0[x] = (v0 + v1) * mul;
            row1[x] = (v0 - v1) * mul;
        }
        dct_1d(row0, w, scratch, inverse);
        dct_1d(row1, w, scratch, inverse);
        return;
    }

#ifdef JXL_DCT_SSE2
    if (w == 8 && h == 8 && jxl_has_avx2()) {
        if (inverse && jxl_has_avx2_fma()) idct_8x8_fma(data, stride);
        else dct_8x8(data, stride, inverse);
        return;
    }
    use_avx2 = jxl_has_avx2();
#endif
    y = 0;
#ifdef JXL_DCT_SSE2
    if (use_avx2) {
        for (; y + 8 <= h; y += 8)
            dct_rows8(data + (size_t)y * stride, stride, w, inverse);
    }
    for (; y + 4 <= h; y += 4)
        dct_rows4(data + (size_t)y * stride, stride, w, inverse);
#endif
    for (; y < h; y++) dct_1d(data + (size_t)y * stride, w, scratch, inverse);
    x = 0;
#ifdef JXL_DCT_SSE2
    if (use_avx2) x = dct_cols8(data, stride, w, h, inverse);
    {
        __m128 vcol[256], vscratch[256];
        for (; x + 4 <= w; x += 4) {
            for (y = 0; y < h; y++)
                vcol[y] = _mm_loadu_ps(data + (size_t)y * stride + x);
            dct_1d_v4(vcol, h, vscratch, inverse);
            for (y = 0; y < h; y++)
                _mm_storeu_ps(data + (size_t)y * stride + x, vcol[y]);
        }
    }
#endif
    for (; x < w; x++) {
        for (y = 0; y < h; y++) col[y] = data[(size_t)y * stride + x];
        dct_1d(col, h, scratch, inverse);
        for (y = 0; y < h; y++) data[(size_t)y * stride + x] = col[y];
    }
}

#include <math.h>
#include <stddef.h>

#if !defined(JXL_EPF_FORCE_SCALAR) &&     (defined(__SSE2__) || defined(_M_X64) || defined(_M_AMD64) ||      (defined(_M_IX86_FP) && _M_IX86_FP >= 2))
#define JXL_EPF_SSE2 1
#include <emmintrin.h>
#include <immintrin.h>
#endif

static uint32_t jxl_mirror(int64_t offset, uint32_t len) {
    for (;;) {
        if (offset < 0) offset = -(offset + 1);
        else if ((uint64_t)offset >= len) offset = (int64_t)len * 2 - 1 - offset;
        else return (uint32_t)offset;
    }
}

#ifdef JXL_EPF_SSE2

JXL_TARGET_AVX2
static void gabor_plane_avx2(float *plane, uint32_t w, uint32_t h,
                             size_t stride, float *ring,
                             float w0, float w1, float gw) {
    const __m256 v0 = _mm256_set1_ps(w0);
    const __m256 v1 = _mm256_set1_ps(w1);
    const __m256 vg = _mm256_set1_ps(gw);
    uint32_t x, y;

    for (y = 0; y < h; y++) {
        float *cur = ring + (size_t)(y & 1u) * w;
        const float *rn, *rc, *rs;
        float *dst = plane + (size_t)y * stride;
        uint32_t xlo = w > 1 ? 1 : 0;
        uint32_t xhi = w > 1 ? w - 1 : 0;

        memcpy(cur, dst, (size_t)w * sizeof(float));
        rc = cur;
        rn = y > 0 ? ring + (size_t)((y - 1) & 1u) * w : cur;
        rs = y + 1 < h ? plane + (size_t)(y + 1) * stride : cur;

        for (x = 0; x < xlo; x++) {
            uint32_t xm = 0, xp = (w > 1) ? 1 : 0;
            dst[x] = (rc[x] + (rn[x] + rs[x] + rc[xm] + rc[xp]) * w0 +
                      (rn[xm] + rn[xp] + rs[xm] + rs[xp]) * w1) * gw;
        }
        for (; x + 8 <= xhi; x += 8) {
            __m256 cc = _mm256_loadu_ps(rc + x);
            __m256 side =
                _mm256_add_ps(_mm256_loadu_ps(rn + x),
                              _mm256_loadu_ps(rs + x));
            __m256 diag;
            __m256 r;
            side = _mm256_add_ps(side, _mm256_loadu_ps(rc + x - 1));
            side = _mm256_add_ps(side, _mm256_loadu_ps(rc + x + 1));
            diag = _mm256_add_ps(_mm256_loadu_ps(rn + x - 1),
                                 _mm256_loadu_ps(rn + x + 1));
            diag = _mm256_add_ps(diag, _mm256_loadu_ps(rs + x - 1));
            diag = _mm256_add_ps(diag, _mm256_loadu_ps(rs + x + 1));
            r = _mm256_add_ps(cc, _mm256_mul_ps(side, v0));
            r = _mm256_add_ps(r, _mm256_mul_ps(diag, v1));
            _mm256_storeu_ps(dst + x, _mm256_mul_ps(r, vg));
        }
        for (; x < xhi; x++) {
            dst[x] = (rc[x] +
                      (rn[x] + rs[x] + rc[x - 1] + rc[x + 1]) * w0 +
                      (rn[x - 1] + rn[x + 1] + rs[x - 1] + rs[x + 1]) * w1) *
                     gw;
        }
        for (x = xhi; x < w; x++) {
            uint32_t xm = x > 0 ? x - 1 : 0;
            uint32_t xp = x + 1 < w ? x + 1 : w - 1;
            dst[x] = (rc[x] + (rn[x] + rs[x] + rc[xm] + rc[xp]) * w0 +
                      (rn[xm] + rn[xp] + rs[xm] + rs[xp]) * w1) * gw;
        }
    }
}
#endif

int jxl_apply_gabor(jxl_ctx *ctx, float *plane[3], uint32_t w, uint32_t h,
                    size_t stride, const float weights[3][2]) {
    float *ring;
    int c;
#ifdef JXL_EPF_SSE2
    int use_avx2 = jxl_has_avx2();
#endif
    if (w == 0 || h == 0) return 0;

    ring = (float *)jxl_malloc(ctx, (size_t)w * 2 * sizeof(float));
    if (!ring) return -1;

    for (c = 0; c < 3; c++) {
        float w0 = weights[c][0], w1 = weights[c][1];
        float gw = 1.0f / (1.0f + w0 * 4.0f + w1 * 4.0f);
        uint32_t x, y;
#ifdef JXL_EPF_SSE2
        const __m128 v0 = _mm_set1_ps(w0), v1 = _mm_set1_ps(w1);
        const __m128 vg = _mm_set1_ps(gw);
        if (use_avx2) {
            gabor_plane_avx2(plane[c], w, h, stride, ring, w0, w1, gw);
            continue;
        }
#endif
        for (y = 0; y < h; y++) {

            float *cur = ring + (size_t)(y & 1u) * w;
            const float *rn, *rc, *rs;
            float *dst = plane[c] + (size_t)y * stride;

            memcpy(cur, dst, (size_t)w * sizeof(float));
            rc = cur;
            rn = y > 0 ? ring + (size_t)((y - 1) & 1u) * w : cur;
            rs = y + 1 < h ? plane[c] + (size_t)(y + 1) * stride : cur;
            uint32_t xlo = w > 1 ? 1 : 0, xhi = w > 1 ? w - 1 : 0;

            for (x = 0; x < xlo; x++) {
                uint32_t xm = 0, xp = (w > 1) ? 1 : 0;
                dst[x] = (rc[x] + (rn[x] + rs[x] + rc[xm] + rc[xp]) * w0 +
                          (rn[xm] + rn[xp] + rs[xm] + rs[xp]) * w1) * gw;
            }
            x = xlo;
#ifdef JXL_EPF_SSE2

            for (; x + 4 <= xhi; x += 4) {
                __m128 cc = _mm_loadu_ps(rc + x);

                __m128 side = _mm_add_ps(_mm_loadu_ps(rn + x), _mm_loadu_ps(rs + x));
                __m128 diag;
                __m128 r;
                side = _mm_add_ps(side, _mm_loadu_ps(rc + x - 1));
                side = _mm_add_ps(side, _mm_loadu_ps(rc + x + 1));
                diag = _mm_add_ps(_mm_loadu_ps(rn + x - 1), _mm_loadu_ps(rn + x + 1));
                diag = _mm_add_ps(diag, _mm_loadu_ps(rs + x - 1));
                diag = _mm_add_ps(diag, _mm_loadu_ps(rs + x + 1));
                r = _mm_add_ps(cc, _mm_mul_ps(side, v0));
                r = _mm_add_ps(r, _mm_mul_ps(diag, v1));
                _mm_storeu_ps(dst + x, _mm_mul_ps(r, vg));
            }
#endif
            for (; x < xhi; x++) {
                dst[x] = (rc[x] +
                          (rn[x] + rs[x] + rc[x - 1] + rc[x + 1]) * w0 +
                          (rn[x - 1] + rn[x + 1] + rs[x - 1] + rs[x + 1]) * w1) * gw;
            }
            for (x = xhi; x < w; x++) {
                uint32_t xm = x > 0 ? x - 1 : 0;
                uint32_t xp = x + 1 < w ? x + 1 : w - 1;
                dst[x] = (rc[x] + (rn[x] + rs[x] + rc[xm] + rc[xp]) * w0 +
                          (rn[xm] + rn[xp] + rs[xm] + rs[xp]) * w1) * gw;
            }
        }
    }
    jxl_free(ctx, ring);
    return 0;
}

static const int8_t epf_kernel_2[12][2] = {
    {0,-2},{-1,-1},{0,-1},{1,-1},{-2,0},{-1,0},{1,0},{2,0},
    {-1,1},{0,1},{1,1},{0,2}
};
static const int8_t epf_kernel_1[4][2] = {{0,-1},{0,1},{-1,0},{1,0}};
static const int8_t epf_dist_0[5][2] = {{0,-1},{1,0},{0,0},{-1,0},{0,1}};
static const int8_t epf_dist_1[5][2] = {{0,-1},{0,0},{0,1},{-1,0},{1,0}};
static const int8_t epf_dist_2[1][2] = {{0,0}};

#ifdef JXL_EPF_SSE2

JXL_TARGET_AVX2
static void epf_row8(float *in[3], float *out[3], size_t row, uint32_t x,
                     const ptrdiff_t koff[12], const ptrdiff_t doff[5],
                     int nkernel, int ndist, const float cscale[3],
                     float sigma_val, float step_mul, float border_mul,
                     int is_y_border) {
    const __m256 absmask = _mm256_castsi256_ps(_mm256_set1_epi32(0x7FFFFFFF));
    const __m256 one = _mm256_set1_ps(1.0f);
    __m256 dist8[12], sum8[3], sw, nis, smv;
    int c, k, d;

    if (is_y_border) {
        smv = _mm256_set1_ps(border_mul);
    } else {
        smv = _mm256_setr_ps(border_mul, step_mul, step_mul, step_mul,
                             step_mul, step_mul, step_mul, border_mul);
    }
    nis = _mm256_mul_ps(_mm256_set1_ps(sigma_val), smv);

    for (k = 0; k < nkernel; k++) dist8[k] = _mm256_setzero_ps();
    for (c = 0; c < 3; c++) {
        const float *p = in[c] + row + x;
        __m256 cs = _mm256_set1_ps(cscale[c]);
        __m256 cen[5];
        for (d = 0; d < ndist; d++) cen[d] = _mm256_loadu_ps(p + doff[d]);
        for (k = 0; k < nkernel; k++) {
            const float *pk = p + koff[k];
            __m256 acc = _mm256_setzero_ps();
            for (d = 0; d < ndist; d++) {
                acc = _mm256_add_ps(acc, _mm256_and_ps(absmask,
                    _mm256_sub_ps(_mm256_loadu_ps(pk + doff[d]), cen[d])));
            }
            dist8[k] = _mm256_add_ps(dist8[k], _mm256_mul_ps(cs, acc));
        }
    }
    for (c = 0; c < 3; c++) sum8[c] = _mm256_loadu_ps(in[c] + row + x);
    sw = one;
    for (k = 0; k < nkernel; k++) {
        __m256 wgt = _mm256_add_ps(one, _mm256_mul_ps(dist8[k], nis));
        wgt = _mm256_max_ps(wgt, _mm256_setzero_ps());
        sw = _mm256_add_ps(sw, wgt);
        for (c = 0; c < 3; c++) {
            sum8[c] = _mm256_add_ps(sum8[c], _mm256_mul_ps(wgt,
                _mm256_loadu_ps(in[c] + row + x + koff[k])));
        }
    }

    sw = _mm256_rcp_ps(sw);
    for (c = 0; c < 3; c++)
        _mm256_storeu_ps(out[c] + row + x, _mm256_mul_ps(sum8[c], sw));
    _mm256_zeroupper();
}

#if defined(_MSC_VER)
#define JXL_EPF_NOINLINE __declspec(noinline)
#else
#define JXL_EPF_NOINLINE __attribute__((noinline))
#endif

static JXL_EPF_NOINLINE float epf_hsad_one(
    float *in[3], size_t row, uint32_t x, size_t stride,
    const float cscale[3]) {
    float dist = 0.0f;
    int c;
    for (c = 0; c < 3; c++) {
        const float *p = in[c] + row + x;
        float acc = fabsf(p[-(ptrdiff_t)stride + 1] -
                          p[-(ptrdiff_t)stride]);
        acc += fabsf(p[1] - p[0]);
        acc += fabsf(p[(ptrdiff_t)stride + 1] -
                     p[(ptrdiff_t)stride]);
        acc += fabsf(p[0] - p[-1]);
        acc += fabsf(p[2] - p[1]);
        dist += cscale[c] * acc;
    }
    return dist;
}
#undef JXL_EPF_NOINLINE

JXL_TARGET_AVX2_FMA
static JXL_INLINE_HINT float epf_row8_pass1(
    float *in[3], float *out[3], size_t row, uint32_t x, size_t stride,
    const float cscale[3], float sigma_val, __m256 smv, __m256 absmask,
    __m256 zero, __m256 one, float prev_hsad, float *prev_vsad,
    int reuse_vtop) {
    __m256 dist0 = reuse_vtop ? _mm256_loadu_ps(prev_vsad + x) : zero;
    __m256 dist1 = zero, dist2, dist3 = zero;
    __m256 sum0, sum1, sum2, sw, nis, wgt;
    int c;

    nis = _mm256_mul_ps(_mm256_set1_ps(sigma_val), smv);

    if (!reuse_vtop) {
        for (c = 0; c < 3; c++) {
            const float *p = in[c] + row + x;
            const __m256 p20 =
                _mm256_loadu_ps(p - 2 * (ptrdiff_t)stride);
            const __m256 p21 =
                _mm256_loadu_ps(p - (ptrdiff_t)stride);
            const __m256 p11 =
                _mm256_loadu_ps(p - (ptrdiff_t)stride - 1);
            const __m256 p31 =
                _mm256_loadu_ps(p - (ptrdiff_t)stride + 1);
            const __m256 p12 = _mm256_loadu_ps(p - 1);
            const __m256 p22 = _mm256_loadu_ps(p);
            const __m256 p32 = _mm256_loadu_ps(p + 1);
            const __m256 p23 =
                _mm256_loadu_ps(p + (ptrdiff_t)stride);
            const __m256 cs = _mm256_set1_ps(cscale[c]);
            __m256 acc0;

            acc0 = _mm256_add_ps(zero,
                _mm256_and_ps(absmask, _mm256_sub_ps(p20, p21)));
            acc0 = _mm256_add_ps(acc0,
                _mm256_and_ps(absmask, _mm256_sub_ps(p21, p22)));
            acc0 = _mm256_add_ps(acc0,
                _mm256_and_ps(absmask, _mm256_sub_ps(p22, p23)));
            acc0 = _mm256_add_ps(acc0,
                _mm256_and_ps(absmask, _mm256_sub_ps(p11, p12)));
            acc0 = _mm256_add_ps(acc0,
                _mm256_and_ps(absmask, _mm256_sub_ps(p31, p32)));
            dist0 = _mm256_fmadd_ps(cs, acc0, dist0);
        }
    }

    for (c = 0; c < 3; c++) {
        const float *p = in[c] + row + x;
        const __m256 p21 = _mm256_loadu_ps(p - (ptrdiff_t)stride);
        const __m256 p22 = _mm256_loadu_ps(p);
        const __m256 p23 = _mm256_loadu_ps(p + (ptrdiff_t)stride);
        const __m256 cs = _mm256_set1_ps(cscale[c]);
        __m256 acc1, acc3;

        acc1 = _mm256_add_ps(zero, _mm256_and_ps(
            absmask, _mm256_sub_ps(p21, p22)));
        acc1 = _mm256_add_ps(acc1, _mm256_and_ps(
            absmask, _mm256_sub_ps(p22, p23)));
        acc1 = _mm256_add_ps(acc1,
            _mm256_and_ps(absmask, _mm256_sub_ps(
                _mm256_loadu_ps(p + 2 * (ptrdiff_t)stride), p23)));
        acc1 = _mm256_add_ps(acc1,
            _mm256_and_ps(absmask, _mm256_sub_ps(
                _mm256_loadu_ps(p + (ptrdiff_t)stride - 1),
                _mm256_loadu_ps(p - 1))));
        acc1 = _mm256_add_ps(acc1,
            _mm256_and_ps(absmask, _mm256_sub_ps(
                _mm256_loadu_ps(p + (ptrdiff_t)stride + 1),
                _mm256_loadu_ps(p + 1))));

        acc3 = _mm256_add_ps(zero,
            _mm256_and_ps(absmask, _mm256_sub_ps(
                _mm256_loadu_ps(p - (ptrdiff_t)stride + 1), p21)));
        acc3 = _mm256_add_ps(acc3,
            _mm256_and_ps(absmask, _mm256_sub_ps(
                _mm256_loadu_ps(p + 1), p22)));
        acc3 = _mm256_add_ps(acc3,
            _mm256_and_ps(absmask, _mm256_sub_ps(
                _mm256_loadu_ps(p + (ptrdiff_t)stride + 1), p23)));
        acc3 = _mm256_add_ps(acc3,
            _mm256_and_ps(absmask, _mm256_sub_ps(
                _mm256_loadu_ps(p - 1), p22)));
        acc3 = _mm256_add_ps(acc3,
            _mm256_and_ps(absmask, _mm256_sub_ps(
                _mm256_loadu_ps(p + 2), _mm256_loadu_ps(p + 1))));

        dist1 = _mm256_fmadd_ps(cs, acc1, dist1);
        dist3 = _mm256_fmadd_ps(cs, acc3, dist3);
    }
    if (prev_vsad) _mm256_storeu_ps(prev_vsad + x, dist1);

    {

        __m256i carry = _mm256_castps_si256(
            _mm256_permute2f128_ps(dist3, dist3, 0x08));
        dist2 = _mm256_castsi256_ps(_mm256_alignr_epi8(
            _mm256_castps_si256(dist3), carry, 12));
    }
    dist2 = _mm256_blend_ps(dist2, _mm256_set1_ps(prev_hsad), 0x01);

    sum0 = _mm256_loadu_ps(in[0] + row + x);
    sum1 = _mm256_loadu_ps(in[1] + row + x);
    sum2 = _mm256_loadu_ps(in[2] + row + x);
    sw = one;

    wgt = _mm256_fmadd_ps(dist0, nis, one);
    wgt = _mm256_max_ps(wgt, zero);
    sw = _mm256_add_ps(sw, wgt);
    sum0 = _mm256_fmadd_ps(
        wgt, _mm256_loadu_ps(in[0] + row + x - (ptrdiff_t)stride), sum0);
    sum1 = _mm256_fmadd_ps(
        wgt, _mm256_loadu_ps(in[1] + row + x - (ptrdiff_t)stride), sum1);
    sum2 = _mm256_fmadd_ps(
        wgt, _mm256_loadu_ps(in[2] + row + x - (ptrdiff_t)stride), sum2);

    wgt = _mm256_fmadd_ps(dist1, nis, one);
    wgt = _mm256_max_ps(wgt, zero);
    sw = _mm256_add_ps(sw, wgt);
    sum0 = _mm256_fmadd_ps(
        wgt, _mm256_loadu_ps(in[0] + row + x + stride), sum0);
    sum1 = _mm256_fmadd_ps(
        wgt, _mm256_loadu_ps(in[1] + row + x + stride), sum1);
    sum2 = _mm256_fmadd_ps(
        wgt, _mm256_loadu_ps(in[2] + row + x + stride), sum2);

    wgt = _mm256_fmadd_ps(dist2, nis, one);
    wgt = _mm256_max_ps(wgt, zero);
    sw = _mm256_add_ps(sw, wgt);
    sum0 = _mm256_fmadd_ps(wgt, _mm256_loadu_ps(in[0] + row + x - 1), sum0);
    sum1 = _mm256_fmadd_ps(wgt, _mm256_loadu_ps(in[1] + row + x - 1), sum1);
    sum2 = _mm256_fmadd_ps(wgt, _mm256_loadu_ps(in[2] + row + x - 1), sum2);

    wgt = _mm256_fmadd_ps(dist3, nis, one);
    wgt = _mm256_max_ps(wgt, zero);
    sw = _mm256_add_ps(sw, wgt);
    sum0 = _mm256_fmadd_ps(wgt, _mm256_loadu_ps(in[0] + row + x + 1), sum0);
    sum1 = _mm256_fmadd_ps(wgt, _mm256_loadu_ps(in[1] + row + x + 1), sum1);
    sum2 = _mm256_fmadd_ps(wgt, _mm256_loadu_ps(in[2] + row + x + 1), sum2);

    sw = _mm256_rcp_ps(sw);
    _mm256_storeu_ps(out[0] + row + x, _mm256_mul_ps(sum0, sw));
    _mm256_storeu_ps(out[1] + row + x, _mm256_mul_ps(sum1, sw));
    _mm256_storeu_ps(out[2] + row + x, _mm256_mul_ps(sum2, sw));
    {
        __m128 hi = _mm256_extractf128_ps(dist3, 1);
        hi = _mm_shuffle_ps(hi, hi, _MM_SHUFFLE(3, 3, 3, 3));
        return _mm_cvtss_f32(hi);
    }
}

JXL_TARGET_AVX2_FMA
static uint32_t epf_row_pass1_avx2(
    float *in[3], float *out[3], size_t row, uint32_t x, uint32_t w,
    size_t stride, const float *sigma_row, const float cscale[3],
    float step_mul, float border_mul, int is_y_border, float *prev_vsad,
    const float *prev_sigma_row, int can_reuse_vtop) {
    const __m256 absmask =
        _mm256_castsi256_ps(_mm256_set1_epi32(0x7FFFFFFF));
    const __m256 zero = _mm256_setzero_ps();
    const __m256 one = _mm256_set1_ps(1.0f);
    const __m256 smv = is_y_border
        ? _mm256_set1_ps(border_mul)
        : _mm256_setr_ps(border_mul, step_mul, step_mul, step_mul,
                         step_mul, step_mul, step_mul, border_mul);
    const int have_vtop = can_reuse_vtop && prev_vsad != NULL;
    float prev_hsad = 0.0f;
    int prev_valid = 0;
    for (; x + 9 < w; x += 8) {
        float sigma_val = sigma_row[x / 8];
        if (sigma_val == 0.0f) {
            int c;
            for (c = 0; c < 3; c++) {
                _mm256_storeu_ps(out[c] + row + x,
                                 _mm256_loadu_ps(in[c] + row + x));
            }
            prev_valid = 0;
        } else {
            int reuse_vtop =
                have_vtop && prev_sigma_row[x / 8] != 0.0f;
            if (!prev_valid)
                prev_hsad = epf_hsad_one(in, row, x - 1, stride, cscale);
            prev_hsad = epf_row8_pass1(
                in, out, row, x, stride, cscale, sigma_val, smv, absmask,
                zero, one, prev_hsad, prev_vsad, reuse_vtop);
            prev_valid = 1;
        }
    }
    return x;
}
#endif

static int epf_pass(float *in[3], float *out[3], uint32_t w, uint32_t h,
                    size_t stride, const float *sigma, uint32_t sigma_stride,
                    const jxl_epf *epf, int step, float *vsad_cache) {
    const int8_t (*kernel)[2];
    const int8_t (*dist_off)[2];
    ptrdiff_t koff[12];
    ptrdiff_t doff[5];
    float cscale[3];
    int nkernel, ndist, pad;
    float step_mul, border_mul;
    uint32_t x, y;
    int c, k, d;
#ifdef JXL_EPF_SSE2
    __m128 epf_absmask, sm_border, sm_lo, sm_hi;
    const int use_avx2 = jxl_has_avx2();
    const int use_avx2_fma = jxl_has_avx2_fma();
#else
    (void)vsad_cache;
#endif

    if (step == 0) {
        kernel = epf_kernel_2; nkernel = 12;
        dist_off = epf_dist_0; ndist = 5;
        step_mul = epf->pass0_sigma_scale;
        pad = 3;
    } else if (step == 1) {
        kernel = epf_kernel_1; nkernel = 4;
        dist_off = epf_dist_1; ndist = 5;
        step_mul = 1.0f;
        pad = 2;
    } else {
        kernel = epf_kernel_1; nkernel = 4;
        dist_off = epf_dist_2; ndist = 1;
        step_mul = epf->pass2_sigma_scale;
        pad = 1;
    }
    border_mul = step_mul * epf->border_sad_mul;
#ifdef JXL_EPF_SSE2
    epf_absmask = _mm_castsi128_ps(_mm_set1_epi32(0x7FFFFFFF));
    sm_border = _mm_set1_ps(border_mul);
    sm_lo = _mm_setr_ps(border_mul, step_mul, step_mul, step_mul);
    sm_hi = _mm_setr_ps(step_mul, step_mul, step_mul, border_mul);
#endif
    for (c = 0; c < 3; c++) cscale[c] = epf->channel_scale[c];
    for (d = 0; d < ndist; d++)
        doff[d] = (ptrdiff_t)dist_off[d][1] * (ptrdiff_t)stride + dist_off[d][0];
    for (k = 0; k < nkernel; k++)
        koff[k] = (ptrdiff_t)kernel[k][1] * (ptrdiff_t)stride + kernel[k][0];

    for (y = 0; y < h; y++) {
        int is_y_border = ((y + 1) & 6u) == 0;

        int y_inside = (y >= (uint32_t)pad && y + (uint32_t)pad < h);
        const float *sigma_row = sigma + (size_t)(y / 8) * sigma_stride;
#ifdef JXL_EPF_SSE2
        const float *prev_sigma_row = y
            ? sigma + (size_t)((y - 1) / 8) * sigma_stride
            : sigma_row;
#endif
        size_t row = (size_t)y * stride;
        for (x = 0; x < w; ) {
            float sigma_val = sigma_row[x / 8];

#ifdef JXL_EPF_SSE2
            if (use_avx2_fma && step == 1 && y_inside && (x & 7u) == 0 &&
                x >= 2 && x + 9 < w) {
                x = epf_row_pass1_avx2(in, out, row, x, w, stride, sigma_row,
                                       cscale, step_mul, border_mul,
                                       is_y_border, vsad_cache,
                                       prev_sigma_row,
                                       y > (uint32_t)pad);
                continue;
            }

            if (use_avx2 && y_inside && (x & 7u) == 0 &&
                x >= (uint32_t)pad && x + 7 + (uint32_t)pad < w) {
                if (sigma_val == 0.0f) {
                    for (c = 0; c < 3; c++) {
                        memcpy(out[c] + row + x, in[c] + row + x,
                               8 * sizeof(float));
                    }
                    x += 8;
                    continue;
                }
                epf_row8(in, out, row, x, koff, doff, nkernel, ndist,
                         cscale, sigma_val, step_mul, border_mul,
                         is_y_border);
                x += 8;
                continue;
            }
            if (y_inside && (x & 3u) == 0 &&
                x >= (uint32_t)pad && x + 3 + (uint32_t)pad < w) {
                __m128 dist4[12], sum4[3], sw, nis;
                if (sigma_val == 0.0f) {
                    for (c = 0; c < 3; c++) {
                        _mm_storeu_ps(out[c] + row + x,
                                      _mm_loadu_ps(in[c] + row + x));
                    }
                    x += 4;
                    continue;
                }

                nis = _mm_mul_ps(_mm_set1_ps(sigma_val),
                                 is_y_border ? sm_border
                                             : ((x & 7u) == 0 ? sm_lo : sm_hi));
                for (k = 0; k < nkernel; k++) dist4[k] = _mm_setzero_ps();
                for (c = 0; c < 3; c++) {
                    const float *p = in[c] + row + x;
                    __m128 cs = _mm_set1_ps(cscale[c]);
                    __m128 cen[5];
                    for (d = 0; d < ndist; d++) cen[d] = _mm_loadu_ps(p + doff[d]);
                    for (k = 0; k < nkernel; k++) {
                        const float *pk = p + koff[k];
                        __m128 acc = _mm_setzero_ps();
                        for (d = 0; d < ndist; d++) {
                            acc = _mm_add_ps(acc, _mm_and_ps(epf_absmask,
                                _mm_sub_ps(_mm_loadu_ps(pk + doff[d]), cen[d])));
                        }
                        dist4[k] = _mm_add_ps(dist4[k], _mm_mul_ps(cs, acc));
                    }
                }
                for (c = 0; c < 3; c++) sum4[c] = _mm_loadu_ps(in[c] + row + x);
                sw = _mm_set1_ps(1.0f);
                for (k = 0; k < nkernel; k++) {
                    __m128 wgt = _mm_add_ps(_mm_set1_ps(1.0f),
                                            _mm_mul_ps(dist4[k], nis));
                    wgt = _mm_max_ps(wgt, _mm_setzero_ps());
                    sw = _mm_add_ps(sw, wgt);
                    for (c = 0; c < 3; c++) {
                        sum4[c] = _mm_add_ps(sum4[c], _mm_mul_ps(wgt,
                            _mm_loadu_ps(in[c] + row + x + koff[k])));
                    }
                }

                sw = _mm_div_ps(_mm_set1_ps(1.0f), sw);
                for (c = 0; c < 3; c++) {
                    _mm_storeu_ps(out[c] + row + x, _mm_mul_ps(sum4[c], sw));
                }
                x += 4;
                continue;
            }
#endif
            float dist[12];
            size_t soff[12];
            float sum[3];
            float sum_weights, inv_w, sm, neg_inv_sigma;

            if (sigma_val == 0.0f) {
                for (c = 0; c < 3; c++) out[c][row + x] = in[c][row + x];
                x++;
                continue;
            }
            if (is_y_border || (x & 7u) == 0 || (x & 7u) == 7) sm = border_mul;
            else sm = step_mul;
            neg_inv_sigma = sigma_val * sm;

            for (k = 0; k < nkernel; k++) dist[k] = 0.0f;

            if (y_inside && x >= (uint32_t)pad && x + (uint32_t)pad < w) {

                for (k = 0; k < nkernel; k++) soff[k] = row + x + koff[k];
                for (c = 0; c < 3; c++) {
                    const float *p = in[c] + row + x;
                    float cs = cscale[c];
                    float cen[5];
                    for (d = 0; d < ndist; d++) cen[d] = p[doff[d]];
                    for (k = 0; k < nkernel; k++) {
                        const float *pk = p + koff[k];
                        float acc = 0.0f;
                        for (d = 0; d < ndist; d++)
                            acc += fabsf(pk[doff[d]] - cen[d]);
                        dist[k] += cs * acc;
                    }
                }
            } else {

                size_t bo[5], ao[12][5];
                for (d = 0; d < ndist; d++) {
                    uint32_t bx = jxl_mirror((int64_t)x + dist_off[d][0], w);
                    uint32_t by = jxl_mirror((int64_t)y + dist_off[d][1], h);
                    bo[d] = (size_t)by * stride + bx;
                }
                for (k = 0; k < nkernel; k++) {
                    int64_t kx = (int64_t)x + kernel[k][0];
                    int64_t ky = (int64_t)y + kernel[k][1];
                    for (d = 0; d < ndist; d++) {
                        uint32_t ax = jxl_mirror(kx + dist_off[d][0], w);
                        uint32_t ay = jxl_mirror(ky + dist_off[d][1], h);
                        ao[k][d] = (size_t)ay * stride + ax;
                    }
                    soff[k] = (size_t)jxl_mirror(ky, h) * stride +
                              jxl_mirror(kx, w);
                }
                for (c = 0; c < 3; c++) {
                    const float *p = in[c];
                    float cs = cscale[c];
                    float cen[5];
                    for (d = 0; d < ndist; d++) cen[d] = p[bo[d]];
                    for (k = 0; k < nkernel; k++) {
                        float acc = 0.0f;
                        for (d = 0; d < ndist; d++)
                            acc += fabsf(p[ao[k][d]] - cen[d]);
                        dist[k] += cs * acc;
                    }
                }
            }

            for (c = 0; c < 3; c++) sum[c] = in[c][row + x];
            sum_weights = 1.0f;
            for (k = 0; k < nkernel; k++) {
                float weight = 1.0f + dist[k] * neg_inv_sigma;
                if (weight < 0.0f) weight = 0.0f;
                sum_weights += weight;
                for (c = 0; c < 3; c++) sum[c] += weight * in[c][soff[k]];
            }

            inv_w = 1.0f / sum_weights;
            for (c = 0; c < 3; c++) out[c][row + x] = sum[c] * inv_w;
            x++;
        }
    }
    return 0;
}

int jxl_apply_epf(jxl_ctx *ctx, float *plane[3], uint32_t w, uint32_t h,
                  size_t stride, const float *sigma, uint32_t sigma_stride,
                  const jxl_epf *epf) {
    float *scratch[3];
    float *vsad_cache = NULL;
    float *in[3], *out[3], *t;
    int c, rc = -1;

    scratch[0] = scratch[1] = scratch[2] = NULL;
    if (!epf->enabled || w == 0 || h == 0) return 0;
    for (c = 0; c < 3; c++) {

        size_t n;
        if (!jxl_size_mul(stride * h, sizeof(float), &n)) goto done;
        scratch[c] = (float *)jxl_malloc(ctx, n);
        if (!scratch[c]) goto done;
    }
#ifdef JXL_EPF_SSE2
    if (jxl_has_avx2()) {
        size_t n;
        if (jxl_size_mul(stride, sizeof(float), &n))
            vsad_cache = (float *)jxl_malloc(ctx, n);

    }
#endif
    for (c = 0; c < 3; c++) { in[c] = plane[c]; out[c] = scratch[c]; }

    if (epf->iters == 3) {
        if (epf_pass(in, out, w, h, stride, sigma, sigma_stride, epf, 0,
                     vsad_cache) != 0) goto done;
        for (c = 0; c < 3; c++) { t = in[c]; in[c] = out[c]; out[c] = t; }
    }
    if (epf_pass(in, out, w, h, stride, sigma, sigma_stride, epf, 1,
                 vsad_cache) != 0) goto done;
    for (c = 0; c < 3; c++) { t = in[c]; in[c] = out[c]; out[c] = t; }
    if (epf->iters >= 2) {
        if (epf_pass(in, out, w, h, stride, sigma, sigma_stride, epf, 2,
                     vsad_cache) != 0) goto done;
        for (c = 0; c < 3; c++) { t = in[c]; in[c] = out[c]; out[c] = t; }
    }

    for (c = 0; c < 3; c++) {
        if (in[c] == plane[c]) continue;
#ifdef JXL_EPF_FORCE_COPY_BACK
        {
            uint32_t y;
            for (y = 0; y < h; y++) {
                memcpy(plane[c] + (size_t)y * stride,
                       in[c] + (size_t)y * stride,
                       (size_t)w * sizeof(float));
            }
        }
#else
        t = plane[c];
        plane[c] = in[c];
        scratch[c] = t;
#endif
    }
    rc = 0;

done:
    jxl_free(ctx, vsad_cache);
    for (c = 0; c < 3; c++) jxl_free(ctx, scratch[c]);
    return rc;
}

#include <math.h>

#if !defined(JXL_COLOR_FORCE_SCALAR) && \
    (defined(__SSE2__) || defined(_M_X64) || defined(_M_AMD64) || \
     (defined(_M_IX86_FP) && _M_IX86_FP >= 2))
#define JXL_COLOR_SSE2 1
#include <emmintrin.h>
#include <immintrin.h>
#endif

#ifdef JXL_COLOR_SSE2
JXL_TARGET_AVX2
static size_t xyb_to_linear_x8(float *x, float *y, float *b, size_t i,
                               size_t n, const float opsin_inv[9],
                               const float opsin_bias[3],
                               const float cbrt_ob[3], float itscale) {
    const __m256 c0 = _mm256_set1_ps(cbrt_ob[0]);
    const __m256 c1 = _mm256_set1_ps(cbrt_ob[1]);
    const __m256 c2 = _mm256_set1_ps(cbrt_ob[2]);
    const __m256 b0 = _mm256_set1_ps(opsin_bias[0]);
    const __m256 b1 = _mm256_set1_ps(opsin_bias[1]);
    const __m256 b2 = _mm256_set1_ps(opsin_bias[2]);
    const __m256 its = _mm256_set1_ps(itscale);
    __m256 oi[9];
    int k;

    for (k = 0; k < 9; k++) oi[k] = _mm256_set1_ps(opsin_inv[k]);
    for (; i + 8 <= n; i += 8) {
        __m256 vx = _mm256_loadu_ps(x + i);
        __m256 vy = _mm256_loadu_ps(y + i);
        __m256 vb = _mm256_loadu_ps(b + i);
        __m256 gl = _mm256_sub_ps(_mm256_add_ps(vy, vx), c0);
        __m256 gm = _mm256_sub_ps(_mm256_sub_ps(vy, vx), c1);
        __m256 gs = _mm256_sub_ps(vb, c2);
        __m256 l =
            _mm256_add_ps(_mm256_mul_ps(_mm256_mul_ps(gl, gl), gl), b0);
        __m256 m =
            _mm256_add_ps(_mm256_mul_ps(_mm256_mul_ps(gm, gm), gm), b1);
        __m256 s =
            _mm256_add_ps(_mm256_mul_ps(_mm256_mul_ps(gs, gs), gs), b2);
        if (itscale != 1.0f) {
            l = _mm256_mul_ps(l, its);
            m = _mm256_mul_ps(m, its);
            s = _mm256_mul_ps(s, its);
        }
        _mm256_storeu_ps(
            x + i,
            _mm256_add_ps(
                _mm256_add_ps(_mm256_mul_ps(oi[0], l),
                              _mm256_mul_ps(oi[1], m)),
                _mm256_mul_ps(oi[2], s)));
        _mm256_storeu_ps(
            y + i,
            _mm256_add_ps(
                _mm256_add_ps(_mm256_mul_ps(oi[3], l),
                              _mm256_mul_ps(oi[4], m)),
                _mm256_mul_ps(oi[5], s)));
        _mm256_storeu_ps(
            b + i,
            _mm256_add_ps(
                _mm256_add_ps(_mm256_mul_ps(oi[6], l),
                              _mm256_mul_ps(oi[7], m)),
                _mm256_mul_ps(oi[8], s)));
    }
    _mm256_zeroupper();
    return i;
}
#endif

void jxl_xyb_to_linear(float *x, float *y, float *b, size_t n,
                       const float opsin_inv[9], const float opsin_bias[3],
                       float intensity_target) {
    float itscale = 255.0f / intensity_target;
    float cbrt_ob[3];
    size_t i;
    int k;

    for (k = 0; k < 3; k++) cbrt_ob[k] = cbrtf(opsin_bias[k]);

    i = 0;
#ifdef JXL_COLOR_SSE2
    if (jxl_has_avx2()) {
        i = xyb_to_linear_x8(x, y, b, i, n, opsin_inv, opsin_bias, cbrt_ob,
                             itscale);
    }
    {

        const __m128 c0 = _mm_set1_ps(cbrt_ob[0]), c1 = _mm_set1_ps(cbrt_ob[1]);
        const __m128 c2 = _mm_set1_ps(cbrt_ob[2]);
        const __m128 b0 = _mm_set1_ps(opsin_bias[0]), b1 = _mm_set1_ps(opsin_bias[1]);
        const __m128 b2 = _mm_set1_ps(opsin_bias[2]);
        const __m128 its = _mm_set1_ps(itscale);
        __m128 oi[9];
        for (k = 0; k < 9; k++) oi[k] = _mm_set1_ps(opsin_inv[k]);
        for (; i + 4 <= n; i += 4) {
            __m128 vx = _mm_loadu_ps(x + i);
            __m128 vy = _mm_loadu_ps(y + i);
            __m128 vb = _mm_loadu_ps(b + i);
            __m128 gl = _mm_sub_ps(_mm_add_ps(vy, vx), c0);
            __m128 gm = _mm_sub_ps(_mm_sub_ps(vy, vx), c1);
            __m128 gs = _mm_sub_ps(vb, c2);
            __m128 l = _mm_add_ps(_mm_mul_ps(_mm_mul_ps(gl, gl), gl), b0);
            __m128 m = _mm_add_ps(_mm_mul_ps(_mm_mul_ps(gm, gm), gm), b1);
            __m128 s = _mm_add_ps(_mm_mul_ps(_mm_mul_ps(gs, gs), gs), b2);
            if (itscale != 1.0f) {
                l = _mm_mul_ps(l, its);
                m = _mm_mul_ps(m, its);
                s = _mm_mul_ps(s, its);
            }
            _mm_storeu_ps(x + i, _mm_add_ps(_mm_add_ps(
                _mm_mul_ps(oi[0], l), _mm_mul_ps(oi[1], m)), _mm_mul_ps(oi[2], s)));
            _mm_storeu_ps(y + i, _mm_add_ps(_mm_add_ps(
                _mm_mul_ps(oi[3], l), _mm_mul_ps(oi[4], m)), _mm_mul_ps(oi[5], s)));
            _mm_storeu_ps(b + i, _mm_add_ps(_mm_add_ps(
                _mm_mul_ps(oi[6], l), _mm_mul_ps(oi[7], m)), _mm_mul_ps(oi[8], s)));
        }
    }
#endif
    for (; i < n; i++) {
        float gl = y[i] + x[i] - cbrt_ob[0];
        float gm = y[i] - x[i] - cbrt_ob[1];
        float gs = b[i] - cbrt_ob[2];
        float l = gl * gl * gl + opsin_bias[0];
        float m = gm * gm * gm + opsin_bias[1];
        float s = gs * gs * gs + opsin_bias[2];
        if (itscale != 1.0f) {
            l *= itscale;
            m *= itscale;
            s *= itscale;
        }
        x[i] = opsin_inv[0] * l + opsin_inv[1] * m + opsin_inv[2] * s;
        y[i] = opsin_inv[3] * l + opsin_inv[4] * m + opsin_inv[5] * s;
        b[i] = opsin_inv[6] * l + opsin_inv[7] * m + opsin_inv[8] * s;
    }
}

static const uint8_t srgb_powtable_upper[16] = {
    0x00, 0x0a, 0x19, 0x26, 0x32, 0x41, 0x4d, 0x5c,
    0x68, 0x75, 0x83, 0x8f, 0xa0, 0xaa, 0xb9, 0xc6
};
static const uint8_t srgb_powtable_lower[16] = {
    0x00, 0xb7, 0x04, 0x0d, 0xcb, 0xe7, 0x41, 0x68,
    0x51, 0xd1, 0xeb, 0xf2, 0x00, 0xb7, 0x04, 0x0d
};

static float bits_to_float(uint32_t b) {
    float f;
    memcpy(&f, &b, sizeof(f));
    return f;
}

static uint32_t float_to_bits(float f) {
    uint32_t b;
    memcpy(&b, &f, sizeof(b));
    return b;
}

static float tf_srgb(float s) {
    uint32_t sign = float_to_bits(s) & 0x80000000u;
    uint32_t v = float_to_bits(s) & 0x7fffffffu;
    float v_adj = bits_to_float((v | 0x3e800000u) & 0x3effffffu);
    float pow = 0.059914046f;
    uint32_t idx, mul_bits;
    float fv, small, acc, out;

    pow = pow * v_adj - 0.10889456f;
    pow = pow * v_adj + 0.107963754f;
    pow = pow * v_adj + 0.018092343f;

    idx = ((v >> 23) - 118u) & 0xf;
    mul_bits = 0x40000000u | ((uint32_t)srgb_powtable_upper[idx] << 18) |
               ((uint32_t)srgb_powtable_lower[idx] << 10);

    fv = bits_to_float(v);
    small = fv * 12.92f;
    acc = pow * bits_to_float(mul_bits) - 0.055f;
    out = (fv <= 0.0031308f) ? small : acc;
    return bits_to_float(float_to_bits(out) | sign);
}

static float fast_log2f(float x) {
    const float p0 = -1.8503833400518310E-06f, p1 = 1.4287160470083755E+00f;
    const float p2 = 7.4245873327820566E-01f;
    const float q0 = 9.9032814277590719E-01f, q1 = 1.0096718572241148E+00f;
    const float q2 = 1.7409343003366853E-01f;
    uint32_t xb = float_to_bits(x);

    int32_t es = (int32_t)(xb - 0x3f2aaaabu) >> 23;
    float m = bits_to_float(xb - ((uint32_t)es << 23));
    float t = m - 1.0f;
    float yp = p2 * t + p1;
    float yq = q2 * t + q1;
    yp = yp * t + p0;
    yq = yq * t + q0;
    return yp / yq + (float)es;
}

static float fast_pow2f(float x) {
    float fl = floorf(x);
    float ex = bits_to_float((uint32_t)((int32_t)fl + 127) << 23);
    float frac = x - fl;
    float num = frac + 1.01749063e+01f;
    float den = frac * 2.10242958e-01f - 2.22328856e-02f;
    num = num * frac + 4.88687798e+01f;
    den = den * frac - 1.94414990e+01f;
    num = num * frac + 9.85506591e+01f;
    den = den * frac + 9.85506633e+01f;
    return num * ex / den;
}

static float tf_bt709(float v) {
    if (v <= 0.018053968510807f) return 4.5f * v;
    return 1.09929682680944f * fast_pow2f(fast_log2f(v) * 0.45f) -
           0.09929682680944f;
}

static float tf_pq(float v) {

    const float m1 = 2610.0f / 16384.0f;
    const float m2 = 2523.0f / 4096.0f * 128.0f;
    const float c1 = 3424.0f / 4096.0f;
    const float c2 = 2413.0f / 4096.0f * 32.0f;
    const float c3 = 2392.0f / 4096.0f * 32.0f;
    float p;
    if (v <= 0.0f) return 0.0f;
    p = powf(v, m1);
    return powf((c1 + c2 * p) / (1.0f + c3 * p), m2);
}

static float tf_hlg(float v) {
    const float a = 0.17883277f;
    const float b = 0.28466892f;
    const float c = 0.55991073f;
    if (v <= 0.0f) return 0.0f;
    if (v <= 1.0f / 12.0f) return sqrtf(3.0f * v);
    return a * logf(12.0f * v - b) + c;
}

#ifdef JXL_COLOR_SSE2

static void tf_srgb_x4(float *v, size_t n, size_t *pos) {
    const __m128i signmask = _mm_set1_epi32((int)0x80000000u);
    const __m128i absmask = _mm_set1_epi32(0x7fffffff);
    const __m128i adj_or = _mm_set1_epi32(0x3e800000);
    const __m128i adj_and = _mm_set1_epi32(0x3effffff);
    const __m128 k3 = _mm_set1_ps(0.059914046f), k2 = _mm_set1_ps(-0.10889456f);
    const __m128 k1 = _mm_set1_ps(0.107963754f), k0 = _mm_set1_ps(0.018092343f);
    const __m128 c1292 = _mm_set1_ps(12.92f), c055 = _mm_set1_ps(0.055f);
    const __m128 cutoff = _mm_set1_ps(0.0031308f);
    size_t i = *pos;
    for (; i + 4 <= n; i += 4) {
        __m128i bits = _mm_castps_si128(_mm_loadu_ps(v + i));
        __m128i sign = _mm_and_si128(bits, signmask);
        __m128i vi = _mm_and_si128(bits, absmask);
        __m128 v_adj = _mm_castsi128_ps(
            _mm_and_si128(_mm_or_si128(vi, adj_or), adj_and));
        __m128 pw = _mm_add_ps(_mm_mul_ps(k3, v_adj), k2);
        __m128 fv = _mm_castsi128_ps(vi);
        __m128i idx = _mm_and_si128(_mm_sub_epi32(_mm_srli_epi32(vi, 23),
                                                  _mm_set1_epi32(118)),
                                    _mm_set1_epi32(0xf));
        int ix[4];
        __m128i mul_bits;
        __m128 small, acc, out;
        pw = _mm_add_ps(_mm_mul_ps(pw, v_adj), k1);
        pw = _mm_add_ps(_mm_mul_ps(pw, v_adj), k0);
        _mm_storeu_si128((__m128i *)ix, idx);
        mul_bits = _mm_setr_epi32(
            (int)(0x40000000u | ((uint32_t)srgb_powtable_upper[ix[0] & 0xf] << 18) |
                  ((uint32_t)srgb_powtable_lower[ix[0] & 0xf] << 10)),
            (int)(0x40000000u | ((uint32_t)srgb_powtable_upper[ix[1] & 0xf] << 18) |
                  ((uint32_t)srgb_powtable_lower[ix[1] & 0xf] << 10)),
            (int)(0x40000000u | ((uint32_t)srgb_powtable_upper[ix[2] & 0xf] << 18) |
                  ((uint32_t)srgb_powtable_lower[ix[2] & 0xf] << 10)),
            (int)(0x40000000u | ((uint32_t)srgb_powtable_upper[ix[3] & 0xf] << 18) |
                  ((uint32_t)srgb_powtable_lower[ix[3] & 0xf] << 10)));
        small = _mm_mul_ps(fv, c1292);
        acc = _mm_sub_ps(_mm_mul_ps(pw, _mm_castsi128_ps(mul_bits)), c055);

        {
            __m128 m = _mm_cmple_ps(fv, cutoff);
            out = _mm_or_ps(_mm_and_ps(m, small), _mm_andnot_ps(m, acc));
        }
        _mm_storeu_ps(v + i, _mm_castsi128_ps(
            _mm_or_si128(_mm_castps_si128(out), sign)));
    }
    *pos = i;
}

JXL_TARGET_AVX2
static void tf_srgb_x8(float *v, size_t n, size_t *pos) {
    const __m256i signmask = _mm256_set1_epi32((int)0x80000000u);
    const __m256i absmask = _mm256_set1_epi32(0x7fffffff);
    const __m256i adj_or = _mm256_set1_epi32(0x3e800000);
    const __m256i adj_and = _mm256_set1_epi32(0x3effffff);
    const __m256i shuffle_zero = _mm256_set1_epi32((int)0x80808000u);
    const __m256i table_upper = _mm256_broadcastsi128_si256(
        _mm_loadu_si128((const __m128i *)srgb_powtable_upper));
    const __m256i table_lower = _mm256_broadcastsi128_si256(
        _mm_loadu_si128((const __m128i *)srgb_powtable_lower));
    const __m256 k3 = _mm256_set1_ps(0.059914046f);
    const __m256 k2 = _mm256_set1_ps(-0.10889456f);
    const __m256 k1 = _mm256_set1_ps(0.107963754f);
    const __m256 k0 = _mm256_set1_ps(0.018092343f);
    const __m256 c1292 = _mm256_set1_ps(12.92f), c055 = _mm256_set1_ps(0.055f);
    const __m256 cutoff = _mm256_set1_ps(0.0031308f);
    size_t i = *pos;
    for (; i + 8 <= n; i += 8) {
        __m256i bits = _mm256_castps_si256(_mm256_loadu_ps(v + i));
        __m256i sign = _mm256_and_si256(bits, signmask);
        __m256i vi = _mm256_and_si256(bits, absmask);
        __m256 v_adj = _mm256_castsi256_ps(
            _mm256_and_si256(_mm256_or_si256(vi, adj_or), adj_and));
        __m256 pw = _mm256_add_ps(_mm256_mul_ps(k3, v_adj), k2);
        __m256 fv = _mm256_castsi256_ps(vi);
        __m256i idx = _mm256_and_si256(
            _mm256_sub_epi32(_mm256_srli_epi32(vi, 23), _mm256_set1_epi32(118)),
            _mm256_set1_epi32(0xf));
        __m256i shuffle, upper, lower, mul_bits;
        __m256 small, acc, out, m;
        pw = _mm256_add_ps(_mm256_mul_ps(pw, v_adj), k1);
        pw = _mm256_add_ps(_mm256_mul_ps(pw, v_adj), k0);
        shuffle = _mm256_or_si256(idx, shuffle_zero);
        upper = _mm256_shuffle_epi8(table_upper, shuffle);
        lower = _mm256_shuffle_epi8(table_lower, shuffle);
        mul_bits = _mm256_or_si256(
            _mm256_set1_epi32(0x40000000),
            _mm256_or_si256(_mm256_slli_epi32(upper, 18),
                            _mm256_slli_epi32(lower, 10)));
        small = _mm256_mul_ps(fv, c1292);
        acc = _mm256_sub_ps(_mm256_mul_ps(pw, _mm256_castsi256_ps(mul_bits)),
                            c055);
        m = _mm256_cmp_ps(fv, cutoff, _CMP_LE_OQ);
        out = _mm256_or_ps(_mm256_and_ps(m, small), _mm256_andnot_ps(m, acc));
        _mm256_storeu_ps(v + i, _mm256_castsi256_ps(
            _mm256_or_si256(_mm256_castps_si256(out), sign)));
    }
    _mm256_zeroupper();
    *pos = i;
}

static __m128 fast_log2f_x4(__m128 x) {
    const __m128 one = _mm_set1_ps(1.0f);
    __m128i xb = _mm_castps_si128(x);
    __m128i es = _mm_srai_epi32(_mm_sub_epi32(xb, _mm_set1_epi32(0x3f2aaaab)),
                                23);
    __m128 m = _mm_castsi128_ps(_mm_sub_epi32(xb, _mm_slli_epi32(es, 23)));
    __m128 t = _mm_sub_ps(m, one);
    __m128 yp = _mm_add_ps(_mm_mul_ps(_mm_set1_ps(7.4245873327820566E-01f), t),
                           _mm_set1_ps(1.4287160470083755E+00f));
    __m128 yq = _mm_add_ps(_mm_mul_ps(_mm_set1_ps(1.7409343003366853E-01f), t),
                           _mm_set1_ps(1.0096718572241148E+00f));
    yp = _mm_add_ps(_mm_mul_ps(yp, t), _mm_set1_ps(-1.8503833400518310E-06f));
    yq = _mm_add_ps(_mm_mul_ps(yq, t), _mm_set1_ps(9.9032814277590719E-01f));
    return _mm_add_ps(_mm_div_ps(yp, yq), _mm_cvtepi32_ps(es));
}

static __m128 fast_pow2f_x4(__m128 x) {
    const __m128 one = _mm_set1_ps(1.0f);
    __m128 tf = _mm_cvtepi32_ps(_mm_cvttps_epi32(x));
    __m128 fl = _mm_sub_ps(tf, _mm_and_ps(_mm_cmplt_ps(x, tf), one));
    __m128 ex = _mm_castsi128_ps(_mm_slli_epi32(
        _mm_add_epi32(_mm_cvttps_epi32(fl), _mm_set1_epi32(127)), 23));
    __m128 frac = _mm_sub_ps(x, fl);
    __m128 num = _mm_add_ps(frac, _mm_set1_ps(1.01749063e+01f));
    __m128 den = _mm_sub_ps(_mm_mul_ps(frac, _mm_set1_ps(2.10242958e-01f)),
                            _mm_set1_ps(2.22328856e-02f));
    num = _mm_add_ps(_mm_mul_ps(num, frac), _mm_set1_ps(4.88687798e+01f));
    den = _mm_sub_ps(_mm_mul_ps(den, frac), _mm_set1_ps(1.94414990e+01f));
    num = _mm_add_ps(_mm_mul_ps(num, frac), _mm_set1_ps(9.85506591e+01f));
    den = _mm_add_ps(_mm_mul_ps(den, frac), _mm_set1_ps(9.85506633e+01f));
    return _mm_div_ps(_mm_mul_ps(num, ex), den);
}

JXL_TARGET_AVX2
static __m256 fast_log2f_x8(__m256 x) {
    const __m256 one = _mm256_set1_ps(1.0f);
    __m256i xb = _mm256_castps_si256(x);
    __m256i es = _mm256_srai_epi32(
        _mm256_sub_epi32(xb, _mm256_set1_epi32(0x3f2aaaab)), 23);
    __m256 m = _mm256_castsi256_ps(
        _mm256_sub_epi32(xb, _mm256_slli_epi32(es, 23)));
    __m256 t = _mm256_sub_ps(m, one);
    __m256 yp = _mm256_add_ps(
        _mm256_mul_ps(_mm256_set1_ps(7.4245873327820566E-01f), t),
        _mm256_set1_ps(1.4287160470083755E+00f));
    __m256 yq = _mm256_add_ps(
        _mm256_mul_ps(_mm256_set1_ps(1.7409343003366853E-01f), t),
        _mm256_set1_ps(1.0096718572241148E+00f));
    yp = _mm256_add_ps(
        _mm256_mul_ps(yp, t), _mm256_set1_ps(-1.8503833400518310E-06f));
    yq = _mm256_add_ps(
        _mm256_mul_ps(yq, t), _mm256_set1_ps(9.9032814277590719E-01f));
    return _mm256_add_ps(_mm256_div_ps(yp, yq), _mm256_cvtepi32_ps(es));
}

JXL_TARGET_AVX2
static __m256 fast_pow2f_x8(__m256 x) {
    const __m256 one = _mm256_set1_ps(1.0f);
    __m256 tf = _mm256_cvtepi32_ps(_mm256_cvttps_epi32(x));
    __m256 fl = _mm256_sub_ps(
        tf, _mm256_and_ps(_mm256_cmp_ps(x, tf, _CMP_LT_OQ), one));
    __m256 ex = _mm256_castsi256_ps(_mm256_slli_epi32(
        _mm256_add_epi32(_mm256_cvttps_epi32(fl), _mm256_set1_epi32(127)),
        23));
    __m256 frac = _mm256_sub_ps(x, fl);
    __m256 num = _mm256_add_ps(frac, _mm256_set1_ps(1.01749063e+01f));
    __m256 den = _mm256_sub_ps(
        _mm256_mul_ps(frac, _mm256_set1_ps(2.10242958e-01f)),
        _mm256_set1_ps(2.22328856e-02f));
    num = _mm256_add_ps(
        _mm256_mul_ps(num, frac), _mm256_set1_ps(4.88687798e+01f));
    den = _mm256_sub_ps(
        _mm256_mul_ps(den, frac), _mm256_set1_ps(1.94414990e+01f));
    num = _mm256_add_ps(
        _mm256_mul_ps(num, frac), _mm256_set1_ps(9.85506591e+01f));
    den = _mm256_add_ps(
        _mm256_mul_ps(den, frac), _mm256_set1_ps(9.85506633e+01f));
    return _mm256_div_ps(_mm256_mul_ps(num, ex), den);
}

JXL_TARGET_AVX2
static void tf_bt709_x8(float *v, size_t n, size_t *pos) {
    const __m256 thresh = _mm256_set1_ps(0.018053968510807f);
    const __m256 mul_low = _mm256_set1_ps(4.5f);
    const __m256 mul_hi = _mm256_set1_ps(1.09929682680944f);
    const __m256 sub = _mm256_set1_ps(0.09929682680944f);
    const __m256 e = _mm256_set1_ps(0.45f);
    size_t i = *pos;
    for (; i + 8 <= n; i += 8) {
        __m256 x = _mm256_loadu_ps(v + i);
        __m256 low = _mm256_mul_ps(mul_low, x);
        __m256 hi = _mm256_sub_ps(
            _mm256_mul_ps(
                mul_hi,
                fast_pow2f_x8(_mm256_mul_ps(fast_log2f_x8(x), e))),
            sub);
        __m256 m = _mm256_cmp_ps(x, thresh, _CMP_LE_OQ);
        _mm256_storeu_ps(
            v + i, _mm256_or_ps(_mm256_and_ps(m, low),
                                _mm256_andnot_ps(m, hi)));
    }
    _mm256_zeroupper();
    *pos = i;
}

static void tf_bt709_x4(float *v, size_t n, size_t *pos) {
    const __m128 thresh = _mm_set1_ps(0.018053968510807f);
    const __m128 mul_low = _mm_set1_ps(4.5f);
    const __m128 mul_hi = _mm_set1_ps(1.09929682680944f);
    const __m128 sub = _mm_set1_ps(0.09929682680944f);
    const __m128 e = _mm_set1_ps(0.45f);
    size_t i = *pos;
    for (; i + 4 <= n; i += 4) {
        __m128 x = _mm_loadu_ps(v + i);
        __m128 low = _mm_mul_ps(mul_low, x);
        __m128 hi = _mm_sub_ps(
            _mm_mul_ps(mul_hi, fast_pow2f_x4(_mm_mul_ps(fast_log2f_x4(x), e))),
            sub);
        __m128 m = _mm_cmple_ps(x, thresh);
        _mm_storeu_ps(v + i, _mm_or_ps(_mm_and_ps(m, low),
                                       _mm_andnot_ps(m, hi)));
    }
    *pos = i;
}
#endif

void jxl_linear_to_tf(float *v, size_t n, const jxl_colour_encoding *enc,
                      float intensity_target) {
    size_t i;
    (void)intensity_target;

    if (enc->tf_have_gamma) {
        float g = (float)enc->tf_gamma / 1e7f;
        for (i = 0; i < n; i++) {
            float x = v[i];
            v[i] = x <= 0.0f ? 0.0f : powf(x, g);
        }
        return;
    }
    switch (enc->tf) {
        case JXL_TF_LINEAR:
            break;
        case JXL_TF_709:
            i = 0;
#ifdef JXL_COLOR_SSE2
            if (jxl_has_avx2()) tf_bt709_x8(v, n, &i);
            tf_bt709_x4(v, n, &i);
#endif
            for (; i < n; i++) v[i] = tf_bt709(v[i]);
            break;
        case JXL_TF_PQ:
            for (i = 0; i < n; i++) v[i] = tf_pq(v[i]);
            break;
        case JXL_TF_DCI:
            for (i = 0; i < n; i++) {
                v[i] = v[i] <= 0.0f ? 0.0f : powf(v[i], 1.0f / 2.6f);
            }
            break;
        case JXL_TF_HLG:
            for (i = 0; i < n; i++) v[i] = tf_hlg(v[i]);
            break;
        case JXL_TF_SRGB:
        default:
            i = 0;
#ifdef JXL_COLOR_SSE2
#ifndef JXL_COLOR_FORCE_SRGB_X4
            if (jxl_has_avx2()) tf_srgb_x8(v, n, &i);
#endif
            tf_srgb_x4(v, n, &i);
#endif
            for (; i < n; i++) v[i] = tf_srgb(v[i]);
            break;
    }
}

static void mat3_mul(const float a[9], const float b[9], float out[9]) {
    int i, j, k;
    float t[9];
    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            float s = 0.0f;
            for (k = 0; k < 3; k++) s += a[i * 3 + k] * b[k * 3 + j];
            t[i * 3 + j] = s;
        }
    }
    memcpy(out, t, sizeof(t));
}

static int mat3_inv(const float m[9], float out[9]) {
    float det;
    float a = m[0], b = m[1], c = m[2];
    float d = m[3], e = m[4], f = m[5];
    float g = m[6], h = m[7], i = m[8];
    det = a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g);
    if (det == 0.0f) return -1;
    out[0] = (e * i - f * h) / det;
    out[1] = (c * h - b * i) / det;
    out[2] = (b * f - c * e) / det;
    out[3] = (f * g - d * i) / det;
    out[4] = (a * i - c * g) / det;
    out[5] = (c * d - a * f) / det;
    out[6] = (d * h - e * g) / det;
    out[7] = (b * g - a * h) / det;
    out[8] = (a * e - b * d) / det;
    return 0;
}

static int primaries_to_xyz(const float p[6], float wx, float wy, float out[9]) {
    float prim[9], inv[9], w[3], scale[3];
    int i;
    prim[0] = p[0]; prim[1] = p[2]; prim[2] = p[4];
    prim[3] = p[1]; prim[4] = p[3]; prim[5] = p[5];
    prim[6] = 1.0f - p[0] - p[1];
    prim[7] = 1.0f - p[2] - p[3];
    prim[8] = 1.0f - p[4] - p[5];
    if (wy == 0.0f || mat3_inv(prim, inv) != 0) return -1;
    w[0] = wx / wy;
    w[1] = 1.0f;
    w[2] = (1.0f - wx - wy) / wy;
    for (i = 0; i < 3; i++) {
        scale[i] = inv[i * 3] * w[0] + inv[i * 3 + 1] * w[1] + inv[i * 3 + 2] * w[2];
    }
    for (i = 0; i < 3; i++) {
        out[i * 3] = prim[i * 3] * scale[0];
        out[i * 3 + 1] = prim[i * 3 + 1] * scale[1];
        out[i * 3 + 2] = prim[i * 3 + 2] * scale[2];
    }
    return 0;
}

static int adapt_to_xyz_d50(float wx, float wy, float out[9]) {
    static const float bradford[9] = {
        0.8951f, 0.2664f, -0.1614f,
        -0.7502f, 1.7135f, 0.0367f,
        0.0389f, -0.0685f, 1.0296f
    };
    static const float w50[3] = {0.96422f, 1.0f, 0.82521f};
    float w[3], lms[3], lms50[3], a[9], inv[9];
    int i;
    if (wy == 0.0f) return -1;
    w[0] = wx / wy;
    w[1] = 1.0f;
    w[2] = (1.0f - wx - wy) / wy;
    for (i = 0; i < 3; i++) {
        lms[i] = bradford[i * 3] * w[0] + bradford[i * 3 + 1] * w[1] +
                 bradford[i * 3 + 2] * w[2];
        lms50[i] = bradford[i * 3] * w50[0] + bradford[i * 3 + 1] * w50[1] +
                   bradford[i * 3 + 2] * w50[2];
        if (lms[i] == 0.0f) return -1;
    }
    memset(a, 0, sizeof(a));
    for (i = 0; i < 3; i++) a[i * 3 + i] = lms50[i] / lms[i];
    if (mat3_inv(bradford, inv) != 0) return -1;
    mat3_mul(a, bradford, out);
    mat3_mul(inv, out, out);
    return 0;
}

static void get_primaries(const jxl_colour_encoding *enc, float p[6]) {
    switch (enc->primaries) {
        case JXL_PRIMARIES_2100:
            p[0] = 0.708f; p[1] = 0.292f;
            p[2] = 0.170f; p[3] = 0.797f;
            p[4] = 0.131f; p[5] = 0.046f;
            break;
        case JXL_PRIMARIES_P3:
            p[0] = 0.680f; p[1] = 0.320f;
            p[2] = 0.265f; p[3] = 0.690f;
            p[4] = 0.150f; p[5] = 0.060f;
            break;
        case JXL_PRIMARIES_CUSTOM:
            memcpy(p, enc->prim_xy, 6 * sizeof(float));
            break;
        default:
            p[0] = 0.640f; p[1] = 0.330f;
            p[2] = 0.300f; p[3] = 0.600f;
            p[4] = 0.150f; p[5] = 0.060f;
            break;
    }
}

static void get_white_point(const jxl_colour_encoding *enc, float *wx, float *wy) {
    switch (enc->white_point) {
        case JXL_WP_E: *wx = 1.0f / 3.0f; *wy = 1.0f / 3.0f; break;
        case JXL_WP_DCI: *wx = 0.314f; *wy = 0.351f; break;
        case JXL_WP_CUSTOM: *wx = enc->white_xy[0]; *wy = enc->white_xy[1]; break;
        default: *wx = 0.3127f; *wy = 0.3290f; break;
    }
}

void jxl_opsin_matrix_for(const jxl_image_metadata *meta, float out[9]) {
    const jxl_colour_encoding *enc = &meta->colour;
    float luminances[3] = {0.2126f, 0.7152f, 0.0722f};
    int is_gray = (enc->colour_space == JXLDEC_CS_GRAY);

    memcpy(out, meta->opsin_inv, 9 * sizeof(float));

    if (!is_gray &&
        (enc->primaries != JXL_PRIMARIES_SRGB || enc->white_point != JXL_WP_D65)) {
        static const float srgb_p[6] = {0.640f, 0.330f, 0.300f, 0.600f,
                                        0.150f, 0.060f};
        float srgb_to_xyzd50[9], orig_to_xyz[9], adapt[9], tmp[9];
        float p[6], wx, wy;
        if (primaries_to_xyz(srgb_p, 0.3127f, 0.3290f, tmp) != 0) return;
        if (adapt_to_xyz_d50(0.3127f, 0.3290f, adapt) != 0) return;
        mat3_mul(adapt, tmp, srgb_to_xyzd50);

        get_primaries(enc, p);
        get_white_point(enc, &wx, &wy);
        if (primaries_to_xyz(p, wx, wy, orig_to_xyz) != 0) return;
        memcpy(luminances, orig_to_xyz + 3, 3 * sizeof(float));

        if (meta->xyb_encoded) {
            float xyzd50_to_orig[9], srgb_to_orig[9];
            if (adapt_to_xyz_d50(wx, wy, adapt) != 0) return;
            mat3_mul(adapt, orig_to_xyz, tmp);
            if (mat3_inv(tmp, xyzd50_to_orig) != 0) return;
            mat3_mul(xyzd50_to_orig, srgb_to_xyzd50, srgb_to_orig);
            mat3_mul(srgb_to_orig, meta->opsin_inv, out);
        }
    }

    if (is_gray) {
        float luma[9], tmp[9];
        int i;
        for (i = 0; i < 3; i++) memcpy(luma + i * 3, luminances, 3 * sizeof(float));
        memcpy(tmp, out, sizeof(tmp));
        mat3_mul(luma, tmp, out);
    }
}

#include <math.h>

static int walk_frames(jxl_doc *doc, int frame_no, jxl_fimage *img,
                       int *count_out, jxl_frame_info *info_out);

int jxl_doc_frame_count(jxl_doc *doc) {
    int count = 0;
    if (!doc) return 0;
    if (doc->frame_count > 0) return doc->frame_count;
    if (walk_frames(doc, -1, NULL, &count, NULL) != 0) return 1;
    doc->frame_count = count > 0 ? count : 1;
    return doc->frame_count;
}

int jxl_doc_frame_info(jxl_doc *doc, int frame_no, jxl_frame_info *info) {
    jxl_fimage img;
    int rc;
    if (!doc || !info || frame_no < 0) return -1;
    memset(info, 0, sizeof(*info));
    info->tps_numerator = (int)doc->meta.animation.tps_numerator;
    info->tps_denominator = (int)doc->meta.animation.tps_denominator;
    info->is_last = 1;
    memset(&img, 0, sizeof(img));
    rc = walk_frames(doc, frame_no, &img, NULL, info);
    jxl_fimage_free(doc->ctx, &img);
    return rc;
}

static jxl_format resolve_format(const jxl_image_info *ii, jxl_format fmt) {
    int wide, gray, alpha;
    if (fmt != JXLDEC_FORMAT_NATIVE) return fmt;
    wide = ii->bits_per_sample > 8 || ii->exponent_bits > 0;
    gray = ii->num_color_channels == 1;
    alpha = ii->alpha_bits > 0;
    if (gray) {
        return wide ? (alpha ? JXLDEC_FORMAT_GRAYA16 : JXLDEC_FORMAT_GRAY16)
                    : (alpha ? JXLDEC_FORMAT_GRAYA8 : JXLDEC_FORMAT_GRAY8);
    }
    return wide ? (alpha ? JXLDEC_FORMAT_RGBA64 : JXLDEC_FORMAT_RGB48)
                : (alpha ? JXLDEC_FORMAT_RGBA32 : JXLDEC_FORMAT_RGB24);
}

int jxl_frame_render_info(jxl_doc *doc, int frame_no, jxl_format fmt,
                          jxl_render_info *info) {
    jxl_image_info ii;
    if (!doc || !info) return -1;
    if (jxl_doc_info(doc, &ii) != 0) return -1;
    (void)frame_no;
    info->width = ii.width;
    info->height = ii.height;
    info->format = resolve_format(&ii, fmt);
    return 0;
}

static uint32_t quantize(float v, uint32_t maxval) {
    float s;
    if (!(v > 0.0f)) return 0;
    s = v * (float)maxval + 0.5f;
    if (s >= (float)maxval) return maxval;
    return (uint32_t)s;
}

#if !defined(JXL_RENDER_FORCE_SCALAR) && \
    (defined(__SSE2__) || defined(_M_X64) || defined(_M_AMD64) || \
     (defined(_M_IX86_FP) && _M_IX86_FP >= 2))
#define JXL_RENDER_SSE2 1
#include <emmintrin.h>
#include <immintrin.h>

static __m128i quantize4m(__m128 v, uint32_t maxval) {
    const __m128 vm = _mm_set1_ps((float)maxval);
    const __m128 zero = _mm_setzero_ps();
    __m128 s = _mm_add_ps(_mm_mul_ps(v, vm), _mm_set1_ps(0.5f));
    __m128i t = _mm_cvttps_epi32(s);
    __m128 pos = _mm_cmpgt_ps(v, zero);
    __m128 sat = _mm_cmpge_ps(s, vm);
    __m128i mx = _mm_set1_epi32((int)maxval);
    t = _mm_or_si128(_mm_and_si128(_mm_castps_si128(sat), mx),
                     _mm_andnot_si128(_mm_castps_si128(sat), t));
    return _mm_and_si128(_mm_castps_si128(pos), t);
}

static __m128i quantize4v(const float *src, uint32_t maxval) {
    return quantize4m(_mm_loadu_ps(src), maxval);
}

static __m128i quantize4_stride(const float *src, ptrdiff_t step,
                                uint32_t maxval) {
    return quantize4m(_mm_setr_ps(src[0], src[step], src[2 * step],
                                 src[3 * step]), maxval);
}

static void quantize4(const float *src, uint32_t maxval, uint32_t out[4]) {
    _mm_storeu_si128((__m128i *)out, quantize4v(src, maxval));
}

static void quantize4_stride_store(const float *src, ptrdiff_t step,
                                   uint32_t maxval, uint32_t out[4]) {
    _mm_storeu_si128((__m128i *)out, quantize4_stride(src, step, maxval));
}
#endif

static float plane_sample(const jxl_fplane *p, uint32_t x, uint32_t y,
                          uint32_t w, uint32_t h) {
    uint32_t px, py;
    if (p->w == 0 || p->h == 0) return 0.0f;
    px = (p->w == w) ? x : (uint32_t)((uint64_t)x * p->w / (w ? w : 1));
    py = (p->h == h) ? y : (uint32_t)((uint64_t)y * p->h / (h ? h : 1));
    if (px >= p->w) px = p->w - 1;
    if (py >= p->h) py = p->h - 1;
    return p->data[(size_t)py * p->stride + px];
}

static int plane_is_full(const jxl_fplane *p, uint32_t w, uint32_t h) {
    return p && p->w == w && p->h == h && w != 0 && h != 0;
}

static void unapply_orientation(uint32_t orientation, uint32_t sw, uint32_t sh,
                                uint32_t ox, uint32_t oy, uint32_t *sx,
                                uint32_t *sy) {
    switch (orientation) {
        case 1: *sx = ox; *sy = oy; break;
        case 2: *sx = sw - 1 - ox; *sy = oy; break;
        case 3: *sx = sw - 1 - ox; *sy = sh - 1 - oy; break;
        case 4: *sx = ox; *sy = sh - 1 - oy; break;
        case 5: *sx = oy; *sy = ox; break;
        case 6: *sx = oy; *sy = sh - 1 - ox; break;
        case 7: *sx = sw - 1 - oy; *sy = sh - 1 - ox; break;
        default: *sx = sw - 1 - oy; *sy = ox; break;
    }
}

typedef struct {
    const jxl_fplane *r, *g, *b, *a;
} jxl_out_planes;

static void pick_planes(const jxl_fimage *img, const jxl_image_metadata *meta,
                        jxl_out_planes *op) {
    memset(op, 0, sizeof(*op));
    if (img->nplane == 0) return;
    op->r = &img->plane[0];
    if (img->ncolor >= 3 && img->nplane >= 3) {
        op->g = &img->plane[1];
        op->b = &img->plane[2];
    } else {
        op->g = op->r;
        op->b = op->r;
    }
    if (meta->alpha_index >= 0) {
        uint32_t idx = img->ncolor + (uint32_t)meta->alpha_index;
        if (idx < img->nplane) op->a = &img->plane[idx];
    }
}

#ifdef JXL_RENDER_SSE2
JXL_TARGET_AVX2
static __m256i quantize8_255(const float *src) {
    const __m256 vm = _mm256_set1_ps(255.0f);
    const __m256 zero = _mm256_setzero_ps();
    __m256 v = _mm256_loadu_ps(src);
    __m256 s = _mm256_add_ps(_mm256_mul_ps(v, vm), _mm256_set1_ps(0.5f));
    __m256i t = _mm256_cvttps_epi32(s);
    __m256 pos = _mm256_cmp_ps(v, zero, _CMP_GT_OQ);
    __m256 sat = _mm256_cmp_ps(s, vm, _CMP_GE_OQ);
    __m256i mx = _mm256_set1_epi32(255);
    t = _mm256_or_si256(_mm256_and_si256(_mm256_castps_si256(sat), mx),
                        _mm256_andnot_si256(_mm256_castps_si256(sat), t));
    return _mm256_and_si256(_mm256_castps_si256(pos), t);
}

JXL_TARGET_AVX2
static uint32_t write_rgb8_row_avx2(const float *r, const float *g,
                                    const float *b, const float *a,
                                    uint8_t *dst, uint32_t x, uint32_t w,
                                    int ncomp, int bgr) {
    const __m256i opaque = _mm256_set1_epi32(255);
    const __m256i rgb_shuffle = _mm256_setr_epi8(
        0, 1, 2, 4, 5, 6, 8, 9, 10, 12, 13, 14, -1, -1, -1, -1,
        0, 1, 2, 4, 5, 6, 8, 9, 10, 12, 13, 14, -1, -1, -1, -1);
    const float *c0 = bgr ? b : r;
    const float *c2 = bgr ? r : b;
    uint32_t end = ncomp == 4 ? w : (w >= 8 ? w - 8 : 0);

    for (; x + 8 <= end; x += 8) {
        __m256i qr = quantize8_255(c0 + x);
        __m256i qg = quantize8_255(g + x);
        __m256i qb = quantize8_255(c2 + x);
        __m256i packed = _mm256_or_si256(
            _mm256_or_si256(qr, _mm256_slli_epi32(qg, 8)),
            _mm256_slli_epi32(qb, 16));
        if (ncomp == 4) {
            __m256i qa = a ? quantize8_255(a + x) : opaque;
            packed = _mm256_or_si256(packed, _mm256_slli_epi32(qa, 24));
            _mm256_storeu_si256((__m256i *)(dst + (size_t)x * 4), packed);
        } else {
            __m256i rgb = _mm256_shuffle_epi8(packed, rgb_shuffle);
            _mm_storeu_si128((__m128i *)(dst + (size_t)x * 3),
                             _mm256_castsi256_si128(rgb));
            _mm_storeu_si128((__m128i *)(dst + (size_t)x * 3 + 12),
                             _mm256_extracti128_si256(rgb, 1));
        }
    }
    _mm256_zeroupper();
    return x;
}

JXL_TARGET_AVX2
static void transpose8x8_i32(__m256i p[8]) {
    __m256i t0 = _mm256_unpacklo_epi32(p[0], p[1]);
    __m256i t1 = _mm256_unpackhi_epi32(p[0], p[1]);
    __m256i t2 = _mm256_unpacklo_epi32(p[2], p[3]);
    __m256i t3 = _mm256_unpackhi_epi32(p[2], p[3]);
    __m256i t4 = _mm256_unpacklo_epi32(p[4], p[5]);
    __m256i t5 = _mm256_unpackhi_epi32(p[4], p[5]);
    __m256i t6 = _mm256_unpacklo_epi32(p[6], p[7]);
    __m256i t7 = _mm256_unpackhi_epi32(p[6], p[7]);
    __m256i u0 = _mm256_unpacklo_epi64(t0, t2);
    __m256i u1 = _mm256_unpackhi_epi64(t0, t2);
    __m256i u2 = _mm256_unpacklo_epi64(t1, t3);
    __m256i u3 = _mm256_unpackhi_epi64(t1, t3);
    __m256i u4 = _mm256_unpacklo_epi64(t4, t6);
    __m256i u5 = _mm256_unpackhi_epi64(t4, t6);
    __m256i u6 = _mm256_unpacklo_epi64(t5, t7);
    __m256i u7 = _mm256_unpackhi_epi64(t5, t7);
    p[0] = _mm256_permute2x128_si256(u0, u4, 0x20);
    p[1] = _mm256_permute2x128_si256(u1, u5, 0x20);
    p[2] = _mm256_permute2x128_si256(u2, u6, 0x20);
    p[3] = _mm256_permute2x128_si256(u3, u7, 0x20);
    p[4] = _mm256_permute2x128_si256(u0, u4, 0x31);
    p[5] = _mm256_permute2x128_si256(u1, u5, 0x31);
    p[6] = _mm256_permute2x128_si256(u2, u6, 0x31);
    p[7] = _mm256_permute2x128_si256(u3, u7, 0x31);
}

JXL_TARGET_AVX2
static void write_transposed_rgba8_avx2(const jxl_out_planes *op,
                                        uint32_t orientation,
                                        uint32_t sw, uint32_t sh,
                                        uint8_t *dst, int stride, int bgr) {
    const uint32_t ow = sh, oh = sw;
    const jxl_fplane *rp = bgr ? op->b : op->r;
    const jxl_fplane *bp = bgr ? op->r : op->b;
    const __m256i opaque = _mm256_set1_epi32(255);
    const __m256i reverse = _mm256_setr_epi32(7, 6, 5, 4, 3, 2, 1, 0);
    uint32_t by, bx;

    for (by = 0; by < oh; by += 64) {
        uint32_t ylim = JXL_MIN(by + 64, oh);
        for (bx = 0; bx < ow; bx += 64) {
            uint32_t xlim = JXL_MIN(bx + 64, ow);
            uint32_t oy, ox;
            for (oy = by; oy < ylim; oy += 8) {
                uint32_t sx = (orientation == 7 || orientation == 8)
                                  ? sw - oy - 8 : oy;
                for (ox = bx; ox < xlim; ox += 8) {
                    __m256i p[8];
                    uint32_t i;
                    for (i = 0; i < 8; i++) {
                        uint32_t sy = (orientation == 6 || orientation == 7)
                                          ? sh - 1 - ox - i : ox + i;
                        const float *r = rp->data +
                                         (size_t)sy * rp->stride + sx;
                        const float *g = op->g->data +
                                         (size_t)sy * op->g->stride + sx;
                        const float *b = bp->data +
                                         (size_t)sy * bp->stride + sx;
                        __m256i qr = quantize8_255(r);
                        __m256i qg = quantize8_255(g);
                        __m256i qb = quantize8_255(b);
                        __m256i qa = opaque;
                        if (op->a) {
                            const float *a = op->a->data +
                                             (size_t)sy * op->a->stride + sx;
                            qa = quantize8_255(a);
                        }
                        p[i] = _mm256_or_si256(
                            _mm256_or_si256(qr, _mm256_slli_epi32(qg, 8)),
                            _mm256_or_si256(_mm256_slli_epi32(qb, 16),
                                            _mm256_slli_epi32(qa, 24)));
                        if (orientation == 7 || orientation == 8)
                            p[i] = _mm256_permutevar8x32_epi32(p[i], reverse);
                    }
                    transpose8x8_i32(p);
                    for (i = 0; i < 8; i++) {
                        _mm256_storeu_si256(
                            (__m256i *)(dst + (size_t)(oy + i) * stride +
                                        (size_t)ox * 4),
                            p[i]);
                    }
                }
            }
        }
    }
    _mm256_zeroupper();
}

static void write_transposed_rgba8(const jxl_out_planes *op,
                                   uint32_t orientation,
                                   uint32_t sw, uint32_t sh,
                                   uint8_t *dst, int stride, int bgr) {
    const uint32_t ow = sh, oh = sw;
    const jxl_fplane *rp = bgr ? op->b : op->r;
    const jxl_fplane *bp = bgr ? op->r : op->b;
    uint32_t by, bx;
    const __m128i opaque = _mm_set1_epi32(255);

    for (by = 0; by < oh; by += 64) {
        uint32_t ylim = JXL_MIN(by + 64, oh);
        for (bx = 0; bx < ow; bx += 64) {
            uint32_t xlim = JXL_MIN(bx + 64, ow);
            uint32_t oy, ox;
            for (oy = by; oy < ylim; oy += 4) {
                uint32_t sx = (orientation == 7 || orientation == 8)
                                  ? sw - oy - 4 : oy;
                for (ox = bx; ox < xlim; ox += 4) {
                    __m128i p[4];
                    uint32_t i;
                    for (i = 0; i < 4; i++) {
                        uint32_t sy = (orientation == 6 || orientation == 7)
                                          ? sh - 1 - ox - i : ox + i;
                        const float *r = rp->data +
                                         (size_t)sy * rp->stride + sx;
                        const float *g = op->g->data +
                                         (size_t)sy * op->g->stride + sx;
                        const float *b = bp->data +
                                         (size_t)sy * bp->stride + sx;
                        __m128i qr = quantize4v(r, 255);
                        __m128i qg = quantize4v(g, 255);
                        __m128i qb = quantize4v(b, 255);
                        __m128i qa = opaque;
                        if (op->a) {
                            const float *a = op->a->data +
                                             (size_t)sy * op->a->stride + sx;
                            qa = quantize4v(a, 255);
                        }
                        p[i] = _mm_or_si128(
                            _mm_or_si128(qr, _mm_slli_epi32(qg, 8)),
                            _mm_or_si128(_mm_slli_epi32(qb, 16),
                                         _mm_slli_epi32(qa, 24)));
                        if (orientation == 7 || orientation == 8)
                            p[i] = _mm_shuffle_epi32(
                                p[i], _MM_SHUFFLE(0, 1, 2, 3));
                    }
                    {
                        __m128 q0 = _mm_castsi128_ps(p[0]);
                        __m128 q1 = _mm_castsi128_ps(p[1]);
                        __m128 q2 = _mm_castsi128_ps(p[2]);
                        __m128 q3 = _mm_castsi128_ps(p[3]);
                        _MM_TRANSPOSE4_PS(q0, q1, q2, q3);
                        _mm_storeu_si128(
                            (__m128i *)(dst + (size_t)oy * stride +
                                        (size_t)ox * 4),
                            _mm_castps_si128(q0));
                        _mm_storeu_si128(
                            (__m128i *)(dst + (size_t)(oy + 1) * stride +
                                        (size_t)ox * 4),
                            _mm_castps_si128(q1));
                        _mm_storeu_si128(
                            (__m128i *)(dst + (size_t)(oy + 2) * stride +
                                        (size_t)ox * 4),
                            _mm_castps_si128(q2));
                        _mm_storeu_si128(
                            (__m128i *)(dst + (size_t)(oy + 3) * stride +
                                        (size_t)ox * 4),
                            _mm_castps_si128(q3));
                    }
                }
            }
        }
    }
}
#endif

static int write_pixels(jxl_ctx *ctx, jxl_doc *doc, const jxl_fimage *img,
                        jxl_format fmt, uint8_t *dst, int stride) {
    const jxl_image_metadata *meta = &doc->meta;
    jxl_out_planes op;

    uint32_t sw = doc->size.width, sh = doc->size.height;
    uint32_t ow, oh, ox, oy;
    uint32_t orientation = ctx->keep_orientation ? 1 : meta->orientation;
    int ncomp = 0, wide = 0, has_alpha = 0, gray = 0;
    int direct, reverse_x, transposed;
    uint32_t maxval;
    int bgr = ctx->bgr;
#ifdef JXL_RENDER_SSE2
    const int use_avx2 = jxl_has_avx2();
#endif

    pick_planes(img, meta, &op);
    jxl_apply_orientation_dims(orientation, sw, sh, &ow, &oh);

    switch (fmt) {
        case JXLDEC_FORMAT_GRAY8: ncomp = 1; gray = 1; break;
        case JXLDEC_FORMAT_GRAYA8: ncomp = 2; gray = 1; has_alpha = 1; break;
        case JXLDEC_FORMAT_RGB24: ncomp = 3; break;
        case JXLDEC_FORMAT_RGBA32: ncomp = 4; has_alpha = 1; break;
        case JXLDEC_FORMAT_GRAY16: ncomp = 1; gray = 1; wide = 1; break;
        case JXLDEC_FORMAT_GRAYA16: ncomp = 2; gray = 1; has_alpha = 1; wide = 1; break;
        case JXLDEC_FORMAT_RGB48: ncomp = 3; wide = 1; break;
        case JXLDEC_FORMAT_RGBA64: ncomp = 4; has_alpha = 1; wide = 1; break;
        default:
            JXL_ERR(ctx, "render: unsupported output format %d", (int)fmt);
            return -1;
    }
    maxval = wide ? 65535u : 255u;

#ifdef JXL_RENDER_FORCE_GENERAL_ORIENTATION
    direct = (orientation == 1) &&
#else
    direct = (orientation >= 1 && orientation <= 8) &&
#endif
             plane_is_full(op.r, sw, sh) &&
             (gray || (plane_is_full(op.g, sw, sh) &&
                       plane_is_full(op.b, sw, sh))) &&
             (!op.a || plane_is_full(op.a, sw, sh));
    reverse_x = orientation == 2 || orientation == 3;
    transposed = orientation >= 5;

#ifdef JXL_RENDER_SSE2
    if (direct && transposed && !wide && !gray && ncomp == 4 &&
        (ow & 3u) == 0 && (oh & 3u) == 0) {
        if (use_avx2 && (ow & 7u) == 0 && (oh & 7u) == 0) {
            write_transposed_rgba8_avx2(&op, orientation, sw, sh, dst, stride,
                                        bgr);
            return 0;
        }
        write_transposed_rgba8(&op, orientation, sw, sh, dst, stride, bgr);
        return 0;
    }
#endif

    for (oy = 0; oy < oh; oy++) {
        uint8_t *row8 = dst + (size_t)oy * stride;
        uint16_t *row16 = (uint16_t *)row8;
        const float *pr = NULL, *pg = NULL, *pb = NULL, *pa = NULL;
        ptrdiff_t sr = 1, sg = 1, sb = 1, sa = 1;
        if (direct) {
            uint32_t sx, sy;
            if (transposed) {
                sx = (orientation == 7 || orientation == 8)
                         ? sw - 1 - oy : oy;
                sy = (orientation == 6 || orientation == 7) ? sh - 1 : 0;
            } else {
                sx = 0;
                sy = (orientation == 3 || orientation == 4)
                         ? sh - 1 - oy : oy;
            }
            pr = op.r->data + (size_t)sy * op.r->stride + sx;
            if (!gray) {
                pg = op.g->data + (size_t)sy * op.g->stride + sx;
                pb = op.b->data + (size_t)sy * op.b->stride + sx;
            }
            if (op.a) pa = op.a->data + (size_t)sy * op.a->stride + sx;
            if (transposed) {
                sr = (ptrdiff_t)op.r->stride;
                sg = gray ? sr : (ptrdiff_t)op.g->stride;
                sb = gray ? sr : (ptrdiff_t)op.b->stride;
                sa = op.a ? (ptrdiff_t)op.a->stride : 1;
                if (orientation == 6 || orientation == 7) {
                    sr = -sr; sg = -sg; sb = -sb; sa = -sa;
                }
            }
        }
        ox = 0;
#ifdef JXL_RENDER_SSE2

        if (direct && !wide && gray && ncomp == 1 &&
            !transposed && !reverse_x) {
            for (; ox + 16 <= ow; ox += 16) {
                __m128i q0 = quantize4v(pr + ox, maxval);
                __m128i q1 = quantize4v(pr + ox + 4, maxval);
                __m128i q2 = quantize4v(pr + ox + 8, maxval);
                __m128i q3 = quantize4v(pr + ox + 12, maxval);
                __m128i h0 = _mm_packs_epi32(q0, q1);
                __m128i h1 = _mm_packs_epi32(q2, q3);
                _mm_storeu_si128((__m128i *)(row8 + ox),
                                 _mm_packus_epi16(h0, h1));
            }
        }
        if (use_avx2 && direct && !wide && !gray &&
            !transposed && !reverse_x && (ncomp == 3 || ncomp == 4)) {
            ox = write_rgb8_row_avx2(pr, pg, pb, pa, row8, ox, ow, ncomp, bgr);
        }

        if (direct && !wide && !gray && (ncomp == 3 || ncomp == 4)) {
            uint32_t lim = ncomp == 4 ? ow : (ow >= 4 ? ow - 4 : 0);
            const __m128i amax = _mm_set1_epi32((int)maxval);
            for (; ox + 4 <= lim; ox += 4) {
                uint32_t qx = reverse_x ? ow - ox - 4 : ox;
                __m128i r, g, b, a;
                if (transposed) {
                    r = quantize4_stride(pr + (ptrdiff_t)ox * sr, sr, maxval);
                    g = quantize4_stride(pg + (ptrdiff_t)ox * sg, sg, maxval);
                    b = quantize4_stride(pb + (ptrdiff_t)ox * sb, sb, maxval);
                    a = (pa && ncomp == 4)
                            ? quantize4_stride(pa + (ptrdiff_t)ox * sa, sa, maxval)
                            : amax;
                } else {
                    r = quantize4v(pr + qx, maxval);
                    g = quantize4v(pg + qx, maxval);
                    b = quantize4v(pb + qx, maxval);
                    a = (pa && ncomp == 4) ? quantize4v(pa + qx, maxval)
                                           : amax;
                }
                if (bgr) {
                    __m128i t = r;
                    r = b;
                    b = t;
                }

                __m128i packed = _mm_or_si128(
                    _mm_or_si128(r, _mm_slli_epi32(g, 8)),
                    _mm_or_si128(_mm_slli_epi32(b, 16), _mm_slli_epi32(a, 24)));
                if (!transposed && reverse_x)
                    packed = _mm_shuffle_epi32(packed, _MM_SHUFFLE(0, 1, 2, 3));
                if (ncomp == 4) {
                    _mm_storeu_si128((__m128i *)(row8 + (size_t)ox * 4), packed);
                } else {
                    uint32_t tmp[4];
                    uint32_t j;
                    _mm_storeu_si128((__m128i *)tmp, packed);
                    for (j = 0; j < 4; j++) {
                        memcpy(row8 + (size_t)(ox + j) * 3, &tmp[j], 4);
                    }
                }
            }
        }

        if (direct && wide && !gray && !bgr && !transposed && !reverse_x &&
            (ncomp == 3 || ncomp == 4)) {
            uint32_t lim = ncomp == 4 ? ow : (ow >= 4 ? ow - 4 : 0);
            for (; ox + 4 <= lim; ox += 4) {
                uint32_t qr[4], qg[4], qb[4], qa[4], j;
                quantize4(pr + ox, maxval, qr);
                quantize4(pg + ox, maxval, qg);
                quantize4(pb + ox, maxval, qb);
                if (pa && ncomp == 4) quantize4(pa + ox, maxval, qa);
                for (j = 0; j < 4; j++) {
                    uint64_t packed = (uint64_t)qr[j] |
                                      ((uint64_t)qg[j] << 16) |
                                      ((uint64_t)qb[j] << 32);
                    if (ncomp == 4) {
                        uint32_t a = pa ? qa[j] : maxval;
                        packed |= (uint64_t)a << 48;
                    }
                    memcpy(row8 + (size_t)(ox + j) * ncomp * 2,
                           &packed, sizeof(packed));
                }
            }
        }
        if (direct) {
            uint32_t qr[4], qg[4], qb[4], qa[4];
            for (; ox + 4 <= ow; ox += 4) {
                uint32_t j, qx = reverse_x ? ow - ox - 4 : ox;
                if (transposed) {
                    quantize4_stride_store(pr + (ptrdiff_t)ox * sr, sr,
                                           maxval, qr);
                    if (!gray) {
                        quantize4_stride_store(pg + (ptrdiff_t)ox * sg, sg,
                                               maxval, qg);
                        quantize4_stride_store(pb + (ptrdiff_t)ox * sb, sb,
                                               maxval, qb);
                    }
                    if (pa)
                        quantize4_stride_store(pa + (ptrdiff_t)ox * sa, sa,
                                               maxval, qa);
                } else {
                    quantize4(pr + qx, maxval, qr);
                    if (!gray) {
                        quantize4(pg + qx, maxval, qg);
                        quantize4(pb + qx, maxval, qb);
                    }
                    if (pa) quantize4(pa + qx, maxval, qa);
                }
                for (j = 0; j < 4; j++) {
                    uint32_t comps[4];
                    uint32_t px = ox + j;
                    uint32_t qj = (!transposed && reverse_x) ? 3 - j : j;
                    uint32_t r = qr[qj];
                    uint32_t g = gray ? r : qg[qj];
                    uint32_t b = gray ? r : qb[qj];
                    uint32_t a = pa ? qa[qj] : maxval;
                    if (gray) {
                        comps[0] = r;
                        if (has_alpha) comps[1] = a;
                    } else if (bgr) {
                        comps[0] = b; comps[1] = g; comps[2] = r;
                        if (has_alpha) comps[3] = a;
                    } else {
                        comps[0] = r; comps[1] = g; comps[2] = b;
                        if (has_alpha) comps[3] = a;
                    }
                    if (wide) {
                        uint16_t *o = row16 + (size_t)px * ncomp;
                        o[0] = (uint16_t)comps[0];
                        if (ncomp > 1) o[1] = (uint16_t)comps[1];
                        if (ncomp > 2) o[2] = (uint16_t)comps[2];
                        if (ncomp > 3) o[3] = (uint16_t)comps[3];
                    } else {
                        uint8_t *o = row8 + (size_t)px * ncomp;
                        o[0] = (uint8_t)comps[0];
                        if (ncomp > 1) o[1] = (uint8_t)comps[1];
                        if (ncomp > 2) o[2] = (uint8_t)comps[2];
                        if (ncomp > 3) o[3] = (uint8_t)comps[3];
                    }
                }
            }
        }
#endif
        for (; ox < ow; ox++) {
            uint32_t sx, sy;
            float rv, gv, bv, av = 1.0f;
            uint32_t comps[4];

            if (direct) {
                if (transposed) {
                    ptrdiff_t px = (ptrdiff_t)ox;
                    rv = pr[px * sr];
                    if (gray) {
                        gv = bv = rv;
                    } else {
                        gv = pg[px * sg];
                        bv = pb[px * sb];
                    }
                    if (pa) av = pa[px * sa];
                } else {
                    uint32_t px = reverse_x ? ow - 1 - ox : ox;
                    rv = pr[px];
                    if (gray) {
                        gv = bv = rv;
                    } else {
                        gv = pg[px];
                        bv = pb[px];
                    }
                    if (pa) av = pa[px];
                }
            } else {
                unapply_orientation(orientation, sw, sh, ox, oy, &sx, &sy);
                rv = plane_sample(op.r, sx, sy, sw, sh);
                if (gray) {
                    gv = bv = rv;
                } else {
                    gv = plane_sample(op.g, sx, sy, sw, sh);
                    bv = plane_sample(op.b, sx, sy, sw, sh);
                }
                if (op.a) av = plane_sample(op.a, sx, sy, sw, sh);
            }

            if (gray) {
                comps[0] = quantize(rv, maxval);
                if (has_alpha) comps[1] = quantize(av, maxval);
            } else if (bgr) {
                comps[0] = quantize(bv, maxval);
                comps[1] = quantize(gv, maxval);
                comps[2] = quantize(rv, maxval);
                if (has_alpha) comps[3] = quantize(av, maxval);
            } else {
                comps[0] = quantize(rv, maxval);
                comps[1] = quantize(gv, maxval);
                comps[2] = quantize(bv, maxval);
                if (has_alpha) comps[3] = quantize(av, maxval);
            }

            if (wide) {
                uint16_t *o = row16 + (size_t)ox * ncomp;
                o[0] = (uint16_t)comps[0];
                if (ncomp > 1) o[1] = (uint16_t)comps[1];
                if (ncomp > 2) o[2] = (uint16_t)comps[2];
                if (ncomp > 3) o[3] = (uint16_t)comps[3];
            } else {
                uint8_t *o = row8 + (size_t)ox * ncomp;
                o[0] = (uint8_t)comps[0];
                if (ncomp > 1) o[1] = (uint8_t)comps[1];
                if (ncomp > 2) o[2] = (uint8_t)comps[2];
                if (ncomp > 3) o[3] = (uint8_t)comps[3];
            }
        }
    }
    return 0;
}

static int frame_is_keyframe(const jxl_frame_header *fh) {
    if (fh->frame_type != JXL_FRAME_REGULAR &&
        fh->frame_type != JXL_FRAME_SKIP_PROGRESSIVE)
        return 0;
    return fh->is_last || fh->duration != 0;
}

static int walk_frames(jxl_doc *doc, int frame_no, jxl_fimage *img,
                       int *count_out, jxl_frame_info *info_out) {
    jxl_ctx *ctx = doc->ctx;
    jxl_frame_state st;
    jxl_fimage canvas;
    int canvas_valid = 0;
    jxl_br br;
    int idx = 0;
    int rc = -1;

    memset(&st, 0, sizeof(st));
    memset(&canvas, 0, sizeof(canvas));
    jxl_br_init(&br, doc->container.cs, doc->container.cs_len);
    jxl_br_seek_byte(&br, doc->first_frame_off);

    for (;;) {
        jxl_frame_header fh;
        jxl_toc toc;
        size_t end;
        int last, keyframe, want, apply_ct;
        jxl_fimage tmp;

        memset(&toc, 0, sizeof(toc));
        memset(&tmp, 0, sizeof(tmp));
        if (jxl_read_frame_header(ctx, &br, &doc->size, &doc->meta, &fh) != 0) {
            jxl_frame_header_free(ctx, &fh);
            goto done;
        }
        if (jxl_read_toc(ctx, &br, &fh, &toc) != 0) {
            jxl_frame_header_free(ctx, &fh);
            goto done;
        }
        end = toc.end_off + toc.total_size;
        last = fh.is_last;
        keyframe = frame_is_keyframe(&fh);
        want = keyframe && idx == frame_no;
        apply_ct = want || !fh.save_before_ct;

        if (keyframe) {
            st.visible_frames++;
            st.invisible_frames = 0;
        } else {
            st.invisible_frames++;
        }

        if (count_out && !want) {

            if (keyframe) idx++;
            jxl_toc_free(ctx, &toc);
            jxl_frame_header_free(ctx, &fh);
            if (last || end >= doc->container.cs_len) break;
            jxl_br_seek_byte(&br, end);
            continue;
        }

        if (jxl_frame_decode(ctx, doc, &fh, &toc, &st, apply_ct, &tmp) != 0) {
            jxl_toc_free(ctx, &toc);
            jxl_frame_header_free(ctx, &fh);
            goto done;
        }

        if (fh.frame_type == JXL_FRAME_LF) {
            jxl_fimage_free(ctx, &st.lf_image);
            st.lf_image = tmp;
            st.lf_valid = 1;
            memset(&tmp, 0, sizeof(tmp));
        } else if (!keyframe) {
            if (fh.save_as_reference < 4) {
                uint32_t slot = fh.save_as_reference;
                jxl_fimage_free(ctx, &st.refs[slot]);
                st.refs[slot] = tmp;
                st.refs_valid[slot] = 1;
                memset(&tmp, 0, sizeof(tmp));
            }
        } else {
            uint32_t src = fh.blending.source;
            uint32_t iw = doc->size.width, ih = doc->size.height;
            int cropped = fh.have_crop || tmp.w != iw || tmp.h != ih;
            int needs_canvas = cropped || fh.blending.mode != JXL_BLEND_REPLACE;
            int failed = 0;

            if (!needs_canvas) {
                jxl_fimage_free(ctx, &canvas);
                canvas = tmp;
                memset(&tmp, 0, sizeof(tmp));
            } else {

                jxl_fimage base;
                memset(&base, 0, sizeof(base));
                if (src < 4 && st.refs_valid[src] &&
                    st.refs[src].w == iw && st.refs[src].h == ih) {
                    failed = jxl_fimage_copy(ctx, &base, &st.refs[src]) != 0;
                } else if (canvas_valid && canvas.w == iw && canvas.h == ih) {
                    failed = jxl_fimage_copy(ctx, &base, &canvas) != 0;
                } else {
                    failed = jxl_fimage_blank_like(ctx, &base, &tmp, iw, ih) != 0;
                }
                if (!failed) {
                    jxl_blend_frame(ctx, &base, &tmp, &fh, &doc->meta);
                    jxl_fimage_free(ctx, &canvas);
                    canvas = base;
                } else {
                    jxl_fimage_free(ctx, &base);
                }
                jxl_fimage_free(ctx, &tmp);
            }
            if (failed) {
                jxl_toc_free(ctx, &toc);
                jxl_frame_header_free(ctx, &fh);
                goto done;
            }
            canvas_valid = 1;
            if (fh.save_as_reference < 4 && !fh.is_last) {
                uint32_t slot = fh.save_as_reference;
                jxl_fimage_free(ctx, &st.refs[slot]);
                memset(&st.refs[slot], 0, sizeof(st.refs[slot]));
                if (jxl_fimage_copy(ctx, &st.refs[slot], &canvas) == 0) {
                    st.refs_valid[slot] = 1;
                }
            }
        }

        if (want) {
            *img = canvas;
            memset(&canvas, 0, sizeof(canvas));
            canvas_valid = 0;
            if (info_out) {
                info_out->duration_ticks = (int)fh.duration;
                info_out->is_last = fh.is_last;
            }
            jxl_toc_free(ctx, &toc);
            jxl_frame_header_free(ctx, &fh);
            rc = 0;
            goto done;
        }
        jxl_fimage_free(ctx, &tmp);
        if (keyframe) idx++;
        jxl_toc_free(ctx, &toc);
        jxl_frame_header_free(ctx, &fh);
        if (last || end >= doc->container.cs_len) break;
        jxl_br_seek_byte(&br, end);
    }

    if (count_out) {
        *count_out = idx;
        rc = 0;
    } else {
        JXL_ERR(ctx, "render: no such frame %d", frame_no);
    }

done:
    jxl_fimage_free(ctx, &canvas);
    jxl_frame_state_free(ctx, &st);
    return rc;
}

static int decode_frame_planes(jxl_doc *doc, int frame_no, jxl_fimage *img) {
    return walk_frames(doc, frame_no, img, NULL, NULL);
}

jxl_image *jxl_frame_render(jxl_doc *doc, int frame_no, jxl_format fmt) {
    jxl_ctx *ctx;
    jxl_render_info info;
    jxl_fimage img;
    jxl_image *out = NULL;
    size_t total;

    if (!doc) return NULL;
    ctx = doc->ctx;
    if (jxl_frame_render_info(doc, frame_no, fmt, &info) != 0) return NULL;
    memset(&img, 0, sizeof(img));
    if (decode_frame_planes(doc, frame_no, &img) != 0) return NULL;

    out = (jxl_image *)jxl_calloc(ctx, 1, sizeof(jxl_image));
    if (!out) goto done;
    out->width = info.width;
    out->height = info.height;
    out->format = info.format;
    out->stride = info.width * jxl_format_bpp(info.format);
    if (!jxl_size_mul((size_t)out->stride, (size_t)out->height, &total)) {
        jxl_free(ctx, out);
        out = NULL;
        goto done;
    }

    out->data = (uint8_t *)jxl_malloc(ctx, total ? total : 1);
#ifdef JXL_POISON_UNINIT
    if (out->data) memset(out->data, 0xCD, total ? total : 1);
#endif
    if (!out->data) {
        jxl_free(ctx, out);
        out = NULL;
        goto done;
    }
    if (write_pixels(ctx, doc, &img, info.format, out->data, out->stride) != 0) {
        jxl_image_destroy(ctx, out);
        out = NULL;
    }

done:
    jxl_fimage_free(ctx, &img);
    return out;
}

int jxl_frame_render_into(jxl_doc *doc, int frame_no, jxl_format fmt,
                          uint8_t *dst, int stride) {
    jxl_ctx *ctx;
    jxl_render_info info;
    jxl_fimage img;
    int rc;

    if (!doc || !dst) return -1;
    ctx = doc->ctx;
    if (jxl_frame_render_info(doc, frame_no, fmt, &info) != 0) return -1;
    memset(&img, 0, sizeof(img));
    if (decode_frame_planes(doc, frame_no, &img) != 0) return -1;
    rc = write_pixels(ctx, doc, &img, info.format, dst, stride);
    jxl_fimage_free(ctx, &img);
    return rc;
}

#include <stdlib.h>

jxl_doc *jxl_doc_open(jxl_ctx *ctx, const uint8_t *data, size_t len) {
    jxl_doc *doc;
    jxl_br br;

    if (!ctx || !data) return NULL;
    doc = (jxl_doc *)jxl_calloc(ctx, 1, sizeof(*doc));
    if (!doc) return NULL;
    doc->ctx = ctx;
    doc->data = data;
    doc->len = len;

    if (jxl_container_parse(ctx, data, len, &doc->container) != 0) goto fail;

    jxl_br_init(&br, doc->container.cs, doc->container.cs_len);
    if (jxl_read_image_header(ctx, &br, &doc->size, &doc->meta) != 0) goto fail;

    if (doc->meta.colour.want_icc) {
        if (jxl_read_icc(ctx, &br, &doc->icc, &doc->icc_len) != 0) goto fail;

        doc->meta.colour.primaries = JXL_PRIMARIES_SRGB;
        doc->meta.colour.white_point = JXL_WP_D65;
        doc->meta.colour.tf_have_gamma = 0;
        doc->meta.colour.tf = JXL_TF_LINEAR;
    }
    jxl_br_zero_pad_to_byte(&br);
    if (br.err) {
        JXL_ERR(ctx, "codestream: truncated headers");
        goto fail;
    }
    doc->first_frame_bitpos = jxl_br_bits_read(&br);
    doc->first_frame_off = doc->first_frame_bitpos / 8;
    return doc;

fail:
    jxl_doc_close(doc);
    return NULL;
}

void jxl_doc_close(jxl_doc *doc) {
    jxl_ctx *ctx;
    if (!doc) return;
    ctx = doc->ctx;
    jxl_image_metadata_free(ctx, &doc->meta);
    jxl_container_free(ctx, &doc->container);
    jxl_free(ctx, doc->icc);
    jxl_free(ctx, doc);
}

int jxl_doc_info(jxl_doc *doc, jxl_image_info *info) {
    const jxl_image_metadata *m;
    uint32_t w, h;

    if (!doc || !info) return -1;
    m = &doc->meta;
    memset(info, 0, sizeof(*info));

    if (doc->ctx->keep_orientation) {
        w = doc->size.width;
        h = doc->size.height;
    } else {
        jxl_apply_orientation_dims(m->orientation, doc->size.width,
                                   doc->size.height, &w, &h);
    }
    info->width = (int)w;
    info->height = (int)h;
    info->bits_per_sample = (int)m->bit_depth.bits_per_sample;
    info->exponent_bits = (int)m->bit_depth.exp_bits;
    info->num_color_channels = (m->colour.colour_space == JXLDEC_CS_GRAY) ? 1 : 3;
    info->num_extra_channels = (int)m->num_extra;
    if (m->alpha_index >= 0) {
        const jxl_ec_info *a = &m->ec_info[m->alpha_index];
        info->alpha_bits = (int)a->bit_depth.bits_per_sample;
        info->alpha_premultiplied = a->alpha_associated;
    }
    info->have_animation = m->have_animation;
    info->num_frames = jxl_doc_frame_count(doc);
    info->orientation = (int)m->orientation;
    info->have_preview = m->have_preview;
    info->uses_original_profile = !m->xyb_encoded;
    info->color_space = m->colour.colour_space;
    if (m->have_intr_size) {
        uint32_t iw, ih;
        if (doc->ctx->keep_orientation) {
            iw = m->intrinsic.width;
            ih = m->intrinsic.height;
        } else {
            jxl_apply_orientation_dims(m->orientation, m->intrinsic.width,
                                       m->intrinsic.height, &iw, &ih);
        }
        info->intrinsic_width = (int)iw;
        info->intrinsic_height = (int)ih;
    } else {
        info->intrinsic_width = info->width;
        info->intrinsic_height = info->height;
    }
    return 0;
}

const uint8_t *jxl_doc_icc_profile(jxl_doc *doc, size_t *len) {
    if (!doc || !doc->icc) {
        if (len) *len = 0;
        return NULL;
    }
    if (len) *len = doc->icc_len;
    return doc->icc;
}

int jxl_decode_size(jxl_ctx *ctx, const uint8_t *data, size_t len,
                    int *width, int *height) {
    jxl_doc *doc = jxl_doc_open(ctx, data, len);
    jxl_image_info info;
    int rc;
    if (!doc) return -1;
    rc = jxl_doc_info(doc, &info);
    if (rc == 0) {
        if (width) *width = info.width;
        if (height) *height = info.height;
    }
    jxl_doc_close(doc);
    return rc;
}

jxl_image *jxl_decode(jxl_ctx *ctx, const uint8_t *data, size_t len,
                      jxl_format fmt) {
    jxl_doc *doc = jxl_doc_open(ctx, data, len);
    jxl_image *img;
    if (!doc) return NULL;
    img = jxl_frame_render(doc, 0, fmt);
    jxl_doc_close(doc);
    return img;
}
