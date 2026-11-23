/* jxl.h -- plain-C JPEG XL decoder (jbig2dec/djvudec-flavored API).
 *
 * Decode-only. The caller supplies the entire JPEG XL file up-front as an
 * in-memory buffer that must outlive the jxl_doc.
 *
 * Copyright (c) 2026 Krzysztof Kowalczyk. MIT License; see LICENSE.md, which
 * also covers the jxl-oxide (MIT OR Apache-2.0) and libjxl (BSD-3-Clause)
 * code this is derived from.
 */
#ifndef JXLDEC_H
#define JXLDEC_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ----- allocator + diagnostics (jbig2dec style) ----- */

/* ctx identifies the jxl_ctx the allocation belongs to, so a caller can
   account for allocations per context. It is NULL only for the bootstrap
   allocation/free of the jxl_ctx struct itself (which has no context yet). */
typedef void *(*jxl_alloc_cb)(void *user, void *ctx, size_t size);
typedef void  (*jxl_free_cb)(void *user, void *ctx, void *ptr);

typedef enum {
    JXLDEC_SEVERITY_DEBUG,
    JXLDEC_SEVERITY_INFO,
    JXLDEC_SEVERITY_WARNING,
    JXLDEC_SEVERITY_ERROR,
    JXLDEC_SEVERITY_FATAL
} jxl_severity;

/* msg is a NUL-terminated, already-formatted message. */
typedef void (*jxl_error_cb)(void *user, jxl_severity sev, const char *msg);

typedef struct jxl_ctx jxl_ctx;
typedef struct jxl_doc jxl_doc;

/* Pass NULL for alloc/free to use the default malloc/free.
   Pass NULL for error to silently ignore diagnostics. */
jxl_ctx *jxl_ctx_new(jxl_alloc_cb alloc, jxl_free_cb free_cb,
                     jxl_error_cb error, void *user);
void jxl_ctx_free(jxl_ctx *ctx);

/* When enabled, 4-component output (JXLDEC_FORMAT_RGBA32) is written in
   B,G,R,A byte order instead of R,G,B,A, and 3-component output
   (JXLDEC_FORMAT_RGB24) in B,G,R order. The image is still tagged RGB24/RGBA32;
   only the channel order changes. Lets a caller whose target wants BGR (e.g.
   a Windows DIB) skip a separate swizzle pass. */
void jxl_ctx_set_bgr(jxl_ctx *ctx, int enable);

/* By default the decoder applies the image's EXIF-style orientation field
   (like libjxl's JxlDecoder does), so the returned image is upright and
   width/height may be swapped relative to the codestream dimensions. Enable
   to keep the raw codestream orientation instead. */
void jxl_ctx_set_keep_orientation(jxl_ctx *ctx, int enable);

/* Decode to display-referred sRGB instead of the image's own colour encoding.
   An xyb-encoded image that declares a linear transfer function decodes to
   linear light, which looks dark and over-contrasty when handed to a display
   as if it were sRGB; with this enabled the sRGB transfer curve is applied on
   the way out, while the samples are still float, so nothing bands.
   Only the transfer function is handled, and only for xyb-encoded images:
   primaries are left alone, and an image stored in its original colour space
   is left as-is (its samples are not ours to reinterpret).
   An xyb-encoded image with an embedded ICC profile also counts as linear
   here: like libjxl without a CMS, it decodes to linear sRGB, so this flag
   gives it the sRGB curve too.
   Off by default, so `djxl`-compatible output stays the default. */
void jxl_ctx_set_srgb_output(jxl_ctx *ctx, int enable);

/* Cooperative abort. Safe to call from any thread. Renders on ctx that are in
   progress (jxl_frame_render, jxl_frame_render_into, jxl_decode,
   jxl_doc_frame_info, jxl_doc_frame_count) stop at the next frame or group
   boundary and fail: NULL / -1 (jxl_doc_frame_count returns 1). Renders that
   start after the call run normally. The doc stays usable; render again to
   get the frame.
   The frame count of an animation is found by a walk over its frame headers
   that the same abort interrupts, so jxl_doc_info and jxl_decode_size, which
   report it, then say num_frames = 1 for that one call. */
void jxl_request_abort(jxl_ctx *ctx);

/* ----- signature detection ----- */

typedef enum {
    JXLDEC_SIG_INVALID = 0,     /* definitely not JPEG XL                    */
    JXLDEC_SIG_NOT_ENOUGH_BYTES = 1, /* need more bytes to decide            */
    JXLDEC_SIG_CODESTREAM = 2,  /* bare codestream (0xFF 0x0A)               */
    JXLDEC_SIG_CONTAINER = 3    /* ISOBMFF container (JXL box signature)     */
} jxl_signature;

/* Cheap header sniff; no context needed. */
jxl_signature jxl_signature_check(const uint8_t *data, size_t len);

/* ----- documents ----- */

/* Open a JPEG XL file over an in-memory buffer (NOT copied; must remain valid
   until jxl_doc_close). Parses the container and image header only -- no pixel
   decoding. Returns NULL on failure (diagnostics via the error cb). */
jxl_doc *jxl_doc_open(jxl_ctx *ctx, const uint8_t *data, size_t len);
void jxl_doc_close(jxl_doc *doc);

typedef enum {
    JXLDEC_CS_RGB   = 0,
    JXLDEC_CS_GRAY  = 1,
    JXLDEC_CS_XYB   = 2,
    JXLDEC_CS_UNKNOWN = 3
} jxl_color_space;

