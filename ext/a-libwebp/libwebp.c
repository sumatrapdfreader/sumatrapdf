#include <assert.h>
#include <stdlib.h>

#ifndef WEBP_DEC_ALPHAI_DEC_H_
#define WEBP_DEC_ALPHAI_DEC_H_

#ifndef WEBP_DEC_VP8_DEC_H_
#define WEBP_DEC_VP8_DEC_H_

#include <stddef.h>

#ifndef WEBP_WEBP_DECODE_H_
#define WEBP_WEBP_DECODE_H_

#include <stddef.h>

#ifndef WEBP_WEBP_TYPES_H_
#define WEBP_WEBP_TYPES_H_

#include <stddef.h>

#ifndef _MSC_VER
#include <inttypes.h>
#if defined(__cplusplus) || !defined(__STRICT_ANSI__) || \
    (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L)
#define WEBP_INLINE inline
#else
#define WEBP_INLINE
#endif
#else
typedef signed   char int8_t;
typedef unsigned char uint8_t;
typedef signed   short int16_t;
typedef unsigned short uint16_t;
typedef signed   int int32_t;
typedef unsigned int uint32_t;
typedef unsigned long long int uint64_t;
typedef long long int int64_t;
#define WEBP_INLINE __forceinline
#endif

#ifndef WEBP_NODISCARD
#if defined(WEBP_ENABLE_NODISCARD) && WEBP_ENABLE_NODISCARD
#if (defined(__cplusplus) && __cplusplus >= 201703L) || \
    (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L)
#define WEBP_NODISCARD [[nodiscard]]
#else

#if defined(__clang__) && defined(__has_attribute)
#if __has_attribute(warn_unused_result)
#define WEBP_NODISCARD __attribute__((warn_unused_result))
#else
#define WEBP_NODISCARD
#endif
#else
#define WEBP_NODISCARD
#endif
#endif

#else
#define WEBP_NODISCARD
#endif
#endif

#ifndef WEBP_EXTERN

# if defined(_WIN32) && defined(WEBP_DLL)
#  define WEBP_EXTERN __declspec(dllexport)
# elif defined(__GNUC__) && __GNUC__ >= 4
#  define WEBP_EXTERN extern __attribute__ ((visibility ("default")))
# else
#  define WEBP_EXTERN extern
# endif
#endif

#define WEBP_ABI_IS_INCOMPATIBLE(a, b) (((a) >> 8) != ((b) >> 8))

#ifdef __cplusplus
extern "C" {
#endif

WEBP_NODISCARD WEBP_EXTERN void* WebPMalloc(size_t size);

WEBP_EXTERN void WebPFree(void* ptr);

#ifdef __cplusplus
}
#endif

#endif

#ifdef __cplusplus
extern "C" {
#endif

#define WEBP_DECODER_ABI_VERSION 0x0210

typedef struct WebPRGBABuffer WebPRGBABuffer;
typedef struct WebPYUVABuffer WebPYUVABuffer;
typedef struct WebPDecBuffer WebPDecBuffer;
typedef struct WebPIDecoder WebPIDecoder;
typedef struct WebPBitstreamFeatures WebPBitstreamFeatures;
typedef struct WebPDecoderOptions WebPDecoderOptions;
typedef struct WebPDecoderConfig WebPDecoderConfig;

WEBP_EXTERN int WebPGetDecoderVersion(void);

WEBP_NODISCARD WEBP_EXTERN int WebPGetInfo(
    const uint8_t* data, size_t data_size, int* width, int* height);

WEBP_NODISCARD WEBP_EXTERN uint8_t* WebPDecodeRGBA(
    const uint8_t* data, size_t data_size, int* width, int* height);

WEBP_NODISCARD WEBP_EXTERN uint8_t* WebPDecodeARGB(
    const uint8_t* data, size_t data_size, int* width, int* height);

WEBP_NODISCARD WEBP_EXTERN uint8_t* WebPDecodeBGRA(
    const uint8_t* data, size_t data_size, int* width, int* height);

WEBP_NODISCARD WEBP_EXTERN uint8_t* WebPDecodeRGB(
    const uint8_t* data, size_t data_size, int* width, int* height);

WEBP_NODISCARD WEBP_EXTERN uint8_t* WebPDecodeBGR(
    const uint8_t* data, size_t data_size, int* width, int* height);

WEBP_NODISCARD WEBP_EXTERN uint8_t* WebPDecodeYUV(
    const uint8_t* data, size_t data_size, int* width, int* height,
    uint8_t** u, uint8_t** v, int* stride, int* uv_stride);

WEBP_NODISCARD WEBP_EXTERN uint8_t* WebPDecodeRGBAInto(
    const uint8_t* data, size_t data_size,
    uint8_t* output_buffer, size_t output_buffer_size, int output_stride);
WEBP_NODISCARD WEBP_EXTERN uint8_t* WebPDecodeARGBInto(
    const uint8_t* data, size_t data_size,
    uint8_t* output_buffer, size_t output_buffer_size, int output_stride);
WEBP_NODISCARD WEBP_EXTERN uint8_t* WebPDecodeBGRAInto(
    const uint8_t* data, size_t data_size,
    uint8_t* output_buffer, size_t output_buffer_size, int output_stride);

WEBP_NODISCARD WEBP_EXTERN uint8_t* WebPDecodeRGBInto(
    const uint8_t* data, size_t data_size,
    uint8_t* output_buffer, size_t output_buffer_size, int output_stride);
WEBP_NODISCARD WEBP_EXTERN uint8_t* WebPDecodeBGRInto(
    const uint8_t* data, size_t data_size,
    uint8_t* output_buffer, size_t output_buffer_size, int output_stride);

WEBP_NODISCARD WEBP_EXTERN uint8_t* WebPDecodeYUVInto(
    const uint8_t* data, size_t data_size,
    uint8_t* luma, size_t luma_size, int luma_stride,
    uint8_t* u, size_t u_size, int u_stride,
    uint8_t* v, size_t v_size, int v_stride);

typedef enum WEBP_CSP_MODE {
  MODE_RGB = 0, MODE_RGBA = 1,
  MODE_BGR = 2, MODE_BGRA = 3,
  MODE_ARGB = 4, MODE_RGBA_4444 = 5,
  MODE_RGB_565 = 6,

  MODE_rgbA = 7,
  MODE_bgrA = 8,
  MODE_Argb = 9,
  MODE_rgbA_4444 = 10,

  MODE_YUV = 11, MODE_YUVA = 12,
  MODE_LAST = 13
} WEBP_CSP_MODE;

static WEBP_INLINE int WebPIsPremultipliedMode(WEBP_CSP_MODE mode) {
  return (mode == MODE_rgbA || mode == MODE_bgrA || mode == MODE_Argb ||
          mode == MODE_rgbA_4444);
}

static WEBP_INLINE int WebPIsAlphaMode(WEBP_CSP_MODE mode) {
  return (mode == MODE_RGBA || mode == MODE_BGRA || mode == MODE_ARGB ||
          mode == MODE_RGBA_4444 || mode == MODE_YUVA ||
          WebPIsPremultipliedMode(mode));
}

static WEBP_INLINE int WebPIsRGBMode(WEBP_CSP_MODE mode) {
  return (mode < MODE_YUV);
}

struct WebPRGBABuffer {
  uint8_t* rgba;
  int stride;
  size_t size;
};

struct WebPYUVABuffer {
  uint8_t* y, *u, *v, *a;
  int y_stride;
  int u_stride, v_stride;
  int a_stride;
  size_t y_size;
  size_t u_size, v_size;
  size_t a_size;
};

struct WebPDecBuffer {
  WEBP_CSP_MODE colorspace;
  int width, height;
  int is_external_memory;

  union {
    WebPRGBABuffer RGBA;
    WebPYUVABuffer YUVA;
  } u;
  uint32_t       pad[4];

  uint8_t* private_memory;

};

WEBP_NODISCARD WEBP_EXTERN int WebPInitDecBufferInternal(WebPDecBuffer*, int);

WEBP_NODISCARD static WEBP_INLINE int WebPInitDecBuffer(WebPDecBuffer* buffer) {
  return WebPInitDecBufferInternal(buffer, WEBP_DECODER_ABI_VERSION);
}

WEBP_EXTERN void WebPFreeDecBuffer(WebPDecBuffer* buffer);

typedef enum WEBP_NODISCARD VP8StatusCode {
  VP8_STATUS_OK = 0,
  VP8_STATUS_OUT_OF_MEMORY,
  VP8_STATUS_INVALID_PARAM,
  VP8_STATUS_BITSTREAM_ERROR,
  VP8_STATUS_UNSUPPORTED_FEATURE,
  VP8_STATUS_SUSPENDED,
  VP8_STATUS_USER_ABORT,
  VP8_STATUS_NOT_ENOUGH_DATA
} VP8StatusCode;

WEBP_NODISCARD WEBP_EXTERN WebPIDecoder* WebPINewDecoder(
    WebPDecBuffer* output_buffer);

WEBP_NODISCARD WEBP_EXTERN WebPIDecoder* WebPINewRGB(
    WEBP_CSP_MODE csp,
    uint8_t* output_buffer, size_t output_buffer_size, int output_stride);

WEBP_NODISCARD WEBP_EXTERN WebPIDecoder* WebPINewYUVA(
    uint8_t* luma, size_t luma_size, int luma_stride,
    uint8_t* u, size_t u_size, int u_stride,
    uint8_t* v, size_t v_size, int v_stride,
    uint8_t* a, size_t a_size, int a_stride);

WEBP_NODISCARD WEBP_EXTERN WebPIDecoder* WebPINewYUV(
    uint8_t* luma, size_t luma_size, int luma_stride,
    uint8_t* u, size_t u_size, int u_stride,
    uint8_t* v, size_t v_size, int v_stride);

WEBP_EXTERN void WebPIDelete(WebPIDecoder* idec);

WEBP_EXTERN VP8StatusCode WebPIAppend(
    WebPIDecoder* idec, const uint8_t* data, size_t data_size);

WEBP_EXTERN VP8StatusCode WebPIUpdate(
    WebPIDecoder* idec, const uint8_t* data, size_t data_size);

WEBP_NODISCARD WEBP_EXTERN uint8_t* WebPIDecGetRGB(
    const WebPIDecoder* idec, int* last_y,
    int* width, int* height, int* stride);

WEBP_NODISCARD WEBP_EXTERN uint8_t* WebPIDecGetYUVA(
    const WebPIDecoder* idec, int* last_y,
    uint8_t** u, uint8_t** v, uint8_t** a,
    int* width, int* height, int* stride, int* uv_stride, int* a_stride);

WEBP_NODISCARD static WEBP_INLINE uint8_t* WebPIDecGetYUV(
    const WebPIDecoder* idec, int* last_y, uint8_t** u, uint8_t** v,
    int* width, int* height, int* stride, int* uv_stride) {
  return WebPIDecGetYUVA(idec, last_y, u, v, NULL, width, height,
                         stride, uv_stride, NULL);
}

WEBP_NODISCARD WEBP_EXTERN const WebPDecBuffer* WebPIDecodedArea(
    const WebPIDecoder* idec, int* left, int* top, int* width, int* height);

struct WebPBitstreamFeatures {
  int width;
  int height;
  int has_alpha;
  int has_animation;
  int format;

  uint32_t pad[5];
};

WEBP_EXTERN VP8StatusCode WebPGetFeaturesInternal(
    const uint8_t*, size_t, WebPBitstreamFeatures*, int);

static WEBP_INLINE VP8StatusCode WebPGetFeatures(
    const uint8_t* data, size_t data_size,
    WebPBitstreamFeatures* features) {
  return WebPGetFeaturesInternal(data, data_size, features,
                                 WEBP_DECODER_ABI_VERSION);
}

struct WebPDecoderOptions {
  int bypass_filtering;
  int no_fancy_upsampling;
  int use_cropping;
  int crop_left, crop_top;

  int crop_width, crop_height;
  int use_scaling;
  int scaled_width, scaled_height;

  int use_threads;
  int dithering_strength;
  int flip;
  int alpha_dithering_strength;

  uint32_t pad[5];
};

struct WebPDecoderConfig {
  WebPBitstreamFeatures input;
  WebPDecBuffer output;
  WebPDecoderOptions options;
};

WEBP_NODISCARD WEBP_EXTERN int WebPInitDecoderConfigInternal(WebPDecoderConfig*,
                                                             int);

WEBP_NODISCARD static WEBP_INLINE int WebPInitDecoderConfig(
    WebPDecoderConfig* config) {
  return WebPInitDecoderConfigInternal(config, WEBP_DECODER_ABI_VERSION);
}

WEBP_NODISCARD WEBP_EXTERN int WebPValidateDecoderConfig(
    const WebPDecoderConfig* config);

WEBP_NODISCARD WEBP_EXTERN WebPIDecoder* WebPIDecode(
    const uint8_t* data, size_t data_size, WebPDecoderConfig* config);

WEBP_EXTERN VP8StatusCode WebPDecode(const uint8_t* data, size_t data_size,
                                     WebPDecoderConfig* config);

#ifdef __cplusplus
}
#endif

#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct VP8Io VP8Io;
typedef int (*VP8IoPutHook)(const VP8Io* io);
typedef int (*VP8IoSetupHook)(VP8Io* io);
typedef void (*VP8IoTeardownHook)(const VP8Io* io);

struct VP8Io {

  int width, height;

  int mb_y;
  int mb_w;
  int mb_h;
  const uint8_t* y, *u, *v;
  int y_stride;
  int uv_stride;

  void* opaque;

  VP8IoPutHook put;

  VP8IoSetupHook setup;

  VP8IoTeardownHook teardown;

  int fancy_upsampling;

  size_t data_size;
  const uint8_t* data;

  int bypass_filtering;

  int use_cropping;
  int crop_left, crop_right, crop_top, crop_bottom;

  int use_scaling;
  int scaled_width, scaled_height;

  const uint8_t* a;
};

WEBP_NODISCARD int VP8InitIoInternal(VP8Io* const, int);

WEBP_NODISCARD int WebPISetIOHooks(WebPIDecoder* const idec, VP8IoPutHook put,
                                   VP8IoSetupHook setup,
                                   VP8IoTeardownHook teardown, void* user_data);

typedef struct VP8Decoder VP8Decoder;

VP8Decoder* VP8New(void);

WEBP_NODISCARD static WEBP_INLINE int VP8InitIo(VP8Io* const io) {
  return VP8InitIoInternal(io, WEBP_DECODER_ABI_VERSION);
}

WEBP_NODISCARD int VP8GetHeaders(VP8Decoder* const dec, VP8Io* const io);

WEBP_NODISCARD int VP8Decode(VP8Decoder* const dec, VP8Io* const io);

VP8StatusCode VP8Status(VP8Decoder* const dec);

const char* VP8StatusMessage(VP8Decoder* const dec);

void VP8Clear(VP8Decoder* const dec);

void VP8Delete(VP8Decoder* const dec);

WEBP_EXTERN int VP8CheckSignature(const uint8_t* const data, size_t data_size);

WEBP_EXTERN int VP8GetInfo(
    const uint8_t* data,
    size_t data_size,
    size_t chunk_size,
    int* const width, int* const height);

WEBP_EXTERN int VP8LCheckSignature(const uint8_t* const data, size_t size);

WEBP_EXTERN int VP8LGetInfo(
    const uint8_t* data, size_t data_size,
    int* const width, int* const height, int* const has_alpha);

#ifdef __cplusplus
}
#endif

#endif

#ifndef WEBP_DEC_WEBPI_DEC_H_
#define WEBP_DEC_WEBPI_DEC_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

#ifndef WEBP_UTILS_RESCALER_UTILS_H_
#define WEBP_UTILS_RESCALER_UTILS_H_

#ifdef __cplusplus
extern "C" {
#endif

#define WEBP_RESCALER_RFIX 32
#define WEBP_RESCALER_ONE (1ull << WEBP_RESCALER_RFIX)
#define WEBP_RESCALER_FRAC(x, y) \
    ((uint32_t)(((uint64_t)(x) << WEBP_RESCALER_RFIX) / (y)))

typedef uint32_t rescaler_t;
typedef struct WebPRescaler WebPRescaler;
struct WebPRescaler {
  int x_expand;
  int y_expand;
  int num_channels;
  uint32_t fx_scale;
  uint32_t fy_scale;
  uint32_t fxy_scale;
  int y_accum;
  int y_add, y_sub;
  int x_add, x_sub;
  int src_width, src_height;
  int dst_width, dst_height;
  int src_y, dst_y;
  uint8_t* dst;
  int dst_stride;
  rescaler_t* irow, *frow;
};

int WebPRescalerInit(WebPRescaler* const rescaler,
                     int src_width, int src_height,
                     uint8_t* const dst,
                     int dst_width, int dst_height, int dst_stride,
                     int num_channels,
                     rescaler_t* const work);

int WebPRescalerGetScaledDimensions(int src_width, int src_height,
                                    int* const scaled_width,
                                    int* const scaled_height);

int WebPRescaleNeededLines(const WebPRescaler* const rescaler,
                           int max_num_lines);

int WebPRescalerImport(WebPRescaler* const rescaler, int num_rows,
                       const uint8_t* src, int src_stride);

int WebPRescalerExport(WebPRescaler* const rescaler);

static WEBP_INLINE
int WebPRescalerInputDone(const WebPRescaler* const rescaler) {
  return (rescaler->src_y >= rescaler->src_height);
}

static WEBP_INLINE
int WebPRescalerOutputDone(const WebPRescaler* const rescaler) {
  return (rescaler->dst_y >= rescaler->dst_height);
}

static WEBP_INLINE
int WebPRescalerHasPendingOutput(const WebPRescaler* const rescaler) {
  return !WebPRescalerOutputDone(rescaler) && (rescaler->y_accum <= 0);
}

#ifdef __cplusplus
}
#endif

#endif

typedef struct WebPDecParams WebPDecParams;
typedef int (*OutputFunc)(const VP8Io* const io, WebPDecParams* const p);
typedef int (*OutputAlphaFunc)(const VP8Io* const io, WebPDecParams* const p,
                               int expected_num_out_lines);
typedef int (*OutputRowFunc)(WebPDecParams* const p, int y_pos,
                             int max_out_lines);

struct WebPDecParams {
  WebPDecBuffer* output;
  uint8_t* tmp_y, *tmp_u, *tmp_v;

  int last_y;
  const WebPDecoderOptions* options;

  WebPRescaler* scaler_y, *scaler_u, *scaler_v, *scaler_a;
  void* memory;

  OutputFunc emit;
  OutputAlphaFunc emit_alpha;
  OutputRowFunc emit_alpha_row;
};

void WebPResetDecParams(WebPDecParams* const params);

typedef struct {
  const uint8_t* data;
  size_t data_size;
  int have_all_data;
  size_t offset;
  const uint8_t* alpha_data;
  size_t alpha_data_size;
  size_t compressed_size;
  size_t riff_size;
  int is_lossless;
} WebPHeaderStructure;

VP8StatusCode WebPParseHeaders(WebPHeaderStructure* const headers);

int WebPCheckCropDimensions(int image_width, int image_height,
                            int x, int y, int w, int h);

void WebPInitCustomIo(WebPDecParams* const params, VP8Io* const io);

WEBP_NODISCARD int WebPIoInitFromOptions(
    const WebPDecoderOptions* const options, VP8Io* const io,
    WEBP_CSP_MODE src_colorspace);

VP8StatusCode WebPAllocateDecBuffer(int width, int height,
                                    const WebPDecoderOptions* const options,
                                    WebPDecBuffer* const buffer);

VP8StatusCode WebPFlipBuffer(WebPDecBuffer* const buffer);

void WebPCopyDecBuffer(const WebPDecBuffer* const src,
                       WebPDecBuffer* const dst);

void WebPGrabDecBuffer(WebPDecBuffer* const src, WebPDecBuffer* const dst);

VP8StatusCode WebPCopyDecBufferPixels(const WebPDecBuffer* const src,
                                      WebPDecBuffer* const dst);

int WebPAvoidSlowMemory(const WebPDecBuffer* const output,
                        const WebPBitstreamFeatures* const features);

#ifdef __cplusplus
}
#endif

#endif

#ifndef WEBP_DSP_DSP_H_
#define WEBP_DSP_DSP_H_

#ifdef HAVE_CONFIG_H
#include "src/webp/config.h"
#endif

#ifndef WEBP_DSP_CPU_H_
#define WEBP_DSP_CPU_H_

#include <stddef.h>

#ifdef HAVE_CONFIG_H
#include "src/webp/config.h"
#endif

#if defined(__GNUC__)
#define LOCAL_GCC_VERSION ((__GNUC__ << 8) | __GNUC_MINOR__)
#define LOCAL_GCC_PREREQ(maj, min) (LOCAL_GCC_VERSION >= (((maj) << 8) | (min)))
#else
#define LOCAL_GCC_VERSION 0
#define LOCAL_GCC_PREREQ(maj, min) 0
#endif

#if defined(__clang__)
#define LOCAL_CLANG_VERSION ((__clang_major__ << 8) | __clang_minor__)
#define LOCAL_CLANG_PREREQ(maj, min) \
  (LOCAL_CLANG_VERSION >= (((maj) << 8) | (min)))
#else
#define LOCAL_CLANG_VERSION 0
#define LOCAL_CLANG_PREREQ(maj, min) 0
#endif

#ifndef __has_builtin
#define __has_builtin(x) 0
#endif

#if !defined(HAVE_CONFIG_H)
#if defined(_MSC_VER) && _MSC_VER > 1310 && \
    (defined(_M_X64) || defined(_M_IX86))
#define WEBP_MSC_SSE2
#endif

#if defined(_MSC_VER) && _MSC_VER >= 1500 && \
    (defined(_M_X64) || defined(_M_IX86))
#define WEBP_MSC_SSE41
#endif

#if defined(_MSC_VER) && _MSC_VER >= 1700 && \
    (defined(_M_X64) || defined(_M_IX86))
#define WEBP_MSC_AVX2
#endif
#endif

#if (defined(__SSE2__) || defined(WEBP_MSC_SSE2)) && \
    (!defined(HAVE_CONFIG_H) || defined(WEBP_HAVE_SSE2))
#define WEBP_USE_SSE2
#endif

#if defined(WEBP_USE_SSE2) && !defined(WEBP_HAVE_SSE2)
#define WEBP_HAVE_SSE2
#endif

#if (defined(__SSE4_1__) || defined(WEBP_MSC_SSE41)) && \
    (!defined(HAVE_CONFIG_H) || defined(WEBP_HAVE_SSE41))
#define WEBP_USE_SSE41
#endif

#if defined(WEBP_USE_SSE41) && !defined(WEBP_HAVE_SSE41)
#define WEBP_HAVE_SSE41
#endif

#if (defined(__AVX2__) || defined(WEBP_MSC_AVX2)) && \
    (!defined(HAVE_CONFIG_H) || defined(WEBP_HAVE_AVX2))
#define WEBP_USE_AVX2
#endif

#if defined(WEBP_USE_AVX2) && !defined(WEBP_HAVE_AVX2)
#define WEBP_HAVE_AVX2
#endif

#undef WEBP_MSC_AVX2
#undef WEBP_MSC_SSE41
#undef WEBP_MSC_SSE2

#if ((defined(__ARM_NEON__) || defined(__aarch64__)) &&       \
     (!defined(HAVE_CONFIG_H) || defined(WEBP_HAVE_NEON))) && \
    !defined(__native_client__)
#define WEBP_USE_NEON
#endif

#if !defined(WEBP_USE_NEON) && defined(__ANDROID__) && \
    defined(__ARM_ARCH_7A__) && defined(HAVE_CPU_FEATURES_H)
#define WEBP_ANDROID_NEON
#define WEBP_USE_NEON
#endif

#if defined(_MSC_VER) && \
    ((_MSC_VER >= 1700 && defined(_M_ARM)) || \
     (_MSC_VER >= 1926 && (defined(_M_ARM64) || defined(_M_ARM64EC))))
#define WEBP_USE_NEON
#define WEBP_USE_INTRINSICS
#endif

#if defined(__aarch64__) || defined(_M_ARM64) || defined(_M_ARM64EC)
#define WEBP_AARCH64 1
#else
#define WEBP_AARCH64 0
#endif

#if defined(WEBP_USE_NEON) && !defined(WEBP_HAVE_NEON)
#define WEBP_HAVE_NEON
#endif

#if defined(__mips__) && !defined(__mips64) && defined(__mips_isa_rev) && \
    (__mips_isa_rev >= 1) && (__mips_isa_rev < 6)
#define WEBP_USE_MIPS32
#if (__mips_isa_rev >= 2)
#define WEBP_USE_MIPS32_R2
#if defined(__mips_dspr2) || (defined(__mips_dsp_rev) && __mips_dsp_rev >= 2)
#define WEBP_USE_MIPS_DSP_R2
#endif
#endif
#endif

#if defined(__mips_msa) && defined(__mips_isa_rev) && (__mips_isa_rev >= 5)
#define WEBP_USE_MSA
#endif

#ifndef WEBP_DSP_OMIT_C_CODE
#define WEBP_DSP_OMIT_C_CODE 1
#endif

#if defined(WEBP_USE_NEON) && WEBP_DSP_OMIT_C_CODE
#define WEBP_NEON_OMIT_C_CODE 1
#else
#define WEBP_NEON_OMIT_C_CODE 0
#endif

#if !(LOCAL_CLANG_PREREQ(3, 8) || LOCAL_GCC_PREREQ(4, 8) || WEBP_AARCH64)
#define WEBP_NEON_WORK_AROUND_GCC 1
#else
#define WEBP_NEON_WORK_AROUND_GCC 0
#endif

#define WEBP_TSAN_IGNORE_FUNCTION
#if defined(__has_feature)
#if __has_feature(thread_sanitizer)
#undef WEBP_TSAN_IGNORE_FUNCTION
#define WEBP_TSAN_IGNORE_FUNCTION __attribute__((no_sanitize_thread))
#endif
#endif

#if defined(__has_feature)
#if __has_feature(memory_sanitizer)
#define WEBP_MSAN
#endif
#endif

#if defined(WEBP_USE_THREAD) && !defined(_WIN32)
#include <pthread.h>

#define WEBP_DSP_INIT(func)                                         \
  do {                                                              \
    static volatile VP8CPUInfo func##_last_cpuinfo_used =           \
        (VP8CPUInfo)&func##_last_cpuinfo_used;                      \
    static pthread_mutex_t func##_lock = PTHREAD_MUTEX_INITIALIZER; \
    if (pthread_mutex_lock(&func##_lock)) break;                    \
    if (func##_last_cpuinfo_used != VP8GetCPUInfo) func();          \
    func##_last_cpuinfo_used = VP8GetCPUInfo;                       \
    (void)pthread_mutex_unlock(&func##_lock);                       \
  } while (0)
#else
#define WEBP_DSP_INIT(func)                               \
  do {                                                    \
    static volatile VP8CPUInfo func##_last_cpuinfo_used = \
        (VP8CPUInfo)&func##_last_cpuinfo_used;            \
    if (func##_last_cpuinfo_used == VP8GetCPUInfo) break; \
    func();                                               \
    func##_last_cpuinfo_used = VP8GetCPUInfo;             \
  } while (0)
#endif

#define WEBP_DSP_INIT_FUNC(name)                                            \
  static WEBP_TSAN_IGNORE_FUNCTION void name##_body(void);                  \
  WEBP_TSAN_IGNORE_FUNCTION void name(void) { WEBP_DSP_INIT(name##_body); } \
  static WEBP_TSAN_IGNORE_FUNCTION void name##_body(void)

#define WEBP_UBSAN_IGNORE_UNDEF
#define WEBP_UBSAN_IGNORE_UNSIGNED_OVERFLOW
#if defined(__clang__) && defined(__has_attribute)
#if __has_attribute(no_sanitize)

#undef WEBP_UBSAN_IGNORE_UNDEF
#define WEBP_UBSAN_IGNORE_UNDEF __attribute__((no_sanitize("undefined")))

#undef WEBP_UBSAN_IGNORE_UNSIGNED_OVERFLOW
#define WEBP_UBSAN_IGNORE_UNSIGNED_OVERFLOW \
  __attribute__((no_sanitize("unsigned-integer-overflow")))
#endif
#endif

#if !defined(WEBP_OFFSET_PTR)
#define WEBP_OFFSET_PTR(ptr, off) (((ptr) == NULL) ? NULL : ((ptr) + (off)))
#endif

#if !defined(WEBP_SWAP_16BIT_CSP)
#define WEBP_SWAP_16BIT_CSP 0
#endif

#if !defined(WORDS_BIGENDIAN) &&                   \
    (defined(__BIG_ENDIAN__) || defined(_M_PPC) || \
     (defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)))
#define WORDS_BIGENDIAN
#endif

typedef enum {
  kSSE2,
  kSSE3,
  kSlowSSSE3,
  kSSE4_1,
  kAVX,
  kAVX2,
  kNEON,
  kMIPS32,
  kMIPSdspR2,
  kMSA
} CPUFeature;

typedef int (*VP8CPUInfo)(CPUFeature feature);

#endif

#ifdef __cplusplus
extern "C" {
#endif

#define BPS 32

#if defined(__GNUC__)
#define WEBP_RESTRICT __restrict__
#elif defined(_MSC_VER)
#define WEBP_RESTRICT __restrict
#else
#define WEBP_RESTRICT
#endif

#define WEBP_DSP_INIT_STUB(func) \
  extern void func(void); \
  void func(void) {}

typedef void (*VP8Idct)(const uint8_t* WEBP_RESTRICT ref,
                        const int16_t* WEBP_RESTRICT in,
                        uint8_t* WEBP_RESTRICT dst, int do_two);
typedef void (*VP8Fdct)(const uint8_t* WEBP_RESTRICT src,
                        const uint8_t* WEBP_RESTRICT ref,
                        int16_t* WEBP_RESTRICT out);
typedef void (*VP8WHT)(const int16_t* WEBP_RESTRICT in,
                       int16_t* WEBP_RESTRICT out);
extern VP8Idct VP8ITransform;
extern VP8Fdct VP8FTransform;
extern VP8Fdct VP8FTransform2;
extern VP8WHT VP8FTransformWHT;

typedef void (*VP8IntraPreds)(uint8_t* WEBP_RESTRICT dst,
                              const uint8_t* WEBP_RESTRICT left,
                              const uint8_t* WEBP_RESTRICT top);
typedef void (*VP8Intra4Preds)(uint8_t* WEBP_RESTRICT dst,
                               const uint8_t* WEBP_RESTRICT top);
extern VP8Intra4Preds VP8EncPredLuma4;
extern VP8IntraPreds VP8EncPredLuma16;
extern VP8IntraPreds VP8EncPredChroma8;

typedef int (*VP8Metric)(const uint8_t* WEBP_RESTRICT pix,
                         const uint8_t* WEBP_RESTRICT ref);
extern VP8Metric VP8SSE16x16, VP8SSE16x8, VP8SSE8x8, VP8SSE4x4;
typedef int (*VP8WMetric)(const uint8_t* WEBP_RESTRICT pix,
                          const uint8_t* WEBP_RESTRICT ref,
                          const uint16_t* WEBP_RESTRICT const weights);

extern VP8WMetric VP8TDisto4x4, VP8TDisto16x16;

typedef void (*VP8MeanMetric)(const uint8_t* WEBP_RESTRICT ref,
                              uint32_t dc[4]);
extern VP8MeanMetric VP8Mean16x4;

typedef void (*VP8BlockCopy)(const uint8_t* WEBP_RESTRICT src,
                             uint8_t* WEBP_RESTRICT dst);
extern VP8BlockCopy VP8Copy4x4;
extern VP8BlockCopy VP8Copy16x8;

struct VP8Matrix;
typedef int (*VP8QuantizeBlock)(
    int16_t in[16], int16_t out[16],
    const struct VP8Matrix* WEBP_RESTRICT const mtx);

typedef int (*VP8Quantize2Blocks)(
    int16_t in[32], int16_t out[32],
    const struct VP8Matrix* WEBP_RESTRICT const mtx);

extern VP8QuantizeBlock VP8EncQuantizeBlock;
extern VP8Quantize2Blocks VP8EncQuantize2Blocks;

typedef int (*VP8QuantizeBlockWHT)(
    int16_t in[16], int16_t out[16],
    const struct VP8Matrix* WEBP_RESTRICT const mtx);
extern VP8QuantizeBlockWHT VP8EncQuantizeBlockWHT;

extern const int VP8DspScan[16 + 4 + 4];

#define MAX_COEFF_THRESH   31
typedef struct {

  int max_value;
  int last_non_zero;
} VP8Histogram;
typedef void (*VP8CHisto)(const uint8_t* WEBP_RESTRICT ref,
                          const uint8_t* WEBP_RESTRICT pred,
                          int start_block, int end_block,
                          VP8Histogram* WEBP_RESTRICT const histo);
extern VP8CHisto VP8CollectHistogram;

void VP8SetHistogramData(const int distribution[MAX_COEFF_THRESH + 1],
                         VP8Histogram* const histo);

void VP8EncDspInit(void);

extern const uint16_t VP8EntropyCost[256];

extern const uint16_t VP8LevelFixedCosts[2047  + 1];
extern const uint8_t VP8EncBands[16 + 1];

struct VP8Residual;
typedef void (*VP8SetResidualCoeffsFunc)(
    const int16_t* WEBP_RESTRICT const coeffs,
    struct VP8Residual* WEBP_RESTRICT const res);
extern VP8SetResidualCoeffsFunc VP8SetResidualCoeffs;

typedef int (*VP8GetResidualCostFunc)(int ctx0,
                                      const struct VP8Residual* const res);
extern VP8GetResidualCostFunc VP8GetResidualCost;

void VP8EncDspCostInit(void);

typedef struct {
  uint32_t w;
  uint32_t xm, ym;
  uint32_t xxm, xym, yym;
} VP8DistoStats;

double VP8SSIMFromStats(const VP8DistoStats* const stats);
double VP8SSIMFromStatsClipped(const VP8DistoStats* const stats);

#define VP8_SSIM_KERNEL 3
typedef double (*VP8SSIMGetClippedFunc)(const uint8_t* src1, int stride1,
                                        const uint8_t* src2, int stride2,
                                        int xo, int yo,
                                        int W, int H);

#if !defined(WEBP_REDUCE_SIZE)

typedef double (*VP8SSIMGetFunc)(const uint8_t* src1, int stride1,
                                 const uint8_t* src2, int stride2);

extern VP8SSIMGetFunc VP8SSIMGet;
extern VP8SSIMGetClippedFunc VP8SSIMGetClipped;
#endif

#if !defined(WEBP_DISABLE_STATS)
typedef uint32_t (*VP8AccumulateSSEFunc)(const uint8_t* src1,
                                         const uint8_t* src2, int len);
extern VP8AccumulateSSEFunc VP8AccumulateSSE;
#endif

void VP8SSIMDspInit(void);

typedef void (*VP8DecIdct)(const int16_t* WEBP_RESTRICT coeffs,
                           uint8_t* WEBP_RESTRICT dst);

typedef void (*VP8DecIdct2)(const int16_t* WEBP_RESTRICT coeffs,
                            uint8_t* WEBP_RESTRICT dst, int do_two);
extern VP8DecIdct2 VP8Transform;
extern VP8DecIdct VP8TransformAC3;
extern VP8DecIdct VP8TransformUV;
extern VP8DecIdct VP8TransformDC;
extern VP8DecIdct VP8TransformDCUV;
extern VP8WHT VP8TransformWHT;

#define WEBP_TRANSFORM_AC3_C1 20091
#define WEBP_TRANSFORM_AC3_C2 35468
#define WEBP_TRANSFORM_AC3_MUL1(a) ((((a) * WEBP_TRANSFORM_AC3_C1) >> 16) + (a))
#define WEBP_TRANSFORM_AC3_MUL2(a) (((a) * WEBP_TRANSFORM_AC3_C2) >> 16)

typedef void (*VP8PredFunc)(uint8_t* dst);
extern VP8PredFunc VP8PredLuma16[];
extern VP8PredFunc VP8PredChroma8[];
extern VP8PredFunc VP8PredLuma4[];

extern const int8_t* const VP8ksclip1;
extern const int8_t* const VP8ksclip2;
extern const uint8_t* const VP8kclip1;
extern const uint8_t* const VP8kabs0;

void VP8InitClipTables(void);

typedef void (*VP8SimpleFilterFunc)(uint8_t* p, int stride, int thresh);
extern VP8SimpleFilterFunc VP8SimpleVFilter16;
extern VP8SimpleFilterFunc VP8SimpleHFilter16;
extern VP8SimpleFilterFunc VP8SimpleVFilter16i;
extern VP8SimpleFilterFunc VP8SimpleHFilter16i;

typedef void (*VP8LumaFilterFunc)(uint8_t* luma, int stride,
                                  int thresh, int ithresh, int hev_t);
typedef void (*VP8ChromaFilterFunc)(uint8_t* WEBP_RESTRICT u,
                                    uint8_t* WEBP_RESTRICT v, int stride,
                                    int thresh, int ithresh, int hev_t);

extern VP8LumaFilterFunc VP8VFilter16;
extern VP8LumaFilterFunc VP8HFilter16;
extern VP8ChromaFilterFunc VP8VFilter8;
extern VP8ChromaFilterFunc VP8HFilter8;

extern VP8LumaFilterFunc VP8VFilter16i;
extern VP8LumaFilterFunc VP8HFilter16i;
extern VP8ChromaFilterFunc VP8VFilter8i;
extern VP8ChromaFilterFunc VP8HFilter8i;

#define VP8_DITHER_DESCALE 4
#define VP8_DITHER_DESCALE_ROUNDER (1 << (VP8_DITHER_DESCALE - 1))
#define VP8_DITHER_AMP_BITS 7
#define VP8_DITHER_AMP_CENTER (1 << VP8_DITHER_AMP_BITS)
extern void (*VP8DitherCombine8x8)(const uint8_t* WEBP_RESTRICT dither,
                                   uint8_t* WEBP_RESTRICT dst, int dst_stride);

void VP8DspInit(void);

#define FANCY_UPSAMPLING

typedef void (*WebPUpsampleLinePairFunc)(
    const uint8_t* WEBP_RESTRICT top_y, const uint8_t* WEBP_RESTRICT bottom_y,
    const uint8_t* WEBP_RESTRICT top_u, const uint8_t* WEBP_RESTRICT top_v,
    const uint8_t* WEBP_RESTRICT cur_u, const uint8_t* WEBP_RESTRICT cur_v,
    uint8_t* WEBP_RESTRICT top_dst, uint8_t* WEBP_RESTRICT bottom_dst, int len);

#ifdef FANCY_UPSAMPLING

extern WebPUpsampleLinePairFunc WebPUpsamplers[];

#endif

typedef void (*WebPSamplerRowFunc)(const uint8_t* WEBP_RESTRICT y,
                                   const uint8_t* WEBP_RESTRICT u,
                                   const uint8_t* WEBP_RESTRICT v,
                                   uint8_t* WEBP_RESTRICT dst, int len);

void WebPSamplerProcessPlane(const uint8_t* WEBP_RESTRICT y, int y_stride,
                             const uint8_t* WEBP_RESTRICT u,
                             const uint8_t* WEBP_RESTRICT v, int uv_stride,
                             uint8_t* WEBP_RESTRICT dst, int dst_stride,
                             int width, int height, WebPSamplerRowFunc func);

extern WebPSamplerRowFunc WebPSamplers[];

WebPUpsampleLinePairFunc WebPGetLinePairConverter(int alpha_is_last);

typedef void (*WebPYUV444Converter)(const uint8_t* WEBP_RESTRICT y,
                                    const uint8_t* WEBP_RESTRICT u,
                                    const uint8_t* WEBP_RESTRICT v,
                                    uint8_t* WEBP_RESTRICT dst, int len);

extern WebPYUV444Converter WebPYUV444Converters[];

void WebPInitUpsamplers(void);

void WebPInitSamplers(void);

void WebPInitYUV444Converters(void);

extern void (*WebPConvertARGBToY)(const uint32_t* WEBP_RESTRICT argb,
                                  uint8_t* WEBP_RESTRICT y, int width);

extern void (*WebPConvertARGBToUV)(const uint32_t* WEBP_RESTRICT argb,
                                   uint8_t* WEBP_RESTRICT u,
                                   uint8_t* WEBP_RESTRICT v,
                                   int src_width, int do_store);

extern void (*WebPConvertRGBA32ToUV)(const uint16_t* WEBP_RESTRICT rgb,
                                     uint8_t* WEBP_RESTRICT u,
                                     uint8_t* WEBP_RESTRICT v, int width);

extern void (*WebPConvertRGB24ToY)(const uint8_t* WEBP_RESTRICT rgb,
                                   uint8_t* WEBP_RESTRICT y, int width);
extern void (*WebPConvertBGR24ToY)(const uint8_t* WEBP_RESTRICT bgr,
                                   uint8_t* WEBP_RESTRICT y, int width);

extern void WebPConvertARGBToUV_C(const uint32_t* WEBP_RESTRICT argb,
                                  uint8_t* WEBP_RESTRICT u,
                                  uint8_t* WEBP_RESTRICT v,
                                  int src_width, int do_store);
extern void WebPConvertRGBA32ToUV_C(const uint16_t* WEBP_RESTRICT rgb,
                                    uint8_t* WEBP_RESTRICT u,
                                    uint8_t* WEBP_RESTRICT v, int width);

void WebPInitConvertARGBToYUV(void);

struct WebPRescaler;

typedef void (*WebPRescalerImportRowFunc)(
    struct WebPRescaler* WEBP_RESTRICT const wrk,
    const uint8_t* WEBP_RESTRICT src);

extern WebPRescalerImportRowFunc WebPRescalerImportRowExpand;
extern WebPRescalerImportRowFunc WebPRescalerImportRowShrink;

typedef void (*WebPRescalerExportRowFunc)(struct WebPRescaler* const wrk);
extern WebPRescalerExportRowFunc WebPRescalerExportRowExpand;
extern WebPRescalerExportRowFunc WebPRescalerExportRowShrink;

extern void WebPRescalerImportRowExpand_C(
    struct WebPRescaler* WEBP_RESTRICT const wrk,
    const uint8_t* WEBP_RESTRICT src);
extern void WebPRescalerImportRowShrink_C(
    struct WebPRescaler* WEBP_RESTRICT const wrk,
    const uint8_t* WEBP_RESTRICT src);
extern void WebPRescalerExportRowExpand_C(struct WebPRescaler* const wrk);
extern void WebPRescalerExportRowShrink_C(struct WebPRescaler* const wrk);

extern void WebPRescalerImportRow(
    struct WebPRescaler* WEBP_RESTRICT const wrk,
    const uint8_t* WEBP_RESTRICT src);

extern void WebPRescalerExportRow(struct WebPRescaler* const wrk);

void WebPRescalerDspInit(void);

extern void (*WebPApplyAlphaMultiply)(
    uint8_t* rgba, int alpha_first, int w, int h, int stride);

extern void (*WebPApplyAlphaMultiply4444)(
    uint8_t* rgba4444, int w, int h, int stride);

extern int (*WebPDispatchAlpha)(const uint8_t* WEBP_RESTRICT alpha,
                                int alpha_stride, int width, int height,
                                uint8_t* WEBP_RESTRICT dst, int dst_stride);

extern void (*WebPDispatchAlphaToGreen)(const uint8_t* WEBP_RESTRICT alpha,
                                        int alpha_stride, int width, int height,
                                        uint32_t* WEBP_RESTRICT dst,
                                        int dst_stride);

extern int (*WebPExtractAlpha)(const uint8_t* WEBP_RESTRICT argb,
                               int argb_stride, int width, int height,
                               uint8_t* WEBP_RESTRICT alpha,
                               int alpha_stride);

extern void (*WebPExtractGreen)(const uint32_t* WEBP_RESTRICT argb,
                                uint8_t* WEBP_RESTRICT alpha, int size);

extern void (*WebPMultARGBRow)(uint32_t* const ptr, int width, int inverse);

void WebPMultARGBRows(uint8_t* ptr, int stride, int width, int num_rows,
                      int inverse);

extern void (*WebPMultRow)(uint8_t* WEBP_RESTRICT const ptr,
                           const uint8_t* WEBP_RESTRICT const alpha,
                           int width, int inverse);

void WebPMultRows(uint8_t* WEBP_RESTRICT ptr, int stride,
                  const uint8_t* WEBP_RESTRICT alpha, int alpha_stride,
                  int width, int num_rows, int inverse);

void WebPMultRow_C(uint8_t* WEBP_RESTRICT const ptr,
                   const uint8_t* WEBP_RESTRICT const alpha,
                   int width, int inverse);
void WebPMultARGBRow_C(uint32_t* const ptr, int width, int inverse);

#ifdef WORDS_BIGENDIAN

extern void (*WebPPackARGB)(const uint8_t* WEBP_RESTRICT a,
                            const uint8_t* WEBP_RESTRICT r,
                            const uint8_t* WEBP_RESTRICT g,
                            const uint8_t* WEBP_RESTRICT b,
                            int len, uint32_t* WEBP_RESTRICT out);
#endif

extern void (*WebPPackRGB)(const uint8_t* WEBP_RESTRICT r,
                           const uint8_t* WEBP_RESTRICT g,
                           const uint8_t* WEBP_RESTRICT b,
                           int len, int step, uint32_t* WEBP_RESTRICT out);

extern int (*WebPHasAlpha8b)(const uint8_t* src, int length);

extern int (*WebPHasAlpha32b)(const uint8_t* src, int length);

extern void (*WebPAlphaReplace)(uint32_t* src, int length, uint32_t color);

void WebPInitAlphaProcessing(void);

typedef enum {
  WEBP_FILTER_NONE = 0,
  WEBP_FILTER_HORIZONTAL,
  WEBP_FILTER_VERTICAL,
  WEBP_FILTER_GRADIENT,
  WEBP_FILTER_LAST = WEBP_FILTER_GRADIENT + 1,
  WEBP_FILTER_BEST,
  WEBP_FILTER_FAST
} WEBP_FILTER_TYPE;

typedef void (*WebPFilterFunc)(const uint8_t* WEBP_RESTRICT in,
                               int width, int height, int stride,
                               uint8_t* WEBP_RESTRICT out);

typedef void (*WebPUnfilterFunc)(const uint8_t* prev_line, const uint8_t* preds,
                                 uint8_t* cur_line, int width);

extern WebPFilterFunc WebPFilters[WEBP_FILTER_LAST];

extern WebPUnfilterFunc WebPUnfilters[WEBP_FILTER_LAST];

void VP8FiltersInit(void);

#ifdef __cplusplus
}
#endif

#endif

#ifndef WEBP_UTILS_FILTERS_UTILS_H_
#define WEBP_UTILS_FILTERS_UTILS_H_

#ifdef __cplusplus
extern "C" {
#endif

WEBP_FILTER_TYPE WebPEstimateBestFilter(const uint8_t* data,
                                        int width, int height, int stride);

#ifdef __cplusplus
}
#endif

#endif

#ifdef __cplusplus
extern "C" {
#endif

struct VP8LDecoder;

typedef struct ALPHDecoder ALPHDecoder;
struct ALPHDecoder {
  int width;
  int height;
  int method;
  WEBP_FILTER_TYPE filter;
  int pre_processing;
  struct VP8LDecoder* vp8l_dec;
  VP8Io io;
  int use_8b_decode;

  uint8_t* output;
  const uint8_t* prev_line;
};

void WebPDeallocateAlphaMemory(VP8Decoder* const dec);

#ifdef __cplusplus
}
#endif

#endif

#ifndef WEBP_DEC_VP8I_DEC_H_
#define WEBP_DEC_VP8I_DEC_H_

#include <string.h>

#ifndef WEBP_DEC_COMMON_DEC_H_
#define WEBP_DEC_COMMON_DEC_H_

enum { B_DC_PRED = 0,
       B_TM_PRED = 1,
       B_VE_PRED = 2,
       B_HE_PRED = 3,
       B_RD_PRED = 4,
       B_VR_PRED = 5,
       B_LD_PRED = 6,
       B_VL_PRED = 7,
       B_HD_PRED = 8,
       B_HU_PRED = 9,
       NUM_BMODES = B_HU_PRED + 1 - B_DC_PRED,

       DC_PRED = B_DC_PRED, V_PRED = B_VE_PRED,
       H_PRED = B_HE_PRED, TM_PRED = B_TM_PRED,
       B_PRED = NUM_BMODES,
       NUM_PRED_MODES = 4,

       B_DC_PRED_NOTOP = 4,
       B_DC_PRED_NOLEFT = 5,
       B_DC_PRED_NOTOPLEFT = 6,
       NUM_B_DC_MODES = 7 };

enum { MB_FEATURE_TREE_PROBS = 3,
       NUM_MB_SEGMENTS = 4,
       NUM_REF_LF_DELTAS = 4,
       NUM_MODE_LF_DELTAS = 4,
       MAX_NUM_PARTITIONS = 8,

       NUM_TYPES = 4,
       NUM_BANDS = 8,
       NUM_CTX = 3,
       NUM_PROBAS = 11
     };

int IsValidColorspace(int webp_csp_mode);

#endif

#ifndef WEBP_DEC_VP8LI_DEC_H_
#define WEBP_DEC_VP8LI_DEC_H_

#include <string.h>

#ifndef WEBP_UTILS_BIT_READER_UTILS_H_
#define WEBP_UTILS_BIT_READER_UTILS_H_

#include <assert.h>
#include <stddef.h>

#ifdef _MSC_VER
#include <stdlib.h>
#endif

#if !defined(BITTRACE)
#define BITTRACE 0
#endif

#if (BITTRACE > 0)
struct VP8BitReader;
extern void BitTrace(const struct VP8BitReader* const br, const char label[]);
#define BT_TRACK(br) BitTrace(br, label)
#define VP8Get(BR, L) VP8GetValue(BR, 1, L)
#else
#define BT_TRACK(br)

#define VP8GetValue(BR, N, L) VP8GetValue(BR, N)
#define VP8Get(BR, L) VP8GetValue(BR, 1, L)
#define VP8GetSignedValue(BR, N, L) VP8GetSignedValue(BR, N)
#define VP8GetBit(BR, P, L) VP8GetBit(BR, P)
#define VP8GetBitAlt(BR, P, L) VP8GetBitAlt(BR, P)
#define VP8GetSigned(BR, V, L) VP8GetSigned(BR, V)
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__i386__) || defined(_M_IX86)
#define BITS 24
#elif defined(__x86_64__) || defined(_M_X64)
#define BITS 56
#elif defined(__arm__) || defined(_M_ARM)
#define BITS 24
#elif WEBP_AARCH64
#define BITS 56
#elif defined(__mips__)
#define BITS 24
#elif defined(__wasm__)
#define BITS 56
#else
#define BITS 24
#endif

#if (BITS > 24)
typedef uint64_t bit_t;
#else
typedef uint32_t bit_t;
#endif

typedef uint32_t range_t;

typedef struct VP8BitReader VP8BitReader;
struct VP8BitReader {

  bit_t value;
  range_t range;
  int bits;

  const uint8_t* buf;
  const uint8_t* buf_end;
  const uint8_t* buf_max;
  int eof;
};

void VP8InitBitReader(VP8BitReader* const br,
                      const uint8_t* const start, size_t size);

void VP8BitReaderSetBuffer(VP8BitReader* const br,
                           const uint8_t* const start, size_t size);

void VP8RemapBitReader(VP8BitReader* const br, ptrdiff_t offset);

uint32_t VP8GetValue(VP8BitReader* const br, int num_bits, const char label[]);

int32_t VP8GetSignedValue(VP8BitReader* const br, int num_bits,
                          const char label[]);

#define VP8L_MAX_NUM_BIT_READ 24

#define VP8L_LBITS 64
#define VP8L_WBITS 32

typedef uint64_t vp8l_val_t;

typedef struct {
  vp8l_val_t     val;
  const uint8_t* buf;
  size_t         len;
  size_t         pos;
  int            bit_pos;
  int            eos;
} VP8LBitReader;

void VP8LInitBitReader(VP8LBitReader* const br,
                       const uint8_t* const start,
                       size_t length);

void VP8LBitReaderSetBuffer(VP8LBitReader* const br,
                            const uint8_t* const buffer, size_t length);

uint32_t VP8LReadBits(VP8LBitReader* const br, int n_bits);

static WEBP_INLINE uint32_t VP8LPrefetchBits(VP8LBitReader* const br) {
  return (uint32_t)(br->val >> (br->bit_pos & (VP8L_LBITS - 1)));
}

static WEBP_INLINE int VP8LIsEndOfStream(const VP8LBitReader* const br) {
  assert(br->pos <= br->len);
  return br->eos || ((br->pos == br->len) && (br->bit_pos > VP8L_LBITS));
}

static WEBP_INLINE void VP8LSetBitPos(VP8LBitReader* const br, int val) {
  br->bit_pos = val;
}

extern void VP8LDoFillBitWindow(VP8LBitReader* const br);
static WEBP_INLINE void VP8LFillBitWindow(VP8LBitReader* const br) {
  if (br->bit_pos >= VP8L_WBITS) VP8LDoFillBitWindow(br);
}

#ifdef __cplusplus
}
#endif

#endif

#ifndef WEBP_UTILS_COLOR_CACHE_UTILS_H_
#define WEBP_UTILS_COLOR_CACHE_UTILS_H_

#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  uint32_t* colors;
  int hash_shift;
  int hash_bits;
} VP8LColorCache;

static const uint32_t kHashMul = 0x1e35a7bdu;

static WEBP_UBSAN_IGNORE_UNSIGNED_OVERFLOW WEBP_INLINE
int VP8LHashPix(uint32_t argb, int shift) {
  return (int)((argb * kHashMul) >> shift);
}

static WEBP_INLINE uint32_t VP8LColorCacheLookup(
    const VP8LColorCache* const cc, uint32_t key) {
  assert((key >> cc->hash_bits) == 0u);
  return cc->colors[key];
}

static WEBP_INLINE void VP8LColorCacheSet(const VP8LColorCache* const cc,
                                          uint32_t key, uint32_t argb) {
  assert((key >> cc->hash_bits) == 0u);
  cc->colors[key] = argb;
}

static WEBP_INLINE void VP8LColorCacheInsert(const VP8LColorCache* const cc,
                                             uint32_t argb) {
  const int key = VP8LHashPix(argb, cc->hash_shift);
  cc->colors[key] = argb;
}

static WEBP_INLINE int VP8LColorCacheGetIndex(const VP8LColorCache* const cc,
                                              uint32_t argb) {
  return VP8LHashPix(argb, cc->hash_shift);
}

static WEBP_INLINE int VP8LColorCacheContains(const VP8LColorCache* const cc,
                                              uint32_t argb) {
  const int key = VP8LHashPix(argb, cc->hash_shift);
  return (cc->colors[key] == argb) ? key : -1;
}

int VP8LColorCacheInit(VP8LColorCache* const color_cache, int hash_bits);

void VP8LColorCacheCopy(const VP8LColorCache* const src,
                        VP8LColorCache* const dst);

void VP8LColorCacheClear(VP8LColorCache* const color_cache);

#ifdef __cplusplus
}
#endif

#endif

#ifndef WEBP_UTILS_HUFFMAN_UTILS_H_
#define WEBP_UTILS_HUFFMAN_UTILS_H_

#include <assert.h>

#ifndef WEBP_WEBP_FORMAT_CONSTANTS_H_
#define WEBP_WEBP_FORMAT_CONSTANTS_H_

#define MKFOURCC(a, b, c, d) ((a) | (b) << 8 | (c) << 16 | (uint32_t)(d) << 24)

#define VP8_SIGNATURE 0x9d012a
#define VP8_MAX_PARTITION0_SIZE (1 << 19)
#define VP8_MAX_PARTITION_SIZE  (1 << 24)
#define VP8_FRAME_HEADER_SIZE 10

#define VP8L_SIGNATURE_SIZE          1
#define VP8L_MAGIC_BYTE              0x2f
#define VP8L_IMAGE_SIZE_BITS         14

#define VP8L_VERSION_BITS            3
#define VP8L_VERSION                 0
#define VP8L_FRAME_HEADER_SIZE       5

#define MAX_PALETTE_SIZE             256
#define MAX_CACHE_BITS               11
#define HUFFMAN_CODES_PER_META_CODE  5
#define ARGB_BLACK                   0xff000000

#define DEFAULT_CODE_LENGTH          8
#define MAX_ALLOWED_CODE_LENGTH      15

#define NUM_LITERAL_CODES            256
#define NUM_LENGTH_CODES             24
#define NUM_DISTANCE_CODES           40
#define CODE_LENGTH_CODES            19

#define MIN_HUFFMAN_BITS             2
#define NUM_HUFFMAN_BITS             3

#define MIN_TRANSFORM_BITS           2
#define NUM_TRANSFORM_BITS           3

#define TRANSFORM_PRESENT            1

#define NUM_TRANSFORMS               4

typedef enum {
  PREDICTOR_TRANSFORM      = 0,
  CROSS_COLOR_TRANSFORM    = 1,
  SUBTRACT_GREEN_TRANSFORM = 2,
  COLOR_INDEXING_TRANSFORM = 3
} VP8LImageTransformType;

#define ALPHA_HEADER_LEN            1
#define ALPHA_NO_COMPRESSION        0
#define ALPHA_LOSSLESS_COMPRESSION  1
#define ALPHA_PREPROCESSED_LEVELS   1

#define TAG_SIZE           4
#define CHUNK_SIZE_BYTES   4
#define CHUNK_HEADER_SIZE  8
#define RIFF_HEADER_SIZE   12
#define ANMF_CHUNK_SIZE    16
#define ANIM_CHUNK_SIZE    6
#define VP8X_CHUNK_SIZE    10

#define MAX_CANVAS_SIZE     (1 << 24)
#define MAX_IMAGE_AREA      (1ULL << 32)
#define MAX_LOOP_COUNT      (1 << 16)
#define MAX_DURATION        (1 << 24)
#define MAX_POSITION_OFFSET (1 << 24)

#define MAX_CHUNK_PAYLOAD (~0U - CHUNK_HEADER_SIZE - 1)

#endif

#ifdef __cplusplus
extern "C" {
#endif

#define HUFFMAN_TABLE_BITS      8
#define HUFFMAN_TABLE_MASK      ((1 << HUFFMAN_TABLE_BITS) - 1)

#define LENGTHS_TABLE_BITS      7
#define LENGTHS_TABLE_MASK      ((1 << LENGTHS_TABLE_BITS) - 1)

typedef struct {
  uint8_t bits;
  uint16_t value;
} HuffmanCode;

typedef struct {
  int bits;

  uint32_t value;

} HuffmanCode32;

typedef struct HuffmanTablesSegment {
  HuffmanCode* start;

  HuffmanCode* curr_table;

  struct HuffmanTablesSegment* next;
  int size;
} HuffmanTablesSegment;

typedef struct HuffmanTables {
  HuffmanTablesSegment root;

  HuffmanTablesSegment* curr_segment;
} HuffmanTables;

WEBP_NODISCARD int VP8LHuffmanTablesAllocate(int size,
                                             HuffmanTables* huffman_tables);
void VP8LHuffmanTablesDeallocate(HuffmanTables* const huffman_tables);

#define HUFFMAN_PACKED_BITS 6
#define HUFFMAN_PACKED_TABLE_SIZE (1u << HUFFMAN_PACKED_BITS)

typedef struct HTreeGroup HTreeGroup;
struct HTreeGroup {
  HuffmanCode* htrees[HUFFMAN_CODES_PER_META_CODE];
  int      is_trivial_literal;

  uint32_t literal_arb;

  int is_trivial_code;
  int use_packed_table;

  HuffmanCode32 packed_table[HUFFMAN_PACKED_TABLE_SIZE];
};

WEBP_NODISCARD HTreeGroup* VP8LHtreeGroupsNew(int num_htree_groups);

void VP8LHtreeGroupsFree(HTreeGroup* const htree_groups);

WEBP_NODISCARD int VP8LBuildHuffmanTable(HuffmanTables* const root_table,
                                         int root_bits,
                                         const int code_lengths[],
                                         int code_lengths_size);

#ifdef __cplusplus
}
#endif

#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  READ_DATA = 0,
  READ_HDR = 1,
  READ_DIM = 2
} VP8LDecodeState;

typedef struct VP8LTransform VP8LTransform;
struct VP8LTransform {
  VP8LImageTransformType type;
  int                    bits;
  int                    xsize;
  int                    ysize;
  uint32_t*              data;
};

typedef struct {
  int             color_cache_size;
  VP8LColorCache  color_cache;
  VP8LColorCache  saved_color_cache;

  int             huffman_mask;
  int             huffman_subsample_bits;
  int             huffman_xsize;
  uint32_t*       huffman_image;
  int             num_htree_groups;
  HTreeGroup*     htree_groups;
  HuffmanTables   huffman_tables;
} VP8LMetadata;

typedef struct VP8LDecoder VP8LDecoder;
struct VP8LDecoder {
  VP8StatusCode    status;
  VP8LDecodeState  state;
  VP8Io*           io;

  const WebPDecBuffer* output;

  uint32_t*        pixels;

  uint32_t*        argb_cache;

  VP8LBitReader    br;
  int              incremental;
  VP8LBitReader    saved_br;
  int              saved_last_pixel;

  int              width;
  int              height;
  int              last_row;
  int              last_pixel;

  int              last_out_row;

  VP8LMetadata     hdr;

  int              next_transform;
  VP8LTransform    transforms[NUM_TRANSFORMS];

  uint32_t         transforms_seen;

  uint8_t*         rescaler_memory;
  WebPRescaler*    rescaler;
};

struct ALPHDecoder;

WEBP_NODISCARD int VP8LDecodeAlphaHeader(struct ALPHDecoder* const alph_dec,
                                         const uint8_t* const data,
                                         size_t data_size);

WEBP_NODISCARD int VP8LDecodeAlphaImageStream(
    struct ALPHDecoder* const alph_dec, int last_row);

WEBP_NODISCARD VP8LDecoder* VP8LNew(void);

WEBP_NODISCARD int VP8LDecodeHeader(VP8LDecoder* const dec, VP8Io* const io);

WEBP_NODISCARD int VP8LDecodeImage(VP8LDecoder* const dec);

void VP8LDelete(VP8LDecoder* const dec);

WEBP_NODISCARD int ReadHuffmanCodesHelper(
    int color_cache_bits, int num_htree_groups, int num_htree_groups_max,
    const int* const mapping, VP8LDecoder* const dec,
    HuffmanTables* const huffman_tables, HTreeGroup** const htree_groups);

#ifdef __cplusplus
}
#endif

#endif

#ifndef WEBP_UTILS_RANDOM_UTILS_H_
#define WEBP_UTILS_RANDOM_UTILS_H_

#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

#define VP8_RANDOM_DITHER_FIX 8
#define VP8_RANDOM_TABLE_SIZE 55

typedef struct {
  int index1, index2;
  uint32_t tab[VP8_RANDOM_TABLE_SIZE];
  int amp;
} VP8Random;

void VP8InitRandom(VP8Random* const rg, float dithering);

static WEBP_INLINE int VP8RandomBits2(VP8Random* const rg, int num_bits,
                                      int amp) {
  int diff;
  assert(num_bits + VP8_RANDOM_DITHER_FIX <= 31);
  diff = rg->tab[rg->index1] - rg->tab[rg->index2];
  if (diff < 0) diff += (1u << 31);
  rg->tab[rg->index1] = diff;
  if (++rg->index1 == VP8_RANDOM_TABLE_SIZE) rg->index1 = 0;
  if (++rg->index2 == VP8_RANDOM_TABLE_SIZE) rg->index2 = 0;

  diff = (int)((uint32_t)diff << 1) >> (32 - num_bits);
  diff = (diff * amp) >> VP8_RANDOM_DITHER_FIX;
  diff += 1 << (num_bits - 1);
  return diff;
}

static WEBP_INLINE int VP8RandomBits(VP8Random* const rg, int num_bits) {
  return VP8RandomBits2(rg, num_bits, rg->amp);
}

#ifdef __cplusplus
}
#endif

#endif

#ifndef WEBP_UTILS_THREAD_UTILS_H_
#define WEBP_UTILS_THREAD_UTILS_H_

#ifdef HAVE_CONFIG_H
#include "src/webp/config.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  NOT_OK = 0,
  OK,
  WORK
} WebPWorkerStatus;

typedef int (*WebPWorkerHook)(void*, void*);

typedef struct {
  void* impl;
  WebPWorkerStatus status;
  WebPWorkerHook hook;
  void* data1;
  void* data2;
  int had_error;
} WebPWorker;

typedef struct {

  void (*Init)(WebPWorker* const worker);

  int (*Reset)(WebPWorker* const worker);

  int (*Sync)(WebPWorker* const worker);

  void (*Launch)(WebPWorker* const worker);

  void (*Execute)(WebPWorker* const worker);

  void (*End)(WebPWorker* const worker);
} WebPWorkerInterface;

WEBP_EXTERN int WebPSetWorkerInterface(
    const WebPWorkerInterface* const winterface);

WEBP_EXTERN const WebPWorkerInterface* WebPGetWorkerInterface(void);

#ifdef __cplusplus
}
#endif

#endif

#ifdef __cplusplus
extern "C" {
#endif

#define DEC_MAJ_VERSION 1
#define DEC_MIN_VERSION 6
#define DEC_REV_VERSION 0

#define YUV_SIZE (BPS * 17 + BPS * 9)
#define Y_OFF    (BPS * 1 + 8)
#define U_OFF    (Y_OFF + BPS * 16 + BPS)
#define V_OFF    (U_OFF + 16)

#define MIN_WIDTH_FOR_THREADS 512

typedef struct {
  uint8_t key_frame;
  uint8_t profile;
  uint8_t show;
  uint32_t partition_length;
} VP8FrameHeader;

typedef struct {
  uint16_t width;
  uint16_t height;
  uint8_t xscale;
  uint8_t yscale;
  uint8_t colorspace;
  uint8_t clamp_type;
} VP8PictureHeader;

typedef struct {
  int use_segment;
  int update_map;
  int absolute_delta;
  int8_t quantizer[NUM_MB_SEGMENTS];
  int8_t filter_strength[NUM_MB_SEGMENTS];
} VP8SegmentHeader;

typedef uint8_t VP8ProbaArray[NUM_PROBAS];

typedef struct {
  VP8ProbaArray probas[NUM_CTX];
} VP8BandProbas;

typedef struct {
  uint8_t segments[MB_FEATURE_TREE_PROBS];

  VP8BandProbas bands[NUM_TYPES][NUM_BANDS];
  const VP8BandProbas* bands_ptr[NUM_TYPES][16 + 1];
} VP8Proba;

typedef struct {
  int simple;
  int level;
  int sharpness;
  int use_lf_delta;
  int ref_lf_delta[NUM_REF_LF_DELTAS];
  int mode_lf_delta[NUM_MODE_LF_DELTAS];
} VP8FilterHeader;

typedef struct {
  uint8_t f_limit;
  uint8_t f_ilevel;
  uint8_t f_inner;
  uint8_t hev_thresh;
} VP8FInfo;

typedef struct {
  uint8_t nz;
  uint8_t nz_dc;
} VP8MB;

typedef int quant_t[2];
typedef struct {
  quant_t y1_mat, y2_mat, uv_mat;

  int uv_quant;
  int dither;
} VP8QuantMatrix;

typedef struct {
  int16_t coeffs[384];
  uint8_t is_i4x4;
  uint8_t imodes[16];
  uint8_t uvmode;

  uint32_t non_zero_y;
  uint32_t non_zero_uv;
  uint8_t dither;
  uint8_t skip;
  uint8_t segment;
} VP8MBData;

typedef struct {
  int id;
  int mb_y;
  int filter_row;
  VP8FInfo* f_info;
  VP8MBData* mb_data;
  VP8Io io;
} VP8ThreadContext;

typedef struct {
  uint8_t y[16], u[8], v[8];
} VP8TopSamples;

struct VP8Decoder {
  VP8StatusCode status;
  int ready;
  const char* error_msg;

  VP8BitReader br;
  int incremental;

  VP8FrameHeader   frm_hdr;
  VP8PictureHeader pic_hdr;
  VP8FilterHeader  filter_hdr;
  VP8SegmentHeader segment_hdr;

  WebPWorker worker;
  int mt_method;

  int cache_id;
  int num_caches;
  VP8ThreadContext thread_ctx;

  int mb_w, mb_h;

  int tl_mb_x, tl_mb_y;
  int br_mb_x, br_mb_y;

  uint32_t num_parts_minus_one;

  VP8BitReader parts[MAX_NUM_PARTITIONS];

  int dither;
  VP8Random dithering_rg;

  VP8QuantMatrix dqm[NUM_MB_SEGMENTS];

  VP8Proba proba;
  int use_skip_proba;
  uint8_t skip_p;

  uint8_t* intra_t;
  uint8_t  intra_l[4];

  VP8TopSamples* yuv_t;

  VP8MB* mb_info;
  VP8FInfo* f_info;
  uint8_t* yuv_b;

  uint8_t* cache_y;
  uint8_t* cache_u;
  uint8_t* cache_v;
  int cache_y_stride;
  int cache_uv_stride;

  void* mem;
  size_t mem_size;

  int mb_x, mb_y;
  VP8MBData* mb_data;

  int filter_type;
  VP8FInfo fstrengths[NUM_MB_SEGMENTS][2];

  struct ALPHDecoder* alph_dec;
  const uint8_t* alpha_data;
  size_t alpha_data_size;
  int is_alpha_decoded;
  uint8_t* alpha_plane_mem;
  uint8_t* alpha_plane;
  const uint8_t* alpha_prev_line;
  int alpha_dithering;
};

int VP8SetError(VP8Decoder* const dec,
                VP8StatusCode error, const char* const msg);

void VP8ResetProba(VP8Proba* const proba);
void VP8ParseProba(VP8BitReader* const br, VP8Decoder* const dec);

int VP8ParseIntraModeRow(VP8BitReader* const br, VP8Decoder* const dec);

void VP8ParseQuant(VP8Decoder* const dec);

WEBP_NODISCARD int VP8InitFrame(VP8Decoder* const dec, VP8Io* const io);

VP8StatusCode VP8EnterCritical(VP8Decoder* const dec, VP8Io* const io);

WEBP_NODISCARD int VP8ExitCritical(VP8Decoder* const dec, VP8Io* const io);

int VP8GetThreadMethod(const WebPDecoderOptions* const options,
                       const WebPHeaderStructure* const headers,
                       int width, int height);

void VP8InitDithering(const WebPDecoderOptions* const options,
                      VP8Decoder* const dec);

WEBP_NODISCARD int VP8ProcessRow(VP8Decoder* const dec, VP8Io* const io);

void VP8InitScanline(VP8Decoder* const dec);

WEBP_NODISCARD int VP8DecodeMB(VP8Decoder* const dec,
                               VP8BitReader* const token_br);

const uint8_t* VP8DecompressAlphaRows(VP8Decoder* const dec,
                                      const VP8Io* const io,
                                      int row, int num_rows);

#ifdef __cplusplus
}
#endif

#endif

#ifndef WEBP_UTILS_QUANT_LEVELS_DEC_UTILS_H_
#define WEBP_UTILS_QUANT_LEVELS_DEC_UTILS_H_

#ifdef __cplusplus
extern "C" {
#endif

int WebPDequantizeLevels(uint8_t* const data, int width, int height, int stride,
                         int strength);

#ifdef __cplusplus
}
#endif

#endif

#ifndef WEBP_UTILS_UTILS_H_
#define WEBP_UTILS_UTILS_H_

#ifdef HAVE_CONFIG_H
#include "src/webp/config.h"
#endif

#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef WEBP_MAX_ALLOCABLE_MEMORY
#if SIZE_MAX > (1ULL << 34)
#define WEBP_MAX_ALLOCABLE_MEMORY (1ULL << 34)
#else

#define WEBP_MAX_ALLOCABLE_MEMORY ((1ULL << 31) - (1 << 16))
#endif
#endif

static WEBP_INLINE int CheckSizeOverflow(uint64_t size) {
  return size == (size_t)size;
}

WEBP_EXTERN void* WebPSafeMalloc(uint64_t nmemb, size_t size);

WEBP_EXTERN void* WebPSafeCalloc(uint64_t nmemb, size_t size);

WEBP_EXTERN void WebPSafeFree(void* const ptr);

#define WEBP_ALIGN_CST 31
#define WEBP_ALIGN(PTR) (((uintptr_t)(PTR) + WEBP_ALIGN_CST) & \
                         ~(uintptr_t)WEBP_ALIGN_CST)

#include <string.h>

static WEBP_INLINE uint32_t WebPMemToUint32(const uint8_t* const ptr) {
  uint32_t A;
  memcpy(&A, ptr, sizeof(A));
  return A;
}

static WEBP_INLINE int32_t WebPMemToInt32(const uint8_t* const ptr) {
  return (int32_t)WebPMemToUint32(ptr);
}

static WEBP_INLINE void WebPUint32ToMem(uint8_t* const ptr, uint32_t val) {
  memcpy(ptr, &val, sizeof(val));
}

static WEBP_INLINE void WebPInt32ToMem(uint8_t* const ptr, int val) {
  WebPUint32ToMem(ptr, (uint32_t)val);
}

static WEBP_INLINE int GetLE16(const uint8_t* const data) {
  return (int)(data[0] << 0) | (data[1] << 8);
}

static WEBP_INLINE int GetLE24(const uint8_t* const data) {
  return GetLE16(data) | (data[2] << 16);
}

static WEBP_INLINE uint32_t GetLE32(const uint8_t* const data) {
  return GetLE16(data) | ((uint32_t)GetLE16(data + 2) << 16);
}

static WEBP_INLINE void PutLE16(uint8_t* const data, int val) {
  assert(val < (1 << 16));
  data[0] = (val >> 0) & 0xff;
  data[1] = (val >> 8) & 0xff;
}

static WEBP_INLINE void PutLE24(uint8_t* const data, int val) {
  assert(val < (1 << 24));
  PutLE16(data, val & 0xffff);
  data[2] = (val >> 16) & 0xff;
}

static WEBP_INLINE void PutLE32(uint8_t* const data, uint32_t val) {
  PutLE16(data, (int)(val & 0xffff));
  PutLE16(data + 2, (int)(val >> 16));
}

#if defined(__GNUC__) && \
    ((__GNUC__ == 3 && __GNUC_MINOR__ >= 4) || __GNUC__ >= 4)

static WEBP_INLINE int BitsLog2Floor(uint32_t n) {
  return 31 ^ __builtin_clz(n);
}

static WEBP_INLINE int BitsCtz(uint32_t n) { return __builtin_ctz(n); }
#elif defined(_MSC_VER) && _MSC_VER > 1310 && \
      (defined(_M_X64) || defined(_M_IX86))
#include <intrin.h>
#pragma intrinsic(_BitScanReverse)
#pragma intrinsic(_BitScanForward)

static WEBP_INLINE int BitsLog2Floor(uint32_t n) {
  unsigned long first_set_bit;
  _BitScanReverse(&first_set_bit, n);
  return first_set_bit;
}
static WEBP_INLINE int BitsCtz(uint32_t n) {
  unsigned long first_set_bit;
  _BitScanForward(&first_set_bit, n);
  return first_set_bit;
}
#else
#define WEBP_HAVE_SLOW_CLZ_CTZ

#define WEBP_NEED_LOG_TABLE_8BIT
extern const uint8_t WebPLogTable8bit[256];
static WEBP_INLINE int WebPLog2FloorC(uint32_t n) {
  int log_value = 0;
  while (n >= 256) {
    log_value += 8;
    n >>= 8;
  }
  return log_value + WebPLogTable8bit[n];
}

static WEBP_INLINE int BitsLog2Floor(uint32_t n) { return WebPLog2FloorC(n); }

static WEBP_INLINE int BitsCtz(uint32_t n) {
  int i;
  for (i = 0; i < 32; ++i, n >>= 1) {
    if (n & 1) return i;
  }
  return 32;
}

#endif

struct WebPPicture;

WEBP_EXTERN void WebPCopyPlane(const uint8_t* src, int src_stride,
                               uint8_t* dst, int dst_stride,
                               int width, int height);

WEBP_EXTERN void WebPCopyPixels(const struct WebPPicture* const src,
                                struct WebPPicture* const dst);

WEBP_EXTERN int WebPGetColorPalette(const struct WebPPicture* const pic,
                                    uint32_t* const palette);

#ifdef __cplusplus
}
#endif

#endif

WEBP_NODISCARD static ALPHDecoder* ALPHNew(void) {
  ALPHDecoder* const dec = (ALPHDecoder*)WebPSafeCalloc(1ULL, sizeof(*dec));
  return dec;
}

static void ALPHDelete(ALPHDecoder* const dec) {
  if (dec != NULL) {
    VP8LDelete(dec->vp8l_dec);
    dec->vp8l_dec = NULL;
    WebPSafeFree(dec);
  }
}

WEBP_NODISCARD static int ALPHInit(ALPHDecoder* const dec, const uint8_t* data,
                                   size_t data_size, const VP8Io* const src_io,
                                   uint8_t* output) {
  int ok = 0;
  const uint8_t* const alpha_data = data + ALPHA_HEADER_LEN;
  const size_t alpha_data_size = data_size - ALPHA_HEADER_LEN;
  int rsrv;
  VP8Io* const io = &dec->io;

  assert(data != NULL && output != NULL && src_io != NULL);

  VP8FiltersInit();
  dec->output = output;
  dec->width = src_io->width;
  dec->height = src_io->height;
  assert(dec->width > 0 && dec->height > 0);

  if (data_size <= ALPHA_HEADER_LEN) {
    return 0;
  }

  dec->method = (data[0] >> 0) & 0x03;
  dec->filter = (WEBP_FILTER_TYPE)((data[0] >> 2) & 0x03);
  dec->pre_processing = (data[0] >> 4) & 0x03;
  rsrv = (data[0] >> 6) & 0x03;
  if (dec->method < ALPHA_NO_COMPRESSION ||
      dec->method > ALPHA_LOSSLESS_COMPRESSION ||
      dec->filter >= WEBP_FILTER_LAST ||
      dec->pre_processing > ALPHA_PREPROCESSED_LEVELS ||
      rsrv != 0) {
    return 0;
  }

  if (!VP8InitIo(io)) {
    return 0;
  }
  WebPInitCustomIo(NULL, io);
  io->opaque = dec;
  io->width = src_io->width;
  io->height = src_io->height;

  io->use_cropping = src_io->use_cropping;
  io->crop_left = src_io->crop_left;
  io->crop_right = src_io->crop_right;
  io->crop_top = src_io->crop_top;
  io->crop_bottom = src_io->crop_bottom;

  if (dec->method == ALPHA_NO_COMPRESSION) {
    const size_t alpha_decoded_size = dec->width * dec->height;
    ok = (alpha_data_size >= alpha_decoded_size);
  } else {
    assert(dec->method == ALPHA_LOSSLESS_COMPRESSION);
    ok = VP8LDecodeAlphaHeader(dec, alpha_data, alpha_data_size);
  }

  return ok;
}

WEBP_NODISCARD static int ALPHDecode(VP8Decoder* const dec, int row,
                                     int num_rows) {
  ALPHDecoder* const alph_dec = dec->alph_dec;
  const int width = alph_dec->width;
  const int height = alph_dec->io.crop_bottom;
  if (alph_dec->method == ALPHA_NO_COMPRESSION) {
    int y;
    const uint8_t* prev_line = dec->alpha_prev_line;
    const uint8_t* deltas = dec->alpha_data + ALPHA_HEADER_LEN + row * width;
    uint8_t* dst = dec->alpha_plane + row * width;
    assert(deltas <= &dec->alpha_data[dec->alpha_data_size]);
    assert(WebPUnfilters[alph_dec->filter] != NULL);
    for (y = 0; y < num_rows; ++y) {
      WebPUnfilters[alph_dec->filter](prev_line, deltas, dst, width);
      prev_line = dst;
      dst += width;
      deltas += width;
    }
    dec->alpha_prev_line = prev_line;
  } else {
    assert(alph_dec->vp8l_dec != NULL);
    if (!VP8LDecodeAlphaImageStream(alph_dec, row + num_rows)) {
      return 0;
    }
  }

  if (row + num_rows >= height) {
    dec->is_alpha_decoded = 1;
  }
  return 1;
}

WEBP_NODISCARD static int AllocateAlphaPlane(VP8Decoder* const dec,
                                             const VP8Io* const io) {
  const int stride = io->width;
  const int height = io->crop_bottom;
  const uint64_t alpha_size = (uint64_t)stride * height;
  assert(dec->alpha_plane_mem == NULL);
  dec->alpha_plane_mem =
      (uint8_t*)WebPSafeMalloc(alpha_size, sizeof(*dec->alpha_plane));
  if (dec->alpha_plane_mem == NULL) {
    return VP8SetError(dec, VP8_STATUS_OUT_OF_MEMORY,
                       "Alpha decoder initialization failed.");
  }
  dec->alpha_plane = dec->alpha_plane_mem;
  dec->alpha_prev_line = NULL;
  return 1;
}

void WebPDeallocateAlphaMemory(VP8Decoder* const dec) {
  assert(dec != NULL);
  WebPSafeFree(dec->alpha_plane_mem);
  dec->alpha_plane_mem = NULL;
  dec->alpha_plane = NULL;
  ALPHDelete(dec->alph_dec);
  dec->alph_dec = NULL;
}

WEBP_NODISCARD const uint8_t* VP8DecompressAlphaRows(VP8Decoder* const dec,
                                                     const VP8Io* const io,
                                                     int row, int num_rows) {
  const int width = io->width;
  const int height = io->crop_bottom;

  assert(dec != NULL && io != NULL);

  if (row < 0 || num_rows <= 0 || row + num_rows > height) {
    return NULL;
  }

  if (!dec->is_alpha_decoded) {
    if (dec->alph_dec == NULL) {
      dec->alph_dec = ALPHNew();
      if (dec->alph_dec == NULL) {
        VP8SetError(dec, VP8_STATUS_OUT_OF_MEMORY,
                    "Alpha decoder initialization failed.");
        return NULL;
      }
      if (!AllocateAlphaPlane(dec, io)) goto Error;
      if (!ALPHInit(dec->alph_dec, dec->alpha_data, dec->alpha_data_size,
                    io, dec->alpha_plane)) {
        VP8LDecoder* const vp8l_dec = dec->alph_dec->vp8l_dec;
        VP8SetError(dec,
                    (vp8l_dec == NULL) ? VP8_STATUS_OUT_OF_MEMORY
                                       : vp8l_dec->status,
                    "Alpha decoder initialization failed.");
        goto Error;
      }

      if (dec->alph_dec->pre_processing != ALPHA_PREPROCESSED_LEVELS) {
        dec->alpha_dithering = 0;
      } else {
        num_rows = height - row;
      }
    }

    assert(dec->alph_dec != NULL);
    assert(row + num_rows <= height);
    if (!ALPHDecode(dec, row, num_rows)) goto Error;

    if (dec->is_alpha_decoded) {
      ALPHDelete(dec->alph_dec);
      dec->alph_dec = NULL;
      if (dec->alpha_dithering > 0) {
        uint8_t* const alpha = dec->alpha_plane + io->crop_top * width
                             + io->crop_left;
        if (!WebPDequantizeLevels(alpha,
                                  io->crop_right - io->crop_left,
                                  io->crop_bottom - io->crop_top,
                                  width, dec->alpha_dithering)) {
          goto Error;
        }
      }
    }
  }

  return dec->alpha_plane + row * width;

 Error:
  WebPDeallocateAlphaMemory(dec);
  return NULL;
}

#include <assert.h>
#include <stdlib.h>
#include <string.h>

static const uint8_t kModeBpp[MODE_LAST] = {
  3, 4, 3, 4, 4, 2, 2,
  4, 4, 4, 2,
  1, 1 };

int IsValidColorspace(int webp_csp_mode) {
  return (webp_csp_mode >= MODE_RGB && webp_csp_mode < MODE_LAST);
}

#define MIN_BUFFER_SIZE(WIDTH, HEIGHT, STRIDE)       \
    ((uint64_t)(STRIDE) * ((HEIGHT) - 1) + (WIDTH))

static VP8StatusCode CheckDecBuffer(const WebPDecBuffer* const buffer) {
  int ok = 1;
  const WEBP_CSP_MODE mode = buffer->colorspace;
  const int width = buffer->width;
  const int height = buffer->height;
  if (!IsValidColorspace(mode)) {
    ok = 0;
  } else if (!WebPIsRGBMode(mode)) {
    const WebPYUVABuffer* const buf = &buffer->u.YUVA;
    const int uv_width  = (width  + 1) / 2;
    const int uv_height = (height + 1) / 2;
    const int y_stride = abs(buf->y_stride);
    const int u_stride = abs(buf->u_stride);
    const int v_stride = abs(buf->v_stride);
    const int a_stride = abs(buf->a_stride);
    const uint64_t y_size = MIN_BUFFER_SIZE(width, height, y_stride);
    const uint64_t u_size = MIN_BUFFER_SIZE(uv_width, uv_height, u_stride);
    const uint64_t v_size = MIN_BUFFER_SIZE(uv_width, uv_height, v_stride);
    const uint64_t a_size = MIN_BUFFER_SIZE(width, height, a_stride);
    ok &= (y_size <= buf->y_size);
    ok &= (u_size <= buf->u_size);
    ok &= (v_size <= buf->v_size);
    ok &= (y_stride >= width);
    ok &= (u_stride >= uv_width);
    ok &= (v_stride >= uv_width);
    ok &= (buf->y != NULL);
    ok &= (buf->u != NULL);
    ok &= (buf->v != NULL);
    if (mode == MODE_YUVA) {
      ok &= (a_stride >= width);
      ok &= (a_size <= buf->a_size);
      ok &= (buf->a != NULL);
    }
  } else {
    const WebPRGBABuffer* const buf = &buffer->u.RGBA;
    const int stride = abs(buf->stride);
    const uint64_t size =
        MIN_BUFFER_SIZE((uint64_t)width * kModeBpp[mode], height, stride);
    ok &= (size <= buf->size);
    ok &= (stride >= width * kModeBpp[mode]);
    ok &= (buf->rgba != NULL);
  }
  return ok ? VP8_STATUS_OK : VP8_STATUS_INVALID_PARAM;
}
#undef MIN_BUFFER_SIZE

static VP8StatusCode AllocateBuffer(WebPDecBuffer* const buffer) {
  const int w = buffer->width;
  const int h = buffer->height;
  const WEBP_CSP_MODE mode = buffer->colorspace;

  if (w <= 0 || h <= 0 || !IsValidColorspace(mode)) {
    return VP8_STATUS_INVALID_PARAM;
  }

  if (buffer->is_external_memory <= 0 && buffer->private_memory == NULL) {
    uint8_t* output;
    int uv_stride = 0, a_stride = 0;
    uint64_t uv_size = 0, a_size = 0, total_size;

    int stride;
    uint64_t size;

    if ((uint64_t)w * kModeBpp[mode] >= (1ull << 31)) {
      return VP8_STATUS_INVALID_PARAM;
    }
    stride = w * kModeBpp[mode];
    size = (uint64_t)stride * h;
    if (!WebPIsRGBMode(mode)) {
      uv_stride = (w + 1) / 2;
      uv_size = (uint64_t)uv_stride * ((h + 1) / 2);
      if (mode == MODE_YUVA) {
        a_stride = w;
        a_size = (uint64_t)a_stride * h;
      }
    }
    total_size = size + 2 * uv_size + a_size;

    output = (uint8_t*)WebPSafeMalloc(total_size, sizeof(*output));
    if (output == NULL) {
      return VP8_STATUS_OUT_OF_MEMORY;
    }
    buffer->private_memory = output;

    if (!WebPIsRGBMode(mode)) {
      WebPYUVABuffer* const buf = &buffer->u.YUVA;
      buf->y = output;
      buf->y_stride = stride;
      buf->y_size = (size_t)size;
      buf->u = output + size;
      buf->u_stride = uv_stride;
      buf->u_size = (size_t)uv_size;
      buf->v = output + size + uv_size;
      buf->v_stride = uv_stride;
      buf->v_size = (size_t)uv_size;
      if (mode == MODE_YUVA) {
        buf->a = output + size + 2 * uv_size;
      }
      buf->a_size = (size_t)a_size;
      buf->a_stride = a_stride;
    } else {
      WebPRGBABuffer* const buf = &buffer->u.RGBA;
      buf->rgba = output;
      buf->stride = stride;
      buf->size = (size_t)size;
    }
  }
  return CheckDecBuffer(buffer);
}

VP8StatusCode WebPFlipBuffer(WebPDecBuffer* const buffer) {
  if (buffer == NULL) {
    return VP8_STATUS_INVALID_PARAM;
  }
  if (WebPIsRGBMode(buffer->colorspace)) {
    WebPRGBABuffer* const buf = &buffer->u.RGBA;
    buf->rgba += (int64_t)(buffer->height - 1) * buf->stride;
    buf->stride = -buf->stride;
  } else {
    WebPYUVABuffer* const buf = &buffer->u.YUVA;
    const int64_t H = buffer->height;
    buf->y += (H - 1) * buf->y_stride;
    buf->y_stride = -buf->y_stride;
    buf->u += ((H - 1) >> 1) * buf->u_stride;
    buf->u_stride = -buf->u_stride;
    buf->v += ((H - 1) >> 1) * buf->v_stride;
    buf->v_stride = -buf->v_stride;
    if (buf->a != NULL) {
      buf->a += (H - 1) * buf->a_stride;
      buf->a_stride = -buf->a_stride;
    }
  }
  return VP8_STATUS_OK;
}

VP8StatusCode WebPAllocateDecBuffer(int width, int height,
                                    const WebPDecoderOptions* const options,
                                    WebPDecBuffer* const buffer) {
  VP8StatusCode status;
  if (buffer == NULL || width <= 0 || height <= 0) {
    return VP8_STATUS_INVALID_PARAM;
  }
  if (options != NULL) {
    if (options->use_cropping) {
      const int cw = options->crop_width;
      const int ch = options->crop_height;
      const int x = options->crop_left & ~1;
      const int y = options->crop_top & ~1;
      if (!WebPCheckCropDimensions(width, height, x, y, cw, ch)) {
        return VP8_STATUS_INVALID_PARAM;
      }
      width = cw;
      height = ch;
    }

    if (options->use_scaling) {
#if !defined(WEBP_REDUCE_SIZE)
      int scaled_width = options->scaled_width;
      int scaled_height = options->scaled_height;
      if (!WebPRescalerGetScaledDimensions(
              width, height, &scaled_width, &scaled_height)) {
        return VP8_STATUS_INVALID_PARAM;
      }
      width = scaled_width;
      height = scaled_height;
#else
      return VP8_STATUS_INVALID_PARAM;
#endif
    }
  }
  buffer->width = width;
  buffer->height = height;

  status = AllocateBuffer(buffer);
  if (status != VP8_STATUS_OK) return status;

  if (options != NULL && options->flip) {
    status = WebPFlipBuffer(buffer);
  }
  return status;
}

int WebPInitDecBufferInternal(WebPDecBuffer* buffer, int version) {
  if (WEBP_ABI_IS_INCOMPATIBLE(version, WEBP_DECODER_ABI_VERSION)) {
    return 0;
  }
  if (buffer == NULL) return 0;
  memset(buffer, 0, sizeof(*buffer));
  return 1;
}

void WebPFreeDecBuffer(WebPDecBuffer* buffer) {
  if (buffer != NULL) {
    if (buffer->is_external_memory <= 0) {
      WebPSafeFree(buffer->private_memory);
    }
    buffer->private_memory = NULL;
  }
}

void WebPCopyDecBuffer(const WebPDecBuffer* const src,
                       WebPDecBuffer* const dst) {
  if (src != NULL && dst != NULL) {
    *dst = *src;
    if (src->private_memory != NULL) {
      dst->is_external_memory = 1;
      dst->private_memory = NULL;
    }
  }
}

void WebPGrabDecBuffer(WebPDecBuffer* const src, WebPDecBuffer* const dst) {
  if (src != NULL && dst != NULL) {
    *dst = *src;
    if (src->private_memory != NULL) {
      src->is_external_memory = 1;
      src->private_memory = NULL;
    }
  }
}

VP8StatusCode WebPCopyDecBufferPixels(const WebPDecBuffer* const src_buf,
                                      WebPDecBuffer* const dst_buf) {
  assert(src_buf != NULL && dst_buf != NULL);
  assert(src_buf->colorspace == dst_buf->colorspace);

  dst_buf->width = src_buf->width;
  dst_buf->height = src_buf->height;
  if (CheckDecBuffer(dst_buf) != VP8_STATUS_OK) {
    return VP8_STATUS_INVALID_PARAM;
  }
  if (WebPIsRGBMode(src_buf->colorspace)) {
    const WebPRGBABuffer* const src = &src_buf->u.RGBA;
    const WebPRGBABuffer* const dst = &dst_buf->u.RGBA;
    WebPCopyPlane(src->rgba, src->stride, dst->rgba, dst->stride,
                  src_buf->width * kModeBpp[src_buf->colorspace],
                  src_buf->height);
  } else {
    const WebPYUVABuffer* const src = &src_buf->u.YUVA;
    const WebPYUVABuffer* const dst = &dst_buf->u.YUVA;
    WebPCopyPlane(src->y, src->y_stride, dst->y, dst->y_stride,
                  src_buf->width, src_buf->height);
    WebPCopyPlane(src->u, src->u_stride, dst->u, dst->u_stride,
                  (src_buf->width + 1) / 2, (src_buf->height + 1) / 2);
    WebPCopyPlane(src->v, src->v_stride, dst->v, dst->v_stride,
                  (src_buf->width + 1) / 2, (src_buf->height + 1) / 2);
    if (WebPIsAlphaMode(src_buf->colorspace)) {
      WebPCopyPlane(src->a, src->a_stride, dst->a, dst->a_stride,
                    src_buf->width, src_buf->height);
    }
  }
  return VP8_STATUS_OK;
}

int WebPAvoidSlowMemory(const WebPDecBuffer* const output,
                        const WebPBitstreamFeatures* const features) {
  assert(output != NULL);
  return (output->is_external_memory >= 2) &&
         WebPIsPremultipliedMode(output->colorspace) &&
         (features != NULL && features->has_alpha);
}

#include <assert.h>
#include <stdlib.h>
#include <string.h>

static const uint16_t kScan[16] = {
  0 +  0 * BPS,  4 +  0 * BPS, 8 +  0 * BPS, 12 +  0 * BPS,
  0 +  4 * BPS,  4 +  4 * BPS, 8 +  4 * BPS, 12 +  4 * BPS,
  0 +  8 * BPS,  4 +  8 * BPS, 8 +  8 * BPS, 12 +  8 * BPS,
  0 + 12 * BPS,  4 + 12 * BPS, 8 + 12 * BPS, 12 + 12 * BPS
};

static int CheckMode(int mb_x, int mb_y, int mode) {
  if (mode == B_DC_PRED) {
    if (mb_x == 0) {
      return (mb_y == 0) ? B_DC_PRED_NOTOPLEFT : B_DC_PRED_NOLEFT;
    } else {
      return (mb_y == 0) ? B_DC_PRED_NOTOP : B_DC_PRED;
    }
  }
  return mode;
}

static void Copy32b(uint8_t* const dst, const uint8_t* const src) {
  memcpy(dst, src, 4);
}

static WEBP_INLINE void DoTransform(uint32_t bits, const int16_t* const src,
                                    uint8_t* const dst) {
  switch (bits >> 30) {
    case 3:
      VP8Transform(src, dst, 0);
      break;
    case 2:
      VP8TransformAC3(src, dst);
      break;
    case 1:
      VP8TransformDC(src, dst);
      break;
    default:
      break;
  }
}

static void DoUVTransform(uint32_t bits, const int16_t* const src,
                          uint8_t* const dst) {
  if (bits & 0xff) {
    if (bits & 0xaa) {
      VP8TransformUV(src, dst);
    } else {
      VP8TransformDCUV(src, dst);
    }
  }
}

static void ReconstructRow(const VP8Decoder* const dec,
                           const VP8ThreadContext* ctx) {
  int j;
  int mb_x;
  const int mb_y = ctx->mb_y;
  const int cache_id = ctx->id;
  uint8_t* const y_dst = dec->yuv_b + Y_OFF;
  uint8_t* const u_dst = dec->yuv_b + U_OFF;
  uint8_t* const v_dst = dec->yuv_b + V_OFF;

  for (j = 0; j < 16; ++j) {
    y_dst[j * BPS - 1] = 129;
  }
  for (j = 0; j < 8; ++j) {
    u_dst[j * BPS - 1] = 129;
    v_dst[j * BPS - 1] = 129;
  }

  if (mb_y > 0) {
    y_dst[-1 - BPS] = u_dst[-1 - BPS] = v_dst[-1 - BPS] = 129;
  } else {

    memset(y_dst - BPS - 1, 127, 16 + 4 + 1);
    memset(u_dst - BPS - 1, 127, 8 + 1);
    memset(v_dst - BPS - 1, 127, 8 + 1);
  }

  for (mb_x = 0; mb_x < dec->mb_w; ++mb_x) {
    const VP8MBData* const block = ctx->mb_data + mb_x;

    if (mb_x > 0) {
      for (j = -1; j < 16; ++j) {
        Copy32b(&y_dst[j * BPS - 4], &y_dst[j * BPS + 12]);
      }
      for (j = -1; j < 8; ++j) {
        Copy32b(&u_dst[j * BPS - 4], &u_dst[j * BPS + 4]);
        Copy32b(&v_dst[j * BPS - 4], &v_dst[j * BPS + 4]);
      }
    }
    {

      VP8TopSamples* const top_yuv = dec->yuv_t + mb_x;
      const int16_t* const coeffs = block->coeffs;
      uint32_t bits = block->non_zero_y;
      int n;

      if (mb_y > 0) {
        memcpy(y_dst - BPS, top_yuv[0].y, 16);
        memcpy(u_dst - BPS, top_yuv[0].u, 8);
        memcpy(v_dst - BPS, top_yuv[0].v, 8);
      }

      if (block->is_i4x4) {
        uint32_t* const top_right = (uint32_t*)(y_dst - BPS + 16);

        if (mb_y > 0) {
          if (mb_x >= dec->mb_w - 1) {
            memset(top_right, top_yuv[0].y[15], sizeof(*top_right));
          } else {
            memcpy(top_right, top_yuv[1].y, sizeof(*top_right));
          }
        }

        top_right[BPS] = top_right[2 * BPS] = top_right[3 * BPS] = top_right[0];

        for (n = 0; n < 16; ++n, bits <<= 2) {
          uint8_t* const dst = y_dst + kScan[n];
          VP8PredLuma4[block->imodes[n]](dst);
          DoTransform(bits, coeffs + n * 16, dst);
        }
      } else {
        const int pred_func = CheckMode(mb_x, mb_y, block->imodes[0]);
        VP8PredLuma16[pred_func](y_dst);
        if (bits != 0) {
          for (n = 0; n < 16; ++n, bits <<= 2) {
            DoTransform(bits, coeffs + n * 16, y_dst + kScan[n]);
          }
        }
      }
      {

        const uint32_t bits_uv = block->non_zero_uv;
        const int pred_func = CheckMode(mb_x, mb_y, block->uvmode);
        VP8PredChroma8[pred_func](u_dst);
        VP8PredChroma8[pred_func](v_dst);
        DoUVTransform(bits_uv >> 0, coeffs + 16 * 16, u_dst);
        DoUVTransform(bits_uv >> 8, coeffs + 20 * 16, v_dst);
      }

      if (mb_y < dec->mb_h - 1) {
        memcpy(top_yuv[0].y, y_dst + 15 * BPS, 16);
        memcpy(top_yuv[0].u, u_dst +  7 * BPS,  8);
        memcpy(top_yuv[0].v, v_dst +  7 * BPS,  8);
      }
    }

    {
      const int y_offset = cache_id * 16 * dec->cache_y_stride;
      const int uv_offset = cache_id * 8 * dec->cache_uv_stride;
      uint8_t* const y_out = dec->cache_y + mb_x * 16 + y_offset;
      uint8_t* const u_out = dec->cache_u + mb_x * 8 + uv_offset;
      uint8_t* const v_out = dec->cache_v + mb_x * 8 + uv_offset;
      for (j = 0; j < 16; ++j) {
        memcpy(y_out + j * dec->cache_y_stride, y_dst + j * BPS, 16);
      }
      for (j = 0; j < 8; ++j) {
        memcpy(u_out + j * dec->cache_uv_stride, u_dst + j * BPS, 8);
        memcpy(v_out + j * dec->cache_uv_stride, v_dst + j * BPS, 8);
      }
    }
  }
}

static const uint8_t kFilterExtraRows[3] = { 0, 2, 8 };

static void DoFilter(const VP8Decoder* const dec, int mb_x, int mb_y) {
  const VP8ThreadContext* const ctx = &dec->thread_ctx;
  const int cache_id = ctx->id;
  const int y_bps = dec->cache_y_stride;
  const VP8FInfo* const f_info = ctx->f_info + mb_x;
  uint8_t* const y_dst = dec->cache_y + cache_id * 16 * y_bps + mb_x * 16;
  const int ilevel = f_info->f_ilevel;
  const int limit = f_info->f_limit;
  if (limit == 0) {
    return;
  }
  assert(limit >= 3);
  if (dec->filter_type == 1) {
    if (mb_x > 0) {
      VP8SimpleHFilter16(y_dst, y_bps, limit + 4);
    }
    if (f_info->f_inner) {
      VP8SimpleHFilter16i(y_dst, y_bps, limit);
    }
    if (mb_y > 0) {
      VP8SimpleVFilter16(y_dst, y_bps, limit + 4);
    }
    if (f_info->f_inner) {
      VP8SimpleVFilter16i(y_dst, y_bps, limit);
    }
  } else {
    const int uv_bps = dec->cache_uv_stride;
    uint8_t* const u_dst = dec->cache_u + cache_id * 8 * uv_bps + mb_x * 8;
    uint8_t* const v_dst = dec->cache_v + cache_id * 8 * uv_bps + mb_x * 8;
    const int hev_thresh = f_info->hev_thresh;
    if (mb_x > 0) {
      VP8HFilter16(y_dst, y_bps, limit + 4, ilevel, hev_thresh);
      VP8HFilter8(u_dst, v_dst, uv_bps, limit + 4, ilevel, hev_thresh);
    }
    if (f_info->f_inner) {
      VP8HFilter16i(y_dst, y_bps, limit, ilevel, hev_thresh);
      VP8HFilter8i(u_dst, v_dst, uv_bps, limit, ilevel, hev_thresh);
    }
    if (mb_y > 0) {
      VP8VFilter16(y_dst, y_bps, limit + 4, ilevel, hev_thresh);
      VP8VFilter8(u_dst, v_dst, uv_bps, limit + 4, ilevel, hev_thresh);
    }
    if (f_info->f_inner) {
      VP8VFilter16i(y_dst, y_bps, limit, ilevel, hev_thresh);
      VP8VFilter8i(u_dst, v_dst, uv_bps, limit, ilevel, hev_thresh);
    }
  }
}

static void FilterRow(const VP8Decoder* const dec) {
  int mb_x;
  const int mb_y = dec->thread_ctx.mb_y;
  assert(dec->thread_ctx.filter_row);
  for (mb_x = dec->tl_mb_x; mb_x < dec->br_mb_x; ++mb_x) {
    DoFilter(dec, mb_x, mb_y);
  }
}

static void PrecomputeFilterStrengths(VP8Decoder* const dec) {
  if (dec->filter_type > 0) {
    int s;
    const VP8FilterHeader* const hdr = &dec->filter_hdr;
    for (s = 0; s < NUM_MB_SEGMENTS; ++s) {
      int i4x4;

      int base_level;
      if (dec->segment_hdr.use_segment) {
        base_level = dec->segment_hdr.filter_strength[s];
        if (!dec->segment_hdr.absolute_delta) {
          base_level += hdr->level;
        }
      } else {
        base_level = hdr->level;
      }
      for (i4x4 = 0; i4x4 <= 1; ++i4x4) {
        VP8FInfo* const info = &dec->fstrengths[s][i4x4];
        int level = base_level;
        if (hdr->use_lf_delta) {
          level += hdr->ref_lf_delta[0];
          if (i4x4) {
            level += hdr->mode_lf_delta[0];
          }
        }
        level = (level < 0) ? 0 : (level > 63) ? 63 : level;
        if (level > 0) {
          int ilevel = level;
          if (hdr->sharpness > 0) {
            if (hdr->sharpness > 4) {
              ilevel >>= 2;
            } else {
              ilevel >>= 1;
            }
            if (ilevel > 9 - hdr->sharpness) {
              ilevel = 9 - hdr->sharpness;
            }
          }
          if (ilevel < 1) ilevel = 1;
          info->f_ilevel = ilevel;
          info->f_limit = 2 * level + ilevel;
          info->hev_thresh = (level >= 40) ? 2 : (level >= 15) ? 1 : 0;
        } else {
          info->f_limit = 0;
        }
        info->f_inner = i4x4;
      }
    }
  }
}

#define MIN_DITHER_AMP 4

#define DITHER_AMP_TAB_SIZE 12
static const uint8_t kQuantToDitherAmp[DITHER_AMP_TAB_SIZE] = {

  8, 7, 6, 4, 4, 2, 2, 2, 1, 1, 1, 1
};

void VP8InitDithering(const WebPDecoderOptions* const options,
                      VP8Decoder* const dec) {
  assert(dec != NULL);
  if (options != NULL) {
    const int d = options->dithering_strength;
    const int max_amp = (1 << VP8_RANDOM_DITHER_FIX) - 1;
    const int f = (d < 0) ? 0 : (d > 100) ? max_amp : (d * max_amp / 100);
    if (f > 0) {
      int s;
      int all_amp = 0;
      for (s = 0; s < NUM_MB_SEGMENTS; ++s) {
        VP8QuantMatrix* const dqm = &dec->dqm[s];
        if (dqm->uv_quant < DITHER_AMP_TAB_SIZE) {
          const int idx = (dqm->uv_quant < 0) ? 0 : dqm->uv_quant;
          dqm->dither = (f * kQuantToDitherAmp[idx]) >> 3;
        }
        all_amp |= dqm->dither;
      }
      if (all_amp != 0) {
        VP8InitRandom(&dec->dithering_rg, 1.0f);
        dec->dither = 1;
      }
    }

    dec->alpha_dithering = options->alpha_dithering_strength;
    if (dec->alpha_dithering > 100) {
      dec->alpha_dithering = 100;
    } else if (dec->alpha_dithering < 0) {
      dec->alpha_dithering = 0;
    }
  }
}

static void Dither8x8(VP8Random* const rg, uint8_t* dst, int bps, int amp) {
  uint8_t dither[64];
  int i;
  for (i = 0; i < 8 * 8; ++i) {
    dither[i] = VP8RandomBits2(rg, VP8_DITHER_AMP_BITS + 1, amp);
  }
  VP8DitherCombine8x8(dither, dst, bps);
}

static void DitherRow(VP8Decoder* const dec) {
  int mb_x;
  assert(dec->dither);
  for (mb_x = dec->tl_mb_x; mb_x < dec->br_mb_x; ++mb_x) {
    const VP8ThreadContext* const ctx = &dec->thread_ctx;
    const VP8MBData* const data = ctx->mb_data + mb_x;
    const int cache_id = ctx->id;
    const int uv_bps = dec->cache_uv_stride;
    if (data->dither >= MIN_DITHER_AMP) {
      uint8_t* const u_dst = dec->cache_u + cache_id * 8 * uv_bps + mb_x * 8;
      uint8_t* const v_dst = dec->cache_v + cache_id * 8 * uv_bps + mb_x * 8;
      Dither8x8(&dec->dithering_rg, u_dst, uv_bps, data->dither);
      Dither8x8(&dec->dithering_rg, v_dst, uv_bps, data->dither);
    }
  }
}

#define MACROBLOCK_VPOS(mb_y)  ((mb_y) * 16)

static int FinishRow(void* arg1, void* arg2) {
  VP8Decoder* const dec = (VP8Decoder*)arg1;
  VP8Io* const io = (VP8Io*)arg2;
  int ok = 1;
  const VP8ThreadContext* const ctx = &dec->thread_ctx;
  const int cache_id = ctx->id;
  const int extra_y_rows = kFilterExtraRows[dec->filter_type];
  const int ysize = extra_y_rows * dec->cache_y_stride;
  const int uvsize = (extra_y_rows / 2) * dec->cache_uv_stride;
  const int y_offset = cache_id * 16 * dec->cache_y_stride;
  const int uv_offset = cache_id * 8 * dec->cache_uv_stride;
  uint8_t* const ydst = dec->cache_y - ysize + y_offset;
  uint8_t* const udst = dec->cache_u - uvsize + uv_offset;
  uint8_t* const vdst = dec->cache_v - uvsize + uv_offset;
  const int mb_y = ctx->mb_y;
  const int is_first_row = (mb_y == 0);
  const int is_last_row = (mb_y >= dec->br_mb_y - 1);

  if (dec->mt_method == 2) {
    ReconstructRow(dec, ctx);
  }

  if (ctx->filter_row) {
    FilterRow(dec);
  }

  if (dec->dither) {
    DitherRow(dec);
  }

  if (io->put != NULL) {
    int y_start = MACROBLOCK_VPOS(mb_y);
    int y_end = MACROBLOCK_VPOS(mb_y + 1);
    if (!is_first_row) {
      y_start -= extra_y_rows;
      io->y = ydst;
      io->u = udst;
      io->v = vdst;
    } else {
      io->y = dec->cache_y + y_offset;
      io->u = dec->cache_u + uv_offset;
      io->v = dec->cache_v + uv_offset;
    }

    if (!is_last_row) {
      y_end -= extra_y_rows;
    }
    if (y_end > io->crop_bottom) {
      y_end = io->crop_bottom;
    }

    io->a = NULL;
    if (dec->alpha_data != NULL && y_start < y_end) {
      io->a = VP8DecompressAlphaRows(dec, io, y_start, y_end - y_start);
      if (io->a == NULL) {
        return VP8SetError(dec, VP8_STATUS_BITSTREAM_ERROR,
                           "Could not decode alpha data.");
      }
    }
    if (y_start < io->crop_top) {
      const int delta_y = io->crop_top - y_start;
      y_start = io->crop_top;
      assert(!(delta_y & 1));
      io->y += dec->cache_y_stride * delta_y;
      io->u += dec->cache_uv_stride * (delta_y >> 1);
      io->v += dec->cache_uv_stride * (delta_y >> 1);
      if (io->a != NULL) {
        io->a += io->width * delta_y;
      }
    }
    if (y_start < y_end) {
      io->y += io->crop_left;
      io->u += io->crop_left >> 1;
      io->v += io->crop_left >> 1;
      if (io->a != NULL) {
        io->a += io->crop_left;
      }
      io->mb_y = y_start - io->crop_top;
      io->mb_w = io->crop_right - io->crop_left;
      io->mb_h = y_end - y_start;
      ok = io->put(io);
    }
  }

  if (cache_id + 1 == dec->num_caches) {
    if (!is_last_row) {
      memcpy(dec->cache_y - ysize, ydst + 16 * dec->cache_y_stride, ysize);
      memcpy(dec->cache_u - uvsize, udst + 8 * dec->cache_uv_stride, uvsize);
      memcpy(dec->cache_v - uvsize, vdst + 8 * dec->cache_uv_stride, uvsize);
    }
  }

  return ok;
}

#undef MACROBLOCK_VPOS

int VP8ProcessRow(VP8Decoder* const dec, VP8Io* const io) {
  int ok = 1;
  VP8ThreadContext* const ctx = &dec->thread_ctx;
  const int filter_row =
      (dec->filter_type > 0) &&
      (dec->mb_y >= dec->tl_mb_y) && (dec->mb_y <= dec->br_mb_y);
  if (dec->mt_method == 0) {

    ctx->mb_y = dec->mb_y;
    ctx->filter_row = filter_row;
    ReconstructRow(dec, ctx);
    ok = FinishRow(dec, io);
  } else {
    WebPWorker* const worker = &dec->worker;

    ok &= WebPGetWorkerInterface()->Sync(worker);
    assert(worker->status == OK);
    if (ok) {
      ctx->io = *io;
      ctx->id = dec->cache_id;
      ctx->mb_y = dec->mb_y;
      ctx->filter_row = filter_row;
      if (dec->mt_method == 2) {
        VP8MBData* const tmp = ctx->mb_data;
        ctx->mb_data = dec->mb_data;
        dec->mb_data = tmp;
      } else {

        ReconstructRow(dec, ctx);
      }
      if (filter_row) {
        VP8FInfo* const tmp = ctx->f_info;
        ctx->f_info = dec->f_info;
        dec->f_info = tmp;
      }

      WebPGetWorkerInterface()->Launch(worker);
      if (++dec->cache_id == dec->num_caches) {
        dec->cache_id = 0;
      }
    }
  }
  return ok;
}

VP8StatusCode VP8EnterCritical(VP8Decoder* const dec, VP8Io* const io) {

  if (io->setup != NULL && !io->setup(io)) {
    VP8SetError(dec, VP8_STATUS_USER_ABORT, "Frame setup failed");
    return dec->status;
  }

  if (io->bypass_filtering) {
    dec->filter_type = 0;
  }

  {
    const int extra_pixels = kFilterExtraRows[dec->filter_type];
    if (dec->filter_type == 2) {

      dec->tl_mb_x = 0;
      dec->tl_mb_y = 0;
    } else {

      dec->tl_mb_x = (io->crop_left - extra_pixels) >> 4;
      dec->tl_mb_y = (io->crop_top - extra_pixels) >> 4;
      if (dec->tl_mb_x < 0) dec->tl_mb_x = 0;
      if (dec->tl_mb_y < 0) dec->tl_mb_y = 0;
    }

    dec->br_mb_y = (io->crop_bottom + 15 + extra_pixels) >> 4;
    dec->br_mb_x = (io->crop_right + 15 + extra_pixels) >> 4;
    if (dec->br_mb_x > dec->mb_w) {
      dec->br_mb_x = dec->mb_w;
    }
    if (dec->br_mb_y > dec->mb_h) {
      dec->br_mb_y = dec->mb_h;
    }
  }
  PrecomputeFilterStrengths(dec);
  return VP8_STATUS_OK;
}

int VP8ExitCritical(VP8Decoder* const dec, VP8Io* const io) {
  int ok = 1;
  if (dec->mt_method > 0) {
    ok = WebPGetWorkerInterface()->Sync(&dec->worker);
  }

  if (io->teardown != NULL) {
    io->teardown(io);
  }
  return ok;
}

#define MT_CACHE_LINES 3
#define ST_CACHE_LINES 1

static int InitThreadContext(VP8Decoder* const dec) {
  dec->cache_id = 0;
  if (dec->mt_method > 0) {
    WebPWorker* const worker = &dec->worker;
    if (!WebPGetWorkerInterface()->Reset(worker)) {
      return VP8SetError(dec, VP8_STATUS_OUT_OF_MEMORY,
                         "thread initialization failed.");
    }
    worker->data1 = dec;
    worker->data2 = (void*)&dec->thread_ctx.io;
    worker->hook = FinishRow;
    dec->num_caches =
        (dec->filter_type > 0) ? MT_CACHE_LINES : MT_CACHE_LINES - 1;
  } else {
    dec->num_caches = ST_CACHE_LINES;
  }
  return 1;
}

int VP8GetThreadMethod(const WebPDecoderOptions* const options,
                       const WebPHeaderStructure* const headers,
                       int width, int height) {
  if (options == NULL || options->use_threads == 0) {
    return 0;
  }
  (void)headers;
  (void)width;
  (void)height;
  assert(headers == NULL || !headers->is_lossless);
#if defined(WEBP_USE_THREAD)
  if (width >= MIN_WIDTH_FOR_THREADS) return 2;
#endif
  return 0;
}

#undef MT_CACHE_LINES
#undef ST_CACHE_LINES

static int AllocateMemory(VP8Decoder* const dec) {
  const int num_caches = dec->num_caches;
  const int mb_w = dec->mb_w;

  const size_t intra_pred_mode_size = 4 * mb_w * sizeof(uint8_t);
  const size_t top_size = sizeof(VP8TopSamples) * mb_w;
  const size_t mb_info_size = (mb_w + 1) * sizeof(VP8MB);
  const size_t f_info_size =
      (dec->filter_type > 0) ?
          mb_w * (dec->mt_method > 0 ? 2 : 1) * sizeof(VP8FInfo)
        : 0;
  const size_t yuv_size = YUV_SIZE * sizeof(*dec->yuv_b);
  const size_t mb_data_size =
      (dec->mt_method == 2 ? 2 : 1) * mb_w * sizeof(*dec->mb_data);
  const size_t cache_height = (16 * num_caches
                            + kFilterExtraRows[dec->filter_type]) * 3 / 2;
  const size_t cache_size = top_size * cache_height;

  const uint64_t alpha_size = (dec->alpha_data != NULL) ?
      (uint64_t)dec->pic_hdr.width * dec->pic_hdr.height : 0ULL;
  const uint64_t needed = (uint64_t)intra_pred_mode_size
                        + top_size + mb_info_size + f_info_size
                        + yuv_size + mb_data_size
                        + cache_size + alpha_size + WEBP_ALIGN_CST;
  uint8_t* mem;

  if (!CheckSizeOverflow(needed)) return 0;
  if (needed > dec->mem_size) {
    WebPSafeFree(dec->mem);
    dec->mem_size = 0;
    dec->mem = WebPSafeMalloc(needed, sizeof(uint8_t));
    if (dec->mem == NULL) {
      return VP8SetError(dec, VP8_STATUS_OUT_OF_MEMORY,
                         "no memory during frame initialization.");
    }

    dec->mem_size = (size_t)needed;
  }

  mem = (uint8_t*)dec->mem;
  dec->intra_t = mem;
  mem += intra_pred_mode_size;

  dec->yuv_t = (VP8TopSamples*)mem;
  mem += top_size;

  dec->mb_info = ((VP8MB*)mem) + 1;
  mem += mb_info_size;

  dec->f_info = f_info_size ? (VP8FInfo*)mem : NULL;
  mem += f_info_size;
  dec->thread_ctx.id = 0;
  dec->thread_ctx.f_info = dec->f_info;
  if (dec->filter_type > 0 && dec->mt_method > 0) {

    dec->thread_ctx.f_info += mb_w;
  }

  mem = (uint8_t*)WEBP_ALIGN(mem);
  assert((yuv_size & WEBP_ALIGN_CST) == 0);
  dec->yuv_b = mem;
  mem += yuv_size;

  dec->mb_data = (VP8MBData*)mem;
  dec->thread_ctx.mb_data = (VP8MBData*)mem;
  if (dec->mt_method == 2) {
    dec->thread_ctx.mb_data += mb_w;
  }
  mem += mb_data_size;

  dec->cache_y_stride = 16 * mb_w;
  dec->cache_uv_stride = 8 * mb_w;
  {
    const int extra_rows = kFilterExtraRows[dec->filter_type];
    const int extra_y = extra_rows * dec->cache_y_stride;
    const int extra_uv = (extra_rows / 2) * dec->cache_uv_stride;
    dec->cache_y = mem + extra_y;
    dec->cache_u = dec->cache_y
                  + 16 * num_caches * dec->cache_y_stride + extra_uv;
    dec->cache_v = dec->cache_u
                  + 8 * num_caches * dec->cache_uv_stride + extra_uv;
    dec->cache_id = 0;
  }
  mem += cache_size;

  dec->alpha_plane = alpha_size ? mem : NULL;
  mem += alpha_size;
  assert(mem <= (uint8_t*)dec->mem + dec->mem_size);

  memset(dec->mb_info - 1, 0, mb_info_size);
  VP8InitScanline(dec);

  memset(dec->intra_t, B_DC_PRED, intra_pred_mode_size);

  return 1;
}

static void InitIo(VP8Decoder* const dec, VP8Io* io) {

  io->mb_y = 0;
  io->y = dec->cache_y;
  io->u = dec->cache_u;
  io->v = dec->cache_v;
  io->y_stride = dec->cache_y_stride;
  io->uv_stride = dec->cache_uv_stride;
  io->a = NULL;
}

int VP8InitFrame(VP8Decoder* const dec, VP8Io* const io) {
  if (!InitThreadContext(dec)) return 0;
  if (!AllocateMemory(dec)) return 0;
  InitIo(dec, io);
  VP8DspInit();
  return 1;
}

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define CHUNK_SIZE 4096
#define MAX_MB_SIZE 4096

typedef enum {
  STATE_WEBP_HEADER,
  STATE_VP8_HEADER,
  STATE_VP8_PARTS0,
  STATE_VP8_DATA,
  STATE_VP8L_HEADER,
  STATE_VP8L_DATA,
  STATE_DONE,
  STATE_ERROR
} DecState;

typedef enum {
  MEM_MODE_NONE = 0,
  MEM_MODE_APPEND,
  MEM_MODE_MAP
} MemBufferMode;

typedef struct {
  MemBufferMode mode;
  size_t start;
  size_t end;
  size_t buf_size;
  uint8_t* buf;

  size_t part0_size;
  const uint8_t* part0_buf;
} MemBuffer;

struct WebPIDecoder {
  DecState state;
  WebPDecParams params;
  int is_lossless;
  void* dec;
  VP8Io io;

  MemBuffer mem;
  WebPDecBuffer output;

  WebPDecBuffer* final_output;
  size_t chunk_size;

  int last_mb_y;
};

typedef struct {
  VP8MB left;
  VP8MB info;
  VP8BitReader token_br;
} MBContext;

static WEBP_INLINE size_t MemDataSize(const MemBuffer* mem) {
  return (mem->end - mem->start);
}

static int NeedCompressedAlpha(const WebPIDecoder* const idec) {
  if (idec->state == STATE_WEBP_HEADER) {

    return 0;
  }
  if (idec->is_lossless) {
    return 0;
  } else {
    const VP8Decoder* const dec = (VP8Decoder*)idec->dec;
    assert(dec != NULL);
    return (dec->alpha_data != NULL) && !dec->is_alpha_decoded;
  }
}

static void DoRemap(WebPIDecoder* const idec, ptrdiff_t offset) {
  MemBuffer* const mem = &idec->mem;
  const uint8_t* const new_base = mem->buf + mem->start;

  idec->io.data = new_base;
  idec->io.data_size = MemDataSize(mem);

  if (idec->dec != NULL) {
    if (!idec->is_lossless) {
      VP8Decoder* const dec = (VP8Decoder*)idec->dec;
      const uint32_t last_part = dec->num_parts_minus_one;
      if (offset != 0) {
        uint32_t p;
        for (p = 0; p <= last_part; ++p) {
          VP8RemapBitReader(dec->parts + p, offset);
        }

        if (mem->mode == MEM_MODE_MAP) {
          VP8RemapBitReader(&dec->br, offset);
        }
      }
      {
        const uint8_t* const last_start = dec->parts[last_part].buf;

        if (last_start != NULL) {
          VP8BitReaderSetBuffer(&dec->parts[last_part], last_start,
                                mem->buf + mem->end - last_start);
        }
      }
      if (NeedCompressedAlpha(idec)) {
        ALPHDecoder* const alph_dec = dec->alph_dec;
        dec->alpha_data += offset;
        if (alph_dec != NULL && alph_dec->vp8l_dec != NULL) {
          if (alph_dec->method == ALPHA_LOSSLESS_COMPRESSION) {
            VP8LDecoder* const alph_vp8l_dec = alph_dec->vp8l_dec;
            assert(dec->alpha_data_size >= ALPHA_HEADER_LEN);
            VP8LBitReaderSetBuffer(&alph_vp8l_dec->br,
                                   dec->alpha_data + ALPHA_HEADER_LEN,
                                   dec->alpha_data_size - ALPHA_HEADER_LEN);
          } else {

          }
        }
      }
    } else {
      VP8LDecoder* const dec = (VP8LDecoder*)idec->dec;
      VP8LBitReaderSetBuffer(&dec->br, new_base, MemDataSize(mem));
    }
  }
}

WEBP_NODISCARD static int AppendToMemBuffer(WebPIDecoder* const idec,
                                            const uint8_t* const data,
                                            size_t data_size) {
  VP8Decoder* const dec = (VP8Decoder*)idec->dec;
  MemBuffer* const mem = &idec->mem;
  const int need_compressed_alpha = NeedCompressedAlpha(idec);
  const uint8_t* const old_start =
      (mem->buf == NULL) ? NULL : mem->buf + mem->start;
  const uint8_t* const old_base =
      need_compressed_alpha ? dec->alpha_data : old_start;
  assert(mem->buf != NULL || mem->start == 0);
  assert(mem->mode == MEM_MODE_APPEND);
  if (data_size > MAX_CHUNK_PAYLOAD) {

    return 0;
  }

  if (mem->end + data_size > mem->buf_size) {
    const size_t new_mem_start = old_start - old_base;
    const size_t current_size = MemDataSize(mem) + new_mem_start;
    const uint64_t new_size = (uint64_t)current_size + data_size;
    const uint64_t extra_size = (new_size + CHUNK_SIZE - 1) & ~(CHUNK_SIZE - 1);
    uint8_t* const new_buf =
        (uint8_t*)WebPSafeMalloc(extra_size, sizeof(*new_buf));
    if (new_buf == NULL) return 0;
    if (old_base != NULL) memcpy(new_buf, old_base, current_size);
    WebPSafeFree(mem->buf);
    mem->buf = new_buf;
    mem->buf_size = (size_t)extra_size;
    mem->start = new_mem_start;
    mem->end = current_size;
  }

  assert(mem->buf != NULL);
  memcpy(mem->buf + mem->end, data, data_size);
  mem->end += data_size;
  assert(mem->end <= mem->buf_size);

  DoRemap(idec, mem->buf + mem->start - old_start);
  return 1;
}

WEBP_NODISCARD static int RemapMemBuffer(WebPIDecoder* const idec,
                                         const uint8_t* const data,
                                         size_t data_size) {
  MemBuffer* const mem = &idec->mem;
  const uint8_t* const old_buf = mem->buf;
  const uint8_t* const old_start =
      (old_buf == NULL) ? NULL : old_buf + mem->start;
  assert(old_buf != NULL || mem->start == 0);
  assert(mem->mode == MEM_MODE_MAP);

  if (data_size < mem->buf_size) return 0;

  mem->buf = (uint8_t*)data;
  mem->end = mem->buf_size = data_size;

  DoRemap(idec, mem->buf + mem->start - old_start);
  return 1;
}

static void InitMemBuffer(MemBuffer* const mem) {
  mem->mode       = MEM_MODE_NONE;
  mem->buf        = NULL;
  mem->buf_size   = 0;
  mem->part0_buf  = NULL;
  mem->part0_size = 0;
}

static void ClearMemBuffer(MemBuffer* const mem) {
  assert(mem);
  if (mem->mode == MEM_MODE_APPEND) {
    WebPSafeFree(mem->buf);
    WebPSafeFree((void*)mem->part0_buf);
  }
}

WEBP_NODISCARD static int CheckMemBufferMode(MemBuffer* const mem,
                                             MemBufferMode expected) {
  if (mem->mode == MEM_MODE_NONE) {
    mem->mode = expected;
  } else if (mem->mode != expected) {
    return 0;
  }
  assert(mem->mode == expected);
  return 1;
}

WEBP_NODISCARD static VP8StatusCode FinishDecoding(WebPIDecoder* const idec) {
  const WebPDecoderOptions* const options = idec->params.options;
  WebPDecBuffer* const output = idec->params.output;

  idec->state = STATE_DONE;
  if (options != NULL && options->flip) {
    const VP8StatusCode status = WebPFlipBuffer(output);
    if (status != VP8_STATUS_OK) return status;
  }
  if (idec->final_output != NULL) {
    const VP8StatusCode status = WebPCopyDecBufferPixels(
        output, idec->final_output);
    WebPFreeDecBuffer(&idec->output);
    if (status != VP8_STATUS_OK) return status;
    *output = *idec->final_output;
    idec->final_output = NULL;
  }
  return VP8_STATUS_OK;
}

static void SaveContext(const VP8Decoder* dec, const VP8BitReader* token_br,
                        MBContext* const context) {
  context->left = dec->mb_info[-1];
  context->info = dec->mb_info[dec->mb_x];
  context->token_br = *token_br;
}

static void RestoreContext(const MBContext* context, VP8Decoder* const dec,
                           VP8BitReader* const token_br) {
  dec->mb_info[-1] = context->left;
  dec->mb_info[dec->mb_x] = context->info;
  *token_br = context->token_br;
}

static VP8StatusCode IDecError(WebPIDecoder* const idec, VP8StatusCode error) {
  if (idec->state == STATE_VP8_DATA) {

    (void)VP8ExitCritical((VP8Decoder*)idec->dec, &idec->io);
  }
  idec->state = STATE_ERROR;
  return error;
}

static void ChangeState(WebPIDecoder* const idec, DecState new_state,
                        size_t consumed_bytes) {
  MemBuffer* const mem = &idec->mem;
  idec->state = new_state;
  mem->start += consumed_bytes;
  assert(mem->start <= mem->end);
  idec->io.data = mem->buf + mem->start;
  idec->io.data_size = MemDataSize(mem);
}

static VP8StatusCode DecodeWebPHeaders(WebPIDecoder* const idec) {
  MemBuffer* const mem = &idec->mem;
  const uint8_t* data = mem->buf + mem->start;
  size_t curr_size = MemDataSize(mem);
  VP8StatusCode status;
  WebPHeaderStructure headers;

  headers.data = data;
  headers.data_size = curr_size;
  headers.have_all_data = 0;
  status = WebPParseHeaders(&headers);
  if (status == VP8_STATUS_NOT_ENOUGH_DATA) {
    return VP8_STATUS_SUSPENDED;
  } else if (status != VP8_STATUS_OK) {
    return IDecError(idec, status);
  }

  idec->chunk_size = headers.compressed_size;
  idec->is_lossless = headers.is_lossless;
  if (!idec->is_lossless) {
    VP8Decoder* const dec = VP8New();
    if (dec == NULL) {
      return VP8_STATUS_OUT_OF_MEMORY;
    }
    dec->incremental = 1;
    idec->dec = dec;
    dec->alpha_data = headers.alpha_data;
    dec->alpha_data_size = headers.alpha_data_size;
    ChangeState(idec, STATE_VP8_HEADER, headers.offset);
  } else {
    VP8LDecoder* const dec = VP8LNew();
    if (dec == NULL) {
      return VP8_STATUS_OUT_OF_MEMORY;
    }
    idec->dec = dec;
    ChangeState(idec, STATE_VP8L_HEADER, headers.offset);
  }
  return VP8_STATUS_OK;
}

static VP8StatusCode DecodeVP8FrameHeader(WebPIDecoder* const idec) {
  const uint8_t* data = idec->mem.buf + idec->mem.start;
  const size_t curr_size = MemDataSize(&idec->mem);
  int width, height;
  uint32_t bits;

  if (curr_size < VP8_FRAME_HEADER_SIZE) {

    return VP8_STATUS_SUSPENDED;
  }
  if (!VP8GetInfo(data, curr_size, idec->chunk_size, &width, &height)) {
    return IDecError(idec, VP8_STATUS_BITSTREAM_ERROR);
  }

  bits = data[0] | (data[1] << 8) | (data[2] << 16);
  idec->mem.part0_size = (bits >> 5) + VP8_FRAME_HEADER_SIZE;

  idec->io.data = data;
  idec->io.data_size = curr_size;
  idec->state = STATE_VP8_PARTS0;
  return VP8_STATUS_OK;
}

static VP8StatusCode CopyParts0Data(WebPIDecoder* const idec) {
  VP8Decoder* const dec = (VP8Decoder*)idec->dec;
  VP8BitReader* const br = &dec->br;
  const size_t part_size = br->buf_end - br->buf;
  MemBuffer* const mem = &idec->mem;
  assert(!idec->is_lossless);
  assert(mem->part0_buf == NULL);

  assert(part_size <= mem->part0_size);
  if (part_size == 0) {
    return VP8_STATUS_BITSTREAM_ERROR;
  }
  if (mem->mode == MEM_MODE_APPEND) {

    uint8_t* const part0_buf = (uint8_t*)WebPSafeMalloc(1ULL, part_size);
    if (part0_buf == NULL) {
      return VP8_STATUS_OUT_OF_MEMORY;
    }
    memcpy(part0_buf, br->buf, part_size);
    mem->part0_buf = part0_buf;
    VP8BitReaderSetBuffer(br, part0_buf, part_size);
  } else {

  }
  mem->start += part_size;
  return VP8_STATUS_OK;
}

static VP8StatusCode DecodePartition0(WebPIDecoder* const idec) {
  VP8Decoder* const dec = (VP8Decoder*)idec->dec;
  VP8Io* const io = &idec->io;
  const WebPDecParams* const params = &idec->params;
  WebPDecBuffer* const output = params->output;

  if (MemDataSize(&idec->mem) < idec->mem.part0_size) {
    return VP8_STATUS_SUSPENDED;
  }

  if (!VP8GetHeaders(dec, io)) {
    const VP8StatusCode status = dec->status;
    if (status == VP8_STATUS_SUSPENDED ||
        status == VP8_STATUS_NOT_ENOUGH_DATA) {

      return VP8_STATUS_SUSPENDED;
    }
    return IDecError(idec, status);
  }

  dec->status = WebPAllocateDecBuffer(io->width, io->height, params->options,
                                      output);
  if (dec->status != VP8_STATUS_OK) {
    return IDecError(idec, dec->status);
  }

  dec->mt_method = VP8GetThreadMethod(params->options, NULL,
                                      io->width, io->height);
  VP8InitDithering(params->options, dec);

  dec->status = CopyParts0Data(idec);
  if (dec->status != VP8_STATUS_OK) {
    return IDecError(idec, dec->status);
  }

  if (VP8EnterCritical(dec, io) != VP8_STATUS_OK) {
    return IDecError(idec, dec->status);
  }

  idec->state = STATE_VP8_DATA;

  if (!VP8InitFrame(dec, io)) {
    return IDecError(idec, dec->status);
  }
  return VP8_STATUS_OK;
}

static VP8StatusCode DecodeRemaining(WebPIDecoder* const idec) {
  VP8Decoder* const dec = (VP8Decoder*)idec->dec;
  VP8Io* const io = &idec->io;

  if (!dec->ready) {
    return IDecError(idec, VP8_STATUS_BITSTREAM_ERROR);
  }
  for (; dec->mb_y < dec->mb_h; ++dec->mb_y) {
    if (idec->last_mb_y != dec->mb_y) {
      if (!VP8ParseIntraModeRow(&dec->br, dec)) {

        return IDecError(idec, VP8_STATUS_BITSTREAM_ERROR);
      }
      idec->last_mb_y = dec->mb_y;
    }
    for (; dec->mb_x < dec->mb_w; ++dec->mb_x) {
      VP8BitReader* const token_br =
          &dec->parts[dec->mb_y & dec->num_parts_minus_one];
      MBContext context;
      SaveContext(dec, token_br, &context);
      if (!VP8DecodeMB(dec, token_br)) {

        if (dec->num_parts_minus_one == 0 &&
            MemDataSize(&idec->mem) > MAX_MB_SIZE) {
          return IDecError(idec, VP8_STATUS_BITSTREAM_ERROR);
        }

        if (dec->mt_method > 0) {
          if (!WebPGetWorkerInterface()->Sync(&dec->worker)) {
            return IDecError(idec, VP8_STATUS_BITSTREAM_ERROR);
          }
        }
        RestoreContext(&context, dec, token_br);
        return VP8_STATUS_SUSPENDED;
      }

      if (dec->num_parts_minus_one == 0) {
        idec->mem.start = token_br->buf - idec->mem.buf;
        assert(idec->mem.start <= idec->mem.end);
      }
    }
    VP8InitScanline(dec);

    if (!VP8ProcessRow(dec, io)) {
      return IDecError(idec, VP8_STATUS_USER_ABORT);
    }
  }

  if (!VP8ExitCritical(dec, io)) {
    idec->state = STATE_ERROR;
    return IDecError(idec, VP8_STATUS_USER_ABORT);
  }
  dec->ready = 0;
  return FinishDecoding(idec);
}

static VP8StatusCode ErrorStatusLossless(WebPIDecoder* const idec,
                                         VP8StatusCode status) {
  if (status == VP8_STATUS_SUSPENDED || status == VP8_STATUS_NOT_ENOUGH_DATA) {
    return VP8_STATUS_SUSPENDED;
  }
  return IDecError(idec, status);
}

static VP8StatusCode DecodeVP8LHeader(WebPIDecoder* const idec) {
  VP8Io* const io = &idec->io;
  VP8LDecoder* const dec = (VP8LDecoder*)idec->dec;
  const WebPDecParams* const params = &idec->params;
  WebPDecBuffer* const output = params->output;
  size_t curr_size = MemDataSize(&idec->mem);
  assert(idec->is_lossless);

  if (curr_size < (idec->chunk_size >> 3)) {
    dec->status = VP8_STATUS_SUSPENDED;
    return ErrorStatusLossless(idec, dec->status);
  }

  if (!VP8LDecodeHeader(dec, io)) {
    if (dec->status == VP8_STATUS_BITSTREAM_ERROR &&
        curr_size < idec->chunk_size) {
      dec->status = VP8_STATUS_SUSPENDED;
    }
    return ErrorStatusLossless(idec, dec->status);
  }

  dec->status = WebPAllocateDecBuffer(io->width, io->height, params->options,
                                      output);
  if (dec->status != VP8_STATUS_OK) {
    return IDecError(idec, dec->status);
  }

  idec->state = STATE_VP8L_DATA;
  return VP8_STATUS_OK;
}

static VP8StatusCode DecodeVP8LData(WebPIDecoder* const idec) {
  VP8LDecoder* const dec = (VP8LDecoder*)idec->dec;
  const size_t curr_size = MemDataSize(&idec->mem);
  assert(idec->is_lossless);

  dec->incremental = (curr_size < idec->chunk_size);

  if (!VP8LDecodeImage(dec)) {
    return ErrorStatusLossless(idec, dec->status);
  }
  assert(dec->status == VP8_STATUS_OK || dec->status == VP8_STATUS_SUSPENDED);
  return (dec->status == VP8_STATUS_SUSPENDED) ? dec->status
                                               : FinishDecoding(idec);
}

static VP8StatusCode IDecode(WebPIDecoder* idec) {
  VP8StatusCode status = VP8_STATUS_SUSPENDED;

  if (idec->state == STATE_WEBP_HEADER) {
    status = DecodeWebPHeaders(idec);
  } else {
    if (idec->dec == NULL) {
      return VP8_STATUS_SUSPENDED;
    }
  }
  if (idec->state == STATE_VP8_HEADER) {
    status = DecodeVP8FrameHeader(idec);
  }
  if (idec->state == STATE_VP8_PARTS0) {
    status = DecodePartition0(idec);
  }
  if (idec->state == STATE_VP8_DATA) {
    const VP8Decoder* const dec = (VP8Decoder*)idec->dec;
    if (dec == NULL) {
      return VP8_STATUS_SUSPENDED;
    }
    status = DecodeRemaining(idec);
  }
  if (idec->state == STATE_VP8L_HEADER) {
    status = DecodeVP8LHeader(idec);
  }
  if (idec->state == STATE_VP8L_DATA) {
    status = DecodeVP8LData(idec);
  }
  return status;
}

WEBP_NODISCARD static WebPIDecoder* NewDecoder(
    WebPDecBuffer* const output_buffer,
    const WebPBitstreamFeatures* const features) {
  WebPIDecoder* idec = (WebPIDecoder*)WebPSafeCalloc(1ULL, sizeof(*idec));
  if (idec == NULL) {
    return NULL;
  }

  idec->state = STATE_WEBP_HEADER;
  idec->chunk_size = 0;

  idec->last_mb_y = -1;

  InitMemBuffer(&idec->mem);
  if (!WebPInitDecBuffer(&idec->output) || !VP8InitIo(&idec->io)) {
    WebPSafeFree(idec);
    return NULL;
  }

  WebPResetDecParams(&idec->params);
  if (output_buffer == NULL || WebPAvoidSlowMemory(output_buffer, features)) {
    idec->params.output = &idec->output;
    idec->final_output = output_buffer;
    if (output_buffer != NULL) {
      idec->params.output->colorspace = output_buffer->colorspace;
    }
  } else {
    idec->params.output = output_buffer;
    idec->final_output = NULL;
  }
  WebPInitCustomIo(&idec->params, &idec->io);

  return idec;
}

WebPIDecoder* WebPINewDecoder(WebPDecBuffer* output_buffer) {
  return NewDecoder(output_buffer, NULL);
}

WebPIDecoder* WebPIDecode(const uint8_t* data, size_t data_size,
                          WebPDecoderConfig* config) {
  WebPIDecoder* idec;
  WebPBitstreamFeatures tmp_features;
  WebPBitstreamFeatures* const features =
      (config == NULL) ? &tmp_features : &config->input;
  memset(&tmp_features, 0, sizeof(tmp_features));

  if (data != NULL && data_size > 0) {
    if (WebPGetFeatures(data, data_size, features) != VP8_STATUS_OK) {
      return NULL;
    }
  }

  idec = (config != NULL) ? NewDecoder(&config->output, features)
                          : NewDecoder(NULL, features);
  if (idec == NULL) {
    return NULL;
  }

  if (config != NULL) {
    idec->params.options = &config->options;
  }
  return idec;
}

void WebPIDelete(WebPIDecoder* idec) {
  if (idec == NULL) return;
  if (idec->dec != NULL) {
    if (!idec->is_lossless) {
      if (idec->state == STATE_VP8_DATA) {

        (void)VP8ExitCritical((VP8Decoder*)idec->dec, &idec->io);
      }
      VP8Delete((VP8Decoder*)idec->dec);
    } else {
      VP8LDelete((VP8LDecoder*)idec->dec);
    }
  }
  ClearMemBuffer(&idec->mem);
  WebPFreeDecBuffer(&idec->output);
  WebPSafeFree(idec);
}

WebPIDecoder* WebPINewRGB(WEBP_CSP_MODE csp, uint8_t* output_buffer,
                          size_t output_buffer_size, int output_stride) {
  const int is_external_memory = (output_buffer != NULL) ? 1 : 0;
  WebPIDecoder* idec;

  if (csp >= MODE_YUV) return NULL;
  if (is_external_memory == 0) {
    output_buffer_size = 0;
    output_stride = 0;
  } else {
    if (output_stride == 0 || output_buffer_size == 0) {
      return NULL;
    }
  }
  idec = WebPINewDecoder(NULL);
  if (idec == NULL) return NULL;
  idec->output.colorspace = csp;
  idec->output.is_external_memory = is_external_memory;
  idec->output.u.RGBA.rgba = output_buffer;
  idec->output.u.RGBA.stride = output_stride;
  idec->output.u.RGBA.size = output_buffer_size;
  return idec;
}

WebPIDecoder* WebPINewYUVA(uint8_t* luma, size_t luma_size, int luma_stride,
                           uint8_t* u, size_t u_size, int u_stride,
                           uint8_t* v, size_t v_size, int v_stride,
                           uint8_t* a, size_t a_size, int a_stride) {
  const int is_external_memory = (luma != NULL) ? 1 : 0;
  WebPIDecoder* idec;
  WEBP_CSP_MODE colorspace;

  if (is_external_memory == 0) {
    luma_size = u_size = v_size = a_size = 0;
    luma_stride = u_stride = v_stride = a_stride = 0;
    u = v = a = NULL;
    colorspace = MODE_YUVA;
  } else {
    if (u == NULL || v == NULL) return NULL;
    if (luma_size == 0 || u_size == 0 || v_size == 0) return NULL;
    if (luma_stride == 0 || u_stride == 0 || v_stride == 0) return NULL;
    if (a != NULL) {
      if (a_size == 0 || a_stride == 0) return NULL;
    }
    colorspace = (a == NULL) ? MODE_YUV : MODE_YUVA;
  }

  idec = WebPINewDecoder(NULL);
  if (idec == NULL) return NULL;

  idec->output.colorspace = colorspace;
  idec->output.is_external_memory = is_external_memory;
  idec->output.u.YUVA.y = luma;
  idec->output.u.YUVA.y_stride = luma_stride;
  idec->output.u.YUVA.y_size = luma_size;
  idec->output.u.YUVA.u = u;
  idec->output.u.YUVA.u_stride = u_stride;
  idec->output.u.YUVA.u_size = u_size;
  idec->output.u.YUVA.v = v;
  idec->output.u.YUVA.v_stride = v_stride;
  idec->output.u.YUVA.v_size = v_size;
  idec->output.u.YUVA.a = a;
  idec->output.u.YUVA.a_stride = a_stride;
  idec->output.u.YUVA.a_size = a_size;
  return idec;
}

WebPIDecoder* WebPINewYUV(uint8_t* luma, size_t luma_size, int luma_stride,
                          uint8_t* u, size_t u_size, int u_stride,
                          uint8_t* v, size_t v_size, int v_stride) {
  return WebPINewYUVA(luma, luma_size, luma_stride,
                      u, u_size, u_stride,
                      v, v_size, v_stride,
                      NULL, 0, 0);
}

static VP8StatusCode IDecCheckStatus(const WebPIDecoder* const idec) {
  assert(idec);
  if (idec->state == STATE_ERROR) {
    return VP8_STATUS_BITSTREAM_ERROR;
  }
  if (idec->state == STATE_DONE) {
    return VP8_STATUS_OK;
  }
  return VP8_STATUS_SUSPENDED;
}

VP8StatusCode WebPIAppend(WebPIDecoder* idec,
                          const uint8_t* data, size_t data_size) {
  VP8StatusCode status;
  if (idec == NULL || data == NULL) {
    return VP8_STATUS_INVALID_PARAM;
  }
  status = IDecCheckStatus(idec);
  if (status != VP8_STATUS_SUSPENDED) {
    return status;
  }

  if (!CheckMemBufferMode(&idec->mem, MEM_MODE_APPEND)) {
    return VP8_STATUS_INVALID_PARAM;
  }

  if (!AppendToMemBuffer(idec, data, data_size)) {
    return VP8_STATUS_OUT_OF_MEMORY;
  }
  return IDecode(idec);
}

VP8StatusCode WebPIUpdate(WebPIDecoder* idec,
                          const uint8_t* data, size_t data_size) {
  VP8StatusCode status;
  if (idec == NULL || data == NULL) {
    return VP8_STATUS_INVALID_PARAM;
  }
  status = IDecCheckStatus(idec);
  if (status != VP8_STATUS_SUSPENDED) {
    return status;
  }

  if (!CheckMemBufferMode(&idec->mem, MEM_MODE_MAP)) {
    return VP8_STATUS_INVALID_PARAM;
  }

  if (!RemapMemBuffer(idec, data, data_size)) {
    return VP8_STATUS_INVALID_PARAM;
  }
  return IDecode(idec);
}

static const WebPDecBuffer* GetOutputBuffer(const WebPIDecoder* const idec) {
  if (idec == NULL || idec->dec == NULL) {
    return NULL;
  }
  if (idec->state <= STATE_VP8_PARTS0) {
    return NULL;
  }
  if (idec->final_output != NULL) {
    return NULL;
  }
  return idec->params.output;
}

const WebPDecBuffer* WebPIDecodedArea(const WebPIDecoder* idec,
                                      int* left, int* top,
                                      int* width, int* height) {
  const WebPDecBuffer* const src = GetOutputBuffer(idec);
  if (left != NULL) *left = 0;
  if (top != NULL) *top = 0;
  if (src != NULL) {
    if (width != NULL) *width = src->width;
    if (height != NULL) *height = idec->params.last_y;
  } else {
    if (width != NULL) *width = 0;
    if (height != NULL) *height = 0;
  }
  return src;
}

WEBP_NODISCARD uint8_t* WebPIDecGetRGB(const WebPIDecoder* idec, int* last_y,
                                       int* width, int* height, int* stride) {
  const WebPDecBuffer* const src = GetOutputBuffer(idec);
  if (src == NULL) return NULL;
  if (src->colorspace >= MODE_YUV) {
    return NULL;
  }

  if (last_y != NULL) *last_y = idec->params.last_y;
  if (width != NULL) *width = src->width;
  if (height != NULL) *height = src->height;
  if (stride != NULL) *stride = src->u.RGBA.stride;

  return src->u.RGBA.rgba;
}

WEBP_NODISCARD uint8_t* WebPIDecGetYUVA(const WebPIDecoder* idec, int* last_y,
                                        uint8_t** u, uint8_t** v, uint8_t** a,
                                        int* width, int* height, int* stride,
                                        int* uv_stride, int* a_stride) {
  const WebPDecBuffer* const src = GetOutputBuffer(idec);
  if (src == NULL) return NULL;
  if (src->colorspace < MODE_YUV) {
    return NULL;
  }

  if (last_y != NULL) *last_y = idec->params.last_y;
  if (u != NULL) *u = src->u.YUVA.u;
  if (v != NULL) *v = src->u.YUVA.v;
  if (a != NULL) *a = src->u.YUVA.a;
  if (width != NULL) *width = src->width;
  if (height != NULL) *height = src->height;
  if (stride != NULL) *stride = src->u.YUVA.y_stride;
  if (uv_stride != NULL) *uv_stride = src->u.YUVA.u_stride;
  if (a_stride != NULL) *a_stride = src->u.YUVA.a_stride;

  return src->u.YUVA.y;
}

int WebPISetIOHooks(WebPIDecoder* const idec,
                    VP8IoPutHook put,
                    VP8IoSetupHook setup,
                    VP8IoTeardownHook teardown,
                    void* user_data) {
  if (idec == NULL || idec->state > STATE_WEBP_HEADER) {
    return 0;
  }

  idec->io.put = put;
  idec->io.setup = setup;
  idec->io.teardown = teardown;
  idec->io.opaque = user_data;

  return 1;
}

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#ifndef WEBP_DSP_YUV_H_
#define WEBP_DSP_YUV_H_

#ifdef __cplusplus
extern "C" {
#endif

enum {
  YUV_FIX = 16,
  YUV_HALF = 1 << (YUV_FIX - 1),

  YUV_FIX2 = 6,
  YUV_MASK2 = (256 << YUV_FIX2) - 1
};

static WEBP_INLINE int MultHi(int v, int coeff) {
  return (v * coeff) >> 8;
}

static WEBP_INLINE int VP8Clip8(int v) {
  return ((v & ~YUV_MASK2) == 0) ? (v >> YUV_FIX2) : (v < 0) ? 0 : 255;
}

static WEBP_INLINE int VP8YUVToR(int y, int v) {
  return VP8Clip8(MultHi(y, 19077) + MultHi(v, 26149) - 14234);
}

static WEBP_INLINE int VP8YUVToG(int y, int u, int v) {
  return VP8Clip8(MultHi(y, 19077) - MultHi(u, 6419) - MultHi(v, 13320) + 8708);
}

static WEBP_INLINE int VP8YUVToB(int y, int u) {
  return VP8Clip8(MultHi(y, 19077) + MultHi(u, 33050) - 17685);
}

static WEBP_INLINE void VP8YuvToRgb(int y, int u, int v,
                                    uint8_t* const rgb) {
  rgb[0] = VP8YUVToR(y, v);
  rgb[1] = VP8YUVToG(y, u, v);
  rgb[2] = VP8YUVToB(y, u);
}

static WEBP_INLINE void VP8YuvToBgr(int y, int u, int v,
                                    uint8_t* const bgr) {
  bgr[0] = VP8YUVToB(y, u);
  bgr[1] = VP8YUVToG(y, u, v);
  bgr[2] = VP8YUVToR(y, v);
}

static WEBP_INLINE void VP8YuvToRgb565(int y, int u, int v,
                                       uint8_t* const rgb) {
  const int r = VP8YUVToR(y, v);
  const int g = VP8YUVToG(y, u, v);
  const int b = VP8YUVToB(y, u);
  const int rg = (r & 0xf8) | (g >> 5);
  const int gb = ((g << 3) & 0xe0) | (b >> 3);
#if (WEBP_SWAP_16BIT_CSP == 1)
  rgb[0] = gb;
  rgb[1] = rg;
#else
  rgb[0] = rg;
  rgb[1] = gb;
#endif
}

static WEBP_INLINE void VP8YuvToRgba4444(int y, int u, int v,
                                         uint8_t* const argb) {
  const int r = VP8YUVToR(y, v);
  const int g = VP8YUVToG(y, u, v);
  const int b = VP8YUVToB(y, u);
  const int rg = (r & 0xf0) | (g >> 4);
  const int ba = (b & 0xf0) | 0x0f;
#if (WEBP_SWAP_16BIT_CSP == 1)
  argb[0] = ba;
  argb[1] = rg;
#else
  argb[0] = rg;
  argb[1] = ba;
#endif
}

static WEBP_INLINE void VP8YuvToArgb(uint8_t y, uint8_t u, uint8_t v,
                                     uint8_t* const argb) {
  argb[0] = 0xff;
  VP8YuvToRgb(y, u, v, argb + 1);
}

static WEBP_INLINE void VP8YuvToBgra(uint8_t y, uint8_t u, uint8_t v,
                                     uint8_t* const bgra) {
  VP8YuvToBgr(y, u, v, bgra);
  bgra[3] = 0xff;
}

static WEBP_INLINE void VP8YuvToRgba(uint8_t y, uint8_t u, uint8_t v,
                                     uint8_t* const rgba) {
  VP8YuvToRgb(y, u, v, rgba);
  rgba[3] = 0xff;
}

#if defined(WEBP_USE_SSE2)

void VP8YuvToRgba32_SSE2(const uint8_t* WEBP_RESTRICT y,
                         const uint8_t* WEBP_RESTRICT u,
                         const uint8_t* WEBP_RESTRICT v,
                         uint8_t* WEBP_RESTRICT dst);
void VP8YuvToRgb32_SSE2(const uint8_t* WEBP_RESTRICT y,
                        const uint8_t* WEBP_RESTRICT u,
                        const uint8_t* WEBP_RESTRICT v,
                        uint8_t* WEBP_RESTRICT dst);
void VP8YuvToBgra32_SSE2(const uint8_t* WEBP_RESTRICT y,
                         const uint8_t* WEBP_RESTRICT u,
                         const uint8_t* WEBP_RESTRICT v,
                         uint8_t* WEBP_RESTRICT dst);
void VP8YuvToBgr32_SSE2(const uint8_t* WEBP_RESTRICT y,
                        const uint8_t* WEBP_RESTRICT u,
                        const uint8_t* WEBP_RESTRICT v,
                        uint8_t* WEBP_RESTRICT dst);
void VP8YuvToArgb32_SSE2(const uint8_t* WEBP_RESTRICT y,
                         const uint8_t* WEBP_RESTRICT u,
                         const uint8_t* WEBP_RESTRICT v,
                         uint8_t* WEBP_RESTRICT dst);
void VP8YuvToRgba444432_SSE2(const uint8_t* WEBP_RESTRICT y,
                             const uint8_t* WEBP_RESTRICT u,
                             const uint8_t* WEBP_RESTRICT v,
                             uint8_t* WEBP_RESTRICT dst);
void VP8YuvToRgb56532_SSE2(const uint8_t* WEBP_RESTRICT y,
                           const uint8_t* WEBP_RESTRICT u,
                           const uint8_t* WEBP_RESTRICT v,
                           uint8_t* WEBP_RESTRICT dst);

#endif

#if defined(WEBP_USE_SSE41)

void VP8YuvToRgb32_SSE41(const uint8_t* WEBP_RESTRICT y,
                         const uint8_t* WEBP_RESTRICT u,
                         const uint8_t* WEBP_RESTRICT v,
                         uint8_t* WEBP_RESTRICT dst);
void VP8YuvToBgr32_SSE41(const uint8_t* WEBP_RESTRICT y,
                         const uint8_t* WEBP_RESTRICT u,
                         const uint8_t* WEBP_RESTRICT v,
                         uint8_t* WEBP_RESTRICT dst);

#endif

static WEBP_INLINE int VP8ClipUV(int uv, int rounding) {
  uv = (uv + rounding + (128 << (YUV_FIX + 2))) >> (YUV_FIX + 2);
  return ((uv & ~0xff) == 0) ? uv : (uv < 0) ? 0 : 255;
}

static WEBP_INLINE int VP8RGBToY(int r, int g, int b, int rounding) {
  const int luma = 16839 * r + 33059 * g + 6420 * b;
  return (luma + rounding + (16 << YUV_FIX)) >> YUV_FIX;
}

static WEBP_INLINE int VP8RGBToU(int r, int g, int b, int rounding) {
  const int u = -9719 * r - 19081 * g + 28800 * b;
  return VP8ClipUV(u, rounding);
}

static WEBP_INLINE int VP8RGBToV(int r, int g, int b, int rounding) {
  const int v = +28800 * r - 24116 * g - 4684 * b;
  return VP8ClipUV(v, rounding);
}

#ifdef __cplusplus
}
#endif

#endif

static int EmitYUV(const VP8Io* const io, WebPDecParams* const p) {
  WebPDecBuffer* output = p->output;
  const WebPYUVABuffer* const buf = &output->u.YUVA;
  uint8_t* const y_dst = buf->y + (ptrdiff_t)io->mb_y * buf->y_stride;
  uint8_t* const u_dst = buf->u + (ptrdiff_t)(io->mb_y >> 1) * buf->u_stride;
  uint8_t* const v_dst = buf->v + (ptrdiff_t)(io->mb_y >> 1) * buf->v_stride;
  const int mb_w = io->mb_w;
  const int mb_h = io->mb_h;
  const int uv_w = (mb_w + 1) / 2;
  const int uv_h = (mb_h + 1) / 2;
  WebPCopyPlane(io->y, io->y_stride, y_dst, buf->y_stride, mb_w, mb_h);
  WebPCopyPlane(io->u, io->uv_stride, u_dst, buf->u_stride, uv_w, uv_h);
  WebPCopyPlane(io->v, io->uv_stride, v_dst, buf->v_stride, uv_w, uv_h);
  return io->mb_h;
}

static int EmitSampledRGB(const VP8Io* const io, WebPDecParams* const p) {
  WebPDecBuffer* const output = p->output;
  WebPRGBABuffer* const buf = &output->u.RGBA;
  uint8_t* const dst = buf->rgba + (ptrdiff_t)io->mb_y * buf->stride;
  WebPSamplerProcessPlane(io->y, io->y_stride,
                          io->u, io->v, io->uv_stride,
                          dst, buf->stride, io->mb_w, io->mb_h,
                          WebPSamplers[output->colorspace]);
  return io->mb_h;
}

#ifdef FANCY_UPSAMPLING
static int EmitFancyRGB(const VP8Io* const io, WebPDecParams* const p) {
  int num_lines_out = io->mb_h;
  const WebPRGBABuffer* const buf = &p->output->u.RGBA;
  uint8_t* dst = buf->rgba + (ptrdiff_t)io->mb_y * buf->stride;
  WebPUpsampleLinePairFunc upsample = WebPUpsamplers[p->output->colorspace];
  const uint8_t* cur_y = io->y;
  const uint8_t* cur_u = io->u;
  const uint8_t* cur_v = io->v;
  const uint8_t* top_u = p->tmp_u;
  const uint8_t* top_v = p->tmp_v;
  int y = io->mb_y;
  const int y_end = io->mb_y + io->mb_h;
  const int mb_w = io->mb_w;
  const int uv_w = (mb_w + 1) / 2;

  if (y == 0) {

    upsample(cur_y, NULL, cur_u, cur_v, cur_u, cur_v, dst, NULL, mb_w);
  } else {

    upsample(p->tmp_y, cur_y, top_u, top_v, cur_u, cur_v,
             dst - buf->stride, dst, mb_w);
    ++num_lines_out;
  }

  for (; y + 2 < y_end; y += 2) {
    top_u = cur_u;
    top_v = cur_v;
    cur_u += io->uv_stride;
    cur_v += io->uv_stride;
    dst += 2 * buf->stride;
    cur_y += 2 * io->y_stride;
    upsample(cur_y - io->y_stride, cur_y,
             top_u, top_v, cur_u, cur_v,
             dst - buf->stride, dst, mb_w);
  }

  cur_y += io->y_stride;
  if (io->crop_top + y_end < io->crop_bottom) {

    memcpy(p->tmp_y, cur_y, mb_w * sizeof(*p->tmp_y));
    memcpy(p->tmp_u, cur_u, uv_w * sizeof(*p->tmp_u));
    memcpy(p->tmp_v, cur_v, uv_w * sizeof(*p->tmp_v));

    num_lines_out--;
  } else {

    if (!(y_end & 1)) {
      upsample(cur_y, NULL, cur_u, cur_v, cur_u, cur_v,
               dst + buf->stride, NULL, mb_w);
    }
  }
  return num_lines_out;
}

#endif

static void FillAlphaPlane(uint8_t* dst, int w, int h, int stride) {
  int j;
  for (j = 0; j < h; ++j) {
    memset(dst, 0xff, w * sizeof(*dst));
    dst += stride;
  }
}

static int EmitAlphaYUV(const VP8Io* const io, WebPDecParams* const p,
                        int expected_num_lines_out) {
  const uint8_t* alpha = io->a;
  const WebPYUVABuffer* const buf = &p->output->u.YUVA;
  const int mb_w = io->mb_w;
  const int mb_h = io->mb_h;
  uint8_t* dst = buf->a + (ptrdiff_t)io->mb_y * buf->a_stride;
  int j;
  (void)expected_num_lines_out;
  assert(expected_num_lines_out == mb_h);
  if (alpha != NULL) {
    for (j = 0; j < mb_h; ++j) {
      memcpy(dst, alpha, mb_w * sizeof(*dst));
      alpha += io->width;
      dst += buf->a_stride;
    }
  } else if (buf->a != NULL) {

    FillAlphaPlane(dst, mb_w, mb_h, buf->a_stride);
  }
  return 0;
}

static int GetAlphaSourceRow(const VP8Io* const io,
                             const uint8_t** alpha, int* const num_rows) {
  int start_y = io->mb_y;
  *num_rows = io->mb_h;

  if (io->fancy_upsampling) {
    if (start_y == 0) {

      --*num_rows;
    } else {
      --start_y;

      *alpha -= io->width;
    }
    if (io->crop_top + io->mb_y + io->mb_h == io->crop_bottom) {

      *num_rows = io->crop_bottom - io->crop_top - start_y;
    }
  }
  return start_y;
}

static int EmitAlphaRGB(const VP8Io* const io, WebPDecParams* const p,
                        int expected_num_lines_out) {
  const uint8_t* alpha = io->a;
  if (alpha != NULL) {
    const int mb_w = io->mb_w;
    const WEBP_CSP_MODE colorspace = p->output->colorspace;
    const int alpha_first =
        (colorspace == MODE_ARGB || colorspace == MODE_Argb);
    const WebPRGBABuffer* const buf = &p->output->u.RGBA;
    int num_rows;
    const int start_y = GetAlphaSourceRow(io, &alpha, &num_rows);
    uint8_t* const base_rgba = buf->rgba + (ptrdiff_t)start_y * buf->stride;
    uint8_t* const dst = base_rgba + (alpha_first ? 0 : 3);
    const int has_alpha = WebPDispatchAlpha(alpha, io->width, mb_w,
                                            num_rows, dst, buf->stride);
    (void)expected_num_lines_out;
    assert(expected_num_lines_out == num_rows);

    if (has_alpha && WebPIsPremultipliedMode(colorspace)) {
      WebPApplyAlphaMultiply(base_rgba, alpha_first,
                             mb_w, num_rows, buf->stride);
    }
  }
  return 0;
}

static int EmitAlphaRGBA4444(const VP8Io* const io, WebPDecParams* const p,
                             int expected_num_lines_out) {
  const uint8_t* alpha = io->a;
  if (alpha != NULL) {
    const int mb_w = io->mb_w;
    const WEBP_CSP_MODE colorspace = p->output->colorspace;
    const WebPRGBABuffer* const buf = &p->output->u.RGBA;
    int num_rows;
    const int start_y = GetAlphaSourceRow(io, &alpha, &num_rows);
    uint8_t* const base_rgba = buf->rgba + (ptrdiff_t)start_y * buf->stride;
#if (WEBP_SWAP_16BIT_CSP == 1)
    uint8_t* alpha_dst = base_rgba;
#else
    uint8_t* alpha_dst = base_rgba + 1;
#endif
    uint32_t alpha_mask = 0x0f;
    int i, j;
    for (j = 0; j < num_rows; ++j) {
      for (i = 0; i < mb_w; ++i) {

        const uint32_t alpha_value = alpha[i] >> 4;
        alpha_dst[2 * i] = (alpha_dst[2 * i] & 0xf0) | alpha_value;
        alpha_mask &= alpha_value;
      }
      alpha += io->width;
      alpha_dst += buf->stride;
    }
    (void)expected_num_lines_out;
    assert(expected_num_lines_out == num_rows);
    if (alpha_mask != 0x0f && WebPIsPremultipliedMode(colorspace)) {
      WebPApplyAlphaMultiply4444(base_rgba, mb_w, num_rows, buf->stride);
    }
  }
  return 0;
}

#if !defined(WEBP_REDUCE_SIZE)
static int Rescale(const uint8_t* src, int src_stride,
                   int new_lines, WebPRescaler* const wrk) {
  int num_lines_out = 0;
  while (new_lines > 0) {
    const int lines_in = WebPRescalerImport(wrk, new_lines, src, src_stride);
    src += lines_in * src_stride;
    new_lines -= lines_in;
    num_lines_out += WebPRescalerExport(wrk);
  }
  return num_lines_out;
}

static int EmitRescaledYUV(const VP8Io* const io, WebPDecParams* const p) {
  const int mb_h = io->mb_h;
  const int uv_mb_h = (mb_h + 1) >> 1;
  WebPRescaler* const scaler = p->scaler_y;
  int num_lines_out = 0;
  if (WebPIsAlphaMode(p->output->colorspace) && io->a != NULL) {

    WebPMultRows((uint8_t*)io->y, io->y_stride,
                 io->a, io->width, io->mb_w, mb_h, 0);
  }
  num_lines_out = Rescale(io->y, io->y_stride, mb_h, scaler);
  Rescale(io->u, io->uv_stride, uv_mb_h, p->scaler_u);
  Rescale(io->v, io->uv_stride, uv_mb_h, p->scaler_v);
  return num_lines_out;
}

static int EmitRescaledAlphaYUV(const VP8Io* const io, WebPDecParams* const p,
                                int expected_num_lines_out) {
  const WebPYUVABuffer* const buf = &p->output->u.YUVA;
  uint8_t* const dst_a = buf->a + (ptrdiff_t)p->last_y * buf->a_stride;
  if (io->a != NULL) {
    uint8_t* const dst_y = buf->y + (ptrdiff_t)p->last_y * buf->y_stride;
    const int num_lines_out = Rescale(io->a, io->width, io->mb_h, p->scaler_a);
    assert(expected_num_lines_out == num_lines_out);
    if (num_lines_out > 0) {
      WebPMultRows(dst_y, buf->y_stride, dst_a, buf->a_stride,
                   p->scaler_a->dst_width, num_lines_out, 1);
    }
  } else if (buf->a != NULL) {

    assert(p->last_y + expected_num_lines_out <= io->scaled_height);
    FillAlphaPlane(dst_a, io->scaled_width, expected_num_lines_out,
                   buf->a_stride);
  }
  return 0;
}

static int InitYUVRescaler(const VP8Io* const io, WebPDecParams* const p) {
  const int has_alpha = WebPIsAlphaMode(p->output->colorspace);
  const WebPYUVABuffer* const buf = &p->output->u.YUVA;
  const int out_width  = io->scaled_width;
  const int out_height = io->scaled_height;
  const int uv_out_width  = (out_width + 1) >> 1;
  const int uv_out_height = (out_height + 1) >> 1;
  const int uv_in_width  = (io->mb_w + 1) >> 1;
  const int uv_in_height = (io->mb_h + 1) >> 1;

  const size_t work_size = 2 * (size_t)out_width;
  const size_t uv_work_size = 2 * uv_out_width;
  uint64_t total_size;
  size_t rescaler_size;
  rescaler_t* work;
  WebPRescaler* scalers;
  const int num_rescalers = has_alpha ? 4 : 3;

  total_size = ((uint64_t)work_size + 2 * uv_work_size) * sizeof(*work);
  if (has_alpha) {
    total_size += (uint64_t)work_size * sizeof(*work);
  }
  rescaler_size = num_rescalers * sizeof(*p->scaler_y) + WEBP_ALIGN_CST;
  total_size += rescaler_size;
  if (!CheckSizeOverflow(total_size)) {
    return 0;
  }

  p->memory = WebPSafeMalloc(1ULL, (size_t)total_size);
  if (p->memory == NULL) {
    return 0;
  }
  work = (rescaler_t*)p->memory;

  scalers = (WebPRescaler*)WEBP_ALIGN(
      (const uint8_t*)work + total_size - rescaler_size);
  p->scaler_y = &scalers[0];
  p->scaler_u = &scalers[1];
  p->scaler_v = &scalers[2];
  p->scaler_a = has_alpha ? &scalers[3] : NULL;

  if (!WebPRescalerInit(p->scaler_y, io->mb_w, io->mb_h,
                        buf->y, out_width, out_height, buf->y_stride, 1,
                        work) ||
      !WebPRescalerInit(p->scaler_u, uv_in_width, uv_in_height,
                        buf->u, uv_out_width, uv_out_height, buf->u_stride, 1,
                        work + work_size) ||
      !WebPRescalerInit(p->scaler_v, uv_in_width, uv_in_height,
                        buf->v, uv_out_width, uv_out_height, buf->v_stride, 1,
                        work + work_size + uv_work_size)) {
    return 0;
  }
  p->emit = EmitRescaledYUV;

  if (has_alpha) {
    if (!WebPRescalerInit(p->scaler_a, io->mb_w, io->mb_h,
                          buf->a, out_width, out_height, buf->a_stride, 1,
                          work + work_size + 2 * uv_work_size)) {
      return 0;
    }
    p->emit_alpha = EmitRescaledAlphaYUV;
    WebPInitAlphaProcessing();
  }
  return 1;
}

static int ExportRGB(WebPDecParams* const p, int y_pos) {
  const WebPYUV444Converter convert =
      WebPYUV444Converters[p->output->colorspace];
  const WebPRGBABuffer* const buf = &p->output->u.RGBA;
  uint8_t* dst = buf->rgba + (ptrdiff_t)y_pos * buf->stride;
  int num_lines_out = 0;

  while (WebPRescalerHasPendingOutput(p->scaler_y) &&
         WebPRescalerHasPendingOutput(p->scaler_u)) {
    assert(y_pos + num_lines_out < p->output->height);
    assert(p->scaler_u->y_accum == p->scaler_v->y_accum);
    WebPRescalerExportRow(p->scaler_y);
    WebPRescalerExportRow(p->scaler_u);
    WebPRescalerExportRow(p->scaler_v);
    convert(p->scaler_y->dst, p->scaler_u->dst, p->scaler_v->dst,
            dst, p->scaler_y->dst_width);
    dst += buf->stride;
    ++num_lines_out;
  }
  return num_lines_out;
}

static int EmitRescaledRGB(const VP8Io* const io, WebPDecParams* const p) {
  const int mb_h = io->mb_h;
  const int uv_mb_h = (mb_h + 1) >> 1;
  int j = 0, uv_j = 0;
  int num_lines_out = 0;
  while (j < mb_h) {
    const int y_lines_in =
        WebPRescalerImport(p->scaler_y, mb_h - j,
                           io->y + (ptrdiff_t)j * io->y_stride, io->y_stride);
    j += y_lines_in;
    if (WebPRescaleNeededLines(p->scaler_u, uv_mb_h - uv_j)) {
      const int u_lines_in = WebPRescalerImport(
          p->scaler_u, uv_mb_h - uv_j, io->u + (ptrdiff_t)uv_j * io->uv_stride,
          io->uv_stride);
      const int v_lines_in = WebPRescalerImport(
          p->scaler_v, uv_mb_h - uv_j, io->v + (ptrdiff_t)uv_j * io->uv_stride,
          io->uv_stride);
      (void)v_lines_in;
      assert(u_lines_in == v_lines_in);
      uv_j += u_lines_in;
    }
    num_lines_out += ExportRGB(p, p->last_y + num_lines_out);
  }
  return num_lines_out;
}

static int ExportAlpha(WebPDecParams* const p, int y_pos, int max_lines_out) {
  const WebPRGBABuffer* const buf = &p->output->u.RGBA;
  uint8_t* const base_rgba = buf->rgba + (ptrdiff_t)y_pos * buf->stride;
  const WEBP_CSP_MODE colorspace = p->output->colorspace;
  const int alpha_first =
      (colorspace == MODE_ARGB || colorspace == MODE_Argb);
  uint8_t* dst = base_rgba + (alpha_first ? 0 : 3);
  int num_lines_out = 0;
  const int is_premult_alpha = WebPIsPremultipliedMode(colorspace);
  uint32_t non_opaque = 0;
  const int width = p->scaler_a->dst_width;

  while (WebPRescalerHasPendingOutput(p->scaler_a) &&
         num_lines_out < max_lines_out) {
    assert(y_pos + num_lines_out < p->output->height);
    WebPRescalerExportRow(p->scaler_a);
    non_opaque |= WebPDispatchAlpha(p->scaler_a->dst, 0, width, 1, dst, 0);
    dst += buf->stride;
    ++num_lines_out;
  }
  if (is_premult_alpha && non_opaque) {
    WebPApplyAlphaMultiply(base_rgba, alpha_first,
                           width, num_lines_out, buf->stride);
  }
  return num_lines_out;
}

static int ExportAlphaRGBA4444(WebPDecParams* const p, int y_pos,
                               int max_lines_out) {
  const WebPRGBABuffer* const buf = &p->output->u.RGBA;
  uint8_t* const base_rgba = buf->rgba + (ptrdiff_t)y_pos * buf->stride;
#if (WEBP_SWAP_16BIT_CSP == 1)
  uint8_t* alpha_dst = base_rgba;
#else
  uint8_t* alpha_dst = base_rgba + 1;
#endif
  int num_lines_out = 0;
  const WEBP_CSP_MODE colorspace = p->output->colorspace;
  const int width = p->scaler_a->dst_width;
  const int is_premult_alpha = WebPIsPremultipliedMode(colorspace);
  uint32_t alpha_mask = 0x0f;

  while (WebPRescalerHasPendingOutput(p->scaler_a) &&
         num_lines_out < max_lines_out) {
    int i;
    assert(y_pos + num_lines_out < p->output->height);
    WebPRescalerExportRow(p->scaler_a);
    for (i = 0; i < width; ++i) {

      const uint32_t alpha_value = p->scaler_a->dst[i] >> 4;
      alpha_dst[2 * i] = (alpha_dst[2 * i] & 0xf0) | alpha_value;
      alpha_mask &= alpha_value;
    }
    alpha_dst += buf->stride;
    ++num_lines_out;
  }
  if (is_premult_alpha && alpha_mask != 0x0f) {
    WebPApplyAlphaMultiply4444(base_rgba, width, num_lines_out, buf->stride);
  }
  return num_lines_out;
}

static int EmitRescaledAlphaRGB(const VP8Io* const io, WebPDecParams* const p,
                                int expected_num_out_lines) {
  if (io->a != NULL) {
    WebPRescaler* const scaler = p->scaler_a;
    int lines_left = expected_num_out_lines;
    const int y_end = p->last_y + lines_left;
    while (lines_left > 0) {
      const int64_t row_offset = (ptrdiff_t)scaler->src_y - io->mb_y;
      WebPRescalerImport(scaler, io->mb_h + io->mb_y - scaler->src_y,
                         io->a + row_offset * io->width, io->width);
      lines_left -= p->emit_alpha_row(p, y_end - lines_left, lines_left);
    }
  }
  return 0;
}

static int InitRGBRescaler(const VP8Io* const io, WebPDecParams* const p) {
  const int has_alpha = WebPIsAlphaMode(p->output->colorspace);
  const int out_width  = io->scaled_width;
  const int out_height = io->scaled_height;
  const int uv_in_width  = (io->mb_w + 1) >> 1;
  const int uv_in_height = (io->mb_h + 1) >> 1;

  const size_t work_size = 2 * (size_t)out_width;
  rescaler_t* work;
  uint8_t* tmp;
  uint64_t tmp_size1, tmp_size2, total_size;
  size_t rescaler_size;
  WebPRescaler* scalers;
  const int num_rescalers = has_alpha ? 4 : 3;

  tmp_size1 = (uint64_t)num_rescalers * work_size;
  tmp_size2 = (uint64_t)num_rescalers * out_width;
  total_size = tmp_size1 * sizeof(*work) + tmp_size2 * sizeof(*tmp);
  rescaler_size = num_rescalers * sizeof(*p->scaler_y) + WEBP_ALIGN_CST;
  total_size += rescaler_size;
  if (!CheckSizeOverflow(total_size)) {
    return 0;
  }

  p->memory = WebPSafeMalloc(1ULL, (size_t)total_size);
  if (p->memory == NULL) {
    return 0;
  }
  work = (rescaler_t*)p->memory;
  tmp = (uint8_t*)(work + tmp_size1);

  scalers = (WebPRescaler*)WEBP_ALIGN(
      (const uint8_t*)work + total_size - rescaler_size);
  p->scaler_y = &scalers[0];
  p->scaler_u = &scalers[1];
  p->scaler_v = &scalers[2];
  p->scaler_a = has_alpha ? &scalers[3] : NULL;

  if (!WebPRescalerInit(p->scaler_y, io->mb_w, io->mb_h,
                        tmp + 0 * out_width, out_width, out_height, 0, 1,
                        work + 0 * work_size) ||
      !WebPRescalerInit(p->scaler_u, uv_in_width, uv_in_height,
                        tmp + 1 * out_width, out_width, out_height, 0, 1,
                        work + 1 * work_size) ||
      !WebPRescalerInit(p->scaler_v, uv_in_width, uv_in_height,
                        tmp + 2 * out_width, out_width, out_height, 0, 1,
                        work + 2 * work_size)) {
    return 0;
  }
  p->emit = EmitRescaledRGB;
  WebPInitYUV444Converters();

  if (has_alpha) {
    if (!WebPRescalerInit(p->scaler_a, io->mb_w, io->mb_h,
                          tmp + 3 * out_width, out_width, out_height, 0, 1,
                          work + 3 * work_size)) {
      return 0;
    }
    p->emit_alpha = EmitRescaledAlphaRGB;
    if (p->output->colorspace == MODE_RGBA_4444 ||
        p->output->colorspace == MODE_rgbA_4444) {
      p->emit_alpha_row = ExportAlphaRGBA4444;
    } else {
      p->emit_alpha_row = ExportAlpha;
    }
    WebPInitAlphaProcessing();
  }
  return 1;
}

#endif

static int CustomSetup(VP8Io* io) {
  WebPDecParams* const p = (WebPDecParams*)io->opaque;
  const WEBP_CSP_MODE colorspace = p->output->colorspace;
  const int is_rgb = WebPIsRGBMode(colorspace);
  const int is_alpha = WebPIsAlphaMode(colorspace);

  p->memory = NULL;
  p->emit = NULL;
  p->emit_alpha = NULL;
  p->emit_alpha_row = NULL;
  if (!WebPIoInitFromOptions(p->options, io, is_alpha ? MODE_YUV : MODE_YUVA)) {
    return 0;
  }
  if (is_alpha && WebPIsPremultipliedMode(colorspace)) {
    WebPInitUpsamplers();
  }
  if (io->use_scaling) {
#if !defined(WEBP_REDUCE_SIZE)
    const int ok = is_rgb ? InitRGBRescaler(io, p) : InitYUVRescaler(io, p);
    if (!ok) {
      return 0;
    }
#else
    return 0;
#endif
  } else {
    if (is_rgb) {
      WebPInitSamplers();
      p->emit = EmitSampledRGB;
      if (io->fancy_upsampling) {
#ifdef FANCY_UPSAMPLING
        const int uv_width = (io->mb_w + 1) >> 1;
        p->memory = WebPSafeMalloc(1ULL, (size_t)(io->mb_w + 2 * uv_width));
        if (p->memory == NULL) {
          return 0;
        }
        p->tmp_y = (uint8_t*)p->memory;
        p->tmp_u = p->tmp_y + io->mb_w;
        p->tmp_v = p->tmp_u + uv_width;
        p->emit = EmitFancyRGB;
        WebPInitUpsamplers();
#endif
      }
    } else {
      p->emit = EmitYUV;
    }
    if (is_alpha) {
      p->emit_alpha =
          (colorspace == MODE_RGBA_4444 || colorspace == MODE_rgbA_4444) ?
              EmitAlphaRGBA4444
          : is_rgb ? EmitAlphaRGB
          : EmitAlphaYUV;
      if (is_rgb) {
        WebPInitAlphaProcessing();
      }
    }
  }

  return 1;
}

static int CustomPut(const VP8Io* io) {
  WebPDecParams* const p = (WebPDecParams*)io->opaque;
  const int mb_w = io->mb_w;
  const int mb_h = io->mb_h;
  int num_lines_out;
  assert(!(io->mb_y & 1));

  if (mb_w <= 0 || mb_h <= 0) {
    return 0;
  }
  num_lines_out = p->emit(io, p);
  if (p->emit_alpha != NULL) {
    p->emit_alpha(io, p, num_lines_out);
  }
  p->last_y += num_lines_out;
  return 1;
}

static void CustomTeardown(const VP8Io* io) {
  WebPDecParams* const p = (WebPDecParams*)io->opaque;
  WebPSafeFree(p->memory);
  p->memory = NULL;
}

void WebPInitCustomIo(WebPDecParams* const params, VP8Io* const io) {
  io->put      = CustomPut;
  io->setup    = CustomSetup;
  io->teardown = CustomTeardown;
  io->opaque   = params;
}

static WEBP_INLINE int clip(int v, int M) {
  return v < 0 ? 0 : v > M ? M : v;
}

static const uint8_t kDcTable[128] = {
  4,     5,   6,   7,   8,   9,  10,  10,
  11,   12,  13,  14,  15,  16,  17,  17,
  18,   19,  20,  20,  21,  21,  22,  22,
  23,   23,  24,  25,  25,  26,  27,  28,
  29,   30,  31,  32,  33,  34,  35,  36,
  37,   37,  38,  39,  40,  41,  42,  43,
  44,   45,  46,  46,  47,  48,  49,  50,
  51,   52,  53,  54,  55,  56,  57,  58,
  59,   60,  61,  62,  63,  64,  65,  66,
  67,   68,  69,  70,  71,  72,  73,  74,
  75,   76,  76,  77,  78,  79,  80,  81,
  82,   83,  84,  85,  86,  87,  88,  89,
  91,   93,  95,  96,  98, 100, 101, 102,
  104, 106, 108, 110, 112, 114, 116, 118,
  122, 124, 126, 128, 130, 132, 134, 136,
  138, 140, 143, 145, 148, 151, 154, 157
};

static const uint16_t kAcTable[128] = {
  4,     5,   6,   7,   8,   9,  10,  11,
  12,   13,  14,  15,  16,  17,  18,  19,
  20,   21,  22,  23,  24,  25,  26,  27,
  28,   29,  30,  31,  32,  33,  34,  35,
  36,   37,  38,  39,  40,  41,  42,  43,
  44,   45,  46,  47,  48,  49,  50,  51,
  52,   53,  54,  55,  56,  57,  58,  60,
  62,   64,  66,  68,  70,  72,  74,  76,
  78,   80,  82,  84,  86,  88,  90,  92,
  94,   96,  98, 100, 102, 104, 106, 108,
  110, 112, 114, 116, 119, 122, 125, 128,
  131, 134, 137, 140, 143, 146, 149, 152,
  155, 158, 161, 164, 167, 170, 173, 177,
  181, 185, 189, 193, 197, 201, 205, 209,
  213, 217, 221, 225, 229, 234, 239, 245,
  249, 254, 259, 264, 269, 274, 279, 284
};

void VP8ParseQuant(VP8Decoder* const dec) {
  VP8BitReader* const br = &dec->br;
  const int base_q0 = VP8GetValue(br, 7, "global-header");
  const int dqy1_dc = VP8Get(br, "global-header") ?
       VP8GetSignedValue(br, 4, "global-header") : 0;
  const int dqy2_dc = VP8Get(br, "global-header") ?
       VP8GetSignedValue(br, 4, "global-header") : 0;
  const int dqy2_ac = VP8Get(br, "global-header") ?
       VP8GetSignedValue(br, 4, "global-header") : 0;
  const int dquv_dc = VP8Get(br, "global-header") ?
       VP8GetSignedValue(br, 4, "global-header") : 0;
  const int dquv_ac = VP8Get(br, "global-header") ?
       VP8GetSignedValue(br, 4, "global-header") : 0;

  const VP8SegmentHeader* const hdr = &dec->segment_hdr;
  int i;

  for (i = 0; i < NUM_MB_SEGMENTS; ++i) {
    int q;
    if (hdr->use_segment) {
      q = hdr->quantizer[i];
      if (!hdr->absolute_delta) {
        q += base_q0;
      }
    } else {
      if (i > 0) {
        dec->dqm[i] = dec->dqm[0];
        continue;
      } else {
        q = base_q0;
      }
    }
    {
      VP8QuantMatrix* const m = &dec->dqm[i];
      m->y1_mat[0] = kDcTable[clip(q + dqy1_dc, 127)];
      m->y1_mat[1] = kAcTable[clip(q + 0,       127)];

      m->y2_mat[0] = kDcTable[clip(q + dqy2_dc, 127)] * 2;

      m->y2_mat[1] = (kAcTable[clip(q + dqy2_ac, 127)] * 101581) >> 16;
      if (m->y2_mat[1] < 8) m->y2_mat[1] = 8;

      m->uv_mat[0] = kDcTable[clip(q + dquv_dc, 117)];
      m->uv_mat[1] = kAcTable[clip(q + dquv_ac, 127)];

      m->uv_quant = q + dquv_ac;
    }
  }
}

#include <string.h>

#ifndef WEBP_UTILS_BIT_READER_INL_UTILS_H_
#define WEBP_UTILS_BIT_READER_INL_UTILS_H_

#ifdef HAVE_CONFIG_H
#include "src/webp/config.h"
#endif

#include <assert.h>
#include <string.h>

#ifndef WEBP_UTILS_ENDIAN_INL_UTILS_H_
#define WEBP_UTILS_ENDIAN_INL_UTILS_H_

#ifdef HAVE_CONFIG_H
#include "src/webp/config.h"
#endif

#if defined(WORDS_BIGENDIAN)
#define HToLE32 BSwap32
#define HToLE16 BSwap16
#else
#define HToLE32(x) (x)
#define HToLE16(x) (x)
#endif

#if !defined(HAVE_CONFIG_H)
#if LOCAL_GCC_PREREQ(4,8) || __has_builtin(__builtin_bswap16)
#define HAVE_BUILTIN_BSWAP16
#endif
#if LOCAL_GCC_PREREQ(4,3) || __has_builtin(__builtin_bswap32)
#define HAVE_BUILTIN_BSWAP32
#endif
#if LOCAL_GCC_PREREQ(4,3) || __has_builtin(__builtin_bswap64)
#define HAVE_BUILTIN_BSWAP64
#endif
#endif

static WEBP_INLINE uint16_t BSwap16(uint16_t x) {
#if defined(HAVE_BUILTIN_BSWAP16)
  return __builtin_bswap16(x);
#elif defined(_MSC_VER)
  return _byteswap_ushort(x);
#else

  return (x >> 8) | ((x & 0xff) << 8);
#endif
}

static WEBP_INLINE uint32_t BSwap32(uint32_t x) {
#if defined(WEBP_USE_MIPS32_R2)
  uint32_t ret;
  __asm__ volatile (
    "wsbh   %[ret], %[x]          \n\t"
    "rotr   %[ret], %[ret],  16   \n\t"
    : [ret]"=r"(ret)
    : [x]"r"(x)
  );
  return ret;
#elif defined(HAVE_BUILTIN_BSWAP32)
  return __builtin_bswap32(x);
#elif defined(__i386__) || defined(__x86_64__)
  uint32_t swapped_bytes;
  __asm__ volatile("bswap %0" : "=r"(swapped_bytes) : "0"(x));
  return swapped_bytes;
#elif defined(_MSC_VER)
  return (uint32_t)_byteswap_ulong(x);
#else
  return (x >> 24) | ((x >> 8) & 0xff00) | ((x << 8) & 0xff0000) | (x << 24);
#endif
}

static WEBP_INLINE uint64_t BSwap64(uint64_t x) {
#if defined(HAVE_BUILTIN_BSWAP64)
  return __builtin_bswap64(x);
#elif defined(__x86_64__)
  uint64_t swapped_bytes;
  __asm__ volatile("bswapq %0" : "=r"(swapped_bytes) : "0"(x));
  return swapped_bytes;
#elif defined(_MSC_VER)
  return (uint64_t)_byteswap_uint64(x);
#else
  x = ((x & 0xffffffff00000000ull) >> 32) | ((x & 0x00000000ffffffffull) << 32);
  x = ((x & 0xffff0000ffff0000ull) >> 16) | ((x & 0x0000ffff0000ffffull) << 16);
  x = ((x & 0xff00ff00ff00ff00ull) >>  8) | ((x & 0x00ff00ff00ff00ffull) <<  8);
  return x;
#endif
}

#endif

#ifdef __cplusplus
extern "C" {
#endif

#if   (BITS > 32)
typedef uint64_t lbit_t;
#elif (BITS > 16)
typedef uint32_t lbit_t;
#elif (BITS >  8)
typedef uint16_t lbit_t;
#else
typedef uint8_t lbit_t;
#endif

extern const uint8_t kVP8Log2Range[128];
extern const uint8_t kVP8NewRange[128];

void VP8LoadFinalBytes(VP8BitReader* const br);

static WEBP_UBSAN_IGNORE_UNDEF WEBP_INLINE
void VP8LoadNewBytes(VP8BitReader* WEBP_RESTRICT const br) {
  assert(br != NULL && br->buf != NULL);

  if (br->buf < br->buf_max) {

    bit_t bits;
#if defined(WEBP_USE_MIPS32)

    lbit_t in_bits;
    lbit_t* p_buf = (lbit_t*)br->buf;
    __asm__ volatile(
      ".set   push                             \n\t"
      ".set   at                               \n\t"
      ".set   macro                            \n\t"
      "ulw    %[in_bits], 0(%[p_buf])          \n\t"
      ".set   pop                              \n\t"
      : [in_bits]"=r"(in_bits)
      : [p_buf]"r"(p_buf)
      : "memory", "at"
    );
#else
    lbit_t in_bits;
    memcpy(&in_bits, br->buf, sizeof(in_bits));
#endif
    br->buf += BITS >> 3;
#if !defined(WORDS_BIGENDIAN)
#if (BITS > 32)
    bits = BSwap64(in_bits);
    bits >>= 64 - BITS;
#elif (BITS >= 24)
    bits = BSwap32(in_bits);
    bits >>= (32 - BITS);
#elif (BITS == 16)
    bits = BSwap16(in_bits);
#else
    bits = (bit_t)in_bits;
#endif
#else
    bits = (bit_t)in_bits;
    if (BITS != 8 * sizeof(bit_t)) bits >>= (8 * sizeof(bit_t) - BITS);
#endif
    br->value = bits | (br->value << BITS);
    br->bits += BITS;
  } else {
    VP8LoadFinalBytes(br);
  }
}

static WEBP_INLINE int VP8GetBit(VP8BitReader* WEBP_RESTRICT const br,
                                 int prob, const char label[]) {

  range_t range = br->range;
  if (br->bits < 0) {
    VP8LoadNewBytes(br);
  }
  {
    const int pos = br->bits;
    const range_t split = (range * prob) >> 8;
    const range_t value = (range_t)(br->value >> pos);
    const int bit = (value > split);
    if (bit) {
      range -= split;
      br->value -= (bit_t)(split + 1) << pos;
    } else {
      range = split + 1;
    }
    {
      const int shift = 7 ^ BitsLog2Floor(range);
      range <<= shift;
      br->bits -= shift;
    }
    br->range = range - 1;
    BT_TRACK(br);
    return bit;
  }
}

static WEBP_UBSAN_IGNORE_UNSIGNED_OVERFLOW WEBP_INLINE
int VP8GetSigned(VP8BitReader* WEBP_RESTRICT const br, int v,
                 const char label[]) {
  if (br->bits < 0) {
    VP8LoadNewBytes(br);
  }
  {
    const int pos = br->bits;
    const range_t split = br->range >> 1;
    const range_t value = (range_t)(br->value >> pos);
    const int32_t mask = (int32_t)(split - value) >> 31;
    br->bits -= 1;
    br->range += (range_t)mask;
    br->range |= 1;
    br->value -= (bit_t)((split + 1) & (uint32_t)mask) << pos;
    BT_TRACK(br);
    return (v ^ mask) - mask;
  }
}

static WEBP_INLINE int VP8GetBitAlt(VP8BitReader* WEBP_RESTRICT const br,
                                    int prob, const char label[]) {

  range_t range = br->range;
  if (br->bits < 0) {
    VP8LoadNewBytes(br);
  }
  {
    const int pos = br->bits;
    const range_t split = (range * prob) >> 8;
    const range_t value = (range_t)(br->value >> pos);
    int bit;
    if (value > split) {
      range -= split + 1;
      br->value -= (bit_t)(split + 1) << pos;
      bit = 1;
    } else {
      range = split;
      bit = 0;
    }
    if (range <= (range_t)0x7e) {
      const int shift = kVP8Log2Range[range];
      range = kVP8NewRange[range];
      br->bits -= shift;
    }
    br->range = range;
    BT_TRACK(br);
    return bit;
  }
}

#ifdef __cplusplus
}
#endif

#endif

#if !defined(USE_GENERIC_TREE)
#if !defined(__arm__) && !defined(_M_ARM) && !WEBP_AARCH64 && \
    !defined(__wasm__)

#define USE_GENERIC_TREE 1
#else
#define USE_GENERIC_TREE 0
#endif
#endif

#if (USE_GENERIC_TREE == 1)
static const int8_t kYModesIntra4[18] = {
  -B_DC_PRED, 1,
    -B_TM_PRED, 2,
      -B_VE_PRED, 3,
        4, 6,
          -B_HE_PRED, 5,
            -B_RD_PRED, -B_VR_PRED,
        -B_LD_PRED, 7,
          -B_VL_PRED, 8,
            -B_HD_PRED, -B_HU_PRED
};
#endif

static const uint8_t
  CoeffsProba0[NUM_TYPES][NUM_BANDS][NUM_CTX][NUM_PROBAS] = {
  { { { 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128 },
      { 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128 },
      { 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128 }
    },
    { { 253, 136, 254, 255, 228, 219, 128, 128, 128, 128, 128 },
      { 189, 129, 242, 255, 227, 213, 255, 219, 128, 128, 128 },
      { 106, 126, 227, 252, 214, 209, 255, 255, 128, 128, 128 }
    },
    { { 1, 98, 248, 255, 236, 226, 255, 255, 128, 128, 128 },
      { 181, 133, 238, 254, 221, 234, 255, 154, 128, 128, 128 },
      { 78, 134, 202, 247, 198, 180, 255, 219, 128, 128, 128 },
    },
    { { 1, 185, 249, 255, 243, 255, 128, 128, 128, 128, 128 },
      { 184, 150, 247, 255, 236, 224, 128, 128, 128, 128, 128 },
      { 77, 110, 216, 255, 236, 230, 128, 128, 128, 128, 128 },
    },
    { { 1, 101, 251, 255, 241, 255, 128, 128, 128, 128, 128 },
      { 170, 139, 241, 252, 236, 209, 255, 255, 128, 128, 128 },
      { 37, 116, 196, 243, 228, 255, 255, 255, 128, 128, 128 }
    },
    { { 1, 204, 254, 255, 245, 255, 128, 128, 128, 128, 128 },
      { 207, 160, 250, 255, 238, 128, 128, 128, 128, 128, 128 },
      { 102, 103, 231, 255, 211, 171, 128, 128, 128, 128, 128 }
    },
    { { 1, 152, 252, 255, 240, 255, 128, 128, 128, 128, 128 },
      { 177, 135, 243, 255, 234, 225, 128, 128, 128, 128, 128 },
      { 80, 129, 211, 255, 194, 224, 128, 128, 128, 128, 128 }
    },
    { { 1, 1, 255, 128, 128, 128, 128, 128, 128, 128, 128 },
      { 246, 1, 255, 128, 128, 128, 128, 128, 128, 128, 128 },
      { 255, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128 }
    }
  },
  { { { 198, 35, 237, 223, 193, 187, 162, 160, 145, 155, 62 },
      { 131, 45, 198, 221, 172, 176, 220, 157, 252, 221, 1 },
      { 68, 47, 146, 208, 149, 167, 221, 162, 255, 223, 128 }
    },
    { { 1, 149, 241, 255, 221, 224, 255, 255, 128, 128, 128 },
      { 184, 141, 234, 253, 222, 220, 255, 199, 128, 128, 128 },
      { 81, 99, 181, 242, 176, 190, 249, 202, 255, 255, 128 }
    },
    { { 1, 129, 232, 253, 214, 197, 242, 196, 255, 255, 128 },
      { 99, 121, 210, 250, 201, 198, 255, 202, 128, 128, 128 },
      { 23, 91, 163, 242, 170, 187, 247, 210, 255, 255, 128 }
    },
    { { 1, 200, 246, 255, 234, 255, 128, 128, 128, 128, 128 },
      { 109, 178, 241, 255, 231, 245, 255, 255, 128, 128, 128 },
      { 44, 130, 201, 253, 205, 192, 255, 255, 128, 128, 128 }
    },
    { { 1, 132, 239, 251, 219, 209, 255, 165, 128, 128, 128 },
      { 94, 136, 225, 251, 218, 190, 255, 255, 128, 128, 128 },
      { 22, 100, 174, 245, 186, 161, 255, 199, 128, 128, 128 }
    },
    { { 1, 182, 249, 255, 232, 235, 128, 128, 128, 128, 128 },
      { 124, 143, 241, 255, 227, 234, 128, 128, 128, 128, 128 },
      { 35, 77, 181, 251, 193, 211, 255, 205, 128, 128, 128 }
    },
    { { 1, 157, 247, 255, 236, 231, 255, 255, 128, 128, 128 },
      { 121, 141, 235, 255, 225, 227, 255, 255, 128, 128, 128 },
      { 45, 99, 188, 251, 195, 217, 255, 224, 128, 128, 128 }
    },
    { { 1, 1, 251, 255, 213, 255, 128, 128, 128, 128, 128 },
      { 203, 1, 248, 255, 255, 128, 128, 128, 128, 128, 128 },
      { 137, 1, 177, 255, 224, 255, 128, 128, 128, 128, 128 }
    }
  },
  { { { 253, 9, 248, 251, 207, 208, 255, 192, 128, 128, 128 },
      { 175, 13, 224, 243, 193, 185, 249, 198, 255, 255, 128 },
      { 73, 17, 171, 221, 161, 179, 236, 167, 255, 234, 128 }
    },
    { { 1, 95, 247, 253, 212, 183, 255, 255, 128, 128, 128 },
      { 239, 90, 244, 250, 211, 209, 255, 255, 128, 128, 128 },
      { 155, 77, 195, 248, 188, 195, 255, 255, 128, 128, 128 }
    },
    { { 1, 24, 239, 251, 218, 219, 255, 205, 128, 128, 128 },
      { 201, 51, 219, 255, 196, 186, 128, 128, 128, 128, 128 },
      { 69, 46, 190, 239, 201, 218, 255, 228, 128, 128, 128 }
    },
    { { 1, 191, 251, 255, 255, 128, 128, 128, 128, 128, 128 },
      { 223, 165, 249, 255, 213, 255, 128, 128, 128, 128, 128 },
      { 141, 124, 248, 255, 255, 128, 128, 128, 128, 128, 128 }
    },
    { { 1, 16, 248, 255, 255, 128, 128, 128, 128, 128, 128 },
      { 190, 36, 230, 255, 236, 255, 128, 128, 128, 128, 128 },
      { 149, 1, 255, 128, 128, 128, 128, 128, 128, 128, 128 }
    },
    { { 1, 226, 255, 128, 128, 128, 128, 128, 128, 128, 128 },
      { 247, 192, 255, 128, 128, 128, 128, 128, 128, 128, 128 },
      { 240, 128, 255, 128, 128, 128, 128, 128, 128, 128, 128 }
    },
    { { 1, 134, 252, 255, 255, 128, 128, 128, 128, 128, 128 },
      { 213, 62, 250, 255, 255, 128, 128, 128, 128, 128, 128 },
      { 55, 93, 255, 128, 128, 128, 128, 128, 128, 128, 128 }
    },
    { { 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128 },
      { 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128 },
      { 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128 }
    }
  },
  { { { 202, 24, 213, 235, 186, 191, 220, 160, 240, 175, 255 },
      { 126, 38, 182, 232, 169, 184, 228, 174, 255, 187, 128 },
      { 61, 46, 138, 219, 151, 178, 240, 170, 255, 216, 128 }
    },
    { { 1, 112, 230, 250, 199, 191, 247, 159, 255, 255, 128 },
      { 166, 109, 228, 252, 211, 215, 255, 174, 128, 128, 128 },
      { 39, 77, 162, 232, 172, 180, 245, 178, 255, 255, 128 }
    },
    { { 1, 52, 220, 246, 198, 199, 249, 220, 255, 255, 128 },
      { 124, 74, 191, 243, 183, 193, 250, 221, 255, 255, 128 },
      { 24, 71, 130, 219, 154, 170, 243, 182, 255, 255, 128 }
    },
    { { 1, 182, 225, 249, 219, 240, 255, 224, 128, 128, 128 },
      { 149, 150, 226, 252, 216, 205, 255, 171, 128, 128, 128 },
      { 28, 108, 170, 242, 183, 194, 254, 223, 255, 255, 128 }
    },
    { { 1, 81, 230, 252, 204, 203, 255, 192, 128, 128, 128 },
      { 123, 102, 209, 247, 188, 196, 255, 233, 128, 128, 128 },
      { 20, 95, 153, 243, 164, 173, 255, 203, 128, 128, 128 }
    },
    { { 1, 222, 248, 255, 216, 213, 128, 128, 128, 128, 128 },
      { 168, 175, 246, 252, 235, 205, 255, 255, 128, 128, 128 },
      { 47, 116, 215, 255, 211, 212, 255, 255, 128, 128, 128 }
    },
    { { 1, 121, 236, 253, 212, 214, 255, 255, 128, 128, 128 },
      { 141, 84, 213, 252, 201, 202, 255, 219, 128, 128, 128 },
      { 42, 80, 160, 240, 162, 185, 255, 205, 128, 128, 128 }
    },
    { { 1, 1, 255, 128, 128, 128, 128, 128, 128, 128, 128 },
      { 244, 1, 255, 128, 128, 128, 128, 128, 128, 128, 128 },
      { 238, 1, 255, 128, 128, 128, 128, 128, 128, 128, 128 }
    }
  }
};

static const uint8_t kBModesProba[NUM_BMODES][NUM_BMODES][NUM_BMODES - 1] = {
  { { 231, 120, 48, 89, 115, 113, 120, 152, 112 },
    { 152, 179, 64, 126, 170, 118, 46, 70, 95 },
    { 175, 69, 143, 80, 85, 82, 72, 155, 103 },
    { 56, 58, 10, 171, 218, 189, 17, 13, 152 },
    { 114, 26, 17, 163, 44, 195, 21, 10, 173 },
    { 121, 24, 80, 195, 26, 62, 44, 64, 85 },
    { 144, 71, 10, 38, 171, 213, 144, 34, 26 },
    { 170, 46, 55, 19, 136, 160, 33, 206, 71 },
    { 63, 20, 8, 114, 114, 208, 12, 9, 226 },
    { 81, 40, 11, 96, 182, 84, 29, 16, 36 } },
  { { 134, 183, 89, 137, 98, 101, 106, 165, 148 },
    { 72, 187, 100, 130, 157, 111, 32, 75, 80 },
    { 66, 102, 167, 99, 74, 62, 40, 234, 128 },
    { 41, 53, 9, 178, 241, 141, 26, 8, 107 },
    { 74, 43, 26, 146, 73, 166, 49, 23, 157 },
    { 65, 38, 105, 160, 51, 52, 31, 115, 128 },
    { 104, 79, 12, 27, 217, 255, 87, 17, 7 },
    { 87, 68, 71, 44, 114, 51, 15, 186, 23 },
    { 47, 41, 14, 110, 182, 183, 21, 17, 194 },
    { 66, 45, 25, 102, 197, 189, 23, 18, 22 } },
  { { 88, 88, 147, 150, 42, 46, 45, 196, 205 },
    { 43, 97, 183, 117, 85, 38, 35, 179, 61 },
    { 39, 53, 200, 87, 26, 21, 43, 232, 171 },
    { 56, 34, 51, 104, 114, 102, 29, 93, 77 },
    { 39, 28, 85, 171, 58, 165, 90, 98, 64 },
    { 34, 22, 116, 206, 23, 34, 43, 166, 73 },
    { 107, 54, 32, 26, 51, 1, 81, 43, 31 },
    { 68, 25, 106, 22, 64, 171, 36, 225, 114 },
    { 34, 19, 21, 102, 132, 188, 16, 76, 124 },
    { 62, 18, 78, 95, 85, 57, 50, 48, 51 } },
  { { 193, 101, 35, 159, 215, 111, 89, 46, 111 },
    { 60, 148, 31, 172, 219, 228, 21, 18, 111 },
    { 112, 113, 77, 85, 179, 255, 38, 120, 114 },
    { 40, 42, 1, 196, 245, 209, 10, 25, 109 },
    { 88, 43, 29, 140, 166, 213, 37, 43, 154 },
    { 61, 63, 30, 155, 67, 45, 68, 1, 209 },
    { 100, 80, 8, 43, 154, 1, 51, 26, 71 },
    { 142, 78, 78, 16, 255, 128, 34, 197, 171 },
    { 41, 40, 5, 102, 211, 183, 4, 1, 221 },
    { 51, 50, 17, 168, 209, 192, 23, 25, 82 } },
  { { 138, 31, 36, 171, 27, 166, 38, 44, 229 },
    { 67, 87, 58, 169, 82, 115, 26, 59, 179 },
    { 63, 59, 90, 180, 59, 166, 93, 73, 154 },
    { 40, 40, 21, 116, 143, 209, 34, 39, 175 },
    { 47, 15, 16, 183, 34, 223, 49, 45, 183 },
    { 46, 17, 33, 183, 6, 98, 15, 32, 183 },
    { 57, 46, 22, 24, 128, 1, 54, 17, 37 },
    { 65, 32, 73, 115, 28, 128, 23, 128, 205 },
    { 40, 3, 9, 115, 51, 192, 18, 6, 223 },
    { 87, 37, 9, 115, 59, 77, 64, 21, 47 } },
  { { 104, 55, 44, 218, 9, 54, 53, 130, 226 },
    { 64, 90, 70, 205, 40, 41, 23, 26, 57 },
    { 54, 57, 112, 184, 5, 41, 38, 166, 213 },
    { 30, 34, 26, 133, 152, 116, 10, 32, 134 },
    { 39, 19, 53, 221, 26, 114, 32, 73, 255 },
    { 31, 9, 65, 234, 2, 15, 1, 118, 73 },
    { 75, 32, 12, 51, 192, 255, 160, 43, 51 },
    { 88, 31, 35, 67, 102, 85, 55, 186, 85 },
    { 56, 21, 23, 111, 59, 205, 45, 37, 192 },
    { 55, 38, 70, 124, 73, 102, 1, 34, 98 } },
  { { 125, 98, 42, 88, 104, 85, 117, 175, 82 },
    { 95, 84, 53, 89, 128, 100, 113, 101, 45 },
    { 75, 79, 123, 47, 51, 128, 81, 171, 1 },
    { 57, 17, 5, 71, 102, 57, 53, 41, 49 },
    { 38, 33, 13, 121, 57, 73, 26, 1, 85 },
    { 41, 10, 67, 138, 77, 110, 90, 47, 114 },
    { 115, 21, 2, 10, 102, 255, 166, 23, 6 },
    { 101, 29, 16, 10, 85, 128, 101, 196, 26 },
    { 57, 18, 10, 102, 102, 213, 34, 20, 43 },
    { 117, 20, 15, 36, 163, 128, 68, 1, 26 } },
  { { 102, 61, 71, 37, 34, 53, 31, 243, 192 },
    { 69, 60, 71, 38, 73, 119, 28, 222, 37 },
    { 68, 45, 128, 34, 1, 47, 11, 245, 171 },
    { 62, 17, 19, 70, 146, 85, 55, 62, 70 },
    { 37, 43, 37, 154, 100, 163, 85, 160, 1 },
    { 63, 9, 92, 136, 28, 64, 32, 201, 85 },
    { 75, 15, 9, 9, 64, 255, 184, 119, 16 },
    { 86, 6, 28, 5, 64, 255, 25, 248, 1 },
    { 56, 8, 17, 132, 137, 255, 55, 116, 128 },
    { 58, 15, 20, 82, 135, 57, 26, 121, 40 } },
  { { 164, 50, 31, 137, 154, 133, 25, 35, 218 },
    { 51, 103, 44, 131, 131, 123, 31, 6, 158 },
    { 86, 40, 64, 135, 148, 224, 45, 183, 128 },
    { 22, 26, 17, 131, 240, 154, 14, 1, 209 },
    { 45, 16, 21, 91, 64, 222, 7, 1, 197 },
    { 56, 21, 39, 155, 60, 138, 23, 102, 213 },
    { 83, 12, 13, 54, 192, 255, 68, 47, 28 },
    { 85, 26, 85, 85, 128, 128, 32, 146, 171 },
    { 18, 11, 7, 63, 144, 171, 4, 4, 246 },
    { 35, 27, 10, 146, 174, 171, 12, 26, 128 } },
  { { 190, 80, 35, 99, 180, 80, 126, 54, 45 },
    { 85, 126, 47, 87, 176, 51, 41, 20, 32 },
    { 101, 75, 128, 139, 118, 146, 116, 128, 85 },
    { 56, 41, 15, 176, 236, 85, 37, 9, 62 },
    { 71, 30, 17, 119, 118, 255, 17, 18, 138 },
    { 101, 38, 60, 138, 55, 70, 43, 26, 142 },
    { 146, 36, 19, 30, 171, 255, 97, 27, 20 },
    { 138, 45, 61, 62, 219, 1, 81, 188, 64 },
    { 32, 41, 20, 117, 151, 142, 20, 21, 163 },
    { 112, 19, 12, 61, 195, 128, 48, 4, 24 } }
};

void VP8ResetProba(VP8Proba* const proba) {
  memset(proba->segments, 255u, sizeof(proba->segments));

}

static void ParseIntraMode(VP8BitReader* const br,
                           VP8Decoder* const dec, int mb_x) {
  uint8_t* const top = dec->intra_t + 4 * mb_x;
  uint8_t* const left = dec->intra_l;
  VP8MBData* const block = dec->mb_data + mb_x;

  if (dec->segment_hdr.update_map) {

    block->segment = !VP8GetBit(br, dec->proba.segments[0], "segments")
                   ?  VP8GetBit(br, dec->proba.segments[1], "segments")
                   :  VP8GetBit(br, dec->proba.segments[2], "segments") + 2;
  } else {
    block->segment = 0;
  }
  if (dec->use_skip_proba) block->skip = VP8GetBit(br, dec->skip_p, "skip");

  block->is_i4x4 = !VP8GetBit(br, 145, "block-size");
  if (!block->is_i4x4) {

    const int ymode =
        VP8GetBit(br, 156, "pred-modes") ?
            (VP8GetBit(br, 128, "pred-modes") ? TM_PRED : H_PRED) :
            (VP8GetBit(br, 163, "pred-modes") ? V_PRED : DC_PRED);
    block->imodes[0] = ymode;
    memset(top, ymode, 4 * sizeof(*top));
    memset(left, ymode, 4 * sizeof(*left));
  } else {
    uint8_t* modes = block->imodes;
    int y;
    for (y = 0; y < 4; ++y) {
      int ymode = left[y];
      int x;
      for (x = 0; x < 4; ++x) {
        const uint8_t* const prob = kBModesProba[top[x]][ymode];
#if (USE_GENERIC_TREE == 1)

        int i = kYModesIntra4[VP8GetBit(br, prob[0], "pred-modes")];
        while (i > 0) {
          i = kYModesIntra4[2 * i + VP8GetBit(br, prob[i], "pred-modes")];
        }
        ymode = -i;
#else

        ymode = !VP8GetBit(br, prob[0], "pred-modes") ? B_DC_PRED :
                  !VP8GetBit(br, prob[1], "pred-modes") ? B_TM_PRED :
                    !VP8GetBit(br, prob[2], "pred-modes") ? B_VE_PRED :
                      !VP8GetBit(br, prob[3], "pred-modes") ?
                        (!VP8GetBit(br, prob[4], "pred-modes") ? B_HE_PRED :
                          (!VP8GetBit(br, prob[5], "pred-modes") ? B_RD_PRED
                                                                 : B_VR_PRED)) :
                        (!VP8GetBit(br, prob[6], "pred-modes") ? B_LD_PRED :
                          (!VP8GetBit(br, prob[7], "pred-modes") ? B_VL_PRED :
                            (!VP8GetBit(br, prob[8], "pred-modes") ? B_HD_PRED
                                                                   : B_HU_PRED))
                        );
#endif
        top[x] = ymode;
      }
      memcpy(modes, top, 4 * sizeof(*top));
      modes += 4;
      left[y] = ymode;
    }
  }

  block->uvmode = !VP8GetBit(br, 142, "pred-modes-uv") ? DC_PRED
                : !VP8GetBit(br, 114, "pred-modes-uv") ? V_PRED
                : VP8GetBit(br, 183, "pred-modes-uv") ? TM_PRED : H_PRED;
}

int VP8ParseIntraModeRow(VP8BitReader* const br, VP8Decoder* const dec) {
  int mb_x;
  for (mb_x = 0; mb_x < dec->mb_w; ++mb_x) {
    ParseIntraMode(br, dec, mb_x);
  }
  return !dec->br.eof;
}

static const uint8_t
    CoeffsUpdateProba[NUM_TYPES][NUM_BANDS][NUM_CTX][NUM_PROBAS] = {
  { { { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 }
    },
    { { 176, 246, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 223, 241, 252, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 249, 253, 253, 255, 255, 255, 255, 255, 255, 255, 255 }
    },
    { { 255, 244, 252, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 234, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 253, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 }
    },
    { { 255, 246, 254, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 239, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 254, 255, 254, 255, 255, 255, 255, 255, 255, 255, 255 }
    },
    { { 255, 248, 254, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 251, 255, 254, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 }
    },
    { { 255, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 251, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 254, 255, 254, 255, 255, 255, 255, 255, 255, 255, 255 }
    },
    { { 255, 254, 253, 255, 254, 255, 255, 255, 255, 255, 255 },
      { 250, 255, 254, 255, 254, 255, 255, 255, 255, 255, 255 },
      { 254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 }
    },
    { { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 }
    }
  },
  { { { 217, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 225, 252, 241, 253, 255, 255, 254, 255, 255, 255, 255 },
      { 234, 250, 241, 250, 253, 255, 253, 254, 255, 255, 255 }
    },
    { { 255, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 223, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 238, 253, 254, 254, 255, 255, 255, 255, 255, 255, 255 }
    },
    { { 255, 248, 254, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 249, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 }
    },
    { { 255, 253, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 247, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 }
    },
    { { 255, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 252, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 }
    },
    { { 255, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 253, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 }
    },
    { { 255, 254, 253, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 250, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 }
    },
    { { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 }
    }
  },
  { { { 186, 251, 250, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 234, 251, 244, 254, 255, 255, 255, 255, 255, 255, 255 },
      { 251, 251, 243, 253, 254, 255, 254, 255, 255, 255, 255 }
    },
    { { 255, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 236, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 251, 253, 253, 254, 254, 255, 255, 255, 255, 255, 255 }
    },
    { { 255, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 254, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 }
    },
    { { 255, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 254, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 }
    },
    { { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 }
    },
    { { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 }
    },
    { { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 }
    },
    { { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 }
    }
  },
  { { { 248, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 250, 254, 252, 254, 255, 255, 255, 255, 255, 255, 255 },
      { 248, 254, 249, 253, 255, 255, 255, 255, 255, 255, 255 }
    },
    { { 255, 253, 253, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 246, 253, 253, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 252, 254, 251, 254, 254, 255, 255, 255, 255, 255, 255 }
    },
    { { 255, 254, 252, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 248, 254, 253, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 253, 255, 254, 254, 255, 255, 255, 255, 255, 255, 255 }
    },
    { { 255, 251, 254, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 245, 251, 254, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 253, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255 }
    },
    { { 255, 251, 253, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 252, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 255, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255 }
    },
    { { 255, 252, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 249, 255, 254, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 255, 255, 254, 255, 255, 255, 255, 255, 255, 255, 255 }
    },
    { { 255, 255, 253, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 250, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 }
    },
    { { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
      { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 }
    }
  }
};

static const uint8_t kBands[16 + 1] = {
  0, 1, 2, 3, 6, 4, 5, 6, 6, 6, 6, 6, 6, 6, 6, 7,
  0
};

void VP8ParseProba(VP8BitReader* const br, VP8Decoder* const dec) {
  VP8Proba* const proba = &dec->proba;
  int t, b, c, p;
  for (t = 0; t < NUM_TYPES; ++t) {
    for (b = 0; b < NUM_BANDS; ++b) {
      for (c = 0; c < NUM_CTX; ++c) {
        for (p = 0; p < NUM_PROBAS; ++p) {
          const int v =
              VP8GetBit(br, CoeffsUpdateProba[t][b][c][p], "global-header") ?
                        VP8GetValue(br, 8, "global-header") :
                        CoeffsProba0[t][b][c][p];
          proba->bands[t][b].probas[c][p] = v;
        }
      }
    }
    for (b = 0; b < 16 + 1; ++b) {
      proba->bands_ptr[t][b] = &proba->bands[t][kBands[b]];
    }
  }
  dec->use_skip_proba = VP8Get(br, "global-header");
  if (dec->use_skip_proba) {
    dec->skip_p = VP8GetValue(br, 8, "global-header");
  }
}

#include <assert.h>
#include <stdlib.h>
#include <string.h>

int WebPGetDecoderVersion(void) {
  return (DEC_MAJ_VERSION << 16) | (DEC_MIN_VERSION << 8) | DEC_REV_VERSION;
}

typedef int (*GetCoeffsFunc)(VP8BitReader* const br,
                             const VP8BandProbas* const prob[],
                             int ctx, const quant_t dq, int n, int16_t* out);
static volatile GetCoeffsFunc GetCoeffs = NULL;

static void InitGetCoeffs(void);

static void SetOk(VP8Decoder* const dec) {
  dec->status = VP8_STATUS_OK;
  dec->error_msg = "OK";
}

int VP8InitIoInternal(VP8Io* const io, int version) {
  if (WEBP_ABI_IS_INCOMPATIBLE(version, WEBP_DECODER_ABI_VERSION)) {
    return 0;
  }
  if (io != NULL) {
    memset(io, 0, sizeof(*io));
  }
  return 1;
}

VP8Decoder* VP8New(void) {
  VP8Decoder* const dec = (VP8Decoder*)WebPSafeCalloc(1ULL, sizeof(*dec));
  if (dec != NULL) {
    SetOk(dec);
    WebPGetWorkerInterface()->Init(&dec->worker);
    dec->ready = 0;
    dec->num_parts_minus_one = 0;
    InitGetCoeffs();
  }
  return dec;
}

VP8StatusCode VP8Status(VP8Decoder* const dec) {
  if (!dec) return VP8_STATUS_INVALID_PARAM;
  return dec->status;
}

const char* VP8StatusMessage(VP8Decoder* const dec) {
  if (dec == NULL) return "no object";
  if (!dec->error_msg) return "OK";
  return dec->error_msg;
}

void VP8Delete(VP8Decoder* const dec) {
  if (dec != NULL) {
    VP8Clear(dec);
    WebPSafeFree(dec);
  }
}

int VP8SetError(VP8Decoder* const dec,
                VP8StatusCode error, const char* const msg) {

  assert(dec->incremental || error != VP8_STATUS_SUSPENDED);

  if (dec->status == VP8_STATUS_OK) {
    dec->status = error;
    dec->error_msg = msg;
    dec->ready = 0;
  }
  return 0;
}

int VP8CheckSignature(const uint8_t* const data, size_t data_size) {
  return (data_size >= 3 &&
          data[0] == 0x9d && data[1] == 0x01 && data[2] == 0x2a);
}

int VP8GetInfo(const uint8_t* data, size_t data_size, size_t chunk_size,
               int* const width, int* const height) {
  if (data == NULL || data_size < VP8_FRAME_HEADER_SIZE) {
    return 0;
  }

  if (!VP8CheckSignature(data + 3, data_size - 3)) {
    return 0;
  } else {
    const uint32_t bits = data[0] | (data[1] << 8) | (data[2] << 16);
    const int key_frame = !(bits & 1);
    const int w = ((data[7] << 8) | data[6]) & 0x3fff;
    const int h = ((data[9] << 8) | data[8]) & 0x3fff;

    if (!key_frame) {
      return 0;
    }

    if (((bits >> 1) & 7) > 3) {
      return 0;
    }
    if (!((bits >> 4) & 1)) {
      return 0;
    }
    if (((bits >> 5)) >= chunk_size) {
      return 0;
    }
    if (w == 0 || h == 0) {
      return 0;
    }

    if (width) {
      *width = w;
    }
    if (height) {
      *height = h;
    }

    return 1;
  }
}

static void ResetSegmentHeader(VP8SegmentHeader* const hdr) {
  assert(hdr != NULL);
  hdr->use_segment = 0;
  hdr->update_map = 0;
  hdr->absolute_delta = 1;
  memset(hdr->quantizer, 0, sizeof(hdr->quantizer));
  memset(hdr->filter_strength, 0, sizeof(hdr->filter_strength));
}

static int ParseSegmentHeader(VP8BitReader* br,
                              VP8SegmentHeader* hdr, VP8Proba* proba) {
  assert(br != NULL);
  assert(hdr != NULL);
  hdr->use_segment = VP8Get(br, "global-header");
  if (hdr->use_segment) {
    hdr->update_map = VP8Get(br, "global-header");
    if (VP8Get(br, "global-header")) {
      int s;
      hdr->absolute_delta = VP8Get(br, "global-header");
      for (s = 0; s < NUM_MB_SEGMENTS; ++s) {
        hdr->quantizer[s] = VP8Get(br, "global-header") ?
            VP8GetSignedValue(br, 7, "global-header") : 0;
      }
      for (s = 0; s < NUM_MB_SEGMENTS; ++s) {
        hdr->filter_strength[s] = VP8Get(br, "global-header") ?
            VP8GetSignedValue(br, 6, "global-header") : 0;
      }
    }
    if (hdr->update_map) {
      int s;
      for (s = 0; s < MB_FEATURE_TREE_PROBS; ++s) {
        proba->segments[s] = VP8Get(br, "global-header") ?
            VP8GetValue(br, 8, "global-header") : 255u;
      }
    }
  } else {
    hdr->update_map = 0;
  }
  return !br->eof;
}

static VP8StatusCode ParsePartitions(VP8Decoder* const dec,
                                     const uint8_t* buf, size_t size) {
  VP8BitReader* const br = &dec->br;
  const uint8_t* sz = buf;
  const uint8_t* buf_end = buf + size;
  const uint8_t* part_start;
  size_t size_left = size;
  size_t last_part;
  size_t p;

  dec->num_parts_minus_one = (1 << VP8GetValue(br, 2, "global-header")) - 1;
  last_part = dec->num_parts_minus_one;
  if (size < 3 * last_part) {

    return VP8_STATUS_NOT_ENOUGH_DATA;
  }
  part_start = buf + last_part * 3;
  size_left -= last_part * 3;
  for (p = 0; p < last_part; ++p) {
    size_t psize = sz[0] | (sz[1] << 8) | (sz[2] << 16);
    if (psize > size_left) psize = size_left;
    VP8InitBitReader(dec->parts + p, part_start, psize);
    part_start += psize;
    size_left -= psize;
    sz += 3;
  }
  VP8InitBitReader(dec->parts + last_part, part_start, size_left);
  if (part_start < buf_end) return VP8_STATUS_OK;
  return dec->incremental
             ? VP8_STATUS_SUSPENDED
             : VP8_STATUS_NOT_ENOUGH_DATA;
}

static int ParseFilterHeader(VP8BitReader* br, VP8Decoder* const dec) {
  VP8FilterHeader* const hdr = &dec->filter_hdr;
  hdr->simple    = VP8Get(br, "global-header");
  hdr->level     = VP8GetValue(br, 6, "global-header");
  hdr->sharpness = VP8GetValue(br, 3, "global-header");
  hdr->use_lf_delta = VP8Get(br, "global-header");
  if (hdr->use_lf_delta) {
    if (VP8Get(br, "global-header")) {
      int i;
      for (i = 0; i < NUM_REF_LF_DELTAS; ++i) {
        if (VP8Get(br, "global-header")) {
          hdr->ref_lf_delta[i] = VP8GetSignedValue(br, 6, "global-header");
        }
      }
      for (i = 0; i < NUM_MODE_LF_DELTAS; ++i) {
        if (VP8Get(br, "global-header")) {
          hdr->mode_lf_delta[i] = VP8GetSignedValue(br, 6, "global-header");
        }
      }
    }
  }
  dec->filter_type = (hdr->level == 0) ? 0 : hdr->simple ? 1 : 2;
  return !br->eof;
}

int VP8GetHeaders(VP8Decoder* const dec, VP8Io* const io) {
  const uint8_t* buf;
  size_t buf_size;
  VP8FrameHeader* frm_hdr;
  VP8PictureHeader* pic_hdr;
  VP8BitReader* br;
  VP8StatusCode status;

  if (dec == NULL) {
    return 0;
  }
  SetOk(dec);
  if (io == NULL) {
    return VP8SetError(dec, VP8_STATUS_INVALID_PARAM,
                       "null VP8Io passed to VP8GetHeaders()");
  }
  buf = io->data;
  buf_size = io->data_size;
  if (buf_size < 4) {
    return VP8SetError(dec, VP8_STATUS_NOT_ENOUGH_DATA,
                       "Truncated header.");
  }

  {
    const uint32_t bits = buf[0] | (buf[1] << 8) | (buf[2] << 16);
    frm_hdr = &dec->frm_hdr;
    frm_hdr->key_frame = !(bits & 1);
    frm_hdr->profile = (bits >> 1) & 7;
    frm_hdr->show = (bits >> 4) & 1;
    frm_hdr->partition_length = (bits >> 5);
    if (frm_hdr->profile > 3) {
      return VP8SetError(dec, VP8_STATUS_BITSTREAM_ERROR,
                         "Incorrect keyframe parameters.");
    }
    if (!frm_hdr->show) {
      return VP8SetError(dec, VP8_STATUS_UNSUPPORTED_FEATURE,
                         "Frame not displayable.");
    }
    buf += 3;
    buf_size -= 3;
  }

  pic_hdr = &dec->pic_hdr;
  if (frm_hdr->key_frame) {

    if (buf_size < 7) {
      return VP8SetError(dec, VP8_STATUS_NOT_ENOUGH_DATA,
                         "cannot parse picture header");
    }
    if (!VP8CheckSignature(buf, buf_size)) {
      return VP8SetError(dec, VP8_STATUS_BITSTREAM_ERROR,
                         "Bad code word");
    }
    pic_hdr->width = ((buf[4] << 8) | buf[3]) & 0x3fff;
    pic_hdr->xscale = buf[4] >> 6;
    pic_hdr->height = ((buf[6] << 8) | buf[5]) & 0x3fff;
    pic_hdr->yscale = buf[6] >> 6;
    buf += 7;
    buf_size -= 7;

    dec->mb_w = (pic_hdr->width + 15) >> 4;
    dec->mb_h = (pic_hdr->height + 15) >> 4;

    io->width = pic_hdr->width;
    io->height = pic_hdr->height;

    io->use_cropping = 0;
    io->crop_top  = 0;
    io->crop_left = 0;
    io->crop_right  = io->width;
    io->crop_bottom = io->height;
    io->use_scaling  = 0;
    io->scaled_width = io->width;
    io->scaled_height = io->height;

    io->mb_w = io->width;
    io->mb_h = io->height;

    VP8ResetProba(&dec->proba);
    ResetSegmentHeader(&dec->segment_hdr);
  }

  if (frm_hdr->partition_length > buf_size) {
    return VP8SetError(dec, VP8_STATUS_NOT_ENOUGH_DATA,
                       "bad partition length");
  }

  br = &dec->br;
  VP8InitBitReader(br, buf, frm_hdr->partition_length);
  buf += frm_hdr->partition_length;
  buf_size -= frm_hdr->partition_length;

  if (frm_hdr->key_frame) {
    pic_hdr->colorspace = VP8Get(br, "global-header");
    pic_hdr->clamp_type = VP8Get(br, "global-header");
  }
  if (!ParseSegmentHeader(br, &dec->segment_hdr, &dec->proba)) {
    return VP8SetError(dec, VP8_STATUS_BITSTREAM_ERROR,
                       "cannot parse segment header");
  }

  if (!ParseFilterHeader(br, dec)) {
    return VP8SetError(dec, VP8_STATUS_BITSTREAM_ERROR,
                       "cannot parse filter header");
  }
  status = ParsePartitions(dec, buf, buf_size);
  if (status != VP8_STATUS_OK) {
    return VP8SetError(dec, status, "cannot parse partitions");
  }

  VP8ParseQuant(dec);

  if (!frm_hdr->key_frame) {
    return VP8SetError(dec, VP8_STATUS_UNSUPPORTED_FEATURE,
                       "Not a key frame.");
  }

  VP8Get(br, "global-header");

  VP8ParseProba(br, dec);

  dec->ready = 1;
  return 1;
}

static const uint8_t kCat3[] = { 173, 148, 140, 0 };
static const uint8_t kCat4[] = { 176, 155, 140, 135, 0 };
static const uint8_t kCat5[] = { 180, 157, 141, 134, 130, 0 };
static const uint8_t kCat6[] =
  { 254, 254, 243, 230, 196, 177, 153, 140, 133, 130, 129, 0 };
static const uint8_t* const kCat3456[] = { kCat3, kCat4, kCat5, kCat6 };
static const uint8_t kZigzag[16] = {
  0, 1, 4, 8,  5, 2, 3, 6,  9, 12, 13, 10,  7, 11, 14, 15
};

static int GetLargeValue(VP8BitReader* const br, const uint8_t* const p) {
  int v;
  if (!VP8GetBit(br, p[3], "coeffs")) {
    if (!VP8GetBit(br, p[4], "coeffs")) {
      v = 2;
    } else {
      v = 3 + VP8GetBit(br, p[5], "coeffs");
    }
  } else {
    if (!VP8GetBit(br, p[6], "coeffs")) {
      if (!VP8GetBit(br, p[7], "coeffs")) {
        v = 5 + VP8GetBit(br, 159, "coeffs");
      } else {
        v = 7 + 2 * VP8GetBit(br, 165, "coeffs");
        v += VP8GetBit(br, 145, "coeffs");
      }
    } else {
      const uint8_t* tab;
      const int bit1 = VP8GetBit(br, p[8], "coeffs");
      const int bit0 = VP8GetBit(br, p[9 + bit1], "coeffs");
      const int cat = 2 * bit1 + bit0;
      v = 0;
      for (tab = kCat3456[cat]; *tab; ++tab) {
        v += v + VP8GetBit(br, *tab, "coeffs");
      }
      v += 3 + (8 << cat);
    }
  }
  return v;
}

static int GetCoeffsFast(VP8BitReader* const br,
                         const VP8BandProbas* const prob[],
                         int ctx, const quant_t dq, int n, int16_t* out) {
  const uint8_t* p = prob[n]->probas[ctx];
  for (; n < 16; ++n) {
    if (!VP8GetBit(br, p[0], "coeffs")) {
      return n;
    }
    while (!VP8GetBit(br, p[1], "coeffs")) {
      p = prob[++n]->probas[0];
      if (n == 16) return 16;
    }
    {
      const VP8ProbaArray* const p_ctx = &prob[n + 1]->probas[0];
      int v;
      if (!VP8GetBit(br, p[2], "coeffs")) {
        v = 1;
        p = p_ctx[1];
      } else {
        v = GetLargeValue(br, p);
        p = p_ctx[2];
      }
      out[kZigzag[n]] = VP8GetSigned(br, v, "coeffs") * dq[n > 0];
    }
  }
  return 16;
}

static int GetCoeffsAlt(VP8BitReader* const br,
                        const VP8BandProbas* const prob[],
                        int ctx, const quant_t dq, int n, int16_t* out) {
  const uint8_t* p = prob[n]->probas[ctx];
  for (; n < 16; ++n) {
    if (!VP8GetBitAlt(br, p[0], "coeffs")) {
      return n;
    }
    while (!VP8GetBitAlt(br, p[1], "coeffs")) {
      p = prob[++n]->probas[0];
      if (n == 16) return 16;
    }
    {
      const VP8ProbaArray* const p_ctx = &prob[n + 1]->probas[0];
      int v;
      if (!VP8GetBitAlt(br, p[2], "coeffs")) {
        v = 1;
        p = p_ctx[1];
      } else {
        v = GetLargeValue(br, p);
        p = p_ctx[2];
      }
      out[kZigzag[n]] = VP8GetSigned(br, v, "coeffs") * dq[n > 0];
    }
  }
  return 16;
}

extern VP8CPUInfo VP8GetCPUInfo;

WEBP_DSP_INIT_FUNC(InitGetCoeffs) {
  if (VP8GetCPUInfo != NULL && VP8GetCPUInfo(kSlowSSSE3)) {
    GetCoeffs = GetCoeffsAlt;
  } else {
    GetCoeffs = GetCoeffsFast;
  }
}

static WEBP_INLINE uint32_t NzCodeBits(uint32_t nz_coeffs, int nz, int dc_nz) {
  nz_coeffs <<= 2;
  nz_coeffs |= (nz > 3) ? 3 : (nz > 1) ? 2 : dc_nz;
  return nz_coeffs;
}

static int ParseResiduals(VP8Decoder* const dec,
                          VP8MB* const mb, VP8BitReader* const token_br) {
  const VP8BandProbas* (* const bands)[16 + 1] = dec->proba.bands_ptr;
  const VP8BandProbas* const * ac_proba;
  VP8MBData* const block = dec->mb_data + dec->mb_x;
  const VP8QuantMatrix* const q = &dec->dqm[block->segment];
  int16_t* dst = block->coeffs;
  VP8MB* const left_mb = dec->mb_info - 1;
  uint8_t tnz, lnz;
  uint32_t non_zero_y = 0;
  uint32_t non_zero_uv = 0;
  int x, y, ch;
  uint32_t out_t_nz, out_l_nz;
  int first;

  memset(dst, 0, 384 * sizeof(*dst));
  if (!block->is_i4x4) {
    int16_t dc[16] = { 0 };
    const int ctx = mb->nz_dc + left_mb->nz_dc;
    const int nz = GetCoeffs(token_br, bands[1], ctx, q->y2_mat, 0, dc);
    mb->nz_dc = left_mb->nz_dc = (nz > 0);
    if (nz > 1) {
      VP8TransformWHT(dc, dst);
    } else {
      int i;
      const int dc0 = (dc[0] + 3) >> 3;
      for (i = 0; i < 16 * 16; i += 16) dst[i] = dc0;
    }
    first = 1;
    ac_proba = bands[0];
  } else {
    first = 0;
    ac_proba = bands[3];
  }

  tnz = mb->nz & 0x0f;
  lnz = left_mb->nz & 0x0f;
  for (y = 0; y < 4; ++y) {
    int l = lnz & 1;
    uint32_t nz_coeffs = 0;
    for (x = 0; x < 4; ++x) {
      const int ctx = l + (tnz & 1);
      const int nz = GetCoeffs(token_br, ac_proba, ctx, q->y1_mat, first, dst);
      l = (nz > first);
      tnz = (tnz >> 1) | (l << 7);
      nz_coeffs = NzCodeBits(nz_coeffs, nz, dst[0] != 0);
      dst += 16;
    }
    tnz >>= 4;
    lnz = (lnz >> 1) | (l << 7);
    non_zero_y = (non_zero_y << 8) | nz_coeffs;
  }
  out_t_nz = tnz;
  out_l_nz = lnz >> 4;

  for (ch = 0; ch < 4; ch += 2) {
    uint32_t nz_coeffs = 0;
    tnz = mb->nz >> (4 + ch);
    lnz = left_mb->nz >> (4 + ch);
    for (y = 0; y < 2; ++y) {
      int l = lnz & 1;
      for (x = 0; x < 2; ++x) {
        const int ctx = l + (tnz & 1);
        const int nz = GetCoeffs(token_br, bands[2], ctx, q->uv_mat, 0, dst);
        l = (nz > 0);
        tnz = (tnz >> 1) | (l << 3);
        nz_coeffs = NzCodeBits(nz_coeffs, nz, dst[0] != 0);
        dst += 16;
      }
      tnz >>= 2;
      lnz = (lnz >> 1) | (l << 5);
    }

    non_zero_uv |= nz_coeffs << (4 * ch);
    out_t_nz |= (tnz << 4) << ch;
    out_l_nz |= (lnz & 0xf0) << ch;
  }
  mb->nz = out_t_nz;
  left_mb->nz = out_l_nz;

  block->non_zero_y = non_zero_y;
  block->non_zero_uv = non_zero_uv;

  block->dither = (non_zero_uv & 0xaaaa) ? 0 : q->dither;

  return !(non_zero_y | non_zero_uv);
}

int VP8DecodeMB(VP8Decoder* const dec, VP8BitReader* const token_br) {
  VP8MB* const left = dec->mb_info - 1;
  VP8MB* const mb = dec->mb_info + dec->mb_x;
  VP8MBData* const block = dec->mb_data + dec->mb_x;
  int skip = dec->use_skip_proba ? block->skip : 0;

  if (!skip) {
    skip = ParseResiduals(dec, mb, token_br);
  } else {
    left->nz = mb->nz = 0;
    if (!block->is_i4x4) {
      left->nz_dc = mb->nz_dc = 0;
    }
    block->non_zero_y = 0;
    block->non_zero_uv = 0;
    block->dither = 0;
  }

  if (dec->filter_type > 0) {
    VP8FInfo* const finfo = dec->f_info + dec->mb_x;
    *finfo = dec->fstrengths[block->segment][block->is_i4x4];
    finfo->f_inner |= !skip;
  }

  return !token_br->eof;
}

void VP8InitScanline(VP8Decoder* const dec) {
  VP8MB* const left = dec->mb_info - 1;
  left->nz = 0;
  left->nz_dc = 0;
  memset(dec->intra_l, B_DC_PRED, sizeof(dec->intra_l));
  dec->mb_x = 0;
}

static int ParseFrame(VP8Decoder* const dec, VP8Io* io) {
  for (dec->mb_y = 0; dec->mb_y < dec->br_mb_y; ++dec->mb_y) {

    VP8BitReader* const token_br =
        &dec->parts[dec->mb_y & dec->num_parts_minus_one];
    if (!VP8ParseIntraModeRow(&dec->br, dec)) {
      return VP8SetError(dec, VP8_STATUS_NOT_ENOUGH_DATA,
                         "Premature end-of-partition0 encountered.");
    }
    for (; dec->mb_x < dec->mb_w; ++dec->mb_x) {
      if (!VP8DecodeMB(dec, token_br)) {
        return VP8SetError(dec, VP8_STATUS_NOT_ENOUGH_DATA,
                           "Premature end-of-file encountered.");
      }
    }
    VP8InitScanline(dec);

    if (!VP8ProcessRow(dec, io)) {
      return VP8SetError(dec, VP8_STATUS_USER_ABORT, "Output aborted.");
    }
  }
  if (dec->mt_method > 0) {
    if (!WebPGetWorkerInterface()->Sync(&dec->worker)) return 0;
  }

  return 1;
}

int VP8Decode(VP8Decoder* const dec, VP8Io* const io) {
  int ok = 0;
  if (dec == NULL) {
    return 0;
  }
  if (io == NULL) {
    return VP8SetError(dec, VP8_STATUS_INVALID_PARAM,
                       "NULL VP8Io parameter in VP8Decode().");
  }

  if (!dec->ready) {
    if (!VP8GetHeaders(dec, io)) {
      return 0;
    }
  }
  assert(dec->ready);

  ok = (VP8EnterCritical(dec, io) == VP8_STATUS_OK);
  if (ok) {

    if (ok) ok = VP8InitFrame(dec, io);

    if (ok) ok = ParseFrame(dec, io);

    ok &= VP8ExitCritical(dec, io);
  }

  if (!ok) {
    VP8Clear(dec);
    return 0;
  }

  dec->ready = 0;
  return ok;
}

void VP8Clear(VP8Decoder* const dec) {
  if (dec == NULL) {
    return;
  }
  WebPGetWorkerInterface()->End(&dec->worker);
  WebPDeallocateAlphaMemory(dec);
  WebPSafeFree(dec->mem);
  dec->mem = NULL;
  dec->mem_size = 0;
  memset(&dec->br, 0, sizeof(dec->br));
  dec->ready = 0;
}

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#ifndef WEBP_DSP_LOSSLESS_H_
#define WEBP_DSP_LOSSLESS_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t (*VP8LPredictorFunc)(const uint32_t* const left,
                                      const uint32_t* const top);
extern VP8LPredictorFunc VP8LPredictors[16];

uint32_t VP8LPredictor2_C(const uint32_t* const left,
                          const uint32_t* const top);
uint32_t VP8LPredictor3_C(const uint32_t* const left,
                          const uint32_t* const top);
uint32_t VP8LPredictor4_C(const uint32_t* const left,
                          const uint32_t* const top);
uint32_t VP8LPredictor5_C(const uint32_t* const left,
                          const uint32_t* const top);
uint32_t VP8LPredictor6_C(const uint32_t* const left,
                          const uint32_t* const top);
uint32_t VP8LPredictor7_C(const uint32_t* const left,
                          const uint32_t* const top);
uint32_t VP8LPredictor8_C(const uint32_t* const left,
                          const uint32_t* const top);
uint32_t VP8LPredictor9_C(const uint32_t* const left,
                          const uint32_t* const top);
uint32_t VP8LPredictor10_C(const uint32_t* const left,
                           const uint32_t* const top);
uint32_t VP8LPredictor11_C(const uint32_t* const left,
                           const uint32_t* const top);
uint32_t VP8LPredictor12_C(const uint32_t* const left,
                           const uint32_t* const top);
uint32_t VP8LPredictor13_C(const uint32_t* const left,
                           const uint32_t* const top);

typedef void (*VP8LPredictorAddSubFunc)(const uint32_t* in,
                                        const uint32_t* upper, int num_pixels,
                                        uint32_t* WEBP_RESTRICT out);
extern VP8LPredictorAddSubFunc VP8LPredictorsAdd[16];
extern VP8LPredictorAddSubFunc VP8LPredictorsAdd_C[16];
extern VP8LPredictorAddSubFunc VP8LPredictorsAdd_SSE[16];

typedef void (*VP8LProcessDecBlueAndRedFunc)(const uint32_t* src,
                                             int num_pixels, uint32_t* dst);
extern VP8LProcessDecBlueAndRedFunc VP8LAddGreenToBlueAndRed;
extern VP8LProcessDecBlueAndRedFunc VP8LAddGreenToBlueAndRed_SSE;

typedef struct {

  uint8_t green_to_red;
  uint8_t green_to_blue;
  uint8_t red_to_blue;
} VP8LMultipliers;
typedef void (*VP8LTransformColorInverseFunc)(const VP8LMultipliers* const m,
                                              const uint32_t* src,
                                              int num_pixels, uint32_t* dst);
extern VP8LTransformColorInverseFunc VP8LTransformColorInverse;
extern VP8LTransformColorInverseFunc VP8LTransformColorInverse_SSE;

struct VP8LTransform;

void VP8LInverseTransform(const struct VP8LTransform* const transform,
                          int row_start, int row_end,
                          const uint32_t* const in, uint32_t* const out);

typedef void (*VP8LConvertFunc)(const uint32_t* WEBP_RESTRICT src,
                                int num_pixels, uint8_t* WEBP_RESTRICT dst);
extern VP8LConvertFunc VP8LConvertBGRAToRGB;
extern VP8LConvertFunc VP8LConvertBGRAToRGBA;
extern VP8LConvertFunc VP8LConvertBGRAToRGBA4444;
extern VP8LConvertFunc VP8LConvertBGRAToRGB565;
extern VP8LConvertFunc VP8LConvertBGRAToBGR;
extern VP8LConvertFunc VP8LConvertBGRAToRGB_SSE;
extern VP8LConvertFunc VP8LConvertBGRAToRGBA_SSE;

void VP8LConvertFromBGRA(const uint32_t* const in_data, int num_pixels,
                         WEBP_CSP_MODE out_colorspace, uint8_t* const rgba);

typedef void (*VP8LMapARGBFunc)(const uint32_t* src,
                                const uint32_t* const color_map,
                                uint32_t* dst, int y_start,
                                int y_end, int width);
typedef void (*VP8LMapAlphaFunc)(const uint8_t* src,
                                 const uint32_t* const color_map,
                                 uint8_t* dst, int y_start,
                                 int y_end, int width);

extern VP8LMapARGBFunc VP8LMapColor32b;
extern VP8LMapAlphaFunc VP8LMapColor8b;

void VP8LColorIndexInverseTransformAlpha(
    const struct VP8LTransform* const transform, int y_start, int y_end,
    const uint8_t* src, uint8_t* dst);

void VP8LTransformColorInverse_C(const VP8LMultipliers* const m,
                                 const uint32_t* src, int num_pixels,
                                 uint32_t* dst);

void VP8LConvertBGRAToRGB_C(const uint32_t* WEBP_RESTRICT src, int num_pixels,
                            uint8_t* WEBP_RESTRICT dst);
void VP8LConvertBGRAToRGBA_C(const uint32_t* WEBP_RESTRICT src, int num_pixels,
                             uint8_t* WEBP_RESTRICT dst);
void VP8LConvertBGRAToRGBA4444_C(const uint32_t* WEBP_RESTRICT src,
                                 int num_pixels, uint8_t* WEBP_RESTRICT dst);
void VP8LConvertBGRAToRGB565_C(const uint32_t* WEBP_RESTRICT src,
                               int num_pixels, uint8_t* WEBP_RESTRICT dst);
void VP8LConvertBGRAToBGR_C(const uint32_t* WEBP_RESTRICT src, int num_pixels,
                            uint8_t* WEBP_RESTRICT dst);
void VP8LAddGreenToBlueAndRed_C(const uint32_t* src, int num_pixels,
                                uint32_t* dst);

void VP8LDspInit(void);

typedef void (*VP8LProcessEncBlueAndRedFunc)(uint32_t* dst, int num_pixels);
extern VP8LProcessEncBlueAndRedFunc VP8LSubtractGreenFromBlueAndRed;
extern VP8LProcessEncBlueAndRedFunc VP8LSubtractGreenFromBlueAndRed_SSE;
typedef void (*VP8LTransformColorFunc)(
    const VP8LMultipliers* WEBP_RESTRICT const m, uint32_t* WEBP_RESTRICT dst,
    int num_pixels);
extern VP8LTransformColorFunc VP8LTransformColor;
extern VP8LTransformColorFunc VP8LTransformColor_SSE;
typedef void (*VP8LCollectColorBlueTransformsFunc)(
    const uint32_t* WEBP_RESTRICT argb, int stride,
    int tile_width, int tile_height,
    int green_to_blue, int red_to_blue, uint32_t histo[]);
extern VP8LCollectColorBlueTransformsFunc VP8LCollectColorBlueTransforms;
extern VP8LCollectColorBlueTransformsFunc VP8LCollectColorBlueTransforms_SSE;

typedef void (*VP8LCollectColorRedTransformsFunc)(
    const uint32_t* WEBP_RESTRICT argb, int stride,
    int tile_width, int tile_height,
    int green_to_red, uint32_t histo[]);
extern VP8LCollectColorRedTransformsFunc VP8LCollectColorRedTransforms;
extern VP8LCollectColorRedTransformsFunc VP8LCollectColorRedTransforms_SSE;

void VP8LTransformColor_C(const VP8LMultipliers* WEBP_RESTRICT const m,
                          uint32_t* WEBP_RESTRICT data, int num_pixels);
void VP8LSubtractGreenFromBlueAndRed_C(uint32_t* argb_data, int num_pixels);
void VP8LCollectColorRedTransforms_C(const uint32_t* WEBP_RESTRICT argb,
                                     int stride,
                                     int tile_width, int tile_height,
                                     int green_to_red, uint32_t histo[]);
void VP8LCollectColorBlueTransforms_C(const uint32_t* WEBP_RESTRICT argb,
                                      int stride,
                                      int tile_width, int tile_height,
                                      int green_to_blue, int red_to_blue,
                                      uint32_t histo[]);

extern VP8LPredictorAddSubFunc VP8LPredictorsSub[16];
extern VP8LPredictorAddSubFunc VP8LPredictorsSub_C[16];
extern VP8LPredictorAddSubFunc VP8LPredictorsSub_SSE[16];

typedef uint32_t (*VP8LCostFunc)(const uint32_t* population, int length);
typedef uint64_t (*VP8LCombinedShannonEntropyFunc)(const uint32_t X[256],
                                                   const uint32_t Y[256]);
typedef uint64_t (*VP8LShannonEntropyFunc)(const uint32_t* X, int length);

extern VP8LCostFunc VP8LExtraCost;
extern VP8LCombinedShannonEntropyFunc VP8LCombinedShannonEntropy;
extern VP8LShannonEntropyFunc VP8LShannonEntropy;

typedef struct {
  int counts[2];
  int streaks[2][2];
} VP8LStreaks;

typedef struct {
  uint64_t entropy;
  uint32_t sum;
  int nonzeros;
  uint32_t max_val;
  uint32_t nonzero_code;
} VP8LBitEntropy;

void VP8LBitEntropyInit(VP8LBitEntropy* const entropy);

typedef void (*VP8LGetCombinedEntropyUnrefinedFunc)(
    const uint32_t X[], const uint32_t Y[], int length,
    VP8LBitEntropy* WEBP_RESTRICT const bit_entropy,
    VP8LStreaks* WEBP_RESTRICT const stats);
extern VP8LGetCombinedEntropyUnrefinedFunc VP8LGetCombinedEntropyUnrefined;

typedef void (*VP8LGetEntropyUnrefinedFunc)(
    const uint32_t X[], int length,
    VP8LBitEntropy* WEBP_RESTRICT const bit_entropy,
    VP8LStreaks* WEBP_RESTRICT const stats);
extern VP8LGetEntropyUnrefinedFunc VP8LGetEntropyUnrefined;

void VP8LBitsEntropyUnrefined(const uint32_t* WEBP_RESTRICT const array, int n,
                              VP8LBitEntropy* WEBP_RESTRICT const entropy);

typedef void (*VP8LAddVectorFunc)(const uint32_t* WEBP_RESTRICT a,
                                  const uint32_t* WEBP_RESTRICT b,
                                  uint32_t* WEBP_RESTRICT out, int size);
extern VP8LAddVectorFunc VP8LAddVector;
typedef void (*VP8LAddVectorEqFunc)(const uint32_t* WEBP_RESTRICT a,
                                    uint32_t* WEBP_RESTRICT out, int size);
extern VP8LAddVectorEqFunc VP8LAddVectorEq;

typedef int (*VP8LVectorMismatchFunc)(const uint32_t* const array1,
                                      const uint32_t* const array2, int length);

extern VP8LVectorMismatchFunc VP8LVectorMismatch;

typedef void (*VP8LBundleColorMapFunc)(const uint8_t* WEBP_RESTRICT const row,
                                       int width, int xbits,
                                       uint32_t* WEBP_RESTRICT dst);
extern VP8LBundleColorMapFunc VP8LBundleColorMap;
extern VP8LBundleColorMapFunc VP8LBundleColorMap_SSE;
void VP8LBundleColorMap_C(const uint8_t* WEBP_RESTRICT const row,
                          int width, int xbits, uint32_t* WEBP_RESTRICT dst);

void VP8LEncDspInit(void);

#ifdef __cplusplus
}
#endif

#endif

#ifndef WEBP_DSP_LOSSLESS_COMMON_H_
#define WEBP_DSP_LOSSLESS_COMMON_H_

#include <assert.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

static WEBP_INLINE uint32_t VP8GetARGBIndex(uint32_t idx) {
  return (idx >> 8) & 0xff;
}

static WEBP_INLINE uint8_t VP8GetAlphaIndex(uint8_t idx) {
  return idx;
}

static WEBP_INLINE uint32_t VP8GetARGBValue(uint32_t val) {
  return val;
}

static WEBP_INLINE uint8_t VP8GetAlphaValue(uint32_t val) {
  return (val >> 8) & 0xff;
}

static WEBP_INLINE uint32_t VP8LSubSampleSize(uint32_t size,
                                              uint32_t sampling_bits) {
  return (size + (1 << sampling_bits) - 1) >> sampling_bits;
}

static WEBP_INLINE int VP8LNearLosslessBits(int near_lossless_quality) {

  return 5 - near_lossless_quality / 20;
}

#define APPROX_LOG_WITH_CORRECTION_MAX  65536
#define APPROX_LOG_MAX                   4096

#define LOG_2_PRECISION_BITS 23
#define LOG_2_RECIPROCAL 1.44269504088896338700465094007086

#define LOG_2_RECIPROCAL_FIXED_DOUBLE 12102203.161561485379934310913085937500
#define LOG_2_RECIPROCAL_FIXED ((uint64_t)12102203)
#define LOG_LOOKUP_IDX_MAX 256
extern const uint32_t kLog2Table[LOG_LOOKUP_IDX_MAX];
extern const uint64_t kSLog2Table[LOG_LOOKUP_IDX_MAX];
typedef uint32_t (*VP8LFastLog2SlowFunc)(uint32_t v);
typedef uint64_t (*VP8LFastSLog2SlowFunc)(uint32_t v);

extern VP8LFastLog2SlowFunc VP8LFastLog2Slow;
extern VP8LFastSLog2SlowFunc VP8LFastSLog2Slow;

static WEBP_INLINE uint32_t VP8LFastLog2(uint32_t v) {
  return (v < LOG_LOOKUP_IDX_MAX) ? kLog2Table[v] : VP8LFastLog2Slow(v);
}

static WEBP_INLINE uint64_t VP8LFastSLog2(uint32_t v) {
  return (v < LOG_LOOKUP_IDX_MAX) ? kSLog2Table[v] : VP8LFastSLog2Slow(v);
}

static WEBP_INLINE uint64_t RightShiftRound(uint64_t v, uint32_t shift) {
  return (v + (1ull << shift >> 1)) >> shift;
}

static WEBP_INLINE int64_t DivRound(int64_t a, int64_t b) {
  return ((a < 0) == (b < 0)) ? ((a + b / 2) / b) : ((a - b / 2) / b);
}

#define WEBP_INT64_MAX ((int64_t)((1ull << 63) - 1))
#define WEBP_UINT64_MAX (~0ull)

static WEBP_INLINE void VP8LPrefixEncodeBitsNoLUT(int distance, int* const code,
                                                  int* const extra_bits) {
  const int highest_bit = BitsLog2Floor(--distance);
  const int second_highest_bit = (distance >> (highest_bit - 1)) & 1;
  *extra_bits = highest_bit - 1;
  *code = 2 * highest_bit + second_highest_bit;
}

static WEBP_INLINE void VP8LPrefixEncodeNoLUT(int distance, int* const code,
                                              int* const extra_bits,
                                              int* const extra_bits_value) {
  const int highest_bit = BitsLog2Floor(--distance);
  const int second_highest_bit = (distance >> (highest_bit - 1)) & 1;
  *extra_bits = highest_bit - 1;
  *extra_bits_value = distance & ((1 << *extra_bits) - 1);
  *code = 2 * highest_bit + second_highest_bit;
}

#define PREFIX_LOOKUP_IDX_MAX   512
typedef struct {
  int8_t code;
  int8_t extra_bits;
} VP8LPrefixCode;

extern const VP8LPrefixCode kPrefixEncodeCode[PREFIX_LOOKUP_IDX_MAX];
extern const uint8_t kPrefixEncodeExtraBitsValue[PREFIX_LOOKUP_IDX_MAX];
static WEBP_INLINE void VP8LPrefixEncodeBits(int distance, int* const code,
                                             int* const extra_bits) {
  if (distance < PREFIX_LOOKUP_IDX_MAX) {
    const VP8LPrefixCode prefix_code = kPrefixEncodeCode[distance];
    *code = prefix_code.code;
    *extra_bits = prefix_code.extra_bits;
  } else {
    VP8LPrefixEncodeBitsNoLUT(distance, code, extra_bits);
  }
}

static WEBP_INLINE void VP8LPrefixEncode(int distance, int* const code,
                                         int* const extra_bits,
                                         int* const extra_bits_value) {
  if (distance < PREFIX_LOOKUP_IDX_MAX) {
    const VP8LPrefixCode prefix_code = kPrefixEncodeCode[distance];
    *code = prefix_code.code;
    *extra_bits = prefix_code.extra_bits;
    *extra_bits_value = kPrefixEncodeExtraBitsValue[distance];
  } else {
    VP8LPrefixEncodeNoLUT(distance, code, extra_bits, extra_bits_value);
  }
}

static WEBP_UBSAN_IGNORE_UNSIGNED_OVERFLOW WEBP_INLINE
uint32_t VP8LAddPixels(uint32_t a, uint32_t b) {
  const uint32_t alpha_and_green = (a & 0xff00ff00u) + (b & 0xff00ff00u);
  const uint32_t red_and_blue = (a & 0x00ff00ffu) + (b & 0x00ff00ffu);
  return (alpha_and_green & 0xff00ff00u) | (red_and_blue & 0x00ff00ffu);
}

static WEBP_UBSAN_IGNORE_UNSIGNED_OVERFLOW WEBP_INLINE
uint32_t VP8LSubPixels(uint32_t a, uint32_t b) {
  const uint32_t alpha_and_green =
      0x00ff00ffu + (a & 0xff00ff00u) - (b & 0xff00ff00u);
  const uint32_t red_and_blue =
      0xff00ff00u + (a & 0x00ff00ffu) - (b & 0x00ff00ffu);
  return (alpha_and_green & 0xff00ff00u) | (red_and_blue & 0x00ff00ffu);
}

#define GENERATE_PREDICTOR_ADD(PREDICTOR, PREDICTOR_ADD)                 \
static void PREDICTOR_ADD(const uint32_t* in, const uint32_t* upper,     \
                          int num_pixels, uint32_t* WEBP_RESTRICT out) { \
  int x;                                                                 \
  assert(upper != NULL);                                                 \
  for (x = 0; x < num_pixels; ++x) {                                     \
    const uint32_t pred = (PREDICTOR)(&out[x - 1], upper + x);           \
    out[x] = VP8LAddPixels(in[x], pred);                                 \
  }                                                                      \
}

#ifdef __cplusplus
}
#endif

#endif

#define NUM_ARGB_CACHE_ROWS          16

static const int kCodeLengthLiterals = 16;
static const int kCodeLengthRepeatCode = 16;
static const uint8_t kCodeLengthExtraBits[3] = { 2, 3, 7 };
static const uint8_t kCodeLengthRepeatOffsets[3] = { 3, 3, 11 };

typedef enum {
  GREEN = 0,
  RED   = 1,
  BLUE  = 2,
  ALPHA = 3,
  DIST  = 4
} HuffIndex;

static const uint16_t kAlphabetSize[HUFFMAN_CODES_PER_META_CODE] = {
  NUM_LITERAL_CODES + NUM_LENGTH_CODES,
  NUM_LITERAL_CODES, NUM_LITERAL_CODES, NUM_LITERAL_CODES,
  NUM_DISTANCE_CODES
};

static const uint8_t kLiteralMap[HUFFMAN_CODES_PER_META_CODE] = {
  0, 1, 1, 1, 0
};

#define NUM_CODE_LENGTH_CODES       19
static const uint8_t kCodeLengthCodeOrder[NUM_CODE_LENGTH_CODES] = {
  17, 18, 0, 1, 2, 3, 4, 5, 16, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
};

#define CODE_TO_PLANE_CODES        120
static const uint8_t kCodeToPlane[CODE_TO_PLANE_CODES] = {
  0x18, 0x07, 0x17, 0x19, 0x28, 0x06, 0x27, 0x29, 0x16, 0x1a,
  0x26, 0x2a, 0x38, 0x05, 0x37, 0x39, 0x15, 0x1b, 0x36, 0x3a,
  0x25, 0x2b, 0x48, 0x04, 0x47, 0x49, 0x14, 0x1c, 0x35, 0x3b,
  0x46, 0x4a, 0x24, 0x2c, 0x58, 0x45, 0x4b, 0x34, 0x3c, 0x03,
  0x57, 0x59, 0x13, 0x1d, 0x56, 0x5a, 0x23, 0x2d, 0x44, 0x4c,
  0x55, 0x5b, 0x33, 0x3d, 0x68, 0x02, 0x67, 0x69, 0x12, 0x1e,
  0x66, 0x6a, 0x22, 0x2e, 0x54, 0x5c, 0x43, 0x4d, 0x65, 0x6b,
  0x32, 0x3e, 0x78, 0x01, 0x77, 0x79, 0x53, 0x5d, 0x11, 0x1f,
  0x64, 0x6c, 0x42, 0x4e, 0x76, 0x7a, 0x21, 0x2f, 0x75, 0x7b,
  0x31, 0x3f, 0x63, 0x6d, 0x52, 0x5e, 0x00, 0x74, 0x7c, 0x41,
  0x4f, 0x10, 0x20, 0x62, 0x6e, 0x30, 0x73, 0x7d, 0x51, 0x5f,
  0x40, 0x72, 0x7e, 0x61, 0x6f, 0x50, 0x71, 0x7f, 0x60, 0x70
};

#define FIXED_TABLE_SIZE (630 * 3 + 410)
static const uint16_t kTableSize[12] = {
  FIXED_TABLE_SIZE + 654,
  FIXED_TABLE_SIZE + 656,
  FIXED_TABLE_SIZE + 658,
  FIXED_TABLE_SIZE + 662,
  FIXED_TABLE_SIZE + 670,
  FIXED_TABLE_SIZE + 686,
  FIXED_TABLE_SIZE + 718,
  FIXED_TABLE_SIZE + 782,
  FIXED_TABLE_SIZE + 912,
  FIXED_TABLE_SIZE + 1168,
  FIXED_TABLE_SIZE + 1680,
  FIXED_TABLE_SIZE + 2704
};

static int VP8LSetError(VP8LDecoder* const dec, VP8StatusCode error) {

  if (dec->status == VP8_STATUS_OK || dec->status == VP8_STATUS_SUSPENDED) {
    dec->status = error;
  }
  return 0;
}

static int DecodeImageStream(int xsize, int ysize,
                             int is_level0,
                             VP8LDecoder* const dec,
                             uint32_t** const decoded_data);

int VP8LCheckSignature(const uint8_t* const data, size_t size) {
  return (size >= VP8L_FRAME_HEADER_SIZE &&
          data[0] == VP8L_MAGIC_BYTE &&
          (data[4] >> 5) == 0);
}

static int ReadImageInfo(VP8LBitReader* const br,
                         int* const width, int* const height,
                         int* const has_alpha) {
  if (VP8LReadBits(br, 8) != VP8L_MAGIC_BYTE) return 0;
  *width = VP8LReadBits(br, VP8L_IMAGE_SIZE_BITS) + 1;
  *height = VP8LReadBits(br, VP8L_IMAGE_SIZE_BITS) + 1;
  *has_alpha = VP8LReadBits(br, 1);
  if (VP8LReadBits(br, VP8L_VERSION_BITS) != 0) return 0;
  return !br->eos;
}

int VP8LGetInfo(const uint8_t* data, size_t data_size,
                int* const width, int* const height, int* const has_alpha) {
  if (data == NULL || data_size < VP8L_FRAME_HEADER_SIZE) {
    return 0;
  } else if (!VP8LCheckSignature(data, data_size)) {
    return 0;
  } else {
    int w, h, a;
    VP8LBitReader br;
    VP8LInitBitReader(&br, data, data_size);
    if (!ReadImageInfo(&br, &w, &h, &a)) {
      return 0;
    }
    if (width != NULL) *width = w;
    if (height != NULL) *height = h;
    if (has_alpha != NULL) *has_alpha = a;
    return 1;
  }
}

static WEBP_INLINE int GetCopyDistance(int distance_symbol,
                                       VP8LBitReader* const br) {
  int extra_bits, offset;
  if (distance_symbol < 4) {
    return distance_symbol + 1;
  }
  extra_bits = (distance_symbol - 2) >> 1;
  offset = (2 + (distance_symbol & 1)) << extra_bits;
  return offset + VP8LReadBits(br, extra_bits) + 1;
}

static WEBP_INLINE int GetCopyLength(int length_symbol,
                                     VP8LBitReader* const br) {

  return GetCopyDistance(length_symbol, br);
}

static WEBP_INLINE int PlaneCodeToDistance(int xsize, int plane_code) {
  if (plane_code > CODE_TO_PLANE_CODES) {
    return plane_code - CODE_TO_PLANE_CODES;
  } else {
    const int dist_code = kCodeToPlane[plane_code - 1];
    const int yoffset = dist_code >> 4;
    const int xoffset = 8 - (dist_code & 0xf);
    const int dist = yoffset * xsize + xoffset;
    return (dist >= 1) ? dist : 1;
  }
}

static WEBP_INLINE int ReadSymbol(const HuffmanCode* table,
                                  VP8LBitReader* const br) {
  int nbits;
  uint32_t val = VP8LPrefetchBits(br);
  table += val & HUFFMAN_TABLE_MASK;
  nbits = table->bits - HUFFMAN_TABLE_BITS;
  if (nbits > 0) {
    VP8LSetBitPos(br, br->bit_pos + HUFFMAN_TABLE_BITS);
    val = VP8LPrefetchBits(br);
    table += table->value;
    table += val & ((1 << nbits) - 1);
  }
  VP8LSetBitPos(br, br->bit_pos + table->bits);
  return table->value;
}

#define BITS_SPECIAL_MARKER 0x100
#define PACKED_NON_LITERAL_CODE 0
static WEBP_INLINE int ReadPackedSymbols(const HTreeGroup* group,
                                         VP8LBitReader* const br,
                                         uint32_t* const dst) {
  const uint32_t val = VP8LPrefetchBits(br) & (HUFFMAN_PACKED_TABLE_SIZE - 1);
  const HuffmanCode32 code = group->packed_table[val];
  assert(group->use_packed_table);
  if (code.bits < BITS_SPECIAL_MARKER) {
    VP8LSetBitPos(br, br->bit_pos + code.bits);
    *dst = code.value;
    return PACKED_NON_LITERAL_CODE;
  } else {
    VP8LSetBitPos(br, br->bit_pos + code.bits - BITS_SPECIAL_MARKER);
    assert(code.value >= NUM_LITERAL_CODES);
    return code.value;
  }
}

static int AccumulateHCode(HuffmanCode hcode, int shift,
                           HuffmanCode32* const huff) {
  huff->bits += hcode.bits;
  huff->value |= (uint32_t)hcode.value << shift;
  assert(huff->bits <= HUFFMAN_TABLE_BITS);
  return hcode.bits;
}

static void BuildPackedTable(HTreeGroup* const htree_group) {
  uint32_t code;
  for (code = 0; code < HUFFMAN_PACKED_TABLE_SIZE; ++code) {
    uint32_t bits = code;
    HuffmanCode32* const huff = &htree_group->packed_table[bits];
    HuffmanCode hcode = htree_group->htrees[GREEN][bits];
    if (hcode.value >= NUM_LITERAL_CODES) {
      huff->bits = hcode.bits + BITS_SPECIAL_MARKER;
      huff->value = hcode.value;
    } else {
      huff->bits = 0;
      huff->value = 0;
      bits >>= AccumulateHCode(hcode, 8, huff);
      bits >>= AccumulateHCode(htree_group->htrees[RED][bits], 16, huff);
      bits >>= AccumulateHCode(htree_group->htrees[BLUE][bits], 0, huff);
      bits >>= AccumulateHCode(htree_group->htrees[ALPHA][bits], 24, huff);
      (void)bits;
    }
  }
}

static int ReadHuffmanCodeLengths(
    VP8LDecoder* const dec, const int* const code_length_code_lengths,
    int num_symbols, int* const code_lengths) {
  int ok = 0;
  VP8LBitReader* const br = &dec->br;
  int symbol;
  int max_symbol;
  int prev_code_len = DEFAULT_CODE_LENGTH;
  HuffmanTables tables;

  if (!VP8LHuffmanTablesAllocate(1 << LENGTHS_TABLE_BITS, &tables) ||
      !VP8LBuildHuffmanTable(&tables, LENGTHS_TABLE_BITS,
                             code_length_code_lengths, NUM_CODE_LENGTH_CODES)) {
    goto End;
  }

  if (VP8LReadBits(br, 1)) {
    const int length_nbits = 2 + 2 * VP8LReadBits(br, 3);
    max_symbol = 2 + VP8LReadBits(br, length_nbits);
    if (max_symbol > num_symbols) {
      goto End;
    }
  } else {
    max_symbol = num_symbols;
  }

  symbol = 0;
  while (symbol < num_symbols) {
    const HuffmanCode* p;
    int code_len;
    if (max_symbol-- == 0) break;
    VP8LFillBitWindow(br);
    p = &tables.curr_segment->start[VP8LPrefetchBits(br) & LENGTHS_TABLE_MASK];
    VP8LSetBitPos(br, br->bit_pos + p->bits);
    code_len = p->value;
    if (code_len < kCodeLengthLiterals) {
      code_lengths[symbol++] = code_len;
      if (code_len != 0) prev_code_len = code_len;
    } else {
      const int use_prev = (code_len == kCodeLengthRepeatCode);
      const int slot = code_len - kCodeLengthLiterals;
      const int extra_bits = kCodeLengthExtraBits[slot];
      const int repeat_offset = kCodeLengthRepeatOffsets[slot];
      int repeat = VP8LReadBits(br, extra_bits) + repeat_offset;
      if (symbol + repeat > num_symbols) {
        goto End;
      } else {
        const int length = use_prev ? prev_code_len : 0;
        while (repeat-- > 0) code_lengths[symbol++] = length;
      }
    }
  }
  ok = 1;

 End:
  VP8LHuffmanTablesDeallocate(&tables);
  if (!ok) return VP8LSetError(dec, VP8_STATUS_BITSTREAM_ERROR);
  return ok;
}

static int ReadHuffmanCode(int alphabet_size, VP8LDecoder* const dec,
                           int* const code_lengths,
                           HuffmanTables* const table) {
  int ok = 0;
  int size = 0;
  VP8LBitReader* const br = &dec->br;
  const int simple_code = VP8LReadBits(br, 1);

  memset(code_lengths, 0, alphabet_size * sizeof(*code_lengths));

  if (simple_code) {
    const int num_symbols = VP8LReadBits(br, 1) + 1;
    const int first_symbol_len_code = VP8LReadBits(br, 1);

    int symbol = VP8LReadBits(br, (first_symbol_len_code == 0) ? 1 : 8);
    code_lengths[symbol] = 1;

    if (num_symbols == 2) {
      symbol = VP8LReadBits(br, 8);
      code_lengths[symbol] = 1;
    }
    ok = 1;
  } else {
    int i;
    int code_length_code_lengths[NUM_CODE_LENGTH_CODES] = { 0 };
    const int num_codes = VP8LReadBits(br, 4) + 4;
    assert(num_codes <= NUM_CODE_LENGTH_CODES);

    for (i = 0; i < num_codes; ++i) {
      code_length_code_lengths[kCodeLengthCodeOrder[i]] = VP8LReadBits(br, 3);
    }
    ok = ReadHuffmanCodeLengths(dec, code_length_code_lengths, alphabet_size,
                                code_lengths);
  }

  ok = ok && !br->eos;
  if (ok) {
    size = VP8LBuildHuffmanTable(table, HUFFMAN_TABLE_BITS,
                                 code_lengths, alphabet_size);
  }
  if (!ok || size == 0) {
    return VP8LSetError(dec, VP8_STATUS_BITSTREAM_ERROR);
  }
  return size;
}

static int ReadHuffmanCodes(VP8LDecoder* const dec, int xsize, int ysize,
                            int color_cache_bits, int allow_recursion) {
  int i;
  VP8LBitReader* const br = &dec->br;
  VP8LMetadata* const hdr = &dec->hdr;
  uint32_t* huffman_image = NULL;
  HTreeGroup* htree_groups = NULL;
  HuffmanTables* huffman_tables = &hdr->huffman_tables;
  int num_htree_groups = 1;
  int num_htree_groups_max = 1;
  int* mapping = NULL;
  int ok = 0;

  assert(huffman_tables->root.start == NULL);
  assert(huffman_tables->curr_segment == NULL);

  if (allow_recursion && VP8LReadBits(br, 1)) {

    const int huffman_precision =
        MIN_HUFFMAN_BITS + VP8LReadBits(br, NUM_HUFFMAN_BITS);
    const int huffman_xsize = VP8LSubSampleSize(xsize, huffman_precision);
    const int huffman_ysize = VP8LSubSampleSize(ysize, huffman_precision);
    const int huffman_pixs = huffman_xsize * huffman_ysize;
    if (!DecodeImageStream(huffman_xsize, huffman_ysize, 0, dec,
                           &huffman_image)) {
      goto Error;
    }
    hdr->huffman_subsample_bits = huffman_precision;
    for (i = 0; i < huffman_pixs; ++i) {

      const int group = (huffman_image[i] >> 8) & 0xffff;
      huffman_image[i] = group;
      if (group >= num_htree_groups_max) {
        num_htree_groups_max = group + 1;
      }
    }

    if (num_htree_groups_max > 1000 || num_htree_groups_max > xsize * ysize) {

      mapping = (int*)WebPSafeMalloc(num_htree_groups_max, sizeof(*mapping));
      if (mapping == NULL) {
        VP8LSetError(dec, VP8_STATUS_OUT_OF_MEMORY);
        goto Error;
      }

      memset(mapping, 0xff, num_htree_groups_max * sizeof(*mapping));
      for (num_htree_groups = 0, i = 0; i < huffman_pixs; ++i) {

        int* const mapped_group = &mapping[huffman_image[i]];
        if (*mapped_group == -1) *mapped_group = num_htree_groups++;
        huffman_image[i] = *mapped_group;
      }
    } else {
      num_htree_groups = num_htree_groups_max;
    }
  }

  if (br->eos) goto Error;

  if (!ReadHuffmanCodesHelper(color_cache_bits, num_htree_groups,
                              num_htree_groups_max, mapping, dec,
                              huffman_tables, &htree_groups)) {
    goto Error;
  }
  ok = 1;

  hdr->huffman_image = huffman_image;
  hdr->num_htree_groups = num_htree_groups;
  hdr->htree_groups = htree_groups;

 Error:
  WebPSafeFree(mapping);
  if (!ok) {
    WebPSafeFree(huffman_image);
    VP8LHuffmanTablesDeallocate(huffman_tables);
    VP8LHtreeGroupsFree(htree_groups);
  }
  return ok;
}

int ReadHuffmanCodesHelper(int color_cache_bits, int num_htree_groups,
                           int num_htree_groups_max, const int* const mapping,
                           VP8LDecoder* const dec,
                           HuffmanTables* const huffman_tables,
                           HTreeGroup** const htree_groups) {
  int i, j, ok = 0;
  const int max_alphabet_size =
      kAlphabetSize[0] + ((color_cache_bits > 0) ? 1 << color_cache_bits : 0);
  const int table_size = kTableSize[color_cache_bits];
  int* code_lengths = NULL;

  if ((mapping == NULL && num_htree_groups != num_htree_groups_max) ||
      num_htree_groups > num_htree_groups_max) {
    goto Error;
  }

  code_lengths =
      (int*)WebPSafeCalloc((uint64_t)max_alphabet_size, sizeof(*code_lengths));
  *htree_groups = VP8LHtreeGroupsNew(num_htree_groups);

  if (*htree_groups == NULL || code_lengths == NULL ||
      !VP8LHuffmanTablesAllocate(num_htree_groups * table_size,
                                 huffman_tables)) {
    VP8LSetError(dec, VP8_STATUS_OUT_OF_MEMORY);
    goto Error;
  }

  for (i = 0; i < num_htree_groups_max; ++i) {

    if (mapping != NULL && mapping[i] == -1) {
      for (j = 0; j < HUFFMAN_CODES_PER_META_CODE; ++j) {
        int alphabet_size = kAlphabetSize[j];
        if (j == 0 && color_cache_bits > 0) {
          alphabet_size += (1 << color_cache_bits);
        }

        if (!ReadHuffmanCode(alphabet_size, dec, code_lengths, NULL)) {
          goto Error;
        }
      }
    } else {
      HTreeGroup* const htree_group =
          &(*htree_groups)[(mapping == NULL) ? i : mapping[i]];
      HuffmanCode** const htrees = htree_group->htrees;
      int size;
      int total_size = 0;
      int is_trivial_literal = 1;
      int max_bits = 0;
      for (j = 0; j < HUFFMAN_CODES_PER_META_CODE; ++j) {
        int alphabet_size = kAlphabetSize[j];
        if (j == 0 && color_cache_bits > 0) {
          alphabet_size += (1 << color_cache_bits);
        }
        size =
            ReadHuffmanCode(alphabet_size, dec, code_lengths, huffman_tables);
        htrees[j] = huffman_tables->curr_segment->curr_table;
        if (size == 0) {
          goto Error;
        }
        if (is_trivial_literal && kLiteralMap[j] == 1) {
          is_trivial_literal = (htrees[j]->bits == 0);
        }
        total_size += htrees[j]->bits;
        huffman_tables->curr_segment->curr_table += size;
        if (j <= ALPHA) {
          int local_max_bits = code_lengths[0];
          int k;
          for (k = 1; k < alphabet_size; ++k) {
            if (code_lengths[k] > local_max_bits) {
              local_max_bits = code_lengths[k];
            }
          }
          max_bits += local_max_bits;
        }
      }
      htree_group->is_trivial_literal = is_trivial_literal;
      htree_group->is_trivial_code = 0;
      if (is_trivial_literal) {
        const int red = htrees[RED][0].value;
        const int blue = htrees[BLUE][0].value;
        const int alpha = htrees[ALPHA][0].value;
        htree_group->literal_arb = ((uint32_t)alpha << 24) | (red << 16) | blue;
        if (total_size == 0 && htrees[GREEN][0].value < NUM_LITERAL_CODES) {
          htree_group->is_trivial_code = 1;
          htree_group->literal_arb |= htrees[GREEN][0].value << 8;
        }
      }
      htree_group->use_packed_table =
          !htree_group->is_trivial_code && (max_bits < HUFFMAN_PACKED_BITS);
      if (htree_group->use_packed_table) BuildPackedTable(htree_group);
    }
  }
  ok = 1;

 Error:
  WebPSafeFree(code_lengths);
  if (!ok) {
    VP8LHuffmanTablesDeallocate(huffman_tables);
    VP8LHtreeGroupsFree(*htree_groups);
    *htree_groups = NULL;
  }
  return ok;
}

#if !defined(WEBP_REDUCE_SIZE)
static int AllocateAndInitRescaler(VP8LDecoder* const dec, VP8Io* const io) {
  const int num_channels = 4;
  const int in_width = io->mb_w;
  const int out_width = io->scaled_width;
  const int in_height = io->mb_h;
  const int out_height = io->scaled_height;
  const uint64_t work_size = 2 * num_channels * (uint64_t)out_width;
  rescaler_t* work;
  const uint64_t scaled_data_size = (uint64_t)out_width;
  uint32_t* scaled_data;
  const uint64_t memory_size = sizeof(*dec->rescaler) +
                               work_size * sizeof(*work) +
                               scaled_data_size * sizeof(*scaled_data);
  uint8_t* memory = (uint8_t*)WebPSafeMalloc(memory_size, sizeof(*memory));
  if (memory == NULL) {
    return VP8LSetError(dec, VP8_STATUS_OUT_OF_MEMORY);
  }
  assert(dec->rescaler_memory == NULL);
  dec->rescaler_memory = memory;

  dec->rescaler = (WebPRescaler*)memory;
  memory += sizeof(*dec->rescaler);
  work = (rescaler_t*)memory;
  memory += work_size * sizeof(*work);
  scaled_data = (uint32_t*)memory;

  if (!WebPRescalerInit(dec->rescaler, in_width, in_height,
                        (uint8_t*)scaled_data, out_width, out_height,
                        0, num_channels, work)) {
    return 0;
  }
  return 1;
}
#endif

#if !defined(WEBP_REDUCE_SIZE)

static int Export(WebPRescaler* const rescaler, WEBP_CSP_MODE colorspace,
                  int rgba_stride, uint8_t* const rgba) {
  uint32_t* const src = (uint32_t*)rescaler->dst;
  uint8_t* dst = rgba;
  const int dst_width = rescaler->dst_width;
  int num_lines_out = 0;
  while (WebPRescalerHasPendingOutput(rescaler)) {
    WebPRescalerExportRow(rescaler);
    WebPMultARGBRow(src, dst_width, 1);
    VP8LConvertFromBGRA(src, dst_width, colorspace, dst);
    dst += rgba_stride;
    ++num_lines_out;
  }
  return num_lines_out;
}

static int EmitRescaledRowsRGBA(const VP8LDecoder* const dec,
                                uint8_t* in, int in_stride, int mb_h,
                                uint8_t* const out, int out_stride) {
  const WEBP_CSP_MODE colorspace = dec->output->colorspace;
  int num_lines_in = 0;
  int num_lines_out = 0;
  while (num_lines_in < mb_h) {
    uint8_t* const row_in = in + (ptrdiff_t)num_lines_in * in_stride;
    uint8_t* const row_out = out + (ptrdiff_t)num_lines_out * out_stride;
    const int lines_left = mb_h - num_lines_in;
    const int needed_lines = WebPRescaleNeededLines(dec->rescaler, lines_left);
    int lines_imported;
    assert(needed_lines > 0 && needed_lines <= lines_left);
    WebPMultARGBRows(row_in, in_stride,
                     dec->rescaler->src_width, needed_lines, 0);
    lines_imported =
        WebPRescalerImport(dec->rescaler, lines_left, row_in, in_stride);
    assert(lines_imported == needed_lines);
    num_lines_in += lines_imported;
    num_lines_out += Export(dec->rescaler, colorspace, out_stride, row_out);
  }
  return num_lines_out;
}

#endif

static int EmitRows(WEBP_CSP_MODE colorspace,
                    const uint8_t* row_in, int in_stride,
                    int mb_w, int mb_h,
                    uint8_t* const out, int out_stride) {
  int lines = mb_h;
  uint8_t* row_out = out;
  while (lines-- > 0) {
    VP8LConvertFromBGRA((const uint32_t*)row_in, mb_w, colorspace, row_out);
    row_in += in_stride;
    row_out += out_stride;
  }
  return mb_h;
}

static void ConvertToYUVA(const uint32_t* const src, int width, int y_pos,
                          const WebPDecBuffer* const output) {
  const WebPYUVABuffer* const buf = &output->u.YUVA;

  WebPConvertARGBToY(src, buf->y + y_pos * buf->y_stride, width);

  {
    uint8_t* const u = buf->u + (y_pos >> 1) * buf->u_stride;
    uint8_t* const v = buf->v + (y_pos >> 1) * buf->v_stride;

    WebPConvertARGBToUV(src, u, v, width, !(y_pos & 1));
  }

  if (buf->a != NULL) {
    uint8_t* const a = buf->a + y_pos * buf->a_stride;
#if defined(WORDS_BIGENDIAN)
    WebPExtractAlpha((uint8_t*)src + 0, 0, width, 1, a, 0);
#else
    WebPExtractAlpha((uint8_t*)src + 3, 0, width, 1, a, 0);
#endif
  }
}

static int ExportYUVA(const VP8LDecoder* const dec, int y_pos) {
  WebPRescaler* const rescaler = dec->rescaler;
  uint32_t* const src = (uint32_t*)rescaler->dst;
  const int dst_width = rescaler->dst_width;
  int num_lines_out = 0;
  while (WebPRescalerHasPendingOutput(rescaler)) {
    WebPRescalerExportRow(rescaler);
    WebPMultARGBRow(src, dst_width, 1);
    ConvertToYUVA(src, dst_width, y_pos, dec->output);
    ++y_pos;
    ++num_lines_out;
  }
  return num_lines_out;
}

static int EmitRescaledRowsYUVA(const VP8LDecoder* const dec,
                                uint8_t* in, int in_stride, int mb_h) {
  int num_lines_in = 0;
  int y_pos = dec->last_out_row;
  while (num_lines_in < mb_h) {
    const int lines_left = mb_h - num_lines_in;
    const int needed_lines = WebPRescaleNeededLines(dec->rescaler, lines_left);
    int lines_imported;
    WebPMultARGBRows(in, in_stride, dec->rescaler->src_width, needed_lines, 0);
    lines_imported =
        WebPRescalerImport(dec->rescaler, lines_left, in, in_stride);
    assert(lines_imported == needed_lines);
    num_lines_in += lines_imported;
    in += needed_lines * in_stride;
    y_pos += ExportYUVA(dec, y_pos);
  }
  return y_pos;
}

static int EmitRowsYUVA(const VP8LDecoder* const dec,
                        const uint8_t* in, int in_stride,
                        int mb_w, int num_rows) {
  int y_pos = dec->last_out_row;
  while (num_rows-- > 0) {
    ConvertToYUVA((const uint32_t*)in, mb_w, y_pos, dec->output);
    in += in_stride;
    ++y_pos;
  }
  return y_pos;
}

static int SetCropWindow(VP8Io* const io, int y_start, int y_end,
                         uint8_t** const in_data, int pixel_stride) {
  assert(y_start < y_end);
  assert(io->crop_left < io->crop_right);
  if (y_end > io->crop_bottom) {
    y_end = io->crop_bottom;
  }
  if (y_start < io->crop_top) {
    const int delta = io->crop_top - y_start;
    y_start = io->crop_top;
    *in_data += delta * pixel_stride;
  }
  if (y_start >= y_end) return 0;

  *in_data += io->crop_left * sizeof(uint32_t);

  io->mb_y = y_start - io->crop_top;
  io->mb_w = io->crop_right - io->crop_left;
  io->mb_h = y_end - y_start;
  return 1;
}

static WEBP_INLINE int GetMetaIndex(
    const uint32_t* const image, int xsize, int bits, int x, int y) {
  if (bits == 0) return 0;
  return image[xsize * (y >> bits) + (x >> bits)];
}

static WEBP_INLINE HTreeGroup* GetHtreeGroupForPos(VP8LMetadata* const hdr,
                                                   int x, int y) {
  const int meta_index = GetMetaIndex(hdr->huffman_image, hdr->huffman_xsize,
                                      hdr->huffman_subsample_bits, x, y);
  assert(meta_index < hdr->num_htree_groups);
  return hdr->htree_groups + meta_index;
}

typedef void (*ProcessRowsFunc)(VP8LDecoder* const dec, int row);

static void ApplyInverseTransforms(VP8LDecoder* const dec,
                                   int start_row, int num_rows,
                                   const uint32_t* const rows) {
  int n = dec->next_transform;
  const int cache_pixs = dec->width * num_rows;
  const int end_row = start_row + num_rows;
  const uint32_t* rows_in = rows;
  uint32_t* const rows_out = dec->argb_cache;

  while (n-- > 0) {
    VP8LTransform* const transform = &dec->transforms[n];
    VP8LInverseTransform(transform, start_row, end_row, rows_in, rows_out);
    rows_in = rows_out;
  }
  if (rows_in != rows_out) {

    memcpy(rows_out, rows_in, cache_pixs * sizeof(*rows_out));
  }
}

static void ProcessRows(VP8LDecoder* const dec, int row) {
  const uint32_t* const rows = dec->pixels + dec->width * dec->last_row;
  const int num_rows = row - dec->last_row;

  assert(row <= dec->io->crop_bottom);

  assert(num_rows <= NUM_ARGB_CACHE_ROWS);
  if (num_rows > 0) {
    VP8Io* const io = dec->io;
    uint8_t* rows_data = (uint8_t*)dec->argb_cache;
    const int in_stride = io->width * sizeof(uint32_t);
    ApplyInverseTransforms(dec, dec->last_row, num_rows, rows);
    if (!SetCropWindow(io, dec->last_row, row, &rows_data, in_stride)) {

    } else {
      const WebPDecBuffer* const output = dec->output;
      if (WebPIsRGBMode(output->colorspace)) {
        const WebPRGBABuffer* const buf = &output->u.RGBA;
        uint8_t* const rgba =
            buf->rgba + (ptrdiff_t)dec->last_out_row * buf->stride;
        const int num_rows_out =
#if !defined(WEBP_REDUCE_SIZE)
         io->use_scaling ?
            EmitRescaledRowsRGBA(dec, rows_data, in_stride, io->mb_h,
                                 rgba, buf->stride) :
#endif
            EmitRows(output->colorspace, rows_data, in_stride,
                     io->mb_w, io->mb_h, rgba, buf->stride);

        dec->last_out_row += num_rows_out;
      } else {
        dec->last_out_row = io->use_scaling ?
            EmitRescaledRowsYUVA(dec, rows_data, in_stride, io->mb_h) :
            EmitRowsYUVA(dec, rows_data, in_stride, io->mb_w, io->mb_h);
      }
      assert(dec->last_out_row <= output->height);
    }
  }

  dec->last_row = row;
  assert(dec->last_row <= dec->height);
}

static int Is8bOptimizable(const VP8LMetadata* const hdr) {
  int i;
  if (hdr->color_cache_size > 0) return 0;

  for (i = 0; i < hdr->num_htree_groups; ++i) {
    HuffmanCode** const htrees = hdr->htree_groups[i].htrees;
    if (htrees[RED][0].bits > 0) return 0;
    if (htrees[BLUE][0].bits > 0) return 0;
    if (htrees[ALPHA][0].bits > 0) return 0;
  }
  return 1;
}

static void AlphaApplyFilter(ALPHDecoder* const alph_dec,
                             int first_row, int last_row,
                             uint8_t* out, int stride) {
  if (alph_dec->filter != WEBP_FILTER_NONE) {
    int y;
    const uint8_t* prev_line = alph_dec->prev_line;
    assert(WebPUnfilters[alph_dec->filter] != NULL);
    for (y = first_row; y < last_row; ++y) {
      WebPUnfilters[alph_dec->filter](prev_line, out, out, stride);
      prev_line = out;
      out += stride;
    }
    alph_dec->prev_line = prev_line;
  }
}

static void ExtractPalettedAlphaRows(VP8LDecoder* const dec, int last_row) {

  ALPHDecoder* const alph_dec = (ALPHDecoder*)dec->io->opaque;
  const int top_row =
      (alph_dec->filter == WEBP_FILTER_NONE ||
       alph_dec->filter == WEBP_FILTER_HORIZONTAL) ? dec->io->crop_top
                                                    : dec->last_row;
  const int first_row = (dec->last_row < top_row) ? top_row : dec->last_row;
  assert(last_row <= dec->io->crop_bottom);
  if (last_row > first_row) {

    const int width = dec->io->width;
    uint8_t* out = alph_dec->output + width * first_row;
    const uint8_t* const in =
      (uint8_t*)dec->pixels + dec->width * first_row;
    VP8LTransform* const transform = &dec->transforms[0];
    assert(dec->next_transform == 1);
    assert(transform->type == COLOR_INDEXING_TRANSFORM);
    VP8LColorIndexInverseTransformAlpha(transform, first_row, last_row,
                                        in, out);
    AlphaApplyFilter(alph_dec, first_row, last_row, out, width);
  }
  dec->last_row = dec->last_out_row = last_row;
}

static WEBP_INLINE uint32_t Rotate8b(uint32_t V) {
#if defined(WORDS_BIGENDIAN)
  return ((V & 0xff000000u) >> 24) | (V << 8);
#else
  return ((V & 0xffu) << 24) | (V >> 8);
#endif
}

static WEBP_INLINE void CopySmallPattern8b(const uint8_t* src, uint8_t* dst,
                                           int length, uint32_t pattern) {
  int i;

  while ((uintptr_t)dst & 3) {
    *dst++ = *src++;
    pattern = Rotate8b(pattern);
    --length;
  }

  for (i = 0; i < (length >> 2); ++i) {
    ((uint32_t*)dst)[i] = pattern;
  }

  for (i <<= 2; i < length; ++i) {
    dst[i] = src[i];
  }
}

static WEBP_INLINE void CopyBlock8b(uint8_t* const dst, int dist, int length) {
  const uint8_t* src = dst - dist;
  if (length >= 8) {
    uint32_t pattern = 0;
    switch (dist) {
      case 1:
        pattern = src[0];
#if defined(__arm__) || defined(_M_ARM)
        pattern |= pattern << 8;
        pattern |= pattern << 16;
#elif defined(WEBP_USE_MIPS_DSP_R2)
        __asm__ volatile ("replv.qb %0, %0" : "+r"(pattern));
#else
        pattern = 0x01010101u * pattern;
#endif
        break;
      case 2:
#if !defined(WORDS_BIGENDIAN)
        memcpy(&pattern, src, sizeof(uint16_t));
#else
        pattern = ((uint32_t)src[0] << 8) | src[1];
#endif
#if defined(__arm__) || defined(_M_ARM)
        pattern |= pattern << 16;
#elif defined(WEBP_USE_MIPS_DSP_R2)
        __asm__ volatile ("replv.ph %0, %0" : "+r"(pattern));
#else
        pattern = 0x00010001u * pattern;
#endif
        break;
      case 4:
        memcpy(&pattern, src, sizeof(uint32_t));
        break;
      default:
        goto Copy;
    }
    CopySmallPattern8b(src, dst, length, pattern);
    return;
  }
 Copy:
  if (dist >= length) {
    memcpy(dst, src, length * sizeof(*dst));
  } else {
    int i;
    for (i = 0; i < length; ++i) dst[i] = src[i];
  }
}

static WEBP_INLINE void CopySmallPattern32b(const uint32_t* src,
                                            uint32_t* dst,
                                            int length, uint64_t pattern) {
  int i;
  if ((uintptr_t)dst & 4) {
    *dst++ = *src++;
    pattern = (pattern >> 32) | (pattern << 32);
    --length;
  }
  assert(0 == ((uintptr_t)dst & 7));
  for (i = 0; i < (length >> 1); ++i) {
    ((uint64_t*)dst)[i] = pattern;
  }
  if (length & 1) {
    dst[i << 1] = src[i << 1];
  }
}

static WEBP_INLINE void CopyBlock32b(uint32_t* const dst,
                                     int dist, int length) {
  const uint32_t* const src = dst - dist;
  if (dist <= 2 && length >= 4 && ((uintptr_t)dst & 3) == 0) {
    uint64_t pattern;
    if (dist == 1) {
      pattern = (uint64_t)src[0];
      pattern |= pattern << 32;
    } else {
      memcpy(&pattern, src, sizeof(pattern));
    }
    CopySmallPattern32b(src, dst, length, pattern);
  } else if (dist >= length) {
    memcpy(dst, src, length * sizeof(*dst));
  } else {
    int i;
    for (i = 0; i < length; ++i) dst[i] = src[i];
  }
}

static int DecodeAlphaData(VP8LDecoder* const dec, uint8_t* const data,
                           int width, int height, int last_row) {
  int ok = 1;
  int row = dec->last_pixel / width;
  int col = dec->last_pixel % width;
  VP8LBitReader* const br = &dec->br;
  VP8LMetadata* const hdr = &dec->hdr;
  int pos = dec->last_pixel;
  const int end = width * height;
  const int last = width * last_row;
  const int len_code_limit = NUM_LITERAL_CODES + NUM_LENGTH_CODES;
  const int mask = hdr->huffman_mask;
  const HTreeGroup* htree_group =
      (pos < last) ? GetHtreeGroupForPos(hdr, col, row) : NULL;
  assert(pos <= end);
  assert(last_row <= height);
  assert(Is8bOptimizable(hdr));

  while (!br->eos && pos < last) {
    int code;

    if ((col & mask) == 0) {
      htree_group = GetHtreeGroupForPos(hdr, col, row);
    }
    assert(htree_group != NULL);
    VP8LFillBitWindow(br);
    code = ReadSymbol(htree_group->htrees[GREEN], br);
    if (code < NUM_LITERAL_CODES) {
      data[pos] = code;
      ++pos;
      ++col;
      if (col >= width) {
        col = 0;
        ++row;
        if (row <= last_row && (row % NUM_ARGB_CACHE_ROWS == 0)) {
          ExtractPalettedAlphaRows(dec, row);
        }
      }
    } else if (code < len_code_limit) {
      int dist_code, dist;
      const int length_sym = code - NUM_LITERAL_CODES;
      const int length = GetCopyLength(length_sym, br);
      const int dist_symbol = ReadSymbol(htree_group->htrees[DIST], br);
      VP8LFillBitWindow(br);
      dist_code = GetCopyDistance(dist_symbol, br);
      dist = PlaneCodeToDistance(width, dist_code);
      if (pos >= dist && end - pos >= length) {
        CopyBlock8b(data + pos, dist, length);
      } else {
        ok = 0;
        goto End;
      }
      pos += length;
      col += length;
      while (col >= width) {
        col -= width;
        ++row;
        if (row <= last_row && (row % NUM_ARGB_CACHE_ROWS == 0)) {
          ExtractPalettedAlphaRows(dec, row);
        }
      }
      if (pos < last && (col & mask)) {
        htree_group = GetHtreeGroupForPos(hdr, col, row);
      }
    } else {
      ok = 0;
      goto End;
    }
    br->eos = VP8LIsEndOfStream(br);
  }

  ExtractPalettedAlphaRows(dec, row > last_row ? last_row : row);

 End:
  br->eos = VP8LIsEndOfStream(br);
  if (!ok || (br->eos && pos < end)) {
    return VP8LSetError(
        dec, br->eos ? VP8_STATUS_SUSPENDED : VP8_STATUS_BITSTREAM_ERROR);
  }
  dec->last_pixel = pos;
  return ok;
}

static void SaveState(VP8LDecoder* const dec, int last_pixel) {
  assert(dec->incremental);
  dec->saved_br = dec->br;
  dec->saved_last_pixel = last_pixel;
  if (dec->hdr.color_cache_size > 0) {
    VP8LColorCacheCopy(&dec->hdr.color_cache, &dec->hdr.saved_color_cache);
  }
}

static void RestoreState(VP8LDecoder* const dec) {
  assert(dec->br.eos);
  dec->status = VP8_STATUS_SUSPENDED;
  dec->br = dec->saved_br;
  dec->last_pixel = dec->saved_last_pixel;
  if (dec->hdr.color_cache_size > 0) {
    VP8LColorCacheCopy(&dec->hdr.saved_color_cache, &dec->hdr.color_cache);
  }
}

#define SYNC_EVERY_N_ROWS 8
static int DecodeImageData(VP8LDecoder* const dec, uint32_t* const data,
                           int width, int height, int last_row,
                           ProcessRowsFunc process_func) {
  int row = dec->last_pixel / width;
  int col = dec->last_pixel % width;
  VP8LBitReader* const br = &dec->br;
  VP8LMetadata* const hdr = &dec->hdr;
  uint32_t* src = data + dec->last_pixel;
  uint32_t* last_cached = src;
  uint32_t* const src_end = data + width * height;
  uint32_t* const src_last = data + width * last_row;
  const int len_code_limit = NUM_LITERAL_CODES + NUM_LENGTH_CODES;
  const int color_cache_limit = len_code_limit + hdr->color_cache_size;
  int next_sync_row = dec->incremental ? row : 1 << 24;
  VP8LColorCache* const color_cache =
      (hdr->color_cache_size > 0) ? &hdr->color_cache : NULL;
  const int mask = hdr->huffman_mask;
  const HTreeGroup* htree_group =
      (src < src_last) ? GetHtreeGroupForPos(hdr, col, row) : NULL;
  assert(dec->last_row < last_row);
  assert(src_last <= src_end);

  while (src < src_last) {
    int code;
    if (row >= next_sync_row) {
      SaveState(dec, (int)(src - data));
      next_sync_row = row + SYNC_EVERY_N_ROWS;
    }

    if ((col & mask) == 0) {
      htree_group = GetHtreeGroupForPos(hdr, col, row);
    }
    assert(htree_group != NULL);
    if (htree_group->is_trivial_code) {
      *src = htree_group->literal_arb;
      goto AdvanceByOne;
    }
    VP8LFillBitWindow(br);
    if (htree_group->use_packed_table) {
      code = ReadPackedSymbols(htree_group, br, src);
      if (VP8LIsEndOfStream(br)) break;
      if (code == PACKED_NON_LITERAL_CODE) goto AdvanceByOne;
    } else {
      code = ReadSymbol(htree_group->htrees[GREEN], br);
    }
    if (VP8LIsEndOfStream(br)) break;
    if (code < NUM_LITERAL_CODES) {
      if (htree_group->is_trivial_literal) {
        *src = htree_group->literal_arb | (code << 8);
      } else {
        int red, blue, alpha;
        red = ReadSymbol(htree_group->htrees[RED], br);
        VP8LFillBitWindow(br);
        blue = ReadSymbol(htree_group->htrees[BLUE], br);
        alpha = ReadSymbol(htree_group->htrees[ALPHA], br);
        if (VP8LIsEndOfStream(br)) break;
        *src = ((uint32_t)alpha << 24) | (red << 16) | (code << 8) | blue;
      }
    AdvanceByOne:
      ++src;
      ++col;
      if (col >= width) {
        col = 0;
        ++row;
        if (process_func != NULL) {
          if (row <= last_row && (row % NUM_ARGB_CACHE_ROWS == 0)) {
            process_func(dec, row);
          }
        }
        if (color_cache != NULL) {
          while (last_cached < src) {
            VP8LColorCacheInsert(color_cache, *last_cached++);
          }
        }
      }
    } else if (code < len_code_limit) {
      int dist_code, dist;
      const int length_sym = code - NUM_LITERAL_CODES;
      const int length = GetCopyLength(length_sym, br);
      const int dist_symbol = ReadSymbol(htree_group->htrees[DIST], br);
      VP8LFillBitWindow(br);
      dist_code = GetCopyDistance(dist_symbol, br);
      dist = PlaneCodeToDistance(width, dist_code);

      if (VP8LIsEndOfStream(br)) break;
      if (src - data < (ptrdiff_t)dist || src_end - src < (ptrdiff_t)length) {
        goto Error;
      } else {
        CopyBlock32b(src, dist, length);
      }
      src += length;
      col += length;
      while (col >= width) {
        col -= width;
        ++row;
        if (process_func != NULL) {
          if (row <= last_row && (row % NUM_ARGB_CACHE_ROWS == 0)) {
            process_func(dec, row);
          }
        }
      }

      assert(src <= src_end);
      if (col & mask) htree_group = GetHtreeGroupForPos(hdr, col, row);
      if (color_cache != NULL) {
        while (last_cached < src) {
          VP8LColorCacheInsert(color_cache, *last_cached++);
        }
      }
    } else if (code < color_cache_limit) {
      const int key = code - len_code_limit;
      assert(color_cache != NULL);
      while (last_cached < src) {
        VP8LColorCacheInsert(color_cache, *last_cached++);
      }
      *src = VP8LColorCacheLookup(color_cache, key);
      goto AdvanceByOne;
    } else {
      goto Error;
    }
  }

  br->eos = VP8LIsEndOfStream(br);

  assert(!dec->incremental || (br->eos && src < src_last) || src >= src_last);
  if (dec->incremental && br->eos && src < src_last) {
    RestoreState(dec);
  } else if ((dec->incremental && src >= src_last) || !br->eos) {

    if (process_func != NULL) {
      process_func(dec, row > last_row ? last_row : row);
    }
    dec->status = VP8_STATUS_OK;
    dec->last_pixel = (int)(src - data);
  } else {

    goto Error;
  }
  return 1;

 Error:
  return VP8LSetError(dec, VP8_STATUS_BITSTREAM_ERROR);
}

static void ClearTransform(VP8LTransform* const transform) {
  WebPSafeFree(transform->data);
  transform->data = NULL;
}

static int ExpandColorMap(int num_colors, VP8LTransform* const transform) {
  int i;
  const int final_num_colors = 1 << (8 >> transform->bits);
  uint32_t* const new_color_map =
      (uint32_t*)WebPSafeMalloc((uint64_t)final_num_colors,
                                sizeof(*new_color_map));
  if (new_color_map == NULL) {
    return 0;
  } else {
    uint8_t* const data = (uint8_t*)transform->data;
    uint8_t* const new_data = (uint8_t*)new_color_map;
    new_color_map[0] = transform->data[0];
    for (i = 4; i < 4 * num_colors; ++i) {

      new_data[i] = (data[i] + new_data[i - 4]) & 0xff;
    }
    for (; i < 4 * final_num_colors; ++i) {
      new_data[i] = 0;
    }
    WebPSafeFree(transform->data);
    transform->data = new_color_map;
  }
  return 1;
}

static int ReadTransform(int* const xsize, int const* ysize,
                         VP8LDecoder* const dec) {
  int ok = 1;
  VP8LBitReader* const br = &dec->br;
  VP8LTransform* transform = &dec->transforms[dec->next_transform];
  const VP8LImageTransformType type =
      (VP8LImageTransformType)VP8LReadBits(br, 2);

  if (dec->transforms_seen & (1U << type)) {
    return 0;
  }
  dec->transforms_seen |= (1U << type);

  transform->type = type;
  transform->xsize = *xsize;
  transform->ysize = *ysize;
  transform->data = NULL;
  ++dec->next_transform;
  assert(dec->next_transform <= NUM_TRANSFORMS);

  switch (type) {
    case PREDICTOR_TRANSFORM:
    case CROSS_COLOR_TRANSFORM:
      transform->bits =
          MIN_TRANSFORM_BITS + VP8LReadBits(br, NUM_TRANSFORM_BITS);
      ok = DecodeImageStream(VP8LSubSampleSize(transform->xsize,
                                               transform->bits),
                             VP8LSubSampleSize(transform->ysize,
                                               transform->bits),
                             0, dec, &transform->data);
      break;
    case COLOR_INDEXING_TRANSFORM: {
       const int num_colors = VP8LReadBits(br, 8) + 1;
       const int bits = (num_colors > 16) ? 0
                      : (num_colors > 4) ? 1
                      : (num_colors > 2) ? 2
                      : 3;
       *xsize = VP8LSubSampleSize(transform->xsize, bits);
       transform->bits = bits;
       ok = DecodeImageStream(num_colors, 1, 0, dec,
                              &transform->data);
       if (ok && !ExpandColorMap(num_colors, transform)) {
         return VP8LSetError(dec, VP8_STATUS_OUT_OF_MEMORY);
       }
      break;
    }
    case SUBTRACT_GREEN_TRANSFORM:
      break;
    default:
      assert(0);
      break;
  }

  return ok;
}

static void InitMetadata(VP8LMetadata* const hdr) {
  assert(hdr != NULL);
  memset(hdr, 0, sizeof(*hdr));
}

static void ClearMetadata(VP8LMetadata* const hdr) {
  assert(hdr != NULL);

  WebPSafeFree(hdr->huffman_image);
  VP8LHuffmanTablesDeallocate(&hdr->huffman_tables);
  VP8LHtreeGroupsFree(hdr->htree_groups);
  VP8LColorCacheClear(&hdr->color_cache);
  VP8LColorCacheClear(&hdr->saved_color_cache);
  InitMetadata(hdr);
}

VP8LDecoder* VP8LNew(void) {
  VP8LDecoder* const dec = (VP8LDecoder*)WebPSafeCalloc(1ULL, sizeof(*dec));
  if (dec == NULL) return NULL;
  dec->status = VP8_STATUS_OK;
  dec->state = READ_DIM;

  VP8LDspInit();

  return dec;
}

static void VP8LClear(VP8LDecoder* const dec) {
  int i;
  if (dec == NULL) return;
  ClearMetadata(&dec->hdr);

  WebPSafeFree(dec->pixels);
  dec->pixels = NULL;
  for (i = 0; i < dec->next_transform; ++i) {
    ClearTransform(&dec->transforms[i]);
  }
  dec->next_transform = 0;
  dec->transforms_seen = 0;

  WebPSafeFree(dec->rescaler_memory);
  dec->rescaler_memory = NULL;

  dec->output = NULL;
}

void VP8LDelete(VP8LDecoder* const dec) {
  if (dec != NULL) {
    VP8LClear(dec);
    WebPSafeFree(dec);
  }
}

static void UpdateDecoder(VP8LDecoder* const dec, int width, int height) {
  VP8LMetadata* const hdr = &dec->hdr;
  const int num_bits = hdr->huffman_subsample_bits;
  dec->width = width;
  dec->height = height;

  hdr->huffman_xsize = VP8LSubSampleSize(width, num_bits);
  hdr->huffman_mask = (num_bits == 0) ? ~0 : (1 << num_bits) - 1;
}

static int DecodeImageStream(int xsize, int ysize,
                             int is_level0,
                             VP8LDecoder* const dec,
                             uint32_t** const decoded_data) {
  int ok = 1;
  int transform_xsize = xsize;
  int transform_ysize = ysize;
  VP8LBitReader* const br = &dec->br;
  VP8LMetadata* const hdr = &dec->hdr;
  uint32_t* data = NULL;
  int color_cache_bits = 0;

  if (is_level0) {
    while (ok && VP8LReadBits(br, 1)) {
      ok = ReadTransform(&transform_xsize, &transform_ysize, dec);
    }
  }

  if (ok && VP8LReadBits(br, 1)) {
    color_cache_bits = VP8LReadBits(br, 4);
    ok = (color_cache_bits >= 1 && color_cache_bits <= MAX_CACHE_BITS);
    if (!ok) {
      VP8LSetError(dec, VP8_STATUS_BITSTREAM_ERROR);
      goto End;
    }
  }

  ok = ok && ReadHuffmanCodes(dec, transform_xsize, transform_ysize,
                              color_cache_bits, is_level0);
  if (!ok) {
    VP8LSetError(dec, VP8_STATUS_BITSTREAM_ERROR);
    goto End;
  }

  if (color_cache_bits > 0) {
    hdr->color_cache_size = 1 << color_cache_bits;
    if (!VP8LColorCacheInit(&hdr->color_cache, color_cache_bits)) {
      ok = VP8LSetError(dec, VP8_STATUS_OUT_OF_MEMORY);
      goto End;
    }
  } else {
    hdr->color_cache_size = 0;
  }
  UpdateDecoder(dec, transform_xsize, transform_ysize);

  if (is_level0) {
    dec->state = READ_HDR;
    goto End;
  }

  {
    const uint64_t total_size = (uint64_t)transform_xsize * transform_ysize;
    data = (uint32_t*)WebPSafeMalloc(total_size, sizeof(*data));
    if (data == NULL) {
      ok = VP8LSetError(dec, VP8_STATUS_OUT_OF_MEMORY);
      goto End;
    }
  }

  ok = DecodeImageData(dec, data, transform_xsize, transform_ysize,
                       transform_ysize, NULL);
  ok = ok && !br->eos;

 End:
  if (!ok) {
    WebPSafeFree(data);
    ClearMetadata(hdr);
  } else {
    if (decoded_data != NULL) {
      *decoded_data = data;
    } else {

      assert(data == NULL);
      assert(is_level0);
    }
    dec->last_pixel = 0;
    if (!is_level0) ClearMetadata(hdr);
  }
  return ok;
}

static int AllocateInternalBuffers32b(VP8LDecoder* const dec, int final_width) {
  const uint64_t num_pixels = (uint64_t)dec->width * dec->height;

  const uint64_t cache_top_pixels = (uint16_t)final_width;

  const uint64_t cache_pixels = (uint64_t)final_width * NUM_ARGB_CACHE_ROWS;
  const uint64_t total_num_pixels =
      num_pixels + cache_top_pixels + cache_pixels;

  assert(dec->width <= final_width);
  dec->pixels = (uint32_t*)WebPSafeMalloc(total_num_pixels, sizeof(uint32_t));
  if (dec->pixels == NULL) {
    dec->argb_cache = NULL;
    return VP8LSetError(dec, VP8_STATUS_OUT_OF_MEMORY);
  }
  dec->argb_cache = dec->pixels + num_pixels + cache_top_pixels;
  return 1;
}

static int AllocateInternalBuffers8b(VP8LDecoder* const dec) {
  const uint64_t total_num_pixels = (uint64_t)dec->width * dec->height;
  dec->argb_cache = NULL;
  dec->pixels = (uint32_t*)WebPSafeMalloc(total_num_pixels, sizeof(uint8_t));
  if (dec->pixels == NULL) {
    return VP8LSetError(dec, VP8_STATUS_OUT_OF_MEMORY);
  }
  return 1;
}

static void ExtractAlphaRows(VP8LDecoder* const dec, int last_row) {
  int cur_row = dec->last_row;
  int num_rows = last_row - cur_row;
  const uint32_t* in = dec->pixels + dec->width * cur_row;

  assert(last_row <= dec->io->crop_bottom);
  while (num_rows > 0) {
    const int num_rows_to_process =
        (num_rows > NUM_ARGB_CACHE_ROWS) ? NUM_ARGB_CACHE_ROWS : num_rows;

    ALPHDecoder* const alph_dec = (ALPHDecoder*)dec->io->opaque;
    uint8_t* const output = alph_dec->output;
    const int width = dec->io->width;
    const int cache_pixs = width * num_rows_to_process;
    uint8_t* const dst = output + width * cur_row;
    const uint32_t* const src = dec->argb_cache;
    ApplyInverseTransforms(dec, cur_row, num_rows_to_process, in);
    WebPExtractGreen(src, dst, cache_pixs);
    AlphaApplyFilter(alph_dec,
                     cur_row, cur_row + num_rows_to_process, dst, width);
    num_rows -= num_rows_to_process;
    in += num_rows_to_process * dec->width;
    cur_row += num_rows_to_process;
  }
  assert(cur_row == last_row);
  dec->last_row = dec->last_out_row = last_row;
}

int VP8LDecodeAlphaHeader(ALPHDecoder* const alph_dec,
                          const uint8_t* const data, size_t data_size) {
  int ok = 0;
  VP8LDecoder* dec = VP8LNew();

  if (dec == NULL) return 0;

  assert(alph_dec != NULL);

  dec->width = alph_dec->width;
  dec->height = alph_dec->height;
  dec->io = &alph_dec->io;
  dec->io->opaque = alph_dec;
  dec->io->width = alph_dec->width;
  dec->io->height = alph_dec->height;

  dec->status = VP8_STATUS_OK;
  VP8LInitBitReader(&dec->br, data, data_size);

  if (!DecodeImageStream(alph_dec->width, alph_dec->height, 1,
                         dec, NULL)) {
    goto Err;
  }

  if (dec->next_transform == 1 &&
      dec->transforms[0].type == COLOR_INDEXING_TRANSFORM &&
      Is8bOptimizable(&dec->hdr)) {
    alph_dec->use_8b_decode = 1;
    ok = AllocateInternalBuffers8b(dec);
  } else {

    alph_dec->use_8b_decode = 0;
    ok = AllocateInternalBuffers32b(dec, alph_dec->width);
  }

  if (!ok) goto Err;

  alph_dec->vp8l_dec = dec;
  return 1;

 Err:
  VP8LDelete(dec);
  return 0;
}

int VP8LDecodeAlphaImageStream(ALPHDecoder* const alph_dec, int last_row) {
  VP8LDecoder* const dec = alph_dec->vp8l_dec;
  assert(dec != NULL);
  assert(last_row <= dec->height);

  if (dec->last_row >= last_row) {
    return 1;
  }

  if (!alph_dec->use_8b_decode) WebPInitAlphaProcessing();

  return alph_dec->use_8b_decode ?
      DecodeAlphaData(dec, (uint8_t*)dec->pixels, dec->width, dec->height,
                      last_row) :
      DecodeImageData(dec, dec->pixels, dec->width, dec->height,
                      last_row, ExtractAlphaRows);
}

int VP8LDecodeHeader(VP8LDecoder* const dec, VP8Io* const io) {
  int width, height, has_alpha;

  if (dec == NULL) return 0;
  if (io == NULL) {
    return VP8LSetError(dec, VP8_STATUS_INVALID_PARAM);
  }

  dec->io = io;
  dec->status = VP8_STATUS_OK;
  VP8LInitBitReader(&dec->br, io->data, io->data_size);
  if (!ReadImageInfo(&dec->br, &width, &height, &has_alpha)) {
    VP8LSetError(dec, VP8_STATUS_BITSTREAM_ERROR);
    goto Error;
  }
  dec->state = READ_DIM;
  io->width = width;
  io->height = height;

  if (!DecodeImageStream(width, height, 1, dec,
                         NULL)) {
    goto Error;
  }
  return 1;

 Error:
  VP8LClear(dec);
  assert(dec->status != VP8_STATUS_OK);
  return 0;
}

int VP8LDecodeImage(VP8LDecoder* const dec) {
  VP8Io* io = NULL;
  WebPDecParams* params = NULL;

  if (dec == NULL) return 0;

  assert(dec->hdr.huffman_tables.root.start != NULL);
  assert(dec->hdr.htree_groups != NULL);
  assert(dec->hdr.num_htree_groups > 0);

  io = dec->io;
  assert(io != NULL);
  params = (WebPDecParams*)io->opaque;
  assert(params != NULL);

  if (dec->state != READ_DATA) {
    dec->output = params->output;
    assert(dec->output != NULL);

    if (!WebPIoInitFromOptions(params->options, io, MODE_BGRA)) {
      VP8LSetError(dec, VP8_STATUS_INVALID_PARAM);
      goto Err;
    }

    if (!AllocateInternalBuffers32b(dec, io->width)) goto Err;

#if !defined(WEBP_REDUCE_SIZE)
    if (io->use_scaling && !AllocateAndInitRescaler(dec, io)) goto Err;
#else
    if (io->use_scaling) {
      VP8LSetError(dec, VP8_STATUS_INVALID_PARAM);
      goto Err;
    }
#endif
    if (io->use_scaling || WebPIsPremultipliedMode(dec->output->colorspace)) {

      WebPInitAlphaProcessing();
    }

    if (!WebPIsRGBMode(dec->output->colorspace)) {
      WebPInitConvertARGBToYUV();
      if (dec->output->u.YUVA.a != NULL) WebPInitAlphaProcessing();
    }
    if (dec->incremental) {
      if (dec->hdr.color_cache_size > 0 &&
          dec->hdr.saved_color_cache.colors == NULL) {
        if (!VP8LColorCacheInit(&dec->hdr.saved_color_cache,
                                dec->hdr.color_cache.hash_bits)) {
          VP8LSetError(dec, VP8_STATUS_OUT_OF_MEMORY);
          goto Err;
        }
      }
    }
    dec->state = READ_DATA;
  }

  if (!DecodeImageData(dec, dec->pixels, dec->width, dec->height,
                       io->crop_bottom, ProcessRows)) {
    goto Err;
  }

  params->last_y = dec->last_out_row;
  return 1;

 Err:
  VP8LClear(dec);
  assert(dec->status != VP8_STATUS_OK);
  return 0;
}

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#ifndef WEBP_WEBP_MUX_TYPES_H_
#define WEBP_WEBP_MUX_TYPES_H_

#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct WebPData WebPData;

typedef enum WebPFeatureFlags {
  ANIMATION_FLAG  = 0x00000002,
  XMP_FLAG        = 0x00000004,
  EXIF_FLAG       = 0x00000008,
  ALPHA_FLAG      = 0x00000010,
  ICCP_FLAG       = 0x00000020,

  ALL_VALID_FLAGS = 0x0000003e
} WebPFeatureFlags;

typedef enum WebPMuxAnimDispose {
  WEBP_MUX_DISPOSE_NONE,
  WEBP_MUX_DISPOSE_BACKGROUND
} WebPMuxAnimDispose;

typedef enum WebPMuxAnimBlend {
  WEBP_MUX_BLEND,
  WEBP_MUX_NO_BLEND
} WebPMuxAnimBlend;

struct WebPData {
  const uint8_t* bytes;
  size_t size;
};

static WEBP_INLINE void WebPDataInit(WebPData* webp_data) {
  if (webp_data != NULL) {
    memset(webp_data, 0, sizeof(*webp_data));
  }
}

static WEBP_INLINE void WebPDataClear(WebPData* webp_data) {
  if (webp_data != NULL) {
    WebPFree((void*)webp_data->bytes);
    WebPDataInit(webp_data);
  }
}

WEBP_NODISCARD static WEBP_INLINE int WebPDataCopy(const WebPData* src,
                                                   WebPData* dst) {
  if (src == NULL || dst == NULL) return 0;
  WebPDataInit(dst);
  if (src->bytes != NULL && src->size != 0) {
    dst->bytes = (uint8_t*)WebPMalloc(src->size);
    if (dst->bytes == NULL) return 0;
    memcpy((void*)dst->bytes, src->bytes, src->size);
    dst->size = src->size;
  }
  return 1;
}

#ifdef __cplusplus
}
#endif

#endif

static VP8StatusCode ParseRIFF(const uint8_t** const data,
                               size_t* const data_size, int have_all_data,
                               size_t* const riff_size) {
  assert(data != NULL);
  assert(data_size != NULL);
  assert(riff_size != NULL);

  *riff_size = 0;
  if (*data_size >= RIFF_HEADER_SIZE && !memcmp(*data, "RIFF", TAG_SIZE)) {
    if (memcmp(*data + 8, "WEBP", TAG_SIZE)) {
      return VP8_STATUS_BITSTREAM_ERROR;
    } else {
      const uint32_t size = GetLE32(*data + TAG_SIZE);

      if (size < TAG_SIZE + CHUNK_HEADER_SIZE) {
        return VP8_STATUS_BITSTREAM_ERROR;
      }
      if (size > MAX_CHUNK_PAYLOAD) {
        return VP8_STATUS_BITSTREAM_ERROR;
      }
      if (have_all_data && (size > *data_size - CHUNK_HEADER_SIZE)) {
        return VP8_STATUS_NOT_ENOUGH_DATA;
      }

      *riff_size = size;
      *data += RIFF_HEADER_SIZE;
      *data_size -= RIFF_HEADER_SIZE;
    }
  }
  return VP8_STATUS_OK;
}

static VP8StatusCode ParseVP8X(const uint8_t** const data,
                               size_t* const data_size,
                               int* const found_vp8x,
                               int* const width_ptr, int* const height_ptr,
                               uint32_t* const flags_ptr) {
  const uint32_t vp8x_size = CHUNK_HEADER_SIZE + VP8X_CHUNK_SIZE;
  assert(data != NULL);
  assert(data_size != NULL);
  assert(found_vp8x != NULL);

  *found_vp8x = 0;

  if (*data_size < CHUNK_HEADER_SIZE) {
    return VP8_STATUS_NOT_ENOUGH_DATA;
  }

  if (!memcmp(*data, "VP8X", TAG_SIZE)) {
    int width, height;
    uint32_t flags;
    const uint32_t chunk_size = GetLE32(*data + TAG_SIZE);
    if (chunk_size != VP8X_CHUNK_SIZE) {
      return VP8_STATUS_BITSTREAM_ERROR;
    }

    if (*data_size < vp8x_size) {
      return VP8_STATUS_NOT_ENOUGH_DATA;
    }
    flags = GetLE32(*data + 8);
    width = 1 + GetLE24(*data + 12);
    height = 1 + GetLE24(*data + 15);
    if (width * (uint64_t)height >= MAX_IMAGE_AREA) {
      return VP8_STATUS_BITSTREAM_ERROR;
    }

    if (flags_ptr != NULL) *flags_ptr = flags;
    if (width_ptr != NULL) *width_ptr = width;
    if (height_ptr != NULL) *height_ptr = height;

    *data += vp8x_size;
    *data_size -= vp8x_size;
    *found_vp8x = 1;
  }
  return VP8_STATUS_OK;
}

static VP8StatusCode ParseOptionalChunks(const uint8_t** const data,
                                         size_t* const data_size,
                                         size_t const riff_size,
                                         const uint8_t** const alpha_data,
                                         size_t* const alpha_size) {
  const uint8_t* buf;
  size_t buf_size;
  uint32_t total_size = TAG_SIZE +
                        CHUNK_HEADER_SIZE +
                        VP8X_CHUNK_SIZE;
  assert(data != NULL);
  assert(data_size != NULL);
  buf = *data;
  buf_size = *data_size;

  assert(alpha_data != NULL);
  assert(alpha_size != NULL);
  *alpha_data = NULL;
  *alpha_size = 0;

  while (1) {
    uint32_t chunk_size;
    uint32_t disk_chunk_size;

    *data = buf;
    *data_size = buf_size;

    if (buf_size < CHUNK_HEADER_SIZE) {
      return VP8_STATUS_NOT_ENOUGH_DATA;
    }

    chunk_size = GetLE32(buf + TAG_SIZE);
    if (chunk_size > MAX_CHUNK_PAYLOAD) {
      return VP8_STATUS_BITSTREAM_ERROR;
    }

    disk_chunk_size = (CHUNK_HEADER_SIZE + chunk_size + 1) & ~1u;
    total_size += disk_chunk_size;

    if (riff_size > 0 && (total_size > riff_size)) {
      return VP8_STATUS_BITSTREAM_ERROR;
    }

    if (!memcmp(buf, "VP8 ", TAG_SIZE) ||
        !memcmp(buf, "VP8L", TAG_SIZE)) {
      return VP8_STATUS_OK;
    }

    if (buf_size < disk_chunk_size) {
      return VP8_STATUS_NOT_ENOUGH_DATA;
    }

    if (!memcmp(buf, "ALPH", TAG_SIZE)) {
      *alpha_data = buf + CHUNK_HEADER_SIZE;
      *alpha_size = chunk_size;
    }

    buf += disk_chunk_size;
    buf_size -= disk_chunk_size;
  }
}

static VP8StatusCode ParseVP8Header(const uint8_t** const data_ptr,
                                    size_t* const data_size, int have_all_data,
                                    size_t riff_size, size_t* const chunk_size,
                                    int* const is_lossless) {
  const uint8_t* const data = *data_ptr;
  const int is_vp8 = !memcmp(data, "VP8 ", TAG_SIZE);
  const int is_vp8l = !memcmp(data, "VP8L", TAG_SIZE);
  const uint32_t minimal_size =
      TAG_SIZE + CHUNK_HEADER_SIZE;

  assert(data != NULL);
  assert(data_size != NULL);
  assert(chunk_size != NULL);
  assert(is_lossless != NULL);

  if (*data_size < CHUNK_HEADER_SIZE) {
    return VP8_STATUS_NOT_ENOUGH_DATA;
  }

  if (is_vp8 || is_vp8l) {

    const uint32_t size = GetLE32(data + TAG_SIZE);
    if ((riff_size >= minimal_size) && (size > riff_size - minimal_size)) {
      return VP8_STATUS_BITSTREAM_ERROR;
    }
    if (have_all_data && (size > *data_size - CHUNK_HEADER_SIZE)) {
      return VP8_STATUS_NOT_ENOUGH_DATA;
    }

    *chunk_size = size;
    *data_ptr += CHUNK_HEADER_SIZE;
    *data_size -= CHUNK_HEADER_SIZE;
    *is_lossless = is_vp8l;
  } else {

    *is_lossless = VP8LCheckSignature(data, *data_size);
    *chunk_size = *data_size;
  }

  return VP8_STATUS_OK;
}

static VP8StatusCode ParseHeadersInternal(const uint8_t* data,
                                          size_t data_size,
                                          int* const width,
                                          int* const height,
                                          int* const has_alpha,
                                          int* const has_animation,
                                          int* const format,
                                          WebPHeaderStructure* const headers) {
  int canvas_width = 0;
  int canvas_height = 0;
  int image_width = 0;
  int image_height = 0;
  int found_riff = 0;
  int found_vp8x = 0;
  int animation_present = 0;
  const int have_all_data = (headers != NULL) ? headers->have_all_data : 0;

  VP8StatusCode status;
  WebPHeaderStructure hdrs;

  if (data == NULL || data_size < RIFF_HEADER_SIZE) {
    return VP8_STATUS_NOT_ENOUGH_DATA;
  }
  memset(&hdrs, 0, sizeof(hdrs));
  hdrs.data = data;
  hdrs.data_size = data_size;

  status = ParseRIFF(&data, &data_size, have_all_data, &hdrs.riff_size);
  if (status != VP8_STATUS_OK) {
    return status;
  }
  found_riff = (hdrs.riff_size > 0);

  {
    uint32_t flags = 0;
    status = ParseVP8X(&data, &data_size, &found_vp8x,
                       &canvas_width, &canvas_height, &flags);
    if (status != VP8_STATUS_OK) {
      return status;
    }
    animation_present = !!(flags & ANIMATION_FLAG);
    if (!found_riff && found_vp8x) {

      return VP8_STATUS_BITSTREAM_ERROR;
    }
    if (has_alpha != NULL) *has_alpha = !!(flags & ALPHA_FLAG);
    if (has_animation != NULL) *has_animation = animation_present;
    if (format != NULL) *format = 0;

    image_width = canvas_width;
    image_height = canvas_height;
    if (found_vp8x && animation_present && headers == NULL) {
      status = VP8_STATUS_OK;
      goto ReturnWidthHeight;
    }
  }

  if (data_size < TAG_SIZE) {
    status = VP8_STATUS_NOT_ENOUGH_DATA;
    goto ReturnWidthHeight;
  }

  if ((found_riff && found_vp8x) ||
      (!found_riff && !found_vp8x && !memcmp(data, "ALPH", TAG_SIZE))) {
    status = ParseOptionalChunks(&data, &data_size, hdrs.riff_size,
                                 &hdrs.alpha_data, &hdrs.alpha_data_size);
    if (status != VP8_STATUS_OK) {
      goto ReturnWidthHeight;
    }
  }

  status = ParseVP8Header(&data, &data_size, have_all_data, hdrs.riff_size,
                          &hdrs.compressed_size, &hdrs.is_lossless);
  if (status != VP8_STATUS_OK) {
    goto ReturnWidthHeight;
  }
  if (hdrs.compressed_size > MAX_CHUNK_PAYLOAD) {
    return VP8_STATUS_BITSTREAM_ERROR;
  }

  if (format != NULL && !animation_present) {
    *format = hdrs.is_lossless ? 2 : 1;
  }

  if (!hdrs.is_lossless) {
    if (data_size < VP8_FRAME_HEADER_SIZE) {
      status = VP8_STATUS_NOT_ENOUGH_DATA;
      goto ReturnWidthHeight;
    }

    if (!VP8GetInfo(data, data_size, (uint32_t)hdrs.compressed_size,
                    &image_width, &image_height)) {
      return VP8_STATUS_BITSTREAM_ERROR;
    }
  } else {
    if (data_size < VP8L_FRAME_HEADER_SIZE) {
      status = VP8_STATUS_NOT_ENOUGH_DATA;
      goto ReturnWidthHeight;
    }

    if (!VP8LGetInfo(data, data_size, &image_width, &image_height, has_alpha)) {
      return VP8_STATUS_BITSTREAM_ERROR;
    }
  }

  if (found_vp8x) {
    if (canvas_width != image_width || canvas_height != image_height) {
      return VP8_STATUS_BITSTREAM_ERROR;
    }
  }
  if (headers != NULL) {
    *headers = hdrs;
    headers->offset = data - headers->data;
    assert((uint64_t)(data - headers->data) < MAX_CHUNK_PAYLOAD);
    assert(headers->offset == headers->data_size - data_size);
  }
 ReturnWidthHeight:
  if (status == VP8_STATUS_OK ||
      (status == VP8_STATUS_NOT_ENOUGH_DATA && found_vp8x && headers == NULL)) {
    if (has_alpha != NULL) {

      *has_alpha |= (hdrs.alpha_data != NULL);
    }
    if (width != NULL) *width = image_width;
    if (height != NULL) *height = image_height;
    return VP8_STATUS_OK;
  } else {
    return status;
  }
}

VP8StatusCode WebPParseHeaders(WebPHeaderStructure* const headers) {

  volatile VP8StatusCode status;
  int has_animation = 0;
  assert(headers != NULL);

  status = ParseHeadersInternal(headers->data, headers->data_size,
                                NULL, NULL, NULL, &has_animation,
                                NULL, headers);
  if (status == VP8_STATUS_OK || status == VP8_STATUS_NOT_ENOUGH_DATA) {

    if (has_animation) {
      status = VP8_STATUS_UNSUPPORTED_FEATURE;
    }
  }
  return status;
}

void WebPResetDecParams(WebPDecParams* const params) {
  if (params != NULL) {
    memset(params, 0, sizeof(*params));
  }
}

WEBP_NODISCARD static VP8StatusCode DecodeInto(const uint8_t* const data,
                                               size_t data_size,
                                               WebPDecParams* const params) {
  VP8StatusCode status;
  VP8Io io;
  WebPHeaderStructure headers;

  headers.data = data;
  headers.data_size = data_size;
  headers.have_all_data = 1;
  status = WebPParseHeaders(&headers);
  if (status != VP8_STATUS_OK) {
    return status;
  }

  assert(params != NULL);
  if (!VP8InitIo(&io)) {
    return VP8_STATUS_INVALID_PARAM;
  }
  io.data = headers.data + headers.offset;
  io.data_size = headers.data_size - headers.offset;
  WebPInitCustomIo(params, &io);

  if (!headers.is_lossless) {
    VP8Decoder* const dec = VP8New();
    if (dec == NULL) {
      return VP8_STATUS_OUT_OF_MEMORY;
    }
    dec->alpha_data = headers.alpha_data;
    dec->alpha_data_size = headers.alpha_data_size;

    if (!VP8GetHeaders(dec, &io)) {
      status = dec->status;
    } else {

      status = WebPAllocateDecBuffer(io.width, io.height, params->options,
                                     params->output);
      if (status == VP8_STATUS_OK) {

        dec->mt_method = VP8GetThreadMethod(params->options, &headers,
                                            io.width, io.height);
        VP8InitDithering(params->options, dec);
        if (!VP8Decode(dec, &io)) {
          status = dec->status;
        }
      }
    }
    VP8Delete(dec);
  } else {
    VP8LDecoder* const dec = VP8LNew();
    if (dec == NULL) {
      return VP8_STATUS_OUT_OF_MEMORY;
    }
    if (!VP8LDecodeHeader(dec, &io)) {
      status = dec->status;
    } else {

      status = WebPAllocateDecBuffer(io.width, io.height, params->options,
                                     params->output);
      if (status == VP8_STATUS_OK) {
        if (!VP8LDecodeImage(dec)) {
          status = dec->status;
        }
      }
    }
    VP8LDelete(dec);
  }

  if (status != VP8_STATUS_OK) {
    WebPFreeDecBuffer(params->output);
  } else {
    if (params->options != NULL && params->options->flip) {

      status = WebPFlipBuffer(params->output);
    }
  }
  return status;
}

WEBP_NODISCARD static uint8_t* DecodeIntoRGBABuffer(WEBP_CSP_MODE colorspace,
                                                    const uint8_t* const data,
                                                    size_t data_size,
                                                    uint8_t* const rgba,
                                                    int stride, size_t size) {
  WebPDecParams params;
  WebPDecBuffer buf;
  if (rgba == NULL || !WebPInitDecBuffer(&buf)) {
    return NULL;
  }
  WebPResetDecParams(&params);
  params.output = &buf;
  buf.colorspace    = colorspace;
  buf.u.RGBA.rgba   = rgba;
  buf.u.RGBA.stride = stride;
  buf.u.RGBA.size   = size;
  buf.is_external_memory = 1;
  if (DecodeInto(data, data_size, &params) != VP8_STATUS_OK) {
    return NULL;
  }
  return rgba;
}

uint8_t* WebPDecodeRGBInto(const uint8_t* data, size_t data_size,
                           uint8_t* output, size_t size, int stride) {
  return DecodeIntoRGBABuffer(MODE_RGB, data, data_size, output, stride, size);
}

uint8_t* WebPDecodeRGBAInto(const uint8_t* data, size_t data_size,
                            uint8_t* output, size_t size, int stride) {
  return DecodeIntoRGBABuffer(MODE_RGBA, data, data_size, output, stride, size);
}

uint8_t* WebPDecodeARGBInto(const uint8_t* data, size_t data_size,
                            uint8_t* output, size_t size, int stride) {
  return DecodeIntoRGBABuffer(MODE_ARGB, data, data_size, output, stride, size);
}

uint8_t* WebPDecodeBGRInto(const uint8_t* data, size_t data_size,
                           uint8_t* output, size_t size, int stride) {
  return DecodeIntoRGBABuffer(MODE_BGR, data, data_size, output, stride, size);
}

uint8_t* WebPDecodeBGRAInto(const uint8_t* data, size_t data_size,
                            uint8_t* output, size_t size, int stride) {
  return DecodeIntoRGBABuffer(MODE_BGRA, data, data_size, output, stride, size);
}

uint8_t* WebPDecodeYUVInto(const uint8_t* data, size_t data_size,
                           uint8_t* luma, size_t luma_size, int luma_stride,
                           uint8_t* u, size_t u_size, int u_stride,
                           uint8_t* v, size_t v_size, int v_stride) {
  WebPDecParams params;
  WebPDecBuffer output;
  if (luma == NULL || !WebPInitDecBuffer(&output)) return NULL;
  WebPResetDecParams(&params);
  params.output = &output;
  output.colorspace      = MODE_YUV;
  output.u.YUVA.y        = luma;
  output.u.YUVA.y_stride = luma_stride;
  output.u.YUVA.y_size   = luma_size;
  output.u.YUVA.u        = u;
  output.u.YUVA.u_stride = u_stride;
  output.u.YUVA.u_size   = u_size;
  output.u.YUVA.v        = v;
  output.u.YUVA.v_stride = v_stride;
  output.u.YUVA.v_size   = v_size;
  output.is_external_memory = 1;
  if (DecodeInto(data, data_size, &params) != VP8_STATUS_OK) {
    return NULL;
  }
  return luma;
}

WEBP_NODISCARD static uint8_t* Decode(WEBP_CSP_MODE mode,
                                      const uint8_t* const data,
                                      size_t data_size, int* const width,
                                      int* const height,
                                      WebPDecBuffer* const keep_info) {
  WebPDecParams params;
  WebPDecBuffer output;

  if (!WebPInitDecBuffer(&output)) {
    return NULL;
  }
  WebPResetDecParams(&params);
  params.output = &output;
  output.colorspace = mode;

  if (!WebPGetInfo(data, data_size, &output.width, &output.height)) {
    return NULL;
  }
  if (width != NULL) *width = output.width;
  if (height != NULL) *height = output.height;

  if (DecodeInto(data, data_size, &params) != VP8_STATUS_OK) {
    return NULL;
  }
  if (keep_info != NULL) {
    WebPCopyDecBuffer(&output, keep_info);
  }

  return WebPIsRGBMode(mode) ? output.u.RGBA.rgba : output.u.YUVA.y;
}

uint8_t* WebPDecodeRGB(const uint8_t* data, size_t data_size,
                       int* width, int* height) {
  return Decode(MODE_RGB, data, data_size, width, height, NULL);
}

uint8_t* WebPDecodeRGBA(const uint8_t* data, size_t data_size,
                        int* width, int* height) {
  return Decode(MODE_RGBA, data, data_size, width, height, NULL);
}

uint8_t* WebPDecodeARGB(const uint8_t* data, size_t data_size,
                        int* width, int* height) {
  return Decode(MODE_ARGB, data, data_size, width, height, NULL);
}

uint8_t* WebPDecodeBGR(const uint8_t* data, size_t data_size,
                       int* width, int* height) {
  return Decode(MODE_BGR, data, data_size, width, height, NULL);
}

uint8_t* WebPDecodeBGRA(const uint8_t* data, size_t data_size,
                        int* width, int* height) {
  return Decode(MODE_BGRA, data, data_size, width, height, NULL);
}

uint8_t* WebPDecodeYUV(const uint8_t* data, size_t data_size,
                       int* width, int* height, uint8_t** u, uint8_t** v,
                       int* stride, int* uv_stride) {

  if (u == NULL || v == NULL || stride == NULL || uv_stride == NULL) {
    return NULL;
  }

  {
    WebPDecBuffer output;
    uint8_t* const out = Decode(MODE_YUV, data, data_size,
                                width, height, &output);

    if (out != NULL) {
      const WebPYUVABuffer* const buf = &output.u.YUVA;
      *u = buf->u;
      *v = buf->v;
      *stride = buf->y_stride;
      *uv_stride = buf->u_stride;
      assert(buf->u_stride == buf->v_stride);
    }
    return out;
  }
}

static void DefaultFeatures(WebPBitstreamFeatures* const features) {
  assert(features != NULL);
  memset(features, 0, sizeof(*features));
}

static VP8StatusCode GetFeatures(const uint8_t* const data, size_t data_size,
                                 WebPBitstreamFeatures* const features) {
  if (features == NULL || data == NULL) {
    return VP8_STATUS_INVALID_PARAM;
  }
  DefaultFeatures(features);

  return ParseHeadersInternal(data, data_size,
                              &features->width, &features->height,
                              &features->has_alpha, &features->has_animation,
                              &features->format, NULL);
}

int WebPGetInfo(const uint8_t* data, size_t data_size,
                int* width, int* height) {
  WebPBitstreamFeatures features;

  if (GetFeatures(data, data_size, &features) != VP8_STATUS_OK) {
    return 0;
  }

  if (width != NULL) {
    *width  = features.width;
  }
  if (height != NULL) {
    *height = features.height;
  }

  return 1;
}

int WebPInitDecoderConfigInternal(WebPDecoderConfig* config,
                                  int version) {
  if (WEBP_ABI_IS_INCOMPATIBLE(version, WEBP_DECODER_ABI_VERSION)) {
    return 0;
  }
  if (config == NULL) {
    return 0;
  }
  memset(config, 0, sizeof(*config));
  DefaultFeatures(&config->input);
  if (!WebPInitDecBuffer(&config->output)) {
    return 0;
  }
  return 1;
}

static int WebPCheckCropDimensionsBasic(int x, int y, int w, int h) {
  return !(x < 0 || y < 0 || w <= 0 || h <= 0);
}

int WebPValidateDecoderConfig(const WebPDecoderConfig* config) {
  const WebPDecoderOptions* options;
  if (config == NULL) return 0;
  if (!IsValidColorspace(config->output.colorspace)) {
    return 0;
  }

  options = &config->options;

  if (options->use_cropping && !WebPCheckCropDimensionsBasic(
                                   options->crop_left, options->crop_top,
                                   options->crop_width, options->crop_height)) {
    return 0;
  }

  if (options->use_scaling &&
      (options->scaled_width < 0 || options->scaled_height < 0 ||
       (options->scaled_width == 0 && options->scaled_height == 0))) {
    return 0;
  }

  if (config->input.width > 0 || config->input.height > 0) {
    int scaled_width = options->scaled_width;
    int scaled_height = options->scaled_height;
    if (options->use_cropping &&
        !WebPCheckCropDimensions(config->input.width, config->input.height,
                                 options->crop_left, options->crop_top,
                                 options->crop_width, options->crop_height)) {
      return 0;
    }
    if (options->use_scaling && !WebPRescalerGetScaledDimensions(
                                    config->input.width, config->input.height,
                                    &scaled_width, &scaled_height)) {
      return 0;
    }
  }

  if (options->dithering_strength < 0 || options->dithering_strength > 100 ||
      options->alpha_dithering_strength < 0 ||
      options->alpha_dithering_strength > 100) {
    return 0;
  }

  return 1;
}

VP8StatusCode WebPGetFeaturesInternal(const uint8_t* data, size_t data_size,
                                      WebPBitstreamFeatures* features,
                                      int version) {
  if (WEBP_ABI_IS_INCOMPATIBLE(version, WEBP_DECODER_ABI_VERSION)) {
    return VP8_STATUS_INVALID_PARAM;
  }
  if (features == NULL) {
    return VP8_STATUS_INVALID_PARAM;
  }
  return GetFeatures(data, data_size, features);
}

VP8StatusCode WebPDecode(const uint8_t* data, size_t data_size,
                         WebPDecoderConfig* config) {
  WebPDecParams params;
  VP8StatusCode status;

  if (config == NULL) {
    return VP8_STATUS_INVALID_PARAM;
  }

  status = GetFeatures(data, data_size, &config->input);
  if (status != VP8_STATUS_OK) {
    if (status == VP8_STATUS_NOT_ENOUGH_DATA) {
      return VP8_STATUS_BITSTREAM_ERROR;
    }
    return status;
  }

  WebPResetDecParams(&params);
  params.options = &config->options;
  params.output = &config->output;
  if (WebPAvoidSlowMemory(params.output, &config->input)) {

    WebPDecBuffer in_mem_buffer;
    if (!WebPInitDecBuffer(&in_mem_buffer)) {
      return VP8_STATUS_INVALID_PARAM;
    }
    in_mem_buffer.colorspace = config->output.colorspace;
    in_mem_buffer.width = config->input.width;
    in_mem_buffer.height = config->input.height;
    params.output = &in_mem_buffer;
    status = DecodeInto(data, data_size, &params);
    if (status == VP8_STATUS_OK) {
      status = WebPCopyDecBufferPixels(&in_mem_buffer, &config->output);
    }
    WebPFreeDecBuffer(&in_mem_buffer);
  } else {
    status = DecodeInto(data, data_size, &params);
  }

  return status;
}

int WebPCheckCropDimensions(int image_width, int image_height,
                            int x, int y, int w, int h) {
  return WebPCheckCropDimensionsBasic(x, y, w, h) &&
         !(x >= image_width || w > image_width || w > image_width - x ||
           y >= image_height || h > image_height || h > image_height - y);
}

int WebPIoInitFromOptions(const WebPDecoderOptions* const options,
                          VP8Io* const io, WEBP_CSP_MODE src_colorspace) {
  const int W = io->width;
  const int H = io->height;
  int x = 0, y = 0, w = W, h = H;

  io->use_cropping = (options != NULL) && options->use_cropping;
  if (io->use_cropping) {
    w = options->crop_width;
    h = options->crop_height;
    x = options->crop_left;
    y = options->crop_top;
    if (!WebPIsRGBMode(src_colorspace)) {
      x &= ~1;
      y &= ~1;
    }
    if (!WebPCheckCropDimensions(W, H, x, y, w, h)) {
      return 0;
    }
  }
  io->crop_left   = x;
  io->crop_top    = y;
  io->crop_right  = x + w;
  io->crop_bottom = y + h;
  io->mb_w = w;
  io->mb_h = h;

  io->use_scaling = (options != NULL) && options->use_scaling;
  if (io->use_scaling) {
    int scaled_width = options->scaled_width;
    int scaled_height = options->scaled_height;
    if (!WebPRescalerGetScaledDimensions(w, h, &scaled_width, &scaled_height)) {
      return 0;
    }
    io->scaled_width = scaled_width;
    io->scaled_height = scaled_height;
  }

  io->bypass_filtering = (options != NULL) && options->bypass_filtering;

#ifdef FANCY_UPSAMPLING
  io->fancy_upsampling = (options == NULL) || (!options->no_fancy_upsampling);
#endif

  if (io->use_scaling) {

    io->bypass_filtering |= (io->scaled_width < W * 3 / 4) &&
                            (io->scaled_height < H * 3 / 4);
    io->fancy_upsampling = 0;
  }
  return 1;
}

#include <assert.h>
#include <stddef.h>

#if !defined(USE_TABLES_FOR_ALPHA_MULT)
#define USE_TABLES_FOR_ALPHA_MULT 0
#endif

#define MFIX 24
#define HALF ((1u << MFIX) >> 1)
#define KINV_255 ((1u << MFIX) / 255u)

static uint32_t Mult(uint8_t x, uint32_t mult) {
  const uint32_t v = (x * mult + HALF) >> MFIX;
  assert(v <= 255);
  return v;
}

#if (USE_TABLES_FOR_ALPHA_MULT == 1)

static const uint32_t kMultTables[2][256] = {
  {
    0x00000000, 0xff000000, 0x7f800000, 0x55000000, 0x3fc00000, 0x33000000,
    0x2a800000, 0x246db6db, 0x1fe00000, 0x1c555555, 0x19800000, 0x172e8ba2,
    0x15400000, 0x139d89d8, 0x1236db6d, 0x11000000, 0x0ff00000, 0x0f000000,
    0x0e2aaaaa, 0x0d6bca1a, 0x0cc00000, 0x0c249249, 0x0b9745d1, 0x0b1642c8,
    0x0aa00000, 0x0a333333, 0x09cec4ec, 0x0971c71c, 0x091b6db6, 0x08cb08d3,
    0x08800000, 0x0839ce73, 0x07f80000, 0x07ba2e8b, 0x07800000, 0x07492492,
    0x07155555, 0x06e45306, 0x06b5e50d, 0x0689d89d, 0x06600000, 0x063831f3,
    0x06124924, 0x05ee23b8, 0x05cba2e8, 0x05aaaaaa, 0x058b2164, 0x056cefa8,
    0x05500000, 0x05343eb1, 0x05199999, 0x05000000, 0x04e76276, 0x04cfb2b7,
    0x04b8e38e, 0x04a2e8ba, 0x048db6db, 0x0479435e, 0x04658469, 0x045270d0,
    0x04400000, 0x042e29f7, 0x041ce739, 0x040c30c3, 0x03fc0000, 0x03ec4ec4,
    0x03dd1745, 0x03ce540f, 0x03c00000, 0x03b21642, 0x03a49249, 0x03976fc6,
    0x038aaaaa, 0x037e3f1f, 0x03722983, 0x03666666, 0x035af286, 0x034fcace,
    0x0344ec4e, 0x033a5440, 0x03300000, 0x0325ed09, 0x031c18f9, 0x0312818a,
    0x03092492, 0x03000000, 0x02f711dc, 0x02ee5846, 0x02e5d174, 0x02dd7baf,
    0x02d55555, 0x02cd5cd5, 0x02c590b2, 0x02bdef7b, 0x02b677d4, 0x02af286b,
    0x02a80000, 0x02a0fd5c, 0x029a1f58, 0x029364d9, 0x028ccccc, 0x0286562d,
    0x02800000, 0x0279c952, 0x0273b13b, 0x026db6db, 0x0267d95b, 0x026217ec,
    0x025c71c7, 0x0256e62a, 0x0251745d, 0x024c1bac, 0x0246db6d, 0x0241b2f9,
    0x023ca1af, 0x0237a6f4, 0x0232c234, 0x022df2df, 0x02293868, 0x02249249,
    0x02200000, 0x021b810e, 0x021714fb, 0x0212bb51, 0x020e739c, 0x020a3d70,
    0x02061861, 0x02020408, 0x01fe0000, 0x01fa0be8, 0x01f62762, 0x01f25213,
    0x01ee8ba2, 0x01ead3ba, 0x01e72a07, 0x01e38e38, 0x01e00000, 0x01dc7f10,
    0x01d90b21, 0x01d5a3e9, 0x01d24924, 0x01cefa8d, 0x01cbb7e3, 0x01c880e5,
    0x01c55555, 0x01c234f7, 0x01bf1f8f, 0x01bc14e5, 0x01b914c1, 0x01b61eed,
    0x01b33333, 0x01b05160, 0x01ad7943, 0x01aaaaaa, 0x01a7e567, 0x01a5294a,
    0x01a27627, 0x019fcbd2, 0x019d2a20, 0x019a90e7, 0x01980000, 0x01957741,
    0x0192f684, 0x01907da4, 0x018e0c7c, 0x018ba2e8, 0x018940c5, 0x0186e5f0,
    0x01849249, 0x018245ae, 0x01800000, 0x017dc11f, 0x017b88ee, 0x0179574e,
    0x01772c23, 0x01750750, 0x0172e8ba, 0x0170d045, 0x016ebdd7, 0x016cb157,
    0x016aaaaa, 0x0168a9b9, 0x0166ae6a, 0x0164b8a7, 0x0162c859, 0x0160dd67,
    0x015ef7bd, 0x015d1745, 0x015b3bea, 0x01596596, 0x01579435, 0x0155c7b4,
    0x01540000, 0x01523d03, 0x01507eae, 0x014ec4ec, 0x014d0fac, 0x014b5edc,
    0x0149b26c, 0x01480a4a, 0x01466666, 0x0144c6af, 0x01432b16, 0x0141938b,
    0x01400000, 0x013e7063, 0x013ce4a9, 0x013b5cc0, 0x0139d89d, 0x01385830,
    0x0136db6d, 0x01356246, 0x0133ecad, 0x01327a97, 0x01310bf6, 0x012fa0be,
    0x012e38e3, 0x012cd459, 0x012b7315, 0x012a150a, 0x0128ba2e, 0x01276276,
    0x01260dd6, 0x0124bc44, 0x01236db6, 0x01222222, 0x0120d97c, 0x011f93bc,
    0x011e50d7, 0x011d10c4, 0x011bd37a, 0x011a98ef, 0x0119611a, 0x01182bf2,
    0x0116f96f, 0x0115c988, 0x01149c34, 0x0113716a, 0x01124924, 0x01112358,
    0x01100000, 0x010edf12, 0x010dc087, 0x010ca458, 0x010b8a7d, 0x010a72f0,
    0x01095da8, 0x01084a9f, 0x010739ce, 0x01062b2e, 0x01051eb8, 0x01041465,
    0x01030c30, 0x01020612, 0x01010204, 0x01000000 },
  {
    0x00000000, 0x00010101, 0x00020202, 0x00030303, 0x00040404, 0x00050505,
    0x00060606, 0x00070707, 0x00080808, 0x00090909, 0x000a0a0a, 0x000b0b0b,
    0x000c0c0c, 0x000d0d0d, 0x000e0e0e, 0x000f0f0f, 0x00101010, 0x00111111,
    0x00121212, 0x00131313, 0x00141414, 0x00151515, 0x00161616, 0x00171717,
    0x00181818, 0x00191919, 0x001a1a1a, 0x001b1b1b, 0x001c1c1c, 0x001d1d1d,
    0x001e1e1e, 0x001f1f1f, 0x00202020, 0x00212121, 0x00222222, 0x00232323,
    0x00242424, 0x00252525, 0x00262626, 0x00272727, 0x00282828, 0x00292929,
    0x002a2a2a, 0x002b2b2b, 0x002c2c2c, 0x002d2d2d, 0x002e2e2e, 0x002f2f2f,
    0x00303030, 0x00313131, 0x00323232, 0x00333333, 0x00343434, 0x00353535,
    0x00363636, 0x00373737, 0x00383838, 0x00393939, 0x003a3a3a, 0x003b3b3b,
    0x003c3c3c, 0x003d3d3d, 0x003e3e3e, 0x003f3f3f, 0x00404040, 0x00414141,
    0x00424242, 0x00434343, 0x00444444, 0x00454545, 0x00464646, 0x00474747,
    0x00484848, 0x00494949, 0x004a4a4a, 0x004b4b4b, 0x004c4c4c, 0x004d4d4d,
    0x004e4e4e, 0x004f4f4f, 0x00505050, 0x00515151, 0x00525252, 0x00535353,
    0x00545454, 0x00555555, 0x00565656, 0x00575757, 0x00585858, 0x00595959,
    0x005a5a5a, 0x005b5b5b, 0x005c5c5c, 0x005d5d5d, 0x005e5e5e, 0x005f5f5f,
    0x00606060, 0x00616161, 0x00626262, 0x00636363, 0x00646464, 0x00656565,
    0x00666666, 0x00676767, 0x00686868, 0x00696969, 0x006a6a6a, 0x006b6b6b,
    0x006c6c6c, 0x006d6d6d, 0x006e6e6e, 0x006f6f6f, 0x00707070, 0x00717171,
    0x00727272, 0x00737373, 0x00747474, 0x00757575, 0x00767676, 0x00777777,
    0x00787878, 0x00797979, 0x007a7a7a, 0x007b7b7b, 0x007c7c7c, 0x007d7d7d,
    0x007e7e7e, 0x007f7f7f, 0x00808080, 0x00818181, 0x00828282, 0x00838383,
    0x00848484, 0x00858585, 0x00868686, 0x00878787, 0x00888888, 0x00898989,
    0x008a8a8a, 0x008b8b8b, 0x008c8c8c, 0x008d8d8d, 0x008e8e8e, 0x008f8f8f,
    0x00909090, 0x00919191, 0x00929292, 0x00939393, 0x00949494, 0x00959595,
    0x00969696, 0x00979797, 0x00989898, 0x00999999, 0x009a9a9a, 0x009b9b9b,
    0x009c9c9c, 0x009d9d9d, 0x009e9e9e, 0x009f9f9f, 0x00a0a0a0, 0x00a1a1a1,
    0x00a2a2a2, 0x00a3a3a3, 0x00a4a4a4, 0x00a5a5a5, 0x00a6a6a6, 0x00a7a7a7,
    0x00a8a8a8, 0x00a9a9a9, 0x00aaaaaa, 0x00ababab, 0x00acacac, 0x00adadad,
    0x00aeaeae, 0x00afafaf, 0x00b0b0b0, 0x00b1b1b1, 0x00b2b2b2, 0x00b3b3b3,
    0x00b4b4b4, 0x00b5b5b5, 0x00b6b6b6, 0x00b7b7b7, 0x00b8b8b8, 0x00b9b9b9,
    0x00bababa, 0x00bbbbbb, 0x00bcbcbc, 0x00bdbdbd, 0x00bebebe, 0x00bfbfbf,
    0x00c0c0c0, 0x00c1c1c1, 0x00c2c2c2, 0x00c3c3c3, 0x00c4c4c4, 0x00c5c5c5,
    0x00c6c6c6, 0x00c7c7c7, 0x00c8c8c8, 0x00c9c9c9, 0x00cacaca, 0x00cbcbcb,
    0x00cccccc, 0x00cdcdcd, 0x00cecece, 0x00cfcfcf, 0x00d0d0d0, 0x00d1d1d1,
    0x00d2d2d2, 0x00d3d3d3, 0x00d4d4d4, 0x00d5d5d5, 0x00d6d6d6, 0x00d7d7d7,
    0x00d8d8d8, 0x00d9d9d9, 0x00dadada, 0x00dbdbdb, 0x00dcdcdc, 0x00dddddd,
    0x00dedede, 0x00dfdfdf, 0x00e0e0e0, 0x00e1e1e1, 0x00e2e2e2, 0x00e3e3e3,
    0x00e4e4e4, 0x00e5e5e5, 0x00e6e6e6, 0x00e7e7e7, 0x00e8e8e8, 0x00e9e9e9,
    0x00eaeaea, 0x00ebebeb, 0x00ececec, 0x00ededed, 0x00eeeeee, 0x00efefef,
    0x00f0f0f0, 0x00f1f1f1, 0x00f2f2f2, 0x00f3f3f3, 0x00f4f4f4, 0x00f5f5f5,
    0x00f6f6f6, 0x00f7f7f7, 0x00f8f8f8, 0x00f9f9f9, 0x00fafafa, 0x00fbfbfb,
    0x00fcfcfc, 0x00fdfdfd, 0x00fefefe, 0x00ffffff }
};

static WEBP_INLINE uint32_t GetScale(uint32_t a, int inverse) {
  return kMultTables[!inverse][a];
}

#else

static WEBP_INLINE uint32_t GetScale(uint32_t a, int inverse) {
  return inverse ? (255u << MFIX) / a : a * KINV_255;
}

#endif

void WebPMultARGBRow_C(uint32_t* const ptr, int width, int inverse) {
  int x;
  for (x = 0; x < width; ++x) {
    const uint32_t argb = ptr[x];
    if (argb < 0xff000000u) {
      if (argb <= 0x00ffffffu) {
        ptr[x] = 0;
      } else {
        const uint32_t alpha = (argb >> 24) & 0xff;
        const uint32_t scale = GetScale(alpha, inverse);
        uint32_t out = argb & 0xff000000u;
        out |= Mult(argb >>  0, scale) <<  0;
        out |= Mult(argb >>  8, scale) <<  8;
        out |= Mult(argb >> 16, scale) << 16;
        ptr[x] = out;
      }
    }
  }
}

void WebPMultRow_C(uint8_t* WEBP_RESTRICT const ptr,
                   const uint8_t* WEBP_RESTRICT const alpha,
                   int width, int inverse) {
  int x;
  for (x = 0; x < width; ++x) {
    const uint32_t a = alpha[x];
    if (a != 255) {
      if (a == 0) {
        ptr[x] = 0;
      } else {
        const uint32_t scale = GetScale(a, inverse);
        ptr[x] = Mult(ptr[x], scale);
      }
    }
  }
}

#undef KINV_255
#undef HALF
#undef MFIX

void (*WebPMultARGBRow)(uint32_t* const ptr, int width, int inverse);
void (*WebPMultRow)(uint8_t* WEBP_RESTRICT const ptr,
                    const uint8_t* WEBP_RESTRICT const alpha,
                    int width, int inverse);

void WebPMultARGBRows(uint8_t* ptr, int stride, int width, int num_rows,
                      int inverse) {
  int n;
  for (n = 0; n < num_rows; ++n) {
    WebPMultARGBRow((uint32_t*)ptr, width, inverse);
    ptr += stride;
  }
}

void WebPMultRows(uint8_t* WEBP_RESTRICT ptr, int stride,
                  const uint8_t* WEBP_RESTRICT alpha, int alpha_stride,
                  int width, int num_rows, int inverse) {
  int n;
  for (n = 0; n < num_rows; ++n) {
    WebPMultRow(ptr, alpha, width, inverse);
    ptr += stride;
    alpha += alpha_stride;
  }
}

#if 1
#define MULTIPLIER(a)   ((a) * 32897U)
#define PREMULTIPLY(x, m) (((x) * (m)) >> 23)
#else
#define MULTIPLIER(a) ((a) * 65793U)
#define PREMULTIPLY(x, m) (((x) * (m) + (1U << 23)) >> 24)
#endif

#if !WEBP_NEON_OMIT_C_CODE
static void ApplyAlphaMultiply_C(uint8_t* rgba, int alpha_first,
                                 int w, int h, int stride) {
  while (h-- > 0) {
    uint8_t* const rgb = rgba + (alpha_first ? 1 : 0);
    const uint8_t* const alpha = rgba + (alpha_first ? 0 : 3);
    int i;
    for (i = 0; i < w; ++i) {
      const uint32_t a = alpha[4 * i];
      if (a != 0xff) {
        const uint32_t mult = MULTIPLIER(a);
        rgb[4 * i + 0] = PREMULTIPLY(rgb[4 * i + 0], mult);
        rgb[4 * i + 1] = PREMULTIPLY(rgb[4 * i + 1], mult);
        rgb[4 * i + 2] = PREMULTIPLY(rgb[4 * i + 2], mult);
      }
    }
    rgba += stride;
  }
}
#endif
#undef MULTIPLIER
#undef PREMULTIPLY

#define MULTIPLIER(a)  ((a) * 0x1111)

static WEBP_INLINE uint8_t dither_hi(uint8_t x) {
  return (x & 0xf0) | (x >> 4);
}

static WEBP_INLINE uint8_t dither_lo(uint8_t x) {
  return (x & 0x0f) | (x << 4);
}

static WEBP_INLINE uint8_t multiply(uint8_t x, uint32_t m) {
  return (x * m) >> 16;
}

static WEBP_INLINE void ApplyAlphaMultiply4444_C(uint8_t* rgba4444,
                                                 int w, int h, int stride,
                                                 int rg_byte_pos ) {
  while (h-- > 0) {
    int i;
    for (i = 0; i < w; ++i) {
      const uint32_t rg = rgba4444[2 * i + rg_byte_pos];
      const uint32_t ba = rgba4444[2 * i + (rg_byte_pos ^ 1)];
      const uint8_t a = ba & 0x0f;
      const uint32_t mult = MULTIPLIER(a);
      const uint8_t r = multiply(dither_hi(rg), mult);
      const uint8_t g = multiply(dither_lo(rg), mult);
      const uint8_t b = multiply(dither_hi(ba), mult);
      rgba4444[2 * i + rg_byte_pos] = (r & 0xf0) | ((g >> 4) & 0x0f);
      rgba4444[2 * i + (rg_byte_pos ^ 1)] = (b & 0xf0) | a;
    }
    rgba4444 += stride;
  }
}
#undef MULTIPLIER

static void ApplyAlphaMultiply_16b_C(uint8_t* rgba4444,
                                     int w, int h, int stride) {
#if (WEBP_SWAP_16BIT_CSP == 1)
  ApplyAlphaMultiply4444_C(rgba4444, w, h, stride, 1);
#else
  ApplyAlphaMultiply4444_C(rgba4444, w, h, stride, 0);
#endif
}

#if !WEBP_NEON_OMIT_C_CODE
static int DispatchAlpha_C(const uint8_t* WEBP_RESTRICT alpha, int alpha_stride,
                           int width, int height,
                           uint8_t* WEBP_RESTRICT dst, int dst_stride) {
  uint32_t alpha_mask = 0xff;
  int i, j;

  for (j = 0; j < height; ++j) {
    for (i = 0; i < width; ++i) {
      const uint32_t alpha_value = alpha[i];
      dst[4 * i] = alpha_value;
      alpha_mask &= alpha_value;
    }
    alpha += alpha_stride;
    dst += dst_stride;
  }

  return (alpha_mask != 0xff);
}

static void DispatchAlphaToGreen_C(const uint8_t* WEBP_RESTRICT alpha,
                                   int alpha_stride, int width, int height,
                                   uint32_t* WEBP_RESTRICT dst,
                                   int dst_stride) {
  int i, j;
  for (j = 0; j < height; ++j) {
    for (i = 0; i < width; ++i) {
      dst[i] = alpha[i] << 8;
    }
    alpha += alpha_stride;
    dst += dst_stride;
  }
}

static int ExtractAlpha_C(const uint8_t* WEBP_RESTRICT argb, int argb_stride,
                          int width, int height,
                          uint8_t* WEBP_RESTRICT alpha, int alpha_stride) {
  uint8_t alpha_mask = 0xff;
  int i, j;

  for (j = 0; j < height; ++j) {
    for (i = 0; i < width; ++i) {
      const uint8_t alpha_value = argb[4 * i];
      alpha[i] = alpha_value;
      alpha_mask &= alpha_value;
    }
    argb += argb_stride;
    alpha += alpha_stride;
  }
  return (alpha_mask == 0xff);
}

static void ExtractGreen_C(const uint32_t* WEBP_RESTRICT argb,
                           uint8_t* WEBP_RESTRICT alpha, int size) {
  int i;
  for (i = 0; i < size; ++i) alpha[i] = argb[i] >> 8;
}
#endif

static int HasAlpha8b_C(const uint8_t* src, int length) {
  while (length-- > 0) if (*src++ != 0xff) return 1;
  return 0;
}

static int HasAlpha32b_C(const uint8_t* src, int length) {
  int x;
  for (x = 0; length-- > 0; x += 4) if (src[x] != 0xff) return 1;
  return 0;
}

static void AlphaReplace_C(uint32_t* src, int length, uint32_t color) {
  int x;
  for (x = 0; x < length; ++x) if ((src[x] >> 24) == 0) src[x] = color;
}

static WEBP_INLINE uint32_t MakeARGB32(int a, int r, int g, int b) {
  return (((uint32_t)a << 24) | (r << 16) | (g << 8) | b);
}

#ifdef WORDS_BIGENDIAN
static void PackARGB_C(const uint8_t* WEBP_RESTRICT a,
                       const uint8_t* WEBP_RESTRICT r,
                       const uint8_t* WEBP_RESTRICT g,
                       const uint8_t* WEBP_RESTRICT b,
                       int len, uint32_t* WEBP_RESTRICT out) {
  int i;
  for (i = 0; i < len; ++i) {
    out[i] = MakeARGB32(a[4 * i], r[4 * i], g[4 * i], b[4 * i]);
  }
}
#endif

static void PackRGB_C(const uint8_t* WEBP_RESTRICT r,
                      const uint8_t* WEBP_RESTRICT g,
                      const uint8_t* WEBP_RESTRICT b,
                      int len, int step, uint32_t* WEBP_RESTRICT out) {
  int i, offset = 0;
  for (i = 0; i < len; ++i) {
    out[i] = MakeARGB32(0xff, r[offset], g[offset], b[offset]);
    offset += step;
  }
}

void (*WebPApplyAlphaMultiply)(uint8_t*, int, int, int, int);
void (*WebPApplyAlphaMultiply4444)(uint8_t*, int, int, int);
int (*WebPDispatchAlpha)(const uint8_t* WEBP_RESTRICT, int, int, int,
                         uint8_t* WEBP_RESTRICT, int);
void (*WebPDispatchAlphaToGreen)(const uint8_t* WEBP_RESTRICT, int, int, int,
                                 uint32_t* WEBP_RESTRICT, int);
int (*WebPExtractAlpha)(const uint8_t* WEBP_RESTRICT, int, int, int,
                        uint8_t* WEBP_RESTRICT, int);
void (*WebPExtractGreen)(const uint32_t* WEBP_RESTRICT argb,
                         uint8_t* WEBP_RESTRICT alpha, int size);
#ifdef WORDS_BIGENDIAN
void (*WebPPackARGB)(const uint8_t* a, const uint8_t* r, const uint8_t* g,
                     const uint8_t* b, int, uint32_t*);
#endif
void (*WebPPackRGB)(const uint8_t* WEBP_RESTRICT r,
                    const uint8_t* WEBP_RESTRICT g,
                    const uint8_t* WEBP_RESTRICT b,
                    int len, int step, uint32_t* WEBP_RESTRICT out);

int (*WebPHasAlpha8b)(const uint8_t* src, int length);
int (*WebPHasAlpha32b)(const uint8_t* src, int length);
void (*WebPAlphaReplace)(uint32_t* src, int length, uint32_t color);

extern VP8CPUInfo VP8GetCPUInfo;
extern void WebPInitAlphaProcessingMIPSdspR2(void);
extern void WebPInitAlphaProcessingSSE2(void);
extern void WebPInitAlphaProcessingSSE41(void);
extern void WebPInitAlphaProcessingNEON(void);

WEBP_DSP_INIT_FUNC(WebPInitAlphaProcessing) {
  WebPMultARGBRow = WebPMultARGBRow_C;
  WebPMultRow = WebPMultRow_C;
  WebPApplyAlphaMultiply4444 = ApplyAlphaMultiply_16b_C;

#ifdef WORDS_BIGENDIAN
  WebPPackARGB = PackARGB_C;
#endif
  WebPPackRGB = PackRGB_C;
#if !WEBP_NEON_OMIT_C_CODE
  WebPApplyAlphaMultiply = ApplyAlphaMultiply_C;
  WebPDispatchAlpha = DispatchAlpha_C;
  WebPDispatchAlphaToGreen = DispatchAlphaToGreen_C;
  WebPExtractAlpha = ExtractAlpha_C;
  WebPExtractGreen = ExtractGreen_C;
#endif

  WebPHasAlpha8b = HasAlpha8b_C;
  WebPHasAlpha32b = HasAlpha32b_C;
  WebPAlphaReplace = AlphaReplace_C;

  if (VP8GetCPUInfo != NULL) {
#if defined(WEBP_HAVE_SSE2)
    if (VP8GetCPUInfo(kSSE2)) {
      WebPInitAlphaProcessingSSE2();
#if defined(WEBP_HAVE_SSE41)
      if (VP8GetCPUInfo(kSSE4_1)) {
        WebPInitAlphaProcessingSSE41();
      }
#endif
    }
#endif
#if defined(WEBP_USE_MIPS_DSP_R2)
    if (VP8GetCPUInfo(kMIPSdspR2)) {
      WebPInitAlphaProcessingMIPSdspR2();
    }
#endif
  }

#if defined(WEBP_HAVE_NEON)
  if (WEBP_NEON_OMIT_C_CODE ||
      (VP8GetCPUInfo != NULL && VP8GetCPUInfo(kNEON))) {
    WebPInitAlphaProcessingNEON();
  }
#endif

  assert(WebPMultARGBRow != NULL);
  assert(WebPMultRow != NULL);
  assert(WebPApplyAlphaMultiply != NULL);
  assert(WebPApplyAlphaMultiply4444 != NULL);
  assert(WebPDispatchAlpha != NULL);
  assert(WebPDispatchAlphaToGreen != NULL);
  assert(WebPExtractAlpha != NULL);
  assert(WebPExtractGreen != NULL);
#ifdef WORDS_BIGENDIAN
  assert(WebPPackARGB != NULL);
#endif
  assert(WebPPackRGB != NULL);
  assert(WebPHasAlpha8b != NULL);
  assert(WebPHasAlpha32b != NULL);
  assert(WebPAlphaReplace != NULL);
}

#if defined(WEBP_USE_NEON)

#ifndef WEBP_DSP_NEON_H_
#define WEBP_DSP_NEON_H_

#if defined(WEBP_USE_NEON)

#include <arm_neon.h>

#if LOCAL_CLANG_PREREQ(3, 8) || LOCAL_GCC_PREREQ(4, 9) || WEBP_AARCH64
#define WEBP_USE_INTRINSICS
#endif

#define INIT_VECTOR2(v, a, b) do {  \
  v.val[0] = a;                     \
  v.val[1] = b;                     \
} while (0)

#define INIT_VECTOR3(v, a, b, c) do {  \
  v.val[0] = a;                        \
  v.val[1] = b;                        \
  v.val[2] = c;                        \
} while (0)

#define INIT_VECTOR4(v, a, b, c, d) do {  \
  v.val[0] = a;                           \
  v.val[1] = b;                           \
  v.val[2] = c;                           \
  v.val[3] = d;                           \
} while (0)

#if !(LOCAL_CLANG_PREREQ(3, 8) || LOCAL_GCC_PREREQ(4, 8) || WEBP_AARCH64)
#define WORK_AROUND_GCC
#endif

static WEBP_INLINE int32x4x4_t Transpose4x4_NEON(const int32x4x4_t rows) {
  uint64x2x2_t row01, row23;

  row01.val[0] = vreinterpretq_u64_s32(rows.val[0]);
  row01.val[1] = vreinterpretq_u64_s32(rows.val[1]);
  row23.val[0] = vreinterpretq_u64_s32(rows.val[2]);
  row23.val[1] = vreinterpretq_u64_s32(rows.val[3]);

  {
    const uint64x1_t row0h = vget_high_u64(row01.val[0]);
    const uint64x1_t row2l = vget_low_u64(row23.val[0]);
    const uint64x1_t row1h = vget_high_u64(row01.val[1]);
    const uint64x1_t row3l = vget_low_u64(row23.val[1]);
    row01.val[0] = vcombine_u64(vget_low_u64(row01.val[0]), row2l);
    row23.val[0] = vcombine_u64(row0h, vget_high_u64(row23.val[0]));
    row01.val[1] = vcombine_u64(vget_low_u64(row01.val[1]), row3l);
    row23.val[1] = vcombine_u64(row1h, vget_high_u64(row23.val[1]));
  }
  {
    const int32x4x2_t out01 = vtrnq_s32(vreinterpretq_s32_u64(row01.val[0]),
                                        vreinterpretq_s32_u64(row01.val[1]));
    const int32x4x2_t out23 = vtrnq_s32(vreinterpretq_s32_u64(row23.val[0]),
                                        vreinterpretq_s32_u64(row23.val[1]));
    int32x4x4_t out;
    out.val[0] = out01.val[0];
    out.val[1] = out01.val[1];
    out.val[2] = out23.val[0];
    out.val[3] = out23.val[1];
    return out;
  }
}

#if 0
#include <stdio.h>
#define PRINT_REG(REG, SIZE) do {                       \
  int i;                                                \
  printf("%s \t[%d]: 0x", #REG, SIZE);                  \
  if (SIZE == 8) {                                      \
    uint8_t _tmp[8];                                    \
    vst1_u8(_tmp, (REG));                               \
    for (i = 0; i < 8; ++i) printf("%.2x ", _tmp[i]);   \
  } else if (SIZE == 16) {                              \
    uint16_t _tmp[4];                                   \
    vst1_u16(_tmp, (REG));                              \
    for (i = 0; i < 4; ++i) printf("%.4x ", _tmp[i]);   \
  }                                                     \
  printf("\n");                                         \
} while (0)
#endif

#endif
#endif

#define MULTIPLIER(a) ((a) * 0x8081)
#define PREMULTIPLY(x, m) (((x) * (m)) >> 23)

#define MULTIPLY_BY_ALPHA(V, ALPHA, OTHER) do {                        \
  const uint8x8_t alpha = (V).val[(ALPHA)];                            \
  const uint16x8_t r1 = vmull_u8((V).val[1], alpha);                   \
  const uint16x8_t g1 = vmull_u8((V).val[2], alpha);                   \
  const uint16x8_t b1 = vmull_u8((V).val[(OTHER)], alpha);             \
                        \
  const uint16x8_t r2 = vsraq_n_u16(r1, r1, 8);                        \
  const uint16x8_t g2 = vsraq_n_u16(g1, g1, 8);                        \
  const uint16x8_t b2 = vsraq_n_u16(b1, b1, 8);                        \
  const uint16x8_t r3 = vaddq_u16(r2, kOne);                           \
  const uint16x8_t g3 = vaddq_u16(g2, kOne);                           \
  const uint16x8_t b3 = vaddq_u16(b2, kOne);                           \
  (V).val[1] = vshrn_n_u16(r3, 8);                                     \
  (V).val[2] = vshrn_n_u16(g3, 8);                                     \
  (V).val[(OTHER)] = vshrn_n_u16(b3, 8);                               \
} while (0)

static void ApplyAlphaMultiply_NEON(uint8_t* rgba, int alpha_first,
                                    int w, int h, int stride) {
  const uint16x8_t kOne = vdupq_n_u16(1u);
  while (h-- > 0) {
    uint32_t* const rgbx = (uint32_t*)rgba;
    int i = 0;
    if (alpha_first) {
      for (; i + 8 <= w; i += 8) {

        uint8x8x4_t RGBX = vld4_u8((const uint8_t*)(rgbx + i));
        MULTIPLY_BY_ALPHA(RGBX, 0, 3);
        vst4_u8((uint8_t*)(rgbx + i), RGBX);
      }
    } else {
      for (; i + 8 <= w; i += 8) {
        uint8x8x4_t RGBX = vld4_u8((const uint8_t*)(rgbx + i));
        MULTIPLY_BY_ALPHA(RGBX, 3, 0);
        vst4_u8((uint8_t*)(rgbx + i), RGBX);
      }
    }

    for (; i < w; ++i) {
      uint8_t* const rgb = rgba + (alpha_first ? 1 : 0);
      const uint8_t* const alpha = rgba + (alpha_first ? 0 : 3);
      const uint32_t a = alpha[4 * i];
      if (a != 0xff) {
        const uint32_t mult = MULTIPLIER(a);
        rgb[4 * i + 0] = PREMULTIPLY(rgb[4 * i + 0], mult);
        rgb[4 * i + 1] = PREMULTIPLY(rgb[4 * i + 1], mult);
        rgb[4 * i + 2] = PREMULTIPLY(rgb[4 * i + 2], mult);
      }
    }
    rgba += stride;
  }
}
#undef MULTIPLY_BY_ALPHA
#undef MULTIPLIER
#undef PREMULTIPLY

static int DispatchAlpha_NEON(const uint8_t* WEBP_RESTRICT alpha,
                              int alpha_stride, int width, int height,
                              uint8_t* WEBP_RESTRICT dst, int dst_stride) {
  uint32_t alpha_mask = 0xffu;
  uint8x8_t mask8 = vdup_n_u8(0xff);
  uint32_t tmp[2];
  int i, j;
  for (j = 0; j < height; ++j) {

    for (i = 0; i + 8 <= width - 1; i += 8) {
      uint8x8x4_t rgbX = vld4_u8((const uint8_t*)(dst + 4 * i));
      const uint8x8_t alphas = vld1_u8(alpha + i);
      rgbX.val[0] = alphas;
      vst4_u8((uint8_t*)(dst + 4 * i), rgbX);
      mask8 = vand_u8(mask8, alphas);
    }
    for (; i < width; ++i) {
      const uint32_t alpha_value = alpha[i];
      dst[4 * i] = alpha_value;
      alpha_mask &= alpha_value;
    }
    alpha += alpha_stride;
    dst += dst_stride;
  }
  vst1_u8((uint8_t*)tmp, mask8);
  alpha_mask *= 0x01010101;
  alpha_mask &= tmp[0];
  alpha_mask &= tmp[1];
  return (alpha_mask != 0xffffffffu);
}

static void DispatchAlphaToGreen_NEON(const uint8_t* WEBP_RESTRICT alpha,
                                      int alpha_stride, int width, int height,
                                      uint32_t* WEBP_RESTRICT dst,
                                      int dst_stride) {
  int i, j;
  uint8x8x4_t greens;
  greens.val[0] = vdup_n_u8(0);
  greens.val[2] = vdup_n_u8(0);
  greens.val[3] = vdup_n_u8(0);
  for (j = 0; j < height; ++j) {
    for (i = 0; i + 8 <= width; i += 8) {
      greens.val[1] = vld1_u8(alpha + i);
      vst4_u8((uint8_t*)(dst + i), greens);
    }
    for (; i < width; ++i) dst[i] = alpha[i] << 8;
    alpha += alpha_stride;
    dst += dst_stride;
  }
}

static int ExtractAlpha_NEON(const uint8_t* WEBP_RESTRICT argb, int argb_stride,
                             int width, int height,
                             uint8_t* WEBP_RESTRICT alpha, int alpha_stride) {
  uint32_t alpha_mask = 0xffu;
  uint8x8_t mask8 = vdup_n_u8(0xff);
  uint32_t tmp[2];
  int i, j;
  for (j = 0; j < height; ++j) {

    for (i = 0; i + 8 <= width - 1; i += 8) {
      const uint8x8x4_t rgbX = vld4_u8((const uint8_t*)(argb + 4 * i));
      const uint8x8_t alphas = rgbX.val[0];
      vst1_u8((uint8_t*)(alpha + i), alphas);
      mask8 = vand_u8(mask8, alphas);
    }
    for (; i < width; ++i) {
      alpha[i] = argb[4 * i];
      alpha_mask &= alpha[i];
    }
    argb += argb_stride;
    alpha += alpha_stride;
  }
  vst1_u8((uint8_t*)tmp, mask8);
  alpha_mask *= 0x01010101;
  alpha_mask &= tmp[0];
  alpha_mask &= tmp[1];
  return (alpha_mask == 0xffffffffu);
}

static void ExtractGreen_NEON(const uint32_t* WEBP_RESTRICT argb,
                              uint8_t* WEBP_RESTRICT alpha, int size) {
  int i;
  for (i = 0; i + 16 <= size; i += 16) {
    const uint8x16x4_t rgbX = vld4q_u8((const uint8_t*)(argb + i));
    const uint8x16_t greens = rgbX.val[1];
    vst1q_u8(alpha + i, greens);
  }
  for (; i < size; ++i) alpha[i] = (argb[i] >> 8) & 0xff;
}

extern void WebPInitAlphaProcessingNEON(void);

WEBP_TSAN_IGNORE_FUNCTION void WebPInitAlphaProcessingNEON(void) {
  WebPApplyAlphaMultiply = ApplyAlphaMultiply_NEON;
  WebPDispatchAlpha = DispatchAlpha_NEON;
  WebPDispatchAlphaToGreen = DispatchAlphaToGreen_NEON;
  WebPExtractAlpha = ExtractAlpha_NEON;
  WebPExtractGreen = ExtractGreen_NEON;
}

#else

WEBP_DSP_INIT_STUB(WebPInitAlphaProcessingNEON)

#endif

#if defined(WEBP_USE_SSE2)
#include <emmintrin.h>

static int DispatchAlpha_SSE2(const uint8_t* WEBP_RESTRICT alpha,
                              int alpha_stride, int width, int height,
                              uint8_t* WEBP_RESTRICT dst, int dst_stride) {

  uint32_t alpha_and = 0xff;
  int i, j;
  const __m128i zero = _mm_setzero_si128();
  const __m128i alpha_mask = _mm_set1_epi32((int)0xff);
  const __m128i all_0xff = _mm_set1_epi8((char)0xff);
  __m128i all_alphas16 = all_0xff;
  __m128i all_alphas8 = all_0xff;

  for (j = 0; j < height; ++j) {
    char* ptr = (char*)dst;
    for (i = 0; i + 16 <= width - 1; i += 16) {

      const __m128i a0 = _mm_loadu_si128((const __m128i*)&alpha[i]);
      const __m128i a1_lo = _mm_unpacklo_epi8(a0, zero);
      const __m128i a1_hi = _mm_unpackhi_epi8(a0, zero);
      const __m128i a2_lo_lo = _mm_unpacklo_epi16(a1_lo, zero);
      const __m128i a2_lo_hi = _mm_unpackhi_epi16(a1_lo, zero);
      const __m128i a2_hi_lo = _mm_unpacklo_epi16(a1_hi, zero);
      const __m128i a2_hi_hi = _mm_unpackhi_epi16(a1_hi, zero);
      _mm_maskmoveu_si128(a2_lo_lo, alpha_mask, ptr + 0);
      _mm_maskmoveu_si128(a2_lo_hi, alpha_mask, ptr + 16);
      _mm_maskmoveu_si128(a2_hi_lo, alpha_mask, ptr + 32);
      _mm_maskmoveu_si128(a2_hi_hi, alpha_mask, ptr + 48);

      all_alphas16 = _mm_and_si128(all_alphas16, a0);
      ptr += 64;
    }
    if (i + 8 <= width - 1) {

      const __m128i a0 = _mm_loadl_epi64((const __m128i*)&alpha[i]);
      const __m128i a1 = _mm_unpacklo_epi8(a0, zero);
      const __m128i a2_lo = _mm_unpacklo_epi16(a1, zero);
      const __m128i a2_hi = _mm_unpackhi_epi16(a1, zero);
      _mm_maskmoveu_si128(a2_lo, alpha_mask, ptr);
      _mm_maskmoveu_si128(a2_hi, alpha_mask, ptr + 16);

      all_alphas8 = _mm_and_si128(all_alphas8, a0);
      i += 8;
    }
    for (; i < width; ++i) {
      const uint32_t alpha_value = alpha[i];
      dst[4 * i] = alpha_value;
      alpha_and &= alpha_value;
    }
    alpha += alpha_stride;
    dst += dst_stride;
  }

  alpha_and &= _mm_movemask_epi8(_mm_cmpeq_epi8(all_alphas8, all_0xff)) & 0xff;
  return (alpha_and != 0xff ||
          _mm_movemask_epi8(_mm_cmpeq_epi8(all_alphas16, all_0xff)) != 0xffff);
}

static void DispatchAlphaToGreen_SSE2(const uint8_t* WEBP_RESTRICT alpha,
                                      int alpha_stride, int width, int height,
                                      uint32_t* WEBP_RESTRICT dst,
                                      int dst_stride) {
  int i, j;
  const __m128i zero = _mm_setzero_si128();
  const int limit = width & ~15;
  for (j = 0; j < height; ++j) {
    for (i = 0; i < limit; i += 16) {
      const __m128i a0 = _mm_loadu_si128((const __m128i*)&alpha[i]);
      const __m128i a1 = _mm_unpacklo_epi8(zero, a0);
      const __m128i b1 = _mm_unpackhi_epi8(zero, a0);
      const __m128i a2_lo = _mm_unpacklo_epi16(a1, zero);
      const __m128i b2_lo = _mm_unpacklo_epi16(b1, zero);
      const __m128i a2_hi = _mm_unpackhi_epi16(a1, zero);
      const __m128i b2_hi = _mm_unpackhi_epi16(b1, zero);
      _mm_storeu_si128((__m128i*)&dst[i +  0], a2_lo);
      _mm_storeu_si128((__m128i*)&dst[i +  4], a2_hi);
      _mm_storeu_si128((__m128i*)&dst[i +  8], b2_lo);
      _mm_storeu_si128((__m128i*)&dst[i + 12], b2_hi);
    }
    for (; i < width; ++i) dst[i] = alpha[i] << 8;
    alpha += alpha_stride;
    dst += dst_stride;
  }
}

static int ExtractAlpha_SSE2(const uint8_t* WEBP_RESTRICT argb, int argb_stride,
                             int width, int height,
                             uint8_t* WEBP_RESTRICT alpha, int alpha_stride) {

  uint32_t alpha_and = 0xff;
  int i, j;
  const __m128i a_mask = _mm_set1_epi32(0xff);
  const __m128i all_0xff = _mm_set_epi32(0, 0, ~0, ~0);
  __m128i all_alphas = all_0xff;

  const int limit = (width - 1) & ~7;

  for (j = 0; j < height; ++j) {
    const __m128i* src = (const __m128i*)argb;
    for (i = 0; i < limit; i += 8) {

      const __m128i a0 = _mm_loadu_si128(src + 0);
      const __m128i a1 = _mm_loadu_si128(src + 1);
      const __m128i b0 = _mm_and_si128(a0, a_mask);
      const __m128i b1 = _mm_and_si128(a1, a_mask);
      const __m128i c0 = _mm_packs_epi32(b0, b1);
      const __m128i d0 = _mm_packus_epi16(c0, c0);

      _mm_storel_epi64((__m128i*)&alpha[i], d0);

      all_alphas = _mm_and_si128(all_alphas, d0);
      src += 2;
    }
    for (; i < width; ++i) {
      const uint32_t alpha_value = argb[4 * i];
      alpha[i] = alpha_value;
      alpha_and &= alpha_value;
    }
    argb += argb_stride;
    alpha += alpha_stride;
  }

  alpha_and &= _mm_movemask_epi8(_mm_cmpeq_epi8(all_alphas, all_0xff));
  return (alpha_and == 0xff);
}

static void ExtractGreen_SSE2(const uint32_t* WEBP_RESTRICT argb,
                              uint8_t* WEBP_RESTRICT alpha, int size) {
  int i;
  const __m128i mask = _mm_set1_epi32(0xff);
  const __m128i* src = (const __m128i*)argb;

  for (i = 0; i + 16 <= size; i += 16, src += 4) {
    const __m128i a0 = _mm_loadu_si128(src + 0);
    const __m128i a1 = _mm_loadu_si128(src + 1);
    const __m128i a2 = _mm_loadu_si128(src + 2);
    const __m128i a3 = _mm_loadu_si128(src + 3);
    const __m128i b0 = _mm_srli_epi32(a0, 8);
    const __m128i b1 = _mm_srli_epi32(a1, 8);
    const __m128i b2 = _mm_srli_epi32(a2, 8);
    const __m128i b3 = _mm_srli_epi32(a3, 8);
    const __m128i c0 = _mm_and_si128(b0, mask);
    const __m128i c1 = _mm_and_si128(b1, mask);
    const __m128i c2 = _mm_and_si128(b2, mask);
    const __m128i c3 = _mm_and_si128(b3, mask);
    const __m128i d0 = _mm_packs_epi32(c0, c1);
    const __m128i d1 = _mm_packs_epi32(c2, c3);
    const __m128i e = _mm_packus_epi16(d0, d1);

    _mm_storeu_si128((__m128i*)&alpha[i], e);
  }
  if (i + 8 <= size) {
    const __m128i a0 = _mm_loadu_si128(src + 0);
    const __m128i a1 = _mm_loadu_si128(src + 1);
    const __m128i b0 = _mm_srli_epi32(a0, 8);
    const __m128i b1 = _mm_srli_epi32(a1, 8);
    const __m128i c0 = _mm_and_si128(b0, mask);
    const __m128i c1 = _mm_and_si128(b1, mask);
    const __m128i d = _mm_packs_epi32(c0, c1);
    const __m128i e = _mm_packus_epi16(d, d);
    _mm_storel_epi64((__m128i*)&alpha[i], e);
    i += 8;
  }
  for (; i < size; ++i) alpha[i] = argb[i] >> 8;
}

#define MULTIPLIER(a)   ((a) * 0x8081)
#define PREMULTIPLY(x, m) (((x) * (m)) >> 23)

#define APPLY_ALPHA(RGBX, SHUFFLE) do {                              \
  const __m128i argb0 = _mm_loadu_si128((const __m128i*)&(RGBX));    \
  const __m128i argb1_lo = _mm_unpacklo_epi8(argb0, zero);           \
  const __m128i argb1_hi = _mm_unpackhi_epi8(argb0, zero);           \
  const __m128i alpha0_lo = _mm_or_si128(argb1_lo, kMask);           \
  const __m128i alpha0_hi = _mm_or_si128(argb1_hi, kMask);           \
  const __m128i alpha1_lo = _mm_shufflelo_epi16(alpha0_lo, SHUFFLE); \
  const __m128i alpha1_hi = _mm_shufflelo_epi16(alpha0_hi, SHUFFLE); \
  const __m128i alpha2_lo = _mm_shufflehi_epi16(alpha1_lo, SHUFFLE); \
  const __m128i alpha2_hi = _mm_shufflehi_epi16(alpha1_hi, SHUFFLE); \
                            \
  const __m128i A0_lo = _mm_mullo_epi16(alpha2_lo, argb1_lo);        \
  const __m128i A0_hi = _mm_mullo_epi16(alpha2_hi, argb1_hi);        \
  const __m128i A1_lo = _mm_mulhi_epu16(A0_lo, kMult);               \
  const __m128i A1_hi = _mm_mulhi_epu16(A0_hi, kMult);               \
  const __m128i A2_lo = _mm_srli_epi16(A1_lo, 7);                    \
  const __m128i A2_hi = _mm_srli_epi16(A1_hi, 7);                    \
  const __m128i A3 = _mm_packus_epi16(A2_lo, A2_hi);                 \
  _mm_storeu_si128((__m128i*)&(RGBX), A3);                           \
} while (0)

static void ApplyAlphaMultiply_SSE2(uint8_t* rgba, int alpha_first,
                                    int w, int h, int stride) {
  const __m128i zero = _mm_setzero_si128();
  const __m128i kMult = _mm_set1_epi16((short)0x8081);
  const __m128i kMask = _mm_set_epi16(0, 0xff, 0xff, 0, 0, 0xff, 0xff, 0);
  const int kSpan = 4;
  while (h-- > 0) {
    uint32_t* const rgbx = (uint32_t*)rgba;
    int i;
    if (!alpha_first) {
      for (i = 0; i + kSpan <= w; i += kSpan) {
        APPLY_ALPHA(rgbx[i], _MM_SHUFFLE(2, 3, 3, 3));
      }
    } else {
      for (i = 0; i + kSpan <= w; i += kSpan) {
        APPLY_ALPHA(rgbx[i], _MM_SHUFFLE(0, 0, 0, 1));
      }
    }

    for (; i < w; ++i) {
      uint8_t* const rgb = rgba + (alpha_first ? 1 : 0);
      const uint8_t* const alpha = rgba + (alpha_first ? 0 : 3);
      const uint32_t a = alpha[4 * i];
      if (a != 0xff) {
        const uint32_t mult = MULTIPLIER(a);
        rgb[4 * i + 0] = PREMULTIPLY(rgb[4 * i + 0], mult);
        rgb[4 * i + 1] = PREMULTIPLY(rgb[4 * i + 1], mult);
        rgb[4 * i + 2] = PREMULTIPLY(rgb[4 * i + 2], mult);
      }
    }
    rgba += stride;
  }
}
#undef MULTIPLIER
#undef PREMULTIPLY

static int HasAlpha8b_SSE2(const uint8_t* src, int length) {
  const __m128i all_0xff = _mm_set1_epi8((char)0xff);
  int i = 0;
  for (; i + 16 <= length; i += 16) {
    const __m128i v = _mm_loadu_si128((const __m128i*)(src + i));
    const __m128i bits = _mm_cmpeq_epi8(v, all_0xff);
    const int mask = _mm_movemask_epi8(bits);
    if (mask != 0xffff) return 1;
  }
  for (; i < length; ++i) if (src[i] != 0xff) return 1;
  return 0;
}

static int HasAlpha32b_SSE2(const uint8_t* src, int length) {
  const __m128i alpha_mask = _mm_set1_epi32(0xff);
  const __m128i all_0xff = _mm_set1_epi8((char)0xff);
  int i = 0;

  length = length * 4 - 3;
  for (; i + 64 <= length; i += 64) {
    const __m128i a0 = _mm_loadu_si128((const __m128i*)(src + i +  0));
    const __m128i a1 = _mm_loadu_si128((const __m128i*)(src + i + 16));
    const __m128i a2 = _mm_loadu_si128((const __m128i*)(src + i + 32));
    const __m128i a3 = _mm_loadu_si128((const __m128i*)(src + i + 48));
    const __m128i b0 = _mm_and_si128(a0, alpha_mask);
    const __m128i b1 = _mm_and_si128(a1, alpha_mask);
    const __m128i b2 = _mm_and_si128(a2, alpha_mask);
    const __m128i b3 = _mm_and_si128(a3, alpha_mask);
    const __m128i c0 = _mm_packs_epi32(b0, b1);
    const __m128i c1 = _mm_packs_epi32(b2, b3);
    const __m128i d  = _mm_packus_epi16(c0, c1);
    const __m128i bits = _mm_cmpeq_epi8(d, all_0xff);
    const int mask = _mm_movemask_epi8(bits);
    if (mask != 0xffff) return 1;
  }
  for (; i + 32 <= length; i += 32) {
    const __m128i a0 = _mm_loadu_si128((const __m128i*)(src + i +  0));
    const __m128i a1 = _mm_loadu_si128((const __m128i*)(src + i + 16));
    const __m128i b0 = _mm_and_si128(a0, alpha_mask);
    const __m128i b1 = _mm_and_si128(a1, alpha_mask);
    const __m128i c  = _mm_packs_epi32(b0, b1);
    const __m128i d  = _mm_packus_epi16(c, c);
    const __m128i bits = _mm_cmpeq_epi8(d, all_0xff);
    const int mask = _mm_movemask_epi8(bits);
    if (mask != 0xffff) return 1;
  }
  for (; i <= length; i += 4) if (src[i] != 0xff) return 1;
  return 0;
}

static void AlphaReplace_SSE2(uint32_t* src, int length, uint32_t color) {
  const __m128i m_color = _mm_set1_epi32((int)color);
  const __m128i zero = _mm_setzero_si128();
  int i = 0;
  for (; i + 8 <= length; i += 8) {
    const __m128i a0 = _mm_loadu_si128((const __m128i*)(src + i + 0));
    const __m128i a1 = _mm_loadu_si128((const __m128i*)(src + i + 4));
    const __m128i b0 = _mm_srai_epi32(a0, 24);
    const __m128i b1 = _mm_srai_epi32(a1, 24);
    const __m128i c0 = _mm_cmpeq_epi32(b0, zero);
    const __m128i c1 = _mm_cmpeq_epi32(b1, zero);
    const __m128i d0 = _mm_and_si128(c0, m_color);
    const __m128i d1 = _mm_and_si128(c1, m_color);
    const __m128i e0 = _mm_andnot_si128(c0, a0);
    const __m128i e1 = _mm_andnot_si128(c1, a1);
    _mm_storeu_si128((__m128i*)(src + i + 0), _mm_or_si128(d0, e0));
    _mm_storeu_si128((__m128i*)(src + i + 4), _mm_or_si128(d1, e1));
  }
  for (; i < length; ++i) if ((src[i] >> 24) == 0) src[i] = color;
}

static void MultARGBRow_SSE2(uint32_t* const ptr, int width, int inverse) {
  int x = 0;
  if (!inverse) {
    const int kSpan = 2;
    const __m128i zero = _mm_setzero_si128();
    const __m128i k128 = _mm_set1_epi16(128);
    const __m128i kMult = _mm_set1_epi16(0x0101);
    const __m128i kMask = _mm_set_epi16(0, 0xff, 0, 0, 0, 0xff, 0, 0);
    for (x = 0; x + kSpan <= width; x += kSpan) {

      const __m128i A0 = _mm_loadl_epi64((const __m128i*)&ptr[x]);
      const __m128i A1 = _mm_unpacklo_epi8(A0, zero);
      const __m128i A2 = _mm_or_si128(A1, kMask);
      const __m128i A3 = _mm_shufflelo_epi16(A2, _MM_SHUFFLE(2, 3, 3, 3));
      const __m128i A4 = _mm_shufflehi_epi16(A3, _MM_SHUFFLE(2, 3, 3, 3));

      const __m128i A5 = _mm_mullo_epi16(A4, A1);
      const __m128i A6 = _mm_add_epi16(A5, k128);
      const __m128i A7 = _mm_mulhi_epu16(A6, kMult);
      const __m128i A10 = _mm_packus_epi16(A7, zero);
      _mm_storel_epi64((__m128i*)&ptr[x], A10);
    }
  }
  width -= x;
  if (width > 0) WebPMultARGBRow_C(ptr + x, width, inverse);
}

static void MultRow_SSE2(uint8_t* WEBP_RESTRICT const ptr,
                         const uint8_t* WEBP_RESTRICT const alpha,
                         int width, int inverse) {
  int x = 0;
  if (!inverse) {
    const __m128i zero = _mm_setzero_si128();
    const __m128i k128 = _mm_set1_epi16(128);
    const __m128i kMult = _mm_set1_epi16(0x0101);
    for (x = 0; x + 8 <= width; x += 8) {
      const __m128i v0 = _mm_loadl_epi64((__m128i*)&ptr[x]);
      const __m128i a0 = _mm_loadl_epi64((const __m128i*)&alpha[x]);
      const __m128i v1 = _mm_unpacklo_epi8(v0, zero);
      const __m128i a1 = _mm_unpacklo_epi8(a0, zero);
      const __m128i v2 = _mm_mullo_epi16(v1, a1);
      const __m128i v3 = _mm_add_epi16(v2, k128);
      const __m128i v4 = _mm_mulhi_epu16(v3, kMult);
      const __m128i v5 = _mm_packus_epi16(v4, zero);
      _mm_storel_epi64((__m128i*)&ptr[x], v5);
    }
  }
  width -= x;
  if (width > 0) WebPMultRow_C(ptr + x, alpha + x, width, inverse);
}

extern void WebPInitAlphaProcessingSSE2(void);

WEBP_TSAN_IGNORE_FUNCTION void WebPInitAlphaProcessingSSE2(void) {
  WebPMultARGBRow = MultARGBRow_SSE2;
  WebPMultRow = MultRow_SSE2;
  WebPApplyAlphaMultiply = ApplyAlphaMultiply_SSE2;
  WebPDispatchAlpha = DispatchAlpha_SSE2;
  WebPDispatchAlphaToGreen = DispatchAlphaToGreen_SSE2;
  WebPExtractAlpha = ExtractAlpha_SSE2;
  WebPExtractGreen = ExtractGreen_SSE2;

  WebPHasAlpha8b = HasAlpha8b_SSE2;
  WebPHasAlpha32b = HasAlpha32b_SSE2;
  WebPAlphaReplace = AlphaReplace_SSE2;
}

#else

WEBP_DSP_INIT_STUB(WebPInitAlphaProcessingSSE2)

#endif

#if defined(WEBP_USE_SSE41)
#include <emmintrin.h>
#include <smmintrin.h>

static int ExtractAlpha_SSE41(const uint8_t* WEBP_RESTRICT argb,
                              int argb_stride, int width, int height,
                              uint8_t* WEBP_RESTRICT alpha, int alpha_stride) {

  uint32_t alpha_and = 0xff;
  int i, j;
  const __m128i all_0xff = _mm_set1_epi32(~0);
  __m128i all_alphas = all_0xff;

  const int limit = (width - 1) & ~15;
  const __m128i kCstAlpha0 = _mm_set_epi8(-1, -1, -1, -1, -1, -1, -1, -1,
                                          -1, -1, -1, -1, 12, 8, 4, 0);
  const __m128i kCstAlpha1 = _mm_set_epi8(-1, -1, -1, -1, -1, -1, -1, -1,
                                          12, 8, 4, 0, -1, -1, -1, -1);
  const __m128i kCstAlpha2 = _mm_set_epi8(-1, -1, -1, -1, 12, 8, 4, 0,
                                          -1, -1, -1, -1, -1, -1, -1, -1);
  const __m128i kCstAlpha3 = _mm_set_epi8(12, 8, 4, 0, -1, -1, -1, -1,
                                          -1, -1, -1, -1, -1, -1, -1, -1);
  for (j = 0; j < height; ++j) {
    const __m128i* src = (const __m128i*)argb;
    for (i = 0; i < limit; i += 16) {

      const __m128i a0 = _mm_loadu_si128(src + 0);
      const __m128i a1 = _mm_loadu_si128(src + 1);
      const __m128i a2 = _mm_loadu_si128(src + 2);
      const __m128i a3 = _mm_loadu_si128(src + 3);
      const __m128i b0 = _mm_shuffle_epi8(a0, kCstAlpha0);
      const __m128i b1 = _mm_shuffle_epi8(a1, kCstAlpha1);
      const __m128i b2 = _mm_shuffle_epi8(a2, kCstAlpha2);
      const __m128i b3 = _mm_shuffle_epi8(a3, kCstAlpha3);
      const __m128i c0 = _mm_or_si128(b0, b1);
      const __m128i c1 = _mm_or_si128(b2, b3);
      const __m128i d0 = _mm_or_si128(c0, c1);

      _mm_storeu_si128((__m128i*)&alpha[i], d0);

      all_alphas = _mm_and_si128(all_alphas, d0);
      src += 4;
    }
    for (; i < width; ++i) {
      const uint32_t alpha_value = argb[4 * i];
      alpha[i] = alpha_value;
      alpha_and &= alpha_value;
    }
    argb += argb_stride;
    alpha += alpha_stride;
  }

  alpha_and |= 0xff00u;
  alpha_and &= _mm_movemask_epi8(_mm_cmpeq_epi8(all_alphas, all_0xff));
  return (alpha_and == 0xffffu);
}

extern void WebPInitAlphaProcessingSSE41(void);

WEBP_TSAN_IGNORE_FUNCTION void WebPInitAlphaProcessingSSE41(void) {
  WebPExtractAlpha = ExtractAlpha_SSE41;
}

#else

WEBP_DSP_INIT_STUB(WebPInitAlphaProcessingSSE41)

#endif

#if defined(WEBP_HAVE_NEON_RTCD)
#include <stdio.h>
#include <string.h>
#endif

#if defined(WEBP_ANDROID_NEON)
#include <cpu-features.h>
#endif

#include <stddef.h>

#if (defined(__pic__) || defined(__PIC__)) && defined(__i386__)
static WEBP_INLINE void GetCPUInfo(int cpu_info[4], int info_type) {
  __asm__ volatile (
    "mov %%ebx, %%edi\n"
    "cpuid\n"
    "xchg %%edi, %%ebx\n"
    : "=a"(cpu_info[0]), "=D"(cpu_info[1]), "=c"(cpu_info[2]), "=d"(cpu_info[3])
    : "a"(info_type), "c"(0));
}
#elif defined(__i386__) || defined(__x86_64__)
static WEBP_INLINE void GetCPUInfo(int cpu_info[4], int info_type) {
  __asm__ volatile (
    "cpuid\n"
    : "=a"(cpu_info[0]), "=b"(cpu_info[1]), "=c"(cpu_info[2]), "=d"(cpu_info[3])
    : "a"(info_type), "c"(0));
}
#elif defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))

#if defined(_MSC_FULL_VER) && _MSC_FULL_VER >= 150030729
#include <intrin.h>
#define GetCPUInfo(info, type) __cpuidex(info, type, 0)
#define WEBP_HAVE_MSC_CPUID
#elif _MSC_VER > 1310
#include <intrin.h>
#define GetCPUInfo __cpuid
#define WEBP_HAVE_MSC_CPUID
#endif

#endif

#if !defined(__native_client__) && (defined(__i386__) || defined(__x86_64__))
static WEBP_INLINE uint64_t xgetbv(void) {
  const uint32_t ecx = 0;
  uint32_t eax, edx;

  __asm__ volatile (
    ".byte 0x0f, 0x01, 0xd0\n"
    : "=a"(eax), "=d"(edx) : "c" (ecx));
  return ((uint64_t)edx << 32) | eax;
}
#elif (defined(_M_X64) || defined(_M_IX86)) && \
      defined(_MSC_FULL_VER) && _MSC_FULL_VER >= 160040219
#include <immintrin.h>
#define xgetbv() _xgetbv(0)
#elif defined(_MSC_VER) && defined(_M_IX86)
static WEBP_INLINE uint64_t xgetbv(void) {
  uint32_t eax_, edx_;
  __asm {
    xor ecx, ecx

    __asm _emit 0x0f __asm _emit 0x01 __asm _emit 0xd0
    mov eax_, eax
    mov edx_, edx
  }
  return ((uint64_t)edx_ << 32) | eax_;
}
#else
#define xgetbv() 0U
#endif

#if defined(__i386__) || defined(__x86_64__) || defined(WEBP_HAVE_MSC_CPUID)

static int CheckSlowModel(int info) {

  static const uint8_t kSlowModels[] = {
    0x37, 0x4a, 0x4d,
    0x1c, 0x26, 0x27
  };
  const uint32_t model = ((info & 0xf0000) >> 12) | ((info >> 4) & 0xf);
  const uint32_t family = (info >> 8) & 0xf;
  if (family == 0x06) {
    size_t i;
    for (i = 0; i < sizeof(kSlowModels) / sizeof(kSlowModels[0]); ++i) {
      if (model == kSlowModels[i]) return 1;
    }
  }
  return 0;
}

static int x86CPUInfo(CPUFeature feature) {
  int max_cpuid_value;
  int cpu_info[4];
  int is_intel = 0;

  GetCPUInfo(cpu_info, 0);
  max_cpuid_value = cpu_info[0];
  if (max_cpuid_value < 1) {
    return 0;
  } else {
    const int VENDOR_ID_INTEL_EBX = 0x756e6547;
    const int VENDOR_ID_INTEL_EDX = 0x49656e69;
    const int VENDOR_ID_INTEL_ECX = 0x6c65746e;
    is_intel = (cpu_info[1] == VENDOR_ID_INTEL_EBX &&
                cpu_info[2] == VENDOR_ID_INTEL_ECX &&
                cpu_info[3] == VENDOR_ID_INTEL_EDX);
  }

  GetCPUInfo(cpu_info, 1);
  if (feature == kSSE2) {
    return !!(cpu_info[3] & (1 << 26));
  }
  if (feature == kSSE3) {
    return !!(cpu_info[2] & (1 << 0));
  }
  if (feature == kSlowSSSE3) {
    if (is_intel && (cpu_info[2] & (1 << 9))) {
      return CheckSlowModel(cpu_info[0]);
    }
    return 0;
  }

  if (feature == kSSE4_1) {
    return !!(cpu_info[2] & (1 << 19));
  }
  if (feature == kAVX) {

    if ((cpu_info[2] & 0x18000000) == 0x18000000) {

      return (xgetbv() & 0x6) == 0x6;
    }
  }
  if (feature == kAVX2) {
    if (x86CPUInfo(kAVX) && max_cpuid_value >= 7) {
      GetCPUInfo(cpu_info, 7);
      return !!(cpu_info[1] & (1 << 5));
    }
  }
  return 0;
}
WEBP_EXTERN VP8CPUInfo VP8GetCPUInfo;
VP8CPUInfo VP8GetCPUInfo = x86CPUInfo;
#elif defined(WEBP_ANDROID_NEON)
static int AndroidCPUInfo(CPUFeature feature) {
  const AndroidCpuFamily cpu_family = android_getCpuFamily();
  const uint64_t cpu_features = android_getCpuFeatures();
  if (feature == kNEON) {
    return cpu_family == ANDROID_CPU_FAMILY_ARM &&
           (cpu_features & ANDROID_CPU_ARM_FEATURE_NEON) != 0;
  }
  return 0;
}
WEBP_EXTERN VP8CPUInfo VP8GetCPUInfo;
VP8CPUInfo VP8GetCPUInfo = AndroidCPUInfo;
#elif defined(EMSCRIPTEN)

static int wasmCPUInfo(CPUFeature feature) {
  switch (feature) {
#ifdef WEBP_HAVE_SSE2
    case kSSE2:
      return 1;
#endif
#ifdef WEBP_HAVE_SSE41
    case kSSE3:
    case kSlowSSSE3:
    case kSSE4_1:
      return 1;
#endif
#ifdef WEBP_HAVE_NEON
    case kNEON:
      return 1;
#endif
    default:
      break;
  }
  return 0;
}
WEBP_EXTERN VP8CPUInfo VP8GetCPUInfo;
VP8CPUInfo VP8GetCPUInfo = wasmCPUInfo;
#elif defined(WEBP_HAVE_NEON)

static int armCPUInfo(CPUFeature feature) {
  if (feature != kNEON) return 0;
#if defined(__linux__) && defined(WEBP_HAVE_NEON_RTCD)
  {
    int has_neon = 0;
    char line[200];
    FILE* const cpuinfo = fopen("/proc/cpuinfo", "r");
    if (cpuinfo == NULL) return 0;
    while (fgets(line, sizeof(line), cpuinfo)) {
      if (!strncmp(line, "Features", 8)) {
        if (strstr(line, " neon ") != NULL) {
          has_neon = 1;
          break;
        }
      }
    }
    fclose(cpuinfo);
    return has_neon;
  }
#else
  return 1;
#endif
}
WEBP_EXTERN VP8CPUInfo VP8GetCPUInfo;
VP8CPUInfo VP8GetCPUInfo = armCPUInfo;
#elif defined(WEBP_USE_MIPS32) || defined(WEBP_USE_MIPS_DSP_R2) || \
      defined(WEBP_USE_MSA)
static int mipsCPUInfo(CPUFeature feature) {
  if ((feature == kMIPS32) || (feature == kMIPSdspR2) || (feature == kMSA)) {
    return 1;
  } else {
    return 0;
  }

}
WEBP_EXTERN VP8CPUInfo VP8GetCPUInfo;
VP8CPUInfo VP8GetCPUInfo = mipsCPUInfo;
#else
WEBP_EXTERN VP8CPUInfo VP8GetCPUInfo;
VP8CPUInfo VP8GetCPUInfo = NULL;
#endif

#include <assert.h>
#include <stddef.h>
#include <string.h>

static WEBP_INLINE uint8_t clip_8b(int v) {
  return (!(v & ~0xff)) ? v : (v < 0) ? 0 : 255;
}

#define STORE(x, y, v) \
  dst[(x) + (y) * BPS] = clip_8b(dst[(x) + (y) * BPS] + ((v) >> 3))

#define STORE2(y, dc, d, c) do {    \
  const int DC = (dc);              \
  STORE(0, y, DC + (d));            \
  STORE(1, y, DC + (c));            \
  STORE(2, y, DC - (c));            \
  STORE(3, y, DC - (d));            \
} while (0)

#if !WEBP_NEON_OMIT_C_CODE
static void TransformOne_C(const int16_t* WEBP_RESTRICT in,
                           uint8_t* WEBP_RESTRICT dst) {
  int C[4 * 4], *tmp;
  int i;
  tmp = C;
  for (i = 0; i < 4; ++i) {
    const int a = in[0] + in[8];
    const int b = in[0] - in[8];
    const int c = WEBP_TRANSFORM_AC3_MUL2(in[4]) -
                  WEBP_TRANSFORM_AC3_MUL1(in[12]);
    const int d = WEBP_TRANSFORM_AC3_MUL1(in[4]) +
                  WEBP_TRANSFORM_AC3_MUL2(in[12]);
    tmp[0] = a + d;
    tmp[1] = b + c;
    tmp[2] = b - c;
    tmp[3] = a - d;
    tmp += 4;
    in++;
  }

  tmp = C;
  for (i = 0; i < 4; ++i) {
    const int dc = tmp[0] + 4;
    const int a =  dc +  tmp[8];
    const int b =  dc -  tmp[8];
    const int c =
        WEBP_TRANSFORM_AC3_MUL2(tmp[4]) - WEBP_TRANSFORM_AC3_MUL1(tmp[12]);
    const int d =
        WEBP_TRANSFORM_AC3_MUL1(tmp[4]) + WEBP_TRANSFORM_AC3_MUL2(tmp[12]);
    STORE(0, 0, a + d);
    STORE(1, 0, b + c);
    STORE(2, 0, b - c);
    STORE(3, 0, a - d);
    tmp++;
    dst += BPS;
  }
}

static void TransformAC3_C(const int16_t* WEBP_RESTRICT in,
                           uint8_t* WEBP_RESTRICT dst) {
  const int a = in[0] + 4;
  const int c4 = WEBP_TRANSFORM_AC3_MUL2(in[4]);
  const int d4 = WEBP_TRANSFORM_AC3_MUL1(in[4]);
  const int c1 = WEBP_TRANSFORM_AC3_MUL2(in[1]);
  const int d1 = WEBP_TRANSFORM_AC3_MUL1(in[1]);
  STORE2(0, a + d4, d1, c1);
  STORE2(1, a + c4, d1, c1);
  STORE2(2, a - c4, d1, c1);
  STORE2(3, a - d4, d1, c1);
}
#undef STORE2

static void TransformTwo_C(const int16_t* WEBP_RESTRICT in,
                           uint8_t* WEBP_RESTRICT dst, int do_two) {
  TransformOne_C(in, dst);
  if (do_two) {
    TransformOne_C(in + 16, dst + 4);
  }
}
#endif

static void TransformUV_C(const int16_t* WEBP_RESTRICT in,
                          uint8_t* WEBP_RESTRICT dst) {
  VP8Transform(in + 0 * 16, dst, 1);
  VP8Transform(in + 2 * 16, dst + 4 * BPS, 1);
}

#if !WEBP_NEON_OMIT_C_CODE
static void TransformDC_C(const int16_t* WEBP_RESTRICT in,
                          uint8_t* WEBP_RESTRICT dst) {
  const int DC = in[0] + 4;
  int i, j;
  for (j = 0; j < 4; ++j) {
    for (i = 0; i < 4; ++i) {
      STORE(i, j, DC);
    }
  }
}
#endif

static void TransformDCUV_C(const int16_t* WEBP_RESTRICT in,
                            uint8_t* WEBP_RESTRICT dst) {
  if (in[0 * 16]) VP8TransformDC(in + 0 * 16, dst);
  if (in[1 * 16]) VP8TransformDC(in + 1 * 16, dst + 4);
  if (in[2 * 16]) VP8TransformDC(in + 2 * 16, dst + 4 * BPS);
  if (in[3 * 16]) VP8TransformDC(in + 3 * 16, dst + 4 * BPS + 4);
}

#undef STORE

#if !WEBP_NEON_OMIT_C_CODE
static void TransformWHT_C(const int16_t* WEBP_RESTRICT in,
                           int16_t* WEBP_RESTRICT out) {
  int tmp[16];
  int i;
  for (i = 0; i < 4; ++i) {
    const int a0 = in[0 + i] + in[12 + i];
    const int a1 = in[4 + i] + in[ 8 + i];
    const int a2 = in[4 + i] - in[ 8 + i];
    const int a3 = in[0 + i] - in[12 + i];
    tmp[0  + i] = a0 + a1;
    tmp[8  + i] = a0 - a1;
    tmp[4  + i] = a3 + a2;
    tmp[12 + i] = a3 - a2;
  }
  for (i = 0; i < 4; ++i) {
    const int dc = tmp[0 + i * 4] + 3;
    const int a0 = dc             + tmp[3 + i * 4];
    const int a1 = tmp[1 + i * 4] + tmp[2 + i * 4];
    const int a2 = tmp[1 + i * 4] - tmp[2 + i * 4];
    const int a3 = dc             - tmp[3 + i * 4];
    out[ 0] = (a0 + a1) >> 3;
    out[16] = (a3 + a2) >> 3;
    out[32] = (a0 - a1) >> 3;
    out[48] = (a3 - a2) >> 3;
    out += 64;
  }
}
#endif

VP8WHT VP8TransformWHT;

#define DST(x, y) dst[(x) + (y) * BPS]

#if !WEBP_NEON_OMIT_C_CODE
static WEBP_INLINE void TrueMotion(uint8_t* dst, int size) {
  const uint8_t* top = dst - BPS;
  const uint8_t* const clip0 = VP8kclip1 - top[-1];
  int y;
  for (y = 0; y < size; ++y) {
    const uint8_t* const clip = clip0 + dst[-1];
    int x;
    for (x = 0; x < size; ++x) {
      dst[x] = clip[top[x]];
    }
    dst += BPS;
  }
}
static void TM4_C(uint8_t* dst)   { TrueMotion(dst, 4); }
static void TM8uv_C(uint8_t* dst) { TrueMotion(dst, 8); }
static void TM16_C(uint8_t* dst)  { TrueMotion(dst, 16); }

static void VE16_C(uint8_t* dst) {
  int j;
  for (j = 0; j < 16; ++j) {
    memcpy(dst + j * BPS, dst - BPS, 16);
  }
}

static void HE16_C(uint8_t* dst) {
  int j;
  for (j = 16; j > 0; --j) {
    memset(dst, dst[-1], 16);
    dst += BPS;
  }
}

static WEBP_INLINE void Put16(int v, uint8_t* dst) {
  int j;
  for (j = 0; j < 16; ++j) {
    memset(dst + j * BPS, v, 16);
  }
}

static void DC16_C(uint8_t* dst) {
  int DC = 16;
  int j;
  for (j = 0; j < 16; ++j) {
    DC += dst[-1 + j * BPS] + dst[j - BPS];
  }
  Put16(DC >> 5, dst);
}

static void DC16NoTop_C(uint8_t* dst) {
  int DC = 8;
  int j;
  for (j = 0; j < 16; ++j) {
    DC += dst[-1 + j * BPS];
  }
  Put16(DC >> 4, dst);
}

static void DC16NoLeft_C(uint8_t* dst) {
  int DC = 8;
  int i;
  for (i = 0; i < 16; ++i) {
    DC += dst[i - BPS];
  }
  Put16(DC >> 4, dst);
}

static void DC16NoTopLeft_C(uint8_t* dst) {
  Put16(0x80, dst);
}
#endif

VP8PredFunc VP8PredLuma16[NUM_B_DC_MODES];

#define AVG3(a, b, c) ((uint8_t)(((a) + 2 * (b) + (c) + 2) >> 2))
#define AVG2(a, b) (((a) + (b) + 1) >> 1)

#if !WEBP_NEON_OMIT_C_CODE
static void VE4_C(uint8_t* dst) {
  const uint8_t* top = dst - BPS;
  const uint8_t vals[4] = {
    AVG3(top[-1], top[0], top[1]),
    AVG3(top[ 0], top[1], top[2]),
    AVG3(top[ 1], top[2], top[3]),
    AVG3(top[ 2], top[3], top[4])
  };
  int i;
  for (i = 0; i < 4; ++i) {
    memcpy(dst + i * BPS, vals, sizeof(vals));
  }
}
#endif

static void HE4_C(uint8_t* dst) {
  const int A = dst[-1 - BPS];
  const int B = dst[-1];
  const int C = dst[-1 + BPS];
  const int D = dst[-1 + 2 * BPS];
  const int E = dst[-1 + 3 * BPS];
  WebPUint32ToMem(dst + 0 * BPS, 0x01010101U * AVG3(A, B, C));
  WebPUint32ToMem(dst + 1 * BPS, 0x01010101U * AVG3(B, C, D));
  WebPUint32ToMem(dst + 2 * BPS, 0x01010101U * AVG3(C, D, E));
  WebPUint32ToMem(dst + 3 * BPS, 0x01010101U * AVG3(D, E, E));
}

#if !WEBP_NEON_OMIT_C_CODE
static void DC4_C(uint8_t* dst) {
  uint32_t dc = 4;
  int i;
  for (i = 0; i < 4; ++i) dc += dst[i - BPS] + dst[-1 + i * BPS];
  dc >>= 3;
  for (i = 0; i < 4; ++i) memset(dst + i * BPS, dc, 4);
}

static void RD4_C(uint8_t* dst) {
  const int I = dst[-1 + 0 * BPS];
  const int J = dst[-1 + 1 * BPS];
  const int K = dst[-1 + 2 * BPS];
  const int L = dst[-1 + 3 * BPS];
  const int X = dst[-1 - BPS];
  const int A = dst[0 - BPS];
  const int B = dst[1 - BPS];
  const int C = dst[2 - BPS];
  const int D = dst[3 - BPS];
  DST(0, 3)                                     = AVG3(J, K, L);
  DST(1, 3) = DST(0, 2)                         = AVG3(I, J, K);
  DST(2, 3) = DST(1, 2) = DST(0, 1)             = AVG3(X, I, J);
  DST(3, 3) = DST(2, 2) = DST(1, 1) = DST(0, 0) = AVG3(A, X, I);
              DST(3, 2) = DST(2, 1) = DST(1, 0) = AVG3(B, A, X);
                          DST(3, 1) = DST(2, 0) = AVG3(C, B, A);
                                      DST(3, 0) = AVG3(D, C, B);
}

static void LD4_C(uint8_t* dst) {
  const int A = dst[0 - BPS];
  const int B = dst[1 - BPS];
  const int C = dst[2 - BPS];
  const int D = dst[3 - BPS];
  const int E = dst[4 - BPS];
  const int F = dst[5 - BPS];
  const int G = dst[6 - BPS];
  const int H = dst[7 - BPS];
  DST(0, 0)                                     = AVG3(A, B, C);
  DST(1, 0) = DST(0, 1)                         = AVG3(B, C, D);
  DST(2, 0) = DST(1, 1) = DST(0, 2)             = AVG3(C, D, E);
  DST(3, 0) = DST(2, 1) = DST(1, 2) = DST(0, 3) = AVG3(D, E, F);
              DST(3, 1) = DST(2, 2) = DST(1, 3) = AVG3(E, F, G);
                          DST(3, 2) = DST(2, 3) = AVG3(F, G, H);
                                      DST(3, 3) = AVG3(G, H, H);
}
#endif

static void VR4_C(uint8_t* dst) {
  const int I = dst[-1 + 0 * BPS];
  const int J = dst[-1 + 1 * BPS];
  const int K = dst[-1 + 2 * BPS];
  const int X = dst[-1 - BPS];
  const int A = dst[0 - BPS];
  const int B = dst[1 - BPS];
  const int C = dst[2 - BPS];
  const int D = dst[3 - BPS];
  DST(0, 0) = DST(1, 2) = AVG2(X, A);
  DST(1, 0) = DST(2, 2) = AVG2(A, B);
  DST(2, 0) = DST(3, 2) = AVG2(B, C);
  DST(3, 0)             = AVG2(C, D);

  DST(0, 3) =             AVG3(K, J, I);
  DST(0, 2) =             AVG3(J, I, X);
  DST(0, 1) = DST(1, 3) = AVG3(I, X, A);
  DST(1, 1) = DST(2, 3) = AVG3(X, A, B);
  DST(2, 1) = DST(3, 3) = AVG3(A, B, C);
  DST(3, 1) =             AVG3(B, C, D);
}

static void VL4_C(uint8_t* dst) {
  const int A = dst[0 - BPS];
  const int B = dst[1 - BPS];
  const int C = dst[2 - BPS];
  const int D = dst[3 - BPS];
  const int E = dst[4 - BPS];
  const int F = dst[5 - BPS];
  const int G = dst[6 - BPS];
  const int H = dst[7 - BPS];
  DST(0, 0) =             AVG2(A, B);
  DST(1, 0) = DST(0, 2) = AVG2(B, C);
  DST(2, 0) = DST(1, 2) = AVG2(C, D);
  DST(3, 0) = DST(2, 2) = AVG2(D, E);

  DST(0, 1) =             AVG3(A, B, C);
  DST(1, 1) = DST(0, 3) = AVG3(B, C, D);
  DST(2, 1) = DST(1, 3) = AVG3(C, D, E);
  DST(3, 1) = DST(2, 3) = AVG3(D, E, F);
              DST(3, 2) = AVG3(E, F, G);
              DST(3, 3) = AVG3(F, G, H);
}

static void HU4_C(uint8_t* dst) {
  const int I = dst[-1 + 0 * BPS];
  const int J = dst[-1 + 1 * BPS];
  const int K = dst[-1 + 2 * BPS];
  const int L = dst[-1 + 3 * BPS];
  DST(0, 0) =             AVG2(I, J);
  DST(2, 0) = DST(0, 1) = AVG2(J, K);
  DST(2, 1) = DST(0, 2) = AVG2(K, L);
  DST(1, 0) =             AVG3(I, J, K);
  DST(3, 0) = DST(1, 1) = AVG3(J, K, L);
  DST(3, 1) = DST(1, 2) = AVG3(K, L, L);
  DST(3, 2) = DST(2, 2) =
    DST(0, 3) = DST(1, 3) = DST(2, 3) = DST(3, 3) = L;
}

static void HD4_C(uint8_t* dst) {
  const int I = dst[-1 + 0 * BPS];
  const int J = dst[-1 + 1 * BPS];
  const int K = dst[-1 + 2 * BPS];
  const int L = dst[-1 + 3 * BPS];
  const int X = dst[-1 - BPS];
  const int A = dst[0 - BPS];
  const int B = dst[1 - BPS];
  const int C = dst[2 - BPS];

  DST(0, 0) = DST(2, 1) = AVG2(I, X);
  DST(0, 1) = DST(2, 2) = AVG2(J, I);
  DST(0, 2) = DST(2, 3) = AVG2(K, J);
  DST(0, 3)             = AVG2(L, K);

  DST(3, 0)             = AVG3(A, B, C);
  DST(2, 0)             = AVG3(X, A, B);
  DST(1, 0) = DST(3, 1) = AVG3(I, X, A);
  DST(1, 1) = DST(3, 2) = AVG3(J, I, X);
  DST(1, 2) = DST(3, 3) = AVG3(K, J, I);
  DST(1, 3)             = AVG3(L, K, J);
}

#undef DST
#undef AVG3
#undef AVG2

VP8PredFunc VP8PredLuma4[NUM_BMODES];

#if !WEBP_NEON_OMIT_C_CODE
static void VE8uv_C(uint8_t* dst) {
  int j;
  for (j = 0; j < 8; ++j) {
    memcpy(dst + j * BPS, dst - BPS, 8);
  }
}

static void HE8uv_C(uint8_t* dst) {
  int j;
  for (j = 0; j < 8; ++j) {
    memset(dst, dst[-1], 8);
    dst += BPS;
  }
}

static WEBP_INLINE void Put8x8uv(uint8_t value, uint8_t* dst) {
  int j;
  for (j = 0; j < 8; ++j) {
    memset(dst + j * BPS, value, 8);
  }
}

static void DC8uv_C(uint8_t* dst) {
  int dc0 = 8;
  int i;
  for (i = 0; i < 8; ++i) {
    dc0 += dst[i - BPS] + dst[-1 + i * BPS];
  }
  Put8x8uv(dc0 >> 4, dst);
}

static void DC8uvNoLeft_C(uint8_t* dst) {
  int dc0 = 4;
  int i;
  for (i = 0; i < 8; ++i) {
    dc0 += dst[i - BPS];
  }
  Put8x8uv(dc0 >> 3, dst);
}

static void DC8uvNoTop_C(uint8_t* dst) {
  int dc0 = 4;
  int i;
  for (i = 0; i < 8; ++i) {
    dc0 += dst[-1 + i * BPS];
  }
  Put8x8uv(dc0 >> 3, dst);
}

static void DC8uvNoTopLeft_C(uint8_t* dst) {
  Put8x8uv(0x80, dst);
}
#endif

VP8PredFunc VP8PredChroma8[NUM_B_DC_MODES];

#if !WEBP_NEON_OMIT_C_CODE || WEBP_NEON_WORK_AROUND_GCC

static WEBP_INLINE void DoFilter2_C(uint8_t* p, int step) {
  const int p1 = p[-2*step], p0 = p[-step], q0 = p[0], q1 = p[step];
  const int a = 3 * (q0 - p0) + VP8ksclip1[p1 - q1];
  const int a1 = VP8ksclip2[(a + 4) >> 3];
  const int a2 = VP8ksclip2[(a + 3) >> 3];
  p[-step] = VP8kclip1[p0 + a2];
  p[    0] = VP8kclip1[q0 - a1];
}

static WEBP_INLINE void DoFilter4_C(uint8_t* p, int step) {
  const int p1 = p[-2*step], p0 = p[-step], q0 = p[0], q1 = p[step];
  const int a = 3 * (q0 - p0);
  const int a1 = VP8ksclip2[(a + 4) >> 3];
  const int a2 = VP8ksclip2[(a + 3) >> 3];
  const int a3 = (a1 + 1) >> 1;
  p[-2*step] = VP8kclip1[p1 + a3];
  p[-  step] = VP8kclip1[p0 + a2];
  p[      0] = VP8kclip1[q0 - a1];
  p[   step] = VP8kclip1[q1 - a3];
}

static WEBP_INLINE void DoFilter6_C(uint8_t* p, int step) {
  const int p2 = p[-3*step], p1 = p[-2*step], p0 = p[-step];
  const int q0 = p[0], q1 = p[step], q2 = p[2*step];
  const int a = VP8ksclip1[3 * (q0 - p0) + VP8ksclip1[p1 - q1]];

  const int a1 = (27 * a + 63) >> 7;
  const int a2 = (18 * a + 63) >> 7;
  const int a3 = (9  * a + 63) >> 7;
  p[-3*step] = VP8kclip1[p2 + a3];
  p[-2*step] = VP8kclip1[p1 + a2];
  p[-  step] = VP8kclip1[p0 + a1];
  p[      0] = VP8kclip1[q0 - a1];
  p[   step] = VP8kclip1[q1 - a2];
  p[ 2*step] = VP8kclip1[q2 - a3];
}

static WEBP_INLINE int Hev(const uint8_t* p, int step, int thresh) {
  const int p1 = p[-2*step], p0 = p[-step], q0 = p[0], q1 = p[step];
  return (VP8kabs0[p1 - p0] > thresh) || (VP8kabs0[q1 - q0] > thresh);
}
#endif

#if !WEBP_NEON_OMIT_C_CODE
static WEBP_INLINE int NeedsFilter_C(const uint8_t* p, int step, int t) {
  const int p1 = p[-2 * step], p0 = p[-step], q0 = p[0], q1 = p[step];
  return ((4 * VP8kabs0[p0 - q0] + VP8kabs0[p1 - q1]) <= t);
}
#endif

#if !WEBP_NEON_OMIT_C_CODE || WEBP_NEON_WORK_AROUND_GCC
static WEBP_INLINE int NeedsFilter2_C(const uint8_t* p,
                                      int step, int t, int it) {
  const int p3 = p[-4 * step], p2 = p[-3 * step], p1 = p[-2 * step];
  const int p0 = p[-step], q0 = p[0];
  const int q1 = p[step], q2 = p[2 * step], q3 = p[3 * step];
  if ((4 * VP8kabs0[p0 - q0] + VP8kabs0[p1 - q1]) > t) return 0;
  return VP8kabs0[p3 - p2] <= it && VP8kabs0[p2 - p1] <= it &&
         VP8kabs0[p1 - p0] <= it && VP8kabs0[q3 - q2] <= it &&
         VP8kabs0[q2 - q1] <= it && VP8kabs0[q1 - q0] <= it;
}
#endif

#if !WEBP_NEON_OMIT_C_CODE
static void SimpleVFilter16_C(uint8_t* p, int stride, int thresh) {
  int i;
  const int thresh2 = 2 * thresh + 1;
  for (i = 0; i < 16; ++i) {
    if (NeedsFilter_C(p + i, stride, thresh2)) {
      DoFilter2_C(p + i, stride);
    }
  }
}

static void SimpleHFilter16_C(uint8_t* p, int stride, int thresh) {
  int i;
  const int thresh2 = 2 * thresh + 1;
  for (i = 0; i < 16; ++i) {
    if (NeedsFilter_C(p + i * stride, 1, thresh2)) {
      DoFilter2_C(p + i * stride, 1);
    }
  }
}

static void SimpleVFilter16i_C(uint8_t* p, int stride, int thresh) {
  int k;
  for (k = 3; k > 0; --k) {
    p += 4 * stride;
    SimpleVFilter16_C(p, stride, thresh);
  }
}

static void SimpleHFilter16i_C(uint8_t* p, int stride, int thresh) {
  int k;
  for (k = 3; k > 0; --k) {
    p += 4;
    SimpleHFilter16_C(p, stride, thresh);
  }
}
#endif

#if !WEBP_NEON_OMIT_C_CODE || WEBP_NEON_WORK_AROUND_GCC
static WEBP_INLINE void FilterLoop26_C(uint8_t* p,
                                       int hstride, int vstride, int size,
                                       int thresh, int ithresh,
                                       int hev_thresh) {
  const int thresh2 = 2 * thresh + 1;
  while (size-- > 0) {
    if (NeedsFilter2_C(p, hstride, thresh2, ithresh)) {
      if (Hev(p, hstride, hev_thresh)) {
        DoFilter2_C(p, hstride);
      } else {
        DoFilter6_C(p, hstride);
      }
    }
    p += vstride;
  }
}

static WEBP_INLINE void FilterLoop24_C(uint8_t* p,
                                       int hstride, int vstride, int size,
                                       int thresh, int ithresh,
                                       int hev_thresh) {
  const int thresh2 = 2 * thresh + 1;
  while (size-- > 0) {
    if (NeedsFilter2_C(p, hstride, thresh2, ithresh)) {
      if (Hev(p, hstride, hev_thresh)) {
        DoFilter2_C(p, hstride);
      } else {
        DoFilter4_C(p, hstride);
      }
    }
    p += vstride;
  }
}
#endif

#if !WEBP_NEON_OMIT_C_CODE

static void VFilter16_C(uint8_t* p, int stride,
                        int thresh, int ithresh, int hev_thresh) {
  FilterLoop26_C(p, stride, 1, 16, thresh, ithresh, hev_thresh);
}

static void HFilter16_C(uint8_t* p, int stride,
                        int thresh, int ithresh, int hev_thresh) {
  FilterLoop26_C(p, 1, stride, 16, thresh, ithresh, hev_thresh);
}

static void VFilter16i_C(uint8_t* p, int stride,
                         int thresh, int ithresh, int hev_thresh) {
  int k;
  for (k = 3; k > 0; --k) {
    p += 4 * stride;
    FilterLoop24_C(p, stride, 1, 16, thresh, ithresh, hev_thresh);
  }
}
#endif

#if !WEBP_NEON_OMIT_C_CODE || WEBP_NEON_WORK_AROUND_GCC
static void HFilter16i_C(uint8_t* p, int stride,
                         int thresh, int ithresh, int hev_thresh) {
  int k;
  for (k = 3; k > 0; --k) {
    p += 4;
    FilterLoop24_C(p, 1, stride, 16, thresh, ithresh, hev_thresh);
  }
}
#endif

#if !WEBP_NEON_OMIT_C_CODE

static void VFilter8_C(uint8_t* WEBP_RESTRICT u, uint8_t* WEBP_RESTRICT v,
                       int stride, int thresh, int ithresh, int hev_thresh) {
  FilterLoop26_C(u, stride, 1, 8, thresh, ithresh, hev_thresh);
  FilterLoop26_C(v, stride, 1, 8, thresh, ithresh, hev_thresh);
}
#endif

#if !WEBP_NEON_OMIT_C_CODE || WEBP_NEON_WORK_AROUND_GCC
static void HFilter8_C(uint8_t* WEBP_RESTRICT u, uint8_t* WEBP_RESTRICT v,
                       int stride, int thresh, int ithresh, int hev_thresh) {
  FilterLoop26_C(u, 1, stride, 8, thresh, ithresh, hev_thresh);
  FilterLoop26_C(v, 1, stride, 8, thresh, ithresh, hev_thresh);
}
#endif

#if !WEBP_NEON_OMIT_C_CODE
static void VFilter8i_C(uint8_t* WEBP_RESTRICT u, uint8_t* WEBP_RESTRICT v,
                        int stride, int thresh, int ithresh, int hev_thresh) {
  FilterLoop24_C(u + 4 * stride, stride, 1, 8, thresh, ithresh, hev_thresh);
  FilterLoop24_C(v + 4 * stride, stride, 1, 8, thresh, ithresh, hev_thresh);
}
#endif

#if !WEBP_NEON_OMIT_C_CODE || WEBP_NEON_WORK_AROUND_GCC
static void HFilter8i_C(uint8_t* WEBP_RESTRICT u, uint8_t* WEBP_RESTRICT v,
                        int stride, int thresh, int ithresh, int hev_thresh) {
  FilterLoop24_C(u + 4, 1, stride, 8, thresh, ithresh, hev_thresh);
  FilterLoop24_C(v + 4, 1, stride, 8, thresh, ithresh, hev_thresh);
}
#endif

static void DitherCombine8x8_C(const uint8_t* WEBP_RESTRICT dither,
                               uint8_t* WEBP_RESTRICT dst, int dst_stride) {
  int i, j;
  for (j = 0; j < 8; ++j) {
    for (i = 0; i < 8; ++i) {
      const int delta0 = dither[i] - VP8_DITHER_AMP_CENTER;
      const int delta1 =
          (delta0 + VP8_DITHER_DESCALE_ROUNDER) >> VP8_DITHER_DESCALE;
      dst[i] = clip_8b((int)dst[i] + delta1);
    }
    dst += dst_stride;
    dither += 8;
  }
}

VP8DecIdct2 VP8Transform;
VP8DecIdct VP8TransformAC3;
VP8DecIdct VP8TransformUV;
VP8DecIdct VP8TransformDC;
VP8DecIdct VP8TransformDCUV;

VP8LumaFilterFunc VP8VFilter16;
VP8LumaFilterFunc VP8HFilter16;
VP8ChromaFilterFunc VP8VFilter8;
VP8ChromaFilterFunc VP8HFilter8;
VP8LumaFilterFunc VP8VFilter16i;
VP8LumaFilterFunc VP8HFilter16i;
VP8ChromaFilterFunc VP8VFilter8i;
VP8ChromaFilterFunc VP8HFilter8i;
VP8SimpleFilterFunc VP8SimpleVFilter16;
VP8SimpleFilterFunc VP8SimpleHFilter16;
VP8SimpleFilterFunc VP8SimpleVFilter16i;
VP8SimpleFilterFunc VP8SimpleHFilter16i;

void (*VP8DitherCombine8x8)(const uint8_t* WEBP_RESTRICT dither,
                            uint8_t* WEBP_RESTRICT dst, int dst_stride);

extern VP8CPUInfo VP8GetCPUInfo;
extern void VP8DspInitSSE2(void);
extern void VP8DspInitSSE41(void);
extern void VP8DspInitNEON(void);
extern void VP8DspInitMIPS32(void);
extern void VP8DspInitMIPSdspR2(void);
extern void VP8DspInitMSA(void);

WEBP_DSP_INIT_FUNC(VP8DspInit) {
  VP8InitClipTables();

#if !WEBP_NEON_OMIT_C_CODE
  VP8TransformWHT = TransformWHT_C;
  VP8Transform = TransformTwo_C;
  VP8TransformDC = TransformDC_C;
  VP8TransformAC3 = TransformAC3_C;
#endif
  VP8TransformUV = TransformUV_C;
  VP8TransformDCUV = TransformDCUV_C;

#if !WEBP_NEON_OMIT_C_CODE
  VP8VFilter16 = VFilter16_C;
  VP8VFilter16i = VFilter16i_C;
  VP8HFilter16 = HFilter16_C;
  VP8VFilter8 = VFilter8_C;
  VP8VFilter8i = VFilter8i_C;
  VP8SimpleVFilter16 = SimpleVFilter16_C;
  VP8SimpleHFilter16 = SimpleHFilter16_C;
  VP8SimpleVFilter16i = SimpleVFilter16i_C;
  VP8SimpleHFilter16i = SimpleHFilter16i_C;
#endif

#if !WEBP_NEON_OMIT_C_CODE || WEBP_NEON_WORK_AROUND_GCC
  VP8HFilter16i = HFilter16i_C;
  VP8HFilter8 = HFilter8_C;
  VP8HFilter8i = HFilter8i_C;
#endif

#if !WEBP_NEON_OMIT_C_CODE
  VP8PredLuma4[0] = DC4_C;
  VP8PredLuma4[1] = TM4_C;
  VP8PredLuma4[2] = VE4_C;
  VP8PredLuma4[4] = RD4_C;
  VP8PredLuma4[6] = LD4_C;
#endif

  VP8PredLuma4[3] = HE4_C;
  VP8PredLuma4[5] = VR4_C;
  VP8PredLuma4[7] = VL4_C;
  VP8PredLuma4[8] = HD4_C;
  VP8PredLuma4[9] = HU4_C;

#if !WEBP_NEON_OMIT_C_CODE
  VP8PredLuma16[0] = DC16_C;
  VP8PredLuma16[1] = TM16_C;
  VP8PredLuma16[2] = VE16_C;
  VP8PredLuma16[3] = HE16_C;
  VP8PredLuma16[4] = DC16NoTop_C;
  VP8PredLuma16[5] = DC16NoLeft_C;
  VP8PredLuma16[6] = DC16NoTopLeft_C;

  VP8PredChroma8[0] = DC8uv_C;
  VP8PredChroma8[1] = TM8uv_C;
  VP8PredChroma8[2] = VE8uv_C;
  VP8PredChroma8[3] = HE8uv_C;
  VP8PredChroma8[4] = DC8uvNoTop_C;
  VP8PredChroma8[5] = DC8uvNoLeft_C;
  VP8PredChroma8[6] = DC8uvNoTopLeft_C;
#endif

  VP8DitherCombine8x8 = DitherCombine8x8_C;

  if (VP8GetCPUInfo != NULL) {
#if defined(WEBP_HAVE_SSE2)
    if (VP8GetCPUInfo(kSSE2)) {
      VP8DspInitSSE2();
#if defined(WEBP_HAVE_SSE41)
      if (VP8GetCPUInfo(kSSE4_1)) {
        VP8DspInitSSE41();
      }
#endif
    }
#endif
#if defined(WEBP_USE_MIPS32)
    if (VP8GetCPUInfo(kMIPS32)) {
      VP8DspInitMIPS32();
    }
#endif
#if defined(WEBP_USE_MIPS_DSP_R2)
    if (VP8GetCPUInfo(kMIPSdspR2)) {
      VP8DspInitMIPSdspR2();
    }
#endif
#if defined(WEBP_USE_MSA)
    if (VP8GetCPUInfo(kMSA)) {
      VP8DspInitMSA();
    }
#endif
  }

#if defined(WEBP_HAVE_NEON)
  if (WEBP_NEON_OMIT_C_CODE ||
      (VP8GetCPUInfo != NULL && VP8GetCPUInfo(kNEON))) {
    VP8DspInitNEON();
  }
#endif

  assert(VP8TransformWHT != NULL);
  assert(VP8Transform != NULL);
  assert(VP8TransformDC != NULL);
  assert(VP8TransformAC3 != NULL);
  assert(VP8TransformUV != NULL);
  assert(VP8TransformDCUV != NULL);
  assert(VP8VFilter16 != NULL);
  assert(VP8HFilter16 != NULL);
  assert(VP8VFilter8 != NULL);
  assert(VP8HFilter8 != NULL);
  assert(VP8VFilter16i != NULL);
  assert(VP8HFilter16i != NULL);
  assert(VP8VFilter8i != NULL);
  assert(VP8HFilter8i != NULL);
  assert(VP8SimpleVFilter16 != NULL);
  assert(VP8SimpleHFilter16 != NULL);
  assert(VP8SimpleVFilter16i != NULL);
  assert(VP8SimpleHFilter16i != NULL);
  assert(VP8PredLuma4[0] != NULL);
  assert(VP8PredLuma4[1] != NULL);
  assert(VP8PredLuma4[2] != NULL);
  assert(VP8PredLuma4[3] != NULL);
  assert(VP8PredLuma4[4] != NULL);
  assert(VP8PredLuma4[5] != NULL);
  assert(VP8PredLuma4[6] != NULL);
  assert(VP8PredLuma4[7] != NULL);
  assert(VP8PredLuma4[8] != NULL);
  assert(VP8PredLuma4[9] != NULL);
  assert(VP8PredLuma16[0] != NULL);
  assert(VP8PredLuma16[1] != NULL);
  assert(VP8PredLuma16[2] != NULL);
  assert(VP8PredLuma16[3] != NULL);
  assert(VP8PredLuma16[4] != NULL);
  assert(VP8PredLuma16[5] != NULL);
  assert(VP8PredLuma16[6] != NULL);
  assert(VP8PredChroma8[0] != NULL);
  assert(VP8PredChroma8[1] != NULL);
  assert(VP8PredChroma8[2] != NULL);
  assert(VP8PredChroma8[3] != NULL);
  assert(VP8PredChroma8[4] != NULL);
  assert(VP8PredChroma8[5] != NULL);
  assert(VP8PredChroma8[6] != NULL);
  assert(VP8DitherCombine8x8 != NULL);
}

#if !defined(USE_STATIC_TABLES)
#define USE_STATIC_TABLES 1
#endif

#if (USE_STATIC_TABLES == 1)

static const uint8_t abs0[255 + 255 + 1] = {
  0xff, 0xfe, 0xfd, 0xfc, 0xfb, 0xfa, 0xf9, 0xf8, 0xf7, 0xf6, 0xf5, 0xf4,
  0xf3, 0xf2, 0xf1, 0xf0, 0xef, 0xee, 0xed, 0xec, 0xeb, 0xea, 0xe9, 0xe8,
  0xe7, 0xe6, 0xe5, 0xe4, 0xe3, 0xe2, 0xe1, 0xe0, 0xdf, 0xde, 0xdd, 0xdc,
  0xdb, 0xda, 0xd9, 0xd8, 0xd7, 0xd6, 0xd5, 0xd4, 0xd3, 0xd2, 0xd1, 0xd0,
  0xcf, 0xce, 0xcd, 0xcc, 0xcb, 0xca, 0xc9, 0xc8, 0xc7, 0xc6, 0xc5, 0xc4,
  0xc3, 0xc2, 0xc1, 0xc0, 0xbf, 0xbe, 0xbd, 0xbc, 0xbb, 0xba, 0xb9, 0xb8,
  0xb7, 0xb6, 0xb5, 0xb4, 0xb3, 0xb2, 0xb1, 0xb0, 0xaf, 0xae, 0xad, 0xac,
  0xab, 0xaa, 0xa9, 0xa8, 0xa7, 0xa6, 0xa5, 0xa4, 0xa3, 0xa2, 0xa1, 0xa0,
  0x9f, 0x9e, 0x9d, 0x9c, 0x9b, 0x9a, 0x99, 0x98, 0x97, 0x96, 0x95, 0x94,
  0x93, 0x92, 0x91, 0x90, 0x8f, 0x8e, 0x8d, 0x8c, 0x8b, 0x8a, 0x89, 0x88,
  0x87, 0x86, 0x85, 0x84, 0x83, 0x82, 0x81, 0x80, 0x7f, 0x7e, 0x7d, 0x7c,
  0x7b, 0x7a, 0x79, 0x78, 0x77, 0x76, 0x75, 0x74, 0x73, 0x72, 0x71, 0x70,
  0x6f, 0x6e, 0x6d, 0x6c, 0x6b, 0x6a, 0x69, 0x68, 0x67, 0x66, 0x65, 0x64,
  0x63, 0x62, 0x61, 0x60, 0x5f, 0x5e, 0x5d, 0x5c, 0x5b, 0x5a, 0x59, 0x58,
  0x57, 0x56, 0x55, 0x54, 0x53, 0x52, 0x51, 0x50, 0x4f, 0x4e, 0x4d, 0x4c,
  0x4b, 0x4a, 0x49, 0x48, 0x47, 0x46, 0x45, 0x44, 0x43, 0x42, 0x41, 0x40,
  0x3f, 0x3e, 0x3d, 0x3c, 0x3b, 0x3a, 0x39, 0x38, 0x37, 0x36, 0x35, 0x34,
  0x33, 0x32, 0x31, 0x30, 0x2f, 0x2e, 0x2d, 0x2c, 0x2b, 0x2a, 0x29, 0x28,
  0x27, 0x26, 0x25, 0x24, 0x23, 0x22, 0x21, 0x20, 0x1f, 0x1e, 0x1d, 0x1c,
  0x1b, 0x1a, 0x19, 0x18, 0x17, 0x16, 0x15, 0x14, 0x13, 0x12, 0x11, 0x10,
  0x0f, 0x0e, 0x0d, 0x0c, 0x0b, 0x0a, 0x09, 0x08, 0x07, 0x06, 0x05, 0x04,
  0x03, 0x02, 0x01, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
  0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14,
  0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20,
  0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c,
  0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38,
  0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40, 0x41, 0x42, 0x43, 0x44,
  0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50,
  0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x5b, 0x5c,
  0x5d, 0x5e, 0x5f, 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
  0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71, 0x72, 0x73, 0x74,
  0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f, 0x80,
  0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x8b, 0x8c,
  0x8d, 0x8e, 0x8f, 0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98,
  0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f, 0xa0, 0xa1, 0xa2, 0xa3, 0xa4,
  0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf, 0xb0,
  0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xbb, 0xbc,
  0xbd, 0xbe, 0xbf, 0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8,
  0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf, 0xd0, 0xd1, 0xd2, 0xd3, 0xd4,
  0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf, 0xe0,
  0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xeb, 0xec,
  0xed, 0xee, 0xef, 0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
  0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
};

static const uint8_t sclip1[1020 + 1020 + 1] = {
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
  0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f, 0x90, 0x91, 0x92, 0x93,
  0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
  0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab,
  0xac, 0xad, 0xae, 0xaf, 0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7,
  0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf, 0xc0, 0xc1, 0xc2, 0xc3,
  0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,
  0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xdb,
  0xdc, 0xdd, 0xde, 0xdf, 0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7,
  0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef, 0xf0, 0xf1, 0xf2, 0xf3,
  0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff,
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
  0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
  0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23,
  0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
  0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b,
  0x3c, 0x3d, 0x3e, 0x3f, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
  0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53,
  0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
  0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b,
  0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
  0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,
  0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f
};

static const uint8_t sclip2[112 + 112 + 1] = {
  0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0,
  0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0,
  0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0,
  0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0,
  0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0,
  0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0,
  0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0,
  0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0,
  0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb,
  0xfc, 0xfd, 0xfe, 0xff, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
  0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
  0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
  0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
  0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
  0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
  0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
  0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
  0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
  0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f
};

static const uint8_t clip1[255 + 511 + 1] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
  0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14,
  0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20,
  0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c,
  0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38,
  0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40, 0x41, 0x42, 0x43, 0x44,
  0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50,
  0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x5b, 0x5c,
  0x5d, 0x5e, 0x5f, 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
  0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71, 0x72, 0x73, 0x74,
  0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f, 0x80,
  0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x8b, 0x8c,
  0x8d, 0x8e, 0x8f, 0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98,
  0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f, 0xa0, 0xa1, 0xa2, 0xa3, 0xa4,
  0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf, 0xb0,
  0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xbb, 0xbc,
  0xbd, 0xbe, 0xbf, 0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8,
  0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf, 0xd0, 0xd1, 0xd2, 0xd3, 0xd4,
  0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf, 0xe0,
  0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xeb, 0xec,
  0xed, 0xee, 0xef, 0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
  0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};

#else

static uint8_t abs0[255 + 255 + 1];
static int8_t sclip1[1020 + 1020 + 1];
static int8_t sclip2[112 + 112 + 1];
static uint8_t clip1[255 + 511 + 1];

static volatile int tables_ok = 0;

#endif

const int8_t* const VP8ksclip1 = (const int8_t*)&sclip1[1020];
const int8_t* const VP8ksclip2 = (const int8_t*)&sclip2[112];
const uint8_t* const VP8kclip1 = &clip1[255];
const uint8_t* const VP8kabs0 = &abs0[255];

WEBP_TSAN_IGNORE_FUNCTION void VP8InitClipTables(void) {
#if (USE_STATIC_TABLES == 0)
  int i;
  if (!tables_ok) {
    for (i = -255; i <= 255; ++i) {
      abs0[255 + i] = (i < 0) ? -i : i;
    }
    for (i = -1020; i <= 1020; ++i) {
      sclip1[1020 + i] = (i < -128) ? -128 : (i > 127) ? 127 : i;
    }
    for (i = -112; i <= 112; ++i) {
      sclip2[112 + i] = (i < -16) ? -16 : (i > 15) ? 15 : i;
    }
    for (i = -255; i <= 255 + 255; ++i) {
      clip1[255 + i] = (i < 0) ? 0 : (i > 255) ? 255 : i;
    }
    tables_ok = 1;
  }
#endif
}

#if defined(WEBP_USE_NEON)

#if !defined(WORK_AROUND_GCC)

static WEBP_INLINE uint8x8x4_t Load4x8_NEON(const uint8_t* const src,
                                            int stride) {
  const uint8x8_t zero = vdup_n_u8(0);
  uint8x8x4_t out;
  INIT_VECTOR4(out, zero, zero, zero, zero);
  out = vld4_lane_u8(src + 0 * stride, out, 0);
  out = vld4_lane_u8(src + 1 * stride, out, 1);
  out = vld4_lane_u8(src + 2 * stride, out, 2);
  out = vld4_lane_u8(src + 3 * stride, out, 3);
  out = vld4_lane_u8(src + 4 * stride, out, 4);
  out = vld4_lane_u8(src + 5 * stride, out, 5);
  out = vld4_lane_u8(src + 6 * stride, out, 6);
  out = vld4_lane_u8(src + 7 * stride, out, 7);
  return out;
}

static WEBP_INLINE void Load4x16_NEON(const uint8_t* const src, int stride,
                                      uint8x16_t* const p1,
                                      uint8x16_t* const p0,
                                      uint8x16_t* const q0,
                                      uint8x16_t* const q1) {

  const uint8x8x4_t row0 = Load4x8_NEON(src - 2 + 0 * stride, stride);
  const uint8x8x4_t row8 = Load4x8_NEON(src - 2 + 8 * stride, stride);
  *p1 = vcombine_u8(row0.val[0], row8.val[0]);
  *p0 = vcombine_u8(row0.val[1], row8.val[1]);
  *q0 = vcombine_u8(row0.val[2], row8.val[2]);
  *q1 = vcombine_u8(row0.val[3], row8.val[3]);
}

#else

#define LOADQ_LANE_32b(VALUE, LANE) do {                             \
  (VALUE) = vld1q_lane_u32((const uint32_t*)src, (VALUE), (LANE));   \
  src += stride;                                                     \
} while (0)

static WEBP_INLINE void Load4x16_NEON(const uint8_t* src, int stride,
                                      uint8x16_t* const p1,
                                      uint8x16_t* const p0,
                                      uint8x16_t* const q0,
                                      uint8x16_t* const q1) {
  const uint32x4_t zero = vdupq_n_u32(0);
  uint32x4x4_t in;
  INIT_VECTOR4(in, zero, zero, zero, zero);
  src -= 2;
  LOADQ_LANE_32b(in.val[0], 0);
  LOADQ_LANE_32b(in.val[1], 0);
  LOADQ_LANE_32b(in.val[2], 0);
  LOADQ_LANE_32b(in.val[3], 0);
  LOADQ_LANE_32b(in.val[0], 1);
  LOADQ_LANE_32b(in.val[1], 1);
  LOADQ_LANE_32b(in.val[2], 1);
  LOADQ_LANE_32b(in.val[3], 1);
  LOADQ_LANE_32b(in.val[0], 2);
  LOADQ_LANE_32b(in.val[1], 2);
  LOADQ_LANE_32b(in.val[2], 2);
  LOADQ_LANE_32b(in.val[3], 2);
  LOADQ_LANE_32b(in.val[0], 3);
  LOADQ_LANE_32b(in.val[1], 3);
  LOADQ_LANE_32b(in.val[2], 3);
  LOADQ_LANE_32b(in.val[3], 3);

  {
    const uint8x16x2_t row01 = vtrnq_u8(vreinterpretq_u8_u32(in.val[0]),
                                        vreinterpretq_u8_u32(in.val[1]));
    const uint8x16x2_t row23 = vtrnq_u8(vreinterpretq_u8_u32(in.val[2]),
                                        vreinterpretq_u8_u32(in.val[3]));
    const uint16x8x2_t row02 = vtrnq_u16(vreinterpretq_u16_u8(row01.val[0]),
                                         vreinterpretq_u16_u8(row23.val[0]));
    const uint16x8x2_t row13 = vtrnq_u16(vreinterpretq_u16_u8(row01.val[1]),
                                         vreinterpretq_u16_u8(row23.val[1]));
    *p1 = vreinterpretq_u8_u16(row02.val[0]);
    *p0 = vreinterpretq_u8_u16(row13.val[0]);
    *q0 = vreinterpretq_u8_u16(row02.val[1]);
    *q1 = vreinterpretq_u8_u16(row13.val[1]);
  }
}
#undef LOADQ_LANE_32b

#endif

static WEBP_INLINE void Load8x16_NEON(
    const uint8_t* const src, int stride,
    uint8x16_t* const p3, uint8x16_t* const p2, uint8x16_t* const p1,
    uint8x16_t* const p0, uint8x16_t* const q0, uint8x16_t* const q1,
    uint8x16_t* const q2, uint8x16_t* const q3) {
  Load4x16_NEON(src - 2, stride, p3, p2, p1, p0);
  Load4x16_NEON(src + 2, stride, q0, q1, q2, q3);
}

static WEBP_INLINE void Load16x4_NEON(const uint8_t* const src, int stride,
                                      uint8x16_t* const p1,
                                      uint8x16_t* const p0,
                                      uint8x16_t* const q0,
                                      uint8x16_t* const q1) {
  *p1 = vld1q_u8(src - 2 * stride);
  *p0 = vld1q_u8(src - 1 * stride);
  *q0 = vld1q_u8(src + 0 * stride);
  *q1 = vld1q_u8(src + 1 * stride);
}

static WEBP_INLINE void Load16x8_NEON(
    const uint8_t* const src, int stride,
    uint8x16_t* const p3, uint8x16_t* const p2, uint8x16_t* const p1,
    uint8x16_t* const p0, uint8x16_t* const q0, uint8x16_t* const q1,
    uint8x16_t* const q2, uint8x16_t* const q3) {
  Load16x4_NEON(src - 2  * stride, stride, p3, p2, p1, p0);
  Load16x4_NEON(src + 2  * stride, stride, q0, q1, q2, q3);
}

static WEBP_INLINE void Load8x8x2_NEON(
    const uint8_t* const u, const uint8_t* const v, int stride,
    uint8x16_t* const p3, uint8x16_t* const p2, uint8x16_t* const p1,
    uint8x16_t* const p0, uint8x16_t* const q0, uint8x16_t* const q1,
    uint8x16_t* const q2, uint8x16_t* const q3) {

  *p3 = vcombine_u8(vld1_u8(u - 4 * stride), vld1_u8(v - 4 * stride));
  *p2 = vcombine_u8(vld1_u8(u - 3 * stride), vld1_u8(v - 3 * stride));
  *p1 = vcombine_u8(vld1_u8(u - 2 * stride), vld1_u8(v - 2 * stride));
  *p0 = vcombine_u8(vld1_u8(u - 1 * stride), vld1_u8(v - 1 * stride));
  *q0 = vcombine_u8(vld1_u8(u + 0 * stride), vld1_u8(v + 0 * stride));
  *q1 = vcombine_u8(vld1_u8(u + 1 * stride), vld1_u8(v + 1 * stride));
  *q2 = vcombine_u8(vld1_u8(u + 2 * stride), vld1_u8(v + 2 * stride));
  *q3 = vcombine_u8(vld1_u8(u + 3 * stride), vld1_u8(v + 3 * stride));
}

#if !defined(WORK_AROUND_GCC)

#define LOAD_UV_8(ROW) \
  vcombine_u8(vld1_u8(u - 4 + (ROW) * stride), vld1_u8(v - 4 + (ROW) * stride))

static WEBP_INLINE void Load8x8x2T_NEON(
    const uint8_t* const u, const uint8_t* const v, int stride,
    uint8x16_t* const p3, uint8x16_t* const p2, uint8x16_t* const p1,
    uint8x16_t* const p0, uint8x16_t* const q0, uint8x16_t* const q1,
    uint8x16_t* const q2, uint8x16_t* const q3) {

  const uint8x16_t row0 = LOAD_UV_8(0);
  const uint8x16_t row1 = LOAD_UV_8(1);
  const uint8x16_t row2 = LOAD_UV_8(2);
  const uint8x16_t row3 = LOAD_UV_8(3);
  const uint8x16_t row4 = LOAD_UV_8(4);
  const uint8x16_t row5 = LOAD_UV_8(5);
  const uint8x16_t row6 = LOAD_UV_8(6);
  const uint8x16_t row7 = LOAD_UV_8(7);

  const uint8x16x2_t row01 = vtrnq_u8(row0, row1);

  const uint8x16x2_t row23 = vtrnq_u8(row2, row3);

  const uint8x16x2_t row45 = vtrnq_u8(row4, row5);
  const uint8x16x2_t row67 = vtrnq_u8(row6, row7);
  const uint16x8x2_t row02 = vtrnq_u16(vreinterpretq_u16_u8(row01.val[0]),
                                       vreinterpretq_u16_u8(row23.val[0]));
  const uint16x8x2_t row13 = vtrnq_u16(vreinterpretq_u16_u8(row01.val[1]),
                                       vreinterpretq_u16_u8(row23.val[1]));
  const uint16x8x2_t row46 = vtrnq_u16(vreinterpretq_u16_u8(row45.val[0]),
                                       vreinterpretq_u16_u8(row67.val[0]));
  const uint16x8x2_t row57 = vtrnq_u16(vreinterpretq_u16_u8(row45.val[1]),
                                       vreinterpretq_u16_u8(row67.val[1]));
  const uint32x4x2_t row04 = vtrnq_u32(vreinterpretq_u32_u16(row02.val[0]),
                                       vreinterpretq_u32_u16(row46.val[0]));
  const uint32x4x2_t row26 = vtrnq_u32(vreinterpretq_u32_u16(row02.val[1]),
                                       vreinterpretq_u32_u16(row46.val[1]));
  const uint32x4x2_t row15 = vtrnq_u32(vreinterpretq_u32_u16(row13.val[0]),
                                       vreinterpretq_u32_u16(row57.val[0]));
  const uint32x4x2_t row37 = vtrnq_u32(vreinterpretq_u32_u16(row13.val[1]),
                                       vreinterpretq_u32_u16(row57.val[1]));
  *p3 = vreinterpretq_u8_u32(row04.val[0]);
  *p2 = vreinterpretq_u8_u32(row15.val[0]);
  *p1 = vreinterpretq_u8_u32(row26.val[0]);
  *p0 = vreinterpretq_u8_u32(row37.val[0]);
  *q0 = vreinterpretq_u8_u32(row04.val[1]);
  *q1 = vreinterpretq_u8_u32(row15.val[1]);
  *q2 = vreinterpretq_u8_u32(row26.val[1]);
  *q3 = vreinterpretq_u8_u32(row37.val[1]);
}
#undef LOAD_UV_8

#endif

static WEBP_INLINE void Store2x8_NEON(const uint8x8x2_t v,
                                      uint8_t* const dst, int stride) {
  vst2_lane_u8(dst + 0 * stride, v, 0);
  vst2_lane_u8(dst + 1 * stride, v, 1);
  vst2_lane_u8(dst + 2 * stride, v, 2);
  vst2_lane_u8(dst + 3 * stride, v, 3);
  vst2_lane_u8(dst + 4 * stride, v, 4);
  vst2_lane_u8(dst + 5 * stride, v, 5);
  vst2_lane_u8(dst + 6 * stride, v, 6);
  vst2_lane_u8(dst + 7 * stride, v, 7);
}

static WEBP_INLINE void Store2x16_NEON(const uint8x16_t p0, const uint8x16_t q0,
                                       uint8_t* const dst, int stride) {
  uint8x8x2_t lo, hi;
  lo.val[0] = vget_low_u8(p0);
  lo.val[1] = vget_low_u8(q0);
  hi.val[0] = vget_high_u8(p0);
  hi.val[1] = vget_high_u8(q0);
  Store2x8_NEON(lo, dst - 1 + 0 * stride, stride);
  Store2x8_NEON(hi, dst - 1 + 8 * stride, stride);
}

#if !defined(WORK_AROUND_GCC)
static WEBP_INLINE void Store4x8_NEON(const uint8x8x4_t v,
                                      uint8_t* const dst, int stride) {
  vst4_lane_u8(dst + 0 * stride, v, 0);
  vst4_lane_u8(dst + 1 * stride, v, 1);
  vst4_lane_u8(dst + 2 * stride, v, 2);
  vst4_lane_u8(dst + 3 * stride, v, 3);
  vst4_lane_u8(dst + 4 * stride, v, 4);
  vst4_lane_u8(dst + 5 * stride, v, 5);
  vst4_lane_u8(dst + 6 * stride, v, 6);
  vst4_lane_u8(dst + 7 * stride, v, 7);
}

static WEBP_INLINE void Store4x16_NEON(const uint8x16_t p1, const uint8x16_t p0,
                                       const uint8x16_t q0, const uint8x16_t q1,
                                       uint8_t* const dst, int stride) {
  uint8x8x4_t lo, hi;
  INIT_VECTOR4(lo,
               vget_low_u8(p1), vget_low_u8(p0),
               vget_low_u8(q0), vget_low_u8(q1));
  INIT_VECTOR4(hi,
               vget_high_u8(p1), vget_high_u8(p0),
               vget_high_u8(q0), vget_high_u8(q1));
  Store4x8_NEON(lo, dst - 2 + 0 * stride, stride);
  Store4x8_NEON(hi, dst - 2 + 8 * stride, stride);
}
#endif

static WEBP_INLINE void Store16x2_NEON(const uint8x16_t p0, const uint8x16_t q0,
                                       uint8_t* const dst, int stride) {
  vst1q_u8(dst - stride, p0);
  vst1q_u8(dst, q0);
}

static WEBP_INLINE void Store16x4_NEON(const uint8x16_t p1, const uint8x16_t p0,
                                       const uint8x16_t q0, const uint8x16_t q1,
                                       uint8_t* const dst, int stride) {
  Store16x2_NEON(p1, p0, dst - stride, stride);
  Store16x2_NEON(q0, q1, dst + stride, stride);
}

static WEBP_INLINE void Store8x2x2_NEON(const uint8x16_t p0,
                                        const uint8x16_t q0,
                                        uint8_t* const u, uint8_t* const v,
                                        int stride) {

  vst1_u8(u - stride, vget_low_u8(p0));
  vst1_u8(u,          vget_low_u8(q0));
  vst1_u8(v - stride, vget_high_u8(p0));
  vst1_u8(v,          vget_high_u8(q0));
}

static WEBP_INLINE void Store8x4x2_NEON(const uint8x16_t p1,
                                        const uint8x16_t p0,
                                        const uint8x16_t q0,
                                        const uint8x16_t q1,
                                        uint8_t* const u, uint8_t* const v,
                                        int stride) {

  Store8x2x2_NEON(p1, p0, u - stride, v - stride, stride);
  Store8x2x2_NEON(q0, q1, u + stride, v + stride, stride);
}

#if !defined(WORK_AROUND_GCC)

#define STORE6_LANE(DST, VAL0, VAL1, LANE) do {   \
  vst3_lane_u8((DST) - 3, (VAL0), (LANE));        \
  vst3_lane_u8((DST) + 0, (VAL1), (LANE));        \
  (DST) += stride;                                \
} while (0)

static WEBP_INLINE void Store6x8x2_NEON(
    const uint8x16_t p2, const uint8x16_t p1, const uint8x16_t p0,
    const uint8x16_t q0, const uint8x16_t q1, const uint8x16_t q2,
    uint8_t* u, uint8_t* v, int stride) {
  uint8x8x3_t u0, u1, v0, v1;
  INIT_VECTOR3(u0, vget_low_u8(p2), vget_low_u8(p1), vget_low_u8(p0));
  INIT_VECTOR3(u1, vget_low_u8(q0), vget_low_u8(q1), vget_low_u8(q2));
  INIT_VECTOR3(v0, vget_high_u8(p2), vget_high_u8(p1), vget_high_u8(p0));
  INIT_VECTOR3(v1, vget_high_u8(q0), vget_high_u8(q1), vget_high_u8(q2));
  STORE6_LANE(u, u0, u1, 0);
  STORE6_LANE(u, u0, u1, 1);
  STORE6_LANE(u, u0, u1, 2);
  STORE6_LANE(u, u0, u1, 3);
  STORE6_LANE(u, u0, u1, 4);
  STORE6_LANE(u, u0, u1, 5);
  STORE6_LANE(u, u0, u1, 6);
  STORE6_LANE(u, u0, u1, 7);
  STORE6_LANE(v, v0, v1, 0);
  STORE6_LANE(v, v0, v1, 1);
  STORE6_LANE(v, v0, v1, 2);
  STORE6_LANE(v, v0, v1, 3);
  STORE6_LANE(v, v0, v1, 4);
  STORE6_LANE(v, v0, v1, 5);
  STORE6_LANE(v, v0, v1, 6);
  STORE6_LANE(v, v0, v1, 7);
}
#undef STORE6_LANE

static WEBP_INLINE void Store4x8x2_NEON(const uint8x16_t p1,
                                        const uint8x16_t p0,
                                        const uint8x16_t q0,
                                        const uint8x16_t q1,
                                        uint8_t* const u, uint8_t* const v,
                                        int stride) {
  uint8x8x4_t u0, v0;
  INIT_VECTOR4(u0,
               vget_low_u8(p1), vget_low_u8(p0),
               vget_low_u8(q0), vget_low_u8(q1));
  INIT_VECTOR4(v0,
               vget_high_u8(p1), vget_high_u8(p0),
               vget_high_u8(q0), vget_high_u8(q1));
  vst4_lane_u8(u - 2 + 0 * stride, u0, 0);
  vst4_lane_u8(u - 2 + 1 * stride, u0, 1);
  vst4_lane_u8(u - 2 + 2 * stride, u0, 2);
  vst4_lane_u8(u - 2 + 3 * stride, u0, 3);
  vst4_lane_u8(u - 2 + 4 * stride, u0, 4);
  vst4_lane_u8(u - 2 + 5 * stride, u0, 5);
  vst4_lane_u8(u - 2 + 6 * stride, u0, 6);
  vst4_lane_u8(u - 2 + 7 * stride, u0, 7);
  vst4_lane_u8(v - 2 + 0 * stride, v0, 0);
  vst4_lane_u8(v - 2 + 1 * stride, v0, 1);
  vst4_lane_u8(v - 2 + 2 * stride, v0, 2);
  vst4_lane_u8(v - 2 + 3 * stride, v0, 3);
  vst4_lane_u8(v - 2 + 4 * stride, v0, 4);
  vst4_lane_u8(v - 2 + 5 * stride, v0, 5);
  vst4_lane_u8(v - 2 + 6 * stride, v0, 6);
  vst4_lane_u8(v - 2 + 7 * stride, v0, 7);
}

#endif

static WEBP_INLINE int16x8_t ConvertU8ToS16_NEON(uint8x8_t v) {
  return vreinterpretq_s16_u16(vmovl_u8(v));
}

static WEBP_INLINE void SaturateAndStore4x4_NEON(uint8_t* const dst,
                                                 const int16x8_t dst01,
                                                 const int16x8_t dst23) {

  const uint8x8_t dst01_u8 = vqmovun_s16(dst01);
  const uint8x8_t dst23_u8 = vqmovun_s16(dst23);

  vst1_lane_u32((uint32_t*)(dst + 0 * BPS), vreinterpret_u32_u8(dst01_u8), 0);
  vst1_lane_u32((uint32_t*)(dst + 1 * BPS), vreinterpret_u32_u8(dst01_u8), 1);
  vst1_lane_u32((uint32_t*)(dst + 2 * BPS), vreinterpret_u32_u8(dst23_u8), 0);
  vst1_lane_u32((uint32_t*)(dst + 3 * BPS), vreinterpret_u32_u8(dst23_u8), 1);
}

static WEBP_INLINE void Add4x4_NEON(const int16x8_t row01,
                                    const int16x8_t row23,
                                    uint8_t* const dst) {
  uint32x2_t dst01 = vdup_n_u32(0);
  uint32x2_t dst23 = vdup_n_u32(0);

  dst01 = vld1_lane_u32((uint32_t*)(dst + 0 * BPS), dst01, 0);
  dst23 = vld1_lane_u32((uint32_t*)(dst + 2 * BPS), dst23, 0);
  dst01 = vld1_lane_u32((uint32_t*)(dst + 1 * BPS), dst01, 1);
  dst23 = vld1_lane_u32((uint32_t*)(dst + 3 * BPS), dst23, 1);

  {

    const int16x8_t dst01_s16 = ConvertU8ToS16_NEON(vreinterpret_u8_u32(dst01));
    const int16x8_t dst23_s16 = ConvertU8ToS16_NEON(vreinterpret_u8_u32(dst23));

    const int16x8_t out01 = vrsraq_n_s16(dst01_s16, row01, 3);
    const int16x8_t out23 = vrsraq_n_s16(dst23_s16, row23, 3);

    SaturateAndStore4x4_NEON(dst, out01, out23);
  }
}

static uint8x16_t NeedsFilter_NEON(const uint8x16_t p1, const uint8x16_t p0,
                                   const uint8x16_t q0, const uint8x16_t q1,
                                   int thresh) {
  const uint8x16_t thresh_v = vdupq_n_u8((uint8_t)thresh);
  const uint8x16_t a_p0_q0 = vabdq_u8(p0, q0);
  const uint8x16_t a_p1_q1 = vabdq_u8(p1, q1);
  const uint8x16_t a_p0_q0_2 = vqaddq_u8(a_p0_q0, a_p0_q0);
  const uint8x16_t a_p1_q1_2 = vshrq_n_u8(a_p1_q1, 1);
  const uint8x16_t sum = vqaddq_u8(a_p0_q0_2, a_p1_q1_2);
  const uint8x16_t mask = vcgeq_u8(thresh_v, sum);
  return mask;
}

static int8x16_t FlipSign_NEON(const uint8x16_t v) {
  const uint8x16_t sign_bit = vdupq_n_u8(0x80);
  return vreinterpretq_s8_u8(veorq_u8(v, sign_bit));
}

static uint8x16_t FlipSignBack_NEON(const int8x16_t v) {
  const int8x16_t sign_bit = vdupq_n_s8(0x80);
  return vreinterpretq_u8_s8(veorq_s8(v, sign_bit));
}

static int8x16_t GetBaseDelta_NEON(const int8x16_t p1, const int8x16_t p0,
                                   const int8x16_t q0, const int8x16_t q1) {
  const int8x16_t q0_p0 = vqsubq_s8(q0, p0);
  const int8x16_t p1_q1 = vqsubq_s8(p1, q1);
  const int8x16_t s1 = vqaddq_s8(p1_q1, q0_p0);
  const int8x16_t s2 = vqaddq_s8(q0_p0, s1);
  const int8x16_t s3 = vqaddq_s8(q0_p0, s2);
  return s3;
}

static int8x16_t GetBaseDelta0_NEON(const int8x16_t p0, const int8x16_t q0) {
  const int8x16_t q0_p0 = vqsubq_s8(q0, p0);
  const int8x16_t s1 = vqaddq_s8(q0_p0, q0_p0);
  const int8x16_t s2 = vqaddq_s8(q0_p0, s1);
  return s2;
}

static void ApplyFilter2NoFlip_NEON(const int8x16_t p0s, const int8x16_t q0s,
                                    const int8x16_t delta,
                                    int8x16_t* const op0,
                                    int8x16_t* const oq0) {
  const int8x16_t kCst3 = vdupq_n_s8(0x03);
  const int8x16_t kCst4 = vdupq_n_s8(0x04);
  const int8x16_t delta_p3 = vqaddq_s8(delta, kCst3);
  const int8x16_t delta_p4 = vqaddq_s8(delta, kCst4);
  const int8x16_t delta3 = vshrq_n_s8(delta_p3, 3);
  const int8x16_t delta4 = vshrq_n_s8(delta_p4, 3);
  *op0 = vqaddq_s8(p0s, delta3);
  *oq0 = vqsubq_s8(q0s, delta4);
}

#if defined(WEBP_USE_INTRINSICS)

static void ApplyFilter2_NEON(const int8x16_t p0s, const int8x16_t q0s,
                              const int8x16_t delta,
                              uint8x16_t* const op0, uint8x16_t* const oq0) {
  const int8x16_t kCst3 = vdupq_n_s8(0x03);
  const int8x16_t kCst4 = vdupq_n_s8(0x04);
  const int8x16_t delta_p3 = vqaddq_s8(delta, kCst3);
  const int8x16_t delta_p4 = vqaddq_s8(delta, kCst4);
  const int8x16_t delta3 = vshrq_n_s8(delta_p3, 3);
  const int8x16_t delta4 = vshrq_n_s8(delta_p4, 3);
  const int8x16_t sp0 = vqaddq_s8(p0s, delta3);
  const int8x16_t sq0 = vqsubq_s8(q0s, delta4);
  *op0 = FlipSignBack_NEON(sp0);
  *oq0 = FlipSignBack_NEON(sq0);
}

static void DoFilter2_NEON(const uint8x16_t p1, const uint8x16_t p0,
                           const uint8x16_t q0, const uint8x16_t q1,
                           const uint8x16_t mask,
                           uint8x16_t* const op0, uint8x16_t* const oq0) {
  const int8x16_t p1s = FlipSign_NEON(p1);
  const int8x16_t p0s = FlipSign_NEON(p0);
  const int8x16_t q0s = FlipSign_NEON(q0);
  const int8x16_t q1s = FlipSign_NEON(q1);
  const int8x16_t delta0 = GetBaseDelta_NEON(p1s, p0s, q0s, q1s);
  const int8x16_t delta1 = vandq_s8(delta0, vreinterpretq_s8_u8(mask));
  ApplyFilter2_NEON(p0s, q0s, delta1, op0, oq0);
}

static void SimpleVFilter16_NEON(uint8_t* p, int stride, int thresh) {
  uint8x16_t p1, p0, q0, q1, op0, oq0;
  Load16x4_NEON(p, stride, &p1, &p0, &q0, &q1);
  {
    const uint8x16_t mask = NeedsFilter_NEON(p1, p0, q0, q1, thresh);
    DoFilter2_NEON(p1, p0, q0, q1, mask, &op0, &oq0);
  }
  Store16x2_NEON(op0, oq0, p, stride);
}

static void SimpleHFilter16_NEON(uint8_t* p, int stride, int thresh) {
  uint8x16_t p1, p0, q0, q1, oq0, op0;
  Load4x16_NEON(p, stride, &p1, &p0, &q0, &q1);
  {
    const uint8x16_t mask = NeedsFilter_NEON(p1, p0, q0, q1, thresh);
    DoFilter2_NEON(p1, p0, q0, q1, mask, &op0, &oq0);
  }
  Store2x16_NEON(op0, oq0, p, stride);
}

#else

#define LOAD8x4(c1, c2, c3, c4, b1, b2, stride)                                \
  "vld4.8 {" #c1 "[0]," #c2 "[0]," #c3 "[0]," #c4 "[0]}," #b1 "," #stride "\n" \
  "vld4.8 {" #c1 "[1]," #c2 "[1]," #c3 "[1]," #c4 "[1]}," #b2 "," #stride "\n" \
  "vld4.8 {" #c1 "[2]," #c2 "[2]," #c3 "[2]," #c4 "[2]}," #b1 "," #stride "\n" \
  "vld4.8 {" #c1 "[3]," #c2 "[3]," #c3 "[3]," #c4 "[3]}," #b2 "," #stride "\n" \
  "vld4.8 {" #c1 "[4]," #c2 "[4]," #c3 "[4]," #c4 "[4]}," #b1 "," #stride "\n" \
  "vld4.8 {" #c1 "[5]," #c2 "[5]," #c3 "[5]," #c4 "[5]}," #b2 "," #stride "\n" \
  "vld4.8 {" #c1 "[6]," #c2 "[6]," #c3 "[6]," #c4 "[6]}," #b1 "," #stride "\n" \
  "vld4.8 {" #c1 "[7]," #c2 "[7]," #c3 "[7]," #c4 "[7]}," #b2 "," #stride "\n"

#define STORE8x2(c1, c2, p, stride)                                            \
  "vst2.8   {" #c1 "[0], " #c2 "[0]}," #p "," #stride " \n"                    \
  "vst2.8   {" #c1 "[1], " #c2 "[1]}," #p "," #stride " \n"                    \
  "vst2.8   {" #c1 "[2], " #c2 "[2]}," #p "," #stride " \n"                    \
  "vst2.8   {" #c1 "[3], " #c2 "[3]}," #p "," #stride " \n"                    \
  "vst2.8   {" #c1 "[4], " #c2 "[4]}," #p "," #stride " \n"                    \
  "vst2.8   {" #c1 "[5], " #c2 "[5]}," #p "," #stride " \n"                    \
  "vst2.8   {" #c1 "[6], " #c2 "[6]}," #p "," #stride " \n"                    \
  "vst2.8   {" #c1 "[7], " #c2 "[7]}," #p "," #stride " \n"

#define QRegs "q0", "q1", "q2", "q3",                                          \
              "q8", "q9", "q10", "q11", "q12", "q13", "q14", "q15"

#define FLIP_SIGN_BIT2(a, b, s)                                                \
  "veor     " #a "," #a "," #s "               \n"                             \
  "veor     " #b "," #b "," #s "               \n"                             \

#define FLIP_SIGN_BIT4(a, b, c, d, s)                                          \
  FLIP_SIGN_BIT2(a, b, s)                                                      \
  FLIP_SIGN_BIT2(c, d, s)                                                      \

#define NEEDS_FILTER(p1, p0, q0, q1, thresh, mask)                             \
  "vabd.u8    q15," #p0 "," #q0 "         \n"                \
  "vabd.u8    q14," #p1 "," #q1 "         \n"                \
  "vqadd.u8   q15, q15, q15               \n"            \
  "vshr.u8    q14, q14, #1                \n"            \
  "vqadd.u8   q15, q15, q14     \n"   \
  "vdup.8     q14, " #thresh "            \n"                                  \
  "vcge.u8   " #mask ", q14, q15          \n"

#define GET_BASE_DELTA(p1, p0, q0, q1, o)                                      \
  "vqsub.s8   q15," #q0 "," #p0 "         \n"                   \
  "vqsub.s8  " #o "," #p1 "," #q1 "       \n"                   \
  "vqadd.s8  " #o "," #o ", q15           \n"   \
  "vqadd.s8  " #o "," #o ", q15           \n"   \
  "vqadd.s8  " #o "," #o ", q15           \n"

#define DO_SIMPLE_FILTER(p0, q0, fl)                                           \
  "vmov.i8    q15, #0x03                  \n"                                  \
  "vqadd.s8   q15, q15, " #fl "           \n"        \
  "vshr.s8    q15, q15, #3                \n"                \
  "vqadd.s8  " #p0 "," #p0 ", q15         \n"               \
                                                                               \
  "vmov.i8    q15, #0x04                  \n"                                  \
  "vqadd.s8   q15, q15, " #fl "           \n"        \
  "vshr.s8    q15, q15, #3                \n"                \
  "vqsub.s8  " #q0 "," #q0 ", q15         \n"

#define DO_FILTER2(p1, p0, q0, q1, thresh)                                     \
  NEEDS_FILTER(p1, p0, q0, q1, thresh, q9)              \
  "vmov.i8    q10, #0x80                  \n"                    \
  FLIP_SIGN_BIT4(p1, p0, q0, q1, q10)             \
  GET_BASE_DELTA(p1, p0, q0, q1, q11)                   \
  "vand       q9, q9, q11                 \n"           \
  DO_SIMPLE_FILTER(p0, q0, q9)                               \
  FLIP_SIGN_BIT2(p0, q0, q10)

static void SimpleVFilter16_NEON(uint8_t* p, int stride, int thresh) {
  __asm__ volatile (
    "sub        %[p], %[p], %[stride], lsl #1  \n"

    "vld1.u8    {q1}, [%[p]], %[stride]        \n"
    "vld1.u8    {q2}, [%[p]], %[stride]        \n"
    "vld1.u8    {q3}, [%[p]], %[stride]        \n"
    "vld1.u8    {q12}, [%[p]]                  \n"

    DO_FILTER2(q1, q2, q3, q12, %[thresh])

    "sub        %[p], %[p], %[stride], lsl #1  \n"

    "vst1.u8    {q2}, [%[p]], %[stride]        \n"
    "vst1.u8    {q3}, [%[p]]                   \n"
    : [p] "+r"(p)
    : [stride] "r"(stride), [thresh] "r"(thresh)
    : "memory", QRegs
  );
}

static void SimpleHFilter16_NEON(uint8_t* p, int stride, int thresh) {
  __asm__ volatile (
    "sub        r4, %[p], #2                   \n"
    "lsl        r6, %[stride], #1              \n"
    "add        r5, r4, %[stride]              \n"

    LOAD8x4(d2, d3, d4, d5, [r4], [r5], r6)
    LOAD8x4(d24, d25, d26, d27, [r4], [r5], r6)
    "vswp       d3, d24                        \n"
    "vswp       d5, d26                        \n"
    "vswp       q2, q12                        \n"

    DO_FILTER2(q1, q2, q12, q13, %[thresh])

    "sub        %[p], %[p], #1                 \n"

    "vswp        d5, d24                       \n"
    STORE8x2(d4, d5, [%[p]], %[stride])
    STORE8x2(d24, d25, [%[p]], %[stride])

    : [p] "+r"(p)
    : [stride] "r"(stride), [thresh] "r"(thresh)
    : "memory", "r4", "r5", "r6", QRegs
  );
}

#undef LOAD8x4
#undef STORE8x2

#endif

static void SimpleVFilter16i_NEON(uint8_t* p, int stride, int thresh) {
  uint32_t k;
  for (k = 3; k != 0; --k) {
    p += 4 * stride;
    SimpleVFilter16_NEON(p, stride, thresh);
  }
}

static void SimpleHFilter16i_NEON(uint8_t* p, int stride, int thresh) {
  uint32_t k;
  for (k = 3; k != 0; --k) {
    p += 4;
    SimpleHFilter16_NEON(p, stride, thresh);
  }
}

static uint8x16_t NeedsHev_NEON(const uint8x16_t p1, const uint8x16_t p0,
                                const uint8x16_t q0, const uint8x16_t q1,
                                int hev_thresh) {
  const uint8x16_t hev_thresh_v = vdupq_n_u8((uint8_t)hev_thresh);
  const uint8x16_t a_p1_p0 = vabdq_u8(p1, p0);
  const uint8x16_t a_q1_q0 = vabdq_u8(q1, q0);
  const uint8x16_t a_max = vmaxq_u8(a_p1_p0, a_q1_q0);
  const uint8x16_t mask = vcgtq_u8(a_max, hev_thresh_v);
  return mask;
}

static uint8x16_t NeedsFilter2_NEON(const uint8x16_t p3, const uint8x16_t p2,
                                    const uint8x16_t p1, const uint8x16_t p0,
                                    const uint8x16_t q0, const uint8x16_t q1,
                                    const uint8x16_t q2, const uint8x16_t q3,
                                    int ithresh, int thresh) {
  const uint8x16_t ithresh_v = vdupq_n_u8((uint8_t)ithresh);
  const uint8x16_t a_p3_p2 = vabdq_u8(p3, p2);
  const uint8x16_t a_p2_p1 = vabdq_u8(p2, p1);
  const uint8x16_t a_p1_p0 = vabdq_u8(p1, p0);
  const uint8x16_t a_q3_q2 = vabdq_u8(q3, q2);
  const uint8x16_t a_q2_q1 = vabdq_u8(q2, q1);
  const uint8x16_t a_q1_q0 = vabdq_u8(q1, q0);
  const uint8x16_t max1 = vmaxq_u8(a_p3_p2, a_p2_p1);
  const uint8x16_t max2 = vmaxq_u8(a_p1_p0, a_q3_q2);
  const uint8x16_t max3 = vmaxq_u8(a_q2_q1, a_q1_q0);
  const uint8x16_t max12 = vmaxq_u8(max1, max2);
  const uint8x16_t max123 = vmaxq_u8(max12, max3);
  const uint8x16_t mask2 = vcgeq_u8(ithresh_v, max123);
  const uint8x16_t mask1 = NeedsFilter_NEON(p1, p0, q0, q1, thresh);
  const uint8x16_t mask = vandq_u8(mask1, mask2);
  return mask;
}

static void ApplyFilter4_NEON(
    const int8x16_t p1, const int8x16_t p0,
    const int8x16_t q0, const int8x16_t q1,
    const int8x16_t delta0,
    uint8x16_t* const op1, uint8x16_t* const op0,
    uint8x16_t* const oq0, uint8x16_t* const oq1) {
  const int8x16_t kCst3 = vdupq_n_s8(0x03);
  const int8x16_t kCst4 = vdupq_n_s8(0x04);
  const int8x16_t delta1 = vqaddq_s8(delta0, kCst4);
  const int8x16_t delta2 = vqaddq_s8(delta0, kCst3);
  const int8x16_t a1 = vshrq_n_s8(delta1, 3);
  const int8x16_t a2 = vshrq_n_s8(delta2, 3);
  const int8x16_t a3 = vrshrq_n_s8(a1, 1);
  *op0 = FlipSignBack_NEON(vqaddq_s8(p0, a2));
  *oq0 = FlipSignBack_NEON(vqsubq_s8(q0, a1));
  *op1 = FlipSignBack_NEON(vqaddq_s8(p1, a3));
  *oq1 = FlipSignBack_NEON(vqsubq_s8(q1, a3));
}

static void DoFilter4_NEON(
    const uint8x16_t p1, const uint8x16_t p0,
    const uint8x16_t q0, const uint8x16_t q1,
    const uint8x16_t mask, const uint8x16_t hev_mask,
    uint8x16_t* const op1, uint8x16_t* const op0,
    uint8x16_t* const oq0, uint8x16_t* const oq1) {

  const int8x16_t p1s = FlipSign_NEON(p1);
  int8x16_t p0s = FlipSign_NEON(p0);
  int8x16_t q0s = FlipSign_NEON(q0);
  const int8x16_t q1s = FlipSign_NEON(q1);
  const uint8x16_t simple_lf_mask = vandq_u8(mask, hev_mask);

  {
    const int8x16_t delta = GetBaseDelta_NEON(p1s, p0s, q0s, q1s);
    const int8x16_t simple_lf_delta =
        vandq_s8(delta, vreinterpretq_s8_u8(simple_lf_mask));
    ApplyFilter2NoFlip_NEON(p0s, q0s, simple_lf_delta, &p0s, &q0s);
  }

  {
    const int8x16_t delta0 = GetBaseDelta0_NEON(p0s, q0s);

    const uint8x16_t complex_lf_mask = veorq_u8(simple_lf_mask, mask);
    const int8x16_t complex_lf_delta =
        vandq_s8(delta0, vreinterpretq_s8_u8(complex_lf_mask));
    ApplyFilter4_NEON(p1s, p0s, q0s, q1s, complex_lf_delta, op1, op0, oq0, oq1);
  }
}

static void ApplyFilter6_NEON(
    const int8x16_t p2, const int8x16_t p1, const int8x16_t p0,
    const int8x16_t q0, const int8x16_t q1, const int8x16_t q2,
    const int8x16_t delta,
    uint8x16_t* const op2, uint8x16_t* const op1, uint8x16_t* const op0,
    uint8x16_t* const oq0, uint8x16_t* const oq1, uint8x16_t* const oq2) {

  const int8x8_t delta_lo = vget_low_s8(delta);
  const int8x8_t delta_hi = vget_high_s8(delta);
  const int8x8_t kCst9 = vdup_n_s8(9);
  const int16x8_t kCstm1 = vdupq_n_s16(-1);
  const int8x8_t kCst18 = vdup_n_s8(18);
  const int16x8_t S_lo = vmlal_s8(kCstm1, kCst9, delta_lo);
  const int16x8_t S_hi = vmlal_s8(kCstm1, kCst9, delta_hi);
  const int16x8_t Z_lo = vmlal_s8(S_lo, kCst18, delta_lo);
  const int16x8_t Z_hi = vmlal_s8(S_hi, kCst18, delta_hi);
  const int8x8_t a3_lo = vqrshrn_n_s16(S_lo, 7);
  const int8x8_t a3_hi = vqrshrn_n_s16(S_hi, 7);
  const int8x8_t a2_lo = vqrshrn_n_s16(S_lo, 6);
  const int8x8_t a2_hi = vqrshrn_n_s16(S_hi, 6);
  const int8x8_t a1_lo = vqrshrn_n_s16(Z_lo, 7);
  const int8x8_t a1_hi = vqrshrn_n_s16(Z_hi, 7);
  const int8x16_t a1 = vcombine_s8(a1_lo, a1_hi);
  const int8x16_t a2 = vcombine_s8(a2_lo, a2_hi);
  const int8x16_t a3 = vcombine_s8(a3_lo, a3_hi);

  *op0 = FlipSignBack_NEON(vqaddq_s8(p0, a1));
  *oq0 = FlipSignBack_NEON(vqsubq_s8(q0, a1));
  *oq1 = FlipSignBack_NEON(vqsubq_s8(q1, a2));
  *op1 = FlipSignBack_NEON(vqaddq_s8(p1, a2));
  *oq2 = FlipSignBack_NEON(vqsubq_s8(q2, a3));
  *op2 = FlipSignBack_NEON(vqaddq_s8(p2, a3));
}

static void DoFilter6_NEON(
    const uint8x16_t p2, const uint8x16_t p1, const uint8x16_t p0,
    const uint8x16_t q0, const uint8x16_t q1, const uint8x16_t q2,
    const uint8x16_t mask, const uint8x16_t hev_mask,
    uint8x16_t* const op2, uint8x16_t* const op1, uint8x16_t* const op0,
    uint8x16_t* const oq0, uint8x16_t* const oq1, uint8x16_t* const oq2) {

  const int8x16_t p2s = FlipSign_NEON(p2);
  const int8x16_t p1s = FlipSign_NEON(p1);
  int8x16_t p0s = FlipSign_NEON(p0);
  int8x16_t q0s = FlipSign_NEON(q0);
  const int8x16_t q1s = FlipSign_NEON(q1);
  const int8x16_t q2s = FlipSign_NEON(q2);
  const uint8x16_t simple_lf_mask = vandq_u8(mask, hev_mask);
  const int8x16_t delta0 = GetBaseDelta_NEON(p1s, p0s, q0s, q1s);

  {
    const int8x16_t simple_lf_delta =
        vandq_s8(delta0, vreinterpretq_s8_u8(simple_lf_mask));
    ApplyFilter2NoFlip_NEON(p0s, q0s, simple_lf_delta, &p0s, &q0s);
  }

  {

    const uint8x16_t complex_lf_mask = veorq_u8(simple_lf_mask, mask);
    const int8x16_t complex_lf_delta =
        vandq_s8(delta0, vreinterpretq_s8_u8(complex_lf_mask));
    ApplyFilter6_NEON(p2s, p1s, p0s, q0s, q1s, q2s, complex_lf_delta,
                      op2, op1, op0, oq0, oq1, oq2);
  }
}

static void VFilter16_NEON(uint8_t* p, int stride,
                           int thresh, int ithresh, int hev_thresh) {
  uint8x16_t p3, p2, p1, p0, q0, q1, q2, q3;
  Load16x8_NEON(p, stride, &p3, &p2, &p1, &p0, &q0, &q1, &q2, &q3);
  {
    const uint8x16_t mask = NeedsFilter2_NEON(p3, p2, p1, p0, q0, q1, q2, q3,
                                              ithresh, thresh);
    const uint8x16_t hev_mask = NeedsHev_NEON(p1, p0, q0, q1, hev_thresh);
    uint8x16_t op2, op1, op0, oq0, oq1, oq2;
    DoFilter6_NEON(p2, p1, p0, q0, q1, q2, mask, hev_mask,
                   &op2, &op1, &op0, &oq0, &oq1, &oq2);
    Store16x2_NEON(op2, op1, p - 2 * stride, stride);
    Store16x2_NEON(op0, oq0, p + 0 * stride, stride);
    Store16x2_NEON(oq1, oq2, p + 2 * stride, stride);
  }
}

static void HFilter16_NEON(uint8_t* p, int stride,
                           int thresh, int ithresh, int hev_thresh) {
  uint8x16_t p3, p2, p1, p0, q0, q1, q2, q3;
  Load8x16_NEON(p, stride, &p3, &p2, &p1, &p0, &q0, &q1, &q2, &q3);
  {
    const uint8x16_t mask = NeedsFilter2_NEON(p3, p2, p1, p0, q0, q1, q2, q3,
                                              ithresh, thresh);
    const uint8x16_t hev_mask = NeedsHev_NEON(p1, p0, q0, q1, hev_thresh);
    uint8x16_t op2, op1, op0, oq0, oq1, oq2;
    DoFilter6_NEON(p2, p1, p0, q0, q1, q2, mask, hev_mask,
                   &op2, &op1, &op0, &oq0, &oq1, &oq2);
    Store2x16_NEON(op2, op1, p - 2, stride);
    Store2x16_NEON(op0, oq0, p + 0, stride);
    Store2x16_NEON(oq1, oq2, p + 2, stride);
  }
}

static void VFilter16i_NEON(uint8_t* p, int stride,
                            int thresh, int ithresh, int hev_thresh) {
  uint32_t k;
  uint8x16_t p3, p2, p1, p0;
  Load16x4_NEON(p + 2  * stride, stride, &p3, &p2, &p1, &p0);
  for (k = 3; k != 0; --k) {
    uint8x16_t q0, q1, q2, q3;
    p += 4 * stride;
    Load16x4_NEON(p + 2  * stride, stride, &q0, &q1, &q2, &q3);
    {
      const uint8x16_t mask =
          NeedsFilter2_NEON(p3, p2, p1, p0, q0, q1, q2, q3, ithresh, thresh);
      const uint8x16_t hev_mask = NeedsHev_NEON(p1, p0, q0, q1, hev_thresh);

      DoFilter4_NEON(p1, p0, q0, q1, mask, hev_mask, &p1, &p0, &p3, &p2);
      Store16x4_NEON(p1, p0, p3, p2, p, stride);
      p1 = q2;
      p0 = q3;
    }
  }
}

#if !defined(WORK_AROUND_GCC)
static void HFilter16i_NEON(uint8_t* p, int stride,
                            int thresh, int ithresh, int hev_thresh) {
  uint32_t k;
  uint8x16_t p3, p2, p1, p0;
  Load4x16_NEON(p + 2, stride, &p3, &p2, &p1, &p0);
  for (k = 3; k != 0; --k) {
    uint8x16_t q0, q1, q2, q3;
    p += 4;
    Load4x16_NEON(p + 2, stride, &q0, &q1, &q2, &q3);
    {
      const uint8x16_t mask =
          NeedsFilter2_NEON(p3, p2, p1, p0, q0, q1, q2, q3, ithresh, thresh);
      const uint8x16_t hev_mask = NeedsHev_NEON(p1, p0, q0, q1, hev_thresh);
      DoFilter4_NEON(p1, p0, q0, q1, mask, hev_mask, &p1, &p0, &p3, &p2);
      Store4x16_NEON(p1, p0, p3, p2, p, stride);
      p1 = q2;
      p0 = q3;
    }
  }
}
#endif

static void VFilter8_NEON(uint8_t* WEBP_RESTRICT u, uint8_t* WEBP_RESTRICT v,
                          int stride, int thresh, int ithresh, int hev_thresh) {
  uint8x16_t p3, p2, p1, p0, q0, q1, q2, q3;
  Load8x8x2_NEON(u, v, stride, &p3, &p2, &p1, &p0, &q0, &q1, &q2, &q3);
  {
    const uint8x16_t mask = NeedsFilter2_NEON(p3, p2, p1, p0, q0, q1, q2, q3,
                                              ithresh, thresh);
    const uint8x16_t hev_mask = NeedsHev_NEON(p1, p0, q0, q1, hev_thresh);
    uint8x16_t op2, op1, op0, oq0, oq1, oq2;
    DoFilter6_NEON(p2, p1, p0, q0, q1, q2, mask, hev_mask,
                   &op2, &op1, &op0, &oq0, &oq1, &oq2);
    Store8x2x2_NEON(op2, op1, u - 2 * stride, v - 2 * stride, stride);
    Store8x2x2_NEON(op0, oq0, u + 0 * stride, v + 0 * stride, stride);
    Store8x2x2_NEON(oq1, oq2, u + 2 * stride, v + 2 * stride, stride);
  }
}
static void VFilter8i_NEON(uint8_t* WEBP_RESTRICT u, uint8_t* WEBP_RESTRICT v,
                           int stride,
                           int thresh, int ithresh, int hev_thresh) {
  uint8x16_t p3, p2, p1, p0, q0, q1, q2, q3;
  u += 4 * stride;
  v += 4 * stride;
  Load8x8x2_NEON(u, v, stride, &p3, &p2, &p1, &p0, &q0, &q1, &q2, &q3);
  {
    const uint8x16_t mask = NeedsFilter2_NEON(p3, p2, p1, p0, q0, q1, q2, q3,
                                              ithresh, thresh);
    const uint8x16_t hev_mask = NeedsHev_NEON(p1, p0, q0, q1, hev_thresh);
    uint8x16_t op1, op0, oq0, oq1;
    DoFilter4_NEON(p1, p0, q0, q1, mask, hev_mask, &op1, &op0, &oq0, &oq1);
    Store8x4x2_NEON(op1, op0, oq0, oq1, u, v, stride);
  }
}

#if !defined(WORK_AROUND_GCC)
static void HFilter8_NEON(uint8_t* WEBP_RESTRICT u, uint8_t* WEBP_RESTRICT v,
                          int stride, int thresh, int ithresh, int hev_thresh) {
  uint8x16_t p3, p2, p1, p0, q0, q1, q2, q3;
  Load8x8x2T_NEON(u, v, stride, &p3, &p2, &p1, &p0, &q0, &q1, &q2, &q3);
  {
    const uint8x16_t mask = NeedsFilter2_NEON(p3, p2, p1, p0, q0, q1, q2, q3,
                                              ithresh, thresh);
    const uint8x16_t hev_mask = NeedsHev_NEON(p1, p0, q0, q1, hev_thresh);
    uint8x16_t op2, op1, op0, oq0, oq1, oq2;
    DoFilter6_NEON(p2, p1, p0, q0, q1, q2, mask, hev_mask,
                   &op2, &op1, &op0, &oq0, &oq1, &oq2);
    Store6x8x2_NEON(op2, op1, op0, oq0, oq1, oq2, u, v, stride);
  }
}

static void HFilter8i_NEON(uint8_t* WEBP_RESTRICT u, uint8_t* WEBP_RESTRICT v,
                           int stride,
                           int thresh, int ithresh, int hev_thresh) {
  uint8x16_t p3, p2, p1, p0, q0, q1, q2, q3;
  u += 4;
  v += 4;
  Load8x8x2T_NEON(u, v, stride, &p3, &p2, &p1, &p0, &q0, &q1, &q2, &q3);
  {
    const uint8x16_t mask = NeedsFilter2_NEON(p3, p2, p1, p0, q0, q1, q2, q3,
                                              ithresh, thresh);
    const uint8x16_t hev_mask = NeedsHev_NEON(p1, p0, q0, q1, hev_thresh);
    uint8x16_t op1, op0, oq0, oq1;
    DoFilter4_NEON(p1, p0, q0, q1, mask, hev_mask, &op1, &op0, &oq0, &oq1);
    Store4x8x2_NEON(op1, op0, oq0, oq1, u, v, stride);
  }
}
#endif

static const int16_t kC1 = WEBP_TRANSFORM_AC3_C1;
static const int16_t kC2 =
    WEBP_TRANSFORM_AC3_C2 / 2;

#if defined(WEBP_USE_INTRINSICS)
static WEBP_INLINE void Transpose8x2_NEON(const int16x8_t in0,
                                          const int16x8_t in1,
                                          int16x8x2_t* const out) {

  const int16x8x2_t tmp0 = vzipq_s16(in0, in1);

  *out = vzipq_s16(tmp0.val[0], tmp0.val[1]);
}

static WEBP_INLINE void TransformPass_NEON(int16x8x2_t* const rows) {

  const int16x8_t B1 =
      vcombine_s16(vget_high_s16(rows->val[0]), vget_high_s16(rows->val[1]));

  const int16x8_t C0 = vsraq_n_s16(B1, vqdmulhq_n_s16(B1, kC1), 1);
  const int16x8_t C1 = vqdmulhq_n_s16(B1, kC2);
  const int16x4_t a = vqadd_s16(vget_low_s16(rows->val[0]),
                                vget_low_s16(rows->val[1]));
  const int16x4_t b = vqsub_s16(vget_low_s16(rows->val[0]),
                                vget_low_s16(rows->val[1]));

  const int16x4_t c = vqsub_s16(vget_low_s16(C1), vget_high_s16(C0));
  const int16x4_t d = vqadd_s16(vget_low_s16(C0), vget_high_s16(C1));
  const int16x8_t D0 = vcombine_s16(a, b);
  const int16x8_t D1 = vcombine_s16(d, c);
  const int16x8_t E0 = vqaddq_s16(D0, D1);
  const int16x8_t E_tmp = vqsubq_s16(D0, D1);
  const int16x8_t E1 = vcombine_s16(vget_high_s16(E_tmp), vget_low_s16(E_tmp));
  Transpose8x2_NEON(E0, E1, rows);
}

static void TransformOne_NEON(const int16_t* WEBP_RESTRICT in,
                              uint8_t* WEBP_RESTRICT dst) {
  int16x8x2_t rows;
  INIT_VECTOR2(rows, vld1q_s16(in + 0), vld1q_s16(in + 8));
  TransformPass_NEON(&rows);
  TransformPass_NEON(&rows);
  Add4x4_NEON(rows.val[0], rows.val[1], dst);
}

#else

static void TransformOne_NEON(const int16_t* WEBP_RESTRICT in,
                              uint8_t* WEBP_RESTRICT dst) {
  const int kBPS = BPS;

  const int16_t constants[4] = { kC1, kC2, 0, 0 };

  __asm__ volatile (
    "vld1.16         {q1, q2}, [%[in]]           \n"
    "vld1.16         {d0}, [%[constants]]        \n"

    "vswp            d3, d4                      \n"

    "vqdmulh.s16     q8, q2, d0[0]               \n"
    "vqdmulh.s16     q9, q2, d0[1]               \n"

    "vqadd.s16       d22, d2, d3                 \n"
    "vqsub.s16       d23, d2, d3                 \n"

    "vshr.s16        q8, q8, #1                  \n"

    "vqadd.s16       q8, q2, q8                  \n"

    "vqsub.s16       d20, d18, d17               \n"
    "vqadd.s16       d21, d19, d16               \n"

    "vqadd.s16       d2, d22, d21                \n"
    "vqadd.s16       d3, d23, d20                \n"
    "vqsub.s16       d4, d23, d20                \n"
    "vqsub.s16       d5, d22, d21                \n"

    "vzip.16         q1, q2                      \n"
    "vzip.16         q1, q2                      \n"

    "vswp            d3, d4                      \n"

    "vqdmulh.s16     q8, q2, d0[0]               \n"
    "vqdmulh.s16     q9, q2, d0[1]               \n"

    "vqadd.s16       d22, d2, d3                 \n"
    "vqsub.s16       d23, d2, d3                 \n"

    "vshr.s16        q8, q8, #1                  \n"
    "vqadd.s16       q8, q2, q8                  \n"

    "vqsub.s16       d20, d18, d17               \n"
    "vqadd.s16       d21, d19, d16               \n"

    "vqadd.s16       d2, d22, d21                \n"
    "vqadd.s16       d3, d23, d20                \n"
    "vqsub.s16       d4, d23, d20                \n"
    "vqsub.s16       d5, d22, d21                \n"

    "vld1.32         d6[0], [%[dst]], %[kBPS]    \n"
    "vld1.32         d6[1], [%[dst]], %[kBPS]    \n"
    "vld1.32         d7[0], [%[dst]], %[kBPS]    \n"
    "vld1.32         d7[1], [%[dst]], %[kBPS]    \n"

    "sub         %[dst], %[dst], %[kBPS], lsl #2 \n"

    "vrshr.s16       d2, d2, #3                  \n"
    "vrshr.s16       d3, d3, #3                  \n"
    "vrshr.s16       d4, d4, #3                  \n"
    "vrshr.s16       d5, d5, #3                  \n"

    "vzip.16         q1, q2                      \n"
    "vzip.16         q1, q2                      \n"

    "vmovl.u8        q8, d6                      \n"
    "vmovl.u8        q9, d7                      \n"

    "vqadd.s16       q1, q1, q8                  \n"
    "vqadd.s16       q2, q2, q9                  \n"

    "vqmovun.s16     d0, q1                      \n"
    "vqmovun.s16     d1, q2                      \n"

    "vst1.32         d0[0], [%[dst]], %[kBPS]    \n"
    "vst1.32         d0[1], [%[dst]], %[kBPS]    \n"
    "vst1.32         d1[0], [%[dst]], %[kBPS]    \n"
    "vst1.32         d1[1], [%[dst]]             \n"

    : [in] "+r"(in), [dst] "+r"(dst)
    : [kBPS] "r"(kBPS), [constants] "r"(constants)
    : "memory", "q0", "q1", "q2", "q8", "q9", "q10", "q11"
  );
}

#endif

static void TransformTwo_NEON(const int16_t* WEBP_RESTRICT in,
                              uint8_t* WEBP_RESTRICT dst, int do_two) {
  TransformOne_NEON(in, dst);
  if (do_two) {
    TransformOne_NEON(in + 16, dst + 4);
  }
}

static void TransformDC_NEON(const int16_t* WEBP_RESTRICT in,
                             uint8_t* WEBP_RESTRICT dst) {
  const int16x8_t DC = vdupq_n_s16(in[0]);
  Add4x4_NEON(DC, DC, dst);
}

#define STORE_WHT(dst, col, rows) do {                  \
  *dst = vgetq_lane_s32(rows.val[0], col); (dst) += 16; \
  *dst = vgetq_lane_s32(rows.val[1], col); (dst) += 16; \
  *dst = vgetq_lane_s32(rows.val[2], col); (dst) += 16; \
  *dst = vgetq_lane_s32(rows.val[3], col); (dst) += 16; \
} while (0)

static void TransformWHT_NEON(const int16_t* WEBP_RESTRICT in,
                              int16_t* WEBP_RESTRICT out) {
  int32x4x4_t tmp;

  {

    const int16x4_t in00_03 = vld1_s16(in + 0);
    const int16x4_t in04_07 = vld1_s16(in + 4);
    const int16x4_t in08_11 = vld1_s16(in + 8);
    const int16x4_t in12_15 = vld1_s16(in + 12);
    const int32x4_t a0 = vaddl_s16(in00_03, in12_15);
    const int32x4_t a1 = vaddl_s16(in04_07, in08_11);
    const int32x4_t a2 = vsubl_s16(in04_07, in08_11);
    const int32x4_t a3 = vsubl_s16(in00_03, in12_15);
    tmp.val[0] = vaddq_s32(a0, a1);
    tmp.val[1] = vaddq_s32(a3, a2);
    tmp.val[2] = vsubq_s32(a0, a1);
    tmp.val[3] = vsubq_s32(a3, a2);

    tmp = Transpose4x4_NEON(tmp);
  }

  {
    const int32x4_t kCst3 = vdupq_n_s32(3);
    const int32x4_t dc = vaddq_s32(tmp.val[0], kCst3);
    const int32x4_t a0 = vaddq_s32(dc, tmp.val[3]);
    const int32x4_t a1 = vaddq_s32(tmp.val[1], tmp.val[2]);
    const int32x4_t a2 = vsubq_s32(tmp.val[1], tmp.val[2]);
    const int32x4_t a3 = vsubq_s32(dc, tmp.val[3]);

    tmp.val[0] = vaddq_s32(a0, a1);
    tmp.val[1] = vaddq_s32(a3, a2);
    tmp.val[2] = vsubq_s32(a0, a1);
    tmp.val[3] = vsubq_s32(a3, a2);

    tmp.val[0] = vshrq_n_s32(tmp.val[0], 3);
    tmp.val[1] = vshrq_n_s32(tmp.val[1], 3);
    tmp.val[2] = vshrq_n_s32(tmp.val[2], 3);
    tmp.val[3] = vshrq_n_s32(tmp.val[3], 3);

    STORE_WHT(out, 0, tmp);
    STORE_WHT(out, 1, tmp);
    STORE_WHT(out, 2, tmp);
    STORE_WHT(out, 3, tmp);
  }
}

#undef STORE_WHT

static void TransformAC3_NEON(const int16_t* WEBP_RESTRICT in,
                              uint8_t* WEBP_RESTRICT dst) {
  const int16x4_t A = vld1_dup_s16(in);
  const int16x4_t c4 = vdup_n_s16(WEBP_TRANSFORM_AC3_MUL2(in[4]));
  const int16x4_t d4 = vdup_n_s16(WEBP_TRANSFORM_AC3_MUL1(in[4]));
  const int c1 = WEBP_TRANSFORM_AC3_MUL2(in[1]);
  const int d1 = WEBP_TRANSFORM_AC3_MUL1(in[1]);
  const uint64_t cd = (uint64_t)( d1 & 0xffff) <<  0 |
                      (uint64_t)( c1 & 0xffff) << 16 |
                      (uint64_t)(-c1 & 0xffff) << 32 |
                      (uint64_t)(-d1 & 0xffff) << 48;
  const int16x4_t CD = vcreate_s16(cd);
  const int16x4_t B = vqadd_s16(A, CD);
  const int16x8_t m0_m1 = vcombine_s16(vqadd_s16(B, d4), vqadd_s16(B, c4));
  const int16x8_t m2_m3 = vcombine_s16(vqsub_s16(B, c4), vqsub_s16(B, d4));
  Add4x4_NEON(m0_m1, m2_m3, dst);
}

static void DC4_NEON(uint8_t* dst) {
  const uint8x8_t A = vld1_u8(dst - BPS);
  const uint16x4_t p0 = vpaddl_u8(A);
  const uint16x4_t p1 = vpadd_u16(p0, p0);
  const uint8x8_t L0 = vld1_u8(dst + 0 * BPS - 1);
  const uint8x8_t L1 = vld1_u8(dst + 1 * BPS - 1);
  const uint8x8_t L2 = vld1_u8(dst + 2 * BPS - 1);
  const uint8x8_t L3 = vld1_u8(dst + 3 * BPS - 1);
  const uint16x8_t s0 = vaddl_u8(L0, L1);
  const uint16x8_t s1 = vaddl_u8(L2, L3);
  const uint16x8_t s01 = vaddq_u16(s0, s1);
  const uint16x8_t sum = vaddq_u16(s01, vcombine_u16(p1, p1));
  const uint8x8_t dc0 = vrshrn_n_u16(sum, 3);
  const uint8x8_t dc = vdup_lane_u8(dc0, 0);
  int i;
  for (i = 0; i < 4; ++i) {
    vst1_lane_u32((uint32_t*)(dst + i * BPS), vreinterpret_u32_u8(dc), 0);
  }
}

static WEBP_INLINE void TrueMotion_NEON(uint8_t* dst, int size) {
  const uint8x8_t TL = vld1_dup_u8(dst - BPS - 1);
  const uint8x8_t T = vld1_u8(dst - BPS);
  const uint16x8_t d = vsubl_u8(T, TL);
  int y;
  for (y = 0; y < size; y += 4) {

    const uint8x8_t L0 = vld1_dup_u8(dst + 0 * BPS - 1);
    const uint8x8_t L1 = vld1_dup_u8(dst + 1 * BPS - 1);
    const uint8x8_t L2 = vld1_dup_u8(dst + 2 * BPS - 1);
    const uint8x8_t L3 = vld1_dup_u8(dst + 3 * BPS - 1);

    const int16x8_t r0 = vreinterpretq_s16_u16(vaddw_u8(d, L0));
    const int16x8_t r1 = vreinterpretq_s16_u16(vaddw_u8(d, L1));
    const int16x8_t r2 = vreinterpretq_s16_u16(vaddw_u8(d, L2));
    const int16x8_t r3 = vreinterpretq_s16_u16(vaddw_u8(d, L3));

    const uint32x2_t r0_u32 = vreinterpret_u32_u8(vqmovun_s16(r0));
    const uint32x2_t r1_u32 = vreinterpret_u32_u8(vqmovun_s16(r1));
    const uint32x2_t r2_u32 = vreinterpret_u32_u8(vqmovun_s16(r2));
    const uint32x2_t r3_u32 = vreinterpret_u32_u8(vqmovun_s16(r3));
    if (size == 4) {
      vst1_lane_u32((uint32_t*)(dst + 0 * BPS), r0_u32, 0);
      vst1_lane_u32((uint32_t*)(dst + 1 * BPS), r1_u32, 0);
      vst1_lane_u32((uint32_t*)(dst + 2 * BPS), r2_u32, 0);
      vst1_lane_u32((uint32_t*)(dst + 3 * BPS), r3_u32, 0);
    } else {
      vst1_u32((uint32_t*)(dst + 0 * BPS), r0_u32);
      vst1_u32((uint32_t*)(dst + 1 * BPS), r1_u32);
      vst1_u32((uint32_t*)(dst + 2 * BPS), r2_u32);
      vst1_u32((uint32_t*)(dst + 3 * BPS), r3_u32);
    }
    dst += 4 * BPS;
  }
}

static void TM4_NEON(uint8_t* dst) { TrueMotion_NEON(dst, 4); }

static void VE4_NEON(uint8_t* dst) {

  const uint64x1_t A0 = vreinterpret_u64_u8(vld1_u8(dst - BPS - 1));
  const uint64x1_t A1 = vshr_n_u64(A0, 8);
  const uint64x1_t A2 = vshr_n_u64(A0, 16);
  const uint8x8_t ABCDEFGH = vreinterpret_u8_u64(A0);
  const uint8x8_t BCDEFGH0 = vreinterpret_u8_u64(A1);
  const uint8x8_t CDEFGH00 = vreinterpret_u8_u64(A2);
  const uint8x8_t b = vhadd_u8(ABCDEFGH, CDEFGH00);
  const uint8x8_t avg = vrhadd_u8(b, BCDEFGH0);
  int i;
  for (i = 0; i < 4; ++i) {
    vst1_lane_u32((uint32_t*)(dst + i * BPS), vreinterpret_u32_u8(avg), 0);
  }
}

static void RD4_NEON(uint8_t* dst) {
  const uint8x8_t XABCD_u8 = vld1_u8(dst - BPS - 1);
  const uint64x1_t XABCD = vreinterpret_u64_u8(XABCD_u8);
  const uint64x1_t ____XABC = vshl_n_u64(XABCD, 32);
  const uint32_t I = dst[-1 + 0 * BPS];
  const uint32_t J = dst[-1 + 1 * BPS];
  const uint32_t K = dst[-1 + 2 * BPS];
  const uint32_t L = dst[-1 + 3 * BPS];
  const uint64x1_t LKJI____ =
      vcreate_u64((uint64_t)L | (K << 8) | (J << 16) | (I << 24));
  const uint64x1_t LKJIXABC = vorr_u64(LKJI____, ____XABC);
  const uint8x8_t KJIXABC_ = vreinterpret_u8_u64(vshr_n_u64(LKJIXABC, 8));
  const uint8x8_t JIXABC__ = vreinterpret_u8_u64(vshr_n_u64(LKJIXABC, 16));
  const uint8_t D = vget_lane_u8(XABCD_u8, 4);
  const uint8x8_t JIXABCD_ = vset_lane_u8(D, JIXABC__, 6);
  const uint8x8_t LKJIXABC_u8 = vreinterpret_u8_u64(LKJIXABC);
  const uint8x8_t avg1 = vhadd_u8(JIXABCD_, LKJIXABC_u8);
  const uint8x8_t avg2 = vrhadd_u8(avg1, KJIXABC_);
  const uint64x1_t avg2_u64 = vreinterpret_u64_u8(avg2);
  const uint32x2_t r3 = vreinterpret_u32_u8(avg2);
  const uint32x2_t r2 = vreinterpret_u32_u64(vshr_n_u64(avg2_u64, 8));
  const uint32x2_t r1 = vreinterpret_u32_u64(vshr_n_u64(avg2_u64, 16));
  const uint32x2_t r0 = vreinterpret_u32_u64(vshr_n_u64(avg2_u64, 24));
  vst1_lane_u32((uint32_t*)(dst + 0 * BPS), r0, 0);
  vst1_lane_u32((uint32_t*)(dst + 1 * BPS), r1, 0);
  vst1_lane_u32((uint32_t*)(dst + 2 * BPS), r2, 0);
  vst1_lane_u32((uint32_t*)(dst + 3 * BPS), r3, 0);
}

static void LD4_NEON(uint8_t* dst) {

  const uint8x8_t ABCDEFGH = vld1_u8(dst - BPS + 0);
  const uint8x8_t BCDEFGH0 = vld1_u8(dst - BPS + 1);
  const uint8x8_t CDEFGH00 = vld1_u8(dst - BPS + 2);
  const uint8x8_t CDEFGHH0 = vset_lane_u8(dst[-BPS + 7], CDEFGH00, 6);
  const uint8x8_t avg1 = vhadd_u8(ABCDEFGH, CDEFGHH0);
  const uint8x8_t avg2 = vrhadd_u8(avg1, BCDEFGH0);
  const uint64x1_t avg2_u64 = vreinterpret_u64_u8(avg2);
  const uint32x2_t r0 = vreinterpret_u32_u8(avg2);
  const uint32x2_t r1 = vreinterpret_u32_u64(vshr_n_u64(avg2_u64, 8));
  const uint32x2_t r2 = vreinterpret_u32_u64(vshr_n_u64(avg2_u64, 16));
  const uint32x2_t r3 = vreinterpret_u32_u64(vshr_n_u64(avg2_u64, 24));
  vst1_lane_u32((uint32_t*)(dst + 0 * BPS), r0, 0);
  vst1_lane_u32((uint32_t*)(dst + 1 * BPS), r1, 0);
  vst1_lane_u32((uint32_t*)(dst + 2 * BPS), r2, 0);
  vst1_lane_u32((uint32_t*)(dst + 3 * BPS), r3, 0);
}

static void VE8uv_NEON(uint8_t* dst) {
  const uint8x8_t top = vld1_u8(dst - BPS);
  int j;
  for (j = 0; j < 8; ++j) {
    vst1_u8(dst + j * BPS, top);
  }
}

static void HE8uv_NEON(uint8_t* dst) {
  int j;
  for (j = 0; j < 8; ++j) {
    const uint8x8_t left = vld1_dup_u8(dst - 1);
    vst1_u8(dst, left);
    dst += BPS;
  }
}

static WEBP_INLINE void DC8_NEON(uint8_t* dst, int do_top, int do_left) {
  uint16x8_t sum_top;
  uint16x8_t sum_left;
  uint8x8_t dc0;

  if (do_top) {
    const uint8x8_t A = vld1_u8(dst - BPS);
#if WEBP_AARCH64
    const uint16_t p2 = vaddlv_u8(A);
    sum_top = vdupq_n_u16(p2);
#else
    const uint16x4_t p0 = vpaddl_u8(A);
    const uint16x4_t p1 = vpadd_u16(p0, p0);
    const uint16x4_t p2 = vpadd_u16(p1, p1);
    sum_top = vcombine_u16(p2, p2);
#endif
  }

  if (do_left) {
    const uint8x8_t L0 = vld1_u8(dst + 0 * BPS - 1);
    const uint8x8_t L1 = vld1_u8(dst + 1 * BPS - 1);
    const uint8x8_t L2 = vld1_u8(dst + 2 * BPS - 1);
    const uint8x8_t L3 = vld1_u8(dst + 3 * BPS - 1);
    const uint8x8_t L4 = vld1_u8(dst + 4 * BPS - 1);
    const uint8x8_t L5 = vld1_u8(dst + 5 * BPS - 1);
    const uint8x8_t L6 = vld1_u8(dst + 6 * BPS - 1);
    const uint8x8_t L7 = vld1_u8(dst + 7 * BPS - 1);
    const uint16x8_t s0 = vaddl_u8(L0, L1);
    const uint16x8_t s1 = vaddl_u8(L2, L3);
    const uint16x8_t s2 = vaddl_u8(L4, L5);
    const uint16x8_t s3 = vaddl_u8(L6, L7);
    const uint16x8_t s01 = vaddq_u16(s0, s1);
    const uint16x8_t s23 = vaddq_u16(s2, s3);
    sum_left = vaddq_u16(s01, s23);
  }

  if (do_top && do_left) {
    const uint16x8_t sum = vaddq_u16(sum_left, sum_top);
    dc0 = vrshrn_n_u16(sum, 4);
  } else if (do_top) {
    dc0 = vrshrn_n_u16(sum_top, 3);
  } else if (do_left) {
    dc0 = vrshrn_n_u16(sum_left, 3);
  } else {
    dc0 = vdup_n_u8(0x80);
  }

  {
    const uint8x8_t dc = vdup_lane_u8(dc0, 0);
    int i;
    for (i = 0; i < 8; ++i) {
      vst1_u32((uint32_t*)(dst + i * BPS), vreinterpret_u32_u8(dc));
    }
  }
}

static void DC8uv_NEON(uint8_t* dst) { DC8_NEON(dst, 1, 1); }
static void DC8uvNoTop_NEON(uint8_t* dst) { DC8_NEON(dst, 0, 1); }
static void DC8uvNoLeft_NEON(uint8_t* dst) { DC8_NEON(dst, 1, 0); }
static void DC8uvNoTopLeft_NEON(uint8_t* dst) { DC8_NEON(dst, 0, 0); }

static void TM8uv_NEON(uint8_t* dst) { TrueMotion_NEON(dst, 8); }

static void VE16_NEON(uint8_t* dst) {
  const uint8x16_t top = vld1q_u8(dst - BPS);
  int j;
  for (j = 0; j < 16; ++j) {
    vst1q_u8(dst + j * BPS, top);
  }
}

static void HE16_NEON(uint8_t* dst) {
  int j;
  for (j = 0; j < 16; ++j) {
    const uint8x16_t left = vld1q_dup_u8(dst - 1);
    vst1q_u8(dst, left);
    dst += BPS;
  }
}

static WEBP_INLINE void DC16_NEON(uint8_t* dst, int do_top, int do_left) {
  uint16x8_t sum_top;
  uint16x8_t sum_left;
  uint8x8_t dc0;

  if (do_top) {
    const uint8x16_t A = vld1q_u8(dst - BPS);
#if WEBP_AARCH64
    const uint16_t p3 = vaddlvq_u8(A);
    sum_top = vdupq_n_u16(p3);
#else
    const uint16x8_t p0 = vpaddlq_u8(A);
    const uint16x4_t p1 = vadd_u16(vget_low_u16(p0), vget_high_u16(p0));
    const uint16x4_t p2 = vpadd_u16(p1, p1);
    const uint16x4_t p3 = vpadd_u16(p2, p2);
    sum_top = vcombine_u16(p3, p3);
#endif
  }

  if (do_left) {
    int i;
    sum_left = vdupq_n_u16(0);
    for (i = 0; i < 16; i += 8) {
      const uint8x8_t L0 = vld1_u8(dst + (i + 0) * BPS - 1);
      const uint8x8_t L1 = vld1_u8(dst + (i + 1) * BPS - 1);
      const uint8x8_t L2 = vld1_u8(dst + (i + 2) * BPS - 1);
      const uint8x8_t L3 = vld1_u8(dst + (i + 3) * BPS - 1);
      const uint8x8_t L4 = vld1_u8(dst + (i + 4) * BPS - 1);
      const uint8x8_t L5 = vld1_u8(dst + (i + 5) * BPS - 1);
      const uint8x8_t L6 = vld1_u8(dst + (i + 6) * BPS - 1);
      const uint8x8_t L7 = vld1_u8(dst + (i + 7) * BPS - 1);
      const uint16x8_t s0 = vaddl_u8(L0, L1);
      const uint16x8_t s1 = vaddl_u8(L2, L3);
      const uint16x8_t s2 = vaddl_u8(L4, L5);
      const uint16x8_t s3 = vaddl_u8(L6, L7);
      const uint16x8_t s01 = vaddq_u16(s0, s1);
      const uint16x8_t s23 = vaddq_u16(s2, s3);
      const uint16x8_t sum = vaddq_u16(s01, s23);
      sum_left = vaddq_u16(sum_left, sum);
    }
  }

  if (do_top && do_left) {
    const uint16x8_t sum = vaddq_u16(sum_left, sum_top);
    dc0 = vrshrn_n_u16(sum, 5);
  } else if (do_top) {
    dc0 = vrshrn_n_u16(sum_top, 4);
  } else if (do_left) {
    dc0 = vrshrn_n_u16(sum_left, 4);
  } else {
    dc0 = vdup_n_u8(0x80);
  }

  {
    const uint8x16_t dc = vdupq_lane_u8(dc0, 0);
    int i;
    for (i = 0; i < 16; ++i) {
      vst1q_u8(dst + i * BPS, dc);
    }
  }
}

static void DC16TopLeft_NEON(uint8_t* dst) { DC16_NEON(dst, 1, 1); }
static void DC16NoTop_NEON(uint8_t* dst) { DC16_NEON(dst, 0, 1); }
static void DC16NoLeft_NEON(uint8_t* dst) { DC16_NEON(dst, 1, 0); }
static void DC16NoTopLeft_NEON(uint8_t* dst) { DC16_NEON(dst, 0, 0); }

static void TM16_NEON(uint8_t* dst) {
  const uint8x8_t TL = vld1_dup_u8(dst - BPS - 1);
  const uint8x16_t T = vld1q_u8(dst - BPS);

  const uint16x8_t d_lo = vsubl_u8(vget_low_u8(T), TL);
  const uint16x8_t d_hi = vsubl_u8(vget_high_u8(T), TL);
  int y;
  for (y = 0; y < 16; y += 4) {

    const uint8x8_t L0 = vld1_dup_u8(dst + 0 * BPS - 1);
    const uint8x8_t L1 = vld1_dup_u8(dst + 1 * BPS - 1);
    const uint8x8_t L2 = vld1_dup_u8(dst + 2 * BPS - 1);
    const uint8x8_t L3 = vld1_dup_u8(dst + 3 * BPS - 1);

    const int16x8_t r0_lo = vreinterpretq_s16_u16(vaddw_u8(d_lo, L0));
    const int16x8_t r1_lo = vreinterpretq_s16_u16(vaddw_u8(d_lo, L1));
    const int16x8_t r2_lo = vreinterpretq_s16_u16(vaddw_u8(d_lo, L2));
    const int16x8_t r3_lo = vreinterpretq_s16_u16(vaddw_u8(d_lo, L3));
    const int16x8_t r0_hi = vreinterpretq_s16_u16(vaddw_u8(d_hi, L0));
    const int16x8_t r1_hi = vreinterpretq_s16_u16(vaddw_u8(d_hi, L1));
    const int16x8_t r2_hi = vreinterpretq_s16_u16(vaddw_u8(d_hi, L2));
    const int16x8_t r3_hi = vreinterpretq_s16_u16(vaddw_u8(d_hi, L3));

    const uint8x16_t row0 = vcombine_u8(vqmovun_s16(r0_lo), vqmovun_s16(r0_hi));
    const uint8x16_t row1 = vcombine_u8(vqmovun_s16(r1_lo), vqmovun_s16(r1_hi));
    const uint8x16_t row2 = vcombine_u8(vqmovun_s16(r2_lo), vqmovun_s16(r2_hi));
    const uint8x16_t row3 = vcombine_u8(vqmovun_s16(r3_lo), vqmovun_s16(r3_hi));
    vst1q_u8(dst + 0 * BPS, row0);
    vst1q_u8(dst + 1 * BPS, row1);
    vst1q_u8(dst + 2 * BPS, row2);
    vst1q_u8(dst + 3 * BPS, row3);
    dst += 4 * BPS;
  }
}

extern void VP8DspInitNEON(void);

WEBP_TSAN_IGNORE_FUNCTION void VP8DspInitNEON(void) {
  VP8Transform = TransformTwo_NEON;
  VP8TransformAC3 = TransformAC3_NEON;
  VP8TransformDC = TransformDC_NEON;
  VP8TransformWHT = TransformWHT_NEON;

  VP8VFilter16 = VFilter16_NEON;
  VP8VFilter16i = VFilter16i_NEON;
  VP8HFilter16 = HFilter16_NEON;
#if !defined(WORK_AROUND_GCC)
  VP8HFilter16i = HFilter16i_NEON;
#endif
  VP8VFilter8 = VFilter8_NEON;
  VP8VFilter8i = VFilter8i_NEON;
#if !defined(WORK_AROUND_GCC)
  VP8HFilter8 = HFilter8_NEON;
  VP8HFilter8i = HFilter8i_NEON;
#endif
  VP8SimpleVFilter16 = SimpleVFilter16_NEON;
  VP8SimpleHFilter16 = SimpleHFilter16_NEON;
  VP8SimpleVFilter16i = SimpleVFilter16i_NEON;
  VP8SimpleHFilter16i = SimpleHFilter16i_NEON;

  VP8PredLuma4[0] = DC4_NEON;
  VP8PredLuma4[1] = TM4_NEON;
  VP8PredLuma4[2] = VE4_NEON;
  VP8PredLuma4[4] = RD4_NEON;
  VP8PredLuma4[6] = LD4_NEON;

  VP8PredLuma16[0] = DC16TopLeft_NEON;
  VP8PredLuma16[1] = TM16_NEON;
  VP8PredLuma16[2] = VE16_NEON;
  VP8PredLuma16[3] = HE16_NEON;
  VP8PredLuma16[4] = DC16NoTop_NEON;
  VP8PredLuma16[5] = DC16NoLeft_NEON;
  VP8PredLuma16[6] = DC16NoTopLeft_NEON;

  VP8PredChroma8[0] = DC8uv_NEON;
  VP8PredChroma8[1] = TM8uv_NEON;
  VP8PredChroma8[2] = VE8uv_NEON;
  VP8PredChroma8[3] = HE8uv_NEON;
  VP8PredChroma8[4] = DC8uvNoTop_NEON;
  VP8PredChroma8[5] = DC8uvNoLeft_NEON;
  VP8PredChroma8[6] = DC8uvNoTopLeft_NEON;
}

#else

WEBP_DSP_INIT_STUB(VP8DspInitNEON)

#endif

#if defined(WEBP_USE_SSE2)

#if !defined(USE_TRANSFORM_AC3)
#define USE_TRANSFORM_AC3 0
#endif

#include <emmintrin.h>

#ifndef WEBP_DSP_COMMON_SSE2_H_
#define WEBP_DSP_COMMON_SSE2_H_

#ifdef __cplusplus
extern "C" {
#endif

#if defined(WEBP_USE_SSE2)

#include <emmintrin.h>

#if 0
#include <stdio.h>
static WEBP_INLINE void PrintReg(const __m128i r, const char* const name,
                                 int size) {
  int n;
  union {
    __m128i r;
    uint8_t i8[16];
    uint16_t i16[8];
    uint32_t i32[4];
    uint64_t i64[2];
  } tmp;
  tmp.r = r;
  fprintf(stderr, "%s\t: ", name);
  if (size == 8) {
    for (n = 0; n < 16; ++n) fprintf(stderr, "%.2x ", tmp.i8[n]);
  } else if (size == 16) {
    for (n = 0; n < 8; ++n) fprintf(stderr, "%.4x ", tmp.i16[n]);
  } else if (size == 32) {
    for (n = 0; n < 4; ++n) fprintf(stderr, "%.8x ", tmp.i32[n]);
  } else {
    for (n = 0; n < 2; ++n) fprintf(stderr, "%.16lx ", tmp.i64[n]);
  }
  fprintf(stderr, "\n");
}
#endif

static WEBP_INLINE int VP8HorizontalAdd8b(const __m128i* const a) {
  const __m128i zero = _mm_setzero_si128();
  const __m128i sad8x2 = _mm_sad_epu8(*a, zero);

  const __m128i sum = _mm_add_epi32(sad8x2, _mm_shuffle_epi32(sad8x2, 2));
  return _mm_cvtsi128_si32(sum);
}

static WEBP_INLINE void VP8Transpose_2_4x4_16b(
    const __m128i* const in0, const __m128i* const in1,
    const __m128i* const in2, const __m128i* const in3, __m128i* const out0,
    __m128i* const out1, __m128i* const out2, __m128i* const out3) {

  const __m128i transpose0_0 = _mm_unpacklo_epi16(*in0, *in1);
  const __m128i transpose0_1 = _mm_unpacklo_epi16(*in2, *in3);
  const __m128i transpose0_2 = _mm_unpackhi_epi16(*in0, *in1);
  const __m128i transpose0_3 = _mm_unpackhi_epi16(*in2, *in3);

  const __m128i transpose1_0 = _mm_unpacklo_epi32(transpose0_0, transpose0_1);
  const __m128i transpose1_1 = _mm_unpacklo_epi32(transpose0_2, transpose0_3);
  const __m128i transpose1_2 = _mm_unpackhi_epi32(transpose0_0, transpose0_1);
  const __m128i transpose1_3 = _mm_unpackhi_epi32(transpose0_2, transpose0_3);

  *out0 = _mm_unpacklo_epi64(transpose1_0, transpose1_1);
  *out1 = _mm_unpackhi_epi64(transpose1_0, transpose1_1);
  *out2 = _mm_unpacklo_epi64(transpose1_2, transpose1_3);
  *out3 = _mm_unpackhi_epi64(transpose1_2, transpose1_3);

}

#define VP8PlanarTo24bHelper(IN, OUT)                            \
  do {                                                           \
    const __m128i v_mask = _mm_set1_epi16(0x00ff);               \
                         \
    (OUT##0) = _mm_packus_epi16(_mm_and_si128((IN##0), v_mask),  \
                                _mm_and_si128((IN##1), v_mask)); \
    (OUT##1) = _mm_packus_epi16(_mm_and_si128((IN##2), v_mask),  \
                                _mm_and_si128((IN##3), v_mask)); \
    (OUT##2) = _mm_packus_epi16(_mm_and_si128((IN##4), v_mask),  \
                                _mm_and_si128((IN##5), v_mask)); \
                         \
    (OUT##3) = _mm_packus_epi16(_mm_srli_epi16((IN##0), 8),      \
                                _mm_srli_epi16((IN##1), 8));     \
    (OUT##4) = _mm_packus_epi16(_mm_srli_epi16((IN##2), 8),      \
                                _mm_srli_epi16((IN##3), 8));     \
    (OUT##5) = _mm_packus_epi16(_mm_srli_epi16((IN##4), 8),      \
                                _mm_srli_epi16((IN##5), 8));     \
  } while (0)

static WEBP_INLINE void VP8PlanarTo24b_SSE2(
    __m128i* const in0, __m128i* const in1, __m128i* const in2,
    __m128i* const in3, __m128i* const in4, __m128i* const in5) {

  __m128i tmp0, tmp1, tmp2, tmp3, tmp4, tmp5;
  VP8PlanarTo24bHelper(*in, tmp);
  VP8PlanarTo24bHelper(tmp, *in);
  VP8PlanarTo24bHelper(*in, tmp);

  {
    __m128i out0, out1, out2, out3, out4, out5;
    VP8PlanarTo24bHelper(tmp, out);
    VP8PlanarTo24bHelper(out, *in);
  }
}

#undef VP8PlanarTo24bHelper

static WEBP_INLINE void VP8L32bToPlanar_SSE2(__m128i* const in0,
                                             __m128i* const in1,
                                             __m128i* const in2,
                                             __m128i* const in3) {

  const __m128i A0 = _mm_unpacklo_epi8(*in0, *in1);
  const __m128i A1 = _mm_unpackhi_epi8(*in0, *in1);
  const __m128i A2 = _mm_unpacklo_epi8(*in2, *in3);
  const __m128i A3 = _mm_unpackhi_epi8(*in2, *in3);
  const __m128i B0 = _mm_unpacklo_epi8(A0, A1);
  const __m128i B1 = _mm_unpackhi_epi8(A0, A1);
  const __m128i B2 = _mm_unpacklo_epi8(A2, A3);
  const __m128i B3 = _mm_unpackhi_epi8(A2, A3);

  const __m128i C0 = _mm_unpacklo_epi8(B0, B1);
  const __m128i C1 = _mm_unpackhi_epi8(B0, B1);
  const __m128i C2 = _mm_unpacklo_epi8(B2, B3);
  const __m128i C3 = _mm_unpackhi_epi8(B2, B3);

  *in0 = _mm_unpackhi_epi64(C1, C3);
  *in1 = _mm_unpacklo_epi64(C1, C3);
  *in2 = _mm_unpackhi_epi64(C0, C2);
  *in3 = _mm_unpacklo_epi64(C0, C2);
}

#endif

#ifdef __cplusplus
}
#endif

#endif

static void Transform_SSE2(const int16_t* WEBP_RESTRICT in,
                           uint8_t* WEBP_RESTRICT dst, int do_two) {

  const __m128i k1 = _mm_set1_epi16(20091);
  const __m128i k2 = _mm_set1_epi16(-30068);
  __m128i T0, T1, T2, T3;

  __m128i in0, in1, in2, in3;
  {
    in0 = _mm_loadl_epi64((const __m128i*)&in[0]);
    in1 = _mm_loadl_epi64((const __m128i*)&in[4]);
    in2 = _mm_loadl_epi64((const __m128i*)&in[8]);
    in3 = _mm_loadl_epi64((const __m128i*)&in[12]);

    if (do_two) {
      const __m128i inB0 = _mm_loadl_epi64((const __m128i*)&in[16]);
      const __m128i inB1 = _mm_loadl_epi64((const __m128i*)&in[20]);
      const __m128i inB2 = _mm_loadl_epi64((const __m128i*)&in[24]);
      const __m128i inB3 = _mm_loadl_epi64((const __m128i*)&in[28]);
      in0 = _mm_unpacklo_epi64(in0, inB0);
      in1 = _mm_unpacklo_epi64(in1, inB1);
      in2 = _mm_unpacklo_epi64(in2, inB2);
      in3 = _mm_unpacklo_epi64(in3, inB3);

    }
  }

  {

    const __m128i a = _mm_add_epi16(in0, in2);
    const __m128i b = _mm_sub_epi16(in0, in2);

    const __m128i c1 = _mm_mulhi_epi16(in1, k2);
    const __m128i c2 = _mm_mulhi_epi16(in3, k1);
    const __m128i c3 = _mm_sub_epi16(in1, in3);
    const __m128i c4 = _mm_sub_epi16(c1, c2);
    const __m128i c = _mm_add_epi16(c3, c4);

    const __m128i d1 = _mm_mulhi_epi16(in1, k1);
    const __m128i d2 = _mm_mulhi_epi16(in3, k2);
    const __m128i d3 = _mm_add_epi16(in1, in3);
    const __m128i d4 = _mm_add_epi16(d1, d2);
    const __m128i d = _mm_add_epi16(d3, d4);

    const __m128i tmp0 = _mm_add_epi16(a, d);
    const __m128i tmp1 = _mm_add_epi16(b, c);
    const __m128i tmp2 = _mm_sub_epi16(b, c);
    const __m128i tmp3 = _mm_sub_epi16(a, d);

    VP8Transpose_2_4x4_16b(&tmp0, &tmp1, &tmp2, &tmp3, &T0, &T1, &T2, &T3);
  }

  {

    const __m128i four = _mm_set1_epi16(4);
    const __m128i dc = _mm_add_epi16(T0, four);
    const __m128i a =  _mm_add_epi16(dc, T2);
    const __m128i b =  _mm_sub_epi16(dc, T2);

    const __m128i c1 = _mm_mulhi_epi16(T1, k2);
    const __m128i c2 = _mm_mulhi_epi16(T3, k1);
    const __m128i c3 = _mm_sub_epi16(T1, T3);
    const __m128i c4 = _mm_sub_epi16(c1, c2);
    const __m128i c = _mm_add_epi16(c3, c4);

    const __m128i d1 = _mm_mulhi_epi16(T1, k1);
    const __m128i d2 = _mm_mulhi_epi16(T3, k2);
    const __m128i d3 = _mm_add_epi16(T1, T3);
    const __m128i d4 = _mm_add_epi16(d1, d2);
    const __m128i d = _mm_add_epi16(d3, d4);

    const __m128i tmp0 = _mm_add_epi16(a, d);
    const __m128i tmp1 = _mm_add_epi16(b, c);
    const __m128i tmp2 = _mm_sub_epi16(b, c);
    const __m128i tmp3 = _mm_sub_epi16(a, d);
    const __m128i shifted0 = _mm_srai_epi16(tmp0, 3);
    const __m128i shifted1 = _mm_srai_epi16(tmp1, 3);
    const __m128i shifted2 = _mm_srai_epi16(tmp2, 3);
    const __m128i shifted3 = _mm_srai_epi16(tmp3, 3);

    VP8Transpose_2_4x4_16b(&shifted0, &shifted1, &shifted2, &shifted3, &T0, &T1,
                           &T2, &T3);
  }

  {
    const __m128i zero = _mm_setzero_si128();

    __m128i dst0, dst1, dst2, dst3;
    if (do_two) {

      dst0 = _mm_loadl_epi64((__m128i*)(dst + 0 * BPS));
      dst1 = _mm_loadl_epi64((__m128i*)(dst + 1 * BPS));
      dst2 = _mm_loadl_epi64((__m128i*)(dst + 2 * BPS));
      dst3 = _mm_loadl_epi64((__m128i*)(dst + 3 * BPS));
    } else {

      dst0 = _mm_cvtsi32_si128(WebPMemToInt32(dst + 0 * BPS));
      dst1 = _mm_cvtsi32_si128(WebPMemToInt32(dst + 1 * BPS));
      dst2 = _mm_cvtsi32_si128(WebPMemToInt32(dst + 2 * BPS));
      dst3 = _mm_cvtsi32_si128(WebPMemToInt32(dst + 3 * BPS));
    }

    dst0 = _mm_unpacklo_epi8(dst0, zero);
    dst1 = _mm_unpacklo_epi8(dst1, zero);
    dst2 = _mm_unpacklo_epi8(dst2, zero);
    dst3 = _mm_unpacklo_epi8(dst3, zero);

    dst0 = _mm_add_epi16(dst0, T0);
    dst1 = _mm_add_epi16(dst1, T1);
    dst2 = _mm_add_epi16(dst2, T2);
    dst3 = _mm_add_epi16(dst3, T3);

    dst0 = _mm_packus_epi16(dst0, dst0);
    dst1 = _mm_packus_epi16(dst1, dst1);
    dst2 = _mm_packus_epi16(dst2, dst2);
    dst3 = _mm_packus_epi16(dst3, dst3);

    if (do_two) {

      _mm_storel_epi64((__m128i*)(dst + 0 * BPS), dst0);
      _mm_storel_epi64((__m128i*)(dst + 1 * BPS), dst1);
      _mm_storel_epi64((__m128i*)(dst + 2 * BPS), dst2);
      _mm_storel_epi64((__m128i*)(dst + 3 * BPS), dst3);
    } else {

      WebPInt32ToMem(dst + 0 * BPS, _mm_cvtsi128_si32(dst0));
      WebPInt32ToMem(dst + 1 * BPS, _mm_cvtsi128_si32(dst1));
      WebPInt32ToMem(dst + 2 * BPS, _mm_cvtsi128_si32(dst2));
      WebPInt32ToMem(dst + 3 * BPS, _mm_cvtsi128_si32(dst3));
    }
  }
}

#if (USE_TRANSFORM_AC3 == 1)

static void TransformAC3_SSE2(const int16_t* WEBP_RESTRICT in,
                              uint8_t* WEBP_RESTRICT dst) {
  const __m128i A = _mm_set1_epi16(in[0] + 4);
  const __m128i c4 = _mm_set1_epi16(WEBP_TRANSFORM_AC3_MUL2(in[4]));
  const __m128i d4 = _mm_set1_epi16(WEBP_TRANSFORM_AC3_MUL1(in[4]));
  const int c1 = WEBP_TRANSFORM_AC3_MUL2(in[1]);
  const int d1 = WEBP_TRANSFORM_AC3_MUL1(in[1]);
  const __m128i CD = _mm_set_epi16(0, 0, 0, 0, -d1, -c1, c1, d1);
  const __m128i B = _mm_adds_epi16(A, CD);
  const __m128i m0 = _mm_adds_epi16(B, d4);
  const __m128i m1 = _mm_adds_epi16(B, c4);
  const __m128i m2 = _mm_subs_epi16(B, c4);
  const __m128i m3 = _mm_subs_epi16(B, d4);
  const __m128i zero = _mm_setzero_si128();

  __m128i dst0 = _mm_cvtsi32_si128(WebPMemToInt32(dst + 0 * BPS));
  __m128i dst1 = _mm_cvtsi32_si128(WebPMemToInt32(dst + 1 * BPS));
  __m128i dst2 = _mm_cvtsi32_si128(WebPMemToInt32(dst + 2 * BPS));
  __m128i dst3 = _mm_cvtsi32_si128(WebPMemToInt32(dst + 3 * BPS));

  dst0 = _mm_unpacklo_epi8(dst0, zero);
  dst1 = _mm_unpacklo_epi8(dst1, zero);
  dst2 = _mm_unpacklo_epi8(dst2, zero);
  dst3 = _mm_unpacklo_epi8(dst3, zero);

  dst0 = _mm_adds_epi16(dst0, _mm_srai_epi16(m0, 3));
  dst1 = _mm_adds_epi16(dst1, _mm_srai_epi16(m1, 3));
  dst2 = _mm_adds_epi16(dst2, _mm_srai_epi16(m2, 3));
  dst3 = _mm_adds_epi16(dst3, _mm_srai_epi16(m3, 3));

  dst0 = _mm_packus_epi16(dst0, dst0);
  dst1 = _mm_packus_epi16(dst1, dst1);
  dst2 = _mm_packus_epi16(dst2, dst2);
  dst3 = _mm_packus_epi16(dst3, dst3);

  WebPInt32ToMem(dst + 0 * BPS, _mm_cvtsi128_si32(dst0));
  WebPInt32ToMem(dst + 1 * BPS, _mm_cvtsi128_si32(dst1));
  WebPInt32ToMem(dst + 2 * BPS, _mm_cvtsi128_si32(dst2));
  WebPInt32ToMem(dst + 3 * BPS, _mm_cvtsi128_si32(dst3));
}

#endif

#define MM_ABS(p, q)  _mm_or_si128(                                            \
    _mm_subs_epu8((q), (p)),                                                   \
    _mm_subs_epu8((p), (q)))

static WEBP_INLINE void SignedShift8b_SSE2(__m128i* const x) {
  const __m128i zero = _mm_setzero_si128();
  const __m128i lo_0 = _mm_unpacklo_epi8(zero, *x);
  const __m128i hi_0 = _mm_unpackhi_epi8(zero, *x);
  const __m128i lo_1 = _mm_srai_epi16(lo_0, 3 + 8);
  const __m128i hi_1 = _mm_srai_epi16(hi_0, 3 + 8);
  *x = _mm_packs_epi16(lo_1, hi_1);
}

#define FLIP_SIGN_BIT2(a, b) do {                                              \
  (a) = _mm_xor_si128(a, sign_bit);                                            \
  (b) = _mm_xor_si128(b, sign_bit);                                            \
} while (0)

#define FLIP_SIGN_BIT4(a, b, c, d) do {                                        \
  FLIP_SIGN_BIT2(a, b);                                                        \
  FLIP_SIGN_BIT2(c, d);                                                        \
} while (0)

static WEBP_INLINE void GetNotHEV_SSE2(const __m128i* const p1,
                                       const __m128i* const p0,
                                       const __m128i* const q0,
                                       const __m128i* const q1,
                                       int hev_thresh, __m128i* const not_hev) {
  const __m128i zero = _mm_setzero_si128();
  const __m128i t_1 = MM_ABS(*p1, *p0);
  const __m128i t_2 = MM_ABS(*q1, *q0);

  const __m128i h = _mm_set1_epi8(hev_thresh);
  const __m128i t_max = _mm_max_epu8(t_1, t_2);

  const __m128i t_max_h = _mm_subs_epu8(t_max, h);
  *not_hev = _mm_cmpeq_epi8(t_max_h, zero);
}

static WEBP_INLINE void GetBaseDelta_SSE2(const __m128i* const p1,
                                          const __m128i* const p0,
                                          const __m128i* const q0,
                                          const __m128i* const q1,
                                          __m128i* const delta) {

  const __m128i p1_q1 = _mm_subs_epi8(*p1, *q1);
  const __m128i q0_p0 = _mm_subs_epi8(*q0, *p0);
  const __m128i s1 = _mm_adds_epi8(p1_q1, q0_p0);
  const __m128i s2 = _mm_adds_epi8(q0_p0, s1);
  const __m128i s3 = _mm_adds_epi8(q0_p0, s2);
  *delta = s3;
}

static WEBP_INLINE void DoSimpleFilter_SSE2(__m128i* const p0,
                                            __m128i* const q0,
                                            const __m128i* const fl) {
  const __m128i k3 = _mm_set1_epi8(3);
  const __m128i k4 = _mm_set1_epi8(4);
  __m128i v3 = _mm_adds_epi8(*fl, k3);
  __m128i v4 = _mm_adds_epi8(*fl, k4);

  SignedShift8b_SSE2(&v4);
  SignedShift8b_SSE2(&v3);
  *q0 = _mm_subs_epi8(*q0, v4);
  *p0 = _mm_adds_epi8(*p0, v3);
}

static WEBP_INLINE void Update2Pixels_SSE2(__m128i* const pi, __m128i* const qi,
                                           const __m128i* const a0_lo,
                                           const __m128i* const a0_hi) {
  const __m128i a1_lo = _mm_srai_epi16(*a0_lo, 7);
  const __m128i a1_hi = _mm_srai_epi16(*a0_hi, 7);
  const __m128i delta = _mm_packs_epi16(a1_lo, a1_hi);
  const __m128i sign_bit = _mm_set1_epi8((char)0x80);
  *pi = _mm_adds_epi8(*pi, delta);
  *qi = _mm_subs_epi8(*qi, delta);
  FLIP_SIGN_BIT2(*pi, *qi);
}

static WEBP_INLINE void NeedsFilter_SSE2(const __m128i* const p1,
                                         const __m128i* const p0,
                                         const __m128i* const q0,
                                         const __m128i* const q1,
                                         int thresh, __m128i* const mask) {
  const __m128i m_thresh = _mm_set1_epi8((char)thresh);
  const __m128i t1 = MM_ABS(*p1, *q1);
  const __m128i kFE = _mm_set1_epi8((char)0xFE);
  const __m128i t2 = _mm_and_si128(t1, kFE);
  const __m128i t3 = _mm_srli_epi16(t2, 1);

  const __m128i t4 = MM_ABS(*p0, *q0);
  const __m128i t5 = _mm_adds_epu8(t4, t4);
  const __m128i t6 = _mm_adds_epu8(t5, t3);

  const __m128i t7 = _mm_subs_epu8(t6, m_thresh);
  *mask = _mm_cmpeq_epi8(t7, _mm_setzero_si128());
}

static WEBP_INLINE void DoFilter2_SSE2(__m128i* const p1, __m128i* const p0,
                                       __m128i* const q0, __m128i* const q1,
                                       int thresh) {
  __m128i a, mask;
  const __m128i sign_bit = _mm_set1_epi8((char)0x80);

  const __m128i p1s = _mm_xor_si128(*p1, sign_bit);
  const __m128i q1s = _mm_xor_si128(*q1, sign_bit);

  NeedsFilter_SSE2(p1, p0, q0, q1, thresh, &mask);

  FLIP_SIGN_BIT2(*p0, *q0);
  GetBaseDelta_SSE2(&p1s, p0, q0, &q1s, &a);
  a = _mm_and_si128(a, mask);
  DoSimpleFilter_SSE2(p0, q0, &a);
  FLIP_SIGN_BIT2(*p0, *q0);
}

static WEBP_INLINE void DoFilter4_SSE2(__m128i* const p1, __m128i* const p0,
                                       __m128i* const q0, __m128i* const q1,
                                       const __m128i* const mask,
                                       int hev_thresh) {
  const __m128i zero = _mm_setzero_si128();
  const __m128i sign_bit = _mm_set1_epi8((char)0x80);
  const __m128i k64 = _mm_set1_epi8(64);
  const __m128i k3 = _mm_set1_epi8(3);
  const __m128i k4 = _mm_set1_epi8(4);
  __m128i not_hev;
  __m128i t1, t2, t3;

  GetNotHEV_SSE2(p1, p0, q0, q1, hev_thresh, &not_hev);

  FLIP_SIGN_BIT4(*p1, *p0, *q0, *q1);

  t1 = _mm_subs_epi8(*p1, *q1);
  t1 = _mm_andnot_si128(not_hev, t1);
  t2 = _mm_subs_epi8(*q0, *p0);
  t1 = _mm_adds_epi8(t1, t2);
  t1 = _mm_adds_epi8(t1, t2);
  t1 = _mm_adds_epi8(t1, t2);
  t1 = _mm_and_si128(t1, *mask);

  t2 = _mm_adds_epi8(t1, k3);
  t3 = _mm_adds_epi8(t1, k4);
  SignedShift8b_SSE2(&t2);
  SignedShift8b_SSE2(&t3);
  *p0 = _mm_adds_epi8(*p0, t2);
  *q0 = _mm_subs_epi8(*q0, t3);
  FLIP_SIGN_BIT2(*p0, *q0);

  t2 = _mm_add_epi8(t3, sign_bit);
  t3 = _mm_avg_epu8(t2, zero);
  t3 = _mm_sub_epi8(t3, k64);

  t3 = _mm_and_si128(not_hev, t3);
  *q1 = _mm_subs_epi8(*q1, t3);
  *p1 = _mm_adds_epi8(*p1, t3);
  FLIP_SIGN_BIT2(*p1, *q1);
}

static WEBP_INLINE void DoFilter6_SSE2(__m128i* const p2, __m128i* const p1,
                                       __m128i* const p0, __m128i* const q0,
                                       __m128i* const q1, __m128i* const q2,
                                       const __m128i* const mask,
                                       int hev_thresh) {
  const __m128i zero = _mm_setzero_si128();
  const __m128i sign_bit = _mm_set1_epi8((char)0x80);
  __m128i a, not_hev;

  GetNotHEV_SSE2(p1, p0, q0, q1, hev_thresh, &not_hev);

  FLIP_SIGN_BIT4(*p1, *p0, *q0, *q1);
  FLIP_SIGN_BIT2(*p2, *q2);
  GetBaseDelta_SSE2(p1, p0, q0, q1, &a);

  {
    const __m128i m = _mm_andnot_si128(not_hev, *mask);
    const __m128i f = _mm_and_si128(a, m);
    DoSimpleFilter_SSE2(p0, q0, &f);
  }

  {
    const __m128i k9 = _mm_set1_epi16(0x0900);
    const __m128i k63 = _mm_set1_epi16(63);

    const __m128i m = _mm_and_si128(not_hev, *mask);
    const __m128i f = _mm_and_si128(a, m);

    const __m128i f_lo = _mm_unpacklo_epi8(zero, f);
    const __m128i f_hi = _mm_unpackhi_epi8(zero, f);

    const __m128i f9_lo = _mm_mulhi_epi16(f_lo, k9);
    const __m128i f9_hi = _mm_mulhi_epi16(f_hi, k9);

    const __m128i a2_lo = _mm_add_epi16(f9_lo, k63);
    const __m128i a2_hi = _mm_add_epi16(f9_hi, k63);

    const __m128i a1_lo = _mm_add_epi16(a2_lo, f9_lo);
    const __m128i a1_hi = _mm_add_epi16(a2_hi, f9_hi);

    const __m128i a0_lo = _mm_add_epi16(a1_lo, f9_lo);
    const __m128i a0_hi = _mm_add_epi16(a1_hi, f9_hi);

    Update2Pixels_SSE2(p2, q2, &a2_lo, &a2_hi);
    Update2Pixels_SSE2(p1, q1, &a1_lo, &a1_hi);
    Update2Pixels_SSE2(p0, q0, &a0_lo, &a0_hi);
  }
}

static WEBP_INLINE void Load8x4_SSE2(const uint8_t* const b, int stride,
                                     __m128i* const p, __m128i* const q) {

  const __m128i A0 = _mm_set_epi32(
      WebPMemToInt32(&b[6 * stride]), WebPMemToInt32(&b[2 * stride]),
      WebPMemToInt32(&b[4 * stride]), WebPMemToInt32(&b[0 * stride]));
  const __m128i A1 = _mm_set_epi32(
      WebPMemToInt32(&b[7 * stride]), WebPMemToInt32(&b[3 * stride]),
      WebPMemToInt32(&b[5 * stride]), WebPMemToInt32(&b[1 * stride]));

  const __m128i B0 = _mm_unpacklo_epi8(A0, A1);
  const __m128i B1 = _mm_unpackhi_epi8(A0, A1);

  const __m128i C0 = _mm_unpacklo_epi16(B0, B1);
  const __m128i C1 = _mm_unpackhi_epi16(B0, B1);

  *p = _mm_unpacklo_epi32(C0, C1);
  *q = _mm_unpackhi_epi32(C0, C1);
}

static WEBP_INLINE void Load16x4_SSE2(const uint8_t* const r0,
                                      const uint8_t* const r8,
                                      int stride,
                                      __m128i* const p1, __m128i* const p0,
                                      __m128i* const q0, __m128i* const q1) {

  Load8x4_SSE2(r0, stride, p1, q0);
  Load8x4_SSE2(r8, stride, p0, q1);

  {

    const __m128i t1 = *p1;
    const __m128i t2 = *q0;
    *p1 = _mm_unpacklo_epi64(t1, *p0);
    *p0 = _mm_unpackhi_epi64(t1, *p0);
    *q0 = _mm_unpacklo_epi64(t2, *q1);
    *q1 = _mm_unpackhi_epi64(t2, *q1);
  }
}

static WEBP_INLINE void Store4x4_SSE2(__m128i* const x,
                                      uint8_t* dst, int stride) {
  int i;
  for (i = 0; i < 4; ++i, dst += stride) {
    WebPInt32ToMem(dst, _mm_cvtsi128_si32(*x));
    *x = _mm_srli_si128(*x, 4);
  }
}

static WEBP_INLINE void Store16x4_SSE2(const __m128i* const p1,
                                       const __m128i* const p0,
                                       const __m128i* const q0,
                                       const __m128i* const q1,
                                       uint8_t* r0, uint8_t* r8,
                                       int stride) {
  __m128i t1, p1_s, p0_s, q0_s, q1_s;

  t1 = *p0;
  p0_s = _mm_unpacklo_epi8(*p1, t1);
  p1_s = _mm_unpackhi_epi8(*p1, t1);

  t1 = *q0;
  q0_s = _mm_unpacklo_epi8(t1, *q1);
  q1_s = _mm_unpackhi_epi8(t1, *q1);

  t1 = p0_s;
  p0_s = _mm_unpacklo_epi16(t1, q0_s);
  q0_s = _mm_unpackhi_epi16(t1, q0_s);

  t1 = p1_s;
  p1_s = _mm_unpacklo_epi16(t1, q1_s);
  q1_s = _mm_unpackhi_epi16(t1, q1_s);

  Store4x4_SSE2(&p0_s, r0, stride);
  r0 += 4 * stride;
  Store4x4_SSE2(&q0_s, r0, stride);

  Store4x4_SSE2(&p1_s, r8, stride);
  r8 += 4 * stride;
  Store4x4_SSE2(&q1_s, r8, stride);
}

static void SimpleVFilter16_SSE2(uint8_t* p, int stride, int thresh) {

  __m128i p1 = _mm_loadu_si128((__m128i*)&p[-2 * stride]);
  __m128i p0 = _mm_loadu_si128((__m128i*)&p[-stride]);
  __m128i q0 = _mm_loadu_si128((__m128i*)&p[0]);
  __m128i q1 = _mm_loadu_si128((__m128i*)&p[stride]);

  DoFilter2_SSE2(&p1, &p0, &q0, &q1, thresh);

  _mm_storeu_si128((__m128i*)&p[-stride], p0);
  _mm_storeu_si128((__m128i*)&p[0], q0);
}

static void SimpleHFilter16_SSE2(uint8_t* p, int stride, int thresh) {
  __m128i p1, p0, q0, q1;

  p -= 2;

  Load16x4_SSE2(p, p + 8 * stride, stride, &p1, &p0, &q0, &q1);
  DoFilter2_SSE2(&p1, &p0, &q0, &q1, thresh);
  Store16x4_SSE2(&p1, &p0, &q0, &q1, p, p + 8 * stride, stride);
}

static void SimpleVFilter16i_SSE2(uint8_t* p, int stride, int thresh) {
  int k;
  for (k = 3; k > 0; --k) {
    p += 4 * stride;
    SimpleVFilter16_SSE2(p, stride, thresh);
  }
}

static void SimpleHFilter16i_SSE2(uint8_t* p, int stride, int thresh) {
  int k;
  for (k = 3; k > 0; --k) {
    p += 4;
    SimpleHFilter16_SSE2(p, stride, thresh);
  }
}

#define MAX_DIFF1(p3, p2, p1, p0, m) do {                                      \
  (m) = MM_ABS(p1, p0);                                                        \
  (m) = _mm_max_epu8(m, MM_ABS(p3, p2));                                       \
  (m) = _mm_max_epu8(m, MM_ABS(p2, p1));                                       \
} while (0)

#define MAX_DIFF2(p3, p2, p1, p0, m) do {                                      \
  (m) = _mm_max_epu8(m, MM_ABS(p1, p0));                                       \
  (m) = _mm_max_epu8(m, MM_ABS(p3, p2));                                       \
  (m) = _mm_max_epu8(m, MM_ABS(p2, p1));                                       \
} while (0)

#define LOAD_H_EDGES4(p, stride, e1, e2, e3, e4) do {                          \
  (e1) = _mm_loadu_si128((__m128i*)&(p)[0 * (stride)]);                        \
  (e2) = _mm_loadu_si128((__m128i*)&(p)[1 * (stride)]);                        \
  (e3) = _mm_loadu_si128((__m128i*)&(p)[2 * (stride)]);                        \
  (e4) = _mm_loadu_si128((__m128i*)&(p)[3 * (stride)]);                        \
} while (0)

#define LOADUV_H_EDGE(p, u, v, stride) do {                                    \
  const __m128i U = _mm_loadl_epi64((__m128i*)&(u)[(stride)]);                 \
  const __m128i V = _mm_loadl_epi64((__m128i*)&(v)[(stride)]);                 \
  (p) = _mm_unpacklo_epi64(U, V);                                              \
} while (0)

#define LOADUV_H_EDGES4(u, v, stride, e1, e2, e3, e4) do {                     \
  LOADUV_H_EDGE(e1, u, v, 0 * (stride));                                       \
  LOADUV_H_EDGE(e2, u, v, 1 * (stride));                                       \
  LOADUV_H_EDGE(e3, u, v, 2 * (stride));                                       \
  LOADUV_H_EDGE(e4, u, v, 3 * (stride));                                       \
} while (0)

#define STOREUV(p, u, v, stride) do {                                          \
  _mm_storel_epi64((__m128i*)&(u)[(stride)], p);                               \
  (p) = _mm_srli_si128(p, 8);                                                  \
  _mm_storel_epi64((__m128i*)&(v)[(stride)], p);                               \
} while (0)

static WEBP_INLINE void ComplexMask_SSE2(const __m128i* const p1,
                                         const __m128i* const p0,
                                         const __m128i* const q0,
                                         const __m128i* const q1,
                                         int thresh, int ithresh,
                                         __m128i* const mask) {
  const __m128i it = _mm_set1_epi8(ithresh);
  const __m128i diff = _mm_subs_epu8(*mask, it);
  const __m128i thresh_mask = _mm_cmpeq_epi8(diff, _mm_setzero_si128());
  __m128i filter_mask;
  NeedsFilter_SSE2(p1, p0, q0, q1, thresh, &filter_mask);
  *mask = _mm_and_si128(thresh_mask, filter_mask);
}

static void VFilter16_SSE2(uint8_t* p, int stride,
                           int thresh, int ithresh, int hev_thresh) {
  __m128i t1;
  __m128i mask;
  __m128i p2, p1, p0, q0, q1, q2;

  LOAD_H_EDGES4(p - 4 * stride, stride, t1, p2, p1, p0);
  MAX_DIFF1(t1, p2, p1, p0, mask);

  LOAD_H_EDGES4(p, stride, q0, q1, q2, t1);
  MAX_DIFF2(t1, q2, q1, q0, mask);

  ComplexMask_SSE2(&p1, &p0, &q0, &q1, thresh, ithresh, &mask);
  DoFilter6_SSE2(&p2, &p1, &p0, &q0, &q1, &q2, &mask, hev_thresh);

  _mm_storeu_si128((__m128i*)&p[-3 * stride], p2);
  _mm_storeu_si128((__m128i*)&p[-2 * stride], p1);
  _mm_storeu_si128((__m128i*)&p[-1 * stride], p0);
  _mm_storeu_si128((__m128i*)&p[+0 * stride], q0);
  _mm_storeu_si128((__m128i*)&p[+1 * stride], q1);
  _mm_storeu_si128((__m128i*)&p[+2 * stride], q2);
}

static void HFilter16_SSE2(uint8_t* p, int stride,
                           int thresh, int ithresh, int hev_thresh) {
  __m128i mask;
  __m128i p3, p2, p1, p0, q0, q1, q2, q3;

  uint8_t* const b = p - 4;
  Load16x4_SSE2(b, b + 8 * stride, stride, &p3, &p2, &p1, &p0);
  MAX_DIFF1(p3, p2, p1, p0, mask);

  Load16x4_SSE2(p, p + 8 * stride, stride, &q0, &q1, &q2, &q3);
  MAX_DIFF2(q3, q2, q1, q0, mask);

  ComplexMask_SSE2(&p1, &p0, &q0, &q1, thresh, ithresh, &mask);
  DoFilter6_SSE2(&p2, &p1, &p0, &q0, &q1, &q2, &mask, hev_thresh);

  Store16x4_SSE2(&p3, &p2, &p1, &p0, b, b + 8 * stride, stride);
  Store16x4_SSE2(&q0, &q1, &q2, &q3, p, p + 8 * stride, stride);
}

static void VFilter16i_SSE2(uint8_t* p, int stride,
                            int thresh, int ithresh, int hev_thresh) {
  int k;
  __m128i p3, p2, p1, p0;

  LOAD_H_EDGES4(p, stride, p3, p2, p1, p0);

  for (k = 3; k > 0; --k) {
    __m128i mask, tmp1, tmp2;
    uint8_t* const b = p + 2 * stride;
    p += 4 * stride;

    MAX_DIFF1(p3, p2, p1, p0, mask);
    LOAD_H_EDGES4(p, stride, p3, p2, tmp1, tmp2);
    MAX_DIFF2(p3, p2, tmp1, tmp2, mask);

    ComplexMask_SSE2(&p1, &p0, &p3, &p2, thresh, ithresh, &mask);
    DoFilter4_SSE2(&p1, &p0, &p3, &p2, &mask, hev_thresh);

    _mm_storeu_si128((__m128i*)&b[0 * stride], p1);
    _mm_storeu_si128((__m128i*)&b[1 * stride], p0);
    _mm_storeu_si128((__m128i*)&b[2 * stride], p3);
    _mm_storeu_si128((__m128i*)&b[3 * stride], p2);

    p1 = tmp1;
    p0 = tmp2;
  }
}

static void HFilter16i_SSE2(uint8_t* p, int stride,
                            int thresh, int ithresh, int hev_thresh) {
  int k;
  __m128i p3, p2, p1, p0;

  Load16x4_SSE2(p, p + 8 * stride, stride, &p3, &p2, &p1, &p0);

  for (k = 3; k > 0; --k) {
    __m128i mask, tmp1, tmp2;
    uint8_t* const b = p + 2;

    p += 4;

    MAX_DIFF1(p3, p2, p1, p0, mask);
    Load16x4_SSE2(p, p + 8 * stride, stride, &p3, &p2, &tmp1, &tmp2);
    MAX_DIFF2(p3, p2, tmp1, tmp2, mask);

    ComplexMask_SSE2(&p1, &p0, &p3, &p2, thresh, ithresh, &mask);
    DoFilter4_SSE2(&p1, &p0, &p3, &p2, &mask, hev_thresh);

    Store16x4_SSE2(&p1, &p0, &p3, &p2, b, b + 8 * stride, stride);

    p1 = tmp1;
    p0 = tmp2;
  }
}

static void VFilter8_SSE2(uint8_t* WEBP_RESTRICT u, uint8_t* WEBP_RESTRICT v,
                          int stride, int thresh, int ithresh, int hev_thresh) {
  __m128i mask;
  __m128i t1, p2, p1, p0, q0, q1, q2;

  LOADUV_H_EDGES4(u - 4 * stride, v - 4 * stride, stride, t1, p2, p1, p0);
  MAX_DIFF1(t1, p2, p1, p0, mask);

  LOADUV_H_EDGES4(u, v, stride, q0, q1, q2, t1);
  MAX_DIFF2(t1, q2, q1, q0, mask);

  ComplexMask_SSE2(&p1, &p0, &q0, &q1, thresh, ithresh, &mask);
  DoFilter6_SSE2(&p2, &p1, &p0, &q0, &q1, &q2, &mask, hev_thresh);

  STOREUV(p2, u, v, -3 * stride);
  STOREUV(p1, u, v, -2 * stride);
  STOREUV(p0, u, v, -1 * stride);
  STOREUV(q0, u, v, 0 * stride);
  STOREUV(q1, u, v, 1 * stride);
  STOREUV(q2, u, v, 2 * stride);
}

static void HFilter8_SSE2(uint8_t* WEBP_RESTRICT u, uint8_t* WEBP_RESTRICT v,
                          int stride, int thresh, int ithresh, int hev_thresh) {
  __m128i mask;
  __m128i p3, p2, p1, p0, q0, q1, q2, q3;

  uint8_t* const tu = u - 4;
  uint8_t* const tv = v - 4;
  Load16x4_SSE2(tu, tv, stride, &p3, &p2, &p1, &p0);
  MAX_DIFF1(p3, p2, p1, p0, mask);

  Load16x4_SSE2(u, v, stride, &q0, &q1, &q2, &q3);
  MAX_DIFF2(q3, q2, q1, q0, mask);

  ComplexMask_SSE2(&p1, &p0, &q0, &q1, thresh, ithresh, &mask);
  DoFilter6_SSE2(&p2, &p1, &p0, &q0, &q1, &q2, &mask, hev_thresh);

  Store16x4_SSE2(&p3, &p2, &p1, &p0, tu, tv, stride);
  Store16x4_SSE2(&q0, &q1, &q2, &q3, u, v, stride);
}

static void VFilter8i_SSE2(uint8_t* WEBP_RESTRICT u, uint8_t* WEBP_RESTRICT v,
                           int stride,
                           int thresh, int ithresh, int hev_thresh) {
  __m128i mask;
  __m128i t1, t2, p1, p0, q0, q1;

  LOADUV_H_EDGES4(u, v, stride, t2, t1, p1, p0);
  MAX_DIFF1(t2, t1, p1, p0, mask);

  u += 4 * stride;
  v += 4 * stride;

  LOADUV_H_EDGES4(u, v, stride, q0, q1, t1, t2);
  MAX_DIFF2(t2, t1, q1, q0, mask);

  ComplexMask_SSE2(&p1, &p0, &q0, &q1, thresh, ithresh, &mask);
  DoFilter4_SSE2(&p1, &p0, &q0, &q1, &mask, hev_thresh);

  STOREUV(p1, u, v, -2 * stride);
  STOREUV(p0, u, v, -1 * stride);
  STOREUV(q0, u, v, 0 * stride);
  STOREUV(q1, u, v, 1 * stride);
}

static void HFilter8i_SSE2(uint8_t* WEBP_RESTRICT u, uint8_t* WEBP_RESTRICT v,
                           int stride,
                           int thresh, int ithresh, int hev_thresh) {
  __m128i mask;
  __m128i t1, t2, p1, p0, q0, q1;
  Load16x4_SSE2(u, v, stride, &t2, &t1, &p1, &p0);
  MAX_DIFF1(t2, t1, p1, p0, mask);

  u += 4;
  v += 4;
  Load16x4_SSE2(u, v, stride, &q0, &q1, &t1, &t2);
  MAX_DIFF2(t2, t1, q1, q0, mask);

  ComplexMask_SSE2(&p1, &p0, &q0, &q1, thresh, ithresh, &mask);
  DoFilter4_SSE2(&p1, &p0, &q0, &q1, &mask, hev_thresh);

  u -= 2;
  v -= 2;
  Store16x4_SSE2(&p1, &p0, &q0, &q1, u, v, stride);
}

#define DST(x, y) dst[(x) + (y) * BPS]
#define AVG3(a, b, c) (((a) + 2 * (b) + (c) + 2) >> 2)

static void VE4_SSE2(uint8_t* dst) {
  const __m128i one = _mm_set1_epi8(1);
  const __m128i ABCDEFGH = _mm_loadl_epi64((__m128i*)(dst - BPS - 1));
  const __m128i BCDEFGH0 = _mm_srli_si128(ABCDEFGH, 1);
  const __m128i CDEFGH00 = _mm_srli_si128(ABCDEFGH, 2);
  const __m128i a = _mm_avg_epu8(ABCDEFGH, CDEFGH00);
  const __m128i lsb = _mm_and_si128(_mm_xor_si128(ABCDEFGH, CDEFGH00), one);
  const __m128i b = _mm_subs_epu8(a, lsb);
  const __m128i avg = _mm_avg_epu8(b, BCDEFGH0);
  const int vals = _mm_cvtsi128_si32(avg);
  int i;
  for (i = 0; i < 4; ++i) {
    WebPInt32ToMem(dst + i * BPS, vals);
  }
}

static void LD4_SSE2(uint8_t* dst) {
  const __m128i one = _mm_set1_epi8(1);
  const __m128i ABCDEFGH = _mm_loadl_epi64((__m128i*)(dst - BPS));
  const __m128i BCDEFGH0 = _mm_srli_si128(ABCDEFGH, 1);
  const __m128i CDEFGH00 = _mm_srli_si128(ABCDEFGH, 2);
  const __m128i CDEFGHH0 = _mm_insert_epi16(CDEFGH00, dst[-BPS + 7], 3);
  const __m128i avg1 = _mm_avg_epu8(ABCDEFGH, CDEFGHH0);
  const __m128i lsb = _mm_and_si128(_mm_xor_si128(ABCDEFGH, CDEFGHH0), one);
  const __m128i avg2 = _mm_subs_epu8(avg1, lsb);
  const __m128i abcdefg = _mm_avg_epu8(avg2, BCDEFGH0);
  WebPInt32ToMem(dst + 0 * BPS, _mm_cvtsi128_si32(               abcdefg    ));
  WebPInt32ToMem(dst + 1 * BPS, _mm_cvtsi128_si32(_mm_srli_si128(abcdefg, 1)));
  WebPInt32ToMem(dst + 2 * BPS, _mm_cvtsi128_si32(_mm_srli_si128(abcdefg, 2)));
  WebPInt32ToMem(dst + 3 * BPS, _mm_cvtsi128_si32(_mm_srli_si128(abcdefg, 3)));
}

static void VR4_SSE2(uint8_t* dst) {
  const __m128i one = _mm_set1_epi8(1);
  const int I = dst[-1 + 0 * BPS];
  const int J = dst[-1 + 1 * BPS];
  const int K = dst[-1 + 2 * BPS];
  const int X = dst[-1 - BPS];
  const __m128i XABCD = _mm_loadl_epi64((__m128i*)(dst - BPS - 1));
  const __m128i ABCD0 = _mm_srli_si128(XABCD, 1);
  const __m128i abcd = _mm_avg_epu8(XABCD, ABCD0);
  const __m128i _XABCD = _mm_slli_si128(XABCD, 1);
  const __m128i IXABCD = _mm_insert_epi16(_XABCD, (short)(I | (X << 8)), 0);
  const __m128i avg1 = _mm_avg_epu8(IXABCD, ABCD0);
  const __m128i lsb = _mm_and_si128(_mm_xor_si128(IXABCD, ABCD0), one);
  const __m128i avg2 = _mm_subs_epu8(avg1, lsb);
  const __m128i efgh = _mm_avg_epu8(avg2, XABCD);
  WebPInt32ToMem(dst + 0 * BPS, _mm_cvtsi128_si32(               abcd    ));
  WebPInt32ToMem(dst + 1 * BPS, _mm_cvtsi128_si32(               efgh    ));
  WebPInt32ToMem(dst + 2 * BPS, _mm_cvtsi128_si32(_mm_slli_si128(abcd, 1)));
  WebPInt32ToMem(dst + 3 * BPS, _mm_cvtsi128_si32(_mm_slli_si128(efgh, 1)));

  DST(0, 2) = AVG3(J, I, X);
  DST(0, 3) = AVG3(K, J, I);
}

static void VL4_SSE2(uint8_t* dst) {
  const __m128i one = _mm_set1_epi8(1);
  const __m128i ABCDEFGH = _mm_loadl_epi64((__m128i*)(dst - BPS));
  const __m128i BCDEFGH_ = _mm_srli_si128(ABCDEFGH, 1);
  const __m128i CDEFGH__ = _mm_srli_si128(ABCDEFGH, 2);
  const __m128i avg1 = _mm_avg_epu8(ABCDEFGH, BCDEFGH_);
  const __m128i avg2 = _mm_avg_epu8(CDEFGH__, BCDEFGH_);
  const __m128i avg3 = _mm_avg_epu8(avg1, avg2);
  const __m128i lsb1 = _mm_and_si128(_mm_xor_si128(avg1, avg2), one);
  const __m128i ab = _mm_xor_si128(ABCDEFGH, BCDEFGH_);
  const __m128i bc = _mm_xor_si128(CDEFGH__, BCDEFGH_);
  const __m128i abbc = _mm_or_si128(ab, bc);
  const __m128i lsb2 = _mm_and_si128(abbc, lsb1);
  const __m128i avg4 = _mm_subs_epu8(avg3, lsb2);
  const uint32_t extra_out =
      (uint32_t)_mm_cvtsi128_si32(_mm_srli_si128(avg4, 4));
  WebPInt32ToMem(dst + 0 * BPS, _mm_cvtsi128_si32(               avg1    ));
  WebPInt32ToMem(dst + 1 * BPS, _mm_cvtsi128_si32(               avg4    ));
  WebPInt32ToMem(dst + 2 * BPS, _mm_cvtsi128_si32(_mm_srli_si128(avg1, 1)));
  WebPInt32ToMem(dst + 3 * BPS, _mm_cvtsi128_si32(_mm_srli_si128(avg4, 1)));

  DST(3, 2) = (extra_out >> 0) & 0xff;
  DST(3, 3) = (extra_out >> 8) & 0xff;
}

static void RD4_SSE2(uint8_t* dst) {
  const __m128i one = _mm_set1_epi8(1);
  const __m128i XABCD = _mm_loadl_epi64((__m128i*)(dst - BPS - 1));
  const __m128i ____XABCD = _mm_slli_si128(XABCD, 4);
  const uint32_t I = dst[-1 + 0 * BPS];
  const uint32_t J = dst[-1 + 1 * BPS];
  const uint32_t K = dst[-1 + 2 * BPS];
  const uint32_t L = dst[-1 + 3 * BPS];
  const __m128i LKJI_____ =
      _mm_cvtsi32_si128((int)(L | (K << 8) | (J << 16) | (I << 24)));
  const __m128i LKJIXABCD = _mm_or_si128(LKJI_____, ____XABCD);
  const __m128i KJIXABCD_ = _mm_srli_si128(LKJIXABCD, 1);
  const __m128i JIXABCD__ = _mm_srli_si128(LKJIXABCD, 2);
  const __m128i avg1 = _mm_avg_epu8(JIXABCD__, LKJIXABCD);
  const __m128i lsb = _mm_and_si128(_mm_xor_si128(JIXABCD__, LKJIXABCD), one);
  const __m128i avg2 = _mm_subs_epu8(avg1, lsb);
  const __m128i abcdefg = _mm_avg_epu8(avg2, KJIXABCD_);
  WebPInt32ToMem(dst + 3 * BPS, _mm_cvtsi128_si32(               abcdefg    ));
  WebPInt32ToMem(dst + 2 * BPS, _mm_cvtsi128_si32(_mm_srli_si128(abcdefg, 1)));
  WebPInt32ToMem(dst + 1 * BPS, _mm_cvtsi128_si32(_mm_srli_si128(abcdefg, 2)));
  WebPInt32ToMem(dst + 0 * BPS, _mm_cvtsi128_si32(_mm_srli_si128(abcdefg, 3)));
}

#undef DST
#undef AVG3

static WEBP_INLINE void TrueMotion_SSE2(uint8_t* dst, int size) {
  const uint8_t* top = dst - BPS;
  const __m128i zero = _mm_setzero_si128();
  int y;
  if (size == 4) {
    const __m128i top_values = _mm_cvtsi32_si128(WebPMemToInt32(top));
    const __m128i top_base = _mm_unpacklo_epi8(top_values, zero);
    for (y = 0; y < 4; ++y, dst += BPS) {
      const int val = dst[-1] - top[-1];
      const __m128i base = _mm_set1_epi16(val);
      const __m128i out = _mm_packus_epi16(_mm_add_epi16(base, top_base), zero);
      WebPInt32ToMem(dst, _mm_cvtsi128_si32(out));
    }
  } else if (size == 8) {
    const __m128i top_values = _mm_loadl_epi64((const __m128i*)top);
    const __m128i top_base = _mm_unpacklo_epi8(top_values, zero);
    for (y = 0; y < 8; ++y, dst += BPS) {
      const int val = dst[-1] - top[-1];
      const __m128i base = _mm_set1_epi16(val);
      const __m128i out = _mm_packus_epi16(_mm_add_epi16(base, top_base), zero);
      _mm_storel_epi64((__m128i*)dst, out);
    }
  } else {
    const __m128i top_values = _mm_loadu_si128((const __m128i*)top);
    const __m128i top_base_0 = _mm_unpacklo_epi8(top_values, zero);
    const __m128i top_base_1 = _mm_unpackhi_epi8(top_values, zero);
    for (y = 0; y < 16; ++y, dst += BPS) {
      const int val = dst[-1] - top[-1];
      const __m128i base = _mm_set1_epi16(val);
      const __m128i out_0 = _mm_add_epi16(base, top_base_0);
      const __m128i out_1 = _mm_add_epi16(base, top_base_1);
      const __m128i out = _mm_packus_epi16(out_0, out_1);
      _mm_storeu_si128((__m128i*)dst, out);
    }
  }
}

static void TM4_SSE2(uint8_t* dst)   { TrueMotion_SSE2(dst, 4); }
static void TM8uv_SSE2(uint8_t* dst) { TrueMotion_SSE2(dst, 8); }
static void TM16_SSE2(uint8_t* dst)  { TrueMotion_SSE2(dst, 16); }

static void VE16_SSE2(uint8_t* dst) {
  const __m128i top = _mm_loadu_si128((const __m128i*)(dst - BPS));
  int j;
  for (j = 0; j < 16; ++j) {
    _mm_storeu_si128((__m128i*)(dst + j * BPS), top);
  }
}

static void HE16_SSE2(uint8_t* dst) {
  int j;
  for (j = 16; j > 0; --j) {
    const __m128i values = _mm_set1_epi8((char)dst[-1]);
    _mm_storeu_si128((__m128i*)dst, values);
    dst += BPS;
  }
}

static WEBP_INLINE void Put16_SSE2(uint8_t v, uint8_t* dst) {
  int j;
  const __m128i values = _mm_set1_epi8((char)v);
  for (j = 0; j < 16; ++j) {
    _mm_storeu_si128((__m128i*)(dst + j * BPS), values);
  }
}

static void DC16_SSE2(uint8_t* dst) {
  const __m128i zero = _mm_setzero_si128();
  const __m128i top = _mm_loadu_si128((const __m128i*)(dst - BPS));
  const __m128i sad8x2 = _mm_sad_epu8(top, zero);

  const __m128i sum = _mm_add_epi16(sad8x2, _mm_shuffle_epi32(sad8x2, 2));
  int left = 0;
  int j;
  for (j = 0; j < 16; ++j) {
    left += dst[-1 + j * BPS];
  }
  {
    const int DC = _mm_cvtsi128_si32(sum) + left + 16;
    Put16_SSE2(DC >> 5, dst);
  }
}

static void DC16NoTop_SSE2(uint8_t* dst) {
  int DC = 8;
  int j;
  for (j = 0; j < 16; ++j) {
    DC += dst[-1 + j * BPS];
  }
  Put16_SSE2(DC >> 4, dst);
}

static void DC16NoLeft_SSE2(uint8_t* dst) {
  const __m128i zero = _mm_setzero_si128();
  const __m128i top = _mm_loadu_si128((const __m128i*)(dst - BPS));
  const __m128i sad8x2 = _mm_sad_epu8(top, zero);

  const __m128i sum = _mm_add_epi16(sad8x2, _mm_shuffle_epi32(sad8x2, 2));
  const int DC = _mm_cvtsi128_si32(sum) + 8;
  Put16_SSE2(DC >> 4, dst);
}

static void DC16NoTopLeft_SSE2(uint8_t* dst) {
  Put16_SSE2(0x80, dst);
}

static void VE8uv_SSE2(uint8_t* dst) {
  int j;
  const __m128i top = _mm_loadl_epi64((const __m128i*)(dst - BPS));
  for (j = 0; j < 8; ++j) {
    _mm_storel_epi64((__m128i*)(dst + j * BPS), top);
  }
}

static WEBP_INLINE void Put8x8uv_SSE2(uint8_t v, uint8_t* dst) {
  int j;
  const __m128i values = _mm_set1_epi8((char)v);
  for (j = 0; j < 8; ++j) {
    _mm_storel_epi64((__m128i*)(dst + j * BPS), values);
  }
}

static void DC8uv_SSE2(uint8_t* dst) {
  const __m128i zero = _mm_setzero_si128();
  const __m128i top = _mm_loadl_epi64((const __m128i*)(dst - BPS));
  const __m128i sum = _mm_sad_epu8(top, zero);
  int left = 0;
  int j;
  for (j = 0; j < 8; ++j) {
    left += dst[-1 + j * BPS];
  }
  {
    const int DC = _mm_cvtsi128_si32(sum) + left + 8;
    Put8x8uv_SSE2(DC >> 4, dst);
  }
}

static void DC8uvNoLeft_SSE2(uint8_t* dst) {
  const __m128i zero = _mm_setzero_si128();
  const __m128i top = _mm_loadl_epi64((const __m128i*)(dst - BPS));
  const __m128i sum = _mm_sad_epu8(top, zero);
  const int DC = _mm_cvtsi128_si32(sum) + 4;
  Put8x8uv_SSE2(DC >> 3, dst);
}

static void DC8uvNoTop_SSE2(uint8_t* dst) {
  int dc0 = 4;
  int i;
  for (i = 0; i < 8; ++i) {
    dc0 += dst[-1 + i * BPS];
  }
  Put8x8uv_SSE2(dc0 >> 3, dst);
}

static void DC8uvNoTopLeft_SSE2(uint8_t* dst) {
  Put8x8uv_SSE2(0x80, dst);
}

extern void VP8DspInitSSE2(void);

WEBP_TSAN_IGNORE_FUNCTION void VP8DspInitSSE2(void) {
  VP8Transform = Transform_SSE2;
#if (USE_TRANSFORM_AC3 == 1)
  VP8TransformAC3 = TransformAC3_SSE2;
#endif

  VP8VFilter16 = VFilter16_SSE2;
  VP8HFilter16 = HFilter16_SSE2;
  VP8VFilter8 = VFilter8_SSE2;
  VP8HFilter8 = HFilter8_SSE2;
  VP8VFilter16i = VFilter16i_SSE2;
  VP8HFilter16i = HFilter16i_SSE2;
  VP8VFilter8i = VFilter8i_SSE2;
  VP8HFilter8i = HFilter8i_SSE2;

  VP8SimpleVFilter16 = SimpleVFilter16_SSE2;
  VP8SimpleHFilter16 = SimpleHFilter16_SSE2;
  VP8SimpleVFilter16i = SimpleVFilter16i_SSE2;
  VP8SimpleHFilter16i = SimpleHFilter16i_SSE2;

  VP8PredLuma4[1] = TM4_SSE2;
  VP8PredLuma4[2] = VE4_SSE2;
  VP8PredLuma4[4] = RD4_SSE2;
  VP8PredLuma4[5] = VR4_SSE2;
  VP8PredLuma4[6] = LD4_SSE2;
  VP8PredLuma4[7] = VL4_SSE2;

  VP8PredLuma16[0] = DC16_SSE2;
  VP8PredLuma16[1] = TM16_SSE2;
  VP8PredLuma16[2] = VE16_SSE2;
  VP8PredLuma16[3] = HE16_SSE2;
  VP8PredLuma16[4] = DC16NoTop_SSE2;
  VP8PredLuma16[5] = DC16NoLeft_SSE2;
  VP8PredLuma16[6] = DC16NoTopLeft_SSE2;

  VP8PredChroma8[0] = DC8uv_SSE2;
  VP8PredChroma8[1] = TM8uv_SSE2;
  VP8PredChroma8[2] = VE8uv_SSE2;
  VP8PredChroma8[4] = DC8uvNoTop_SSE2;
  VP8PredChroma8[5] = DC8uvNoLeft_SSE2;
  VP8PredChroma8[6] = DC8uvNoTopLeft_SSE2;
}

#else

WEBP_DSP_INIT_STUB(VP8DspInitSSE2)

#endif

#if defined(WEBP_USE_SSE41)
#include <emmintrin.h>
#include <smmintrin.h>

static void HE16_SSE41(uint8_t* dst) {
  int j;
  const __m128i kShuffle3 = _mm_set1_epi8(3);
  for (j = 16; j > 0; --j) {
    const __m128i in = _mm_cvtsi32_si128(WebPMemToInt32(dst - 4));
    const __m128i values = _mm_shuffle_epi8(in, kShuffle3);
    _mm_storeu_si128((__m128i*)dst, values);
    dst += BPS;
  }
}

extern void VP8DspInitSSE41(void);

WEBP_TSAN_IGNORE_FUNCTION void VP8DspInitSSE41(void) {
  VP8PredLuma16[3] = HE16_SSE41;
}

#else

WEBP_DSP_INIT_STUB(VP8DspInitSSE41)

#endif

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define DCHECK(in, out)                                                        \
  do {                                                                         \
    assert((in) != NULL);                                                      \
    assert((out) != NULL);                                                     \
    assert((in) != (out));                                                     \
    assert(width > 0);                                                         \
    assert(height > 0);                                                        \
    assert(stride >= width);                                                   \
  } while (0)

#if !WEBP_NEON_OMIT_C_CODE
static WEBP_INLINE void PredictLine_C(const uint8_t* WEBP_RESTRICT src,
                                      const uint8_t* WEBP_RESTRICT pred,
                                      uint8_t* WEBP_RESTRICT dst, int length) {
  int i;
  for (i = 0; i < length; ++i) dst[i] = (uint8_t)(src[i] - pred[i]);
}

static WEBP_INLINE void DoHorizontalFilter_C(const uint8_t* WEBP_RESTRICT in,
                                             int width, int height, int stride,
                                             uint8_t* WEBP_RESTRICT out) {
  const uint8_t* preds = in;
  int row;
  DCHECK(in, out);

  out[0] = in[0];
  PredictLine_C(in + 1, preds, out + 1, width - 1);
  preds += stride;
  in += stride;
  out += stride;

  for (row = 1; row < height; ++row) {

    PredictLine_C(in, preds - stride, out, 1);
    PredictLine_C(in + 1, preds, out + 1, width - 1);
    preds += stride;
    in += stride;
    out += stride;
  }
}

static WEBP_INLINE void DoVerticalFilter_C(const uint8_t* WEBP_RESTRICT in,
                                           int width, int height, int stride,
                                           uint8_t* WEBP_RESTRICT out) {
  const uint8_t* preds = in;
  int row;
  DCHECK(in, out);

  out[0] = in[0];

  PredictLine_C(in + 1, preds, out + 1, width - 1);
  in += stride;
  out += stride;

  for (row = 1; row < height; ++row) {
    PredictLine_C(in, preds, out, width);
    preds += stride;
    in += stride;
    out += stride;
  }
}
#endif

static WEBP_INLINE int GradientPredictor_C(uint8_t a, uint8_t b, uint8_t c) {
  const int g = a + b - c;
  return ((g & ~0xff) == 0) ? g : (g < 0) ? 0 : 255;
}

#if !WEBP_NEON_OMIT_C_CODE
static WEBP_INLINE void DoGradientFilter_C(const uint8_t* WEBP_RESTRICT in,
                                           int width, int height, int stride,
                                           uint8_t* WEBP_RESTRICT out) {
  const uint8_t* preds = in;
  int row;
  DCHECK(in, out);

  out[0] = in[0];
  PredictLine_C(in + 1, preds, out + 1, width - 1);
  preds += stride;
  in += stride;
  out += stride;

  for (row = 1; row < height; ++row) {
    int w;

    PredictLine_C(in, preds - stride, out, 1);
    for (w = 1; w < width; ++w) {
      const int pred = GradientPredictor_C(preds[w - 1],
                                           preds[w - stride],
                                           preds[w - stride - 1]);
      out[w] = (uint8_t)(in[w] - pred);
    }
    preds += stride;
    in += stride;
    out += stride;
  }
}
#endif

#undef DCHECK

#if !WEBP_NEON_OMIT_C_CODE
static void HorizontalFilter_C(const uint8_t* WEBP_RESTRICT data,
                               int width, int height, int stride,
                               uint8_t* WEBP_RESTRICT filtered_data) {
  DoHorizontalFilter_C(data, width, height, stride, filtered_data);
}

static void VerticalFilter_C(const uint8_t* WEBP_RESTRICT data,
                             int width, int height, int stride,
                             uint8_t* WEBP_RESTRICT filtered_data) {
  DoVerticalFilter_C(data, width, height, stride, filtered_data);
}

static void GradientFilter_C(const uint8_t* WEBP_RESTRICT data,
                             int width, int height, int stride,
                             uint8_t* WEBP_RESTRICT filtered_data) {
  DoGradientFilter_C(data, width, height, stride, filtered_data);
}
#endif

static void NoneUnfilter_C(const uint8_t* prev, const uint8_t* in,
                           uint8_t* out, int width) {
  (void)prev;
  if (out != in) memcpy(out, in, width * sizeof(*out));
}

static void HorizontalUnfilter_C(const uint8_t* prev, const uint8_t* in,
                                 uint8_t* out, int width) {
  uint8_t pred = (prev == NULL) ? 0 : prev[0];
  int i;
  for (i = 0; i < width; ++i) {
    out[i] = (uint8_t)(pred + in[i]);
    pred = out[i];
  }
}

#if !WEBP_NEON_OMIT_C_CODE
static void VerticalUnfilter_C(const uint8_t* prev, const uint8_t* in,
                               uint8_t* out, int width) {
  if (prev == NULL) {
    HorizontalUnfilter_C(NULL, in, out, width);
  } else {
    int i;
    for (i = 0; i < width; ++i) out[i] = (uint8_t)(prev[i] + in[i]);
  }
}
#endif

static void GradientUnfilter_C(const uint8_t* prev, const uint8_t* in,
                               uint8_t* out, int width) {
  if (prev == NULL) {
    HorizontalUnfilter_C(NULL, in, out, width);
  } else {
    uint8_t top = prev[0], top_left = top, left = top;
    int i;
    for (i = 0; i < width; ++i) {
      top = prev[i];
      left = (uint8_t)(in[i] + GradientPredictor_C(left, top, top_left));
      top_left = top;
      out[i] = left;
    }
  }
}

WebPFilterFunc WebPFilters[WEBP_FILTER_LAST];
WebPUnfilterFunc WebPUnfilters[WEBP_FILTER_LAST];

extern VP8CPUInfo VP8GetCPUInfo;
extern void VP8FiltersInitMIPSdspR2(void);
extern void VP8FiltersInitMSA(void);
extern void VP8FiltersInitNEON(void);
extern void VP8FiltersInitSSE2(void);

WEBP_DSP_INIT_FUNC(VP8FiltersInit) {
  WebPUnfilters[WEBP_FILTER_NONE] = NoneUnfilter_C;
#if !WEBP_NEON_OMIT_C_CODE
  WebPUnfilters[WEBP_FILTER_HORIZONTAL] = HorizontalUnfilter_C;
  WebPUnfilters[WEBP_FILTER_VERTICAL] = VerticalUnfilter_C;
#endif
  WebPUnfilters[WEBP_FILTER_GRADIENT] = GradientUnfilter_C;

  WebPFilters[WEBP_FILTER_NONE] = NULL;
#if !WEBP_NEON_OMIT_C_CODE
  WebPFilters[WEBP_FILTER_HORIZONTAL] = HorizontalFilter_C;
  WebPFilters[WEBP_FILTER_VERTICAL] = VerticalFilter_C;
  WebPFilters[WEBP_FILTER_GRADIENT] = GradientFilter_C;
#endif

  if (VP8GetCPUInfo != NULL) {
#if defined(WEBP_HAVE_SSE2)
    if (VP8GetCPUInfo(kSSE2)) {
      VP8FiltersInitSSE2();
    }
#endif
#if defined(WEBP_USE_MIPS_DSP_R2)
    if (VP8GetCPUInfo(kMIPSdspR2)) {
      VP8FiltersInitMIPSdspR2();
    }
#endif
#if defined(WEBP_USE_MSA)
    if (VP8GetCPUInfo(kMSA)) {
      VP8FiltersInitMSA();
    }
#endif
  }

#if defined(WEBP_HAVE_NEON)
  if (WEBP_NEON_OMIT_C_CODE ||
      (VP8GetCPUInfo != NULL && VP8GetCPUInfo(kNEON))) {
    VP8FiltersInitNEON();
  }
#endif

  assert(WebPUnfilters[WEBP_FILTER_NONE] != NULL);
  assert(WebPUnfilters[WEBP_FILTER_HORIZONTAL] != NULL);
  assert(WebPUnfilters[WEBP_FILTER_VERTICAL] != NULL);
  assert(WebPUnfilters[WEBP_FILTER_GRADIENT] != NULL);
  assert(WebPFilters[WEBP_FILTER_HORIZONTAL] != NULL);
  assert(WebPFilters[WEBP_FILTER_VERTICAL] != NULL);
  assert(WebPFilters[WEBP_FILTER_GRADIENT] != NULL);
}

#define GradientPredictor_C filters_neon_GradientPredictor_C
#if defined(WEBP_USE_NEON)

#include <assert.h>

#define DCHECK(in, out)                                                        \
  do {                                                                         \
    assert((in) != NULL);                                                      \
    assert((out) != NULL);                                                     \
    assert((in) != (out));                                                     \
    assert(width > 0);                                                         \
    assert(height > 0);                                                        \
    assert(stride >= width);                                                   \
  } while (0)

#define U8_TO_S16(A) vreinterpretq_s16_u16(vmovl_u8(A))
#define LOAD_U8_TO_S16(A) U8_TO_S16(vld1_u8(A))

#define SHIFT_RIGHT_N_Q(A, N) vextq_u8((A), zero, (N))
#define SHIFT_LEFT_N_Q(A, N) vextq_u8(zero, (A), (16 - (N)) % 16)

#define ROTATE_LEFT_N(A, N)   vext_u8((A), (A), (N))

#define ROTATE_RIGHT_N(A, N)   vext_u8((A), (A), (8 - (N)) % 8)

static void PredictLine_NEON(const uint8_t* src, const uint8_t* pred,
                             uint8_t* WEBP_RESTRICT dst, int length) {
  int i;
  assert(length >= 0);
  for (i = 0; i + 16 <= length; i += 16) {
    const uint8x16_t A = vld1q_u8(&src[i]);
    const uint8x16_t B = vld1q_u8(&pred[i]);
    const uint8x16_t C = vsubq_u8(A, B);
    vst1q_u8(&dst[i], C);
  }
  for (; i < length; ++i) dst[i] = src[i] - pred[i];
}

static void PredictLineLeft_NEON(const uint8_t* WEBP_RESTRICT src,
                                 uint8_t* WEBP_RESTRICT dst, int length) {
  PredictLine_NEON(src, src - 1, dst, length);
}

static WEBP_INLINE void DoHorizontalFilter_NEON(
    const uint8_t* WEBP_RESTRICT in, int width, int height, int stride,
    uint8_t* WEBP_RESTRICT out) {
  int row;
  DCHECK(in, out);

  out[0] = in[0];
  PredictLineLeft_NEON(in + 1, out + 1, width - 1);
  in += stride;
  out += stride;

  for (row = 1; row < height; ++row) {

    out[0] = in[0] - in[-stride];
    PredictLineLeft_NEON(in + 1, out + 1, width - 1);
    in += stride;
    out += stride;
  }
}

static void HorizontalFilter_NEON(const uint8_t* WEBP_RESTRICT data,
                                  int width, int height, int stride,
                                  uint8_t* WEBP_RESTRICT filtered_data) {
  DoHorizontalFilter_NEON(data, width, height, stride, filtered_data);
}

static WEBP_INLINE void DoVerticalFilter_NEON(const uint8_t* WEBP_RESTRICT in,
                                              int width, int height, int stride,
                                              uint8_t* WEBP_RESTRICT out) {
  int row;
  DCHECK(in, out);

  out[0] = in[0];

  PredictLineLeft_NEON(in + 1, out + 1, width - 1);
  in += stride;
  out += stride;

  for (row = 1; row < height; ++row) {
    PredictLine_NEON(in, in - stride, out, width);
    in += stride;
    out += stride;
  }
}

static void VerticalFilter_NEON(const uint8_t* WEBP_RESTRICT data,
                                int width, int height, int stride,
                                uint8_t* WEBP_RESTRICT filtered_data) {
  DoVerticalFilter_NEON(data, width, height, stride, filtered_data);
}

static WEBP_INLINE int GradientPredictor_C(uint8_t a, uint8_t b, uint8_t c) {
  const int g = a + b - c;
  return ((g & ~0xff) == 0) ? g : (g < 0) ? 0 : 255;
}

static void GradientPredictDirect_NEON(const uint8_t* const row,
                                       const uint8_t* const top,
                                       uint8_t* WEBP_RESTRICT const out,
                                       int length) {
  int i;
  for (i = 0; i + 8 <= length; i += 8) {
    const uint8x8_t A = vld1_u8(&row[i - 1]);
    const uint8x8_t B = vld1_u8(&top[i + 0]);
    const int16x8_t C = vreinterpretq_s16_u16(vaddl_u8(A, B));
    const int16x8_t D = LOAD_U8_TO_S16(&top[i - 1]);
    const uint8x8_t E = vqmovun_s16(vsubq_s16(C, D));
    const uint8x8_t F = vld1_u8(&row[i + 0]);
    vst1_u8(&out[i], vsub_u8(F, E));
  }
  for (; i < length; ++i) {
    out[i] = row[i] - GradientPredictor_C(row[i - 1], top[i], top[i - 1]);
  }
}

static WEBP_INLINE void DoGradientFilter_NEON(const uint8_t* WEBP_RESTRICT in,
                                              int width, int height, int stride,
                                              uint8_t* WEBP_RESTRICT out) {
  int row;
  DCHECK(in, out);

  out[0] = in[0];
  PredictLineLeft_NEON(in + 1, out + 1, width - 1);
  in += stride;
  out += stride;

  for (row = 1; row < height; ++row) {
    out[0] = in[0] - in[-stride];
    GradientPredictDirect_NEON(in + 1, in + 1 - stride, out + 1, width - 1);
    in += stride;
    out += stride;
  }
}

static void GradientFilter_NEON(const uint8_t* WEBP_RESTRICT data,
                                int width, int height, int stride,
                                uint8_t* WEBP_RESTRICT filtered_data) {
  DoGradientFilter_NEON(data, width, height, stride, filtered_data);
}

#undef DCHECK

static void HorizontalUnfilter_NEON(const uint8_t* prev, const uint8_t* in,
                                    uint8_t* out, int width) {
  int i;
  const uint8x16_t zero = vdupq_n_u8(0);
  uint8x16_t last;
  out[0] = in[0] + (prev == NULL ? 0 : prev[0]);
  if (width <= 1) return;
  last = vsetq_lane_u8(out[0], zero, 0);
  for (i = 1; i + 16 <= width; i += 16) {
    const uint8x16_t A0 = vld1q_u8(&in[i]);
    const uint8x16_t A1 = vaddq_u8(A0, last);
    const uint8x16_t A2 = SHIFT_LEFT_N_Q(A1, 1);
    const uint8x16_t A3 = vaddq_u8(A1, A2);
    const uint8x16_t A4 = SHIFT_LEFT_N_Q(A3, 2);
    const uint8x16_t A5 = vaddq_u8(A3, A4);
    const uint8x16_t A6 = SHIFT_LEFT_N_Q(A5, 4);
    const uint8x16_t A7 = vaddq_u8(A5, A6);
    const uint8x16_t A8 = SHIFT_LEFT_N_Q(A7, 8);
    const uint8x16_t A9 = vaddq_u8(A7, A8);
    vst1q_u8(&out[i], A9);
    last = SHIFT_RIGHT_N_Q(A9, 15);
  }
  for (; i < width; ++i) out[i] = in[i] + out[i - 1];
}

static void VerticalUnfilter_NEON(const uint8_t* prev, const uint8_t* in,
                                  uint8_t* out, int width) {
  if (prev == NULL) {
    HorizontalUnfilter_NEON(NULL, in, out, width);
  } else {
    int i;
    assert(width >= 0);
    for (i = 0; i + 16 <= width; i += 16) {
      const uint8x16_t A = vld1q_u8(&in[i]);
      const uint8x16_t B = vld1q_u8(&prev[i]);
      const uint8x16_t C = vaddq_u8(A, B);
      vst1q_u8(&out[i], C);
    }
    for (; i < width; ++i) out[i] = in[i] + prev[i];
  }
}

#if !defined(USE_GRADIENT_UNFILTER)
#define USE_GRADIENT_UNFILTER 0
#endif

#if (USE_GRADIENT_UNFILTER == 1)
#define GRAD_PROCESS_LANE(L)  do {                                             \
  const uint8x8_t tmp1 = ROTATE_RIGHT_N(pred, 1);     \
  const int16x8_t tmp2 = vaddq_s16(BC, U8_TO_S16(tmp1));                       \
  const uint8x8_t delta = vqmovun_s16(tmp2);                                   \
  pred = vadd_u8(D, delta);                                                    \
  out = vext_u8(out, ROTATE_LEFT_N(pred, (L)), 1);                             \
} while (0)

static void GradientPredictInverse_NEON(const uint8_t* const in,
                                        const uint8_t* const top,
                                        uint8_t* const row, int length) {
  if (length > 0) {
    int i;
    uint8x8_t pred = vdup_n_u8(row[-1]);
    uint8x8_t out = vdup_n_u8(0);
    for (i = 0; i + 8 <= length; i += 8) {
      const int16x8_t B = LOAD_U8_TO_S16(&top[i + 0]);
      const int16x8_t C = LOAD_U8_TO_S16(&top[i - 1]);
      const int16x8_t BC = vsubq_s16(B, C);
      const uint8x8_t D = vld1_u8(&in[i]);
      GRAD_PROCESS_LANE(0);
      GRAD_PROCESS_LANE(1);
      GRAD_PROCESS_LANE(2);
      GRAD_PROCESS_LANE(3);
      GRAD_PROCESS_LANE(4);
      GRAD_PROCESS_LANE(5);
      GRAD_PROCESS_LANE(6);
      GRAD_PROCESS_LANE(7);
      vst1_u8(&row[i], out);
    }
    for (; i < length; ++i) {
      row[i] = in[i] + GradientPredictor_C(row[i - 1], top[i], top[i - 1]);
    }
  }
}
#undef GRAD_PROCESS_LANE

static void GradientUnfilter_NEON(const uint8_t* prev, const uint8_t* in,
                                  uint8_t* out, int width) {
  if (prev == NULL) {
    HorizontalUnfilter_NEON(NULL, in, out, width);
  } else {
    out[0] = in[0] + prev[0];
    GradientPredictInverse_NEON(in + 1, prev + 1, out + 1, width - 1);
  }
}

#endif

extern void VP8FiltersInitNEON(void);

WEBP_TSAN_IGNORE_FUNCTION void VP8FiltersInitNEON(void) {
  WebPUnfilters[WEBP_FILTER_HORIZONTAL] = HorizontalUnfilter_NEON;
  WebPUnfilters[WEBP_FILTER_VERTICAL] = VerticalUnfilter_NEON;
#if (USE_GRADIENT_UNFILTER == 1)
  WebPUnfilters[WEBP_FILTER_GRADIENT] = GradientUnfilter_NEON;
#endif

  WebPFilters[WEBP_FILTER_HORIZONTAL] = HorizontalFilter_NEON;
  WebPFilters[WEBP_FILTER_VERTICAL] = VerticalFilter_NEON;
  WebPFilters[WEBP_FILTER_GRADIENT] = GradientFilter_NEON;
}

#else

WEBP_DSP_INIT_STUB(VP8FiltersInitNEON)

#endif

#undef GradientPredictor_C

#if defined(WEBP_USE_SSE2)

#include <assert.h>
#include <emmintrin.h>
#include <stdlib.h>
#include <string.h>

#define DCHECK(in, out)                                                        \
  do {                                                                         \
    assert((in) != NULL);                                                      \
    assert((out) != NULL);                                                     \
    assert((in) != (out));                                                     \
    assert(width > 0);                                                         \
    assert(height > 0);                                                        \
    assert(stride >= width);                                                   \
  } while (0)

static void PredictLineTop_SSE2(const uint8_t* WEBP_RESTRICT src,
                                const uint8_t* WEBP_RESTRICT pred,
                                uint8_t* WEBP_RESTRICT dst, int length) {
  int i;
  const int max_pos = length & ~31;
  assert(length >= 0);
  for (i = 0; i < max_pos; i += 32) {
    const __m128i A0 = _mm_loadu_si128((const __m128i*)&src[i +  0]);
    const __m128i A1 = _mm_loadu_si128((const __m128i*)&src[i + 16]);
    const __m128i B0 = _mm_loadu_si128((const __m128i*)&pred[i +  0]);
    const __m128i B1 = _mm_loadu_si128((const __m128i*)&pred[i + 16]);
    const __m128i C0 = _mm_sub_epi8(A0, B0);
    const __m128i C1 = _mm_sub_epi8(A1, B1);
    _mm_storeu_si128((__m128i*)&dst[i +  0], C0);
    _mm_storeu_si128((__m128i*)&dst[i + 16], C1);
  }
  for (; i < length; ++i) dst[i] = src[i] - pred[i];
}

static void PredictLineLeft_SSE2(const uint8_t* WEBP_RESTRICT src,
                                 uint8_t* WEBP_RESTRICT dst, int length) {
  int i;
  const int max_pos = length & ~31;
  assert(length >= 0);
  for (i = 0; i < max_pos; i += 32) {
    const __m128i A0 = _mm_loadu_si128((const __m128i*)(src + i +  0    ));
    const __m128i B0 = _mm_loadu_si128((const __m128i*)(src + i +  0 - 1));
    const __m128i A1 = _mm_loadu_si128((const __m128i*)(src + i + 16    ));
    const __m128i B1 = _mm_loadu_si128((const __m128i*)(src + i + 16 - 1));
    const __m128i C0 = _mm_sub_epi8(A0, B0);
    const __m128i C1 = _mm_sub_epi8(A1, B1);
    _mm_storeu_si128((__m128i*)(dst + i +  0), C0);
    _mm_storeu_si128((__m128i*)(dst + i + 16), C1);
  }
  for (; i < length; ++i) dst[i] = src[i] - src[i - 1];
}

static WEBP_INLINE void DoHorizontalFilter_SSE2(
    const uint8_t* WEBP_RESTRICT in, int width, int height, int stride,
    uint8_t* WEBP_RESTRICT out) {
  int row;
  DCHECK(in, out);

  out[0] = in[0];
  PredictLineLeft_SSE2(in + 1, out + 1, width - 1);
  in += stride;
  out += stride;

  for (row = 1; row < height; ++row) {

    out[0] = in[0] - in[-stride];
    PredictLineLeft_SSE2(in + 1, out + 1, width - 1);
    in += stride;
    out += stride;
  }
}

static WEBP_INLINE void DoVerticalFilter_SSE2(const uint8_t* WEBP_RESTRICT in,
                                              int width, int height, int stride,
                                              uint8_t* WEBP_RESTRICT out) {
  int row;
  DCHECK(in, out);

  out[0] = in[0];

  PredictLineLeft_SSE2(in + 1, out + 1, width - 1);
  in += stride;
  out += stride;

  for (row = 1; row < height; ++row) {
    PredictLineTop_SSE2(in, in - stride, out, width);
    in += stride;
    out += stride;
  }
}

static WEBP_INLINE int GradientPredictor_SSE2(uint8_t a, uint8_t b, uint8_t c) {
  const int g = a + b - c;
  return ((g & ~0xff) == 0) ? g : (g < 0) ? 0 : 255;
}

static void GradientPredictDirect_SSE2(const uint8_t* const row,
                                       const uint8_t* const top,
                                       uint8_t* WEBP_RESTRICT const out,
                                       int length) {
  const int max_pos = length & ~7;
  int i;
  const __m128i zero = _mm_setzero_si128();
  for (i = 0; i < max_pos; i += 8) {
    const __m128i A0 = _mm_loadl_epi64((const __m128i*)&row[i - 1]);
    const __m128i B0 = _mm_loadl_epi64((const __m128i*)&top[i]);
    const __m128i C0 = _mm_loadl_epi64((const __m128i*)&top[i - 1]);
    const __m128i D = _mm_loadl_epi64((const __m128i*)&row[i]);
    const __m128i A1 = _mm_unpacklo_epi8(A0, zero);
    const __m128i B1 = _mm_unpacklo_epi8(B0, zero);
    const __m128i C1 = _mm_unpacklo_epi8(C0, zero);
    const __m128i E = _mm_add_epi16(A1, B1);
    const __m128i F = _mm_sub_epi16(E, C1);
    const __m128i G = _mm_packus_epi16(F, zero);
    const __m128i H = _mm_sub_epi8(D, G);
    _mm_storel_epi64((__m128i*)(out + i), H);
  }
  for (; i < length; ++i) {
    const int delta = GradientPredictor_SSE2(row[i - 1], top[i], top[i - 1]);
    out[i] = (uint8_t)(row[i] - delta);
  }
}

static WEBP_INLINE void DoGradientFilter_SSE2(const uint8_t* WEBP_RESTRICT in,
                                              int width, int height, int stride,
                                              uint8_t* WEBP_RESTRICT out) {
  int row;
  DCHECK(in, out);

  out[0] = in[0];
  PredictLineLeft_SSE2(in + 1, out + 1, width - 1);
  in += stride;
  out += stride;

  for (row = 1; row < height; ++row) {
    out[0] = (uint8_t)(in[0] - in[-stride]);
    GradientPredictDirect_SSE2(in + 1, in + 1 - stride, out + 1, width - 1);
    in += stride;
    out += stride;
  }
}

#undef DCHECK

static void HorizontalFilter_SSE2(const uint8_t* WEBP_RESTRICT data,
                                  int width, int height, int stride,
                                  uint8_t* WEBP_RESTRICT filtered_data) {
  DoHorizontalFilter_SSE2(data, width, height, stride, filtered_data);
}

static void VerticalFilter_SSE2(const uint8_t* WEBP_RESTRICT data,
                                int width, int height, int stride,
                                uint8_t* WEBP_RESTRICT filtered_data) {
  DoVerticalFilter_SSE2(data, width, height, stride, filtered_data);
}

static void GradientFilter_SSE2(const uint8_t* WEBP_RESTRICT data,
                                int width, int height, int stride,
                                uint8_t* WEBP_RESTRICT filtered_data) {
  DoGradientFilter_SSE2(data, width, height, stride, filtered_data);
}

static void HorizontalUnfilter_SSE2(const uint8_t* prev, const uint8_t* in,
                                    uint8_t* out, int width) {
  int i;
  __m128i last;
  out[0] = (uint8_t)(in[0] + (prev == NULL ? 0 : prev[0]));
  if (width <= 1) return;
  last = _mm_set_epi32(0, 0, 0, out[0]);
  for (i = 1; i + 8 <= width; i += 8) {
    const __m128i A0 = _mm_loadl_epi64((const __m128i*)(in + i));
    const __m128i A1 = _mm_add_epi8(A0, last);
    const __m128i A2 = _mm_slli_si128(A1, 1);
    const __m128i A3 = _mm_add_epi8(A1, A2);
    const __m128i A4 = _mm_slli_si128(A3, 2);
    const __m128i A5 = _mm_add_epi8(A3, A4);
    const __m128i A6 = _mm_slli_si128(A5, 4);
    const __m128i A7 = _mm_add_epi8(A5, A6);
    _mm_storel_epi64((__m128i*)(out + i), A7);
    last = _mm_srli_epi64(A7, 56);
  }
  for (; i < width; ++i) out[i] = (uint8_t)(in[i] + out[i - 1]);
}

static void VerticalUnfilter_SSE2(const uint8_t* prev, const uint8_t* in,
                                  uint8_t* out, int width) {
  if (prev == NULL) {
    HorizontalUnfilter_SSE2(NULL, in, out, width);
  } else {
    int i;
    const int max_pos = width & ~31;
    assert(width >= 0);
    for (i = 0; i < max_pos; i += 32) {
      const __m128i A0 = _mm_loadu_si128((const __m128i*)&in[i +  0]);
      const __m128i A1 = _mm_loadu_si128((const __m128i*)&in[i + 16]);
      const __m128i B0 = _mm_loadu_si128((const __m128i*)&prev[i +  0]);
      const __m128i B1 = _mm_loadu_si128((const __m128i*)&prev[i + 16]);
      const __m128i C0 = _mm_add_epi8(A0, B0);
      const __m128i C1 = _mm_add_epi8(A1, B1);
      _mm_storeu_si128((__m128i*)&out[i +  0], C0);
      _mm_storeu_si128((__m128i*)&out[i + 16], C1);
    }
    for (; i < width; ++i) out[i] = (uint8_t)(in[i] + prev[i]);
  }
}

static void GradientPredictInverse_SSE2(const uint8_t* const in,
                                        const uint8_t* const top,
                                        uint8_t* const row, int length) {
  if (length > 0) {
    int i;
    const int max_pos = length & ~7;
    const __m128i zero = _mm_setzero_si128();
    __m128i A = _mm_set_epi32(0, 0, 0, row[-1]);
    for (i = 0; i < max_pos; i += 8) {
      const __m128i tmp0 = _mm_loadl_epi64((const __m128i*)&top[i]);
      const __m128i tmp1 = _mm_loadl_epi64((const __m128i*)&top[i - 1]);
      const __m128i B = _mm_unpacklo_epi8(tmp0, zero);
      const __m128i C = _mm_unpacklo_epi8(tmp1, zero);
      const __m128i D = _mm_loadl_epi64((const __m128i*)&in[i]);
      const __m128i E = _mm_sub_epi16(B, C);
      __m128i out = zero;
      __m128i mask_hi = _mm_set_epi32(0, 0, 0, 0xff);
      int k = 8;
      while (1) {
        const __m128i tmp3 = _mm_add_epi16(A, E);
        const __m128i tmp4 = _mm_packus_epi16(tmp3, zero);
        const __m128i tmp5 = _mm_add_epi8(tmp4, D);
        A = _mm_and_si128(tmp5, mask_hi);
        out = _mm_or_si128(out, A);
        if (--k == 0) break;
        A = _mm_slli_si128(A, 1);
        mask_hi = _mm_slli_si128(mask_hi, 1);
        A = _mm_unpacklo_epi8(A, zero);
      }
      A = _mm_srli_si128(A, 7);
      _mm_storel_epi64((__m128i*)&row[i], out);
    }
    for (; i < length; ++i) {
      const int delta = GradientPredictor_SSE2(row[i - 1], top[i], top[i - 1]);
      row[i] = (uint8_t)(in[i] + delta);
    }
  }
}

static void GradientUnfilter_SSE2(const uint8_t* prev, const uint8_t* in,
                                  uint8_t* out, int width) {
  if (prev == NULL) {
    HorizontalUnfilter_SSE2(NULL, in, out, width);
  } else {
    out[0] = (uint8_t)(in[0] + prev[0]);
    GradientPredictInverse_SSE2(in + 1, prev + 1, out + 1, width - 1);
  }
}

extern void VP8FiltersInitSSE2(void);

WEBP_TSAN_IGNORE_FUNCTION void VP8FiltersInitSSE2(void) {
  WebPUnfilters[WEBP_FILTER_HORIZONTAL] = HorizontalUnfilter_SSE2;
#if defined(CHROMIUM)

  (void)VerticalUnfilter_SSE2;
#else
  WebPUnfilters[WEBP_FILTER_VERTICAL] = VerticalUnfilter_SSE2;
#endif
  WebPUnfilters[WEBP_FILTER_GRADIENT] = GradientUnfilter_SSE2;

  WebPFilters[WEBP_FILTER_HORIZONTAL] = HorizontalFilter_SSE2;
  WebPFilters[WEBP_FILTER_VERTICAL] = VerticalFilter_SSE2;
  WebPFilters[WEBP_FILTER_GRADIENT] = GradientFilter_SSE2;
}

#else

WEBP_DSP_INIT_STUB(VP8FiltersInitSSE2)

#endif

#include <assert.h>
#include <stdlib.h>
#include <string.h>

static WEBP_INLINE uint32_t Average2(uint32_t a0, uint32_t a1) {
  return (((a0 ^ a1) & 0xfefefefeu) >> 1) + (a0 & a1);
}

static WEBP_INLINE uint32_t Average3(uint32_t a0, uint32_t a1, uint32_t a2) {
  return Average2(Average2(a0, a2), a1);
}

static WEBP_INLINE uint32_t Average4(uint32_t a0, uint32_t a1,
                                     uint32_t a2, uint32_t a3) {
  return Average2(Average2(a0, a1), Average2(a2, a3));
}

static WEBP_INLINE uint32_t Clip255(uint32_t a) {
  if (a < 256) {
    return a;
  }

  return ~a >> 24;
}

static WEBP_INLINE int AddSubtractComponentFull(int a, int b, int c) {
  return Clip255((uint32_t)(a + b - c));
}

static WEBP_INLINE uint32_t ClampedAddSubtractFull(uint32_t c0, uint32_t c1,
                                                   uint32_t c2) {
  const int a = AddSubtractComponentFull(c0 >> 24, c1 >> 24, c2 >> 24);
  const int r = AddSubtractComponentFull((c0 >> 16) & 0xff,
                                         (c1 >> 16) & 0xff,
                                         (c2 >> 16) & 0xff);
  const int g = AddSubtractComponentFull((c0 >> 8) & 0xff,
                                         (c1 >> 8) & 0xff,
                                         (c2 >> 8) & 0xff);
  const int b = AddSubtractComponentFull(c0 & 0xff, c1 & 0xff, c2 & 0xff);
  return ((uint32_t)a << 24) | (r << 16) | (g << 8) | b;
}

static WEBP_INLINE int AddSubtractComponentHalf(int a, int b) {
  return Clip255((uint32_t)(a + (a - b) / 2));
}

static WEBP_INLINE uint32_t ClampedAddSubtractHalf(uint32_t c0, uint32_t c1,
                                                   uint32_t c2) {
  const uint32_t ave = Average2(c0, c1);
  const int a = AddSubtractComponentHalf(ave >> 24, c2 >> 24);
  const int r = AddSubtractComponentHalf((ave >> 16) & 0xff, (c2 >> 16) & 0xff);
  const int g = AddSubtractComponentHalf((ave >> 8) & 0xff, (c2 >> 8) & 0xff);
  const int b = AddSubtractComponentHalf((ave >> 0) & 0xff, (c2 >> 0) & 0xff);
  return ((uint32_t)a << 24) | (r << 16) | (g << 8) | b;
}

#if defined(__arm__) && defined(__GNUC__) && LOCAL_GCC_VERSION <= 0x409
# define LOCAL_INLINE __attribute__ ((noinline))
#else
# define LOCAL_INLINE WEBP_INLINE
#endif

static LOCAL_INLINE int Sub3(int a, int b, int c) {
  const int pb = b - c;
  const int pa = a - c;
  return abs(pb) - abs(pa);
}

#undef LOCAL_INLINE

static WEBP_INLINE uint32_t Select(uint32_t a, uint32_t b, uint32_t c) {
  const int pa_minus_pb =
      Sub3((a >> 24)       , (b >> 24)       , (c >> 24)       ) +
      Sub3((a >> 16) & 0xff, (b >> 16) & 0xff, (c >> 16) & 0xff) +
      Sub3((a >>  8) & 0xff, (b >>  8) & 0xff, (c >>  8) & 0xff) +
      Sub3((a      ) & 0xff, (b      ) & 0xff, (c      ) & 0xff);
  return (pa_minus_pb <= 0) ? a : b;
}

static uint32_t VP8LPredictor0_C(const uint32_t* const left,
                                 const uint32_t* const top) {
  (void)top;
  (void)left;
  return ARGB_BLACK;
}
static uint32_t VP8LPredictor1_C(const uint32_t* const left,
                                 const uint32_t* const top) {
  (void)top;
  return *left;
}
uint32_t VP8LPredictor2_C(const uint32_t* const left,
                          const uint32_t* const top) {
  (void)left;
  return top[0];
}
uint32_t VP8LPredictor3_C(const uint32_t* const left,
                          const uint32_t* const top) {
  (void)left;
  return top[1];
}
uint32_t VP8LPredictor4_C(const uint32_t* const left,
                          const uint32_t* const top) {
  (void)left;
  return top[-1];
}
uint32_t VP8LPredictor5_C(const uint32_t* const left,
                          const uint32_t* const top) {
  const uint32_t pred = Average3(*left, top[0], top[1]);
  return pred;
}
uint32_t VP8LPredictor6_C(const uint32_t* const left,
                          const uint32_t* const top) {
  const uint32_t pred = Average2(*left, top[-1]);
  return pred;
}
uint32_t VP8LPredictor7_C(const uint32_t* const left,
                          const uint32_t* const top) {
  const uint32_t pred = Average2(*left, top[0]);
  return pred;
}
uint32_t VP8LPredictor8_C(const uint32_t* const left,
                          const uint32_t* const top) {
  const uint32_t pred = Average2(top[-1], top[0]);
  (void)left;
  return pred;
}
uint32_t VP8LPredictor9_C(const uint32_t* const left,
                          const uint32_t* const top) {
  const uint32_t pred = Average2(top[0], top[1]);
  (void)left;
  return pred;
}
uint32_t VP8LPredictor10_C(const uint32_t* const left,
                           const uint32_t* const top) {
  const uint32_t pred = Average4(*left, top[-1], top[0], top[1]);
  return pred;
}
uint32_t VP8LPredictor11_C(const uint32_t* const left,
                           const uint32_t* const top) {
  const uint32_t pred = Select(top[0], *left, top[-1]);
  return pred;
}
uint32_t VP8LPredictor12_C(const uint32_t* const left,
                           const uint32_t* const top) {
  const uint32_t pred = ClampedAddSubtractFull(*left, top[0], top[-1]);
  return pred;
}
uint32_t VP8LPredictor13_C(const uint32_t* const left,
                           const uint32_t* const top) {
  const uint32_t pred = ClampedAddSubtractHalf(*left, top[0], top[-1]);
  return pred;
}

static void PredictorAdd0_C(const uint32_t* in, const uint32_t* upper,
                            int num_pixels, uint32_t* WEBP_RESTRICT out) {
  int x;
  (void)upper;
  for (x = 0; x < num_pixels; ++x) out[x] = VP8LAddPixels(in[x], ARGB_BLACK);
}
static void PredictorAdd1_C(const uint32_t* in, const uint32_t* upper,
                            int num_pixels, uint32_t* WEBP_RESTRICT out) {
  int i;
  uint32_t left = out[-1];
  (void)upper;
  for (i = 0; i < num_pixels; ++i) {
    out[i] = left = VP8LAddPixels(in[i], left);
  }
}
GENERATE_PREDICTOR_ADD(VP8LPredictor2_C, PredictorAdd2_C)
GENERATE_PREDICTOR_ADD(VP8LPredictor3_C, PredictorAdd3_C)
GENERATE_PREDICTOR_ADD(VP8LPredictor4_C, PredictorAdd4_C)
GENERATE_PREDICTOR_ADD(VP8LPredictor5_C, PredictorAdd5_C)
GENERATE_PREDICTOR_ADD(VP8LPredictor6_C, PredictorAdd6_C)
GENERATE_PREDICTOR_ADD(VP8LPredictor7_C, PredictorAdd7_C)
GENERATE_PREDICTOR_ADD(VP8LPredictor8_C, PredictorAdd8_C)
GENERATE_PREDICTOR_ADD(VP8LPredictor9_C, PredictorAdd9_C)
GENERATE_PREDICTOR_ADD(VP8LPredictor10_C, PredictorAdd10_C)
GENERATE_PREDICTOR_ADD(VP8LPredictor11_C, PredictorAdd11_C)
GENERATE_PREDICTOR_ADD(VP8LPredictor12_C, PredictorAdd12_C)
GENERATE_PREDICTOR_ADD(VP8LPredictor13_C, PredictorAdd13_C)

static void PredictorInverseTransform_C(const VP8LTransform* const transform,
                                        int y_start, int y_end,
                                        const uint32_t* in, uint32_t* out) {
  const int width = transform->xsize;
  if (y_start == 0) {
    PredictorAdd0_C(in, NULL, 1, out);
    PredictorAdd1_C(in + 1, NULL, width - 1, out + 1);
    in += width;
    out += width;
    ++y_start;
  }

  {
    int y = y_start;
    const int tile_width = 1 << transform->bits;
    const int mask = tile_width - 1;
    const int tiles_per_row = VP8LSubSampleSize(width, transform->bits);
    const uint32_t* pred_mode_base =
        transform->data + (y >> transform->bits) * tiles_per_row;

    while (y < y_end) {
      const uint32_t* pred_mode_src = pred_mode_base;
      int x = 1;

      PredictorAdd2_C(in, out - width, 1, out);

      while (x < width) {
        const VP8LPredictorAddSubFunc pred_func =
            VP8LPredictorsAdd[((*pred_mode_src++) >> 8) & 0xf];
        int x_end = (x & ~mask) + tile_width;
        if (x_end > width) x_end = width;
        pred_func(in + x, out + x - width, x_end - x, out + x);
        x = x_end;
      }
      in += width;
      out += width;
      ++y;
      if ((y & mask) == 0) {
        pred_mode_base += tiles_per_row;
      }
    }
  }
}

void VP8LAddGreenToBlueAndRed_C(const uint32_t* src, int num_pixels,
                                uint32_t* dst) {
  int i;
  for (i = 0; i < num_pixels; ++i) {
    const uint32_t argb = src[i];
    const uint32_t green = ((argb >> 8) & 0xff);
    uint32_t red_blue = (argb & 0x00ff00ffu);
    red_blue += (green << 16) | green;
    red_blue &= 0x00ff00ffu;
    dst[i] = (argb & 0xff00ff00u) | red_blue;
  }
}

static WEBP_INLINE int ColorTransformDelta(int8_t color_pred,
                                           int8_t color) {
  return ((int)color_pred * color) >> 5;
}

static WEBP_INLINE void ColorCodeToMultipliers(uint32_t color_code,
                                               VP8LMultipliers* const m) {
  m->green_to_red  = (color_code >>  0) & 0xff;
  m->green_to_blue = (color_code >>  8) & 0xff;
  m->red_to_blue   = (color_code >> 16) & 0xff;
}

void VP8LTransformColorInverse_C(const VP8LMultipliers* const m,
                                 const uint32_t* src, int num_pixels,
                                 uint32_t* dst) {
  int i;
  for (i = 0; i < num_pixels; ++i) {
    const uint32_t argb = src[i];
    const int8_t green = (int8_t)(argb >> 8);
    const uint32_t red = argb >> 16;
    int new_red = red & 0xff;
    int new_blue = argb & 0xff;
    new_red += ColorTransformDelta((int8_t)m->green_to_red, green);
    new_red &= 0xff;
    new_blue += ColorTransformDelta((int8_t)m->green_to_blue, green);
    new_blue += ColorTransformDelta((int8_t)m->red_to_blue, (int8_t)new_red);
    new_blue &= 0xff;
    dst[i] = (argb & 0xff00ff00u) | (new_red << 16) | (new_blue);
  }
}

static void ColorSpaceInverseTransform_C(const VP8LTransform* const transform,
                                         int y_start, int y_end,
                                         const uint32_t* src, uint32_t* dst) {
  const int width = transform->xsize;
  const int tile_width = 1 << transform->bits;
  const int mask = tile_width - 1;
  const int safe_width = width & ~mask;
  const int remaining_width = width - safe_width;
  const int tiles_per_row = VP8LSubSampleSize(width, transform->bits);
  int y = y_start;
  const uint32_t* pred_row =
      transform->data + (y >> transform->bits) * tiles_per_row;

  while (y < y_end) {
    const uint32_t* pred = pred_row;
    VP8LMultipliers m = { 0, 0, 0 };
    const uint32_t* const src_safe_end = src + safe_width;
    const uint32_t* const src_end = src + width;
    while (src < src_safe_end) {
      ColorCodeToMultipliers(*pred++, &m);
      VP8LTransformColorInverse(&m, src, tile_width, dst);
      src += tile_width;
      dst += tile_width;
    }
    if (src < src_end) {
      ColorCodeToMultipliers(*pred++, &m);
      VP8LTransformColorInverse(&m, src, remaining_width, dst);
      src += remaining_width;
      dst += remaining_width;
    }
    ++y;
    if ((y & mask) == 0) pred_row += tiles_per_row;
  }
}

#define COLOR_INDEX_INVERSE(FUNC_NAME, F_NAME, STATIC_DECL, TYPE, BIT_SUFFIX,  \
                            GET_INDEX, GET_VALUE)                              \
static void F_NAME(const TYPE* src, const uint32_t* const color_map,           \
                   TYPE* dst, int y_start, int y_end, int width) {             \
  int y;                                                                       \
  for (y = y_start; y < y_end; ++y) {                                          \
    int x;                                                                     \
    for (x = 0; x < width; ++x) {                                              \
      *dst++ = GET_VALUE(color_map[GET_INDEX(*src++)]);                        \
    }                                                                          \
  }                                                                            \
}                                                                              \
STATIC_DECL void FUNC_NAME(const VP8LTransform* const transform,               \
                           int y_start, int y_end, const TYPE* src,            \
                           TYPE* dst) {                                        \
  int y;                                                                       \
  const int bits_per_pixel = 8 >> transform->bits;                             \
  const int width = transform->xsize;                                          \
  const uint32_t* const color_map = transform->data;                           \
  if (bits_per_pixel < 8) {                                                    \
    const int pixels_per_byte = 1 << transform->bits;                          \
    const int count_mask = pixels_per_byte - 1;                                \
    const uint32_t bit_mask = (1 << bits_per_pixel) - 1;                       \
    for (y = y_start; y < y_end; ++y) {                                        \
      uint32_t packed_pixels = 0;                                              \
      int x;                                                                   \
      for (x = 0; x < width; ++x) {                                            \
          \
          \
          \
          \
        if ((x & count_mask) == 0) packed_pixels = GET_INDEX(*src++);          \
        *dst++ = GET_VALUE(color_map[packed_pixels & bit_mask]);               \
        packed_pixels >>= bits_per_pixel;                                      \
      }                                                                        \
    }                                                                          \
  } else {                                                                     \
    VP8LMapColor##BIT_SUFFIX(src, color_map, dst, y_start, y_end, width);      \
  }                                                                            \
}

COLOR_INDEX_INVERSE(ColorIndexInverseTransform_C, MapARGB_C, static,
                    uint32_t, 32b, VP8GetARGBIndex, VP8GetARGBValue)
COLOR_INDEX_INVERSE(VP8LColorIndexInverseTransformAlpha, MapAlpha_C, ,
                    uint8_t, 8b, VP8GetAlphaIndex, VP8GetAlphaValue)

#undef COLOR_INDEX_INVERSE

void VP8LInverseTransform(const VP8LTransform* const transform,
                          int row_start, int row_end,
                          const uint32_t* const in, uint32_t* const out) {
  const int width = transform->xsize;
  assert(row_start < row_end);
  assert(row_end <= transform->ysize);
  switch (transform->type) {
    case SUBTRACT_GREEN_TRANSFORM:
      VP8LAddGreenToBlueAndRed(in, (row_end - row_start) * width, out);
      break;
    case PREDICTOR_TRANSFORM:
      PredictorInverseTransform_C(transform, row_start, row_end, in, out);
      if (row_end != transform->ysize) {

        memcpy(out - width, out + (row_end - row_start - 1) * width,
               width * sizeof(*out));
      }
      break;
    case CROSS_COLOR_TRANSFORM:
      ColorSpaceInverseTransform_C(transform, row_start, row_end, in, out);
      break;
    case COLOR_INDEXING_TRANSFORM:
      if (in == out && transform->bits > 0) {

        const int out_stride = (row_end - row_start) * width;
        const int in_stride = (row_end - row_start) *
            VP8LSubSampleSize(transform->xsize, transform->bits);
        uint32_t* const src = out + out_stride - in_stride;
        memmove(src, out, in_stride * sizeof(*src));
        ColorIndexInverseTransform_C(transform, row_start, row_end, src, out);
      } else {
        ColorIndexInverseTransform_C(transform, row_start, row_end, in, out);
      }
      break;
  }
}

static int is_big_endian(void) {
  static const union {
    uint16_t w;
    uint8_t b[2];
  } tmp = { 1 };
  return (tmp.b[0] != 1);
}

void VP8LConvertBGRAToRGB_C(const uint32_t* WEBP_RESTRICT src,
                            int num_pixels, uint8_t* WEBP_RESTRICT dst) {
  const uint32_t* const src_end = src + num_pixels;
  while (src < src_end) {
    const uint32_t argb = *src++;
    *dst++ = (argb >> 16) & 0xff;
    *dst++ = (argb >>  8) & 0xff;
    *dst++ = (argb >>  0) & 0xff;
  }
}

void VP8LConvertBGRAToRGBA_C(const uint32_t* WEBP_RESTRICT src,
                             int num_pixels, uint8_t* WEBP_RESTRICT dst) {
  const uint32_t* const src_end = src + num_pixels;
  while (src < src_end) {
    const uint32_t argb = *src++;
    *dst++ = (argb >> 16) & 0xff;
    *dst++ = (argb >>  8) & 0xff;
    *dst++ = (argb >>  0) & 0xff;
    *dst++ = (argb >> 24) & 0xff;
  }
}

void VP8LConvertBGRAToRGBA4444_C(const uint32_t* WEBP_RESTRICT src,
                                 int num_pixels, uint8_t* WEBP_RESTRICT dst) {
  const uint32_t* const src_end = src + num_pixels;
  while (src < src_end) {
    const uint32_t argb = *src++;
    const uint8_t rg = ((argb >> 16) & 0xf0) | ((argb >> 12) & 0xf);
    const uint8_t ba = ((argb >>  0) & 0xf0) | ((argb >> 28) & 0xf);
#if (WEBP_SWAP_16BIT_CSP == 1)
    *dst++ = ba;
    *dst++ = rg;
#else
    *dst++ = rg;
    *dst++ = ba;
#endif
  }
}

void VP8LConvertBGRAToRGB565_C(const uint32_t* WEBP_RESTRICT src,
                               int num_pixels, uint8_t* WEBP_RESTRICT dst) {
  const uint32_t* const src_end = src + num_pixels;
  while (src < src_end) {
    const uint32_t argb = *src++;
    const uint8_t rg = ((argb >> 16) & 0xf8) | ((argb >> 13) & 0x7);
    const uint8_t gb = ((argb >>  5) & 0xe0) | ((argb >>  3) & 0x1f);
#if (WEBP_SWAP_16BIT_CSP == 1)
    *dst++ = gb;
    *dst++ = rg;
#else
    *dst++ = rg;
    *dst++ = gb;
#endif
  }
}

void VP8LConvertBGRAToBGR_C(const uint32_t* WEBP_RESTRICT src,
                            int num_pixels, uint8_t* WEBP_RESTRICT dst) {
  const uint32_t* const src_end = src + num_pixels;
  while (src < src_end) {
    const uint32_t argb = *src++;
    *dst++ = (argb >>  0) & 0xff;
    *dst++ = (argb >>  8) & 0xff;
    *dst++ = (argb >> 16) & 0xff;
  }
}

static void CopyOrSwap(const uint32_t* WEBP_RESTRICT src, int num_pixels,
                       uint8_t* WEBP_RESTRICT dst, int swap_on_big_endian) {
  if (is_big_endian() == swap_on_big_endian) {
    const uint32_t* const src_end = src + num_pixels;
    while (src < src_end) {
      const uint32_t argb = *src++;
      WebPUint32ToMem(dst, BSwap32(argb));
      dst += sizeof(argb);
    }
  } else {
    memcpy(dst, src, num_pixels * sizeof(*src));
  }
}

void VP8LConvertFromBGRA(const uint32_t* const in_data, int num_pixels,
                         WEBP_CSP_MODE out_colorspace, uint8_t* const rgba) {
  switch (out_colorspace) {
    case MODE_RGB:
      VP8LConvertBGRAToRGB(in_data, num_pixels, rgba);
      break;
    case MODE_RGBA:
      VP8LConvertBGRAToRGBA(in_data, num_pixels, rgba);
      break;
    case MODE_rgbA:
      VP8LConvertBGRAToRGBA(in_data, num_pixels, rgba);
      WebPApplyAlphaMultiply(rgba, 0, num_pixels, 1, 0);
      break;
    case MODE_BGR:
      VP8LConvertBGRAToBGR(in_data, num_pixels, rgba);
      break;
    case MODE_BGRA:
      CopyOrSwap(in_data, num_pixels, rgba, 1);
      break;
    case MODE_bgrA:
      CopyOrSwap(in_data, num_pixels, rgba, 1);
      WebPApplyAlphaMultiply(rgba, 0, num_pixels, 1, 0);
      break;
    case MODE_ARGB:
      CopyOrSwap(in_data, num_pixels, rgba, 0);
      break;
    case MODE_Argb:
      CopyOrSwap(in_data, num_pixels, rgba, 0);
      WebPApplyAlphaMultiply(rgba, 1, num_pixels, 1, 0);
      break;
    case MODE_RGBA_4444:
      VP8LConvertBGRAToRGBA4444(in_data, num_pixels, rgba);
      break;
    case MODE_rgbA_4444:
      VP8LConvertBGRAToRGBA4444(in_data, num_pixels, rgba);
      WebPApplyAlphaMultiply4444(rgba, num_pixels, 1, 0);
      break;
    case MODE_RGB_565:
      VP8LConvertBGRAToRGB565(in_data, num_pixels, rgba);
      break;
    default:
      assert(0);
  }
}

VP8LProcessDecBlueAndRedFunc VP8LAddGreenToBlueAndRed;
VP8LProcessDecBlueAndRedFunc VP8LAddGreenToBlueAndRed_SSE;
VP8LPredictorAddSubFunc VP8LPredictorsAdd[16];
VP8LPredictorAddSubFunc VP8LPredictorsAdd_SSE[16];
VP8LPredictorFunc VP8LPredictors[16];

VP8LPredictorAddSubFunc VP8LPredictorsAdd_C[16];

VP8LTransformColorInverseFunc VP8LTransformColorInverse;
VP8LTransformColorInverseFunc VP8LTransformColorInverse_SSE;

VP8LConvertFunc VP8LConvertBGRAToRGB;
VP8LConvertFunc VP8LConvertBGRAToRGB_SSE;
VP8LConvertFunc VP8LConvertBGRAToRGBA;
VP8LConvertFunc VP8LConvertBGRAToRGBA_SSE;
VP8LConvertFunc VP8LConvertBGRAToRGBA4444;
VP8LConvertFunc VP8LConvertBGRAToRGB565;
VP8LConvertFunc VP8LConvertBGRAToBGR;

VP8LMapARGBFunc VP8LMapColor32b;
VP8LMapAlphaFunc VP8LMapColor8b;

extern VP8CPUInfo VP8GetCPUInfo;
extern void VP8LDspInitSSE2(void);
extern void VP8LDspInitSSE41(void);
extern void VP8LDspInitAVX2(void);
extern void VP8LDspInitNEON(void);
extern void VP8LDspInitMIPSdspR2(void);
extern void VP8LDspInitMSA(void);

#define COPY_PREDICTOR_ARRAY(IN, OUT) do {                \
  (OUT)[0] = IN##0_C;                                     \
  (OUT)[1] = IN##1_C;                                     \
  (OUT)[2] = IN##2_C;                                     \
  (OUT)[3] = IN##3_C;                                     \
  (OUT)[4] = IN##4_C;                                     \
  (OUT)[5] = IN##5_C;                                     \
  (OUT)[6] = IN##6_C;                                     \
  (OUT)[7] = IN##7_C;                                     \
  (OUT)[8] = IN##8_C;                                     \
  (OUT)[9] = IN##9_C;                                     \
  (OUT)[10] = IN##10_C;                                   \
  (OUT)[11] = IN##11_C;                                   \
  (OUT)[12] = IN##12_C;                                   \
  (OUT)[13] = IN##13_C;                                   \
  (OUT)[14] = IN##0_C;  \
  (OUT)[15] = IN##0_C;                                    \
} while (0);

WEBP_DSP_INIT_FUNC(VP8LDspInit) {
  COPY_PREDICTOR_ARRAY(VP8LPredictor, VP8LPredictors)
  COPY_PREDICTOR_ARRAY(PredictorAdd, VP8LPredictorsAdd)
  COPY_PREDICTOR_ARRAY(PredictorAdd, VP8LPredictorsAdd_C)

#if !WEBP_NEON_OMIT_C_CODE
  VP8LAddGreenToBlueAndRed = VP8LAddGreenToBlueAndRed_C;

  VP8LTransformColorInverse = VP8LTransformColorInverse_C;

  VP8LConvertBGRAToRGBA = VP8LConvertBGRAToRGBA_C;
  VP8LConvertBGRAToRGB = VP8LConvertBGRAToRGB_C;
  VP8LConvertBGRAToBGR = VP8LConvertBGRAToBGR_C;
#endif

  VP8LConvertBGRAToRGBA4444 = VP8LConvertBGRAToRGBA4444_C;
  VP8LConvertBGRAToRGB565 = VP8LConvertBGRAToRGB565_C;

  VP8LMapColor32b = MapARGB_C;
  VP8LMapColor8b = MapAlpha_C;

  if (VP8GetCPUInfo != NULL) {
#if defined(WEBP_HAVE_SSE2)
    if (VP8GetCPUInfo(kSSE2)) {
      VP8LDspInitSSE2();
#if defined(WEBP_HAVE_SSE41)
      if (VP8GetCPUInfo(kSSE4_1)) {
        VP8LDspInitSSE41();
#if defined(WEBP_HAVE_AVX2)
        if (VP8GetCPUInfo(kAVX2)) {
          VP8LDspInitAVX2();
        }
#endif
      }
#endif
    }
#endif
#if defined(WEBP_USE_MIPS_DSP_R2)
    if (VP8GetCPUInfo(kMIPSdspR2)) {
      VP8LDspInitMIPSdspR2();
    }
#endif
#if defined(WEBP_USE_MSA)
    if (VP8GetCPUInfo(kMSA)) {
      VP8LDspInitMSA();
    }
#endif
  }

#if defined(WEBP_HAVE_NEON)
  if (WEBP_NEON_OMIT_C_CODE ||
      (VP8GetCPUInfo != NULL && VP8GetCPUInfo(kNEON))) {
    VP8LDspInitNEON();
  }
#endif

  assert(VP8LAddGreenToBlueAndRed != NULL);
  assert(VP8LTransformColorInverse != NULL);
  assert(VP8LConvertBGRAToRGBA != NULL);
  assert(VP8LConvertBGRAToRGB != NULL);
  assert(VP8LConvertBGRAToBGR != NULL);
  assert(VP8LConvertBGRAToRGBA4444 != NULL);
  assert(VP8LConvertBGRAToRGB565 != NULL);
  assert(VP8LMapColor32b != NULL);
  assert(VP8LMapColor8b != NULL);
}
#undef COPY_PREDICTOR_ARRAY

#if defined(WEBP_USE_AVX2)

#include <stddef.h>
#include <immintrin.h>

static WEBP_INLINE void Average2_m256i(const __m256i* const a0,
                                       const __m256i* const a1,
                                       __m256i* const avg) {

  const __m256i ones = _mm256_set1_epi8(1);
  const __m256i avg1 = _mm256_avg_epu8(*a0, *a1);
  const __m256i one = _mm256_and_si256(_mm256_xor_si256(*a0, *a1), ones);
  *avg = _mm256_sub_epi8(avg1, one);
}

static void PredictorAdd0_AVX2(const uint32_t* in, const uint32_t* upper,
                               int num_pixels, uint32_t* WEBP_RESTRICT out) {
  int i;
  const __m256i black = _mm256_set1_epi32((int)ARGB_BLACK);
  for (i = 0; i + 8 <= num_pixels; i += 8) {
    const __m256i src = _mm256_loadu_si256((const __m256i*)&in[i]);
    const __m256i res = _mm256_add_epi8(src, black);
    _mm256_storeu_si256((__m256i*)&out[i], res);
  }
  if (i != num_pixels) {
    VP8LPredictorsAdd_SSE[0](in + i, NULL, num_pixels - i, out + i);
  }
  (void)upper;
}

static void PredictorAdd1_AVX2(const uint32_t* in, const uint32_t* upper,
                               int num_pixels, uint32_t* WEBP_RESTRICT out) {
  int i;
  __m256i prev = _mm256_set1_epi32((int)out[-1]);
  for (i = 0; i + 8 <= num_pixels; i += 8) {

    const __m256i src = _mm256_loadu_si256((const __m256i*)&in[i]);

    const __m256i shift0 = _mm256_slli_si256(src, 4);

    const __m256i sum0 = _mm256_add_epi8(src, shift0);

    const __m256i shift1 = _mm256_slli_si256(sum0, 8);

    const __m256i sum1 = _mm256_add_epi8(sum0, shift1);

    const int32_t sum_abcd = _mm256_extract_epi32(sum1, 3);
    const __m256i sum2 = _mm256_add_epi8(
        sum1,
        _mm256_set_epi32(sum_abcd, sum_abcd, sum_abcd, sum_abcd, 0, 0, 0, 0));

    const __m256i res = _mm256_add_epi8(sum2, prev);
    _mm256_storeu_si256((__m256i*)&out[i], res);

    prev = _mm256_permutevar8x32_epi32(
        res, _mm256_set_epi32(7, 7, 7, 7, 7, 7, 7, 7));
  }
  if (i != num_pixels) {
    VP8LPredictorsAdd_SSE[1](in + i, upper + i, num_pixels - i, out + i);
  }
}

#define GENERATE_PREDICTOR_1(X, IN)                                         \
  static void PredictorAdd##X##_AVX2(const uint32_t* in,                    \
                                     const uint32_t* upper, int num_pixels, \
                                     uint32_t* WEBP_RESTRICT out) {         \
    int i;                                                                  \
    for (i = 0; i + 8 <= num_pixels; i += 8) {                              \
      const __m256i src = _mm256_loadu_si256((const __m256i*)&in[i]);       \
      const __m256i other = _mm256_loadu_si256((const __m256i*)&(IN));      \
      const __m256i res = _mm256_add_epi8(src, other);                      \
      _mm256_storeu_si256((__m256i*)&out[i], res);                          \
    }                                                                       \
    if (i != num_pixels) {                                                  \
      VP8LPredictorsAdd_SSE[(X)](in + i, upper + i, num_pixels - i, out + i); \
    }                                                                       \
  }

GENERATE_PREDICTOR_1(2, upper[i])

GENERATE_PREDICTOR_1(3, upper[i + 1])

GENERATE_PREDICTOR_1(4, upper[i - 1])
#undef GENERATE_PREDICTOR_1

#define GENERATE_PREDICTOR_2(X, IN)                                         \
  static void PredictorAdd##X##_AVX2(const uint32_t* in,                    \
                                     const uint32_t* upper, int num_pixels, \
                                     uint32_t* WEBP_RESTRICT out) {         \
    int i;                                                                  \
    for (i = 0; i + 8 <= num_pixels; i += 8) {                              \
      const __m256i Tother = _mm256_loadu_si256((const __m256i*)&(IN));     \
      const __m256i T = _mm256_loadu_si256((const __m256i*)&upper[i]);      \
      const __m256i src = _mm256_loadu_si256((const __m256i*)&in[i]);       \
      __m256i avg, res;                                                     \
      Average2_m256i(&T, &Tother, &avg);                                    \
      res = _mm256_add_epi8(avg, src);                                      \
      _mm256_storeu_si256((__m256i*)&out[i], res);                          \
    }                                                                       \
    if (i != num_pixels) {                                                  \
      VP8LPredictorsAdd_SSE[(X)](in + i, upper + i, num_pixels - i, out + i); \
    }                                                                       \
  }

GENERATE_PREDICTOR_2(8, upper[i - 1])

GENERATE_PREDICTOR_2(9, upper[i + 1])
#undef GENERATE_PREDICTOR_2

#define DO_PRED10(OUT)                                  \
  do {                                                  \
    __m256i avgLTL, avg;                                \
    Average2_m256i(&L, &TL, &avgLTL);                   \
    Average2_m256i(&avgTTR, &avgLTL, &avg);             \
    L = _mm256_add_epi8(avg, src);                      \
    out[i + (OUT)] = (uint32_t)_mm256_cvtsi256_si32(L); \
  } while (0)

#define DO_PRED10_SHIFT                                         \
  do {                                                          \
     \
    avgTTR = _mm256_srli_si256(avgTTR, 4);                      \
    TL = _mm256_srli_si256(TL, 4);                              \
    src = _mm256_srli_si256(src, 4);                            \
  } while (0)

static void PredictorAdd10_AVX2(const uint32_t* in, const uint32_t* upper,
                                int num_pixels, uint32_t* WEBP_RESTRICT out) {
  int i, j;
  __m256i L = _mm256_setr_epi32((int)out[-1], 0, 0, 0, 0, 0, 0, 0);
  for (i = 0; i + 8 <= num_pixels; i += 8) {
    __m256i src = _mm256_loadu_si256((const __m256i*)&in[i]);
    __m256i TL = _mm256_loadu_si256((const __m256i*)&upper[i - 1]);
    const __m256i T = _mm256_loadu_si256((const __m256i*)&upper[i]);
    const __m256i TR = _mm256_loadu_si256((const __m256i*)&upper[i + 1]);
    __m256i avgTTR;
    Average2_m256i(&T, &TR, &avgTTR);
    {
      const __m256i avgTTR_bak = avgTTR;
      const __m256i TL_bak = TL;
      const __m256i src_bak = src;
      for (j = 0; j < 4; ++j) {
        DO_PRED10(j);
        DO_PRED10_SHIFT;
      }
      avgTTR = _mm256_permute2x128_si256(avgTTR_bak, avgTTR_bak, 1);
      TL = _mm256_permute2x128_si256(TL_bak, TL_bak, 1);
      src = _mm256_permute2x128_si256(src_bak, src_bak, 1);
      for (; j < 8; ++j) {
        DO_PRED10(j);
        DO_PRED10_SHIFT;
      }
    }
  }
  if (i != num_pixels) {
    VP8LPredictorsAdd_SSE[10](in + i, upper + i, num_pixels - i, out + i);
  }
}
#undef DO_PRED10
#undef DO_PRED10_SHIFT

#define DO_PRED11(OUT)                                                      \
  do {                                                                      \
    const __m256i L_lo = _mm256_unpacklo_epi32(L, T);                       \
    const __m256i TL_lo = _mm256_unpacklo_epi32(TL, T);                     \
    const __m256i pb = _mm256_sad_epu8(L_lo, TL_lo);    \
    const __m256i mask = _mm256_cmpgt_epi32(pb, pa);                        \
    const __m256i A = _mm256_and_si256(mask, L);                            \
    const __m256i B = _mm256_andnot_si256(mask, T);                         \
    const __m256i pred = _mm256_or_si256(A, B);  \
    L = _mm256_add_epi8(src, pred);                                         \
    out[i + (OUT)] = (uint32_t)_mm256_cvtsi256_si32(L);                     \
  } while (0)

#define DO_PRED11_SHIFT                                       \
  do {                                                        \
     \
    T = _mm256_srli_si256(T, 4);                              \
    TL = _mm256_srli_si256(TL, 4);                            \
    src = _mm256_srli_si256(src, 4);                          \
    pa = _mm256_srli_si256(pa, 4);                            \
  } while (0)

static void PredictorAdd11_AVX2(const uint32_t* in, const uint32_t* upper,
                                int num_pixels, uint32_t* WEBP_RESTRICT out) {
  int i, j;
  __m256i pa;
  __m256i L = _mm256_setr_epi32((int)out[-1], 0, 0, 0, 0, 0, 0, 0);
  for (i = 0; i + 8 <= num_pixels; i += 8) {
    __m256i T = _mm256_loadu_si256((const __m256i*)&upper[i]);
    __m256i TL = _mm256_loadu_si256((const __m256i*)&upper[i - 1]);
    __m256i src = _mm256_loadu_si256((const __m256i*)&in[i]);
    {

      const __m256i T_lo = _mm256_unpacklo_epi32(T, T);
      const __m256i TL_lo = _mm256_unpacklo_epi32(TL, T);
      const __m256i T_hi = _mm256_unpackhi_epi32(T, T);
      const __m256i TL_hi = _mm256_unpackhi_epi32(TL, T);
      const __m256i s_lo = _mm256_sad_epu8(T_lo, TL_lo);
      const __m256i s_hi = _mm256_sad_epu8(T_hi, TL_hi);
      pa = _mm256_packs_epi32(s_lo, s_hi);
    }
    {
      const __m256i T_bak = T;
      const __m256i TL_bak = TL;
      const __m256i src_bak = src;
      const __m256i pa_bak = pa;
      for (j = 0; j < 4; ++j) {
        DO_PRED11(j);
        DO_PRED11_SHIFT;
      }
      T = _mm256_permute2x128_si256(T_bak, T_bak, 1);
      TL = _mm256_permute2x128_si256(TL_bak, TL_bak, 1);
      src = _mm256_permute2x128_si256(src_bak, src_bak, 1);
      pa = _mm256_permute2x128_si256(pa_bak, pa_bak, 1);
      for (; j < 8; ++j) {
        DO_PRED11(j);
        DO_PRED11_SHIFT;
      }
    }
  }
  if (i != num_pixels) {
    VP8LPredictorsAdd_SSE[11](in + i, upper + i, num_pixels - i, out + i);
  }
}
#undef DO_PRED11
#undef DO_PRED11_SHIFT

#define DO_PRED12(DIFF, OUT)                              \
  do {                                                    \
    const __m256i all = _mm256_add_epi16(L, (DIFF));      \
    const __m256i alls = _mm256_packus_epi16(all, all);   \
    const __m256i res = _mm256_add_epi8(src, alls);       \
    out[i + (OUT)] = (uint32_t)_mm256_cvtsi256_si32(res); \
    L = _mm256_unpacklo_epi8(res, zero);                  \
  } while (0)

#define DO_PRED12_SHIFT(DIFF, LANE)                           \
  do {                                                        \
     \
    if ((LANE) == 0) (DIFF) = _mm256_srli_si256(DIFF, 8);     \
    src = _mm256_srli_si256(src, 4);                          \
  } while (0)

static void PredictorAdd12_AVX2(const uint32_t* in, const uint32_t* upper,
                                int num_pixels, uint32_t* WEBP_RESTRICT out) {
  int i;
  const __m256i zero = _mm256_setzero_si256();
  const __m256i L8 = _mm256_setr_epi32((int)out[-1], 0, 0, 0, 0, 0, 0, 0);
  __m256i L = _mm256_unpacklo_epi8(L8, zero);
  for (i = 0; i + 8 <= num_pixels; i += 8) {

    __m256i src = _mm256_loadu_si256((const __m256i*)&in[i]);
    const __m256i T = _mm256_loadu_si256((const __m256i*)&upper[i]);
    const __m256i T_lo = _mm256_unpacklo_epi8(T, zero);
    const __m256i T_hi = _mm256_unpackhi_epi8(T, zero);
    const __m256i TL = _mm256_loadu_si256((const __m256i*)&upper[i - 1]);
    const __m256i TL_lo = _mm256_unpacklo_epi8(TL, zero);
    const __m256i TL_hi = _mm256_unpackhi_epi8(TL, zero);
    __m256i diff_lo = _mm256_sub_epi16(T_lo, TL_lo);
    __m256i diff_hi = _mm256_sub_epi16(T_hi, TL_hi);
    const __m256i diff_lo_bak = diff_lo;
    const __m256i diff_hi_bak = diff_hi;
    const __m256i src_bak = src;
    DO_PRED12(diff_lo, 0);
    DO_PRED12_SHIFT(diff_lo, 0);
    DO_PRED12(diff_lo, 1);
    DO_PRED12_SHIFT(diff_lo, 0);
    DO_PRED12(diff_hi, 2);
    DO_PRED12_SHIFT(diff_hi, 0);
    DO_PRED12(diff_hi, 3);
    DO_PRED12_SHIFT(diff_hi, 0);

    diff_lo = _mm256_permute2x128_si256(diff_lo_bak, diff_lo_bak, 1);
    diff_hi = _mm256_permute2x128_si256(diff_hi_bak, diff_hi_bak, 1);
    src = _mm256_permute2x128_si256(src_bak, src_bak, 1);

    DO_PRED12(diff_lo, 4);
    DO_PRED12_SHIFT(diff_lo, 0);
    DO_PRED12(diff_lo, 5);
    DO_PRED12_SHIFT(diff_lo, 1);
    DO_PRED12(diff_hi, 6);
    DO_PRED12_SHIFT(diff_hi, 0);
    DO_PRED12(diff_hi, 7);
  }
  if (i != num_pixels) {
    VP8LPredictorsAdd_SSE[12](in + i, upper + i, num_pixels - i, out + i);
  }
}
#undef DO_PRED12
#undef DO_PRED12_SHIFT

static void AddGreenToBlueAndRed_AVX2(const uint32_t* const src, int num_pixels,
                                      uint32_t* dst) {
  int i;
  const __m256i kCstShuffle = _mm256_set_epi8(
      -1, 29, -1, 29, -1, 25, -1, 25, -1, 21, -1, 21, -1, 17, -1, 17, -1, 13,
      -1, 13, -1, 9, -1, 9, -1, 5, -1, 5, -1, 1, -1, 1);
  for (i = 0; i + 8 <= num_pixels; i += 8) {
    const __m256i in = _mm256_loadu_si256((const __m256i*)&src[i]);
    const __m256i in_0g0g = _mm256_shuffle_epi8(in, kCstShuffle);
    const __m256i out = _mm256_add_epi8(in, in_0g0g);
    _mm256_storeu_si256((__m256i*)&dst[i], out);
  }

  if (i != num_pixels) {
    VP8LAddGreenToBlueAndRed_SSE(src + i, num_pixels - i, dst + i);
  }
}

static void TransformColorInverse_AVX2(const VP8LMultipliers* const m,
                                       const uint32_t* const src,
                                       int num_pixels, uint32_t* dst) {

#define CST(X)  (((int16_t)(m->X << 8)) >> 5)
  const __m256i mults_rb =
      _mm256_set1_epi32((int)((uint32_t)CST(green_to_red) << 16 |
                              (CST(green_to_blue) & 0xffff)));
  const __m256i mults_b2 = _mm256_set1_epi32(CST(red_to_blue));
#undef CST
  const __m256i mask_ag = _mm256_set1_epi32((int)0xff00ff00);
  const __m256i perm1 = _mm256_setr_epi8(
      -1, 1, -1, 1, -1, 5, -1, 5, -1, 9, -1, 9, -1, 13, -1, 13, -1, 17, -1, 17,
      -1, 21, -1, 21, -1, 25, -1, 25, -1, 29, -1, 29);
  const __m256i perm2 = _mm256_setr_epi8(
      -1, 2, -1, -1, -1, 6, -1, -1, -1, 10, -1, -1, -1, 14, -1, -1, -1, 18, -1,
      -1, -1, 22, -1, -1, -1, 26, -1, -1, -1, 30, -1, -1);
  int i;
  for (i = 0; i + 8 <= num_pixels; i += 8) {
    const __m256i A = _mm256_loadu_si256((const __m256i*)(src + i));
    const __m256i B = _mm256_shuffle_epi8(A, perm1);
    const __m256i C = _mm256_mulhi_epi16(B, mults_rb);
    const __m256i D = _mm256_add_epi8(A, C);
    const __m256i E = _mm256_shuffle_epi8(D, perm2);
    const __m256i F = _mm256_mulhi_epi16(E, mults_b2);
    const __m256i G = _mm256_add_epi8(D, F);
    const __m256i out = _mm256_blendv_epi8(G, A, mask_ag);
    _mm256_storeu_si256((__m256i*)&dst[i], out);
  }

  if (i != num_pixels) {
    VP8LTransformColorInverse_SSE(m, src + i, num_pixels - i, dst + i);
  }
}

static void ConvertBGRAToRGBA_AVX2(const uint32_t* WEBP_RESTRICT src,
                                   int num_pixels, uint8_t* WEBP_RESTRICT dst) {
  const __m256i* in = (const __m256i*)src;
  __m256i* out = (__m256i*)dst;
  while (num_pixels >= 8) {
    const __m256i A = _mm256_loadu_si256(in++);
    const __m256i B = _mm256_shuffle_epi8(
        A,
        _mm256_set_epi8(15, 12, 13, 14, 11, 8, 9, 10, 7, 4, 5, 6, 3, 0, 1, 2,
                        15, 12, 13, 14, 11, 8, 9, 10, 7, 4, 5, 6, 3, 0, 1, 2));
    _mm256_storeu_si256(out++, B);
    num_pixels -= 8;
  }

  if (num_pixels > 0) {
    VP8LConvertBGRAToRGBA_SSE((const uint32_t*)in, num_pixels, (uint8_t*)out);
  }
}

extern void VP8LDspInitAVX2(void);

WEBP_TSAN_IGNORE_FUNCTION void VP8LDspInitAVX2(void) {
  VP8LPredictorsAdd[0] = PredictorAdd0_AVX2;
  VP8LPredictorsAdd[1] = PredictorAdd1_AVX2;
  VP8LPredictorsAdd[2] = PredictorAdd2_AVX2;
  VP8LPredictorsAdd[3] = PredictorAdd3_AVX2;
  VP8LPredictorsAdd[4] = PredictorAdd4_AVX2;
  VP8LPredictorsAdd[8] = PredictorAdd8_AVX2;
  VP8LPredictorsAdd[9] = PredictorAdd9_AVX2;
  VP8LPredictorsAdd[10] = PredictorAdd10_AVX2;
  VP8LPredictorsAdd[11] = PredictorAdd11_AVX2;
  VP8LPredictorsAdd[12] = PredictorAdd12_AVX2;

  VP8LAddGreenToBlueAndRed = AddGreenToBlueAndRed_AVX2;
  VP8LTransformColorInverse = TransformColorInverse_AVX2;
  VP8LConvertBGRAToRGBA = ConvertBGRAToRGBA_AVX2;
}

#else

WEBP_DSP_INIT_STUB(VP8LDspInitAVX2)

#endif

#if defined(WEBP_USE_NEON)

#include <arm_neon.h>

#if !defined(WORK_AROUND_GCC)

static void ConvertBGRAToRGBA_NEON(const uint32_t* WEBP_RESTRICT src,
                                   int num_pixels, uint8_t* WEBP_RESTRICT dst) {
  const uint32_t* const end = src + (num_pixels & ~15);
  for (; src < end; src += 16) {
    uint8x16x4_t pixel = vld4q_u8((uint8_t*)src);

    const uint8x16_t tmp = pixel.val[0];
    pixel.val[0] = pixel.val[2];
    pixel.val[2] = tmp;
    vst4q_u8(dst, pixel);
    dst += 64;
  }
  VP8LConvertBGRAToRGBA_C(src, num_pixels & 15, dst);
}

static void ConvertBGRAToBGR_NEON(const uint32_t* WEBP_RESTRICT src,
                                  int num_pixels, uint8_t* WEBP_RESTRICT dst) {
  const uint32_t* const end = src + (num_pixels & ~15);
  for (; src < end; src += 16) {
    const uint8x16x4_t pixel = vld4q_u8((uint8_t*)src);
    const uint8x16x3_t tmp = { { pixel.val[0], pixel.val[1], pixel.val[2] } };
    vst3q_u8(dst, tmp);
    dst += 48;
  }
  VP8LConvertBGRAToBGR_C(src, num_pixels & 15, dst);
}

static void ConvertBGRAToRGB_NEON(const uint32_t* WEBP_RESTRICT src,
                                  int num_pixels, uint8_t* WEBP_RESTRICT dst) {
  const uint32_t* const end = src + (num_pixels & ~15);
  for (; src < end; src += 16) {
    const uint8x16x4_t pixel = vld4q_u8((uint8_t*)src);
    const uint8x16x3_t tmp = { { pixel.val[2], pixel.val[1], pixel.val[0] } };
    vst3q_u8(dst, tmp);
    dst += 48;
  }
  VP8LConvertBGRAToRGB_C(src, num_pixels & 15, dst);
}

#else

static const uint8_t kRGBAShuffle[8] = { 2, 1, 0, 3, 6, 5, 4, 7 };

static void ConvertBGRAToRGBA_NEON(const uint32_t* WEBP_RESTRICT src,
                                   int num_pixels, uint8_t* WEBP_RESTRICT dst) {
  const uint32_t* const end = src + (num_pixels & ~1);
  const uint8x8_t shuffle = vld1_u8(kRGBAShuffle);
  for (; src < end; src += 2) {
    const uint8x8_t pixels = vld1_u8((uint8_t*)src);
    vst1_u8(dst, vtbl1_u8(pixels, shuffle));
    dst += 8;
  }
  VP8LConvertBGRAToRGBA_C(src, num_pixels & 1, dst);
}

static const uint8_t kBGRShuffle[3][8] = {
  {  0,  1,  2,  4,  5,  6,  8,  9 },
  { 10, 12, 13, 14, 16, 17, 18, 20 },
  { 21, 22, 24, 25, 26, 28, 29, 30 }
};

static void ConvertBGRAToBGR_NEON(const uint32_t* WEBP_RESTRICT src,
                                  int num_pixels, uint8_t* WEBP_RESTRICT dst) {
  const uint32_t* const end = src + (num_pixels & ~7);
  const uint8x8_t shuffle0 = vld1_u8(kBGRShuffle[0]);
  const uint8x8_t shuffle1 = vld1_u8(kBGRShuffle[1]);
  const uint8x8_t shuffle2 = vld1_u8(kBGRShuffle[2]);
  for (; src < end; src += 8) {
    uint8x8x4_t pixels;
    INIT_VECTOR4(pixels,
                 vld1_u8((const uint8_t*)(src + 0)),
                 vld1_u8((const uint8_t*)(src + 2)),
                 vld1_u8((const uint8_t*)(src + 4)),
                 vld1_u8((const uint8_t*)(src + 6)));
    vst1_u8(dst +  0, vtbl4_u8(pixels, shuffle0));
    vst1_u8(dst +  8, vtbl4_u8(pixels, shuffle1));
    vst1_u8(dst + 16, vtbl4_u8(pixels, shuffle2));
    dst += 8 * 3;
  }
  VP8LConvertBGRAToBGR_C(src, num_pixels & 7, dst);
}

static const uint8_t kRGBShuffle[3][8] = {
  {  2,  1,  0,  6,  5,  4, 10,  9 },
  {  8, 14, 13, 12, 18, 17, 16, 22 },
  { 21, 20, 26, 25, 24, 30, 29, 28 }
};

static void ConvertBGRAToRGB_NEON(const uint32_t* WEBP_RESTRICT src,
                                  int num_pixels, uint8_t* WEBP_RESTRICT dst) {
  const uint32_t* const end = src + (num_pixels & ~7);
  const uint8x8_t shuffle0 = vld1_u8(kRGBShuffle[0]);
  const uint8x8_t shuffle1 = vld1_u8(kRGBShuffle[1]);
  const uint8x8_t shuffle2 = vld1_u8(kRGBShuffle[2]);
  for (; src < end; src += 8) {
    uint8x8x4_t pixels;
    INIT_VECTOR4(pixels,
                 vld1_u8((const uint8_t*)(src + 0)),
                 vld1_u8((const uint8_t*)(src + 2)),
                 vld1_u8((const uint8_t*)(src + 4)),
                 vld1_u8((const uint8_t*)(src + 6)));
    vst1_u8(dst +  0, vtbl4_u8(pixels, shuffle0));
    vst1_u8(dst +  8, vtbl4_u8(pixels, shuffle1));
    vst1_u8(dst + 16, vtbl4_u8(pixels, shuffle2));
    dst += 8 * 3;
  }
  VP8LConvertBGRAToRGB_C(src, num_pixels & 7, dst);
}

#endif

#define LOAD_U32_AS_U8(IN) vreinterpret_u8_u32(vdup_n_u32((IN)))
#define LOAD_U32P_AS_U8(IN) vreinterpret_u8_u32(vld1_u32((IN)))
#define LOADQ_U32_AS_U8(IN) vreinterpretq_u8_u32(vdupq_n_u32((IN)))
#define LOADQ_U32P_AS_U8(IN) vreinterpretq_u8_u32(vld1q_u32((IN)))
#define GET_U8_AS_U32(IN) vget_lane_u32(vreinterpret_u32_u8((IN)), 0)
#define GETQ_U8_AS_U32(IN) vgetq_lane_u32(vreinterpretq_u32_u8((IN)), 0)
#define STOREQ_U8_AS_U32P(OUT, IN) vst1q_u32((OUT), vreinterpretq_u32_u8((IN)))
#define ROTATE32_LEFT(L) vextq_u8((L), (L), 12)

static WEBP_INLINE uint8x8_t Average2_u8_NEON(uint32_t a0, uint32_t a1) {
  const uint8x8_t A0 = LOAD_U32_AS_U8(a0);
  const uint8x8_t A1 = LOAD_U32_AS_U8(a1);
  return vhadd_u8(A0, A1);
}

static WEBP_INLINE uint32_t ClampedAddSubtractHalf_NEON(uint32_t c0,
                                                        uint32_t c1,
                                                        uint32_t c2) {
  const uint8x8_t avg = Average2_u8_NEON(c0, c1);

  const uint8x8_t C2 = LOAD_U32_AS_U8(c2);
  const uint8x8_t cmp = vcgt_u8(C2, avg);
  const uint8x8_t C2_1 = vadd_u8(C2, cmp);

  const int8x8_t diff_avg = vreinterpret_s8_u8(vhsub_u8(avg, C2_1));

  const int16x8_t avg_16 = vreinterpretq_s16_u16(vmovl_u8(avg));
  const uint8x8_t res = vqmovun_s16(vaddw_s8(avg_16, diff_avg));
  const uint32_t output = GET_U8_AS_U32(res);
  return output;
}

static WEBP_INLINE uint32_t Average2_NEON(uint32_t a0, uint32_t a1) {
  const uint8x8_t avg_u8x8 = Average2_u8_NEON(a0, a1);
  const uint32_t avg = GET_U8_AS_U32(avg_u8x8);
  return avg;
}

static WEBP_INLINE uint32_t Average3_NEON(uint32_t a0, uint32_t a1,
                                          uint32_t a2) {
  const uint8x8_t avg0 = Average2_u8_NEON(a0, a2);
  const uint8x8_t A1 = LOAD_U32_AS_U8(a1);
  const uint32_t avg = GET_U8_AS_U32(vhadd_u8(avg0, A1));
  return avg;
}

static uint32_t Predictor5_NEON(const uint32_t* const left,
                                const uint32_t* const top) {
  return Average3_NEON(*left, top[0], top[1]);
}
static uint32_t Predictor6_NEON(const uint32_t* const left,
                                const uint32_t* const top) {
  return Average2_NEON(*left, top[-1]);
}
static uint32_t Predictor7_NEON(const uint32_t* const left,
                                const uint32_t* const top) {
  return Average2_NEON(*left, top[0]);
}
static uint32_t Predictor13_NEON(const uint32_t* const left,
                                 const uint32_t* const top) {
  return ClampedAddSubtractHalf_NEON(*left, top[0], top[-1]);
}

static void PredictorAdd0_NEON(const uint32_t* in, const uint32_t* upper,
                               int num_pixels, uint32_t* WEBP_RESTRICT out) {
  int i;
  const uint8x16_t black = vreinterpretq_u8_u32(vdupq_n_u32(ARGB_BLACK));
  for (i = 0; i + 4 <= num_pixels; i += 4) {
    const uint8x16_t src = LOADQ_U32P_AS_U8(&in[i]);
    const uint8x16_t res = vaddq_u8(src, black);
    STOREQ_U8_AS_U32P(&out[i], res);
  }
  VP8LPredictorsAdd_C[0](in + i, upper + i, num_pixels - i, out + i);
}

static void PredictorAdd1_NEON(const uint32_t* in, const uint32_t* upper,
                               int num_pixels, uint32_t* WEBP_RESTRICT out) {
  int i;
  const uint8x16_t zero = LOADQ_U32_AS_U8(0);
  for (i = 0; i + 4 <= num_pixels; i += 4) {

    const uint8x16_t src = LOADQ_U32P_AS_U8(&in[i]);

    const uint8x16_t shift0 = vextq_u8(zero, src, 12);

    const uint8x16_t sum0 = vaddq_u8(src, shift0);

    const uint8x16_t shift1 = vextq_u8(zero, sum0, 8);

    const uint8x16_t sum1 = vaddq_u8(sum0, shift1);
    const uint8x16_t prev = LOADQ_U32_AS_U8(out[i - 1]);
    const uint8x16_t res = vaddq_u8(sum1, prev);
    STOREQ_U8_AS_U32P(&out[i], res);
  }
  VP8LPredictorsAdd_C[1](in + i, upper + i, num_pixels - i, out + i);
}

#define GENERATE_PREDICTOR_1(X, IN)                                       \
static void PredictorAdd##X##_NEON(const uint32_t* in,                    \
                                   const uint32_t* upper, int num_pixels, \
                                   uint32_t* WEBP_RESTRICT out) {         \
  int i;                                                                  \
  for (i = 0; i + 4 <= num_pixels; i += 4) {                              \
    const uint8x16_t src = LOADQ_U32P_AS_U8(&in[i]);                      \
    const uint8x16_t other = LOADQ_U32P_AS_U8(&(IN));                     \
    const uint8x16_t res = vaddq_u8(src, other);                          \
    STOREQ_U8_AS_U32P(&out[i], res);                                      \
  }                                                                       \
  VP8LPredictorsAdd_C[(X)](in + i, upper + i, num_pixels - i, out + i);   \
}

GENERATE_PREDICTOR_1(2, upper[i])

GENERATE_PREDICTOR_1(3, upper[i + 1])

GENERATE_PREDICTOR_1(4, upper[i - 1])
#undef GENERATE_PREDICTOR_1

#define DO_PRED5(LANE) do {                                              \
  const uint8x16_t avgLTR = vhaddq_u8(L, TR);                            \
  const uint8x16_t avg = vhaddq_u8(avgLTR, T);                           \
  const uint8x16_t res = vaddq_u8(avg, src);                             \
  vst1q_lane_u32(&out[i + (LANE)], vreinterpretq_u32_u8(res), (LANE));   \
  L = ROTATE32_LEFT(res);                                                \
} while (0)

static void PredictorAdd5_NEON(const uint32_t* in, const uint32_t* upper,
                               int num_pixels, uint32_t* WEBP_RESTRICT out) {
  int i;
  uint8x16_t L = LOADQ_U32_AS_U8(out[-1]);
  for (i = 0; i + 4 <= num_pixels; i += 4) {
    const uint8x16_t src = LOADQ_U32P_AS_U8(&in[i]);
    const uint8x16_t T = LOADQ_U32P_AS_U8(&upper[i + 0]);
    const uint8x16_t TR = LOADQ_U32P_AS_U8(&upper[i + 1]);
    DO_PRED5(0);
    DO_PRED5(1);
    DO_PRED5(2);
    DO_PRED5(3);
  }
  VP8LPredictorsAdd_C[5](in + i, upper + i, num_pixels - i, out + i);
}
#undef DO_PRED5

#define DO_PRED67(LANE) do {                                             \
  const uint8x16_t avg = vhaddq_u8(L, top);                              \
  const uint8x16_t res = vaddq_u8(avg, src);                             \
  vst1q_lane_u32(&out[i + (LANE)], vreinterpretq_u32_u8(res), (LANE));   \
  L = ROTATE32_LEFT(res);                                                \
} while (0)

static void PredictorAdd6_NEON(const uint32_t* in, const uint32_t* upper,
                               int num_pixels, uint32_t* WEBP_RESTRICT out) {
  int i;
  uint8x16_t L = LOADQ_U32_AS_U8(out[-1]);
  for (i = 0; i + 4 <= num_pixels; i += 4) {
    const uint8x16_t src = LOADQ_U32P_AS_U8(&in[i]);
    const uint8x16_t top = LOADQ_U32P_AS_U8(&upper[i - 1]);
    DO_PRED67(0);
    DO_PRED67(1);
    DO_PRED67(2);
    DO_PRED67(3);
  }
  VP8LPredictorsAdd_C[6](in + i, upper + i, num_pixels - i, out + i);
}

static void PredictorAdd7_NEON(const uint32_t* in, const uint32_t* upper,
                               int num_pixels, uint32_t* WEBP_RESTRICT out) {
  int i;
  uint8x16_t L = LOADQ_U32_AS_U8(out[-1]);
  for (i = 0; i + 4 <= num_pixels; i += 4) {
    const uint8x16_t src = LOADQ_U32P_AS_U8(&in[i]);
    const uint8x16_t top = LOADQ_U32P_AS_U8(&upper[i]);
    DO_PRED67(0);
    DO_PRED67(1);
    DO_PRED67(2);
    DO_PRED67(3);
  }
  VP8LPredictorsAdd_C[7](in + i, upper + i, num_pixels - i, out + i);
}
#undef DO_PRED67

#define GENERATE_PREDICTOR_2(X, IN)                                       \
static void PredictorAdd##X##_NEON(const uint32_t* in,                    \
                                   const uint32_t* upper, int num_pixels, \
                                   uint32_t* WEBP_RESTRICT out) {         \
  int i;                                                                  \
  for (i = 0; i + 4 <= num_pixels; i += 4) {                              \
    const uint8x16_t src = LOADQ_U32P_AS_U8(&in[i]);                      \
    const uint8x16_t Tother = LOADQ_U32P_AS_U8(&(IN));                    \
    const uint8x16_t T = LOADQ_U32P_AS_U8(&upper[i]);                     \
    const uint8x16_t avg = vhaddq_u8(T, Tother);                          \
    const uint8x16_t res = vaddq_u8(avg, src);                            \
    STOREQ_U8_AS_U32P(&out[i], res);                                      \
  }                                                                       \
  VP8LPredictorsAdd_C[(X)](in + i, upper + i, num_pixels - i, out + i);   \
}

GENERATE_PREDICTOR_2(8, upper[i - 1])

GENERATE_PREDICTOR_2(9, upper[i + 1])
#undef GENERATE_PREDICTOR_2

#define DO_PRED10(LANE) do {                                             \
  const uint8x16_t avgLTL = vhaddq_u8(L, TL);                            \
  const uint8x16_t avg = vhaddq_u8(avgTTR, avgLTL);                      \
  const uint8x16_t res = vaddq_u8(avg, src);                             \
  vst1q_lane_u32(&out[i + (LANE)], vreinterpretq_u32_u8(res), (LANE));   \
  L = ROTATE32_LEFT(res);                                                \
} while (0)

static void PredictorAdd10_NEON(const uint32_t* in, const uint32_t* upper,
                                int num_pixels, uint32_t* WEBP_RESTRICT out) {
  int i;
  uint8x16_t L = LOADQ_U32_AS_U8(out[-1]);
  for (i = 0; i + 4 <= num_pixels; i += 4) {
    const uint8x16_t src = LOADQ_U32P_AS_U8(&in[i]);
    const uint8x16_t TL = LOADQ_U32P_AS_U8(&upper[i - 1]);
    const uint8x16_t T = LOADQ_U32P_AS_U8(&upper[i]);
    const uint8x16_t TR = LOADQ_U32P_AS_U8(&upper[i + 1]);
    const uint8x16_t avgTTR = vhaddq_u8(T, TR);
    DO_PRED10(0);
    DO_PRED10(1);
    DO_PRED10(2);
    DO_PRED10(3);
  }
  VP8LPredictorsAdd_C[10](in + i, upper + i, num_pixels - i, out + i);
}
#undef DO_PRED10

#define DO_PRED11(LANE) do {                                                   \
  const uint8x16_t sumLin = vaddq_u8(L, src);                      \
  const uint8x16_t pLTL = vabdq_u8(L, TL);                       \
  const uint16x8_t sum_LTL = vpaddlq_u8(pLTL);                                 \
  const uint32x4_t pa = vpaddlq_u16(sum_LTL);                                  \
  const uint32x4_t mask = vcleq_u32(pa, pb);                                   \
  const uint8x16_t res = vbslq_u8(vreinterpretq_u8_u32(mask), sumTin, sumLin); \
  vst1q_lane_u32(&out[i + (LANE)], vreinterpretq_u32_u8(res), (LANE));         \
  L = ROTATE32_LEFT(res);                                                      \
} while (0)

static void PredictorAdd11_NEON(const uint32_t* in, const uint32_t* upper,
                                int num_pixels, uint32_t* WEBP_RESTRICT out) {
  int i;
  uint8x16_t L = LOADQ_U32_AS_U8(out[-1]);
  for (i = 0; i + 4 <= num_pixels; i += 4) {
    const uint8x16_t T = LOADQ_U32P_AS_U8(&upper[i]);
    const uint8x16_t TL = LOADQ_U32P_AS_U8(&upper[i - 1]);
    const uint8x16_t pTTL = vabdq_u8(T, TL);
    const uint16x8_t sum_TTL = vpaddlq_u8(pTTL);
    const uint32x4_t pb = vpaddlq_u16(sum_TTL);
    const uint8x16_t src = LOADQ_U32P_AS_U8(&in[i]);
    const uint8x16_t sumTin = vaddq_u8(T, src);
    DO_PRED11(0);
    DO_PRED11(1);
    DO_PRED11(2);
    DO_PRED11(3);
  }
  VP8LPredictorsAdd_C[11](in + i, upper + i, num_pixels - i, out + i);
}
#undef DO_PRED11

#define DO_PRED12(DIFF, LANE) do {                                       \
  const uint8x8_t pred =                                                 \
      vqmovun_s16(vaddq_s16(vreinterpretq_s16_u16(L), (DIFF)));          \
  const uint8x8_t res =                                                  \
      vadd_u8(pred, (LANE <= 1) ? vget_low_u8(src) : vget_high_u8(src)); \
  const uint16x8_t res16 = vmovl_u8(res);                                \
  vst1_lane_u32(&out[i + (LANE)], vreinterpret_u32_u8(res), (LANE) & 1); \
                    \
  L = vextq_u16(res16, res16, 4);                                        \
} while (0)

static void PredictorAdd12_NEON(const uint32_t* in, const uint32_t* upper,
                                int num_pixels, uint32_t* WEBP_RESTRICT out) {
  int i;
  uint16x8_t L = vmovl_u8(LOAD_U32_AS_U8(out[-1]));
  for (i = 0; i + 4 <= num_pixels; i += 4) {

    const uint8x16_t src = LOADQ_U32P_AS_U8(&in[i]);

    const uint8x16_t TL = LOADQ_U32P_AS_U8(&upper[i - 1]);
    const uint8x16_t T = LOADQ_U32P_AS_U8(&upper[i]);
    const int16x8_t diff_lo =
        vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(T), vget_low_u8(TL)));
    const int16x8_t diff_hi =
        vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(T), vget_high_u8(TL)));

    DO_PRED12(diff_lo, 0);
    DO_PRED12(diff_lo, 1);
    DO_PRED12(diff_hi, 2);
    DO_PRED12(diff_hi, 3);
  }
  VP8LPredictorsAdd_C[12](in + i, upper + i, num_pixels - i, out + i);
}
#undef DO_PRED12

#define DO_PRED13(LANE, LOW_OR_HI) do {                                        \
  const uint8x16_t avg = vhaddq_u8(L, T);                                      \
  const uint8x16_t cmp = vcgtq_u8(TL, avg);                                    \
  const uint8x16_t TL_1 = vaddq_u8(TL, cmp);                                   \
                      \
  const int8x8_t diff_avg =                                                    \
      vreinterpret_s8_u8(LOW_OR_HI(vhsubq_u8(avg, TL_1)));                     \
                                   \
  const int16x8_t avg_16 = vreinterpretq_s16_u16(vmovl_u8(LOW_OR_HI(avg)));    \
  const uint8x8_t delta = vqmovun_s16(vaddw_s8(avg_16, diff_avg));             \
  const uint8x8_t res = vadd_u8(LOW_OR_HI(src), delta);                        \
  const uint8x16_t res2 = vcombine_u8(res, res);                               \
  vst1_lane_u32(&out[i + (LANE)], vreinterpret_u32_u8(res), (LANE) & 1);       \
  L = ROTATE32_LEFT(res2);                                                     \
} while (0)

static void PredictorAdd13_NEON(const uint32_t* in, const uint32_t* upper,
                                int num_pixels, uint32_t* WEBP_RESTRICT out) {
  int i;
  uint8x16_t L = LOADQ_U32_AS_U8(out[-1]);
  for (i = 0; i + 4 <= num_pixels; i += 4) {
    const uint8x16_t src = LOADQ_U32P_AS_U8(&in[i]);
    const uint8x16_t T = LOADQ_U32P_AS_U8(&upper[i]);
    const uint8x16_t TL = LOADQ_U32P_AS_U8(&upper[i - 1]);
    DO_PRED13(0, vget_low_u8);
    DO_PRED13(1, vget_low_u8);
    DO_PRED13(2, vget_high_u8);
    DO_PRED13(3, vget_high_u8);
  }
  VP8LPredictorsAdd_C[13](in + i, upper + i, num_pixels - i, out + i);
}
#undef DO_PRED13

#undef LOAD_U32_AS_U8
#undef LOAD_U32P_AS_U8
#undef LOADQ_U32_AS_U8
#undef LOADQ_U32P_AS_U8
#undef GET_U8_AS_U32
#undef GETQ_U8_AS_U32
#undef STOREQ_U8_AS_U32P
#undef ROTATE32_LEFT

#if defined(__APPLE__) && WEBP_AARCH64 && \
    defined(__apple_build_version__) && (__apple_build_version__< 6020037)
#define USE_VTBLQ
#endif

#ifdef USE_VTBLQ

static const uint8_t kGreenShuffle[16] = {
  1, 255, 1, 255, 5, 255, 5, 255, 9, 255, 9, 255, 13, 255, 13, 255
};

static WEBP_INLINE uint8x16_t DoGreenShuffle_NEON(const uint8x16_t argb,
                                                  const uint8x16_t shuffle) {
  return vcombine_u8(vtbl1q_u8(argb, vget_low_u8(shuffle)),
                     vtbl1q_u8(argb, vget_high_u8(shuffle)));
}
#else

static const uint8_t kGreenShuffle[8] = { 1, 255, 1, 255, 5, 255, 5, 255  };

static WEBP_INLINE uint8x16_t DoGreenShuffle_NEON(const uint8x16_t argb,
                                                  const uint8x8_t shuffle) {
  return vcombine_u8(vtbl1_u8(vget_low_u8(argb), shuffle),
                     vtbl1_u8(vget_high_u8(argb), shuffle));
}
#endif

static void AddGreenToBlueAndRed_NEON(const uint32_t* src, int num_pixels,
                                      uint32_t* dst) {
  const uint32_t* const end = src + (num_pixels & ~3);
#ifdef USE_VTBLQ
  const uint8x16_t shuffle = vld1q_u8(kGreenShuffle);
#else
  const uint8x8_t shuffle = vld1_u8(kGreenShuffle);
#endif
  for (; src < end; src += 4, dst += 4) {
    const uint8x16_t argb = vld1q_u8((const uint8_t*)src);
    const uint8x16_t greens = DoGreenShuffle_NEON(argb, shuffle);
    vst1q_u8((uint8_t*)dst, vaddq_u8(argb, greens));
  }

  VP8LAddGreenToBlueAndRed_C(src, num_pixels & 3, dst);
}

static void TransformColorInverse_NEON(const VP8LMultipliers* const m,
                                       const uint32_t* const src,
                                       int num_pixels, uint32_t* dst) {

#define CST(X)  (((int16_t)(m->X << 8)) >> 6)
  const int16_t rb[8] = {
    CST(green_to_blue), CST(green_to_red),
    CST(green_to_blue), CST(green_to_red),
    CST(green_to_blue), CST(green_to_red),
    CST(green_to_blue), CST(green_to_red)
  };
  const int16x8_t mults_rb = vld1q_s16(rb);
  const int16_t b2[8] = {
    0, CST(red_to_blue), 0, CST(red_to_blue),
    0, CST(red_to_blue), 0, CST(red_to_blue),
  };
  const int16x8_t mults_b2 = vld1q_s16(b2);
#undef CST
#ifdef USE_VTBLQ
  static const uint8_t kg0g0[16] = {
    255, 1, 255, 1, 255, 5, 255, 5, 255, 9, 255, 9, 255, 13, 255, 13
  };
  const uint8x16_t shuffle = vld1q_u8(kg0g0);
#else
  static const uint8_t k0g0g[8] = { 255, 1, 255, 1, 255, 5, 255, 5 };
  const uint8x8_t shuffle = vld1_u8(k0g0g);
#endif
  const uint32x4_t mask_ag = vdupq_n_u32(0xff00ff00u);
  int i;
  for (i = 0; i + 4 <= num_pixels; i += 4) {
    const uint8x16_t in = vld1q_u8((const uint8_t*)(src + i));
    const uint32x4_t a0g0 = vandq_u32(vreinterpretq_u32_u8(in), mask_ag);

    const uint8x16_t greens = DoGreenShuffle_NEON(in, shuffle);

    const int16x8_t A = vqdmulhq_s16(vreinterpretq_s16_u8(greens), mults_rb);

    const int8x16_t B = vaddq_s8(vreinterpretq_s8_u8(in),
                                 vreinterpretq_s8_s16(A));

    const int16x8_t C = vshlq_n_s16(vreinterpretq_s16_s8(B), 8);

    const int16x8_t D = vqdmulhq_s16(C, mults_b2);

    const uint32x4_t E = vshrq_n_u32(vreinterpretq_u32_s16(D), 8);

    const int8x16_t F = vaddq_s8(vreinterpretq_s8_u32(E),
                                 vreinterpretq_s8_s16(C));

    const uint16x8_t G = vshrq_n_u16(vreinterpretq_u16_s8(F), 8);
    const uint32x4_t out = vorrq_u32(vreinterpretq_u32_u16(G), a0g0);
    vst1q_u32(dst + i, out);
  }

  VP8LTransformColorInverse_C(m, src + i, num_pixels - i, dst + i);
}

#undef USE_VTBLQ

extern void VP8LDspInitNEON(void);

WEBP_TSAN_IGNORE_FUNCTION void VP8LDspInitNEON(void) {
  VP8LPredictors[5] = Predictor5_NEON;
  VP8LPredictors[6] = Predictor6_NEON;
  VP8LPredictors[7] = Predictor7_NEON;
  VP8LPredictors[13] = Predictor13_NEON;

  VP8LPredictorsAdd[0] = PredictorAdd0_NEON;
  VP8LPredictorsAdd[1] = PredictorAdd1_NEON;
  VP8LPredictorsAdd[2] = PredictorAdd2_NEON;
  VP8LPredictorsAdd[3] = PredictorAdd3_NEON;
  VP8LPredictorsAdd[4] = PredictorAdd4_NEON;
  VP8LPredictorsAdd[5] = PredictorAdd5_NEON;
  VP8LPredictorsAdd[6] = PredictorAdd6_NEON;
  VP8LPredictorsAdd[7] = PredictorAdd7_NEON;
  VP8LPredictorsAdd[8] = PredictorAdd8_NEON;
  VP8LPredictorsAdd[9] = PredictorAdd9_NEON;
  VP8LPredictorsAdd[10] = PredictorAdd10_NEON;
  VP8LPredictorsAdd[11] = PredictorAdd11_NEON;
  VP8LPredictorsAdd[12] = PredictorAdd12_NEON;
  VP8LPredictorsAdd[13] = PredictorAdd13_NEON;

  VP8LConvertBGRAToRGBA = ConvertBGRAToRGBA_NEON;
  VP8LConvertBGRAToBGR = ConvertBGRAToBGR_NEON;
  VP8LConvertBGRAToRGB = ConvertBGRAToRGB_NEON;

  VP8LAddGreenToBlueAndRed = AddGreenToBlueAndRed_NEON;
  VP8LTransformColorInverse = TransformColorInverse_NEON;
}

#else

WEBP_DSP_INIT_STUB(VP8LDspInitNEON)

#endif

#if defined(WEBP_USE_SSE2)

#include <emmintrin.h>
#include <string.h>

static WEBP_INLINE uint32_t ClampedAddSubtractFull_SSE2(uint32_t c0,
                                                        uint32_t c1,
                                                        uint32_t c2) {
  const __m128i zero = _mm_setzero_si128();
  const __m128i C0 = _mm_unpacklo_epi8(_mm_cvtsi32_si128((int)c0), zero);
  const __m128i C1 = _mm_unpacklo_epi8(_mm_cvtsi32_si128((int)c1), zero);
  const __m128i C2 = _mm_unpacklo_epi8(_mm_cvtsi32_si128((int)c2), zero);
  const __m128i V1 = _mm_add_epi16(C0, C1);
  const __m128i V2 = _mm_sub_epi16(V1, C2);
  const __m128i b = _mm_packus_epi16(V2, V2);
  return (uint32_t)_mm_cvtsi128_si32(b);
}

static WEBP_INLINE uint32_t ClampedAddSubtractHalf_SSE2(uint32_t c0,
                                                        uint32_t c1,
                                                        uint32_t c2) {
  const __m128i zero = _mm_setzero_si128();
  const __m128i C0 = _mm_unpacklo_epi8(_mm_cvtsi32_si128((int)c0), zero);
  const __m128i C1 = _mm_unpacklo_epi8(_mm_cvtsi32_si128((int)c1), zero);
  const __m128i B0 = _mm_unpacklo_epi8(_mm_cvtsi32_si128((int)c2), zero);
  const __m128i avg = _mm_add_epi16(C1, C0);
  const __m128i A0 = _mm_srli_epi16(avg, 1);
  const __m128i A1 = _mm_sub_epi16(A0, B0);
  const __m128i BgtA = _mm_cmpgt_epi16(B0, A0);
  const __m128i A2 = _mm_sub_epi16(A1, BgtA);
  const __m128i A3 = _mm_srai_epi16(A2, 1);
  const __m128i A4 = _mm_add_epi16(A0, A3);
  const __m128i A5 = _mm_packus_epi16(A4, A4);
  return (uint32_t)_mm_cvtsi128_si32(A5);
}

static WEBP_INLINE uint32_t Select_SSE2(uint32_t a, uint32_t b, uint32_t c) {
  int pa_minus_pb;
  const __m128i zero = _mm_setzero_si128();
  const __m128i A0 = _mm_cvtsi32_si128((int)a);
  const __m128i B0 = _mm_cvtsi32_si128((int)b);
  const __m128i C0 = _mm_cvtsi32_si128((int)c);
  const __m128i AC0 = _mm_subs_epu8(A0, C0);
  const __m128i CA0 = _mm_subs_epu8(C0, A0);
  const __m128i BC0 = _mm_subs_epu8(B0, C0);
  const __m128i CB0 = _mm_subs_epu8(C0, B0);
  const __m128i AC = _mm_or_si128(AC0, CA0);
  const __m128i BC = _mm_or_si128(BC0, CB0);
  const __m128i pa = _mm_unpacklo_epi8(AC, zero);
  const __m128i pb = _mm_unpacklo_epi8(BC, zero);
  const __m128i diff = _mm_sub_epi16(pb, pa);
  {
    int16_t out[8];
    _mm_storeu_si128((__m128i*)out, diff);
    pa_minus_pb = out[0] + out[1] + out[2] + out[3];
  }
  return (pa_minus_pb <= 0) ? a : b;
}

static WEBP_INLINE void Average2_m128i(const __m128i* const a0,
                                       const __m128i* const a1,
                                       __m128i* const avg) {

  const __m128i ones = _mm_set1_epi8(1);
  const __m128i avg1 = _mm_avg_epu8(*a0, *a1);
  const __m128i one = _mm_and_si128(_mm_xor_si128(*a0, *a1), ones);
  *avg = _mm_sub_epi8(avg1, one);
}

static WEBP_INLINE void Average2_uint32_SSE2(const uint32_t a0,
                                             const uint32_t a1,
                                             __m128i* const avg) {

  const __m128i ones = _mm_set1_epi8(1);
  const __m128i A0 = _mm_cvtsi32_si128((int)a0);
  const __m128i A1 = _mm_cvtsi32_si128((int)a1);
  const __m128i avg1 = _mm_avg_epu8(A0, A1);
  const __m128i one = _mm_and_si128(_mm_xor_si128(A0, A1), ones);
  *avg = _mm_sub_epi8(avg1, one);
}

static WEBP_INLINE __m128i Average2_uint32_16_SSE2(uint32_t a0, uint32_t a1) {
  const __m128i zero = _mm_setzero_si128();
  const __m128i A0 = _mm_unpacklo_epi8(_mm_cvtsi32_si128((int)a0), zero);
  const __m128i A1 = _mm_unpacklo_epi8(_mm_cvtsi32_si128((int)a1), zero);
  const __m128i sum = _mm_add_epi16(A1, A0);
  return _mm_srli_epi16(sum, 1);
}

static WEBP_INLINE uint32_t Average2_SSE2(uint32_t a0, uint32_t a1) {
  __m128i output;
  Average2_uint32_SSE2(a0, a1, &output);
  return (uint32_t)_mm_cvtsi128_si32(output);
}

static WEBP_INLINE uint32_t Average3_SSE2(uint32_t a0, uint32_t a1,
                                          uint32_t a2) {
  const __m128i zero = _mm_setzero_si128();
  const __m128i avg1 = Average2_uint32_16_SSE2(a0, a2);
  const __m128i A1 = _mm_unpacklo_epi8(_mm_cvtsi32_si128((int)a1), zero);
  const __m128i sum = _mm_add_epi16(avg1, A1);
  const __m128i avg2 = _mm_srli_epi16(sum, 1);
  const __m128i A2 = _mm_packus_epi16(avg2, avg2);
  return (uint32_t)_mm_cvtsi128_si32(A2);
}

static WEBP_INLINE uint32_t Average4_SSE2(uint32_t a0, uint32_t a1,
                                          uint32_t a2, uint32_t a3) {
  const __m128i avg1 = Average2_uint32_16_SSE2(a0, a1);
  const __m128i avg2 = Average2_uint32_16_SSE2(a2, a3);
  const __m128i sum = _mm_add_epi16(avg2, avg1);
  const __m128i avg3 = _mm_srli_epi16(sum, 1);
  const __m128i A0 = _mm_packus_epi16(avg3, avg3);
  return (uint32_t)_mm_cvtsi128_si32(A0);
}

static uint32_t Predictor5_SSE2(const uint32_t* const left,
                                const uint32_t* const top) {
  const uint32_t pred = Average3_SSE2(*left, top[0], top[1]);
  return pred;
}
static uint32_t Predictor6_SSE2(const uint32_t* const left,
                                const uint32_t* const top) {
  const uint32_t pred = Average2_SSE2(*left, top[-1]);
  return pred;
}
static uint32_t Predictor7_SSE2(const uint32_t* const left,
                                const uint32_t* const top) {
  const uint32_t pred = Average2_SSE2(*left, top[0]);
  return pred;
}
static uint32_t Predictor8_SSE2(const uint32_t* const left,
                                const uint32_t* const top) {
  const uint32_t pred = Average2_SSE2(top[-1], top[0]);
  (void)left;
  return pred;
}
static uint32_t Predictor9_SSE2(const uint32_t* const left,
                                const uint32_t* const top) {
  const uint32_t pred = Average2_SSE2(top[0], top[1]);
  (void)left;
  return pred;
}
static uint32_t Predictor10_SSE2(const uint32_t* const left,
                                 const uint32_t* const top) {
  const uint32_t pred = Average4_SSE2(*left, top[-1], top[0], top[1]);
  return pred;
}
static uint32_t Predictor11_SSE2(const uint32_t* const left,
                                 const uint32_t* const top) {
  const uint32_t pred = Select_SSE2(top[0], *left, top[-1]);
  return pred;
}
static uint32_t Predictor12_SSE2(const uint32_t* const left,
                                 const uint32_t* const top) {
  const uint32_t pred = ClampedAddSubtractFull_SSE2(*left, top[0], top[-1]);
  return pred;
}
static uint32_t Predictor13_SSE2(const uint32_t* const left,
                                 const uint32_t* const top) {
  const uint32_t pred = ClampedAddSubtractHalf_SSE2(*left, top[0], top[-1]);
  return pred;
}

static void PredictorAdd0_SSE2(const uint32_t* in, const uint32_t* upper,
                               int num_pixels, uint32_t* WEBP_RESTRICT out) {
  int i;
  const __m128i black = _mm_set1_epi32((int)ARGB_BLACK);
  for (i = 0; i + 4 <= num_pixels; i += 4) {
    const __m128i src = _mm_loadu_si128((const __m128i*)&in[i]);
    const __m128i res = _mm_add_epi8(src, black);
    _mm_storeu_si128((__m128i*)&out[i], res);
  }
  if (i != num_pixels) {
    VP8LPredictorsAdd_C[0](in + i, NULL, num_pixels - i, out + i);
  }
  (void)upper;
}

static void PredictorAdd1_SSE2(const uint32_t* in, const uint32_t* upper,
                               int num_pixels, uint32_t* WEBP_RESTRICT out) {
  int i;
  __m128i prev = _mm_set1_epi32((int)out[-1]);
  for (i = 0; i + 4 <= num_pixels; i += 4) {

    const __m128i src = _mm_loadu_si128((const __m128i*)&in[i]);

    const __m128i shift0 = _mm_slli_si128(src, 4);

    const __m128i sum0 = _mm_add_epi8(src, shift0);

    const __m128i shift1 = _mm_slli_si128(sum0, 8);

    const __m128i sum1 = _mm_add_epi8(sum0, shift1);
    const __m128i res = _mm_add_epi8(sum1, prev);
    _mm_storeu_si128((__m128i*)&out[i], res);

    prev = _mm_shuffle_epi32(res, (3 << 0) | (3 << 2) | (3 << 4) | (3 << 6));
  }
  if (i != num_pixels) {
    VP8LPredictorsAdd_C[1](in + i, upper + i, num_pixels - i, out + i);
  }
}

#define GENERATE_PREDICTOR_1(X, IN)                                           \
static void PredictorAdd##X##_SSE2(const uint32_t* in, const uint32_t* upper, \
                                   int num_pixels,                            \
                                   uint32_t* WEBP_RESTRICT out) {             \
  int i;                                                                      \
  for (i = 0; i + 4 <= num_pixels; i += 4) {                                  \
    const __m128i src = _mm_loadu_si128((const __m128i*)&in[i]);              \
    const __m128i other = _mm_loadu_si128((const __m128i*)&(IN));             \
    const __m128i res = _mm_add_epi8(src, other);                             \
    _mm_storeu_si128((__m128i*)&out[i], res);                                 \
  }                                                                           \
  if (i != num_pixels) {                                                      \
    VP8LPredictorsAdd_C[(X)](in + i, upper + i, num_pixels - i, out + i);     \
  }                                                                           \
}

GENERATE_PREDICTOR_1(2, upper[i])

GENERATE_PREDICTOR_1(3, upper[i + 1])

GENERATE_PREDICTOR_1(4, upper[i - 1])
#undef GENERATE_PREDICTOR_1

GENERATE_PREDICTOR_ADD(Predictor5_SSE2, PredictorAdd5_SSE2)
GENERATE_PREDICTOR_ADD(Predictor6_SSE2, PredictorAdd6_SSE2)
GENERATE_PREDICTOR_ADD(Predictor7_SSE2, PredictorAdd7_SSE2)

#define GENERATE_PREDICTOR_2(X, IN)                                           \
static void PredictorAdd##X##_SSE2(const uint32_t* in, const uint32_t* upper, \
                                   int num_pixels,                            \
                                   uint32_t* WEBP_RESTRICT out) {             \
  int i;                                                                      \
  for (i = 0; i + 4 <= num_pixels; i += 4) {                                  \
    const __m128i Tother = _mm_loadu_si128((const __m128i*)&(IN));            \
    const __m128i T = _mm_loadu_si128((const __m128i*)&upper[i]);             \
    const __m128i src = _mm_loadu_si128((const __m128i*)&in[i]);              \
    __m128i avg, res;                                                         \
    Average2_m128i(&T, &Tother, &avg);                                        \
    res = _mm_add_epi8(avg, src);                                             \
    _mm_storeu_si128((__m128i*)&out[i], res);                                 \
  }                                                                           \
  if (i != num_pixels) {                                                      \
    VP8LPredictorsAdd_C[(X)](in + i, upper + i, num_pixels - i, out + i);     \
  }                                                                           \
}

GENERATE_PREDICTOR_2(8, upper[i - 1])

GENERATE_PREDICTOR_2(9, upper[i + 1])
#undef GENERATE_PREDICTOR_2

#define DO_PRED10(OUT) do {                         \
  __m128i avgLTL, avg;                              \
  Average2_m128i(&L, &TL, &avgLTL);                 \
  Average2_m128i(&avgTTR, &avgLTL, &avg);           \
  L = _mm_add_epi8(avg, src);                       \
  out[i + (OUT)] = (uint32_t)_mm_cvtsi128_si32(L);  \
} while (0)

#define DO_PRED10_SHIFT do {                                  \
   \
  avgTTR = _mm_srli_si128(avgTTR, 4);                         \
  TL = _mm_srli_si128(TL, 4);                                 \
  src = _mm_srli_si128(src, 4);                               \
} while (0)

static void PredictorAdd10_SSE2(const uint32_t* in, const uint32_t* upper,
                                int num_pixels, uint32_t* WEBP_RESTRICT out) {
  int i;
  __m128i L = _mm_cvtsi32_si128((int)out[-1]);
  for (i = 0; i + 4 <= num_pixels; i += 4) {
    __m128i src = _mm_loadu_si128((const __m128i*)&in[i]);
    __m128i TL = _mm_loadu_si128((const __m128i*)&upper[i - 1]);
    const __m128i T = _mm_loadu_si128((const __m128i*)&upper[i]);
    const __m128i TR = _mm_loadu_si128((const __m128i*)&upper[i + 1]);
    __m128i avgTTR;
    Average2_m128i(&T, &TR, &avgTTR);
    DO_PRED10(0);
    DO_PRED10_SHIFT;
    DO_PRED10(1);
    DO_PRED10_SHIFT;
    DO_PRED10(2);
    DO_PRED10_SHIFT;
    DO_PRED10(3);
  }
  if (i != num_pixels) {
    VP8LPredictorsAdd_C[10](in + i, upper + i, num_pixels - i, out + i);
  }
}
#undef DO_PRED10
#undef DO_PRED10_SHIFT

#define DO_PRED11(OUT) do {                                            \
  const __m128i L_lo = _mm_unpacklo_epi32(L, T);                       \
  const __m128i TL_lo = _mm_unpacklo_epi32(TL, T);                     \
  const __m128i pb = _mm_sad_epu8(L_lo, TL_lo);    \
  const __m128i mask = _mm_cmpgt_epi32(pb, pa);                        \
  const __m128i A = _mm_and_si128(mask, L);                            \
  const __m128i B = _mm_andnot_si128(mask, T);                         \
  const __m128i pred = _mm_or_si128(A, B);  \
  L = _mm_add_epi8(src, pred);                                         \
  out[i + (OUT)] = (uint32_t)_mm_cvtsi128_si32(L);                     \
} while (0)

#define DO_PRED11_SHIFT do {                                \
   \
  T = _mm_srli_si128(T, 4);                                 \
  TL = _mm_srli_si128(TL, 4);                               \
  src = _mm_srli_si128(src, 4);                             \
  pa = _mm_srli_si128(pa, 4);                               \
} while (0)

static void PredictorAdd11_SSE2(const uint32_t* in, const uint32_t* upper,
                                int num_pixels, uint32_t* WEBP_RESTRICT out) {
  int i;
  __m128i pa;
  __m128i L = _mm_cvtsi32_si128((int)out[-1]);
  for (i = 0; i + 4 <= num_pixels; i += 4) {
    __m128i T = _mm_loadu_si128((const __m128i*)&upper[i]);
    __m128i TL = _mm_loadu_si128((const __m128i*)&upper[i - 1]);
    __m128i src = _mm_loadu_si128((const __m128i*)&in[i]);
    {

      const __m128i T_lo = _mm_unpacklo_epi32(T, T);
      const __m128i TL_lo = _mm_unpacklo_epi32(TL, T);
      const __m128i T_hi = _mm_unpackhi_epi32(T, T);
      const __m128i TL_hi = _mm_unpackhi_epi32(TL, T);
      const __m128i s_lo = _mm_sad_epu8(T_lo, TL_lo);
      const __m128i s_hi = _mm_sad_epu8(T_hi, TL_hi);
      pa = _mm_packs_epi32(s_lo, s_hi);
    }
    DO_PRED11(0);
    DO_PRED11_SHIFT;
    DO_PRED11(1);
    DO_PRED11_SHIFT;
    DO_PRED11(2);
    DO_PRED11_SHIFT;
    DO_PRED11(3);
  }
  if (i != num_pixels) {
    VP8LPredictorsAdd_C[11](in + i, upper + i, num_pixels - i, out + i);
  }
}
#undef DO_PRED11
#undef DO_PRED11_SHIFT

#define DO_PRED12(DIFF, LANE, OUT) do {              \
  const __m128i all = _mm_add_epi16(L, (DIFF));      \
  const __m128i alls = _mm_packus_epi16(all, all);   \
  const __m128i res = _mm_add_epi8(src, alls);       \
  out[i + (OUT)] = (uint32_t)_mm_cvtsi128_si32(res); \
  L = _mm_unpacklo_epi8(res, zero);                  \
} while (0)

#define DO_PRED12_SHIFT(DIFF, LANE) do {                    \
   \
  if ((LANE) == 0) (DIFF) = _mm_srli_si128((DIFF), 8);      \
  src = _mm_srli_si128(src, 4);                             \
} while (0)

static void PredictorAdd12_SSE2(const uint32_t* in, const uint32_t* upper,
                                int num_pixels, uint32_t* WEBP_RESTRICT out) {
  int i;
  const __m128i zero = _mm_setzero_si128();
  const __m128i L8 = _mm_cvtsi32_si128((int)out[-1]);
  __m128i L = _mm_unpacklo_epi8(L8, zero);
  for (i = 0; i + 4 <= num_pixels; i += 4) {

    __m128i src = _mm_loadu_si128((const __m128i*)&in[i]);
    const __m128i T = _mm_loadu_si128((const __m128i*)&upper[i]);
    const __m128i T_lo = _mm_unpacklo_epi8(T, zero);
    const __m128i T_hi = _mm_unpackhi_epi8(T, zero);
    const __m128i TL = _mm_loadu_si128((const __m128i*)&upper[i - 1]);
    const __m128i TL_lo = _mm_unpacklo_epi8(TL, zero);
    const __m128i TL_hi = _mm_unpackhi_epi8(TL, zero);
    __m128i diff_lo = _mm_sub_epi16(T_lo, TL_lo);
    __m128i diff_hi = _mm_sub_epi16(T_hi, TL_hi);
    DO_PRED12(diff_lo, 0, 0);
    DO_PRED12_SHIFT(diff_lo, 0);
    DO_PRED12(diff_lo, 1, 1);
    DO_PRED12_SHIFT(diff_lo, 1);
    DO_PRED12(diff_hi, 0, 2);
    DO_PRED12_SHIFT(diff_hi, 0);
    DO_PRED12(diff_hi, 1, 3);
  }
  if (i != num_pixels) {
    VP8LPredictorsAdd_C[12](in + i, upper + i, num_pixels - i, out + i);
  }
}
#undef DO_PRED12
#undef DO_PRED12_SHIFT

GENERATE_PREDICTOR_ADD(Predictor13_SSE2, PredictorAdd13_SSE2)

static void AddGreenToBlueAndRed_SSE2(const uint32_t* const src, int num_pixels,
                                      uint32_t* dst) {
  int i;
  for (i = 0; i + 4 <= num_pixels; i += 4) {
    const __m128i in = _mm_loadu_si128((const __m128i*)&src[i]);
    const __m128i A = _mm_srli_epi16(in, 8);
    const __m128i B = _mm_shufflelo_epi16(A, _MM_SHUFFLE(2, 2, 0, 0));
    const __m128i C = _mm_shufflehi_epi16(B, _MM_SHUFFLE(2, 2, 0, 0));
    const __m128i out = _mm_add_epi8(in, C);
    _mm_storeu_si128((__m128i*)&dst[i], out);
  }

  if (i != num_pixels) {
    VP8LAddGreenToBlueAndRed_C(src + i, num_pixels - i, dst + i);
  }
}

static void TransformColorInverse_SSE2(const VP8LMultipliers* const m,
                                       const uint32_t* const src,
                                       int num_pixels, uint32_t* dst) {

#define CST(X)  (((int16_t)(m->X << 8)) >> 5)
#define MK_CST_16(HI, LO) \
  _mm_set1_epi32((int)(((uint32_t)(HI) << 16) | ((LO) & 0xffff)))
  const __m128i mults_rb = MK_CST_16(CST(green_to_red), CST(green_to_blue));
  const __m128i mults_b2 = MK_CST_16(CST(red_to_blue), 0);
#undef MK_CST_16
#undef CST
  const __m128i mask_ag = _mm_set1_epi32((int)0xff00ff00);
  int i;
  for (i = 0; i + 4 <= num_pixels; i += 4) {
    const __m128i in = _mm_loadu_si128((const __m128i*)&src[i]);
    const __m128i A = _mm_and_si128(in, mask_ag);
    const __m128i B = _mm_shufflelo_epi16(A, _MM_SHUFFLE(2, 2, 0, 0));
    const __m128i C = _mm_shufflehi_epi16(B, _MM_SHUFFLE(2, 2, 0, 0));
    const __m128i D = _mm_mulhi_epi16(C, mults_rb);
    const __m128i E = _mm_add_epi8(in, D);
    const __m128i F = _mm_slli_epi16(E, 8);
    const __m128i G = _mm_mulhi_epi16(F, mults_b2);
    const __m128i H = _mm_srli_epi32(G, 8);
    const __m128i I = _mm_add_epi8(H, F);
    const __m128i J = _mm_srli_epi16(I, 8);
    const __m128i out = _mm_or_si128(J, A);
    _mm_storeu_si128((__m128i*)&dst[i], out);
  }

  if (i != num_pixels) {
    VP8LTransformColorInverse_C(m, src + i, num_pixels - i, dst + i);
  }
}

static void ConvertBGRAToRGB_SSE2(const uint32_t* WEBP_RESTRICT src,
                                  int num_pixels, uint8_t* WEBP_RESTRICT dst) {
  const __m128i* in = (const __m128i*)src;
  __m128i* out = (__m128i*)dst;

  while (num_pixels >= 32) {

    __m128i in0 = _mm_loadu_si128(in + 0);
    __m128i in1 = _mm_loadu_si128(in + 1);
    __m128i in2 = _mm_loadu_si128(in + 2);
    __m128i in3 = _mm_loadu_si128(in + 3);
    __m128i in4 = _mm_loadu_si128(in + 4);
    __m128i in5 = _mm_loadu_si128(in + 5);
    __m128i in6 = _mm_loadu_si128(in + 6);
    __m128i in7 = _mm_loadu_si128(in + 7);
    VP8L32bToPlanar_SSE2(&in0, &in1, &in2, &in3);
    VP8L32bToPlanar_SSE2(&in4, &in5, &in6, &in7);

    VP8PlanarTo24b_SSE2(&in1, &in5, &in2, &in6, &in3, &in7);
    _mm_storeu_si128(out + 0, in1);
    _mm_storeu_si128(out + 1, in5);
    _mm_storeu_si128(out + 2, in2);
    _mm_storeu_si128(out + 3, in6);
    _mm_storeu_si128(out + 4, in3);
    _mm_storeu_si128(out + 5, in7);
    in += 8;
    out += 6;
    num_pixels -= 32;
  }

  if (num_pixels > 0) {
    VP8LConvertBGRAToRGB_C((const uint32_t*)in, num_pixels, (uint8_t*)out);
  }
}

static void ConvertBGRAToRGBA_SSE2(const uint32_t* WEBP_RESTRICT src,
                                   int num_pixels, uint8_t* WEBP_RESTRICT dst) {
  const __m128i red_blue_mask = _mm_set1_epi32(0x00ff00ff);
  const __m128i* in = (const __m128i*)src;
  __m128i* out = (__m128i*)dst;
  while (num_pixels >= 8) {
    const __m128i A1 = _mm_loadu_si128(in++);
    const __m128i A2 = _mm_loadu_si128(in++);
    const __m128i B1 = _mm_and_si128(A1, red_blue_mask);
    const __m128i B2 = _mm_and_si128(A2, red_blue_mask);
    const __m128i C1 = _mm_andnot_si128(red_blue_mask, A1);
    const __m128i C2 = _mm_andnot_si128(red_blue_mask, A2);
    const __m128i D1 = _mm_shufflelo_epi16(B1, _MM_SHUFFLE(2, 3, 0, 1));
    const __m128i D2 = _mm_shufflelo_epi16(B2, _MM_SHUFFLE(2, 3, 0, 1));
    const __m128i E1 = _mm_shufflehi_epi16(D1, _MM_SHUFFLE(2, 3, 0, 1));
    const __m128i E2 = _mm_shufflehi_epi16(D2, _MM_SHUFFLE(2, 3, 0, 1));
    const __m128i F1 = _mm_or_si128(E1, C1);
    const __m128i F2 = _mm_or_si128(E2, C2);
    _mm_storeu_si128(out++, F1);
    _mm_storeu_si128(out++, F2);
    num_pixels -= 8;
  }

  if (num_pixels > 0) {
    VP8LConvertBGRAToRGBA_C((const uint32_t*)in, num_pixels, (uint8_t*)out);
  }
}

static void ConvertBGRAToRGBA4444_SSE2(const uint32_t* WEBP_RESTRICT src,
                                       int num_pixels,
                                       uint8_t* WEBP_RESTRICT dst) {
  const __m128i mask_0x0f = _mm_set1_epi8(0x0f);
  const __m128i mask_0xf0 = _mm_set1_epi8((char)0xf0);
  const __m128i* in = (const __m128i*)src;
  __m128i* out = (__m128i*)dst;
  while (num_pixels >= 8) {
    const __m128i bgra0 = _mm_loadu_si128(in++);
    const __m128i bgra4 = _mm_loadu_si128(in++);
    const __m128i v0l = _mm_unpacklo_epi8(bgra0, bgra4);
    const __m128i v0h = _mm_unpackhi_epi8(bgra0, bgra4);
    const __m128i v1l = _mm_unpacklo_epi8(v0l, v0h);
    const __m128i v1h = _mm_unpackhi_epi8(v0l, v0h);
    const __m128i v2l = _mm_unpacklo_epi8(v1l, v1h);
    const __m128i v2h = _mm_unpackhi_epi8(v1l, v1h);
    const __m128i ga0 = _mm_unpackhi_epi64(v2l, v2h);
    const __m128i rb0 = _mm_unpacklo_epi64(v2h, v2l);
    const __m128i ga1 = _mm_srli_epi16(ga0, 4);
    const __m128i rb1 = _mm_and_si128(rb0, mask_0xf0);
    const __m128i ga2 = _mm_and_si128(ga1, mask_0x0f);
    const __m128i rgba0 = _mm_or_si128(ga2, rb1);
    const __m128i rgba1 = _mm_srli_si128(rgba0, 8);
#if (WEBP_SWAP_16BIT_CSP == 1)
    const __m128i rgba = _mm_unpacklo_epi8(rgba1, rgba0);
#else
    const __m128i rgba = _mm_unpacklo_epi8(rgba0, rgba1);
#endif
    _mm_storeu_si128(out++, rgba);
    num_pixels -= 8;
  }

  if (num_pixels > 0) {
    VP8LConvertBGRAToRGBA4444_C((const uint32_t*)in, num_pixels, (uint8_t*)out);
  }
}

static void ConvertBGRAToRGB565_SSE2(const uint32_t* WEBP_RESTRICT src,
                                     int num_pixels,
                                     uint8_t* WEBP_RESTRICT dst) {
  const __m128i mask_0xe0 = _mm_set1_epi8((char)0xe0);
  const __m128i mask_0xf8 = _mm_set1_epi8((char)0xf8);
  const __m128i mask_0x07 = _mm_set1_epi8(0x07);
  const __m128i* in = (const __m128i*)src;
  __m128i* out = (__m128i*)dst;
  while (num_pixels >= 8) {
    const __m128i bgra0 = _mm_loadu_si128(in++);
    const __m128i bgra4 = _mm_loadu_si128(in++);
    const __m128i v0l = _mm_unpacklo_epi8(bgra0, bgra4);
    const __m128i v0h = _mm_unpackhi_epi8(bgra0, bgra4);
    const __m128i v1l = _mm_unpacklo_epi8(v0l, v0h);
    const __m128i v1h = _mm_unpackhi_epi8(v0l, v0h);
    const __m128i v2l = _mm_unpacklo_epi8(v1l, v1h);
    const __m128i v2h = _mm_unpackhi_epi8(v1l, v1h);
    const __m128i ga0 = _mm_unpackhi_epi64(v2l, v2h);
    const __m128i rb0 = _mm_unpacklo_epi64(v2h, v2l);
    const __m128i rb1 = _mm_and_si128(rb0, mask_0xf8);
    const __m128i g_lo1 = _mm_srli_epi16(ga0, 5);
    const __m128i g_lo2 = _mm_and_si128(g_lo1, mask_0x07);
    const __m128i g_hi1 = _mm_slli_epi16(ga0, 3);
    const __m128i g_hi2 = _mm_and_si128(g_hi1, mask_0xe0);
    const __m128i b0 = _mm_srli_si128(rb1, 8);
    const __m128i rg1 = _mm_or_si128(rb1, g_lo2);
    const __m128i b1 = _mm_srli_epi16(b0, 3);
    const __m128i gb1 = _mm_or_si128(b1, g_hi2);
#if (WEBP_SWAP_16BIT_CSP == 1)
    const __m128i rgba = _mm_unpacklo_epi8(gb1, rg1);
#else
    const __m128i rgba = _mm_unpacklo_epi8(rg1, gb1);
#endif
    _mm_storeu_si128(out++, rgba);
    num_pixels -= 8;
  }

  if (num_pixels > 0) {
    VP8LConvertBGRAToRGB565_C((const uint32_t*)in, num_pixels, (uint8_t*)out);
  }
}

static void ConvertBGRAToBGR_SSE2(const uint32_t* WEBP_RESTRICT src,
                                  int num_pixels, uint8_t* WEBP_RESTRICT dst) {
  const __m128i mask_l = _mm_set_epi32(0, 0x00ffffff, 0, 0x00ffffff);
  const __m128i mask_h = _mm_set_epi32(0x00ffffff, 0, 0x00ffffff, 0);
  const __m128i* in = (const __m128i*)src;
  const uint8_t* const end = dst + num_pixels * 3;

  while (dst + 26 <= end) {
    const __m128i bgra0 = _mm_loadu_si128(in++);
    const __m128i bgra4 = _mm_loadu_si128(in++);
    const __m128i a0l = _mm_and_si128(bgra0, mask_l);
    const __m128i a4l = _mm_and_si128(bgra4, mask_l);
    const __m128i a0h = _mm_and_si128(bgra0, mask_h);
    const __m128i a4h = _mm_and_si128(bgra4, mask_h);
    const __m128i b0h = _mm_srli_epi64(a0h, 8);
    const __m128i b4h = _mm_srli_epi64(a4h, 8);
    const __m128i c0 = _mm_or_si128(a0l, b0h);
    const __m128i c4 = _mm_or_si128(a4l, b4h);
    const __m128i c2 = _mm_srli_si128(c0, 8);
    const __m128i c6 = _mm_srli_si128(c4, 8);
    _mm_storel_epi64((__m128i*)(dst +   0), c0);
    _mm_storel_epi64((__m128i*)(dst +   6), c2);
    _mm_storel_epi64((__m128i*)(dst +  12), c4);
    _mm_storel_epi64((__m128i*)(dst +  18), c6);
    dst += 24;
    num_pixels -= 8;
  }

  if (num_pixels > 0) {
    VP8LConvertBGRAToBGR_C((const uint32_t*)in, num_pixels, dst);
  }
}

extern void VP8LDspInitSSE2(void);

WEBP_TSAN_IGNORE_FUNCTION void VP8LDspInitSSE2(void) {
  VP8LPredictors[5] = Predictor5_SSE2;
  VP8LPredictors[6] = Predictor6_SSE2;
  VP8LPredictors[7] = Predictor7_SSE2;
  VP8LPredictors[8] = Predictor8_SSE2;
  VP8LPredictors[9] = Predictor9_SSE2;
  VP8LPredictors[10] = Predictor10_SSE2;
  VP8LPredictors[11] = Predictor11_SSE2;
  VP8LPredictors[12] = Predictor12_SSE2;
  VP8LPredictors[13] = Predictor13_SSE2;

  VP8LPredictorsAdd[0] = PredictorAdd0_SSE2;
  VP8LPredictorsAdd[1] = PredictorAdd1_SSE2;
  VP8LPredictorsAdd[2] = PredictorAdd2_SSE2;
  VP8LPredictorsAdd[3] = PredictorAdd3_SSE2;
  VP8LPredictorsAdd[4] = PredictorAdd4_SSE2;
  VP8LPredictorsAdd[5] = PredictorAdd5_SSE2;
  VP8LPredictorsAdd[6] = PredictorAdd6_SSE2;
  VP8LPredictorsAdd[7] = PredictorAdd7_SSE2;
  VP8LPredictorsAdd[8] = PredictorAdd8_SSE2;
  VP8LPredictorsAdd[9] = PredictorAdd9_SSE2;
  VP8LPredictorsAdd[10] = PredictorAdd10_SSE2;
  VP8LPredictorsAdd[11] = PredictorAdd11_SSE2;
  VP8LPredictorsAdd[12] = PredictorAdd12_SSE2;
  VP8LPredictorsAdd[13] = PredictorAdd13_SSE2;

  VP8LAddGreenToBlueAndRed = AddGreenToBlueAndRed_SSE2;
  VP8LTransformColorInverse = TransformColorInverse_SSE2;

  VP8LConvertBGRAToRGB = ConvertBGRAToRGB_SSE2;
  VP8LConvertBGRAToRGBA = ConvertBGRAToRGBA_SSE2;
  VP8LConvertBGRAToRGBA4444 = ConvertBGRAToRGBA4444_SSE2;
  VP8LConvertBGRAToRGB565 = ConvertBGRAToRGB565_SSE2;
  VP8LConvertBGRAToBGR = ConvertBGRAToBGR_SSE2;

  memcpy(VP8LPredictorsAdd_SSE, VP8LPredictorsAdd, sizeof(VP8LPredictorsAdd));

  VP8LAddGreenToBlueAndRed_SSE = AddGreenToBlueAndRed_SSE2;
  VP8LTransformColorInverse_SSE = TransformColorInverse_SSE2;

  VP8LConvertBGRAToRGB_SSE = ConvertBGRAToRGB_SSE2;
  VP8LConvertBGRAToRGBA_SSE = ConvertBGRAToRGBA_SSE2;
}

#else

WEBP_DSP_INIT_STUB(VP8LDspInitSSE2)

#endif

#if defined(WEBP_USE_SSE41)
#include <emmintrin.h>
#include <smmintrin.h>

static void TransformColorInverse_SSE41(const VP8LMultipliers* const m,
                                        const uint32_t* const src,
                                        int num_pixels, uint32_t* dst) {

#define CST(X)  (((int16_t)(m->X << 8)) >> 5)
  const __m128i mults_rb =
      _mm_set1_epi32((int)((uint32_t)CST(green_to_red) << 16 |
                           (CST(green_to_blue) & 0xffff)));
  const __m128i mults_b2 = _mm_set1_epi32(CST(red_to_blue));
#undef CST
  const __m128i mask_ag = _mm_set1_epi32((int)0xff00ff00);
  const __m128i perm1 = _mm_setr_epi8(-1, 1, -1, 1, -1, 5, -1, 5,
                                      -1, 9, -1, 9, -1, 13, -1, 13);
  const __m128i perm2 = _mm_setr_epi8(-1, 2, -1, -1, -1, 6, -1, -1,
                                      -1, 10, -1, -1, -1, 14, -1, -1);
  int i;
  for (i = 0; i + 4 <= num_pixels; i += 4) {
    const __m128i A = _mm_loadu_si128((const __m128i*)(src + i));
    const __m128i B = _mm_shuffle_epi8(A, perm1);
    const __m128i C = _mm_mulhi_epi16(B, mults_rb);
    const __m128i D = _mm_add_epi8(A, C);
    const __m128i E = _mm_shuffle_epi8(D, perm2);
    const __m128i F = _mm_mulhi_epi16(E, mults_b2);
    const __m128i G = _mm_add_epi8(D, F);
    const __m128i out = _mm_blendv_epi8(G, A, mask_ag);
    _mm_storeu_si128((__m128i*)&dst[i], out);
  }

  if (i != num_pixels) {
    VP8LTransformColorInverse_C(m, src + i, num_pixels - i, dst + i);
  }
}

#define ARGB_TO_RGB_SSE41 do {                        \
  while (num_pixels >= 16) {                          \
    const __m128i in0 = _mm_loadu_si128(in + 0);      \
    const __m128i in1 = _mm_loadu_si128(in + 1);      \
    const __m128i in2 = _mm_loadu_si128(in + 2);      \
    const __m128i in3 = _mm_loadu_si128(in + 3);      \
    const __m128i a0 = _mm_shuffle_epi8(in0, perm0);  \
    const __m128i a1 = _mm_shuffle_epi8(in1, perm1);  \
    const __m128i a2 = _mm_shuffle_epi8(in2, perm2);  \
    const __m128i a3 = _mm_shuffle_epi8(in3, perm3);  \
    const __m128i b0 = _mm_blend_epi16(a0, a1, 0xc0); \
    const __m128i b1 = _mm_blend_epi16(a1, a2, 0xf0); \
    const __m128i b2 = _mm_blend_epi16(a2, a3, 0xfc); \
    _mm_storeu_si128(out + 0, b0);                    \
    _mm_storeu_si128(out + 1, b1);                    \
    _mm_storeu_si128(out + 2, b2);                    \
    in += 4;                                          \
    out += 3;                                         \
    num_pixels -= 16;                                 \
  }                                                   \
} while (0)

static void ConvertBGRAToRGB_SSE41(const uint32_t* WEBP_RESTRICT src,
                                   int num_pixels, uint8_t* WEBP_RESTRICT dst) {
  const __m128i* in = (const __m128i*)src;
  __m128i* out = (__m128i*)dst;
  const __m128i perm0 = _mm_setr_epi8(2, 1, 0, 6, 5, 4, 10, 9,
                                      8, 14, 13, 12, -1, -1, -1, -1);
  const __m128i perm1 = _mm_shuffle_epi32(perm0, 0x39);
  const __m128i perm2 = _mm_shuffle_epi32(perm0, 0x4e);
  const __m128i perm3 = _mm_shuffle_epi32(perm0, 0x93);

  ARGB_TO_RGB_SSE41;

  if (num_pixels > 0) {
    VP8LConvertBGRAToRGB_C((const uint32_t*)in, num_pixels, (uint8_t*)out);
  }
}

static void ConvertBGRAToBGR_SSE41(const uint32_t* WEBP_RESTRICT src,
                                   int num_pixels, uint8_t* WEBP_RESTRICT dst) {
  const __m128i* in = (const __m128i*)src;
  __m128i* out = (__m128i*)dst;
  const __m128i perm0 = _mm_setr_epi8(0, 1, 2, 4, 5, 6, 8, 9, 10,
                                      12, 13, 14, -1, -1, -1, -1);
  const __m128i perm1 = _mm_shuffle_epi32(perm0, 0x39);
  const __m128i perm2 = _mm_shuffle_epi32(perm0, 0x4e);
  const __m128i perm3 = _mm_shuffle_epi32(perm0, 0x93);

  ARGB_TO_RGB_SSE41;

  if (num_pixels > 0) {
    VP8LConvertBGRAToBGR_C((const uint32_t*)in, num_pixels, (uint8_t*)out);
  }
}

#undef ARGB_TO_RGB_SSE41

extern void VP8LDspInitSSE41(void);

WEBP_TSAN_IGNORE_FUNCTION void VP8LDspInitSSE41(void) {
  VP8LTransformColorInverse = TransformColorInverse_SSE41;
  VP8LConvertBGRAToRGB = ConvertBGRAToRGB_SSE41;
  VP8LConvertBGRAToBGR = ConvertBGRAToBGR_SSE41;

  VP8LTransformColorInverse_SSE = TransformColorInverse_SSE41;
  VP8LConvertBGRAToRGB_SSE = ConvertBGRAToRGB_SSE41;
}

#else

WEBP_DSP_INIT_STUB(VP8LDspInitSSE41)

#endif

#include <assert.h>
#include <stddef.h>

#define ROUNDER (WEBP_RESCALER_ONE >> 1)
#define MULT_FIX(x, y) (((uint64_t)(x) * (y) + ROUNDER) >> WEBP_RESCALER_RFIX)
#define MULT_FIX_FLOOR(x, y) (((uint64_t)(x) * (y)) >> WEBP_RESCALER_RFIX)

void WebPRescalerImportRowExpand_C(WebPRescaler* WEBP_RESTRICT const wrk,
                                   const uint8_t* WEBP_RESTRICT src) {
  const int x_stride = wrk->num_channels;
  const int x_out_max = wrk->dst_width * wrk->num_channels;
  int channel;
  assert(!WebPRescalerInputDone(wrk));
  assert(wrk->x_expand);
  for (channel = 0; channel < x_stride; ++channel) {
    int x_in = channel;
    int x_out = channel;

    int accum = wrk->x_add;
    rescaler_t left = (rescaler_t)src[x_in];
    rescaler_t right =
        (wrk->src_width > 1) ? (rescaler_t)src[x_in + x_stride] : left;
    x_in += x_stride;
    while (1) {
      wrk->frow[x_out] = right * wrk->x_add + (left - right) * accum;
      x_out += x_stride;
      if (x_out >= x_out_max) break;
      accum -= wrk->x_sub;
      if (accum < 0) {
        left = right;
        x_in += x_stride;
        assert(x_in < wrk->src_width * x_stride);
        right = (rescaler_t)src[x_in];
        accum += wrk->x_add;
      }
    }
    assert(wrk->x_sub == 0  || accum == 0);
  }
}

void WebPRescalerImportRowShrink_C(WebPRescaler* WEBP_RESTRICT const wrk,
                                   const uint8_t* WEBP_RESTRICT src) {
  const int x_stride = wrk->num_channels;
  const int x_out_max = wrk->dst_width * wrk->num_channels;
  int channel;
  assert(!WebPRescalerInputDone(wrk));
  assert(!wrk->x_expand);
  for (channel = 0; channel < x_stride; ++channel) {
    int x_in = channel;
    int x_out = channel;
    uint32_t sum = 0;
    int accum = 0;
    while (x_out < x_out_max) {
      uint32_t base = 0;
      accum += wrk->x_add;
      while (accum > 0) {
        accum -= wrk->x_sub;
        assert(x_in < wrk->src_width * x_stride);
        base = src[x_in];
        sum += base;
        x_in += x_stride;
      }
      {
        const rescaler_t frac = base * (-accum);
        wrk->frow[x_out] = sum * wrk->x_sub - frac;

        sum = (int)MULT_FIX(frac, wrk->fx_scale);
      }
      x_out += x_stride;
    }
    assert(accum == 0);
  }
}

void WebPRescalerExportRowExpand_C(WebPRescaler* const wrk) {
  int x_out;
  uint8_t* const dst = wrk->dst;
  rescaler_t* const irow = wrk->irow;
  const int x_out_max = wrk->dst_width * wrk->num_channels;
  const rescaler_t* const frow = wrk->frow;
  assert(!WebPRescalerOutputDone(wrk));
  assert(wrk->y_accum <= 0);
  assert(wrk->y_expand);
  assert(wrk->y_sub != 0);
  if (wrk->y_accum == 0) {
    for (x_out = 0; x_out < x_out_max; ++x_out) {
      const uint32_t J = frow[x_out];
      const int v = (int)MULT_FIX(J, wrk->fy_scale);
      dst[x_out] = (v > 255) ? 255u : (uint8_t)v;
    }
  } else {
    const uint32_t B = WEBP_RESCALER_FRAC(-wrk->y_accum, wrk->y_sub);
    const uint32_t A = (uint32_t)(WEBP_RESCALER_ONE - B);
    for (x_out = 0; x_out < x_out_max; ++x_out) {
      const uint64_t I = (uint64_t)A * frow[x_out]
                       + (uint64_t)B * irow[x_out];
      const uint32_t J = (uint32_t)((I + ROUNDER) >> WEBP_RESCALER_RFIX);
      const int v = (int)MULT_FIX(J, wrk->fy_scale);
      dst[x_out] = (v > 255) ? 255u : (uint8_t)v;
    }
  }
}

void WebPRescalerExportRowShrink_C(WebPRescaler* const wrk) {
  int x_out;
  uint8_t* const dst = wrk->dst;
  rescaler_t* const irow = wrk->irow;
  const int x_out_max = wrk->dst_width * wrk->num_channels;
  const rescaler_t* const frow = wrk->frow;
  const uint32_t yscale = wrk->fy_scale * (-wrk->y_accum);
  assert(!WebPRescalerOutputDone(wrk));
  assert(wrk->y_accum <= 0);
  assert(!wrk->y_expand);
  if (yscale) {
    for (x_out = 0; x_out < x_out_max; ++x_out) {
      const uint32_t frac = (uint32_t)MULT_FIX_FLOOR(frow[x_out], yscale);
      const int v = (int)MULT_FIX(irow[x_out] - frac, wrk->fxy_scale);
      dst[x_out] = (v > 255) ? 255u : (uint8_t)v;
      irow[x_out] = frac;
    }
  } else {
    for (x_out = 0; x_out < x_out_max; ++x_out) {
      const int v = (int)MULT_FIX(irow[x_out], wrk->fxy_scale);
      dst[x_out] = (v > 255) ? 255u : (uint8_t)v;
      irow[x_out] = 0;
    }
  }
}

#undef MULT_FIX_FLOOR
#undef MULT_FIX
#undef ROUNDER

void WebPRescalerImportRow(WebPRescaler* WEBP_RESTRICT const wrk,
                           const uint8_t* WEBP_RESTRICT src) {
  assert(!WebPRescalerInputDone(wrk));
  if (!wrk->x_expand) {
    WebPRescalerImportRowShrink(wrk, src);
  } else {
    WebPRescalerImportRowExpand(wrk, src);
  }
}

void WebPRescalerExportRow(WebPRescaler* const wrk) {
  if (wrk->y_accum <= 0) {
    assert(!WebPRescalerOutputDone(wrk));
    if (wrk->y_expand) {
      WebPRescalerExportRowExpand(wrk);
    } else if (wrk->fxy_scale) {
      WebPRescalerExportRowShrink(wrk);
    } else {
      int i;
      assert(wrk->src_height == wrk->dst_height && wrk->x_add == 1);
      assert(wrk->src_width == 1 && wrk->dst_width <= 2);
      for (i = 0; i < wrk->num_channels * wrk->dst_width; ++i) {
        wrk->dst[i] = wrk->irow[i];
        wrk->irow[i] = 0;
      }
    }
    wrk->y_accum += wrk->y_add;
    wrk->dst += wrk->dst_stride;
    ++wrk->dst_y;
  }
}

WebPRescalerImportRowFunc WebPRescalerImportRowExpand;
WebPRescalerImportRowFunc WebPRescalerImportRowShrink;

WebPRescalerExportRowFunc WebPRescalerExportRowExpand;
WebPRescalerExportRowFunc WebPRescalerExportRowShrink;

extern VP8CPUInfo VP8GetCPUInfo;
extern void WebPRescalerDspInitSSE2(void);
extern void WebPRescalerDspInitMIPS32(void);
extern void WebPRescalerDspInitMIPSdspR2(void);
extern void WebPRescalerDspInitMSA(void);
extern void WebPRescalerDspInitNEON(void);

WEBP_DSP_INIT_FUNC(WebPRescalerDspInit) {
#if !defined(WEBP_REDUCE_SIZE)
#if !WEBP_NEON_OMIT_C_CODE
  WebPRescalerExportRowExpand = WebPRescalerExportRowExpand_C;
  WebPRescalerExportRowShrink = WebPRescalerExportRowShrink_C;
#endif

  WebPRescalerImportRowExpand = WebPRescalerImportRowExpand_C;
  WebPRescalerImportRowShrink = WebPRescalerImportRowShrink_C;

  if (VP8GetCPUInfo != NULL) {
#if defined(WEBP_HAVE_SSE2)
    if (VP8GetCPUInfo(kSSE2)) {
      WebPRescalerDspInitSSE2();
    }
#endif
#if defined(WEBP_USE_MIPS32)
    if (VP8GetCPUInfo(kMIPS32)) {
      WebPRescalerDspInitMIPS32();
    }
#endif
#if defined(WEBP_USE_MIPS_DSP_R2)
    if (VP8GetCPUInfo(kMIPSdspR2)) {
      WebPRescalerDspInitMIPSdspR2();
    }
#endif
#if defined(WEBP_USE_MSA)
    if (VP8GetCPUInfo(kMSA)) {
      WebPRescalerDspInitMSA();
    }
#endif
  }

#if defined(WEBP_HAVE_NEON)
  if (WEBP_NEON_OMIT_C_CODE ||
      (VP8GetCPUInfo != NULL && VP8GetCPUInfo(kNEON))) {
    WebPRescalerDspInitNEON();
  }
#endif

  assert(WebPRescalerExportRowExpand != NULL);
  assert(WebPRescalerExportRowShrink != NULL);
  assert(WebPRescalerImportRowExpand != NULL);
  assert(WebPRescalerImportRowShrink != NULL);
#endif
}

#if defined(WEBP_USE_NEON) && !defined(WEBP_REDUCE_SIZE)

#include <arm_neon.h>
#include <assert.h>

#define ROUNDER (WEBP_RESCALER_ONE >> 1)
#define MULT_FIX_C(x, y) (((uint64_t)(x) * (y) + ROUNDER) >> WEBP_RESCALER_RFIX)
#define MULT_FIX_FLOOR_C(x, y) (((uint64_t)(x) * (y)) >> WEBP_RESCALER_RFIX)

#define LOAD_32x4(SRC, DST) const uint32x4_t DST = vld1q_u32((SRC))
#define LOAD_32x8(SRC, DST0, DST1)                                    \
    LOAD_32x4(SRC + 0, DST0);                                         \
    LOAD_32x4(SRC + 4, DST1)

#define STORE_32x8(SRC0, SRC1, DST) do {                              \
    vst1q_u32((DST) + 0, SRC0);                                       \
    vst1q_u32((DST) + 4, SRC1);                                       \
} while (0)

#if (WEBP_RESCALER_RFIX == 32)
#define MAKE_HALF_CST(C) vdupq_n_s32((int32_t)((C) >> 1))

#define MULT_FIX(A, B) \
    vreinterpretq_u32_s32(vqrdmulhq_s32(vreinterpretq_s32_u32((A)), (B)))
#define MULT_FIX_FLOOR(A, B) \
    vreinterpretq_u32_s32(vqdmulhq_s32(vreinterpretq_s32_u32((A)), (B)))
#else
#error "MULT_FIX/WEBP_RESCALER_RFIX need some more work"
#endif

static uint32x4_t Interpolate_NEON(const rescaler_t* WEBP_RESTRICT const frow,
                                   const rescaler_t* WEBP_RESTRICT const irow,
                                   uint32_t A, uint32_t B) {
  LOAD_32x4(frow, A0);
  LOAD_32x4(irow, B0);
  const uint64x2_t C0 = vmull_n_u32(vget_low_u32(A0), A);
  const uint64x2_t C1 = vmull_n_u32(vget_high_u32(A0), A);
  const uint64x2_t D0 = vmlal_n_u32(C0, vget_low_u32(B0), B);
  const uint64x2_t D1 = vmlal_n_u32(C1, vget_high_u32(B0), B);
  const uint32x4_t E = vcombine_u32(
      vrshrn_n_u64(D0, WEBP_RESCALER_RFIX),
      vrshrn_n_u64(D1, WEBP_RESCALER_RFIX));
  return E;
}

static void RescalerExportRowExpand_NEON(WebPRescaler* const wrk) {
  int x_out;
  uint8_t* const dst = wrk->dst;
  rescaler_t* const irow = wrk->irow;
  const int x_out_max = wrk->dst_width * wrk->num_channels;
  const int max_span = x_out_max & ~7;
  const rescaler_t* const frow = wrk->frow;
  const uint32_t fy_scale = wrk->fy_scale;
  const int32x4_t fy_scale_half = MAKE_HALF_CST(fy_scale);
  assert(!WebPRescalerOutputDone(wrk));
  assert(wrk->y_accum <= 0);
  assert(wrk->y_expand);
  assert(wrk->y_sub != 0);
  if (wrk->y_accum == 0) {
    for (x_out = 0; x_out < max_span; x_out += 8) {
      LOAD_32x4(frow + x_out + 0, A0);
      LOAD_32x4(frow + x_out + 4, A1);
      const uint32x4_t B0 = MULT_FIX(A0, fy_scale_half);
      const uint32x4_t B1 = MULT_FIX(A1, fy_scale_half);
      const uint16x4_t C0 = vmovn_u32(B0);
      const uint16x4_t C1 = vmovn_u32(B1);
      const uint8x8_t D = vqmovn_u16(vcombine_u16(C0, C1));
      vst1_u8(dst + x_out, D);
    }
    for (; x_out < x_out_max; ++x_out) {
      const uint32_t J = frow[x_out];
      const int v = (int)MULT_FIX_C(J, fy_scale);
      dst[x_out] = (v > 255) ? 255u : (uint8_t)v;
    }
  } else {
    const uint32_t B = WEBP_RESCALER_FRAC(-wrk->y_accum, wrk->y_sub);
    const uint32_t A = (uint32_t)(WEBP_RESCALER_ONE - B);
    for (x_out = 0; x_out < max_span; x_out += 8) {
      const uint32x4_t C0 =
          Interpolate_NEON(frow + x_out + 0, irow + x_out + 0, A, B);
      const uint32x4_t C1 =
          Interpolate_NEON(frow + x_out + 4, irow + x_out + 4, A, B);
      const uint32x4_t D0 = MULT_FIX(C0, fy_scale_half);
      const uint32x4_t D1 = MULT_FIX(C1, fy_scale_half);
      const uint16x4_t E0 = vmovn_u32(D0);
      const uint16x4_t E1 = vmovn_u32(D1);
      const uint8x8_t F = vqmovn_u16(vcombine_u16(E0, E1));
      vst1_u8(dst + x_out, F);
    }
    for (; x_out < x_out_max; ++x_out) {
      const uint64_t I = (uint64_t)A * frow[x_out]
                       + (uint64_t)B * irow[x_out];
      const uint32_t J = (uint32_t)((I + ROUNDER) >> WEBP_RESCALER_RFIX);
      const int v = (int)MULT_FIX_C(J, fy_scale);
      dst[x_out] = (v > 255) ? 255u : (uint8_t)v;
    }
  }
}

static void RescalerExportRowShrink_NEON(WebPRescaler* const wrk) {
  int x_out;
  uint8_t* const dst = wrk->dst;
  rescaler_t* const irow = wrk->irow;
  const int x_out_max = wrk->dst_width * wrk->num_channels;
  const int max_span = x_out_max & ~7;
  const rescaler_t* const frow = wrk->frow;
  const uint32_t yscale = wrk->fy_scale * (-wrk->y_accum);
  const uint32_t fxy_scale = wrk->fxy_scale;
  const uint32x4_t zero = vdupq_n_u32(0);
  const int32x4_t yscale_half = MAKE_HALF_CST(yscale);
  const int32x4_t fxy_scale_half = MAKE_HALF_CST(fxy_scale);
  assert(!WebPRescalerOutputDone(wrk));
  assert(wrk->y_accum <= 0);
  assert(!wrk->y_expand);
  if (yscale) {
    for (x_out = 0; x_out < max_span; x_out += 8) {
      LOAD_32x8(frow + x_out, in0, in1);
      LOAD_32x8(irow + x_out, in2, in3);
      const uint32x4_t A0 = MULT_FIX_FLOOR(in0, yscale_half);
      const uint32x4_t A1 = MULT_FIX_FLOOR(in1, yscale_half);
      const uint32x4_t B0 = vqsubq_u32(in2, A0);
      const uint32x4_t B1 = vqsubq_u32(in3, A1);
      const uint32x4_t C0 = MULT_FIX(B0, fxy_scale_half);
      const uint32x4_t C1 = MULT_FIX(B1, fxy_scale_half);
      const uint16x4_t D0 = vmovn_u32(C0);
      const uint16x4_t D1 = vmovn_u32(C1);
      const uint8x8_t E = vqmovn_u16(vcombine_u16(D0, D1));
      vst1_u8(dst + x_out, E);
      STORE_32x8(A0, A1, irow + x_out);
    }
    for (; x_out < x_out_max; ++x_out) {
      const uint32_t frac = (uint32_t)MULT_FIX_FLOOR_C(frow[x_out], yscale);
      const int v = (int)MULT_FIX_C(irow[x_out] - frac, fxy_scale);
      dst[x_out] = (v > 255) ? 255u : (uint8_t)v;
      irow[x_out] = frac;
    }
  } else {
    for (x_out = 0; x_out < max_span; x_out += 8) {
      LOAD_32x8(irow + x_out, in0, in1);
      const uint32x4_t A0 = MULT_FIX(in0, fxy_scale_half);
      const uint32x4_t A1 = MULT_FIX(in1, fxy_scale_half);
      const uint16x4_t B0 = vmovn_u32(A0);
      const uint16x4_t B1 = vmovn_u32(A1);
      const uint8x8_t C = vqmovn_u16(vcombine_u16(B0, B1));
      vst1_u8(dst + x_out, C);
      STORE_32x8(zero, zero, irow + x_out);
    }
    for (; x_out < x_out_max; ++x_out) {
      const int v = (int)MULT_FIX_C(irow[x_out], fxy_scale);
      dst[x_out] = (v > 255) ? 255u : (uint8_t)v;
      irow[x_out] = 0;
    }
  }
}

#undef MULT_FIX_FLOOR_C
#undef MULT_FIX_C
#undef MULT_FIX_FLOOR
#undef MULT_FIX
#undef ROUNDER

extern void WebPRescalerDspInitNEON(void);

WEBP_TSAN_IGNORE_FUNCTION void WebPRescalerDspInitNEON(void) {
  WebPRescalerExportRowExpand = RescalerExportRowExpand_NEON;
  WebPRescalerExportRowShrink = RescalerExportRowShrink_NEON;
}

#else

WEBP_DSP_INIT_STUB(WebPRescalerDspInitNEON)

#endif

#if defined(WEBP_USE_SSE2) && !defined(WEBP_REDUCE_SIZE)
#include <emmintrin.h>

#include <assert.h>
#include <stddef.h>

#define ROUNDER (WEBP_RESCALER_ONE >> 1)
#define MULT_FIX(x, y) (((uint64_t)(x) * (y) + ROUNDER) >> WEBP_RESCALER_RFIX)
#define MULT_FIX_FLOOR(x, y) (((uint64_t)(x) * (y)) >> WEBP_RESCALER_RFIX)

static void LoadTwoPixels_SSE2(const uint8_t* const src, __m128i* out) {
  const __m128i zero = _mm_setzero_si128();
  const __m128i A = _mm_loadl_epi64((const __m128i*)(src));
  const __m128i B = _mm_unpacklo_epi8(A, zero);
  const __m128i C = _mm_srli_si128(B, 8);
  *out = _mm_unpacklo_epi16(B, C);
}

static void LoadEightPixels_SSE2(const uint8_t* const src, __m128i* out) {
  const __m128i zero = _mm_setzero_si128();
  const __m128i A = _mm_loadl_epi64((const __m128i*)(src));
  *out = _mm_unpacklo_epi8(A, zero);
}

static void RescalerImportRowExpand_SSE2(WebPRescaler* WEBP_RESTRICT const wrk,
                                         const uint8_t* WEBP_RESTRICT src) {
  rescaler_t* frow = wrk->frow;
  const rescaler_t* const frow_end = frow + wrk->dst_width * wrk->num_channels;
  const int x_add = wrk->x_add;
  int accum = x_add;
  __m128i cur_pixels;

  if (wrk->src_width < 8 || accum >= (1 << 15)) {
    WebPRescalerImportRowExpand_C(wrk, src);
    return;
  }

  assert(!WebPRescalerInputDone(wrk));
  assert(wrk->x_expand);
  if (wrk->num_channels == 4) {
    LoadTwoPixels_SSE2(src, &cur_pixels);
    src += 4;
    while (1) {
      const __m128i mult = _mm_set1_epi32(((x_add - accum) << 16) | accum);
      const __m128i out = _mm_madd_epi16(cur_pixels, mult);
      _mm_storeu_si128((__m128i*)frow, out);
      frow += 4;
      if (frow >= frow_end) break;
      accum -= wrk->x_sub;
      if (accum < 0) {
        LoadTwoPixels_SSE2(src, &cur_pixels);
        src += 4;
        accum += x_add;
      }
    }
  } else {
    int left;
    const uint8_t* const src_limit = src + wrk->src_width - 8;
    LoadEightPixels_SSE2(src, &cur_pixels);
    src += 7;
    left = 7;
    while (1) {
      const __m128i mult = _mm_cvtsi32_si128(((x_add - accum) << 16) | accum);
      const __m128i out = _mm_madd_epi16(cur_pixels, mult);
      assert(sizeof(*frow) == sizeof(uint32_t));
      WebPInt32ToMem((uint8_t*)frow, _mm_cvtsi128_si32(out));
      frow += 1;
      if (frow >= frow_end) break;
      accum -= wrk->x_sub;
      if (accum < 0) {
        if (--left) {
          cur_pixels = _mm_srli_si128(cur_pixels, 2);
        } else if (src <= src_limit) {
          LoadEightPixels_SSE2(src, &cur_pixels);
          src += 7;
          left = 7;
        } else {
          cur_pixels = _mm_srli_si128(cur_pixels, 2);
          cur_pixels = _mm_insert_epi16(cur_pixels, src[1], 1);
          src += 1;
          left = 1;
        }
        accum += x_add;
      }
    }
  }
  assert(accum == 0);
}

static void RescalerImportRowShrink_SSE2(WebPRescaler* WEBP_RESTRICT const wrk,
                                         const uint8_t* WEBP_RESTRICT src) {
  const int x_sub = wrk->x_sub;
  int accum = 0;
  const __m128i zero = _mm_setzero_si128();
  const __m128i mult0 = _mm_set1_epi16(x_sub);
  const __m128i mult1 = _mm_set1_epi32(wrk->fx_scale);
  const __m128i rounder = _mm_set_epi32(0, ROUNDER, 0, ROUNDER);
  __m128i sum = zero;
  rescaler_t* frow = wrk->frow;
  const rescaler_t* const frow_end = wrk->frow + 4 * wrk->dst_width;

  if (wrk->num_channels != 4 || wrk->x_add > (x_sub << 7)) {
    WebPRescalerImportRowShrink_C(wrk, src);
    return;
  }
  assert(!WebPRescalerInputDone(wrk));
  assert(!wrk->x_expand);

  for (; frow < frow_end; frow += 4) {
    __m128i base = zero;
    accum += wrk->x_add;
    while (accum > 0) {
      const __m128i A = _mm_cvtsi32_si128(WebPMemToInt32(src));
      src += 4;
      base = _mm_unpacklo_epi8(A, zero);

      sum = _mm_add_epi16(sum, base);
      accum -= x_sub;
    }
    {
      const __m128i mult = _mm_set1_epi16(-accum);
      const __m128i frac0 = _mm_mullo_epi16(base, mult);
      const __m128i frac1 = _mm_mulhi_epu16(base, mult);
      const __m128i frac = _mm_unpacklo_epi16(frac0, frac1);
      const __m128i A0 = _mm_mullo_epi16(sum, mult0);
      const __m128i A1 = _mm_mulhi_epu16(sum, mult0);
      const __m128i B0 = _mm_unpacklo_epi16(A0, A1);
      const __m128i frow_out = _mm_sub_epi32(B0, frac);
      const __m128i D0 = _mm_srli_epi64(frac, 32);
      const __m128i D1 = _mm_mul_epu32(frac, mult1);
      const __m128i D2 = _mm_mul_epu32(D0, mult1);
      const __m128i E1 = _mm_add_epi64(D1, rounder);
      const __m128i E2 = _mm_add_epi64(D2, rounder);
      const __m128i F1 = _mm_shuffle_epi32(E1, 1 | (3 << 2));
      const __m128i F2 = _mm_shuffle_epi32(E2, 1 | (3 << 2));
      const __m128i G = _mm_unpacklo_epi32(F1, F2);
      sum = _mm_packs_epi32(G, zero);
      _mm_storeu_si128((__m128i*)frow, frow_out);
    }
  }
  assert(accum == 0);
}

static WEBP_INLINE void LoadDispatchAndMult_SSE2(
    const rescaler_t* WEBP_RESTRICT const src, const __m128i* const mult,
    __m128i* const out0, __m128i* const out1, __m128i* const out2,
    __m128i* const out3) {
  const __m128i A0 = _mm_loadu_si128((const __m128i*)(src + 0));
  const __m128i A1 = _mm_loadu_si128((const __m128i*)(src + 4));
  const __m128i A2 = _mm_srli_epi64(A0, 32);
  const __m128i A3 = _mm_srli_epi64(A1, 32);
  if (mult != NULL) {
    *out0 = _mm_mul_epu32(A0, *mult);
    *out1 = _mm_mul_epu32(A1, *mult);
    *out2 = _mm_mul_epu32(A2, *mult);
    *out3 = _mm_mul_epu32(A3, *mult);
  } else {
    *out0 = A0;
    *out1 = A1;
    *out2 = A2;
    *out3 = A3;
  }
}

static WEBP_INLINE void ProcessRow_SSE2(const __m128i* const A0,
                                        const __m128i* const A1,
                                        const __m128i* const A2,
                                        const __m128i* const A3,
                                        const __m128i* const mult,
                                        uint8_t* const dst) {
  const __m128i rounder = _mm_set_epi32(0, ROUNDER, 0, ROUNDER);
  const __m128i mask = _mm_set_epi32(~0, 0, ~0, 0);
  const __m128i B0 = _mm_mul_epu32(*A0, *mult);
  const __m128i B1 = _mm_mul_epu32(*A1, *mult);
  const __m128i B2 = _mm_mul_epu32(*A2, *mult);
  const __m128i B3 = _mm_mul_epu32(*A3, *mult);
  const __m128i C0 = _mm_add_epi64(B0, rounder);
  const __m128i C1 = _mm_add_epi64(B1, rounder);
  const __m128i C2 = _mm_add_epi64(B2, rounder);
  const __m128i C3 = _mm_add_epi64(B3, rounder);
  const __m128i D0 = _mm_srli_epi64(C0, WEBP_RESCALER_RFIX);
  const __m128i D1 = _mm_srli_epi64(C1, WEBP_RESCALER_RFIX);
#if (WEBP_RESCALER_RFIX < 32)
  const __m128i D2 =
      _mm_and_si128(_mm_slli_epi64(C2, 32 - WEBP_RESCALER_RFIX), mask);
  const __m128i D3 =
      _mm_and_si128(_mm_slli_epi64(C3, 32 - WEBP_RESCALER_RFIX), mask);
#else
  const __m128i D2 = _mm_and_si128(C2, mask);
  const __m128i D3 = _mm_and_si128(C3, mask);
#endif
  const __m128i E0 = _mm_or_si128(D0, D2);
  const __m128i E1 = _mm_or_si128(D1, D3);
  const __m128i F = _mm_packs_epi32(E0, E1);
  const __m128i G = _mm_packus_epi16(F, F);
  _mm_storel_epi64((__m128i*)dst, G);
}

static void RescalerExportRowExpand_SSE2(WebPRescaler* const wrk) {
  int x_out;
  uint8_t* const dst = wrk->dst;
  rescaler_t* const irow = wrk->irow;
  const int x_out_max = wrk->dst_width * wrk->num_channels;
  const rescaler_t* const frow = wrk->frow;
  const __m128i mult = _mm_set_epi32(0, wrk->fy_scale, 0, wrk->fy_scale);

  assert(!WebPRescalerOutputDone(wrk));
  assert(wrk->y_accum <= 0 && wrk->y_sub + wrk->y_accum >= 0);
  assert(wrk->y_expand);
  if (wrk->y_accum == 0) {
    for (x_out = 0; x_out + 8 <= x_out_max; x_out += 8) {
      __m128i A0, A1, A2, A3;
      LoadDispatchAndMult_SSE2(frow + x_out, NULL, &A0, &A1, &A2, &A3);
      ProcessRow_SSE2(&A0, &A1, &A2, &A3, &mult, dst + x_out);
    }
    for (; x_out < x_out_max; ++x_out) {
      const uint32_t J = frow[x_out];
      const int v = (int)MULT_FIX(J, wrk->fy_scale);
      dst[x_out] = (v > 255) ? 255u : (uint8_t)v;
    }
  } else {
    const uint32_t B = WEBP_RESCALER_FRAC(-wrk->y_accum, wrk->y_sub);
    const uint32_t A = (uint32_t)(WEBP_RESCALER_ONE - B);
    const __m128i mA = _mm_set_epi32(0, A, 0, A);
    const __m128i mB = _mm_set_epi32(0, B, 0, B);
    const __m128i rounder = _mm_set_epi32(0, ROUNDER, 0, ROUNDER);
    for (x_out = 0; x_out + 8 <= x_out_max; x_out += 8) {
      __m128i A0, A1, A2, A3, B0, B1, B2, B3;
      LoadDispatchAndMult_SSE2(frow + x_out, &mA, &A0, &A1, &A2, &A3);
      LoadDispatchAndMult_SSE2(irow + x_out, &mB, &B0, &B1, &B2, &B3);
      {
        const __m128i C0 = _mm_add_epi64(A0, B0);
        const __m128i C1 = _mm_add_epi64(A1, B1);
        const __m128i C2 = _mm_add_epi64(A2, B2);
        const __m128i C3 = _mm_add_epi64(A3, B3);
        const __m128i D0 = _mm_add_epi64(C0, rounder);
        const __m128i D1 = _mm_add_epi64(C1, rounder);
        const __m128i D2 = _mm_add_epi64(C2, rounder);
        const __m128i D3 = _mm_add_epi64(C3, rounder);
        const __m128i E0 = _mm_srli_epi64(D0, WEBP_RESCALER_RFIX);
        const __m128i E1 = _mm_srli_epi64(D1, WEBP_RESCALER_RFIX);
        const __m128i E2 = _mm_srli_epi64(D2, WEBP_RESCALER_RFIX);
        const __m128i E3 = _mm_srli_epi64(D3, WEBP_RESCALER_RFIX);
        ProcessRow_SSE2(&E0, &E1, &E2, &E3, &mult, dst + x_out);
      }
    }
    for (; x_out < x_out_max; ++x_out) {
      const uint64_t I = (uint64_t)A * frow[x_out]
                       + (uint64_t)B * irow[x_out];
      const uint32_t J = (uint32_t)((I + ROUNDER) >> WEBP_RESCALER_RFIX);
      const int v = (int)MULT_FIX(J, wrk->fy_scale);
      dst[x_out] = (v > 255) ? 255u : (uint8_t)v;
    }
  }
}

static void RescalerExportRowShrink_SSE2(WebPRescaler* const wrk) {
  int x_out;
  uint8_t* const dst = wrk->dst;
  rescaler_t* const irow = wrk->irow;
  const int x_out_max = wrk->dst_width * wrk->num_channels;
  const rescaler_t* const frow = wrk->frow;
  const uint32_t yscale = wrk->fy_scale * (-wrk->y_accum);
  assert(!WebPRescalerOutputDone(wrk));
  assert(wrk->y_accum <= 0);
  assert(!wrk->y_expand);
  if (yscale) {
    const int scale_xy = wrk->fxy_scale;
    const __m128i mult_xy = _mm_set_epi32(0, scale_xy, 0, scale_xy);
    const __m128i mult_y = _mm_set_epi32(0, yscale, 0, yscale);
    for (x_out = 0; x_out + 8 <= x_out_max; x_out += 8) {
      __m128i A0, A1, A2, A3, B0, B1, B2, B3;
      LoadDispatchAndMult_SSE2(irow + x_out, NULL, &A0, &A1, &A2, &A3);
      LoadDispatchAndMult_SSE2(frow + x_out, &mult_y, &B0, &B1, &B2, &B3);
      {
        const __m128i D0 = _mm_srli_epi64(B0, WEBP_RESCALER_RFIX);
        const __m128i D1 = _mm_srli_epi64(B1, WEBP_RESCALER_RFIX);
        const __m128i D2 = _mm_srli_epi64(B2, WEBP_RESCALER_RFIX);
        const __m128i D3 = _mm_srli_epi64(B3, WEBP_RESCALER_RFIX);
        const __m128i E0 = _mm_sub_epi64(A0, D0);
        const __m128i E1 = _mm_sub_epi64(A1, D1);
        const __m128i E2 = _mm_sub_epi64(A2, D2);
        const __m128i E3 = _mm_sub_epi64(A3, D3);
        const __m128i F2 = _mm_slli_epi64(D2, 32);
        const __m128i F3 = _mm_slli_epi64(D3, 32);
        const __m128i G0 = _mm_or_si128(D0, F2);
        const __m128i G1 = _mm_or_si128(D1, F3);
        _mm_storeu_si128((__m128i*)(irow + x_out + 0), G0);
        _mm_storeu_si128((__m128i*)(irow + x_out + 4), G1);
        ProcessRow_SSE2(&E0, &E1, &E2, &E3, &mult_xy, dst + x_out);
      }
    }
    for (; x_out < x_out_max; ++x_out) {
      const uint32_t frac = (int)MULT_FIX_FLOOR(frow[x_out], yscale);
      const int v = (int)MULT_FIX(irow[x_out] - frac, wrk->fxy_scale);
      dst[x_out] = (v > 255) ? 255u : (uint8_t)v;
      irow[x_out] = frac;
    }
  } else {
    const uint32_t scale = wrk->fxy_scale;
    const __m128i mult = _mm_set_epi32(0, scale, 0, scale);
    const __m128i zero = _mm_setzero_si128();
    for (x_out = 0; x_out + 8 <= x_out_max; x_out += 8) {
      __m128i A0, A1, A2, A3;
      LoadDispatchAndMult_SSE2(irow + x_out, NULL, &A0, &A1, &A2, &A3);
      _mm_storeu_si128((__m128i*)(irow + x_out + 0), zero);
      _mm_storeu_si128((__m128i*)(irow + x_out + 4), zero);
      ProcessRow_SSE2(&A0, &A1, &A2, &A3, &mult, dst + x_out);
    }
    for (; x_out < x_out_max; ++x_out) {
      const int v = (int)MULT_FIX(irow[x_out], scale);
      dst[x_out] = (v > 255) ? 255u : (uint8_t)v;
      irow[x_out] = 0;
    }
  }
}

#undef MULT_FIX_FLOOR
#undef MULT_FIX
#undef ROUNDER

extern void WebPRescalerDspInitSSE2(void);

WEBP_TSAN_IGNORE_FUNCTION void WebPRescalerDspInitSSE2(void) {
  WebPRescalerImportRowExpand = RescalerImportRowExpand_SSE2;
  WebPRescalerImportRowShrink = RescalerImportRowShrink_SSE2;
  WebPRescalerExportRowExpand = RescalerExportRowExpand_SSE2;
  WebPRescalerExportRowShrink = RescalerExportRowShrink_SSE2;
}

#else

WEBP_DSP_INIT_STUB(WebPRescalerDspInitSSE2)

#endif

#include <assert.h>
#include <stdlib.h>

#if !defined(WEBP_REDUCE_SIZE)

static const uint32_t kWeight[2 * VP8_SSIM_KERNEL + 1] = {
  1, 2, 3, 4, 3, 2, 1
};
static const uint32_t kWeightSum = 16 * 16;

static WEBP_INLINE double SSIMCalculation(
    const VP8DistoStats* const stats, uint32_t N  ) {
  const uint32_t w2 =  N * N;
  const uint32_t C1 = 20 * w2;
  const uint32_t C2 = 60 * w2;
  const uint32_t C3 = 8 * 8 * w2;
  const uint64_t xmxm = (uint64_t)stats->xm * stats->xm;
  const uint64_t ymym = (uint64_t)stats->ym * stats->ym;
  if (xmxm + ymym >= C3) {
    const int64_t xmym = (int64_t)stats->xm * stats->ym;
    const int64_t sxy = (int64_t)stats->xym * N - xmym;
    const uint64_t sxx = (uint64_t)stats->xxm * N - xmxm;
    const uint64_t syy = (uint64_t)stats->yym * N - ymym;

    const uint64_t num_S = (2 * (uint64_t)(sxy < 0 ? 0 : sxy) + C2) >> 8;
    const uint64_t den_S = (sxx + syy + C2) >> 8;
    const uint64_t fnum = (2 * xmym + C1) * num_S;
    const uint64_t fden = (xmxm + ymym + C1) * den_S;
    const double r = (double)fnum / fden;
    assert(r >= 0. && r <= 1.0);
    return r;
  }
  return 1.;
}

double VP8SSIMFromStats(const VP8DistoStats* const stats) {
  return SSIMCalculation(stats, kWeightSum);
}

double VP8SSIMFromStatsClipped(const VP8DistoStats* const stats) {
  return SSIMCalculation(stats, stats->w);
}

static double SSIMGetClipped_C(const uint8_t* src1, int stride1,
                               const uint8_t* src2, int stride2,
                               int xo, int yo, int W, int H) {
  VP8DistoStats stats = { 0, 0, 0, 0, 0, 0 };
  const int ymin = (yo - VP8_SSIM_KERNEL < 0) ? 0 : yo - VP8_SSIM_KERNEL;
  const int ymax = (yo + VP8_SSIM_KERNEL > H - 1) ? H - 1
                                                  : yo + VP8_SSIM_KERNEL;
  const int xmin = (xo - VP8_SSIM_KERNEL < 0) ? 0 : xo - VP8_SSIM_KERNEL;
  const int xmax = (xo + VP8_SSIM_KERNEL > W - 1) ? W - 1
                                                  : xo + VP8_SSIM_KERNEL;
  int x, y;
  src1 += ymin * stride1;
  src2 += ymin * stride2;
  for (y = ymin; y <= ymax; ++y, src1 += stride1, src2 += stride2) {
    for (x = xmin; x <= xmax; ++x) {
      const uint32_t w = kWeight[VP8_SSIM_KERNEL + x - xo]
                       * kWeight[VP8_SSIM_KERNEL + y - yo];
      const uint32_t s1 = src1[x];
      const uint32_t s2 = src2[x];
      stats.w   += w;
      stats.xm  += w * s1;
      stats.ym  += w * s2;
      stats.xxm += w * s1 * s1;
      stats.xym += w * s1 * s2;
      stats.yym += w * s2 * s2;
    }
  }
  return VP8SSIMFromStatsClipped(&stats);
}

static double SSIMGet_C(const uint8_t* src1, int stride1,
                        const uint8_t* src2, int stride2) {
  VP8DistoStats stats = { 0, 0, 0, 0, 0, 0 };
  int x, y;
  for (y = 0; y <= 2 * VP8_SSIM_KERNEL; ++y, src1 += stride1, src2 += stride2) {
    for (x = 0; x <= 2 * VP8_SSIM_KERNEL; ++x) {
      const uint32_t w = kWeight[x] * kWeight[y];
      const uint32_t s1 = src1[x];
      const uint32_t s2 = src2[x];
      stats.xm  += w * s1;
      stats.ym  += w * s2;
      stats.xxm += w * s1 * s1;
      stats.xym += w * s1 * s2;
      stats.yym += w * s2 * s2;
    }
  }
  return VP8SSIMFromStats(&stats);
}

#endif

#if !defined(WEBP_DISABLE_STATS)
static uint32_t AccumulateSSE_C(const uint8_t* src1,
                                const uint8_t* src2, int len) {
  int i;
  uint32_t sse2 = 0;
  assert(len <= 65535);
  for (i = 0; i < len; ++i) {
    const int32_t diff = src1[i] - src2[i];
    sse2 += diff * diff;
  }
  return sse2;
}
#endif

#if !defined(WEBP_REDUCE_SIZE)
VP8SSIMGetFunc VP8SSIMGet;
VP8SSIMGetClippedFunc VP8SSIMGetClipped;
#endif
#if !defined(WEBP_DISABLE_STATS)
VP8AccumulateSSEFunc VP8AccumulateSSE;
#endif

extern VP8CPUInfo VP8GetCPUInfo;
extern void VP8SSIMDspInitSSE2(void);

WEBP_DSP_INIT_FUNC(VP8SSIMDspInit) {
#if !defined(WEBP_REDUCE_SIZE)
  VP8SSIMGetClipped = SSIMGetClipped_C;
  VP8SSIMGet = SSIMGet_C;
#endif

#if !defined(WEBP_DISABLE_STATS)
  VP8AccumulateSSE = AccumulateSSE_C;
#endif

  if (VP8GetCPUInfo != NULL) {
#if defined(WEBP_HAVE_SSE2)
    if (VP8GetCPUInfo(kSSE2)) {
      VP8SSIMDspInitSSE2();
    }
#endif
  }
}

#define kWeight ssim_sse2_kWeight
#if defined(WEBP_USE_SSE2)
#include <emmintrin.h>

#include <assert.h>

#if !defined(WEBP_DISABLE_STATS)

static WEBP_INLINE void SubtractAndSquare_SSE2(const __m128i a, const __m128i b,
                                               __m128i* const sum) {

  const __m128i a_b = _mm_subs_epu8(a, b);
  const __m128i b_a = _mm_subs_epu8(b, a);
  const __m128i abs_a_b = _mm_or_si128(a_b, b_a);

  const __m128i zero = _mm_setzero_si128();
  const __m128i C0 = _mm_unpacklo_epi8(abs_a_b, zero);
  const __m128i C1 = _mm_unpackhi_epi8(abs_a_b, zero);

  const __m128i sum1 = _mm_madd_epi16(C0, C0);
  const __m128i sum2 = _mm_madd_epi16(C1, C1);
  *sum = _mm_add_epi32(sum1, sum2);
}

static uint32_t AccumulateSSE_SSE2(const uint8_t* src1,
                                   const uint8_t* src2, int len) {
  int i = 0;
  uint32_t sse2 = 0;
  if (len >= 16) {
    const int limit = len - 32;
    int32_t tmp[4];
    __m128i sum1;
    __m128i sum = _mm_setzero_si128();
    __m128i a0 = _mm_loadu_si128((const __m128i*)&src1[i]);
    __m128i b0 = _mm_loadu_si128((const __m128i*)&src2[i]);
    i += 16;
    while (i <= limit) {
      const __m128i a1 = _mm_loadu_si128((const __m128i*)&src1[i]);
      const __m128i b1 = _mm_loadu_si128((const __m128i*)&src2[i]);
      __m128i sum2;
      i += 16;
      SubtractAndSquare_SSE2(a0, b0, &sum1);
      sum = _mm_add_epi32(sum, sum1);
      a0 = _mm_loadu_si128((const __m128i*)&src1[i]);
      b0 = _mm_loadu_si128((const __m128i*)&src2[i]);
      i += 16;
      SubtractAndSquare_SSE2(a1, b1, &sum2);
      sum = _mm_add_epi32(sum, sum2);
    }
    SubtractAndSquare_SSE2(a0, b0, &sum1);
    sum = _mm_add_epi32(sum, sum1);
    _mm_storeu_si128((__m128i*)tmp, sum);
    sse2 += (tmp[3] + tmp[2] + tmp[1] + tmp[0]);
  }

  for (; i < len; ++i) {
    const int32_t diff = src1[i] - src2[i];
    sse2 += diff * diff;
  }
  return sse2;
}
#endif

#if !defined(WEBP_REDUCE_SIZE)

static uint32_t HorizontalAdd16b_SSE2(const __m128i* const m) {
  uint16_t tmp[8];
  const __m128i a = _mm_srli_si128(*m, 8);
  const __m128i b = _mm_add_epi16(*m, a);
  _mm_storeu_si128((__m128i*)tmp, b);
  return (uint32_t)tmp[3] + tmp[2] + tmp[1] + tmp[0];
}

static uint32_t HorizontalAdd32b_SSE2(const __m128i* const m) {
  const __m128i a = _mm_srli_si128(*m, 8);
  const __m128i b = _mm_add_epi32(*m, a);
  const __m128i c = _mm_add_epi32(b, _mm_srli_si128(b, 4));
  return (uint32_t)_mm_cvtsi128_si32(c);
}

static const uint16_t kWeight[] = { 1, 2, 3, 4, 3, 2, 1, 0 };

#define ACCUMULATE_ROW(WEIGHT) do {                         \
                          \
  const __m128i Wy = _mm_set1_epi16((WEIGHT));              \
  const __m128i W = _mm_mullo_epi16(Wx, Wy);                \
         \
  const __m128i a0 = _mm_loadl_epi64((const __m128i*)src1); \
  const __m128i b0 = _mm_loadl_epi64((const __m128i*)src2); \
                 \
  const __m128i a1 = _mm_unpacklo_epi8(a0, zero);           \
  const __m128i b1 = _mm_unpacklo_epi8(b0, zero);           \
  const __m128i wa1 = _mm_mullo_epi16(a1, W);               \
  const __m128i wb1 = _mm_mullo_epi16(b1, W);               \
                                            \
  xm  = _mm_add_epi16(xm, wa1);                             \
  ym  = _mm_add_epi16(ym, wb1);                             \
  xxm = _mm_add_epi32(xxm, _mm_madd_epi16(a1, wa1));        \
  xym = _mm_add_epi32(xym, _mm_madd_epi16(a1, wb1));        \
  yym = _mm_add_epi32(yym, _mm_madd_epi16(b1, wb1));        \
  src1 += stride1;                                          \
  src2 += stride2;                                          \
} while (0)

static double SSIMGet_SSE2(const uint8_t* src1, int stride1,
                           const uint8_t* src2, int stride2) {
  VP8DistoStats stats;
  const __m128i zero = _mm_setzero_si128();
  __m128i xm = zero, ym = zero;
  __m128i xxm = zero, yym = zero, xym = zero;
  const __m128i Wx = _mm_loadu_si128((const __m128i*)kWeight);
  assert(2 * VP8_SSIM_KERNEL + 1 == 7);
  ACCUMULATE_ROW(1);
  ACCUMULATE_ROW(2);
  ACCUMULATE_ROW(3);
  ACCUMULATE_ROW(4);
  ACCUMULATE_ROW(3);
  ACCUMULATE_ROW(2);
  ACCUMULATE_ROW(1);
  stats.xm  = HorizontalAdd16b_SSE2(&xm);
  stats.ym  = HorizontalAdd16b_SSE2(&ym);
  stats.xxm = HorizontalAdd32b_SSE2(&xxm);
  stats.xym = HorizontalAdd32b_SSE2(&xym);
  stats.yym = HorizontalAdd32b_SSE2(&yym);
  return VP8SSIMFromStats(&stats);
}

#endif

extern void VP8SSIMDspInitSSE2(void);

WEBP_TSAN_IGNORE_FUNCTION void VP8SSIMDspInitSSE2(void) {
#if !defined(WEBP_DISABLE_STATS)
  VP8AccumulateSSE = AccumulateSSE_SSE2;
#endif
#if !defined(WEBP_REDUCE_SIZE)
  VP8SSIMGet = SSIMGet_SSE2;
#endif
}

#else

WEBP_DSP_INIT_STUB(VP8SSIMDspInitSSE2)

#endif

#undef kWeight

#include <assert.h>
#include <stddef.h>

#ifdef FANCY_UPSAMPLING

WebPUpsampleLinePairFunc WebPUpsamplers[MODE_LAST];

#define LOAD_UV(u, v) ((u) | ((v) << 16))

#define UPSAMPLE_FUNC(FUNC_NAME, FUNC, XSTEP)                                  \
static void FUNC_NAME(const uint8_t* WEBP_RESTRICT top_y,                      \
                      const uint8_t* WEBP_RESTRICT bottom_y,                   \
                      const uint8_t* WEBP_RESTRICT top_u,                      \
                      const uint8_t* WEBP_RESTRICT top_v,                      \
                      const uint8_t* WEBP_RESTRICT cur_u,                      \
                      const uint8_t* WEBP_RESTRICT cur_v,                      \
                      uint8_t* WEBP_RESTRICT top_dst,                          \
                      uint8_t* WEBP_RESTRICT bottom_dst, int len) {            \
  int x;                                                                       \
  const int last_pixel_pair = (len - 1) >> 1;                                  \
  uint32_t tl_uv = LOAD_UV(top_u[0], top_v[0]);           \
  uint32_t l_uv  = LOAD_UV(cur_u[0], cur_v[0]);               \
  assert(top_y != NULL);                                                       \
  {                                                                            \
    const uint32_t uv0 = (3 * tl_uv + l_uv + 0x00020002u) >> 2;                \
    FUNC(top_y[0], uv0 & 0xff, (uv0 >> 16), top_dst);                          \
  }                                                                            \
  if (bottom_y != NULL) {                                                      \
    const uint32_t uv0 = (3 * l_uv + tl_uv + 0x00020002u) >> 2;                \
    FUNC(bottom_y[0], uv0 & 0xff, (uv0 >> 16), bottom_dst);                    \
  }                                                                            \
  for (x = 1; x <= last_pixel_pair; ++x) {                                     \
    const uint32_t t_uv = LOAD_UV(top_u[x], top_v[x]);         \
    const uint32_t uv   = LOAD_UV(cur_u[x], cur_v[x]);             \
    \
    const uint32_t avg = tl_uv + t_uv + l_uv + uv + 0x00080008u;               \
    const uint32_t diag_12 = (avg + 2 * (t_uv + l_uv)) >> 3;                   \
    const uint32_t diag_03 = (avg + 2 * (tl_uv + uv)) >> 3;                    \
    {                                                                          \
      const uint32_t uv0 = (diag_12 + tl_uv) >> 1;                             \
      const uint32_t uv1 = (diag_03 + t_uv) >> 1;                              \
      FUNC(top_y[2 * x - 1], uv0 & 0xff, (uv0 >> 16),                          \
           top_dst + (2 * x - 1) * (XSTEP));                                   \
      FUNC(top_y[2 * x - 0], uv1 & 0xff, (uv1 >> 16),                          \
           top_dst + (2 * x - 0) * (XSTEP));                                   \
    }                                                                          \
    if (bottom_y != NULL) {                                                    \
      const uint32_t uv0 = (diag_03 + l_uv) >> 1;                              \
      const uint32_t uv1 = (diag_12 + uv) >> 1;                                \
      FUNC(bottom_y[2 * x - 1], uv0 & 0xff, (uv0 >> 16),                       \
           bottom_dst + (2 * x - 1) * (XSTEP));                                \
      FUNC(bottom_y[2 * x + 0], uv1 & 0xff, (uv1 >> 16),                       \
           bottom_dst + (2 * x + 0) * (XSTEP));                                \
    }                                                                          \
    tl_uv = t_uv;                                                              \
    l_uv = uv;                                                                 \
  }                                                                            \
  if (!(len & 1)) {                                                            \
    {                                                                          \
      const uint32_t uv0 = (3 * tl_uv + l_uv + 0x00020002u) >> 2;              \
      FUNC(top_y[len - 1], uv0 & 0xff, (uv0 >> 16),                            \
           top_dst + (len - 1) * (XSTEP));                                     \
    }                                                                          \
    if (bottom_y != NULL) {                                                    \
      const uint32_t uv0 = (3 * l_uv + tl_uv + 0x00020002u) >> 2;              \
      FUNC(bottom_y[len - 1], uv0 & 0xff, (uv0 >> 16),                         \
           bottom_dst + (len - 1) * (XSTEP));                                  \
    }                                                                          \
  }                                                                            \
}

#if !WEBP_NEON_OMIT_C_CODE
UPSAMPLE_FUNC(UpsampleRgbaLinePair_C, VP8YuvToRgba, 4)
UPSAMPLE_FUNC(UpsampleBgraLinePair_C, VP8YuvToBgra, 4)
#if !defined(WEBP_REDUCE_CSP)
UPSAMPLE_FUNC(UpsampleArgbLinePair_C, VP8YuvToArgb, 4)
UPSAMPLE_FUNC(UpsampleRgbLinePair_C,  VP8YuvToRgb,  3)
UPSAMPLE_FUNC(UpsampleBgrLinePair_C,  VP8YuvToBgr,  3)
UPSAMPLE_FUNC(UpsampleRgba4444LinePair_C, VP8YuvToRgba4444, 2)
UPSAMPLE_FUNC(UpsampleRgb565LinePair_C,  VP8YuvToRgb565,  2)
#else
static void EmptyUpsampleFunc(const uint8_t* top_y, const uint8_t* bottom_y,
                              const uint8_t* top_u, const uint8_t* top_v,
                              const uint8_t* cur_u, const uint8_t* cur_v,
                              uint8_t* top_dst, uint8_t* bottom_dst, int len) {
  (void)top_y;
  (void)bottom_y;
  (void)top_u;
  (void)top_v;
  (void)cur_u;
  (void)cur_v;
  (void)top_dst;
  (void)bottom_dst;
  (void)len;
  assert(0);
}
#define UpsampleArgbLinePair_C EmptyUpsampleFunc
#define UpsampleRgbLinePair_C EmptyUpsampleFunc
#define UpsampleBgrLinePair_C EmptyUpsampleFunc
#define UpsampleRgba4444LinePair_C EmptyUpsampleFunc
#define UpsampleRgb565LinePair_C EmptyUpsampleFunc
#endif

#endif

#undef LOAD_UV
#undef UPSAMPLE_FUNC

#endif

#if !defined(FANCY_UPSAMPLING)
#define DUAL_SAMPLE_FUNC(FUNC_NAME, FUNC)                                      \
static void FUNC_NAME(const uint8_t* WEBP_RESTRICT top_y,                      \
                      const uint8_t* WEBP_RESTRICT bot_y,                      \
                      const uint8_t* WEBP_RESTRICT top_u,                      \
                      const uint8_t* WEBP_RESTRICT top_v,                      \
                      const uint8_t* WEBP_RESTRICT bot_u,                      \
                      const uint8_t* WEBP_RESTRICT bot_v,                      \
                      uint8_t* WEBP_RESTRICT top_dst,                          \
                      uint8_t* WEBP_RESTRICT bot_dst, int len) {               \
  const int half_len = len >> 1;                                               \
  int x;                                                                       \
  assert(top_dst != NULL);                                                     \
  {                                                                            \
    for (x = 0; x < half_len; ++x) {                                           \
      FUNC(top_y[2 * x + 0], top_u[x], top_v[x], top_dst + 8 * x + 0);         \
      FUNC(top_y[2 * x + 1], top_u[x], top_v[x], top_dst + 8 * x + 4);         \
    }                                                                          \
    if (len & 1) FUNC(top_y[2 * x + 0], top_u[x], top_v[x], top_dst + 8 * x);  \
  }                                                                            \
  if (bot_dst != NULL) {                                                       \
    for (x = 0; x < half_len; ++x) {                                           \
      FUNC(bot_y[2 * x + 0], bot_u[x], bot_v[x], bot_dst + 8 * x + 0);         \
      FUNC(bot_y[2 * x + 1], bot_u[x], bot_v[x], bot_dst + 8 * x + 4);         \
    }                                                                          \
    if (len & 1) FUNC(bot_y[2 * x + 0], bot_u[x], bot_v[x], bot_dst + 8 * x);  \
  }                                                                            \
}

DUAL_SAMPLE_FUNC(DualLineSamplerBGRA, VP8YuvToBgra)
DUAL_SAMPLE_FUNC(DualLineSamplerARGB, VP8YuvToArgb)
#undef DUAL_SAMPLE_FUNC

#endif

WebPUpsampleLinePairFunc WebPGetLinePairConverter(int alpha_is_last) {
  WebPInitUpsamplers();
#ifdef FANCY_UPSAMPLING
  return WebPUpsamplers[alpha_is_last ? MODE_BGRA : MODE_ARGB];
#else
  return (alpha_is_last ? DualLineSamplerBGRA : DualLineSamplerARGB);
#endif
}

#define YUV444_FUNC(FUNC_NAME, FUNC, XSTEP)                                    \
extern void FUNC_NAME(const uint8_t* WEBP_RESTRICT y,                          \
                      const uint8_t* WEBP_RESTRICT u,                          \
                      const uint8_t* WEBP_RESTRICT v,                          \
                      uint8_t* WEBP_RESTRICT dst, int len);                    \
void FUNC_NAME(const uint8_t* WEBP_RESTRICT y,                                 \
               const uint8_t* WEBP_RESTRICT u,                                 \
               const uint8_t* WEBP_RESTRICT v,                                 \
               uint8_t* WEBP_RESTRICT dst, int len) {                          \
  int i;                                                                       \
  for (i = 0; i < len; ++i) FUNC(y[i], u[i], v[i], &dst[i * (XSTEP)]);         \
}

YUV444_FUNC(WebPYuv444ToRgba_C,     VP8YuvToRgba, 4)
YUV444_FUNC(WebPYuv444ToBgra_C,     VP8YuvToBgra, 4)
#if !defined(WEBP_REDUCE_CSP)
YUV444_FUNC(WebPYuv444ToRgb_C,      VP8YuvToRgb,  3)
YUV444_FUNC(WebPYuv444ToBgr_C,      VP8YuvToBgr,  3)
YUV444_FUNC(WebPYuv444ToArgb_C,     VP8YuvToArgb, 4)
YUV444_FUNC(WebPYuv444ToRgba4444_C, VP8YuvToRgba4444, 2)
YUV444_FUNC(WebPYuv444ToRgb565_C,   VP8YuvToRgb565, 2)
#else
static void EmptyYuv444Func(const uint8_t* y,
                            const uint8_t* u, const uint8_t* v,
                            uint8_t* dst, int len) {
  (void)y;
  (void)u;
  (void)v;
  (void)dst;
  (void)len;
}
#define WebPYuv444ToRgb_C EmptyYuv444Func
#define WebPYuv444ToBgr_C EmptyYuv444Func
#define WebPYuv444ToArgb_C EmptyYuv444Func
#define WebPYuv444ToRgba4444_C EmptyYuv444Func
#define WebPYuv444ToRgb565_C EmptyYuv444Func
#endif

#undef YUV444_FUNC

WebPYUV444Converter WebPYUV444Converters[MODE_LAST];

extern VP8CPUInfo VP8GetCPUInfo;
extern void WebPInitYUV444ConvertersMIPSdspR2(void);
extern void WebPInitYUV444ConvertersSSE2(void);
extern void WebPInitYUV444ConvertersSSE41(void);

WEBP_DSP_INIT_FUNC(WebPInitYUV444Converters) {
  WebPYUV444Converters[MODE_RGBA]      = WebPYuv444ToRgba_C;
  WebPYUV444Converters[MODE_BGRA]      = WebPYuv444ToBgra_C;
  WebPYUV444Converters[MODE_RGB]       = WebPYuv444ToRgb_C;
  WebPYUV444Converters[MODE_BGR]       = WebPYuv444ToBgr_C;
  WebPYUV444Converters[MODE_ARGB]      = WebPYuv444ToArgb_C;
  WebPYUV444Converters[MODE_RGBA_4444] = WebPYuv444ToRgba4444_C;
  WebPYUV444Converters[MODE_RGB_565]   = WebPYuv444ToRgb565_C;
  WebPYUV444Converters[MODE_rgbA]      = WebPYuv444ToRgba_C;
  WebPYUV444Converters[MODE_bgrA]      = WebPYuv444ToBgra_C;
  WebPYUV444Converters[MODE_Argb]      = WebPYuv444ToArgb_C;
  WebPYUV444Converters[MODE_rgbA_4444] = WebPYuv444ToRgba4444_C;

  if (VP8GetCPUInfo != NULL) {
#if defined(WEBP_HAVE_SSE2)
    if (VP8GetCPUInfo(kSSE2)) {
      WebPInitYUV444ConvertersSSE2();
    }
#endif
#if defined(WEBP_HAVE_SSE41)
    if (VP8GetCPUInfo(kSSE4_1)) {
      WebPInitYUV444ConvertersSSE41();
    }
#endif
#if defined(WEBP_USE_MIPS_DSP_R2)
    if (VP8GetCPUInfo(kMIPSdspR2)) {
      WebPInitYUV444ConvertersMIPSdspR2();
    }
#endif
  }
}

extern void WebPInitUpsamplersSSE2(void);
extern void WebPInitUpsamplersSSE41(void);
extern void WebPInitUpsamplersNEON(void);
extern void WebPInitUpsamplersMIPSdspR2(void);
extern void WebPInitUpsamplersMSA(void);

WEBP_DSP_INIT_FUNC(WebPInitUpsamplers) {
#ifdef FANCY_UPSAMPLING
#if !WEBP_NEON_OMIT_C_CODE
  WebPUpsamplers[MODE_RGBA]      = UpsampleRgbaLinePair_C;
  WebPUpsamplers[MODE_BGRA]      = UpsampleBgraLinePair_C;
  WebPUpsamplers[MODE_rgbA]      = UpsampleRgbaLinePair_C;
  WebPUpsamplers[MODE_bgrA]      = UpsampleBgraLinePair_C;
  WebPUpsamplers[MODE_RGB]       = UpsampleRgbLinePair_C;
  WebPUpsamplers[MODE_BGR]       = UpsampleBgrLinePair_C;
  WebPUpsamplers[MODE_ARGB]      = UpsampleArgbLinePair_C;
  WebPUpsamplers[MODE_RGBA_4444] = UpsampleRgba4444LinePair_C;
  WebPUpsamplers[MODE_RGB_565]   = UpsampleRgb565LinePair_C;
  WebPUpsamplers[MODE_Argb]      = UpsampleArgbLinePair_C;
  WebPUpsamplers[MODE_rgbA_4444] = UpsampleRgba4444LinePair_C;
#endif

  if (VP8GetCPUInfo != NULL) {
#if defined(WEBP_HAVE_SSE2)
    if (VP8GetCPUInfo(kSSE2)) {
      WebPInitUpsamplersSSE2();
    }
#endif
#if defined(WEBP_HAVE_SSE41)
    if (VP8GetCPUInfo(kSSE4_1)) {
      WebPInitUpsamplersSSE41();
    }
#endif
#if defined(WEBP_USE_MIPS_DSP_R2)
    if (VP8GetCPUInfo(kMIPSdspR2)) {
      WebPInitUpsamplersMIPSdspR2();
    }
#endif
#if defined(WEBP_USE_MSA)
    if (VP8GetCPUInfo(kMSA)) {
      WebPInitUpsamplersMSA();
    }
#endif
  }

#if defined(WEBP_HAVE_NEON)
  if (WEBP_NEON_OMIT_C_CODE ||
      (VP8GetCPUInfo != NULL && VP8GetCPUInfo(kNEON))) {
    WebPInitUpsamplersNEON();
  }
#endif

  assert(WebPUpsamplers[MODE_RGBA] != NULL);
  assert(WebPUpsamplers[MODE_BGRA] != NULL);
  assert(WebPUpsamplers[MODE_rgbA] != NULL);
  assert(WebPUpsamplers[MODE_bgrA] != NULL);
#if !defined(WEBP_REDUCE_CSP) || !WEBP_NEON_OMIT_C_CODE
  assert(WebPUpsamplers[MODE_RGB] != NULL);
  assert(WebPUpsamplers[MODE_BGR] != NULL);
  assert(WebPUpsamplers[MODE_ARGB] != NULL);
  assert(WebPUpsamplers[MODE_RGBA_4444] != NULL);
  assert(WebPUpsamplers[MODE_RGB_565] != NULL);
  assert(WebPUpsamplers[MODE_Argb] != NULL);
  assert(WebPUpsamplers[MODE_rgbA_4444] != NULL);
#endif

#endif
}

#if defined(WEBP_USE_NEON)

#include <assert.h>
#include <arm_neon.h>
#include <string.h>

#ifdef FANCY_UPSAMPLING

#define UPSAMPLE_16PIXELS(r1, r2, out) do {                             \
  const uint8x8_t a = vld1_u8(r1 + 0);                                  \
  const uint8x8_t b = vld1_u8(r1 + 1);                                  \
  const uint8x8_t c = vld1_u8(r2 + 0);                                  \
  const uint8x8_t d = vld1_u8(r2 + 1);                                  \
                                                     \
  const uint16x8_t ad = vaddl_u8(a,  d);                                \
  const uint16x8_t bc = vaddl_u8(b,  c);                                \
  const uint16x8_t abcd = vaddq_u16(ad, bc);                            \
                                                 \
  const uint16x8_t al = vaddq_u16(abcd, vshlq_n_u16(ad, 1));            \
                                                 \
  const uint16x8_t bl = vaddq_u16(abcd, vshlq_n_u16(bc, 1));            \
                                                                        \
  const uint8x8_t diag2 = vshrn_n_u16(al, 3);                           \
  const uint8x8_t diag1 = vshrn_n_u16(bl, 3);                           \
                                                                        \
  const uint8x8_t A = vrhadd_u8(a, diag1);                              \
  const uint8x8_t B = vrhadd_u8(b, diag2);                              \
  const uint8x8_t C = vrhadd_u8(c, diag2);                              \
  const uint8x8_t D = vrhadd_u8(d, diag1);                              \
                                                                        \
  uint8x8x2_t A_B, C_D;                                                 \
  INIT_VECTOR2(A_B, A, B);                                              \
  INIT_VECTOR2(C_D, C, D);                                              \
  vst2_u8(out +  0, A_B);                                               \
  vst2_u8(out + 32, C_D);                                               \
} while (0)

static void Upsample16Pixels_NEON(const uint8_t* WEBP_RESTRICT const r1,
                                  const uint8_t* WEBP_RESTRICT const r2,
                                  uint8_t* WEBP_RESTRICT const out) {
  UPSAMPLE_16PIXELS(r1, r2, out);
}

#define UPSAMPLE_LAST_BLOCK(tb, bb, num_pixels, out) {                  \
  uint8_t r1[9], r2[9];                                                 \
  memcpy(r1, (tb), (num_pixels));                                       \
  memcpy(r2, (bb), (num_pixels));                                       \
                                               \
  memset(r1 + (num_pixels), r1[(num_pixels) - 1], 9 - (num_pixels));    \
  memset(r2 + (num_pixels), r2[(num_pixels) - 1], 9 - (num_pixels));    \
  Upsample16Pixels_NEON(r1, r2, out);                                   \
}

static const int16_t kCoeffs1[4] = { 19077, 26149, 6419, 13320 };

#define v255 vdup_n_u8(255)

#define STORE_Rgb(out, r, g, b) do {                                    \
  uint8x8x3_t r_g_b;                                                    \
  INIT_VECTOR3(r_g_b, r, g, b);                                         \
  vst3_u8(out, r_g_b);                                                  \
} while (0)

#define STORE_Bgr(out, r, g, b) do {                                    \
  uint8x8x3_t b_g_r;                                                    \
  INIT_VECTOR3(b_g_r, b, g, r);                                         \
  vst3_u8(out, b_g_r);                                                  \
} while (0)

#define STORE_Rgba(out, r, g, b) do {                                   \
  uint8x8x4_t r_g_b_v255;                                               \
  INIT_VECTOR4(r_g_b_v255, r, g, b, v255);                              \
  vst4_u8(out, r_g_b_v255);                                             \
} while (0)

#define STORE_Bgra(out, r, g, b) do {                                   \
  uint8x8x4_t b_g_r_v255;                                               \
  INIT_VECTOR4(b_g_r_v255, b, g, r, v255);                              \
  vst4_u8(out, b_g_r_v255);                                             \
} while (0)

#define STORE_Argb(out, r, g, b) do {                                   \
  uint8x8x4_t v255_r_g_b;                                               \
  INIT_VECTOR4(v255_r_g_b, v255, r, g, b);                              \
  vst4_u8(out, v255_r_g_b);                                             \
} while (0)

#if (WEBP_SWAP_16BIT_CSP == 0)
#define ZIP_U8(lo, hi) vzip_u8((lo), (hi))
#else
#define ZIP_U8(lo, hi) vzip_u8((hi), (lo))
#endif

#define STORE_Rgba4444(out, r, g, b) do {                               \
  const uint8x8_t rg = vsri_n_u8(r, g, 4);       \
  const uint8x8_t ba = vsri_n_u8(b, v255, 4);    \
  const uint8x8x2_t rgba4444 = ZIP_U8(rg, ba);                          \
  vst1q_u8(out, vcombine_u8(rgba4444.val[0], rgba4444.val[1]));         \
} while (0)

#define STORE_Rgb565(out, r, g, b) do {                                 \
  const uint8x8_t rg = vsri_n_u8(r, g, 5);    \
  const uint8x8_t g1 = vshl_n_u8(g, 3);         \
  const uint8x8_t gb = vsri_n_u8(g1, b, 3);   \
  const uint8x8x2_t rgb565 = ZIP_U8(rg, gb);                            \
  vst1q_u8(out, vcombine_u8(rgb565.val[0], rgb565.val[1]));             \
} while (0)

#define CONVERT8(FMT, XSTEP, N, src_y, src_uv, out, cur_x) do {         \
  int i;                                                                \
  for (i = 0; i < N; i += 8) {                                          \
    const int off = ((cur_x) + i) * XSTEP;                              \
    const uint8x8_t y  = vld1_u8((src_y) + (cur_x)  + i);               \
    const uint8x8_t u  = vld1_u8((src_uv) + i +  0);                    \
    const uint8x8_t v  = vld1_u8((src_uv) + i + 16);                    \
    const int16x8_t Y0 = vreinterpretq_s16_u16(vshll_n_u8(y, 7));       \
    const int16x8_t U0 = vreinterpretq_s16_u16(vshll_n_u8(u, 7));       \
    const int16x8_t V0 = vreinterpretq_s16_u16(vshll_n_u8(v, 7));       \
    const int16x8_t Y1 = vqdmulhq_lane_s16(Y0, coeff1, 0);              \
    const int16x8_t R0 = vqdmulhq_lane_s16(V0, coeff1, 1);              \
    const int16x8_t G0 = vqdmulhq_lane_s16(U0, coeff1, 2);              \
    const int16x8_t G1 = vqdmulhq_lane_s16(V0, coeff1, 3);              \
    const int16x8_t B0 = vqdmulhq_n_s16(U0, 282);                       \
    const int16x8_t R1 = vqaddq_s16(Y1, R_Rounder);                     \
    const int16x8_t G2 = vqaddq_s16(Y1, G_Rounder);                     \
    const int16x8_t B1 = vqaddq_s16(Y1, B_Rounder);                     \
    const int16x8_t R2 = vqaddq_s16(R0, R1);                            \
    const int16x8_t G3 = vqaddq_s16(G0, G1);                            \
    const int16x8_t B2 = vqaddq_s16(B0, B1);                            \
    const int16x8_t G4 = vqsubq_s16(G2, G3);                            \
    const int16x8_t B3 = vqaddq_s16(B2, U0);                            \
    const uint8x8_t R = vqshrun_n_s16(R2, YUV_FIX2);                    \
    const uint8x8_t G = vqshrun_n_s16(G4, YUV_FIX2);                    \
    const uint8x8_t B = vqshrun_n_s16(B3, YUV_FIX2);                    \
    STORE_ ## FMT(out + off, R, G, B);                                  \
  }                                                                     \
} while (0)

#define CONVERT1(FUNC, XSTEP, N, src_y, src_uv, rgb, cur_x) {           \
  int i;                                                                \
  for (i = 0; i < N; i++) {                                             \
    const int off = ((cur_x) + i) * XSTEP;                              \
    const int y = src_y[(cur_x) + i];                                   \
    const int u = (src_uv)[i];                                          \
    const int v = (src_uv)[i + 16];                                     \
    FUNC(y, u, v, rgb + off);                                           \
  }                                                                     \
}

#define CONVERT2RGB_8(FMT, XSTEP, top_y, bottom_y, uv,                  \
                      top_dst, bottom_dst, cur_x, len) {                \
  CONVERT8(FMT, XSTEP, len, top_y, uv, top_dst, cur_x);                 \
  if (bottom_y != NULL) {                                               \
    CONVERT8(FMT, XSTEP, len, bottom_y, (uv) + 32, bottom_dst, cur_x);  \
  }                                                                     \
}

#define CONVERT2RGB_1(FUNC, XSTEP, top_y, bottom_y, uv,                 \
                      top_dst, bottom_dst, cur_x, len) {                \
  CONVERT1(FUNC, XSTEP, len, top_y, uv, top_dst, cur_x);                \
  if (bottom_y != NULL) {                                               \
    CONVERT1(FUNC, XSTEP, len, bottom_y, (uv) + 32, bottom_dst, cur_x); \
  }                                                                     \
}

#define NEON_UPSAMPLE_FUNC(FUNC_NAME, FMT, XSTEP)                              \
static void FUNC_NAME(const uint8_t* WEBP_RESTRICT top_y,                      \
                      const uint8_t* WEBP_RESTRICT bottom_y,                   \
                      const uint8_t* WEBP_RESTRICT top_u,                      \
                      const uint8_t* WEBP_RESTRICT top_v,                      \
                      const uint8_t* WEBP_RESTRICT cur_u,                      \
                      const uint8_t* WEBP_RESTRICT cur_v,                      \
                      uint8_t* WEBP_RESTRICT top_dst,                          \
                      uint8_t* WEBP_RESTRICT bottom_dst, int len) {            \
  int block;                                                                   \
                     \
  uint8_t uv_buf[2 * 32 + 15];                                                 \
  uint8_t* const r_uv = (uint8_t*)((uintptr_t)(uv_buf + 15) & ~(uintptr_t)15); \
  const int uv_len = (len + 1) >> 1;                                           \
                                \
  const int num_blocks = (uv_len - 1) >> 3;                                    \
  const int leftover = uv_len - num_blocks * 8;                                \
  const int last_pos = 1 + 16 * num_blocks;                                    \
                                                                               \
  const int u_diag = ((top_u[0] + cur_u[0]) >> 1) + 1;                         \
  const int v_diag = ((top_v[0] + cur_v[0]) >> 1) + 1;                         \
                                                                               \
  const int16x4_t coeff1 = vld1_s16(kCoeffs1);                                 \
  const int16x8_t R_Rounder = vdupq_n_s16(-14234);                             \
  const int16x8_t G_Rounder = vdupq_n_s16(8708);                               \
  const int16x8_t B_Rounder = vdupq_n_s16(-17685);                             \
                                                                               \
                                     \
  assert(top_y != NULL);                                                       \
  {                                                                            \
    const int u0 = (top_u[0] + u_diag) >> 1;                                   \
    const int v0 = (top_v[0] + v_diag) >> 1;                                   \
    VP8YuvTo ## FMT(top_y[0], u0, v0, top_dst);                                \
  }                                                                            \
  if (bottom_y != NULL) {                                                      \
    const int u0 = (cur_u[0] + u_diag) >> 1;                                   \
    const int v0 = (cur_v[0] + v_diag) >> 1;                                   \
    VP8YuvTo ## FMT(bottom_y[0], u0, v0, bottom_dst);                          \
  }                                                                            \
                                                                               \
  for (block = 0; block < num_blocks; ++block) {                               \
    UPSAMPLE_16PIXELS(top_u, cur_u, r_uv);                                     \
    UPSAMPLE_16PIXELS(top_v, cur_v, r_uv + 16);                                \
    CONVERT2RGB_8(FMT, XSTEP, top_y, bottom_y, r_uv,                           \
                  top_dst, bottom_dst, 16 * block + 1, 16);                    \
    top_u += 8;                                                                \
    cur_u += 8;                                                                \
    top_v += 8;                                                                \
    cur_v += 8;                                                                \
  }                                                                            \
                                                                               \
  UPSAMPLE_LAST_BLOCK(top_u, cur_u, leftover, r_uv);                           \
  UPSAMPLE_LAST_BLOCK(top_v, cur_v, leftover, r_uv + 16);                      \
  CONVERT2RGB_1(VP8YuvTo ## FMT, XSTEP, top_y, bottom_y, r_uv,                 \
                top_dst, bottom_dst, last_pos, len - last_pos);                \
}

NEON_UPSAMPLE_FUNC(UpsampleRgbaLinePair_NEON, Rgba, 4)
NEON_UPSAMPLE_FUNC(UpsampleBgraLinePair_NEON, Bgra, 4)
#if !defined(WEBP_REDUCE_CSP)
NEON_UPSAMPLE_FUNC(UpsampleRgbLinePair_NEON,  Rgb,  3)
NEON_UPSAMPLE_FUNC(UpsampleBgrLinePair_NEON,  Bgr,  3)
NEON_UPSAMPLE_FUNC(UpsampleArgbLinePair_NEON, Argb, 4)
NEON_UPSAMPLE_FUNC(UpsampleRgba4444LinePair_NEON, Rgba4444, 2)
NEON_UPSAMPLE_FUNC(UpsampleRgb565LinePair_NEON, Rgb565, 2)
#endif

extern WebPUpsampleLinePairFunc WebPUpsamplers[];

extern void WebPInitUpsamplersNEON(void);

WEBP_TSAN_IGNORE_FUNCTION void WebPInitUpsamplersNEON(void) {
  WebPUpsamplers[MODE_RGBA] = UpsampleRgbaLinePair_NEON;
  WebPUpsamplers[MODE_BGRA] = UpsampleBgraLinePair_NEON;
  WebPUpsamplers[MODE_rgbA] = UpsampleRgbaLinePair_NEON;
  WebPUpsamplers[MODE_bgrA] = UpsampleBgraLinePair_NEON;
#if !defined(WEBP_REDUCE_CSP)
  WebPUpsamplers[MODE_RGB]  = UpsampleRgbLinePair_NEON;
  WebPUpsamplers[MODE_BGR]  = UpsampleBgrLinePair_NEON;
  WebPUpsamplers[MODE_ARGB] = UpsampleArgbLinePair_NEON;
  WebPUpsamplers[MODE_Argb] = UpsampleArgbLinePair_NEON;
  WebPUpsamplers[MODE_RGB_565] = UpsampleRgb565LinePair_NEON;
  WebPUpsamplers[MODE_RGBA_4444] = UpsampleRgba4444LinePair_NEON;
  WebPUpsamplers[MODE_rgbA_4444] = UpsampleRgba4444LinePair_NEON;
#endif
}

#endif

#endif

#if !(defined(FANCY_UPSAMPLING) && defined(WEBP_USE_NEON))
WEBP_DSP_INIT_STUB(WebPInitUpsamplersNEON)
#endif

#if defined(WEBP_USE_SSE2)
#include <emmintrin.h>

#include <assert.h>
#include <string.h>

#ifdef FANCY_UPSAMPLING

#define GET_M(ij, in, out) do {                                                \
  const __m128i tmp0 = _mm_avg_epu8(k, (in));            \
  const __m128i tmp1 = _mm_and_si128((ij), st);              \
  const __m128i tmp2 = _mm_xor_si128(k, (in));                     \
  const __m128i tmp3 = _mm_or_si128(tmp1, tmp2);  \
  const __m128i tmp4 = _mm_and_si128(tmp3, one);    \
  (out) = _mm_sub_epi8(tmp0, tmp4);     \
} while (0)

#define PACK_AND_STORE(a, b, da, db, out) do {                                 \
  const __m128i t_a = _mm_avg_epu8(a, da);   \
  const __m128i t_b = _mm_avg_epu8(b, db);   \
  const __m128i t_1 = _mm_unpacklo_epi8(t_a, t_b);                             \
  const __m128i t_2 = _mm_unpackhi_epi8(t_a, t_b);                             \
  _mm_store_si128(((__m128i*)(out)) + 0, t_1);                                 \
  _mm_store_si128(((__m128i*)(out)) + 1, t_2);                                 \
} while (0)

#define UPSAMPLE_32PIXELS(r1, r2, out) do {                                    \
  const __m128i one = _mm_set1_epi8(1);                                        \
  const __m128i a = _mm_loadu_si128((const __m128i*)&(r1)[0]);                 \
  const __m128i b = _mm_loadu_si128((const __m128i*)&(r1)[1]);                 \
  const __m128i c = _mm_loadu_si128((const __m128i*)&(r2)[0]);                 \
  const __m128i d = _mm_loadu_si128((const __m128i*)&(r2)[1]);                 \
                                                                               \
  const __m128i s = _mm_avg_epu8(a, d);               \
  const __m128i t = _mm_avg_epu8(b, c);               \
  const __m128i st = _mm_xor_si128(s, t);                        \
                                                                               \
  const __m128i ad = _mm_xor_si128(a, d);                        \
  const __m128i bc = _mm_xor_si128(b, c);                        \
                                                                               \
  const __m128i t1 = _mm_or_si128(ad, bc);                  \
  const __m128i t2 = _mm_or_si128(t1, st);          \
  const __m128i t3 = _mm_and_si128(t2, one);    \
  const __m128i t4 = _mm_avg_epu8(s, t);                                       \
  const __m128i k = _mm_sub_epi8(t4, t3);         \
  __m128i diag1, diag2;                                                        \
                                                                               \
  GET_M(bc, t, diag1);                      \
  GET_M(ad, s, diag2);                      \
                                                                               \
                                                \
  PACK_AND_STORE(a, b, diag1, diag2, (out) +      0);           \
  PACK_AND_STORE(c, d, diag2, diag1, (out) + 2 * 32);        \
} while (0)

static void Upsample32Pixels_SSE2(const uint8_t* WEBP_RESTRICT const r1,
                                  const uint8_t* WEBP_RESTRICT const r2,
                                  uint8_t* WEBP_RESTRICT const out) {
  UPSAMPLE_32PIXELS(r1, r2, out);
}

#define UPSAMPLE_LAST_BLOCK(tb, bb, num_pixels, out) {                         \
  uint8_t r1[17], r2[17];                                                      \
  memcpy(r1, (tb), (num_pixels));                                              \
  memcpy(r2, (bb), (num_pixels));                                              \
                                                      \
  memset(r1 + (num_pixels), r1[(num_pixels) - 1], 17 - (num_pixels));          \
  memset(r2 + (num_pixels), r2[(num_pixels) - 1], 17 - (num_pixels));          \
       \
  Upsample32Pixels_SSE2(r1, r2, out);                                          \
}

#define CONVERT2RGB_32(FUNC, XSTEP, top_y, bottom_y,                           \
                       top_dst, bottom_dst, cur_x) do {                        \
  FUNC##32_SSE2((top_y) + (cur_x), r_u, r_v, (top_dst) + (cur_x) * (XSTEP));   \
  if ((bottom_y) != NULL) {                                                    \
    FUNC##32_SSE2((bottom_y) + (cur_x), r_u + 64, r_v + 64,                    \
                  (bottom_dst) + (cur_x) * (XSTEP));                           \
  }                                                                            \
} while (0)

#define SSE2_UPSAMPLE_FUNC(FUNC_NAME, FUNC, XSTEP)                             \
static void FUNC_NAME(const uint8_t* WEBP_RESTRICT top_y,                      \
                      const uint8_t* WEBP_RESTRICT bottom_y,                   \
                      const uint8_t* WEBP_RESTRICT top_u,                      \
                      const uint8_t* WEBP_RESTRICT top_v,                      \
                      const uint8_t* WEBP_RESTRICT cur_u,                      \
                      const uint8_t* WEBP_RESTRICT cur_v,                      \
                      uint8_t* WEBP_RESTRICT top_dst,                          \
                      uint8_t* WEBP_RESTRICT bottom_dst, int len) {            \
  int uv_pos, pos;                                                             \
                      \
  uint8_t uv_buf[14 * 32 + 15] = { 0 };                                        \
  uint8_t* const r_u = (uint8_t*)((uintptr_t)(uv_buf + 15) & ~(uintptr_t)15);  \
  uint8_t* const r_v = r_u + 32;                                               \
                                                                               \
  assert(top_y != NULL);                                                       \
  {                                  \
    const int u_diag = ((top_u[0] + cur_u[0]) >> 1) + 1;                       \
    const int v_diag = ((top_v[0] + cur_v[0]) >> 1) + 1;                       \
    const int u0_t = (top_u[0] + u_diag) >> 1;                                 \
    const int v0_t = (top_v[0] + v_diag) >> 1;                                 \
    FUNC(top_y[0], u0_t, v0_t, top_dst);                                       \
    if (bottom_y != NULL) {                                                    \
      const int u0_b = (cur_u[0] + u_diag) >> 1;                               \
      const int v0_b = (cur_v[0] + v_diag) >> 1;                               \
      FUNC(bottom_y[0], u0_b, v0_b, bottom_dst);                               \
    }                                                                          \
  }                                                                            \
    \
  for (pos = 1, uv_pos = 0; pos + 32 + 1 <= len; pos += 32, uv_pos += 16) {    \
    UPSAMPLE_32PIXELS(top_u + uv_pos, cur_u + uv_pos, r_u);                    \
    UPSAMPLE_32PIXELS(top_v + uv_pos, cur_v + uv_pos, r_v);                    \
    CONVERT2RGB_32(FUNC, XSTEP, top_y, bottom_y, top_dst, bottom_dst, pos);    \
  }                                                                            \
  if (len > 1) {                                                               \
    const int left_over = ((len + 1) >> 1) - (pos >> 1);                       \
    uint8_t* const tmp_top_dst = r_u + 4 * 32;                                 \
    uint8_t* const tmp_bottom_dst = tmp_top_dst + 4 * 32;                      \
    uint8_t* const tmp_top = tmp_bottom_dst + 4 * 32;                          \
    uint8_t* const tmp_bottom = (bottom_y == NULL) ? NULL : tmp_top + 32;      \
    assert(left_over > 0);                                                     \
    UPSAMPLE_LAST_BLOCK(top_u + uv_pos, cur_u + uv_pos, left_over, r_u);       \
    UPSAMPLE_LAST_BLOCK(top_v + uv_pos, cur_v + uv_pos, left_over, r_v);       \
    memcpy(tmp_top, top_y + pos, len - pos);                                   \
    if (bottom_y != NULL) memcpy(tmp_bottom, bottom_y + pos, len - pos);       \
    CONVERT2RGB_32(FUNC, XSTEP, tmp_top, tmp_bottom, tmp_top_dst,              \
         tmp_bottom_dst, 0);                                                   \
    memcpy(top_dst + pos * (XSTEP), tmp_top_dst, (len - pos) * (XSTEP));       \
    if (bottom_y != NULL) {                                                    \
      memcpy(bottom_dst + pos * (XSTEP), tmp_bottom_dst,                       \
             (len - pos) * (XSTEP));                                           \
    }                                                                          \
  }                                                                            \
}

SSE2_UPSAMPLE_FUNC(UpsampleRgbaLinePair_SSE2, VP8YuvToRgba, 4)
SSE2_UPSAMPLE_FUNC(UpsampleBgraLinePair_SSE2, VP8YuvToBgra, 4)

#if !defined(WEBP_REDUCE_CSP)
SSE2_UPSAMPLE_FUNC(UpsampleRgbLinePair_SSE2,  VP8YuvToRgb,  3)
SSE2_UPSAMPLE_FUNC(UpsampleBgrLinePair_SSE2,  VP8YuvToBgr,  3)
SSE2_UPSAMPLE_FUNC(UpsampleArgbLinePair_SSE2, VP8YuvToArgb, 4)
SSE2_UPSAMPLE_FUNC(UpsampleRgba4444LinePair_SSE2, VP8YuvToRgba4444, 2)
SSE2_UPSAMPLE_FUNC(UpsampleRgb565LinePair_SSE2, VP8YuvToRgb565, 2)
#endif

#undef GET_M
#undef PACK_AND_STORE
#undef UPSAMPLE_32PIXELS
#undef UPSAMPLE_LAST_BLOCK
#undef CONVERT2RGB
#undef CONVERT2RGB_32
#undef SSE2_UPSAMPLE_FUNC

extern WebPUpsampleLinePairFunc WebPUpsamplers[];

extern void WebPInitUpsamplersSSE2(void);

WEBP_TSAN_IGNORE_FUNCTION void WebPInitUpsamplersSSE2(void) {
  WebPUpsamplers[MODE_RGBA] = UpsampleRgbaLinePair_SSE2;
  WebPUpsamplers[MODE_BGRA] = UpsampleBgraLinePair_SSE2;
  WebPUpsamplers[MODE_rgbA] = UpsampleRgbaLinePair_SSE2;
  WebPUpsamplers[MODE_bgrA] = UpsampleBgraLinePair_SSE2;
#if !defined(WEBP_REDUCE_CSP)
  WebPUpsamplers[MODE_RGB]  = UpsampleRgbLinePair_SSE2;
  WebPUpsamplers[MODE_BGR]  = UpsampleBgrLinePair_SSE2;
  WebPUpsamplers[MODE_ARGB] = UpsampleArgbLinePair_SSE2;
  WebPUpsamplers[MODE_Argb] = UpsampleArgbLinePair_SSE2;
  WebPUpsamplers[MODE_RGB_565] = UpsampleRgb565LinePair_SSE2;
  WebPUpsamplers[MODE_RGBA_4444] = UpsampleRgba4444LinePair_SSE2;
  WebPUpsamplers[MODE_rgbA_4444] = UpsampleRgba4444LinePair_SSE2;
#endif
}

#endif

extern WebPYUV444Converter WebPYUV444Converters[];
extern void WebPInitYUV444ConvertersSSE2(void);

#define YUV444_FUNC(FUNC_NAME, CALL, CALL_C, XSTEP)                            \
extern void CALL_C(const uint8_t* WEBP_RESTRICT y,                             \
                   const uint8_t* WEBP_RESTRICT u,                             \
                   const uint8_t* WEBP_RESTRICT v,                             \
                   uint8_t* WEBP_RESTRICT dst, int len);                       \
static void FUNC_NAME(const uint8_t* WEBP_RESTRICT y,                          \
                      const uint8_t* WEBP_RESTRICT u,                          \
                      const uint8_t* WEBP_RESTRICT v,                          \
                      uint8_t* WEBP_RESTRICT dst, int len) {                   \
  int i;                                                                       \
  const int max_len = len & ~31;                                               \
  for (i = 0; i < max_len; i += 32) {                                          \
    CALL(y + i, u + i, v + i, dst + i * (XSTEP));                              \
  }                                                                            \
  if (i < len) {                                               \
    CALL_C(y + i, u + i, v + i, dst + i * (XSTEP), len - i);                   \
  }                                                                            \
}

YUV444_FUNC(Yuv444ToRgba_SSE2, VP8YuvToRgba32_SSE2, WebPYuv444ToRgba_C, 4)
YUV444_FUNC(Yuv444ToBgra_SSE2, VP8YuvToBgra32_SSE2, WebPYuv444ToBgra_C, 4)
#if !defined(WEBP_REDUCE_CSP)
YUV444_FUNC(Yuv444ToRgb_SSE2, VP8YuvToRgb32_SSE2, WebPYuv444ToRgb_C, 3)
YUV444_FUNC(Yuv444ToBgr_SSE2, VP8YuvToBgr32_SSE2, WebPYuv444ToBgr_C, 3)
YUV444_FUNC(Yuv444ToArgb_SSE2, VP8YuvToArgb32_SSE2, WebPYuv444ToArgb_C, 4)
YUV444_FUNC(Yuv444ToRgba4444_SSE2, VP8YuvToRgba444432_SSE2, \
            WebPYuv444ToRgba4444_C, 2)
YUV444_FUNC(Yuv444ToRgb565_SSE2, VP8YuvToRgb56532_SSE2, WebPYuv444ToRgb565_C, 2)
#endif

WEBP_TSAN_IGNORE_FUNCTION void WebPInitYUV444ConvertersSSE2(void) {
  WebPYUV444Converters[MODE_RGBA]      = Yuv444ToRgba_SSE2;
  WebPYUV444Converters[MODE_BGRA]      = Yuv444ToBgra_SSE2;
  WebPYUV444Converters[MODE_rgbA]      = Yuv444ToRgba_SSE2;
  WebPYUV444Converters[MODE_bgrA]      = Yuv444ToBgra_SSE2;
#if !defined(WEBP_REDUCE_CSP)
  WebPYUV444Converters[MODE_RGB]       = Yuv444ToRgb_SSE2;
  WebPYUV444Converters[MODE_BGR]       = Yuv444ToBgr_SSE2;
  WebPYUV444Converters[MODE_ARGB]      = Yuv444ToArgb_SSE2;
  WebPYUV444Converters[MODE_RGBA_4444] = Yuv444ToRgba4444_SSE2;
  WebPYUV444Converters[MODE_RGB_565]   = Yuv444ToRgb565_SSE2;
  WebPYUV444Converters[MODE_Argb]      = Yuv444ToArgb_SSE2;
  WebPYUV444Converters[MODE_rgbA_4444] = Yuv444ToRgba4444_SSE2;
#endif
}

#else

WEBP_DSP_INIT_STUB(WebPInitYUV444ConvertersSSE2)

#endif

#if !(defined(FANCY_UPSAMPLING) && defined(WEBP_USE_SSE2))
WEBP_DSP_INIT_STUB(WebPInitUpsamplersSSE2)
#endif

#if defined(WEBP_USE_SSE41)
#include <smmintrin.h>

#include <assert.h>
#include <string.h>

#ifdef FANCY_UPSAMPLING

#if !defined(WEBP_REDUCE_CSP)

#define GET_M(ij, in, out) do {                                                \
  const __m128i tmp0 = _mm_avg_epu8(k, (in));            \
  const __m128i tmp1 = _mm_and_si128((ij), st);              \
  const __m128i tmp2 = _mm_xor_si128(k, (in));                     \
  const __m128i tmp3 = _mm_or_si128(tmp1, tmp2);  \
  const __m128i tmp4 = _mm_and_si128(tmp3, one);    \
  (out) = _mm_sub_epi8(tmp0, tmp4);     \
} while (0)

#define PACK_AND_STORE(a, b, da, db, out) do {                                 \
  const __m128i t_a = _mm_avg_epu8(a, da);   \
  const __m128i t_b = _mm_avg_epu8(b, db);   \
  const __m128i t_1 = _mm_unpacklo_epi8(t_a, t_b);                             \
  const __m128i t_2 = _mm_unpackhi_epi8(t_a, t_b);                             \
  _mm_store_si128(((__m128i*)(out)) + 0, t_1);                                 \
  _mm_store_si128(((__m128i*)(out)) + 1, t_2);                                 \
} while (0)

#define UPSAMPLE_32PIXELS(r1, r2, out) do {                                    \
  const __m128i one = _mm_set1_epi8(1);                                        \
  const __m128i a = _mm_loadu_si128((const __m128i*)&(r1)[0]);                 \
  const __m128i b = _mm_loadu_si128((const __m128i*)&(r1)[1]);                 \
  const __m128i c = _mm_loadu_si128((const __m128i*)&(r2)[0]);                 \
  const __m128i d = _mm_loadu_si128((const __m128i*)&(r2)[1]);                 \
                                                                               \
  const __m128i s = _mm_avg_epu8(a, d);               \
  const __m128i t = _mm_avg_epu8(b, c);               \
  const __m128i st = _mm_xor_si128(s, t);                        \
                                                                               \
  const __m128i ad = _mm_xor_si128(a, d);                        \
  const __m128i bc = _mm_xor_si128(b, c);                        \
                                                                               \
  const __m128i t1 = _mm_or_si128(ad, bc);                  \
  const __m128i t2 = _mm_or_si128(t1, st);          \
  const __m128i t3 = _mm_and_si128(t2, one);    \
  const __m128i t4 = _mm_avg_epu8(s, t);                                       \
  const __m128i k = _mm_sub_epi8(t4, t3);         \
  __m128i diag1, diag2;                                                        \
                                                                               \
  GET_M(bc, t, diag1);                      \
  GET_M(ad, s, diag2);                      \
                                                                               \
                                                \
  PACK_AND_STORE(a, b, diag1, diag2, (out) +      0);           \
  PACK_AND_STORE(c, d, diag2, diag1, (out) + 2 * 32);        \
} while (0)

static void Upsample32Pixels_SSE41(const uint8_t* WEBP_RESTRICT const r1,
                                   const uint8_t* WEBP_RESTRICT const r2,
                                   uint8_t* WEBP_RESTRICT const out) {
  UPSAMPLE_32PIXELS(r1, r2, out);
}

#define UPSAMPLE_LAST_BLOCK(tb, bb, num_pixels, out) {                         \
  uint8_t r1[17], r2[17];                                                      \
  memcpy(r1, (tb), (num_pixels));                                              \
  memcpy(r2, (bb), (num_pixels));                                              \
                                                      \
  memset(r1 + (num_pixels), r1[(num_pixels) - 1], 17 - (num_pixels));          \
  memset(r2 + (num_pixels), r2[(num_pixels) - 1], 17 - (num_pixels));          \
       \
  Upsample32Pixels_SSE41(r1, r2, out);                                         \
}

#define CONVERT2RGB_32(FUNC, XSTEP, top_y, bottom_y,                           \
                       top_dst, bottom_dst, cur_x) do {                        \
  FUNC##32_SSE41((top_y) + (cur_x), r_u, r_v, (top_dst) + (cur_x) * (XSTEP));  \
  if ((bottom_y) != NULL) {                                                    \
    FUNC##32_SSE41((bottom_y) + (cur_x), r_u + 64, r_v + 64,                   \
                  (bottom_dst) + (cur_x) * (XSTEP));                           \
  }                                                                            \
} while (0)

#define SSE4_UPSAMPLE_FUNC(FUNC_NAME, FUNC, XSTEP)                             \
static void FUNC_NAME(const uint8_t* WEBP_RESTRICT top_y,                      \
                      const uint8_t* WEBP_RESTRICT bottom_y,                   \
                      const uint8_t* WEBP_RESTRICT top_u,                      \
                      const uint8_t* WEBP_RESTRICT top_v,                      \
                      const uint8_t* WEBP_RESTRICT cur_u,                      \
                      const uint8_t* WEBP_RESTRICT cur_v,                      \
                      uint8_t* WEBP_RESTRICT top_dst,                          \
                      uint8_t* WEBP_RESTRICT bottom_dst, int len) {            \
  int uv_pos, pos;                                                             \
                      \
  uint8_t uv_buf[14 * 32 + 15] = { 0 };                                        \
  uint8_t* const r_u = (uint8_t*)((uintptr_t)(uv_buf + 15) & ~(uintptr_t)15);  \
  uint8_t* const r_v = r_u + 32;                                               \
                                                                               \
  assert(top_y != NULL);                                                       \
  {                                  \
    const int u_diag = ((top_u[0] + cur_u[0]) >> 1) + 1;                       \
    const int v_diag = ((top_v[0] + cur_v[0]) >> 1) + 1;                       \
    const int u0_t = (top_u[0] + u_diag) >> 1;                                 \
    const int v0_t = (top_v[0] + v_diag) >> 1;                                 \
    FUNC(top_y[0], u0_t, v0_t, top_dst);                                       \
    if (bottom_y != NULL) {                                                    \
      const int u0_b = (cur_u[0] + u_diag) >> 1;                               \
      const int v0_b = (cur_v[0] + v_diag) >> 1;                               \
      FUNC(bottom_y[0], u0_b, v0_b, bottom_dst);                               \
    }                                                                          \
  }                                                                            \
    \
  for (pos = 1, uv_pos = 0; pos + 32 + 1 <= len; pos += 32, uv_pos += 16) {    \
    UPSAMPLE_32PIXELS(top_u + uv_pos, cur_u + uv_pos, r_u);                    \
    UPSAMPLE_32PIXELS(top_v + uv_pos, cur_v + uv_pos, r_v);                    \
    CONVERT2RGB_32(FUNC, XSTEP, top_y, bottom_y, top_dst, bottom_dst, pos);    \
  }                                                                            \
  if (len > 1) {                                                               \
    const int left_over = ((len + 1) >> 1) - (pos >> 1);                       \
    uint8_t* const tmp_top_dst = r_u + 4 * 32;                                 \
    uint8_t* const tmp_bottom_dst = tmp_top_dst + 4 * 32;                      \
    uint8_t* const tmp_top = tmp_bottom_dst + 4 * 32;                          \
    uint8_t* const tmp_bottom = (bottom_y == NULL) ? NULL : tmp_top + 32;      \
    assert(left_over > 0);                                                     \
    UPSAMPLE_LAST_BLOCK(top_u + uv_pos, cur_u + uv_pos, left_over, r_u);       \
    UPSAMPLE_LAST_BLOCK(top_v + uv_pos, cur_v + uv_pos, left_over, r_v);       \
    memcpy(tmp_top, top_y + pos, len - pos);                                   \
    if (bottom_y != NULL) memcpy(tmp_bottom, bottom_y + pos, len - pos);       \
    CONVERT2RGB_32(FUNC, XSTEP, tmp_top, tmp_bottom, tmp_top_dst,              \
         tmp_bottom_dst, 0);                                                   \
    memcpy(top_dst + pos * (XSTEP), tmp_top_dst, (len - pos) * (XSTEP));       \
    if (bottom_y != NULL) {                                                    \
      memcpy(bottom_dst + pos * (XSTEP), tmp_bottom_dst,                       \
             (len - pos) * (XSTEP));                                           \
    }                                                                          \
  }                                                                            \
}

SSE4_UPSAMPLE_FUNC(UpsampleRgbLinePair_SSE41,  VP8YuvToRgb,  3)
SSE4_UPSAMPLE_FUNC(UpsampleBgrLinePair_SSE41,  VP8YuvToBgr,  3)

#undef GET_M
#undef PACK_AND_STORE
#undef UPSAMPLE_32PIXELS
#undef UPSAMPLE_LAST_BLOCK
#undef CONVERT2RGB
#undef CONVERT2RGB_32
#undef SSE4_UPSAMPLE_FUNC

#endif

extern WebPUpsampleLinePairFunc WebPUpsamplers[];

extern void WebPInitUpsamplersSSE41(void);

WEBP_TSAN_IGNORE_FUNCTION void WebPInitUpsamplersSSE41(void) {
#if !defined(WEBP_REDUCE_CSP)
  WebPUpsamplers[MODE_RGB]  = UpsampleRgbLinePair_SSE41;
  WebPUpsamplers[MODE_BGR]  = UpsampleBgrLinePair_SSE41;
#endif
}

#endif

extern WebPYUV444Converter WebPYUV444Converters[];
extern void WebPInitYUV444ConvertersSSE41(void);

#define YUV444_FUNC(FUNC_NAME, CALL, CALL_C, XSTEP)                            \
extern void CALL_C(const uint8_t* WEBP_RESTRICT y,                             \
                   const uint8_t* WEBP_RESTRICT u,                             \
                   const uint8_t* WEBP_RESTRICT v,                             \
                   uint8_t* WEBP_RESTRICT dst, int len);                       \
static void FUNC_NAME(const uint8_t* WEBP_RESTRICT y,                          \
                      const uint8_t* WEBP_RESTRICT u,                          \
                      const uint8_t* WEBP_RESTRICT v,                          \
                      uint8_t* WEBP_RESTRICT dst, int len) {                   \
  int i;                                                                       \
  const int max_len = len & ~31;                                               \
  for (i = 0; i < max_len; i += 32) {                                          \
    CALL(y + i, u + i, v + i, dst + i * (XSTEP));                              \
  }                                                                            \
  if (i < len) {                                               \
    CALL_C(y + i, u + i, v + i, dst + i * (XSTEP), len - i);                   \
  }                                                                            \
}

#if !defined(WEBP_REDUCE_CSP)
YUV444_FUNC(Yuv444ToRgb_SSE41, VP8YuvToRgb32_SSE41, WebPYuv444ToRgb_C, 3)
YUV444_FUNC(Yuv444ToBgr_SSE41, VP8YuvToBgr32_SSE41, WebPYuv444ToBgr_C, 3)
#endif

WEBP_TSAN_IGNORE_FUNCTION void WebPInitYUV444ConvertersSSE41(void) {
#if !defined(WEBP_REDUCE_CSP)
  WebPYUV444Converters[MODE_RGB]       = Yuv444ToRgb_SSE41;
  WebPYUV444Converters[MODE_BGR]       = Yuv444ToBgr_SSE41;
#endif
}

#else

WEBP_DSP_INIT_STUB(WebPInitYUV444ConvertersSSE41)

#endif

#if !(defined(FANCY_UPSAMPLING) && defined(WEBP_USE_SSE41))
WEBP_DSP_INIT_STUB(WebPInitUpsamplersSSE41)
#endif

#include <assert.h>
#include <stdlib.h>

#define ROW_FUNC(FUNC_NAME, FUNC, XSTEP)                                       \
static void FUNC_NAME(const uint8_t* WEBP_RESTRICT y,                          \
                      const uint8_t* WEBP_RESTRICT u,                          \
                      const uint8_t* WEBP_RESTRICT v,                          \
                      uint8_t* WEBP_RESTRICT dst, int len) {                   \
  const uint8_t* const end = dst + (len & ~1) * (XSTEP);                       \
  while (dst != end) {                                                         \
    FUNC(y[0], u[0], v[0], dst);                                               \
    FUNC(y[1], u[0], v[0], dst + (XSTEP));                                     \
    y += 2;                                                                    \
    ++u;                                                                       \
    ++v;                                                                       \
    dst += 2 * (XSTEP);                                                        \
  }                                                                            \
  if (len & 1) {                                                               \
    FUNC(y[0], u[0], v[0], dst);                                               \
  }                                                                            \
}                                                                              \

ROW_FUNC(YuvToRgbRow,      VP8YuvToRgb,  3)
ROW_FUNC(YuvToBgrRow,      VP8YuvToBgr,  3)
ROW_FUNC(YuvToRgbaRow,     VP8YuvToRgba, 4)
ROW_FUNC(YuvToBgraRow,     VP8YuvToBgra, 4)
ROW_FUNC(YuvToArgbRow,     VP8YuvToArgb, 4)
ROW_FUNC(YuvToRgba4444Row, VP8YuvToRgba4444, 2)
ROW_FUNC(YuvToRgb565Row,   VP8YuvToRgb565, 2)

#undef ROW_FUNC

void WebPSamplerProcessPlane(const uint8_t* WEBP_RESTRICT y, int y_stride,
                             const uint8_t* WEBP_RESTRICT u,
                             const uint8_t* WEBP_RESTRICT v, int uv_stride,
                             uint8_t* WEBP_RESTRICT dst, int dst_stride,
                             int width, int height, WebPSamplerRowFunc func) {
  int j;
  for (j = 0; j < height; ++j) {
    func(y, u, v, dst, width);
    y += y_stride;
    if (j & 1) {
      u += uv_stride;
      v += uv_stride;
    }
    dst += dst_stride;
  }
}

WebPSamplerRowFunc WebPSamplers[MODE_LAST];

extern VP8CPUInfo VP8GetCPUInfo;
extern void WebPInitSamplersSSE2(void);
extern void WebPInitSamplersSSE41(void);
extern void WebPInitSamplersMIPS32(void);
extern void WebPInitSamplersMIPSdspR2(void);

WEBP_DSP_INIT_FUNC(WebPInitSamplers) {
  WebPSamplers[MODE_RGB]       = YuvToRgbRow;
  WebPSamplers[MODE_RGBA]      = YuvToRgbaRow;
  WebPSamplers[MODE_BGR]       = YuvToBgrRow;
  WebPSamplers[MODE_BGRA]      = YuvToBgraRow;
  WebPSamplers[MODE_ARGB]      = YuvToArgbRow;
  WebPSamplers[MODE_RGBA_4444] = YuvToRgba4444Row;
  WebPSamplers[MODE_RGB_565]   = YuvToRgb565Row;
  WebPSamplers[MODE_rgbA]      = YuvToRgbaRow;
  WebPSamplers[MODE_bgrA]      = YuvToBgraRow;
  WebPSamplers[MODE_Argb]      = YuvToArgbRow;
  WebPSamplers[MODE_rgbA_4444] = YuvToRgba4444Row;

  if (VP8GetCPUInfo != NULL) {
#if defined(WEBP_HAVE_SSE2)
    if (VP8GetCPUInfo(kSSE2)) {
      WebPInitSamplersSSE2();
    }
#endif
#if defined(WEBP_HAVE_SSE41)
    if (VP8GetCPUInfo(kSSE4_1)) {
      WebPInitSamplersSSE41();
    }
#endif
#if defined(WEBP_USE_MIPS32)
    if (VP8GetCPUInfo(kMIPS32)) {
      WebPInitSamplersMIPS32();
    }
#endif
#if defined(WEBP_USE_MIPS_DSP_R2)
    if (VP8GetCPUInfo(kMIPSdspR2)) {
      WebPInitSamplersMIPSdspR2();
    }
#endif
  }
}

static void ConvertARGBToY_C(const uint32_t* WEBP_RESTRICT argb,
                             uint8_t* WEBP_RESTRICT y, int width) {
  int i;
  for (i = 0; i < width; ++i) {
    const uint32_t p = argb[i];
    y[i] = VP8RGBToY((p >> 16) & 0xff, (p >> 8) & 0xff, (p >>  0) & 0xff,
                     YUV_HALF);
  }
}

void WebPConvertARGBToUV_C(const uint32_t* WEBP_RESTRICT argb,
                           uint8_t* WEBP_RESTRICT u, uint8_t* WEBP_RESTRICT v,
                           int src_width, int do_store) {

  const int uv_width = src_width >> 1;
  int i;
  for (i = 0; i < uv_width; ++i) {
    const uint32_t v0 = argb[2 * i + 0];
    const uint32_t v1 = argb[2 * i + 1];

    const int r = ((v0 >> 15) & 0x1fe) + ((v1 >> 15) & 0x1fe);
    const int g = ((v0 >>  7) & 0x1fe) + ((v1 >>  7) & 0x1fe);
    const int b = ((v0 <<  1) & 0x1fe) + ((v1 <<  1) & 0x1fe);
    const int tmp_u = VP8RGBToU(r, g, b, YUV_HALF << 2);
    const int tmp_v = VP8RGBToV(r, g, b, YUV_HALF << 2);
    if (do_store) {
      u[i] = tmp_u;
      v[i] = tmp_v;
    } else {

      u[i] = (u[i] + tmp_u + 1) >> 1;
      v[i] = (v[i] + tmp_v + 1) >> 1;
    }
  }
  if (src_width & 1) {
    const uint32_t v0 = argb[2 * i + 0];
    const int r = (v0 >> 14) & 0x3fc;
    const int g = (v0 >>  6) & 0x3fc;
    const int b = (v0 <<  2) & 0x3fc;
    const int tmp_u = VP8RGBToU(r, g, b, YUV_HALF << 2);
    const int tmp_v = VP8RGBToV(r, g, b, YUV_HALF << 2);
    if (do_store) {
      u[i] = tmp_u;
      v[i] = tmp_v;
    } else {
      u[i] = (u[i] + tmp_u + 1) >> 1;
      v[i] = (v[i] + tmp_v + 1) >> 1;
    }
  }
}

static void ConvertRGB24ToY_C(const uint8_t* WEBP_RESTRICT rgb,
                              uint8_t* WEBP_RESTRICT y, int width) {
  int i;
  for (i = 0; i < width; ++i, rgb += 3) {
    y[i] = VP8RGBToY(rgb[0], rgb[1], rgb[2], YUV_HALF);
  }
}

static void ConvertBGR24ToY_C(const uint8_t* WEBP_RESTRICT bgr,
                              uint8_t* WEBP_RESTRICT y, int width) {
  int i;
  for (i = 0; i < width; ++i, bgr += 3) {
    y[i] = VP8RGBToY(bgr[2], bgr[1], bgr[0], YUV_HALF);
  }
}

void WebPConvertRGBA32ToUV_C(const uint16_t* WEBP_RESTRICT rgb,
                             uint8_t* WEBP_RESTRICT u, uint8_t* WEBP_RESTRICT v,
                             int width) {
  int i;
  for (i = 0; i < width; i += 1, rgb += 4) {
    const int r = rgb[0], g = rgb[1], b = rgb[2];
    u[i] = VP8RGBToU(r, g, b, YUV_HALF << 2);
    v[i] = VP8RGBToV(r, g, b, YUV_HALF << 2);
  }
}

void (*WebPConvertRGB24ToY)(const uint8_t* WEBP_RESTRICT rgb,
                            uint8_t* WEBP_RESTRICT y, int width);
void (*WebPConvertBGR24ToY)(const uint8_t* WEBP_RESTRICT bgr,
                            uint8_t* WEBP_RESTRICT y, int width);
void (*WebPConvertRGBA32ToUV)(const uint16_t* WEBP_RESTRICT rgb,
                              uint8_t* WEBP_RESTRICT u,
                              uint8_t* WEBP_RESTRICT v, int width);

void (*WebPConvertARGBToY)(const uint32_t* WEBP_RESTRICT argb,
                           uint8_t* WEBP_RESTRICT y, int width);
void (*WebPConvertARGBToUV)(const uint32_t* WEBP_RESTRICT argb,
                            uint8_t* WEBP_RESTRICT u, uint8_t* WEBP_RESTRICT v,
                            int src_width, int do_store);

extern void WebPInitConvertARGBToYUVSSE2(void);
extern void WebPInitConvertARGBToYUVSSE41(void);
extern void WebPInitConvertARGBToYUVNEON(void);

WEBP_DSP_INIT_FUNC(WebPInitConvertARGBToYUV) {
  WebPConvertARGBToY = ConvertARGBToY_C;
  WebPConvertARGBToUV = WebPConvertARGBToUV_C;

  WebPConvertRGB24ToY = ConvertRGB24ToY_C;
  WebPConvertBGR24ToY = ConvertBGR24ToY_C;

  WebPConvertRGBA32ToUV = WebPConvertRGBA32ToUV_C;

  if (VP8GetCPUInfo != NULL) {
#if defined(WEBP_HAVE_SSE2)
    if (VP8GetCPUInfo(kSSE2)) {
      WebPInitConvertARGBToYUVSSE2();
    }
#endif
#if defined(WEBP_HAVE_SSE41)
    if (VP8GetCPUInfo(kSSE4_1)) {
      WebPInitConvertARGBToYUVSSE41();
    }
#endif
  }

#if defined(WEBP_HAVE_NEON)
  if (WEBP_NEON_OMIT_C_CODE ||
      (VP8GetCPUInfo != NULL && VP8GetCPUInfo(kNEON))) {
    WebPInitConvertARGBToYUVNEON();
  }
#endif

  assert(WebPConvertARGBToY != NULL);
  assert(WebPConvertARGBToUV != NULL);
  assert(WebPConvertRGB24ToY != NULL);
  assert(WebPConvertBGR24ToY != NULL);
  assert(WebPConvertRGBA32ToUV != NULL);
}

#if defined(WEBP_USE_NEON)

#include <assert.h>
#include <stdlib.h>

static uint8x8_t ConvertRGBToY_NEON(const uint8x8_t R,
                                    const uint8x8_t G,
                                    const uint8x8_t B) {
  const uint16x8_t r = vmovl_u8(R);
  const uint16x8_t g = vmovl_u8(G);
  const uint16x8_t b = vmovl_u8(B);
  const uint16x4_t r_lo = vget_low_u16(r);
  const uint16x4_t r_hi = vget_high_u16(r);
  const uint16x4_t g_lo = vget_low_u16(g);
  const uint16x4_t g_hi = vget_high_u16(g);
  const uint16x4_t b_lo = vget_low_u16(b);
  const uint16x4_t b_hi = vget_high_u16(b);
  const uint32x4_t tmp0_lo = vmull_n_u16(         r_lo, 16839u);
  const uint32x4_t tmp0_hi = vmull_n_u16(         r_hi, 16839u);
  const uint32x4_t tmp1_lo = vmlal_n_u16(tmp0_lo, g_lo, 33059u);
  const uint32x4_t tmp1_hi = vmlal_n_u16(tmp0_hi, g_hi, 33059u);
  const uint32x4_t tmp2_lo = vmlal_n_u16(tmp1_lo, b_lo, 6420u);
  const uint32x4_t tmp2_hi = vmlal_n_u16(tmp1_hi, b_hi, 6420u);
  const uint16x8_t Y1 = vcombine_u16(vrshrn_n_u32(tmp2_lo, 16),
                                     vrshrn_n_u32(tmp2_hi, 16));
  const uint16x8_t Y2 = vaddq_u16(Y1, vdupq_n_u16(16));
  return vqmovn_u16(Y2);
}

static void ConvertRGB24ToY_NEON(const uint8_t* WEBP_RESTRICT rgb,
                                 uint8_t* WEBP_RESTRICT y, int width) {
  int i;
  for (i = 0; i + 8 <= width; i += 8, rgb += 3 * 8) {
    const uint8x8x3_t RGB = vld3_u8(rgb);
    const uint8x8_t Y = ConvertRGBToY_NEON(RGB.val[0], RGB.val[1], RGB.val[2]);
    vst1_u8(y + i, Y);
  }
  for (; i < width; ++i, rgb += 3) {
    y[i] = VP8RGBToY(rgb[0], rgb[1], rgb[2], YUV_HALF);
  }
}

static void ConvertBGR24ToY_NEON(const uint8_t* WEBP_RESTRICT bgr,
                                 uint8_t* WEBP_RESTRICT y, int width) {
  int i;
  for (i = 0; i + 8 <= width; i += 8, bgr += 3 * 8) {
    const uint8x8x3_t BGR = vld3_u8(bgr);
    const uint8x8_t Y = ConvertRGBToY_NEON(BGR.val[2], BGR.val[1], BGR.val[0]);
    vst1_u8(y + i, Y);
  }
  for (; i < width; ++i, bgr += 3) {
    y[i] = VP8RGBToY(bgr[2], bgr[1], bgr[0], YUV_HALF);
  }
}

static void ConvertARGBToY_NEON(const uint32_t* WEBP_RESTRICT argb,
                                uint8_t* WEBP_RESTRICT y, int width) {
  int i;
  for (i = 0; i + 8 <= width; i += 8) {
    const uint8x8x4_t RGB = vld4_u8((const uint8_t*)&argb[i]);
    const uint8x8_t Y = ConvertRGBToY_NEON(RGB.val[2], RGB.val[1], RGB.val[0]);
    vst1_u8(y + i, Y);
  }
  for (; i < width; ++i) {
    const uint32_t p = argb[i];
    y[i] = VP8RGBToY((p >> 16) & 0xff, (p >> 8) & 0xff, (p >>  0) & 0xff,
                     YUV_HALF);
  }
}

#define MULTIPLY_16b_PREAMBLE(r, g, b)                           \
  const int16x4_t r_lo = vreinterpret_s16_u16(vget_low_u16(r));  \
  const int16x4_t r_hi = vreinterpret_s16_u16(vget_high_u16(r)); \
  const int16x4_t g_lo = vreinterpret_s16_u16(vget_low_u16(g));  \
  const int16x4_t g_hi = vreinterpret_s16_u16(vget_high_u16(g)); \
  const int16x4_t b_lo = vreinterpret_s16_u16(vget_low_u16(b));  \
  const int16x4_t b_hi = vreinterpret_s16_u16(vget_high_u16(b))

#define MULTIPLY_16b(C0, C1, C2, CST, DST_s16) do {              \
  const int32x4_t tmp0_lo = vmull_n_s16(         r_lo, C0);      \
  const int32x4_t tmp0_hi = vmull_n_s16(         r_hi, C0);      \
  const int32x4_t tmp1_lo = vmlal_n_s16(tmp0_lo, g_lo, C1);      \
  const int32x4_t tmp1_hi = vmlal_n_s16(tmp0_hi, g_hi, C1);      \
  const int32x4_t tmp2_lo = vmlal_n_s16(tmp1_lo, b_lo, C2);      \
  const int32x4_t tmp2_hi = vmlal_n_s16(tmp1_hi, b_hi, C2);      \
  const int16x8_t tmp3 = vcombine_s16(vshrn_n_s32(tmp2_lo, 16),  \
                                      vshrn_n_s32(tmp2_hi, 16)); \
  DST_s16 = vaddq_s16(tmp3, vdupq_n_s16(CST));                   \
} while (0)

#define CONVERT_RGB_TO_UV(r, g, b, SHIFT, U_DST, V_DST) do {     \
  MULTIPLY_16b_PREAMBLE(r, g, b);                                \
  MULTIPLY_16b(-9719, -19081, 28800, 128 << SHIFT, U_DST);       \
  MULTIPLY_16b(28800, -24116, -4684, 128 << SHIFT, V_DST);       \
} while (0)

static void ConvertRGBA32ToUV_NEON(const uint16_t* WEBP_RESTRICT rgb,
                                   uint8_t* WEBP_RESTRICT u,
                                   uint8_t* WEBP_RESTRICT v, int width) {
  int i;
  for (i = 0; i + 8 <= width; i += 8, rgb += 4 * 8) {
    const uint16x8x4_t RGB = vld4q_u16((const uint16_t*)rgb);
    int16x8_t U, V;
    CONVERT_RGB_TO_UV(RGB.val[0], RGB.val[1], RGB.val[2], 2, U, V);
    vst1_u8(u + i, vqrshrun_n_s16(U, 2));
    vst1_u8(v + i, vqrshrun_n_s16(V, 2));
  }
  for (; i < width; i += 1, rgb += 4) {
    const int r = rgb[0], g = rgb[1], b = rgb[2];
    u[i] = VP8RGBToU(r, g, b, YUV_HALF << 2);
    v[i] = VP8RGBToV(r, g, b, YUV_HALF << 2);
  }
}

static void ConvertARGBToUV_NEON(const uint32_t* WEBP_RESTRICT argb,
                                 uint8_t* WEBP_RESTRICT u,
                                 uint8_t* WEBP_RESTRICT v,
                                 int src_width, int do_store) {
  int i;
  for (i = 0; i + 16 <= src_width; i += 16, u += 8, v += 8) {
    const uint8x16x4_t RGB = vld4q_u8((const uint8_t*)&argb[i]);
    const uint16x8_t R = vpaddlq_u8(RGB.val[2]);
    const uint16x8_t G = vpaddlq_u8(RGB.val[1]);
    const uint16x8_t B = vpaddlq_u8(RGB.val[0]);
    int16x8_t U_tmp, V_tmp;
    CONVERT_RGB_TO_UV(R, G, B, 1, U_tmp, V_tmp);
    {
      const uint8x8_t U = vqrshrun_n_s16(U_tmp, 1);
      const uint8x8_t V = vqrshrun_n_s16(V_tmp, 1);
      if (do_store) {
        vst1_u8(u, U);
        vst1_u8(v, V);
      } else {
        const uint8x8_t prev_u = vld1_u8(u);
        const uint8x8_t prev_v = vld1_u8(v);
        vst1_u8(u, vrhadd_u8(U, prev_u));
        vst1_u8(v, vrhadd_u8(V, prev_v));
      }
    }
  }
  if (i < src_width) {
    WebPConvertARGBToUV_C(argb + i, u, v, src_width - i, do_store);
  }
}

extern void WebPInitConvertARGBToYUVNEON(void);

WEBP_TSAN_IGNORE_FUNCTION void WebPInitConvertARGBToYUVNEON(void) {
  WebPConvertRGB24ToY = ConvertRGB24ToY_NEON;
  WebPConvertBGR24ToY = ConvertBGR24ToY_NEON;
  WebPConvertARGBToY = ConvertARGBToY_NEON;
  WebPConvertARGBToUV = ConvertARGBToUV_NEON;
  WebPConvertRGBA32ToUV = ConvertRGBA32ToUV_NEON;
}

#else

WEBP_DSP_INIT_STUB(WebPInitConvertARGBToYUVNEON)

#endif

#if defined(WEBP_USE_SSE2)
#include <emmintrin.h>

#include <stdlib.h>

static void ConvertYUV444ToRGB_SSE2(const __m128i* const Y0,
                                    const __m128i* const U0,
                                    const __m128i* const V0,
                                    __m128i* const R,
                                    __m128i* const G,
                                    __m128i* const B) {
  const __m128i k19077 = _mm_set1_epi16(19077);
  const __m128i k26149 = _mm_set1_epi16(26149);
  const __m128i k14234 = _mm_set1_epi16(14234);

  const __m128i k33050 = _mm_set1_epi16((short)33050);
  const __m128i k17685 = _mm_set1_epi16(17685);
  const __m128i k6419  = _mm_set1_epi16(6419);
  const __m128i k13320 = _mm_set1_epi16(13320);
  const __m128i k8708  = _mm_set1_epi16(8708);

  const __m128i Y1 = _mm_mulhi_epu16(*Y0, k19077);

  const __m128i R0 = _mm_mulhi_epu16(*V0, k26149);
  const __m128i R1 = _mm_sub_epi16(Y1, k14234);
  const __m128i R2 = _mm_add_epi16(R1, R0);

  const __m128i G0 = _mm_mulhi_epu16(*U0, k6419);
  const __m128i G1 = _mm_mulhi_epu16(*V0, k13320);
  const __m128i G2 = _mm_add_epi16(Y1, k8708);
  const __m128i G3 = _mm_add_epi16(G0, G1);
  const __m128i G4 = _mm_sub_epi16(G2, G3);

  const __m128i B0 = _mm_mulhi_epu16(*U0, k33050);
  const __m128i B1 = _mm_adds_epu16(B0, Y1);
  const __m128i B2 = _mm_subs_epu16(B1, k17685);

  *R = _mm_srai_epi16(R2, 6);
  *G = _mm_srai_epi16(G4, 6);
  *B = _mm_srli_epi16(B2, 6);
}

static WEBP_INLINE __m128i Load_HI_16_SSE2(const uint8_t* src) {
  const __m128i zero = _mm_setzero_si128();
  return _mm_unpacklo_epi8(zero, _mm_loadl_epi64((const __m128i*)src));
}

static WEBP_INLINE __m128i Load_UV_HI_8_SSE2(const uint8_t* src) {
  const __m128i zero = _mm_setzero_si128();
  const __m128i tmp0 = _mm_cvtsi32_si128(WebPMemToInt32(src));
  const __m128i tmp1 = _mm_unpacklo_epi8(zero, tmp0);
  return _mm_unpacklo_epi16(tmp1, tmp1);
}

static void YUV444ToRGB_SSE2(const uint8_t* WEBP_RESTRICT const y,
                             const uint8_t* WEBP_RESTRICT const u,
                             const uint8_t* WEBP_RESTRICT const v,
                             __m128i* const R, __m128i* const G,
                             __m128i* const B) {
  const __m128i Y0 = Load_HI_16_SSE2(y), U0 = Load_HI_16_SSE2(u),
                V0 = Load_HI_16_SSE2(v);
  ConvertYUV444ToRGB_SSE2(&Y0, &U0, &V0, R, G, B);
}

static void YUV420ToRGB_SSE2(const uint8_t* WEBP_RESTRICT const y,
                             const uint8_t* WEBP_RESTRICT const u,
                             const uint8_t* WEBP_RESTRICT const v,
                             __m128i* const R, __m128i* const G,
                             __m128i* const B) {
  const __m128i Y0 = Load_HI_16_SSE2(y), U0 = Load_UV_HI_8_SSE2(u),
                V0 = Load_UV_HI_8_SSE2(v);
  ConvertYUV444ToRGB_SSE2(&Y0, &U0, &V0, R, G, B);
}

static WEBP_INLINE void PackAndStore4_SSE2(const __m128i* const R,
                                           const __m128i* const G,
                                           const __m128i* const B,
                                           const __m128i* const A,
                                           uint8_t* WEBP_RESTRICT const dst) {
  const __m128i rb = _mm_packus_epi16(*R, *B);
  const __m128i ga = _mm_packus_epi16(*G, *A);
  const __m128i rg = _mm_unpacklo_epi8(rb, ga);
  const __m128i ba = _mm_unpackhi_epi8(rb, ga);
  const __m128i RGBA_lo = _mm_unpacklo_epi16(rg, ba);
  const __m128i RGBA_hi = _mm_unpackhi_epi16(rg, ba);
  _mm_storeu_si128((__m128i*)(dst +  0), RGBA_lo);
  _mm_storeu_si128((__m128i*)(dst + 16), RGBA_hi);
}

static WEBP_INLINE void PackAndStore4444_SSE2(
     const __m128i* const R, const __m128i* const G, const __m128i* const B,
     const __m128i* const A, uint8_t* WEBP_RESTRICT const dst) {
#if (WEBP_SWAP_16BIT_CSP == 0)
  const __m128i rg0 = _mm_packus_epi16(*R, *G);
  const __m128i ba0 = _mm_packus_epi16(*B, *A);
#else
  const __m128i rg0 = _mm_packus_epi16(*B, *A);
  const __m128i ba0 = _mm_packus_epi16(*R, *G);
#endif
  const __m128i mask_0xf0 = _mm_set1_epi8((char)0xf0);
  const __m128i rb1 = _mm_unpacklo_epi8(rg0, ba0);
  const __m128i ga1 = _mm_unpackhi_epi8(rg0, ba0);
  const __m128i rb2 = _mm_and_si128(rb1, mask_0xf0);
  const __m128i ga2 = _mm_srli_epi16(_mm_and_si128(ga1, mask_0xf0), 4);
  const __m128i rgba4444 = _mm_or_si128(rb2, ga2);
  _mm_storeu_si128((__m128i*)dst, rgba4444);
}

static WEBP_INLINE void PackAndStore565_SSE2(const __m128i* const R,
                                             const __m128i* const G,
                                             const __m128i* const B,
                                             uint8_t* WEBP_RESTRICT const dst) {
  const __m128i r0 = _mm_packus_epi16(*R, *R);
  const __m128i g0 = _mm_packus_epi16(*G, *G);
  const __m128i b0 = _mm_packus_epi16(*B, *B);
  const __m128i r1 = _mm_and_si128(r0, _mm_set1_epi8((char)0xf8));
  const __m128i b1 = _mm_and_si128(_mm_srli_epi16(b0, 3), _mm_set1_epi8(0x1f));
  const __m128i g1 =
      _mm_srli_epi16(_mm_and_si128(g0, _mm_set1_epi8((char)0xe0)), 5);
  const __m128i g2 = _mm_slli_epi16(_mm_and_si128(g0, _mm_set1_epi8(0x1c)), 3);
  const __m128i rg = _mm_or_si128(r1, g1);
  const __m128i gb = _mm_or_si128(g2, b1);
#if (WEBP_SWAP_16BIT_CSP == 0)
  const __m128i rgb565 = _mm_unpacklo_epi8(rg, gb);
#else
  const __m128i rgb565 = _mm_unpacklo_epi8(gb, rg);
#endif
  _mm_storeu_si128((__m128i*)dst, rgb565);
}

static WEBP_INLINE void PlanarTo24b_SSE2(__m128i* const in0, __m128i* const in1,
                                         __m128i* const in2, __m128i* const in3,
                                         __m128i* const in4, __m128i* const in5,
                                         uint8_t* WEBP_RESTRICT const rgb) {

  VP8PlanarTo24b_SSE2(in0, in1, in2, in3, in4, in5);

  _mm_storeu_si128((__m128i*)(rgb +  0), *in0);
  _mm_storeu_si128((__m128i*)(rgb + 16), *in1);
  _mm_storeu_si128((__m128i*)(rgb + 32), *in2);
  _mm_storeu_si128((__m128i*)(rgb + 48), *in3);
  _mm_storeu_si128((__m128i*)(rgb + 64), *in4);
  _mm_storeu_si128((__m128i*)(rgb + 80), *in5);
}

void VP8YuvToRgba32_SSE2(const uint8_t* WEBP_RESTRICT y,
                         const uint8_t* WEBP_RESTRICT u,
                         const uint8_t* WEBP_RESTRICT v,
                         uint8_t* WEBP_RESTRICT dst) {
  const __m128i kAlpha = _mm_set1_epi16(255);
  int n;
  for (n = 0; n < 32; n += 8, dst += 32) {
    __m128i R, G, B;
    YUV444ToRGB_SSE2(y + n, u + n, v + n, &R, &G, &B);
    PackAndStore4_SSE2(&R, &G, &B, &kAlpha, dst);
  }
}

void VP8YuvToBgra32_SSE2(const uint8_t* WEBP_RESTRICT y,
                         const uint8_t* WEBP_RESTRICT u,
                         const uint8_t* WEBP_RESTRICT v,
                         uint8_t* WEBP_RESTRICT dst) {
  const __m128i kAlpha = _mm_set1_epi16(255);
  int n;
  for (n = 0; n < 32; n += 8, dst += 32) {
    __m128i R, G, B;
    YUV444ToRGB_SSE2(y + n, u + n, v + n, &R, &G, &B);
    PackAndStore4_SSE2(&B, &G, &R, &kAlpha, dst);
  }
}

void VP8YuvToArgb32_SSE2(const uint8_t* WEBP_RESTRICT y,
                         const uint8_t* WEBP_RESTRICT u,
                         const uint8_t* WEBP_RESTRICT v,
                         uint8_t* WEBP_RESTRICT dst) {
  const __m128i kAlpha = _mm_set1_epi16(255);
  int n;
  for (n = 0; n < 32; n += 8, dst += 32) {
    __m128i R, G, B;
    YUV444ToRGB_SSE2(y + n, u + n, v + n, &R, &G, &B);
    PackAndStore4_SSE2(&kAlpha, &R, &G, &B, dst);
  }
}

void VP8YuvToRgba444432_SSE2(const uint8_t* WEBP_RESTRICT y,
                             const uint8_t* WEBP_RESTRICT u,
                             const uint8_t* WEBP_RESTRICT v,
                             uint8_t* WEBP_RESTRICT dst) {
  const __m128i kAlpha = _mm_set1_epi16(255);
  int n;
  for (n = 0; n < 32; n += 8, dst += 16) {
    __m128i R, G, B;
    YUV444ToRGB_SSE2(y + n, u + n, v + n, &R, &G, &B);
    PackAndStore4444_SSE2(&R, &G, &B, &kAlpha, dst);
  }
}

void VP8YuvToRgb56532_SSE2(const uint8_t* WEBP_RESTRICT y,
                           const uint8_t* WEBP_RESTRICT u,
                           const uint8_t* WEBP_RESTRICT v,
                           uint8_t* WEBP_RESTRICT dst) {
  int n;
  for (n = 0; n < 32; n += 8, dst += 16) {
    __m128i R, G, B;
    YUV444ToRGB_SSE2(y + n, u + n, v + n, &R, &G, &B);
    PackAndStore565_SSE2(&R, &G, &B, dst);
  }
}

void VP8YuvToRgb32_SSE2(const uint8_t* WEBP_RESTRICT y,
                        const uint8_t* WEBP_RESTRICT u,
                        const uint8_t* WEBP_RESTRICT v,
                        uint8_t* WEBP_RESTRICT dst) {
  __m128i R0, R1, R2, R3, G0, G1, G2, G3, B0, B1, B2, B3;
  __m128i rgb0, rgb1, rgb2, rgb3, rgb4, rgb5;

  YUV444ToRGB_SSE2(y + 0, u + 0, v + 0, &R0, &G0, &B0);
  YUV444ToRGB_SSE2(y + 8, u + 8, v + 8, &R1, &G1, &B1);
  YUV444ToRGB_SSE2(y + 16, u + 16, v + 16, &R2, &G2, &B2);
  YUV444ToRGB_SSE2(y + 24, u + 24, v + 24, &R3, &G3, &B3);

  rgb0 = _mm_packus_epi16(R0, R1);
  rgb1 = _mm_packus_epi16(R2, R3);
  rgb2 = _mm_packus_epi16(G0, G1);
  rgb3 = _mm_packus_epi16(G2, G3);
  rgb4 = _mm_packus_epi16(B0, B1);
  rgb5 = _mm_packus_epi16(B2, B3);

  PlanarTo24b_SSE2(&rgb0, &rgb1, &rgb2, &rgb3, &rgb4, &rgb5, dst);
}

void VP8YuvToBgr32_SSE2(const uint8_t* WEBP_RESTRICT y,
                        const uint8_t* WEBP_RESTRICT u,
                        const uint8_t* WEBP_RESTRICT v,
                        uint8_t* WEBP_RESTRICT dst) {
  __m128i R0, R1, R2, R3, G0, G1, G2, G3, B0, B1, B2, B3;
  __m128i bgr0, bgr1, bgr2, bgr3, bgr4, bgr5;

  YUV444ToRGB_SSE2(y +  0, u +  0, v +  0, &R0, &G0, &B0);
  YUV444ToRGB_SSE2(y +  8, u +  8, v +  8, &R1, &G1, &B1);
  YUV444ToRGB_SSE2(y + 16, u + 16, v + 16, &R2, &G2, &B2);
  YUV444ToRGB_SSE2(y + 24, u + 24, v + 24, &R3, &G3, &B3);

  bgr0 = _mm_packus_epi16(B0, B1);
  bgr1 = _mm_packus_epi16(B2, B3);
  bgr2 = _mm_packus_epi16(G0, G1);
  bgr3 = _mm_packus_epi16(G2, G3);
  bgr4 = _mm_packus_epi16(R0, R1);
  bgr5= _mm_packus_epi16(R2, R3);

  PlanarTo24b_SSE2(&bgr0, &bgr1, &bgr2, &bgr3, &bgr4, &bgr5, dst);
}

static void YuvToRgbaRow_SSE2(const uint8_t* WEBP_RESTRICT y,
                              const uint8_t* WEBP_RESTRICT u,
                              const uint8_t* WEBP_RESTRICT v,
                              uint8_t* WEBP_RESTRICT dst, int len) {
  const __m128i kAlpha = _mm_set1_epi16(255);
  int n;
  for (n = 0; n + 8 <= len; n += 8, dst += 32) {
    __m128i R, G, B;
    YUV420ToRGB_SSE2(y, u, v, &R, &G, &B);
    PackAndStore4_SSE2(&R, &G, &B, &kAlpha, dst);
    y += 8;
    u += 4;
    v += 4;
  }
  for (; n < len; ++n) {
    VP8YuvToRgba(y[0], u[0], v[0], dst);
    dst += 4;
    y += 1;
    u += (n & 1);
    v += (n & 1);
  }
}

static void YuvToBgraRow_SSE2(const uint8_t* WEBP_RESTRICT y,
                              const uint8_t* WEBP_RESTRICT u,
                              const uint8_t* WEBP_RESTRICT v,
                              uint8_t* WEBP_RESTRICT dst, int len) {
  const __m128i kAlpha = _mm_set1_epi16(255);
  int n;
  for (n = 0; n + 8 <= len; n += 8, dst += 32) {
    __m128i R, G, B;
    YUV420ToRGB_SSE2(y, u, v, &R, &G, &B);
    PackAndStore4_SSE2(&B, &G, &R, &kAlpha, dst);
    y += 8;
    u += 4;
    v += 4;
  }
  for (; n < len; ++n) {
    VP8YuvToBgra(y[0], u[0], v[0], dst);
    dst += 4;
    y += 1;
    u += (n & 1);
    v += (n & 1);
  }
}

static void YuvToArgbRow_SSE2(const uint8_t* WEBP_RESTRICT y,
                              const uint8_t* WEBP_RESTRICT u,
                              const uint8_t* WEBP_RESTRICT v,
                              uint8_t* WEBP_RESTRICT dst, int len) {
  const __m128i kAlpha = _mm_set1_epi16(255);
  int n;
  for (n = 0; n + 8 <= len; n += 8, dst += 32) {
    __m128i R, G, B;
    YUV420ToRGB_SSE2(y, u, v, &R, &G, &B);
    PackAndStore4_SSE2(&kAlpha, &R, &G, &B, dst);
    y += 8;
    u += 4;
    v += 4;
  }
  for (; n < len; ++n) {
    VP8YuvToArgb(y[0], u[0], v[0], dst);
    dst += 4;
    y += 1;
    u += (n & 1);
    v += (n & 1);
  }
}

static void YuvToRgbRow_SSE2(const uint8_t* WEBP_RESTRICT y,
                             const uint8_t* WEBP_RESTRICT u,
                             const uint8_t* WEBP_RESTRICT v,
                             uint8_t* WEBP_RESTRICT dst, int len) {
  int n;
  for (n = 0; n + 32 <= len; n += 32, dst += 32 * 3) {
    __m128i R0, R1, R2, R3, G0, G1, G2, G3, B0, B1, B2, B3;
    __m128i rgb0, rgb1, rgb2, rgb3, rgb4, rgb5;

    YUV420ToRGB_SSE2(y +  0, u +  0, v +  0, &R0, &G0, &B0);
    YUV420ToRGB_SSE2(y +  8, u +  4, v +  4, &R1, &G1, &B1);
    YUV420ToRGB_SSE2(y + 16, u +  8, v +  8, &R2, &G2, &B2);
    YUV420ToRGB_SSE2(y + 24, u + 12, v + 12, &R3, &G3, &B3);

    rgb0 = _mm_packus_epi16(R0, R1);
    rgb1 = _mm_packus_epi16(R2, R3);
    rgb2 = _mm_packus_epi16(G0, G1);
    rgb3 = _mm_packus_epi16(G2, G3);
    rgb4 = _mm_packus_epi16(B0, B1);
    rgb5 = _mm_packus_epi16(B2, B3);

    PlanarTo24b_SSE2(&rgb0, &rgb1, &rgb2, &rgb3, &rgb4, &rgb5, dst);

    y += 32;
    u += 16;
    v += 16;
  }
  for (; n < len; ++n) {
    VP8YuvToRgb(y[0], u[0], v[0], dst);
    dst += 3;
    y += 1;
    u += (n & 1);
    v += (n & 1);
  }
}

static void YuvToBgrRow_SSE2(const uint8_t* WEBP_RESTRICT y,
                             const uint8_t* WEBP_RESTRICT u,
                             const uint8_t* WEBP_RESTRICT v,
                             uint8_t* WEBP_RESTRICT dst, int len) {
  int n;
  for (n = 0; n + 32 <= len; n += 32, dst += 32 * 3) {
    __m128i R0, R1, R2, R3, G0, G1, G2, G3, B0, B1, B2, B3;
    __m128i bgr0, bgr1, bgr2, bgr3, bgr4, bgr5;

    YUV420ToRGB_SSE2(y +  0, u +  0, v +  0, &R0, &G0, &B0);
    YUV420ToRGB_SSE2(y +  8, u +  4, v +  4, &R1, &G1, &B1);
    YUV420ToRGB_SSE2(y + 16, u +  8, v +  8, &R2, &G2, &B2);
    YUV420ToRGB_SSE2(y + 24, u + 12, v + 12, &R3, &G3, &B3);

    bgr0 = _mm_packus_epi16(B0, B1);
    bgr1 = _mm_packus_epi16(B2, B3);
    bgr2 = _mm_packus_epi16(G0, G1);
    bgr3 = _mm_packus_epi16(G2, G3);
    bgr4 = _mm_packus_epi16(R0, R1);
    bgr5 = _mm_packus_epi16(R2, R3);

    PlanarTo24b_SSE2(&bgr0, &bgr1, &bgr2, &bgr3, &bgr4, &bgr5, dst);

    y += 32;
    u += 16;
    v += 16;
  }
  for (; n < len; ++n) {
    VP8YuvToBgr(y[0], u[0], v[0], dst);
    dst += 3;
    y += 1;
    u += (n & 1);
    v += (n & 1);
  }
}

extern void WebPInitSamplersSSE2(void);

WEBP_TSAN_IGNORE_FUNCTION void WebPInitSamplersSSE2(void) {
  WebPSamplers[MODE_RGB]  = YuvToRgbRow_SSE2;
  WebPSamplers[MODE_RGBA] = YuvToRgbaRow_SSE2;
  WebPSamplers[MODE_BGR]  = YuvToBgrRow_SSE2;
  WebPSamplers[MODE_BGRA] = YuvToBgraRow_SSE2;
  WebPSamplers[MODE_ARGB] = YuvToArgbRow_SSE2;
}

#define LOAD_16(src) _mm_loadu_si128((const __m128i*)(src))

#define STORE_16(V, dst) _mm_storeu_si128((__m128i*)(dst), (V))

static WEBP_INLINE void RGB24PackedToPlanarHelper_SSE2(
    const __m128i* const in , __m128i* const out ) {
  out[0] = _mm_unpacklo_epi8(in[0], in[3]);
  out[1] = _mm_unpackhi_epi8(in[0], in[3]);
  out[2] = _mm_unpacklo_epi8(in[1], in[4]);
  out[3] = _mm_unpackhi_epi8(in[1], in[4]);
  out[4] = _mm_unpacklo_epi8(in[2], in[5]);
  out[5] = _mm_unpackhi_epi8(in[2], in[5]);
}

static WEBP_INLINE void RGB24PackedToPlanar_SSE2(
    const uint8_t* WEBP_RESTRICT const rgb, __m128i* const out ) {
  __m128i tmp[6];
  tmp[0] = _mm_loadu_si128((const __m128i*)(rgb +  0));
  tmp[1] = _mm_loadu_si128((const __m128i*)(rgb + 16));
  tmp[2] = _mm_loadu_si128((const __m128i*)(rgb + 32));
  tmp[3] = _mm_loadu_si128((const __m128i*)(rgb + 48));
  tmp[4] = _mm_loadu_si128((const __m128i*)(rgb + 64));
  tmp[5] = _mm_loadu_si128((const __m128i*)(rgb + 80));

  RGB24PackedToPlanarHelper_SSE2(tmp, out);
  RGB24PackedToPlanarHelper_SSE2(out, tmp);
  RGB24PackedToPlanarHelper_SSE2(tmp, out);
  RGB24PackedToPlanarHelper_SSE2(out, tmp);
  RGB24PackedToPlanarHelper_SSE2(tmp, out);
}

static WEBP_INLINE void RGB32PackedToPlanar_SSE2(
    const uint32_t* WEBP_RESTRICT const argb, __m128i* const rgb ) {
  const __m128i zero = _mm_setzero_si128();
  __m128i a0 = LOAD_16(argb + 0);
  __m128i a1 = LOAD_16(argb + 4);
  __m128i a2 = LOAD_16(argb + 8);
  __m128i a3 = LOAD_16(argb + 12);
  VP8L32bToPlanar_SSE2(&a0, &a1, &a2, &a3);
  rgb[0] = _mm_unpacklo_epi8(a1, zero);
  rgb[1] = _mm_unpackhi_epi8(a1, zero);
  rgb[2] = _mm_unpacklo_epi8(a2, zero);
  rgb[3] = _mm_unpackhi_epi8(a2, zero);
  rgb[4] = _mm_unpacklo_epi8(a3, zero);
  rgb[5] = _mm_unpackhi_epi8(a3, zero);
}

#define TRANSFORM(RG_LO, RG_HI, GB_LO, GB_HI, MULT_RG, MULT_GB, \
                  ROUNDER, DESCALE_FIX, OUT) do {               \
  const __m128i V0_lo = _mm_madd_epi16(RG_LO, MULT_RG);         \
  const __m128i V0_hi = _mm_madd_epi16(RG_HI, MULT_RG);         \
  const __m128i V1_lo = _mm_madd_epi16(GB_LO, MULT_GB);         \
  const __m128i V1_hi = _mm_madd_epi16(GB_HI, MULT_GB);         \
  const __m128i V2_lo = _mm_add_epi32(V0_lo, V1_lo);            \
  const __m128i V2_hi = _mm_add_epi32(V0_hi, V1_hi);            \
  const __m128i V3_lo = _mm_add_epi32(V2_lo, ROUNDER);          \
  const __m128i V3_hi = _mm_add_epi32(V2_hi, ROUNDER);          \
  const __m128i V5_lo = _mm_srai_epi32(V3_lo, DESCALE_FIX);     \
  const __m128i V5_hi = _mm_srai_epi32(V3_hi, DESCALE_FIX);     \
  (OUT) = _mm_packs_epi32(V5_lo, V5_hi);                        \
} while (0)

#define MK_CST_16(A, B) _mm_set_epi16((B), (A), (B), (A), (B), (A), (B), (A))
static WEBP_INLINE void ConvertRGBToY_SSE2(const __m128i* const R,
                                           const __m128i* const G,
                                           const __m128i* const B,
                                           __m128i* const Y) {
  const __m128i kRG_y = MK_CST_16(16839, 33059 - 16384);
  const __m128i kGB_y = MK_CST_16(16384, 6420);
  const __m128i kHALF_Y = _mm_set1_epi32((16 << YUV_FIX) + YUV_HALF);

  const __m128i RG_lo = _mm_unpacklo_epi16(*R, *G);
  const __m128i RG_hi = _mm_unpackhi_epi16(*R, *G);
  const __m128i GB_lo = _mm_unpacklo_epi16(*G, *B);
  const __m128i GB_hi = _mm_unpackhi_epi16(*G, *B);
  TRANSFORM(RG_lo, RG_hi, GB_lo, GB_hi, kRG_y, kGB_y, kHALF_Y, YUV_FIX, *Y);
}

static WEBP_INLINE void ConvertRGBToUV_SSE2(const __m128i* const R,
                                            const __m128i* const G,
                                            const __m128i* const B,
                                            __m128i* const U,
                                            __m128i* const V) {
  const __m128i kRG_u = MK_CST_16(-9719, -19081);
  const __m128i kGB_u = MK_CST_16(0, 28800);
  const __m128i kRG_v = MK_CST_16(28800, 0);
  const __m128i kGB_v = MK_CST_16(-24116, -4684);
  const __m128i kHALF_UV = _mm_set1_epi32(((128 << YUV_FIX) + YUV_HALF) << 2);

  const __m128i RG_lo = _mm_unpacklo_epi16(*R, *G);
  const __m128i RG_hi = _mm_unpackhi_epi16(*R, *G);
  const __m128i GB_lo = _mm_unpacklo_epi16(*G, *B);
  const __m128i GB_hi = _mm_unpackhi_epi16(*G, *B);
  TRANSFORM(RG_lo, RG_hi, GB_lo, GB_hi, kRG_u, kGB_u,
            kHALF_UV, YUV_FIX + 2, *U);
  TRANSFORM(RG_lo, RG_hi, GB_lo, GB_hi, kRG_v, kGB_v,
            kHALF_UV, YUV_FIX + 2, *V);
}

#undef MK_CST_16
#undef TRANSFORM

static void ConvertRGB24ToY_SSE2(const uint8_t* WEBP_RESTRICT rgb,
                                 uint8_t* WEBP_RESTRICT y, int width) {
  const int max_width = width & ~31;
  int i;
  for (i = 0; i < max_width; rgb += 3 * 16 * 2) {
    __m128i rgb_plane[6];
    int j;

    RGB24PackedToPlanar_SSE2(rgb, rgb_plane);

    for (j = 0; j < 2; ++j, i += 16) {
      const __m128i zero = _mm_setzero_si128();
      __m128i r, g, b, Y0, Y1;

      r = _mm_unpacklo_epi8(rgb_plane[0 + j], zero);
      g = _mm_unpacklo_epi8(rgb_plane[2 + j], zero);
      b = _mm_unpacklo_epi8(rgb_plane[4 + j], zero);
      ConvertRGBToY_SSE2(&r, &g, &b, &Y0);

      r = _mm_unpackhi_epi8(rgb_plane[0 + j], zero);
      g = _mm_unpackhi_epi8(rgb_plane[2 + j], zero);
      b = _mm_unpackhi_epi8(rgb_plane[4 + j], zero);
      ConvertRGBToY_SSE2(&r, &g, &b, &Y1);

      STORE_16(_mm_packus_epi16(Y0, Y1), y + i);
    }
  }
  for (; i < width; ++i, rgb += 3) {
    y[i] = VP8RGBToY(rgb[0], rgb[1], rgb[2], YUV_HALF);
  }
}

static void ConvertBGR24ToY_SSE2(const uint8_t* WEBP_RESTRICT bgr,
                                 uint8_t* WEBP_RESTRICT y, int width) {
  const int max_width = width & ~31;
  int i;
  for (i = 0; i < max_width; bgr += 3 * 16 * 2) {
    __m128i bgr_plane[6];
    int j;

    RGB24PackedToPlanar_SSE2(bgr, bgr_plane);

    for (j = 0; j < 2; ++j, i += 16) {
      const __m128i zero = _mm_setzero_si128();
      __m128i r, g, b, Y0, Y1;

      b = _mm_unpacklo_epi8(bgr_plane[0 + j], zero);
      g = _mm_unpacklo_epi8(bgr_plane[2 + j], zero);
      r = _mm_unpacklo_epi8(bgr_plane[4 + j], zero);
      ConvertRGBToY_SSE2(&r, &g, &b, &Y0);

      b = _mm_unpackhi_epi8(bgr_plane[0 + j], zero);
      g = _mm_unpackhi_epi8(bgr_plane[2 + j], zero);
      r = _mm_unpackhi_epi8(bgr_plane[4 + j], zero);
      ConvertRGBToY_SSE2(&r, &g, &b, &Y1);

      STORE_16(_mm_packus_epi16(Y0, Y1), y + i);
    }
  }
  for (; i < width; ++i, bgr += 3) {
    y[i] = VP8RGBToY(bgr[2], bgr[1], bgr[0], YUV_HALF);
  }
}

static void ConvertARGBToY_SSE2(const uint32_t* WEBP_RESTRICT argb,
                                uint8_t* WEBP_RESTRICT y, int width) {
  const int max_width = width & ~15;
  int i;
  for (i = 0; i < max_width; i += 16) {
    __m128i Y0, Y1, rgb[6];
    RGB32PackedToPlanar_SSE2(&argb[i], rgb);
    ConvertRGBToY_SSE2(&rgb[0], &rgb[2], &rgb[4], &Y0);
    ConvertRGBToY_SSE2(&rgb[1], &rgb[3], &rgb[5], &Y1);
    STORE_16(_mm_packus_epi16(Y0, Y1), y + i);
  }
  for (; i < width; ++i) {
    const uint32_t p = argb[i];
    y[i] = VP8RGBToY((p >> 16) & 0xff, (p >> 8) & 0xff, (p >>  0) & 0xff,
                     YUV_HALF);
  }
}

static void HorizontalAddPack_SSE2(const __m128i* const A,
                                   const __m128i* const B,
                                   __m128i* const out) {
  const __m128i k2 = _mm_set1_epi16(2);
  const __m128i C = _mm_madd_epi16(*A, k2);
  const __m128i D = _mm_madd_epi16(*B, k2);
  *out = _mm_packs_epi32(C, D);
}

static void ConvertARGBToUV_SSE2(const uint32_t* WEBP_RESTRICT argb,
                                 uint8_t* WEBP_RESTRICT u,
                                 uint8_t* WEBP_RESTRICT v,
                                 int src_width, int do_store) {
  const int max_width = src_width & ~31;
  int i;
  for (i = 0; i < max_width; i += 32, u += 16, v += 16) {
    __m128i rgb[6], U0, V0, U1, V1;
    RGB32PackedToPlanar_SSE2(&argb[i], rgb);
    HorizontalAddPack_SSE2(&rgb[0], &rgb[1], &rgb[0]);
    HorizontalAddPack_SSE2(&rgb[2], &rgb[3], &rgb[2]);
    HorizontalAddPack_SSE2(&rgb[4], &rgb[5], &rgb[4]);
    ConvertRGBToUV_SSE2(&rgb[0], &rgb[2], &rgb[4], &U0, &V0);

    RGB32PackedToPlanar_SSE2(&argb[i + 16], rgb);
    HorizontalAddPack_SSE2(&rgb[0], &rgb[1], &rgb[0]);
    HorizontalAddPack_SSE2(&rgb[2], &rgb[3], &rgb[2]);
    HorizontalAddPack_SSE2(&rgb[4], &rgb[5], &rgb[4]);
    ConvertRGBToUV_SSE2(&rgb[0], &rgb[2], &rgb[4], &U1, &V1);

    U0 = _mm_packus_epi16(U0, U1);
    V0 = _mm_packus_epi16(V0, V1);
    if (!do_store) {
      const __m128i prev_u = LOAD_16(u);
      const __m128i prev_v = LOAD_16(v);
      U0 = _mm_avg_epu8(U0, prev_u);
      V0 = _mm_avg_epu8(V0, prev_v);
    }
    STORE_16(U0, u);
    STORE_16(V0, v);
  }
  if (i < src_width) {
    WebPConvertARGBToUV_C(argb + i, u, v, src_width - i, do_store);
  }
}

static WEBP_INLINE void RGBA32PackedToPlanar_16b_SSE2(
    const uint16_t* WEBP_RESTRICT const rgbx,
    __m128i* const r, __m128i* const g, __m128i* const b) {
  const __m128i in0 = LOAD_16(rgbx +  0);
  const __m128i in1 = LOAD_16(rgbx +  8);
  const __m128i in2 = LOAD_16(rgbx + 16);
  const __m128i in3 = LOAD_16(rgbx + 24);

  const __m128i A0 = _mm_unpacklo_epi16(in0, in1);
  const __m128i A1 = _mm_unpackhi_epi16(in0, in1);
  const __m128i A2 = _mm_unpacklo_epi16(in2, in3);
  const __m128i A3 = _mm_unpackhi_epi16(in2, in3);
  const __m128i B0 = _mm_unpacklo_epi16(A0, A1);
  const __m128i B1 = _mm_unpackhi_epi16(A0, A1);
  const __m128i B2 = _mm_unpacklo_epi16(A2, A3);
  const __m128i B3 = _mm_unpackhi_epi16(A2, A3);
  *r = _mm_unpacklo_epi64(B0, B2);
  *g = _mm_unpackhi_epi64(B0, B2);
  *b = _mm_unpacklo_epi64(B1, B3);
}

static void ConvertRGBA32ToUV_SSE2(const uint16_t* WEBP_RESTRICT rgb,
                                   uint8_t* WEBP_RESTRICT u,
                                   uint8_t* WEBP_RESTRICT v, int width) {
  const int max_width = width & ~15;
  const uint16_t* const last_rgb = rgb + 4 * max_width;
  while (rgb < last_rgb) {
    __m128i r, g, b, U0, V0, U1, V1;
    RGBA32PackedToPlanar_16b_SSE2(rgb +  0, &r, &g, &b);
    ConvertRGBToUV_SSE2(&r, &g, &b, &U0, &V0);
    RGBA32PackedToPlanar_16b_SSE2(rgb + 32, &r, &g, &b);
    ConvertRGBToUV_SSE2(&r, &g, &b, &U1, &V1);
    STORE_16(_mm_packus_epi16(U0, U1), u);
    STORE_16(_mm_packus_epi16(V0, V1), v);
    u += 16;
    v += 16;
    rgb += 2 * 32;
  }
  if (max_width < width) {
    WebPConvertRGBA32ToUV_C(rgb, u, v, width - max_width);
  }
}

extern void WebPInitConvertARGBToYUVSSE2(void);

WEBP_TSAN_IGNORE_FUNCTION void WebPInitConvertARGBToYUVSSE2(void) {
  WebPConvertARGBToY = ConvertARGBToY_SSE2;
  WebPConvertARGBToUV = ConvertARGBToUV_SSE2;

  WebPConvertRGB24ToY = ConvertRGB24ToY_SSE2;
  WebPConvertBGR24ToY = ConvertBGR24ToY_SSE2;

  WebPConvertRGBA32ToUV = ConvertRGBA32ToUV_SSE2;
}

#else

WEBP_DSP_INIT_STUB(WebPInitSamplersSSE2)
WEBP_DSP_INIT_STUB(WebPInitConvertARGBToYUVSSE2)

#endif

#if defined(WEBP_USE_SSE41)
#include <emmintrin.h>
#include <smmintrin.h>

#include <stdlib.h>

#ifndef WEBP_DSP_COMMON_SSE41_H_
#define WEBP_DSP_COMMON_SSE41_H_

#ifdef __cplusplus
extern "C" {
#endif

#if defined(WEBP_USE_SSE41)
#include <smmintrin.h>

#define WEBP_SSE41_SHUFF(OUT, IN0, IN1)    \
  OUT##0 = _mm_shuffle_epi8(*IN0, shuff0); \
  OUT##1 = _mm_shuffle_epi8(*IN0, shuff1); \
  OUT##2 = _mm_shuffle_epi8(*IN0, shuff2); \
  OUT##3 = _mm_shuffle_epi8(*IN1, shuff0); \
  OUT##4 = _mm_shuffle_epi8(*IN1, shuff1); \
  OUT##5 = _mm_shuffle_epi8(*IN1, shuff2);

static WEBP_INLINE void VP8PlanarTo24b_SSE41(
    __m128i* const in0, __m128i* const in1, __m128i* const in2,
    __m128i* const in3, __m128i* const in4, __m128i* const in5) {
  __m128i R0, R1, R2, R3, R4, R5;
  __m128i G0, G1, G2, G3, G4, G5;
  __m128i B0, B1, B2, B3, B4, B5;

  {
    const __m128i shuff0 = _mm_set_epi8(
        5, -1, -1, 4, -1, -1, 3, -1, -1, 2, -1, -1, 1, -1, -1, 0);
    const __m128i shuff1 = _mm_set_epi8(
        -1, 10, -1, -1, 9, -1, -1, 8, -1, -1, 7, -1, -1, 6, -1, -1);
    const __m128i shuff2 = _mm_set_epi8(
     -1, -1, 15, -1, -1, 14, -1, -1, 13, -1, -1, 12, -1, -1, 11, -1);
    WEBP_SSE41_SHUFF(R, in0, in1)
  }

  {

    const __m128i shuff0 = _mm_set_epi8(
        -1, -1, 4, -1, -1, 3, -1, -1, 2, -1, -1, 1, -1, -1, 0, -1);
    const __m128i shuff1 = _mm_set_epi8(
        10, -1, -1, 9, -1, -1, 8, -1, -1, 7, -1, -1, 6, -1, -1, 5);
    const __m128i shuff2 = _mm_set_epi8(
     -1, 15, -1, -1, 14, -1, -1, 13, -1, -1, 12, -1, -1, 11, -1, -1);
    WEBP_SSE41_SHUFF(G, in2, in3)
  }

  {
    const __m128i shuff0 = _mm_set_epi8(
        -1, 4, -1, -1, 3, -1, -1, 2, -1, -1, 1, -1, -1, 0, -1, -1);
    const __m128i shuff1 = _mm_set_epi8(
        -1, -1, 9, -1, -1, 8, -1, -1, 7, -1, -1, 6, -1, -1, 5, -1);
    const __m128i shuff2 = _mm_set_epi8(
      15, -1, -1, 14, -1, -1, 13, -1, -1, 12, -1, -1, 11, -1, -1, 10);
    WEBP_SSE41_SHUFF(B, in4, in5)
  }

  {
    const __m128i RG0 = _mm_or_si128(R0, G0);
    const __m128i RG1 = _mm_or_si128(R1, G1);
    const __m128i RG2 = _mm_or_si128(R2, G2);
    const __m128i RG3 = _mm_or_si128(R3, G3);
    const __m128i RG4 = _mm_or_si128(R4, G4);
    const __m128i RG5 = _mm_or_si128(R5, G5);
    *in0 = _mm_or_si128(RG0, B0);
    *in1 = _mm_or_si128(RG1, B1);
    *in2 = _mm_or_si128(RG2, B2);
    *in3 = _mm_or_si128(RG3, B3);
    *in4 = _mm_or_si128(RG4, B4);
    *in5 = _mm_or_si128(RG5, B5);
  }
}

#undef WEBP_SSE41_SHUFF

static WEBP_INLINE void VP8L32bToPlanar_SSE41(__m128i* const in0,
                                              __m128i* const in1,
                                              __m128i* const in2,
                                              __m128i* const in3) {

  const __m128i shuff0 =
      _mm_set_epi8(15, 11, 7, 3, 14, 10, 6, 2, 13, 9, 5, 1, 12, 8, 4, 0);
  const __m128i A0 = _mm_shuffle_epi8(*in0, shuff0);
  const __m128i A1 = _mm_shuffle_epi8(*in1, shuff0);
  const __m128i A2 = _mm_shuffle_epi8(*in2, shuff0);
  const __m128i A3 = _mm_shuffle_epi8(*in3, shuff0);

  const __m128i B0 = _mm_unpacklo_epi32(A0, A1);
  const __m128i B1 = _mm_unpackhi_epi32(A0, A1);
  const __m128i B2 = _mm_unpacklo_epi32(A2, A3);
  const __m128i B3 = _mm_unpackhi_epi32(A2, A3);
  *in3 = _mm_unpacklo_epi64(B0, B2);
  *in2 = _mm_unpackhi_epi64(B0, B2);
  *in1 = _mm_unpacklo_epi64(B1, B3);
  *in0 = _mm_unpackhi_epi64(B1, B3);
}

#endif

#ifdef __cplusplus
}
#endif

#endif

static void ConvertYUV444ToRGB_SSE41(const __m128i* const Y0,
                                     const __m128i* const U0,
                                     const __m128i* const V0,
                                     __m128i* const R,
                                     __m128i* const G,
                                     __m128i* const B) {
  const __m128i k19077 = _mm_set1_epi16(19077);
  const __m128i k26149 = _mm_set1_epi16(26149);
  const __m128i k14234 = _mm_set1_epi16(14234);

  const __m128i k33050 = _mm_set1_epi16((short)33050);
  const __m128i k17685 = _mm_set1_epi16(17685);
  const __m128i k6419  = _mm_set1_epi16(6419);
  const __m128i k13320 = _mm_set1_epi16(13320);
  const __m128i k8708  = _mm_set1_epi16(8708);

  const __m128i Y1 = _mm_mulhi_epu16(*Y0, k19077);

  const __m128i R0 = _mm_mulhi_epu16(*V0, k26149);
  const __m128i R1 = _mm_sub_epi16(Y1, k14234);
  const __m128i R2 = _mm_add_epi16(R1, R0);

  const __m128i G0 = _mm_mulhi_epu16(*U0, k6419);
  const __m128i G1 = _mm_mulhi_epu16(*V0, k13320);
  const __m128i G2 = _mm_add_epi16(Y1, k8708);
  const __m128i G3 = _mm_add_epi16(G0, G1);
  const __m128i G4 = _mm_sub_epi16(G2, G3);

  const __m128i B0 = _mm_mulhi_epu16(*U0, k33050);
  const __m128i B1 = _mm_adds_epu16(B0, Y1);
  const __m128i B2 = _mm_subs_epu16(B1, k17685);

  *R = _mm_srai_epi16(R2, 6);
  *G = _mm_srai_epi16(G4, 6);
  *B = _mm_srli_epi16(B2, 6);
}

static WEBP_INLINE __m128i Load_HI_16_SSE41(const uint8_t* src) {
  const __m128i zero = _mm_setzero_si128();
  return _mm_unpacklo_epi8(zero, _mm_loadl_epi64((const __m128i*)src));
}

static WEBP_INLINE __m128i Load_UV_HI_8_SSE41(const uint8_t* src) {
  const __m128i zero = _mm_setzero_si128();
  const __m128i tmp0 = _mm_cvtsi32_si128(WebPMemToInt32(src));
  const __m128i tmp1 = _mm_unpacklo_epi8(zero, tmp0);
  return _mm_unpacklo_epi16(tmp1, tmp1);
}

static void YUV444ToRGB_SSE41(const uint8_t* WEBP_RESTRICT const y,
                              const uint8_t* WEBP_RESTRICT const u,
                              const uint8_t* WEBP_RESTRICT const v,
                              __m128i* const R, __m128i* const G,
                              __m128i* const B) {
  const __m128i Y0 = Load_HI_16_SSE41(y), U0 = Load_HI_16_SSE41(u),
                V0 = Load_HI_16_SSE41(v);
  ConvertYUV444ToRGB_SSE41(&Y0, &U0, &V0, R, G, B);
}

static void YUV420ToRGB_SSE41(const uint8_t* WEBP_RESTRICT const y,
                              const uint8_t* WEBP_RESTRICT const u,
                              const uint8_t* WEBP_RESTRICT const v,
                              __m128i* const R, __m128i* const G,
                              __m128i* const B) {
  const __m128i Y0 = Load_HI_16_SSE41(y), U0 = Load_UV_HI_8_SSE41(u),
                V0 = Load_UV_HI_8_SSE41(v);
  ConvertYUV444ToRGB_SSE41(&Y0, &U0, &V0, R, G, B);
}

static WEBP_INLINE void PlanarTo24b_SSE41(
    __m128i* const in0, __m128i* const in1, __m128i* const in2,
    __m128i* const in3, __m128i* const in4, __m128i* const in5,
    uint8_t* WEBP_RESTRICT const rgb) {

  VP8PlanarTo24b_SSE41(in0, in1, in2, in3, in4, in5);

  _mm_storeu_si128((__m128i*)(rgb +  0), *in0);
  _mm_storeu_si128((__m128i*)(rgb + 16), *in1);
  _mm_storeu_si128((__m128i*)(rgb + 32), *in2);
  _mm_storeu_si128((__m128i*)(rgb + 48), *in3);
  _mm_storeu_si128((__m128i*)(rgb + 64), *in4);
  _mm_storeu_si128((__m128i*)(rgb + 80), *in5);
}

void VP8YuvToRgb32_SSE41(const uint8_t* WEBP_RESTRICT y,
                         const uint8_t* WEBP_RESTRICT u,
                         const uint8_t* WEBP_RESTRICT v,
                         uint8_t* WEBP_RESTRICT dst) {
  __m128i R0, R1, R2, R3, G0, G1, G2, G3, B0, B1, B2, B3;
  __m128i rgb0, rgb1, rgb2, rgb3, rgb4, rgb5;

  YUV444ToRGB_SSE41(y + 0, u + 0, v + 0, &R0, &G0, &B0);
  YUV444ToRGB_SSE41(y + 8, u + 8, v + 8, &R1, &G1, &B1);
  YUV444ToRGB_SSE41(y + 16, u + 16, v + 16, &R2, &G2, &B2);
  YUV444ToRGB_SSE41(y + 24, u + 24, v + 24, &R3, &G3, &B3);

  rgb0 = _mm_packus_epi16(R0, R1);
  rgb1 = _mm_packus_epi16(R2, R3);
  rgb2 = _mm_packus_epi16(G0, G1);
  rgb3 = _mm_packus_epi16(G2, G3);
  rgb4 = _mm_packus_epi16(B0, B1);
  rgb5 = _mm_packus_epi16(B2, B3);

  PlanarTo24b_SSE41(&rgb0, &rgb1, &rgb2, &rgb3, &rgb4, &rgb5, dst);
}

void VP8YuvToBgr32_SSE41(const uint8_t* WEBP_RESTRICT y,
                         const uint8_t* WEBP_RESTRICT u,
                         const uint8_t* WEBP_RESTRICT v,
                         uint8_t* WEBP_RESTRICT dst) {
  __m128i R0, R1, R2, R3, G0, G1, G2, G3, B0, B1, B2, B3;
  __m128i bgr0, bgr1, bgr2, bgr3, bgr4, bgr5;

  YUV444ToRGB_SSE41(y +  0, u +  0, v +  0, &R0, &G0, &B0);
  YUV444ToRGB_SSE41(y +  8, u +  8, v +  8, &R1, &G1, &B1);
  YUV444ToRGB_SSE41(y + 16, u + 16, v + 16, &R2, &G2, &B2);
  YUV444ToRGB_SSE41(y + 24, u + 24, v + 24, &R3, &G3, &B3);

  bgr0 = _mm_packus_epi16(B0, B1);
  bgr1 = _mm_packus_epi16(B2, B3);
  bgr2 = _mm_packus_epi16(G0, G1);
  bgr3 = _mm_packus_epi16(G2, G3);
  bgr4 = _mm_packus_epi16(R0, R1);
  bgr5= _mm_packus_epi16(R2, R3);

  PlanarTo24b_SSE41(&bgr0, &bgr1, &bgr2, &bgr3, &bgr4, &bgr5, dst);
}

static void YuvToRgbRow_SSE41(const uint8_t* WEBP_RESTRICT y,
                              const uint8_t* WEBP_RESTRICT u,
                              const uint8_t* WEBP_RESTRICT v,
                              uint8_t* WEBP_RESTRICT dst, int len) {
  int n;
  for (n = 0; n + 32 <= len; n += 32, dst += 32 * 3) {
    __m128i R0, R1, R2, R3, G0, G1, G2, G3, B0, B1, B2, B3;
    __m128i rgb0, rgb1, rgb2, rgb3, rgb4, rgb5;

    YUV420ToRGB_SSE41(y +  0, u +  0, v +  0, &R0, &G0, &B0);
    YUV420ToRGB_SSE41(y +  8, u +  4, v +  4, &R1, &G1, &B1);
    YUV420ToRGB_SSE41(y + 16, u +  8, v +  8, &R2, &G2, &B2);
    YUV420ToRGB_SSE41(y + 24, u + 12, v + 12, &R3, &G3, &B3);

    rgb0 = _mm_packus_epi16(R0, R1);
    rgb1 = _mm_packus_epi16(R2, R3);
    rgb2 = _mm_packus_epi16(G0, G1);
    rgb3 = _mm_packus_epi16(G2, G3);
    rgb4 = _mm_packus_epi16(B0, B1);
    rgb5 = _mm_packus_epi16(B2, B3);

    PlanarTo24b_SSE41(&rgb0, &rgb1, &rgb2, &rgb3, &rgb4, &rgb5, dst);

    y += 32;
    u += 16;
    v += 16;
  }
  for (; n < len; ++n) {
    VP8YuvToRgb(y[0], u[0], v[0], dst);
    dst += 3;
    y += 1;
    u += (n & 1);
    v += (n & 1);
  }
}

static void YuvToBgrRow_SSE41(const uint8_t* WEBP_RESTRICT y,
                              const uint8_t* WEBP_RESTRICT u,
                              const uint8_t* WEBP_RESTRICT v,
                              uint8_t* WEBP_RESTRICT dst, int len) {
  int n;
  for (n = 0; n + 32 <= len; n += 32, dst += 32 * 3) {
    __m128i R0, R1, R2, R3, G0, G1, G2, G3, B0, B1, B2, B3;
    __m128i bgr0, bgr1, bgr2, bgr3, bgr4, bgr5;

    YUV420ToRGB_SSE41(y +  0, u +  0, v +  0, &R0, &G0, &B0);
    YUV420ToRGB_SSE41(y +  8, u +  4, v +  4, &R1, &G1, &B1);
    YUV420ToRGB_SSE41(y + 16, u +  8, v +  8, &R2, &G2, &B2);
    YUV420ToRGB_SSE41(y + 24, u + 12, v + 12, &R3, &G3, &B3);

    bgr0 = _mm_packus_epi16(B0, B1);
    bgr1 = _mm_packus_epi16(B2, B3);
    bgr2 = _mm_packus_epi16(G0, G1);
    bgr3 = _mm_packus_epi16(G2, G3);
    bgr4 = _mm_packus_epi16(R0, R1);
    bgr5 = _mm_packus_epi16(R2, R3);

    PlanarTo24b_SSE41(&bgr0, &bgr1, &bgr2, &bgr3, &bgr4, &bgr5, dst);

    y += 32;
    u += 16;
    v += 16;
  }
  for (; n < len; ++n) {
    VP8YuvToBgr(y[0], u[0], v[0], dst);
    dst += 3;
    y += 1;
    u += (n & 1);
    v += (n & 1);
  }
}

extern void WebPInitSamplersSSE41(void);

WEBP_TSAN_IGNORE_FUNCTION void WebPInitSamplersSSE41(void) {
  WebPSamplers[MODE_RGB]  = YuvToRgbRow_SSE41;
  WebPSamplers[MODE_BGR]  = YuvToBgrRow_SSE41;
}

#define LOAD_16(src) _mm_loadu_si128((const __m128i*)(src))

#define STORE_16(V, dst) _mm_storeu_si128((__m128i*)(dst), (V))

#define WEBP_SSE41_SHUFF(OUT)  do {                  \
  const __m128i tmp0 = _mm_shuffle_epi8(A0, shuff0); \
  const __m128i tmp1 = _mm_shuffle_epi8(A1, shuff1); \
  const __m128i tmp2 = _mm_shuffle_epi8(A2, shuff2); \
  const __m128i tmp3 = _mm_shuffle_epi8(A3, shuff0); \
  const __m128i tmp4 = _mm_shuffle_epi8(A4, shuff1); \
  const __m128i tmp5 = _mm_shuffle_epi8(A5, shuff2); \
                                                     \
               \
  const __m128i tmp6 = _mm_or_si128(tmp0, tmp1);     \
  const __m128i tmp7 = _mm_or_si128(tmp3, tmp4);     \
  out[OUT + 0] = _mm_or_si128(tmp6, tmp2);           \
  out[OUT + 1] = _mm_or_si128(tmp7, tmp5);           \
} while (0);

static WEBP_INLINE void RGB24PackedToPlanar_SSE41(
    const uint8_t* WEBP_RESTRICT const rgb, __m128i* const out ) {
  const __m128i A0 = _mm_loadu_si128((const __m128i*)(rgb +  0));
  const __m128i A1 = _mm_loadu_si128((const __m128i*)(rgb + 16));
  const __m128i A2 = _mm_loadu_si128((const __m128i*)(rgb + 32));
  const __m128i A3 = _mm_loadu_si128((const __m128i*)(rgb + 48));
  const __m128i A4 = _mm_loadu_si128((const __m128i*)(rgb + 64));
  const __m128i A5 = _mm_loadu_si128((const __m128i*)(rgb + 80));

  {
    const __m128i shuff0 = _mm_set_epi8(
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 15, 12, 9, 6, 3, 0);
    const __m128i shuff1 = _mm_set_epi8(
        -1, -1, -1, -1, -1, 14, 11, 8, 5, 2, -1, -1, -1, -1, -1, -1);
    const __m128i shuff2 = _mm_set_epi8(
        13, 10, 7, 4, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1);
    WEBP_SSE41_SHUFF(0)
  }

  {
    const __m128i shuff0 = _mm_set_epi8(
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 13, 10, 7, 4, 1);
    const __m128i shuff1 = _mm_set_epi8(
        -1, -1, -1, -1, -1, 15, 12, 9, 6, 3, 0, -1, -1, -1, -1, -1);
    const __m128i shuff2 = _mm_set_epi8(
        14, 11, 8, 5, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1);
    WEBP_SSE41_SHUFF(2)
  }

  {
    const __m128i shuff0 = _mm_set_epi8(
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 14, 11, 8, 5, 2);
    const __m128i shuff1 = _mm_set_epi8(
        -1, -1, -1, -1, -1, -1, 13, 10, 7, 4, 1, -1, -1, -1, -1, -1);
    const __m128i shuff2 = _mm_set_epi8(
        15, 12, 9, 6, 3, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1);
    WEBP_SSE41_SHUFF(4)
  }
}

#undef WEBP_SSE41_SHUFF

static WEBP_INLINE void RGB32PackedToPlanar_SSE41(
    const uint32_t* WEBP_RESTRICT const argb, __m128i* const rgb ) {
  const __m128i zero = _mm_setzero_si128();
  __m128i a0 = LOAD_16(argb + 0);
  __m128i a1 = LOAD_16(argb + 4);
  __m128i a2 = LOAD_16(argb + 8);
  __m128i a3 = LOAD_16(argb + 12);
  VP8L32bToPlanar_SSE41(&a0, &a1, &a2, &a3);
  rgb[0] = _mm_unpacklo_epi8(a1, zero);
  rgb[1] = _mm_unpackhi_epi8(a1, zero);
  rgb[2] = _mm_unpacklo_epi8(a2, zero);
  rgb[3] = _mm_unpackhi_epi8(a2, zero);
  rgb[4] = _mm_unpacklo_epi8(a3, zero);
  rgb[5] = _mm_unpackhi_epi8(a3, zero);
}

#define TRANSFORM(RG_LO, RG_HI, GB_LO, GB_HI, MULT_RG, MULT_GB, \
                  ROUNDER, DESCALE_FIX, OUT) do {               \
  const __m128i V0_lo = _mm_madd_epi16(RG_LO, MULT_RG);         \
  const __m128i V0_hi = _mm_madd_epi16(RG_HI, MULT_RG);         \
  const __m128i V1_lo = _mm_madd_epi16(GB_LO, MULT_GB);         \
  const __m128i V1_hi = _mm_madd_epi16(GB_HI, MULT_GB);         \
  const __m128i V2_lo = _mm_add_epi32(V0_lo, V1_lo);            \
  const __m128i V2_hi = _mm_add_epi32(V0_hi, V1_hi);            \
  const __m128i V3_lo = _mm_add_epi32(V2_lo, ROUNDER);          \
  const __m128i V3_hi = _mm_add_epi32(V2_hi, ROUNDER);          \
  const __m128i V5_lo = _mm_srai_epi32(V3_lo, DESCALE_FIX);     \
  const __m128i V5_hi = _mm_srai_epi32(V3_hi, DESCALE_FIX);     \
  (OUT) = _mm_packs_epi32(V5_lo, V5_hi);                        \
} while (0)

#define MK_CST_16(A, B) _mm_set_epi16((B), (A), (B), (A), (B), (A), (B), (A))
static WEBP_INLINE void ConvertRGBToY_SSE41(const __m128i* const R,
                                            const __m128i* const G,
                                            const __m128i* const B,
                                            __m128i* const Y) {
  const __m128i kRG_y = MK_CST_16(16839, 33059 - 16384);
  const __m128i kGB_y = MK_CST_16(16384, 6420);
  const __m128i kHALF_Y = _mm_set1_epi32((16 << YUV_FIX) + YUV_HALF);

  const __m128i RG_lo = _mm_unpacklo_epi16(*R, *G);
  const __m128i RG_hi = _mm_unpackhi_epi16(*R, *G);
  const __m128i GB_lo = _mm_unpacklo_epi16(*G, *B);
  const __m128i GB_hi = _mm_unpackhi_epi16(*G, *B);
  TRANSFORM(RG_lo, RG_hi, GB_lo, GB_hi, kRG_y, kGB_y, kHALF_Y, YUV_FIX, *Y);
}

static WEBP_INLINE void ConvertRGBToUV_SSE41(const __m128i* const R,
                                             const __m128i* const G,
                                             const __m128i* const B,
                                             __m128i* const U,
                                             __m128i* const V) {
  const __m128i kRG_u = MK_CST_16(-9719, -19081);
  const __m128i kGB_u = MK_CST_16(0, 28800);
  const __m128i kRG_v = MK_CST_16(28800, 0);
  const __m128i kGB_v = MK_CST_16(-24116, -4684);
  const __m128i kHALF_UV = _mm_set1_epi32(((128 << YUV_FIX) + YUV_HALF) << 2);

  const __m128i RG_lo = _mm_unpacklo_epi16(*R, *G);
  const __m128i RG_hi = _mm_unpackhi_epi16(*R, *G);
  const __m128i GB_lo = _mm_unpacklo_epi16(*G, *B);
  const __m128i GB_hi = _mm_unpackhi_epi16(*G, *B);
  TRANSFORM(RG_lo, RG_hi, GB_lo, GB_hi, kRG_u, kGB_u,
            kHALF_UV, YUV_FIX + 2, *U);
  TRANSFORM(RG_lo, RG_hi, GB_lo, GB_hi, kRG_v, kGB_v,
            kHALF_UV, YUV_FIX + 2, *V);
}

#undef MK_CST_16
#undef TRANSFORM

static void ConvertRGB24ToY_SSE41(const uint8_t* WEBP_RESTRICT rgb,
                                  uint8_t* WEBP_RESTRICT y, int width) {
  const int max_width = width & ~31;
  int i;
  for (i = 0; i < max_width; rgb += 3 * 16 * 2) {
    __m128i rgb_plane[6];
    int j;

    RGB24PackedToPlanar_SSE41(rgb, rgb_plane);

    for (j = 0; j < 2; ++j, i += 16) {
      const __m128i zero = _mm_setzero_si128();
      __m128i r, g, b, Y0, Y1;

      r = _mm_unpacklo_epi8(rgb_plane[0 + j], zero);
      g = _mm_unpacklo_epi8(rgb_plane[2 + j], zero);
      b = _mm_unpacklo_epi8(rgb_plane[4 + j], zero);
      ConvertRGBToY_SSE41(&r, &g, &b, &Y0);

      r = _mm_unpackhi_epi8(rgb_plane[0 + j], zero);
      g = _mm_unpackhi_epi8(rgb_plane[2 + j], zero);
      b = _mm_unpackhi_epi8(rgb_plane[4 + j], zero);
      ConvertRGBToY_SSE41(&r, &g, &b, &Y1);

      STORE_16(_mm_packus_epi16(Y0, Y1), y + i);
    }
  }
  for (; i < width; ++i, rgb += 3) {
    y[i] = VP8RGBToY(rgb[0], rgb[1], rgb[2], YUV_HALF);
  }
}

static void ConvertBGR24ToY_SSE41(const uint8_t* WEBP_RESTRICT bgr,
                                  uint8_t* WEBP_RESTRICT y, int width) {
  const int max_width = width & ~31;
  int i;
  for (i = 0; i < max_width; bgr += 3 * 16 * 2) {
    __m128i bgr_plane[6];
    int j;

    RGB24PackedToPlanar_SSE41(bgr, bgr_plane);

    for (j = 0; j < 2; ++j, i += 16) {
      const __m128i zero = _mm_setzero_si128();
      __m128i r, g, b, Y0, Y1;

      b = _mm_unpacklo_epi8(bgr_plane[0 + j], zero);
      g = _mm_unpacklo_epi8(bgr_plane[2 + j], zero);
      r = _mm_unpacklo_epi8(bgr_plane[4 + j], zero);
      ConvertRGBToY_SSE41(&r, &g, &b, &Y0);

      b = _mm_unpackhi_epi8(bgr_plane[0 + j], zero);
      g = _mm_unpackhi_epi8(bgr_plane[2 + j], zero);
      r = _mm_unpackhi_epi8(bgr_plane[4 + j], zero);
      ConvertRGBToY_SSE41(&r, &g, &b, &Y1);

      STORE_16(_mm_packus_epi16(Y0, Y1), y + i);
    }
  }
  for (; i < width; ++i, bgr += 3) {
    y[i] = VP8RGBToY(bgr[2], bgr[1], bgr[0], YUV_HALF);
  }
}

static void ConvertARGBToY_SSE41(const uint32_t* WEBP_RESTRICT argb,
                                 uint8_t* WEBP_RESTRICT y, int width) {
  const int max_width = width & ~15;
  int i;
  for (i = 0; i < max_width; i += 16) {
    __m128i Y0, Y1, rgb[6];
    RGB32PackedToPlanar_SSE41(&argb[i], rgb);
    ConvertRGBToY_SSE41(&rgb[0], &rgb[2], &rgb[4], &Y0);
    ConvertRGBToY_SSE41(&rgb[1], &rgb[3], &rgb[5], &Y1);
    STORE_16(_mm_packus_epi16(Y0, Y1), y + i);
  }
  for (; i < width; ++i) {
    const uint32_t p = argb[i];
    y[i] = VP8RGBToY((p >> 16) & 0xff, (p >> 8) & 0xff, (p >>  0) & 0xff,
                     YUV_HALF);
  }
}

static void HorizontalAddPack_SSE41(const __m128i* const A,
                                    const __m128i* const B,
                                    __m128i* const out) {
  const __m128i k2 = _mm_set1_epi16(2);
  const __m128i C = _mm_madd_epi16(*A, k2);
  const __m128i D = _mm_madd_epi16(*B, k2);
  *out = _mm_packs_epi32(C, D);
}

static void ConvertARGBToUV_SSE41(const uint32_t* WEBP_RESTRICT argb,
                                  uint8_t* WEBP_RESTRICT u,
                                  uint8_t* WEBP_RESTRICT v,
                                  int src_width, int do_store) {
  const int max_width = src_width & ~31;
  int i;
  for (i = 0; i < max_width; i += 32, u += 16, v += 16) {
    __m128i rgb[6], U0, V0, U1, V1;
    RGB32PackedToPlanar_SSE41(&argb[i], rgb);
    HorizontalAddPack_SSE41(&rgb[0], &rgb[1], &rgb[0]);
    HorizontalAddPack_SSE41(&rgb[2], &rgb[3], &rgb[2]);
    HorizontalAddPack_SSE41(&rgb[4], &rgb[5], &rgb[4]);
    ConvertRGBToUV_SSE41(&rgb[0], &rgb[2], &rgb[4], &U0, &V0);

    RGB32PackedToPlanar_SSE41(&argb[i + 16], rgb);
    HorizontalAddPack_SSE41(&rgb[0], &rgb[1], &rgb[0]);
    HorizontalAddPack_SSE41(&rgb[2], &rgb[3], &rgb[2]);
    HorizontalAddPack_SSE41(&rgb[4], &rgb[5], &rgb[4]);
    ConvertRGBToUV_SSE41(&rgb[0], &rgb[2], &rgb[4], &U1, &V1);

    U0 = _mm_packus_epi16(U0, U1);
    V0 = _mm_packus_epi16(V0, V1);
    if (!do_store) {
      const __m128i prev_u = LOAD_16(u);
      const __m128i prev_v = LOAD_16(v);
      U0 = _mm_avg_epu8(U0, prev_u);
      V0 = _mm_avg_epu8(V0, prev_v);
    }
    STORE_16(U0, u);
    STORE_16(V0, v);
  }
  if (i < src_width) {
    WebPConvertARGBToUV_C(argb + i, u, v, src_width - i, do_store);
  }
}

static WEBP_INLINE void RGBA32PackedToPlanar_16b_SSE41(
    const uint16_t* WEBP_RESTRICT const rgbx,
    __m128i* const r, __m128i* const g, __m128i* const b) {
  const __m128i in0 = LOAD_16(rgbx +  0);
  const __m128i in1 = LOAD_16(rgbx +  8);
  const __m128i in2 = LOAD_16(rgbx + 16);
  const __m128i in3 = LOAD_16(rgbx + 24);

  const __m128i shuff0 =
      _mm_set_epi8(-1, -1, -1, -1, 13, 12, 5, 4, 11, 10, 3, 2, 9, 8, 1, 0);
  const __m128i shuff1 =
      _mm_set_epi8(13, 12, 5, 4, -1, -1, -1, -1, 11, 10, 3, 2, 9, 8, 1, 0);
  const __m128i A0 = _mm_shuffle_epi8(in0, shuff0);
  const __m128i A1 = _mm_shuffle_epi8(in1, shuff1);
  const __m128i A2 = _mm_shuffle_epi8(in2, shuff0);
  const __m128i A3 = _mm_shuffle_epi8(in3, shuff1);

  const __m128i B0 = _mm_unpacklo_epi32(A0, A1);
  const __m128i B1 = _mm_or_si128(A0, A1);
  const __m128i B2 = _mm_unpacklo_epi32(A2, A3);
  const __m128i B3 = _mm_or_si128(A2, A3);

  *r = _mm_unpacklo_epi64(B0, B2);
  *g = _mm_unpackhi_epi64(B0, B2);
  *b = _mm_unpackhi_epi64(B1, B3);
}

static void ConvertRGBA32ToUV_SSE41(const uint16_t* WEBP_RESTRICT rgb,
                                    uint8_t* WEBP_RESTRICT u,
                                    uint8_t* WEBP_RESTRICT v, int width) {
  const int max_width = width & ~15;
  const uint16_t* const last_rgb = rgb + 4 * max_width;
  while (rgb < last_rgb) {
    __m128i r, g, b, U0, V0, U1, V1;
    RGBA32PackedToPlanar_16b_SSE41(rgb +  0, &r, &g, &b);
    ConvertRGBToUV_SSE41(&r, &g, &b, &U0, &V0);
    RGBA32PackedToPlanar_16b_SSE41(rgb + 32, &r, &g, &b);
    ConvertRGBToUV_SSE41(&r, &g, &b, &U1, &V1);
    STORE_16(_mm_packus_epi16(U0, U1), u);
    STORE_16(_mm_packus_epi16(V0, V1), v);
    u += 16;
    v += 16;
    rgb += 2 * 32;
  }
  if (max_width < width) {
    WebPConvertRGBA32ToUV_C(rgb, u, v, width - max_width);
  }
}

extern void WebPInitConvertARGBToYUVSSE41(void);

WEBP_TSAN_IGNORE_FUNCTION void WebPInitConvertARGBToYUVSSE41(void) {
  WebPConvertARGBToY = ConvertARGBToY_SSE41;
  WebPConvertARGBToUV = ConvertARGBToUV_SSE41;

  WebPConvertRGB24ToY = ConvertRGB24ToY_SSE41;
  WebPConvertBGR24ToY = ConvertBGR24ToY_SSE41;

  WebPConvertRGBA32ToUV = ConvertRGBA32ToUV_SSE41;
}

#else

WEBP_DSP_INIT_STUB(WebPInitSamplersSSE41)
WEBP_DSP_INIT_STUB(WebPInitConvertARGBToYUVSSE41)

#endif

#ifdef HAVE_CONFIG_H
#include "src/webp/config.h"
#endif

#include <assert.h>
#include <stddef.h>

void VP8BitReaderSetBuffer(VP8BitReader* const br,
                           const uint8_t* const start,
                           size_t size) {
  assert(start != NULL);
  br->buf = start;
  br->buf_end = start + size;
  br->buf_max =
      (size >= sizeof(lbit_t)) ? start + size - sizeof(lbit_t) + 1 : start;
}

void VP8InitBitReader(VP8BitReader* const br,
                      const uint8_t* const start, size_t size) {
  assert(br != NULL);
  assert(start != NULL);
  assert(size < (1u << 31));
  br->range   = 255 - 1;
  br->value   = 0;
  br->bits    = -8;
  br->eof     = 0;
  VP8BitReaderSetBuffer(br, start, size);
  VP8LoadNewBytes(br);
}

void VP8RemapBitReader(VP8BitReader* const br, ptrdiff_t offset) {
  if (br->buf != NULL) {
    br->buf += offset;
    br->buf_end += offset;
    br->buf_max += offset;
  }
}

const uint8_t kVP8Log2Range[128] = {
     7, 6, 6, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4,
  3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
  2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
  2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  0
};

const uint8_t kVP8NewRange[128] = {
  127, 127, 191, 127, 159, 191, 223, 127,
  143, 159, 175, 191, 207, 223, 239, 127,
  135, 143, 151, 159, 167, 175, 183, 191,
  199, 207, 215, 223, 231, 239, 247, 127,
  131, 135, 139, 143, 147, 151, 155, 159,
  163, 167, 171, 175, 179, 183, 187, 191,
  195, 199, 203, 207, 211, 215, 219, 223,
  227, 231, 235, 239, 243, 247, 251, 127,
  129, 131, 133, 135, 137, 139, 141, 143,
  145, 147, 149, 151, 153, 155, 157, 159,
  161, 163, 165, 167, 169, 171, 173, 175,
  177, 179, 181, 183, 185, 187, 189, 191,
  193, 195, 197, 199, 201, 203, 205, 207,
  209, 211, 213, 215, 217, 219, 221, 223,
  225, 227, 229, 231, 233, 235, 237, 239,
  241, 243, 245, 247, 249, 251, 253, 127
};

void VP8LoadFinalBytes(VP8BitReader* const br) {
  assert(br != NULL && br->buf != NULL);

  if (br->buf < br->buf_end) {
    br->bits += 8;
    br->value = (bit_t)(*br->buf++) | (br->value << 8);
  } else if (!br->eof) {
    br->value <<= 8;
    br->bits += 8;
    br->eof = 1;
  } else {
    br->bits = 0;
  }
}

uint32_t VP8GetValue(VP8BitReader* const br, int bits, const char label[]) {
  uint32_t v = 0;
  while (bits-- > 0) {
    v |= VP8GetBit(br, 0x80, label) << bits;
  }
  return v;
}

int32_t VP8GetSignedValue(VP8BitReader* const br, int bits,
                          const char label[]) {
  const int value = VP8GetValue(br, bits, label);
  return VP8Get(br, label) ? -value : value;
}

#define VP8L_LOG8_WBITS 4

#if defined(__arm__) || defined(_M_ARM) || WEBP_AARCH64 || \
    defined(__i386__) || defined(_M_IX86) || \
    defined(__x86_64__) || defined(_M_X64) || \
    defined(__wasm__)
#define VP8L_USE_FAST_LOAD
#endif

static const uint32_t kBitMask[VP8L_MAX_NUM_BIT_READ + 1] = {
  0,
  0x000001, 0x000003, 0x000007, 0x00000f,
  0x00001f, 0x00003f, 0x00007f, 0x0000ff,
  0x0001ff, 0x0003ff, 0x0007ff, 0x000fff,
  0x001fff, 0x003fff, 0x007fff, 0x00ffff,
  0x01ffff, 0x03ffff, 0x07ffff, 0x0fffff,
  0x1fffff, 0x3fffff, 0x7fffff, 0xffffff
};

void VP8LInitBitReader(VP8LBitReader* const br, const uint8_t* const start,
                       size_t length) {
  size_t i;
  vp8l_val_t value = 0;
  assert(br != NULL);
  assert(start != NULL);
  assert(length < 0xfffffff8u);

  br->len = length;
  br->val = 0;
  br->bit_pos = 0;
  br->eos = 0;

  if (length > sizeof(br->val)) {
    length = sizeof(br->val);
  }
  for (i = 0; i < length; ++i) {
    value |= (vp8l_val_t)start[i] << (8 * i);
  }
  br->val = value;
  br->pos = length;
  br->buf = start;
}

void VP8LBitReaderSetBuffer(VP8LBitReader* const br,
                            const uint8_t* const buf, size_t len) {
  assert(br != NULL);
  assert(buf != NULL);
  assert(len < 0xfffffff8u);
  br->buf = buf;
  br->len = len;

  br->eos = (br->pos > br->len) || VP8LIsEndOfStream(br);
}

static void VP8LSetEndOfStream(VP8LBitReader* const br) {
  br->eos = 1;
  br->bit_pos = 0;
}

static void ShiftBytes(VP8LBitReader* const br) {
  while (br->bit_pos >= 8 && br->pos < br->len) {
    br->val >>= 8;
    br->val |= ((vp8l_val_t)br->buf[br->pos]) << (VP8L_LBITS - 8);
    ++br->pos;
    br->bit_pos -= 8;
  }
  if (VP8LIsEndOfStream(br)) {
    VP8LSetEndOfStream(br);
  }
}

void VP8LDoFillBitWindow(VP8LBitReader* const br) {
  assert(br->bit_pos >= VP8L_WBITS);
#if defined(VP8L_USE_FAST_LOAD)
  if (br->pos + sizeof(br->val) < br->len) {
    br->val >>= VP8L_WBITS;
    br->bit_pos -= VP8L_WBITS;
    br->val |= (vp8l_val_t)HToLE32(WebPMemToUint32(br->buf + br->pos)) <<
               (VP8L_LBITS - VP8L_WBITS);
    br->pos += VP8L_LOG8_WBITS;
    return;
  }
#endif
  ShiftBytes(br);
}

uint32_t VP8LReadBits(VP8LBitReader* const br, int n_bits) {
  assert(n_bits >= 0);

  if (!br->eos && n_bits <= VP8L_MAX_NUM_BIT_READ) {
    const uint32_t val = VP8LPrefetchBits(br) & kBitMask[n_bits];
    const int new_bits = br->bit_pos + n_bits;
    br->bit_pos = new_bits;
    ShiftBytes(br);
    return val;
  } else {
    VP8LSetEndOfStream(br);
    return 0;
  }
}

#if (BITTRACE > 0)

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define MAX_NUM_LABELS 32
static struct {
  const char* label;
  int size;
  int count;
} kLabels[MAX_NUM_LABELS];

static int last_label = 0;
static int last_pos = 0;
static const uint8_t* buf_start = NULL;
static int init_done = 0;

static void PrintBitTraces(void) {
  int i;
  int scale = 1;
  int total = 0;
  const char* units = "bits";
#if (BITTRACE == 2)
  scale = 8;
  units = "bytes";
#endif
  for (i = 0; i < last_label; ++i) total += kLabels[i].size;
  if (total < 1) total = 1;
  printf("=== Bit traces ===\n");
  for (i = 0; i < last_label; ++i) {
    const int skip = 16 - (int)strlen(kLabels[i].label);
    const int value = (kLabels[i].size + scale - 1) / scale;
    assert(skip > 0);
    printf("%s \%*s: %6d %s   \t[%5.2f%%] [count: %7d]\n",
           kLabels[i].label, skip, "", value, units,
           100.f * kLabels[i].size / total,
           kLabels[i].count);
  }
  total = (total + scale - 1) / scale;
  printf("Total: %d %s\n", total, units);
}

void BitTrace(const struct VP8BitReader* const br, const char label[]) {
  int i, pos;
  if (!init_done) {
    memset(kLabels, 0, sizeof(kLabels));
    atexit(PrintBitTraces);
    buf_start = br->buf;
    init_done = 1;
  }
  pos = (int)(br->buf - buf_start) * 8 - br->bits;

  if (abs(pos - last_pos) > 32) {
    buf_start = br->buf;
    pos = 0;
    last_pos = 0;
  }
  if (br->range >= 0x7f) pos += kVP8Log2Range[br->range - 0x7f];
  for (i = 0; i < last_label; ++i) {
    if (!strcmp(label, kLabels[i].label)) break;
  }
  if (i == MAX_NUM_LABELS) abort();
  kLabels[i].label = label;
  kLabels[i].size += pos - last_pos;
  kLabels[i].count += 1;
  if (i == last_label) ++last_label;
  last_pos = pos;
}

#endif

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#ifndef WEBP_UTILS_BIT_WRITER_UTILS_H_
#define WEBP_UTILS_BIT_WRITER_UTILS_H_

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct VP8BitWriter VP8BitWriter;
struct VP8BitWriter {
  int32_t  range;
  int32_t  value;
  int      run;
  int      nb_bits;
  uint8_t* buf;
  size_t   pos;
  size_t   max_pos;
  int      error;
};

int VP8BitWriterInit(VP8BitWriter* const bw, size_t expected_size);

uint8_t* VP8BitWriterFinish(VP8BitWriter* const bw);

void VP8BitWriterWipeOut(VP8BitWriter* const bw);

int VP8PutBit(VP8BitWriter* const bw, int bit, int prob);
int VP8PutBitUniform(VP8BitWriter* const bw, int bit);
void VP8PutBits(VP8BitWriter* const bw, uint32_t value, int nb_bits);
void VP8PutSignedBits(VP8BitWriter* const bw, int value, int nb_bits);

int VP8BitWriterAppend(VP8BitWriter* const bw,
                       const uint8_t* data, size_t size);

static WEBP_INLINE uint64_t VP8BitWriterPos(const VP8BitWriter* const bw) {
  const uint64_t nb_bits = 8 + bw->nb_bits;
  return (bw->pos + bw->run) * 8 + nb_bits;
}

static WEBP_INLINE uint8_t* VP8BitWriterBuf(const VP8BitWriter* const bw) {
  return bw->buf;
}

static WEBP_INLINE size_t VP8BitWriterSize(const VP8BitWriter* const bw) {
  return bw->pos;
}

#if defined(__x86_64__) || defined(_M_X64)
typedef uint64_t vp8l_atype_t;
typedef uint32_t vp8l_wtype_t;
#define WSWAP HToLE32
#define VP8L_WRITER_BYTES    4
#define VP8L_WRITER_BITS     32
#define VP8L_WRITER_MAX_BITS 64
#else
typedef uint32_t vp8l_atype_t;
typedef uint16_t vp8l_wtype_t;
#define WSWAP HToLE16
#define VP8L_WRITER_BYTES    2
#define VP8L_WRITER_BITS     16
#define VP8L_WRITER_MAX_BITS 32
#endif

typedef struct {
  vp8l_atype_t bits;
  int          used;
  uint8_t*     buf;
  uint8_t*     cur;
  uint8_t*     end;

  int error;
} VP8LBitWriter;

static WEBP_INLINE size_t VP8LBitWriterNumBytes(const VP8LBitWriter* const bw) {
  return (bw->cur - bw->buf) + ((bw->used + 7) >> 3);
}

int VP8LBitWriterInit(VP8LBitWriter* const bw, size_t expected_size);

int VP8LBitWriterClone(const VP8LBitWriter* const src,
                       VP8LBitWriter* const dst);

uint8_t* VP8LBitWriterFinish(VP8LBitWriter* const bw);

void VP8LBitWriterWipeOut(VP8LBitWriter* const bw);

void VP8LBitWriterReset(const VP8LBitWriter* const bw_init,
                        VP8LBitWriter* const bw);

void VP8LBitWriterSwap(VP8LBitWriter* const src, VP8LBitWriter* const dst);

void VP8LPutBitsFlushBits(VP8LBitWriter* const bw);

void VP8LPutBitsInternal(VP8LBitWriter* const bw, uint32_t bits, int n_bits);

static WEBP_INLINE void VP8LPutBits(VP8LBitWriter* const bw,
                                    uint32_t bits, int n_bits) {
  if (sizeof(vp8l_wtype_t) == 4) {
    if (n_bits > 0) {
      if (bw->used >= 32) {
        VP8LPutBitsFlushBits(bw);
      }
      bw->bits |= (vp8l_atype_t)bits << bw->used;
      bw->used += n_bits;
    }
  } else {
    VP8LPutBitsInternal(bw, bits, n_bits);
  }
}

#ifdef __cplusplus
}
#endif

#endif

static int BitWriterResize(VP8BitWriter* const bw, size_t extra_size) {
  uint8_t* new_buf;
  size_t new_size;
  const uint64_t needed_size_64b = (uint64_t)bw->pos + extra_size;
  const size_t needed_size = (size_t)needed_size_64b;
  if (needed_size_64b != needed_size) {
    bw->error = 1;
    return 0;
  }
  if (needed_size <= bw->max_pos) return 1;

  new_size = 2 * bw->max_pos;
  if (new_size < needed_size) new_size = needed_size;
  if (new_size < 1024) new_size = 1024;
  new_buf = (uint8_t*)WebPSafeMalloc(1ULL, new_size);
  if (new_buf == NULL) {
    bw->error = 1;
    return 0;
  }
  if (bw->pos > 0) {
    assert(bw->buf != NULL);
    memcpy(new_buf, bw->buf, bw->pos);
  }
  WebPSafeFree(bw->buf);
  bw->buf = new_buf;
  bw->max_pos = new_size;
  return 1;
}

static void Flush(VP8BitWriter* const bw) {
  const int s = 8 + bw->nb_bits;
  const int32_t bits = bw->value >> s;
  assert(bw->nb_bits >= 0);
  bw->value -= bits << s;
  bw->nb_bits -= 8;
  if ((bits & 0xff) != 0xff) {
    size_t pos = bw->pos;
    if (!BitWriterResize(bw, bw->run + 1)) {
      return;
    }
    if (bits & 0x100) {
      if (pos > 0) bw->buf[pos - 1]++;
    }
    if (bw->run > 0) {
      const int value = (bits & 0x100) ? 0x00 : 0xff;
      for (; bw->run > 0; --bw->run) bw->buf[pos++] = value;
    }
    bw->buf[pos++] = bits & 0xff;
    bw->pos = pos;
  } else {
    bw->run++;
  }
}

static const uint8_t kNorm[128] = {
     7, 6, 6, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4,
  3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
  2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
  2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  0
};

static const uint8_t kNewRange[128] = {
  127, 127, 191, 127, 159, 191, 223, 127, 143, 159, 175, 191, 207, 223, 239,
  127, 135, 143, 151, 159, 167, 175, 183, 191, 199, 207, 215, 223, 231, 239,
  247, 127, 131, 135, 139, 143, 147, 151, 155, 159, 163, 167, 171, 175, 179,
  183, 187, 191, 195, 199, 203, 207, 211, 215, 219, 223, 227, 231, 235, 239,
  243, 247, 251, 127, 129, 131, 133, 135, 137, 139, 141, 143, 145, 147, 149,
  151, 153, 155, 157, 159, 161, 163, 165, 167, 169, 171, 173, 175, 177, 179,
  181, 183, 185, 187, 189, 191, 193, 195, 197, 199, 201, 203, 205, 207, 209,
  211, 213, 215, 217, 219, 221, 223, 225, 227, 229, 231, 233, 235, 237, 239,
  241, 243, 245, 247, 249, 251, 253, 127
};

int VP8PutBit(VP8BitWriter* const bw, int bit, int prob) {
  const int split = (bw->range * prob) >> 8;
  if (bit) {
    bw->value += split + 1;
    bw->range -= split + 1;
  } else {
    bw->range = split;
  }
  if (bw->range < 127) {
    const int shift = kNorm[bw->range];
    bw->range = kNewRange[bw->range];
    bw->value <<= shift;
    bw->nb_bits += shift;
    if (bw->nb_bits > 0) Flush(bw);
  }
  return bit;
}

int VP8PutBitUniform(VP8BitWriter* const bw, int bit) {
  const int split = bw->range >> 1;
  if (bit) {
    bw->value += split + 1;
    bw->range -= split + 1;
  } else {
    bw->range = split;
  }
  if (bw->range < 127) {
    bw->range = kNewRange[bw->range];
    bw->value <<= 1;
    bw->nb_bits += 1;
    if (bw->nb_bits > 0) Flush(bw);
  }
  return bit;
}

void VP8PutBits(VP8BitWriter* const bw, uint32_t value, int nb_bits) {
  uint32_t mask;
  assert(nb_bits > 0 && nb_bits < 32);
  for (mask = 1u << (nb_bits - 1); mask; mask >>= 1) {
    VP8PutBitUniform(bw, value & mask);
  }
}

void VP8PutSignedBits(VP8BitWriter* const bw, int value, int nb_bits) {
  if (!VP8PutBitUniform(bw, value != 0)) return;
  if (value < 0) {
    VP8PutBits(bw, ((-value) << 1) | 1, nb_bits + 1);
  } else {
    VP8PutBits(bw, value << 1, nb_bits + 1);
  }
}

int VP8BitWriterInit(VP8BitWriter* const bw, size_t expected_size) {
  bw->range   = 255 - 1;
  bw->value   = 0;
  bw->run     = 0;
  bw->nb_bits = -8;
  bw->pos     = 0;
  bw->max_pos = 0;
  bw->error   = 0;
  bw->buf     = NULL;
  return (expected_size > 0) ? BitWriterResize(bw, expected_size) : 1;
}

uint8_t* VP8BitWriterFinish(VP8BitWriter* const bw) {
  VP8PutBits(bw, 0, 9 - bw->nb_bits);
  bw->nb_bits = 0;
  Flush(bw);
  return bw->buf;
}

int VP8BitWriterAppend(VP8BitWriter* const bw,
                       const uint8_t* data, size_t size) {
  assert(data != NULL);
  if (bw->nb_bits != -8) return 0;
  if (!BitWriterResize(bw, size)) return 0;
  memcpy(bw->buf + bw->pos, data, size);
  bw->pos += size;
  return 1;
}

void VP8BitWriterWipeOut(VP8BitWriter* const bw) {
  if (bw != NULL) {
    WebPSafeFree(bw->buf);
    memset(bw, 0, sizeof(*bw));
  }
}

#define MIN_EXTRA_SIZE  (32768ULL)

static int VP8LBitWriterResize(VP8LBitWriter* const bw, size_t extra_size) {
  uint8_t* allocated_buf;
  size_t allocated_size;
  const size_t max_bytes = bw->end - bw->buf;
  const size_t current_size = bw->cur - bw->buf;
  const uint64_t size_required_64b = (uint64_t)current_size + extra_size;
  const size_t size_required = (size_t)size_required_64b;
  if (size_required != size_required_64b) {
    bw->error = 1;
    return 0;
  }
  if (max_bytes > 0 && size_required <= max_bytes) return 1;
  allocated_size = (3 * max_bytes) >> 1;
  if (allocated_size < size_required) allocated_size = size_required;

  allocated_size = (((allocated_size >> 10) + 1) << 10);
  allocated_buf = (uint8_t*)WebPSafeMalloc(1ULL, allocated_size);
  if (allocated_buf == NULL) {
    bw->error = 1;
    return 0;
  }
  if (current_size > 0) {
    memcpy(allocated_buf, bw->buf, current_size);
  }
  WebPSafeFree(bw->buf);
  bw->buf = allocated_buf;
  bw->cur = bw->buf + current_size;
  bw->end = bw->buf + allocated_size;
  return 1;
}

int VP8LBitWriterInit(VP8LBitWriter* const bw, size_t expected_size) {
  memset(bw, 0, sizeof(*bw));
  return VP8LBitWriterResize(bw, expected_size);
}

int VP8LBitWriterClone(const VP8LBitWriter* const src,
                       VP8LBitWriter* const dst) {
  const size_t current_size = src->cur - src->buf;
  assert(src->cur >= src->buf && src->cur <= src->end);
  if (!VP8LBitWriterResize(dst, current_size)) return 0;
  memcpy(dst->buf, src->buf, current_size);
  dst->bits = src->bits;
  dst->used = src->used;
  dst->error = src->error;
  dst->cur = dst->buf + current_size;
  return 1;
}

void VP8LBitWriterWipeOut(VP8LBitWriter* const bw) {
  if (bw != NULL) {
    WebPSafeFree(bw->buf);
    memset(bw, 0, sizeof(*bw));
  }
}

void VP8LBitWriterReset(const VP8LBitWriter* const bw_init,
                        VP8LBitWriter* const bw) {
  bw->bits = bw_init->bits;
  bw->used = bw_init->used;
  bw->cur = bw->buf + (bw_init->cur - bw_init->buf);
  assert(bw->cur <= bw->end);
  bw->error = bw_init->error;
}

void VP8LBitWriterSwap(VP8LBitWriter* const src, VP8LBitWriter* const dst) {
  const VP8LBitWriter tmp = *src;
  *src = *dst;
  *dst = tmp;
}

void VP8LPutBitsFlushBits(VP8LBitWriter* const bw) {

  if (bw->cur + VP8L_WRITER_BYTES > bw->end) {
    const uint64_t extra_size = (bw->end - bw->buf) + MIN_EXTRA_SIZE;
    if (!CheckSizeOverflow(extra_size) ||
        !VP8LBitWriterResize(bw, (size_t)extra_size)) {
      bw->cur = bw->buf;
      bw->error = 1;
      return;
    }
  }
  *(vp8l_wtype_t*)bw->cur = (vp8l_wtype_t)WSWAP((vp8l_wtype_t)bw->bits);
  bw->cur += VP8L_WRITER_BYTES;
  bw->bits >>= VP8L_WRITER_BITS;
  bw->used -= VP8L_WRITER_BITS;
}

void VP8LPutBitsInternal(VP8LBitWriter* const bw, uint32_t bits, int n_bits) {
  assert(n_bits <= 32);

  assert(sizeof(vp8l_wtype_t) == 2);
  if (n_bits > 0) {
    vp8l_atype_t lbits = bw->bits;
    int used = bw->used;

#if VP8L_WRITER_BITS == 16
    if (used + n_bits >= VP8L_WRITER_MAX_BITS) {

      const int shift = VP8L_WRITER_MAX_BITS - used;
      lbits |= (vp8l_atype_t)bits << used;
      used = VP8L_WRITER_MAX_BITS;
      n_bits -= shift;
      bits >>= shift;
      assert(n_bits <= VP8L_WRITER_MAX_BITS);
    }
#endif

    while (used >= VP8L_WRITER_BITS) {
      if (bw->cur + VP8L_WRITER_BYTES > bw->end) {
        const uint64_t extra_size = (bw->end - bw->buf) + MIN_EXTRA_SIZE;
        if (!CheckSizeOverflow(extra_size) ||
            !VP8LBitWriterResize(bw, (size_t)extra_size)) {
          bw->cur = bw->buf;
          bw->error = 1;
          return;
        }
      }
      *(vp8l_wtype_t*)bw->cur = (vp8l_wtype_t)WSWAP((vp8l_wtype_t)lbits);
      bw->cur += VP8L_WRITER_BYTES;
      lbits >>= VP8L_WRITER_BITS;
      used -= VP8L_WRITER_BITS;
    }
    bw->bits = lbits | ((vp8l_atype_t)bits << used);
    bw->used = used + n_bits;
  }
}

uint8_t* VP8LBitWriterFinish(VP8LBitWriter* const bw) {

  if (VP8LBitWriterResize(bw, (bw->used + 7) >> 3)) {
    while (bw->used > 0) {
      *bw->cur++ = (uint8_t)bw->bits;
      bw->bits >>= 8;
      bw->used -= 8;
    }
    bw->used = 0;
  }
  return bw->buf;
}

#include <assert.h>
#include <stdlib.h>
#include <string.h>

int VP8LColorCacheInit(VP8LColorCache* const color_cache, int hash_bits) {
  const int hash_size = 1 << hash_bits;
  assert(color_cache != NULL);
  assert(hash_bits > 0);
  color_cache->colors = (uint32_t*)WebPSafeCalloc(
      (uint64_t)hash_size, sizeof(*color_cache->colors));
  if (color_cache->colors == NULL) return 0;
  color_cache->hash_shift = 32 - hash_bits;
  color_cache->hash_bits = hash_bits;
  return 1;
}

void VP8LColorCacheClear(VP8LColorCache* const color_cache) {
  if (color_cache != NULL) {
    WebPSafeFree(color_cache->colors);
    color_cache->colors = NULL;
  }
}

void VP8LColorCacheCopy(const VP8LColorCache* const src,
                        VP8LColorCache* const dst) {
  assert(src != NULL);
  assert(dst != NULL);
  assert(src->hash_bits == dst->hash_bits);
  memcpy(dst->colors, src->colors,
         ((size_t)1u << dst->hash_bits) * sizeof(*dst->colors));
}

#include <stdlib.h>
#include <string.h>

#define SMAX 16
#define SDIFF(a, b) (abs((a) - (b)) >> 4)

static WEBP_INLINE int GradientPredictor(uint8_t a, uint8_t b, uint8_t c) {
  const int g = a + b - c;
  return ((g & ~0xff) == 0) ? g : (g < 0) ? 0 : 255;
}

WEBP_FILTER_TYPE WebPEstimateBestFilter(const uint8_t* data,
                                        int width, int height, int stride) {
  int i, j;
  int bins[WEBP_FILTER_LAST][SMAX];
  memset(bins, 0, sizeof(bins));

  for (j = 2; j < height - 1; j += 2) {
    const uint8_t* const p = data + j * stride;
    int mean = p[0];
    for (i = 2; i < width - 1; i += 2) {
      const int diff0 = SDIFF(p[i], mean);
      const int diff1 = SDIFF(p[i], p[i - 1]);
      const int diff2 = SDIFF(p[i], p[i - width]);
      const int grad_pred =
          GradientPredictor(p[i - 1], p[i - width], p[i - width - 1]);
      const int diff3 = SDIFF(p[i], grad_pred);
      bins[WEBP_FILTER_NONE][diff0] = 1;
      bins[WEBP_FILTER_HORIZONTAL][diff1] = 1;
      bins[WEBP_FILTER_VERTICAL][diff2] = 1;
      bins[WEBP_FILTER_GRADIENT][diff3] = 1;
      mean = (3 * mean + p[i] + 2) >> 2;
    }
  }
  {
    int filter;
    WEBP_FILTER_TYPE best_filter = WEBP_FILTER_NONE;
    int best_score = 0x7fffffff;
    for (filter = WEBP_FILTER_NONE; filter < WEBP_FILTER_LAST; ++filter) {
      int score = 0;
      for (i = 0; i < SMAX; ++i) {
        if (bins[filter][i] > 0) {
          score += i;
        }
      }
      if (score < best_score) {
        best_score = score;
        best_filter = (WEBP_FILTER_TYPE)filter;
      }
    }
    return best_filter;
  }
}

#undef SMAX
#undef SDIFF

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#ifndef WEBP_UTILS_HUFFMAN_ENCODE_UTILS_H_
#define WEBP_UTILS_HUFFMAN_ENCODE_UTILS_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  uint8_t code;
  uint8_t extra_bits;
} HuffmanTreeToken;

typedef struct {
  int       num_symbols;
  uint8_t*  code_lengths;
  uint16_t* codes;
} HuffmanTreeCode;

typedef struct {
  uint32_t total_count;
  int value;
  int pool_index_left;
  int pool_index_right;
} HuffmanTree;

int VP8LCreateCompressedHuffmanTree(const HuffmanTreeCode* const tree,
                                    HuffmanTreeToken* tokens, int max_tokens);

void VP8LCreateHuffmanTree(uint32_t* const histogram, int tree_depth_limit,
                           uint8_t* const buf_rle, HuffmanTree* const huff_tree,
                           HuffmanTreeCode* const huff_code);

#ifdef __cplusplus
}
#endif

#endif

static int ValuesShouldBeCollapsedToStrideAverage(int a, int b) {
  return abs(a - b) < 4;
}

static void OptimizeHuffmanForRle(int length, uint8_t* const good_for_rle,
                                  uint32_t* const counts) {

  int i;
  for (; length >= 0; --length) {
    if (length == 0) {
      return;
    }
    if (counts[length - 1] != 0) {

      break;
    }
  }

  {

    uint32_t symbol = counts[0];
    int stride = 0;
    for (i = 0; i < length + 1; ++i) {
      if (i == length || counts[i] != symbol) {
        if ((symbol == 0 && stride >= 5) ||
            (symbol != 0 && stride >= 7)) {
          int k;
          for (k = 0; k < stride; ++k) {
            good_for_rle[i - k - 1] = 1;
          }
        }
        stride = 1;
        if (i != length) {
          symbol = counts[i];
        }
      } else {
        ++stride;
      }
    }
  }

  {
    uint32_t stride = 0;
    uint32_t limit = counts[0];
    uint32_t sum = 0;
    for (i = 0; i < length + 1; ++i) {
      if (i == length || good_for_rle[i] ||
          (i != 0 && good_for_rle[i - 1]) ||
          !ValuesShouldBeCollapsedToStrideAverage(counts[i], limit)) {
        if (stride >= 4 || (stride >= 3 && sum == 0)) {
          uint32_t k;

          uint32_t count = (sum + stride / 2) / stride;
          if (count < 1) {
            count = 1;
          }
          if (sum == 0) {

            count = 0;
          }
          for (k = 0; k < stride; ++k) {

            counts[i - k - 1] = count;
          }
        }
        stride = 0;
        sum = 0;
        if (i < length - 3) {

          limit = (counts[i] + counts[i + 1] +
                   counts[i + 2] + counts[i + 3] + 2) / 4;
        } else if (i < length) {
          limit = counts[i];
        } else {
          limit = 0;
        }
      }
      ++stride;
      if (i != length) {
        sum += counts[i];
        if (stride >= 4) {
          limit = (sum + stride / 2) / stride;
        }
      }
    }
  }
}

static int CompareHuffmanTrees(const void* ptr1, const void* ptr2) {
  const HuffmanTree* const t1 = (const HuffmanTree*)ptr1;
  const HuffmanTree* const t2 = (const HuffmanTree*)ptr2;
  if (t1->total_count > t2->total_count) {
    return -1;
  } else if (t1->total_count < t2->total_count) {
    return 1;
  } else {
    assert(t1->value != t2->value);
    return (t1->value < t2->value) ? -1 : 1;
  }
}

static void SetBitDepths(const HuffmanTree* const tree,
                         const HuffmanTree* const pool,
                         uint8_t* const bit_depths, int level) {
  if (tree->pool_index_left >= 0) {
    SetBitDepths(&pool[tree->pool_index_left], pool, bit_depths, level + 1);
    SetBitDepths(&pool[tree->pool_index_right], pool, bit_depths, level + 1);
  } else {
    bit_depths[tree->value] = level;
  }
}

static void GenerateOptimalTree(const uint32_t* const histogram,
                                int histogram_size,
                                HuffmanTree* tree, int tree_depth_limit,
                                uint8_t* const bit_depths) {
  uint32_t count_min;
  HuffmanTree* tree_pool;
  int tree_size_orig = 0;
  int i;

  for (i = 0; i < histogram_size; ++i) {
    if (histogram[i] != 0) {
      ++tree_size_orig;
    }
  }

  if (tree_size_orig == 0) {
    return;
  }

  tree_pool = tree + tree_size_orig;

  assert(tree_size_orig <= (1 << (tree_depth_limit - 1)));
  for (count_min = 1; ; count_min *= 2) {
    int tree_size = tree_size_orig;

    int idx = 0;
    int j;
    for (j = 0; j < histogram_size; ++j) {
      if (histogram[j] != 0) {
        const uint32_t count =
            (histogram[j] < count_min) ? count_min : histogram[j];
        tree[idx].total_count = count;
        tree[idx].value = j;
        tree[idx].pool_index_left = -1;
        tree[idx].pool_index_right = -1;
        ++idx;
      }
    }

    qsort(tree, tree_size, sizeof(*tree), CompareHuffmanTrees);

    if (tree_size > 1) {
      int tree_pool_size = 0;
      while (tree_size > 1) {
        uint32_t count;
        tree_pool[tree_pool_size++] = tree[tree_size - 1];
        tree_pool[tree_pool_size++] = tree[tree_size - 2];
        count = tree_pool[tree_pool_size - 1].total_count +
                tree_pool[tree_pool_size - 2].total_count;
        tree_size -= 2;
        {

          int k;
          for (k = 0; k < tree_size; ++k) {
            if (tree[k].total_count <= count) {
              break;
            }
          }
          memmove(tree + (k + 1), tree + k, (tree_size - k) * sizeof(*tree));
          tree[k].total_count = count;
          tree[k].value = -1;

          tree[k].pool_index_left = tree_pool_size - 1;
          tree[k].pool_index_right = tree_pool_size - 2;
          tree_size = tree_size + 1;
        }
      }
      SetBitDepths(&tree[0], tree_pool, bit_depths, 0);
    } else if (tree_size == 1) {
      bit_depths[tree[0].value] = 1;
    }

    {

      int max_depth = bit_depths[0];
      for (j = 1; j < histogram_size; ++j) {
        if (max_depth < bit_depths[j]) {
          max_depth = bit_depths[j];
        }
      }
      if (max_depth <= tree_depth_limit) {
        break;
      }
    }
  }
}

static HuffmanTreeToken* CodeRepeatedValues(int repetitions,
                                            HuffmanTreeToken* tokens,
                                            int value, int prev_value) {
  assert(value <= MAX_ALLOWED_CODE_LENGTH);
  if (value != prev_value) {
    tokens->code = value;
    tokens->extra_bits = 0;
    ++tokens;
    --repetitions;
  }
  while (repetitions >= 1) {
    if (repetitions < 3) {
      int i;
      for (i = 0; i < repetitions; ++i) {
        tokens->code = value;
        tokens->extra_bits = 0;
        ++tokens;
      }
      break;
    } else if (repetitions < 7) {
      tokens->code = 16;
      tokens->extra_bits = repetitions - 3;
      ++tokens;
      break;
    } else {
      tokens->code = 16;
      tokens->extra_bits = 3;
      ++tokens;
      repetitions -= 6;
    }
  }
  return tokens;
}

static HuffmanTreeToken* CodeRepeatedZeros(int repetitions,
                                           HuffmanTreeToken* tokens) {
  while (repetitions >= 1) {
    if (repetitions < 3) {
      int i;
      for (i = 0; i < repetitions; ++i) {
        tokens->code = 0;
        tokens->extra_bits = 0;
        ++tokens;
      }
      break;
    } else if (repetitions < 11) {
      tokens->code = 17;
      tokens->extra_bits = repetitions - 3;
      ++tokens;
      break;
    } else if (repetitions < 139) {
      tokens->code = 18;
      tokens->extra_bits = repetitions - 11;
      ++tokens;
      break;
    } else {
      tokens->code = 18;
      tokens->extra_bits = 0x7f;
      ++tokens;
      repetitions -= 138;
    }
  }
  return tokens;
}

int VP8LCreateCompressedHuffmanTree(const HuffmanTreeCode* const tree,
                                    HuffmanTreeToken* tokens, int max_tokens) {
  HuffmanTreeToken* const starting_token = tokens;
  HuffmanTreeToken* const ending_token = tokens + max_tokens;
  const int depth_size = tree->num_symbols;
  int prev_value = 8;
  int i = 0;
  assert(tokens != NULL);
  while (i < depth_size) {
    const int value = tree->code_lengths[i];
    int k = i + 1;
    int runs;
    while (k < depth_size && tree->code_lengths[k] == value) ++k;
    runs = k - i;
    if (value == 0) {
      tokens = CodeRepeatedZeros(runs, tokens);
    } else {
      tokens = CodeRepeatedValues(runs, tokens, value, prev_value);
      prev_value = value;
    }
    i += runs;
    assert(tokens <= ending_token);
  }
  (void)ending_token;
  return (int)(tokens - starting_token);
}

static const uint8_t kReversedBits[16] = {
  0x0, 0x8, 0x4, 0xc, 0x2, 0xa, 0x6, 0xe,
  0x1, 0x9, 0x5, 0xd, 0x3, 0xb, 0x7, 0xf
};

static uint32_t ReverseBits(int num_bits, uint32_t bits) {
  uint32_t retval = 0;
  int i = 0;
  while (i < num_bits) {
    i += 4;
    retval |= kReversedBits[bits & 0xf] << (MAX_ALLOWED_CODE_LENGTH + 1 - i);
    bits >>= 4;
  }
  retval >>= (MAX_ALLOWED_CODE_LENGTH + 1 - num_bits);
  return retval;
}

static void ConvertBitDepthsToSymbols(HuffmanTreeCode* const tree) {

  int i;
  int len;
  uint32_t next_code[MAX_ALLOWED_CODE_LENGTH + 1];
  int depth_count[MAX_ALLOWED_CODE_LENGTH + 1] = { 0 };

  assert(tree != NULL);
  len = tree->num_symbols;
  for (i = 0; i < len; ++i) {
    const int code_length = tree->code_lengths[i];
    assert(code_length <= MAX_ALLOWED_CODE_LENGTH);
    ++depth_count[code_length];
  }
  depth_count[0] = 0;
  next_code[0] = 0;
  {
    uint32_t code = 0;
    for (i = 1; i <= MAX_ALLOWED_CODE_LENGTH; ++i) {
      code = (code + depth_count[i - 1]) << 1;
      next_code[i] = code;
    }
  }
  for (i = 0; i < len; ++i) {
    const int code_length = tree->code_lengths[i];
    tree->codes[i] = ReverseBits(code_length, next_code[code_length]++);
  }
}

void VP8LCreateHuffmanTree(uint32_t* const histogram, int tree_depth_limit,
                           uint8_t* const buf_rle, HuffmanTree* const huff_tree,
                           HuffmanTreeCode* const huff_code) {
  const int num_symbols = huff_code->num_symbols;
  memset(buf_rle, 0, num_symbols * sizeof(*buf_rle));
  OptimizeHuffmanForRle(num_symbols, buf_rle, histogram);
  GenerateOptimalTree(histogram, num_symbols, huff_tree, tree_depth_limit,
                      huff_code->code_lengths);

  ConvertBitDepthsToSymbols(huff_code);
}

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define MAX_HTREE_GROUPS    0x10000

HTreeGroup* VP8LHtreeGroupsNew(int num_htree_groups) {
  HTreeGroup* const htree_groups =
      (HTreeGroup*)WebPSafeMalloc(num_htree_groups, sizeof(*htree_groups));
  if (htree_groups == NULL) {
    return NULL;
  }
  assert(num_htree_groups <= MAX_HTREE_GROUPS);
  return htree_groups;
}

void VP8LHtreeGroupsFree(HTreeGroup* const htree_groups) {
  if (htree_groups != NULL) {
    WebPSafeFree(htree_groups);
  }
}

static WEBP_INLINE uint32_t GetNextKey(uint32_t key, int len) {
  uint32_t step = 1 << (len - 1);
  while (key & step) {
    step >>= 1;
  }
  return step ? (key & (step - 1)) + step : key;
}

static WEBP_INLINE void ReplicateValue(HuffmanCode* table,
                                       int step, int end,
                                       HuffmanCode code) {
  assert(end % step == 0);
  do {
    end -= step;
    table[end] = code;
  } while (end > 0);
}

static WEBP_INLINE int NextTableBitSize(const int* const count,
                                        int len, int root_bits) {
  int left = 1 << (len - root_bits);
  while (len < MAX_ALLOWED_CODE_LENGTH) {
    left -= count[len];
    if (left <= 0) break;
    ++len;
    left <<= 1;
  }
  return len - root_bits;
}

static int BuildHuffmanTable(HuffmanCode* const root_table, int root_bits,
                             const int code_lengths[], int code_lengths_size,
                             uint16_t sorted[]) {
  HuffmanCode* table = root_table;
  int total_size = 1 << root_bits;
  int len;
  int symbol;

  int count[MAX_ALLOWED_CODE_LENGTH + 1] = { 0 };

  int offset[MAX_ALLOWED_CODE_LENGTH + 1];

  assert(code_lengths_size != 0);
  assert(code_lengths != NULL);
  assert((root_table != NULL && sorted != NULL) ||
         (root_table == NULL && sorted == NULL));
  assert(root_bits > 0);

  for (symbol = 0; symbol < code_lengths_size; ++symbol) {
    if (code_lengths[symbol] > MAX_ALLOWED_CODE_LENGTH) {
      return 0;
    }
    ++count[code_lengths[symbol]];
  }

  if (count[0] == code_lengths_size) {
    return 0;
  }

  offset[1] = 0;
  for (len = 1; len < MAX_ALLOWED_CODE_LENGTH; ++len) {
    if (count[len] > (1 << len)) {
      return 0;
    }
    offset[len + 1] = offset[len] + count[len];
  }

  for (symbol = 0; symbol < code_lengths_size; ++symbol) {
    const int symbol_code_length = code_lengths[symbol];
    if (code_lengths[symbol] > 0) {
      if (sorted != NULL) {
        if(offset[symbol_code_length] >= code_lengths_size) {
            return 0;
        }
        sorted[offset[symbol_code_length]++] = symbol;
      } else {
        offset[symbol_code_length]++;
      }
    }
  }

  if (offset[MAX_ALLOWED_CODE_LENGTH] == 1) {
    if (sorted != NULL) {
      HuffmanCode code;
      code.bits = 0;
      code.value = (uint16_t)sorted[0];
      ReplicateValue(table, 1, total_size, code);
    }
    return total_size;
  }

  {
    int step;
    uint32_t low = 0xffffffffu;
    uint32_t mask = total_size - 1;
    uint32_t key = 0;
    int num_nodes = 1;
    int num_open = 1;
    int table_bits = root_bits;
    int table_size = 1 << table_bits;
    symbol = 0;

    for (len = 1, step = 2; len <= root_bits; ++len, step <<= 1) {
      num_open <<= 1;
      num_nodes += num_open;
      num_open -= count[len];
      if (num_open < 0) {
        return 0;
      }
      if (root_table == NULL) continue;
      for (; count[len] > 0; --count[len]) {
        HuffmanCode code;
        code.bits = (uint8_t)len;
        code.value = (uint16_t)sorted[symbol++];
        ReplicateValue(&table[key], step, table_size, code);
        key = GetNextKey(key, len);
      }
    }

    for (len = root_bits + 1, step = 2; len <= MAX_ALLOWED_CODE_LENGTH;
         ++len, step <<= 1) {
      num_open <<= 1;
      num_nodes += num_open;
      num_open -= count[len];
      if (num_open < 0) {
        return 0;
      }
      for (; count[len] > 0; --count[len]) {
        HuffmanCode code;
        if ((key & mask) != low) {
          if (root_table != NULL) table += table_size;
          table_bits = NextTableBitSize(count, len, root_bits);
          table_size = 1 << table_bits;
          total_size += table_size;
          low = key & mask;
          if (root_table != NULL) {
            root_table[low].bits = (uint8_t)(table_bits + root_bits);
            root_table[low].value = (uint16_t)((table - root_table) - low);
          }
        }
        if (root_table != NULL) {
          code.bits = (uint8_t)(len - root_bits);
          code.value = (uint16_t)sorted[symbol++];
          ReplicateValue(&table[key >> root_bits], step, table_size, code);
        }
        key = GetNextKey(key, len);
      }
    }

    if (num_nodes != 2 * offset[MAX_ALLOWED_CODE_LENGTH] - 1) {
      return 0;
    }
  }

  return total_size;
}

#define MAX_CODE_LENGTHS_SIZE \
  ((1 << MAX_CACHE_BITS) + NUM_LITERAL_CODES + NUM_LENGTH_CODES)

#define SORTED_SIZE_CUTOFF 512
int VP8LBuildHuffmanTable(HuffmanTables* const root_table, int root_bits,
                          const int code_lengths[], int code_lengths_size) {
  const int total_size =
      BuildHuffmanTable(NULL, root_bits, code_lengths, code_lengths_size, NULL);
  assert(code_lengths_size <= MAX_CODE_LENGTHS_SIZE);
  if (total_size == 0 || root_table == NULL) return total_size;

  if (root_table->curr_segment->curr_table + total_size >=
      root_table->curr_segment->start + root_table->curr_segment->size) {

    const int segment_size = root_table->curr_segment->size;
    struct HuffmanTablesSegment* next =
        (HuffmanTablesSegment*)WebPSafeMalloc(1, sizeof(*next));
    if (next == NULL) return 0;

    next->size = total_size > segment_size ? total_size : segment_size;
    next->start =
        (HuffmanCode*)WebPSafeMalloc(next->size, sizeof(*next->start));
    if (next->start == NULL) {
      WebPSafeFree(next);
      return 0;
    }
    next->curr_table = next->start;
    next->next = NULL;

    root_table->curr_segment->next = next;
    root_table->curr_segment = next;
  }
  if (code_lengths_size <= SORTED_SIZE_CUTOFF) {

    uint16_t sorted[SORTED_SIZE_CUTOFF];
    BuildHuffmanTable(root_table->curr_segment->curr_table, root_bits,
                      code_lengths, code_lengths_size, sorted);
  } else {
    uint16_t* const sorted =
        (uint16_t*)WebPSafeMalloc(code_lengths_size, sizeof(*sorted));
    if (sorted == NULL) return 0;
    BuildHuffmanTable(root_table->curr_segment->curr_table, root_bits,
                      code_lengths, code_lengths_size, sorted);
    WebPSafeFree(sorted);
  }
  return total_size;
}

int VP8LHuffmanTablesAllocate(int size, HuffmanTables* huffman_tables) {

  HuffmanTablesSegment* const root = &huffman_tables->root;
  huffman_tables->curr_segment = root;
  root->next = NULL;

  root->start = (HuffmanCode*)WebPSafeMalloc(size, sizeof(*root->start));
  if (root->start == NULL) return 0;
  root->curr_table = root->start;
  root->size = size;
  return 1;
}

void VP8LHuffmanTablesDeallocate(HuffmanTables* const huffman_tables) {
  HuffmanTablesSegment *current, *next;
  if (huffman_tables == NULL) return;

  current = &huffman_tables->root;
  next = current->next;
  WebPSafeFree(current->start);
  current->start = NULL;
  current->next = NULL;
  current = next;

  while (current != NULL) {
    next = current->next;
    WebPSafeFree(current->start);
    WebPSafeFree(current);
    current = next;
  }
}

#ifndef WEBP_UTILS_PALETTE_H_
#define WEBP_UTILS_PALETTE_H_

struct WebPPicture;

typedef enum PaletteSorting {
  kSortedDefault = 0,

  kMinimizeDelta = 1,

  kModifiedZeng = 2,
  kUnusedPalette = 3,
  kPaletteSortingNum = 4
} PaletteSorting;

int SearchColorNoIdx(const uint32_t sorted[], uint32_t color, int num_colors);

void PrepareMapToPalette(const uint32_t palette[], uint32_t num_colors,
                         uint32_t sorted[], uint32_t idx_map[]);

int GetColorPalette(const struct WebPPicture* const pic,
                    uint32_t* const palette);

int PaletteSort(PaletteSorting method, const struct WebPPicture* const pic,
                const uint32_t* const palette_sorted, uint32_t num_colors,
                uint32_t* const palette);

#endif

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#ifndef WEBP_WEBP_ENCODE_H_
#define WEBP_WEBP_ENCODE_H_

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WEBP_ENCODER_ABI_VERSION 0x0210

typedef struct WebPConfig WebPConfig;
typedef struct WebPPicture WebPPicture;
typedef struct WebPAuxStats WebPAuxStats;
typedef struct WebPMemoryWriter WebPMemoryWriter;

WEBP_EXTERN int WebPGetEncoderVersion(void);

WEBP_EXTERN size_t WebPEncodeRGB(const uint8_t* rgb,
                                 int width, int height, int stride,
                                 float quality_factor, uint8_t** output);
WEBP_EXTERN size_t WebPEncodeBGR(const uint8_t* bgr,
                                 int width, int height, int stride,
                                 float quality_factor, uint8_t** output);
WEBP_EXTERN size_t WebPEncodeRGBA(const uint8_t* rgba,
                                  int width, int height, int stride,
                                  float quality_factor, uint8_t** output);
WEBP_EXTERN size_t WebPEncodeBGRA(const uint8_t* bgra,
                                  int width, int height, int stride,
                                  float quality_factor, uint8_t** output);

WEBP_EXTERN size_t WebPEncodeLosslessRGB(const uint8_t* rgb,
                                         int width, int height, int stride,
                                         uint8_t** output);
WEBP_EXTERN size_t WebPEncodeLosslessBGR(const uint8_t* bgr,
                                         int width, int height, int stride,
                                         uint8_t** output);
WEBP_EXTERN size_t WebPEncodeLosslessRGBA(const uint8_t* rgba,
                                          int width, int height, int stride,
                                          uint8_t** output);
WEBP_EXTERN size_t WebPEncodeLosslessBGRA(const uint8_t* bgra,
                                          int width, int height, int stride,
                                          uint8_t** output);

typedef enum WebPImageHint {
  WEBP_HINT_DEFAULT = 0,
  WEBP_HINT_PICTURE,
  WEBP_HINT_PHOTO,
  WEBP_HINT_GRAPH,
  WEBP_HINT_LAST
} WebPImageHint;

struct WebPConfig {
  int lossless;
  float quality;

  int method;

  WebPImageHint image_hint;

  int target_size;

  float target_PSNR;

  int segments;
  int sns_strength;
  int filter_strength;
  int filter_sharpness;
  int filter_type;

  int autofilter;
  int alpha_compression;

  int alpha_filtering;

  int alpha_quality;

  int pass;

  int show_compressed;

  int preprocessing;

  int partitions;

  int partition_limit;

  int emulate_jpeg_size;

  int thread_level;
  int low_memory;

  int near_lossless;

  int exact;

  int use_delta_palette;
  int use_sharp_yuv;

  int qmin;
  int qmax;
};

typedef enum WebPPreset {
  WEBP_PRESET_DEFAULT = 0,
  WEBP_PRESET_PICTURE,
  WEBP_PRESET_PHOTO,
  WEBP_PRESET_DRAWING,
  WEBP_PRESET_ICON,
  WEBP_PRESET_TEXT
} WebPPreset;

WEBP_NODISCARD WEBP_EXTERN int WebPConfigInitInternal(WebPConfig*, WebPPreset,
                                                      float, int);

WEBP_NODISCARD static WEBP_INLINE int WebPConfigInit(WebPConfig* config) {
  return WebPConfigInitInternal(config, WEBP_PRESET_DEFAULT, 75.f,
                                WEBP_ENCODER_ABI_VERSION);
}

WEBP_NODISCARD static WEBP_INLINE int WebPConfigPreset(WebPConfig* config,
                                                       WebPPreset preset,
                                                       float quality) {
  return WebPConfigInitInternal(config, preset, quality,
                                WEBP_ENCODER_ABI_VERSION);
}

WEBP_NODISCARD WEBP_EXTERN int WebPConfigLosslessPreset(WebPConfig* config,
                                                        int level);

WEBP_NODISCARD WEBP_EXTERN int WebPValidateConfig(const WebPConfig* config);

struct WebPAuxStats {
  int coded_size;

  float PSNR[5];
  int block_count[3];
  int header_bytes[2];

  int residual_bytes[3][4];

  int segment_size[4];
  int segment_quant[4];
  int segment_level[4];

  int alpha_data_size;
  int layer_data_size;

  uint32_t lossless_features;

  int histogram_bits;
  int transform_bits;
  int cache_bits;
  int palette_size;
  int lossless_size;
  int lossless_hdr_size;
  int lossless_data_size;
  int cross_color_transform_bits;

  uint32_t pad[1];
};

typedef int (*WebPWriterFunction)(const uint8_t* data, size_t data_size,
                                  const WebPPicture* picture);

struct WebPMemoryWriter {
  uint8_t* mem;
  size_t   size;
  size_t   max_size;
  uint32_t pad[1];
};

WEBP_EXTERN void WebPMemoryWriterInit(WebPMemoryWriter* writer);

WEBP_EXTERN void WebPMemoryWriterClear(WebPMemoryWriter* writer);

WEBP_NODISCARD WEBP_EXTERN int WebPMemoryWrite(
    const uint8_t* data, size_t data_size, const WebPPicture* picture);

typedef int (*WebPProgressHook)(int percent, const WebPPicture* picture);

typedef enum WebPEncCSP {

  WEBP_YUV420  = 0,
  WEBP_YUV420A = 4,
  WEBP_CSP_UV_MASK = 3,
  WEBP_CSP_ALPHA_BIT = 4
} WebPEncCSP;

typedef enum WebPEncodingError {
  VP8_ENC_OK = 0,
  VP8_ENC_ERROR_OUT_OF_MEMORY,
  VP8_ENC_ERROR_BITSTREAM_OUT_OF_MEMORY,
  VP8_ENC_ERROR_NULL_PARAMETER,
  VP8_ENC_ERROR_INVALID_CONFIGURATION,
  VP8_ENC_ERROR_BAD_DIMENSION,
  VP8_ENC_ERROR_PARTITION0_OVERFLOW,
  VP8_ENC_ERROR_PARTITION_OVERFLOW,
  VP8_ENC_ERROR_BAD_WRITE,
  VP8_ENC_ERROR_FILE_TOO_BIG,
  VP8_ENC_ERROR_USER_ABORT,
  VP8_ENC_ERROR_LAST
} WebPEncodingError;

#define WEBP_MAX_DIMENSION 16383

struct WebPPicture {

  int use_argb;

  WebPEncCSP colorspace;
  int width, height;
  uint8_t* y, *u, *v;
  int y_stride, uv_stride;
  uint8_t* a;
  int a_stride;
  uint32_t pad1[2];

  uint32_t* argb;
  int argb_stride;
  uint32_t pad2[3];

  WebPWriterFunction writer;
  void* custom_ptr;

  int extra_info_type;

  uint8_t* extra_info;

  WebPAuxStats* stats;

  WebPEncodingError error_code;

  WebPProgressHook progress_hook;

  void* user_data;

  uint32_t pad3[3];

  uint8_t* pad4, *pad5;
  uint32_t pad6[8];

  void* memory_;
  void* memory_argb_;
  void* pad7[2];
};

WEBP_NODISCARD WEBP_EXTERN int WebPPictureInitInternal(WebPPicture*, int);

WEBP_NODISCARD static WEBP_INLINE int WebPPictureInit(WebPPicture* picture) {
  return WebPPictureInitInternal(picture, WEBP_ENCODER_ABI_VERSION);
}

WEBP_NODISCARD WEBP_EXTERN int WebPPictureAlloc(WebPPicture* picture);

WEBP_EXTERN void WebPPictureFree(WebPPicture* picture);

WEBP_NODISCARD WEBP_EXTERN int WebPPictureCopy(const WebPPicture* src,
                                               WebPPicture* dst);

WEBP_NODISCARD WEBP_EXTERN int WebPPlaneDistortion(
    const uint8_t* src, size_t src_stride,
    const uint8_t* ref, size_t ref_stride, int width, int height, size_t x_step,
    int type,
    float* distortion, float* result);

WEBP_NODISCARD WEBP_EXTERN int WebPPictureDistortion(
    const WebPPicture* src, const WebPPicture* ref,
    int metric_type,
    float result[5]);

WEBP_NODISCARD WEBP_EXTERN int WebPPictureCrop(
    WebPPicture* picture, int left, int top, int width, int height);

WEBP_NODISCARD WEBP_EXTERN int WebPPictureView(
    const WebPPicture* src, int left, int top, int width, int height,
    WebPPicture* dst);

WEBP_EXTERN int WebPPictureIsView(const WebPPicture* picture);

WEBP_NODISCARD WEBP_EXTERN int WebPPictureRescale(WebPPicture* picture,
                                                  int width, int height);

WEBP_NODISCARD WEBP_EXTERN int WebPPictureImportRGB(
    WebPPicture* picture, const uint8_t* rgb, int rgb_stride);

WEBP_NODISCARD WEBP_EXTERN int WebPPictureImportRGBA(
    WebPPicture* picture, const uint8_t* rgba, int rgba_stride);

WEBP_NODISCARD WEBP_EXTERN int WebPPictureImportRGBX(
    WebPPicture* picture, const uint8_t* rgbx, int rgbx_stride);

WEBP_NODISCARD WEBP_EXTERN int WebPPictureImportBGR(
    WebPPicture* picture, const uint8_t* bgr, int bgr_stride);
WEBP_NODISCARD WEBP_EXTERN int WebPPictureImportBGRA(
    WebPPicture* picture, const uint8_t* bgra, int bgra_stride);
WEBP_NODISCARD WEBP_EXTERN int WebPPictureImportBGRX(
    WebPPicture* picture, const uint8_t* bgrx, int bgrx_stride);

WEBP_NODISCARD WEBP_EXTERN int WebPPictureARGBToYUVA(
    WebPPicture* picture, WebPEncCSP );

WEBP_NODISCARD WEBP_EXTERN int WebPPictureARGBToYUVADithered(
    WebPPicture* picture, WebPEncCSP colorspace, float dithering);

WEBP_NODISCARD WEBP_EXTERN int WebPPictureSharpARGBToYUVA(WebPPicture* picture);

WEBP_NODISCARD WEBP_EXTERN int WebPPictureSmartARGBToYUVA(WebPPicture* picture);

WEBP_NODISCARD WEBP_EXTERN int WebPPictureYUVAToARGB(WebPPicture* picture);

WEBP_EXTERN void WebPCleanupTransparentArea(WebPPicture* picture);

WEBP_EXTERN int WebPPictureHasTransparency(const WebPPicture* picture);

WEBP_EXTERN void WebPBlendAlpha(WebPPicture* picture, uint32_t background_rgb);

WEBP_NODISCARD WEBP_EXTERN int WebPEncode(const WebPConfig* config,
                                          WebPPicture* picture);

#ifdef __cplusplus
}
#endif

#endif

static int PaletteCompareColorsForQsort(const void* p1, const void* p2) {
  const uint32_t a = WebPMemToUint32((uint8_t*)p1);
  const uint32_t b = WebPMemToUint32((uint8_t*)p2);
  assert(a != b);
  return (a < b) ? -1 : 1;
}

static WEBP_INLINE uint32_t PaletteComponentDistance(uint32_t v) {
  return (v <= 128) ? v : (256 - v);
}

static WEBP_INLINE uint32_t PaletteColorDistance(uint32_t col1, uint32_t col2) {
  const uint32_t diff = VP8LSubPixels(col1, col2);
  const int kMoreWeightForRGBThanForAlpha = 9;
  uint32_t score;
  score = PaletteComponentDistance((diff >> 0) & 0xff);
  score += PaletteComponentDistance((diff >> 8) & 0xff);
  score += PaletteComponentDistance((diff >> 16) & 0xff);
  score *= kMoreWeightForRGBThanForAlpha;
  score += PaletteComponentDistance((diff >> 24) & 0xff);
  return score;
}

static WEBP_INLINE void SwapColor(uint32_t* const col1, uint32_t* const col2) {
  const uint32_t tmp = *col1;
  *col1 = *col2;
  *col2 = tmp;
}

int SearchColorNoIdx(const uint32_t sorted[], uint32_t color, int num_colors) {
  int low = 0, hi = num_colors;
  if (sorted[low] == color) return low;
  while (1) {
    const int mid = (low + hi) >> 1;
    if (sorted[mid] == color) {
      return mid;
    } else if (sorted[mid] < color) {
      low = mid;
    } else {
      hi = mid;
    }
  }
  assert(0);
  return 0;
}

void PrepareMapToPalette(const uint32_t palette[], uint32_t num_colors,
                         uint32_t sorted[], uint32_t idx_map[]) {
  uint32_t i;
  memcpy(sorted, palette, num_colors * sizeof(*sorted));
  qsort(sorted, num_colors, sizeof(*sorted), PaletteCompareColorsForQsort);
  for (i = 0; i < num_colors; ++i) {
    idx_map[SearchColorNoIdx(sorted, palette[i], num_colors)] = i;
  }
}

#define COLOR_HASH_SIZE (MAX_PALETTE_SIZE * 4)
#define COLOR_HASH_RIGHT_SHIFT 22

int GetColorPalette(const WebPPicture* const pic, uint32_t* const palette) {
  int i;
  int x, y;
  int num_colors = 0;
  uint8_t in_use[COLOR_HASH_SIZE] = {0};
  uint32_t colors[COLOR_HASH_SIZE] = {0};
  const uint32_t* argb = pic->argb;
  const int width = pic->width;
  const int height = pic->height;
  uint32_t last_pix = ~argb[0];
  assert(pic != NULL);
  assert(pic->use_argb);

  for (y = 0; y < height; ++y) {
    for (x = 0; x < width; ++x) {
      int key;
      if (argb[x] == last_pix) {
        continue;
      }
      last_pix = argb[x];
      key = VP8LHashPix(last_pix, COLOR_HASH_RIGHT_SHIFT);
      while (1) {
        if (!in_use[key]) {
          colors[key] = last_pix;
          in_use[key] = 1;
          ++num_colors;
          if (num_colors > MAX_PALETTE_SIZE) {
            return MAX_PALETTE_SIZE + 1;
          }
          break;
        } else if (colors[key] == last_pix) {
          break;
        } else {

          ++key;
          key &= (COLOR_HASH_SIZE - 1);
        }
      }
    }
    argb += pic->argb_stride;
  }

  if (palette != NULL) {
    num_colors = 0;
    for (i = 0; i < COLOR_HASH_SIZE; ++i) {
      if (in_use[i]) {
        palette[num_colors] = colors[i];
        ++num_colors;
      }
    }
    qsort(palette, num_colors, sizeof(*palette), PaletteCompareColorsForQsort);
  }
  return num_colors;
}

#undef COLOR_HASH_SIZE
#undef COLOR_HASH_RIGHT_SHIFT

static int PaletteHasNonMonotonousDeltas(const uint32_t* const palette,
                                         int num_colors) {
  uint32_t predict = 0x000000;
  int i;
  uint8_t sign_found = 0x00;
  for (i = 0; i < num_colors; ++i) {
    const uint32_t diff = VP8LSubPixels(palette[i], predict);
    const uint8_t rd = (diff >> 16) & 0xff;
    const uint8_t gd = (diff >> 8) & 0xff;
    const uint8_t bd = (diff >> 0) & 0xff;
    if (rd != 0x00) {
      sign_found |= (rd < 0x80) ? 1 : 2;
    }
    if (gd != 0x00) {
      sign_found |= (gd < 0x80) ? 8 : 16;
    }
    if (bd != 0x00) {
      sign_found |= (bd < 0x80) ? 64 : 128;
    }
    predict = palette[i];
  }
  return (sign_found & (sign_found << 1)) != 0;
}

static void PaletteSortMinimizeDeltas(const uint32_t* const palette_sorted,
                                      int num_colors, uint32_t* const palette) {
  uint32_t predict = 0x00000000;
  int i, k;
  memcpy(palette, palette_sorted, num_colors * sizeof(*palette));
  if (!PaletteHasNonMonotonousDeltas(palette_sorted, num_colors)) return;

  if (num_colors > 17) {
    if (palette[0] == 0) {
      --num_colors;
      SwapColor(&palette[num_colors], &palette[0]);
    }
  }
  for (i = 0; i < num_colors; ++i) {
    int best_ix = i;
    uint32_t best_score = ~0U;
    for (k = i; k < num_colors; ++k) {
      const uint32_t cur_score = PaletteColorDistance(palette[k], predict);
      if (best_score > cur_score) {
        best_score = cur_score;
        best_ix = k;
      }
    }
    SwapColor(&palette[best_ix], &palette[i]);
    predict = palette[i];
  }
}

static void CoOccurrenceFindMax(const uint32_t* const cooccurrence,
                                uint32_t num_colors, uint8_t* const c1,
                                uint8_t* const c2) {

  uint32_t best_sum = 0u;
  uint32_t i, j, best_cooccurrence;
  *c1 = 0u;
  for (i = 0; i < num_colors; ++i) {
    uint32_t sum = 0;
    for (j = 0; j < num_colors; ++j) sum += cooccurrence[i * num_colors + j];
    if (sum > best_sum) {
      best_sum = sum;
      *c1 = i;
    }
  }

  *c2 = 0u;
  best_cooccurrence = 0u;
  for (i = 0; i < num_colors; ++i) {
    if (cooccurrence[*c1 * num_colors + i] > best_cooccurrence) {
      best_cooccurrence = cooccurrence[*c1 * num_colors + i];
      *c2 = i;
    }
  }
  assert(*c1 != *c2);
}

static int CoOccurrenceBuild(const WebPPicture* const pic,
                             const uint32_t* const palette, uint32_t num_colors,
                             uint32_t* cooccurrence) {
  uint32_t *lines, *line_top, *line_current, *line_tmp;
  int x, y;
  const uint32_t* src = pic->argb;
  uint32_t prev_pix = ~src[0];
  uint32_t prev_idx = 0u;
  uint32_t idx_map[MAX_PALETTE_SIZE] = {0};
  uint32_t palette_sorted[MAX_PALETTE_SIZE];
  lines = (uint32_t*)WebPSafeMalloc(2 * pic->width, sizeof(*lines));
  if (lines == NULL) {
    return 0;
  }
  line_top = &lines[0];
  line_current = &lines[pic->width];
  PrepareMapToPalette(palette, num_colors, palette_sorted, idx_map);
  for (y = 0; y < pic->height; ++y) {
    for (x = 0; x < pic->width; ++x) {
      const uint32_t pix = src[x];
      if (pix != prev_pix) {
        prev_idx = idx_map[SearchColorNoIdx(palette_sorted, pix, num_colors)];
        prev_pix = pix;
      }
      line_current[x] = prev_idx;

      if (x > 0 && prev_idx != line_current[x - 1]) {
        const uint32_t left_idx = line_current[x - 1];
        ++cooccurrence[prev_idx * num_colors + left_idx];
        ++cooccurrence[left_idx * num_colors + prev_idx];
      }
      if (y > 0 && prev_idx != line_top[x]) {
        const uint32_t top_idx = line_top[x];
        ++cooccurrence[prev_idx * num_colors + top_idx];
        ++cooccurrence[top_idx * num_colors + prev_idx];
      }
    }
    line_tmp = line_top;
    line_top = line_current;
    line_current = line_tmp;
    src += pic->argb_stride;
  }
  WebPSafeFree(lines);
  return 1;
}

struct Sum {
  uint8_t index;
  uint32_t sum;
};

static int PaletteSortModifiedZeng(const WebPPicture* const pic,
                                   const uint32_t* const palette_in,
                                   uint32_t num_colors,
                                   uint32_t* const palette) {
  uint32_t i, j, ind;
  uint8_t remapping[MAX_PALETTE_SIZE];
  uint32_t* cooccurrence;
  struct Sum sums[MAX_PALETTE_SIZE];
  uint32_t first, last;
  uint32_t num_sums;

  if (num_colors <= 1) return 1;

  cooccurrence =
      (uint32_t*)WebPSafeCalloc(num_colors * num_colors, sizeof(*cooccurrence));
  if (cooccurrence == NULL) {
    return 0;
  }
  if (!CoOccurrenceBuild(pic, palette_in, num_colors, cooccurrence)) {
    WebPSafeFree(cooccurrence);
    return 0;
  }

  CoOccurrenceFindMax(cooccurrence, num_colors, &remapping[0], &remapping[1]);

  first = 0;
  last = 1;
  num_sums = num_colors - 2;
  if (num_sums > 0) {

    struct Sum* best_sum = &sums[0];
    best_sum->index = 0u;
    best_sum->sum = 0u;
    for (i = 0, j = 0; i < num_colors; ++i) {
      if (i == remapping[0] || i == remapping[1]) continue;
      sums[j].index = i;
      sums[j].sum = cooccurrence[i * num_colors + remapping[0]] +
                    cooccurrence[i * num_colors + remapping[1]];
      if (sums[j].sum > best_sum->sum) best_sum = &sums[j];
      ++j;
    }

    while (num_sums > 0) {
      const uint8_t best_index = best_sum->index;

      int32_t delta = 0;
      const int32_t n = num_colors - num_sums;
      for (ind = first, j = 0; (ind + j) % num_colors != last + 1; ++j) {
        const uint16_t l_j = remapping[(ind + j) % num_colors];
        delta += (n - 1 - 2 * (int32_t)j) *
                 (int32_t)cooccurrence[best_index * num_colors + l_j];
      }
      if (delta > 0) {
        first = (first == 0) ? num_colors - 1 : first - 1;
        remapping[first] = best_index;
      } else {
        ++last;
        remapping[last] = best_index;
      }

      *best_sum = sums[num_sums - 1];
      --num_sums;

      best_sum = &sums[0];
      for (i = 0; i < num_sums; ++i) {
        sums[i].sum += cooccurrence[best_index * num_colors + sums[i].index];
        if (sums[i].sum > best_sum->sum) best_sum = &sums[i];
      }
    }
  }
  assert((last + 1) % num_colors == first);
  WebPSafeFree(cooccurrence);

  for (i = 0; i < num_colors; ++i) {
    palette[i] = palette_in[remapping[(first + i) % num_colors]];
  }
  return 1;
}

int PaletteSort(PaletteSorting method, const struct WebPPicture* const pic,
                const uint32_t* const palette_sorted, uint32_t num_colors,
                uint32_t* const palette) {
  switch (method) {
    case kSortedDefault:
      if (palette_sorted[0] == 0 && num_colors > 17) {
        memcpy(palette, palette_sorted + 1,
               (num_colors - 1) * sizeof(*palette_sorted));
        palette[num_colors - 1] = 0;
      } else {
        memcpy(palette, palette_sorted, num_colors * sizeof(*palette));
      }
      return 1;
    case kMinimizeDelta:
      PaletteSortMinimizeDeltas(palette_sorted, num_colors, palette);
      return 1;
    case kModifiedZeng:
      return PaletteSortModifiedZeng(pic, palette_sorted, num_colors, palette);
    case kUnusedPalette:
    case kPaletteSortingNum:
      break;
  }

  assert(0);
  return 0;
}

#define clip_8b quant_levels_dec_utils_clip_8b
#include <string.h>

#define FIX 16
#define LFIX 2
#define LUT_SIZE ((1 << (8 + LFIX)) - 1)

#if defined(USE_DITHERING)

#define DFIX 4
#define DSIZE 4

static const uint8_t kOrderedDither[DSIZE][DSIZE] = {
  {  0,  8,  2, 10 },
  { 12,  4, 14,  6 },
  {  3, 11,  1,  9 },
  { 15,  7, 13,  5 }
};

#else
#define DFIX 0
#endif

typedef struct {
  int width, height;
  int stride;
  int row;
  uint8_t* src;
  uint8_t* dst;

  int radius;
  int scale;

  void* mem;

  uint16_t* start;
  uint16_t* cur;
  uint16_t* end;
  uint16_t* top;
  uint16_t* average;

  int num_levels;
  int min, max;
  int min_level_dist;

  int16_t* correction;
} SmoothParams;

#define CLIP_8b_MASK (int)(~0U << (8 + DFIX))
static WEBP_INLINE uint8_t clip_8b(int v) {
  return (!(v & CLIP_8b_MASK)) ? (uint8_t)(v >> DFIX) : (v < 0) ? 0u : 255u;
}
#undef CLIP_8b_MASK

static void VFilter(SmoothParams* const p) {
  const uint8_t* src = p->src;
  const int w = p->width;
  uint16_t* const cur = p->cur;
  const uint16_t* const top = p->top;
  uint16_t* const out = p->end;
  uint16_t sum = 0;
  int x;

  for (x = 0; x < w; ++x) {
    uint16_t new_value;
    sum += src[x];
    new_value = top[x] + sum;
    out[x] = new_value - cur[x];
    cur[x] = new_value;
  }

  p->top = p->cur;
  p->cur += w;
  if (p->cur == p->end) p->cur = p->start;

  if (p->row >= 0 && p->row < p->height - 1) {
    p->src += p->stride;
  }
}

static void HFilter(SmoothParams* const p) {
  const uint16_t* const in = p->end;
  uint16_t* const out = p->average;
  const uint32_t scale = p->scale;
  const int w = p->width;
  const int r = p->radius;

  int x;
  for (x = 0; x <= r; ++x) {
    const uint16_t delta = in[x + r - 1] + in[r - x];
    out[x] = (delta * scale) >> FIX;
  }
  for (; x < w - r; ++x) {
    const uint16_t delta = in[x + r] - in[x - r - 1];
    out[x] = (delta * scale) >> FIX;
  }
  for (; x < w; ++x) {
    const uint16_t delta =
        2 * in[w - 1] - in[2 * w - 2 - r - x] - in[x - r - 1];
    out[x] = (delta * scale) >> FIX;
  }
}

static void ApplyFilter(SmoothParams* const p) {
  const uint16_t* const average = p->average;
  const int w = p->width;
  const int16_t* const correction = p->correction;
#if defined(USE_DITHERING)
  const uint8_t* const dither = kOrderedDither[p->row % DSIZE];
#endif
  uint8_t* const dst = p->dst;
  int x;
  for (x = 0; x < w; ++x) {
    const int v = dst[x];
    if (v < p->max && v > p->min) {
      const int c = (v << DFIX) + correction[average[x] - (v << LFIX)];
#if defined(USE_DITHERING)
      dst[x] = clip_8b(c + dither[x % DSIZE]);
#else
      dst[x] = clip_8b(c);
#endif
    }
  }
  p->dst += p->stride;
}

static void InitCorrectionLUT(int16_t* const lut, int min_dist) {

  const int threshold1 = min_dist << LFIX;
  const int threshold2 = (3 * threshold1) >> 2;
  const int max_threshold = threshold2 << DFIX;
  const int delta = threshold1 - threshold2;
  int i;
  for (i = 1; i <= LUT_SIZE; ++i) {
    int c = (i <= threshold2) ? (i << DFIX)
          : (i < threshold1) ? max_threshold * (threshold1 - i) / delta
          : 0;
    c >>= LFIX;
    lut[+i] = +c;
    lut[-i] = -c;
  }
  lut[0] = 0;
}

static void CountLevels(SmoothParams* const p) {
  int i, j, last_level;
  uint8_t used_levels[256] = { 0 };
  const uint8_t* data = p->src;
  p->min = 255;
  p->max = 0;
  for (j = 0; j < p->height; ++j) {
    for (i = 0; i < p->width; ++i) {
      const int v = data[i];
      if (v < p->min) p->min = v;
      if (v > p->max) p->max = v;
      used_levels[v] = 1;
    }
    data += p->stride;
  }

  p->min_level_dist = p->max - p->min;
  last_level = -1;
  for (i = 0; i < 256; ++i) {
    if (used_levels[i]) {
      ++p->num_levels;
      if (last_level >= 0) {
        const int level_dist = i - last_level;
        if (level_dist < p->min_level_dist) {
          p->min_level_dist = level_dist;
        }
      }
      last_level = i;
    }
  }
}

static int InitParams(uint8_t* const data, int width, int height, int stride,
                      int radius, SmoothParams* const p) {
  const int R = 2 * radius + 1;

  const size_t size_scratch_m = (R + 1) * width * sizeof(*p->start);
  const size_t size_m =  width * sizeof(*p->average);
  const size_t size_lut = (1 + 2 * LUT_SIZE) * sizeof(*p->correction);
  const size_t total_size = size_scratch_m + size_m + size_lut;
  uint8_t* mem = (uint8_t*)WebPSafeMalloc(1U, total_size);

  if (mem == NULL) return 0;
  p->mem = (void*)mem;

  p->start = (uint16_t*)mem;
  p->cur = p->start;
  p->end = p->start + R * width;
  p->top = p->end - width;
  memset(p->top, 0, width * sizeof(*p->top));
  mem += size_scratch_m;

  p->average = (uint16_t*)mem;
  mem += size_m;

  p->width = width;
  p->height = height;
  p->stride = stride;
  p->src = data;
  p->dst = data;
  p->radius = radius;
  p->scale = (1 << (FIX + LFIX)) / (R * R);
  p->row = -radius;

  CountLevels(p);

  p->correction = ((int16_t*)mem) + LUT_SIZE;
  InitCorrectionLUT(p->correction, p->min_level_dist);

  return 1;
}

static void CleanupParams(SmoothParams* const p) {
  WebPSafeFree(p->mem);
}

int WebPDequantizeLevels(uint8_t* const data, int width, int height, int stride,
                         int strength) {
  int radius = 4 * strength / 100;

  if (strength < 0 || strength > 100) return 0;
  if (data == NULL || width <= 0 || height <= 0) return 0;

  if (2 * radius + 1 > width) radius = (width - 1) >> 1;
  if (2 * radius + 1 > height) radius = (height - 1) >> 1;

  if (radius > 0) {
    SmoothParams p;
    memset(&p, 0, sizeof(p));
    if (!InitParams(data, width, height, stride, radius, &p)) return 0;
    if (p.num_levels > 2) {
      for (; p.row < p.height; ++p.row) {
        VFilter(&p);

        if (p.row >= p.radius) {
          HFilter(&p);
          ApplyFilter(&p);
        }
      }
    }
    CleanupParams(&p);
  }
  return 1;
}

#undef clip_8b

#include <assert.h>
#include <stddef.h>

#ifndef WEBP_UTILS_QUANT_LEVELS_UTILS_H_
#define WEBP_UTILS_QUANT_LEVELS_UTILS_H_

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

int QuantizeLevels(uint8_t* const data, int width, int height, int num_levels,
                   uint64_t* const sse);

#ifdef __cplusplus
}
#endif

#endif

#define NUM_SYMBOLS     256

#define MAX_ITER  6
#define ERROR_THRESHOLD 1e-4

int QuantizeLevels(uint8_t* const data, int width, int height,
                   int num_levels, uint64_t* const sse) {
  int freq[NUM_SYMBOLS] = { 0 };
  int q_level[NUM_SYMBOLS] = { 0 };
  double inv_q_level[NUM_SYMBOLS] = { 0 };
  int min_s = 255, max_s = 0;
  const size_t data_size = height * width;
  int i, num_levels_in, iter;
  double last_err = 1.e38, err = 0.;
  const double err_threshold = ERROR_THRESHOLD * data_size;

  if (data == NULL) {
    return 0;
  }

  if (width <= 0 || height <= 0) {
    return 0;
  }

  if (num_levels < 2 || num_levels > 256) {
    return 0;
  }

  {
    size_t n;
    num_levels_in = 0;
    for (n = 0; n < data_size; ++n) {
      num_levels_in += (freq[data[n]] == 0);
      if (min_s > data[n]) min_s = data[n];
      if (max_s < data[n]) max_s = data[n];
      ++freq[data[n]];
    }
  }

  if (num_levels_in <= num_levels) goto End;

  for (i = 0; i < num_levels; ++i) {
    inv_q_level[i] = min_s + (double)(max_s - min_s) * i / (num_levels - 1);
  }

  q_level[min_s] = 0;
  q_level[max_s] = num_levels - 1;
  assert(inv_q_level[0] == min_s);
  assert(inv_q_level[num_levels - 1] == max_s);

  for (iter = 0; iter < MAX_ITER; ++iter) {
    double q_sum[NUM_SYMBOLS] = { 0 };
    double q_count[NUM_SYMBOLS] = { 0 };
    int s, slot = 0;

    for (s = min_s; s <= max_s; ++s) {

      while (slot < num_levels - 1 &&
             2 * s > inv_q_level[slot] + inv_q_level[slot + 1]) {
        ++slot;
      }
      if (freq[s] > 0) {
        q_sum[slot] += s * freq[s];
        q_count[slot] += freq[s];
      }
      q_level[s] = slot;
    }

    if (num_levels > 2) {
      for (slot = 1; slot < num_levels - 1; ++slot) {
        const double count = q_count[slot];
        if (count > 0.) {
          inv_q_level[slot] = q_sum[slot] / count;
        }
      }
    }

    err = 0.;
    for (s = min_s; s <= max_s; ++s) {
      const double error = s - inv_q_level[q_level[s]];
      err += freq[s] * error * error;
    }

    if (last_err - err < err_threshold) break;
    last_err = err;
  }

  {

    uint8_t map[NUM_SYMBOLS];
    int s;
    size_t n;
    for (s = min_s; s <= max_s; ++s) {
      const int slot = q_level[s];
      map[s] = (uint8_t)(inv_q_level[slot] + .5);
    }

    for (n = 0; n < data_size; ++n) {
      data[n] = map[data[n]];
    }
  }
 End:

  if (sse != NULL) *sse = (uint64_t)err;

  return 1;
}

#include <string.h>

static const uint32_t kRandomTable[VP8_RANDOM_TABLE_SIZE] = {
  0x0de15230, 0x03b31886, 0x775faccb, 0x1c88626a, 0x68385c55, 0x14b3b828,
  0x4a85fef8, 0x49ddb84b, 0x64fcf397, 0x5c550289, 0x4a290000, 0x0d7ec1da,
  0x5940b7ab, 0x5492577d, 0x4e19ca72, 0x38d38c69, 0x0c01ee65, 0x32a1755f,
  0x5437f652, 0x5abb2c32, 0x0faa57b1, 0x73f533e7, 0x685feeda, 0x7563cce2,
  0x6e990e83, 0x4730a7ed, 0x4fc0d9c6, 0x496b153c, 0x4f1403fa, 0x541afb0c,
  0x73990b32, 0x26d7cb1c, 0x6fcc3706, 0x2cbb77d8, 0x75762f2a, 0x6425ccdd,
  0x24b35461, 0x0a7d8715, 0x220414a8, 0x141ebf67, 0x56b41583, 0x73e502e3,
  0x44cab16f, 0x28264d42, 0x73baaefb, 0x0a50ebed, 0x1d6ab6fb, 0x0d3ad40b,
  0x35db3b68, 0x2b081e83, 0x77ce6b95, 0x5181e5f0, 0x78853bbc, 0x009f9494,
  0x27e5ed3c
};

void VP8InitRandom(VP8Random* const rg, float dithering) {
  memcpy(rg->tab, kRandomTable, sizeof(rg->tab));
  rg->index1 = 0;
  rg->index2 = 31;
  rg->amp = (dithering < 0.0) ? 0
          : (dithering > 1.0) ? (1 << VP8_RANDOM_DITHER_FIX)
          : (uint32_t)((1 << VP8_RANDOM_DITHER_FIX) * dithering);
}

#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

int WebPRescalerInit(WebPRescaler* const rescaler,
                     int src_width, int src_height,
                     uint8_t* const dst,
                     int dst_width, int dst_height, int dst_stride,
                     int num_channels, rescaler_t* const work) {
  const int x_add = src_width, x_sub = dst_width;
  const int y_add = src_height, y_sub = dst_height;
  const uint64_t total_size = 2ull * dst_width * num_channels * sizeof(*work);
  if (!CheckSizeOverflow(total_size)) return 0;

  rescaler->x_expand = (src_width < dst_width);
  rescaler->y_expand = (src_height < dst_height);
  rescaler->src_width = src_width;
  rescaler->src_height = src_height;
  rescaler->dst_width = dst_width;
  rescaler->dst_height = dst_height;
  rescaler->src_y = 0;
  rescaler->dst_y = 0;
  rescaler->dst = dst;
  rescaler->dst_stride = dst_stride;
  rescaler->num_channels = num_channels;

  rescaler->x_add = rescaler->x_expand ? (x_sub - 1) : x_add;
  rescaler->x_sub = rescaler->x_expand ? (x_add - 1) : x_sub;
  if (!rescaler->x_expand) {
    rescaler->fx_scale = WEBP_RESCALER_FRAC(1, rescaler->x_sub);
  }

  rescaler->y_add = rescaler->y_expand ? y_add - 1 : y_add;
  rescaler->y_sub = rescaler->y_expand ? y_sub - 1 : y_sub;
  rescaler->y_accum = rescaler->y_expand ? rescaler->y_sub : rescaler->y_add;
  if (!rescaler->y_expand) {

    const uint64_t num = (uint64_t)dst_height * WEBP_RESCALER_ONE;
    const uint64_t den = (uint64_t)rescaler->x_add * rescaler->y_add;
    const uint64_t ratio = num / den;
    if (ratio != (uint32_t)ratio) {

      rescaler->fxy_scale = 0;
    } else {
      rescaler->fxy_scale = (uint32_t)ratio;
    }
    rescaler->fy_scale = WEBP_RESCALER_FRAC(1, rescaler->y_sub);
  } else {
    rescaler->fy_scale = WEBP_RESCALER_FRAC(1, rescaler->x_add);

  }
  rescaler->irow = work;
  rescaler->frow = work + num_channels * dst_width;
  memset(work, 0, (size_t)total_size);

  WebPRescalerDspInit();
  return 1;
}

int WebPRescalerGetScaledDimensions(int src_width, int src_height,
                                    int* const scaled_width,
                                    int* const scaled_height) {
  assert(scaled_width != NULL);
  assert(scaled_height != NULL);
  {
    int width = *scaled_width;
    int height = *scaled_height;
    const int max_size = INT_MAX / 2;

    if (width == 0 && src_height > 0) {
      width =
          (int)(((uint64_t)src_width * height + src_height - 1) / src_height);
    }

    if (height == 0 && src_width > 0) {
      height =
          (int)(((uint64_t)src_height * width + src_width - 1) / src_width);
    }

    if (width <= 0 || height <= 0 || width > max_size || height > max_size) {
      return 0;
    }

    *scaled_width = width;
    *scaled_height = height;
    return 1;
  }
}

int WebPRescaleNeededLines(const WebPRescaler* const rescaler,
                           int max_num_lines) {
  const int num_lines =
      (rescaler->y_accum + rescaler->y_sub - 1) / rescaler->y_sub;
  return (num_lines > max_num_lines) ? max_num_lines : num_lines;
}

int WebPRescalerImport(WebPRescaler* const rescaler, int num_lines,
                       const uint8_t* src, int src_stride) {
  int total_imported = 0;
  while (total_imported < num_lines &&
         !WebPRescalerHasPendingOutput(rescaler)) {
    if (rescaler->y_expand) {
      rescaler_t* const tmp = rescaler->irow;
      rescaler->irow = rescaler->frow;
      rescaler->frow = tmp;
    }
    WebPRescalerImportRow(rescaler, src);
    if (!rescaler->y_expand) {
      int x;
      for (x = 0; x < rescaler->num_channels * rescaler->dst_width; ++x) {
        rescaler->irow[x] += rescaler->frow[x];
      }
    }
    ++rescaler->src_y;
    src += src_stride;
    ++total_imported;
    rescaler->y_accum -= rescaler->y_sub;
  }
  return total_imported;
}

int WebPRescalerExport(WebPRescaler* const rescaler) {
  int total_exported = 0;
  while (WebPRescalerHasPendingOutput(rescaler)) {
    WebPRescalerExportRow(rescaler);
    ++total_exported;
  }
  return total_exported;
}

#include <assert.h>
#include <string.h>

#ifdef WEBP_USE_THREAD

#if defined(_WIN32)

#include <windows.h>
typedef HANDLE pthread_t;
typedef CRITICAL_SECTION pthread_mutex_t;

#if _WIN32_WINNT >= 0x0600
#define USE_WINDOWS_CONDITION_VARIABLE
typedef CONDITION_VARIABLE pthread_cond_t;
#else
typedef struct {
  HANDLE waiting_sem;
  HANDLE received_sem;
  HANDLE signal_event;
} pthread_cond_t;
#endif

#ifndef WINAPI_FAMILY_PARTITION
#define WINAPI_PARTITION_DESKTOP 1
#define WINAPI_FAMILY_PARTITION(x) x
#endif

#if !WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
#define USE_CREATE_THREAD
#endif

#else

#include <pthread.h>

#endif

typedef struct {
  pthread_mutex_t mutex;
  pthread_cond_t  condition;
  pthread_t       thread;
} WebPWorkerImpl;

#if defined(_WIN32)

#include <process.h>

#define THREADFN unsigned int __stdcall
#define THREAD_RETURN(val) (unsigned int)((DWORD_PTR)val)

#if _WIN32_WINNT >= 0x0501
#define WaitForSingleObject(obj, timeout) \
  WaitForSingleObjectEx(obj, timeout, FALSE )
#endif

static int pthread_create(pthread_t* const thread, const void* attr,
                          unsigned int (__stdcall* start)(void*), void* arg) {
  (void)attr;
#ifdef USE_CREATE_THREAD
  *thread = CreateThread(NULL,
                         0,
                         start,
                         arg,
                         0,
                         NULL);
#else
  *thread = (pthread_t)_beginthreadex(NULL,
                                      0,
                                      start,
                                      arg,
                                      0,
                                      NULL);
#endif
  if (*thread == NULL) return 1;
  SetThreadPriority(*thread, THREAD_PRIORITY_ABOVE_NORMAL);
  return 0;
}

static int pthread_join(pthread_t thread, void** value_ptr) {
  (void)value_ptr;
  return (WaitForSingleObject(thread, INFINITE) != WAIT_OBJECT_0 ||
          CloseHandle(thread) == 0);
}

static int pthread_mutex_init(pthread_mutex_t* const mutex, void* mutexattr) {
  (void)mutexattr;
#if _WIN32_WINNT >= 0x0600
  InitializeCriticalSectionEx(mutex, 0 , 0 );
#else
  InitializeCriticalSection(mutex);
#endif
  return 0;
}

static int pthread_mutex_lock(pthread_mutex_t* const mutex) {
  EnterCriticalSection(mutex);
  return 0;
}

static int pthread_mutex_unlock(pthread_mutex_t* const mutex) {
  LeaveCriticalSection(mutex);
  return 0;
}

static int pthread_mutex_destroy(pthread_mutex_t* const mutex) {
  DeleteCriticalSection(mutex);
  return 0;
}

static int pthread_cond_destroy(pthread_cond_t* const condition) {
  int ok = 1;
#ifdef USE_WINDOWS_CONDITION_VARIABLE
  (void)condition;
#else
  ok &= (CloseHandle(condition->waiting_sem) != 0);
  ok &= (CloseHandle(condition->received_sem) != 0);
  ok &= (CloseHandle(condition->signal_event) != 0);
#endif
  return !ok;
}

static int pthread_cond_init(pthread_cond_t* const condition, void* cond_attr) {
  (void)cond_attr;
#ifdef USE_WINDOWS_CONDITION_VARIABLE
  InitializeConditionVariable(condition);
#else
  condition->waiting_sem = CreateSemaphore(NULL, 0, 1, NULL);
  condition->received_sem = CreateSemaphore(NULL, 0, 1, NULL);
  condition->signal_event = CreateEvent(NULL, FALSE, FALSE, NULL);
  if (condition->waiting_sem == NULL ||
      condition->received_sem == NULL ||
      condition->signal_event == NULL) {
    pthread_cond_destroy(condition);
    return 1;
  }
#endif
  return 0;
}

static int pthread_cond_signal(pthread_cond_t* const condition) {
  int ok = 1;
#ifdef USE_WINDOWS_CONDITION_VARIABLE
  WakeConditionVariable(condition);
#else
  if (WaitForSingleObject(condition->waiting_sem, 0) == WAIT_OBJECT_0) {

    ok = SetEvent(condition->signal_event);

    ok &= (WaitForSingleObject(condition->received_sem, INFINITE) !=
           WAIT_OBJECT_0);
  }
#endif
  return !ok;
}

static int pthread_cond_wait(pthread_cond_t* const condition,
                             pthread_mutex_t* const mutex) {
  int ok;
#ifdef USE_WINDOWS_CONDITION_VARIABLE
  ok = SleepConditionVariableCS(condition, mutex, INFINITE);
#else

  if (!ReleaseSemaphore(condition->waiting_sem, 1, NULL)) return 1;

  pthread_mutex_unlock(mutex);
  ok = (WaitForSingleObject(condition->signal_event, INFINITE) ==
        WAIT_OBJECT_0);
  ok &= ReleaseSemaphore(condition->received_sem, 1, NULL);
  pthread_mutex_lock(mutex);
#endif
  return !ok;
}

#else
# define THREADFN void*
# define THREAD_RETURN(val) val
#endif

static THREADFN ThreadLoop(void* ptr) {
  WebPWorker* const worker = (WebPWorker*)ptr;
  WebPWorkerImpl* const impl = (WebPWorkerImpl*)worker->impl;
  int done = 0;
  while (!done) {
    pthread_mutex_lock(&impl->mutex);
    while (worker->status == OK) {
      pthread_cond_wait(&impl->condition, &impl->mutex);
    }
    if (worker->status == WORK) {
      WebPGetWorkerInterface()->Execute(worker);
      worker->status = OK;
    } else if (worker->status == NOT_OK) {
      done = 1;
    }

    pthread_mutex_unlock(&impl->mutex);
    pthread_cond_signal(&impl->condition);
  }
  return THREAD_RETURN(NULL);
}

static void ChangeState(WebPWorker* const worker, WebPWorkerStatus new_status) {

  WebPWorkerImpl* const impl = (WebPWorkerImpl*)worker->impl;
  if (impl == NULL) return;

  pthread_mutex_lock(&impl->mutex);
  if (worker->status >= OK) {

    while (worker->status != OK) {
      pthread_cond_wait(&impl->condition, &impl->mutex);
    }

    if (new_status != OK) {
      worker->status = new_status;

      pthread_mutex_unlock(&impl->mutex);
      pthread_cond_signal(&impl->condition);
      return;
    }
  }
  pthread_mutex_unlock(&impl->mutex);
}

#endif

static void Init(WebPWorker* const worker) {
  memset(worker, 0, sizeof(*worker));
  worker->status = NOT_OK;
}

static int Sync(WebPWorker* const worker) {
#ifdef WEBP_USE_THREAD
  ChangeState(worker, OK);
#endif
  assert(worker->status <= OK);
  return !worker->had_error;
}

static int Reset(WebPWorker* const worker) {
  int ok = 1;
  worker->had_error = 0;
  if (worker->status < OK) {
#ifdef WEBP_USE_THREAD
    WebPWorkerImpl* const impl =
        (WebPWorkerImpl*)WebPSafeCalloc(1, sizeof(WebPWorkerImpl));
    worker->impl = (void*)impl;
    if (worker->impl == NULL) {
      return 0;
    }
    if (pthread_mutex_init(&impl->mutex, NULL)) {
      goto Error;
    }
    if (pthread_cond_init(&impl->condition, NULL)) {
      pthread_mutex_destroy(&impl->mutex);
      goto Error;
    }
    pthread_mutex_lock(&impl->mutex);
    ok = !pthread_create(&impl->thread, NULL, ThreadLoop, worker);
    if (ok) worker->status = OK;
    pthread_mutex_unlock(&impl->mutex);
    if (!ok) {
      pthread_mutex_destroy(&impl->mutex);
      pthread_cond_destroy(&impl->condition);
 Error:
      WebPSafeFree(impl);
      worker->impl = NULL;
      return 0;
    }
#else
    worker->status = OK;
#endif
  } else if (worker->status > OK) {
    ok = Sync(worker);
  }
  assert(!ok || (worker->status == OK));
  return ok;
}

static void Execute(WebPWorker* const worker) {
  if (worker->hook != NULL) {
    worker->had_error |= !worker->hook(worker->data1, worker->data2);
  }
}

static void Launch(WebPWorker* const worker) {
#ifdef WEBP_USE_THREAD
  ChangeState(worker, WORK);
#else
  Execute(worker);
#endif
}

static void End(WebPWorker* const worker) {
#ifdef WEBP_USE_THREAD
  if (worker->impl != NULL) {
    WebPWorkerImpl* const impl = (WebPWorkerImpl*)worker->impl;
    ChangeState(worker, NOT_OK);
    pthread_join(impl->thread, NULL);
    pthread_mutex_destroy(&impl->mutex);
    pthread_cond_destroy(&impl->condition);
    WebPSafeFree(impl);
    worker->impl = NULL;
  }
#else
  worker->status = NOT_OK;
  assert(worker->impl == NULL);
#endif
  assert(worker->status == NOT_OK);
}

static WebPWorkerInterface g_worker_interface = {
  Init, Reset, Sync, Launch, Execute, End
};

int WebPSetWorkerInterface(const WebPWorkerInterface* const winterface) {
  if (winterface == NULL ||
      winterface->Init == NULL || winterface->Reset == NULL ||
      winterface->Sync == NULL || winterface->Launch == NULL ||
      winterface->Execute == NULL || winterface->End == NULL) {
    return 0;
  }
  g_worker_interface = *winterface;
  return 1;
}

const WebPWorkerInterface* WebPGetWorkerInterface(void) {
  return &g_worker_interface;
}

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#if defined(PRINT_MEM_INFO)

#include <stdio.h>

static int num_malloc_calls = 0;
static int num_calloc_calls = 0;
static int num_free_calls = 0;
static int countdown_to_fail = 0;

typedef struct MemBlock MemBlock;
struct MemBlock {
  void* ptr;
  size_t size;
  MemBlock* next;
};

static MemBlock* all_blocks = NULL;
static size_t total_mem = 0;
static size_t total_mem_allocated = 0;
static size_t high_water_mark = 0;
static size_t mem_limit = 0;

static int exit_registered = 0;

static void PrintMemInfo(void) {
  fprintf(stderr, "\nMEMORY INFO:\n");
  fprintf(stderr, "num calls to: malloc = %4d\n", num_malloc_calls);
  fprintf(stderr, "              calloc = %4d\n", num_calloc_calls);
  fprintf(stderr, "              free   = %4d\n", num_free_calls);
  fprintf(stderr, "total_mem: %u\n", (uint32_t)total_mem);
  fprintf(stderr, "total_mem allocated: %u\n", (uint32_t)total_mem_allocated);
  fprintf(stderr, "high-water mark: %u\n", (uint32_t)high_water_mark);
  while (all_blocks != NULL) {
    MemBlock* b = all_blocks;
    all_blocks = b->next;
    free(b);
  }
}

static void Increment(int* const v) {
  if (!exit_registered) {
#if defined(MALLOC_FAIL_AT)
    {
      const char* const malloc_fail_at_str = getenv("MALLOC_FAIL_AT");
      if (malloc_fail_at_str != NULL) {
        countdown_to_fail = atoi(malloc_fail_at_str);
      }
    }
#endif
#if defined(MALLOC_LIMIT)
    {
      const char* const malloc_limit_str = getenv("MALLOC_LIMIT");
#if MALLOC_LIMIT > 1
      mem_limit = (size_t)MALLOC_LIMIT;
#endif
      if (malloc_limit_str != NULL) {
        mem_limit = atoi(malloc_limit_str);
      }
    }
#endif
    (void)countdown_to_fail;
    (void)mem_limit;
    atexit(PrintMemInfo);
    exit_registered = 1;
  }
  ++*v;
}

static void AddMem(void* ptr, size_t size) {
  if (ptr != NULL) {
    MemBlock* const b = (MemBlock*)malloc(sizeof(*b));
    if (b == NULL) abort();
    b->next = all_blocks;
    all_blocks = b;
    b->ptr = ptr;
    b->size = size;
    total_mem += size;
    total_mem_allocated += size;
#if defined(PRINT_MEM_TRAFFIC)
#if defined(MALLOC_FAIL_AT)
    fprintf(stderr, "fail-count: %5d [mem=%u]\n",
            num_malloc_calls + num_calloc_calls, (uint32_t)total_mem);
#else
    fprintf(stderr, "Mem: %u (+%u)\n", (uint32_t)total_mem, (uint32_t)size);
#endif
#endif
    if (total_mem > high_water_mark) high_water_mark = total_mem;
  }
}

static void SubMem(void* ptr) {
  if (ptr != NULL) {
    MemBlock** b = &all_blocks;

    while (*b != NULL && (*b)->ptr != ptr) b = &(*b)->next;
    if (*b == NULL) {
      fprintf(stderr, "Invalid pointer free! (%p)\n", ptr);
      abort();
    }
    {
      MemBlock* const block = *b;
      *b = block->next;
      total_mem -= block->size;
#if defined(PRINT_MEM_TRAFFIC)
      fprintf(stderr, "Mem: %u (-%u)\n",
              (uint32_t)total_mem, (uint32_t)block->size);
#endif
      free(block);
    }
  }
}

#else
#define Increment(v) do {} while (0)
#define AddMem(p, s) do {} while (0)
#define SubMem(p)    do {} while (0)
#endif

static int CheckSizeArgumentsOverflow(uint64_t nmemb, size_t size) {
  const uint64_t total_size = nmemb * size;
  if (nmemb == 0) return 1;
  if ((uint64_t)size > WEBP_MAX_ALLOCABLE_MEMORY / nmemb) return 0;
  if (!CheckSizeOverflow(total_size)) return 0;
#if defined(PRINT_MEM_INFO) && defined(MALLOC_FAIL_AT)
  if (countdown_to_fail > 0 && --countdown_to_fail == 0) {
    return 0;
  }
#endif
#if defined(PRINT_MEM_INFO) && defined(MALLOC_LIMIT)
  if (mem_limit > 0) {
    const uint64_t new_total_mem = (uint64_t)total_mem + total_size;
    if (!CheckSizeOverflow(new_total_mem) ||
        new_total_mem > mem_limit) {
      return 0;
    }
  }
#endif

  return 1;
}

void* WebPSafeMalloc(uint64_t nmemb, size_t size) {
  void* ptr;
  Increment(&num_malloc_calls);
  if (!CheckSizeArgumentsOverflow(nmemb, size)) return NULL;
  assert(nmemb * size > 0);
  ptr = malloc((size_t)(nmemb * size));
  AddMem(ptr, (size_t)(nmemb * size));
  return ptr;
}

void* WebPSafeCalloc(uint64_t nmemb, size_t size) {
  void* ptr;
  Increment(&num_calloc_calls);
  if (!CheckSizeArgumentsOverflow(nmemb, size)) return NULL;
  assert(nmemb * size > 0);
  ptr = calloc((size_t)nmemb, size);
  AddMem(ptr, (size_t)(nmemb * size));
  return ptr;
}

void WebPSafeFree(void* const ptr) {
  if (ptr != NULL) {
    Increment(&num_free_calls);
    SubMem(ptr);
  }
  free(ptr);
}

void* WebPMalloc(size_t size) {
  return WebPSafeMalloc(1, size);
}

void WebPFree(void* ptr) {
  WebPSafeFree(ptr);
}

void WebPCopyPlane(const uint8_t* src, int src_stride,
                   uint8_t* dst, int dst_stride, int width, int height) {
  assert(src != NULL && dst != NULL);
  assert(abs(src_stride) >= width && abs(dst_stride) >= width);
  while (height-- > 0) {
    memcpy(dst, src, width);
    src += src_stride;
    dst += dst_stride;
  }
}

void WebPCopyPixels(const WebPPicture* const src, WebPPicture* const dst) {
  assert(src != NULL && dst != NULL);
  assert(src->width == dst->width && src->height == dst->height);
  assert(src->use_argb && dst->use_argb);
  WebPCopyPlane((uint8_t*)src->argb, 4 * src->argb_stride, (uint8_t*)dst->argb,
                4 * dst->argb_stride, 4 * src->width, src->height);
}

int WebPGetColorPalette(const WebPPicture* const pic, uint32_t* const palette) {
  return GetColorPalette(pic, palette);
}

#if defined(WEBP_NEED_LOG_TABLE_8BIT)
const uint8_t WebPLogTable8bit[256] = {
  0, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3,
  4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
  6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
  6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
  6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
  7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
  7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
  7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
  7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
  7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
  7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
  7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
  7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7
};
#endif
