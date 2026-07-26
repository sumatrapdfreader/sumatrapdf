/* heic.h -- plain-C HEIC/HEIF/AVIF decoder (port of imazen/heic).
 *
 * Decode-only. The caller supplies the entire file up-front as an
 * in-memory buffer that must outlive the heic_doc.
 *
 * Designed for SumatraPDF's image path (see AvifReader.cpp):
 *   - probe dimensions without full decode
 *   - decode primary image to RGB/BGR/RGBA/BGRA
 *   - extract EXIF (TIFF payload, HEIF 4-byte prefix stripped)
 *
 * Codecs:
 *   - HEVC (hvc1): pure-C port of imazen/heic
 *   - AV1  (av01): videolan dav1d (vendored / linked; not re-ported)
 */
#ifndef HEIC_H
#define HEIC_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ----- allocator + diagnostics (jbig2dec / djvudec style) ----- */

/* ctx identifies the heic_ctx the allocation belongs to so a caller can
   account per context. It is NULL only for the bootstrap alloc/free of the
   heic_ctx struct itself. */
typedef void *(*heic_alloc_cb)(void *user, void *ctx, size_t size);
typedef void  (*heic_free_cb)(void *user, void *ctx, void *ptr);

typedef enum {
    HEIC_SEVERITY_DEBUG,
    HEIC_SEVERITY_INFO,
    HEIC_SEVERITY_WARNING,
    HEIC_SEVERITY_ERROR,
    HEIC_SEVERITY_FATAL
} heic_severity;

/* msg is a NUL-terminated, already-formatted message. */
typedef void (*heic_error_cb)(void *user, heic_severity sev, const char *msg);

/* Cooperative abort token. Initialize with heic_abort_init; pass to
   heic_doc_decode_abortable. heic_abort_request (any thread) makes the
   decode exit promptly. Sticky: re-init to reuse. */
typedef struct {
    volatile int requested;
} heic_abort;

void heic_abort_init(heic_abort *ab);
void heic_abort_request(heic_abort *ab);

typedef struct heic_ctx heic_ctx;
typedef struct heic_doc heic_doc;

/* Idempotent process-wide setup. Call once from the main thread before
   concurrent decode. Safe to call again; also invoked by heic_doc_open. */
void heic_init(void);

/* Pass NULL for alloc/free to use malloc/free.
   Pass NULL for error to silently ignore diagnostics. */
heic_ctx *heic_ctx_new(heic_alloc_cb alloc, heic_free_cb free_cb,
                       heic_error_cb error, void *user);
void heic_ctx_free(heic_ctx *ctx);

/* Resource limits (0 = use default). Defaults match imazen/heic
   Limits::server_defaults: 16384x16384, 256 MP, 1 GiB. */
typedef struct {
    uint32_t max_width;         /* 0 = default 16384 */
    uint32_t max_height;        /* 0 = default 16384 */
    uint64_t max_pixels;        /* 0 = default 256e6 */
    size_t   max_memory_bytes;  /* 0 = default 1 GiB */
} heic_limits;

void heic_ctx_set_limits(heic_ctx *ctx, const heic_limits *limits);

/* ----- documents ----- */

/* Open a HEIC/HEIF/AVIF file over an in-memory buffer (NOT copied; must
   remain valid until heic_doc_close). Returns NULL on failure. */
heic_doc *heic_doc_open(heic_ctx *ctx, const uint8_t *data, size_t len);
void heic_doc_close(heic_doc *doc);

/* Brand / kind of the opened file. */
typedef enum {
    HEIC_KIND_UNKNOWN = 0,
    HEIC_KIND_HEIC,     /* HEVC still (heic/heix/…) */
    HEIC_KIND_AVIF,     /* AV1 still */
    HEIC_KIND_HEIF,     /* generic HEIF (mif1 without heic brand) */
    HEIC_KIND_SEQUENCE  /* image sequence (msf1/avis) — first frame only */
} heic_kind;

heic_kind heic_doc_kind(const heic_doc *doc);

typedef struct {
    uint32_t width;
    uint32_t height;
    int      bit_depth;       /* 8, 10, or 12 */
    int      has_alpha;
    int      has_exif;
    int      has_xmp;
    int      has_thumbnail;
    int      has_gain_map;
    /* CICP (0 = unspecified / unknown) */
    uint16_t color_primaries;
    uint16_t transfer_characteristics;
    uint16_t matrix_coefficients;
    int      full_range;      /* 1 = full, 0 = limited, -1 = unknown */
} heic_image_info;

/* Probe primary image without decoding pixels. Returns 0 on success. */
int heic_doc_info(const heic_doc *doc, heic_image_info *info);

/* ----- rendering ----- */

typedef enum {
    HEIC_FORMAT_RGB  = 3,  /* 3 bytes/pixel R,G,B */
    HEIC_FORMAT_RGBA = 4,  /* 4 bytes/pixel R,G,B,A (A=255 if no alpha) */
    HEIC_FORMAT_BGR  = 13, /* 3 bytes/pixel B,G,R */
    HEIC_FORMAT_BGRA = 14  /* 4 bytes/pixel B,G,R,A */
} heic_format;

typedef struct {
    uint32_t    width;
    uint32_t    height;
    heic_format format;
    int         stride;   /* bytes per row */
    uint8_t    *data;     /* owned; free with heic_image_destroy */
} heic_image;

/* Decode the primary image. Returns NULL on error; free with
   heic_image_destroy(ctx, img). 8-bit output; 10/12-bit sources are
   right-shifted (no EOTF / tone-map). */
heic_image *heic_doc_decode(heic_doc *doc, heic_format format);
heic_image *heic_doc_decode_abortable(heic_doc *doc, heic_format format,
                                      heic_abort *ab);
void heic_image_destroy(heic_ctx *ctx, heic_image *img);

/* Decode into a caller-owned buffer. buf_size must be at least
   heic_doc_output_size(doc, format). Returns 0 on success. */
size_t heic_doc_output_size(const heic_doc *doc, heic_format format);
int heic_doc_decode_into(heic_doc *doc, heic_format format,
                         uint8_t *buf, size_t buf_size, int stride);

/* Decode embedded thumbnail if present. Returns NULL if none. */
heic_image *heic_doc_decode_thumbnail(heic_doc *doc, heic_format format);

/* ----- metadata ----- */

/* EXIF: returns 1 and sets out/out_len to a freshly allocated TIFF
   payload (HEIF 4-byte prefix stripped). Caller frees with
   heic_free(ctx, *out) or free() if default allocator. Returns 0 if none. */
int heic_doc_exif(heic_doc *doc, uint8_t **out, size_t *out_len);

/* XMP: raw XML bytes. Same ownership as EXIF. */
int heic_doc_xmp(heic_doc *doc, uint8_t **out, size_t *out_len);

/* ICC profile bytes, or 0 if none / nclx-only. */
int heic_doc_icc(heic_doc *doc, uint8_t **out, size_t *out_len);

/* Free a buffer returned by heic_doc_exif/xmp/icc (uses ctx allocator). */
void heic_free(heic_ctx *ctx, void *p);

/* Library version string (static). */
const char *heic_version(void);

#ifdef __cplusplus
}
#endif

#endif /* HEIC_H */