typedef struct {
    int width;             /* width in pixels; orientation applied unless   */
    int height;            /* jxl_ctx_set_keep_orientation is enabled       */
    int bits_per_sample;   /* nominal bit depth of the color channels       */
    int exponent_bits;     /* >0 when samples are floating point            */
    int num_color_channels;/* 1 (gray) or 3 (color)                         */
    int num_extra_channels;
    int alpha_bits;        /* 0 when there is no alpha channel              */
    int alpha_premultiplied;
    int have_animation;
    int num_frames;        /* animation frames; 1 for a still image         */
    int orientation;       /* 1..8, EXIF-style                              */
    int have_preview;
    int uses_original_profile; /* 1 when NOT xyb-encoded                    */
    jxl_color_space color_space;
    int intrinsic_width;   /* recommended display size, or == width/height  */
    int intrinsic_height;
} jxl_image_info;

/* Fill *info. Returns 0 on success, -1 on error. */
int jxl_doc_info(jxl_doc *doc, jxl_image_info *info);

/* Number of animation frames: 1 for a still image, without looking past the
   image header. For an animation the first call reads every frame header
   (no pixels) and the answer is cached; a truncated animation reports the
   frames it could find, at least 1. Returns 0 only for a NULL doc. */
int jxl_doc_frame_count(jxl_doc *doc);

/* Embedded ICC profile of the original color space, or NULL if the image has
   none (its color encoding is then enumerated in the codestream; only the
   color_space part of it is exposed, via jxl_doc_info -- primaries, white
   point and transfer function are not).
   The returned pointer is owned by the doc and valid until jxl_doc_close. */
const uint8_t *jxl_doc_icc_profile(jxl_doc *doc, size_t *len);

/* ----- rendering ----- */

typedef enum {
    JXLDEC_FORMAT_NATIVE  = 0,  /* pick from the image: gray/rgb, +alpha, 8/16 */
    JXLDEC_FORMAT_GRAY8   = 1,  /* 1 byte/pixel                                */
    JXLDEC_FORMAT_GRAYA8  = 2,  /* 2 bytes/pixel: gray, alpha                  */
    JXLDEC_FORMAT_RGB24   = 3,  /* 3 bytes/pixel: R,G,B (or B,G,R -- set_bgr)  */
    JXLDEC_FORMAT_RGBA32  = 4,  /* 4 bytes/pixel: R,G,B,A (or B,G,R,A)         */
    JXLDEC_FORMAT_GRAY16  = 5,  /* 2 bytes/pixel, native-endian u16            */
    JXLDEC_FORMAT_GRAYA16 = 6,  /* 4 bytes/pixel                               */
    JXLDEC_FORMAT_RGB48   = 7,  /* 6 bytes/pixel                               */
    JXLDEC_FORMAT_RGBA64  = 8   /* 8 bytes/pixel                               */
} jxl_format;

/* Bytes per pixel for a resolved (non-NATIVE) format. */
int jxl_format_bpp(jxl_format fmt);

typedef struct {
    int width;
    int height;
    jxl_format format;
    int stride;        /* bytes per row */
    uint8_t *data;     /* top-down */
} jxl_image;

/* Decode a frame (0-based; 0 is the first/only frame of a still image). Each
   frame comes back composited onto the full canvas, as it is displayed.
   Frames may be requested in any order. No decode state is kept on the doc:
   each call re-decodes frames 0..N (so blending and reference frames work),
   which makes playing an N-frame animation O(N^2) in frame decodes.
   fmt == JXLDEC_FORMAT_NATIVE picks the format from the image metadata.
   Returns NULL on error; free with jxl_image_destroy. */
jxl_image *jxl_frame_render(jxl_doc *doc, int frame_no, jxl_format fmt);
void jxl_image_destroy(jxl_ctx *ctx, jxl_image *img);

/* Geometry + format a render will produce, without decoding pixels. */
typedef struct {
    int width;
    int height;
    jxl_format format;
} jxl_render_info;

int jxl_frame_render_info(jxl_doc *doc, int frame_no, jxl_format fmt,
                          jxl_render_info *info);

/* Render directly into a caller-provided top-down buffer with `stride` bytes
   per row, eliminating one full-frame copy. The buffer must match the geometry
   reported by jxl_frame_render_info: stride >= width * jxl_format_bpp(format)
   and at least height rows. Neither is checked -- that is the caller's job.
   Returns 0 on success, -1 on error. */
int jxl_frame_render_into(jxl_doc *doc, int frame_no, jxl_format fmt,
                          uint8_t *dst, int stride);

/* Per-frame animation timing / identity. Reads the frame headers up to
   frame_no; no pixels are decoded. Returns 0 on success, -1 on error
   (including a frame_no past the last frame). */
typedef struct {
    int duration_ticks;  /* frame duration in animation ticks */
    int tps_numerator;   /* ticks per second = numerator / denominator */
    int tps_denominator;
    int is_last;
} jxl_frame_info;

int jxl_doc_frame_info(jxl_doc *doc, int frame_no, jxl_frame_info *info);

/* ----- one-shot convenience (matches the common "decode this blob" case) --- */

/* Decode the first frame of a file into a freshly allocated image. */
jxl_image *jxl_decode(jxl_ctx *ctx, const uint8_t *data, size_t len,
                      jxl_format fmt);

/* Size query: jxl_doc_open + jxl_doc_info, so the image header only for a
   still image. Returns 0 on success, -1 on error. */
int jxl_decode_size(jxl_ctx *ctx, const uint8_t *data, size_t len,
                    int *width, int *height);

#ifdef __cplusplus
}
#endif
#endif /* JXLDEC_H */
