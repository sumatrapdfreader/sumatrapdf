
#ifndef HEIC_H
#define HEIC_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void *(*heic_alloc_cb)(void *user, void *ctx, size_t size);
typedef void  (*heic_free_cb)(void *user, void *ctx, void *ptr);

typedef enum {
    HEIC_SEVERITY_DEBUG,
    HEIC_SEVERITY_INFO,
    HEIC_SEVERITY_WARNING,
    HEIC_SEVERITY_ERROR,
    HEIC_SEVERITY_FATAL
} heic_severity;

typedef void (*heic_error_cb)(void *user, heic_severity sev, const char *msg);

typedef struct {
    volatile int requested;
} heic_abort;

void heic_abort_init(heic_abort *ab);
void heic_abort_request(heic_abort *ab);

typedef struct heic_ctx heic_ctx;
typedef struct heic_doc heic_doc;

void heic_init(void);

heic_ctx *heic_ctx_new(heic_alloc_cb alloc, heic_free_cb free_cb,
                       heic_error_cb error, void *user);
void heic_ctx_free(heic_ctx *ctx);

typedef struct {
    uint32_t max_width;
    uint32_t max_height;
    uint64_t max_pixels;
    size_t   max_memory_bytes;
} heic_limits;

void heic_ctx_set_limits(heic_ctx *ctx, const heic_limits *limits);

heic_doc *heic_doc_open(heic_ctx *ctx, const uint8_t *data, size_t len);
void heic_doc_close(heic_doc *doc);

typedef enum {
    HEIC_KIND_UNKNOWN = 0,
    HEIC_KIND_HEIC,
    HEIC_KIND_AVIF,
    HEIC_KIND_HEIF,
    HEIC_KIND_SEQUENCE
} heic_kind;

heic_kind heic_doc_kind(const heic_doc *doc);

typedef struct {
    uint32_t width;
    uint32_t height;
    int      bit_depth;
    int      has_alpha;
    int      has_exif;
    int      has_xmp;
    int      has_thumbnail;
    int      has_gain_map;

    uint16_t color_primaries;
    uint16_t transfer_characteristics;
    uint16_t matrix_coefficients;
    int      full_range;
} heic_image_info;

int heic_doc_info(const heic_doc *doc, heic_image_info *info);

typedef enum {
    HEIC_FORMAT_RGB  = 3,
    HEIC_FORMAT_RGBA = 4,
    HEIC_FORMAT_BGR  = 13,
    HEIC_FORMAT_BGRA = 14
} heic_format;

typedef struct {
    uint32_t    width;
    uint32_t    height;
    heic_format format;
    int         stride;
    uint8_t    *data;
} heic_image;

heic_image *heic_doc_decode(heic_doc *doc, heic_format format);
heic_image *heic_doc_decode_abortable(heic_doc *doc, heic_format format,
                                      heic_abort *ab);
void heic_image_destroy(heic_ctx *ctx, heic_image *img);

size_t heic_doc_output_size(const heic_doc *doc, heic_format format);
int heic_doc_decode_into(heic_doc *doc, heic_format format,
                         uint8_t *buf, size_t buf_size, int stride);

heic_image *heic_doc_decode_thumbnail(heic_doc *doc, heic_format format);

int heic_doc_exif(heic_doc *doc, uint8_t **out, size_t *out_len);

int heic_doc_xmp(heic_doc *doc, uint8_t **out, size_t *out_len);

int heic_doc_icc(heic_doc *doc, uint8_t **out, size_t *out_len);

void heic_free(heic_ctx *ctx, void *p);

const char *heic_version(void);

#ifdef __cplusplus
}
#endif

#endif

#ifndef HEIC_INTERNAL_H
#define HEIC_INTERNAL_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <limits.h>

#ifndef HEIC_MIN
#define HEIC_MIN(a, b) ((a) < (b) ? (a) : (b))
#endif
#ifndef HEIC_MAX
#define HEIC_MAX(a, b) ((a) > (b) ? (a) : (b))
#endif
#ifndef HEIC_CLAMP
#define HEIC_CLAMP(x, lo, hi) ((x) < (lo) ? (lo) : ((x) > (hi) ? (hi) : (x)))
#endif

#define HEIC_COUNTOF(a) (sizeof(a) / sizeof((a)[0]))

#define HEIC_DEFAULT_MAX_WIDTH  16384u
#define HEIC_DEFAULT_MAX_HEIGHT 16384u
#define HEIC_DEFAULT_MAX_PIXELS ((uint64_t)256 * 1000 * 1000)
#define HEIC_DEFAULT_MAX_MEMORY ((size_t)1 * 1024 * 1024 * 1024)

#define HEIC_MAX_ITEMS            65536u
#define HEIC_MAX_PROPERTIES       65536u
#define HEIC_MAX_EXTENTS_PER_ITEM 1024u
#define HEIC_MAX_REFERENCES       65536u
#define HEIC_MAX_REFS_PER_ENTRY   4096u
#define HEIC_MAX_COMPAT_BRANDS    256
#define HEIC_MAX_STRING_LEN       4096
#define HEIC_MAX_NAL_UNIT_SIZE    (16 * 1024 * 1024)
#define HEIC_MAX_ICC_SIZE         (4 * 1024 * 1024)

#define HEIC_UNINIT_SAMPLE 0xFFFFu

typedef uint32_t heic_fourcc;

#define HEIC_FCC(a, b, c, d) \
    ((heic_fourcc)(uint8_t)(a) | ((heic_fourcc)(uint8_t)(b) << 8) | \
     ((heic_fourcc)(uint8_t)(c) << 16) | ((heic_fourcc)(uint8_t)(d) << 24))

#define HEIC_BOX_FTYP HEIC_FCC('f', 't', 'y', 'p')
#define HEIC_BOX_META HEIC_FCC('m', 'e', 't', 'a')
#define HEIC_BOX_HDLR HEIC_FCC('h', 'd', 'l', 'r')
#define HEIC_BOX_PITM HEIC_FCC('p', 'i', 't', 'm')
#define HEIC_BOX_ILOC HEIC_FCC('i', 'l', 'o', 'c')
#define HEIC_BOX_IINF HEIC_FCC('i', 'i', 'n', 'f')
#define HEIC_BOX_INFE HEIC_FCC('i', 'n', 'f', 'e')
#define HEIC_BOX_IPRP HEIC_FCC('i', 'p', 'r', 'p')
#define HEIC_BOX_IPCO HEIC_FCC('i', 'p', 'c', 'o')
#define HEIC_BOX_IPMA HEIC_FCC('i', 'p', 'm', 'a')
#define HEIC_BOX_MDAT HEIC_FCC('m', 'd', 'a', 't')
#define HEIC_BOX_ISPE HEIC_FCC('i', 's', 'p', 'e')
#define HEIC_BOX_HVCC HEIC_FCC('h', 'v', 'c', 'C')
#define HEIC_BOX_HVCB HEIC_FCC('h', 'v', 'c', 'B')
#define HEIC_BOX_COLR HEIC_FCC('c', 'o', 'l', 'r')
#define HEIC_BOX_PIXI HEIC_FCC('p', 'i', 'x', 'i')
#define HEIC_BOX_IREF HEIC_FCC('i', 'r', 'e', 'f')
#define HEIC_BOX_AUXC HEIC_FCC('a', 'u', 'x', 'C')
#define HEIC_BOX_IDAT HEIC_FCC('i', 'd', 'a', 't')
#define HEIC_BOX_CLAP HEIC_FCC('c', 'l', 'a', 'p')
#define HEIC_BOX_IROT HEIC_FCC('i', 'r', 'o', 't')
#define HEIC_BOX_IMIR HEIC_FCC('i', 'm', 'i', 'r')
#define HEIC_BOX_AV1C HEIC_FCC('a', 'v', '1', 'C')
#define HEIC_BOX_UNCC HEIC_FCC('u', 'n', 'c', 'C')
#define HEIC_BOX_CMPC HEIC_FCC('c', 'm', 'p', 'C')
#define HEIC_BOX_CMPD HEIC_FCC('c', 'm', 'p', 'd')
#define HEIC_BOX_ICEF HEIC_FCC('i', 'c', 'e', 'f')
#define HEIC_BOX_CLLI HEIC_FCC('c', 'L', 'L', 'i')
#define HEIC_BOX_MDCV HEIC_FCC('m', 'D', 'C', 'v')
#define HEIC_BOX_MOOV HEIC_FCC('m', 'o', 'o', 'v')
#define HEIC_BOX_TRAK HEIC_FCC('t', 'r', 'a', 'k')
#define HEIC_BOX_TKHD HEIC_FCC('t', 'k', 'h', 'd')
#define HEIC_BOX_MDIA HEIC_FCC('m', 'd', 'i', 'a')
#define HEIC_BOX_MDHD HEIC_FCC('m', 'd', 'h', 'd')
#define HEIC_BOX_MINF HEIC_FCC('m', 'i', 'n', 'f')
#define HEIC_BOX_STBL HEIC_FCC('s', 't', 'b', 'l')
#define HEIC_BOX_STSD HEIC_FCC('s', 't', 's', 'd')
#define HEIC_BOX_STSZ HEIC_FCC('s', 't', 's', 'z')
#define HEIC_BOX_STCO HEIC_FCC('s', 't', 'c', 'o')
#define HEIC_BOX_CO64 HEIC_FCC('c', 'o', '6', '4')
#define HEIC_BOX_STSC HEIC_FCC('s', 't', 's', 'c')
#define HEIC_BOX_STSS HEIC_FCC('s', 't', 's', 's')
#define HEIC_BOX_HVC1 HEIC_FCC('h', 'v', 'c', '1')
#define HEIC_BOX_HEV1 HEIC_FCC('h', 'e', 'v', '1')
#define HEIC_BOX_TREF HEIC_FCC('t', 'r', 'e', 'f')

#define HEIC_TYPE_HVC1 HEIC_FCC('h', 'v', 'c', '1')
#define HEIC_TYPE_AV01 HEIC_FCC('a', 'v', '0', '1')
#define HEIC_TYPE_AVC1 HEIC_FCC('a', 'v', 'c', '1')
#define HEIC_TYPE_JPEG HEIC_FCC('j', 'p', 'e', 'g')
#define HEIC_TYPE_UNCI HEIC_FCC('u', 'n', 'c', 'i')
#define HEIC_TYPE_GRID HEIC_FCC('g', 'r', 'i', 'd')
#define HEIC_TYPE_IOVL HEIC_FCC('i', 'o', 'v', 'l')
#define HEIC_TYPE_IDEN HEIC_FCC('i', 'd', 'e', 'n')
#define HEIC_TYPE_TMAP HEIC_FCC('t', 'm', 'a', 'p')
#define HEIC_TYPE_EXIF HEIC_FCC('E', 'x', 'i', 'f')
#define HEIC_TYPE_MIME HEIC_FCC('m', 'i', 'm', 'e')

#define HEIC_REF_DIMG HEIC_FCC('d', 'i', 'm', 'g')
#define HEIC_REF_AUXL HEIC_FCC('a', 'u', 'x', 'l')
#define HEIC_REF_THMB HEIC_FCC('t', 'h', 'm', 'b')
#define HEIC_REF_CDSC HEIC_FCC('c', 'd', 's', 'c')

static inline uint16_t heic_rb16(const uint8_t *p)
{
    return (uint16_t)((p[0] << 8) | p[1]);
}
static inline uint32_t heic_rb32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}
static inline uint64_t heic_rb64(const uint8_t *p)
{
    return ((uint64_t)heic_rb32(p) << 32) | (uint64_t)heic_rb32(p + 4);
}

static inline heic_fourcc heic_read_fcc(const uint8_t *p)
{
    return HEIC_FCC(p[0], p[1], p[2], p[3]);
}

struct heic_ctx {
    heic_alloc_cb alloc;
    heic_free_cb  free_cb;
    heic_error_cb error;
    void         *user;
    heic_limits   limits;

    void         *dav1d_ctx;
};

void *heic_zalloc(heic_ctx *ctx, size_t size);
void *heic_realloc_buf(heic_ctx *ctx, void *p, size_t old_size, size_t new_size);
void  heic_free_buf(heic_ctx *ctx, void *p);
void  heic_error(heic_ctx *ctx, heic_severity sev, const char *fmt, ...);
int   heic_abort_check(const heic_abort *ab);

typedef struct {
    uint64_t offset;
    uint64_t length;
} heic_extent;

typedef struct {
    uint32_t     item_id;
    uint8_t      construction_method;
    uint64_t     base_offset;
    heic_extent *extents;
    uint32_t     n_extents;
} heic_item_loc;

typedef struct {
    uint32_t    item_id;
    heic_fourcc item_type;
    char       *item_name;
    char       *content_type;
    int         hidden;
} heic_item_info;

typedef struct {
    uint32_t width, height;
} heic_ispe;

typedef struct {
    uint8_t   config_version;
    uint8_t   general_profile_space;
    int       general_tier_flag;
    uint8_t   general_profile_idc;
    uint32_t  general_profile_compatibility_flags;
    uint64_t  general_constraint_indicator_flags;
    uint8_t   general_level_idc;
    uint8_t   chroma_format;
    uint8_t   bit_depth_luma_minus8;
    uint8_t   bit_depth_chroma_minus8;
    uint8_t   length_size_minus_one;

    uint8_t  *nal_blob;
    size_t    nal_blob_len;

    uint8_t **nal_units;
    size_t   *nal_unit_lens;
    int       n_nal_units;
} heic_hvcc;

typedef struct {
    uint8_t  seq_profile;
    uint8_t  seq_level_idx_0;
    int      high_bitdepth;
    int      twelve_bit;
    int      monochrome;
    int      chroma_subsampling_x;
    int      chroma_subsampling_y;
    uint8_t *config_obus;
    size_t   config_obus_len;
} heic_av1c;

typedef enum {
    HEIC_COLR_NONE = 0,
    HEIC_COLR_NCLX,
    HEIC_COLR_ICC
} heic_colr_kind;

typedef struct {
    heic_colr_kind kind;
    uint16_t color_primaries;
    uint16_t transfer_characteristics;
    uint16_t matrix_coefficients;
    int      full_range;
    uint8_t *icc;
    size_t   icc_len;
} heic_colr;

typedef struct {
    uint32_t width_n, width_d;
    uint32_t height_n, height_d;
    int32_t  horiz_off_n;
    uint32_t horiz_off_d;
    int32_t  vert_off_n;
    uint32_t vert_off_d;
} heic_clap;

typedef struct {
    uint16_t angle;
} heic_irot;

typedef struct {
    uint8_t axis;
} heic_imir;

typedef enum {
    HEIC_XFORM_CLAP = 1,
    HEIC_XFORM_IMIR,
    HEIC_XFORM_IROT
} heic_xform_kind;

typedef struct {
    heic_xform_kind kind;
    heic_clap clap;
    heic_imir imir;
    heic_irot irot;
} heic_xform;

typedef struct {
    char    *aux_type;
    uint8_t *subtype_data;
    size_t   subtype_len;
} heic_auxc;

typedef struct {
    uint16_t component_index;
    uint8_t  component_bit_depth_minus_one;
    uint8_t  component_format;
    uint8_t  component_align_size;
} heic_uncc_comp;

typedef struct {
    uint32_t       profile;
    heic_uncc_comp *components;
    int            n_components;
    uint8_t        sampling_type;
    uint8_t        interleave_type;
    uint8_t        block_size;
    uint8_t        components_little_endian;
    uint8_t        block_pad_lsb;
    uint8_t        block_little_endian;
    uint8_t        block_reversed;
    uint8_t        pad_unknown;
    uint32_t       pixel_size;
    uint32_t       row_align_size;
    uint32_t       tile_align_size;
    uint32_t       num_tile_cols_minus_one;
    uint32_t       num_tile_rows_minus_one;
} heic_uncc;

typedef struct {
    heic_fourcc compression_type;
    uint8_t     unit_type;
} heic_cmpc;

typedef struct {
    uint16_t *types;
    int       n_types;
} heic_cmpd;

typedef struct {
    uint64_t offset;
    uint64_t size;
} heic_icef_unit;

typedef struct {
    heic_icef_unit *units;
    int             n_units;
} heic_icef;

typedef enum {
    HEIC_PROP_UNKNOWN = 0,
    HEIC_PROP_ISPE,
    HEIC_PROP_HVCC,
    HEIC_PROP_AV1C,
    HEIC_PROP_COLR,
    HEIC_PROP_CLAP,
    HEIC_PROP_IROT,
    HEIC_PROP_IMIR,
    HEIC_PROP_AUXC,
    HEIC_PROP_UNCC,
    HEIC_PROP_CMPC,
    HEIC_PROP_CMPD,
    HEIC_PROP_ICEF
} heic_prop_kind;

typedef struct {
    heic_prop_kind kind;
    heic_ispe ispe;
    heic_hvcc hvcc;
    heic_av1c av1c;
    heic_colr colr;
    heic_clap clap;
    heic_irot irot;
    heic_imir imir;
    heic_auxc auxc;
    heic_uncc uncc;
    heic_cmpc cmpc;
    heic_cmpd cmpd;
    heic_icef icef;
} heic_property;

typedef struct {
    uint32_t  item_id;
    uint16_t *prop_indices;
    uint8_t  *essential;
    int       n_props;
} heic_ipma;

typedef struct {
    heic_fourcc  ref_type;
    uint32_t     from_item_id;
    uint32_t    *to_item_ids;
    int          n_to;
} heic_iref;

typedef struct {
    heic_ctx          *ctx;
    const uint8_t     *data;
    size_t             len;
    heic_fourcc        brand;
    heic_fourcc       *compatible_brands;
    int                n_compatible_brands;
    uint32_t           primary_item_id;
    heic_item_loc     *item_locations;
    int                n_item_locations;
    heic_item_info    *item_infos;
    int                n_item_infos;
    heic_property     *properties;
    int                n_properties;
    heic_ipma         *property_associations;
    int                n_property_associations;
    heic_iref         *item_references;
    int                n_item_references;
    const uint8_t     *idat;
    size_t             idat_len;
    size_t             mdat_offset;
    size_t             mdat_len;
    int                has_meta;
} heic_container;

typedef struct {
    uint32_t           id;
    heic_fourcc        item_type;
    const char        *name;
    const char        *content_type;
    int                has_dims;
    uint32_t           width, height;
    const heic_hvcc   *hvcc;
    const heic_av1c   *av1c;
    const heic_colr   *colr;
    const heic_clap   *clap;
    const heic_irot   *irot;
    const heic_imir   *imir;
    const heic_auxc   *auxc;
    const heic_uncc   *uncc;
    const heic_cmpc   *cmpc;
    const heic_cmpd   *cmpd;
    const heic_icef   *icef;
    heic_xform         transforms[8];
    int                n_transforms;
} heic_item;

int  heic_container_parse(heic_ctx *ctx, const uint8_t *data, size_t len,
                          heic_container *out, const heic_abort *ab);
void heic_container_free(heic_container *c);
int  heic_container_get_item(const heic_container *c, uint32_t item_id,
                             heic_item *out);

int  heic_container_item_data(const heic_container *c, uint32_t item_id,
                              const uint8_t **out_data, size_t *out_len,
                              int *owned_out);
int  heic_container_find_refs(const heic_container *c, uint32_t from_id,
                              heic_fourcc ref_type, uint32_t *out_ids, int max_out);
int  heic_container_find_aux(const heic_container *c, uint32_t target_id,
                             const char *urn_prefix, uint32_t *out_ids, int max_out);
int  heic_container_find_thumbs(const heic_container *c, uint32_t target_id,
                                uint32_t *out_ids, int max_out);

typedef struct {
    int width, height;
    int crop_left, crop_right, crop_top, crop_bottom;
    int bit_depth;
    int chroma_format;
    int full_range;
    uint8_t matrix_coeffs;
    uint8_t color_primaries;
    uint8_t transfer_characteristics;
    uint16_t *y;
    uint16_t *cb;
    uint16_t *cr;

    uint16_t *a;
    int y_stride;
    int c_stride;
    int a_stride;
    int c_width, c_height;
} heic_frame;

void heic_frame_free(heic_ctx *ctx, heic_frame *f);
int  heic_frame_alloc(heic_ctx *ctx, heic_frame *f, int w, int h,
                      int bit_depth, int chroma_format);

int heic_hevc_decode(heic_ctx *ctx, const heic_hvcc *cfg,
                     const uint8_t *data, size_t len,
                     heic_frame *out, const heic_abort *ab);

void heic_dav1d_ctx_close(heic_ctx *ctx);
int heic_av1_decode(heic_ctx *ctx, const heic_av1c *cfg,
                    const uint8_t *data, size_t len,
                    heic_frame *out, const heic_abort *ab);

int heic_unci_decode(heic_ctx *ctx, const heic_uncc *uncc, const heic_cmpc *cmpc,
                     const heic_cmpd *cmpd, const heic_icef *icef,
                     const uint8_t *data, size_t len, uint32_t width,
                     uint32_t height, heic_frame *out, const heic_abort *ab);

int heic_frame_to_rgb(heic_ctx *ctx, const heic_frame *f, heic_format format,
                      uint8_t *dst, int stride);

struct heic_doc {
    heic_ctx       *ctx;
    const uint8_t  *data;
    size_t          len;
    heic_container  container;
    heic_kind       kind;
};

typedef enum {
    HEIC_NAL_TRAIL_N = 0,
    HEIC_NAL_TRAIL_R = 1,
    HEIC_NAL_TSA_N = 2,
    HEIC_NAL_TSA_R = 3,
    HEIC_NAL_STSA_N = 4,
    HEIC_NAL_STSA_R = 5,
    HEIC_NAL_RADL_N = 6,
    HEIC_NAL_RADL_R = 7,
    HEIC_NAL_RASL_N = 8,
    HEIC_NAL_RASL_R = 9,
    HEIC_NAL_BLA_W_LP = 16,
    HEIC_NAL_BLA_W_RADL = 17,
    HEIC_NAL_BLA_N_LP = 18,
    HEIC_NAL_IDR_W_RADL = 19,
    HEIC_NAL_IDR_N_LP = 20,
    HEIC_NAL_CRA = 21,
    HEIC_NAL_VPS = 32,
    HEIC_NAL_SPS = 33,
    HEIC_NAL_PPS = 34,
    HEIC_NAL_AUD = 35,
    HEIC_NAL_EOS = 36,
    HEIC_NAL_EOB = 37,
    HEIC_NAL_FD = 38,
    HEIC_NAL_PREFIX_SEI = 39,
    HEIC_NAL_SUFFIX_SEI = 40,
    HEIC_NAL_UNKNOWN = 255
} heic_nal_type;

typedef struct {
    heic_nal_type type;
    uint8_t       nuh_layer_id;
    uint8_t       temporal_id;
    const uint8_t *payload;
    size_t         payload_len;
    uint8_t       *owned;

    uint32_t     *ep_positions;
    int           n_ep_positions;
} heic_nal;

int  heic_nal_is_slice(heic_nal_type t);
int  heic_parse_length_prefixed(heic_ctx *ctx, const uint8_t *data, size_t len,
                                int length_size, heic_nal **out, int *out_n);
int  heic_parse_single_nal(heic_ctx *ctx, const uint8_t *data, size_t len,
                           heic_nal *out);
void heic_nal_free(heic_ctx *ctx, heic_nal *n);
void heic_nals_free(heic_ctx *ctx, heic_nal *nals, int n);

typedef struct {
    const uint8_t *data;
    size_t         len;
    size_t         byte_pos;
    int            bit_pos;
    int            error;
} heic_bs;

void     heic_bs_init(heic_bs *bs, const uint8_t *data, size_t len);
int      heic_bs_bit(heic_bs *bs);
uint32_t heic_bs_bits(heic_bs *bs, int n);
uint32_t heic_bs_ue(heic_bs *bs);
int32_t  heic_bs_se(heic_bs *bs);
int      heic_bs_byte_aligned(const heic_bs *bs);
void     heic_bs_byte_align(heic_bs *bs);
size_t   heic_bs_bits_left(const heic_bs *bs);

typedef struct {
    uint8_t  sps_video_parameter_set_id;
    uint8_t  sps_max_sub_layers_minus1;
    int      sps_temporal_id_nesting_flag;

    uint8_t  general_profile_idc;
    uint8_t  general_level_idc;
    uint8_t  chroma_format_idc;
    int      separate_colour_plane_flag;
    uint32_t pic_width_in_luma_samples;
    uint32_t pic_height_in_luma_samples;
    int      conformance_window_flag;
    uint32_t conf_win_left_offset;
    uint32_t conf_win_right_offset;
    uint32_t conf_win_top_offset;
    uint32_t conf_win_bottom_offset;
    uint8_t  bit_depth_luma_minus8;
    uint8_t  bit_depth_chroma_minus8;
    uint8_t  log2_max_pic_order_cnt_lsb_minus4;
    uint8_t  log2_min_luma_coding_block_size_minus3;
    uint8_t  log2_diff_max_min_luma_coding_block_size;
    uint8_t  log2_min_luma_transform_block_size_minus2;
    uint8_t  log2_diff_max_min_luma_transform_block_size;
    uint8_t  max_transform_hierarchy_depth_inter;
    uint8_t  max_transform_hierarchy_depth_intra;
    int      scaling_list_enabled_flag;
    int      amp_enabled_flag;
    int      sample_adaptive_offset_enabled_flag;
    int      pcm_enabled_flag;
    int      pcm_loop_filter_disabled_flag;
    uint8_t  pcm_sample_bit_depth_luma_minus1;
    uint8_t  pcm_sample_bit_depth_chroma_minus1;
    uint8_t  log2_min_pcm_luma_coding_block_size_minus3;
    uint8_t  log2_diff_max_min_pcm_luma_coding_block_size;
    int      long_term_ref_pics_present_flag;
    int      sps_temporal_mvp_enabled_flag;
    int      strong_intra_smoothing_enabled_flag;
    int      vui_parameters_present_flag;
    int      video_signal_type_present_flag;
    int      video_full_range_flag;
    int      colour_description_present_flag;
    uint8_t  colour_primaries;
    uint8_t  transfer_characteristics;
    uint8_t  matrix_coeffs;
    int      tiles_enabled;

    uint8_t  log2_min_cb_size;
    uint8_t  log2_ctb_size;
    uint8_t  log2_min_tb_size;
    uint8_t  log2_max_tb_size;
    uint32_t ctb_width;
    uint32_t ctb_height;
    uint32_t pic_width_in_ctbs;
    uint32_t pic_height_in_ctbs;
    uint32_t pic_size_in_ctbs;
} heic_sps;

typedef struct {
    uint8_t  pps_pic_parameter_set_id;
    uint8_t  pps_seq_parameter_set_id;
    int      dependent_slice_segments_enabled_flag;
    int      output_flag_present_flag;
    uint8_t  num_extra_slice_header_bits;
    int      sign_data_hiding_enabled_flag;
    int      cabac_init_present_flag;
    uint8_t  num_ref_idx_l0_default_active_minus1;
    uint8_t  num_ref_idx_l1_default_active_minus1;
    int8_t   init_qp_minus26;
    int      constrained_intra_pred_flag;
    int      transform_skip_enabled_flag;
    int      cu_qp_delta_enabled_flag;
    uint8_t  diff_cu_qp_delta_depth;
    int8_t   pps_cb_qp_offset;
    int8_t   pps_cr_qp_offset;
    int      pps_slice_chroma_qp_offsets_present_flag;
    int      weighted_pred_flag;
    int      weighted_bipred_flag;
    int      transquant_bypass_enabled_flag;
    int      tiles_enabled_flag;
    int      entropy_coding_sync_enabled_flag;
    uint16_t num_tile_columns_minus1;
    uint16_t num_tile_rows_minus1;
    int      uniform_spacing_flag;
    uint16_t *column_width_minus1;
    uint16_t *row_height_minus1;
    int      loop_filter_across_tiles_enabled_flag;
    int      pps_loop_filter_across_slices_enabled_flag;
    int      deblocking_filter_control_present_flag;
    int      deblocking_filter_override_enabled_flag;
    int      pps_deblocking_filter_disabled_flag;
    int8_t   pps_beta_offset_div2;
    int8_t   pps_tc_offset_div2;
    int      pps_scaling_list_data_present_flag;
    int      lists_modification_present_flag;
    uint8_t  log2_parallel_merge_level_minus2;
    int      slice_segment_header_extension_present_flag;
} heic_pps;

int heic_parse_sps(heic_ctx *ctx, const uint8_t *rbsp, size_t len, heic_sps *out);
int heic_parse_pps(heic_ctx *ctx, const uint8_t *rbsp, size_t len, heic_pps *out);
void heic_pps_free(heic_ctx *ctx, heic_pps *pps);

#define HEIC_NUM_CONTEXTS 170

#define HEIC_CTX_SPLIT_CU_FLAG              0
#define HEIC_CTX_CU_TRANSQUANT_BYPASS_FLAG  3
#define HEIC_CTX_CU_SKIP_FLAG               4
#define HEIC_CTX_PRED_MODE_FLAG             8
#define HEIC_CTX_PART_MODE                  9
#define HEIC_CTX_PREV_INTRA_LUMA_PRED_FLAG  13
#define HEIC_CTX_INTRA_CHROMA_PRED_MODE     14
#define HEIC_CTX_SPLIT_TRANSFORM_FLAG       28
#define HEIC_CTX_CBF_LUMA                   31
#define HEIC_CTX_CBF_CBCR                   33
#define HEIC_CTX_TRANSFORM_SKIP_FLAG        38
#define HEIC_CTX_LAST_SIG_COEFF_X_PREFIX    40
#define HEIC_CTX_LAST_SIG_COEFF_Y_PREFIX    58
#define HEIC_CTX_CODED_SUB_BLOCK_FLAG       76
#define HEIC_CTX_SIG_COEFF_FLAG             80
#define HEIC_CTX_COEFF_ABS_LEVEL_GREATER1   124
#define HEIC_CTX_COEFF_ABS_LEVEL_GREATER2   148
#define HEIC_CTX_SAO_MERGE_FLAG             154
#define HEIC_CTX_SAO_TYPE_IDX               155
#define HEIC_CTX_CU_QP_DELTA_ABS            156

typedef struct {
    uint8_t state;
    uint8_t mps;
} heic_ctx_model;

typedef struct {
    const uint8_t *data;
    size_t         len;
    size_t         byte_pos;
    uint32_t       range;
    uint32_t       value;
    int            bits_needed;
    uint32_t       overread_bytes;
    int            error;
} heic_cabac;

void heic_ctx_model_init(heic_ctx_model *m, uint8_t init_value, int slice_qp);
void heic_cabac_init_contexts(heic_ctx_model *ctx, int slice_type, int cabac_init_flag,
                              int slice_qp);
int  heic_cabac_new(heic_cabac *c, const uint8_t *data, size_t len);
void heic_cabac_reinit(heic_cabac *c);
void heic_cabac_seek(heic_cabac *c, size_t byte_pos);
int  heic_cabac_overread(const heic_cabac *c);
int  heic_cabac_decode_bin(heic_cabac *c, heic_ctx_model *ctx);
int  heic_cabac_decode_bypass(heic_cabac *c);
uint32_t heic_cabac_decode_bypass_bits(heic_cabac *c, int n);
int  heic_cabac_decode_terminate(heic_cabac *c);
uint32_t heic_cabac_decode_egk(heic_cabac *c, int k);

#define HEIC_SLICE_B 0
#define HEIC_SLICE_P 1
#define HEIC_SLICE_I 2

typedef struct {
    int      first_slice_segment_in_pic_flag;
    int      no_output_of_prior_pics_flag;
    uint8_t  pps_id;
    int      dependent_slice_segment_flag;
    uint32_t slice_segment_address;
    int      slice_type;
    int      pic_output_flag;
    uint8_t  colour_plane_id;
    uint32_t slice_pic_order_cnt_lsb;
    int      slice_sao_luma_flag;
    int      slice_sao_chroma_flag;
    int8_t   slice_qp_delta;
    int8_t   slice_cb_qp_offset;
    int8_t   slice_cr_qp_offset;
    int      deblocking_filter_override_flag;
    int      slice_deblocking_filter_disabled_flag;
    int8_t   slice_beta_offset_div2;
    int8_t   slice_tc_offset_div2;
    int      slice_loop_filter_across_slices_enabled_flag;
    uint32_t  num_entry_point_offsets;
    uint32_t *entry_point_offsets;
    int       cabac_init_flag;
    int       slice_qp_y;
    size_t    data_offset;
} heic_slice_header;

void heic_slice_header_free(heic_ctx *ctx, heic_slice_header *sh);

int heic_parse_slice_header(heic_ctx *ctx, const heic_nal *nal,
                            const heic_sps *sps, const heic_pps *pps,
                            heic_slice_header *out);

#define HEIC_MAX_COEFF 1024

enum {
    HEIC_SCAN_DIAG = 0,
    HEIC_SCAN_HORIZ = 1,
    HEIC_SCAN_VERT = 2
};

typedef struct {
    int16_t coeffs[HEIC_MAX_COEFF];
    uint8_t log2_size;
    uint16_t num_nonzero;
} heic_coeff_buf;

int heic_get_scan_order(uint8_t log2_size, uint8_t intra_mode, uint8_t c_idx,
                        int chroma_444);

int heic_decode_residual(heic_cabac *cabac, heic_ctx_model *ctx,
                         uint8_t log2_size, uint8_t c_idx, int scan_order,
                         int sign_data_hiding, int cu_transquant_bypass,
                         int transform_skip_enabled,
                         heic_coeff_buf *out, int *transform_skip);

void heic_idst4(const int16_t *coeffs , int16_t *out , int bit_depth);
void heic_idct4(const int16_t *coeffs, int16_t *out, int bit_depth);
void heic_idct8(const int16_t *coeffs , int16_t *out, int bit_depth);
void heic_idct16(const int16_t *coeffs , int16_t *out, int bit_depth);
void heic_idct32(const int16_t *coeffs , int16_t *out, int bit_depth);

int32_t *heic_idct_scratch_buf(void);
void heic_dequantize(int16_t *coeffs, int n, int qp, int bit_depth,
                     uint8_t log2_tr_size);
void heic_inverse_transform(const int16_t *coeffs, int16_t *output, int size,
                            int bit_depth, int is_intra_4x4_luma);
void heic_add_residual(uint16_t *plane, int stride, int x0, int y0,
                       const int16_t *residual, int size, int max_val);

void heic_simd_init(void);
int  heic_simd_enabled(void);
int  heic_simd_idct8(const int16_t *coeffs, int16_t *out, int bit_depth);
int  heic_simd_idct16(const int16_t *coeffs, int16_t *out, int bit_depth);
int  heic_simd_idct32(const int16_t *coeffs, int16_t *out, int bit_depth);
int  heic_simd_add_residual(uint16_t *plane, int stride, int x0, int y0,
                            const int16_t *residual, int size, int max_val);

int  heic_simd_ycc_444_row(const uint16_t *yp, const uint16_t *cbp, const uint16_t *crp,
                           uint8_t *row, int w, int full,
                           const int32_t yv[256], const int32_t cr_r[256],
                           const int32_t cb_g[256], const int32_t cr_g[256],
                           const int32_t cb_b[256]);

int  heic_simd_chroma_edge4(uint16_t *plane, int stride, size_t base_q0, int across,
                            int tc, int max_val, int along_is_stride);

int  heic_simd_luma_filter4(uint16_t *plane, size_t base_p, size_t base_q,
                            size_t step_along, size_t step_across, int strong,
                            int d_ep, int d_eq, int tc, int max_val);

int  heic_simd_dequant(int16_t *coeffs, int n, int32_t combined, int shift);

int  heic_simd_intra_ang_row(uint16_t *dst, const int32_t *ref, int n, int a, int b,
                             int max_val);

int  heic_simd_u16_to_i32_avail(const uint16_t *src, int32_t *border, int *avail, int n);

int  heic_simd_border_top_ext(const uint16_t *src, int32_t *border, int *avail, int n);

int  heic_simd_intra_ang_row_var(uint16_t *dst, const int32_t *ref, int n, int row_base,
                                 int32_t angle, int max_val);

int  heic_simd_sao_band_row(uint16_t *row, int x0, int x1, int band_shift,
                            const int16_t band_table[32], int max_val);

int  heic_simd_sao_edge_h_row(const uint16_t *srow, uint16_t *drow, int x0, int x1,
                              const int offset_table[5], int max_val);

int  heic_simd_sao_edge_v_row(const uint16_t *src, uint16_t *dst, int stride, int y,
                              int x0, int x1, const int offset_table[5], int max_val);

void heic_fill_mpm(uint8_t cand_a, uint8_t cand_b, uint8_t mpm[3]);

int heic_predict_intra(heic_frame *frame, uint32_t x, uint32_t y,
                       uint8_t log2_size, uint8_t mode, uint8_t c_idx,
                       int strong_intra_smoothing);

typedef struct {
    uint8_t sao_type_idx[3];
    int16_t sao_offset_val[3][4];
    uint8_t sao_band_position[3];
    uint8_t sao_eo_class[3];
} heic_sao_info;

void heic_apply_sao(heic_ctx *ctx, heic_frame *frame, const heic_sao_info *map,
                    uint32_t width_ctbs, uint32_t height_ctbs, uint32_t ctb_size);

void heic_mark_tu_boundary(uint8_t *flags, uint32_t deblock_stride, uint32_t map_n,
                           uint32_t x, uint32_t y, uint32_t size);
void heic_store_deblock_qp(int8_t *qp_map, uint32_t deblock_stride, uint32_t map_n,
                           uint32_t x, uint32_t y, uint32_t size, int8_t qp);
void heic_mark_pb_boundary(uint8_t *flags, uint32_t deblock_stride, uint32_t map_n,
                           uint32_t x, uint32_t y, uint32_t width, uint32_t height,
                           int vertical);

void heic_apply_deblock(heic_frame *frame, const uint8_t *flags, const int8_t *qp_map,
                        uint32_t deblock_stride, int beta_offset, int tc_offset,
                        int cb_qp_offset, int cr_qp_offset);

int heic_hevc_decode_slice_i(heic_ctx *ctx, const heic_sps *sps,
                             const heic_pps *pps, const heic_slice_header *sh,
                             const uint8_t *data, size_t len,
                             const uint32_t *ep_positions, int n_ep,
                             heic_frame *out, const heic_abort *ab);

int heic_decode_primary(heic_doc *doc, heic_format format,
                        heic_image **out_img, uint8_t *into, size_t into_size,
                        int into_stride, const heic_abort *ab);

#endif

static void *default_alloc(void *user, void *ctx, size_t size)
{
    (void)user;
    (void)ctx;
    return malloc(size);
}

static void default_free(void *user, void *ctx, void *p)
{
    (void)user;
    (void)ctx;
    free(p);
}

static int g_inited;

void heic_init(void)
{
    g_inited = 1;
    heic_simd_init();
}

const char *heic_version(void)
{
    return "0.1.0-dev";
}

void heic_abort_init(heic_abort *ab)
{
    if (ab) ab->requested = 0;
}

void heic_abort_request(heic_abort *ab)
{
    if (ab) ab->requested = 1;
}

int heic_abort_check(const heic_abort *ab)
{
    return ab && ab->requested;
}

heic_ctx *heic_ctx_new(heic_alloc_cb alloc, heic_free_cb free_cb,
                       heic_error_cb error, void *user)
{
    heic_alloc_cb a = alloc ? alloc : default_alloc;
    heic_free_cb f = free_cb ? free_cb : default_free;
    heic_ctx *ctx = (heic_ctx *)a(user, NULL, sizeof(heic_ctx));
    if (!ctx) return NULL;
    memset(ctx, 0, sizeof(*ctx));
    ctx->alloc = a;
    ctx->free_cb = f;
    ctx->error = error;
    ctx->user = user;
    ctx->limits.max_width = HEIC_DEFAULT_MAX_WIDTH;
    ctx->limits.max_height = HEIC_DEFAULT_MAX_HEIGHT;
    ctx->limits.max_pixels = HEIC_DEFAULT_MAX_PIXELS;
    ctx->limits.max_memory_bytes = HEIC_DEFAULT_MAX_MEMORY;
    return ctx;
}

void heic_ctx_free(heic_ctx *ctx)
{
    if (!ctx) return;
    heic_dav1d_ctx_close(ctx);
    {
        heic_free_cb f = ctx->free_cb;
        void *user = ctx->user;
        f(user, NULL, ctx);
    }
}

void heic_ctx_set_limits(heic_ctx *ctx, const heic_limits *limits)
{
    if (!ctx || !limits) return;
    if (limits->max_width) ctx->limits.max_width = limits->max_width;
    if (limits->max_height) ctx->limits.max_height = limits->max_height;
    if (limits->max_pixels) ctx->limits.max_pixels = limits->max_pixels;
    if (limits->max_memory_bytes) ctx->limits.max_memory_bytes = limits->max_memory_bytes;
}

void *heic_zalloc(heic_ctx *ctx, size_t size)
{
    void *p;
    if (!ctx || size == 0) return NULL;
    p = ctx->alloc(ctx->user, ctx, size);
    if (p) memset(p, 0, size);
    return p;
}

void *heic_realloc_buf(heic_ctx *ctx, void *p, size_t old_size, size_t new_size)
{
    void *q;
    if (!ctx) return NULL;
    if (new_size == 0) {
        heic_free_buf(ctx, p);
        return NULL;
    }
    q = ctx->alloc(ctx->user, ctx, new_size);
    if (!q) return NULL;
    if (p) {
        size_t n = old_size < new_size ? old_size : new_size;
        memcpy(q, p, n);
        if (new_size > old_size) memset((uint8_t *)q + old_size, 0, new_size - old_size);
        heic_free_buf(ctx, p);
    } else {
        memset(q, 0, new_size);
    }
    return q;
}

void heic_free_buf(heic_ctx *ctx, void *p)
{
    if (!ctx || !p) return;
    ctx->free_cb(ctx->user, ctx, p);
}

void heic_free(heic_ctx *ctx, void *p)
{
    heic_free_buf(ctx, p);
}

void heic_error(heic_ctx *ctx, heic_severity sev, const char *fmt, ...)
{
    char buf[512];
    va_list ap;
    if (!ctx || !ctx->error || !fmt) return;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    buf[sizeof(buf) - 1] = '\0';
    ctx->error(ctx->user, sev, buf);
}

void heic_image_destroy(heic_ctx *ctx, heic_image *img)
{
    if (!img) return;
    if (ctx) heic_free_buf(ctx, img->data);
    else free(img->data);
    if (ctx) heic_free_buf(ctx, img);
    else free(img);
}

void heic_frame_free(heic_ctx *ctx, heic_frame *f)
{
    if (!f) return;
    heic_free_buf(ctx, f->y);
    heic_free_buf(ctx, f->cb);
    heic_free_buf(ctx, f->cr);
    heic_free_buf(ctx, f->a);
    memset(f, 0, sizeof(*f));
}

int heic_frame_alloc(heic_ctx *ctx, heic_frame *f, int w, int h,
                     int bit_depth, int chroma_format)
{
    int cw, ch;
    size_t y_n, c_n;
    if (!ctx || !f || w <= 0 || h <= 0) return -1;
    if (w > (int)ctx->limits.max_width || h > (int)ctx->limits.max_height) return -1;
    if ((uint64_t)w * (uint64_t)h > ctx->limits.max_pixels) return -1;

    memset(f, 0, sizeof(*f));
    f->width = w;
    f->height = h;
    f->bit_depth = bit_depth;
    f->chroma_format = chroma_format;
    f->y_stride = w;

    switch (chroma_format) {
    case 0: cw = 0; ch = 0; break;
    case 1: cw = (w + 1) / 2; ch = (h + 1) / 2; break;
    case 2: cw = (w + 1) / 2; ch = h; break;
    case 3: cw = w; ch = h; break;
    default: return -1;
    }
    f->c_width = cw;
    f->c_height = ch;
    f->c_stride = cw > 0 ? cw : 0;

    y_n = (size_t)w * (size_t)h * sizeof(uint16_t);

    f->y = (uint16_t *)ctx->alloc(ctx->user, ctx, y_n);
    if (!f->y) return -1;

    memset(f->y, 0xFF, y_n);
    if (cw > 0 && ch > 0) {
        c_n = (size_t)cw * (size_t)ch * sizeof(uint16_t);
        f->cb = (uint16_t *)ctx->alloc(ctx->user, ctx, c_n);
        f->cr = (uint16_t *)ctx->alloc(ctx->user, ctx, c_n);
        if (!f->cb || !f->cr) {
            heic_frame_free(ctx, f);
            return -1;
        }
        memset(f->cb, 0xFF, c_n);
        memset(f->cr, 0xFF, c_n);
    }
    return 0;
}

typedef struct {
    const uint8_t *data;
    size_t         len;
    size_t         offset;
} heic_box_iter;

typedef struct {
    heic_fourcc    type;
    uint64_t       size;
    size_t         content_off;
    const uint8_t *content;
    size_t         content_len;
} heic_box;

static void box_iter_init(heic_box_iter *it, const uint8_t *data, size_t len)
{
    it->data = data;
    it->len = len;
    it->offset = 0;
}

static int box_iter_next(heic_box_iter *it, heic_box *out)
{
    size_t off, header_size;
    uint32_t size32;
    uint64_t size;
    heic_fourcc type;
    size_t size_usize, box_end;

    if (!it || !out) return 0;
    off = it->offset;
    if (off + 8 > it->len) return 0;

    size32 = heic_rb32(it->data + off);
    type = heic_read_fcc(it->data + off + 4);

    if (size32 == 1) {
        if (off + 16 > it->len) return 0;
        size = heic_rb64(it->data + off + 8);
        header_size = 16;
    } else if (size32 == 0) {
        size = (uint64_t)(it->len - off);
        header_size = 8;
    } else {
        size = size32;
        header_size = 8;
    }

    if (size < header_size) return 0;
    size_usize = (size_t)size;
    if ((uint64_t)size_usize != size) return 0;
    box_end = off + size_usize;
    if (box_end > it->len) return 0;

    out->type = type;
    out->size = size;
    out->content_off = off + header_size;
    out->content = it->data + off + header_size;
    out->content_len = size_usize - header_size;
    it->offset = box_end;
    return 1;
}

static uint64_t read_sized_int(const uint8_t *data, size_t len, size_t *pos, size_t size)
{
    uint64_t value = 0;
    size_t i;
    if (size == 0 || *pos + size > len) return 0;
    for (i = 0; i < size; i++) value = (value << 8) | data[*pos + i];
    *pos += size;
    return value;
}

static char *dup_cstr_z(heic_ctx *ctx, const uint8_t *p, size_t maxlen)
{
    size_t n = 0;
    char *s;
    while (n < maxlen && p[n] != 0) n++;
    if (n > HEIC_MAX_STRING_LEN) return NULL;
    s = (char *)heic_zalloc(ctx, n + 1);
    if (!s) return NULL;
    if (n) memcpy(s, p, n);
    s[n] = '\0';
    return s;
}

static int is_heif_brand(heic_fourcc b)
{
    return b == HEIC_FCC('h', 'e', 'i', 'c') || b == HEIC_FCC('h', 'e', 'i', 'x') ||
           b == HEIC_FCC('h', 'e', 'v', 'c') || b == HEIC_FCC('h', 'e', 'v', 'x') ||
           b == HEIC_FCC('m', 'i', 'f', '1') || b == HEIC_FCC('m', 's', 'f', '1') ||
           b == HEIC_FCC('m', 'i', 'f', '2') || b == HEIC_FCC('m', 'i', 'f', '3') ||
           b == HEIC_FCC('a', 'v', 'i', 'f') || b == HEIC_FCC('a', 'v', 'i', 's');
}

static void free_hvcc(heic_ctx *ctx, heic_hvcc *h)
{
    int i;
    if (!h) return;
    for (i = 0; i < h->n_nal_units; i++) heic_free_buf(ctx, h->nal_units[i]);
    heic_free_buf(ctx, h->nal_units);
    heic_free_buf(ctx, h->nal_unit_lens);
    heic_free_buf(ctx, h->nal_blob);
    memset(h, 0, sizeof(*h));
}

static void free_property(heic_ctx *ctx, heic_property *p)
{
    if (!p) return;
    if (p->kind == HEIC_PROP_HVCC) free_hvcc(ctx, &p->hvcc);
    if (p->kind == HEIC_PROP_AV1C) {
        heic_free_buf(ctx, p->av1c.config_obus);
        memset(&p->av1c, 0, sizeof(p->av1c));
    }
    if (p->kind == HEIC_PROP_COLR) {
        heic_free_buf(ctx, p->colr.icc);
        memset(&p->colr, 0, sizeof(p->colr));
    }
    if (p->kind == HEIC_PROP_AUXC) {
        heic_free_buf(ctx, p->auxc.aux_type);
        heic_free_buf(ctx, p->auxc.subtype_data);
        memset(&p->auxc, 0, sizeof(p->auxc));
    }
    if (p->kind == HEIC_PROP_UNCC) {
        heic_free_buf(ctx, p->uncc.components);
        memset(&p->uncc, 0, sizeof(p->uncc));
    }
    if (p->kind == HEIC_PROP_CMPD) {
        heic_free_buf(ctx, p->cmpd.types);
        memset(&p->cmpd, 0, sizeof(p->cmpd));
    }
    if (p->kind == HEIC_PROP_ICEF) {
        heic_free_buf(ctx, p->icef.units);
        memset(&p->icef, 0, sizeof(p->icef));
    }
    p->kind = HEIC_PROP_UNKNOWN;
}

static void free_property_associations(heic_ctx *ctx, heic_container *c)
{
    int i;
    for (i = 0; i < c->n_property_associations; i++) {
        heic_free_buf(ctx, c->property_associations[i].prop_indices);
        heic_free_buf(ctx, c->property_associations[i].essential);
    }
    heic_free_buf(ctx, c->property_associations);
    c->property_associations = NULL;
    c->n_property_associations = 0;
}

static void free_item_locations(heic_ctx *ctx, heic_container *c)
{
    int i;
    for (i = 0; i < c->n_item_locations; i++)
        heic_free_buf(ctx, c->item_locations[i].extents);
    heic_free_buf(ctx, c->item_locations);
    c->item_locations = NULL;
    c->n_item_locations = 0;
}

static void free_item_infos(heic_ctx *ctx, heic_container *c)
{
    int i;
    for (i = 0; i < c->n_item_infos; i++) {
        heic_free_buf(ctx, c->item_infos[i].item_name);
        heic_free_buf(ctx, c->item_infos[i].content_type);
    }
    heic_free_buf(ctx, c->item_infos);
    c->item_infos = NULL;
    c->n_item_infos = 0;
}

void heic_container_free(heic_container *c)
{
    int i;
    heic_ctx *ctx;
    if (!c || !c->ctx) return;
    ctx = c->ctx;
    heic_free_buf(ctx, c->compatible_brands);
    free_item_locations(ctx, c);
    free_item_infos(ctx, c);
    for (i = 0; i < c->n_properties; i++) free_property(ctx, &c->properties[i]);
    heic_free_buf(ctx, c->properties);
    free_property_associations(ctx, c);
    for (i = 0; i < c->n_item_references; i++)
        heic_free_buf(ctx, c->item_references[i].to_item_ids);
    heic_free_buf(ctx, c->item_references);
    memset(c, 0, sizeof(*c));
}

static int parse_ftyp(heic_ctx *ctx, const heic_box *b, heic_container *c)
{
    size_t pos;
    int n = 0;
    if (b->content_len < 8) {
        heic_error(ctx, HEIC_SEVERITY_ERROR, "ftyp too short");
        return -1;
    }
    c->brand = heic_read_fcc(b->content);
    pos = 8;
    while (pos + 4 <= b->content_len && n < HEIC_MAX_COMPAT_BRANDS) n++, pos += 4;
    if (n > 0) {
        int i;
        c->compatible_brands = (heic_fourcc *)heic_zalloc(ctx, (size_t)n * sizeof(heic_fourcc));
        if (!c->compatible_brands) return -1;
        pos = 8;
        for (i = 0; i < n; i++, pos += 4)
            c->compatible_brands[i] = heic_read_fcc(b->content + pos);
        c->n_compatible_brands = n;
    }
    if (!is_heif_brand(c->brand)) {
        int i, ok = 0;
        for (i = 0; i < c->n_compatible_brands; i++)
            if (is_heif_brand(c->compatible_brands[i])) { ok = 1; break; }
        if (!ok) {
            heic_error(ctx, HEIC_SEVERITY_ERROR, "not a HEIF/AVIF file");
            return -1;
        }
    }
    return 0;
}

static int parse_pitm(heic_ctx *ctx, const heic_box *b, heic_container *c)
{
    uint8_t version;
    if (b->content_len < 6) {
        heic_error(ctx, HEIC_SEVERITY_ERROR, "pitm too short");
        return -1;
    }
    version = b->content[0];
    if (version == 0)
        c->primary_item_id = heic_rb16(b->content + 4);
    else {
        if (b->content_len < 8) return -1;
        c->primary_item_id = heic_rb32(b->content + 4);
    }
    return 0;
}

static int parse_iloc(heic_ctx *ctx, const heic_box *b, heic_container *c,
                      const heic_abort *ab)
{
    const uint8_t *content = b->content;
    size_t len = b->content_len;
    uint8_t version, offset_size, length_size, base_offset_size, index_size;
    size_t pos;
    uint32_t item_count, i;

    if (len < 8) return -1;
    version = content[0];
    offset_size = (content[4] >> 4) & 0xF;
    length_size = content[4] & 0xF;
    base_offset_size = (content[5] >> 4) & 0xF;
    index_size = version >= 1 ? (content[5] & 0xF) : 0;
    pos = 6;

    if (version < 2) {
        if (pos + 2 > len) return -1;
        item_count = heic_rb16(content + pos);
        pos += 2;
    } else {
        if (pos + 4 > len) return -1;
        item_count = heic_rb32(content + pos);
        pos += 4;
    }
    if (item_count > HEIC_MAX_ITEMS) return -1;

    free_item_locations(ctx, c);
    c->item_locations = (heic_item_loc *)heic_zalloc(ctx, item_count * sizeof(heic_item_loc));
    if (!c->item_locations && item_count) return -1;

    for (i = 0; i < item_count; i++) {
        heic_item_loc *loc;
        uint16_t extent_count;
        uint32_t e;
        if (heic_abort_check(ab)) return -1;
        loc = &c->item_locations[i];
        if (version < 2) {
            if (pos + 2 > len) break;
            loc->item_id = heic_rb16(content + pos);
            pos += 2;
        } else {
            if (pos + 4 > len) break;
            loc->item_id = heic_rb32(content + pos);
            pos += 4;
        }
        if (version >= 1) {
            if (pos + 2 > len) break;
            loc->construction_method = content[pos + 1] & 0xF;
            pos += 2;
        } else
            loc->construction_method = 0;
        if (pos + 2 > len) break;
        pos += 2;
        loc->base_offset = read_sized_int(content, len, &pos, base_offset_size);
        if (pos + 2 > len) break;
        extent_count = heic_rb16(content + pos);
        pos += 2;
        if (extent_count > HEIC_MAX_EXTENTS_PER_ITEM) return -1;
        loc->extents = (heic_extent *)heic_zalloc(ctx, extent_count * sizeof(heic_extent));
        if (!loc->extents && extent_count) return -1;
        loc->n_extents = extent_count;
        for (e = 0; e < extent_count; e++) {
            if (index_size) (void)read_sized_int(content, len, &pos, index_size);
            loc->extents[e].offset = read_sized_int(content, len, &pos, offset_size);
            loc->extents[e].length = read_sized_int(content, len, &pos, length_size);
        }
        c->n_item_locations = (int)(i + 1);
    }
    return 0;
}

static int parse_infe(heic_ctx *ctx, const heic_box *b, heic_item_info *info)
{
    const uint8_t *content = b->content;
    size_t len = b->content_len;
    uint8_t version;
    uint32_t flags;
    size_t pos;
    size_t name_end, ct_end;

    memset(info, 0, sizeof(*info));
    if (len < 8) return -1;
    version = content[0];
    flags = ((uint32_t)content[1] << 16) | ((uint32_t)content[2] << 8) | content[3];
    info->hidden = (flags & 1) != 0;
    pos = 4;
    if (version < 3) {
        if (pos + 2 > len) return -1;
        info->item_id = heic_rb16(content + pos);
        pos += 2;
    } else {
        if (pos + 4 > len) return -1;
        info->item_id = heic_rb32(content + pos);
        pos += 4;
    }
    if (pos + 2 > len) return -1;
    pos += 2;
    if (version >= 2) {
        if (pos + 4 > len) return -1;
        info->item_type = heic_read_fcc(content + pos);
        pos += 4;
    }
    if (pos >= len) {
        info->item_name = (char *)heic_zalloc(ctx, 1);
        info->content_type = (char *)heic_zalloc(ctx, 1);
        return 0;
    }
    name_end = 0;
    while (pos + name_end < len && content[pos + name_end] != 0) name_end++;
    if (name_end > HEIC_MAX_STRING_LEN) return -1;
    info->item_name = dup_cstr_z(ctx, content + pos, name_end + 1);
    pos += name_end + (pos + name_end < len ? 1 : 0);
    ct_end = 0;
    if (pos < len) {
        while (pos + ct_end < len && content[pos + ct_end] != 0) ct_end++;
        if (ct_end > HEIC_MAX_STRING_LEN) return -1;
        info->content_type = dup_cstr_z(ctx, content + pos, ct_end + 1);
    } else
        info->content_type = (char *)heic_zalloc(ctx, 1);
    if (!info->item_name || !info->content_type) return -1;
    return 0;
}

static int parse_iinf(heic_ctx *ctx, const heic_box *b, heic_container *c,
                      const heic_abort *ab)
{
    const uint8_t *content = b->content;
    size_t len = b->content_len;
    uint8_t version;
    size_t pos;
    uint32_t entry_count;
    heic_box_iter it;
    heic_box child;

    if (len < 6) return -1;
    version = content[0];
    pos = 4;
    if (version == 0) {
        if (pos + 2 > len) return -1;
        entry_count = heic_rb16(content + pos);
        pos += 2;
    } else {
        if (pos + 4 > len) return -1;
        entry_count = heic_rb32(content + pos);
        pos += 4;
    }
    if (entry_count > HEIC_MAX_ITEMS) return -1;
    free_item_infos(ctx, c);
    c->item_infos = (heic_item_info *)heic_zalloc(ctx, entry_count * sizeof(heic_item_info));
    if (!c->item_infos && entry_count) return -1;

    box_iter_init(&it, content + pos, len - pos);
    while (box_iter_next(&it, &child)) {
        heic_item_info info;
        if (heic_abort_check(ab)) return -1;
        if (child.type != HEIC_BOX_INFE) continue;
        if (parse_infe(ctx, &child, &info) != 0) continue;
        if ((uint32_t)c->n_item_infos >= entry_count) break;
        c->item_infos[c->n_item_infos++] = info;
    }
    return 0;
}

static int parse_ispe(const heic_box *b, heic_ispe *out)
{
    if (b->content_len < 12) return -1;
    out->width = heic_rb32(b->content + 4);
    out->height = heic_rb32(b->content + 8);
    if (out->width == 0 || out->height == 0) return -1;
    return 0;
}

static int parse_hvcc(heic_ctx *ctx, const heic_box *b, heic_hvcc *out)
{
    const uint8_t *content = b->content;
    size_t len = b->content_len;
    size_t pos;
    uint8_t num_arrays, a;
    int n_nals = 0, cap = 0;
    uint8_t **nals = NULL;
    size_t *nal_lens = NULL;

    memset(out, 0, sizeof(*out));
    if (len < 23) return -1;
    out->config_version = content[0];
    out->general_profile_space = (content[1] >> 6) & 3;
    out->general_tier_flag = (content[1] >> 5) & 1;
    out->general_profile_idc = content[1] & 0x1F;
    out->general_profile_compatibility_flags = heic_rb32(content + 2);
    out->general_constraint_indicator_flags =
        ((uint64_t)content[6] << 40) | ((uint64_t)content[7] << 32) |
        ((uint64_t)content[8] << 24) | ((uint64_t)content[9] << 16) |
        ((uint64_t)content[10] << 8) | (uint64_t)content[11];
    out->general_level_idc = content[12];
    out->chroma_format = content[16] & 3;
    out->bit_depth_luma_minus8 = content[17] & 7;
    out->bit_depth_chroma_minus8 = content[18] & 7;
    out->length_size_minus_one = content[21] & 3;
    if (out->length_size_minus_one == 2) return -1;
    num_arrays = content[22];
    pos = 23;

    for (a = 0; a < num_arrays; a++) {
        uint16_t num_nalus, n;
        if (pos + 3 > len) break;
        pos += 1;
        num_nalus = heic_rb16(content + pos);
        pos += 2;
        for (n = 0; n < num_nalus; n++) {
            uint16_t nalu_len;
            uint8_t *copy;
            if (pos + 2 > len) break;
            nalu_len = heic_rb16(content + pos);
            pos += 2;
            if (pos + (size_t)nalu_len > len) break;
            if (n_nals >= cap) {
                int ncap = cap ? cap * 2 : 8;
                uint8_t **nn = (uint8_t **)heic_realloc_buf(ctx, nals,
                    (size_t)cap * sizeof(uint8_t *), (size_t)ncap * sizeof(uint8_t *));
                size_t *nl = (size_t *)heic_realloc_buf(ctx, nal_lens,
                    (size_t)cap * sizeof(size_t), (size_t)ncap * sizeof(size_t));
                if (!nn || !nl) {
                    heic_free_buf(ctx, nn);
                    heic_free_buf(ctx, nl);
                    goto fail;
                }
                nals = nn;
                nal_lens = nl;
                cap = ncap;
            }
            copy = (uint8_t *)heic_zalloc(ctx, nalu_len);
            if (!copy) goto fail;
            memcpy(copy, content + pos, nalu_len);
            nals[n_nals] = copy;
            nal_lens[n_nals] = nalu_len;
            n_nals++;
            pos += nalu_len;
        }
    }
    out->nal_units = nals;
    out->nal_unit_lens = nal_lens;
    out->n_nal_units = n_nals;
    return 0;
fail:
    {
        int i;
        for (i = 0; i < n_nals; i++) heic_free_buf(ctx, nals[i]);
        heic_free_buf(ctx, nals);
        heic_free_buf(ctx, nal_lens);
    }
    return -1;
}

static int parse_av1c(heic_ctx *ctx, const heic_box *b, heic_av1c *out)
{
    const uint8_t *c = b->content;
    size_t len = b->content_len;
    memset(out, 0, sizeof(*out));
    if (len < 4) return -1;

    out->seq_profile = (c[1] >> 5) & 7;
    out->seq_level_idx_0 = c[1] & 0x1F;
    out->high_bitdepth = (c[2] >> 6) & 1;
    out->twelve_bit = (c[2] >> 5) & 1;
    out->monochrome = (c[2] >> 4) & 1;
    out->chroma_subsampling_x = (c[2] >> 3) & 1;
    out->chroma_subsampling_y = (c[2] >> 2) & 1;
    if (len > 4) {
        out->config_obus_len = len - 4;
        out->config_obus = (uint8_t *)heic_zalloc(ctx, out->config_obus_len);
        if (!out->config_obus) return -1;
        memcpy(out->config_obus, c + 4, out->config_obus_len);
    }
    return 0;
}

static int parse_colr(heic_ctx *ctx, const heic_box *b, heic_colr *out)
{
    heic_fourcc ct;
    memset(out, 0, sizeof(*out));
    if (b->content_len < 4) return -1;
    ct = heic_read_fcc(b->content);
    if (ct == HEIC_FCC('n', 'c', 'l', 'x')) {
        if (b->content_len < 11) return -1;
        out->kind = HEIC_COLR_NCLX;
        out->color_primaries = heic_rb16(b->content + 4);
        out->transfer_characteristics = heic_rb16(b->content + 6);
        out->matrix_coefficients = heic_rb16(b->content + 8);
        out->full_range = (b->content[10] >> 7) != 0;
        return 0;
    }
    if (ct == HEIC_FCC('p', 'r', 'o', 'f') || ct == HEIC_FCC('r', 'i', 'c', 'c')) {
        size_t icc_len = b->content_len - 4;
        if (icc_len > HEIC_MAX_ICC_SIZE) return -1;
        out->kind = HEIC_COLR_ICC;
        out->icc_len = icc_len;
        out->icc = (uint8_t *)heic_zalloc(ctx, icc_len ? icc_len : 1);
        if (!out->icc) return -1;
        if (icc_len) memcpy(out->icc, b->content + 4, icc_len);
        return 0;
    }
    return -1;
}

static int parse_clap(const heic_box *b, heic_clap *out)
{
    const uint8_t *c = b->content;
    if (b->content_len < 32) return -1;
    out->width_n = heic_rb32(c + 0);
    out->width_d = heic_rb32(c + 4);
    out->height_n = heic_rb32(c + 8);
    out->height_d = heic_rb32(c + 12);
    out->horiz_off_n = (int32_t)heic_rb32(c + 16);
    out->horiz_off_d = heic_rb32(c + 20);
    out->vert_off_n = (int32_t)heic_rb32(c + 24);
    out->vert_off_d = heic_rb32(c + 28);
    if (!out->width_d || !out->height_d || !out->horiz_off_d || !out->vert_off_d)
        return -1;
    return 0;
}

static int parse_irot(const heic_box *b, heic_irot *out)
{
    if (b->content_len < 1) return -1;
    switch (b->content[0] & 3) {
    case 0: out->angle = 0; break;
    case 1: out->angle = 270; break;
    case 2: out->angle = 180; break;
    case 3: out->angle = 90; break;
    }
    return 0;
}

static int parse_imir(const heic_box *b, heic_imir *out)
{
    if (b->content_len < 1) return -1;
    out->axis = b->content[0] & 1;
    return 0;
}

static int parse_auxc(heic_ctx *ctx, const heic_box *b, heic_auxc *out)
{
    const uint8_t *data;
    size_t dlen, end;
    memset(out, 0, sizeof(*out));
    if (b->content_len < 5) return -1;
    data = b->content + 4;
    dlen = b->content_len - 4;
    end = 0;
    while (end < dlen && data[end] != 0) end++;
    if (end > HEIC_MAX_STRING_LEN) return -1;
    out->aux_type = dup_cstr_z(ctx, data, end + 1);
    if (!out->aux_type) return -1;
    if (end + 1 < dlen) {
        out->subtype_len = dlen - (end + 1);
        out->subtype_data = (uint8_t *)heic_zalloc(ctx, out->subtype_len);
        if (!out->subtype_data) return -1;
        memcpy(out->subtype_data, data + end + 1, out->subtype_len);
    }
    return 0;
}

static int parse_uncc(heic_ctx *ctx, const heic_box *b, heic_uncc *out)
{
    const uint8_t *c = b->content;
    size_t len = b->content_len, pos = 4;
    uint32_t ncomp, i;

    memset(out, 0, sizeof(*out));
    if (len < 12) return -1;
    out->profile = heic_rb32(c + pos);
    pos += 4;
    ncomp = heic_rb32(c + pos);
    pos += 4;
    if (ncomp == 0 || ncomp > 16) return -1;
    out->components =
        (heic_uncc_comp *)heic_zalloc(ctx, ncomp * sizeof(heic_uncc_comp));
    if (!out->components) return -1;
    out->n_components = (int)ncomp;
    for (i = 0; i < ncomp; i++) {
        if (pos + 5 > len) {
            heic_free_buf(ctx, out->components);
            memset(out, 0, sizeof(*out));
            return -1;
        }
        out->components[i].component_index = heic_rb16(c + pos);
        pos += 2;
        out->components[i].component_bit_depth_minus_one = c[pos++];
        out->components[i].component_format = c[pos++];
        out->components[i].component_align_size = c[pos++];
    }
    if (pos + 4 <= len) {
        uint8_t flags;
        out->sampling_type = c[pos++];
        out->interleave_type = c[pos++];
        out->block_size = c[pos++];
        flags = c[pos++];
        out->components_little_endian = (flags & 0x80) ? 1 : 0;
        out->block_pad_lsb = (flags & 0x40) ? 1 : 0;
        out->block_little_endian = (flags & 0x20) ? 1 : 0;
        out->block_reversed = (flags & 0x10) ? 1 : 0;
        out->pad_unknown = (flags & 0x08) ? 1 : 0;
    }

    if (pos + 4 <= len) {
        out->pixel_size = heic_rb32(c + pos);
        pos += 4;
    }
    if (pos + 4 <= len) {
        out->row_align_size = heic_rb32(c + pos);
        pos += 4;
    }
    if (pos + 4 <= len) {
        out->tile_align_size = heic_rb32(c + pos);
        pos += 4;
    }
    if (pos + 4 <= len) {
        out->num_tile_cols_minus_one = heic_rb32(c + pos);
        pos += 4;
    }
    if (pos + 4 <= len) out->num_tile_rows_minus_one = heic_rb32(c + pos);
    return 0;
}

static int parse_cmpc(const heic_box *b, heic_cmpc *out)
{
    memset(out, 0, sizeof(*out));

    if (b->content_len < 9) return -1;
    out->compression_type = heic_read_fcc(b->content + 4);
    out->unit_type = b->content[8];
    if (out->unit_type > 4) return -1;
    return 0;
}

static const uint8_t icef_offset_bits[] = {0, 16, 24, 32, 64};
static const uint8_t icef_size_bits[] = {8, 16, 24, 32, 64};

static uint64_t icef_read_bits(const uint8_t *p, size_t len, size_t *bitpos, int nbits)
{
    uint64_t v = 0;
    int i;
    for (i = 0; i < nbits; i++) {
        size_t bp = *bitpos + (size_t)i;
        size_t byte = bp / 8;
        int bit = 7 - (int)(bp % 8);
        if (byte >= len) return 0;
        v = (v << 1) | (uint64_t)((p[byte] >> bit) & 1);
    }
    *bitpos += (size_t)nbits;
    return v;
}

static int parse_icef(heic_ctx *ctx, const heic_box *b, heic_icef *out)
{
    const uint8_t *c;
    size_t len, bitpos;
    uint8_t codes, off_code, sz_code;
    uint32_t n, i;
    uint64_t implied = 0;
    int off_bits, sz_bits;

    memset(out, 0, sizeof(*out));

    if (b->content_len < 9) return -1;
    c = b->content + 4;
    len = b->content_len - 4;
    codes = c[0];
    off_code = (uint8_t)((codes >> 5) & 7);
    sz_code = (uint8_t)((codes >> 2) & 7);
    if (off_code > 4 || sz_code > 4) return -1;
    off_bits = (int)icef_offset_bits[off_code];
    sz_bits = (int)icef_size_bits[sz_code];
    if (len < 5) return -1;
    n = heic_rb32(c + 1);
    if (n > 1000000) return -1;
    bitpos = 40;
    if (n == 0) return 0;
    out->units = (heic_icef_unit *)heic_zalloc(ctx, (size_t)n * sizeof(heic_icef_unit));
    if (!out->units) return -1;
    out->n_units = (int)n;
    for (i = 0; i < n; i++) {
        uint64_t off, sz;
        if (off_code == 0)
            off = implied;
        else
            off = icef_read_bits(c, len, &bitpos, off_bits);
        sz = icef_read_bits(c, len, &bitpos, sz_bits);
        if (bitpos / 8 > len) {
            heic_free_buf(ctx, out->units);
            memset(out, 0, sizeof(*out));
            return -1;
        }
        out->units[i].offset = off;
        out->units[i].size = sz;
        if (off_code == 0) implied += sz;
    }
    return 0;
}

static int parse_cmpd(heic_ctx *ctx, const heic_box *b, heic_cmpd *out)
{
    const uint8_t *c = b->content;
    size_t len = b->content_len, pos = 0;
    uint32_t n, i;

    memset(out, 0, sizeof(*out));
    if (len < 4) return -1;
    n = heic_rb32(c);
    pos = 4;
    if (n == 0 || n > 16) return -1;
    out->types = (uint16_t *)heic_zalloc(ctx, n * sizeof(uint16_t));
    if (!out->types) return -1;
    out->n_types = (int)n;
    for (i = 0; i < n; i++) {
        if (pos + 2 > len) {
            heic_free_buf(ctx, out->types);
            memset(out, 0, sizeof(*out));
            return -1;
        }
        out->types[i] = heic_rb16(c + pos);
        pos += 2;
        if (out->types[i] >= 0x8000) {

            while (pos < len && c[pos]) pos++;
            if (pos < len) pos++;
        }
    }
    return 0;
}

static int parse_ipco(heic_ctx *ctx, const heic_box *b, heic_container *c,
                      const heic_abort *ab)
{
    heic_box_iter it;
    heic_box child;
    heic_property *props = NULL;
    int n = 0, cap = 0;

    box_iter_init(&it, b->content, b->content_len);
    while (box_iter_next(&it, &child)) {
        heic_property prop;
        if (heic_abort_check(ab)) goto fail;
        if ((uint32_t)n >= HEIC_MAX_PROPERTIES) goto fail;
        memset(&prop, 0, sizeof(prop));
        prop.kind = HEIC_PROP_UNKNOWN;
        if (child.type == HEIC_BOX_ISPE) {
            if (parse_ispe(&child, &prop.ispe) == 0) prop.kind = HEIC_PROP_ISPE;
        } else if (child.type == HEIC_BOX_HVCC || child.type == HEIC_BOX_HVCB) {
            if (parse_hvcc(ctx, &child, &prop.hvcc) == 0) prop.kind = HEIC_PROP_HVCC;
        } else if (child.type == HEIC_BOX_AV1C) {
            if (parse_av1c(ctx, &child, &prop.av1c) == 0) prop.kind = HEIC_PROP_AV1C;
        } else if (child.type == HEIC_BOX_COLR) {
            if (parse_colr(ctx, &child, &prop.colr) == 0) prop.kind = HEIC_PROP_COLR;
        } else if (child.type == HEIC_BOX_CLAP) {
            if (parse_clap(&child, &prop.clap) == 0) prop.kind = HEIC_PROP_CLAP;
        } else if (child.type == HEIC_BOX_IROT) {
            if (parse_irot(&child, &prop.irot) == 0) prop.kind = HEIC_PROP_IROT;
        } else if (child.type == HEIC_BOX_IMIR) {
            if (parse_imir(&child, &prop.imir) == 0) prop.kind = HEIC_PROP_IMIR;
        } else if (child.type == HEIC_BOX_AUXC) {
            if (parse_auxc(ctx, &child, &prop.auxc) == 0) prop.kind = HEIC_PROP_AUXC;
        } else if (child.type == HEIC_BOX_UNCC) {
            if (parse_uncc(ctx, &child, &prop.uncc) == 0) prop.kind = HEIC_PROP_UNCC;
        } else if (child.type == HEIC_BOX_CMPC) {
            if (parse_cmpc(&child, &prop.cmpc) == 0) prop.kind = HEIC_PROP_CMPC;
        } else if (child.type == HEIC_BOX_CMPD) {
            if (parse_cmpd(ctx, &child, &prop.cmpd) == 0) prop.kind = HEIC_PROP_CMPD;
        } else if (child.type == HEIC_BOX_ICEF) {
            if (parse_icef(ctx, &child, &prop.icef) == 0) prop.kind = HEIC_PROP_ICEF;
        }
        if (n >= cap) {
            int ncap = cap ? cap * 2 : 16;
            heic_property *np = (heic_property *)heic_realloc_buf(
                ctx, props, (size_t)cap * sizeof(heic_property),
                (size_t)ncap * sizeof(heic_property));
            if (!np) goto fail;
            props = np;
            cap = ncap;
        }
        props[n++] = prop;
    }
    c->properties = props;
    c->n_properties = n;
    return 0;
fail:
    {
        int i;
        for (i = 0; i < n; i++) free_property(ctx, &props[i]);
        heic_free_buf(ctx, props);
    }
    return -1;
}

static int parse_ipma(heic_ctx *ctx, const heic_box *b, heic_container *c,
                      const heic_abort *ab)
{
    const uint8_t *content = b->content;
    size_t len = b->content_len;
    uint8_t version;
    uint32_t flags, entry_count, e;
    size_t pos;

    if (len < 8) return -1;
    version = content[0];
    flags = ((uint32_t)content[1] << 16) | ((uint32_t)content[2] << 8) | content[3];
    pos = 4;
    entry_count = heic_rb32(content + pos);
    pos += 4;
    if (entry_count > HEIC_MAX_ITEMS) return -1;

    free_property_associations(ctx, c);
    c->property_associations =
        (heic_ipma *)heic_zalloc(ctx, entry_count * sizeof(heic_ipma));
    if (!c->property_associations && entry_count) return -1;

    for (e = 0; e < entry_count; e++) {
        heic_ipma *a;
        uint8_t assoc_count, k;
        if (heic_abort_check(ab)) return -1;
        a = &c->property_associations[e];
        if (version < 1) {
            if (pos + 2 > len) break;
            a->item_id = heic_rb16(content + pos);
            pos += 2;
        } else {
            if (pos + 4 > len) break;
            a->item_id = heic_rb32(content + pos);
            pos += 4;
        }
        if (pos >= len) break;
        assoc_count = content[pos++];
        a->prop_indices = (uint16_t *)heic_zalloc(ctx, assoc_count * sizeof(uint16_t));
        a->essential = (uint8_t *)heic_zalloc(ctx, assoc_count);
        if ((!a->prop_indices || !a->essential) && assoc_count) return -1;
        a->n_props = 0;
        for (k = 0; k < assoc_count; k++) {
            if (flags & 1) {
                uint16_t val;
                if (pos + 2 > len) break;
                val = heic_rb16(content + pos);
                pos += 2;
                a->essential[a->n_props] = (val >> 15) != 0;
                a->prop_indices[a->n_props] = val & 0x7FFF;
            } else {
                uint8_t val;
                if (pos >= len) break;
                val = content[pos++];
                a->essential[a->n_props] = (val >> 7) != 0;
                a->prop_indices[a->n_props] = val & 0x7F;
            }
            a->n_props++;
        }
        c->n_property_associations = (int)(e + 1);
    }
    return 0;
}

static int parse_iprp(heic_ctx *ctx, const heic_box *b, heic_container *c,
                      const heic_abort *ab)
{
    heic_box_iter it;
    heic_box child;
    box_iter_init(&it, b->content, b->content_len);
    while (box_iter_next(&it, &child)) {
        if (heic_abort_check(ab)) return -1;
        if (child.type == HEIC_BOX_IPCO) {
            if (parse_ipco(ctx, &child, c, ab) != 0) return -1;
        } else if (child.type == HEIC_BOX_IPMA) {
            if (parse_ipma(ctx, &child, c, ab) != 0) return -1;
        }
    }
    return 0;
}

static int parse_iref(heic_ctx *ctx, const heic_box *b, heic_container *c,
                      const heic_abort *ab)
{
    const uint8_t *content = b->content;
    size_t len = b->content_len;
    uint8_t version;
    heic_box_iter it;
    heic_box child;
    heic_iref *refs = NULL;
    int n = 0, cap = 0;

    if (len < 4) return -1;
    version = content[0];
    box_iter_init(&it, content + 4, len - 4);
    while (box_iter_next(&it, &child)) {
        const uint8_t *data = child.content;
        size_t dlen = child.content_len;
        size_t pos = 0;
        size_t id_size = version == 0 ? 2u : 4u;
        while (pos < dlen) {
            heic_iref r;
            uint16_t ref_count, t;
            if (heic_abort_check(ab)) goto fail;
            if ((uint32_t)n >= HEIC_MAX_REFERENCES) goto fail;
            memset(&r, 0, sizeof(r));
            r.ref_type = child.type;
            if (pos + id_size > dlen) break;
            r.from_item_id = id_size == 2 ? heic_rb16(data + pos) : heic_rb32(data + pos);
            pos += id_size;
            if (pos + 2 > dlen) break;
            ref_count = heic_rb16(data + pos);
            pos += 2;
            if (ref_count > HEIC_MAX_REFS_PER_ENTRY) goto fail;
            r.to_item_ids = (uint32_t *)heic_zalloc(ctx, ref_count * sizeof(uint32_t));
            if (!r.to_item_ids && ref_count) goto fail;
            for (t = 0; t < ref_count; t++) {
                if (pos + id_size > dlen) break;
                r.to_item_ids[r.n_to++] =
                    id_size == 2 ? heic_rb16(data + pos) : heic_rb32(data + pos);
                pos += id_size;
            }
            if (n >= cap) {
                int ncap = cap ? cap * 2 : 8;
                heic_iref *nr = (heic_iref *)heic_realloc_buf(
                    ctx, refs, (size_t)cap * sizeof(heic_iref),
                    (size_t)ncap * sizeof(heic_iref));
                if (!nr) {
                    heic_free_buf(ctx, r.to_item_ids);
                    goto fail;
                }
                refs = nr;
                cap = ncap;
            }
            refs[n++] = r;
        }
    }
    c->item_references = refs;
    c->n_item_references = n;
    return 0;
fail:
    {
        int i;
        for (i = 0; i < n; i++) heic_free_buf(ctx, refs[i].to_item_ids);
        heic_free_buf(ctx, refs);
    }
    return -1;
}

static int parse_meta(heic_ctx *ctx, const heic_box *meta, heic_container *c,
                      const heic_abort *ab)
{
    heic_box_iter it;
    heic_box child;
    if (meta->content_len < 4) return -1;
    box_iter_init(&it, meta->content + 4, meta->content_len - 4);
    while (box_iter_next(&it, &child)) {
        if (heic_abort_check(ab)) return -1;
        if (child.type == HEIC_BOX_PITM) {
            if (parse_pitm(ctx, &child, c) != 0) return -1;
        } else if (child.type == HEIC_BOX_ILOC) {
            if (parse_iloc(ctx, &child, c, ab) != 0) return -1;
        } else if (child.type == HEIC_BOX_IINF) {
            if (parse_iinf(ctx, &child, c, ab) != 0) return -1;
        } else if (child.type == HEIC_BOX_IPRP) {
            if (parse_iprp(ctx, &child, c, ab) != 0) return -1;
        } else if (child.type == HEIC_BOX_IREF) {
            if (parse_iref(ctx, &child, c, ab) != 0) return -1;
        } else if (child.type == HEIC_BOX_IDAT) {
            c->idat = child.content;
            c->idat_len = child.content_len;
        }
    }
    c->has_meta = 1;
    return 0;
}

int heic_container_parse(heic_ctx *ctx, const uint8_t *data, size_t len,
                         heic_container *out, const heic_abort *ab)
{
    heic_box_iter it;
    heic_box top;

    if (!ctx || !data || !out || len < 16) return -1;
    memset(out, 0, sizeof(*out));
    out->ctx = ctx;
    out->data = data;
    out->len = len;

    box_iter_init(&it, data, len);
    while (box_iter_next(&it, &top)) {
        if (heic_abort_check(ab)) {
            heic_container_free(out);
            return -1;
        }
        if (top.type == HEIC_BOX_FTYP) {
            if (parse_ftyp(ctx, &top, out) != 0) {
                heic_container_free(out);
                return -1;
            }
        } else if (top.type == HEIC_BOX_META) {

            if (out->has_meta) continue;
            if (parse_meta(ctx, &top, out, ab) != 0) {
                heic_container_free(out);
                return -1;
            }
        } else if (top.type == HEIC_BOX_MDAT) {
            out->mdat_offset = top.content_off;
            out->mdat_len = top.content_len;
        }

    }
    if (out->brand == 0 && out->n_compatible_brands == 0) {
        heic_error(ctx, HEIC_SEVERITY_ERROR, "missing ftyp");
        heic_container_free(out);
        return -1;
    }
    if (!out->has_meta) {
        heic_error(ctx, HEIC_SEVERITY_ERROR, "missing meta (image sequences not yet supported)");
        heic_container_free(out);
        return -1;
    }
    return 0;
}

int heic_container_get_item(const heic_container *c, uint32_t item_id, heic_item *out)
{
    int i, j;
    const heic_item_info *info = NULL;
    const heic_ipma *assoc = NULL;

    if (!c || !out) return -1;
    memset(out, 0, sizeof(*out));
    for (i = 0; i < c->n_item_infos; i++)
        if (c->item_infos[i].item_id == item_id) {
            info = &c->item_infos[i];
            break;
        }
    if (!info) return -1;
    out->id = item_id;
    out->item_type = info->item_type;
    out->name = info->item_name ? info->item_name : "";
    out->content_type = info->content_type ? info->content_type : "";

    for (i = 0; i < c->n_property_associations; i++)
        if (c->property_associations[i].item_id == item_id) {
            assoc = &c->property_associations[i];
            break;
        }
    if (!assoc) return 0;

    for (j = 0; j < assoc->n_props; j++) {
        uint16_t idx = assoc->prop_indices[j];
        const heic_property *p;
        if (idx == 0 || idx > (uint16_t)c->n_properties) continue;
        p = &c->properties[idx - 1];
        switch (p->kind) {
        case HEIC_PROP_ISPE:
            out->has_dims = 1;
            out->width = p->ispe.width;
            out->height = p->ispe.height;
            break;
        case HEIC_PROP_HVCC:
            out->hvcc = &p->hvcc;
            break;
        case HEIC_PROP_AV1C:
            out->av1c = &p->av1c;
            break;
        case HEIC_PROP_COLR:
            out->colr = &p->colr;
            break;
        case HEIC_PROP_CLAP:
            out->clap = &p->clap;
            if (out->n_transforms < 8) {
                out->transforms[out->n_transforms].kind = HEIC_XFORM_CLAP;
                out->transforms[out->n_transforms].clap = p->clap;
                out->n_transforms++;
            }
            break;
        case HEIC_PROP_IROT:
            out->irot = &p->irot;
            if (out->n_transforms < 8) {
                out->transforms[out->n_transforms].kind = HEIC_XFORM_IROT;
                out->transforms[out->n_transforms].irot = p->irot;
                out->n_transforms++;
            }
            break;
        case HEIC_PROP_IMIR:
            out->imir = &p->imir;
            if (out->n_transforms < 8) {
                out->transforms[out->n_transforms].kind = HEIC_XFORM_IMIR;
                out->transforms[out->n_transforms].imir = p->imir;
                out->n_transforms++;
            }
            break;
        case HEIC_PROP_AUXC:
            out->auxc = &p->auxc;
            break;
        case HEIC_PROP_UNCC:
            out->uncc = &p->uncc;
            break;
        case HEIC_PROP_CMPC:
            out->cmpc = &p->cmpc;
            break;
        case HEIC_PROP_CMPD:
            out->cmpd = &p->cmpd;
            break;
        case HEIC_PROP_ICEF:
            out->icef = &p->icef;
            break;
        default:
            break;
        }
    }
    return 0;
}

int heic_container_item_data(const heic_container *c, uint32_t item_id,
                             const uint8_t **out_data, size_t *out_len, int *owned_out)
{
    const heic_item_loc *loc = NULL;
    const uint8_t *source;
    size_t source_len;
    int i;

    if (!c || !out_data || !out_len || !owned_out) return -1;
    *out_data = NULL;
    *out_len = 0;
    *owned_out = 0;

    for (i = 0; i < c->n_item_locations; i++)
        if (c->item_locations[i].item_id == item_id) {
            loc = &c->item_locations[i];
            break;
        }
    if (!loc || loc->n_extents == 0) return -1;

    if (loc->construction_method == 0) {
        source = c->data;
        source_len = c->len;
    } else if (loc->construction_method == 1) {
        if (!c->idat) return -1;
        source = c->idat;
        source_len = c->idat_len;
    } else
        return -1;

    if (loc->n_extents == 1) {
        uint64_t off64 = loc->base_offset + loc->extents[0].offset;
        uint64_t len64 = loc->extents[0].length;
        size_t off, length, end;
        if (off64 > SIZE_MAX || len64 > SIZE_MAX) return -1;
        off = (size_t)off64;
        length = (size_t)len64;
        if (off > source_len || length > source_len - off) return -1;
        end = off + length;
        if (end > source_len) return -1;
        *out_data = source + off;
        *out_len = length;
        *owned_out = 0;
        return 0;
    }

    {
        uint64_t total = 0;
        size_t tlen;
        uint8_t *buf;
        size_t w = 0;
        for (i = 0; i < (int)loc->n_extents; i++) {
            if (total > UINT64_MAX - loc->extents[i].length) return -1;
            total += loc->extents[i].length;
        }
        if (total > source_len || total > SIZE_MAX) return -1;
        tlen = (size_t)total;
        buf = (uint8_t *)heic_zalloc(c->ctx, tlen ? tlen : 1);
        if (!buf) return -1;
        for (i = 0; i < (int)loc->n_extents; i++) {
            uint64_t off64 = loc->base_offset + loc->extents[i].offset;
            size_t off, length;
            if (off64 > SIZE_MAX) {
                heic_free_buf(c->ctx, buf);
                return -1;
            }
            off = (size_t)off64;
            length = (size_t)loc->extents[i].length;
            if (off > source_len || length > source_len - off) {
                heic_free_buf(c->ctx, buf);
                return -1;
            }
            memcpy(buf + w, source + off, length);
            w += length;
        }
        *out_data = buf;
        *out_len = tlen;
        *owned_out = 1;
        return 0;
    }
}

int heic_container_find_refs(const heic_container *c, uint32_t from_id,
                             heic_fourcc ref_type, uint32_t *out_ids, int max_out)
{
    int i, n = 0, j;
    if (!c) return 0;
    for (i = 0; i < c->n_item_references; i++) {
        const heic_iref *r = &c->item_references[i];
        if (r->from_item_id != from_id || r->ref_type != ref_type) continue;
        for (j = 0; j < r->n_to && n < max_out; j++)
            if (out_ids) out_ids[n++] = r->to_item_ids[j];
            else n++;
    }
    return n;
}

int heic_container_find_aux(const heic_container *c, uint32_t target_id,
                            const char *urn_prefix, uint32_t *out_ids, int max_out)
{
    int i, n = 0, j;
    size_t plen = urn_prefix ? strlen(urn_prefix) : 0;
    if (!c) return 0;
    for (i = 0; i < c->n_item_references; i++) {
        const heic_iref *r = &c->item_references[i];
        heic_item item;
        if (r->ref_type != HEIC_REF_AUXL) continue;
        for (j = 0; j < r->n_to; j++) {
            if (r->to_item_ids[j] != target_id) continue;
            if (heic_container_get_item(c, r->from_item_id, &item) != 0) continue;
            if (urn_prefix && item.auxc && item.auxc->aux_type &&
                strncmp(item.auxc->aux_type, urn_prefix, plen) != 0)
                continue;
            if (n < max_out && out_ids) out_ids[n] = r->from_item_id;
            n++;
        }
    }
    return n;
}

int heic_container_find_thumbs(const heic_container *c, uint32_t target_id,
                               uint32_t *out_ids, int max_out)
{
    int i, n = 0, j;
    if (!c) return 0;
    for (i = 0; i < c->n_item_references; i++) {
        const heic_iref *r = &c->item_references[i];
        if (r->ref_type != HEIC_REF_THMB) continue;
        for (j = 0; j < r->n_to; j++) {
            if (r->to_item_ids[j] != target_id) continue;
            if (n < max_out && out_ids) out_ids[n] = r->from_item_id;
            n++;
        }
    }
    return n;
}

static heic_kind brand_kind(const heic_container *c)
{
    int i;
    heic_fourcc brands[16];
    int n = 0;
    brands[n++] = c->brand;
    for (i = 0; i < c->n_compatible_brands && n < 16; i++)
        brands[n++] = c->compatible_brands[i];
    for (i = 0; i < n; i++) {
        if (brands[i] == HEIC_FCC('a', 'v', 'i', 'f') ||
            brands[i] == HEIC_FCC('a', 'v', 'i', 's'))
            return HEIC_KIND_AVIF;
    }
    for (i = 0; i < n; i++) {
        if (brands[i] == HEIC_FCC('h', 'e', 'i', 'c') ||
            brands[i] == HEIC_FCC('h', 'e', 'i', 'x') ||
            brands[i] == HEIC_FCC('h', 'e', 'v', 'c') ||
            brands[i] == HEIC_FCC('h', 'e', 'v', 'x'))
            return HEIC_KIND_HEIC;
    }
    for (i = 0; i < n; i++) {
        if (brands[i] == HEIC_FCC('m', 's', 'f', '1'))
            return HEIC_KIND_SEQUENCE;
    }
    return HEIC_KIND_HEIF;
}

heic_doc *heic_doc_open(heic_ctx *ctx, const uint8_t *data, size_t len)
{
    heic_doc *doc;
    heic_init();
    if (!ctx || !data || len == 0) return NULL;
    doc = (heic_doc *)heic_zalloc(ctx, sizeof(heic_doc));
    if (!doc) return NULL;
    doc->ctx = ctx;
    doc->data = data;
    doc->len = len;
    if (heic_container_parse(ctx, data, len, &doc->container, NULL) != 0) {
        heic_free_buf(ctx, doc);
        return NULL;
    }
    doc->kind = brand_kind(&doc->container);
    return doc;
}

void heic_doc_close(heic_doc *doc)
{
    heic_ctx *ctx;
    if (!doc) return;
    ctx = doc->ctx;
    heic_container_free(&doc->container);
    heic_free_buf(ctx, doc);
}

heic_kind heic_doc_kind(const heic_doc *doc)
{
    return doc ? doc->kind : HEIC_KIND_UNKNOWN;
}

static int primary_item(const heic_doc *doc, heic_item *item)
{
    if (!doc) return -1;
    return heic_container_get_item(&doc->container, doc->container.primary_item_id, item);
}

static void apply_transform_dims(uint32_t *w, uint32_t *h, const heic_item *item)
{
    int i;
    for (i = 0; i < item->n_transforms; i++) {
        const heic_xform *t = &item->transforms[i];
        if (t->kind == HEIC_XFORM_IROT &&
            (t->irot.angle == 90 || t->irot.angle == 270)) {
            uint32_t tmp = *w;
            *w = *h;
            *h = tmp;
        } else if (t->kind == HEIC_XFORM_CLAP && t->clap.width_d && t->clap.height_d) {
            uint32_t cw = t->clap.width_n / t->clap.width_d;
            uint32_t ch = t->clap.height_n / t->clap.height_d;
            if (cw > 0 && ch > 0) {
                *w = cw;
                *h = ch;
            }
        }
    }
}

#define HEIC_RESOLVE_MAX_DEPTH 8

static int resolve_seen(const uint32_t *seen, int n_seen, uint32_t id)
{
    int i;
    for (i = 0; i < n_seen; i++)
        if (seen[i] == id) return 1;
    return 0;
}

static int resolve_dims_r(const heic_doc *doc, const heic_item *item, uint32_t *w,
                          uint32_t *h, uint32_t *seen, int n_seen)
{
    if (item->has_dims) {
        *w = item->width;
        *h = item->height;
        apply_transform_dims(w, h, item);
        return 0;
    }
    if (n_seen >= HEIC_RESOLVE_MAX_DEPTH || resolve_seen(seen, n_seen, item->id))
        return -1;
    if (item->item_type == HEIC_TYPE_IDEN || item->item_type == HEIC_TYPE_TMAP ||
        item->item_type == HEIC_TYPE_GRID) {
        uint32_t refs[4];
        int n = heic_container_find_refs(&doc->container, item->id, HEIC_REF_DIMG, refs, 4);
        heic_item child;
        if (n >= 1 && heic_container_get_item(&doc->container, refs[0], &child) == 0) {
            seen[n_seen] = item->id;
            return resolve_dims_r(doc, &child, w, h, seen, n_seen + 1);
        }
    }
    return -1;
}

static int resolve_dims(const heic_doc *doc, const heic_item *item, uint32_t *w,
                        uint32_t *h)
{
    uint32_t seen[HEIC_RESOLVE_MAX_DEPTH];
    return resolve_dims_r(doc, item, w, h, seen, 0);
}

static int resolve_codec_item_r(const heic_doc *doc, const heic_item *item,
                                heic_item *out, uint32_t *seen, int n_seen)
{
    uint32_t refs[8];
    int n, i;
    if (item->hvcc || item->av1c) {
        *out = *item;
        return 0;
    }
    if (n_seen >= HEIC_RESOLVE_MAX_DEPTH || resolve_seen(seen, n_seen, item->id))
        return -1;
    if (item->item_type != HEIC_TYPE_GRID && item->item_type != HEIC_TYPE_IDEN &&
        item->item_type != HEIC_TYPE_TMAP)
        return -1;
    seen[n_seen] = item->id;
    n = heic_container_find_refs(&doc->container, item->id, HEIC_REF_DIMG, refs, 8);
    for (i = 0; i < n; i++) {
        heic_item child;
        if (heic_container_get_item(&doc->container, refs[i], &child) != 0) continue;
        if (resolve_codec_item_r(doc, &child, out, seen, n_seen + 1) == 0) return 0;
    }
    return -1;
}

static int resolve_codec_item(const heic_doc *doc, const heic_item *item, heic_item *out)
{
    uint32_t seen[HEIC_RESOLVE_MAX_DEPTH];
    return resolve_codec_item_r(doc, item, out, seen, 0);
}

static int bit_depth_from_item(const heic_item *item)
{
    if (item->hvcc) return 8 + item->hvcc->bit_depth_luma_minus8;
    if (item->av1c) {
        if (!item->av1c->high_bitdepth) return 8;
        return item->av1c->twelve_bit ? 12 : 10;
    }
    return 8;
}

int heic_doc_info(const heic_doc *doc, heic_image_info *info)
{
    heic_item item;
    heic_item codec;
    uint32_t w = 0, h = 0;
    uint32_t thumbs[4];
    uint32_t aux[4];
    int i;

    if (!doc || !info) return -1;
    memset(info, 0, sizeof(*info));
    info->full_range = -1;
    if (primary_item(doc, &item) != 0) return -1;
    if (resolve_dims(doc, &item, &w, &h) != 0) return -1;
    info->width = w;
    info->height = h;

    if (item.hvcc || item.av1c) {
        info->bit_depth = bit_depth_from_item(&item);
    } else if (resolve_codec_item(doc, &item, &codec) == 0) {
        info->bit_depth = bit_depth_from_item(&codec);
    } else
        info->bit_depth = 8;

    if (item.colr && item.colr->kind == HEIC_COLR_NCLX) {
        info->color_primaries = item.colr->color_primaries;
        info->transfer_characteristics = item.colr->transfer_characteristics;
        info->matrix_coefficients = item.colr->matrix_coefficients;
        info->full_range = item.colr->full_range ? 1 : 0;
    } else if (resolve_codec_item(doc, &item, &codec) == 0 && codec.colr &&
               codec.colr->kind == HEIC_COLR_NCLX) {
        info->color_primaries = codec.colr->color_primaries;
        info->transfer_characteristics = codec.colr->transfer_characteristics;
        info->matrix_coefficients = codec.colr->matrix_coefficients;
        info->full_range = codec.colr->full_range ? 1 : 0;
    }

    for (i = 0; i < doc->container.n_item_infos; i++) {
        heic_fourcc t = doc->container.item_infos[i].item_type;
        if (t == HEIC_TYPE_EXIF) info->has_exif = 1;
        if (t == HEIC_TYPE_MIME) {
            const char *ct = doc->container.item_infos[i].content_type;
            if (ct && (strstr(ct, "xmp") || strstr(ct, "rdf+xml")))
                info->has_xmp = 1;
        }
    }

    if (heic_container_find_thumbs(&doc->container, item.id, thumbs, 4) > 0)
        info->has_thumbnail = 1;

    if (heic_container_find_aux(&doc->container, item.id,
                                "urn:mpeg:mpegB:cicp:systems:auxiliary:alpha",
                                aux, 4) > 0 ||
        heic_container_find_aux(&doc->container, item.id,
                                "urn:mpeg:hevc:2015:auxid:1", aux, 4) > 0)
        info->has_alpha = 1;

    if (heic_container_find_aux(&doc->container, item.id,
                                "urn:com:apple:photo:2020:aux:hdrgainmap",
                                aux, 4) > 0)
        info->has_gain_map = 1;

    return 0;
}

size_t heic_doc_output_size(const heic_doc *doc, heic_format format)
{
    heic_image_info info;
    int bpp;
    if (heic_doc_info(doc, &info) != 0) return 0;
    bpp = (format == HEIC_FORMAT_RGBA || format == HEIC_FORMAT_BGRA) ? 4 : 3;
    return (size_t)info.width * (size_t)info.height * (size_t)bpp;
}

int heic_doc_exif(heic_doc *doc, uint8_t **out, size_t *out_len)
{
    int i;
    if (!doc || !out || !out_len) return 0;
    *out = NULL;
    *out_len = 0;
    for (i = 0; i < doc->container.n_item_infos; i++) {
        const heic_item_info *info = &doc->container.item_infos[i];
        const uint8_t *data = NULL;
        size_t len = 0;
        int owned = 0;
        uint8_t *copy;
        size_t payload;
        if (info->item_type != HEIC_TYPE_EXIF) continue;
        if (heic_container_item_data(&doc->container, info->item_id, &data, &len, &owned) != 0)
            continue;
        if (len <= 4) {
            if (owned) heic_free_buf(doc->ctx, (void *)data);
            continue;
        }

        payload = len - 4;
        copy = (uint8_t *)heic_zalloc(doc->ctx, payload);
        if (!copy) {
            if (owned) heic_free_buf(doc->ctx, (void *)data);
            return 0;
        }
        memcpy(copy, data + 4, payload);
        if (owned) heic_free_buf(doc->ctx, (void *)data);
        *out = copy;
        *out_len = payload;
        return 1;
    }
    return 0;
}

int heic_doc_xmp(heic_doc *doc, uint8_t **out, size_t *out_len)
{
    int i;
    if (!doc || !out || !out_len) return 0;
    *out = NULL;
    *out_len = 0;
    for (i = 0; i < doc->container.n_item_infos; i++) {
        const heic_item_info *info = &doc->container.item_infos[i];
        const uint8_t *data = NULL;
        size_t len = 0;
        int owned = 0;
        uint8_t *copy;
        if (info->item_type != HEIC_TYPE_MIME) continue;
        if (!info->content_type) continue;
        if (!strstr(info->content_type, "xmp") && !strstr(info->content_type, "rdf+xml"))
            continue;
        if (heic_container_item_data(&doc->container, info->item_id, &data, &len, &owned) != 0)
            continue;
        copy = (uint8_t *)heic_zalloc(doc->ctx, len ? len : 1);
        if (!copy) {
            if (owned) heic_free_buf(doc->ctx, (void *)data);
            return 0;
        }
        if (len) memcpy(copy, data, len);
        if (owned) heic_free_buf(doc->ctx, (void *)data);
        *out = copy;
        *out_len = len;
        return 1;
    }
    return 0;
}

int heic_doc_icc(heic_doc *doc, uint8_t **out, size_t *out_len)
{
    heic_item item;
    if (!doc || !out || !out_len) return 0;
    *out = NULL;
    *out_len = 0;
    if (primary_item(doc, &item) != 0) return 0;
    if (!item.colr || item.colr->kind != HEIC_COLR_ICC || !item.colr->icc) return 0;
    {
        uint8_t *copy = (uint8_t *)heic_zalloc(doc->ctx, item.colr->icc_len ? item.colr->icc_len : 1);
        if (!copy) return 0;
        if (item.colr->icc_len) memcpy(copy, item.colr->icc, item.colr->icc_len);
        *out = copy;
        *out_len = item.colr->icc_len;
        return 1;
    }
}

int heic_hevc_decode(heic_ctx *ctx, const heic_hvcc *cfg,
                     const uint8_t *data, size_t len,
                     heic_frame *out, const heic_abort *ab);
int heic_av1_decode(heic_ctx *ctx, const heic_av1c *cfg,
                    const uint8_t *data, size_t len,
                    heic_frame *out, const heic_abort *ab);

static int decode_item(heic_doc *doc, const heic_item *item, heic_frame *frame,
                       const heic_abort *ab, int depth);

static int frame_cropped_w(const heic_frame *f)
{
    return f->width - f->crop_left - f->crop_right;
}
static int frame_cropped_h(const heic_frame *f)
{
    return f->height - f->crop_top - f->crop_bottom;
}

static int32_t heic_round_i32(double v)
{
    if (v >= 0.0) return (int32_t)(v + 0.5);
    return (int32_t)(v - 0.5);
}

static void frame_apply_clap(heic_frame *f, const heic_clap *clap)
{
    int conf_w, conf_h;
    uint32_t clean_w, clean_h, max_extra_h, max_extra_v, extra_left, extra_top;
    uint32_t extra_right, extra_bottom;
    double horiz_off, vert_off;

    if (!f || !clap || !clap->width_d || !clap->height_d) return;
    conf_w = frame_cropped_w(f);
    conf_h = frame_cropped_h(f);
    if (conf_w <= 0 || conf_h <= 0) return;

    clean_w = clap->width_n / clap->width_d;
    clean_h = clap->height_n / clap->height_d;
    if (clean_w == 0 || clean_h == 0) return;
    if (clean_w >= (uint32_t)conf_w && clean_h >= (uint32_t)conf_h) return;
    if (clean_w > (uint32_t)conf_w) clean_w = (uint32_t)conf_w;
    if (clean_h > (uint32_t)conf_h) clean_h = (uint32_t)conf_h;

    horiz_off = clap->horiz_off_d > 0
                    ? (double)clap->horiz_off_n / (double)clap->horiz_off_d
                    : 0.0;
    vert_off = clap->vert_off_d > 0
                   ? (double)clap->vert_off_n / (double)clap->vert_off_d
                   : 0.0;

    max_extra_h = (uint32_t)conf_w - clean_w;
    max_extra_v = (uint32_t)conf_h - clean_h;
    {
        int32_t el = heic_round_i32(((double)conf_w - (double)clean_w) / 2.0 + horiz_off);
        int32_t et = heic_round_i32(((double)conf_h - (double)clean_h) / 2.0 + vert_off);
        if (el < 0) el = 0;
        if (et < 0) et = 0;
        extra_left = (uint32_t)el;
        extra_top = (uint32_t)et;
    }
    if (extra_left > max_extra_h) extra_left = max_extra_h;
    if (extra_top > max_extra_v) extra_top = max_extra_v;
    extra_right = max_extra_h - extra_left;
    extra_bottom = max_extra_v - extra_top;

    f->crop_left += (int)extra_left;
    f->crop_right += (int)extra_right;
    f->crop_top += (int)extra_top;
    f->crop_bottom += (int)extra_bottom;
}

static int frame_mirror_lr(heic_ctx *ctx, heic_frame *f)
{

    int y, x, w = f->width, h = f->height;
    (void)ctx;
    for (y = 0; y < h; y++) {
        uint16_t *row = f->y + (size_t)y * (size_t)f->y_stride;
        for (x = 0; x < w / 2; x++) {
            uint16_t t = row[x];
            row[x] = row[w - 1 - x];
            row[w - 1 - x] = t;
        }
    }
    if (f->cb && f->cr && f->c_width > 0) {
        int cw = f->c_width, ch = f->c_height;
        for (y = 0; y < ch; y++) {
            uint16_t *cb = f->cb + (size_t)y * (size_t)f->c_stride;
            uint16_t *cr = f->cr + (size_t)y * (size_t)f->c_stride;
            for (x = 0; x < cw / 2; x++) {
                uint16_t t = cb[x];
                cb[x] = cb[cw - 1 - x];
                cb[cw - 1 - x] = t;
                t = cr[x];
                cr[x] = cr[cw - 1 - x];
                cr[cw - 1 - x] = t;
            }
        }
    }
    {
        int tmp = f->crop_left;
        f->crop_left = f->crop_right;
        f->crop_right = tmp;
    }
    return 0;
}

static int frame_mirror_tb(heic_ctx *ctx, heic_frame *f)
{

    int y, x, w = f->width, h = f->height;
    (void)ctx;
    for (y = 0; y < h / 2; y++) {
        uint16_t *a = f->y + (size_t)y * (size_t)f->y_stride;
        uint16_t *b = f->y + (size_t)(h - 1 - y) * (size_t)f->y_stride;
        for (x = 0; x < w; x++) {
            uint16_t t = a[x];
            a[x] = b[x];
            b[x] = t;
        }
    }
    if (f->cb && f->cr && f->c_height > 0) {
        int cw = f->c_width, ch = f->c_height;
        for (y = 0; y < ch / 2; y++) {
            uint16_t *a = f->cb + (size_t)y * (size_t)f->c_stride;
            uint16_t *b = f->cb + (size_t)(ch - 1 - y) * (size_t)f->c_stride;
            uint16_t *c = f->cr + (size_t)y * (size_t)f->c_stride;
            uint16_t *d = f->cr + (size_t)(ch - 1 - y) * (size_t)f->c_stride;
            for (x = 0; x < cw; x++) {
                uint16_t t = a[x];
                a[x] = b[x];
                b[x] = t;
                t = c[x];
                c[x] = d[x];
                d[x] = t;
            }
        }
    }
    {
        int tmp = f->crop_top;
        f->crop_top = f->crop_bottom;
        f->crop_bottom = tmp;
    }
    return 0;
}

static int frame_rotate_90_cw(heic_ctx *ctx, heic_frame *f)
{
    heic_frame g;
    int ow = f->width, oh = f->height;
    int nw = oh, nh = ow;
    int dy, dx;
    if (heic_frame_alloc(ctx, &g, nw, nh, f->bit_depth, f->chroma_format) != 0)
        return -1;
    g.full_range = f->full_range;
    g.matrix_coeffs = f->matrix_coeffs;
    g.color_primaries = f->color_primaries;
    g.transfer_characteristics = f->transfer_characteristics;

    for (dy = 0; dy < nh; dy++) {
        for (dx = 0; dx < nw; dx++) {
            int sx = dy, sy = oh - 1 - dx;
            g.y[(size_t)dy * (size_t)g.y_stride + (size_t)dx] =
                f->y[(size_t)sy * (size_t)f->y_stride + (size_t)sx];
        }
    }
    if (f->cb && f->cr && f->c_width > 0) {
        int ocw = f->c_width, och = f->c_height;
        int ncw = och, nch = ocw;
        for (dy = 0; dy < nch; dy++) {
            for (dx = 0; dx < ncw; dx++) {
                int sx = dy, sy = och - 1 - dx;
                size_t si = (size_t)sy * (size_t)f->c_stride + (size_t)sx;
                size_t di = (size_t)dy * (size_t)g.c_stride + (size_t)dx;
                g.cb[di] = f->cb[si];
                g.cr[di] = f->cr[si];
            }
        }
    }

    g.crop_left = f->crop_bottom;
    g.crop_right = f->crop_top;
    g.crop_top = f->crop_left;
    g.crop_bottom = f->crop_right;
    heic_frame_free(ctx, f);
    *f = g;
    return 0;
}

static int frame_rotate_180(heic_ctx *ctx, heic_frame *f)
{
    if (frame_mirror_lr(ctx, f) != 0) return -1;
    return frame_mirror_tb(ctx, f);
}

static int frame_rotate_270_cw(heic_ctx *ctx, heic_frame *f)
{

    heic_frame g;
    int ow = f->width, oh = f->height;
    int nw = oh, nh = ow;
    int dy, dx;
    if (heic_frame_alloc(ctx, &g, nw, nh, f->bit_depth, f->chroma_format) != 0)
        return -1;
    g.full_range = f->full_range;
    g.matrix_coeffs = f->matrix_coeffs;
    g.color_primaries = f->color_primaries;
    g.transfer_characteristics = f->transfer_characteristics;
    for (dy = 0; dy < nh; dy++) {
        for (dx = 0; dx < nw; dx++) {
            int sx = ow - 1 - dy, sy = dx;
            g.y[(size_t)dy * (size_t)g.y_stride + (size_t)dx] =
                f->y[(size_t)sy * (size_t)f->y_stride + (size_t)sx];
        }
    }
    if (f->cb && f->cr && f->c_width > 0) {
        int ocw = f->c_width, och = f->c_height;
        int ncw = och, nch = ocw;
        for (dy = 0; dy < nch; dy++) {
            for (dx = 0; dx < ncw; dx++) {
                int sx = ocw - 1 - dy, sy = dx;
                size_t si = (size_t)sy * (size_t)f->c_stride + (size_t)sx;
                size_t di = (size_t)dy * (size_t)g.c_stride + (size_t)dx;
                g.cb[di] = f->cb[si];
                g.cr[di] = f->cr[si];
            }
        }
    }

    g.crop_left = f->crop_top;
    g.crop_right = f->crop_bottom;
    g.crop_top = f->crop_right;
    g.crop_bottom = f->crop_left;
    heic_frame_free(ctx, f);
    *f = g;
    return 0;
}

static int apply_transforms(heic_ctx *ctx, heic_frame *f, const heic_item *item)
{
    int i;
    for (i = 0; i < item->n_transforms; i++) {
        const heic_xform *t = &item->transforms[i];
        if (t->kind == HEIC_XFORM_IMIR) {

            if (t->imir.axis == 0) {
                if (frame_mirror_tb(ctx, f) != 0) return -1;
            } else {
                if (frame_mirror_lr(ctx, f) != 0) return -1;
            }
        } else if (t->kind == HEIC_XFORM_IROT) {
            switch (t->irot.angle) {
            case 90:
                if (frame_rotate_90_cw(ctx, f) != 0) return -1;
                break;
            case 180:
                if (frame_rotate_180(ctx, f) != 0) return -1;
                break;
            case 270:
                if (frame_rotate_270_cw(ctx, f) != 0) return -1;
                break;
            default:
                break;
            }
        } else if (t->kind == HEIC_XFORM_CLAP) {
            frame_apply_clap(f, &t->clap);
        }
    }
    return 0;
}

static void blit_tile(heic_frame *out, const heic_frame *tile, int tile_idx,
                      uint32_t cols, uint32_t tile_w, uint32_t tile_h,
                      uint32_t out_w, uint32_t out_h)
{
    uint32_t tile_row = (uint32_t)tile_idx / cols;
    uint32_t tile_col = (uint32_t)tile_idx % cols;
    uint32_t dst_x = tile_col * tile_w;
    uint32_t dst_y = tile_row * tile_h;
    uint32_t copy_w, copy_h, row, col;
    uint32_t src_x0 = (uint32_t)tile->crop_left;
    uint32_t src_y0 = (uint32_t)tile->crop_top;
    uint32_t tw = (uint32_t)frame_cropped_w(tile);
    uint32_t th = (uint32_t)frame_cropped_h(tile);

    if (dst_x >= out_w || dst_y >= out_h) return;
    copy_w = tw;
    if (copy_w > out_w - dst_x) copy_w = out_w - dst_x;
    copy_h = th;
    if (copy_h > out_h - dst_y) copy_h = out_h - dst_y;

    for (row = 0; row < copy_h; row++) {
        const uint16_t *src =
            tile->y + (size_t)(src_y0 + row) * (size_t)tile->y_stride + src_x0;
        uint16_t *dst =
            out->y + (size_t)(dst_y + row) * (size_t)out->y_stride + dst_x;
        memcpy(dst, src, (size_t)copy_w * sizeof(uint16_t));
    }

    if (out->chroma_format > 0 && tile->cb && out->cb) {
        uint32_t sub_x = 2, sub_y = 2;
        uint32_t c_copy_w, c_copy_h, c_dst_x, c_dst_y, c_src_x, c_src_y;
        if (out->chroma_format == 2) {
            sub_x = 2;
            sub_y = 1;
        } else if (out->chroma_format == 3) {
            sub_x = 1;
            sub_y = 1;
        }
        c_copy_w = (copy_w + sub_x - 1) / sub_x;
        c_copy_h = (copy_h + sub_y - 1) / sub_y;
        c_dst_x = dst_x / sub_x;
        c_dst_y = dst_y / sub_y;
        c_src_x = src_x0 / sub_x;
        c_src_y = src_y0 / sub_y;
        for (row = 0; row < c_copy_h; row++) {
            if (c_dst_y + row >= (uint32_t)out->c_height) break;
            if (c_src_y + row >= (uint32_t)tile->c_height) break;
            for (col = 0; col < c_copy_w; col++) {
                size_t si, di;
                if (c_dst_x + col >= (uint32_t)out->c_width) break;
                if (c_src_x + col >= (uint32_t)tile->c_width) break;
                si = (size_t)(c_src_y + row) * (size_t)tile->c_stride + (c_src_x + col);
                di = (size_t)(c_dst_y + row) * (size_t)out->c_stride + (c_dst_x + col);
                out->cb[di] = tile->cb[si];
                out->cr[di] = tile->cr[si];
            }
        }
    }
}

static int decode_grid(heic_doc *doc, const heic_item *grid_item, heic_frame *out,
                       const heic_abort *ab, int depth)
{
    const uint8_t *gdata = NULL;
    size_t glen = 0;
    int owned = 0;
    uint8_t flags;
    uint32_t rows, cols, out_w, out_h;
    uint32_t tile_ids[512];
    int n_tiles, expected, ti;
    heic_item first;
    uint32_t tile_w, tile_h;
    int bit_depth, chroma;

    if (heic_container_item_data(&doc->container, grid_item->id, &gdata, &glen, &owned) != 0)
        return -1;
    if (glen < 8) {
        if (owned) heic_free_buf(doc->ctx, (void *)gdata);
        return -1;
    }
    flags = gdata[1];
    rows = (uint32_t)gdata[2] + 1;
    cols = (uint32_t)gdata[3] + 1;
    if (flags & 1) {
        if (glen < 12) {
            if (owned) heic_free_buf(doc->ctx, (void *)gdata);
            return -1;
        }
        out_w = heic_rb32(gdata + 4);
        out_h = heic_rb32(gdata + 8);
    } else {
        out_w = heic_rb16(gdata + 4);
        out_h = heic_rb16(gdata + 6);
    }
    if (owned) heic_free_buf(doc->ctx, (void *)gdata);

    if (out_w == 0 || out_h == 0 || out_w > doc->ctx->limits.max_width ||
        out_h > doc->ctx->limits.max_height)
        return -1;
    if ((uint64_t)out_w * out_h > doc->ctx->limits.max_pixels) return -1;

    expected = (int)(rows * cols);
    if (expected <= 0 || expected > 512) {
        heic_error(doc->ctx, HEIC_SEVERITY_ERROR, "grid tile count out of range");
        return -1;
    }
    n_tiles = heic_container_find_refs(&doc->container, grid_item->id, HEIC_REF_DIMG,
                                       tile_ids, expected);
    if (n_tiles != expected) {
        heic_error(doc->ctx, HEIC_SEVERITY_ERROR, "grid tile count mismatch %d vs %d",
                   n_tiles, expected);
        return -1;
    }
    if (heic_container_get_item(&doc->container, tile_ids[0], &first) != 0) return -1;
    if (!first.has_dims) return -1;

    tile_w = first.width;
    tile_h = first.height;
    {
        int i;
        for (i = 0; i < first.n_transforms; i++) {
            const heic_xform *t = &first.transforms[i];
            if (t->kind == HEIC_XFORM_IROT &&
                (t->irot.angle == 90 || t->irot.angle == 270)) {
                uint32_t tmp = tile_w;
                tile_w = tile_h;
                tile_h = tmp;
            } else if (t->kind == HEIC_XFORM_CLAP && t->clap.width_d &&
                       t->clap.height_d) {
                uint32_t cw = t->clap.width_n / t->clap.width_d;
                uint32_t ch = t->clap.height_n / t->clap.height_d;
                if (cw > 0 && ch > 0) {
                    tile_w = cw;
                    tile_h = ch;
                }
            }
        }
    }

    if (first.hvcc) {
        bit_depth = 8 + first.hvcc->bit_depth_luma_minus8;
        chroma = first.hvcc->chroma_format;
    } else if (first.av1c) {
        bit_depth = first.av1c->high_bitdepth ? (first.av1c->twelve_bit ? 12 : 10) : 8;
        chroma = first.av1c->monochrome
                     ? 0
                     : (first.av1c->chroma_subsampling_x
                            ? (first.av1c->chroma_subsampling_y ? 1 : 2)
                            : 3);
    } else {
        heic_error(doc->ctx, HEIC_SEVERITY_ERROR, "grid tiles missing codec config");
        return -1;
    }

    if (heic_frame_alloc(doc->ctx, out, (int)out_w, (int)out_h, bit_depth, chroma) != 0)
        return -1;

    for (ti = 0; ti < n_tiles; ti++) {
        heic_item tile_item;
        heic_frame tile_frame;
        if (heic_abort_check(ab)) {
            heic_frame_free(doc->ctx, out);
            return -1;
        }
        if (heic_container_get_item(&doc->container, tile_ids[ti], &tile_item) != 0) {
            heic_frame_free(doc->ctx, out);
            return -1;
        }
        memset(&tile_frame, 0, sizeof(tile_frame));
        if (decode_item(doc, &tile_item, &tile_frame, ab, depth + 1) != 0) {
            heic_frame_free(doc->ctx, &tile_frame);
            heic_frame_free(doc->ctx, out);
            heic_error(doc->ctx, HEIC_SEVERITY_ERROR, "grid tile %d decode failed", ti);
            return -1;
        }
        if (ti == 0) {
            out->full_range = tile_frame.full_range;
            out->matrix_coeffs = tile_frame.matrix_coeffs;
            out->color_primaries = tile_frame.color_primaries;
            out->transfer_characteristics = tile_frame.transfer_characteristics;
        }
        blit_tile(out, &tile_frame, ti, cols, tile_w, tile_h, out_w, out_h);
        heic_frame_free(doc->ctx, &tile_frame);
    }

    out->crop_left = out->crop_right = out->crop_top = out->crop_bottom = 0;
    return 0;
}

static void blit_overlay(heic_frame *out, const heic_frame *tile, int32_t off_x,
                         int32_t off_y)
{

    int32_t src_x = 0, src_y = 0;
    int32_t dst_x = off_x, dst_y = off_y;
    int32_t tw = frame_cropped_w(tile);
    int32_t th = frame_cropped_h(tile);
    int32_t copy_w, copy_h, row, col;
    int32_t tile_sx = tile->crop_left, tile_sy = tile->crop_top;

    if (dst_x < 0) {
        src_x = -dst_x;
        dst_x = 0;
    }
    if (dst_y < 0) {
        src_y = -dst_y;
        dst_y = 0;
    }
    if (src_x >= tw || src_y >= th) return;
    if (dst_x >= out->width || dst_y >= out->height) return;

    copy_w = tw - src_x;
    if (copy_w > out->width - dst_x) copy_w = out->width - dst_x;
    copy_h = th - src_y;
    if (copy_h > out->height - dst_y) copy_h = out->height - dst_y;
    if (copy_w <= 0 || copy_h <= 0) return;

    for (row = 0; row < copy_h; row++) {
        const uint16_t *s = tile->y +
            (size_t)(tile_sy + src_y + row) * (size_t)tile->y_stride +
            (size_t)(tile_sx + src_x);
        uint16_t *d =
            out->y + (size_t)(dst_y + row) * (size_t)out->y_stride + (size_t)dst_x;
        memcpy(d, s, (size_t)copy_w * sizeof(uint16_t));
    }
    if (out->cb && tile->cb && out->chroma_format > 0) {
        uint32_t sub_x = 2, sub_y = 2;
        int32_t c_src_x, c_src_y, c_dst_x, c_dst_y, c_w, c_h;
        if (out->chroma_format == 2) {
            sub_x = 2;
            sub_y = 1;
        } else if (out->chroma_format == 3) {
            sub_x = 1;
            sub_y = 1;
        }
        c_src_x = (tile_sx + src_x) / (int32_t)sub_x;
        c_src_y = (tile_sy + src_y) / (int32_t)sub_y;
        c_dst_x = dst_x / (int32_t)sub_x;
        c_dst_y = dst_y / (int32_t)sub_y;
        c_w = (copy_w + (int32_t)sub_x - 1) / (int32_t)sub_x;
        c_h = (copy_h + (int32_t)sub_y - 1) / (int32_t)sub_y;
        for (row = 0; row < c_h; row++) {
            if (c_dst_y + row >= out->c_height || c_src_y + row >= tile->c_height)
                break;
            for (col = 0; col < c_w; col++) {
                size_t si, di;
                if (c_dst_x + col >= out->c_width || c_src_x + col >= tile->c_width)
                    break;
                si = (size_t)(c_src_y + row) * (size_t)tile->c_stride +
                     (size_t)(c_src_x + col);
                di = (size_t)(c_dst_y + row) * (size_t)out->c_stride +
                     (size_t)(c_dst_x + col);
                out->cb[di] = tile->cb[si];
                out->cr[di] = tile->cr[si];
            }
        }
    }
}

static int decode_iovl(heic_doc *doc, const heic_item *iovl_item, heic_frame *out,
                       const heic_abort *ab, int depth)
{
    const uint8_t *data = NULL;
    size_t len = 0;
    int owned = 0;
    uint8_t version, flags;
    int large;
    uint32_t tile_ids[64];
    int n_tiles, i;
    uint16_t fill[4];
    size_t pos;
    uint32_t canvas_w, canvas_h;
    int32_t offsets[64][2];
    heic_item first;
    int bit_depth, chroma;
    uint16_t fill_y, fill_cb, fill_cr;
    size_t n;

    if (heic_container_item_data(&doc->container, iovl_item->id, &data, &len, &owned) != 0)
        return -1;
    if (len < 2 + 8 + 4) {
        if (owned) heic_free_buf(doc->ctx, (void *)data);
        return -1;
    }
    version = data[0];
    flags = data[1];
    if (version != 0) {
        heic_error(doc->ctx, HEIC_SEVERITY_ERROR, "overlay version != 0");
        if (owned) heic_free_buf(doc->ctx, (void *)data);
        return -1;
    }
    large = (flags & 1) != 0;
    n_tiles = heic_container_find_refs(&doc->container, iovl_item->id, HEIC_REF_DIMG,
                                       tile_ids, 64);
    if (n_tiles < 1) {
        if (owned) heic_free_buf(doc->ctx, (void *)data);
        return -1;
    }
    for (i = 0; i < 4; i++) fill[i] = heic_rb16(data + 2 + i * 2);
    pos = 2 + 8;
    if (large) {
        if (pos + 8 > len) {
            if (owned) heic_free_buf(doc->ctx, (void *)data);
            return -1;
        }
        canvas_w = heic_rb32(data + pos);
        canvas_h = heic_rb32(data + pos + 4);
        pos += 8;
    } else {
        if (pos + 4 > len) {
            if (owned) heic_free_buf(doc->ctx, (void *)data);
            return -1;
        }
        canvas_w = heic_rb16(data + pos);
        canvas_h = heic_rb16(data + pos + 2);
        pos += 4;
    }
    for (i = 0; i < n_tiles; i++) {
        if (large) {
            if (pos + 8 > len) break;
            offsets[i][0] = (int32_t)heic_rb32(data + pos);
            offsets[i][1] = (int32_t)heic_rb32(data + pos + 4);
            pos += 8;
        } else {
            if (pos + 4 > len) break;
            offsets[i][0] = (int16_t)heic_rb16(data + pos);
            offsets[i][1] = (int16_t)heic_rb16(data + pos + 2);
            pos += 4;
        }
    }
    if (owned) {
        heic_free_buf(doc->ctx, (void *)data);
        owned = 0;
    }

    if (canvas_w == 0 || canvas_h == 0 || canvas_w > doc->ctx->limits.max_width ||
        canvas_h > doc->ctx->limits.max_height)
        return -1;

    if (heic_container_get_item(&doc->container, tile_ids[0], &first) != 0) return -1;
    if (first.hvcc) {
        bit_depth = 8 + first.hvcc->bit_depth_luma_minus8;
        chroma = first.hvcc->chroma_format;
    } else {
        bit_depth = 8;
        chroma = 1;
    }
    if (heic_frame_alloc(doc->ctx, out, (int)canvas_w, (int)canvas_h, bit_depth, chroma) != 0)
        return -1;

    fill_y = (uint16_t)((fill[0] >> 8) << (bit_depth > 8 ? bit_depth - 8 : 0));
    fill_cb = (uint16_t)(128 << (bit_depth > 8 ? bit_depth - 8 : 0));
    fill_cr = fill_cb;
    n = (size_t)out->width * (size_t)out->height;
    for (i = 0; i < (int)n; i++) out->y[i] = fill_y;
    if (out->cb) {
        size_t cn = (size_t)out->c_width * (size_t)out->c_height;
        size_t k;
        for (k = 0; k < cn; k++) {
            out->cb[k] = fill_cb;
            out->cr[k] = fill_cr;
        }
    }

    for (i = 0; i < n_tiles; i++) {
        heic_item tile_item;
        heic_frame tile;
        if (heic_abort_check(ab)) {
            heic_frame_free(doc->ctx, out);
            return -1;
        }
        if (heic_container_get_item(&doc->container, tile_ids[i], &tile_item) != 0) {
            heic_frame_free(doc->ctx, out);
            return -1;
        }
        memset(&tile, 0, sizeof(tile));
        if (decode_item(doc, &tile_item, &tile, ab, depth + 1) != 0) {
            heic_frame_free(doc->ctx, &tile);
            heic_frame_free(doc->ctx, out);
            return -1;
        }
        if (i == 0) {
            out->full_range = tile.full_range;
            out->matrix_coeffs = tile.matrix_coeffs;
        }
        blit_overlay(out, &tile, offsets[i][0], offsets[i][1]);
        heic_frame_free(doc->ctx, &tile);
    }
    out->crop_left = out->crop_right = out->crop_top = out->crop_bottom = 0;
    return 0;
}

static int decode_item(heic_doc *doc, const heic_item *item, heic_frame *frame,
                       const heic_abort *ab, int depth)
{
    const uint8_t *data = NULL;
    size_t len = 0;
    int owned = 0;
    int rc = -1;

    if (depth > 4) {
        heic_error(doc->ctx, HEIC_SEVERITY_ERROR, "derived item recursion too deep");
        return -1;
    }
    if (heic_abort_check(ab)) return -1;

    if (item->item_type == HEIC_TYPE_GRID) {
        rc = decode_grid(doc, item, frame, ab, depth);
        if (rc == 0 && item->colr && item->colr->kind == HEIC_COLR_NCLX) {
            frame->color_primaries = (uint8_t)item->colr->color_primaries;
            frame->transfer_characteristics =
                (uint8_t)item->colr->transfer_characteristics;
            frame->matrix_coeffs = (uint8_t)item->colr->matrix_coefficients;
            frame->full_range = item->colr->full_range;
        }
        if (rc == 0) rc = apply_transforms(doc->ctx, frame, item);
        return rc;
    }

    if (item->item_type == HEIC_TYPE_IDEN || item->item_type == HEIC_TYPE_TMAP) {
        uint32_t refs[8];
        heic_item child;
        int n = heic_container_find_refs(&doc->container, item->id, HEIC_REF_DIMG, refs, 8);
        if (n < 1) {
            heic_error(doc->ctx, HEIC_SEVERITY_ERROR, "iden/tmap missing dimg");
            return -1;
        }
        if (heic_container_get_item(&doc->container, refs[0], &child) != 0) return -1;
        rc = decode_item(doc, &child, frame, ab, depth + 1);
        if (rc == 0) rc = apply_transforms(doc->ctx, frame, item);
        return rc;
    }

    if (heic_container_item_data(&doc->container, item->id, &data, &len, &owned) != 0)
        return -1;

    if (item->item_type == HEIC_TYPE_HVC1 || item->hvcc) {
        if (!item->hvcc) {
            heic_error(doc->ctx, HEIC_SEVERITY_ERROR, "hvc1 item missing hvcC");
            goto done;
        }
        rc = heic_hevc_decode(doc->ctx, item->hvcc, data, len, frame, ab);
    } else if (item->item_type == HEIC_TYPE_AV01 || item->av1c) {
        if (!item->av1c) {
            heic_error(doc->ctx, HEIC_SEVERITY_ERROR, "av01 item missing av1C");
            goto done;
        }
        rc = heic_av1_decode(doc->ctx, item->av1c, data, len, frame, ab);
    } else if (item->item_type == HEIC_TYPE_UNCI || item->uncc) {
        if (!item->uncc) {
            heic_error(doc->ctx, HEIC_SEVERITY_ERROR, "unci item missing uncC");
            goto done;
        }
        {
            uint32_t w = item->has_dims ? item->width : 0;
            uint32_t h = item->has_dims ? item->height : 0;
            if (!w || !h) {
                heic_error(doc->ctx, HEIC_SEVERITY_ERROR, "unci item missing ispe");
                goto done;
            }
            rc = heic_unci_decode(doc->ctx, item->uncc, item->cmpc, item->cmpd, item->icef,
                                  data, len, w, h, frame, ab);
        }
    } else if (item->item_type == HEIC_TYPE_IOVL) {
        if (owned) {
            heic_free_buf(doc->ctx, (void *)data);
            owned = 0;
        }
        rc = decode_iovl(doc, item, frame, ab, depth);
        if (rc == 0) rc = apply_transforms(doc->ctx, frame, item);
        return rc;
    } else {
        heic_error(doc->ctx, HEIC_SEVERITY_ERROR,
                   "unsupported item type (hvc1/av01/unci/grid/iden/iovl/tmap)");
        rc = -1;
    }

    if (rc == 0 && item->colr && item->colr->kind == HEIC_COLR_NCLX) {
        frame->color_primaries = (uint8_t)item->colr->color_primaries;
        frame->transfer_characteristics = (uint8_t)item->colr->transfer_characteristics;
        frame->matrix_coeffs = (uint8_t)item->colr->matrix_coefficients;
        frame->full_range = item->colr->full_range;
    }
    if (rc == 0) rc = apply_transforms(doc->ctx, frame, item);
done:
    if (owned) heic_free_buf(doc->ctx, (void *)data);
    return rc;
}

static uint16_t scale_alpha_sample(uint16_t v, int src_bd, int dst_bd)
{
    if (v == HEIC_UNINIT_SAMPLE) return 0;
    if (src_bd == dst_bd || src_bd <= 0 || dst_bd <= 0) return v;
    if (src_bd < dst_bd) return (uint16_t)(v << (dst_bd - src_bd));
    return (uint16_t)(v >> (src_bd - dst_bd));
}

static void attach_alpha(heic_doc *doc, uint32_t primary_id, heic_frame *frame,
                         const heic_abort *ab)
{
    uint32_t aux_ids[4];
    int n;
    heic_item alpha_item;
    heic_frame alpha;
    int pw, ph, aw, ah, y, x;
    int abd, pbd;

    n = heic_container_find_aux(&doc->container, primary_id,
                                "urn:mpeg:mpegB:cicp:systems:auxiliary:alpha",
                                aux_ids, 4);
    if (n <= 0)
        n = heic_container_find_aux(&doc->container, primary_id,
                                    "urn:mpeg:hevc:2015:auxid:1", aux_ids, 4);
    if (n <= 0) return;
    if (heic_container_get_item(&doc->container, aux_ids[0], &alpha_item) != 0) return;
    memset(&alpha, 0, sizeof(alpha));
    if (decode_item(doc, &alpha_item, &alpha, ab, 1) != 0) {
        heic_error(doc->ctx, HEIC_SEVERITY_WARNING, "alpha item decode failed");
        heic_frame_free(doc->ctx, &alpha);
        return;
    }
    pw = frame->width - frame->crop_left - frame->crop_right;
    ph = frame->height - frame->crop_top - frame->crop_bottom;
    aw = alpha.width - alpha.crop_left - alpha.crop_right;
    ah = alpha.height - alpha.crop_top - alpha.crop_bottom;
    if (pw <= 0 || ph <= 0 || !alpha.y) {
        heic_frame_free(doc->ctx, &alpha);
        return;
    }

    frame->a = (uint16_t *)heic_zalloc(
        doc->ctx, (size_t)frame->width * (size_t)frame->height * sizeof(uint16_t));
    if (!frame->a) {
        heic_frame_free(doc->ctx, &alpha);
        return;
    }
    frame->a_stride = frame->width;
    pbd = frame->bit_depth > 0 ? frame->bit_depth : 8;
    abd = alpha.bit_depth > 0 ? alpha.bit_depth : 8;

    {
        size_t i, N = (size_t)frame->width * (size_t)frame->height;
        uint16_t opaque = (uint16_t)((1u << pbd) - 1);
        for (i = 0; i < N; i++) frame->a[i] = opaque;
    }

    if (aw == pw && ah == ph) {
        for (y = 0; y < ph; y++) {
            for (x = 0; x < pw; x++) {
                uint16_t v = alpha.y[(size_t)(alpha.crop_top + y) * (size_t)alpha.y_stride +
                                     (size_t)(alpha.crop_left + x)];
                v = scale_alpha_sample(v, abd, pbd);
                frame->a[(size_t)(frame->crop_top + y) * (size_t)frame->a_stride +
                         (size_t)(frame->crop_left + x)] = v;
            }
        }
    } else if (aw > 0 && ah > 0) {

        for (y = 0; y < ph; y++) {
            for (x = 0; x < pw; x++) {
                int sx = (int)((int64_t)x * aw / pw);
                int sy = (int)((int64_t)y * ah / ph);
                uint16_t v;
                if (sx >= aw) sx = aw - 1;
                if (sy >= ah) sy = ah - 1;
                v = alpha.y[(size_t)(alpha.crop_top + sy) * (size_t)alpha.y_stride +
                            (size_t)(alpha.crop_left + sx)];
                v = scale_alpha_sample(v, abd, pbd);
                frame->a[(size_t)(frame->crop_top + y) * (size_t)frame->a_stride +
                         (size_t)(frame->crop_left + x)] = v;
            }
        }
    }
    heic_frame_free(doc->ctx, &alpha);
}

int heic_decode_primary(heic_doc *doc, heic_format format,
                        heic_image **out_img, uint8_t *into, size_t into_size,
                        int into_stride, const heic_abort *ab)
{
    heic_item item;
    heic_frame frame;
    heic_image_info info;
    int bpp, need_stride;
    size_t need;
    uint8_t *dst;
    heic_image *img = NULL;
    int rc;
    int out_w, out_h;

    if (!doc) return -1;
    memset(&frame, 0, sizeof(frame));

    if (heic_container_get_item(&doc->container, doc->container.primary_item_id, &item) != 0)
        return -1;
    if (heic_doc_info(doc, &info) != 0) return -1;

    bpp = (format == HEIC_FORMAT_RGBA || format == HEIC_FORMAT_BGRA) ? 4 : 3;
    need_stride = into_stride > 0 ? into_stride : (int)info.width * bpp;
    need = (size_t)need_stride * (size_t)info.height;

    if (heic_abort_check(ab)) return -1;
    rc = decode_item(doc, &item, &frame, ab, 0);
    if (rc != 0) {
        heic_frame_free(doc->ctx, &frame);
        return -1;
    }
    attach_alpha(doc, item.id, &frame, ab);

    out_w = frame_cropped_w(&frame);
    out_h = frame_cropped_h(&frame);
    if (out_w <= 0 || out_h <= 0) {
        heic_frame_free(doc->ctx, &frame);
        return -1;
    }

    need_stride = into_stride > 0 ? into_stride : out_w * bpp;
    need = (size_t)need_stride * (size_t)out_h;

    if (into) {
        if (into_size < need) {
            heic_frame_free(doc->ctx, &frame);
            return -1;
        }
        dst = into;
        if (heic_frame_to_rgb(doc->ctx, &frame, format, dst, need_stride) != 0) {
            heic_frame_free(doc->ctx, &frame);
            return -1;
        }
        heic_frame_free(doc->ctx, &frame);
        return 0;
    }

    img = (heic_image *)heic_zalloc(doc->ctx, sizeof(heic_image));
    if (!img) {
        heic_frame_free(doc->ctx, &frame);
        return -1;
    }
    img->width = (uint32_t)out_w;
    img->height = (uint32_t)out_h;
    img->format = format;
    img->stride = out_w * bpp;
    img->data = (uint8_t *)heic_zalloc(doc->ctx, need);
    if (!img->data) {
        heic_free_buf(doc->ctx, img);
        heic_frame_free(doc->ctx, &frame);
        return -1;
    }
    if (heic_frame_to_rgb(doc->ctx, &frame, format, img->data, img->stride) != 0) {
        heic_image_destroy(doc->ctx, img);
        heic_frame_free(doc->ctx, &frame);
        return -1;
    }
    heic_frame_free(doc->ctx, &frame);
    if (out_img) *out_img = img;
    else heic_image_destroy(doc->ctx, img);
    return 0;
}

heic_image *heic_doc_decode(heic_doc *doc, heic_format format)
{
    heic_image *img = NULL;
    if (heic_decode_primary(doc, format, &img, NULL, 0, 0, NULL) != 0) return NULL;
    return img;
}

heic_image *heic_doc_decode_abortable(heic_doc *doc, heic_format format, heic_abort *ab)
{
    heic_image *img = NULL;
    if (heic_decode_primary(doc, format, &img, NULL, 0, 0, ab) != 0) return NULL;
    return img;
}

int heic_doc_decode_into(heic_doc *doc, heic_format format,
                         uint8_t *buf, size_t buf_size, int stride)
{
    return heic_decode_primary(doc, format, NULL, buf, buf_size, stride, NULL);
}

heic_image *heic_doc_decode_thumbnail(heic_doc *doc, heic_format format)
{
    uint32_t thumbs[8];
    int n;
    heic_item item;
    heic_frame frame;
    heic_image *img;
    int bpp;
    size_t need;
    int w, h;

    if (!doc) return NULL;
    n = heic_container_find_thumbs(&doc->container, doc->container.primary_item_id,
                                   thumbs, 8);
    if (n <= 0) return NULL;
    if (heic_container_get_item(&doc->container, thumbs[0], &item) != 0) return NULL;
    memset(&frame, 0, sizeof(frame));
    if (decode_item(doc, &item, &frame, NULL, 0) != 0) {
        heic_frame_free(doc->ctx, &frame);
        return NULL;
    }
    w = frame_cropped_w(&frame);
    h = frame_cropped_h(&frame);
    bpp = (format == HEIC_FORMAT_RGBA || format == HEIC_FORMAT_BGRA) ? 4 : 3;
    need = (size_t)w * (size_t)h * (size_t)bpp;
    img = (heic_image *)heic_zalloc(doc->ctx, sizeof(heic_image));
    if (!img) {
        heic_frame_free(doc->ctx, &frame);
        return NULL;
    }
    img->width = (uint32_t)w;
    img->height = (uint32_t)h;
    img->format = format;
    img->stride = w * bpp;
    img->data = (uint8_t *)heic_zalloc(doc->ctx, need);
    if (!img->data || heic_frame_to_rgb(doc->ctx, &frame, format, img->data, img->stride) != 0) {
        heic_image_destroy(doc->ctx, img);
        heic_frame_free(doc->ctx, &frame);
        return NULL;
    }
    heic_frame_free(doc->ctx, &frame);
    return img;
}

typedef struct {
    int full;
    int matrix;

    int cr_r, cb_g, cr_g, cb_b;

    int cr_r_l, cb_g_l, cr_g_l, cb_b_l;
} ycc_coeffs;

static void ycc_select(int matrix, int full, ycc_coeffs *c)
{
    c->full = full;
    c->matrix = matrix;
    if (matrix == 1) {
        c->cr_r = 403;
        c->cb_g = -48;
        c->cr_g = -120;
        c->cb_b = 475;
        c->cr_r_l = 14744;
        c->cb_g_l = -1754;
        c->cr_g_l = -4383;
        c->cb_b_l = 17373;
    } else if (matrix == 9) {
        c->cr_r = 377;
        c->cb_g = -42;
        c->cr_g = -146;
        c->cb_b = 482;
        c->cr_r_l = 13806;
        c->cb_g_l = -1541;
        c->cr_g_l = -5349;
        c->cb_b_l = 17615;
    } else {

        c->cr_r = 359;
        c->cb_g = -88;
        c->cr_g = -183;
        c->cb_b = 454;
        c->cr_r_l = 13126;
        c->cb_g_l = -3222;
        c->cr_g_l = -6686;
        c->cb_b_l = 16591;
    }
}

static inline uint8_t ycc_clamp8(int v)
{
    if (v < 0) return 0;
    if (v > 255) return 255;
    return (uint8_t)v;
}

typedef struct {
    int32_t yv[256];
    int32_t cr_r[256];
    int32_t cb_g[256];
    int32_t cr_g[256];
    int32_t cb_b[256];
    int full;
} ycc_lut;

static void ycc_build_lut(const ycc_coeffs *c, ycc_lut *lut)
{
    int i;
    lut->full = c->full;
    if (c->full) {
        for (i = 0; i < 256; i++) {
            int cz = i - 128;
            lut->yv[i] = i;
            lut->cr_r[i] = c->cr_r * cz;
            lut->cb_g[i] = c->cb_g * cz;
            lut->cr_g[i] = c->cr_g * cz;
            lut->cb_b[i] = c->cb_b * cz;
        }
    } else {
        for (i = 0; i < 256; i++) {
            int Y = i < 16 ? 16 : (i > 235 ? 235 : i);
            int C = i < 16 ? 16 : (i > 240 ? 240 : i);
            int cz = C - 128;
            lut->yv[i] = (Y - 16) * 9576;
            lut->cr_r[i] = c->cr_r_l * cz;
            lut->cb_g[i] = c->cb_g_l * cz;
            lut->cr_g[i] = c->cr_g_l * cz;
            lut->cb_b[i] = c->cb_b_l * cz;
        }
    }
}

static inline void ycc_pixel_lut(const ycc_lut *lut, int y, int cb, int cr,
                                 uint8_t *r, uint8_t *g, uint8_t *b)
{
    int rr, gg, bb;
    y &= 255;
    cb &= 255;
    cr &= 255;
    if (lut->full) {
        rr = lut->yv[y] + ((lut->cr_r[cr] + 128) >> 8);
        gg = lut->yv[y] + ((lut->cb_g[cb] + lut->cr_g[cr] + 128) >> 8);
        bb = lut->yv[y] + ((lut->cb_b[cb] + 128) >> 8);
    } else {
        rr = (lut->yv[y] + lut->cr_r[cr] + 4096) >> 13;
        gg = (lut->yv[y] + lut->cb_g[cb] + lut->cr_g[cr] + 4096) >> 13;
        bb = (lut->yv[y] + lut->cb_b[cb] + 4096) >> 13;
    }
    *r = ycc_clamp8(rr);
    *g = ycc_clamp8(gg);
    *b = ycc_clamp8(bb);
}

static inline void ycc_pixel(const ycc_coeffs *c, int y, int cb, int cr,
                             uint8_t *r, uint8_t *g, uint8_t *b)
{
    int cbz, crz, rr, gg, bb;

    if (c->matrix == 0) {
        *r = ycc_clamp8(cr);
        *g = ycc_clamp8(y);
        *b = ycc_clamp8(cb);
        return;
    }

    if (!c->full) {
        int yv;
        y = y < 16 ? 16 : (y > 235 ? 235 : y);
        cb = cb < 16 ? 16 : (cb > 240 ? 240 : cb);
        cr = cr < 16 ? 16 : (cr > 240 ? 240 : cr);
        cbz = cb - 128;
        crz = cr - 128;
        yv = (y - 16) * 9576;
        rr = (yv + c->cr_r_l * crz + 4096) >> 13;
        gg = (yv + c->cb_g_l * cbz + c->cr_g_l * crz + 4096) >> 13;
        bb = (yv + c->cb_b_l * cbz + 4096) >> 13;
    } else {
        cbz = cb - 128;
        crz = cr - 128;
        rr = y + ((c->cr_r * crz + 128) >> 8);
        gg = y + ((c->cb_g * cbz + c->cr_g * crz + 128) >> 8);
        bb = y + ((c->cb_b * cbz + 128) >> 8);
    }
    *r = ycc_clamp8(rr);
    *g = ycc_clamp8(gg);
    *b = ycc_clamp8(bb);
}

static inline void ycc_store(uint8_t *row, int is_bgr, int has_a, uint8_t r,
                             uint8_t g, uint8_t b, uint8_t a)
{
    if (is_bgr) {
        row[0] = b;
        row[1] = g;
        row[2] = r;
    } else {
        row[0] = r;
        row[1] = g;
        row[2] = b;
    }
    if (has_a) row[3] = a;
}

static void convert_444_8_rgb(const heic_frame *f, const ycc_lut *lut, int x0, int y0,
                              int w, int h, uint8_t *dst, int stride)
{
    int y, x;
    int full = lut->full;
    const int32_t *yv = lut->yv;
    const int32_t *cr_r = lut->cr_r;
    const int32_t *cb_g = lut->cb_g;
    const int32_t *cr_g = lut->cr_g;
    const int32_t *cb_b = lut->cb_b;
    for (y = 0; y < h; y++) {
        uint8_t *row = dst + (size_t)y * (size_t)stride;
        const uint16_t *yp =
            f->y + (size_t)(y0 + y) * (size_t)f->y_stride + (size_t)x0;
        const uint16_t *cbp =
            f->cb + (size_t)(y0 + y) * (size_t)f->c_stride + (size_t)x0;
        const uint16_t *crp =
            f->cr + (size_t)(y0 + y) * (size_t)f->c_stride + (size_t)x0;
        if (heic_simd_ycc_444_row(yp, cbp, crp, row, w, full, yv, cr_r, cb_g, cr_g,
                                  cb_b))
            continue;
        if (full) {
            for (x = 0; x < w; x++) {
                int Y = (int)yp[x] & 255;
                int Cb = (int)cbp[x] & 255;
                int Cr = (int)crp[x] & 255;
                int rr = yv[Y] + ((cr_r[Cr] + 128) >> 8);
                int gg = yv[Y] + ((cb_g[Cb] + cr_g[Cr] + 128) >> 8);
                int bb = yv[Y] + ((cb_b[Cb] + 128) >> 8);
                row[0] = ycc_clamp8(rr);
                row[1] = ycc_clamp8(gg);
                row[2] = ycc_clamp8(bb);
                row += 3;
            }
        } else {
            for (x = 0; x < w; x++) {
                int Y = (int)yp[x] & 255;
                int Cb = (int)cbp[x] & 255;
                int Cr = (int)crp[x] & 255;
                int rr = (yv[Y] + cr_r[Cr] + 4096) >> 13;
                int gg = (yv[Y] + cb_g[Cb] + cr_g[Cr] + 4096) >> 13;
                int bb = (yv[Y] + cb_b[Cb] + 4096) >> 13;
                row[0] = ycc_clamp8(rr);
                row[1] = ycc_clamp8(gg);
                row[2] = ycc_clamp8(bb);
                row += 3;
            }
        }
    }
}

static void convert_444(const heic_frame *f, const ycc_coeffs *cc, int x0, int y0,
                        int w, int h, int shift, uint8_t *dst, int stride, int bpp,
                        int is_bgr, int has_a)
{
    int y, x;
    for (y = 0; y < h; y++) {
        uint8_t *row = dst + (size_t)y * (size_t)stride;
        int sy = y0 + y;
        const uint16_t *yp = f->y + (size_t)sy * (size_t)f->y_stride + (size_t)x0;
        const uint16_t *cbp = f->cb + (size_t)sy * (size_t)f->c_stride + (size_t)x0;
        const uint16_t *crp = f->cr + (size_t)sy * (size_t)f->c_stride + (size_t)x0;
        const uint16_t *ap = NULL;
        if (has_a && f->a)
            ap = f->a + (size_t)sy * (size_t)(f->a_stride ? f->a_stride : f->y_stride) +
                 (size_t)x0;
        for (x = 0; x < w; x++) {
            int Y = (int)(yp[x] >> shift);
            int Cb = (int)(cbp[x] >> shift);
            int Cr = (int)(crp[x] >> shift);
            uint8_t r, g, b, av = 255;
            ycc_pixel(cc, Y, Cb, Cr, &r, &g, &b);
            if (ap) av = (uint8_t)(ap[x] >> shift);
            ycc_store(row, is_bgr, has_a, r, g, b, av);
            row += bpp;
        }
    }
}

static void convert_420_8_rgb(const heic_frame *f, const ycc_lut *lut, int x0, int y0,
                              int w, int h, uint8_t *dst, int stride)
{
    int y, x;
    uint16_t *cb_row = NULL, *cr_row = NULL;
    uint16_t *heap_chr = NULL;

    if (heic_simd_enabled() && w >= 4) {
        heap_chr = (uint16_t *)malloc((size_t)w * 2u * sizeof(uint16_t));
        if (heap_chr) {
            cb_row = heap_chr;
            cr_row = heap_chr + w;
        }
    }
    for (y = 0; y < h; y++) {
        uint8_t *row = dst + (size_t)y * (size_t)stride;
        int sy = y0 + y;
        int cy = sy >> 1;
        const uint16_t *yp = f->y + (size_t)sy * (size_t)f->y_stride + (size_t)x0;
        const uint16_t *cbp, *crp;
        if (cy >= f->c_height) cy = f->c_height - 1;
        if (cy < 0) cy = 0;
        cbp = f->cb + (size_t)cy * (size_t)f->c_stride;
        crp = f->cr + (size_t)cy * (size_t)f->c_stride;
        if (cb_row && cr_row) {

            int cx0 = x0 >> 1;
            for (x = 0; x < w; ) {
                int cx = cx0 + (x >> 1);
                uint16_t cb, cr;
                if (cx >= f->c_width) cx = f->c_width - 1;
                if (cx < 0) cx = 0;
                cb = cbp[cx];
                cr = crp[cx];
                cb_row[x] = cb;
                cr_row[x] = cr;
                x++;
                if (x < w) {
                    cb_row[x] = cb;
                    cr_row[x] = cr;
                    x++;
                }
            }
            if (heic_simd_ycc_444_row(yp, cb_row, cr_row, row, w, lut->full, lut->yv,
                                      lut->cr_r, lut->cb_g, lut->cr_g, lut->cb_b))
                continue;
        }
        for (x = 0; x < w; x++) {
            int cx = (x0 + x) >> 1;
            uint8_t r, g, b;
            if (cx >= f->c_width) cx = f->c_width - 1;
            if (cx < 0) cx = 0;
            ycc_pixel_lut(lut, (int)yp[x], (int)cbp[cx], (int)crp[cx], &r, &g, &b);
            row[0] = r;
            row[1] = g;
            row[2] = b;
            row += 3;
        }
    }
    free(heap_chr);
}

static void convert_420(const heic_frame *f, const ycc_coeffs *cc, int x0, int y0,
                        int w, int h, int shift, uint8_t *dst, int stride, int bpp,
                        int is_bgr, int has_a)
{
    int y, x;
    for (y = 0; y < h; y++) {
        uint8_t *row = dst + (size_t)y * (size_t)stride;
        int sy = y0 + y;
        int cy = sy >> 1;
        const uint16_t *yp = f->y + (size_t)sy * (size_t)f->y_stride + (size_t)x0;
        const uint16_t *cbp, *crp;
        const uint16_t *ap = NULL;
        if (cy >= f->c_height) cy = f->c_height - 1;
        if (cy < 0) cy = 0;
        cbp = f->cb + (size_t)cy * (size_t)f->c_stride;
        crp = f->cr + (size_t)cy * (size_t)f->c_stride;
        if (has_a && f->a)
            ap = f->a + (size_t)sy * (size_t)(f->a_stride ? f->a_stride : f->y_stride) +
                 (size_t)x0;
        for (x = 0; x < w; x++) {
            int cx = (x0 + x) >> 1;
            int Y, Cb, Cr;
            uint8_t r, g, b, av = 255;
            if (cx >= f->c_width) cx = f->c_width - 1;
            if (cx < 0) cx = 0;
            Y = (int)(yp[x] >> shift);
            Cb = (int)(cbp[cx] >> shift);
            Cr = (int)(crp[cx] >> shift);
            ycc_pixel(cc, Y, Cb, Cr, &r, &g, &b);
            if (ap) av = (uint8_t)(ap[x] >> shift);
            ycc_store(row, is_bgr, has_a, r, g, b, av);
            row += bpp;
        }
    }
}

int heic_frame_to_rgb(heic_ctx *ctx, const heic_frame *f, heic_format format,
                      uint8_t *dst, int stride)
{
    int x0, y0, x1, y1, w, h;
    int shift;
    int full;
    int matrix;
    int bpp;
    int is_bgr;
    int has_a;
    ycc_coeffs cc;

    (void)ctx;
    if (!f || !f->y || !dst) return -1;
    x0 = f->crop_left;
    y0 = f->crop_top;
    x1 = f->width - f->crop_right;
    y1 = f->height - f->crop_bottom;
    if (x1 <= x0 || y1 <= y0) return -1;
    w = x1 - x0;
    h = y1 - y0;
    shift = f->bit_depth > 8 ? f->bit_depth - 8 : 0;
    full = f->full_range;
    matrix = f->matrix_coeffs;

    if (matrix == 0 && f->chroma_format != 3) matrix = 6;
    if (matrix == 2) matrix = 6;

    bpp = (format == HEIC_FORMAT_RGBA || format == HEIC_FORMAT_BGRA) ? 4 : 3;
    is_bgr = (format == HEIC_FORMAT_BGR || format == HEIC_FORMAT_BGRA);
    has_a = (bpp == 4);
    if (stride < w * bpp) return -1;

    ycc_select(matrix, full, &cc);

    if (shift == 0 && !has_a && !is_bgr && matrix != 0
        && f->cb && f->cr
        && (f->chroma_format == 1 || f->chroma_format == 3)) {

        ycc_lut *lut = (ycc_lut *)malloc(sizeof(ycc_lut));
        if (!lut) return -1;
        ycc_build_lut(&cc, lut);
        if (f->chroma_format == 3)
            convert_444_8_rgb(f, lut, x0, y0, w, h, dst, stride);
        else
            convert_420_8_rgb(f, lut, x0, y0, w, h, dst, stride);
        free(lut);
        return 0;
    }

    if (f->chroma_format == 0 || !f->cb || !f->cr) {
        int y, x;
        for (y = 0; y < h; y++) {
            uint8_t *row = dst + (size_t)y * (size_t)stride;
            int sy = y0 + y;
            const uint16_t *yp = f->y + (size_t)sy * (size_t)f->y_stride + (size_t)x0;
            const uint16_t *ap = NULL;
            if (has_a && f->a)
                ap = f->a +
                     (size_t)sy * (size_t)(f->a_stride ? f->a_stride : f->y_stride) +
                     (size_t)x0;
            for (x = 0; x < w; x++) {
                uint16_t ys = yp[x];
                int Y;
                uint8_t r, g, b, av = 255;
                if (ys == HEIC_UNINIT_SAMPLE) ys = 0;
                Y = (int)(ys >> shift);
                ycc_pixel(&cc, Y, 128, 128, &r, &g, &b);
                if (ap) {
                    uint16_t as = ap[x];
                    if (as != HEIC_UNINIT_SAMPLE) av = (uint8_t)(as >> shift);
                }
                ycc_store(row, is_bgr, has_a, r, g, b, av);
                row += bpp;
            }
        }
        return 0;
    }

    if (f->chroma_format == 3) {
        convert_444(f, &cc, x0, y0, w, h, shift, dst, stride, bpp, is_bgr, has_a);
        return 0;
    }
    if (f->chroma_format == 1) {
        convert_420(f, &cc, x0, y0, w, h, shift, dst, stride, bpp, is_bgr, has_a);
        return 0;
    }

    {
        int y, x;
        for (y = 0; y < h; y++) {
            uint8_t *row = dst + (size_t)y * (size_t)stride;
            int sy = y0 + y;
            for (x = 0; x < w; x++) {
                int sx = x0 + x;
                int Y, Cb, Cr;
                uint8_t r, g, b, av = 255;
                uint16_t ys = f->y[(size_t)sy * (size_t)f->y_stride + (size_t)sx];
                int cx = sx >> 1, cy = sy;
                uint16_t cbs, crs;
                if (ys == HEIC_UNINIT_SAMPLE) ys = 0;
                Y = (int)(ys >> shift);
                if (cx >= f->c_width) cx = f->c_width - 1;
                if (cy >= f->c_height) cy = f->c_height - 1;
                if (cx < 0) cx = 0;
                if (cy < 0) cy = 0;
                cbs = f->cb[(size_t)cy * (size_t)f->c_stride + (size_t)cx];
                crs = f->cr[(size_t)cy * (size_t)f->c_stride + (size_t)cx];
                if (cbs == HEIC_UNINIT_SAMPLE) cbs = (uint16_t)(128u << shift);
                if (crs == HEIC_UNINIT_SAMPLE) crs = (uint16_t)(128u << shift);
                Cb = (int)(cbs >> shift);
                Cr = (int)(crs >> shift);
                ycc_pixel(&cc, Y, Cb, Cr, &r, &g, &b);
                if (has_a && f->a) {
                    uint16_t as =
                        f->a[(size_t)sy * (size_t)(f->a_stride ? f->a_stride : f->y_stride) +
                             (size_t)sx];
                    if (as != HEIC_UNINIT_SAMPLE) av = (uint8_t)(as >> shift);
                }
                ycc_store(row, is_bgr, has_a, r, g, b, av);
                row += bpp;
            }
        }
    }
    return 0;
}

int heic_nal_is_slice(heic_nal_type t)
{
    return t <= HEIC_NAL_RASL_R ||
           (t >= HEIC_NAL_BLA_W_LP && t <= HEIC_NAL_CRA);
}

static heic_nal_type nal_type_from_u8(uint8_t v)
{
    if (v <= 21 || (v >= 32 && v <= 40)) return (heic_nal_type)v;
    return HEIC_NAL_UNKNOWN;
}

static int rbsp_unescape(heic_ctx *ctx, const uint8_t *src, size_t len,
                         uint8_t **out, size_t *out_len,
                         uint32_t **ep_out, int *n_ep)
{
    uint8_t *dst;
    uint32_t *eps = NULL;
    int ne = 0, cap = 0;
    size_t i, j;
    dst = (uint8_t *)heic_zalloc(ctx, len ? len : 1);
    if (!dst) return -1;
    j = 0;
    for (i = 0; i < len; i++) {
        if (i + 2 < len && src[i] == 0 && src[i + 1] == 0 && src[i + 2] == 3) {
            dst[j++] = 0;
            dst[j++] = 0;

            if (ne >= cap) {
                int ncap = cap ? cap * 2 : 8;
                uint32_t *np = (uint32_t *)heic_realloc_buf(
                    ctx, eps, (size_t)cap * sizeof(uint32_t),
                    (size_t)ncap * sizeof(uint32_t));
                if (!np) {
                    heic_free_buf(ctx, dst);
                    heic_free_buf(ctx, eps);
                    return -1;
                }
                eps = np;
                cap = ncap;
            }
            eps[ne++] = (uint32_t)(i + 2);
            i += 2;
            continue;
        }
        dst[j++] = src[i];
    }
    *out = dst;
    *out_len = j;
    if (ep_out) *ep_out = eps;
    else heic_free_buf(ctx, eps);
    if (n_ep) *n_ep = ne;
    return 0;
}

int heic_parse_single_nal(heic_ctx *ctx, const uint8_t *data, size_t len, heic_nal *out)
{
    uint8_t nal_hdr0, nal_hdr1;
    uint8_t nal_unit_type;
    const uint8_t *payload;
    size_t payload_len;

    memset(out, 0, sizeof(*out));
    if (!data || len < 2) return -1;

    if (len >= 4 && data[0] == 0 && data[1] == 0 && data[2] == 0 && data[3] == 1) {
        data += 4;
        len -= 4;
    } else if (len >= 3 && data[0] == 0 && data[1] == 0 && data[2] == 1) {
        data += 3;
        len -= 3;
    }
    if (len < 2) return -1;
    nal_hdr0 = data[0];
    nal_hdr1 = data[1];
    if (nal_hdr0 & 0x80) return -1;
    nal_unit_type = (nal_hdr0 >> 1) & 0x3F;
    out->type = nal_type_from_u8(nal_unit_type);
    out->nuh_layer_id = (uint8_t)(((nal_hdr0 & 1) << 5) | (nal_hdr1 >> 3));
    out->temporal_id = (nal_hdr1 & 7);
    if (out->temporal_id == 0) return -1;
    out->temporal_id -= 1;
    payload = data + 2;
    payload_len = len - 2;
    if (rbsp_unescape(ctx, payload, payload_len, &out->owned, &out->payload_len,
                      &out->ep_positions, &out->n_ep_positions) != 0)
        return -1;
    out->payload = out->owned;
    return 0;
}

void heic_nal_free(heic_ctx *ctx, heic_nal *n)
{
    if (!n) return;
    heic_free_buf(ctx, n->owned);
    heic_free_buf(ctx, n->ep_positions);
    memset(n, 0, sizeof(*n));
}

void heic_nals_free(heic_ctx *ctx, heic_nal *nals, int n)
{
    int i;
    if (!nals) return;
    for (i = 0; i < n; i++) heic_nal_free(ctx, &nals[i]);
    heic_free_buf(ctx, nals);
}

int heic_parse_length_prefixed(heic_ctx *ctx, const uint8_t *data, size_t len,
                               int length_size, heic_nal **out, int *out_n)
{
    size_t pos = 0;
    heic_nal *nals = NULL;
    int n = 0, cap = 0;

    if (!out || !out_n || length_size < 1 || length_size > 4) return -1;
    *out = NULL;
    *out_n = 0;

    while (pos + (size_t)length_size <= len) {
        size_t nalu_len = 0;
        int k;
        heic_nal nal;
        for (k = 0; k < length_size; k++)
            nalu_len = (nalu_len << 8) | data[pos + k];
        pos += (size_t)length_size;
        if (nalu_len > len - pos || nalu_len > HEIC_MAX_NAL_UNIT_SIZE) break;
        if (heic_parse_single_nal(ctx, data + pos, nalu_len, &nal) != 0) {
            pos += nalu_len;
            continue;
        }
        pos += nalu_len;
        if (n >= cap) {
            int ncap = cap ? cap * 2 : 8;
            heic_nal *nn = (heic_nal *)heic_realloc_buf(
                ctx, nals, (size_t)cap * sizeof(heic_nal),
                (size_t)ncap * sizeof(heic_nal));
            if (!nn) {
                heic_nal_free(ctx, &nal);
                heic_nals_free(ctx, nals, n);
                return -1;
            }
            nals = nn;
            cap = ncap;
        }
        nals[n++] = nal;
    }
    *out = nals;
    *out_n = n;
    return 0;
}

void heic_bs_init(heic_bs *bs, const uint8_t *data, size_t len)
{
    memset(bs, 0, sizeof(*bs));
    bs->data = data;
    bs->len = len;
}

int heic_bs_bit(heic_bs *bs)
{
    int b;
    if (!bs || bs->error || bs->byte_pos >= bs->len) {
        if (bs) bs->error = 1;
        return 0;
    }
    b = (bs->data[bs->byte_pos] >> (7 - bs->bit_pos)) & 1;
    bs->bit_pos++;
    if (bs->bit_pos == 8) {
        bs->bit_pos = 0;
        bs->byte_pos++;
    }
    return b;
}

uint32_t heic_bs_bits(heic_bs *bs, int n)
{
    uint32_t v = 0;
    int i;
    for (i = 0; i < n; i++) v = (v << 1) | (uint32_t)heic_bs_bit(bs);
    return v;
}

uint32_t heic_bs_ue(heic_bs *bs)
{
    int leading = 0;
    uint32_t suffix;
    while (heic_bs_bit(bs) == 0) {
        leading++;
        if (leading > 31 || bs->error) {
            bs->error = 1;
            return 0;
        }
    }
    if (leading == 0) return 0;
    suffix = heic_bs_bits(bs, leading);
    return ((1u << leading) - 1u) + suffix;
}

int32_t heic_bs_se(heic_bs *bs)
{
    uint32_t code = heic_bs_ue(bs);
    if (code == 0) return 0;
    if (code & 1) return (int32_t)((code + 1) >> 1);
    return -(int32_t)(code >> 1);
}

int heic_bs_byte_aligned(const heic_bs *bs)
{
    return bs && bs->bit_pos == 0;
}

void heic_bs_byte_align(heic_bs *bs)
{
    if (!bs) return;
    if (bs->bit_pos != 0) {
        bs->bit_pos = 0;
        bs->byte_pos++;
    }
}

size_t heic_bs_bits_left(const heic_bs *bs)
{
    if (!bs || bs->byte_pos >= bs->len) return 0;
    return (bs->len - bs->byte_pos) * 8u - (size_t)bs->bit_pos;
}

static void skip_profile_tier_level(heic_bs *bs, int max_sub_layers_minus1)
{
    int i, j;
    (void)heic_bs_bits(bs, 2 + 1 + 5);
    (void)heic_bs_bits(bs, 32);
    (void)heic_bs_bits(bs, 48);
    (void)heic_bs_bits(bs, 8);
    {
        int sub_layer_profile_present[8];
        int sub_layer_level_present[8];
        for (i = 0; i < max_sub_layers_minus1; i++) {
            sub_layer_profile_present[i] = heic_bs_bit(bs);
            sub_layer_level_present[i] = heic_bs_bit(bs);
        }
        if (max_sub_layers_minus1 > 0)
            for (i = max_sub_layers_minus1; i < 8; i++) (void)heic_bs_bits(bs, 2);
        for (i = 0; i < max_sub_layers_minus1; i++) {
            if (sub_layer_profile_present[i]) {
                (void)heic_bs_bits(bs, 2 + 1 + 5);
                (void)heic_bs_bits(bs, 32);
                (void)heic_bs_bits(bs, 48);
            }
            if (sub_layer_level_present[i]) (void)heic_bs_bits(bs, 8);
        }
    }
    (void)j;
}

static void skip_scaling_list_data(heic_bs *bs)
{
    int sizeId, matrixId;
    for (sizeId = 0; sizeId < 4; sizeId++) {
        for (matrixId = 0; matrixId < (sizeId == 3 ? 2 : 6); matrixId++) {
            if (!heic_bs_bit(bs)) {
                (void)heic_bs_ue(bs);
            } else {
                int coefNum = HEIC_MIN(64, 1 << (4 + (sizeId << 1)));
                int i;
                if (sizeId > 1) (void)heic_bs_se(bs);
                for (i = 0; i < coefNum; i++) (void)heic_bs_se(bs);
            }
        }
    }
}

static void skip_st_ref_pic_set(heic_bs *bs, int stRpsIdx, int num_short_term_ref_pic_sets)
{
    int inter_ref_pic_set_prediction_flag = 0;
    if (stRpsIdx != 0) inter_ref_pic_set_prediction_flag = heic_bs_bit(bs);
    if (inter_ref_pic_set_prediction_flag) {
        if (stRpsIdx == num_short_term_ref_pic_sets) (void)heic_bs_ue(bs);
        (void)heic_bs_bit(bs);
        (void)heic_bs_ue(bs);

        bs->error = 1;
    } else {
        uint32_t num_neg = heic_bs_ue(bs);
        uint32_t num_pos = heic_bs_ue(bs);
        uint32_t i;

        if (num_neg > 16 || num_pos > 16 || num_neg + num_pos > 16) {
            bs->error = 1;
            return;
        }
        for (i = 0; i < num_neg; i++) {
            (void)heic_bs_ue(bs);
            (void)heic_bs_bit(bs);
        }
        for (i = 0; i < num_pos; i++) {
            (void)heic_bs_ue(bs);
            (void)heic_bs_bit(bs);
        }
    }
}

int heic_parse_sps(heic_ctx *ctx, const uint8_t *rbsp, size_t len, heic_sps *out)
{
    heic_bs bs;
    uint32_t min_cb, ctb, min_tb, max_tb;
    uint32_t num_st_rps, i;

    memset(out, 0, sizeof(*out));
    if (!rbsp || len < 4) return -1;
    heic_bs_init(&bs, rbsp, len);

    out->sps_video_parameter_set_id = (uint8_t)heic_bs_bits(&bs, 4);
    out->sps_max_sub_layers_minus1 = (uint8_t)heic_bs_bits(&bs, 3);
    out->sps_temporal_id_nesting_flag = heic_bs_bit(&bs);
    skip_profile_tier_level(&bs, out->sps_max_sub_layers_minus1);

    (void)heic_bs_ue(&bs);
    out->chroma_format_idc = (uint8_t)heic_bs_ue(&bs);
    if (out->chroma_format_idc == 3) out->separate_colour_plane_flag = heic_bs_bit(&bs);

    out->pic_width_in_luma_samples = heic_bs_ue(&bs);
    out->pic_height_in_luma_samples = heic_bs_ue(&bs);
    if (out->pic_width_in_luma_samples == 0 || out->pic_height_in_luma_samples == 0 ||
        out->pic_width_in_luma_samples > 16384 || out->pic_height_in_luma_samples > 16384) {
        heic_error(ctx, HEIC_SEVERITY_ERROR, "SPS invalid dimensions");
        return -1;
    }

    out->conformance_window_flag = heic_bs_bit(&bs);
    if (out->conformance_window_flag) {
        out->conf_win_left_offset = heic_bs_ue(&bs);
        out->conf_win_right_offset = heic_bs_ue(&bs);
        out->conf_win_top_offset = heic_bs_ue(&bs);
        out->conf_win_bottom_offset = heic_bs_ue(&bs);
    }

    out->bit_depth_luma_minus8 = (uint8_t)heic_bs_ue(&bs);
    out->bit_depth_chroma_minus8 = (uint8_t)heic_bs_ue(&bs);
    if (out->bit_depth_luma_minus8 > 8 || out->bit_depth_chroma_minus8 > 8) {
        heic_error(ctx, HEIC_SEVERITY_ERROR, "SPS bit_depth_minus8 out of range");
        return -1;
    }
    out->log2_max_pic_order_cnt_lsb_minus4 = (uint8_t)heic_bs_ue(&bs);
    if (out->log2_max_pic_order_cnt_lsb_minus4 > 12) {
        heic_error(ctx, HEIC_SEVERITY_ERROR, "SPS log2_max_poc_lsb_minus4 out of range");
        return -1;
    }

    {
        int sublayer_ordering_info = heic_bs_bit(&bs);
        int start = sublayer_ordering_info ? 0 : out->sps_max_sub_layers_minus1;
        for (i = (uint32_t)start; i <= out->sps_max_sub_layers_minus1; i++) {
            (void)heic_bs_ue(&bs);
            (void)heic_bs_ue(&bs);
            (void)heic_bs_ue(&bs);
        }
    }

    out->log2_min_luma_coding_block_size_minus3 = (uint8_t)heic_bs_ue(&bs);
    out->log2_diff_max_min_luma_coding_block_size = (uint8_t)heic_bs_ue(&bs);
    out->log2_min_luma_transform_block_size_minus2 = (uint8_t)heic_bs_ue(&bs);
    out->log2_diff_max_min_luma_transform_block_size = (uint8_t)heic_bs_ue(&bs);

    if (out->log2_min_luma_coding_block_size_minus3 > 3
        || out->log2_diff_max_min_luma_coding_block_size > 3
        || out->log2_min_luma_transform_block_size_minus2 > 3
        || out->log2_diff_max_min_luma_transform_block_size > 3) {
        heic_error(ctx, HEIC_SEVERITY_ERROR, "SPS coding/transform size fields out of range");
        return -1;
    }
    out->max_transform_hierarchy_depth_inter = (uint8_t)heic_bs_ue(&bs);
    out->max_transform_hierarchy_depth_intra = (uint8_t)heic_bs_ue(&bs);
    if (out->max_transform_hierarchy_depth_inter > 5
        || out->max_transform_hierarchy_depth_intra > 5) {
        heic_error(ctx, HEIC_SEVERITY_ERROR, "SPS max_transform_hierarchy_depth out of range");
        return -1;
    }

    out->scaling_list_enabled_flag = heic_bs_bit(&bs);
    if (out->scaling_list_enabled_flag) {
        if (heic_bs_bit(&bs)) skip_scaling_list_data(&bs);
    }

    out->amp_enabled_flag = heic_bs_bit(&bs);
    out->sample_adaptive_offset_enabled_flag = heic_bs_bit(&bs);
    out->pcm_enabled_flag = heic_bs_bit(&bs);
    if (out->pcm_enabled_flag) {
        out->pcm_sample_bit_depth_luma_minus1 = (uint8_t)heic_bs_bits(&bs, 4);
        out->pcm_sample_bit_depth_chroma_minus1 = (uint8_t)heic_bs_bits(&bs, 4);
        out->log2_min_pcm_luma_coding_block_size_minus3 = (uint8_t)heic_bs_ue(&bs);
        out->log2_diff_max_min_pcm_luma_coding_block_size = (uint8_t)heic_bs_ue(&bs);
        out->pcm_loop_filter_disabled_flag = heic_bs_bit(&bs);
    }

    num_st_rps = heic_bs_ue(&bs);
    if (num_st_rps > 64) return -1;
    for (i = 0; i < num_st_rps; i++) skip_st_ref_pic_set(&bs, (int)i, (int)num_st_rps);

    out->long_term_ref_pics_present_flag = heic_bs_bit(&bs);
    if (out->long_term_ref_pics_present_flag) {
        uint32_t num_lt = heic_bs_ue(&bs);
        uint32_t k;

        if (num_lt > 32) {
            heic_error(ctx, HEIC_SEVERITY_ERROR, "SPS num_long_term_ref_pics_sps out of range");
            return -1;
        }
        for (k = 0; k < num_lt; k++) {
            (void)heic_bs_bits(&bs, out->log2_max_pic_order_cnt_lsb_minus4 + 4);
            (void)heic_bs_bit(&bs);
        }
    }
    out->sps_temporal_mvp_enabled_flag = heic_bs_bit(&bs);
    out->strong_intra_smoothing_enabled_flag = heic_bs_bit(&bs);

    out->vui_parameters_present_flag = heic_bs_bit(&bs);
    if (out->vui_parameters_present_flag) {

        if (heic_bs_bit(&bs)) {
            uint8_t idc = (uint8_t)heic_bs_bits(&bs, 8);
            if (idc == 255) {
                (void)heic_bs_bits(&bs, 16);
                (void)heic_bs_bits(&bs, 16);
            }
        }
        if (heic_bs_bit(&bs)) (void)heic_bs_bit(&bs);
        out->video_signal_type_present_flag = heic_bs_bit(&bs);
        if (out->video_signal_type_present_flag) {
            (void)heic_bs_bits(&bs, 3);
            out->video_full_range_flag = heic_bs_bit(&bs);
            out->colour_description_present_flag = heic_bs_bit(&bs);
            if (out->colour_description_present_flag) {
                out->colour_primaries = (uint8_t)heic_bs_bits(&bs, 8);
                out->transfer_characteristics = (uint8_t)heic_bs_bits(&bs, 8);
                out->matrix_coeffs = (uint8_t)heic_bs_bits(&bs, 8);
            }
        }

    }

    if (bs.error) return -1;

    out->log2_min_cb_size = (uint8_t)(out->log2_min_luma_coding_block_size_minus3 + 3);
    out->log2_ctb_size =
        (uint8_t)(out->log2_min_cb_size + out->log2_diff_max_min_luma_coding_block_size);
    out->log2_min_tb_size = (uint8_t)(out->log2_min_luma_transform_block_size_minus2 + 2);
    out->log2_max_tb_size =
        (uint8_t)(out->log2_min_tb_size + out->log2_diff_max_min_luma_transform_block_size);

    if (out->log2_max_tb_size > 5 || out->log2_min_tb_size > out->log2_min_cb_size
        || out->log2_max_tb_size > out->log2_ctb_size || out->log2_ctb_size > 6) {
        heic_error(ctx, HEIC_SEVERITY_ERROR, "SPS derived coding/transform sizes invalid");
        return -1;
    }

    min_cb = 1u << out->log2_min_cb_size;
    ctb = 1u << out->log2_ctb_size;
    min_tb = 1u << out->log2_min_tb_size;
    max_tb = 1u << out->log2_max_tb_size;
    (void)min_tb;
    (void)max_tb;

    if ((out->pic_width_in_luma_samples % min_cb) != 0
        || (out->pic_height_in_luma_samples % min_cb) != 0) {
        heic_error(ctx, HEIC_SEVERITY_ERROR,
                   "SPS dimensions not multiple of MinCbSizeY");
        return -1;
    }
    out->ctb_width = ctb;
    out->ctb_height = ctb;
    out->pic_width_in_ctbs = (out->pic_width_in_luma_samples + ctb - 1) / ctb;
    out->pic_height_in_ctbs = (out->pic_height_in_luma_samples + ctb - 1) / ctb;
    out->pic_size_in_ctbs = out->pic_width_in_ctbs * out->pic_height_in_ctbs;
    return 0;
}

int heic_parse_pps(heic_ctx *ctx, const uint8_t *rbsp, size_t len, heic_pps *out)
{
    heic_bs bs;
    memset(out, 0, sizeof(*out));
    if (!rbsp || len < 1) return -1;
    heic_bs_init(&bs, rbsp, len);

    out->pps_pic_parameter_set_id = (uint8_t)heic_bs_ue(&bs);
    out->pps_seq_parameter_set_id = (uint8_t)heic_bs_ue(&bs);
    out->dependent_slice_segments_enabled_flag = heic_bs_bit(&bs);
    out->output_flag_present_flag = heic_bs_bit(&bs);
    out->num_extra_slice_header_bits = (uint8_t)heic_bs_bits(&bs, 3);
    out->sign_data_hiding_enabled_flag = heic_bs_bit(&bs);
    out->cabac_init_present_flag = heic_bs_bit(&bs);
    out->num_ref_idx_l0_default_active_minus1 = (uint8_t)heic_bs_ue(&bs);
    out->num_ref_idx_l1_default_active_minus1 = (uint8_t)heic_bs_ue(&bs);
    out->init_qp_minus26 = (int8_t)heic_bs_se(&bs);
    out->constrained_intra_pred_flag = heic_bs_bit(&bs);
    out->transform_skip_enabled_flag = heic_bs_bit(&bs);
    out->cu_qp_delta_enabled_flag = heic_bs_bit(&bs);
    if (out->cu_qp_delta_enabled_flag) {
        uint32_t d = heic_bs_ue(&bs);

        if (d > 6) {
            heic_error(ctx, HEIC_SEVERITY_ERROR, "PPS diff_cu_qp_delta_depth out of range");
            return -1;
        }
        out->diff_cu_qp_delta_depth = (uint8_t)d;
    }
    out->pps_cb_qp_offset = (int8_t)heic_bs_se(&bs);
    out->pps_cr_qp_offset = (int8_t)heic_bs_se(&bs);
    out->pps_slice_chroma_qp_offsets_present_flag = heic_bs_bit(&bs);
    out->weighted_pred_flag = heic_bs_bit(&bs);
    out->weighted_bipred_flag = heic_bs_bit(&bs);
    out->transquant_bypass_enabled_flag = heic_bs_bit(&bs);
    out->tiles_enabled_flag = heic_bs_bit(&bs);
    out->entropy_coding_sync_enabled_flag = heic_bs_bit(&bs);
    if (out->tiles_enabled_flag) {
        uint32_t nc = heic_bs_ue(&bs);
        uint32_t nr = heic_bs_ue(&bs);

        if (nc > 19 || nr > 21) {
            heic_error(ctx, HEIC_SEVERITY_ERROR, "PPS tile grid exceeds HEVC max 20x22");
            return -1;
        }
        out->num_tile_columns_minus1 = (uint16_t)nc;
        out->num_tile_rows_minus1 = (uint16_t)nr;
        out->uniform_spacing_flag = heic_bs_bit(&bs);
        if (!out->uniform_spacing_flag) {
            uint16_t i;
            out->column_width_minus1 =
                (uint16_t *)heic_zalloc(ctx, (size_t)nc * sizeof(uint16_t));
            out->row_height_minus1 =
                (uint16_t *)heic_zalloc(ctx, (size_t)nr * sizeof(uint16_t));
            if ((!out->column_width_minus1 && nc) || (!out->row_height_minus1 && nr))
                return -1;
            for (i = 0; i < out->num_tile_columns_minus1; i++)
                out->column_width_minus1[i] = (uint16_t)heic_bs_ue(&bs);
            for (i = 0; i < out->num_tile_rows_minus1; i++)
                out->row_height_minus1[i] = (uint16_t)heic_bs_ue(&bs);
        }
        out->loop_filter_across_tiles_enabled_flag = heic_bs_bit(&bs);
    }
    out->pps_loop_filter_across_slices_enabled_flag = heic_bs_bit(&bs);
    out->deblocking_filter_control_present_flag = heic_bs_bit(&bs);
    if (out->deblocking_filter_control_present_flag) {
        out->deblocking_filter_override_enabled_flag = heic_bs_bit(&bs);
        out->pps_deblocking_filter_disabled_flag = heic_bs_bit(&bs);
        if (!out->pps_deblocking_filter_disabled_flag) {
            out->pps_beta_offset_div2 = (int8_t)heic_bs_se(&bs);
            out->pps_tc_offset_div2 = (int8_t)heic_bs_se(&bs);
        }
    }
    out->pps_scaling_list_data_present_flag = heic_bs_bit(&bs);
    if (out->pps_scaling_list_data_present_flag)
        skip_scaling_list_data(&bs);
    out->lists_modification_present_flag = heic_bs_bit(&bs);
    out->log2_parallel_merge_level_minus2 = (uint8_t)heic_bs_ue(&bs);
    out->slice_segment_header_extension_present_flag = heic_bs_bit(&bs);

    if (bs.error) return -1;
    return 0;
}

void heic_pps_free(heic_ctx *ctx, heic_pps *pps)
{
    if (!pps) return;
    heic_free_buf(ctx, pps->column_width_minus1);
    heic_free_buf(ctx, pps->row_height_minus1);
    memset(pps, 0, sizeof(*pps));
}

static const uint8_t HEIC_LPS_TABLE[64][4] = {
    {128, 176, 208, 240}, {128, 167, 197, 227}, {128, 158, 187, 216},
    {123, 150, 178, 205}, {116, 142, 169, 195}, {111, 135, 160, 185},
    {105, 128, 152, 175}, {100, 122, 144, 166}, {95, 116, 137, 158},
    {90, 110, 130, 150},  {85, 104, 123, 142},  {81, 99, 117, 135},
    {77, 94, 111, 128},   {73, 89, 105, 122},   {69, 85, 100, 116},
    {66, 80, 95, 110},    {62, 76, 90, 104},    {59, 72, 86, 99},
    {56, 69, 81, 94},     {53, 65, 77, 89},     {51, 62, 73, 85},
    {48, 59, 69, 80},     {46, 56, 66, 76},     {43, 53, 63, 72},
    {41, 50, 59, 69},     {39, 48, 56, 65},     {37, 45, 54, 62},
    {35, 43, 51, 59},     {33, 41, 48, 56},     {32, 39, 46, 53},
    {30, 37, 43, 50},     {29, 35, 41, 48},     {27, 33, 39, 45},
    {26, 31, 37, 43},     {24, 30, 35, 41},     {23, 28, 33, 39},
    {22, 27, 32, 37},     {21, 26, 30, 35},     {20, 24, 29, 33},
    {19, 23, 27, 31},     {18, 22, 26, 30},     {17, 21, 25, 28},
    {16, 20, 23, 27},     {15, 19, 22, 25},     {14, 18, 21, 24},
    {14, 17, 20, 23},     {13, 16, 19, 22},     {12, 15, 18, 21},
    {12, 14, 17, 20},     {11, 14, 16, 19},     {11, 13, 15, 18},
    {10, 12, 15, 17},     {10, 12, 14, 16},     {9, 11, 13, 15},
    {9, 11, 12, 14},      {8, 10, 12, 14},      {8, 9, 11, 13},
    {7, 9, 11, 12},       {7, 9, 10, 12},       {7, 8, 10, 11},
    {6, 8, 9, 11},        {6, 7, 9, 10},        {6, 7, 8, 9},
    {2, 2, 2, 2},
};

static const uint8_t HEIC_STATE_TRANS_MPS[64] = {
    1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16,
    17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32,
    33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48,
    49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 62, 63,
};

static const uint8_t HEIC_STATE_TRANS_LPS[64] = {
    0,  0,  1,  2,  2,  4,  4,  5,  6,  7,  8,  9,  9,  11, 11, 12,
    13, 13, 15, 15, 16, 16, 18, 18, 19, 19, 21, 21, 22, 22, 23, 24,
    24, 25, 26, 26, 27, 27, 28, 29, 29, 30, 30, 30, 31, 32, 32, 33,
    33, 33, 34, 34, 35, 35, 35, 36, 36, 36, 37, 37, 37, 38, 38, 63,
};

static const uint8_t HEIC_CABAC_INIT_I[HEIC_NUM_CONTEXTS] = {
    139, 141, 157, 154, 197, 185, 201, 154, 149, 184, 154, 139, 154, 184,  63,  95,
     79,  63,  31,  31, 110, 122, 168, 153, 153, 140, 198, 140, 153, 138, 138, 111,
    141,  94, 138, 182, 154, 154, 139, 139, 110, 110, 124, 125, 140, 153, 125, 127,
    140, 109, 111, 143, 127, 111,  79, 108, 123,  63, 110, 110, 124, 125, 140, 153,
    125, 127, 140, 109, 111, 143, 127, 111,  79, 108, 123,  63,  91, 171, 134, 141,
    111, 111, 125, 110, 110,  94, 124, 108, 124, 107, 125, 141, 179, 153, 125, 107,
    125, 141, 179, 153, 125, 107, 125, 141, 179, 153, 125, 140, 139, 182, 182, 152,
    136, 152, 136, 153, 136, 139, 111, 136, 139, 111, 155, 154, 140,  92, 137, 138,
    140, 152, 138, 139, 153,  74, 149,  92, 139, 107, 122, 152, 140, 179, 166, 182,
    140, 227, 122, 197, 138, 153, 136, 167, 152, 152, 153, 200, 154, 154, 154, 154,
    154, 154, 154, 154, 154, 154, 154, 154, 154, 154
};

static const uint8_t HEIC_CABAC_INIT_P[HEIC_NUM_CONTEXTS] = {
    107, 139, 126, 154, 197, 185, 201, 154, 149, 154, 139, 154, 154, 154, 152,  95,
     79,  63,  31,  31, 110, 122, 168, 153, 153, 140, 198,  79, 124, 138,  94, 153,
    111, 149, 107, 167, 154, 154, 139, 139, 125, 110,  94, 110,  95,  79, 125, 111,
    110,  78, 110, 111, 111,  95,  94, 108, 123, 108, 125, 110,  94, 110,  95,  79,
    125, 111, 110,  78, 110, 111, 111,  95,  94, 108, 123, 108, 121, 140,  61, 154,
    155, 154, 139, 153, 139, 123, 123,  63, 153, 166, 183, 140, 136, 153, 154, 166,
    183, 140, 136, 153, 154, 166, 183, 140, 136, 153, 154, 170, 153, 123, 123, 107,
    121, 107, 121, 167, 151, 183, 140, 151, 183, 140, 140, 140, 154, 196, 196, 167,
    154, 152, 167, 182, 182, 134, 149, 136, 153, 121, 136, 137, 169, 194, 166, 167,
    154, 167, 137, 182, 107, 167,  91, 122, 107, 167, 153, 185, 154, 154, 154, 154,
    154, 154, 154, 154, 154, 154, 154, 154, 154, 154
};

static const uint8_t HEIC_CABAC_INIT_B[HEIC_NUM_CONTEXTS] = {
    107, 139, 126, 154, 197, 185, 201, 154, 134, 154, 139, 154, 154, 183, 152,  95,
     79,  63,  31,  31, 154, 137, 168, 153, 153, 169, 198,  79, 224, 167, 122, 153,
    111, 149,  92, 167, 154, 154, 139, 139, 125, 110, 124, 110,  95,  94, 125, 111,
    111,  79, 125, 126, 111, 111,  79, 108, 123,  93, 125, 110, 124, 110,  95,  94,
    125, 111, 111,  79, 125, 126, 111, 111,  79, 108, 123,  93, 121, 140,  61, 154,
    170, 154, 139, 153, 139, 123, 123,  63, 124, 166, 183, 140, 136, 153, 154, 166,
    183, 140, 136, 153, 154, 166, 183, 140, 136, 153, 154, 170, 153, 138, 138, 122,
    121, 122, 121, 167, 151, 183, 140, 151, 183, 140, 140, 140, 154, 196, 167, 167,
    154, 152, 167, 182, 182, 134, 149, 136, 153, 121, 136, 122, 169, 208, 166, 167,
    154, 152, 167, 182, 107, 167,  91, 107, 107, 167, 153, 160, 154, 154, 154, 154,
    154, 154, 154, 154, 154, 154, 154, 154, 154, 154
};

void heic_ctx_model_init(heic_ctx_model *m, uint8_t init_value, int slice_qp)
{
    int slope_idx = (int)(init_value >> 4);
    int offset_idx = (int)(init_value & 15);
    int mm = slope_idx * 5 - 45;
    int nn = (offset_idx << 3) - 16;
    int qp = slice_qp;
    int init_state;
    if (qp < 0) qp = 0;
    if (qp > 51) qp = 51;
    init_state = ((mm * qp) >> 4) + nn;
    if (init_state < 1) init_state = 1;
    if (init_state > 126) init_state = 126;
    if (init_state >= 64) {
        m->state = (uint8_t)(init_state - 64);
        m->mps = 1;
    } else {
        m->state = (uint8_t)(63 - init_state);
        m->mps = 0;
    }
}

void heic_cabac_init_contexts(heic_ctx_model *ctx, int slice_type, int cabac_init_flag,
                              int slice_qp)
{
    const uint8_t *table;
    int i;
    if (slice_type == HEIC_SLICE_I)
        table = HEIC_CABAC_INIT_I;
    else if (slice_type == HEIC_SLICE_P)
        table = cabac_init_flag ? HEIC_CABAC_INIT_B : HEIC_CABAC_INIT_P;
    else
        table = cabac_init_flag ? HEIC_CABAC_INIT_P : HEIC_CABAC_INIT_B;
    for (i = 0; i < HEIC_NUM_CONTEXTS; i++)
        heic_ctx_model_init(&ctx[i], table[i], slice_qp);
}

static int cabac_read_bit(heic_cabac *c)
{
    c->value <<= 1;
    c->bits_needed += 1;
    if (c->bits_needed >= 0) {
        if (c->byte_pos < c->len) {
            c->bits_needed = -8;
            c->value |= c->data[c->byte_pos];
            c->byte_pos++;
        } else {
            c->bits_needed = -8;
            c->overread_bytes++;
        }
    }
    return 0;
}

static int cabac_renorm(heic_cabac *c)
{
    int iters = 0;
    while (c->range < 256) {
        c->range <<= 1;
        cabac_read_bit(c);
        iters++;
        if (iters > 16) {
            c->error = 1;
            return -1;
        }
    }
    return 0;
}

int heic_cabac_new(heic_cabac *c, const uint8_t *data, size_t len)
{
    memset(c, 0, sizeof(*c));
    if (!data || len < 2) return -1;
    c->data = data;
    c->len = len;
    c->range = 510;
    c->bits_needed = -8;
    if (c->byte_pos < c->len) {
        c->value = c->data[c->byte_pos];
        c->byte_pos++;
    }
    c->value <<= 8;
    c->bits_needed = 0;
    if (c->byte_pos < c->len) {
        c->value |= c->data[c->byte_pos];
        c->byte_pos++;
        c->bits_needed = -8;
    }
    return 0;
}

void heic_cabac_seek(heic_cabac *c, size_t byte_pos)
{
    if (!c) return;
    c->byte_pos = byte_pos < c->len ? byte_pos : c->len;
    c->overread_bytes = 0;
}

void heic_cabac_reinit(heic_cabac *c)
{
    size_t remaining;
    c->range = 510;
    c->bits_needed = 8;
    c->value = 0;
    c->overread_bytes = 0;
    remaining = c->len > c->byte_pos ? c->len - c->byte_pos : 0;
    if (remaining > 0) {
        c->value = ((uint32_t)c->data[c->byte_pos]) << 8;
        c->byte_pos++;
        c->bits_needed -= 8;
    }
    if (remaining > 1) {
        c->value |= c->data[c->byte_pos];
        c->byte_pos++;
        c->bits_needed -= 8;
    }
}

int heic_cabac_overread(const heic_cabac *c)
{
    size_t slack = c->len + 256;
    return (size_t)c->overread_bytes > slack;
}

int heic_cabac_decode_bin(heic_cabac *c, heic_ctx_model *ctx)
{
    uint32_t q_range_idx, lps_range, scaled_range;
    int bin_val;
    if (c->error) return 0;
    q_range_idx = (c->range >> 6) & 3;
    lps_range = HEIC_LPS_TABLE[ctx->state][q_range_idx];
    c->range -= lps_range;
    scaled_range = c->range << 7;
    if (c->value < scaled_range) {
        bin_val = ctx->mps;
        ctx->state = HEIC_STATE_TRANS_MPS[ctx->state];
    } else {
        bin_val = 1 - ctx->mps;
        c->value -= scaled_range;
        c->range = lps_range;
        if (ctx->state == 0) ctx->mps = (uint8_t)(1 - ctx->mps);
        ctx->state = HEIC_STATE_TRANS_LPS[ctx->state];
    }
    if (cabac_renorm(c) != 0) return 0;
    return bin_val;
}

int heic_cabac_decode_bypass(heic_cabac *c)
{
    uint32_t scaled_range;
    int bin_val;
    if (c->error) return 0;
    c->value <<= 1;
    c->bits_needed += 1;
    if (c->bits_needed >= 0) {
        if (c->byte_pos < c->len) {
            c->bits_needed = -8;
            c->value |= c->data[c->byte_pos];
            c->byte_pos++;
        } else {
            c->bits_needed = -8;
            c->overread_bytes++;
        }
    }
    scaled_range = c->range << 7;
    if (c->value >= scaled_range) {
        c->value -= scaled_range;
        bin_val = 1;
    } else
        bin_val = 0;
    return bin_val;
}

uint32_t heic_cabac_decode_bypass_bits(heic_cabac *c, int n)
{
    uint32_t result = 0;
    int i;
    if (c->error || n <= 0) return 0;

    for (i = 0; i < n; i++) {
        uint32_t scaled_range;
        int bin_val;
        c->value <<= 1;
        c->bits_needed += 1;
        if (c->bits_needed >= 0) {
            if (c->byte_pos < c->len) {
                c->bits_needed = -8;
                c->value |= c->data[c->byte_pos];
                c->byte_pos++;
            } else {
                c->bits_needed = -8;
                c->overread_bytes++;
            }
        }
        scaled_range = c->range << 7;
        if (c->value >= scaled_range) {
            c->value -= scaled_range;
            bin_val = 1;
        } else
            bin_val = 0;
        result = (result << 1) | (uint32_t)bin_val;
    }
    return result;
}

int heic_cabac_decode_terminate(heic_cabac *c)
{
    uint32_t scaled_range;
    if (c->error) return 1;
    c->range -= 2;
    scaled_range = c->range << 7;
    if (c->value >= scaled_range) return 1;
    cabac_renorm(c);
    return 0;
}

uint32_t heic_cabac_decode_egk(heic_cabac *c, int k)
{
    uint32_t base = 0;
    int n = k;
    for (;;) {
        int bit = heic_cabac_decode_bypass(c);
        if (bit == 0) break;
        if (n >= 31) {
            c->error = 1;
            return 0;
        }
        base += 1u << n;
        n++;
        if (n >= k + 32) {
            c->error = 1;
            return 0;
        }
    }
    return base + heic_cabac_decode_bypass_bits(c, n);
}

static int ceil_log2(uint32_t x)
{
    int n = 0;
    if (x <= 1) return 0;
    x--;
    while (x) {
        x >>= 1;
        n++;
    }
    return n;
}

static int nal_is_idr(heic_nal_type t)
{
    return t == HEIC_NAL_IDR_W_RADL || t == HEIC_NAL_IDR_N_LP;
}

static int nal_is_irap(heic_nal_type t)
{
    return t >= HEIC_NAL_BLA_W_LP && t <= HEIC_NAL_CRA;
}

int heic_parse_slice_header(heic_ctx *ctx, const heic_nal *nal,
                            const heic_sps *sps, const heic_pps *pps,
                            heic_slice_header *out)
{
    heic_bs bs;
    uint32_t st;
    int i;

    memset(out, 0, sizeof(*out));
    if (!nal || !sps || !pps || !out) return -1;
    heic_bs_init(&bs, nal->payload, nal->payload_len);

    out->first_slice_segment_in_pic_flag = heic_bs_bit(&bs);
    if (nal_is_irap(nal->type))
        out->no_output_of_prior_pics_flag = heic_bs_bit(&bs);

    out->pps_id = (uint8_t)heic_bs_ue(&bs);
    if (out->pps_id != pps->pps_pic_parameter_set_id) {
        heic_error(ctx, HEIC_SEVERITY_ERROR, "slice PPS id mismatch");
        return -1;
    }

    if (!out->first_slice_segment_in_pic_flag) {
        if (pps->dependent_slice_segments_enabled_flag)
            out->dependent_slice_segment_flag = heic_bs_bit(&bs);
        {
            int bits = ceil_log2(sps->pic_size_in_ctbs);
            out->slice_segment_address = heic_bs_bits(&bs, bits);
        }
    }
    if (out->dependent_slice_segment_flag) {
        heic_error(ctx, HEIC_SEVERITY_ERROR, "dependent slice segments not supported");
        return -1;
    }

    for (i = 0; i < pps->num_extra_slice_header_bits; i++)
        (void)heic_bs_bit(&bs);

    st = heic_bs_ue(&bs);
    if (st > 2) {
        heic_error(ctx, HEIC_SEVERITY_ERROR, "invalid slice type");
        return -1;
    }
    out->slice_type = (int)st;

    out->pic_output_flag = pps->output_flag_present_flag ? heic_bs_bit(&bs) : 1;
    if (sps->separate_colour_plane_flag)
        out->colour_plane_id = (uint8_t)heic_bs_bits(&bs, 2);

    if (!nal_is_idr(nal->type)) {
        int poc_bits = sps->log2_max_pic_order_cnt_lsb_minus4 + 4;
        out->slice_pic_order_cnt_lsb = heic_bs_bits(&bs, poc_bits);

        {
            int short_term_ref_pic_set_sps_flag = heic_bs_bit(&bs);
            if (!short_term_ref_pic_set_sps_flag) {
                heic_error(ctx, HEIC_SEVERITY_ERROR,
                           "inline short-term RPS not yet parsed");
                return -1;
            }

        }
        if (sps->long_term_ref_pics_present_flag) {
            heic_error(ctx, HEIC_SEVERITY_ERROR, "long-term RPS not yet parsed");
            return -1;
        }
        if (sps->sps_temporal_mvp_enabled_flag)
            (void)heic_bs_bit(&bs);
    }

    if (sps->sample_adaptive_offset_enabled_flag) {
        out->slice_sao_luma_flag = heic_bs_bit(&bs);
        if (sps->chroma_format_idc != 0)
            out->slice_sao_chroma_flag = heic_bs_bit(&bs);
    }

    if (out->slice_type != HEIC_SLICE_I) {

        heic_error(ctx, HEIC_SEVERITY_ERROR, "P/B slices not yet supported");
        return -1;
    }

    out->slice_qp_delta = (int8_t)heic_bs_se(&bs);
    if (pps->pps_slice_chroma_qp_offsets_present_flag) {
        out->slice_cb_qp_offset = (int8_t)heic_bs_se(&bs);
        out->slice_cr_qp_offset = (int8_t)heic_bs_se(&bs);
    }

    if (pps->deblocking_filter_override_enabled_flag)
        out->deblocking_filter_override_flag = heic_bs_bit(&bs);
    if (out->deblocking_filter_override_flag) {
        out->slice_deblocking_filter_disabled_flag = heic_bs_bit(&bs);
        if (!out->slice_deblocking_filter_disabled_flag) {
            out->slice_beta_offset_div2 = (int8_t)heic_bs_se(&bs);
            out->slice_tc_offset_div2 = (int8_t)heic_bs_se(&bs);
        }
    } else {
        out->slice_deblocking_filter_disabled_flag =
            pps->pps_deblocking_filter_disabled_flag;
        out->slice_beta_offset_div2 = pps->pps_beta_offset_div2;
        out->slice_tc_offset_div2 = pps->pps_tc_offset_div2;
    }

    if (pps->pps_loop_filter_across_slices_enabled_flag &&
        (out->slice_sao_luma_flag || out->slice_sao_chroma_flag ||
         !out->slice_deblocking_filter_disabled_flag))
        out->slice_loop_filter_across_slices_enabled_flag = heic_bs_bit(&bs);
    else
        out->slice_loop_filter_across_slices_enabled_flag =
            pps->pps_loop_filter_across_slices_enabled_flag;

    if (pps->tiles_enabled_flag || pps->entropy_coding_sync_enabled_flag) {
        out->num_entry_point_offsets = heic_bs_ue(&bs);
        if (out->num_entry_point_offsets > 0) {
            uint32_t offset_len_minus1 = heic_bs_ue(&bs);
            uint32_t e;

            if (offset_len_minus1 > 31 || out->num_entry_point_offsets > 4096)
                return -1;
            out->entry_point_offsets = (uint32_t *)heic_zalloc(
                ctx, out->num_entry_point_offsets * sizeof(uint32_t));
            if (!out->entry_point_offsets) return -1;
            for (e = 0; e < out->num_entry_point_offsets; e++) {

                uint32_t v = heic_bs_bits(&bs, (int)offset_len_minus1 + 1);
                out->entry_point_offsets[e] = v + 1;
            }
        }
    }

    if (pps->slice_segment_header_extension_present_flag) {
        uint32_t ext_len = heic_bs_ue(&bs);
        uint32_t b;
        for (b = 0; b < ext_len; b++)
            (void)heic_bs_bits(&bs, 8);
    }

    (void)heic_bs_bit(&bs);
    heic_bs_byte_align(&bs);

    if (bs.error) return -1;
    out->data_offset = bs.byte_pos;
    out->slice_qp_y = 26 + (int)pps->init_qp_minus26 + (int)out->slice_qp_delta;
    return 0;
}

void heic_slice_header_free(heic_ctx *ctx, heic_slice_header *sh)
{
    if (!sh) return;
    heic_free_buf(ctx, sh->entry_point_offsets);
    sh->entry_point_offsets = NULL;
    sh->num_entry_point_offsets = 0;
}

static const uint8_t HEIC_SCAN_4X4_DIAG[16][2] = {
    {0, 0}, {0, 1}, {1, 0}, {0, 2}, {1, 1}, {2, 0}, {0, 3}, {1, 2},
    {2, 1}, {3, 0}, {1, 3}, {2, 2}, {3, 1}, {2, 3}, {3, 2}, {3, 3},
};
static const uint8_t HEIC_SCAN_4X4_HORIZ[16][2] = {
    {0, 0}, {1, 0}, {2, 0}, {3, 0}, {0, 1}, {1, 1}, {2, 1}, {3, 1},
    {0, 2}, {1, 2}, {2, 2}, {3, 2}, {0, 3}, {1, 3}, {2, 3}, {3, 3},
};
static const uint8_t HEIC_SCAN_4X4_VERT[16][2] = {
    {0, 0}, {0, 1}, {0, 2}, {0, 3}, {1, 0}, {1, 1}, {1, 2}, {1, 3},
    {2, 0}, {2, 1}, {2, 2}, {2, 3}, {3, 0}, {3, 1}, {3, 2}, {3, 3},
};

static const uint8_t HEIC_SCAN_1X1[1][2] = {{0, 0}};
static const uint8_t HEIC_SCAN_2X2_DIAG[4][2] = {{0, 0}, {0, 1}, {1, 0}, {1, 1}};
static const uint8_t HEIC_SCAN_2X2_HORIZ[4][2] = {{0, 0}, {1, 0}, {0, 1}, {1, 1}};
static const uint8_t HEIC_SCAN_4X4_SB_DIAG[16][2] = {
    {0, 0}, {0, 1}, {1, 0}, {0, 2}, {1, 1}, {2, 0}, {0, 3}, {1, 2},
    {2, 1}, {3, 0}, {1, 3}, {2, 2}, {3, 1}, {2, 3}, {3, 2}, {3, 3},
};
static const uint8_t HEIC_SCAN_8X8_SB_DIAG[64][2] = {
    {0, 0}, {0, 1}, {1, 0}, {0, 2}, {1, 1}, {2, 0}, {0, 3}, {1, 2},
    {2, 1}, {3, 0}, {0, 4}, {1, 3}, {2, 2}, {3, 1}, {4, 0}, {0, 5},
    {1, 4}, {2, 3}, {3, 2}, {4, 1}, {5, 0}, {0, 6}, {1, 5}, {2, 4},
    {3, 3}, {4, 2}, {5, 1}, {6, 0}, {0, 7}, {1, 6}, {2, 5}, {3, 4},
    {4, 3}, {5, 2}, {6, 1}, {7, 0}, {1, 7}, {2, 6}, {3, 5}, {4, 4},
    {5, 3}, {6, 2}, {7, 1}, {2, 7}, {3, 6}, {4, 5}, {5, 4}, {6, 3},
    {7, 2}, {3, 7}, {4, 6}, {5, 5}, {6, 4}, {7, 3}, {4, 7}, {5, 6},
    {6, 5}, {7, 4}, {5, 7}, {6, 6}, {7, 5}, {6, 7}, {7, 6}, {7, 7},
};

static const uint8_t HEIC_CTX_IDX_MAP_4X4[16] = {
    0, 1, 4, 5, 2, 3, 4, 5, 6, 6, 8, 8, 7, 7, 8, 8
};

int heic_get_scan_order(uint8_t log2_size, uint8_t intra_mode, uint8_t c_idx,
                        int chroma_444)
{
    int use_directional;
    if (c_idx == 0)
        use_directional = (log2_size == 2 || log2_size == 3);
    else
        use_directional = (log2_size == 2 || (log2_size == 3 && chroma_444));
    if (!use_directional) return HEIC_SCAN_DIAG;
    if (intra_mode >= 6 && intra_mode <= 14) return HEIC_SCAN_VERT;
    if (intra_mode >= 22 && intra_mode <= 30) return HEIC_SCAN_HORIZ;
    return HEIC_SCAN_DIAG;
}

static const uint8_t (*scan_4x4(int order))[2]
{
    if (order == HEIC_SCAN_HORIZ) return HEIC_SCAN_4X4_HORIZ;
    if (order == HEIC_SCAN_VERT) return HEIC_SCAN_4X4_VERT;
    return HEIC_SCAN_4X4_DIAG;
}

static void get_scan_sub(uint8_t log2_size, int order,
                         const uint8_t (**out)[2], int *out_n)
{
    switch (log2_size) {
    case 2:
        *out = HEIC_SCAN_1X1;
        *out_n = 1;
        break;
    case 3:
        if (order == HEIC_SCAN_HORIZ) {
            *out = HEIC_SCAN_2X2_HORIZ;
            *out_n = 4;
        } else {
            *out = HEIC_SCAN_2X2_DIAG;
            *out_n = 4;
        }
        break;
    case 4:
        *out = HEIC_SCAN_4X4_SB_DIAG;
        *out_n = 16;
        break;
    case 5:
        *out = HEIC_SCAN_8X8_SB_DIAG;
        *out_n = 64;
        break;
    default:
        *out = HEIC_SCAN_1X1;
        *out_n = 1;
        break;
    }
}

static uint32_t find_scan_pos(const uint8_t (*scan)[2], int n, uint32_t x, uint32_t y)
{
    int i;
    for (i = 0; i < n; i++)
        if (scan[i][0] == x && scan[i][1] == y) return (uint32_t)i;
    return 0;
}

static uint8_t find_scan_pos_4x4(const uint8_t (*scan)[2], uint8_t x, uint8_t y)
{
    int i;
    for (i = 0; i < 16; i++)
        if (scan[i][0] == x && scan[i][1] == y) return (uint8_t)i;
    return 0;
}

static void coeff_set(heic_coeff_buf *b, int x, int y, int16_t v)
{
    int stride = 1 << b->log2_size;
    b->coeffs[y * stride + x] = v;
    if (v != 0) {
        if (b->num_nonzero < 0xFFFFu) b->num_nonzero++;
    }
}

static uint32_t decode_last_sig_prefix(heic_cabac *cabac, heic_ctx_model *ctx,
                                       uint8_t log2_size, uint8_t c_idx, int is_x)
{
    int ctx_base = is_x ? HEIC_CTX_LAST_SIG_COEFF_X_PREFIX
                        : HEIC_CTX_LAST_SIG_COEFF_Y_PREFIX;
    int ctx_offset, ctx_shift, max_prefix;
    uint32_t prefix = 0;

    if (c_idx == 0) {
        ctx_offset = 3 * ((int)log2_size - 2) + (((int)log2_size - 1) >> 2);
        ctx_shift = ((int)log2_size + 1) >> 2;
    } else {
        ctx_offset = 15;
        ctx_shift = (int)log2_size - 2;
    }
    max_prefix = ((int)log2_size << 1) - 1;
    while ((int)prefix < max_prefix) {
        int ctx_idx = ctx_base + ctx_offset + ((int)prefix >> ctx_shift);
        if (heic_cabac_decode_bin(cabac, &ctx[ctx_idx]) == 0) break;
        prefix++;
    }
    return prefix;
}

static int decode_last_sig_pos(heic_cabac *cabac, heic_ctx_model *ctx,
                               uint8_t log2_size, uint8_t c_idx,
                               uint32_t *out_x, uint32_t *out_y)
{
    uint32_t x_prefix = decode_last_sig_prefix(cabac, ctx, log2_size, c_idx, 1);
    uint32_t y_prefix = decode_last_sig_prefix(cabac, ctx, log2_size, c_idx, 0);
    uint32_t x, y;
    if (x_prefix > 3) {
        uint32_t n_bits = (x_prefix >> 1) - 1;
        uint32_t suffix = heic_cabac_decode_bypass_bits(cabac, (int)n_bits);
        x = ((2 + (x_prefix & 1)) << n_bits) + suffix;
    } else {
        x = x_prefix;
    }
    if (y_prefix > 3) {
        uint32_t n_bits = (y_prefix >> 1) - 1;
        uint32_t suffix = heic_cabac_decode_bypass_bits(cabac, (int)n_bits);
        y = ((2 + (y_prefix & 1)) << n_bits) + suffix;
    } else {
        y = y_prefix;
    }
    *out_x = x;
    *out_y = y;
    return 0;
}

static int decode_coded_sb_flag(heic_cabac *cabac, heic_ctx_model *ctx,
                                uint8_t c_idx, uint8_t csbf_neighbors)
{
    int csbf_ctx = csbf_neighbors != 0 ? 1 : 0;
    int ctx_idx = HEIC_CTX_CODED_SUB_BLOCK_FLAG + csbf_ctx + (c_idx > 0 ? 2 : 0);
    return heic_cabac_decode_bin(cabac, &ctx[ctx_idx]) != 0;
}

static int calc_sig_ctx(uint8_t x_c, uint8_t y_c, uint8_t log2_size, uint8_t c_idx,
                        uint8_t scan_idx, uint8_t prev_csbf)
{
    uint8_t sb_width = (uint8_t)(1u << (log2_size - 2));
    int sig_ctx;
    if (sb_width == 1) {
        sig_ctx = HEIC_CTX_IDX_MAP_4X4[(y_c * 4 + x_c) & 15];
    } else if (x_c == 0 && y_c == 0) {
        sig_ctx = 0;
    } else {
        uint8_t x_s = x_c >> 2, y_s = y_c >> 2;
        uint8_t x_p = x_c & 3, y_p = y_c & 3;
        int ctx;
        switch (prev_csbf) {
        case 0:
            if (x_p + y_p >= 3) ctx = 0;
            else if (x_p + y_p > 0) ctx = 1;
            else ctx = 2;
            break;
        case 1:
            if (y_p == 0) ctx = 2;
            else if (y_p == 1) ctx = 1;
            else ctx = 0;
            break;
        case 2:
            if (x_p == 0) ctx = 2;
            else if (x_p == 1) ctx = 1;
            else ctx = 0;
            break;
        default:
            ctx = 2;
            break;
        }
        if (c_idx == 0) {
            if (x_s + y_s > 0) ctx += 3;
            if (sb_width == 2)
                ctx += (scan_idx == 0) ? 9 : 15;
            else
                ctx += 21;
        } else {
            if (sb_width == 2) ctx += 9;
            else ctx += 12;
        }
        sig_ctx = ctx;
    }
    return HEIC_CTX_SIG_COEFF_FLAG + (c_idx > 0 ? 27 : 0) + sig_ctx;
}

static int decode_sig_flag(heic_cabac *cabac, heic_ctx_model *ctx, uint8_t c_idx,
                           uint8_t pos, uint8_t log2_size, uint8_t scan_idx,
                           uint8_t sb_x, uint8_t sb_y, uint8_t prev_csbf,
                           const uint8_t (*scan_pos)[2])
{
    uint8_t x_c = (uint8_t)(sb_x * 4 + scan_pos[pos][0]);
    uint8_t y_c = (uint8_t)(sb_y * 4 + scan_pos[pos][1]);
    int ctx_idx = calc_sig_ctx(x_c, y_c, log2_size, c_idx, scan_idx, prev_csbf);
    return heic_cabac_decode_bin(cabac, &ctx[ctx_idx]) != 0;
}

static int decode_greater1(heic_cabac *cabac, heic_ctx_model *ctx, uint8_t c_idx,
                           uint8_t ctx_set, uint8_t greater1_ctx)
{
    int ctx_idx = HEIC_CTX_COEFF_ABS_LEVEL_GREATER1
                + (c_idx > 0 ? 16 : 0)
                + (int)ctx_set * 4
                + (greater1_ctx < 3 ? greater1_ctx : 3);
    return heic_cabac_decode_bin(cabac, &ctx[ctx_idx]) != 0;
}

static int decode_greater2(heic_cabac *cabac, heic_ctx_model *ctx, uint8_t c_idx,
                           uint8_t ctx_set)
{
    int ctx_idx = HEIC_CTX_COEFF_ABS_LEVEL_GREATER2
                + (c_idx > 0 ? 4 : 0) + (int)ctx_set;
    return heic_cabac_decode_bin(cabac, &ctx[ctx_idx]) != 0;
}

static int16_t decode_abs_remaining(heic_cabac *cabac, uint8_t *rice_param,
                                    int16_t base_level)
{
    uint32_t prefix = 0;
    int16_t value;
    uint8_t rice = *rice_param;
    uint32_t threshold;
    while (heic_cabac_decode_bypass(cabac) != 0 && prefix < 24)
        prefix++;
    if (prefix <= 3) {
        uint32_t suffix = rice > 0 ? heic_cabac_decode_bypass_bits(cabac, rice) : 0;
        value = (int16_t)((prefix << rice) + suffix);
    } else {
        uint8_t suffix_bits = (uint8_t)(prefix - 3 + rice);
        uint32_t suffix = heic_cabac_decode_bypass_bits(cabac, suffix_bits);
        uint32_t base = ((1u << (prefix - 3)) + 2u) << rice;
        value = (int16_t)(base + suffix);
    }
    threshold = 3u * (1u << rice);
    if ((uint32_t)(base_level < 0 ? -base_level : base_level)
          + (uint32_t)(value < 0 ? -value : value)
        > threshold) {
        if (rice < 4) rice++;
    }
    *rice_param = rice;
    return value;
}

int heic_decode_residual(heic_cabac *cabac, heic_ctx_model *ctx,
                         uint8_t log2_size, uint8_t c_idx, int scan_order,
                         int sign_data_hiding, int cu_transquant_bypass,
                         int transform_skip_enabled,
                         heic_coeff_buf *out, int *transform_skip)
{
    uint32_t size, last_x, last_y, last_sb_idx, last_sb_x, last_sb_y;
    int sb_width, transform_skip_flag = 0;
    const uint8_t (*scan_sub)[2];
    const uint8_t (*scan_pos)[2];
    int scan_sub_n, scan_idx;
    uint8_t local_x, local_y, last_pos_in_sb;
    int coded_sb_flags[8][8];
    int prev_subblock_had_gt1 = 0;
    uint32_t sb_idx;

    if (!cabac || !ctx || !out || log2_size < 2 || log2_size > 5) return -1;
    memset(out, 0, sizeof(*out));
    out->log2_size = log2_size;
    size = 1u << log2_size;

    if (transform_skip_enabled && !cu_transquant_bypass && log2_size <= 2) {
        int ctx_idx = HEIC_CTX_TRANSFORM_SKIP_FLAG + (c_idx > 0 ? 1 : 0);
        transform_skip_flag = heic_cabac_decode_bin(cabac, &ctx[ctx_idx]) != 0;
    }
    if (transform_skip) *transform_skip = transform_skip_flag;

    if (decode_last_sig_pos(cabac, ctx, log2_size, c_idx, &last_x, &last_y) != 0)
        return -1;

    if (scan_order == HEIC_SCAN_VERT) {
        uint32_t t = last_x;
        last_x = last_y;
        last_y = t;
    }
    if (last_x >= size || last_y >= size) return -1;

    get_scan_sub(log2_size, scan_order, &scan_sub, &scan_sub_n);
    scan_pos = scan_4x4(scan_order);
    scan_idx = scan_order;

    sb_width = (int)(size / 4);
    last_sb_x = last_x / 4;
    last_sb_y = last_y / 4;
    last_sb_idx = find_scan_pos(scan_sub, scan_sub_n, last_sb_x, last_sb_y);
    local_x = (uint8_t)(last_x % 4);
    local_y = (uint8_t)(last_y % 4);
    last_pos_in_sb = find_scan_pos_4x4(scan_pos, local_x, local_y);

    memset(coded_sb_flags, 0, sizeof(coded_sb_flags));

    sb_idx = last_sb_idx + 1;
    while (sb_idx > 0) {
        uint8_t sb_x, sb_y;
        sb_idx--;
        sb_x = scan_sub[sb_idx][0];
        sb_y = scan_sub[sb_idx][1];
        int right_coded, below_coded, csbf_neighbors, sb_coded, infer_sb_dc_sig;
        int prev_csbf;
        uint8_t start_pos, last_coeff;
        int16_t coeff_values[16];
        int coeff_flags[16];
        uint8_t num_coeffs = 0;
        int can_infer_dc;
        int n;
        int base_cs, ctx_set, this_subblock_had_gt1;
        uint8_t greater1_ctx, last_greater1_flag;
        int first_g1_idx;
        int g1_positions[16];
        int needs_remaining[16];
        int max_g1, g1_count;
        uint8_t first_sig_pos, last_sig_pos;
        int sign_hidden;
        uint8_t sig_positions[16];
        int n_sig, i;
        uint8_t coeff_signs[16];
        uint8_t rice_param;
        int32_t sum_abs_level;

        right_coded = ((int)sb_x + 1) < sb_width
                          ? coded_sb_flags[sb_y][sb_x + 1]
                          : 0;
        below_coded = ((int)sb_y + 1) < sb_width
                          ? coded_sb_flags[sb_y + 1][sb_x]
                          : 0;
        csbf_neighbors = (right_coded ? 1 : 0) | (below_coded ? 2 : 0);

        if (sb_idx > 0 && sb_idx < last_sb_idx) {
            sb_coded = decode_coded_sb_flag(cabac, ctx, c_idx, (uint8_t)csbf_neighbors);
            infer_sb_dc_sig = sb_coded;
        } else {
            sb_coded = 1;
            infer_sb_dc_sig = 0;
        }
        if (sb_coded) coded_sb_flags[sb_y][sb_x] = 1;
        prev_csbf = csbf_neighbors;
        if (!sb_coded) continue;

        start_pos = (sb_idx == last_sb_idx) ? last_pos_in_sb : 15;
        memset(coeff_values, 0, sizeof(coeff_values));
        memset(coeff_flags, 0, sizeof(coeff_flags));
        can_infer_dc = infer_sb_dc_sig;

        if (sb_idx == last_sb_idx) {
            coeff_flags[start_pos] = 1;
            coeff_values[start_pos] = 1;
            num_coeffs = 1;
            can_infer_dc = 0;
            last_coeff = start_pos > 0 ? (uint8_t)(start_pos - 1) : 0;
        } else {
            last_coeff = 15;
        }

        if (!(sb_idx == last_sb_idx && start_pos == 0)) {
            for (n = (int)last_coeff; n >= 1; n--) {
                int sig = decode_sig_flag(cabac, ctx, c_idx, (uint8_t)n, log2_size,
                                          (uint8_t)scan_idx, sb_x, sb_y,
                                          (uint8_t)prev_csbf, scan_pos);
                if (sig) {
                    coeff_flags[n] = 1;
                    coeff_values[n] = 1;
                    num_coeffs++;
                    can_infer_dc = 0;
                }
            }
        }

        if (start_pos > 0) {
            if (can_infer_dc) {
                coeff_flags[0] = 1;
                coeff_values[0] = 1;
                num_coeffs++;
            } else {
                int sig = decode_sig_flag(cabac, ctx, c_idx, 0, log2_size,
                                          (uint8_t)scan_idx, sb_x, sb_y,
                                          (uint8_t)prev_csbf, scan_pos);
                if (sig) {
                    coeff_flags[0] = 1;
                    coeff_values[0] = 1;
                    num_coeffs++;
                }
            }
        }

        if (num_coeffs == 0) continue;

        base_cs = (sb_idx == 0 || c_idx > 0) ? 0 : 2;
        ctx_set = base_cs + (prev_subblock_had_gt1 ? 1 : 0);
        this_subblock_had_gt1 = 0;
        greater1_ctx = 1;
        last_greater1_flag = 0;
        first_g1_idx = -1;
        memset(g1_positions, 0, sizeof(g1_positions));
        memset(needs_remaining, 0, sizeof(needs_remaining));
        max_g1 = num_coeffs < 8 ? num_coeffs : 8;
        g1_count = 0;

        for (n = (int)start_pos; n >= 0; n--) {
            int g1;
            if (!coeff_flags[n]) continue;
            if (g1_count >= max_g1) {
                needs_remaining[n] = 1;
                continue;
            }
            if (g1_count > 0 && greater1_ctx > 0) {
                if (last_greater1_flag)
                    greater1_ctx = 0;
                else
                    greater1_ctx++;
            }
            g1 = decode_greater1(cabac, ctx, c_idx, (uint8_t)ctx_set, greater1_ctx);
            last_greater1_flag = (uint8_t)g1;
            if (g1) {
                coeff_values[n] = 2;
                g1_positions[n] = 1;
                this_subblock_had_gt1 = 1;
                if (first_g1_idx < 0)
                    first_g1_idx = n;
                else
                    needs_remaining[n] = 1;
            }
            g1_count++;
        }

        if (first_g1_idx >= 0) {
            if (decode_greater2(cabac, ctx, c_idx, (uint8_t)ctx_set)) {
                coeff_values[first_g1_idx] = 3;
                needs_remaining[first_g1_idx] = 1;
            }
        }

        n_sig = 0;
        for (n = (int)start_pos; n >= 0; n--) {
            if (coeff_flags[n]) sig_positions[n_sig++] = (uint8_t)n;
        }
        if (n_sig == 0) {
            prev_subblock_had_gt1 = this_subblock_had_gt1;
            continue;
        }

        last_sig_pos = sig_positions[0];
        first_sig_pos = sig_positions[n_sig - 1];
        sign_hidden = sign_data_hiding && !cu_transquant_bypass
                      && (last_sig_pos - first_sig_pos) > 3;

        memset(coeff_signs, 0, sizeof(coeff_signs));
        for (i = 0; i < n_sig - 1; i++)
            coeff_signs[i] = (uint8_t)heic_cabac_decode_bypass(cabac);
        if (!sign_hidden)
            coeff_signs[n_sig - 1] = (uint8_t)heic_cabac_decode_bypass(cabac);

        rice_param = 0;
        sum_abs_level = 0;
        for (i = 0; i < n_sig; i++) {
            int pos = sig_positions[i];
            int16_t v = coeff_values[pos];
            if (needs_remaining[pos]) {
                int16_t rem = decode_abs_remaining(cabac, &rice_param, v);
                int32_t sum = (int32_t)v + (int32_t)rem;
                if (sum > 32767) sum = 32767;
                v = (int16_t)sum;
            }
            if (coeff_signs[i]) v = (int16_t)(-v);
            sum_abs_level += v;
            if (i == n_sig - 1 && sign_hidden && (sum_abs_level & 1) != 0)
                v = (int16_t)(-v);
            coeff_values[pos] = v;
            {
                int x = (int)sb_x * 4 + scan_pos[pos][0];
                int y = (int)sb_y * 4 + scan_pos[pos][1];
                coeff_set(out, x, y, v);
            }
        }

        prev_subblock_had_gt1 = this_subblock_had_gt1;
    }

    return cabac->error ? -1 : 0;
}

#if defined(_MSC_VER)
#define HEIC_TLS __declspec(thread)
#elif defined(__GNUC__) || defined(__clang__)
#define HEIC_TLS __thread
#else
#define HEIC_TLS
#endif

static HEIC_TLS int32_t heic_idct_scratch[1024];

int32_t *heic_idct_scratch_buf(void)
{
    return heic_idct_scratch;
}

static const int16_t HEIC_DST4[4][4] = {
    {29, 55, 74, 84},
    {74, 74, 0, -74},
    {84, -29, -74, 55},
    {55, -84, 74, -29},
};
static const int16_t HEIC_DCT4[4][4] = {
    {64, 64, 64, 64},
    {83, 36, -36, -83},
    {64, -64, -64, 64},
    {36, -83, 83, -36},
};

static inline int32_t htx_clip16(int32_t v)
{
    if (v < -32768) return -32768;
    if (v > 32767) return 32767;
    return v;
}

static int htx_only_dc(const int16_t *coeffs, int n)
{
    int i, nn = n * n;
    for (i = 1; i < nn; i++) {
        if (coeffs[i]) return 0;
    }
    return 1;
}

static void htx_idct_dc_fill(int16_t *output, int n, int16_t dc, int bit_depth)
{
    int shift1 = 7, shift2 = 20 - bit_depth;
    int32_t add1 = 1 << (shift1 - 1), add2 = 1 << (shift2 - 1);
    int32_t v1 = htx_clip16((64 * (int32_t)dc + add1) >> shift1);
    int16_t v = (int16_t)htx_clip16((64 * v1 + add2) >> shift2);
    int i, nn = n * n;
    for (i = 0; i < nn; i++) output[i] = v;
}

static int htx_col_zero(const int16_t *coeffs, int n, int col)
{
    int k;
    for (k = 0; k < n; k++) {
        if (coeffs[k * n + col]) return 0;
    }
    return 1;
}

void heic_idst4(const int16_t *coeffs, int16_t *output, int bit_depth)
{
    int shift1 = 7, shift2 = 20 - bit_depth;
    int add1 = 1 << (shift1 - 1), add2 = 1 << (shift2 - 1);
    int32_t tmp[16];
    int i, j, k;
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            int32_t sum = 0;
            for (k = 0; k < 4; k++)
                sum += (int32_t)HEIC_DST4[k][j] * coeffs[k * 4 + i];
            tmp[j * 4 + i] = htx_clip16((sum + add1) >> shift1);
        }
    }
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            int32_t sum = 0;
            for (k = 0; k < 4; k++)
                sum += (int32_t)HEIC_DST4[k][j] * tmp[i * 4 + k];
            output[i * 4 + j] = (int16_t)((sum + add2) >> shift2);
        }
    }
}

void heic_idct4(const int16_t *coeffs, int16_t *output, int bit_depth)
{
    int shift1 = 7, shift2 = 20 - bit_depth;
    int add1 = 1 << (shift1 - 1), add2 = 1 << (shift2 - 1);
    int32_t tmp[16];
    int i, j, k;
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            int32_t sum = 0;
            for (k = 0; k < 4; k++)
                sum += (int32_t)HEIC_DCT4[k][j] * coeffs[k * 4 + i];
            tmp[j * 4 + i] = htx_clip16((sum + add1) >> shift1);
        }
    }
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            int32_t sum = 0;
            for (k = 0; k < 4; k++)
                sum += (int32_t)HEIC_DCT4[k][j] * tmp[i * 4 + k];
            output[i * 4 + j] = (int16_t)((sum + add2) >> shift2);
        }
    }
}

static void htx_idct8_1d(const int32_t src[8], int32_t dst[8], int shift)
{
    int32_t add = 1 << (shift - 1);
    int32_t o0 = 89 * src[1] + 75 * src[3] + 50 * src[5] + 18 * src[7];
    int32_t o1 = 75 * src[1] - 18 * src[3] - 89 * src[5] - 50 * src[7];
    int32_t o2 = 50 * src[1] - 89 * src[3] + 18 * src[5] + 75 * src[7];
    int32_t o3 = 18 * src[1] - 50 * src[3] + 75 * src[5] - 89 * src[7];
    int32_t ee0 = 64 * src[0] + 64 * src[4];
    int32_t ee1 = 64 * src[0] - 64 * src[4];
    int32_t eo0 = 83 * src[2] + 36 * src[6];
    int32_t eo1 = 36 * src[2] - 83 * src[6];
    int32_t e0 = ee0 + eo0, e1 = ee1 + eo1, e2 = ee1 - eo1, e3 = ee0 - eo0;
    dst[0] = htx_clip16((e0 + o0 + add) >> shift);
    dst[1] = htx_clip16((e1 + o1 + add) >> shift);
    dst[2] = htx_clip16((e2 + o2 + add) >> shift);
    dst[3] = htx_clip16((e3 + o3 + add) >> shift);
    dst[4] = htx_clip16((e3 - o3 + add) >> shift);
    dst[5] = htx_clip16((e2 - o2 + add) >> shift);
    dst[6] = htx_clip16((e1 - o1 + add) >> shift);
    dst[7] = htx_clip16((e0 - o0 + add) >> shift);
}

void heic_idct8(const int16_t *coeffs, int16_t *output, int bit_depth)
{
    int shift1 = 7, shift2 = 20 - bit_depth;
    int32_t tmp[64];
    int col, row;
    if (heic_simd_idct8(coeffs, output, bit_depth)) return;
    if (htx_only_dc(coeffs, 8)) {
        htx_idct_dc_fill(output, 8, coeffs[0], bit_depth);
        return;
    }
    for (col = 0; col < 8; col++) {
        int32_t src[8], d[8];
        int k;
        if (htx_col_zero(coeffs, 8, col)) {
            for (row = 0; row < 8; row++) tmp[row * 8 + col] = 0;
            continue;
        }
        for (k = 0; k < 8; k++) src[k] = coeffs[k * 8 + col];
        htx_idct8_1d(src, d, shift1);
        for (row = 0; row < 8; row++) tmp[row * 8 + col] = d[row];
    }
    for (row = 0; row < 8; row++) {
        int32_t src[8], d[8];
        int base = row * 8, col2;
        for (col2 = 0; col2 < 8; col2++) src[col2] = tmp[base + col2];
        htx_idct8_1d(src, d, shift2);
        for (col2 = 0; col2 < 8; col2++) output[base + col2] = (int16_t)d[col2];
    }
}

static void htx_idct16_1d(const int32_t src[16], int32_t dst[16], int shift)
{
    int32_t add = 1 << (shift - 1);
    int32_t s1 = src[1], s3 = src[3], s5 = src[5], s7 = src[7];
    int32_t s9 = src[9], s11 = src[11], s13 = src[13], s15 = src[15];
    int32_t o0 = 90 * s1 + 87 * s3 + 80 * s5 + 70 * s7 + 57 * s9 + 43 * s11 + 25 * s13 + 9 * s15;
    int32_t o1 = 87 * s1 + 57 * s3 + 9 * s5 - 43 * s7 - 80 * s9 - 90 * s11 - 70 * s13 - 25 * s15;
    int32_t o2 = 80 * s1 + 9 * s3 - 70 * s5 - 87 * s7 - 25 * s9 + 57 * s11 + 90 * s13 + 43 * s15;
    int32_t o3 = 70 * s1 - 43 * s3 - 87 * s5 + 9 * s7 + 90 * s9 + 25 * s11 - 80 * s13 - 57 * s15;
    int32_t o4 = 57 * s1 - 80 * s3 - 25 * s5 + 90 * s7 - 9 * s9 - 87 * s11 + 43 * s13 + 70 * s15;
    int32_t o5 = 43 * s1 - 90 * s3 + 57 * s5 + 25 * s7 - 87 * s9 + 70 * s11 + 9 * s13 - 80 * s15;
    int32_t o6 = 25 * s1 - 70 * s3 + 90 * s5 - 80 * s7 + 43 * s9 + 9 * s11 - 57 * s13 + 87 * s15;
    int32_t o7 = 9 * s1 - 25 * s3 + 43 * s5 - 57 * s7 + 70 * s9 - 80 * s11 + 87 * s13 - 90 * s15;
    int32_t s0 = src[0], s2 = src[2], s4 = src[4], s6 = src[6];
    int32_t s8 = src[8], s10 = src[10], s12 = src[12], s14 = src[14];
    int32_t eo0 = 89 * s2 + 75 * s6 + 50 * s10 + 18 * s14;
    int32_t eo1 = 75 * s2 - 18 * s6 - 89 * s10 - 50 * s14;
    int32_t eo2 = 50 * s2 - 89 * s6 + 18 * s10 + 75 * s14;
    int32_t eo3 = 18 * s2 - 50 * s6 + 75 * s10 - 89 * s14;
    int32_t eee0 = 64 * s0 + 64 * s8, eee1 = 64 * s0 - 64 * s8;
    int32_t eeo0 = 83 * s4 + 36 * s12, eeo1 = 36 * s4 - 83 * s12;
    int32_t ee0 = eee0 + eeo0, ee1 = eee1 + eeo1, ee2 = eee1 - eeo1, ee3 = eee0 - eeo0;
    int32_t e0 = ee0 + eo0, e1 = ee1 + eo1, e2 = ee2 + eo2, e3 = ee3 + eo3;
    int32_t e4 = ee3 - eo3, e5 = ee2 - eo2, e6 = ee1 - eo1, e7 = ee0 - eo0;
    dst[0] = htx_clip16((e0 + o0 + add) >> shift);
    dst[1] = htx_clip16((e1 + o1 + add) >> shift);
    dst[2] = htx_clip16((e2 + o2 + add) >> shift);
    dst[3] = htx_clip16((e3 + o3 + add) >> shift);
    dst[4] = htx_clip16((e4 + o4 + add) >> shift);
    dst[5] = htx_clip16((e5 + o5 + add) >> shift);
    dst[6] = htx_clip16((e6 + o6 + add) >> shift);
    dst[7] = htx_clip16((e7 + o7 + add) >> shift);
    dst[8] = htx_clip16((e7 - o7 + add) >> shift);
    dst[9] = htx_clip16((e6 - o6 + add) >> shift);
    dst[10] = htx_clip16((e5 - o5 + add) >> shift);
    dst[11] = htx_clip16((e4 - o4 + add) >> shift);
    dst[12] = htx_clip16((e3 - o3 + add) >> shift);
    dst[13] = htx_clip16((e2 - o2 + add) >> shift);
    dst[14] = htx_clip16((e1 - o1 + add) >> shift);
    dst[15] = htx_clip16((e0 - o0 + add) >> shift);
}

void heic_idct16(const int16_t *coeffs, int16_t *output, int bit_depth)
{
    int shift1 = 7, shift2 = 20 - bit_depth;
    int32_t *tmp = heic_idct_scratch_buf();
    int col, row, k, last_col;
    if (heic_simd_idct16(coeffs, output, bit_depth)) return;
    if (htx_only_dc(coeffs, 16)) {
        htx_idct_dc_fill(output, 16, coeffs[0], bit_depth);
        return;
    }
    last_col = 15;
    while (last_col > 0 && htx_col_zero(coeffs, 16, last_col)) last_col--;
    memset(tmp, 0, 256 * sizeof(int32_t));
    for (col = 0; col <= last_col; col++) {
        int32_t src[16], d[16];
        if (htx_col_zero(coeffs, 16, col)) continue;
        for (k = 0; k < 16; k++) src[k] = coeffs[k * 16 + col];
        htx_idct16_1d(src, d, shift1);
        for (row = 0; row < 16; row++) tmp[row * 16 + col] = d[row];
    }
    for (row = 0; row < 16; row++) {
        int32_t src[16], d[16];
        int base = row * 16;
        for (k = 0; k < 16; k++) src[k] = tmp[base + k];
        htx_idct16_1d(src, d, shift2);
        for (k = 0; k < 16; k++) output[base + k] = (int16_t)d[k];
    }
}

static void htx_idct32_1d(const int32_t src[32], int32_t dst[32], int shift)
{
    int32_t add = 1 << (shift - 1);
    int32_t s1 = src[1], s3 = src[3], s5 = src[5], s7 = src[7];
    int32_t s9 = src[9], s11 = src[11], s13 = src[13], s15 = src[15];
    int32_t s17 = src[17], s19 = src[19], s21 = src[21], s23 = src[23];
    int32_t s25 = src[25], s27 = src[27], s29 = src[29], s31 = src[31];
    int32_t o0 = 90*s1+90*s3+88*s5+85*s7+82*s9+78*s11+73*s13+67*s15
               +61*s17+54*s19+46*s21+38*s23+31*s25+22*s27+13*s29+4*s31;
    int32_t o1 = 90*s1+82*s3+67*s5+46*s7+22*s9-4*s11-31*s13-54*s15
               -73*s17-85*s19-90*s21-88*s23-78*s25-61*s27-38*s29-13*s31;
    int32_t o2 = 88*s1+67*s3+31*s5-13*s7-54*s9-82*s11-90*s13-78*s15
               -46*s17-4*s19+38*s21+73*s23+90*s25+85*s27+61*s29+22*s31;
    int32_t o3 = 85*s1+46*s3-13*s5-67*s7-90*s9-73*s11-22*s13+38*s15
               +82*s17+88*s19+54*s21-4*s23-61*s25-90*s27-78*s29-31*s31;
    int32_t o4 = 82*s1+22*s3-54*s5-90*s7-61*s9+13*s11+78*s13+85*s15+31*s17
               -46*s19-90*s21-67*s23+4*s25+73*s27+88*s29+38*s31;
    int32_t o5 = 78*s1-4*s3-82*s5-73*s7+13*s9+85*s11+67*s13-22*s15
               -88*s17-61*s19+31*s21+90*s23+54*s25-38*s27-90*s29-46*s31;
    int32_t o6 = 73*s1-31*s3-90*s5-22*s7+78*s9+67*s11-38*s13-90*s15-13*s17
               +82*s19+61*s21-46*s23-88*s25-4*s27+85*s29+54*s31;
    int32_t o7 = 67*s1-54*s3-78*s5+38*s7+85*s9-22*s11-90*s13+4*s15
               +90*s17+13*s19-88*s21-31*s23+82*s25+46*s27-73*s29-61*s31;
    int32_t o8 = 61*s1-73*s3-46*s5+82*s7+31*s9-88*s11-13*s13+90*s15
               -4*s17-90*s19+22*s21+85*s23-38*s25-78*s27+54*s29+67*s31;
    int32_t o9 = 54*s1-85*s3-4*s5+88*s7-46*s9-61*s11+82*s13+13*s15
               -90*s17+38*s19+67*s21-78*s23-22*s25+90*s27-31*s29-73*s31;
    int32_t o10 = 46*s1-90*s3+38*s5+54*s7-90*s9+31*s11+61*s13-88*s15
                +22*s17+67*s19-85*s21+13*s23+73*s25-82*s27+4*s29+78*s31;
    int32_t o11 = 38*s1-88*s3+73*s5-4*s7-67*s9+90*s11-46*s13-31*s15
                +85*s17-78*s19+13*s21+61*s23-90*s25+54*s27+22*s29-82*s31;
    int32_t o12 = 31*s1-78*s3+90*s5-61*s7+4*s9+54*s11-88*s13+82*s15
                -38*s17-22*s19+73*s21-90*s23+67*s25-13*s27-46*s29+85*s31;
    int32_t o13 = 22*s1-61*s3+85*s5-90*s7+73*s9-38*s11-4*s13+46*s15
                -78*s17+90*s19-82*s21+54*s23-13*s25-31*s27+67*s29-88*s31;
    int32_t o14 = 13*s1-38*s3+61*s5-78*s7+88*s9-90*s11+85*s13-73*s15
                +54*s17-31*s19+4*s21+22*s23-46*s25+67*s27-82*s29+90*s31;
    int32_t o15 = 4*s1-13*s3+22*s5-31*s7+38*s9-46*s11+54*s13-61*s15
                +67*s17-73*s19+78*s21-82*s23+85*s25-88*s27+90*s29-90*s31;
    {
        int32_t s0 = src[0], s2 = src[2], s4 = src[4], s6 = src[6];
        int32_t s8 = src[8], s10 = src[10], s12 = src[12], s14 = src[14];
        int32_t s16 = src[16], s18 = src[18], s20 = src[20], s22 = src[22];
        int32_t s24 = src[24], s26 = src[26], s28 = src[28], s30 = src[30];
        int32_t eo0 = 90*s2+87*s6+80*s10+70*s14+57*s18+43*s22+25*s26+9*s30;
        int32_t eo1 = 87*s2+57*s6+9*s10-43*s14-80*s18-90*s22-70*s26-25*s30;
        int32_t eo2 = 80*s2+9*s6-70*s10-87*s14-25*s18+57*s22+90*s26+43*s30;
        int32_t eo3 = 70*s2-43*s6-87*s10+9*s14+90*s18+25*s22-80*s26-57*s30;
        int32_t eo4 = 57*s2-80*s6-25*s10+90*s14-9*s18-87*s22+43*s26+70*s30;
        int32_t eo5 = 43*s2-90*s6+57*s10+25*s14-87*s18+70*s22+9*s26-80*s30;
        int32_t eo6 = 25*s2-70*s6+90*s10-80*s14+43*s18+9*s22-57*s26+87*s30;
        int32_t eo7 = 9*s2-25*s6+43*s10-57*s14+70*s18-80*s22+87*s26-90*s30;
        int32_t eeo0 = 89*s4+75*s12+50*s20+18*s28;
        int32_t eeo1 = 75*s4-18*s12-89*s20-50*s28;
        int32_t eeo2 = 50*s4-89*s12+18*s20+75*s28;
        int32_t eeo3 = 18*s4-50*s12+75*s20-89*s28;
        int32_t eeee0 = 64*s0+64*s16, eeee1 = 64*s0-64*s16;
        int32_t eeeo0 = 83*s8+36*s24, eeeo1 = 36*s8-83*s24;
        int32_t eee0 = eeee0+eeeo0, eee1 = eeee1+eeeo1;
        int32_t eee2 = eeee1-eeeo1, eee3 = eeee0-eeeo0;
        int32_t ee0 = eee0+eeo0, ee1 = eee1+eeo1, ee2 = eee2+eeo2, ee3 = eee3+eeo3;
        int32_t ee4 = eee3-eeo3, ee5 = eee2-eeo2, ee6 = eee1-eeo1, ee7 = eee0-eeo0;
        int32_t e0 = ee0+eo0, e1 = ee1+eo1, e2 = ee2+eo2, e3 = ee3+eo3;
        int32_t e4 = ee4+eo4, e5 = ee5+eo5, e6 = ee6+eo6, e7 = ee7+eo7;
        int32_t e8 = ee7-eo7, e9 = ee6-eo6, e10 = ee5-eo5, e11 = ee4-eo4;
        int32_t e12 = ee3-eo3, e13 = ee2-eo2, e14 = ee1-eo1, e15 = ee0-eo0;
        dst[0] = htx_clip16((e0 + o0 + add) >> shift);
        dst[1] = htx_clip16((e1 + o1 + add) >> shift);
        dst[2] = htx_clip16((e2 + o2 + add) >> shift);
        dst[3] = htx_clip16((e3 + o3 + add) >> shift);
        dst[4] = htx_clip16((e4 + o4 + add) >> shift);
        dst[5] = htx_clip16((e5 + o5 + add) >> shift);
        dst[6] = htx_clip16((e6 + o6 + add) >> shift);
        dst[7] = htx_clip16((e7 + o7 + add) >> shift);
        dst[8] = htx_clip16((e8 + o8 + add) >> shift);
        dst[9] = htx_clip16((e9 + o9 + add) >> shift);
        dst[10] = htx_clip16((e10 + o10 + add) >> shift);
        dst[11] = htx_clip16((e11 + o11 + add) >> shift);
        dst[12] = htx_clip16((e12 + o12 + add) >> shift);
        dst[13] = htx_clip16((e13 + o13 + add) >> shift);
        dst[14] = htx_clip16((e14 + o14 + add) >> shift);
        dst[15] = htx_clip16((e15 + o15 + add) >> shift);
        dst[16] = htx_clip16((e15 - o15 + add) >> shift);
        dst[17] = htx_clip16((e14 - o14 + add) >> shift);
        dst[18] = htx_clip16((e13 - o13 + add) >> shift);
        dst[19] = htx_clip16((e12 - o12 + add) >> shift);
        dst[20] = htx_clip16((e11 - o11 + add) >> shift);
        dst[21] = htx_clip16((e10 - o10 + add) >> shift);
        dst[22] = htx_clip16((e9 - o9 + add) >> shift);
        dst[23] = htx_clip16((e8 - o8 + add) >> shift);
        dst[24] = htx_clip16((e7 - o7 + add) >> shift);
        dst[25] = htx_clip16((e6 - o6 + add) >> shift);
        dst[26] = htx_clip16((e5 - o5 + add) >> shift);
        dst[27] = htx_clip16((e4 - o4 + add) >> shift);
        dst[28] = htx_clip16((e3 - o3 + add) >> shift);
        dst[29] = htx_clip16((e2 - o2 + add) >> shift);
        dst[30] = htx_clip16((e1 - o1 + add) >> shift);
        dst[31] = htx_clip16((e0 - o0 + add) >> shift);
    }
}

void heic_idct32(const int16_t *coeffs, int16_t *output, int bit_depth)
{
    int shift1 = 7, shift2 = 20 - bit_depth;
    int32_t *tmp = heic_idct_scratch_buf();
    int col, row, k, last_col;
    if (heic_simd_idct32(coeffs, output, bit_depth)) return;
    if (htx_only_dc(coeffs, 32)) {
        htx_idct_dc_fill(output, 32, coeffs[0], bit_depth);
        return;
    }

    last_col = 31;
    while (last_col > 0 && htx_col_zero(coeffs, 32, last_col)) last_col--;
    memset(tmp, 0, 1024 * sizeof(int32_t));
    for (col = 0; col <= last_col; col++) {
        int32_t src[32], d[32];
        if (htx_col_zero(coeffs, 32, col)) continue;
        for (k = 0; k < 32; k++) src[k] = coeffs[k * 32 + col];
        htx_idct32_1d(src, d, shift1);
        for (row = 0; row < 32; row++) tmp[row * 32 + col] = d[row];
    }
    for (row = 0; row < 32; row++) {
        int32_t src[32], d[32];
        int base = row * 32;
        int allz = 1;
        for (k = 0; k <= last_col; k++) {
            if (tmp[base + k]) {
                allz = 0;
                break;
            }
        }
        if (allz) {
            memset(output + base, 0, 32 * sizeof(int16_t));
            continue;
        }
        for (k = 0; k < 32; k++) src[k] = tmp[base + k];
        htx_idct32_1d(src, d, shift2);
        for (k = 0; k < 32; k++) output[base + k] = (int16_t)d[k];
    }
}

void heic_dequantize(int16_t *coeffs, int n, int qp, int bit_depth,
                     uint8_t log2_tr_size)
{
    static const int32_t LEVEL_SCALE[6] = {40, 45, 51, 57, 64, 72};
    int32_t qp_clamped = qp > 180 ? 180 : qp;
    int32_t qp_per = qp_clamped / 6;
    int32_t qp_rem = qp_clamped % 6;
    int64_t combined = ((int64_t)LEVEL_SCALE[qp_rem]) << qp_per;
    int32_t shift = (int32_t)bit_depth - 9 + (int32_t)log2_tr_size;
    int i;
    if (shift >= 0) {
        int64_t add = shift > 0 ? (1LL << (shift - 1)) : 0;

        if (combined <= 65536 && shift < 31) {
            int32_t c32 = (int32_t)combined;
            int32_t add32 = (int32_t)add;
            if (heic_simd_dequant(coeffs, n, c32, shift))
                return;
            for (i = 0; i < n; i++) {
                int32_t v = ((int32_t)coeffs[i] * c32 + add32) >> shift;
                if (v < -32768) v = -32768;
                if (v > 32767) v = 32767;
                coeffs[i] = (int16_t)v;
            }
        } else {
            for (i = 0; i < n; i++) {
                int64_t v = ((int64_t)coeffs[i] * combined + add) >> shift;
                if (v < -32768) v = -32768;
                if (v > 32767) v = 32767;
                coeffs[i] = (int16_t)v;
            }
        }
    } else {
        int neg = -shift;
        for (i = 0; i < n; i++) {
            int64_t v = ((int64_t)coeffs[i] * combined) << neg;
            if (v < -32768) v = -32768;
            if (v > 32767) v = 32767;
            coeffs[i] = (int16_t)v;
        }
    }
}

void heic_inverse_transform(const int16_t *coeffs, int16_t *output, int size,
                            int bit_depth, int is_intra_4x4_luma)
{
    switch (size) {
    case 4:
        if (is_intra_4x4_luma) heic_idst4(coeffs, output, bit_depth);
        else heic_idct4(coeffs, output, bit_depth);
        break;
    case 8:
        heic_idct8(coeffs, output, bit_depth);
        break;
    case 16:
        heic_idct16(coeffs, output, bit_depth);
        break;
    case 32:
        heic_idct32(coeffs, output, bit_depth);
        break;
    default:
        break;
    }
}

void heic_add_residual(uint16_t *plane, int stride, int x0, int y0,
                       const int16_t *residual, int size, int max_val)
{
    int py, px;
    if (heic_simd_add_residual(plane, stride, x0, y0, residual, size, max_val))
        return;
    for (py = 0; py < size; py++) {
        uint16_t *dst = plane + (y0 + py) * stride + x0;
        const int16_t *src = residual + py * size;
        for (px = 0; px < size; px++) {
            int32_t v = (int32_t)dst[px] + (int32_t)src[px];
            if (v < 0) v = 0;
            else if (v > max_val) v = max_val;
            dst[px] = (uint16_t)v;
        }
    }
}

#if defined(_M_X64) || defined(__x86_64__) || defined(_M_IX86) || defined(__i386__)
#define HEIC_X86 1
#include <emmintrin.h>
#include <tmmintrin.h>
#include <smmintrin.h>
#if defined(_MSC_VER)
#include <intrin.h>
#endif
#else
#define HEIC_X86 0
#endif

static int g_simd;

void heic_simd_init(void)
{
    g_simd = 0;
#if HEIC_X86
    {
#if defined(_MSC_VER)
        int info[4] = {0, 0, 0, 0};
        __cpuid(info, 1);
        if (info[2] & (1 << 19)) g_simd = 1;
#elif defined(__GNUC__) || defined(__clang__)
        if (__builtin_cpu_supports("sse4.1")) g_simd = 1;
#else
        g_simd = 1;
#endif
    }
#endif
}

int heic_simd_enabled(void) { return g_simd; }

#if HEIC_X86

static inline __m128i hs_mul(__m128i a, int32_t c)
{
    return _mm_mullo_epi32(a, _mm_set1_epi32(c));
}
static inline __m128i hs_add(__m128i a, __m128i b) { return _mm_add_epi32(a, b); }
static inline __m128i hs_sub(__m128i a, __m128i b) { return _mm_sub_epi32(a, b); }

static inline __m128i hs_clip16_shift(__m128i v, __m128i add, int shift)
{
    __m128i r = _mm_srai_epi32(_mm_add_epi32(v, add), shift);
    r = _mm_min_epi32(r, _mm_set1_epi32(32767));
    r = _mm_max_epi32(r, _mm_set1_epi32(-32768));
    return r;
}

static void idct8_1d_x4(const __m128i s[8], __m128i d[8], int shift)
{
    __m128i add = _mm_set1_epi32(1 << (shift - 1));
    __m128i o0 = hs_add(hs_add(hs_mul(s[1], 89), hs_mul(s[3], 75)),
                        hs_add(hs_mul(s[5], 50), hs_mul(s[7], 18)));
    __m128i o1 = hs_add(hs_add(hs_mul(s[1], 75), hs_mul(s[3], -18)),
                        hs_add(hs_mul(s[5], -89), hs_mul(s[7], -50)));
    __m128i o2 = hs_add(hs_add(hs_mul(s[1], 50), hs_mul(s[3], -89)),
                        hs_add(hs_mul(s[5], 18), hs_mul(s[7], 75)));
    __m128i o3 = hs_add(hs_add(hs_mul(s[1], 18), hs_mul(s[3], -50)),
                        hs_add(hs_mul(s[5], 75), hs_mul(s[7], -89)));
    __m128i ee0 = hs_add(hs_mul(s[0], 64), hs_mul(s[4], 64));
    __m128i ee1 = hs_sub(hs_mul(s[0], 64), hs_mul(s[4], 64));
    __m128i eo0 = hs_add(hs_mul(s[2], 83), hs_mul(s[6], 36));
    __m128i eo1 = hs_sub(hs_mul(s[2], 36), hs_mul(s[6], 83));
    __m128i e0 = hs_add(ee0, eo0), e1 = hs_add(ee1, eo1);
    __m128i e2 = hs_sub(ee1, eo1), e3 = hs_sub(ee0, eo0);
    d[0] = hs_clip16_shift(hs_add(e0, o0), add, shift);
    d[1] = hs_clip16_shift(hs_add(e1, o1), add, shift);
    d[2] = hs_clip16_shift(hs_add(e2, o2), add, shift);
    d[3] = hs_clip16_shift(hs_add(e3, o3), add, shift);
    d[4] = hs_clip16_shift(hs_sub(e3, o3), add, shift);
    d[5] = hs_clip16_shift(hs_sub(e2, o2), add, shift);
    d[6] = hs_clip16_shift(hs_sub(e1, o1), add, shift);
    d[7] = hs_clip16_shift(hs_sub(e0, o0), add, shift);
}

static void load4_cols_i16(const int16_t *c, int n, int col, __m128i *s, int nfreq)
{
    int k;
    for (k = 0; k < nfreq; k++) {
        const int16_t *p = c + k * n + col;
        s[k] = _mm_setr_epi32(p[0], p[1], p[2], p[3]);
    }
}

static void store4_cols_i32(int32_t *tmp, int n, int col, const __m128i *d, int nfreq)
{
    int k;
    for (k = 0; k < nfreq; k++) {
        int32_t *p = tmp + k * n + col;
        _mm_storeu_si128((__m128i *)p, d[k]);
    }
}

static void load4_rows_i32(const int32_t *tmp, int n, int row, __m128i *s, int nfreq)
{
    int k;
    for (k = 0; k < nfreq; k++) {
        s[k] = _mm_setr_epi32(tmp[row * n + k], tmp[(row + 1) * n + k],
                              tmp[(row + 2) * n + k], tmp[(row + 3) * n + k]);
    }
}

static void store4_rows_i16(int16_t *out, int n, int row, const __m128i *d, int nfreq)
{
    int k, r;
    int32_t lane[4];
    for (k = 0; k < nfreq; k++) {
        _mm_storeu_si128((__m128i *)lane, d[k]);
        for (r = 0; r < 4; r++)
            out[(row + r) * n + k] = (int16_t)lane[r];
    }
}

static void idct16_1d_x4(const __m128i s[16], __m128i d[16], int shift)
{
    __m128i add = _mm_set1_epi32(1 << (shift - 1));
    __m128i s1 = s[1], s3 = s[3], s5 = s[5], s7 = s[7];
    __m128i s9 = s[9], s11 = s[11], s13 = s[13], s15 = s[15];
    __m128i o0, o1, o2, o3, o4, o5, o6, o7;
    __m128i eo0, eo1, eo2, eo3, eee0, eee1, eeo0, eeo1;
    __m128i ee0, ee1, ee2, ee3, e0, e1, e2, e3, e4, e5, e6, e7;

#define M4(a, c0, b, c1, c, c2, d, c3)                                         \
    hs_add(hs_add(hs_mul((a), (c0)), hs_mul((b), (c1))),                     \
           hs_add(hs_mul((c), (c2)), hs_mul((d), (c3))))

    o0 = hs_add(M4(s1, 90, s3, 87, s5, 80, s7, 70), M4(s9, 57, s11, 43, s13, 25, s15, 9));
    o1 = hs_add(M4(s1, 87, s3, 57, s5, 9, s7, -43), M4(s9, -80, s11, -90, s13, -70, s15, -25));
    o2 = hs_add(M4(s1, 80, s3, 9, s5, -70, s7, -87), M4(s9, -25, s11, 57, s13, 90, s15, 43));
    o3 = hs_add(M4(s1, 70, s3, -43, s5, -87, s7, 9), M4(s9, 90, s11, 25, s13, -80, s15, -57));
    o4 = hs_add(M4(s1, 57, s3, -80, s5, -25, s7, 90), M4(s9, -9, s11, -87, s13, 43, s15, 70));
    o5 = hs_add(M4(s1, 43, s3, -90, s5, 57, s7, 25), M4(s9, -87, s11, 70, s13, 9, s15, -80));
    o6 = hs_add(M4(s1, 25, s3, -70, s5, 90, s7, -80), M4(s9, 43, s11, 9, s13, -57, s15, 87));
    o7 = hs_add(M4(s1, 9, s3, -25, s5, 43, s7, -57), M4(s9, 70, s11, -80, s13, 87, s15, -90));

    eo0 = M4(s[2], 89, s[6], 75, s[10], 50, s[14], 18);
    eo1 = M4(s[2], 75, s[6], -18, s[10], -89, s[14], -50);
    eo2 = M4(s[2], 50, s[6], -89, s[10], 18, s[14], 75);
    eo3 = M4(s[2], 18, s[6], -50, s[10], 75, s[14], -89);
    eee0 = hs_add(hs_mul(s[0], 64), hs_mul(s[8], 64));
    eee1 = hs_sub(hs_mul(s[0], 64), hs_mul(s[8], 64));
    eeo0 = hs_add(hs_mul(s[4], 83), hs_mul(s[12], 36));
    eeo1 = hs_sub(hs_mul(s[4], 36), hs_mul(s[12], 83));
    ee0 = hs_add(eee0, eeo0);
    ee1 = hs_add(eee1, eeo1);
    ee2 = hs_sub(eee1, eeo1);
    ee3 = hs_sub(eee0, eeo0);
    e0 = hs_add(ee0, eo0);
    e1 = hs_add(ee1, eo1);
    e2 = hs_add(ee2, eo2);
    e3 = hs_add(ee3, eo3);
    e4 = hs_sub(ee3, eo3);
    e5 = hs_sub(ee2, eo2);
    e6 = hs_sub(ee1, eo1);
    e7 = hs_sub(ee0, eo0);
    d[0] = hs_clip16_shift(hs_add(e0, o0), add, shift);
    d[1] = hs_clip16_shift(hs_add(e1, o1), add, shift);
    d[2] = hs_clip16_shift(hs_add(e2, o2), add, shift);
    d[3] = hs_clip16_shift(hs_add(e3, o3), add, shift);
    d[4] = hs_clip16_shift(hs_add(e4, o4), add, shift);
    d[5] = hs_clip16_shift(hs_add(e5, o5), add, shift);
    d[6] = hs_clip16_shift(hs_add(e6, o6), add, shift);
    d[7] = hs_clip16_shift(hs_add(e7, o7), add, shift);
    d[8] = hs_clip16_shift(hs_sub(e7, o7), add, shift);
    d[9] = hs_clip16_shift(hs_sub(e6, o6), add, shift);
    d[10] = hs_clip16_shift(hs_sub(e5, o5), add, shift);
    d[11] = hs_clip16_shift(hs_sub(e4, o4), add, shift);
    d[12] = hs_clip16_shift(hs_sub(e3, o3), add, shift);
    d[13] = hs_clip16_shift(hs_sub(e2, o2), add, shift);
    d[14] = hs_clip16_shift(hs_sub(e1, o1), add, shift);
    d[15] = hs_clip16_shift(hs_sub(e0, o0), add, shift);
#undef M4
}

static void idct32_1d_x4(const __m128i s[32], __m128i d[32], int shift)
{
    __m128i add = _mm_set1_epi32(1 << (shift - 1));
    __m128i s1 = s[1], s3 = s[3], s5 = s[5], s7 = s[7];
    __m128i s9 = s[9], s11 = s[11], s13 = s[13], s15 = s[15];
    __m128i s17 = s[17], s19 = s[19], s21 = s[21], s23 = s[23];
    __m128i s25 = s[25], s27 = s[27], s29 = s[29], s31 = s[31];
    __m128i o0, o1, o2, o3, o4, o5, o6, o7, o8, o9, o10, o11, o12, o13, o14, o15;
    __m128i eo0, eo1, eo2, eo3, eo4, eo5, eo6, eo7;
    __m128i eeo0, eeo1, eeo2, eeo3, eeee0, eeee1, eeeo0, eeeo1;
    __m128i eee0, eee1, eee2, eee3, ee0, ee1, ee2, ee3, ee4, ee5, ee6, ee7;
    __m128i e0, e1, e2, e3, e4, e5, e6, e7, e8, e9, e10, e11, e12, e13, e14, e15;

#define A2(a, ca, b, cb) hs_add(hs_mul((a), (ca)), hs_mul((b), (cb)))
#define A4(a, ca, b, cb, c, cc, d, cd)                                         \
    hs_add(A2((a), (ca), (b), (cb)), A2((c), (cc), (d), (cd)))
#define A8(a, ca, b, cb, c, cc, d, cd, e, ce, f, cf, g, cg, h, ch)             \
    hs_add(A4((a), (ca), (b), (cb), (c), (cc), (d), (cd)),                     \
           A4((e), (ce), (f), (cf), (g), (cg), (h), (ch)))

    o0 = A8(s1, 90, s3, 90, s5, 88, s7, 85, s9, 82, s11, 78, s13, 73, s15, 67);
    o0 = hs_add(o0, A8(s17, 61, s19, 54, s21, 46, s23, 38, s25, 31, s27, 22, s29, 13, s31, 4));
    o1 = A8(s1, 90, s3, 82, s5, 67, s7, 46, s9, 22, s11, -4, s13, -31, s15, -54);
    o1 = hs_add(o1, A8(s17, -73, s19, -85, s21, -90, s23, -88, s25, -78, s27, -61, s29, -38, s31, -13));
    o2 = A8(s1, 88, s3, 67, s5, 31, s7, -13, s9, -54, s11, -82, s13, -90, s15, -78);
    o2 = hs_add(o2, A8(s17, -46, s19, -4, s21, 38, s23, 73, s25, 90, s27, 85, s29, 61, s31, 22));
    o3 = A8(s1, 85, s3, 46, s5, -13, s7, -67, s9, -90, s11, -73, s13, -22, s15, 38);
    o3 = hs_add(o3, A8(s17, 82, s19, 88, s21, 54, s23, -4, s25, -61, s27, -90, s29, -78, s31, -31));
    o4 = A8(s1, 82, s3, 22, s5, -54, s7, -90, s9, -61, s11, 13, s13, 78, s15, 85);
    o4 = hs_add(o4, A8(s17, 31, s19, -46, s21, -90, s23, -67, s25, 4, s27, 73, s29, 88, s31, 38));
    o5 = A8(s1, 78, s3, -4, s5, -82, s7, -73, s9, 13, s11, 85, s13, 67, s15, -22);
    o5 = hs_add(o5, A8(s17, -88, s19, -61, s21, 31, s23, 90, s25, 54, s27, -38, s29, -90, s31, -46));
    o6 = A8(s1, 73, s3, -31, s5, -90, s7, -22, s9, 78, s11, 67, s13, -38, s15, -90);
    o6 = hs_add(o6, A8(s17, -13, s19, 82, s21, 61, s23, -46, s25, -88, s27, -4, s29, 85, s31, 54));
    o7 = A8(s1, 67, s3, -54, s5, -78, s7, 38, s9, 85, s11, -22, s13, -90, s15, 4);
    o7 = hs_add(o7, A8(s17, 90, s19, 13, s21, -88, s23, -31, s25, 82, s27, 46, s29, -73, s31, -61));
    o8 = A8(s1, 61, s3, -73, s5, -46, s7, 82, s9, 31, s11, -88, s13, -13, s15, 90);
    o8 = hs_add(o8, A8(s17, -4, s19, -90, s21, 22, s23, 85, s25, -38, s27, -78, s29, 54, s31, 67));
    o9 = A8(s1, 54, s3, -85, s5, -4, s7, 88, s9, -46, s11, -61, s13, 82, s15, 13);
    o9 = hs_add(o9, A8(s17, -90, s19, 38, s21, 67, s23, -78, s25, -22, s27, 90, s29, -31, s31, -73));
    o10 = A8(s1, 46, s3, -90, s5, 38, s7, 54, s9, -90, s11, 31, s13, 61, s15, -88);
    o10 = hs_add(o10, A8(s17, 22, s19, 67, s21, -85, s23, 13, s25, 73, s27, -82, s29, 4, s31, 78));
    o11 = A8(s1, 38, s3, -88, s5, 73, s7, -4, s9, -67, s11, 90, s13, -46, s15, -31);
    o11 = hs_add(o11, A8(s17, 85, s19, -78, s21, 13, s23, 61, s25, -90, s27, 54, s29, 22, s31, -82));
    o12 = A8(s1, 31, s3, -78, s5, 90, s7, -61, s9, 4, s11, 54, s13, -88, s15, 82);
    o12 = hs_add(o12, A8(s17, -38, s19, -22, s21, 73, s23, -90, s25, 67, s27, -13, s29, -46, s31, 85));
    o13 = A8(s1, 22, s3, -61, s5, 85, s7, -90, s9, 73, s11, -38, s13, -4, s15, 46);
    o13 = hs_add(o13, A8(s17, -78, s19, 90, s21, -82, s23, 54, s25, -13, s27, -31, s29, 67, s31, -88));
    o14 = A8(s1, 13, s3, -38, s5, 61, s7, -78, s9, 88, s11, -90, s13, 85, s15, -73);
    o14 = hs_add(o14, A8(s17, 54, s19, -31, s21, 4, s23, 22, s25, -46, s27, 67, s29, -82, s31, 90));
    o15 = A8(s1, 4, s3, -13, s5, 22, s7, -31, s9, 38, s11, -46, s13, 54, s15, -61);
    o15 = hs_add(o15, A8(s17, 67, s19, -73, s21, 78, s23, -82, s25, 85, s27, -88, s29, 90, s31, -90));

    {
        __m128i s0 = s[0], s2 = s[2], s4 = s[4], s6 = s[6];
        __m128i s8 = s[8], s10 = s[10], s12 = s[12], s14 = s[14];
        __m128i s16 = s[16], s18 = s[18], s20 = s[20], s22 = s[22];
        __m128i s24 = s[24], s26 = s[26], s28 = s[28], s30 = s[30];
        eo0 = A8(s2, 90, s6, 87, s10, 80, s14, 70, s18, 57, s22, 43, s26, 25, s30, 9);
        eo1 = A8(s2, 87, s6, 57, s10, 9, s14, -43, s18, -80, s22, -90, s26, -70, s30, -25);
        eo2 = A8(s2, 80, s6, 9, s10, -70, s14, -87, s18, -25, s22, 57, s26, 90, s30, 43);
        eo3 = A8(s2, 70, s6, -43, s10, -87, s14, 9, s18, 90, s22, 25, s26, -80, s30, -57);
        eo4 = A8(s2, 57, s6, -80, s10, -25, s14, 90, s18, -9, s22, -87, s26, 43, s30, 70);
        eo5 = A8(s2, 43, s6, -90, s10, 57, s14, 25, s18, -87, s22, 70, s26, 9, s30, -80);
        eo6 = A8(s2, 25, s6, -70, s10, 90, s14, -80, s18, 43, s22, 9, s26, -57, s30, 87);
        eo7 = A8(s2, 9, s6, -25, s10, 43, s14, -57, s18, 70, s22, -80, s26, 87, s30, -90);
        eeo0 = A4(s4, 89, s12, 75, s20, 50, s28, 18);
        eeo1 = A4(s4, 75, s12, -18, s20, -89, s28, -50);
        eeo2 = A4(s4, 50, s12, -89, s20, 18, s28, 75);
        eeo3 = A4(s4, 18, s12, -50, s20, 75, s28, -89);
        eeee0 = A2(s0, 64, s16, 64);
        eeee1 = A2(s0, 64, s16, -64);
        eeeo0 = A2(s8, 83, s24, 36);
        eeeo1 = A2(s8, 36, s24, -83);
        eee0 = hs_add(eeee0, eeeo0);
        eee1 = hs_add(eeee1, eeeo1);
        eee2 = hs_sub(eeee1, eeeo1);
        eee3 = hs_sub(eeee0, eeeo0);
        ee0 = hs_add(eee0, eeo0);
        ee1 = hs_add(eee1, eeo1);
        ee2 = hs_add(eee2, eeo2);
        ee3 = hs_add(eee3, eeo3);
        ee4 = hs_sub(eee3, eeo3);
        ee5 = hs_sub(eee2, eeo2);
        ee6 = hs_sub(eee1, eeo1);
        ee7 = hs_sub(eee0, eeo0);
        e0 = hs_add(ee0, eo0);
        e1 = hs_add(ee1, eo1);
        e2 = hs_add(ee2, eo2);
        e3 = hs_add(ee3, eo3);
        e4 = hs_add(ee4, eo4);
        e5 = hs_add(ee5, eo5);
        e6 = hs_add(ee6, eo6);
        e7 = hs_add(ee7, eo7);
        e8 = hs_sub(ee7, eo7);
        e9 = hs_sub(ee6, eo6);
        e10 = hs_sub(ee5, eo5);
        e11 = hs_sub(ee4, eo4);
        e12 = hs_sub(ee3, eo3);
        e13 = hs_sub(ee2, eo2);
        e14 = hs_sub(ee1, eo1);
        e15 = hs_sub(ee0, eo0);
        d[0] = hs_clip16_shift(hs_add(e0, o0), add, shift);
        d[1] = hs_clip16_shift(hs_add(e1, o1), add, shift);
        d[2] = hs_clip16_shift(hs_add(e2, o2), add, shift);
        d[3] = hs_clip16_shift(hs_add(e3, o3), add, shift);
        d[4] = hs_clip16_shift(hs_add(e4, o4), add, shift);
        d[5] = hs_clip16_shift(hs_add(e5, o5), add, shift);
        d[6] = hs_clip16_shift(hs_add(e6, o6), add, shift);
        d[7] = hs_clip16_shift(hs_add(e7, o7), add, shift);
        d[8] = hs_clip16_shift(hs_add(e8, o8), add, shift);
        d[9] = hs_clip16_shift(hs_add(e9, o9), add, shift);
        d[10] = hs_clip16_shift(hs_add(e10, o10), add, shift);
        d[11] = hs_clip16_shift(hs_add(e11, o11), add, shift);
        d[12] = hs_clip16_shift(hs_add(e12, o12), add, shift);
        d[13] = hs_clip16_shift(hs_add(e13, o13), add, shift);
        d[14] = hs_clip16_shift(hs_add(e14, o14), add, shift);
        d[15] = hs_clip16_shift(hs_add(e15, o15), add, shift);
        d[16] = hs_clip16_shift(hs_sub(e15, o15), add, shift);
        d[17] = hs_clip16_shift(hs_sub(e14, o14), add, shift);
        d[18] = hs_clip16_shift(hs_sub(e13, o13), add, shift);
        d[19] = hs_clip16_shift(hs_sub(e12, o12), add, shift);
        d[20] = hs_clip16_shift(hs_sub(e11, o11), add, shift);
        d[21] = hs_clip16_shift(hs_sub(e10, o10), add, shift);
        d[22] = hs_clip16_shift(hs_sub(e9, o9), add, shift);
        d[23] = hs_clip16_shift(hs_sub(e8, o8), add, shift);
        d[24] = hs_clip16_shift(hs_sub(e7, o7), add, shift);
        d[25] = hs_clip16_shift(hs_sub(e6, o6), add, shift);
        d[26] = hs_clip16_shift(hs_sub(e5, o5), add, shift);
        d[27] = hs_clip16_shift(hs_sub(e4, o4), add, shift);
        d[28] = hs_clip16_shift(hs_sub(e3, o3), add, shift);
        d[29] = hs_clip16_shift(hs_sub(e2, o2), add, shift);
        d[30] = hs_clip16_shift(hs_sub(e1, o1), add, shift);
        d[31] = hs_clip16_shift(hs_sub(e0, o0), add, shift);
    }
#undef A8
#undef A4
#undef A2
}

static int only_dc_n(const int16_t *c, int n)
{
    int i, nn = n * n;
    for (i = 1; i < nn; i++)
        if (c[i]) return 0;
    return 1;
}

static void dc_fill_n(int16_t *out, int n, int16_t dc, int bit_depth)
{
    int shift1 = 7, shift2 = 20 - bit_depth;
    int32_t add1 = 1 << (shift1 - 1), add2 = 1 << (shift2 - 1);
    int32_t v1 = (64 * (int32_t)dc + add1) >> shift1;
    int16_t v;
    int i, nn = n * n;
    if (v1 < -32768) v1 = -32768;
    if (v1 > 32767) v1 = 32767;
    v1 = (64 * v1 + add2) >> shift2;
    if (v1 < -32768) v1 = -32768;
    if (v1 > 32767) v1 = 32767;
    v = (int16_t)v1;
    for (i = 0; i < nn; i++) out[i] = v;
}

int heic_simd_idct8(const int16_t *coeffs, int16_t *output, int bit_depth)
{
    int shift1 = 7, shift2 = 20 - bit_depth;
    int32_t tmp[64];
    int col, row;
    __m128i s[8], d[8];
    if (!g_simd) return 0;
    if (only_dc_n(coeffs, 8)) {
        dc_fill_n(output, 8, coeffs[0], bit_depth);
        return 1;
    }
    memset(tmp, 0, sizeof(tmp));
    for (col = 0; col < 8; col += 4) {
        load4_cols_i16(coeffs, 8, col, s, 8);
        idct8_1d_x4(s, d, shift1);
        store4_cols_i32(tmp, 8, col, d, 8);
    }
    for (row = 0; row < 8; row += 4) {
        load4_rows_i32(tmp, 8, row, s, 8);
        idct8_1d_x4(s, d, shift2);
        store4_rows_i16(output, 8, row, d, 8);
    }
    return 1;
}

int heic_simd_idct16(const int16_t *coeffs, int16_t *output, int bit_depth)
{
    int shift1 = 7, shift2 = 20 - bit_depth;
    int32_t *tmp = heic_idct_scratch_buf();
    int col, row;
    __m128i s[16], d[16];
    if (!g_simd) return 0;
    if (only_dc_n(coeffs, 16)) {
        dc_fill_n(output, 16, coeffs[0], bit_depth);
        return 1;
    }
    memset(tmp, 0, 256 * sizeof(int32_t));
    for (col = 0; col < 16; col += 4) {
        load4_cols_i16(coeffs, 16, col, s, 16);
        idct16_1d_x4(s, d, shift1);
        store4_cols_i32(tmp, 16, col, d, 16);
    }
    for (row = 0; row < 16; row += 4) {
        load4_rows_i32(tmp, 16, row, s, 16);
        idct16_1d_x4(s, d, shift2);
        store4_rows_i16(output, 16, row, d, 16);
    }
    return 1;
}

int heic_simd_idct32(const int16_t *coeffs, int16_t *output, int bit_depth)
{
    int shift1 = 7, shift2 = 20 - bit_depth;
    int32_t *tmp = heic_idct_scratch_buf();
    int col, row;
    __m128i s[32], d[32];
    if (!g_simd) return 0;
    if (only_dc_n(coeffs, 32)) {
        dc_fill_n(output, 32, coeffs[0], bit_depth);
        return 1;
    }
    memset(tmp, 0, 1024 * sizeof(int32_t));
    for (col = 0; col < 32; col += 4) {

        {
            int k, any = 0;
            for (k = 0; k < 32 && !any; k++) {
                const int16_t *p = coeffs + k * 32 + col;
                if (p[0] | p[1] | p[2] | p[3]) any = 1;
            }
            if (!any) continue;
        }
        load4_cols_i16(coeffs, 32, col, s, 32);
        idct32_1d_x4(s, d, shift1);
        store4_cols_i32(tmp, 32, col, d, 32);
    }
    for (row = 0; row < 32; row += 4) {
        load4_rows_i32(tmp, 32, row, s, 32);
        idct32_1d_x4(s, d, shift2);
        store4_rows_i16(output, 32, row, d, 32);
    }
    return 1;
}

int heic_simd_add_residual(uint16_t *plane, int stride, int x0, int y0,
                           const int16_t *residual, int size, int max_val)
{
    int py, px;
    __m128i vzero;
    if (!g_simd) return 0;
    if (max_val > 65535) return 0;
    vzero = _mm_setzero_si128();
    for (py = 0; py < size; py++) {
        uint16_t *dst = plane + (y0 + py) * stride + x0;
        const int16_t *src = residual + py * size;
        px = 0;
        for (; px + 8 <= size; px += 8) {
            __m128i d = _mm_loadu_si128((const __m128i *)(dst + px));
            __m128i r = _mm_loadu_si128((const __m128i *)(src + px));

            __m128i d_lo = _mm_unpacklo_epi16(d, vzero);
            __m128i d_hi = _mm_unpackhi_epi16(d, vzero);
            __m128i r_lo = _mm_cvtepi16_epi32(r);
            __m128i r_hi = _mm_cvtepi16_epi32(_mm_srli_si128(r, 8));
            __m128i s_lo = _mm_add_epi32(d_lo, r_lo);
            __m128i s_hi = _mm_add_epi32(d_hi, r_hi);
            s_lo = _mm_max_epi32(s_lo, _mm_setzero_si128());
            s_hi = _mm_max_epi32(s_hi, _mm_setzero_si128());
            s_lo = _mm_min_epi32(s_lo, _mm_set1_epi32(max_val));
            s_hi = _mm_min_epi32(s_hi, _mm_set1_epi32(max_val));
            _mm_storeu_si128((__m128i *)(dst + px), _mm_packus_epi32(s_lo, s_hi));
        }
        for (; px < size; px++) {
            int32_t v = (int32_t)dst[px] + (int32_t)src[px];
            if (v < 0) v = 0;
            else if (v > max_val) v = max_val;
            dst[px] = (uint16_t)v;
        }
    }
    return 1;
}

static inline __m128i lut4_i32(const int32_t *tab, const uint16_t *p)
{
    return _mm_setr_epi32(tab[p[0] & 255], tab[p[1] & 255], tab[p[2] & 255],
                          tab[p[3] & 255]);
}

static inline __m128i clamp_u8_epi32(__m128i v)
{
    v = _mm_max_epi32(v, _mm_setzero_si128());
    v = _mm_min_epi32(v, _mm_set1_epi32(255));
    return v;
}

static inline void store_rgb4(__m128i r, __m128i g, __m128i b, uint8_t *dst)
{
    __m128i r8 = _mm_packus_epi16(_mm_packus_epi32(r, r), _mm_setzero_si128());
    __m128i g8 = _mm_packus_epi16(_mm_packus_epi32(g, g), _mm_setzero_si128());
    __m128i b8 = _mm_packus_epi16(_mm_packus_epi32(b, b), _mm_setzero_si128());

    __m128i rgb = _mm_set_epi32(0, _mm_cvtsi128_si32(b8), _mm_cvtsi128_si32(g8),
                                _mm_cvtsi128_si32(r8));
    static const char k_shuf[16] = {0, 4, 8, 1, 5, 9, 2, 6, 10, 3, 7, 11, -1, -1, -1, -1};
    __m128i out = _mm_shuffle_epi8(rgb, _mm_loadu_si128((const __m128i *)k_shuf));
    _mm_storel_epi64((__m128i *)dst, out);
    {
        unsigned t = (unsigned)_mm_extract_epi32(out, 2);
        dst[8] = (uint8_t)t;
        dst[9] = (uint8_t)(t >> 8);
        dst[10] = (uint8_t)(t >> 16);
        dst[11] = (uint8_t)(t >> 24);
    }
}

int heic_simd_ycc_444_row(const uint16_t *yp, const uint16_t *cbp, const uint16_t *crp,
                          uint8_t *row, int w, int full, const int32_t yv[256],
                          const int32_t cr_r[256], const int32_t cb_g[256],
                          const int32_t cr_g[256], const int32_t cb_b[256])
{
    int x;
    if (!g_simd || w < 4) return 0;
    if (full) {
        __m128i round = _mm_set1_epi32(128);
        for (x = 0; x + 4 <= w; x += 4) {
            __m128i Y = lut4_i32(yv, yp + x);
            __m128i CrR = lut4_i32(cr_r, crp + x);
            __m128i CbG = lut4_i32(cb_g, cbp + x);
            __m128i CrG = lut4_i32(cr_g, crp + x);
            __m128i CbB = lut4_i32(cb_b, cbp + x);
            __m128i rr = clamp_u8_epi32(
                _mm_add_epi32(Y, _mm_srai_epi32(_mm_add_epi32(CrR, round), 8)));
            __m128i gg = clamp_u8_epi32(_mm_add_epi32(
                Y, _mm_srai_epi32(_mm_add_epi32(_mm_add_epi32(CbG, CrG), round), 8)));
            __m128i bb = clamp_u8_epi32(
                _mm_add_epi32(Y, _mm_srai_epi32(_mm_add_epi32(CbB, round), 8)));
            store_rgb4(rr, gg, bb, row + x * 3);
        }
    } else {
        __m128i round = _mm_set1_epi32(4096);
        for (x = 0; x + 4 <= w; x += 4) {
            __m128i Y = lut4_i32(yv, yp + x);
            __m128i CrR = lut4_i32(cr_r, crp + x);
            __m128i CbG = lut4_i32(cb_g, cbp + x);
            __m128i CrG = lut4_i32(cr_g, crp + x);
            __m128i CbB = lut4_i32(cb_b, cbp + x);
            __m128i rr = clamp_u8_epi32(
                _mm_srai_epi32(_mm_add_epi32(_mm_add_epi32(Y, CrR), round), 13));
            __m128i gg = clamp_u8_epi32(_mm_srai_epi32(
                _mm_add_epi32(_mm_add_epi32(_mm_add_epi32(Y, CbG), CrG), round), 13));
            __m128i bb = clamp_u8_epi32(
                _mm_srai_epi32(_mm_add_epi32(_mm_add_epi32(Y, CbB), round), 13));
            store_rgb4(rr, gg, bb, row + x * 3);
        }
    }

    for (; x < w; x++) {
        int Y = (int)yp[x] & 255, Cb = (int)cbp[x] & 255, Cr = (int)crp[x] & 255;
        int rr, gg, bb;
        if (full) {
            rr = yv[Y] + ((cr_r[Cr] + 128) >> 8);
            gg = yv[Y] + ((cb_g[Cb] + cr_g[Cr] + 128) >> 8);
            bb = yv[Y] + ((cb_b[Cb] + 128) >> 8);
        } else {
            rr = (yv[Y] + cr_r[Cr] + 4096) >> 13;
            gg = (yv[Y] + cb_g[Cb] + cr_g[Cr] + 4096) >> 13;
            bb = (yv[Y] + cb_b[Cb] + 4096) >> 13;
        }
        if (rr < 0) rr = 0;
        else if (rr > 255) rr = 255;
        if (gg < 0) gg = 0;
        else if (gg > 255) gg = 255;
        if (bb < 0) bb = 0;
        else if (bb > 255) bb = 255;
        row[x * 3 + 0] = (uint8_t)rr;
        row[x * 3 + 1] = (uint8_t)gg;
        row[x * 3 + 2] = (uint8_t)bb;
    }
    return 1;
}

int heic_simd_chroma_edge4(uint16_t *plane, int stride, size_t base_q0, int across,
                           int tc, int max_val, int along_is_stride)
{
    size_t along = along_is_stride ? (size_t)stride : 1;
    size_t ac = (size_t)across;
    __m128i p1, p0, q0, q1, delta, p0n, q0n, vtc, vmtc, vmax;
    size_t b0, b1, b2, b3;
    if (!g_simd || tc <= 0) return 0;
    b0 = base_q0;
    b1 = base_q0 + along;
    b2 = base_q0 + 2 * along;
    b3 = base_q0 + 3 * along;

    p1 = _mm_setr_epi32(plane[b0 - 2 * ac], plane[b1 - 2 * ac], plane[b2 - 2 * ac],
                        plane[b3 - 2 * ac]);
    p0 = _mm_setr_epi32(plane[b0 - ac], plane[b1 - ac], plane[b2 - ac], plane[b3 - ac]);
    q0 = _mm_setr_epi32(plane[b0], plane[b1], plane[b2], plane[b3]);
    q1 = _mm_setr_epi32(plane[b0 + ac], plane[b1 + ac], plane[b2 + ac], plane[b3 + ac]);

    delta = _mm_sub_epi32(q0, p0);
    delta = _mm_slli_epi32(delta, 2);
    delta = _mm_add_epi32(delta, p1);
    delta = _mm_sub_epi32(delta, q1);
    delta = _mm_add_epi32(delta, _mm_set1_epi32(4));
    delta = _mm_srai_epi32(delta, 3);
    vtc = _mm_set1_epi32(tc);
    vmtc = _mm_set1_epi32(-tc);
    delta = _mm_min_epi32(delta, vtc);
    delta = _mm_max_epi32(delta, vmtc);
    vmax = _mm_set1_epi32(max_val);
    p0n = _mm_add_epi32(p0, delta);
    q0n = _mm_sub_epi32(q0, delta);
    p0n = _mm_max_epi32(p0n, _mm_setzero_si128());
    q0n = _mm_max_epi32(q0n, _mm_setzero_si128());
    p0n = _mm_min_epi32(p0n, vmax);
    q0n = _mm_min_epi32(q0n, vmax);
    {
        int32_t pn[4], qn[4];
        _mm_storeu_si128((__m128i *)pn, p0n);
        _mm_storeu_si128((__m128i *)qn, q0n);
        plane[b0 - ac] = (uint16_t)pn[0];
        plane[b1 - ac] = (uint16_t)pn[1];
        plane[b2 - ac] = (uint16_t)pn[2];
        plane[b3 - ac] = (uint16_t)pn[3];
        plane[b0] = (uint16_t)qn[0];
        plane[b1] = (uint16_t)qn[1];
        plane[b2] = (uint16_t)qn[2];
        plane[b3] = (uint16_t)qn[3];
    }
    return 1;
}

static inline __m128i load4_along(const uint16_t *plane, size_t base, size_t along)
{
    return _mm_setr_epi32(plane[base], plane[base + along], plane[base + 2 * along],
                          plane[base + 3 * along]);
}

static inline void store4_along(uint16_t *plane, size_t base, size_t along, __m128i v)
{
    int32_t t[4];
    _mm_storeu_si128((__m128i *)t, v);
    plane[base] = (uint16_t)t[0];
    plane[base + along] = (uint16_t)t[1];
    plane[base + 2 * along] = (uint16_t)t[2];
    plane[base + 3 * along] = (uint16_t)t[3];
}

static inline __m128i clamp_i32(__m128i v, int lo, int hi)
{
    v = _mm_max_epi32(v, _mm_set1_epi32(lo));
    v = _mm_min_epi32(v, _mm_set1_epi32(hi));
    return v;
}

int heic_simd_luma_filter4(uint16_t *plane, size_t base_p, size_t base_q,
                           size_t step_along, size_t step_across, int strong, int d_ep,
                           int d_eq, int tc, int max_val)
{
    __m128i p0, p1, p2, p3, q0, q1, q2, q3;
    if (!g_simd || tc <= 0) return 0;
    p0 = load4_along(plane, base_p, step_along);
    p1 = load4_along(plane, base_p - step_across, step_along);
    p2 = load4_along(plane, base_p - 2 * step_across, step_along);
    q0 = load4_along(plane, base_q, step_along);
    q1 = load4_along(plane, base_q + step_across, step_along);
    q2 = load4_along(plane, base_q + 2 * step_across, step_along);

    if (strong) {
        int tc2 = 2 * tc;
        __m128i four = _mm_set1_epi32(4);
        __m128i two = _mm_set1_epi32(2);
        __m128i p0f, p1f, p2f, q0f, q1f, q2f;
        p3 = load4_along(plane, base_p - 3 * step_across, step_along);
        q3 = load4_along(plane, base_q + 3 * step_across, step_along);

        p0f = _mm_add_epi32(p2, _mm_slli_epi32(p1, 1));
        p0f = _mm_add_epi32(p0f, _mm_slli_epi32(p0, 1));
        p0f = _mm_add_epi32(p0f, _mm_slli_epi32(q0, 1));
        p0f = _mm_add_epi32(p0f, q1);
        p0f = _mm_srai_epi32(_mm_add_epi32(p0f, four), 3);

        p0f = _mm_min_epi32(p0f, _mm_add_epi32(p0, _mm_set1_epi32(tc2)));
        p0f = _mm_max_epi32(p0f, _mm_sub_epi32(p0, _mm_set1_epi32(tc2)));
        p0f = clamp_i32(p0f, 0, max_val);

        p1f = _mm_add_epi32(_mm_add_epi32(p2, p1), _mm_add_epi32(p0, q0));
        p1f = _mm_srai_epi32(_mm_add_epi32(p1f, two), 2);
        p1f = _mm_min_epi32(p1f, _mm_add_epi32(p1, _mm_set1_epi32(tc2)));
        p1f = _mm_max_epi32(p1f, _mm_sub_epi32(p1, _mm_set1_epi32(tc2)));
        p1f = clamp_i32(p1f, 0, max_val);

        p2f = _mm_add_epi32(_mm_slli_epi32(p3, 1), _mm_add_epi32(_mm_mullo_epi32(p2, _mm_set1_epi32(3)), p1));
        p2f = _mm_add_epi32(p2f, _mm_add_epi32(p0, q0));
        p2f = _mm_srai_epi32(_mm_add_epi32(p2f, four), 3);
        p2f = _mm_min_epi32(p2f, _mm_add_epi32(p2, _mm_set1_epi32(tc2)));
        p2f = _mm_max_epi32(p2f, _mm_sub_epi32(p2, _mm_set1_epi32(tc2)));
        p2f = clamp_i32(p2f, 0, max_val);

        q0f = _mm_add_epi32(p1, _mm_slli_epi32(p0, 1));
        q0f = _mm_add_epi32(q0f, _mm_slli_epi32(q0, 1));
        q0f = _mm_add_epi32(q0f, _mm_slli_epi32(q1, 1));
        q0f = _mm_add_epi32(q0f, q2);
        q0f = _mm_srai_epi32(_mm_add_epi32(q0f, four), 3);
        q0f = _mm_min_epi32(q0f, _mm_add_epi32(q0, _mm_set1_epi32(tc2)));
        q0f = _mm_max_epi32(q0f, _mm_sub_epi32(q0, _mm_set1_epi32(tc2)));
        q0f = clamp_i32(q0f, 0, max_val);

        q1f = _mm_add_epi32(_mm_add_epi32(p0, q0), _mm_add_epi32(q1, q2));
        q1f = _mm_srai_epi32(_mm_add_epi32(q1f, two), 2);
        q1f = _mm_min_epi32(q1f, _mm_add_epi32(q1, _mm_set1_epi32(tc2)));
        q1f = _mm_max_epi32(q1f, _mm_sub_epi32(q1, _mm_set1_epi32(tc2)));
        q1f = clamp_i32(q1f, 0, max_val);

        q2f = _mm_add_epi32(p0, q0);
        q2f = _mm_add_epi32(q2f, q1);
        q2f = _mm_add_epi32(q2f, _mm_mullo_epi32(q2, _mm_set1_epi32(3)));
        q2f = _mm_add_epi32(q2f, _mm_slli_epi32(q3, 1));
        q2f = _mm_srai_epi32(_mm_add_epi32(q2f, four), 3);
        q2f = _mm_min_epi32(q2f, _mm_add_epi32(q2, _mm_set1_epi32(tc2)));
        q2f = _mm_max_epi32(q2f, _mm_sub_epi32(q2, _mm_set1_epi32(tc2)));
        q2f = clamp_i32(q2f, 0, max_val);

        store4_along(plane, base_p, step_along, p0f);
        store4_along(plane, base_p - step_across, step_along, p1f);
        store4_along(plane, base_p - 2 * step_across, step_along, p2f);
        store4_along(plane, base_q, step_along, q0f);
        store4_along(plane, base_q + step_across, step_along, q1f);
        store4_along(plane, base_q + 2 * step_across, step_along, q2f);
    } else {

        __m128i diff0 = _mm_sub_epi32(q0, p0);
        __m128i diff1 = _mm_sub_epi32(q1, p1);
        __m128i delta =
            _mm_sub_epi32(_mm_mullo_epi32(diff0, _mm_set1_epi32(9)),
                          _mm_mullo_epi32(diff1, _mm_set1_epi32(3)));
        __m128i absd, mask, thr;
        delta = _mm_srai_epi32(_mm_add_epi32(delta, _mm_set1_epi32(8)), 4);
        absd = _mm_abs_epi32(delta);
        thr = _mm_set1_epi32(10 * tc);
        mask = _mm_cmplt_epi32(absd, thr);
        delta = clamp_i32(delta, -tc, tc);
        delta = _mm_and_si128(delta, mask);
        {
            __m128i p0n = clamp_i32(_mm_add_epi32(p0, delta), 0, max_val);
            __m128i q0n = clamp_i32(_mm_sub_epi32(q0, delta), 0, max_val);

            p0n = _mm_blendv_epi8(p0, p0n, mask);
            q0n = _mm_blendv_epi8(q0, q0n, mask);
            store4_along(plane, base_p, step_along, p0n);
            store4_along(plane, base_q, step_along, q0n);
            if (d_ep) {
                int tch = tc >> 1;
                __m128i dp = _mm_srai_epi32(_mm_add_epi32(p2, p0), 1);
                dp = _mm_srai_epi32(_mm_add_epi32(_mm_add_epi32(p2, p0), _mm_set1_epi32(1)), 1);
                dp = _mm_sub_epi32(dp, p1);
                dp = _mm_add_epi32(dp, delta);
                dp = _mm_srai_epi32(dp, 1);
                dp = clamp_i32(dp, -tch, tch);
                dp = _mm_and_si128(dp, mask);
                {
                    __m128i p1n = clamp_i32(_mm_add_epi32(p1, dp), 0, max_val);
                    p1n = _mm_blendv_epi8(p1, p1n, mask);
                    store4_along(plane, base_p - step_across, step_along, p1n);
                }
            }
            if (d_eq) {
                int tch = tc >> 1;
                __m128i dq =
                    _mm_srai_epi32(_mm_add_epi32(_mm_add_epi32(q2, q0), _mm_set1_epi32(1)), 1);
                dq = _mm_sub_epi32(dq, q1);
                dq = _mm_sub_epi32(dq, delta);
                dq = _mm_srai_epi32(dq, 1);
                dq = clamp_i32(dq, -tch, tch);
                dq = _mm_and_si128(dq, mask);
                {
                    __m128i q1n = clamp_i32(_mm_add_epi32(q1, dq), 0, max_val);
                    q1n = _mm_blendv_epi8(q1, q1n, mask);
                    store4_along(plane, base_q + step_across, step_along, q1n);
                }
            }
        }
    }
    return 1;
}

int heic_simd_dequant(int16_t *coeffs, int n, int32_t combined, int shift)
{
    int i;
    __m128i c, add;
    if (!g_simd || shift < 0 || combined > 65536 || n < 8) return 0;
    c = _mm_set1_epi32(combined);
    add = shift > 0 ? _mm_set1_epi32(1 << (shift - 1)) : _mm_setzero_si128();
    for (i = 0; i + 8 <= n; i += 8) {
        __m128i v = _mm_loadu_si128((const __m128i *)(coeffs + i));
        __m128i a = _mm_cvtepi16_epi32(v);
        __m128i b = _mm_cvtepi16_epi32(_mm_srli_si128(v, 8));
        a = _mm_srai_epi32(_mm_add_epi32(_mm_mullo_epi32(a, c), add), shift);
        b = _mm_srai_epi32(_mm_add_epi32(_mm_mullo_epi32(b, c), add), shift);
        a = clamp_i32(a, -32768, 32767);
        b = clamp_i32(b, -32768, 32767);
        _mm_storeu_si128((__m128i *)(coeffs + i), _mm_packs_epi32(a, b));
    }
    for (; i < n; i++) {
        int32_t v = ((int32_t)coeffs[i] * combined + (shift > 0 ? (1 << (shift - 1)) : 0)) >> shift;
        if (v < -32768) v = -32768;
        if (v > 32767) v = 32767;
        coeffs[i] = (int16_t)v;
    }
    return 1;
}

int heic_simd_intra_ang_row(uint16_t *dst, const int32_t *ref, int n, int a, int b,
                            int max_val)
{
    int i;
    __m128i va, vb, round, vmax;
    if (!g_simd || n < 4) return 0;
    va = _mm_set1_epi32(a);
    vb = _mm_set1_epi32(b);
    round = _mm_set1_epi32(16);
    vmax = _mm_set1_epi32(max_val);
    for (i = 0; i + 4 <= n; i += 4) {
        __m128i r0 = _mm_loadu_si128((const __m128i *)(ref + i));
        __m128i r1 = _mm_loadu_si128((const __m128i *)(ref + i + 1));
        __m128i pred =
            _mm_srai_epi32(_mm_add_epi32(_mm_add_epi32(_mm_mullo_epi32(va, r0),
                                                       _mm_mullo_epi32(vb, r1)),
                                         round),
                           5);
        pred = clamp_i32(pred, 0, max_val);
        {
            int32_t t[4];
            _mm_storeu_si128((__m128i *)t, pred);
            dst[i] = (uint16_t)t[0];
            dst[i + 1] = (uint16_t)t[1];
            dst[i + 2] = (uint16_t)t[2];
            dst[i + 3] = (uint16_t)t[3];
        }
        (void)vmax;
    }
    for (; i < n; i++) {
        int32_t pred = (a * ref[i] + b * ref[i + 1] + 16) >> 5;
        if (pred < 0) pred = 0;
        if (pred > max_val) pred = max_val;
        dst[i] = (uint16_t)pred;
    }
    return 1;
}

int heic_simd_u16_to_i32_avail(const uint16_t *src, int32_t *border, int *avail, int n)
{
    int i;
    if (!g_simd || n < 4) return 0;
    for (i = 0; i + 4 <= n; i += 4) {
        __m128i v = _mm_loadl_epi64((const __m128i *)(src + i));
        __m128i z = _mm_setzero_si128();
        __m128i lo = _mm_unpacklo_epi16(v, z);
        _mm_storeu_si128((__m128i *)(border + i), lo);
        avail[i] = avail[i + 1] = avail[i + 2] = avail[i + 3] = 1;
    }
    for (; i < n; i++) {
        border[i] = src[i];
        avail[i] = 1;
    }
    return 1;
}

int heic_simd_border_top_ext(const uint16_t *src, int32_t *border, int *avail, int n)
{
    int i, count = 0;
    __m128i uninit = _mm_set1_epi16(-1);
    if (!g_simd || n < 4) return 0;
    for (i = 0; i + 4 <= n; i += 4) {
        __m128i v = _mm_loadl_epi64((const __m128i *)(src + i));
        __m128i ok = _mm_cmpeq_epi16(v, uninit);
        ok = _mm_xor_si128(ok, _mm_set1_epi16(-1));
        {
            int32_t t[4];
            int16_t s[4], m[4];
            _mm_storel_epi64((__m128i *)s, v);
            _mm_storel_epi64((__m128i *)m, ok);
            t[0] = s[0];
            t[1] = s[1];
            t[2] = s[2];
            t[3] = s[3];
            if (m[0]) {
                border[i] = t[0];
                avail[i] = 1;
                count++;
            }
            if (m[1]) {
                border[i + 1] = t[1];
                avail[i + 1] = 1;
                count++;
            }
            if (m[2]) {
                border[i + 2] = t[2];
                avail[i + 2] = 1;
                count++;
            }
            if (m[3]) {
                border[i + 3] = t[3];
                avail[i + 3] = 1;
                count++;
            }
        }
    }
    for (; i < n; i++) {
        if (src[i] != HEIC_UNINIT_SAMPLE) {
            border[i] = src[i];
            avail[i] = 1;
            count++;
        }
    }
    return count >= 0 ? 1 : 0;
}

int heic_simd_intra_ang_row_var(uint16_t *dst, const int32_t *ref, int n, int row_base,
                                int32_t angle, int max_val)
{
    int px;
    if (!g_simd || n < 4) return 0;

    for (px = 0; px + 4 <= n; px += 4) {
        int k;
        int32_t pred4[4];
        for (k = 0; k < 4; k++) {
            int p = px + k;
            int32_t i_idx = ((p + 1) * angle) >> 5;
            int32_t i_fact = ((p + 1) * angle) & 31;
            int idx = row_base + (int)i_idx;
            int32_t pred;
            if (i_fact != 0)
                pred = ((32 - i_fact) * ref[idx] + i_fact * ref[idx + 1] + 16) >> 5;
            else
                pred = ref[idx];
            pred4[k] = pred;
        }
        {
            __m128i p = _mm_loadu_si128((const __m128i *)pred4);
            p = clamp_i32(p, 0, max_val);
            _mm_storeu_si128((__m128i *)pred4, p);
            dst[px] = (uint16_t)pred4[0];
            dst[px + 1] = (uint16_t)pred4[1];
            dst[px + 2] = (uint16_t)pred4[2];
            dst[px + 3] = (uint16_t)pred4[3];
        }
    }
    for (; px < n; px++) {
        int32_t i_idx = ((px + 1) * angle) >> 5;
        int32_t i_fact = ((px + 1) * angle) & 31;
        int idx = row_base + (int)i_idx;
        int32_t pred;
        if (i_fact != 0)
            pred = ((32 - i_fact) * ref[idx] + i_fact * ref[idx + 1] + 16) >> 5;
        else
            pred = ref[idx];
        if (pred < 0) pred = 0;
        if (pred > max_val) pred = max_val;
        dst[px] = (uint16_t)pred;
    }
    return 1;
}

static inline __m128i isign4(__m128i a)
{
    __m128i gt = _mm_cmpgt_epi32(a, _mm_setzero_si128());
    __m128i lt = _mm_cmplt_epi32(a, _mm_setzero_si128());
    return _mm_or_si128(_mm_and_si128(gt, _mm_set1_epi32(1)),
                        _mm_and_si128(lt, _mm_set1_epi32(-1)));
}

int heic_simd_sao_band_row(uint16_t *row, int x0, int x1, int band_shift,
                           const int16_t band_table[32], int max_val)
{
    int x;
    if (!g_simd || x1 - x0 < 8) return 0;

    for (x = x0; x + 8 <= x1; x += 8) {
        __m128i v = _mm_loadu_si128((const __m128i *)(row + x));
        int16_t s[8];
        int k;
        _mm_storeu_si128((__m128i *)s, v);
        for (k = 0; k < 8; k++) {
            int sample = (int)(uint16_t)s[k];
            int band, offset, out;
            if (sample > max_val) sample = max_val;
            band = sample >> band_shift;
            offset = (int)band_table[band & 31];
            if (!offset) continue;
            out = sample + offset;
            if (out < 0) out = 0;
            else if (out > max_val) out = max_val;
            s[k] = (int16_t)out;
        }
        _mm_storeu_si128((__m128i *)(row + x), _mm_loadu_si128((const __m128i *)s));
    }
    for (; x < x1; x++) {
        int sample = (int)row[x];
        int band, offset;
        if (sample > max_val) sample = max_val;
        band = sample >> band_shift;
        offset = (int)band_table[band & 31];
        if (offset) {
            int v = sample + offset;
            if (v < 0) v = 0;
            else if (v > max_val) v = max_val;
            row[x] = (uint16_t)v;
        }
    }
    return 1;
}

int heic_simd_sao_edge_h_row(const uint16_t *srow, uint16_t *drow, int x0, int x1,
                             const int offset_table[5], int max_val)
{
    int x;
    if (!g_simd || x1 - x0 < 4) return 0;
    for (x = x0; x + 4 <= x1; x += 4) {
        __m128i s = _mm_setr_epi32(srow[x], srow[x + 1], srow[x + 2], srow[x + 3]);
        __m128i n0 = _mm_setr_epi32(srow[x - 1], srow[x], srow[x + 1], srow[x + 2]);
        __m128i n1 = _mm_setr_epi32(srow[x + 1], srow[x + 2], srow[x + 3], srow[x + 4]);
        __m128i e = _mm_add_epi32(_mm_set1_epi32(2),
                                  _mm_add_epi32(isign4(_mm_sub_epi32(s, n0)),
                                                isign4(_mm_sub_epi32(s, n1))));
        int32_t ei[4], si[4];
        int k;
        _mm_storeu_si128((__m128i *)ei, e);
        _mm_storeu_si128((__m128i *)si, s);
        for (k = 0; k < 4; k++) {
            int off = offset_table[ei[k]];
            if (off) {
                int v = si[k] + off;
                if (v < 0) v = 0;
                else if (v > max_val) v = max_val;
                drow[x + k] = (uint16_t)v;
            }
        }
    }
    for (; x < x1; x++) {
        int sample = (int)srow[x];
        int edge_idx = 2 + (sample > srow[x - 1] ? 1 : sample < srow[x - 1] ? -1 : 0) +
                       (sample > srow[x + 1] ? 1 : sample < srow[x + 1] ? -1 : 0);
        int off = offset_table[edge_idx];
        if (off) {
            int v = sample + off;
            if (v < 0) v = 0;
            else if (v > max_val) v = max_val;
            drow[x] = (uint16_t)v;
        }
    }
    return 1;
}

int heic_simd_sao_edge_v_row(const uint16_t *src, uint16_t *dst, int stride, int y,
                             int x0, int x1, const int offset_table[5], int max_val)
{
    int x;
    const uint16_t *srow = src + (size_t)y * (size_t)stride;
    const uint16_t *up = src + (size_t)(y - 1) * (size_t)stride;
    const uint16_t *dn = src + (size_t)(y + 1) * (size_t)stride;
    uint16_t *drow = dst + (size_t)y * (size_t)stride;
    if (!g_simd || x1 - x0 < 4) return 0;
    for (x = x0; x + 4 <= x1; x += 4) {
        __m128i s = _mm_setr_epi32(srow[x], srow[x + 1], srow[x + 2], srow[x + 3]);
        __m128i n0 = _mm_setr_epi32(up[x], up[x + 1], up[x + 2], up[x + 3]);
        __m128i n1 = _mm_setr_epi32(dn[x], dn[x + 1], dn[x + 2], dn[x + 3]);
        __m128i e = _mm_add_epi32(_mm_set1_epi32(2),
                                  _mm_add_epi32(isign4(_mm_sub_epi32(s, n0)),
                                                isign4(_mm_sub_epi32(s, n1))));
        int32_t ei[4], si[4];
        int k;
        _mm_storeu_si128((__m128i *)ei, e);
        _mm_storeu_si128((__m128i *)si, s);
        for (k = 0; k < 4; k++) {
            int off = offset_table[ei[k]];
            if (off) {
                int v = si[k] + off;
                if (v < 0) v = 0;
                else if (v > max_val) v = max_val;
                drow[x + k] = (uint16_t)v;
            }
        }
    }
    for (; x < x1; x++) {
        int sample = (int)srow[x];
        int edge_idx = 2 + (sample > up[x] ? 1 : sample < up[x] ? -1 : 0) +
                       (sample > dn[x] ? 1 : sample < dn[x] ? -1 : 0);
        int off = offset_table[edge_idx];
        if (off) {
            int v = sample + off;
            if (v < 0) v = 0;
            else if (v > max_val) v = max_val;
            drow[x] = (uint16_t)v;
        }
    }
    return 1;
}

#else

int heic_simd_idct8(const int16_t *c, int16_t *o, int bd)
{
    (void)c;
    (void)o;
    (void)bd;
    return 0;
}
int heic_simd_idct16(const int16_t *c, int16_t *o, int bd)
{
    (void)c;
    (void)o;
    (void)bd;
    return 0;
}
int heic_simd_idct32(const int16_t *c, int16_t *o, int bd)
{
    (void)c;
    (void)o;
    (void)bd;
    return 0;
}
int heic_simd_add_residual(uint16_t *p, int s, int x, int y, const int16_t *r, int n,
                           int m)
{
    (void)p;
    (void)s;
    (void)x;
    (void)y;
    (void)r;
    (void)n;
    (void)m;
    return 0;
}
int heic_simd_ycc_444_row(const uint16_t *a, const uint16_t *b, const uint16_t *c,
                          uint8_t *d, int w, int full, const int32_t *e, const int32_t *f,
                          const int32_t *g, const int32_t *h, const int32_t *i)
{
    (void)a;
    (void)b;
    (void)c;
    (void)d;
    (void)w;
    (void)full;
    (void)e;
    (void)f;
    (void)g;
    (void)h;
    (void)i;
    return 0;
}
int heic_simd_chroma_edge4(uint16_t *p, int s, size_t b, int a, int t, int m, int al)
{
    (void)p;
    (void)s;
    (void)b;
    (void)a;
    (void)t;
    (void)m;
    (void)al;
    return 0;
}
int heic_simd_luma_filter4(uint16_t *p, size_t bp, size_t bq, size_t sa, size_t sc, int st,
                           int dep, int deq, int tc, int mv)
{
    (void)p;
    (void)bp;
    (void)bq;
    (void)sa;
    (void)sc;
    (void)st;
    (void)dep;
    (void)deq;
    (void)tc;
    (void)mv;
    return 0;
}
int heic_simd_dequant(int16_t *c, int n, int32_t comb, int sh)
{
    (void)c;
    (void)n;
    (void)comb;
    (void)sh;
    return 0;
}
int heic_simd_intra_ang_row(uint16_t *d, const int32_t *r, int n, int a, int b, int m)
{
    (void)d;
    (void)r;
    (void)n;
    (void)a;
    (void)b;
    (void)m;
    return 0;
}
int heic_simd_u16_to_i32_avail(const uint16_t *s, int32_t *b, int *a, int n)
{
    (void)s;
    (void)b;
    (void)a;
    (void)n;
    return 0;
}
int heic_simd_border_top_ext(const uint16_t *s, int32_t *b, int *a, int n)
{
    (void)s;
    (void)b;
    (void)a;
    (void)n;
    return 0;
}
int heic_simd_intra_ang_row_var(uint16_t *d, const int32_t *r, int n, int rb, int32_t ang,
                                int m)
{
    (void)d;
    (void)r;
    (void)n;
    (void)rb;
    (void)ang;
    (void)m;
    return 0;
}
int heic_simd_sao_band_row(uint16_t *r, int a, int b, int s, const int16_t *t, int m)
{
    (void)r;
    (void)a;
    (void)b;
    (void)s;
    (void)t;
    (void)m;
    return 0;
}
int heic_simd_sao_edge_h_row(const uint16_t *s, uint16_t *d, int a, int b, const int *o,
                             int m)
{
    (void)s;
    (void)d;
    (void)a;
    (void)b;
    (void)o;
    (void)m;
    return 0;
}
int heic_simd_sao_edge_v_row(const uint16_t *s, uint16_t *d, int st, int y, int a, int b,
                             const int *o, int m)
{
    (void)s;
    (void)d;
    (void)st;
    (void)y;
    (void)a;
    (void)b;
    (void)o;
    (void)m;
    return 0;
}

#endif

#define HEIC_MAX_INTRA 32
#define HEIC_BORDER_N  (4 * HEIC_MAX_INTRA + 1)

static const int16_t HEIC_INTRA_ANGLE[35] = {
    0, 0,
    32, 26, 21, 17, 13, 9, 5, 2,
    0,
    -2, -5, -9, -13, -17, -21, -26,
    -32,
    -26, -21, -17, -13, -9, -5, -2,
    0,
    2, 5, 9, 13, 17, 21, 26,
    32,
};
static const int32_t HEIC_INV_ANGLE[15] = {
    -4096, -1638, -910, -630, -482, -390, -315,
    -256,
    -315, -390, -482, -630, -910, -1638, -4096,
};

void heic_fill_mpm(uint8_t cand_a, uint8_t cand_b, uint8_t mpm[3])
{
    if (cand_a == cand_b) {
        if (cand_a < 2) {
            mpm[0] = 0;
            mpm[1] = 1;
            mpm[2] = 26;
        } else {
            uint8_t mode = cand_a;
            uint8_t left = (uint8_t)(2 + ((mode - 2 + 31) % 32));
            uint8_t right = (uint8_t)(2 + ((mode - 2 + 1) % 32));
            mpm[0] = cand_a;
            mpm[1] = left;
            mpm[2] = right;
        }
    } else {
        uint8_t third;
        if (cand_a != 0 && cand_b != 0) third = 0;
        else if (cand_a != 1 && cand_b != 1) third = 1;
        else third = 26;
        mpm[0] = cand_a;
        mpm[1] = cand_b;
        mpm[2] = third;
    }
}

static int32_t inv_angle(uint8_t mode)
{
    if (mode >= 11 && mode <= 25) return HEIC_INV_ANGLE[mode - 11];
    return 0;
}

static inline void plane_put(uint16_t *p, int stride, int x, int y, uint16_t v)
{
    p[(size_t)y * (size_t)stride + (size_t)x] = v;
}

static void ref_subst(int32_t *border, const int *avail, int center, int size,
                      int32_t def)
{
    int first_val = def, found = 0, current, i, idx;
    for (i = (int)(2 * size) - 1; i >= 0; i--) {
        idx = center - 1 - i;
        if (avail[idx]) {
            first_val = border[idx];
            found = 1;
            break;
        }
    }
    if (!found && avail[center]) {
        first_val = border[center];
        found = 1;
    }
    if (!found) {
        for (i = 0; i < 2 * size; i++) {
            idx = center + 1 + i;
            if (avail[idx]) {
                first_val = border[idx];
                break;
            }
        }
    }
    current = first_val;
    for (i = (int)(2 * size) - 1; i >= 0; i--) {
        idx = center - 1 - i;
        if (avail[idx]) current = border[idx];
        else border[idx] = current;
    }
    if (avail[center]) current = border[center];
    else border[center] = current;
    for (i = 0; i < 2 * size; i++) {
        idx = center + 1 + i;
        if (avail[idx]) current = border[idx];
        else border[idx] = current;
    }
}

static void fill_border(heic_frame *frame, uint32_t x, uint32_t y, uint32_t size,
                        uint8_t c_idx, int32_t *border, int center)
{
    uint16_t *plane;
    int stride, plane_n, frame_w, frame_h, avail_left, avail_top, avail_tl;
    int avail[HEIC_BORDER_N];
    uint32_t avail_count = 0, total = 4 * size + 1;
    int32_t def;
    int corner_ok = 0;
    uint32_t i;

    if (c_idx == 0) {
        plane = frame->y;
        stride = frame->y_stride;
        plane_n = frame->width * frame->height;
        frame_w = frame->width;
        frame_h = frame->height;
    } else {
        plane = (c_idx == 1) ? frame->cb : frame->cr;
        stride = frame->c_stride;
        plane_n = frame->c_width * frame->c_height;
        frame_w = frame->c_width;
        frame_h = frame->c_height;
    }
    if (!plane) {
        def = 1 << (frame->bit_depth - 1);
        for (i = 0; i < total; i++) border[center - 2 * (int)size + (int)i] = def;
        return;
    }
    def = 1 << (frame->bit_depth - 1);
    avail_left = x > 0;
    avail_top = y > 0;
    avail_tl = avail_left && avail_top;

    {
        int span = (int)(4 * size + 1);
        int base = center - 2 * (int)size;
        memset(avail + base, 0, (size_t)span * sizeof(int));
    }

    if (avail_tl) {
        uint16_t raw =
            plane[(size_t)(y - 1) * (size_t)stride + (size_t)(x - 1)];
        if (raw != HEIC_UNINIT_SAMPLE) {
            border[center] = raw;
            corner_ok = 1;
            avail_count++;
        }
    }
    if (!corner_ok && avail_top) {
        uint16_t raw = plane[(size_t)(y - 1) * (size_t)stride + (size_t)x];
        if (raw != HEIC_UNINIT_SAMPLE) {
            border[center] = raw;
            corner_ok = 1;
            avail_count++;
        }
    }
    if (!corner_ok && avail_left) {
        uint16_t raw = plane[(size_t)y * (size_t)stride + (size_t)(x - 1)];
        if (raw != HEIC_UNINIT_SAMPLE) {
            border[center] = raw;
            corner_ok = 1;
            avail_count++;
        }
    }
    if (!corner_ok) border[center] = def;
    avail[center] = corner_ok;

    if (avail_top) {
        uint32_t top_count = 2 * size;
        uint32_t n_avail = size;
        const uint16_t *top_row;
        if (top_count > (uint32_t)frame_w - x) top_count = (uint32_t)frame_w - x;
        if (n_avail > top_count) n_avail = top_count;
        top_row = plane + (size_t)(y - 1) * (size_t)stride + (size_t)x;
        if (!heic_simd_u16_to_i32_avail(top_row, &border[center + 1],
                                        &avail[center + 1], (int)n_avail)) {
            for (i = 0; i < n_avail; i++) {
                border[center + 1 + (int)i] = top_row[i];
                avail[center + 1 + (int)i] = 1;
            }
        }
        avail_count += n_avail;

        if (top_count > n_avail) {
            if (!heic_simd_border_top_ext(top_row + n_avail, &border[center + 1 + (int)n_avail],
                                          &avail[center + 1 + (int)n_avail],
                                          (int)(top_count - n_avail))) {
                for (i = n_avail; i < top_count; i++) {
                    uint16_t raw = top_row[i];
                    if (raw != HEIC_UNINIT_SAMPLE) {
                        int idx = center + 1 + (int)i;
                        border[idx] = raw;
                        avail[idx] = 1;
                        avail_count++;
                    }
                }
            } else {

                for (i = n_avail; i < top_count; i++)
                    if (avail[center + 1 + (int)i]) avail_count++;
            }
        }
    }
    if (avail_left) {
        uint32_t left_count = 2 * size;
        uint32_t n_avail = size;
        const uint16_t *left_p;
        if (left_count > (uint32_t)frame_h - y) left_count = (uint32_t)frame_h - y;
        if (n_avail > left_count) n_avail = left_count;
        left_p = plane + (size_t)y * (size_t)stride + (size_t)(x - 1);
        for (i = 0; i < n_avail; i++) {
            border[center - 1 - (int)i] = left_p[(size_t)i * (size_t)stride];
            avail[center - 1 - (int)i] = 1;
            avail_count++;
        }
        for (i = n_avail; i < left_count; i++) {
            uint16_t raw = left_p[(size_t)i * (size_t)stride];
            if (raw != HEIC_UNINIT_SAMPLE) {
                int idx = center - 1 - (int)i;
                border[idx] = raw;
                avail[idx] = 1;
                avail_count++;
            }
        }
    }
    if (avail_count < total)
        ref_subst(border, avail, center, (int)size, def);
    (void)plane_n;
}

static void sample_filter(int32_t *border, int center, int n_t, uint8_t c_idx,
                          uint8_t mode, int strong, int bit_depth)
{
    int filter_flag, bi_int, i;
    int32_t pf[HEIC_BORDER_N];
    int pfc = 2 * HEIC_MAX_INTRA;

    if (mode == 1 || n_t == 4) filter_flag = 0;
    else {
        int mdvh = abs((int)mode - 26);
        int mdhh = abs((int)mode - 10);
        int min_d = mdvh < mdhh ? mdvh : mdhh;
        if (n_t == 8) filter_flag = min_d > 7;
        else if (n_t == 16) filter_flag = min_d > 1;
        else if (n_t == 32) filter_flag = min_d > 0;
        else filter_flag = 0;
    }
    if (!filter_flag) return;

    bi_int = strong && c_idx == 0 && n_t == 32
             && abs(border[center] + border[center + 64] - 2 * border[center + 32])
                    < (1 << (bit_depth - 5))
             && abs(border[center] + border[center - 64] - 2 * border[center - 32])
                    < (1 << (bit_depth - 5));

    if (bi_int) {
        int32_t p0 = border[center];
        int32_t p_neg = border[center - 64];
        int32_t p_pos = border[center + 64];
        pf[pfc - 2 * n_t] = border[center - 2 * n_t];
        pf[pfc + 2 * n_t] = border[center + 2 * n_t];
        pf[pfc] = border[center];
        for (i = 1; i < 64; i++) {
            pf[pfc - i] = p0 + ((i * (p_neg - p0) + 32) >> 6);
            pf[pfc + i] = p0 + ((i * (p_pos - p0) + 32) >> 6);
        }
    } else {
        pf[pfc - 2 * n_t] = border[center - 2 * n_t];
        pf[pfc + 2 * n_t] = border[center + 2 * n_t];
        for (i = -(2 * n_t - 1); i <= (2 * n_t - 1); i++) {
            int idx = center + i;
            pf[pfc + i] = (border[idx + 1] + 2 * border[idx] + border[idx - 1] + 2) >> 2;
        }
    }
    for (i = 0; i <= 4 * n_t; i++)
        border[center - 2 * n_t + i] = pf[pfc - 2 * n_t + i];
}

static void predict_planar(uint16_t *plane, int stride, uint32_t x, uint32_t y,
                           uint32_t size, uint8_t log2_size, int max_val,
                           const int32_t *border, int center)
{
    int n = (int)size, px, py;
    int32_t right = border[center + 1 + n];
    int32_t bottom = border[center - 1 - n];
    for (py = 0; py < n; py++) {
        int32_t left = border[center - 1 - py];
        uint16_t *dst = plane + ((size_t)y + (size_t)py) * (size_t)stride + (size_t)x;
        for (px = 0; px < n; px++) {
            int32_t top = border[center + 1 + px];
            int32_t pred = ((n - 1 - px) * left + (px + 1) * right
                            + (n - 1 - py) * top + (py + 1) * bottom + n)
                           >> (log2_size + 1);
            if (pred < 0) pred = 0;
            if (pred > max_val) pred = max_val;
            dst[px] = (uint16_t)pred;
        }
    }
}

static void predict_dc(uint16_t *plane, int stride, uint32_t x, uint32_t y,
                       uint32_t size, uint8_t log2_size, uint8_t c_idx, int max_val,
                       const int32_t *border, int center)
{
    int n = (int)size;
    int32_t dc = 0;
    int i, px, py;
    uint16_t dcu;
    for (i = 0; i < n; i++) {
        dc += border[center + 1 + i];
        dc += border[center - 1 - i];
    }
    dc = (dc + n) >> (log2_size + 1);
    if (dc < 0) dc = 0;
    if (dc > max_val) dc = max_val;
    dcu = (uint16_t)dc;

    if (c_idx == 0 && size < 32) {
        int32_t corner = (border[center - 1] + 2 * dc + border[center + 1] + 2) >> 2;
        if (corner < 0) corner = 0;
        if (corner > max_val) corner = max_val;
        plane_put(plane, stride, (int)x, (int)y, (uint16_t)corner);
        for (px = 1; px < n; px++) {
            int32_t pred = (border[center + 1 + px] + 3 * dc + 2) >> 2;
            if (pred < 0) pred = 0;
            if (pred > max_val) pred = max_val;
            plane_put(plane, stride, (int)x + px, (int)y, (uint16_t)pred);
        }
        for (py = 1; py < n; py++) {
            int32_t pred = (border[center - 1 - py] + 3 * dc + 2) >> 2;
            if (pred < 0) pred = 0;
            if (pred > max_val) pred = max_val;
            plane_put(plane, stride, (int)x, (int)y + py, (uint16_t)pred);
        }
        for (py = 1; py < n; py++) {
            uint16_t *dst =
                plane + ((size_t)y + (size_t)py) * (size_t)stride + (size_t)x + 1;
            for (px = 1; px < n; px++) dst[px - 1] = dcu;
        }
    } else {
        for (py = 0; py < n; py++) {
            uint16_t *dst =
                plane + ((size_t)y + (size_t)py) * (size_t)stride + (size_t)x;
            for (px = 0; px < n; px++) dst[px] = dcu;
        }
    }
}

static void predict_angular(uint16_t *plane, int stride, uint32_t x, uint32_t y,
                            uint32_t size, uint8_t c_idx, uint8_t mode, int max_val,
                            const int32_t *border, int center)
{
    int n = (int)size;
    int32_t angle = HEIC_INTRA_ANGLE[mode];
    int32_t ref_arr[HEIC_BORDER_N];
    int rc = 2 * HEIC_MAX_INTRA;
    int px, py;

    if (mode >= 18) {
        int i;
        for (i = 0; i <= n; i++)
            ref_arr[rc + i] = border[center + i];
        if (angle < 0) {
            int32_t inv = inv_angle(mode);
            int32_t ext = (n * angle) >> 5;
            int32_t xx;
            if (ext < -1) {
                for (xx = ext; xx <= -1; xx++) {
                    int32_t idx = (xx * inv + 128) >> 8;
                    if (idx >= 0 && idx <= 2 * n)
                        ref_arr[rc + (int)xx] = border[center - (int)idx];
                }
            }
        } else {
            for (i = 0; i < n; i++)
                ref_arr[rc + n + 1 + i] = border[center + n + 1 + i];
        }
        for (py = 0; py < n; py++) {
            int32_t i_idx = ((py + 1) * angle) >> 5;
            int32_t i_fact = ((py + 1) * angle) & 31;
            int base = rc + (int)i_idx + 1;
            uint16_t *dst =
                plane + ((size_t)y + (size_t)py) * (size_t)stride + (size_t)x;
            if (i_fact != 0 &&
                heic_simd_intra_ang_row(dst, &ref_arr[base], n, 32 - (int)i_fact,
                                        (int)i_fact, max_val))
                continue;
            for (px = 0; px < n; px++) {
                int32_t pred;
                if (i_fact != 0)
                    pred = ((32 - i_fact) * ref_arr[base + px]
                            + i_fact * ref_arr[base + px + 1] + 16)
                           >> 5;
                else
                    pred = ref_arr[base + px];
                if (pred < 0) pred = 0;
                if (pred > max_val) pred = max_val;
                dst[px] = (uint16_t)pred;
            }
        }
        if (mode == 26 && c_idx == 0 && size < 32) {
            for (py = 0; py < n; py++) {
                int32_t pred = border[center + 1]
                               + ((border[center - 1 - py] - border[center]) >> 1);
                if (pred < 0) pred = 0;
                if (pred > max_val) pred = max_val;
                plane_put(plane, stride, (int)x, (int)y + py, (uint16_t)pred);
            }
        }
    } else {
        int i;
        for (i = 0; i <= n; i++)
            ref_arr[rc + i] = border[center - i];
        if (angle < 0) {
            int32_t inv = inv_angle(mode);
            int32_t ext = (n * angle) >> 5;
            int32_t xx;
            if (ext < -1) {
                for (xx = ext; xx <= -1; xx++) {
                    int32_t idx = (xx * inv + 128) >> 8;
                    if (idx >= 0 && idx <= 2 * n)
                        ref_arr[rc + (int)xx] = border[center + (int)idx];
                }
            }
        } else {
            for (i = n + 1; i <= 2 * n; i++)
                ref_arr[rc + i] = border[center - i];
        }
        for (py = 0; py < n; py++) {
            int row_base = rc + py + 1;
            uint16_t *dst =
                plane + ((size_t)y + (size_t)py) * (size_t)stride + (size_t)x;
            if (heic_simd_intra_ang_row_var(dst, ref_arr, n, row_base, angle, max_val))
                continue;
            for (px = 0; px < n; px++) {
                int32_t i_idx = ((px + 1) * angle) >> 5;
                int32_t i_fact = ((px + 1) * angle) & 31;
                int idx = row_base + (int)i_idx;
                int32_t pred;
                if (i_fact != 0)
                    pred = ((32 - i_fact) * ref_arr[idx] + i_fact * ref_arr[idx + 1] + 16) >> 5;
                else
                    pred = ref_arr[idx];
                if (pred < 0) pred = 0;
                if (pred > max_val) pred = max_val;
                dst[px] = (uint16_t)pred;
            }
        }
        if (mode == 10 && c_idx == 0 && size < 32) {
            for (px = 0; px < n; px++) {
                int32_t pred = border[center - 1]
                               + ((border[center + 1 + px] - border[center]) >> 1);
                if (pred < 0) pred = 0;
                if (pred > max_val) pred = max_val;
                plane_put(plane, stride, (int)x + px, (int)y, (uint16_t)pred);
            }
        }
    }
}

int heic_predict_intra(heic_frame *frame, uint32_t x, uint32_t y,
                       uint8_t log2_size, uint8_t mode, uint8_t c_idx,
                       int strong_intra_smoothing)
{
    uint32_t size;
    int32_t border[HEIC_BORDER_N];
    int center = 2 * HEIC_MAX_INTRA;
    uint16_t *plane;
    int stride, plane_n, max_val;

    if (!frame || log2_size > 5) return -1;
    size = 1u << log2_size;
    memset(border, 0, sizeof(border));
    fill_border(frame, x, y, size, c_idx, border, center);

    if (c_idx == 0 || frame->chroma_format == 3)
        sample_filter(border, center, (int)size, c_idx, mode,
                      strong_intra_smoothing, frame->bit_depth);

    if (c_idx == 0) {
        plane = frame->y;
        stride = frame->y_stride;
        plane_n = frame->width * frame->height;
    } else {
        plane = (c_idx == 1) ? frame->cb : frame->cr;
        stride = frame->c_stride;
        plane_n = frame->c_width * frame->c_height;
    }
    if (!plane) return 0;

    {
        size_t last;
        if (size == 0 || stride <= 0) return -1;
        last = ((size_t)y + (size_t)size - 1) * (size_t)stride + (size_t)x + (size_t)size;
        if (last > (size_t)plane_n) return -1;
    }
    max_val = (1 << frame->bit_depth) - 1;

    if (mode == 0)
        predict_planar(plane, stride, x, y, size, log2_size, max_val, border, center);
    else if (mode == 1)
        predict_dc(plane, stride, x, y, size, log2_size, c_idx, max_val, border, center);
    else
        predict_angular(plane, stride, x, y, size, c_idx, mode, max_val, border, center);
    return 0;
}

typedef struct {
    heic_ctx *hctx;
    const heic_sps *sps;
    const heic_pps *pps;
    const heic_slice_header *sh;
    heic_cabac cabac;
    heic_ctx_model models[HEIC_NUM_CONTEXTS];
    heic_frame *frame;

    uint32_t ctb_x, ctb_y;
    int qp_y, qp_cb, qp_cr;
    int is_cu_qp_delta_coded;
    int cu_qp_delta;
    int cu_transquant_bypass;
    uint32_t cu_base_x, cu_base_y;
    uint8_t cu_log2_size;

    uint8_t *ct_depth_map;
    uint32_t ct_depth_stride;
    size_t   ct_depth_n;
    uint8_t *intra_mode_map;
    uint8_t *intra_chroma_mode_map;
    uint32_t intra_mode_stride;
    size_t   intra_mode_n;
    int8_t *qp_map;
    uint32_t qp_map_stride;
    size_t   qp_map_n;
    int current_qpy;
    int last_qpy_in_prev_qg;
    int current_qg_x, current_qg_y;

    heic_sao_info *sao_map;
    uint32_t sao_stride;

    uint8_t *deblock_flags;
    int8_t  *deblock_qp;
    uint32_t deblock_stride;
    uint32_t deblock_n;

    int16_t *residual_buf;
    heic_coeff_buf *coeff;
} heic_slice_ctx;

static int bit_depth_y(const heic_sps *s) { return 8 + s->bit_depth_luma_minus8; }
static int bit_depth_c(const heic_sps *s) { return 8 + s->bit_depth_chroma_minus8; }
static uint32_t ctb_size_px(const heic_sps *s) { return 1u << s->log2_ctb_size; }
static uint32_t min_pu_size(const heic_sps *s)
{
    uint32_t m = (1u << s->log2_min_cb_size) / 2;
    return m ? m : 1;
}

static int chroma_array_type(const heic_sps *s)
{
    return s->separate_colour_plane_flag ? 0 : (int)s->chroma_format_idc;
}

static int chroma_qp_from_luma(int qpi, int chroma_array_type)
{
    static const int TAB[13] = {29, 30, 31, 32, 33, 33, 34, 34, 35, 35, 36, 36, 37};
    if (chroma_array_type != 1) {
        if (qpi < 0) return 0;
        if (qpi > 51) return 51;
        return qpi;
    }
    if (qpi < 30) return qpi;
    if (qpi >= 43) return qpi - 6;
    return TAB[qpi - 30];
}

static int neighbor_avail(const heic_slice_ctx *sc, int32_t x, int32_t y)
{
    if (x < 0 || y < 0) return 0;
    if ((uint32_t)x >= sc->sps->pic_width_in_luma_samples) return 0;
    if ((uint32_t)y >= sc->sps->pic_height_in_luma_samples) return 0;
    return 1;
}

static uint8_t get_ct_depth(const heic_slice_ctx *sc, uint32_t x, uint32_t y)
{
    uint32_t min_cb = 1u << sc->sps->log2_min_cb_size;
    uint32_t mx = x / min_cb, my = y / min_cb;
    size_t idx;
    if (mx >= sc->ct_depth_stride) return 0xFF;
    idx = (size_t)my * sc->ct_depth_stride + mx;
    if (idx >= sc->ct_depth_n) return 0xFF;
    return sc->ct_depth_map[idx];
}

static void set_ct_depth(heic_slice_ctx *sc, uint32_t x0, uint32_t y0,
                        uint8_t log2_cb, uint8_t depth)
{
    uint32_t min_cb = 1u << sc->sps->log2_min_cb_size;
    uint32_t cb = 1u << log2_cb;
    uint32_t sx = x0 / min_cb, sy = y0 / min_cb, n = cb / min_cb, dx, dy;
    for (dy = 0; dy < n; dy++)
        for (dx = 0; dx < n; dx++) {
            uint32_t mx = sx + dx, my = sy + dy;
            size_t idx;
            if (mx >= sc->ct_depth_stride) continue;
            idx = (size_t)my * sc->ct_depth_stride + mx;
            if (idx >= sc->ct_depth_n) continue;
            sc->ct_depth_map[idx] = depth;
        }
}

static void store_intra_mode(heic_slice_ctx *sc, uint32_t x0, uint32_t y0,
                             uint8_t log2_size, uint8_t mode, int chroma)
{
    uint32_t mpu = min_pu_size(sc->sps);
    uint32_t count = ((1u << log2_size) / mpu);
    uint32_t sx = x0 / mpu, sy = y0 / mpu, dx, dy;
    uint8_t *map = chroma ? sc->intra_chroma_mode_map : sc->intra_mode_map;
    if (count == 0) count = 1;
    for (dy = 0; dy < count; dy++)
        for (dx = 0; dx < count; dx++) {
            size_t idx = (size_t)(sy + dy) * sc->intra_mode_stride + (sx + dx);

            if (idx < sc->intra_mode_n) map[idx] = mode;
        }
}

static uint8_t get_intra_mode(const heic_slice_ctx *sc, uint32_t x, uint32_t y, int chroma)
{
    uint32_t mpu = min_pu_size(sc->sps);
    size_t idx = (size_t)(y / mpu) * sc->intra_mode_stride + (x / mpu);
    const uint8_t *map = chroma ? sc->intra_chroma_mode_map : sc->intra_mode_map;
    if (idx >= sc->intra_mode_n) return 1;
    return map[idx];
}

static uint8_t neighbor_intra_left(const heic_slice_ctx *sc, uint32_t x0, uint32_t y0)
{
    if (x0 == 0) return 1;
    return get_intra_mode(sc, x0 - 1, y0, 0);
}

static uint8_t neighbor_intra_above(const heic_slice_ctx *sc, uint32_t x0, uint32_t y0)
{
    uint32_t ctb = ctb_size_px(sc->sps);
    uint32_t ctb_y0;
    if (y0 == 0) return 1;
    ctb_y0 = (y0 / ctb) * ctb;
    if (y0 - 1 < ctb_y0) return 1;
    return get_intra_mode(sc, x0, y0 - 1, 0);
}

static void store_qpy(heic_slice_ctx *sc, uint32_t x0, uint32_t y0,
                      uint8_t log2_cb, int qpy)
{
    uint32_t min_tb = 1u << sc->sps->log2_min_tb_size;
    uint32_t count = ((1u << log2_cb) / min_tb);
    uint32_t sx = x0 / min_tb, sy = y0 / min_tb, dx, dy;
    if (count == 0) count = 1;
    for (dy = 0; dy < count; dy++)
        for (dx = 0; dx < count; dx++) {
            size_t idx = (size_t)(sy + dy) * sc->qp_map_stride + (sx + dx);

            if (idx < sc->qp_map_n) sc->qp_map[idx] = (int8_t)qpy;
        }
}

static int get_qpy_at(const heic_slice_ctx *sc, uint32_t x, uint32_t y)
{
    uint32_t min_tb = 1u << sc->sps->log2_min_tb_size;
    size_t idx = (size_t)(y / min_tb) * sc->qp_map_stride + (x / min_tb);
    if (idx >= sc->qp_map_n) return sc->sh->slice_qp_y;
    return sc->qp_map[idx];
}

static void decode_quant_params(heic_slice_ctx *sc, uint32_t x0,
                                uint32_t x_cu, uint32_t y_cu)
{

    uint8_t log2_min_qg =
        sc->sps->log2_ctb_size > sc->pps->diff_cu_qp_delta_depth
            ? (uint8_t)(sc->sps->log2_ctb_size - sc->pps->diff_cu_qp_delta_depth)
            : 0;
    uint32_t qg_mask = (1u << log2_min_qg) - 1;
    int x_qg = (int)(x_cu & ~qg_mask);
    int y_qg = (int)(y_cu & ~qg_mask);
    int ctb_mask = (int)((1u << sc->sps->log2_ctb_size) - 1);
    int first_in_ctb_row, first_qg_in_slice;
    int qp_y_pred, qp_y_a, qp_y_b, qp_bd_y, qp_bd_c, qpy, qpi_cb, qpi_cr;
    uint32_t slice_sx, slice_sy;

    if (x_qg != sc->current_qg_x || y_qg != sc->current_qg_y) {
        sc->last_qpy_in_prev_qg = sc->current_qpy;
        sc->current_qg_x = x_qg;
        sc->current_qg_y = y_qg;
    }

    first_in_ctb_row = (x_qg == 0 && (y_qg & ctb_mask) == 0);
    slice_sx = (sc->sh->slice_segment_address % sc->sps->pic_width_in_ctbs)
               * ctb_size_px(sc->sps);
    slice_sy = (sc->sh->slice_segment_address / sc->sps->pic_width_in_ctbs)
               * ctb_size_px(sc->sps);
    first_qg_in_slice = ((int)slice_sx == x_qg && (int)slice_sy == y_qg);

    if (first_qg_in_slice
        || (first_in_ctb_row && sc->pps->entropy_coding_sync_enabled_flag))
        qp_y_pred = sc->sh->slice_qp_y;
    else
        qp_y_pred = sc->last_qpy_in_prev_qg;

    if (x_qg > 0) {
        uint32_t lx = (uint32_t)(x_qg - 1), ly = (uint32_t)y_qg;
        uint32_t ctb = ctb_size_px(sc->sps);
        if (lx / ctb == sc->ctb_x)
            qp_y_a = get_qpy_at(sc, lx, ly);
        else
            qp_y_a = qp_y_pred;
        (void)x0;
    } else {
        qp_y_a = qp_y_pred;
    }
    if (y_qg > 0) {
        uint32_t ax = (uint32_t)x_qg, ay = (uint32_t)(y_qg - 1);
        uint32_t ctb = ctb_size_px(sc->sps);
        if (ay / ctb == sc->ctb_y)
            qp_y_b = get_qpy_at(sc, ax, ay);
        else
            qp_y_b = qp_y_pred;
    } else {
        qp_y_b = qp_y_pred;
    }
    qp_y_pred = (qp_y_a + qp_y_b + 1) >> 1;

    qp_bd_y = 6 * (bit_depth_y(sc->sps) - 8);
    qpy = ((qp_y_pred + sc->cu_qp_delta + 52 + 2 * qp_bd_y) % (52 + qp_bd_y))
          - qp_bd_y;
    sc->qp_y = qpy + qp_bd_y;
    if (sc->qp_y < 0) sc->qp_y = 0;

    qp_bd_c = 6 * (bit_depth_c(sc->sps) - 8);
    qpi_cb = qpy + sc->pps->pps_cb_qp_offset + sc->sh->slice_cb_qp_offset;
    qpi_cr = qpy + sc->pps->pps_cr_qp_offset + sc->sh->slice_cr_qp_offset;
    if (qpi_cb < -qp_bd_c) qpi_cb = -qp_bd_c;
    if (qpi_cb > 57) qpi_cb = 57;
    if (qpi_cr < -qp_bd_c) qpi_cr = -qp_bd_c;
    if (qpi_cr > 57) qpi_cr = 57;
    {
        int cat = chroma_array_type(sc->sps);
        sc->qp_cb = chroma_qp_from_luma(qpi_cb, cat) + qp_bd_c;
        sc->qp_cr = chroma_qp_from_luma(qpi_cr, cat) + qp_bd_c;
    }
    sc->current_qpy = qpy;
}

static int decode_split_cu(heic_slice_ctx *sc, uint32_t x0, uint32_t y0, uint8_t ct_depth)
{
    int cond_l = 0, cond_a = 0, ctx_idx, bin;
    if (neighbor_avail(sc, (int32_t)x0 - 1, (int32_t)y0)) {
        uint8_t d = get_ct_depth(sc, x0 - 1, y0);
        if (d != 0xFF && d > ct_depth) cond_l = 1;
    }
    if (neighbor_avail(sc, (int32_t)x0, (int32_t)y0 - 1)) {
        uint8_t d = get_ct_depth(sc, x0, y0 - 1);
        if (d != 0xFF && d > ct_depth) cond_a = 1;
    }
    ctx_idx = HEIC_CTX_SPLIT_CU_FLAG + cond_l + cond_a;
    bin = heic_cabac_decode_bin(&sc->cabac, &sc->models[ctx_idx]);
    return bin != 0;
}

static int decode_part_mode_intra(heic_slice_ctx *sc, uint8_t log2_cb)
{
    int bin = heic_cabac_decode_bin(&sc->cabac, &sc->models[HEIC_CTX_PART_MODE]);
    if (bin != 0) return 0;
    if (log2_cb == sc->sps->log2_min_cb_size) return 1;
    return -1;
}

static int decode_prev_intra_flag(heic_slice_ctx *sc)
{
    return heic_cabac_decode_bin(&sc->cabac,
                                 &sc->models[HEIC_CTX_PREV_INTRA_LUMA_PRED_FLAG])
           != 0;
}

static uint8_t decode_mpm_idx(heic_slice_ctx *sc)
{
    if (heic_cabac_decode_bypass(&sc->cabac) == 0) return 0;
    if (heic_cabac_decode_bypass(&sc->cabac) == 0) return 1;
    return 2;
}

static uint32_t decode_rem_intra(heic_slice_ctx *sc)
{
    uint32_t v = 0;
    int i;
    for (i = 0; i < 5; i++)
        v = (v << 1) | (uint32_t)heic_cabac_decode_bypass(&sc->cabac);
    return v;
}

static uint8_t map_rem_mode(uint32_t rem, const uint8_t mpm[3])
{
    uint8_t sorted[3] = {mpm[0], mpm[1], mpm[2]};
    uint8_t mode;
    int i, j;
    for (i = 0; i < 2; i++)
        for (j = i + 1; j < 3; j++)
            if (sorted[i] > sorted[j]) {
                uint8_t t = sorted[i];
                sorted[i] = sorted[j];
                sorted[j] = t;
            }
    mode = (uint8_t)rem;
    for (i = 0; i < 3; i++)
        if (mode >= sorted[i]) mode++;
    return mode;
}

static uint8_t derive_intra_luma(heic_slice_ctx *sc, uint32_t x0, uint32_t y0,
                                 int prev_flag)
{
    uint8_t cand_a = neighbor_intra_left(sc, x0, y0);
    uint8_t cand_b = neighbor_intra_above(sc, x0, y0);
    uint8_t mpm[3];
    heic_fill_mpm(cand_a, cand_b, mpm);
    if (prev_flag) return mpm[decode_mpm_idx(sc)];
    return map_rem_mode(decode_rem_intra(sc), mpm);
}

static uint8_t decode_intra_chroma(heic_slice_ctx *sc, uint8_t luma)
{
    int first;
    uint32_t mode_idx;
    uint8_t cand;
    if (chroma_array_type(sc->sps) == 0) return luma;
    first = heic_cabac_decode_bin(&sc->cabac,
                                  &sc->models[HEIC_CTX_INTRA_CHROMA_PRED_MODE]);
    if (first == 0) return luma;
    mode_idx = heic_cabac_decode_bypass_bits(&sc->cabac, 2);
    if (mode_idx == 0) cand = 0;
    else if (mode_idx == 1) cand = 26;
    else if (mode_idx == 2) cand = 10;
    else cand = 1;
    if (cand == luma) return 34;
    return cand;
}

static uint32_t decode_cu_qp_delta_abs(heic_slice_ctx *sc)
{
    int first = heic_cabac_decode_bin(&sc->cabac, &sc->models[HEIC_CTX_CU_QP_DELTA_ABS]);
    uint32_t prefix;
    int i;
    if (first == 0) return 0;
    prefix = 1;
    for (i = 0; i < 4; i++) {
        int bin = heic_cabac_decode_bin(&sc->cabac, &sc->models[HEIC_CTX_CU_QP_DELTA_ABS + 1]);
        if (bin == 0) break;
        prefix++;
    }
    if (prefix == 5) {
        uint32_t suffix = heic_cabac_decode_egk(&sc->cabac, 0);
        return suffix + 5;
    }
    return prefix;
}

static int decode_and_apply_residual(heic_slice_ctx *sc, uint32_t x0, uint32_t y0,
                                     uint8_t log2_size, uint8_t c_idx, int scan_order)
{
    heic_coeff_buf *coeff = sc->coeff;
    int transform_skip = 0;
    int size, num, qp, bd, is_intra_4x4, max_val;
    uint16_t *plane;
    int stride, plane_w, plane_h;

    if (!coeff) return -1;
    if (heic_decode_residual(&sc->cabac, sc->models, log2_size, c_idx, scan_order,
                             sc->pps->sign_data_hiding_enabled_flag,
                             sc->cu_transquant_bypass,
                             sc->pps->transform_skip_enabled_flag,
                             coeff, &transform_skip)
        != 0)
        return -1;
    if (coeff->num_nonzero == 0) return 0;

    size = 1 << log2_size;
    num = size * size;
    if (c_idx == 0) {
        qp = sc->qp_y;
        bd = bit_depth_y(sc->sps);
        plane = sc->frame->y;
        stride = sc->frame->y_stride;
        plane_w = sc->frame->width;
        plane_h = sc->frame->height;
    } else if (c_idx == 1) {
        qp = sc->qp_cb;
        bd = bit_depth_c(sc->sps);
        plane = sc->frame->cb;
        stride = sc->frame->c_stride;
        plane_w = sc->frame->c_width;
        plane_h = sc->frame->c_height;
    } else {
        qp = sc->qp_cr;
        bd = bit_depth_c(sc->sps);
        plane = sc->frame->cr;
        stride = sc->frame->c_stride;
        plane_w = sc->frame->c_width;
        plane_h = sc->frame->c_height;
    }
    if (!plane) return 0;
    max_val = (1 << bd) - 1;

    if (sc->cu_transquant_bypass) {
        int py, px;
        memcpy(sc->residual_buf, coeff->coeffs, (size_t)num * sizeof(int16_t));
        for (py = 0; py < size; py++) {
            if ((int)y0 + py >= plane_h) break;
            for (px = 0; px < size; px++) {
                int32_t v;
                if ((int)x0 + px >= plane_w) break;
                v = (int32_t)plane[((int)y0 + py) * stride + (int)x0 + px]
                    + sc->residual_buf[py * size + px];
                if (v < 0) v = 0;
                if (v > max_val) v = max_val;
                plane[((int)y0 + py) * stride + (int)x0 + px] = (uint16_t)v;
            }
        }
        return 0;
    }

    heic_dequantize(coeff->coeffs, num, qp, bd, log2_size);

    if (transform_skip) {
        int ts_shift = 5 + (int)log2_size;
        int bd_shift = 20 - bd;
        int rnd, i;
        if (bd_shift < 0) bd_shift = 0;
        rnd = bd_shift > 0 ? (1 << (bd_shift - 1)) : 0;
        for (i = 0; i < num; i++) {
            int32_t c = ((int32_t)coeff->coeffs[i] << ts_shift);
            sc->residual_buf[i] = (int16_t)((c + rnd) >> bd_shift);
        }
    } else {
        is_intra_4x4 = (log2_size == 2 && c_idx == 0);
        heic_inverse_transform(coeff->coeffs, sc->residual_buf, size, bd, is_intra_4x4);
    }

    if ((int)x0 + size <= plane_w && (int)y0 + size <= plane_h) {
        heic_add_residual(plane, stride, (int)x0, (int)y0, sc->residual_buf, size,
                          max_val);
    } else {
        int py, px;
        for (py = 0; py < size; py++) {
            if ((int)y0 + py >= plane_h) break;
            for (px = 0; px < size; px++) {
                int32_t v;
                if ((int)x0 + px >= plane_w) break;
                v = (int32_t)plane[((int)y0 + py) * stride + (int)x0 + px]
                    + sc->residual_buf[py * size + px];
                if (v < 0) v = 0;
                if (v > max_val) v = max_val;
                plane[((int)y0 + py) * stride + (int)x0 + px] = (uint16_t)v;
            }
        }
    }
    return 0;
}

static int decode_tu_leaf(heic_slice_ctx *sc, uint32_t x0, uint32_t y0,
                          uint8_t log2_size, uint8_t trafo_depth, int cbf_cb,
                          int cbf_cr);
static int decode_tt_inner(heic_slice_ctx *sc, uint32_t x0, uint32_t y0,
                           uint8_t log2_size, uint8_t trafo_depth,
                           int intra_split, int cbf_cb_parent, int cbf_cr_parent);

static int decode_tu_leaf(heic_slice_ctx *sc, uint32_t x0, uint32_t y0,
                          uint8_t log2_size, uint8_t trafo_depth, int cbf_cb,
                          int cbf_cr)
{
    int cbf_luma, sis, scan, chroma_here, is_444;
    uint8_t luma_mode, chroma_mode;
    int ctx_off, ctx_idx;

    ctx_off = trafo_depth == 0 ? 1 : 0;
    ctx_idx = HEIC_CTX_CBF_LUMA + ctx_off;
    cbf_luma = heic_cabac_decode_bin(&sc->cabac, &sc->models[ctx_idx]) != 0;

    if ((cbf_luma || cbf_cb || cbf_cr) && sc->pps->cu_qp_delta_enabled_flag
        && !sc->is_cu_qp_delta_coded) {
        uint32_t absd = decode_cu_qp_delta_abs(sc);
        int sign = 0;
        int64_t delta, qp_bd;
        if (absd != 0) sign = heic_cabac_decode_bypass(&sc->cabac);
        sc->is_cu_qp_delta_coded = 1;
        delta = (int64_t)absd * (1 - 2 * (int64_t)sign);
        qp_bd = 6 * ((int64_t)bit_depth_y(sc->sps) - 8);
        if (delta < -(26 + qp_bd / 2) || delta > 25 + qp_bd / 2) return -1;
        sc->cu_qp_delta = (int)delta;
        decode_quant_params(sc, x0, sc->cu_base_x, sc->cu_base_y);
        store_qpy(sc, sc->cu_base_x, sc->cu_base_y, sc->cu_log2_size, sc->current_qpy);
    }

    luma_mode = get_intra_mode(sc, x0, y0, 0);
    sis = sc->sps->strong_intra_smoothing_enabled_flag;
    if (heic_predict_intra(sc->frame, x0, y0, log2_size, luma_mode, 0, sis) != 0)
        return -1;
    scan = heic_get_scan_order(log2_size, luma_mode, 0, 0);
    if (cbf_luma) {
        if (decode_and_apply_residual(sc, x0, y0, log2_size, 0, scan) != 0)
            return -1;
    }

    {
        uint32_t tu_size = 1u << log2_size;
        heic_mark_tu_boundary(sc->deblock_flags, sc->deblock_stride, sc->deblock_n,
                              x0, y0, tu_size);
        heic_store_deblock_qp(sc->deblock_qp, sc->deblock_stride, sc->deblock_n,
                              x0, y0, tu_size, (int8_t)sc->current_qpy);
    }

    is_444 = sc->frame->chroma_format == 3;
    chroma_here = is_444 || log2_size >= 3;
    if (chroma_here && chroma_array_type(sc->sps) != 0) {
        uint8_t clog2;
        uint32_t cx, cy;
        int cscan;
        if (is_444) {
            clog2 = log2_size;
            cx = x0;
            cy = y0;
        } else {
            clog2 = (uint8_t)(log2_size - 1);
            cx = x0 / 2;
            cy = y0 / 2;
        }
        chroma_mode = get_intra_mode(sc, x0, y0, 1);
        cscan = heic_get_scan_order(clog2, chroma_mode, 1, is_444);
        if (heic_predict_intra(sc->frame, cx, cy, clog2, chroma_mode, 1, sis) != 0)
            return -1;
        if (cbf_cb) {
            if (decode_and_apply_residual(sc, cx, cy, clog2, 1, cscan) != 0)
                return -1;
        }
        if (heic_predict_intra(sc->frame, cx, cy, clog2, chroma_mode, 2, sis) != 0)
            return -1;
        if (cbf_cr) {
            if (decode_and_apply_residual(sc, cx, cy, clog2, 2, cscan) != 0)
                return -1;
        }
    }
    return 0;
}

static int decode_tt_inner(heic_slice_ctx *sc, uint32_t x0, uint32_t y0,
                           uint8_t log2_size, uint8_t trafo_depth,
                           int intra_split, int cbf_cb_parent, int cbf_cr_parent)
{
    uint8_t max_depth = (uint8_t)(sc->sps->max_transform_hierarchy_depth_intra
                                  + (intra_split ? 1 : 0));
    uint8_t log2_min = sc->sps->log2_min_tb_size;
    uint8_t log2_max = sc->sps->log2_max_tb_size;
    int split, cbf_cb, cbf_cr, cat;

    if (log2_size < 2 || log2_size > 6 || trafo_depth > 8) return -1;
    if (log2_min < 2) log2_min = 2;
    if (log2_max > 5) log2_max = 5;
    if (log2_max < log2_min) log2_max = log2_min;

    if (log2_size <= log2_max && log2_size > log2_min && trafo_depth < max_depth
        && !(intra_split && trafo_depth == 0)) {
        int ctx = HEIC_CTX_SPLIT_TRANSFORM_FLAG
                  + ((5 - (int)log2_size) < 2 ? (5 - (int)log2_size) : 2);
        if (ctx < HEIC_CTX_SPLIT_TRANSFORM_FLAG) ctx = HEIC_CTX_SPLIT_TRANSFORM_FLAG;
        split = heic_cabac_decode_bin(&sc->cabac, &sc->models[ctx]) != 0;
    } else if ((log2_size > log2_max || (intra_split && trafo_depth == 0))
               && log2_size > log2_min) {

        split = 1;
    } else {
        split = 0;
    }

    cat = chroma_array_type(sc->sps);
    if (cat == 0) {
        cbf_cb = 0;
        cbf_cr = 0;
    } else if (log2_size > 2 || cat == 3) {
        if (trafo_depth == 0 || cbf_cb_parent) {
            int ctx = HEIC_CTX_CBF_CBCR + trafo_depth;
            cbf_cb = heic_cabac_decode_bin(&sc->cabac, &sc->models[ctx]) != 0;
        } else
            cbf_cb = 0;
        if (trafo_depth == 0 || cbf_cr_parent) {
            int ctx = HEIC_CTX_CBF_CBCR + trafo_depth;
            cbf_cr = heic_cabac_decode_bin(&sc->cabac, &sc->models[ctx]) != 0;
        } else
            cbf_cr = 0;
    } else {
        cbf_cb = cbf_cb_parent;
        cbf_cr = cbf_cr_parent;
    }

    if (split) {
        uint32_t half = 1u << (log2_size - 1);
        uint8_t nd = (uint8_t)(trafo_depth + 1);
        uint8_t nl = (uint8_t)(log2_size - 1);
        if (decode_tt_inner(sc, x0, y0, nl, nd, intra_split, cbf_cb, cbf_cr) != 0)
            return -1;
        if (decode_tt_inner(sc, x0 + half, y0, nl, nd, intra_split, cbf_cb, cbf_cr) != 0)
            return -1;
        if (decode_tt_inner(sc, x0, y0 + half, nl, nd, intra_split, cbf_cb, cbf_cr) != 0)
            return -1;
        if (decode_tt_inner(sc, x0 + half, y0 + half, nl, nd, intra_split, cbf_cb, cbf_cr)
            != 0)
            return -1;

        if (log2_size == 3 && sc->frame->chroma_format != 3 && cat != 0) {
            int sis = sc->sps->strong_intra_smoothing_enabled_flag;
            uint8_t cm = get_intra_mode(sc, x0, y0, 1);
            int scan = heic_get_scan_order(2, cm, 1, 0);
            if (heic_predict_intra(sc->frame, x0 / 2, y0 / 2, 2, cm, 1, sis) != 0)
                return -1;
            if (cbf_cb
                && decode_and_apply_residual(sc, x0 / 2, y0 / 2, 2, 1, scan) != 0)
                return -1;
            if (heic_predict_intra(sc->frame, x0 / 2, y0 / 2, 2, cm, 2, sis) != 0)
                return -1;
            if (cbf_cr
                && decode_and_apply_residual(sc, x0 / 2, y0 / 2, 2, 2, scan) != 0)
                return -1;
        }
    } else {
        if (decode_tu_leaf(sc, x0, y0, log2_size, trafo_depth, cbf_cb, cbf_cr) != 0)
            return -1;
    }
    return 0;
}

static int decode_coding_unit(heic_slice_ctx *sc, uint32_t x0, uint32_t y0,
                              uint8_t log2_cb, uint8_t ct_depth)
{
    uint32_t cb_size = 1u << log2_cb;
    int part_nxn = 0;
    uint8_t luma0 = 1, chroma = 1;
    int intra_split;

    sc->cu_base_x = x0;
    sc->cu_base_y = y0;
    sc->cu_log2_size = log2_cb;
    decode_quant_params(sc, x0, x0, y0);
    store_qpy(sc, x0, y0, log2_cb, sc->current_qpy);
    set_ct_depth(sc, x0, y0, log2_cb, ct_depth);

    sc->cu_transquant_bypass = 0;
    if (sc->pps->transquant_bypass_enabled_flag)
        sc->cu_transquant_bypass =
            heic_cabac_decode_bin(&sc->cabac,
                                  &sc->models[HEIC_CTX_CU_TRANSQUANT_BYPASS_FLAG])
            != 0;

    if (sc->sps->pcm_enabled_flag) {
        uint8_t log2_min = (uint8_t)(sc->sps->log2_min_pcm_luma_coding_block_size_minus3
                                     + 3);
        uint8_t log2_max =
            (uint8_t)(log2_min + sc->sps->log2_diff_max_min_pcm_luma_coding_block_size);
        if (log2_cb >= log2_min && log2_cb <= log2_max) {
            if (heic_cabac_decode_terminate(&sc->cabac) != 0) {
                heic_error(sc->hctx, HEIC_SEVERITY_ERROR, "PCM mode not supported");
                return -1;
            }
        }
    }

    if (log2_cb == sc->sps->log2_min_cb_size) {
        int pm = decode_part_mode_intra(sc, log2_cb);
        if (pm < 0) return -1;
        part_nxn = pm;
    }

    if (!part_nxn) {
        int prev = decode_prev_intra_flag(sc);
        luma0 = derive_intra_luma(sc, x0, y0, prev);
        store_intra_mode(sc, x0, y0, log2_cb, luma0, 0);
        chroma = decode_intra_chroma(sc, luma0);
        store_intra_mode(sc, x0, y0, log2_cb, chroma, 1);
    } else {
        uint32_t half = cb_size / 2;
        uint8_t log2_pu = (uint8_t)(log2_cb - 1);
        int pf[4];
        uint8_t lm[4];
        int i;
        for (i = 0; i < 4; i++) pf[i] = decode_prev_intra_flag(sc);
        lm[0] = derive_intra_luma(sc, x0, y0, pf[0]);
        store_intra_mode(sc, x0, y0, log2_pu, lm[0], 0);
        lm[1] = derive_intra_luma(sc, x0 + half, y0, pf[1]);
        store_intra_mode(sc, x0 + half, y0, log2_pu, lm[1], 0);
        lm[2] = derive_intra_luma(sc, x0, y0 + half, pf[2]);
        store_intra_mode(sc, x0, y0 + half, log2_pu, lm[2], 0);
        lm[3] = derive_intra_luma(sc, x0 + half, y0 + half, pf[3]);
        store_intra_mode(sc, x0 + half, y0 + half, log2_pu, lm[3], 0);
        if (chroma_array_type(sc->sps) == 3) {
            uint8_t cm;
            cm = decode_intra_chroma(sc, lm[0]);
            store_intra_mode(sc, x0, y0, log2_pu, cm, 1);
            cm = decode_intra_chroma(sc, lm[1]);
            store_intra_mode(sc, x0 + half, y0, log2_pu, cm, 1);
            cm = decode_intra_chroma(sc, lm[2]);
            store_intra_mode(sc, x0, y0 + half, log2_pu, cm, 1);
            cm = decode_intra_chroma(sc, lm[3]);
            store_intra_mode(sc, x0 + half, y0 + half, log2_pu, cm, 1);
            chroma = cm;
        } else {
            chroma = decode_intra_chroma(sc, lm[0]);
            store_intra_mode(sc, x0, y0, log2_cb, chroma, 1);
        }
        luma0 = lm[0];

        if (sc->deblock_flags) {
            heic_mark_pb_boundary(sc->deblock_flags, sc->deblock_stride, sc->deblock_n,
                                  x0 + half, y0, half, cb_size, 1);
            heic_mark_pb_boundary(sc->deblock_flags, sc->deblock_stride, sc->deblock_n,
                                  x0, y0 + half, cb_size, half, 0);
        }
    }
    (void)luma0;
    (void)chroma;

    intra_split = part_nxn;
    return decode_tt_inner(sc, x0, y0, log2_cb, 0, intra_split, 1, 1);
}

static int decode_cqt(heic_slice_ctx *sc, uint32_t x0, uint32_t y0,
                      uint8_t log2_cb, uint8_t ct_depth)
{
    uint32_t cb;
    uint32_t pw = sc->sps->pic_width_in_luma_samples;
    uint32_t ph = sc->sps->pic_height_in_luma_samples;
    uint8_t log2_min = sc->sps->log2_min_cb_size;
    uint8_t log2_qg;
    int split;

    if (log2_cb < 3 || log2_cb > 6 || log2_cb < log2_min || ct_depth > 6)
        return -1;
    cb = 1u << log2_cb;

    if (x0 + cb <= pw && y0 + cb <= ph && log2_cb > log2_min)
        split = decode_split_cu(sc, x0, y0, ct_depth);
    else if (log2_cb > log2_min)
        split = 1;
    else
        split = 0;

    log2_qg = sc->sps->log2_ctb_size > sc->pps->diff_cu_qp_delta_depth
                  ? (uint8_t)(sc->sps->log2_ctb_size - sc->pps->diff_cu_qp_delta_depth)
                  : 0;
    if (sc->pps->cu_qp_delta_enabled_flag && log2_cb >= log2_qg) {
        sc->is_cu_qp_delta_coded = 0;
        sc->cu_qp_delta = 0;
    }

    if (split) {
        uint32_t half = cb / 2;
        uint32_t x1 = x0 + half, y1 = y0 + half;
        uint8_t child = (uint8_t)(log2_cb - 1);
        uint8_t nd = (uint8_t)(ct_depth + 1);
        if (decode_cqt(sc, x0, y0, child, nd) != 0) return -1;
        if (x1 < pw && decode_cqt(sc, x1, y0, child, nd) != 0) return -1;
        if (y1 < ph && decode_cqt(sc, x0, y1, child, nd) != 0) return -1;
        if (x1 < pw && y1 < ph && decode_cqt(sc, x1, y1, child, nd) != 0) return -1;
    } else {
        if (decode_coding_unit(sc, x0, y0, log2_cb, ct_depth) != 0) return -1;
    }
    return 0;
}

static uint8_t decode_sao_type_idx(heic_slice_ctx *sc)
{
    int b0 = heic_cabac_decode_bin(&sc->cabac, &sc->models[HEIC_CTX_SAO_TYPE_IDX]);
    if (b0 == 0) return 0;
    return heic_cabac_decode_bypass(&sc->cabac) == 0 ? 1 : 2;
}

static uint32_t decode_tu_bypass(heic_slice_ctx *sc, uint32_t c_max)
{
    uint32_t i;
    for (i = 0; i < c_max; i++) {
        if (heic_cabac_decode_bypass(&sc->cabac) == 0) return i;
    }
    return c_max;
}

static int decode_sao(heic_slice_ctx *sc, uint32_t x_ctb_px, uint32_t y_ctb_px)
{
    uint32_t ctb = ctb_size_px(sc->sps);
    uint32_t x_ctb = x_ctb_px / ctb, y_ctb = y_ctb_px / ctb;
    int merge_left = 0, merge_up = 0;
    heic_sao_info info;
    uint32_t pic_w = sc->sps->pic_width_in_ctbs;
    uint32_t addr_rs = y_ctb * pic_w + x_ctb;
    uint32_t slice_rs = sc->sh->slice_segment_address;

    memset(&info, 0, sizeof(info));

    if (x_ctb > 0) {
        int left_in_slice = addr_rs > slice_rs;
        if (left_in_slice)
            merge_left =
                heic_cabac_decode_bin(&sc->cabac, &sc->models[HEIC_CTX_SAO_MERGE_FLAG])
                != 0;
    }
    if (y_ctb > 0 && !merge_left) {
        int up_in_slice = addr_rs >= pic_w + slice_rs;
        if (up_in_slice)
            merge_up =
                heic_cabac_decode_bin(&sc->cabac, &sc->models[HEIC_CTX_SAO_MERGE_FLAG])
                != 0;
    }

    if (merge_left)
        info = sc->sao_map[y_ctb * sc->sao_stride + (x_ctb - 1)];
    else if (merge_up)
        info = sc->sao_map[(y_ctb - 1) * sc->sao_stride + x_ctb];
    else {
        int is_mono = sc->sps->chroma_format_idc == 0;
        int n_chroma = is_mono ? 1 : 3;
        uint8_t sao_type_luma = 0, sao_type_chroma = 0, eo_chroma = 0;
        int c_idx;
        for (c_idx = 0; c_idx < n_chroma; c_idx++) {
            int should = (sc->sh->slice_sao_luma_flag && c_idx == 0)
                         || (sc->sh->slice_sao_chroma_flag && c_idx > 0);
            uint8_t type_idx;
            if (!should) continue;
            if (c_idx == 0) {
                sao_type_luma = decode_sao_type_idx(sc);
                type_idx = sao_type_luma;
            } else if (c_idx == 1) {
                sao_type_chroma = decode_sao_type_idx(sc);
                type_idx = sao_type_chroma;
            } else {
                type_idx = sao_type_chroma;
            }
            info.sao_type_idx[c_idx] = type_idx;
            if (type_idx != 0) {
                int bd = c_idx == 0 ? bit_depth_y(sc->sps) : bit_depth_c(sc->sps);
                uint32_t c_max = (1u << (((bd < 10 ? bd : 10) - 5))) - 1;
                int scale = 1 << (bd > 10 ? bd - 10 : 0);
                uint32_t offs[4];
                int e;
                for (e = 0; e < 4; e++) offs[e] = decode_tu_bypass(sc, c_max);
                if (type_idx == 1) {
                    for (e = 0; e < 4; e++) {
                        if (offs[e]) {
                            int sign = heic_cabac_decode_bypass(&sc->cabac);
                            int16_t val = (int16_t)((int)offs[e] * scale);
                            info.sao_offset_val[c_idx][e] = sign ? (int16_t)(-val) : val;
                        }
                    }
                    info.sao_band_position[c_idx] =
                        (uint8_t)heic_cabac_decode_bypass_bits(&sc->cabac, 5);
                } else {
                    for (e = 0; e < 4; e++)
                        info.sao_offset_val[c_idx][e] =
                            (int16_t)((int)offs[e] * scale);
                    if (c_idx <= 1) {
                        uint8_t eo = (uint8_t)heic_cabac_decode_bypass_bits(&sc->cabac, 2);
                        if (c_idx == 0) info.sao_eo_class[0] = eo;
                        else {
                            eo_chroma = eo;
                            info.sao_eo_class[1] = eo;
                        }
                    } else {
                        info.sao_eo_class[2] = eo_chroma;
                    }
                }
            }
        }
    }
    sc->sao_map[y_ctb * sc->sao_stride + x_ctb] = info;
    return 0;
}

static int decode_ctu(heic_slice_ctx *sc, uint32_t x_ctb, uint32_t y_ctb)
{
    if (sc->pps->cu_qp_delta_enabled_flag) {
        sc->is_cu_qp_delta_coded = 0;
        sc->cu_qp_delta = 0;
    }
    if (sc->sh->slice_sao_luma_flag || sc->sh->slice_sao_chroma_flag) {
        if (decode_sao(sc, x_ctb, y_ctb) != 0) return -1;
    }
    return decode_cqt(sc, x_ctb, y_ctb, sc->sps->log2_ctb_size, 0);
}

static int slice_ctx_init(heic_slice_ctx *sc, heic_ctx *ctx, const heic_sps *sps,
                          const heic_pps *pps, const heic_slice_header *sh,
                          const uint8_t *data, size_t len, heic_frame *frame)
{
    uint32_t min_cb = 1u << sps->log2_min_cb_size;
    uint32_t min_pu = min_pu_size(sps);
    uint32_t min_tb = 1u << sps->log2_min_tb_size;
    uint32_t ct_w, ct_h, pu_w, pu_h, qp_w, qp_h;
    size_t ct_n, pu_n, qp_n, sao_n;

    memset(sc, 0, sizeof(*sc));
    sc->hctx = ctx;
    sc->sps = sps;
    sc->pps = pps;
    sc->sh = sh;
    sc->frame = frame;
    sc->current_qg_x = -1;
    sc->current_qg_y = -1;
    sc->current_qpy = sh->slice_qp_y;
    sc->last_qpy_in_prev_qg = sh->slice_qp_y;
    sc->qp_y = sh->slice_qp_y;

    if (heic_cabac_new(&sc->cabac, data, len) != 0) return -1;
    heic_cabac_init_contexts(sc->models, sh->slice_type, sh->cabac_init_flag,
                             sh->slice_qp_y);

    ct_w = (sps->pic_width_in_luma_samples + min_cb - 1) / min_cb;
    ct_h = (sps->pic_height_in_luma_samples + min_cb - 1) / min_cb;
    sc->ct_depth_stride = ct_w;
    ct_n = (size_t)ct_w * ct_h;
    sc->ct_depth_n = ct_n;
    sc->ct_depth_map = (uint8_t *)heic_zalloc(ctx, ct_n);
    if (!sc->ct_depth_map) return -1;
    memset(sc->ct_depth_map, 0xFF, ct_n);

    pu_w = (sps->pic_width_in_luma_samples + min_pu - 1) / min_pu;
    pu_h = (sps->pic_height_in_luma_samples + min_pu - 1) / min_pu;
    sc->intra_mode_stride = pu_w;
    pu_n = (size_t)pu_w * pu_h;
    sc->intra_mode_n = pu_n;
    sc->intra_mode_map = (uint8_t *)heic_zalloc(ctx, pu_n);
    sc->intra_chroma_mode_map = (uint8_t *)heic_zalloc(ctx, pu_n);
    if (!sc->intra_mode_map || !sc->intra_chroma_mode_map) return -1;
    memset(sc->intra_mode_map, 1, pu_n);
    memset(sc->intra_chroma_mode_map, 1, pu_n);

    qp_w = (sps->pic_width_in_luma_samples + min_tb - 1) / min_tb;
    qp_h = (sps->pic_height_in_luma_samples + min_tb - 1) / min_tb;
    sc->qp_map_stride = qp_w;
    qp_n = (size_t)qp_w * qp_h;
    sc->qp_map_n = qp_n;
    sc->qp_map = (int8_t *)heic_zalloc(ctx, qp_n);
    if (!sc->qp_map) return -1;
    {
        size_t i;
        for (i = 0; i < qp_n; i++) sc->qp_map[i] = (int8_t)sh->slice_qp_y;
    }

    sc->sao_stride = sps->pic_width_in_ctbs;
    sao_n = (size_t)sps->pic_width_in_ctbs * sps->pic_height_in_ctbs;
    sc->sao_map = (heic_sao_info *)heic_zalloc(ctx, sao_n * sizeof(heic_sao_info));
    if (!sc->sao_map) return -1;

    sc->deblock_stride = (sps->pic_width_in_luma_samples + 3) / 4;
    {
        uint32_t db_h = (sps->pic_height_in_luma_samples + 3) / 4;
        size_t db_n = (size_t)sc->deblock_stride * db_h;
        sc->deblock_n = (uint32_t)db_n;
        sc->deblock_flags = (uint8_t *)heic_zalloc(ctx, db_n);
        sc->deblock_qp = (int8_t *)heic_zalloc(ctx, db_n);
        if (!sc->deblock_flags || !sc->deblock_qp) return -1;
        {
            size_t i;
            for (i = 0; i < db_n; i++) sc->deblock_qp[i] = (int8_t)sh->slice_qp_y;
        }
    }
    sc->residual_buf =
        (int16_t *)heic_zalloc(ctx, (size_t)HEIC_MAX_COEFF * sizeof(int16_t));
    sc->coeff = (heic_coeff_buf *)heic_zalloc(ctx, sizeof(heic_coeff_buf));
    if (!sc->residual_buf || !sc->coeff) return -1;
    return 0;
}

static void slice_ctx_free(heic_slice_ctx *sc)
{
    if (!sc || !sc->hctx) return;
    heic_free_buf(sc->hctx, sc->ct_depth_map);
    heic_free_buf(sc->hctx, sc->intra_mode_map);
    heic_free_buf(sc->hctx, sc->intra_chroma_mode_map);
    heic_free_buf(sc->hctx, sc->qp_map);
    heic_free_buf(sc->hctx, sc->sao_map);
    heic_free_buf(sc->hctx, sc->deblock_flags);
    heic_free_buf(sc->hctx, sc->deblock_qp);
    heic_free_buf(sc->hctx, sc->residual_buf);
    heic_free_buf(sc->hctx, sc->coeff);
    memset(sc, 0, sizeof(*sc));
}

static uint32_t ebsp_to_rbsp(const uint32_t *eps, int n_ep, uint32_t ebsp_off)
{
    int i, count = 0;
    for (i = 0; i < n_ep; i++)
        if (eps[i] < ebsp_off) count++;
        else break;
    return ebsp_off - (uint32_t)count;
}

static int compute_tile_bd(const heic_pps *pps, uint32_t pic_w, uint32_t pic_h,
                           uint32_t *col_bd, uint32_t *row_bd,
                           int *n_cols, int *n_rows)
{
    uint32_t nc = (uint32_t)pps->num_tile_columns_minus1 + 1;
    uint32_t nr = (uint32_t)pps->num_tile_rows_minus1 + 1;
    uint32_t i;
    if (nc == 0 || nr == 0 || nc > 64 || nr > 64) return -1;
    if (pic_w == 0 || pic_h == 0) return -1;
    *n_cols = (int)nc;
    *n_rows = (int)nr;
    if (pps->uniform_spacing_flag) {
        for (i = 0; i <= nc; i++) col_bd[i] = (i * pic_w) / nc;
        for (i = 0; i <= nr; i++) row_bd[i] = (i * pic_h) / nr;
    } else {
        uint32_t pos = 0;
        col_bd[0] = 0;
        for (i = 0; i < nc - 1 && pps->column_width_minus1; i++) {
            pos = pos + (uint32_t)pps->column_width_minus1[i] + 1;
            if (pos > pic_w) pos = pic_w;
            col_bd[i + 1] = pos;
        }
        col_bd[nc] = pic_w;
        pos = 0;
        row_bd[0] = 0;
        for (i = 0; i < nr - 1 && pps->row_height_minus1; i++) {
            pos = pos + (uint32_t)pps->row_height_minus1[i] + 1;
            if (pos > pic_h) pos = pic_h;
            row_bd[i + 1] = pos;
        }
        row_bd[nr] = pic_h;
    }
    return 0;
}

static uint32_t get_tile_id(const uint32_t *col_bd, int n_cols,
                            const uint32_t *row_bd, int n_rows,
                            uint32_t cx, uint32_t cy)
{
    int tc = 0, tr = 0, i;
    for (i = 0; i < n_cols; i++)
        if (cx >= col_bd[i] && cx < col_bd[i + 1]) {
            tc = i;
            break;
        }
    for (i = 0; i < n_rows; i++)
        if (cy >= row_bd[i] && cy < row_bd[i + 1]) {
            tr = i;
            break;
        }
    return (uint32_t)tr * (uint32_t)n_cols + (uint32_t)tc;
}

static int build_tile_scan(heic_ctx *ctx, const uint32_t *col_bd, int n_cols,
                           const uint32_t *row_bd, int n_rows,
                           uint32_t **out_xy, int *out_n)
{
    int tr, tc;
    int n = 0, cap = 0;
    uint32_t *scan = NULL;
    for (tr = 0; tr < n_rows; tr++) {
        for (tc = 0; tc < n_cols; tc++) {
            uint32_t cy, cx;
            for (cy = row_bd[tr]; cy < row_bd[tr + 1]; cy++) {
                for (cx = col_bd[tc]; cx < col_bd[tc + 1]; cx++) {
                    if (n + 2 > cap) {
                        int ncap = cap ? cap * 2 : 64;
                        uint32_t *np = (uint32_t *)heic_realloc_buf(
                            ctx, scan, (size_t)cap * sizeof(uint32_t),
                            (size_t)ncap * sizeof(uint32_t));
                        if (!np) {
                            heic_free_buf(ctx, scan);
                            return -1;
                        }
                        scan = np;
                        cap = ncap;
                    }
                    scan[n++] = cx;
                    scan[n++] = cy;
                }
            }
        }
    }
    *out_xy = scan;
    *out_n = n / 2;
    return 0;
}

typedef struct {
    heic_slice_ctx   sc;
    uint32_t         col_bd[65];
    uint32_t         row_bd[65];
    uint32_t         entry_cum[4096];
    heic_ctx_model   wpp_saved[HEIC_NUM_CONTEXTS];
} heic_slice_work;

int heic_hevc_decode_slice_i(heic_ctx *ctx, const heic_sps *sps,
                             const heic_pps *pps, const heic_slice_header *sh,
                             const uint8_t *data, size_t len,
                             const uint32_t *ep_positions, int n_ep,
                             heic_frame *out, const heic_abort *ab)
{
    heic_slice_work *work;
    heic_slice_ctx *sc;
    uint32_t ctb = 0, total, start, pic_w, pic_h, ctb_sz;
    int tiles, wpp;
    int n_cols = 1, n_rows = 1;
    uint32_t *tile_scan = NULL;
    int tile_scan_n = 0, tile_scan_pos = 0;
    int n_entry = 0, entry_idx = 0;
    int wpp_have_saved = 0;
    int i;

    if (!ctx || !sps || !pps || !sh || !data || !out) return -1;
    if (sh->slice_type != HEIC_SLICE_I) {
        heic_error(ctx, HEIC_SEVERITY_ERROR, "only I-slice CTU path implemented");
        return -1;
    }
    total = sps->pic_size_in_ctbs;
    start = sh->slice_segment_address;
    if (total == 0 || start >= total) {
        heic_error(ctx, HEIC_SEVERITY_ERROR, "slice_segment_address out of range");
        return -1;
    }

    work = (heic_slice_work *)heic_zalloc(ctx, sizeof(*work));
    if (!work) return -1;
    sc = &work->sc;

    if (slice_ctx_init(sc, ctx, sps, pps, sh, data, len, out) != 0) {
        slice_ctx_free(sc);
        heic_free_buf(ctx, work);
        return -1;
    }

    pic_w = sps->pic_width_in_ctbs;
    pic_h = sps->pic_height_in_ctbs;
    ctb_sz = ctb_size_px(sps);
    tiles = pps->tiles_enabled_flag;
    wpp = pps->entropy_coding_sync_enabled_flag;

    if (tiles) {
        if (compute_tile_bd(pps, pic_w, pic_h, work->col_bd, work->row_bd, &n_cols,
                            &n_rows) != 0) {
            heic_error(ctx, HEIC_SEVERITY_ERROR, "invalid tile grid");
            slice_ctx_free(sc);
            heic_free_buf(ctx, work);
            return -1;
        }
        if (build_tile_scan(ctx, work->col_bd, n_cols, work->row_bd, n_rows, &tile_scan,
                            &tile_scan_n) != 0) {
            slice_ctx_free(sc);
            heic_free_buf(ctx, work);
            return -1;
        }

        for (i = 0; i < tile_scan_n; i++) {
            if (tile_scan[i * 2] == start % pic_w &&
                tile_scan[i * 2 + 1] == start / pic_w) {
                tile_scan_pos = i;
                break;
            }
        }
        sc->ctb_x = tile_scan[tile_scan_pos * 2];
        sc->ctb_y = tile_scan[tile_scan_pos * 2 + 1];
    } else {
        sc->ctb_y = start / pic_w;
        sc->ctb_x = start % pic_w;
    }

    if (sh->num_entry_point_offsets > 0 && sh->entry_point_offsets) {
        uint32_t cum = 0;
        n_entry = (int)sh->num_entry_point_offsets;
        if (n_entry > 4096) n_entry = 4096;
        for (i = 0; i < n_entry; i++) {
            cum += sh->entry_point_offsets[i];
            work->entry_cum[i] = cum;
        }
    }

    for (;;) {
        uint32_t x = sc->ctb_x * ctb_sz;
        uint32_t y = sc->ctb_y * ctb_sz;
        int end_flag;
        uint32_t prev_x = sc->ctb_x, prev_y = sc->ctb_y;

        if (heic_abort_check(ab)) {
            heic_free_buf(ctx, tile_scan);
            slice_ctx_free(sc);
            heic_free_buf(ctx, work);
            return -1;
        }
        if (heic_cabac_overread(&sc->cabac)) {
            heic_error(ctx, HEIC_SEVERITY_ERROR, "CABAC overread during CTU decode");
            heic_free_buf(ctx, tile_scan);
            slice_ctx_free(sc);
            heic_free_buf(ctx, work);
            return -1;
        }
        if (decode_ctu(sc, x, y) != 0 || sc->cabac.error) {
            heic_error(ctx, HEIC_SEVERITY_ERROR, "CTU decode failed at (%u,%u) ctu=%u",
                       sc->ctb_x, sc->ctb_y, ctb);
            heic_free_buf(ctx, tile_scan);
            slice_ctx_free(sc);
            heic_free_buf(ctx, work);
            return -1;
        }
        ctb++;

        if (wpp && sc->ctb_x == 1 && sc->ctb_y + 1 < pic_h) {
            memcpy(work->wpp_saved, sc->models, sizeof(work->wpp_saved));
            wpp_have_saved = 1;
        }

        end_flag = heic_cabac_decode_terminate(&sc->cabac);
        if (end_flag) break;

        if (tiles) {
            tile_scan_pos++;
            if (tile_scan_pos >= tile_scan_n) break;
            sc->ctb_x = tile_scan[tile_scan_pos * 2];
            sc->ctb_y = tile_scan[tile_scan_pos * 2 + 1];
            {
                uint32_t pt =
                    get_tile_id(work->col_bd, n_cols, work->row_bd, n_rows, prev_x, prev_y);
                uint32_t ct = get_tile_id(work->col_bd, n_cols, work->row_bd, n_rows,
                                          sc->ctb_x, sc->ctb_y);
                if (ct != pt) {
                    (void)heic_cabac_decode_terminate(&sc->cabac);
                    if (entry_idx < n_entry) {
                        uint32_t ebsp = work->entry_cum[entry_idx];
                        uint32_t rbsp = ep_positions && n_ep > 0
                                           ? ebsp_to_rbsp(ep_positions, n_ep, ebsp)
                                           : ebsp;
                        heic_cabac_seek(&sc->cabac, rbsp);
                        heic_cabac_reinit(&sc->cabac);
                        entry_idx++;
                    }
                    heic_cabac_init_contexts(sc->models, sh->slice_type, sh->cabac_init_flag,
                                             sh->slice_qp_y);
                    sc->current_qpy = sh->slice_qp_y;
                    sc->last_qpy_in_prev_qg = sh->slice_qp_y;
                    sc->current_qg_x = -1;
                    sc->current_qg_y = -1;
                    sc->is_cu_qp_delta_coded = 0;
                    sc->cu_qp_delta = 0;
                }
            }
        } else {
            sc->ctb_x++;
            if (sc->ctb_x >= pic_w) {
                sc->ctb_x = 0;
                sc->ctb_y++;

                if (wpp && sc->ctb_y < pic_h && pic_w > 1) {
                    (void)heic_cabac_decode_terminate(&sc->cabac);
                    if (wpp_have_saved)
                        memcpy(sc->models, work->wpp_saved, sizeof(sc->models));
                    if (entry_idx < n_entry) {
                        uint32_t ebsp = work->entry_cum[entry_idx];
                        uint32_t rbsp = ep_positions && n_ep > 0
                                           ? ebsp_to_rbsp(ep_positions, n_ep, ebsp)
                                           : ebsp;
                        heic_cabac_seek(&sc->cabac, rbsp);
                        heic_cabac_reinit(&sc->cabac);
                        entry_idx++;
                    }
                }
            }
            if (sc->ctb_y >= pic_h) break;
        }
    }

    heic_error(ctx, HEIC_SEVERITY_INFO, "I-slice decoded %u CTUs", (unsigned)ctb);

    if (!sh->slice_deblocking_filter_disabled_flag && sc->deblock_flags &&
        sc->deblock_qp) {
        int beta = (int)sh->slice_beta_offset_div2 * 2;
        int tc = (int)sh->slice_tc_offset_div2 * 2;
        int cb_off = (int)pps->pps_cb_qp_offset + (int)sh->slice_cb_qp_offset;
        int cr_off = (int)pps->pps_cr_qp_offset + (int)sh->slice_cr_qp_offset;
        heic_apply_deblock(out, sc->deblock_flags, sc->deblock_qp, sc->deblock_stride,
                           beta, tc, cb_off, cr_off);
    }
    if (sps->sample_adaptive_offset_enabled_flag &&
        (sh->slice_sao_luma_flag || sh->slice_sao_chroma_flag) && sc->sao_map) {
        heic_apply_sao(ctx, out, sc->sao_map, sps->pic_width_in_ctbs,
                       sps->pic_height_in_ctbs, ctb_sz);
    }

    heic_free_buf(ctx, tile_scan);
    slice_ctx_free(sc);
    heic_free_buf(ctx, work);
    return 0;
}

#include <string.h>

static const int EO_OFFSETS[4][4] = {
    { -1, 0, 1, 0 },
    { 0, -1, 0, 1 },
    { -1, -1, 1, 1 },
    { 1, -1, -1, 1 },
};

static int isignum(int v)
{
    if (v > 0) return 1;
    if (v < 0) return -1;
    return 0;
}

static int heic_sao_clampi(int v, int lo, int hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static void apply_sao_band(uint16_t *plane, int stride, int x0, int y0, int x1,
                           int y1, uint8_t band_pos, const int16_t offs[4],
                           int bit_depth)
{
    int max_val = (1 << bit_depth) - 1;
    int band_shift = bit_depth > 5 ? bit_depth - 5 : 0;
    int16_t band_table[32];
    int k, y, x;

    memset(band_table, 0, sizeof(band_table));
    for (k = 0; k < 4; k++)
        band_table[(band_pos + k) & 31] = offs[k];

    if (stride <= 0) return;
    for (y = y0; y < y1; y++) {
        uint16_t *row = plane + (size_t)y * (size_t)stride;
        if (heic_simd_sao_band_row(row, x0, x1, band_shift, band_table, max_val))
            continue;
        for (x = x0; x < x1; x++) {
            int sample = (int)row[x];
            if (sample > max_val) sample = max_val;
            {
                int band = sample >> band_shift;
                int offset = (int)band_table[band & 31];
                if (offset)
                    row[x] = (uint16_t)heic_sao_clampi(sample + offset, 0, max_val);
            }
        }
    }
}

static void apply_sao_edge_pixel(const uint16_t *src, uint16_t *dst, int stride,
                                 int x, int y, int dx0, int dy0, int dx1, int dy1,
                                 int plane_w, int plane_h, int max_val,
                                 const int offset_table[5])
{
    int nx0 = x + dx0, ny0 = y + dy0, nx1 = x + dx1, ny1 = y + dy1;
    int sample, n0, n1, edge_idx, offset;
    size_t idx;

    if (nx0 < 0 || nx0 >= plane_w || ny0 < 0 || ny0 >= plane_h || nx1 < 0 ||
        nx1 >= plane_w || ny1 < 0 || ny1 >= plane_h)
        return;

    idx = (size_t)y * (size_t)stride + (size_t)x;
    sample = (int)src[idx];
    n0 = (int)src[(size_t)ny0 * (size_t)stride + (size_t)nx0];
    n1 = (int)src[(size_t)ny1 * (size_t)stride + (size_t)nx1];
    edge_idx = 2 + isignum(sample - n0) + isignum(sample - n1);
    if (edge_idx < 0 || edge_idx > 4) return;
    offset = offset_table[edge_idx];
    if (offset)
        dst[idx] = (uint16_t)heic_sao_clampi(sample + offset, 0, max_val);
}

static void apply_sao_edge(const uint16_t *src, uint16_t *dst, int stride,
                           int plane_w, int plane_h, int x0, int y0, int x1,
                           int y1, uint8_t eo_class, const int16_t offs[4],
                           int bit_depth)
{
    int max_val = (1 << bit_depth) - 1;
    int dx0, dy0, dx1, dy1;
    int offset_table[5];
    int safe_x0, safe_x1, safe_y0, safe_y1;
    int y, x;
    int cls = (int)(eo_class & 3);

    dx0 = EO_OFFSETS[cls][0];
    dy0 = EO_OFFSETS[cls][1];
    dx1 = EO_OFFSETS[cls][2];
    dy1 = EO_OFFSETS[cls][3];

    offset_table[0] = (int)offs[0];
    offset_table[1] = (int)offs[1];
    offset_table[2] = 0;
    offset_table[3] = -(int)offs[2];
    offset_table[4] = -(int)offs[3];

    if (stride <= 0 || plane_w <= 0 || plane_h <= 0) return;
    if (plane_w > stride) plane_w = stride;

    safe_x0 = x0;
    if (-dx0 > safe_x0) safe_x0 = -dx0;
    if (-dx1 > safe_x0) safe_x0 = -dx1;
    if (safe_x0 < 0) safe_x0 = 0;

    safe_x1 = x1;
    {
        int mx = dx0 > dx1 ? dx0 : dx1;
        if (mx < 0) mx = 0;
        if (plane_w - mx < safe_x1) safe_x1 = plane_w - mx;
    }

    safe_y0 = y0;
    if (-dy0 > safe_y0) safe_y0 = -dy0;
    if (-dy1 > safe_y0) safe_y0 = -dy1;
    if (safe_y0 < 0) safe_y0 = 0;

    safe_y1 = y1;
    {
        int my = dy0 > dy1 ? dy0 : dy1;
        if (my < 0) my = 0;
        if (plane_h - my < safe_y1) safe_y1 = plane_h - my;
    }

    for (y = safe_y0; y < safe_y1; y++) {
        const uint16_t *srow = src + (size_t)y * (size_t)stride;
        uint16_t *drow = dst + (size_t)y * (size_t)stride;
        if (cls == 0 &&
            heic_simd_sao_edge_h_row(srow, drow, safe_x0, safe_x1, offset_table, max_val))
            continue;
        if (cls == 1 &&
            heic_simd_sao_edge_v_row(src, dst, stride, y, safe_x0, safe_x1, offset_table,
                                     max_val))
            continue;
        for (x = safe_x0; x < safe_x1; x++) {
            int sample = (int)srow[x];
            int n0 = (int)src[(size_t)(y + dy0) * (size_t)stride + (size_t)(x + dx0)];
            int n1 = (int)src[(size_t)(y + dy1) * (size_t)stride + (size_t)(x + dx1)];
            int edge_idx = 2 + isignum(sample - n0) + isignum(sample - n1);
            int offset = offset_table[edge_idx];
            if (offset)
                drow[x] = (uint16_t)heic_sao_clampi(sample + offset, 0, max_val);
        }
    }

    for (y = y0; y < y1; y++) {
        if (y >= safe_y0 && y < safe_y1) {
            for (x = x0; x < safe_x0 && x < x1; x++)
                apply_sao_edge_pixel(src, dst, stride, x, y, dx0, dy0, dx1, dy1,
                                     plane_w, plane_h, max_val, offset_table);
            for (x = safe_x1 > x0 ? safe_x1 : x0; x < x1; x++)
                apply_sao_edge_pixel(src, dst, stride, x, y, dx0, dy0, dx1, dy1,
                                     plane_w, plane_h, max_val, offset_table);
        } else {
            for (x = x0; x < x1; x++)
                apply_sao_edge_pixel(src, dst, stride, x, y, dx0, dy0, dx1, dy1,
                                     plane_w, plane_h, max_val, offset_table);
        }
    }
}

void heic_apply_sao(heic_ctx *ctx, heic_frame *frame, const heic_sao_info *map,
                    uint32_t width_ctbs, uint32_t height_ctbs, uint32_t ctb_size)
{
    uint32_t ctb_x, ctb_y;
    int need_y = 0, need_cb = 0, need_cr = 0;
    uint16_t *orig_y = NULL, *orig_cb = NULL, *orig_cr = NULL;
    size_t y_n, c_n;
    int w, h, cw, ch, sub_x, sub_y;
    uint32_t n_ctb;

    if (!ctx || !frame || !map || ctb_size == 0) return;
    w = frame->width;
    h = frame->height;
    if (w <= 0 || h <= 0) return;

    n_ctb = width_ctbs * height_ctbs;
    {
        uint32_t i;
        for (i = 0; i < n_ctb; i++) {
            if (map[i].sao_type_idx[0] == 2) need_y = 1;
            if (map[i].sao_type_idx[1] == 2) need_cb = 1;
            if (map[i].sao_type_idx[2] == 2) need_cr = 1;
        }
    }

    y_n = (size_t)frame->y_stride * (size_t)h * sizeof(uint16_t);
    if (need_y && frame->y) {
        orig_y = (uint16_t *)heic_zalloc(ctx, y_n);
        if (orig_y) memcpy(orig_y, frame->y, y_n);
        else need_y = 0;
    }
    cw = frame->c_width;
    ch = frame->c_height;
    c_n = (cw > 0 && ch > 0)
              ? (size_t)frame->c_stride * (size_t)ch * sizeof(uint16_t)
              : 0;
    if (need_cb && frame->cb && c_n) {
        orig_cb = (uint16_t *)heic_zalloc(ctx, c_n);
        if (orig_cb) memcpy(orig_cb, frame->cb, c_n);
        else need_cb = 0;
    }
    if (need_cr && frame->cr && c_n) {
        orig_cr = (uint16_t *)heic_zalloc(ctx, c_n);
        if (orig_cr) memcpy(orig_cr, frame->cr, c_n);
        else need_cr = 0;
    }

    switch (frame->chroma_format) {
    case 1: sub_x = 2; sub_y = 2; break;
    case 2: sub_x = 2; sub_y = 1; break;
    case 3: sub_x = 1; sub_y = 1; break;
    default: sub_x = 1; sub_y = 1; break;
    }

    for (ctb_y = 0; ctb_y < height_ctbs; ctb_y++) {
        for (ctb_x = 0; ctb_x < width_ctbs; ctb_x++) {
            const heic_sao_info *sao = &map[ctb_y * width_ctbs + ctb_x];
            int x_px = (int)(ctb_x * ctb_size);
            int y_px = (int)(ctb_y * ctb_size);
            int x_end = x_px + (int)ctb_size;
            int y_end = y_px + (int)ctb_size;
            if (x_end > w) x_end = w;
            if (y_end > h) y_end = h;

            if (sao->sao_type_idx[0] == 1 && frame->y) {
                apply_sao_band(frame->y, frame->y_stride, x_px, y_px, x_end, y_end,
                               sao->sao_band_position[0], sao->sao_offset_val[0],
                               frame->bit_depth);
            } else if (sao->sao_type_idx[0] == 2 && frame->y && orig_y) {
                apply_sao_edge(orig_y, frame->y, frame->y_stride, w, h, x_px, y_px,
                               x_end, y_end, sao->sao_eo_class[0],
                               sao->sao_offset_val[0], frame->bit_depth);
            }

            if (frame->chroma_format > 0 && frame->cb && frame->cr && cw > 0 &&
                ch > 0) {
                int cx0 = x_px / sub_x;
                int cy0 = y_px / sub_y;
                int cx1 = (x_px + (int)ctb_size) / sub_x;
                int cy1 = (y_px + (int)ctb_size) / sub_y;
                if (cx1 > cw) cx1 = cw;
                if (cy1 > ch) cy1 = ch;

                if (sao->sao_type_idx[1] == 1) {
                    apply_sao_band(frame->cb, frame->c_stride, cx0, cy0, cx1, cy1,
                                   sao->sao_band_position[1], sao->sao_offset_val[1],
                                   frame->bit_depth);
                } else if (sao->sao_type_idx[1] == 2 && orig_cb) {
                    apply_sao_edge(orig_cb, frame->cb, frame->c_stride, cw, ch, cx0,
                                   cy0, cx1, cy1, sao->sao_eo_class[1],
                                   sao->sao_offset_val[1], frame->bit_depth);
                }
                if (sao->sao_type_idx[2] == 1) {
                    apply_sao_band(frame->cr, frame->c_stride, cx0, cy0, cx1, cy1,
                                   sao->sao_band_position[2], sao->sao_offset_val[2],
                                   frame->bit_depth);
                } else if (sao->sao_type_idx[2] == 2 && orig_cr) {
                    apply_sao_edge(orig_cr, frame->cr, frame->c_stride, cw, ch, cx0,
                                   cy0, cx1, cy1, sao->sao_eo_class[2],
                                   sao->sao_offset_val[2], frame->bit_depth);
                }
            }
        }
    }

    heic_free_buf(ctx, orig_y);
    heic_free_buf(ctx, orig_cb);
    heic_free_buf(ctx, orig_cr);
}

#define HEIC_DEBLOCK_FLAG_VERT      1
#define HEIC_DEBLOCK_FLAG_HORIZ     2
#define HEIC_DEBLOCK_PB_EDGE_VERT   4
#define HEIC_DEBLOCK_PB_EDGE_HORIZ  8

static const uint16_t BETA_PRIME[52] = {
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 20, 22, 24,
    26, 28, 30, 32, 34, 36, 38, 40, 42, 44, 46, 48, 50, 52, 54, 56,
    58, 60, 62, 64,
};

static const uint16_t TC_PRIME[54] = {
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  1,  1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  3,
     3,  3,  3,  4,  4,  4,  5,  5,  6,  6,  7,  8,  9, 10, 11, 13,
    14, 16, 18, 20, 22, 24,
};

static const int CHROMA_QP_TABLE[13] = {
    29, 30, 31, 32, 33, 33, 34, 34, 35, 35, 36, 36, 37,
};

static inline int clampi(int v, int lo, int hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static inline int iabs(int v) { return v < 0 ? -v : v; }

static int chroma_qp_mapping(int qp_i)
{
    if (qp_i < 30) return qp_i;
    if (qp_i >= 43) return qp_i - 6;
    return CHROMA_QP_TABLE[qp_i - 30];
}

static int compute_bs_i(void) { return 2; }

static void filter_edge_luma(heic_frame *frame, uint32_t x, uint32_t y,
                             int vertical, int qp_p, int qp_q, int beta_offset,
                             int tc_offset, int bs)
{
    int bit_depth = frame->bit_depth;
    int max_val = (1 << bit_depth) - 1;
    int qp_l, q_beta, beta, q_tc, tc;
    int stride = frame->y_stride;
    uint16_t *plane = frame->y;
    size_t step_along, step_across, base_q, base_p, last_q;
    int p0_0, p1_0, p2_0, p3_0, q0_0, q1_0, q2_0, q3_0;
    int p0_3, p1_3, p2_3, p3_3, q0_3, q1_3, q2_3, q3_3;
    int dp0, dp3, dq0, dq3, dpq0, dpq3, dp, dq, d;
    int d_sam0, d_sam3, strong, d_ep, d_eq;
    size_t k3;
    int k;

    if (!plane || stride <= 0) return;

    qp_l = (qp_q + qp_p + 1) >> 1;
    q_beta = clampi(qp_l + beta_offset, 0, 51);
    beta = ((int)BETA_PRIME[q_beta]) << (bit_depth - 8);
    q_tc = clampi(qp_l + 2 * (bs - 1) + tc_offset, 0, 53);
    tc = ((int)TC_PRIME[q_tc]) << (bit_depth - 8);
    if (tc == 0) return;

    if (vertical) {
        step_along = (size_t)stride;
        step_across = 1;
        base_q = (size_t)y * (size_t)stride + (size_t)x;
    } else {
        step_along = 1;
        step_across = (size_t)stride;
        base_q = (size_t)y * (size_t)stride + (size_t)x;
    }
    if (base_q < step_across) return;
    base_p = base_q - step_across;
    if (base_p < 3 * step_across) return;
    last_q = base_q + 3 * step_along + 3 * step_across;
    if (last_q >= (size_t)stride * (size_t)frame->height) return;

    k3 = 3 * step_along;
    p0_0 = plane[base_p];
    p1_0 = plane[base_p - step_across];
    p2_0 = plane[base_p - 2 * step_across];
    p3_0 = plane[base_p - 3 * step_across];
    q0_0 = plane[base_q];
    q1_0 = plane[base_q + step_across];
    q2_0 = plane[base_q + 2 * step_across];
    q3_0 = plane[base_q + 3 * step_across];
    p0_3 = plane[base_p + k3];
    p1_3 = plane[base_p + k3 - step_across];
    p2_3 = plane[base_p + k3 - 2 * step_across];
    p3_3 = plane[base_p + k3 - 3 * step_across];
    q0_3 = plane[base_q + k3];
    q1_3 = plane[base_q + k3 + step_across];
    q2_3 = plane[base_q + k3 + 2 * step_across];
    q3_3 = plane[base_q + k3 + 3 * step_across];

    dp0 = iabs(p2_0 - 2 * p1_0 + p0_0);
    dp3 = iabs(p2_3 - 2 * p1_3 + p0_3);
    dq0 = iabs(q2_0 - 2 * q1_0 + q0_0);
    dq3 = iabs(q2_3 - 2 * q1_3 + q0_3);
    dpq0 = dp0 + dq0;
    dpq3 = dp3 + dq3;
    dp = dp0 + dp3;
    dq = dq0 + dq3;
    d = dpq0 + dpq3;
    if (d >= beta) return;

    d_sam0 = (2 * dpq0 < (beta >> 2)) &&
             (iabs(p3_0 - p0_0) + iabs(q0_0 - q3_0) < (beta >> 3)) &&
             (iabs(p0_0 - q0_0) < ((5 * tc + 1) >> 1));
    d_sam3 = (2 * dpq3 < (beta >> 2)) &&
             (iabs(p3_3 - p0_3) + iabs(q0_3 - q3_3) < (beta >> 3)) &&
             (iabs(p0_3 - q0_3) < ((5 * tc + 1) >> 1));
    strong = d_sam0 && d_sam3;
    d_ep = dp < ((beta + (beta >> 1)) >> 3);
    d_eq = dq < ((beta + (beta >> 1)) >> 3);

    if (heic_simd_luma_filter4(plane, base_p, base_q, step_along, step_across, strong,
                               d_ep, d_eq, tc, max_val))
        return;

    for (k = 0; k < 4; k++) {
        size_t k_off = (size_t)k * step_along;
        int p0 = plane[base_p + k_off];
        int p1 = plane[base_p + k_off - step_across];
        int p2 = plane[base_p + k_off - 2 * step_across];
        int q0 = plane[base_q + k_off];
        int q1 = plane[base_q + k_off + step_across];
        int q2 = plane[base_q + k_off + 2 * step_across];

        if (strong) {
            int p3 = plane[base_p + k_off - 3 * step_across];
            int q3 = plane[base_q + k_off + 3 * step_across];
            int tc2 = 2 * tc;
            int p0_f = clampi(clampi((p2 + 2 * p1 + 2 * p0 + 2 * q0 + q1 + 4) >> 3,
                                     p0 - tc2, p0 + tc2),
                              0, max_val);
            int p1_f = clampi(clampi((p2 + p1 + p0 + q0 + 2) >> 2, p1 - tc2, p1 + tc2),
                              0, max_val);
            int p2_f = clampi(clampi((2 * p3 + 3 * p2 + p1 + p0 + q0 + 4) >> 3,
                                     p2 - tc2, p2 + tc2),
                              0, max_val);
            int q0_f = clampi(clampi((p1 + 2 * p0 + 2 * q0 + 2 * q1 + q2 + 4) >> 3,
                                     q0 - tc2, q0 + tc2),
                              0, max_val);
            int q1_f = clampi(clampi((p0 + q0 + q1 + q2 + 2) >> 2, q1 - tc2, q1 + tc2),
                              0, max_val);
            int q2_f = clampi(clampi((p0 + q0 + q1 + 3 * q2 + 2 * q3 + 4) >> 3,
                                     q2 - tc2, q2 + tc2),
                              0, max_val);
            plane[base_p + k_off] = (uint16_t)p0_f;
            plane[base_p + k_off - step_across] = (uint16_t)p1_f;
            plane[base_p + k_off - 2 * step_across] = (uint16_t)p2_f;
            plane[base_q + k_off] = (uint16_t)q0_f;
            plane[base_q + k_off + step_across] = (uint16_t)q1_f;
            plane[base_q + k_off + 2 * step_across] = (uint16_t)q2_f;
        } else {
            int delta = (9 * (q0 - p0) - 3 * (q1 - p1) + 8) >> 4;
            if (iabs(delta) < 10 * tc) {
                delta = clampi(delta, -tc, tc);
                plane[base_p + k_off] = (uint16_t)clampi(p0 + delta, 0, max_val);
                plane[base_q + k_off] = (uint16_t)clampi(q0 - delta, 0, max_val);
                if (d_ep) {
                    int delta_p = clampi(((((p2 + p0 + 1) >> 1) - p1 + delta) >> 1),
                                         -(tc >> 1), tc >> 1);
                    plane[base_p + k_off - step_across] =
                        (uint16_t)clampi(p1 + delta_p, 0, max_val);
                }
                if (d_eq) {
                    int delta_q = clampi(((((q2 + q0 + 1) >> 1) - q1 - delta) >> 1),
                                         -(tc >> 1), tc >> 1);
                    plane[base_q + k_off + step_across] =
                        (uint16_t)clampi(q1 + delta_q, 0, max_val);
                }
            }
        }
    }
}

static void apply_chroma_deblocking(heic_frame *frame, const uint8_t *flags,
                                    const int8_t *qp_map, uint32_t deblock_stride,
                                    int tc_offset, int cb_qp_offset, int cr_qp_offset)
{
    int w = frame->width, h = frame->height;
    int bit_depth_c = frame->bit_depth;
    int max_val = (1 << bit_depth_c) - 1;
    int sub_x, sub_y;
    int c_stride, c_height, c_width;
    uint32_t x_step_vert, y_step_vert, x_step_horiz, y_step_horiz;
    uint32_t x, y;
    int vert_edge_mask = HEIC_DEBLOCK_FLAG_VERT | HEIC_DEBLOCK_PB_EDGE_VERT;
    int horiz_edge_mask = HEIC_DEBLOCK_FLAG_HORIZ | HEIC_DEBLOCK_PB_EDGE_HORIZ;

    if (frame->chroma_format == 0 || !frame->cb || !frame->cr) return;
    switch (frame->chroma_format) {
    case 1: sub_x = 2; sub_y = 2; break;
    case 2: sub_x = 2; sub_y = 1; break;
    case 3: sub_x = 1; sub_y = 1; break;
    default: return;
    }
    c_stride = frame->c_stride;
    c_height = frame->c_height;
    c_width = frame->c_width;
    if (c_stride <= 0 || c_height <= 0 || c_width <= 0) return;

    x_step_vert = (uint32_t)(8 * sub_x);
    y_step_vert = (uint32_t)(4 * sub_y);
    x_step_horiz = (uint32_t)(4 * sub_x);
    y_step_horiz = (uint32_t)(8 * sub_y);

    for (x = x_step_vert; x < (uint32_t)w; x += x_step_vert) {
        for (y = 0; y < (uint32_t)h; y += y_step_vert) {
            uint32_t bx = x / 4, by = y / 4;
            size_t idx = (size_t)by * deblock_stride + bx;
            int bs, qp_q, qp_p, c_idx;
            uint32_t cx, cy;

            if ((flags[idx] & (uint8_t)vert_edge_mask) == 0) continue;
            bs = compute_bs_i();
            if (bs < 2) continue;
            qp_q = (int)qp_map[idx];
            qp_p = bx > 0 ? (int)qp_map[(size_t)by * deblock_stride + (bx - 1)] : qp_q;
            cx = x / (uint32_t)sub_x;
            cy = y / (uint32_t)sub_y;

            if (cx < 2 || (int)cx + 1 >= c_stride) continue;
            for (c_idx = 0; c_idx < 2; c_idx++) {
                int qp_offset = c_idx == 0 ? cb_qp_offset : cr_qp_offset;
                int qp_i = ((qp_q + qp_p + 1) >> 1) + qp_offset;
                int qp_c = chroma_qp_mapping(qp_i);
                int q_tc = clampi(qp_c + 2 + tc_offset, 0, 53);
                int tc = ((int)TC_PRIME[q_tc]) << (bit_depth_c - 8);
                uint16_t *plane = c_idx == 0 ? frame->cb : frame->cr;
                uint32_t k, num;
                size_t ci = (size_t)cx;

                if (tc == 0) continue;
                num = 4;
                if (cy + num > (uint32_t)c_height) num = (uint32_t)c_height - cy;

                if (num == 4 &&
                    heic_simd_chroma_edge4(plane, c_stride,
                                           (size_t)cy * (size_t)c_stride + ci, 1, tc,
                                           max_val, 1))
                    continue;
                for (k = 0; k < num; k++) {
                    size_t base = ((size_t)cy + k) * (size_t)c_stride + ci;
                    int p1 = plane[base - 2];
                    int p0 = plane[base - 1];
                    int q0 = plane[base];
                    int q1 = plane[base + 1];
                    int delta = ((q0 - p0) * 4 + p1 - q1 + 4) >> 3;
                    int p0n, q0n;
                    if (delta > tc) delta = tc;
                    else if (delta < -tc) delta = -tc;
                    p0n = p0 + delta;
                    q0n = q0 - delta;
                    if (p0n < 0) p0n = 0;
                    else if (p0n > max_val) p0n = max_val;
                    if (q0n < 0) q0n = 0;
                    else if (q0n > max_val) q0n = max_val;
                    plane[base - 1] = (uint16_t)p0n;
                    plane[base] = (uint16_t)q0n;
                }
            }
        }
    }

    for (y = y_step_horiz; y < (uint32_t)h; y += y_step_horiz) {
        for (x = 0; x < (uint32_t)w; x += x_step_horiz) {
            uint32_t bx = x / 4, by = y / 4;
            size_t idx = (size_t)by * deblock_stride + bx;
            int bs, qp_q, qp_p, c_idx;
            uint32_t cx, cy;

            if ((flags[idx] & (uint8_t)horiz_edge_mask) == 0) continue;
            bs = compute_bs_i();
            if (bs < 2) continue;
            qp_q = (int)qp_map[idx];
            qp_p = by > 0 ? (int)qp_map[(size_t)(by - 1) * deblock_stride + bx] : qp_q;
            cx = x / (uint32_t)sub_x;
            cy = y / (uint32_t)sub_y;
            if (cy < 2 || (int)cy + 1 >= c_height) continue;

            for (c_idx = 0; c_idx < 2; c_idx++) {
                int qp_offset = c_idx == 0 ? cb_qp_offset : cr_qp_offset;
                int qp_i = ((qp_q + qp_p + 1) >> 1) + qp_offset;
                int qp_c = chroma_qp_mapping(qp_i);
                int q_tc = clampi(qp_c + 2 + tc_offset, 0, 53);
                int tc = ((int)TC_PRIME[q_tc]) << (bit_depth_c - 8);
                uint16_t *plane = c_idx == 0 ? frame->cb : frame->cr;
                uint32_t k, num;
                size_t row_q = (size_t)cy;
                size_t row_p = row_q - 1;
                size_t cs = (size_t)c_stride;

                if (tc == 0) continue;
                num = 4;
                if (cx + num > (uint32_t)c_width) num = (uint32_t)c_width - cx;

                if (num == 4 &&
                    heic_simd_chroma_edge4(plane, c_stride, row_q * cs + (size_t)cx,
                                           (int)cs, tc, max_val, 0))
                    continue;
                for (k = 0; k < num; k++) {
                    size_t col = (size_t)(cx + k);
                    int p1 = plane[(row_p - 1) * cs + col];
                    int p0 = plane[row_p * cs + col];
                    int q0 = plane[row_q * cs + col];
                    int q1 = plane[(row_q + 1) * cs + col];
                    int delta = ((q0 - p0) * 4 + p1 - q1 + 4) >> 3;
                    int p0n, q0n;
                    if (delta > tc) delta = tc;
                    else if (delta < -tc) delta = -tc;
                    p0n = p0 + delta;
                    q0n = q0 - delta;
                    if (p0n < 0) p0n = 0;
                    else if (p0n > max_val) p0n = max_val;
                    if (q0n < 0) q0n = 0;
                    else if (q0n > max_val) q0n = max_val;
                    plane[row_p * cs + col] = (uint16_t)p0n;
                    plane[row_q * cs + col] = (uint16_t)q0n;
                }
            }
        }
    }
}

void heic_apply_deblock(heic_frame *frame, const uint8_t *flags, const int8_t *qp_map,
                        uint32_t deblock_stride, int beta_offset, int tc_offset,
                        int cb_qp_offset, int cr_qp_offset)
{
    uint32_t w, h, x, y;
    int vert_edge_mask = HEIC_DEBLOCK_FLAG_VERT | HEIC_DEBLOCK_PB_EDGE_VERT;
    int horiz_edge_mask = HEIC_DEBLOCK_FLAG_HORIZ | HEIC_DEBLOCK_PB_EDGE_HORIZ;

    if (!frame || !flags || !qp_map || deblock_stride == 0) return;
    w = (uint32_t)frame->width;
    h = (uint32_t)frame->height;
    if (w == 0 || h == 0) return;

    for (x = 8; x < w; x += 8) {
        for (y = 0; y < h; y += 4) {
            uint32_t bx = x / 4, by = y / 4;
            size_t idx = (size_t)by * deblock_stride + bx;
            int flags_v = flags[idx];
            int qp_q, qp_p, bs;
            if ((flags_v & vert_edge_mask) == 0) continue;
            qp_q = (int)qp_map[idx];
            qp_p = bx > 0 ? (int)qp_map[(size_t)by * deblock_stride + (bx - 1)] : qp_q;
            bs = compute_bs_i();
            if (bs > 0)
                filter_edge_luma(frame, x, y, 1, qp_p, qp_q, beta_offset, tc_offset, bs);
        }
    }

    for (y = 8; y < h; y += 8) {
        for (x = 0; x < w; x += 4) {
            uint32_t bx = x / 4, by = y / 4;
            size_t idx = (size_t)by * deblock_stride + bx;
            int flags_h = flags[idx];
            int qp_q, qp_p, bs;
            if ((flags_h & horiz_edge_mask) == 0) continue;
            qp_q = (int)qp_map[idx];
            qp_p = by > 0 ? (int)qp_map[(size_t)(by - 1) * deblock_stride + bx] : qp_q;
            bs = compute_bs_i();
            if (bs > 0)
                filter_edge_luma(frame, x, y, 0, qp_p, qp_q, beta_offset, tc_offset, bs);
        }
    }

    apply_chroma_deblocking(frame, flags, qp_map, deblock_stride, tc_offset,
                            cb_qp_offset, cr_qp_offset);
}

void heic_mark_tu_boundary(uint8_t *flags, uint32_t deblock_stride, uint32_t map_n,
                           uint32_t x, uint32_t y, uint32_t size)
{
    uint32_t bx = x / 4, by = y / 4, bs = size / 4, j, i;
    if (!flags || deblock_stride == 0 || size < 4) return;
    if (x > 0) {
        for (j = 0; j < bs; j++) {
            size_t idx = (size_t)(by + j) * deblock_stride + bx;
            if (idx < map_n) flags[idx] |= HEIC_DEBLOCK_FLAG_VERT;
        }
    }
    if (y > 0) {
        for (i = 0; i < bs; i++) {
            size_t idx = (size_t)by * deblock_stride + bx + i;
            if (idx < map_n) flags[idx] |= HEIC_DEBLOCK_FLAG_HORIZ;
        }
    }
}

void heic_store_deblock_qp(int8_t *qp_map, uint32_t deblock_stride, uint32_t map_n,
                           uint32_t x, uint32_t y, uint32_t size, int8_t qp)
{
    uint32_t bx = x / 4, by = y / 4, bs = size / 4, j, i;
    if (!qp_map || deblock_stride == 0 || size < 4) return;
    for (j = 0; j < bs; j++) {
        for (i = 0; i < bs; i++) {
            size_t idx = (size_t)(by + j) * deblock_stride + bx + i;
            if (idx < map_n) qp_map[idx] = qp;
        }
    }
}

void heic_mark_pb_boundary(uint8_t *flags, uint32_t deblock_stride, uint32_t map_n,
                           uint32_t x, uint32_t y, uint32_t width, uint32_t height,
                           int vertical)
{
    uint32_t bx, by, bs, j, i;
    if (!flags || deblock_stride == 0) return;
    if (vertical) {
        if (x == 0 || height < 4) return;
        bx = x / 4;
        by = y / 4;
        bs = height / 4;
        for (j = 0; j < bs; j++) {
            size_t idx = (size_t)(by + j) * deblock_stride + bx;
            if (idx < map_n) flags[idx] |= HEIC_DEBLOCK_PB_EDGE_VERT;
        }
    } else {
        if (y == 0 || width < 4) return;
        bx = x / 4;
        by = y / 4;
        bs = width / 4;
        for (i = 0; i < bs; i++) {
            size_t idx = (size_t)by * deblock_stride + bx + i;
            if (idx < map_n) flags[idx] |= HEIC_DEBLOCK_PB_EDGE_HORIZ;
        }
    }
}

int heic_hevc_decode(heic_ctx *ctx, const heic_hvcc *cfg,
                     const uint8_t *data, size_t len,
                     heic_frame *out, const heic_abort *ab)
{
    heic_nal *nals = NULL;
    int n_nals = 0, i;
    heic_sps sps;
    heic_pps pps;
    int have_sps = 0, have_pps = 0;
    int length_size;
    heic_nal param;
    int sub_w = 2, sub_h = 2;
    int decode_ok = 0;

    memset(out, 0, sizeof(*out));
    memset(&sps, 0, sizeof(sps));
    memset(&pps, 0, sizeof(pps));
    if (!ctx || !cfg || !data) return -1;
    if (heic_abort_check(ab)) return -1;

    for (i = 0; i < cfg->n_nal_units; i++) {
        if (heic_parse_single_nal(ctx, cfg->nal_units[i], cfg->nal_unit_lens[i], &param) != 0)
            continue;
        if (param.type == HEIC_NAL_SPS) {
            if (heic_parse_sps(ctx, param.payload, param.payload_len, &sps) == 0)
                have_sps = 1;
        } else if (param.type == HEIC_NAL_PPS) {
            heic_pps_free(ctx, &pps);
            if (heic_parse_pps(ctx, param.payload, param.payload_len, &pps) == 0)
                have_pps = 1;
        }
        heic_nal_free(ctx, &param);
    }
    if (!have_sps) {
        heic_error(ctx, HEIC_SEVERITY_ERROR, "missing SPS");
        return -1;
    }

    length_size = (int)cfg->length_size_minus_one + 1;
    if (heic_parse_length_prefixed(ctx, data, len, length_size, &nals, &n_nals) != 0) {
        heic_pps_free(ctx, &pps);
        return -1;
    }

    for (i = 0; i < n_nals; i++) {
        if (nals[i].type == HEIC_NAL_PPS) {
            heic_pps_free(ctx, &pps);
            if (heic_parse_pps(ctx, nals[i].payload, nals[i].payload_len, &pps) == 0)
                have_pps = 1;
        } else if (nals[i].type == HEIC_NAL_SPS) {
            if (heic_parse_sps(ctx, nals[i].payload, nals[i].payload_len, &sps) == 0)
                have_sps = 1;
        }
    }

    if (heic_frame_alloc(ctx, out, (int)sps.pic_width_in_luma_samples,
                         (int)sps.pic_height_in_luma_samples,
                         8 + sps.bit_depth_luma_minus8, sps.chroma_format_idc) != 0) {
        heic_nals_free(ctx, nals, n_nals);
        heic_pps_free(ctx, &pps);
        return -1;
    }
    out->full_range = sps.video_full_range_flag;

    if (sps.colour_description_present_flag && sps.matrix_coeffs != 2)
        out->matrix_coeffs = sps.matrix_coeffs;
    else
        out->matrix_coeffs = 6;
    out->color_primaries = sps.colour_description_present_flag ? sps.colour_primaries : 1;
    out->transfer_characteristics =
        sps.colour_description_present_flag ? sps.transfer_characteristics : 13;

    if (sps.conformance_window_flag) {
        switch (sps.chroma_format_idc) {
        case 0: sub_w = 1; sub_h = 1; break;
        case 1: sub_w = 2; sub_h = 2; break;
        case 2: sub_w = 2; sub_h = 1; break;
        case 3: sub_w = 1; sub_h = 1; break;
        }
        out->crop_left = (int)(sps.conf_win_left_offset * (uint32_t)sub_w);
        out->crop_right = (int)(sps.conf_win_right_offset * (uint32_t)sub_w);
        out->crop_top = (int)(sps.conf_win_top_offset * (uint32_t)sub_h);
        out->crop_bottom = (int)(sps.conf_win_bottom_offset * (uint32_t)sub_h);
    }

    {
        int has_slice = 0;
        for (i = 0; i < n_nals; i++) {
            heic_slice_header sh;
            const uint8_t *slice_data;
            size_t slice_len;
            if (!heic_nal_is_slice(nals[i].type) || nals[i].nuh_layer_id != 0) continue;
            has_slice = 1;
            if (!have_pps) {
                heic_error(ctx, HEIC_SEVERITY_ERROR, "missing PPS");
                break;
            }
            if (heic_parse_slice_header(ctx, &nals[i], &sps, &pps, &sh) != 0) break;
            heic_error(ctx, HEIC_SEVERITY_INFO,
                       "slice hdr OK type=%d qp_y=%d data_off=%u CTUs=%u entries=%u",
                       sh.slice_type, sh.slice_qp_y, (unsigned)sh.data_offset,
                       (unsigned)sps.pic_size_in_ctbs,
                       (unsigned)sh.num_entry_point_offsets);
            if (sh.data_offset >= nals[i].payload_len) {
                heic_error(ctx, HEIC_SEVERITY_ERROR, "empty slice data");
                heic_slice_header_free(ctx, &sh);
                break;
            }
            slice_data = nals[i].payload + sh.data_offset;
            slice_len = nals[i].payload_len - sh.data_offset;

            {
                uint32_t *eps = NULL;
                int ne = 0, e;
                size_t off = sh.data_offset;

                int ep_in_hdr = 0;
                for (e = 0; e < nals[i].n_ep_positions; e++) {

                    uint32_t p = nals[i].ep_positions[e];
                    uint32_t rbsp_pos = p - (uint32_t)e;
                    if (rbsp_pos < off) ep_in_hdr++;
                }
                for (e = 0; e < nals[i].n_ep_positions; e++) {
                    uint32_t p = nals[i].ep_positions[e];
                    uint32_t rbsp_pos = p - (uint32_t)e;
                    if (rbsp_pos < off) continue;

                    {
                        uint32_t slice_ebsp_start = (uint32_t)off + (uint32_t)ep_in_hdr;
                        uint32_t rel;
                        uint32_t *np;
                        if (p < slice_ebsp_start) continue;
                        rel = p - slice_ebsp_start;
                        np = (uint32_t *)heic_realloc_buf(
                            ctx, eps, (size_t)ne * sizeof(uint32_t),
                            (size_t)(ne + 1) * sizeof(uint32_t));
                        if (!np) break;
                        eps = np;
                        eps[ne++] = rel;
                    }
                }
                if (heic_hevc_decode_slice_i(ctx, &sps, &pps, &sh, slice_data, slice_len,
                                            eps, ne, out, ab)
                    == 0)
                    decode_ok = 1;
                heic_free_buf(ctx, eps);
            }
            heic_slice_header_free(ctx, &sh);
            break;
        }
        if (!has_slice)
            heic_error(ctx, HEIC_SEVERITY_ERROR, "no VCL slice NAL");
    }

    heic_nals_free(ctx, nals, n_nals);
    heic_pps_free(ctx, &pps);
    if (!decode_ok) {
        heic_frame_free(ctx, out);
        return -1;
    }
    return 0;
}

#ifdef HEIC_HAVE_DAV1D
#include <dav1d/dav1d.h>
#include <string.h>

#if defined(_M_X64) || defined(__x86_64__) || defined(_M_IX86) || defined(__i386__)
#define HEIC_AV1_X86 1
#include <emmintrin.h>
#include <smmintrin.h>
#else
#define HEIC_AV1_X86 0
#endif

void heic_dav1d_ctx_close(heic_ctx *ctx)
{
    if (!ctx || !ctx->dav1d_ctx) return;
    dav1d_close((Dav1dContext **)&ctx->dav1d_ctx);
    ctx->dav1d_ctx = NULL;
}

static void dav1d_data_free_nop(const uint8_t *buf, void *cookie)
{
    (void)buf;
    (void)cookie;
}

static void plane8_to_u16(uint16_t *dst, const uint8_t *src, int w, int h,
                          int dst_stride, int src_stride)
{
    int y, x;
    for (y = 0; y < h; y++) {
        const uint8_t *s = src + (size_t)y * (size_t)src_stride;
        uint16_t *d = dst + (size_t)y * (size_t)dst_stride;
        x = 0;
#if HEIC_AV1_X86

        for (; x + 16 <= w; x += 16) {
            __m128i b = _mm_loadu_si128((const __m128i *)(s + x));
            __m128i lo = _mm_cvtepu8_epi16(b);
            __m128i hi = _mm_cvtepu8_epi16(_mm_srli_si128(b, 8));
            _mm_storeu_si128((__m128i *)(d + x), lo);
            _mm_storeu_si128((__m128i *)(d + x + 8), hi);
        }
        for (; x + 8 <= w; x += 8) {
            __m128i b = _mm_loadl_epi64((const __m128i *)(s + x));
            _mm_storeu_si128((__m128i *)(d + x), _mm_cvtepu8_epi16(b));
        }
#endif
        for (; x < w; x++) d[x] = s[x];
    }
}

static int picture_to_frame(heic_ctx *ctx, const Dav1dPicture *pic, heic_frame *out)
{
    int w = pic->p.w;
    int h = pic->p.h;
    int bd = pic->p.bpc;
    int chroma;
    const uint8_t *ys, *us, *vs;
    int y_stride, u_stride, v_stride;

    if (w <= 0 || h <= 0) return -1;
    switch (pic->p.layout) {
    case DAV1D_PIXEL_LAYOUT_I400: chroma = 0; break;
    case DAV1D_PIXEL_LAYOUT_I420: chroma = 1; break;
    case DAV1D_PIXEL_LAYOUT_I422: chroma = 2; break;
    case DAV1D_PIXEL_LAYOUT_I444: chroma = 3; break;
    default: chroma = 1; break;
    }
    if (heic_frame_alloc(ctx, out, w, h, bd, chroma) != 0) return -1;

    out->full_range = pic->seq_hdr && pic->seq_hdr->color_range;
    if (pic->seq_hdr) {
        out->matrix_coeffs = (uint8_t)pic->seq_hdr->mtrx;
        out->color_primaries = (uint8_t)pic->seq_hdr->pri;
        out->transfer_characteristics = (uint8_t)pic->seq_hdr->trc;
    }

    y_stride = (int)pic->stride[0];
    u_stride = (int)pic->stride[1];
    v_stride = (int)pic->stride[1];
    ys = (const uint8_t *)pic->data[0];
    us = (const uint8_t *)pic->data[1];
    vs = (const uint8_t *)pic->data[2];

    if (bd == 8) {
        plane8_to_u16(out->y, ys, w, h, out->y_stride, y_stride);
        if (chroma > 0 && us && vs && out->cb && out->cr) {
            int cw = out->c_width, ch = out->c_height;
            plane8_to_u16(out->cb, us, cw, ch, out->c_stride, u_stride);
            plane8_to_u16(out->cr, vs, cw, ch, out->c_stride, v_stride);
        }
    } else {
        int y;

        for (y = 0; y < h; y++) {
            const uint16_t *src =
                (const uint16_t *)(ys + (size_t)y * (size_t)y_stride);
            uint16_t *dst = out->y + (size_t)y * (size_t)out->y_stride;
            memcpy(dst, src, (size_t)w * sizeof(uint16_t));
        }
        if (chroma > 0 && us && vs && out->cb && out->cr) {
            int cw = out->c_width, ch = out->c_height;
            for (y = 0; y < ch; y++) {
                const uint16_t *su =
                    (const uint16_t *)(us + (size_t)y * (size_t)u_stride);
                const uint16_t *sv =
                    (const uint16_t *)(vs + (size_t)y * (size_t)v_stride);
                uint16_t *du = out->cb + (size_t)y * (size_t)out->c_stride;
                uint16_t *dv = out->cr + (size_t)y * (size_t)out->c_stride;
                memcpy(du, su, (size_t)cw * sizeof(uint16_t));
                memcpy(dv, sv, (size_t)cw * sizeof(uint16_t));
            }
        }
    }
    return 0;
}

static Dav1dContext *ensure_dav1d(heic_ctx *ctx)
{
    Dav1dSettings settings;
    Dav1dContext *c;

    if (ctx->dav1d_ctx) return (Dav1dContext *)ctx->dav1d_ctx;

    dav1d_default_settings(&settings);

    settings.n_threads = 1;
    settings.apply_grain = 0;
    settings.max_frame_delay = 1;
    if (ctx->limits.max_pixels)
        settings.frame_size_limit = (unsigned)ctx->limits.max_pixels;

    c = NULL;
    if (dav1d_open(&c, &settings) != 0) return NULL;
    ctx->dav1d_ctx = c;
    return c;
}

static int send_all(Dav1dContext *c, Dav1dData *pd)
{
    while (pd->sz > 0) {
        int res = dav1d_send_data(c, pd);
        if (res == 0) continue;
        if (res == DAV1D_ERR(EAGAIN)) {

            return 0;
        }
        return res;
    }
    return 0;
}

int heic_av1_decode(heic_ctx *ctx, const heic_av1c *cfg,
                    const uint8_t *data, size_t len,
                    heic_frame *out, const heic_abort *ab)
{
    Dav1dContext *c = NULL;
    Dav1dData d_cfg, d_sample;
    Dav1dPicture pic;
    int rc = -1;
    int got = 0;
    int have_cfg = 0;

    memset(out, 0, sizeof(*out));
    memset(&d_cfg, 0, sizeof(d_cfg));
    memset(&d_sample, 0, sizeof(d_sample));
    if (!ctx || !data || len == 0) return -1;
    if (heic_abort_check(ab)) return -1;

    c = ensure_dav1d(ctx);
    if (!c) {
        heic_error(ctx, HEIC_SEVERITY_ERROR, "dav1d_open failed");
        return -1;
    }

    if (cfg && cfg->config_obus && cfg->config_obus_len > 0) {
        if (dav1d_data_wrap(&d_cfg, cfg->config_obus, cfg->config_obus_len,
                            dav1d_data_free_nop, NULL) != 0) {
            heic_error(ctx, HEIC_SEVERITY_ERROR, "dav1d_data_wrap(config) failed");
            return -1;
        }
        have_cfg = 1;
        if (send_all(c, &d_cfg) < 0) {
            heic_error(ctx, HEIC_SEVERITY_ERROR, "dav1d_send_data(config) failed");
            if (d_cfg.sz) dav1d_data_unref(&d_cfg);
            heic_dav1d_ctx_close(ctx);
            return -1;
        }
    }

    if (dav1d_data_wrap(&d_sample, data, len, dav1d_data_free_nop, NULL) != 0) {
        heic_error(ctx, HEIC_SEVERITY_ERROR, "dav1d_data_wrap(sample) failed");
        if (have_cfg && d_cfg.sz) dav1d_data_unref(&d_cfg);
        return -1;
    }

    memset(&pic, 0, sizeof(pic));
    for (;;) {
        int res;
        if (heic_abort_check(ab)) break;

        if (have_cfg && d_cfg.sz > 0) {
            if (send_all(c, &d_cfg) < 0) {
                heic_error(ctx, HEIC_SEVERITY_ERROR, "dav1d_send_data(config) failed");
                if (d_cfg.sz) dav1d_data_unref(&d_cfg);
                if (d_sample.sz) dav1d_data_unref(&d_sample);
                heic_dav1d_ctx_close(ctx);
                return -1;
            }
        }
        if (d_sample.sz > 0) {
            res = send_all(c, &d_sample);
            if (res < 0) {
                heic_error(ctx, HEIC_SEVERITY_ERROR, "dav1d_send_data failed (%d)", res);
                if (d_sample.sz) dav1d_data_unref(&d_sample);
                if (have_cfg && d_cfg.sz) dav1d_data_unref(&d_cfg);
                heic_dav1d_ctx_close(ctx);
                return -1;
            }
        }

        res = dav1d_get_picture(c, &pic);
        if (res == 0) {
            rc = picture_to_frame(ctx, &pic, out);
            dav1d_picture_unref(&pic);
            got = 1;
            break;
        }
        if (res != DAV1D_ERR(EAGAIN)) {
            heic_error(ctx, HEIC_SEVERITY_ERROR, "dav1d_get_picture failed (%d)", res);
            heic_dav1d_ctx_close(ctx);
            c = NULL;
            break;
        }

        if (d_sample.sz == 0 && (!have_cfg || d_cfg.sz == 0)) {
            res = dav1d_get_picture(c, &pic);
            if (res == 0) {
                rc = picture_to_frame(ctx, &pic, out);
                dav1d_picture_unref(&pic);
                got = 1;
            }
            break;
        }
    }
    if (d_sample.sz) dav1d_data_unref(&d_sample);
    if (have_cfg && d_cfg.sz) dav1d_data_unref(&d_cfg);

    if (!got) {
        heic_error(ctx, HEIC_SEVERITY_ERROR, "dav1d produced no picture");
        rc = -1;
    }

    if (c) dav1d_flush(c);
    return rc;
}

#else

void heic_dav1d_ctx_close(heic_ctx *ctx)
{
    if (ctx) ctx->dav1d_ctx = NULL;
}

int heic_av1_decode(heic_ctx *ctx, const heic_av1c *cfg,
                    const uint8_t *data, size_t len,
                    heic_frame *out, const heic_abort *ab)
{
    (void)cfg;
    (void)data;
    (void)len;
    (void)ab;
    memset(out, 0, sizeof(*out));
    if (ctx)
        heic_error(ctx, HEIC_SEVERITY_ERROR,
                   "AV1 decode requires dav1d (build with HEIC_HAVE_DAV1D)");
    return -1;
}

#endif

#ifdef HEIC_HAVE_ZLIB
#include <zlib.h>
#endif
#ifdef HEIC_HAVE_BROTLI
#include <brotli/decode.h>
#endif

enum {
    HEIC_UNCI_MONO = 0,
    HEIC_UNCI_Y = 1,
    HEIC_UNCI_CB = 2,
    HEIC_UNCI_CR = 3,
    HEIC_UNCI_R = 4,
    HEIC_UNCI_G = 5,
    HEIC_UNCI_B = 6,
    HEIC_UNCI_A = 7,
    HEIC_UNCI_PAD = 8
};

typedef struct {
    const uint8_t *data;
    size_t         len;
    size_t         bit_pos;
} unci_br;

static void br_init(unci_br *br, const uint8_t *d, size_t n)
{
    br->data = d;
    br->len = n;
    br->bit_pos = 0;
}

static void br_skip_bits(unci_br *br, int n)
{
    if (n > 0) br->bit_pos += (size_t)n;
}

static void br_byte_align(unci_br *br)
{
    if (br->bit_pos & 7)
        br->bit_pos = (br->bit_pos + 7) & ~(size_t)7;
}

static int br_get_bits(unci_br *br, int n)
{
    int v = 0, i;
    if (n <= 0 || n > 16) return 0;
    for (i = 0; i < n; i++) {
        size_t byte = br->bit_pos >> 3;
        int bit = 7 - (int)(br->bit_pos & 7);
        int b = 0;
        if (byte < br->len) b = (br->data[byte] >> bit) & 1;
        v = (v << 1) | b;
        br->bit_pos++;
    }
    return v;
}

static void br_skip_bytes(unci_br *br, size_t n)
{
    br_byte_align(br);
    br->bit_pos += n * 8;
}

static int type_to_plane(uint16_t t)
{
    switch (t) {
    case HEIC_UNCI_MONO:
    case HEIC_UNCI_Y:
    case HEIC_UNCI_G:
        return 0;
    case HEIC_UNCI_CB:
    case HEIC_UNCI_B:
        return 1;
    case HEIC_UNCI_CR:
    case HEIC_UNCI_R:
        return 2;
    case HEIC_UNCI_A:
        return 3;
    default:
        return -1;
    }
}

static uint16_t resolve_type(const heic_uncc_comp *c, const heic_cmpd *cmpd)
{
    uint16_t idx = c->component_index;
    if (cmpd && cmpd->types && idx < (uint16_t)cmpd->n_types)
        return cmpd->types[idx];
    return idx;
}

static uint16_t expand8(int val, int bit_depth)
{
    int maxv;
    if (bit_depth > 8) return (uint16_t)((val >> (bit_depth - 8)) & 0xFF);
    if (bit_depth == 8) return (uint16_t)(val & 0xFF);
    if (bit_depth <= 0) return 0;
    maxv = (1 << bit_depth) - 1;
    if (maxv <= 0) return 0;
    return (uint16_t)((val * 255 + maxv / 2) / maxv);
}

#if defined(HEIC_HAVE_ZLIB) || defined(HEIC_HAVE_BROTLI)

static int unci_append_bytes(heic_ctx *ctx, uint8_t **dst, size_t *dst_len, size_t *dst_cap,
                             const uint8_t *src, size_t n)
{
    size_t need;
    uint8_t *p;
    if (!n) return 0;
    need = *dst_len + n;
    if (need < *dst_len) return -1;
    if (need > *dst_cap) {
        size_t ncap = *dst_cap ? *dst_cap : 4096;
        while (ncap < need) {
            if (ncap > (SIZE_MAX / 2)) return -1;
            ncap *= 2;
        }
        p = (uint8_t *)heic_realloc_buf(ctx, *dst, *dst_cap, ncap);
        if (!p) return -1;
        *dst = p;
        *dst_cap = ncap;
    }
    memcpy(*dst + *dst_len, src, n);
    *dst_len = need;
    return 0;
}
#endif

#ifdef HEIC_HAVE_ZLIB
static int inflate_zlib_or_deflate(heic_ctx *ctx, const uint8_t *src, size_t src_len,
                                   uint8_t **dst, size_t *dst_len, size_t *dst_cap,
                                   int raw_deflate)
{
    z_stream z;
    int rc;
    uint8_t chunk[1024];

    memset(&z, 0, sizeof(z));
    z.next_in = (Bytef *)src;
    z.avail_in = (uInt)src_len;

    rc = inflateInit2(&z, raw_deflate ? -15 : 15);
    if (rc != Z_OK) {
        heic_error(ctx, HEIC_SEVERITY_ERROR, "unci: inflateInit failed");
        return -1;
    }
    for (;;) {
        z.next_out = (Bytef *)chunk;
        z.avail_out = (uInt)sizeof(chunk);
        rc = inflate(&z, Z_NO_FLUSH);
        if (rc == Z_STREAM_ERROR || rc == Z_DATA_ERROR || rc == Z_NEED_DICT) {
            inflateEnd(&z);
            heic_error(ctx, HEIC_SEVERITY_ERROR, "unci: zlib/deflate decompress failed");
            return -1;
        }
        {
            size_t got = sizeof(chunk) - (size_t)z.avail_out;
            if (got && unci_append_bytes(ctx, dst, dst_len, dst_cap, chunk, got) != 0) {
                inflateEnd(&z);
                return -1;
            }
        }
        if (rc == Z_STREAM_END) break;
        if (rc == Z_BUF_ERROR && z.avail_in == 0) break;
        if (rc != Z_OK && rc != Z_BUF_ERROR) {
            inflateEnd(&z);
            heic_error(ctx, HEIC_SEVERITY_ERROR, "unci: zlib/deflate decompress failed");
            return -1;
        }
    }
    inflateEnd(&z);
    return 0;
}
#endif

#ifdef HEIC_HAVE_BROTLI
static int inflate_brotli(heic_ctx *ctx, const uint8_t *src, size_t src_len,
                          uint8_t **dst, size_t *dst_len, size_t *dst_cap)
{
    BrotliDecoderState *st;
    BrotliDecoderResult res;
    const uint8_t *next_in = src;
    size_t avail_in = src_len;
    uint8_t chunk[1024];
    uint8_t *next_out = chunk;
    size_t avail_out = sizeof(chunk);

    st = BrotliDecoderCreateInstance(NULL, NULL, NULL);
    if (!st) {
        heic_error(ctx, HEIC_SEVERITY_ERROR, "unci: brotli init failed");
        return -1;
    }
    for (;;) {
        res = BrotliDecoderDecompressStream(st, &avail_in, &next_in, &avail_out, &next_out,
                                            NULL);
        if (res == BROTLI_DECODER_RESULT_NEEDS_MORE_OUTPUT ||
            res == BROTLI_DECODER_RESULT_SUCCESS) {
            size_t got = sizeof(chunk) - avail_out;
            if (got && unci_append_bytes(ctx, dst, dst_len, dst_cap, chunk, got) != 0) {
                BrotliDecoderDestroyInstance(st);
                return -1;
            }
            next_out = chunk;
            avail_out = sizeof(chunk);
            if (res == BROTLI_DECODER_RESULT_SUCCESS) break;
        } else if (res == BROTLI_DECODER_RESULT_NEEDS_MORE_INPUT) {
            BrotliDecoderDestroyInstance(st);
            heic_error(ctx, HEIC_SEVERITY_ERROR, "unci: brotli truncated input");
            return -1;
        } else {
            BrotliDecoderDestroyInstance(st);
            heic_error(ctx, HEIC_SEVERITY_ERROR, "unci: brotli decompress failed");
            return -1;
        }
    }
    BrotliDecoderDestroyInstance(st);
    return 0;
}
#endif

static int unci_decompress_unit(heic_ctx *ctx, heic_fourcc ct, const uint8_t *src,
                                size_t src_len, uint8_t **dst, size_t *dst_len,
                                size_t *dst_cap)
{
    if (ct == HEIC_FCC('z', 'l', 'i', 'b')) {
#ifdef HEIC_HAVE_ZLIB
        return inflate_zlib_or_deflate(ctx, src, src_len, dst, dst_len, dst_cap, 0);
#else
        (void)src;
        (void)src_len;
        (void)dst;
        (void)dst_len;
        (void)dst_cap;
        heic_error(ctx, HEIC_SEVERITY_ERROR, "unci: zlib needs HEIC_HAVE_ZLIB");
        return -1;
#endif
    }
    if (ct == HEIC_FCC('d', 'e', 'f', 'l')) {
#ifdef HEIC_HAVE_ZLIB
        return inflate_zlib_or_deflate(ctx, src, src_len, dst, dst_len, dst_cap, 1);
#else
        (void)src;
        (void)src_len;
        (void)dst;
        (void)dst_len;
        (void)dst_cap;
        heic_error(ctx, HEIC_SEVERITY_ERROR, "unci: deflate needs HEIC_HAVE_ZLIB");
        return -1;
#endif
    }
    if (ct == HEIC_FCC('b', 'r', 'o', 't')) {
#ifdef HEIC_HAVE_BROTLI
        return inflate_brotli(ctx, src, src_len, dst, dst_len, dst_cap);
#else
        (void)src;
        (void)src_len;
        (void)dst;
        (void)dst_len;
        (void)dst_cap;
        heic_error(ctx, HEIC_SEVERITY_ERROR, "unci: brotli needs HEIC_HAVE_BROTLI");
        return -1;
#endif
    }
    heic_error(ctx, HEIC_SEVERITY_ERROR, "unci: compression type not supported");
    return -1;
}

static int unci_decompress_payload(heic_ctx *ctx, const heic_cmpc *cmpc,
                                   const heic_icef *icef, const uint8_t *data,
                                   size_t len, uint8_t **out_buf, size_t *out_len)
{
    uint8_t *dst = NULL;
    size_t dst_len = 0, dst_cap = 0;
    heic_fourcc ct;

    if (!cmpc || !data || !out_buf || !out_len) return -1;
    *out_buf = NULL;
    *out_len = 0;
    ct = cmpc->compression_type;

    if (icef && icef->n_units > 0 && icef->units) {
        int i;
        for (i = 0; i < icef->n_units; i++) {
            uint64_t off = icef->units[i].offset;
            uint64_t sz = icef->units[i].size;
            if (off > len || sz > len - off) {
                heic_error(ctx, HEIC_SEVERITY_ERROR, "unci: icef unit out of range");
                heic_free_buf(ctx, dst);
                return -1;
            }
            if (unci_decompress_unit(ctx, ct, data + (size_t)off, (size_t)sz, &dst,
                                     &dst_len, &dst_cap) != 0) {
                heic_free_buf(ctx, dst);
                return -1;
            }
        }
    } else {

        if (unci_decompress_unit(ctx, ct, data, len, &dst, &dst_len, &dst_cap) != 0) {
            heic_free_buf(ctx, dst);
            return -1;
        }
    }
    *out_buf = dst;
    *out_len = dst_len;
    return 0;
}

static int read_comp_sample(unci_br *br, int bit_depth, uint8_t align)
{
    if (align) {
        int pad = (int)align * 8 - bit_depth;
        br_byte_align(br);
        if (pad > 0) br_skip_bits(br, pad);
    }
    return br_get_bits(br, bit_depth);
}

static void store_sample(heic_frame *out, int plane, uint32_t x, uint32_t y, uint16_t v8)
{
    size_t di;
    if (plane == 0) {
        di = (size_t)y * (size_t)out->y_stride + x;
        if (di < (size_t)out->y_stride * (size_t)out->height) out->y[di] = v8;
    } else if (plane == 1) {
        di = (size_t)y * (size_t)out->c_stride + x;
        if (di < (size_t)out->c_stride * (size_t)out->c_height) out->cb[di] = v8;
    } else if (plane == 2) {
        di = (size_t)y * (size_t)out->c_stride + x;
        if (di < (size_t)out->c_stride * (size_t)out->c_height) out->cr[di] = v8;
    } else if (plane == 3 && out->a) {
        di = (size_t)y * (size_t)out->a_stride + x;
        if (di < (size_t)out->a_stride * (size_t)out->height) out->a[di] = v8;
    }
}

static void plane_tile_dims(int plane, int sampling, uint32_t tw, uint32_t th,
                            uint32_t *pw, uint32_t *ph)
{
    if (plane == 1 || plane == 2) {
        if (sampling == 1) {
            *pw = tw / 2;
            *ph = th;
            return;
        }
        if (sampling == 2) {
            *pw = tw / 2;
            *ph = th / 2;
            return;
        }
    }
    *pw = tw;
    *ph = th;
}

static int sampling_to_chroma(int sampling)
{
    if (sampling == 1) return 2;
    if (sampling == 2) return 1;
    return 3;
}

static int br_read_block_u64(unci_br *br, uint8_t block_size, int little_endian,
                             uint64_t *out)
{
    size_t byte;
    uint32_t b;
    uint64_t v = 0;
    br_byte_align(br);
    byte = br->bit_pos >> 3;
    if (block_size == 0 || block_size > 8) return -1;
    if (byte + block_size > br->len) return -1;
    if (little_endian) {
        for (b = 0; b < block_size; b++)
            v |= (uint64_t)br->data[byte + b] << (b * 8);
    } else {
        for (b = 0; b < block_size; b++)
            v = (v << 8) | br->data[byte + b];
    }
    br->bit_pos += (size_t)block_size * 8u;
    *out = v;
    return 0;
}

static int decode_block_component(heic_ctx *ctx, const heic_uncc *uncc,
                                  const int *plane_map, const int *bit_depth,
                                  int ncomp, uint32_t tile_cols, uint32_t tile_rows,
                                  uint32_t tile_w, uint32_t tile_h, unci_br *br,
                                  heic_frame *out, const heic_abort *ab)
{
    uint8_t bsz = uncc->block_size;
    uint32_t ty, tx, y, x;
    int i;
    if (bsz < 1 || bsz > 8 || uncc->pixel_size != 0 || uncc->sampling_type != 0 ||
        uncc->components_little_endian) {
        heic_error(ctx, HEIC_SEVERITY_ERROR,
                   "unci: block+component constraints not met");
        return -1;
    }
    for (i = 0; i < ncomp; i++) {
        int bd = bit_depth[i];
        int bits = (int)bsz * 8;
        if (bd <= bits / 2 || bd > bits) {
            heic_error(ctx, HEIC_SEVERITY_ERROR,
                       "unci: block component bit depth out of range");
            return -1;
        }
    }
    for (ty = 0; ty < tile_rows; ty++) {
        for (tx = 0; tx < tile_cols; tx++) {
            size_t tile_start;
            if (heic_abort_check(ab)) return -1;
            br_byte_align(br);
            tile_start = br->bit_pos >> 3;
            for (i = 0; i < ncomp; i++) {
                int plane = plane_map[i];
                int bd = bit_depth[i];
                uint32_t shift =
                    uncc->block_pad_lsb ? (uint32_t)(bsz * 8 - bd) : 0;
                uint64_t mask = (bd >= 64) ? ~0ull : ((1ull << bd) - 1ull);
                uint32_t ox = tx * tile_w, oy = ty * tile_h;
                for (y = 0; y < tile_h; y++) {
                    size_t row_start = br->bit_pos >> 3;
                    for (x = 0; x < tile_w; x++) {
                        uint64_t blk;
                        uint32_t val;
                        if (br_read_block_u64(br, bsz, uncc->block_little_endian,
                                              &blk) != 0)
                            return -1;
                        val = (uint32_t)((blk >> shift) & mask);
                        if (plane >= 0)
                            store_sample(out, plane, ox + x, oy + y,
                                         expand8((int)val, bd));
                    }
                    br_byte_align(br);
                    if (uncc->row_align_size) {
                        size_t row_bytes = (br->bit_pos >> 3) - row_start;
                        if (row_bytes % uncc->row_align_size)
                            br_skip_bytes(br, uncc->row_align_size -
                                                   (row_bytes % uncc->row_align_size));
                    }
                }
            }
            if (uncc->tile_align_size) {
                size_t tile_bytes = (br->bit_pos >> 3) - tile_start;
                if (tile_bytes % uncc->tile_align_size)
                    br_skip_bytes(br, uncc->tile_align_size -
                                           (tile_bytes % uncc->tile_align_size));
            }
        }
    }
    return 0;
}

static int decode_block_pixel(heic_ctx *ctx, const heic_uncc *uncc,
                              const int *plane_map, const int *bit_depth, int ncomp,
                              uint32_t tile_cols, uint32_t tile_rows, uint32_t tile_w,
                              uint32_t tile_h, unci_br *br, heic_frame *out,
                              const heic_abort *ab)
{
    uint8_t bsz = uncc->block_size;
    uint32_t psz = uncc->pixel_size;
    uint32_t shifts[16];
    uint64_t masks[16];
    uint32_t ty, tx, y, x;
    int i;
    if (bsz == 0) bsz = (uint8_t)(psz > 255 ? 0 : psz);
    if (bsz < 1 || bsz > 8 || psz == 0 || uncc->sampling_type != 0 ||
        uncc->components_little_endian) {
        heic_error(ctx, HEIC_SEVERITY_ERROR, "unci: block+pixel constraints not met");
        return -1;
    }
    if (uncc->block_size != 0 && uncc->block_size != psz && psz < uncc->block_size) {
        heic_error(ctx, HEIC_SEVERITY_ERROR, "unci: pixel_size < block_size");
        return -1;
    }

    if (!uncc->block_pad_lsb) {
        uint32_t bit_offset = 0;
        for (i = 0; i < ncomp; i++) {
            int idx = uncc->block_reversed ? i : (ncomp - 1 - i);
            shifts[idx] = bit_offset;
            bit_offset += (uint32_t)bit_depth[idx];
            masks[idx] = (bit_depth[idx] >= 64)
                             ? ~0ull
                             : ((1ull << bit_depth[idx]) - 1ull);
        }
    } else {
        uint32_t total_bits = (uint32_t)bsz * 8u;
        uint32_t bit_offset = total_bits;
        for (i = 0; i < ncomp; i++) {
            int idx = uncc->block_reversed ? i : (ncomp - 1 - i);
            bit_offset -= (uint32_t)bit_depth[idx];
            shifts[idx] = bit_offset;
            masks[idx] = (bit_depth[idx] >= 64)
                             ? ~0ull
                             : ((1ull << bit_depth[idx]) - 1ull);
        }
    }

    for (ty = 0; ty < tile_rows; ty++) {
        for (tx = 0; tx < tile_cols; tx++) {
            size_t tile_start;
            if (heic_abort_check(ab)) return -1;
            br_byte_align(br);
            tile_start = br->bit_pos >> 3;
            for (y = 0; y < tile_h; y++) {
                size_t row_start = br->bit_pos >> 3;
                for (x = 0; x < tile_w; x++) {
                    uint64_t blk;
                    if (br_read_block_u64(br, bsz, uncc->block_little_endian, &blk) != 0)
                        return -1;
                    for (i = 0; i < ncomp; i++) {
                        int plane = plane_map[i];
                        uint32_t val =
                            (uint32_t)((blk >> shifts[i]) & masks[i]);
                        if (plane >= 0)
                            store_sample(out, plane, tx * tile_w + x, ty * tile_h + y,
                                         expand8((int)val, bit_depth[i]));
                    }
                    if (psz > bsz) br_skip_bytes(br, (size_t)(psz - bsz));
                }
                br_byte_align(br);
                if (uncc->row_align_size) {
                    size_t row_bytes = (br->bit_pos >> 3) - row_start;
                    if (row_bytes % uncc->row_align_size)
                        br_skip_bytes(br, uncc->row_align_size -
                                               (row_bytes % uncc->row_align_size));
                }
            }
            if (uncc->tile_align_size) {
                size_t tile_bytes = (br->bit_pos >> 3) - tile_start;
                if (tile_bytes % uncc->tile_align_size)
                    br_skip_bytes(br, uncc->tile_align_size -
                                           (tile_bytes % uncc->tile_align_size));
            }
        }
    }
    return 0;
}

int heic_unci_decode(heic_ctx *ctx, const heic_uncc *uncc, const heic_cmpc *cmpc,
                     const heic_cmpd *cmpd, const heic_icef *icef,
                     const uint8_t *data, size_t len, uint32_t width,
                     uint32_t height, heic_frame *out, const heic_abort *ab)
{
    int i, ncomp, has_rgb = 0, has_alpha = 0, has_yuv = 0;
    int plane_map[16], bit_depth[16];
    uint8_t align[16];
    uint32_t tile_cols, tile_rows, tile_w, tile_h;
    uint8_t *owned = NULL;
    const uint8_t *pix;
    size_t pix_len;
    unci_br br;
    uint32_t ty, tx, y, x;
    int interleave, sampling, chroma_fmt;

    memset(out, 0, sizeof(*out));
    if (!ctx || !uncc || !data || !width || !height) return -1;
    if (heic_abort_check(ab)) return -1;

    ncomp = uncc->n_components;
    if (ncomp <= 0 || ncomp > 16 || !uncc->components) {
        heic_error(ctx, HEIC_SEVERITY_ERROR, "unci: bad component count");
        return -1;
    }
    sampling = uncc->sampling_type;
    if (sampling != 0 && sampling != 1 && sampling != 2) {
        heic_error(ctx, HEIC_SEVERITY_ERROR, "unci: sampling type %d not supported",
                   sampling);
        return -1;
    }
    interleave = uncc->interleave_type;
    if (interleave != 0 && interleave != 1 && interleave != 2 && interleave != 3 &&
        interleave != 4) {
        heic_error(ctx, HEIC_SEVERITY_ERROR,
                   "unci: interleave type %d not supported", interleave);
        return -1;
    }

    if (interleave == 2 && sampling != 1 && sampling != 2) {
        heic_error(ctx, HEIC_SEVERITY_ERROR,
                   "unci: mixed interleave needs 4:2:2 or 4:2:0");
        return -1;
    }

    if (sampling != 0 && interleave != 0 && interleave != 2 && interleave != 4) {
        heic_error(ctx, HEIC_SEVERITY_ERROR,
                   "unci: subsampled chroma needs planar/mixed/tile-component");
        return -1;
    }

    if (uncc->pixel_size != 0 && interleave != 1) {
        heic_error(ctx, HEIC_SEVERITY_ERROR,
                   "unci: pixel_size only with pixel interleave");
        return -1;
    }
    if (uncc->block_size != 0 && interleave != 0 && interleave != 1) {
        heic_error(ctx, HEIC_SEVERITY_ERROR,
                   "unci: blocked layout only for component/pixel interleave");
        return -1;
    }

    for (i = 0; i < ncomp; i++) {
        const heic_uncc_comp *c = &uncc->components[i];
        uint16_t typ;
        int bd = (int)c->component_bit_depth_minus_one + 1;
        if (bd < 1 || bd > 16 || c->component_format != 0) {
            heic_error(ctx, HEIC_SEVERITY_ERROR,
                       "unci: only 1..16-bit unsigned integer components");
            return -1;
        }
        bit_depth[i] = bd;
        align[i] = c->component_align_size;
        typ = resolve_type(c, cmpd);
        if (typ >= 4 && typ <= 6) has_rgb = 1;
        if (typ >= 1 && typ <= 3) has_yuv = 1;
        if (typ == HEIC_UNCI_A) has_alpha = 1;
        plane_map[i] = type_to_plane(typ);
    }

    tile_cols = uncc->num_tile_cols_minus_one + 1;
    tile_rows = uncc->num_tile_rows_minus_one + 1;
    if (tile_cols == 0 || tile_rows == 0 || width % tile_cols || height % tile_rows) {
        heic_error(ctx, HEIC_SEVERITY_ERROR, "unci: invalid tile grid");
        return -1;
    }
    tile_w = width / tile_cols;
    tile_h = height / tile_rows;
    if (!tile_w || !tile_h) {
        heic_error(ctx, HEIC_SEVERITY_ERROR, "unci: zero tile size");
        return -1;
    }
    if (sampling == 1 && (tile_w & 1)) {
        heic_error(ctx, HEIC_SEVERITY_ERROR, "unci: 4:2:2 needs even tile width");
        return -1;
    }
    if (sampling == 2 && ((tile_w & 1) || (tile_h & 1))) {
        heic_error(ctx, HEIC_SEVERITY_ERROR, "unci: 4:2:0 needs even tile size");
        return -1;
    }

    pix = data;
    pix_len = len;
    if (cmpc) {
        if (unci_decompress_payload(ctx, cmpc, icef, data, len, &owned, &pix_len) != 0)
            return -1;
        pix = owned;
    }

    chroma_fmt = sampling_to_chroma(sampling);
    if (heic_frame_alloc(ctx, out, (int)width, (int)height, 8, chroma_fmt) != 0) {
        heic_free_buf(ctx, owned);
        return -1;
    }
    out->full_range = 1;

    out->matrix_coeffs = has_rgb ? 0 : (has_yuv || ncomp == 1) ? 6 : 0;
    out->color_primaries = 1;
    out->transfer_characteristics = 13;

    if (has_alpha) {
        size_t a_n = (size_t)width * height * sizeof(uint16_t);
        size_t k;
        out->a = (uint16_t *)heic_zalloc(ctx, a_n);
        if (!out->a) goto fail;
        out->a_stride = (int)width;
        for (k = 0; k < (size_t)width * height; k++) out->a[k] = 255;
    }

    br_init(&br, pix, pix_len);

    if (uncc->block_size != 0) {
        int brc;
        if (interleave == 0)
            brc = decode_block_component(ctx, uncc, plane_map, bit_depth, ncomp,
                                         tile_cols, tile_rows, tile_w, tile_h, &br,
                                         out, ab);
        else
            brc = decode_block_pixel(ctx, uncc, plane_map, bit_depth, ncomp,
                                     tile_cols, tile_rows, tile_w, tile_h, &br, out,
                                     ab);
        if (brc != 0) goto fail;
        goto done;
    }

    if (interleave == 4) {
        for (i = 0; i < ncomp; i++) {
            uint32_t tr, tc, ptw, pth;
            int plane = plane_map[i];
            if (heic_abort_check(ab)) goto fail;
            plane_tile_dims(plane, sampling, tile_w, tile_h, &ptw, &pth);
            for (tr = 0; tr < tile_rows; tr++) {
                for (tc = 0; tc < tile_cols; tc++) {
                    uint32_t ox = tc * ptw, oy = tr * pth;
                    size_t tile_start_bytes;
                    br_byte_align(&br);
                    tile_start_bytes = br.bit_pos >> 3;
                    for (y = 0; y < pth; y++) {
                        size_t row_start = br.bit_pos >> 3;
                        for (x = 0; x < ptw; x++) {
                            int val = read_comp_sample(&br, bit_depth[i], align[i]);
                            uint16_t v8 = expand8(val, bit_depth[i]);
                            if (plane >= 0)
                                store_sample(out, plane, ox + x, oy + y, v8);
                        }
                        br_byte_align(&br);
                        if (uncc->row_align_size) {
                            size_t row_bytes = (br.bit_pos >> 3) - row_start;
                            if (row_bytes % uncc->row_align_size)
                                br_skip_bytes(&br, uncc->row_align_size -
                                                       (row_bytes % uncc->row_align_size));
                        }
                    }
                    if (uncc->tile_align_size) {
                        size_t tile_bytes = (br.bit_pos >> 3) - tile_start_bytes;
                        if (tile_bytes % uncc->tile_align_size)
                            br_skip_bytes(&br, uncc->tile_align_size -
                                                   (tile_bytes % uncc->tile_align_size));
                    }
                }
            }
        }
        goto done;
    }

    for (ty = 0; ty < tile_rows; ty++) {
        for (tx = 0; tx < tile_cols; tx++) {
            size_t tile_start_bytes;
            if (heic_abort_check(ab)) goto fail;
            br_byte_align(&br);
            tile_start_bytes = br.bit_pos >> 3;

            if (interleave == 0) {

                for (i = 0; i < ncomp; i++) {
                    uint32_t ptw, pth, ox, oy;
                    int plane = plane_map[i];
                    plane_tile_dims(plane, sampling, tile_w, tile_h, &ptw, &pth);
                    ox = tx * ptw;
                    oy = ty * pth;
                    for (y = 0; y < pth; y++) {
                        size_t row_start = br.bit_pos >> 3;
                        for (x = 0; x < ptw; x++) {
                            int val = read_comp_sample(&br, bit_depth[i], align[i]);
                            uint16_t v8 = expand8(val, bit_depth[i]);
                            if (plane >= 0)
                                store_sample(out, plane, ox + x, oy + y, v8);
                        }
                        br_byte_align(&br);
                        if (uncc->row_align_size) {
                            size_t row_bytes = (br.bit_pos >> 3) - row_start;
                            if (row_bytes % uncc->row_align_size)
                                br_skip_bytes(&br, uncc->row_align_size -
                                                       (row_bytes % uncc->row_align_size));
                        }
                    }
                }
            } else if (interleave == 2) {

                int chroma_done = 0;
                for (i = 0; i < ncomp; i++) {
                    uint32_t ptw, pth, ox, oy;
                    int plane = plane_map[i];
                    plane_tile_dims(plane, sampling, tile_w, tile_h, &ptw, &pth);
                    ox = tx * ptw;
                    oy = ty * pth;
                    if (plane == 1 || plane == 2) {
                        int p0 = plane, p1 = (plane == 1) ? 2 : 1;
                        int bd = bit_depth[i];
                        if (chroma_done) continue;
                        for (y = 0; y < pth; y++) {
                            for (x = 0; x < ptw; x++) {

                                int v0 = br_get_bits(&br, bd);
                                int v1 = br_get_bits(&br, bd);
                                store_sample(out, p0, ox + x, oy + y, expand8(v0, bd));
                                store_sample(out, p1, ox + x, oy + y, expand8(v1, bd));
                            }
                            br_byte_align(&br);
                        }
                        chroma_done = 1;
                    } else {
                        for (y = 0; y < pth; y++) {
                            size_t row_start = br.bit_pos >> 3;
                            for (x = 0; x < ptw; x++) {
                                int val = read_comp_sample(&br, bit_depth[i], align[i]);
                                uint16_t v8 = expand8(val, bit_depth[i]);
                                if (plane >= 0)
                                    store_sample(out, plane, ox + x, oy + y, v8);
                            }
                            br_byte_align(&br);
                            if (uncc->row_align_size) {
                                size_t row_bytes = (br.bit_pos >> 3) - row_start;
                                if (row_bytes % uncc->row_align_size)
                                    br_skip_bytes(&br, uncc->row_align_size -
                                                           (row_bytes % uncc->row_align_size));
                            }
                        }
                    }
                }
            } else if (interleave == 1) {

                uint32_t ox = tx * tile_w, oy = ty * tile_h;
                for (y = 0; y < tile_h; y++) {
                    size_t row_start = br.bit_pos >> 3;
                    for (x = 0; x < tile_w; x++) {
                        size_t pix_start = br.bit_pos >> 3;
                        for (i = 0; i < ncomp; i++) {
                            int val = read_comp_sample(&br, bit_depth[i], align[i]);
                            uint16_t v8 = expand8(val, bit_depth[i]);
                            if (plane_map[i] >= 0)
                                store_sample(out, plane_map[i], ox + x, oy + y, v8);
                        }
                        if (uncc->pixel_size) {
                            size_t used;
                            br_byte_align(&br);
                            used = (br.bit_pos >> 3) - pix_start;
                            if (uncc->pixel_size < used) {
                                heic_error(ctx, HEIC_SEVERITY_ERROR,
                                           "unci: invalid pixel_size (too small)");
                                goto fail;
                            }
                            if (uncc->pixel_size > used)
                                br_skip_bytes(&br, uncc->pixel_size - used);
                        }
                    }
                    br_byte_align(&br);
                    if (uncc->row_align_size) {
                        size_t row_bytes = (br.bit_pos >> 3) - row_start;
                        if (row_bytes % uncc->row_align_size)
                            br_skip_bytes(&br, uncc->row_align_size -
                                                   (row_bytes % uncc->row_align_size));
                    }
                }
            } else {

                uint32_t ox = tx * tile_w, oy = ty * tile_h;
                for (y = 0; y < tile_h; y++) {
                    for (i = 0; i < ncomp; i++) {
                        size_t row_start = br.bit_pos >> 3;
                        for (x = 0; x < tile_w; x++) {
                            int val = read_comp_sample(&br, bit_depth[i], align[i]);
                            uint16_t v8 = expand8(val, bit_depth[i]);
                            if (plane_map[i] >= 0)
                                store_sample(out, plane_map[i], ox + x, oy + y, v8);
                        }
                        br_byte_align(&br);
                        if (uncc->row_align_size) {
                            size_t row_bytes = (br.bit_pos >> 3) - row_start;
                            if (row_bytes % uncc->row_align_size)
                                br_skip_bytes(&br, uncc->row_align_size -
                                                       (row_bytes % uncc->row_align_size));
                        }
                    }
                }
            }

            if (uncc->tile_align_size) {
                size_t tile_bytes = (br.bit_pos >> 3) - tile_start_bytes;
                if (tile_bytes % uncc->tile_align_size)
                    br_skip_bytes(&br, uncc->tile_align_size -
                                           (tile_bytes % uncc->tile_align_size));
            }
        }
    }

done:
    if (!has_rgb && !has_yuv && ncomp == 1) {
        size_t k, n = (size_t)out->c_width * (size_t)out->c_height;
        for (k = 0; k < n; k++) {
            out->cb[k] = 128;
            out->cr[k] = 128;
        }
        out->matrix_coeffs = 6;
    }
    heic_free_buf(ctx, owned);
    return 0;

fail:
    heic_frame_free(ctx, out);
    heic_free_buf(ctx, owned);
    return -1;
}
