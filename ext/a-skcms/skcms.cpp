#include "skcms.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ARRAY_COUNT(arr) (int)(sizeof((arr)) / sizeof(*(arr)))

#if defined(__clang__) && defined(__has_cpp_attribute)
    #if __has_cpp_attribute(clang::fallthrough)
        #define SKCMS_FALLTHROUGH [[clang::fallthrough]]
    #endif

    #ifndef SKCMS_HAS_MUSTTAIL

        #if __has_cpp_attribute(clang::musttail) && !__has_feature(memory_sanitizer) \
                                                 && !__has_feature(address_sanitizer) \
                                                 && !defined(__EMSCRIPTEN__) \
                                                 && !defined(__arm__) \
                                                 && !defined(__riscv) \
                                                 && !defined(__powerpc__) \
                                                 && !defined(__loongarch__) \
                                                 && !defined(_WIN32) && !defined(__SYMBIAN32__)
            #define SKCMS_HAS_MUSTTAIL 1
        #endif
    #endif
#endif

#ifndef SKCMS_FALLTHROUGH
    #define SKCMS_FALLTHROUGH
#endif
#ifndef SKCMS_HAS_MUSTTAIL
    #define SKCMS_HAS_MUSTTAIL 0
#endif

#if defined(__clang__)
    #define SKCMS_MAYBE_UNUSED __attribute__((unused))
    #pragma clang diagnostic ignored "-Wused-but-marked-unused"
#elif defined(__GNUC__)
    #define SKCMS_MAYBE_UNUSED __attribute__((unused))
#elif defined(_MSC_VER)
    #define SKCMS_MAYBE_UNUSED __pragma(warning(suppress:4100))
#else
    #define SKCMS_MAYBE_UNUSED
#endif

#define SAFE_SIZEOF(x) ((uint64_t)sizeof(x))

#define SAFE_FIXED_SIZE(type) ((uint64_t)offsetof(type, variable))

#if !defined(SKCMS_PORTABLE) && !(defined(__clang__) || \
                                  defined(__GNUC__) || \
                                  (defined(__EMSCRIPTEN__) && defined(__wasm_simd128__)))
    #define SKCMS_PORTABLE 1
#endif

#if defined(SKCMS_PORTABLE) || !defined(__x86_64__) || defined(ANDROID) || defined(__ANDROID__)
    #undef SKCMS_FORCE_HSW
    #if !defined(SKCMS_DISABLE_HSW)
        #define SKCMS_DISABLE_HSW 1
    #endif

    #undef SKCMS_FORCE_SKX
    #if !defined(SKCMS_DISABLE_SKX)
        #define SKCMS_DISABLE_SKX 1
    #endif
#endif

typedef struct skcms_ICCTag {
    uint32_t       signature;
    uint32_t       type;
    uint32_t       size;
    const uint8_t* buf;
} skcms_ICCTag;

typedef struct skcms_ICCProfile skcms_ICCProfile;
typedef struct skcms_TransferFunction skcms_TransferFunction;
typedef union skcms_Curve skcms_Curve;

void skcms_GetTagByIndex    (const skcms_ICCProfile*, uint32_t idx, skcms_ICCTag*);
bool skcms_GetTagBySignature(const skcms_ICCProfile*, uint32_t sig, skcms_ICCTag*);

float skcms_MaxRoundtripError(const skcms_Curve* curve, const skcms_TransferFunction* inv_tf);

extern const uint8_t skcms_252_random_bytes[252];

static inline float floorf_(float x) {
    float roundtrip = (float)((int)x);
    return roundtrip > x ? roundtrip - 1 : roundtrip;
}
static inline float fabsf_(float x) { return x < 0 ? -x : x; }
float powf_(float, float);

#ifdef __cplusplus
}
#endif

#include <stddef.h>
#include <stdint.h>

namespace skcms_private {

#define SKCMS_WORK_OPS(M) \
    M(load_a8)            \
    M(load_g8)            \
    M(load_ga88)          \
    M(load_4444)          \
    M(load_565)           \
    M(load_888)           \
    M(load_8888)          \
    M(load_1010102)       \
    M(load_101010x_XR)    \
    M(load_10101010_XR)   \
    M(load_161616LE)      \
    M(load_16161616LE)    \
    M(load_161616BE)      \
    M(load_16161616BE)    \
    M(load_hhh)           \
    M(load_hhhh)          \
    M(load_fff)           \
    M(load_ffff)          \
                          \
    M(swap_rb)            \
    M(clamp)              \
    M(invert)             \
    M(force_opaque)       \
    M(premul)             \
    M(unpremul)           \
    M(matrix_3x3)         \
    M(matrix_3x4)         \
                          \
    M(lab_to_xyz)         \
    M(xyz_to_lab)         \
                          \
    M(gamma_r)            \
    M(gamma_g)            \
    M(gamma_b)            \
    M(gamma_a)            \
    M(gamma_rgb)          \
                          \
    M(tf_r)               \
    M(tf_g)               \
    M(tf_b)               \
    M(tf_a)               \
    M(tf_rgb)             \
                          \
    M(pq_r)               \
    M(pq_g)               \
    M(pq_b)               \
    M(pq_a)               \
    M(pq_rgb)             \
                          \
    M(hlg_r)              \
    M(hlg_g)              \
    M(hlg_b)              \
    M(hlg_a)              \
    M(hlg_rgb)            \
                          \
    M(hlginv_r)           \
    M(hlginv_g)           \
    M(hlginv_b)           \
    M(hlginv_a)           \
    M(hlginv_rgb)         \
                          \
    M(table_r)            \
    M(table_g)            \
    M(table_b)            \
    M(table_a)            \
                          \
    M(clut_A2B)           \
    M(clut_B2A)

#define SKCMS_STORE_OPS(M) \
    M(store_a8)            \
    M(store_g8)            \
    M(store_ga88)          \
    M(store_4444)          \
    M(store_565)           \
    M(store_888)           \
    M(store_8888)          \
    M(store_1010102)       \
    M(store_161616LE)      \
    M(store_16161616LE)    \
    M(store_161616BE)      \
    M(store_16161616BE)    \
    M(store_101010x_XR)    \
    M(store_hhh)           \
    M(store_hhhh)          \
    M(store_fff)           \
    M(store_ffff)

enum class Op : int {
#define M(op) op,
    SKCMS_WORK_OPS(M)
    SKCMS_STORE_OPS(M)
#undef M
};

#if defined(__clang__) || defined(__GNUC__)
    static constexpr float INFINITY_ = __builtin_inff();
#else
    static const union {
        uint32_t bits;
        float    f;
    } inf_ = { 0x7f800000 };
    #define INFINITY_ inf_.f
#endif

#if defined(__clang__)
    template <int N, typename T> using Vec = T __attribute__((ext_vector_type(N)));
#elif defined(__GNUC__)

    template <int N, typename T> struct VecHelper {
        typedef T __attribute__((vector_size(N * sizeof(T)))) V;
    };
    template <int N, typename T> using Vec = typename VecHelper<N, T>::V;
#endif

namespace baseline {

void run_program(const Op* program, const void** contexts, ptrdiff_t programSize,
                 const char* src, char* dst, int n,
                 const size_t src_bpp, const size_t dst_bpp);

}
namespace hsw {

void run_program(const Op* program, const void** contexts, ptrdiff_t programSize,
                 const char* src, char* dst, int n,
                 const size_t src_bpp, const size_t dst_bpp);

}
namespace skx {

void run_program(const Op* program, const void** contexts, ptrdiff_t programSize,
                 const char* src, char* dst, int n,
                 const size_t src_bpp, const size_t dst_bpp);

}
}

#include <assert.h>
#include <float.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#if defined(__ARM_NEON)
    #include <arm_neon.h>
#elif defined(__SSE__)
    #include <immintrin.h>

    #if defined(__clang__)

        #include <smmintrin.h>
        #include <avxintrin.h>
        #include <avx2intrin.h>
        #include <avx512fintrin.h>
        #include <avx512dqintrin.h>
    #endif
#endif

using namespace skcms_private;

static bool sAllowRuntimeCPUDetection = true;

void skcms_DisableRuntimeCPUDetection() {
    sAllowRuntimeCPUDetection = false;
}

static float log2f_(float x) {

    int32_t bits;
    memcpy(&bits, &x, sizeof(bits));

    float e = (float)bits * (1.0f / (1<<23));

    int32_t m_bits = (bits & 0x007fffff) | 0x3f000000;
    float m;
    memcpy(&m, &m_bits, sizeof(m));

    return (e - 124.225514990f
              -   1.498030302f*m
              -   1.725879990f/(0.3520887068f + m));
}
static float logf_(float x) {
    const float ln2 = 0.69314718f;
    return ln2*log2f_(x);
}

static float exp2f_(float x) {
    if (x > 128.0f) {
        return INFINITY_;
    } else if (x < -127.0f) {
        return 0.0f;
    }
    float fract = x - floorf_(x);

    float fbits = (1.0f * (1<<23)) * (x + 121.274057500f
                                        -   1.490129070f*fract
                                        +  27.728023300f/(4.84252568f - fract));

    if (fbits >= (float)INT_MAX) {
        return INFINITY_;
    } else if (fbits < 0) {
        return 0;
    }

    int32_t bits = (int32_t)fbits;
    memcpy(&x, &bits, sizeof(x));
    return x;
}

float powf_(float x, float y) {
    if (x <= 0.f) {
        return 0.f;
    }
    if (x == 1.f) {
        return 1.f;
    }
    return exp2f_(log2f_(x) * y);
}

static float expf_(float x) {
    const float log2_e = 1.4426950408889634074f;
    return exp2f_(log2_e * x);
}

static float fmaxf_(float x, float y) { return x > y ? x : y; }
static float fminf_(float x, float y) { return x < y ? x : y; }

static bool isfinitef_(float x) { return 0 == x*0; }

static float minus_1_ulp(float x) {
    int32_t bits;
    memcpy(&bits, &x, sizeof(bits));
    bits = bits - 1;
    memcpy(&x, &bits, sizeof(bits));
    return x;
}

struct TF_PQish  { float A,B,C,D,E,F; };
struct TF_HLGish { float R,G,a,b,c,K_minus_1; };

static float TFKind_marker(skcms_TFType kind) {

    return -(float)kind;
}

static skcms_TFType classify(const skcms_TransferFunction& tf, TF_PQish*   pq = nullptr
                                                             , TF_HLGish* hlg = nullptr) {
    if (tf.g < 0) {

        if (tf.g < -128) {
            return skcms_TFType_Invalid;
        }
        int enum_g = -static_cast<int>(tf.g);

        if (static_cast<float>(-enum_g) != tf.g) {
            return skcms_TFType_Invalid;
        }

        switch (enum_g) {
            case skcms_TFType_PQish:
                if (pq) {
                    memcpy(pq , &tf.a, sizeof(*pq ));
                }
                return skcms_TFType_PQish;
            case skcms_TFType_HLGish:
                if (hlg) {
                    memcpy(hlg, &tf.a, sizeof(*hlg));
                }
                return skcms_TFType_HLGish;
            case skcms_TFType_HLGinvish:
                if (hlg) {
                    memcpy(hlg, &tf.a, sizeof(*hlg));
                }
                return skcms_TFType_HLGinvish;
        }
        return skcms_TFType_Invalid;
    }

    if (isfinitef_(tf.a + tf.b + tf.c + tf.d + tf.e + tf.f + tf.g)

            && tf.a >= 0
            && tf.c >= 0
            && tf.d >= 0
            && tf.g >= 0

            && tf.a * tf.d + tf.b >= 0) {
        return skcms_TFType_sRGBish;
    }

    return skcms_TFType_Invalid;
}

skcms_TFType skcms_TransferFunction_getType(const skcms_TransferFunction* tf) {
    return classify(*tf);
}
bool skcms_TransferFunction_isSRGBish(const skcms_TransferFunction* tf) {
    return classify(*tf) == skcms_TFType_sRGBish;
}
bool skcms_TransferFunction_isPQish(const skcms_TransferFunction* tf) {
    return classify(*tf) == skcms_TFType_PQish;
}
bool skcms_TransferFunction_isHLGish(const skcms_TransferFunction* tf) {
    return classify(*tf) == skcms_TFType_HLGish;
}

bool skcms_TransferFunction_makePQish(skcms_TransferFunction* tf,
                                      float A, float B, float C,
                                      float D, float E, float F) {
    *tf = { TFKind_marker(skcms_TFType_PQish), A,B,C,D,E,F };
    assert(skcms_TransferFunction_isPQish(tf));
    return true;
}

bool skcms_TransferFunction_makeScaledHLGish(skcms_TransferFunction* tf,
                                             float K, float R, float G,
                                             float a, float b, float c) {
    *tf = { TFKind_marker(skcms_TFType_HLGish), R,G, a,b,c, K-1.0f };
    assert(skcms_TransferFunction_isHLGish(tf));
    return true;
}

float skcms_TransferFunction_eval(const skcms_TransferFunction* tf, float x) {
    float sign = x < 0 ? -1.0f : 1.0f;
    x *= sign;

    TF_PQish  pq;
    TF_HLGish hlg;
    switch (classify(*tf, &pq, &hlg)) {
        case skcms_TFType_Invalid: break;

        case skcms_TFType_HLGish: {
            const float K = hlg.K_minus_1 + 1.0f;
            return K * sign * (x*hlg.R <= 1 ? powf_(x*hlg.R, hlg.G)
                                            : expf_((x-hlg.c)*hlg.a) + hlg.b);
        }

        case skcms_TFType_HLGinvish: {
            const float K = hlg.K_minus_1 + 1.0f;
            x /= K;
            return sign * (x <= 1 ? hlg.R * powf_(x, hlg.G)
                                  : hlg.a * logf_(x - hlg.b) + hlg.c);
        }

        case skcms_TFType_sRGBish:
            return sign * (x < tf->d ?       tf->c * x + tf->f
                                     : powf_(tf->a * x + tf->b, tf->g) + tf->e);

        case skcms_TFType_PQish:
            return sign *
                   powf_((pq.A + pq.B * powf_(x, pq.C)) / (pq.D + pq.E * powf_(x, pq.C)), pq.F);
    }
    return 0;
}

static float eval_curve(const skcms_Curve* curve, float x) {
    if (curve->table_entries == 0) {
        return skcms_TransferFunction_eval(&curve->parametric, x);
    }

    float ix = fmaxf_(0, fminf_(x, 1)) * static_cast<float>(curve->table_entries - 1);
    int   lo = (int)                   ix        ,
          hi = (int)(float)minus_1_ulp(ix + 1.0f);
    float t = ix - (float)lo;

    float l, h;
    if (curve->table_8) {
        l = curve->table_8[lo] * (1/255.0f);
        h = curve->table_8[hi] * (1/255.0f);
    } else {
        uint16_t be_l, be_h;
        memcpy(&be_l, curve->table_16 + 2*lo, 2);
        memcpy(&be_h, curve->table_16 + 2*hi, 2);
        uint16_t le_l = ((be_l << 8) | (be_l >> 8)) & 0xffff;
        uint16_t le_h = ((be_h << 8) | (be_h >> 8)) & 0xffff;
        l = le_l * (1/65535.0f);
        h = le_h * (1/65535.0f);
    }
    return l + (h-l)*t;
}

float skcms_MaxRoundtripError(const skcms_Curve* curve, const skcms_TransferFunction* inv_tf) {
    uint32_t N = curve->table_entries > 256 ? curve->table_entries : 256;
    const float dx = 1.0f / static_cast<float>(N - 1);
    float err = 0;
    for (uint32_t i = 0; i < N; i++) {
        float x = static_cast<float>(i) * dx,
              y = eval_curve(curve, x);
        err = fmaxf_(err, fabsf_(x - skcms_TransferFunction_eval(inv_tf, y)));
    }
    return err;
}

bool skcms_AreApproximateInverses(const skcms_Curve* curve, const skcms_TransferFunction* inv_tf) {
    return skcms_MaxRoundtripError(curve, inv_tf) < (1/512.0f);
}

enum {

    skcms_Signature_acsp = 0x61637370,

    skcms_Signature_rTRC = 0x72545243,
    skcms_Signature_gTRC = 0x67545243,
    skcms_Signature_bTRC = 0x62545243,
    skcms_Signature_kTRC = 0x6B545243,

    skcms_Signature_rXYZ = 0x7258595A,
    skcms_Signature_gXYZ = 0x6758595A,
    skcms_Signature_bXYZ = 0x6258595A,

    skcms_Signature_A2B0 = 0x41324230,
    skcms_Signature_B2A0 = 0x42324130,

    skcms_Signature_CHAD = 0x63686164,
    skcms_Signature_WTPT = 0x77747074,

    skcms_Signature_CICP = 0x63696370,

    skcms_Signature_curv = 0x63757276,
    skcms_Signature_mft1 = 0x6D667431,
    skcms_Signature_mft2 = 0x6D667432,
    skcms_Signature_mAB  = 0x6D414220,
    skcms_Signature_mBA  = 0x6D424120,
    skcms_Signature_para = 0x70617261,
    skcms_Signature_sf32 = 0x73663332,

};

static uint16_t read_big_u16(const uint8_t* ptr) {
    uint16_t be;
    memcpy(&be, ptr, sizeof(be));
#if defined(_MSC_VER)
    return _byteswap_ushort(be);
#else
    return __builtin_bswap16(be);
#endif
}

static uint32_t read_big_u32(const uint8_t* ptr) {
    uint32_t be;
    memcpy(&be, ptr, sizeof(be));
#if defined(_MSC_VER)
    return _byteswap_ulong(be);
#else
    return __builtin_bswap32(be);
#endif
}

static int32_t read_big_i32(const uint8_t* ptr) {
    return (int32_t)read_big_u32(ptr);
}

static float read_big_fixed(const uint8_t* ptr) {
    return static_cast<float>(read_big_i32(ptr)) * (1.0f / 65536.0f);
}

typedef struct {
    uint8_t size                [ 4];
    uint8_t cmm_type            [ 4];
    uint8_t version             [ 4];
    uint8_t profile_class       [ 4];
    uint8_t data_color_space    [ 4];
    uint8_t pcs                 [ 4];
    uint8_t creation_date_time  [12];
    uint8_t signature           [ 4];
    uint8_t platform            [ 4];
    uint8_t flags               [ 4];
    uint8_t device_manufacturer [ 4];
    uint8_t device_model        [ 4];
    uint8_t device_attributes   [ 8];
    uint8_t rendering_intent    [ 4];
    uint8_t illuminant_X        [ 4];
    uint8_t illuminant_Y        [ 4];
    uint8_t illuminant_Z        [ 4];
    uint8_t creator             [ 4];
    uint8_t profile_id          [16];
    uint8_t reserved            [28];
    uint8_t tag_count           [ 4];
} header_Layout;

typedef struct {
    uint8_t signature [4];
    uint8_t offset    [4];
    uint8_t size      [4];
} tag_Layout;

static const tag_Layout* get_tag_table(const skcms_ICCProfile* profile) {
    return (const tag_Layout*)(profile->buffer + SAFE_SIZEOF(header_Layout));
}

typedef struct {
    uint8_t type     [ 4];
    uint8_t reserved [ 4];
    uint8_t values   [36];
} sf32_Layout;

bool skcms_GetCHAD(const skcms_ICCProfile* profile, skcms_Matrix3x3* m) {
    skcms_ICCTag tag;
    if (!skcms_GetTagBySignature(profile, skcms_Signature_CHAD, &tag)) {
        return false;
    }

    if (tag.type != skcms_Signature_sf32 || tag.size < SAFE_SIZEOF(sf32_Layout)) {
        return false;
    }

    const sf32_Layout* sf32Tag = (const sf32_Layout*)tag.buf;
    const uint8_t* values = sf32Tag->values;
    for (int r = 0; r < 3; ++r)
    for (int c = 0; c < 3; ++c, values += 4) {
        m->vals[r][c] = read_big_fixed(values);
    }
    return true;
}

typedef struct {
    uint8_t type     [4];
    uint8_t reserved [4];
    uint8_t X        [4];
    uint8_t Y        [4];
    uint8_t Z        [4];
} XYZ_Layout;

static bool read_tag_xyz(const skcms_ICCTag* tag, float* x, float* y, float* z) {
    if (tag->type != skcms_Signature_XYZ || tag->size < SAFE_SIZEOF(XYZ_Layout)) {
        return false;
    }

    const XYZ_Layout* xyzTag = (const XYZ_Layout*)tag->buf;

    *x = read_big_fixed(xyzTag->X);
    *y = read_big_fixed(xyzTag->Y);
    *z = read_big_fixed(xyzTag->Z);
    return true;
}

bool skcms_GetWTPT(const skcms_ICCProfile* profile, float xyz[3]) {
    skcms_ICCTag tag;
    return skcms_GetTagBySignature(profile, skcms_Signature_WTPT, &tag) &&
           read_tag_xyz(&tag, &xyz[0], &xyz[1], &xyz[2]);
}

static int data_color_space_channel_count(uint32_t data_color_space) {
    switch (data_color_space) {
        case skcms_Signature_CMYK:   return 4;
        case skcms_Signature_Gray:   return 1;
        case skcms_Signature_RGB:    return 3;
        case skcms_Signature_Lab:    return 3;
        case skcms_Signature_XYZ:    return 3;
        case skcms_Signature_CIELUV: return 3;
        case skcms_Signature_YCbCr:  return 3;
        case skcms_Signature_CIEYxy: return 3;
        case skcms_Signature_HSV:    return 3;
        case skcms_Signature_HLS:    return 3;
        case skcms_Signature_CMY:    return 3;
        case skcms_Signature_2CLR:   return 2;
        case skcms_Signature_3CLR:   return 3;
        case skcms_Signature_4CLR:   return 4;
        case skcms_Signature_5CLR:   return 5;
        case skcms_Signature_6CLR:   return 6;
        case skcms_Signature_7CLR:   return 7;
        case skcms_Signature_8CLR:   return 8;
        case skcms_Signature_9CLR:   return 9;
        case skcms_Signature_10CLR:  return 10;
        case skcms_Signature_11CLR:  return 11;
        case skcms_Signature_12CLR:  return 12;
        case skcms_Signature_13CLR:  return 13;
        case skcms_Signature_14CLR:  return 14;
        case skcms_Signature_15CLR:  return 15;
        default:                     return -1;
    }
}

int skcms_GetInputChannelCount(const skcms_ICCProfile* profile) {
    int a2b_count = 0;
    if (profile->has_A2B) {
        a2b_count = profile->A2B.input_channels != 0
                        ? static_cast<int>(profile->A2B.input_channels)
                        : 3;
    }

    skcms_ICCTag tag;
    int trc_count = 0;
    if (skcms_GetTagBySignature(profile, skcms_Signature_kTRC, &tag)) {
        trc_count = 1;
    } else if (profile->has_trc) {
        trc_count = 3;
    }

    int dcs_count = data_color_space_channel_count(profile->data_color_space);

    if (dcs_count < 0) {
        return -1;
    }

    if (a2b_count > 0 && a2b_count != dcs_count) {
        return -1;
    }
    if (trc_count > 0 && trc_count != dcs_count) {
        return -1;
    }

    return dcs_count;
}

static bool read_to_XYZD50(const skcms_ICCTag* rXYZ, const skcms_ICCTag* gXYZ,
                           const skcms_ICCTag* bXYZ, skcms_Matrix3x3* toXYZ) {
    return read_tag_xyz(rXYZ, &toXYZ->vals[0][0], &toXYZ->vals[1][0], &toXYZ->vals[2][0]) &&
           read_tag_xyz(gXYZ, &toXYZ->vals[0][1], &toXYZ->vals[1][1], &toXYZ->vals[2][1]) &&
           read_tag_xyz(bXYZ, &toXYZ->vals[0][2], &toXYZ->vals[1][2], &toXYZ->vals[2][2]);
}

typedef struct {
    uint8_t type          [4];
    uint8_t reserved_a    [4];
    uint8_t function_type [2];
    uint8_t reserved_b    [2];
    uint8_t variable      [1];
} para_Layout;

static bool read_curve_para(const uint8_t* buf, uint32_t size,
                            skcms_Curve* curve, uint32_t* curve_size) {
    if (size < SAFE_FIXED_SIZE(para_Layout)) {
        return false;
    }

    const para_Layout* paraTag = (const para_Layout*)buf;

    enum { kG = 0, kGAB = 1, kGABC = 2, kGABCD = 3, kGABCDEF = 4 };
    uint16_t function_type = read_big_u16(paraTag->function_type);
    if (function_type > kGABCDEF) {
        return false;
    }

    static const uint32_t curve_bytes[] = { 4, 12, 16, 20, 28 };
    if (size < SAFE_FIXED_SIZE(para_Layout) + curve_bytes[function_type]) {
        return false;
    }

    if (curve_size) {
        *curve_size = SAFE_FIXED_SIZE(para_Layout) + curve_bytes[function_type];
    }

    curve->table_entries = 0;
    curve->parametric.a  = 1.0f;
    curve->parametric.b  = 0.0f;
    curve->parametric.c  = 0.0f;
    curve->parametric.d  = 0.0f;
    curve->parametric.e  = 0.0f;
    curve->parametric.f  = 0.0f;
    curve->parametric.g  = read_big_fixed(paraTag->variable);

    switch (function_type) {
        case kGAB:
            curve->parametric.a = read_big_fixed(paraTag->variable + 4);
            curve->parametric.b = read_big_fixed(paraTag->variable + 8);
            if (curve->parametric.a == 0) {
                return false;
            }
            curve->parametric.d = -curve->parametric.b / curve->parametric.a;
            break;
        case kGABC:
            curve->parametric.a = read_big_fixed(paraTag->variable + 4);
            curve->parametric.b = read_big_fixed(paraTag->variable + 8);
            curve->parametric.e = read_big_fixed(paraTag->variable + 12);
            if (curve->parametric.a == 0) {
                return false;
            }
            curve->parametric.d = -curve->parametric.b / curve->parametric.a;
            curve->parametric.f = curve->parametric.e;
            break;
        case kGABCD:
            curve->parametric.a = read_big_fixed(paraTag->variable + 4);
            curve->parametric.b = read_big_fixed(paraTag->variable + 8);
            curve->parametric.c = read_big_fixed(paraTag->variable + 12);
            curve->parametric.d = read_big_fixed(paraTag->variable + 16);
            break;
        case kGABCDEF:
            curve->parametric.a = read_big_fixed(paraTag->variable + 4);
            curve->parametric.b = read_big_fixed(paraTag->variable + 8);
            curve->parametric.c = read_big_fixed(paraTag->variable + 12);
            curve->parametric.d = read_big_fixed(paraTag->variable + 16);
            curve->parametric.e = read_big_fixed(paraTag->variable + 20);
            curve->parametric.f = read_big_fixed(paraTag->variable + 24);
            break;
    }
    return skcms_TransferFunction_isSRGBish(&curve->parametric);
}

typedef struct {
    uint8_t type          [4];
    uint8_t reserved      [4];
    uint8_t value_count   [4];
    uint8_t variable      [1];
} curv_Layout;

static bool read_curve_curv(const uint8_t* buf, uint32_t size,
                            skcms_Curve* curve, uint32_t* curve_size) {
    if (size < SAFE_FIXED_SIZE(curv_Layout)) {
        return false;
    }

    const curv_Layout* curvTag = (const curv_Layout*)buf;

    uint32_t value_count = read_big_u32(curvTag->value_count);
    if (size < SAFE_FIXED_SIZE(curv_Layout) + value_count * SAFE_SIZEOF(uint16_t)) {
        return false;
    }

    if (curve_size) {
        *curve_size = SAFE_FIXED_SIZE(curv_Layout) + value_count * SAFE_SIZEOF(uint16_t);
    }

    if (value_count < 2) {
        curve->table_entries = 0;
        curve->parametric.a  = 1.0f;
        curve->parametric.b  = 0.0f;
        curve->parametric.c  = 0.0f;
        curve->parametric.d  = 0.0f;
        curve->parametric.e  = 0.0f;
        curve->parametric.f  = 0.0f;
        if (value_count == 0) {

            curve->parametric.g = 1.0f;
        } else {

            curve->parametric.g = read_big_u16(curvTag->variable) * (1.0f / 256.0f);
        }
    } else {
        curve->table_8       = nullptr;
        curve->table_16      = curvTag->variable;
        curve->table_entries = value_count;
    }

    return true;
}

static bool read_curve(const uint8_t* buf, uint32_t size,
                       skcms_Curve* curve, uint32_t* curve_size) {
    if (!buf || size < 4 || !curve) {
        return false;
    }

    uint32_t type = read_big_u32(buf);
    if (type == skcms_Signature_para) {
        return read_curve_para(buf, size, curve, curve_size);
    } else if (type == skcms_Signature_curv) {
        return read_curve_curv(buf, size, curve, curve_size);
    }

    return false;
}

typedef struct {
    uint8_t type                 [ 4];
    uint8_t reserved_a           [ 4];
    uint8_t input_channels       [ 1];
    uint8_t output_channels      [ 1];
    uint8_t grid_points          [ 1];
    uint8_t reserved_b           [ 1];
    uint8_t matrix               [36];
} mft_CommonLayout;

typedef struct {
    mft_CommonLayout common      [1];

    uint8_t variable             [1];
} mft1_Layout;

typedef struct {
    mft_CommonLayout common      [1];

    uint8_t input_table_entries  [2];
    uint8_t output_table_entries [2];
    uint8_t variable             [1];
} mft2_Layout;

static bool read_mft_common(const mft_CommonLayout* mftTag, skcms_A2B* a2b) {

    a2b->matrix_channels = 0;
    a2b-> input_channels = mftTag-> input_channels[0];
    a2b->output_channels = mftTag->output_channels[0];

    if (a2b->output_channels != ARRAY_COUNT(a2b->output_curves)) {
        return false;
    }

    if (a2b->input_channels < 1 || a2b->input_channels > ARRAY_COUNT(a2b->input_curves)) {
        return false;
    }

    for (uint32_t i = 0; i < a2b->input_channels; ++i) {
        a2b->grid_points[i] = mftTag->grid_points[0];
    }

    if (a2b->grid_points[0] < 2) {
        return false;
    }
    return true;
}

static bool read_mft_common(const mft_CommonLayout* mftTag, skcms_B2A* b2a) {

    b2a->matrix_channels = 0;
    b2a-> input_channels = mftTag-> input_channels[0];
    b2a->output_channels = mftTag->output_channels[0];

    if (b2a->input_channels != ARRAY_COUNT(b2a->input_curves)) {
        return false;
    }
    if (b2a->output_channels < 3 || b2a->output_channels > ARRAY_COUNT(b2a->output_curves)) {
        return false;
    }

    for (uint32_t i = 0; i < b2a->input_channels; ++i) {
        b2a->grid_points[i] = mftTag->grid_points[0];
    }
    if (b2a->grid_points[0] < 2) {
        return false;
    }
    return true;
}

template <typename A2B_or_B2A>
static bool init_tables(const uint8_t* table_base, uint64_t max_tables_len, uint32_t byte_width,
                        uint32_t input_table_entries, uint32_t output_table_entries,
                        A2B_or_B2A* out) {

    uint32_t byte_len_per_input_table  = input_table_entries * byte_width;
    uint32_t byte_len_per_output_table = output_table_entries * byte_width;

    uint32_t byte_len_all_input_tables  = out->input_channels * byte_len_per_input_table;
    uint32_t byte_len_all_output_tables = out->output_channels * byte_len_per_output_table;

    uint64_t grid_size = out->output_channels * byte_width;
    for (uint32_t axis = 0; axis < out->input_channels; ++axis) {
        grid_size *= out->grid_points[axis];
    }

    if (max_tables_len < byte_len_all_input_tables + grid_size + byte_len_all_output_tables) {
        return false;
    }

    for (uint32_t i = 0; i < out->input_channels; ++i) {
        out->input_curves[i].table_entries = input_table_entries;
        if (byte_width == 1) {
            out->input_curves[i].table_8  = table_base + i * byte_len_per_input_table;
            out->input_curves[i].table_16 = nullptr;
        } else {
            out->input_curves[i].table_8  = nullptr;
            out->input_curves[i].table_16 = table_base + i * byte_len_per_input_table;
        }
    }

    if (byte_width == 1) {
        out->grid_8  = table_base + byte_len_all_input_tables;
        out->grid_16 = nullptr;
    } else {
        out->grid_8  = nullptr;
        out->grid_16 = table_base + byte_len_all_input_tables;
    }

    const uint8_t* output_table_base = table_base + byte_len_all_input_tables + grid_size;
    for (uint32_t i = 0; i < out->output_channels; ++i) {
        out->output_curves[i].table_entries = output_table_entries;
        if (byte_width == 1) {
            out->output_curves[i].table_8  = output_table_base + i * byte_len_per_output_table;
            out->output_curves[i].table_16 = nullptr;
        } else {
            out->output_curves[i].table_8  = nullptr;
            out->output_curves[i].table_16 = output_table_base + i * byte_len_per_output_table;
        }
    }

    return true;
}

template <typename A2B_or_B2A>
static bool read_tag_mft1(const skcms_ICCTag* tag, A2B_or_B2A* out) {
    if (tag->size < SAFE_FIXED_SIZE(mft1_Layout)) {
        return false;
    }

    const mft1_Layout* mftTag = (const mft1_Layout*)tag->buf;
    if (!read_mft_common(mftTag->common, out)) {
        return false;
    }

    uint32_t input_table_entries  = 256;
    uint32_t output_table_entries = 256;

    return init_tables(mftTag->variable, tag->size - SAFE_FIXED_SIZE(mft1_Layout), 1,
                       input_table_entries, output_table_entries, out);
}

template <typename A2B_or_B2A>
static bool read_tag_mft2(const skcms_ICCTag* tag, A2B_or_B2A* out) {
    if (tag->size < SAFE_FIXED_SIZE(mft2_Layout)) {
        return false;
    }

    const mft2_Layout* mftTag = (const mft2_Layout*)tag->buf;
    if (!read_mft_common(mftTag->common, out)) {
        return false;
    }

    uint32_t input_table_entries = read_big_u16(mftTag->input_table_entries);
    uint32_t output_table_entries = read_big_u16(mftTag->output_table_entries);

    if (input_table_entries < 2 || input_table_entries > 4096 ||
        output_table_entries < 2 || output_table_entries > 4096) {
        return false;
    }

    return init_tables(mftTag->variable, tag->size - SAFE_FIXED_SIZE(mft2_Layout), 2,
                       input_table_entries, output_table_entries, out);
}

static bool read_curves(const uint8_t* buf, uint32_t size, uint32_t curve_offset,
                        uint32_t num_curves, skcms_Curve* curves) {
    for (uint32_t i = 0; i < num_curves; ++i) {
        if (curve_offset > size) {
            return false;
        }

        uint32_t curve_bytes;
        if (!read_curve(buf + curve_offset, size - curve_offset, &curves[i], &curve_bytes)) {
            return false;
        }

        if (curve_bytes > UINT32_MAX - 3) {
            return false;
        }
        curve_bytes = (curve_bytes + 3) & ~3U;

        uint64_t new_offset_64 = (uint64_t)curve_offset + curve_bytes;
        curve_offset = (uint32_t)new_offset_64;
        if (new_offset_64 != curve_offset) {
            return false;
        }
    }

    return true;
}

typedef struct {
    uint8_t type                 [ 4];
    uint8_t reserved_a           [ 4];
    uint8_t input_channels       [ 1];
    uint8_t output_channels      [ 1];
    uint8_t reserved_b           [ 2];
    uint8_t b_curve_offset       [ 4];
    uint8_t matrix_offset        [ 4];
    uint8_t m_curve_offset       [ 4];
    uint8_t clut_offset          [ 4];
    uint8_t a_curve_offset       [ 4];
} mAB_or_mBA_Layout;

typedef struct {
    uint8_t grid_points          [16];
    uint8_t grid_byte_width      [ 1];
    uint8_t reserved             [ 3];
    uint8_t variable             [1];
} CLUT_Layout;

static bool read_tag_mab(const skcms_ICCTag* tag, skcms_A2B* a2b, bool pcs_is_xyz) {
    if (tag->size < SAFE_SIZEOF(mAB_or_mBA_Layout)) {
        return false;
    }

    const mAB_or_mBA_Layout* mABTag = (const mAB_or_mBA_Layout*)tag->buf;

    a2b->input_channels  = mABTag->input_channels[0];
    a2b->output_channels = mABTag->output_channels[0];

    if (a2b->output_channels != ARRAY_COUNT(a2b->output_curves)) {
        return false;
    }

    if (a2b->input_channels > ARRAY_COUNT(a2b->input_curves)) {
        return false;
    }

    uint32_t b_curve_offset = read_big_u32(mABTag->b_curve_offset);
    uint32_t matrix_offset  = read_big_u32(mABTag->matrix_offset);
    uint32_t m_curve_offset = read_big_u32(mABTag->m_curve_offset);
    uint32_t clut_offset    = read_big_u32(mABTag->clut_offset);
    uint32_t a_curve_offset = read_big_u32(mABTag->a_curve_offset);

    if (0 == b_curve_offset) {
        return false;
    }

    if (!read_curves(tag->buf, tag->size, b_curve_offset, a2b->output_channels,
                     a2b->output_curves)) {
        return false;
    }

    if (0 != m_curve_offset) {
        if (0 == matrix_offset) {
            return false;
        }
        a2b->matrix_channels = a2b->output_channels;
        if (!read_curves(tag->buf, tag->size, m_curve_offset, a2b->matrix_channels,
                         a2b->matrix_curves)) {
            return false;
        }

        if (tag->size < matrix_offset + 12 * SAFE_SIZEOF(uint32_t)) {
            return false;
        }
        float encoding_factor = pcs_is_xyz ? (65535 / 32768.0f) : 1.0f;
        const uint8_t* mtx_buf = tag->buf + matrix_offset;
        a2b->matrix.vals[0][0] = encoding_factor * read_big_fixed(mtx_buf +  0);
        a2b->matrix.vals[0][1] = encoding_factor * read_big_fixed(mtx_buf +  4);
        a2b->matrix.vals[0][2] = encoding_factor * read_big_fixed(mtx_buf +  8);
        a2b->matrix.vals[1][0] = encoding_factor * read_big_fixed(mtx_buf + 12);
        a2b->matrix.vals[1][1] = encoding_factor * read_big_fixed(mtx_buf + 16);
        a2b->matrix.vals[1][2] = encoding_factor * read_big_fixed(mtx_buf + 20);
        a2b->matrix.vals[2][0] = encoding_factor * read_big_fixed(mtx_buf + 24);
        a2b->matrix.vals[2][1] = encoding_factor * read_big_fixed(mtx_buf + 28);
        a2b->matrix.vals[2][2] = encoding_factor * read_big_fixed(mtx_buf + 32);
        a2b->matrix.vals[0][3] = encoding_factor * read_big_fixed(mtx_buf + 36);
        a2b->matrix.vals[1][3] = encoding_factor * read_big_fixed(mtx_buf + 40);
        a2b->matrix.vals[2][3] = encoding_factor * read_big_fixed(mtx_buf + 44);
    } else {
        if (0 != matrix_offset) {
            return false;
        }
        a2b->matrix_channels = 0;
    }

    if (0 != a_curve_offset) {
        if (0 == clut_offset) {
            return false;
        }
        if (!read_curves(tag->buf, tag->size, a_curve_offset, a2b->input_channels,
                         a2b->input_curves)) {
            return false;
        }

        if (tag->size < clut_offset + SAFE_FIXED_SIZE(CLUT_Layout)) {
            return false;
        }
        const CLUT_Layout* clut = (const CLUT_Layout*)(tag->buf + clut_offset);

        if (clut->grid_byte_width[0] == 1) {
            a2b->grid_8  = clut->variable;
            a2b->grid_16 = nullptr;
        } else if (clut->grid_byte_width[0] == 2) {
            a2b->grid_8  = nullptr;
            a2b->grid_16 = clut->variable;
        } else {
            return false;
        }

        uint64_t grid_size = a2b->output_channels * clut->grid_byte_width[0];
        for (uint32_t i = 0; i < a2b->input_channels; ++i) {
            a2b->grid_points[i] = clut->grid_points[i];

            if (a2b->grid_points[i] < 2) {
                return false;
            }
            grid_size *= a2b->grid_points[i];
        }
        if (tag->size < clut_offset + SAFE_FIXED_SIZE(CLUT_Layout) + grid_size) {
            return false;
        }
    } else {
        if (0 != clut_offset) {
            return false;
        }

        if (a2b->input_channels != a2b->output_channels) {
            return false;
        }

        a2b->input_channels = 0;
    }

    return true;
}

static bool read_tag_mba(const skcms_ICCTag* tag, skcms_B2A* b2a, bool pcs_is_xyz) {
    if (tag->size < SAFE_SIZEOF(mAB_or_mBA_Layout)) {
        return false;
    }

    const mAB_or_mBA_Layout* mBATag = (const mAB_or_mBA_Layout*)tag->buf;

    b2a->input_channels  = mBATag->input_channels[0];
    b2a->output_channels = mBATag->output_channels[0];

    if (b2a->input_channels != ARRAY_COUNT(b2a->input_curves)) {
        return false;
    }
    if (b2a->output_channels < 3 || b2a->output_channels > ARRAY_COUNT(b2a->output_curves)) {
        return false;
    }

    uint32_t b_curve_offset = read_big_u32(mBATag->b_curve_offset);
    uint32_t matrix_offset  = read_big_u32(mBATag->matrix_offset);
    uint32_t m_curve_offset = read_big_u32(mBATag->m_curve_offset);
    uint32_t clut_offset    = read_big_u32(mBATag->clut_offset);
    uint32_t a_curve_offset = read_big_u32(mBATag->a_curve_offset);

    if (0 == b_curve_offset) {
        return false;
    }

    if (!read_curves(tag->buf, tag->size, b_curve_offset, b2a->input_channels,
                     b2a->input_curves)) {
        return false;
    }

    if (0 != m_curve_offset) {
        if (0 == matrix_offset) {
            return false;
        }

        b2a->matrix_channels = b2a->input_channels;

        if (!read_curves(tag->buf, tag->size, m_curve_offset, b2a->matrix_channels,
                         b2a->matrix_curves)) {
            return false;
        }

        if (tag->size < matrix_offset + 12 * SAFE_SIZEOF(uint32_t)) {
            return false;
        }
        float encoding_factor = pcs_is_xyz ? (32768 / 65535.0f) : 1.0f;
        const uint8_t* mtx_buf = tag->buf + matrix_offset;
        b2a->matrix.vals[0][0] = encoding_factor * read_big_fixed(mtx_buf +  0);
        b2a->matrix.vals[0][1] = encoding_factor * read_big_fixed(mtx_buf +  4);
        b2a->matrix.vals[0][2] = encoding_factor * read_big_fixed(mtx_buf +  8);
        b2a->matrix.vals[1][0] = encoding_factor * read_big_fixed(mtx_buf + 12);
        b2a->matrix.vals[1][1] = encoding_factor * read_big_fixed(mtx_buf + 16);
        b2a->matrix.vals[1][2] = encoding_factor * read_big_fixed(mtx_buf + 20);
        b2a->matrix.vals[2][0] = encoding_factor * read_big_fixed(mtx_buf + 24);
        b2a->matrix.vals[2][1] = encoding_factor * read_big_fixed(mtx_buf + 28);
        b2a->matrix.vals[2][2] = encoding_factor * read_big_fixed(mtx_buf + 32);
        b2a->matrix.vals[0][3] = encoding_factor * read_big_fixed(mtx_buf + 36);
        b2a->matrix.vals[1][3] = encoding_factor * read_big_fixed(mtx_buf + 40);
        b2a->matrix.vals[2][3] = encoding_factor * read_big_fixed(mtx_buf + 44);
    } else {
        if (0 != matrix_offset) {
            return false;
        }
        b2a->matrix_channels = 0;
    }

    if (0 != a_curve_offset) {
        if (0 == clut_offset) {
            return false;
        }

        if (!read_curves(tag->buf, tag->size, a_curve_offset, b2a->output_channels,
                         b2a->output_curves)) {
            return false;
        }

        if (tag->size < clut_offset + SAFE_FIXED_SIZE(CLUT_Layout)) {
            return false;
        }
        const CLUT_Layout* clut = (const CLUT_Layout*)(tag->buf + clut_offset);

        if (clut->grid_byte_width[0] == 1) {
            b2a->grid_8  = clut->variable;
            b2a->grid_16 = nullptr;
        } else if (clut->grid_byte_width[0] == 2) {
            b2a->grid_8  = nullptr;
            b2a->grid_16 = clut->variable;
        } else {
            return false;
        }

        uint64_t grid_size = b2a->output_channels * clut->grid_byte_width[0];
        for (uint32_t i = 0; i < b2a->input_channels; ++i) {
            b2a->grid_points[i] = clut->grid_points[i];
            if (b2a->grid_points[i] < 2) {
                return false;
            }
            grid_size *= b2a->grid_points[i];
        }
        if (tag->size < clut_offset + SAFE_FIXED_SIZE(CLUT_Layout) + grid_size) {
            return false;
        }
    } else {
        if (0 != clut_offset) {
            return false;
        }

        if (b2a->input_channels != b2a->output_channels) {
            return false;
        }

        b2a->output_channels = 0;
    }
    return true;
}

static int fit_linear(const skcms_Curve* curve, int N, float tol,
                      float* c, float* d, float* f = nullptr) {
    assert(N > 1);

    const float dx = 1.0f / static_cast<float>(N - 1);

    int lin_points = 1;

    float f_zero = 0.0f;
    if (f) {
        *f = eval_curve(curve, 0);
    } else {
        f = &f_zero;
    }

    float slope_min = -INFINITY_;
    float slope_max = +INFINITY_;
    for (int i = 1; i < N; ++i) {
        float x = static_cast<float>(i) * dx;
        float y = eval_curve(curve, x);

        float slope_max_i = (y + tol - *f) / x,
              slope_min_i = (y - tol - *f) / x;
        if (slope_max_i < slope_min || slope_max < slope_min_i) {

            break;
        }
        slope_max = fminf_(slope_max, slope_max_i);
        slope_min = fmaxf_(slope_min, slope_min_i);

        float cur_slope = (y - *f) / x;
        if (slope_min <= cur_slope && cur_slope <= slope_max) {
            lin_points = i + 1;
            *c = cur_slope;
        }
    }

    *d = static_cast<float>(lin_points - 1) * dx;
    return lin_points;
}

static void canonicalize_identity(skcms_Curve* curve) {
    if (curve->table_entries && curve->table_entries <= (uint32_t)INT_MAX) {
        int N = (int)curve->table_entries;

        float c = 0.0f, d = 0.0f, f = 0.0f;
        if (N == fit_linear(curve, N, 1.0f/static_cast<float>(2*N), &c,&d,&f)
            && c == 1.0f
            && f == 0.0f) {
            curve->table_entries = 0;
            curve->table_8       = nullptr;
            curve->table_16      = nullptr;
            curve->parametric    = skcms_TransferFunction{1,1,0,0,0,0,0};
        }
    }
}

static bool read_a2b(const skcms_ICCTag* tag, skcms_A2B* a2b, bool pcs_is_xyz) {
    bool ok = false;
    if (tag->type == skcms_Signature_mft1) { ok = read_tag_mft1(tag, a2b); }
    if (tag->type == skcms_Signature_mft2) { ok = read_tag_mft2(tag, a2b); }
    if (tag->type == skcms_Signature_mAB ) { ok = read_tag_mab(tag, a2b, pcs_is_xyz); }
    if (!ok) {
        return false;
    }

    if (a2b->input_channels > 0) { canonicalize_identity(a2b->input_curves + 0); }
    if (a2b->input_channels > 1) { canonicalize_identity(a2b->input_curves + 1); }
    if (a2b->input_channels > 2) { canonicalize_identity(a2b->input_curves + 2); }
    if (a2b->input_channels > 3) { canonicalize_identity(a2b->input_curves + 3); }

    if (a2b->matrix_channels > 0) { canonicalize_identity(a2b->matrix_curves + 0); }
    if (a2b->matrix_channels > 1) { canonicalize_identity(a2b->matrix_curves + 1); }
    if (a2b->matrix_channels > 2) { canonicalize_identity(a2b->matrix_curves + 2); }

    if (a2b->output_channels > 0) { canonicalize_identity(a2b->output_curves + 0); }
    if (a2b->output_channels > 1) { canonicalize_identity(a2b->output_curves + 1); }
    if (a2b->output_channels > 2) { canonicalize_identity(a2b->output_curves + 2); }

    return true;
}

static bool read_b2a(const skcms_ICCTag* tag, skcms_B2A* b2a, bool pcs_is_xyz) {
    bool ok = false;
    if (tag->type == skcms_Signature_mft1) { ok = read_tag_mft1(tag, b2a); }
    if (tag->type == skcms_Signature_mft2) { ok = read_tag_mft2(tag, b2a); }
    if (tag->type == skcms_Signature_mBA ) { ok = read_tag_mba(tag, b2a, pcs_is_xyz); }
    if (!ok) {
        return false;
    }

    if (b2a->input_channels > 0) { canonicalize_identity(b2a->input_curves + 0); }
    if (b2a->input_channels > 1) { canonicalize_identity(b2a->input_curves + 1); }
    if (b2a->input_channels > 2) { canonicalize_identity(b2a->input_curves + 2); }

    if (b2a->matrix_channels > 0) { canonicalize_identity(b2a->matrix_curves + 0); }
    if (b2a->matrix_channels > 1) { canonicalize_identity(b2a->matrix_curves + 1); }
    if (b2a->matrix_channels > 2) { canonicalize_identity(b2a->matrix_curves + 2); }

    if (b2a->output_channels > 0) { canonicalize_identity(b2a->output_curves + 0); }
    if (b2a->output_channels > 1) { canonicalize_identity(b2a->output_curves + 1); }
    if (b2a->output_channels > 2) { canonicalize_identity(b2a->output_curves + 2); }
    if (b2a->output_channels > 3) { canonicalize_identity(b2a->output_curves + 3); }

    return true;
}

typedef struct {
    uint8_t type                     [4];
    uint8_t reserved                 [4];
    uint8_t color_primaries          [1];
    uint8_t transfer_characteristics [1];
    uint8_t matrix_coefficients      [1];
    uint8_t video_full_range_flag    [1];
} CICP_Layout;

static bool read_cicp(const skcms_ICCTag* tag, skcms_CICP* cicp) {
    if (tag->type != skcms_Signature_CICP || tag->size < SAFE_SIZEOF(CICP_Layout)) {
        return false;
    }

    const CICP_Layout* cicpTag = (const CICP_Layout*)tag->buf;

    cicp->color_primaries          = cicpTag->color_primaries[0];
    cicp->transfer_characteristics = cicpTag->transfer_characteristics[0];
    cicp->matrix_coefficients      = cicpTag->matrix_coefficients[0];
    cicp->video_full_range_flag    = cicpTag->video_full_range_flag[0];
    return true;
}

void skcms_GetTagByIndex(const skcms_ICCProfile* profile, uint32_t idx, skcms_ICCTag* tag) {
    if (!profile || !profile->buffer || !tag) { return; }
    if (idx > profile->tag_count) { return; }
    const tag_Layout* tags = get_tag_table(profile);
    tag->signature = read_big_u32(tags[idx].signature);
    tag->size      = read_big_u32(tags[idx].size);
    tag->buf       = read_big_u32(tags[idx].offset) + profile->buffer;
    tag->type      = read_big_u32(tag->buf);
}

bool skcms_GetTagBySignature(const skcms_ICCProfile* profile, uint32_t sig, skcms_ICCTag* tag) {
    if (!profile || !profile->buffer || !tag) { return false; }
    const tag_Layout* tags = get_tag_table(profile);
    for (uint32_t i = 0; i < profile->tag_count; ++i) {
        if (read_big_u32(tags[i].signature) == sig) {
            tag->signature = sig;
            tag->size      = read_big_u32(tags[i].size);
            tag->buf       = read_big_u32(tags[i].offset) + profile->buffer;
            tag->type      = read_big_u32(tag->buf);
            return true;
        }
    }
    return false;
}

static bool usable_as_src(const skcms_ICCProfile* profile) {
    return profile->has_A2B
       || (profile->has_trc && profile->has_toXYZD50);
}

bool skcms_ParseWithA2BPriority(const void* buf, size_t len,
                                const int priority[], const int priorities,
                                skcms_ICCProfile* profile) {
    static_assert(SAFE_SIZEOF(header_Layout) == 132, "need to update header code");

    if (!profile) {
        return false;
    }
    memset(profile, 0, SAFE_SIZEOF(*profile));

    if (len < SAFE_SIZEOF(header_Layout)) {
        return false;
    }

    const header_Layout* header  = (const header_Layout*)buf;
    profile->buffer              = (const uint8_t*)buf;
    profile->size                = read_big_u32(header->size);
    uint32_t version             = read_big_u32(header->version);
    profile->data_color_space    = read_big_u32(header->data_color_space);
    profile->pcs                 = read_big_u32(header->pcs);
    uint32_t signature           = read_big_u32(header->signature);
    float illuminant_X           = read_big_fixed(header->illuminant_X);
    float illuminant_Y           = read_big_fixed(header->illuminant_Y);
    float illuminant_Z           = read_big_fixed(header->illuminant_Z);
    profile->tag_count           = read_big_u32(header->tag_count);

    uint64_t tag_table_size = profile->tag_count * SAFE_SIZEOF(tag_Layout);
    if (signature != skcms_Signature_acsp ||
        profile->size > len ||
        profile->size < SAFE_SIZEOF(header_Layout) + tag_table_size ||
        (version >> 24) > 4) {
        return false;
    }

    if (fabsf_(illuminant_X - 0.9642f) > 0.0100f ||
        fabsf_(illuminant_Y - 1.0000f) > 0.0100f ||
        fabsf_(illuminant_Z - 0.8249f) > 0.0100f) {
        return false;
    }

    const tag_Layout* tags = get_tag_table(profile);
    for (uint32_t i = 0; i < profile->tag_count; ++i) {
        uint32_t tag_offset = read_big_u32(tags[i].offset);
        uint32_t tag_size   = read_big_u32(tags[i].size);
        uint64_t tag_end    = (uint64_t)tag_offset + (uint64_t)tag_size;
        if (tag_size < 4 || tag_end > profile->size) {
            return false;
        }
    }

    if (profile->pcs != skcms_Signature_XYZ && profile->pcs != skcms_Signature_Lab) {
        return false;
    }

    bool pcs_is_xyz = profile->pcs == skcms_Signature_XYZ;

    skcms_ICCTag kTRC;
    if (profile->data_color_space == skcms_Signature_Gray &&
        skcms_GetTagBySignature(profile, skcms_Signature_kTRC, &kTRC)) {
        if (!read_curve(kTRC.buf, kTRC.size, &profile->trc[0], nullptr)) {

            return false;
        }
        profile->trc[1] = profile->trc[0];
        profile->trc[2] = profile->trc[0];
        profile->has_trc = true;

        if (pcs_is_xyz) {
            profile->toXYZD50.vals[0][0] = illuminant_X;
            profile->toXYZD50.vals[1][1] = illuminant_Y;
            profile->toXYZD50.vals[2][2] = illuminant_Z;
            profile->has_toXYZD50 = true;
        }
    } else {
        skcms_ICCTag rTRC, gTRC, bTRC;
        if (skcms_GetTagBySignature(profile, skcms_Signature_rTRC, &rTRC) &&
            skcms_GetTagBySignature(profile, skcms_Signature_gTRC, &gTRC) &&
            skcms_GetTagBySignature(profile, skcms_Signature_bTRC, &bTRC)) {
            if (!read_curve(rTRC.buf, rTRC.size, &profile->trc[0], nullptr) ||
                !read_curve(gTRC.buf, gTRC.size, &profile->trc[1], nullptr) ||
                !read_curve(bTRC.buf, bTRC.size, &profile->trc[2], nullptr)) {

                return false;
            }
            profile->has_trc = true;
        }

        skcms_ICCTag rXYZ, gXYZ, bXYZ;
        if (skcms_GetTagBySignature(profile, skcms_Signature_rXYZ, &rXYZ) &&
            skcms_GetTagBySignature(profile, skcms_Signature_gXYZ, &gXYZ) &&
            skcms_GetTagBySignature(profile, skcms_Signature_bXYZ, &bXYZ)) {
            if (!read_to_XYZD50(&rXYZ, &gXYZ, &bXYZ, &profile->toXYZD50)) {

                return false;
            }
            profile->has_toXYZD50 = true;
        }
    }

    for (int i = 0; i < priorities; i++) {

        if (priority[i] < 0 || priority[i] > 2) {
            return false;
        }
        uint32_t sig = skcms_Signature_A2B0 + static_cast<uint32_t>(priority[i]);
        skcms_ICCTag tag;
        if (skcms_GetTagBySignature(profile, sig, &tag)) {
            if (!read_a2b(&tag, &profile->A2B, pcs_is_xyz)) {

                return false;
            }
            profile->has_A2B = true;
            break;
        }
    }

    for (int i = 0; i < priorities; i++) {

        if (priority[i] < 0 || priority[i] > 2) {
            return false;
        }
        uint32_t sig = skcms_Signature_B2A0 + static_cast<uint32_t>(priority[i]);
        skcms_ICCTag tag;
        if (skcms_GetTagBySignature(profile, sig, &tag)) {
            if (!read_b2a(&tag, &profile->B2A, pcs_is_xyz)) {

                return false;
            }
            profile->has_B2A = true;
            break;
        }
    }

    skcms_ICCTag cicp_tag;
    if (skcms_GetTagBySignature(profile, skcms_Signature_CICP, &cicp_tag)) {
        if (!read_cicp(&cicp_tag, &profile->CICP)) {

            return false;
        }
        profile->has_CICP = true;
    }

    return usable_as_src(profile);
}

const skcms_ICCProfile* skcms_sRGB_profile() {
    static const skcms_ICCProfile sRGB_profile = {
        nullptr,

        0,
        skcms_Signature_RGB,
        skcms_Signature_XYZ,
        0,

        {
            {{0, {2.4f, (float)(1/1.055), (float)(0.055/1.055), (float)(1/12.92), 0.04045f, 0, 0}}},
            {{0, {2.4f, (float)(1/1.055), (float)(0.055/1.055), (float)(1/12.92), 0.04045f, 0, 0}}},
            {{0, {2.4f, (float)(1/1.055), (float)(0.055/1.055), (float)(1/12.92), 0.04045f, 0, 0}}},
        },

        {{
            { 0.436065674f, 0.385147095f, 0.143066406f },
            { 0.222488403f, 0.716873169f, 0.060607910f },
            { 0.013916016f, 0.097076416f, 0.714096069f },
        }},

        {
            {
                {{0, {0,0, 0,0,0,0,0}}},
                {{0, {0,0, 0,0,0,0,0}}},
                {{0, {0,0, 0,0,0,0,0}}},
                {{0, {0,0, 0,0,0,0,0}}},
            },
            nullptr,
            nullptr,
            0,
            {0,0,0,0},

            {
                {{0, {0,0, 0,0,0,0,0}}},
                {{0, {0,0, 0,0,0,0,0}}},
                {{0, {0,0, 0,0,0,0,0}}},
            },
            {{
                { 0,0,0,0 },
                { 0,0,0,0 },
                { 0,0,0,0 },
            }},
            0,

            0,
            {
                {{0, {0,0, 0,0,0,0,0}}},
                {{0, {0,0, 0,0,0,0,0}}},
                {{0, {0,0, 0,0,0,0,0}}},
            },
        },

        {
            {
                {{0, {0,0, 0,0,0,0,0}}},
                {{0, {0,0, 0,0,0,0,0}}},
                {{0, {0,0, 0,0,0,0,0}}},
            },
            0,

            0,
            {
                {{0, {0,0, 0,0,0,0,0}}},
                {{0, {0,0, 0,0,0,0,0}}},
                {{0, {0,0, 0,0,0,0,0}}},
            },
            {{
                { 0,0,0,0 },
                { 0,0,0,0 },
                { 0,0,0,0 },
            }},

            {
                {{0, {0,0, 0,0,0,0,0}}},
                {{0, {0,0, 0,0,0,0,0}}},
                {{0, {0,0, 0,0,0,0,0}}},
                {{0, {0,0, 0,0,0,0,0}}},
            },
            nullptr,
            nullptr,
            {0,0,0,0},
            0,
        },

        { 0, 0, 0, 0 },

        true,
        true,
        false,
        false,
        false,
    };
    return &sRGB_profile;
}

const skcms_ICCProfile* skcms_XYZD50_profile() {

    static const skcms_ICCProfile XYZD50_profile = {
        nullptr,

        0,
        skcms_Signature_RGB,
        skcms_Signature_XYZ,
        0,

        {
            {{0, {1,1, 0,0,0,0,0}}},
            {{0, {1,1, 0,0,0,0,0}}},
            {{0, {1,1, 0,0,0,0,0}}},
        },

        {{
            { 1,0,0 },
            { 0,1,0 },
            { 0,0,1 },
        }},

        {
            {
                {{0, {0,0, 0,0,0,0,0}}},
                {{0, {0,0, 0,0,0,0,0}}},
                {{0, {0,0, 0,0,0,0,0}}},
                {{0, {0,0, 0,0,0,0,0}}},
            },
            nullptr,
            nullptr,
            0,
            {0,0,0,0},

            {
                {{0, {0,0, 0,0,0,0,0}}},
                {{0, {0,0, 0,0,0,0,0}}},
                {{0, {0,0, 0,0,0,0,0}}},
            },
            {{
                { 0,0,0,0 },
                { 0,0,0,0 },
                { 0,0,0,0 },
            }},
            0,

            0,
            {
                {{0, {0,0, 0,0,0,0,0}}},
                {{0, {0,0, 0,0,0,0,0}}},
                {{0, {0,0, 0,0,0,0,0}}},
            },
        },

        {
            {
                {{0, {0,0, 0,0,0,0,0}}},
                {{0, {0,0, 0,0,0,0,0}}},
                {{0, {0,0, 0,0,0,0,0}}},
            },
            0,

            0,
            {
                {{0, {0,0, 0,0,0,0,0}}},
                {{0, {0,0, 0,0,0,0,0}}},
                {{0, {0,0, 0,0,0,0,0}}},
            },
            {{
                { 0,0,0,0 },
                { 0,0,0,0 },
                { 0,0,0,0 },
            }},

            {
                {{0, {0,0, 0,0,0,0,0}}},
                {{0, {0,0, 0,0,0,0,0}}},
                {{0, {0,0, 0,0,0,0,0}}},
                {{0, {0,0, 0,0,0,0,0}}},
            },
            nullptr,
            nullptr,
            {0,0,0,0},
            0,
        },

        { 0, 0, 0, 0 },

        true,
        true,
        false,
        false,
        false,
    };

    return &XYZD50_profile;
}

const skcms_TransferFunction* skcms_sRGB_TransferFunction() {
    return &skcms_sRGB_profile()->trc[0].parametric;
}

const skcms_TransferFunction* skcms_sRGB_Inverse_TransferFunction() {
    static const skcms_TransferFunction sRGB_inv =
        {0.416666657f, 1.137283325f, -0.0f, 12.920000076f, 0.003130805f, -0.054969788f, -0.0f};
    return &sRGB_inv;
}

const skcms_TransferFunction* skcms_Identity_TransferFunction() {
    static const skcms_TransferFunction identity = {1,1,0,0,0,0,0};
    return &identity;
}

const uint8_t skcms_252_random_bytes[] = {
    8, 179, 128, 204, 253, 38, 134, 184, 68, 102, 32, 138, 99, 39, 169, 215,
    119, 26, 3, 223, 95, 239, 52, 132, 114, 74, 81, 234, 97, 116, 244, 205, 30,
    154, 173, 12, 51, 159, 122, 153, 61, 226, 236, 178, 229, 55, 181, 220, 191,
    194, 160, 126, 168, 82, 131, 18, 180, 245, 163, 22, 246, 69, 235, 252, 57,
    108, 14, 6, 152, 240, 255, 171, 242, 20, 227, 177, 238, 96, 85, 16, 211,
    70, 200, 149, 155, 146, 127, 145, 100, 151, 109, 19, 165, 208, 195, 164,
    137, 254, 182, 248, 64, 201, 45, 209, 5, 147, 207, 210, 113, 162, 83, 225,
    9, 31, 15, 231, 115, 37, 58, 53, 24, 49, 197, 56, 120, 172, 48, 21, 214,
    129, 111, 11, 50, 187, 196, 34, 60, 103, 71, 144, 47, 203, 77, 80, 232,
    140, 222, 250, 206, 166, 247, 139, 249, 221, 72, 106, 27, 199, 117, 54,
    219, 135, 118, 40, 79, 41, 251, 46, 93, 212, 92, 233, 148, 28, 121, 63,
    123, 158, 105, 59, 29, 42, 143, 23, 0, 107, 176, 87, 104, 183, 156, 193,
    189, 90, 188, 65, 190, 17, 198, 7, 186, 161, 1, 124, 78, 125, 170, 133,
    174, 218, 67, 157, 75, 101, 89, 217, 62, 33, 141, 228, 25, 35, 91, 230, 4,
    2, 13, 73, 86, 167, 237, 84, 243, 44, 185, 66, 130, 110, 150, 142, 216, 88,
    112, 36, 224, 136, 202, 76, 94, 98, 175, 213
};

bool skcms_ApproximatelyEqualProfiles(const skcms_ICCProfile* A, const skcms_ICCProfile* B) {

    if (A == B || 0 == memcmp(A,B, sizeof(skcms_ICCProfile))) {
        return true;
    }

    const auto CMYK = skcms_Signature_CMYK;
    if ((A->data_color_space == CMYK) != (B->data_color_space == CMYK)) {
        return false;
    }

    skcms_PixelFormat fmt = skcms_PixelFormat_RGB_888;
    size_t npixels = 84;
    if (A->data_color_space == skcms_Signature_CMYK) {
        fmt = skcms_PixelFormat_RGBA_8888;
        npixels = 63;
    }

    uint8_t dstA[252],
            dstB[252];
    if (!skcms_Transform(
                skcms_252_random_bytes,     fmt, skcms_AlphaFormat_Unpremul, A,
                dstA, skcms_PixelFormat_RGB_888, skcms_AlphaFormat_Unpremul, skcms_XYZD50_profile(),
                npixels)) {
        return false;
    }
    if (!skcms_Transform(
                skcms_252_random_bytes,     fmt, skcms_AlphaFormat_Unpremul, B,
                dstB, skcms_PixelFormat_RGB_888, skcms_AlphaFormat_Unpremul, skcms_XYZD50_profile(),
                npixels)) {
        return false;
    }

    for (size_t i = 0; i < 252; i++) {
        if (abs((int)dstA[i] - (int)dstB[i]) > 1) {
            return false;
        }
    }
    return true;
}

bool skcms_TRCs_AreApproximateInverse(const skcms_ICCProfile* profile,
                                      const skcms_TransferFunction* inv_tf) {
    if (!profile || !profile->has_trc) {
        return false;
    }

    return skcms_AreApproximateInverses(&profile->trc[0], inv_tf) &&
           skcms_AreApproximateInverses(&profile->trc[1], inv_tf) &&
           skcms_AreApproximateInverses(&profile->trc[2], inv_tf);
}

static bool is_zero_to_one(float x) {
    return 0 <= x && x <= 1;
}

typedef struct { float vals[3]; } skcms_Vector3;

static skcms_Vector3 mv_mul(const skcms_Matrix3x3* m, const skcms_Vector3* v) {
    skcms_Vector3 dst = {{0,0,0}};
    for (int row = 0; row < 3; ++row) {
        dst.vals[row] = m->vals[row][0] * v->vals[0]
                      + m->vals[row][1] * v->vals[1]
                      + m->vals[row][2] * v->vals[2];
    }
    return dst;
}

bool skcms_AdaptToXYZD50(float wx, float wy,
                         skcms_Matrix3x3* toXYZD50) {
    if (!is_zero_to_one(wx) || !is_zero_to_one(wy) ||
        !toXYZD50) {
        return false;
    }

    skcms_Vector3 wXYZ = { { wx / wy, 1, (1 - wx - wy) / wy } };

    skcms_Vector3 wXYZD50 = { { 0.96422f, 1.0f, 0.82521f } };

    skcms_Matrix3x3 xyz_to_lms = {{
        {  0.8951f,  0.2664f, -0.1614f },
        { -0.7502f,  1.7135f,  0.0367f },
        {  0.0389f, -0.0685f,  1.0296f },
    }};
    skcms_Matrix3x3 lms_to_xyz = {{
        {  0.9869929f, -0.1470543f, 0.1599627f },
        {  0.4323053f,  0.5183603f, 0.0492912f },
        { -0.0085287f,  0.0400428f, 0.9684867f },
    }};

    skcms_Vector3 srcCone = mv_mul(&xyz_to_lms, &wXYZ);
    skcms_Vector3 dstCone = mv_mul(&xyz_to_lms, &wXYZD50);

    *toXYZD50 = {{
        { dstCone.vals[0] / srcCone.vals[0], 0, 0 },
        { 0, dstCone.vals[1] / srcCone.vals[1], 0 },
        { 0, 0, dstCone.vals[2] / srcCone.vals[2] },
    }};
    *toXYZD50 = skcms_Matrix3x3_concat(toXYZD50, &xyz_to_lms);
    *toXYZD50 = skcms_Matrix3x3_concat(&lms_to_xyz, toXYZD50);

    return true;
}

bool skcms_PrimariesToXYZD50(float rx, float ry,
                             float gx, float gy,
                             float bx, float by,
                             float wx, float wy,
                             skcms_Matrix3x3* toXYZD50) {
    if (!is_zero_to_one(rx) || !is_zero_to_one(ry) ||
        !is_zero_to_one(gx) || !is_zero_to_one(gy) ||
        !is_zero_to_one(bx) || !is_zero_to_one(by) ||
        !is_zero_to_one(wx) || !is_zero_to_one(wy) ||
        !toXYZD50) {
        return false;
    }

    skcms_Matrix3x3 primaries = {{
        { rx, gx, bx },
        { ry, gy, by },
        { 1 - rx - ry, 1 - gx - gy, 1 - bx - by },
    }};
    skcms_Matrix3x3 primaries_inv;
    if (!skcms_Matrix3x3_invert(&primaries, &primaries_inv)) {
        return false;
    }

    skcms_Vector3 wXYZ = { { wx / wy, 1, (1 - wx - wy) / wy } };
    skcms_Vector3 XYZ = mv_mul(&primaries_inv, &wXYZ);

    skcms_Matrix3x3 toXYZ = {{
        { XYZ.vals[0],           0,           0 },
        {           0, XYZ.vals[1],           0 },
        {           0,           0, XYZ.vals[2] },
    }};
    toXYZ = skcms_Matrix3x3_concat(&primaries, &toXYZ);

    skcms_Matrix3x3 DXtoD50;
    if (!skcms_AdaptToXYZD50(wx, wy, &DXtoD50)) {
        return false;
    }

    *toXYZD50 = skcms_Matrix3x3_concat(&DXtoD50, &toXYZ);
    return true;
}

bool skcms_Matrix3x3_invert(const skcms_Matrix3x3* src, skcms_Matrix3x3* dst) {
    double a00 = src->vals[0][0],
           a01 = src->vals[1][0],
           a02 = src->vals[2][0],
           a10 = src->vals[0][1],
           a11 = src->vals[1][1],
           a12 = src->vals[2][1],
           a20 = src->vals[0][2],
           a21 = src->vals[1][2],
           a22 = src->vals[2][2];

    double b0 = a00*a11 - a01*a10,
           b1 = a00*a12 - a02*a10,
           b2 = a01*a12 - a02*a11,
           b3 = a20,
           b4 = a21,
           b5 = a22;

    double determinant = b0*b5
                       - b1*b4
                       + b2*b3;

    if (determinant == 0) {
        return false;
    }

    double invdet = 1.0 / determinant;
    if (invdet > +FLT_MAX || invdet < -FLT_MAX || !isfinitef_((float)invdet)) {
        return false;
    }

    b0 *= invdet;
    b1 *= invdet;
    b2 *= invdet;
    b3 *= invdet;
    b4 *= invdet;
    b5 *= invdet;

    dst->vals[0][0] = (float)( a11*b5 - a12*b4 );
    dst->vals[1][0] = (float)( a02*b4 - a01*b5 );
    dst->vals[2][0] = (float)(        +     b2 );
    dst->vals[0][1] = (float)( a12*b3 - a10*b5 );
    dst->vals[1][1] = (float)( a00*b5 - a02*b3 );
    dst->vals[2][1] = (float)(        -     b1 );
    dst->vals[0][2] = (float)( a10*b4 - a11*b3 );
    dst->vals[1][2] = (float)( a01*b3 - a00*b4 );
    dst->vals[2][2] = (float)(        +     b0 );

    for (int r = 0; r < 3; ++r)
    for (int c = 0; c < 3; ++c) {
        if (!isfinitef_(dst->vals[r][c])) {
            return false;
        }
    }
    return true;
}

skcms_Matrix3x3 skcms_Matrix3x3_concat(const skcms_Matrix3x3* A, const skcms_Matrix3x3* B) {
    skcms_Matrix3x3 m = { { { 0,0,0 },{ 0,0,0 },{ 0,0,0 } } };
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 3; c++) {
            m.vals[r][c] = A->vals[r][0] * B->vals[0][c]
                         + A->vals[r][1] * B->vals[1][c]
                         + A->vals[r][2] * B->vals[2][c];
        }
    return m;
}

#if defined(__clang__)
    [[clang::no_sanitize("float-divide-by-zero")]]
#endif
bool skcms_TransferFunction_invert(const skcms_TransferFunction* src, skcms_TransferFunction* dst) {
    TF_PQish  pq;
    TF_HLGish hlg;
    switch (classify(*src, &pq, &hlg)) {
        case skcms_TFType_Invalid: return false;
        case skcms_TFType_sRGBish: break;

        case skcms_TFType_PQish:
            *dst = { TFKind_marker(skcms_TFType_PQish), -pq.A,  pq.D, 1.0f/pq.F
                                                      ,  pq.B, -pq.E, 1.0f/pq.C};
            return true;

        case skcms_TFType_HLGish:
            *dst = { TFKind_marker(skcms_TFType_HLGinvish), 1.0f/hlg.R, 1.0f/hlg.G
                                                          , 1.0f/hlg.a, hlg.b, hlg.c
                                                          , hlg.K_minus_1 };
            return true;

        case skcms_TFType_HLGinvish:
            *dst = { TFKind_marker(skcms_TFType_HLGish), 1.0f/hlg.R, 1.0f/hlg.G
                                                       , 1.0f/hlg.a, hlg.b, hlg.c
                                                       , hlg.K_minus_1 };
            return true;
    }

    assert (classify(*src) == skcms_TFType_sRGBish);

    skcms_TransferFunction inv = {0,0,0,0,0,0,0};

    float d_l =       src->c * src->d + src->f,
          d_r = powf_(src->a * src->d + src->b, src->g) + src->e;
    if (fabsf_(d_l - d_r) > 1/512.0f) {
        return false;
    }
    inv.d = d_l;

    if (inv.d > 0) {

        inv.c =    1.0f/src->c;
        inv.f = -src->f/src->c;
    }

    float k = powf_(src->a, -src->g);
    inv.g = 1.0f / src->g;
    inv.a = k;
    inv.b = -k * src->e;
    inv.e = -src->b / src->a;

    if (inv.a < 0) {
        return false;
    }

    if (inv.a * inv.d + inv.b < 0) {
        inv.b = -inv.a * inv.d;
    }

    if (classify(inv) != skcms_TFType_sRGBish) {
        return false;
    }

    assert (inv.a >= 0);
    assert (inv.a * inv.d + inv.b >= 0);

    float s = skcms_TransferFunction_eval(src, 1.0f);
    if (!isfinitef_(s)) {
        return false;
    }

    float sign = s < 0 ? -1.0f : 1.0f;
    s *= sign;
    if (s < inv.d) {
        inv.f = 1.0f - sign * inv.c * s;
    } else {
        inv.e = 1.0f - sign * powf_(inv.a * s + inv.b, inv.g);
    }

    *dst = inv;
    return classify(*dst) == skcms_TFType_sRGBish;
}

static float rg_nonlinear(float x,
                          const skcms_Curve* curve,
                          const skcms_TransferFunction* tf,
                          float dfdP[3]) {
    const float y = eval_curve(curve, x);

    const float g = tf->g, a = tf->a, b = tf->b,
                c = tf->c, d = tf->d, f = tf->f;

    const float Y = fmaxf_(a*y + b, 0.0f),
                D =        a*d + b;
    assert (D >= 0);

    dfdP[0] = logf_(Y)*powf_(Y, g)
            - logf_(D)*powf_(D, g);
    dfdP[1] = y*g*powf_(Y, g-1)
            - d*g*powf_(D, g-1);
    dfdP[2] =   g*powf_(Y, g-1)
            -   g*powf_(D, g-1);

    const float f_inv = powf_(Y, g)
                      - powf_(D, g)
                      + c*d + f;
    return x - f_inv;
}

static bool gauss_newton_step(const skcms_Curve* curve,
                                    skcms_TransferFunction* tf,
                              float x0, float dx, int N) {

    skcms_Matrix3x3 lhs = {{ {0,0,0}, {0,0,0}, {0,0,0} }};
    skcms_Vector3   rhs = {  {0,0,0} };

    for (int i = 0; i < N; i++) {
        float x = x0 + static_cast<float>(i)*dx;

        float dfdP[3] = {0,0,0};
        float resid = rg_nonlinear(x,curve,tf, dfdP);

        for (int r = 0; r < 3; r++) {
            for (int c = 0; c < 3; c++) {
                lhs.vals[r][c] += dfdP[r] * dfdP[c];
            }
            rhs.vals[r] += dfdP[r] * resid;
        }
    }

    for (int k = 0; k < 3; k++) {
        if (lhs.vals[0][k]==0 && lhs.vals[1][k]==0 && lhs.vals[2][k]==0 &&
            lhs.vals[k][0]==0 && lhs.vals[k][1]==0 && lhs.vals[k][2]==0) {
            lhs.vals[k][k] = 1;
        }
    }

    skcms_Matrix3x3 lhs_inv;
    if (!skcms_Matrix3x3_invert(&lhs, &lhs_inv)) {
        return false;
    }

    skcms_Vector3 dP = mv_mul(&lhs_inv, &rhs);
    tf->g += dP.vals[0];
    tf->a += dP.vals[1];
    tf->b += dP.vals[2];
    return isfinitef_(tf->g) && isfinitef_(tf->a) && isfinitef_(tf->b);
}

static float max_roundtrip_error_checked(const skcms_Curve* curve,
                                         const skcms_TransferFunction* tf_inv) {
    skcms_TransferFunction tf;
    if (!skcms_TransferFunction_invert(tf_inv, &tf) || skcms_TFType_sRGBish != classify(tf)) {
        return INFINITY_;
    }

    skcms_TransferFunction tf_inv_again;
    if (!skcms_TransferFunction_invert(&tf, &tf_inv_again)) {
        return INFINITY_;
    }

    return skcms_MaxRoundtripError(curve, &tf_inv_again);
}

static bool fit_nonlinear(const skcms_Curve* curve, int L, int N, skcms_TransferFunction* tf) {

    auto fixup_tf = [tf]() {

        if (tf->a < 0) {
            return false;
        }

        if (tf->a * tf->d + tf->b < 0) {
            tf->b = -tf->a * tf->d;
        }
        assert (tf->a >= 0 &&
                tf->a * tf->d + tf->b >= 0);

        tf->e =   tf->c*tf->d + tf->f
          - powf_(tf->a*tf->d + tf->b, tf->g);

        return isfinitef_(tf->e);
    };

    if (!fixup_tf()) {
        return false;
    }

    const float dx = 1.0f / static_cast<float>(N-1);

    skcms_TransferFunction best_tf = *tf;
    float best_max_error = INFINITY_;

    float init_error = max_roundtrip_error_checked(curve, tf);
    if (init_error < best_max_error) {
        best_max_error = init_error;
        best_tf = *tf;
    }

    for (int j = 0; j < 8; j++) {
        if (!gauss_newton_step(curve, tf, static_cast<float>(L)*dx, dx, N-L) || !fixup_tf()) {
            *tf = best_tf;
            return isfinitef_(best_max_error);
        }

        float max_error = max_roundtrip_error_checked(curve, tf);
        if (max_error < best_max_error) {
            best_max_error = max_error;
            best_tf = *tf;
        }
    }

    *tf = best_tf;
    return isfinitef_(best_max_error);
}

bool skcms_ApproximateCurve(const skcms_Curve* curve,
                            skcms_TransferFunction* approx,
                            float* max_error) {
    if (!curve || !approx || !max_error) {
        return false;
    }

    if (curve->table_entries == 0) {

        return false;
    }

    if (curve->table_entries == 1 || curve->table_entries > (uint32_t)INT_MAX) {

        return false;
    }

    int N = (int)curve->table_entries;
    const float dx = 1.0f / static_cast<float>(N - 1);

    *max_error = INFINITY_;
    const float kTolerances[] = { 1.5f / 65535.0f, 1.0f / 512.0f };
    for (int t = 0; t < ARRAY_COUNT(kTolerances); t++) {
        skcms_TransferFunction tf,
                               tf_inv;

        tf.f = 0.0f;
        int L = fit_linear(curve, N, kTolerances[t], &tf.c, &tf.d);

        if (L == N) {

            tf.g = 1;
            tf.a = tf.c;
            tf.b = tf.f;
            tf.c = tf.d = tf.e = tf.f = 0;
        } else if (L == N - 1) {

            tf.g = 1;
            tf.a = (eval_curve(curve, static_cast<float>(N-1)*dx) -
                    eval_curve(curve, static_cast<float>(N-2)*dx))
                 / dx;
            tf.b = eval_curve(curve, static_cast<float>(N-2)*dx)
                 - tf.a * static_cast<float>(N-2)*dx;
            tf.e = 0;
        } else {

            int mid = (L + N) / 2;
            float mid_x = static_cast<float>(mid) / static_cast<float>(N - 1);
            float mid_y = eval_curve(curve, mid_x);
            tf.g = log2f_(mid_y) / log2f_(mid_x);
            tf.a = 1;
            tf.b = 0;
            tf.e =    tf.c*tf.d + tf.f
              - powf_(tf.a*tf.d + tf.b, tf.g);

            if (!skcms_TransferFunction_invert(&tf, &tf_inv) ||
                !fit_nonlinear(curve, L,N, &tf_inv)) {
                continue;
            }

            if (!skcms_TransferFunction_invert(&tf_inv, &tf)) {
                assert(false);
                continue;
            }
        }

        if (skcms_TFType_sRGBish != classify(tf)) {
            continue;
        }

        if (!skcms_TransferFunction_invert(&tf, &tf_inv)) {
            continue;
        }

        float err = skcms_MaxRoundtripError(curve, &tf_inv);
        if (*max_error > err) {
            *max_error = err;
            *approx    = tf;
        }
    }
    return isfinitef_(*max_error);
}

enum class CpuType { Baseline, HSW, SKX };

static CpuType cpu_type() {
    #if defined(SKCMS_PORTABLE) || !defined(__x86_64__) || defined(SKCMS_FORCE_BASELINE)
        return CpuType::Baseline;
    #elif defined(SKCMS_FORCE_HSW)
        return CpuType::HSW;
    #elif defined(SKCMS_FORCE_SKX)
        return CpuType::SKX;
    #else
        static const CpuType type = []{
            if (!sAllowRuntimeCPUDetection) {
                return CpuType::Baseline;
            }

            uint32_t eax, ebx, ecx, edx;
            __asm__ __volatile__("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
                                         : "0"(1), "2"(0));
            if ((edx & (1u<<25)) &&
                (edx & (1u<<26)) &&
                (ecx & (1u<< 0)) &&
                (ecx & (1u<< 9)) &&
                (ecx & (1u<<12)) &&
                (ecx & (1u<<19)) &&
                (ecx & (1u<<20)) &&
                (ecx & (1u<<26)) &&
                (ecx & (1u<<27)) &&
                (ecx & (1u<<28)) &&
                (ecx & (1u<<29))) {

                __asm__ __volatile__("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
                                             : "0"(7), "2"(0));

                uint32_t xcr0, dont_need_edx;
                __asm__ __volatile__("xgetbv" : "=a"(xcr0), "=d"(dont_need_edx) : "c"(0));

                if ((xcr0 & (1u<<1)) &&
                    (xcr0 & (1u<<2)) &&
                    (ebx  & (1u<<5))) {

                    if ((xcr0 & (1u<< 5)) &&
                        (xcr0 & (1u<< 6)) &&
                        (xcr0 & (1u<< 7)) &&
                        (ebx  & (1u<<16)) &&
                        (ebx  & (1u<<17)) &&
                        (ebx  & (1u<<28)) &&
                        (ebx  & (1u<<30)) &&
                        (ebx  & (1u<<31))) {
                        return CpuType::SKX;
                    }
                    return CpuType::HSW;
                }
            }
            return CpuType::Baseline;
        }();
        return type;
    #endif
}

static bool tf_is_gamma(const skcms_TransferFunction& tf) {
    return tf.g > 0 && tf.a == 1 &&
           tf.b == 0 && tf.c == 0 && tf.d == 0 && tf.e == 0 && tf.f == 0;
}

struct OpAndArg {
    Op          op;
    const void* arg;
};

static OpAndArg select_curve_op(const skcms_Curve* curve, int channel) {
    struct OpType {
        Op sGamma, sRGBish, PQish, HLGish, HLGinvish, table;
    };
    static constexpr OpType kOps[] = {
        { Op::gamma_r, Op::tf_r, Op::pq_r, Op::hlg_r, Op::hlginv_r, Op::table_r },
        { Op::gamma_g, Op::tf_g, Op::pq_g, Op::hlg_g, Op::hlginv_g, Op::table_g },
        { Op::gamma_b, Op::tf_b, Op::pq_b, Op::hlg_b, Op::hlginv_b, Op::table_b },
        { Op::gamma_a, Op::tf_a, Op::pq_a, Op::hlg_a, Op::hlginv_a, Op::table_a },
    };
    const auto& op = kOps[channel];

    if (curve->table_entries == 0) {
        const OpAndArg noop = { Op::load_a8, nullptr };

        const skcms_TransferFunction& tf = curve->parametric;

        if (tf_is_gamma(tf)) {
            return tf.g != 1 ? OpAndArg{op.sGamma, &tf}
                             : noop;
        }

        switch (classify(tf)) {
            case skcms_TFType_Invalid:    return noop;
            case skcms_TFType_sRGBish:    return OpAndArg{op.sRGBish,   &tf};
            case skcms_TFType_PQish:      return OpAndArg{op.PQish,     &tf};
            case skcms_TFType_HLGish:     return OpAndArg{op.HLGish,    &tf};
            case skcms_TFType_HLGinvish:  return OpAndArg{op.HLGinvish, &tf};
        }
    }
    return OpAndArg{op.table, curve};
}

static int select_curve_ops(const skcms_Curve* curves, int numChannels, OpAndArg* ops) {

    int cursor = 0;
    for (int index = numChannels; index-- > 0; ) {
        ops[cursor] = select_curve_op(&curves[index], index);
        if (ops[cursor].arg) {
            ++cursor;
        }
    }

    if (cursor >= 3) {
        struct FusableOps {
            Op r, g, b, rgb;
        };
        static constexpr FusableOps kFusableOps[] = {
            {Op::gamma_r,  Op::gamma_g,  Op::gamma_b,  Op::gamma_rgb},
            {Op::tf_r,     Op::tf_g,     Op::tf_b,     Op::tf_rgb},
            {Op::pq_r,     Op::pq_g,     Op::pq_b,     Op::pq_rgb},
            {Op::hlg_r,    Op::hlg_g,    Op::hlg_b,    Op::hlg_rgb},
            {Op::hlginv_r, Op::hlginv_g, Op::hlginv_b, Op::hlginv_rgb},
        };

        int posR = cursor - 1;
        int posG = cursor - 2;
        int posB = cursor - 3;
        for (const FusableOps& fusableOp : kFusableOps) {
            if (ops[posR].op == fusableOp.r &&
                ops[posG].op == fusableOp.g &&
                ops[posB].op == fusableOp.b &&
                (0 == memcmp(ops[posR].arg, ops[posG].arg, sizeof(skcms_TransferFunction))) &&
                (0 == memcmp(ops[posR].arg, ops[posB].arg, sizeof(skcms_TransferFunction)))) {

                ops[posB].op = fusableOp.rgb;
                cursor -= 2;
                break;
            }
        }
    }

    return cursor;
}

static size_t bytes_per_pixel(skcms_PixelFormat fmt) {
    switch (fmt >> 1) {
        case skcms_PixelFormat_A_8              >> 1: return  1;
        case skcms_PixelFormat_G_8              >> 1: return  1;
        case skcms_PixelFormat_GA_88            >> 1: return  2;
        case skcms_PixelFormat_ABGR_4444        >> 1: return  2;
        case skcms_PixelFormat_RGB_565          >> 1: return  2;
        case skcms_PixelFormat_RGB_888          >> 1: return  3;
        case skcms_PixelFormat_RGBA_8888        >> 1: return  4;
        case skcms_PixelFormat_RGBA_8888_sRGB   >> 1: return  4;
        case skcms_PixelFormat_RGBA_1010102     >> 1: return  4;
        case skcms_PixelFormat_RGB_101010x_XR   >> 1: return  4;
        case skcms_PixelFormat_RGB_161616LE     >> 1: return  6;
        case skcms_PixelFormat_RGBA_10101010_XR >> 1: return  8;
        case skcms_PixelFormat_RGBA_16161616LE  >> 1: return  8;
        case skcms_PixelFormat_RGB_161616BE     >> 1: return  6;
        case skcms_PixelFormat_RGBA_16161616BE  >> 1: return  8;
        case skcms_PixelFormat_RGB_hhh_Norm     >> 1: return  6;
        case skcms_PixelFormat_RGBA_hhhh_Norm   >> 1: return  8;
        case skcms_PixelFormat_RGB_hhh          >> 1: return  6;
        case skcms_PixelFormat_RGBA_hhhh        >> 1: return  8;
        case skcms_PixelFormat_RGB_fff          >> 1: return 12;
        case skcms_PixelFormat_RGBA_ffff        >> 1: return 16;
    }
    assert(false);
    return 0;
}

static bool prep_for_destination(const skcms_ICCProfile* profile,
                                 skcms_Matrix3x3* fromXYZD50,
                                 skcms_TransferFunction* invR,
                                 skcms_TransferFunction* invG,
                                 skcms_TransferFunction* invB) {

    if (profile->has_B2A) { return true; }

    return profile->has_trc
        && profile->has_toXYZD50
        && profile->trc[0].table_entries == 0
        && profile->trc[1].table_entries == 0
        && profile->trc[2].table_entries == 0
        && skcms_TransferFunction_invert(&profile->trc[0].parametric, invR)
        && skcms_TransferFunction_invert(&profile->trc[1].parametric, invG)
        && skcms_TransferFunction_invert(&profile->trc[2].parametric, invB)
        && skcms_Matrix3x3_invert(&profile->toXYZD50, fromXYZD50);
}

bool skcms_Transform(const void*             src,
                     skcms_PixelFormat       srcFmt,
                     skcms_AlphaFormat       srcAlpha,
                     const skcms_ICCProfile* srcProfile,
                     void*                   dst,
                     skcms_PixelFormat       dstFmt,
                     skcms_AlphaFormat       dstAlpha,
                     const skcms_ICCProfile* dstProfile,
                     size_t                  nz) {
    const size_t dst_bpp = bytes_per_pixel(dstFmt),
                 src_bpp = bytes_per_pixel(srcFmt);

    if (nz * dst_bpp > INT_MAX || nz * src_bpp > INT_MAX) {
        return false;
    }
    int n = (int)nz;

    if (!srcProfile) {
        srcProfile = skcms_sRGB_profile();
    }
    if (!dstProfile) {
        dstProfile = skcms_sRGB_profile();
    }

    if (dst == src && dst_bpp != src_bpp) {
        return false;
    }

    Op          program[32];
    const void* context[32];

    Op*          ops      = program;
    const void** contexts = context;

    auto add_op = [&](Op o) {
        *ops++ = o;
        *contexts++ = nullptr;
    };

    auto add_op_ctx = [&](Op o, const void* c) {
        *ops++ = o;
        *contexts++ = c;
    };

    auto add_curve_ops = [&](const skcms_Curve* curves, int numChannels) {
        OpAndArg oa[4];
        assert(numChannels <= ARRAY_COUNT(oa));

        int numOps = select_curve_ops(curves, numChannels, oa);

        for (int i = 0; i < numOps; ++i) {
            add_op_ctx(oa[i].op, oa[i].arg);
        }
    };

    skcms_Curve dst_curves[3];
    dst_curves[0].table_entries =
    dst_curves[1].table_entries =
    dst_curves[2].table_entries = 0;

    skcms_Matrix3x3        from_xyz;

    switch (srcFmt >> 1) {
        default: return false;
        case skcms_PixelFormat_A_8              >> 1: add_op(Op::load_a8);          break;
        case skcms_PixelFormat_G_8              >> 1: add_op(Op::load_g8);          break;
        case skcms_PixelFormat_GA_88            >> 1: add_op(Op::load_ga88);        break;
        case skcms_PixelFormat_ABGR_4444        >> 1: add_op(Op::load_4444);        break;
        case skcms_PixelFormat_RGB_565          >> 1: add_op(Op::load_565);         break;
        case skcms_PixelFormat_RGB_888          >> 1: add_op(Op::load_888);         break;
        case skcms_PixelFormat_RGBA_8888        >> 1: add_op(Op::load_8888);        break;
        case skcms_PixelFormat_RGBA_1010102     >> 1: add_op(Op::load_1010102);     break;
        case skcms_PixelFormat_RGB_101010x_XR   >> 1: add_op(Op::load_101010x_XR);  break;
        case skcms_PixelFormat_RGBA_10101010_XR >> 1: add_op(Op::load_10101010_XR); break;
        case skcms_PixelFormat_RGB_161616LE     >> 1: add_op(Op::load_161616LE);    break;
        case skcms_PixelFormat_RGBA_16161616LE  >> 1: add_op(Op::load_16161616LE);  break;
        case skcms_PixelFormat_RGB_161616BE     >> 1: add_op(Op::load_161616BE);    break;
        case skcms_PixelFormat_RGBA_16161616BE  >> 1: add_op(Op::load_16161616BE);  break;
        case skcms_PixelFormat_RGB_hhh_Norm     >> 1: add_op(Op::load_hhh);         break;
        case skcms_PixelFormat_RGBA_hhhh_Norm   >> 1: add_op(Op::load_hhhh);        break;
        case skcms_PixelFormat_RGB_hhh          >> 1: add_op(Op::load_hhh);         break;
        case skcms_PixelFormat_RGBA_hhhh        >> 1: add_op(Op::load_hhhh);        break;
        case skcms_PixelFormat_RGB_fff          >> 1: add_op(Op::load_fff);         break;
        case skcms_PixelFormat_RGBA_ffff        >> 1: add_op(Op::load_ffff);        break;

        case skcms_PixelFormat_RGBA_8888_sRGB >> 1:
            add_op(Op::load_8888);
            add_op_ctx(Op::tf_rgb, skcms_sRGB_TransferFunction());
            break;
    }
    if (srcFmt == skcms_PixelFormat_RGB_hhh_Norm ||
        srcFmt == skcms_PixelFormat_RGBA_hhhh_Norm) {
        add_op(Op::clamp);
    }
    if (srcFmt & 1) {
        add_op(Op::swap_rb);
    }
    skcms_ICCProfile gray_dst_profile;
    switch (dstFmt >> 1) {
        case skcms_PixelFormat_G_8:
        case skcms_PixelFormat_GA_88:

            gray_dst_profile = *dstProfile;
            skcms_SetXYZD50(&gray_dst_profile, &skcms_XYZD50_profile()->toXYZD50);
            dstProfile = &gray_dst_profile;
            break;
        default:
            break;
    }

    if (srcProfile->data_color_space == skcms_Signature_CMYK) {

        add_op(Op::invert);

        srcAlpha = skcms_AlphaFormat_Unpremul;
    }

    if (srcAlpha == skcms_AlphaFormat_Opaque) {
        add_op(Op::force_opaque);
    } else if (srcAlpha == skcms_AlphaFormat_PremulAsEncoded) {
        add_op(Op::unpremul);
    }

    if (dstProfile != srcProfile) {

        if (!prep_for_destination(dstProfile,
                                  &from_xyz,
                                  &dst_curves[0].parametric,
                                  &dst_curves[1].parametric,
                                  &dst_curves[2].parametric)) {
            return false;
        }

        if (srcProfile->has_A2B) {
            if (srcProfile->A2B.input_channels) {
                add_curve_ops(srcProfile->A2B.input_curves,
                              (int)srcProfile->A2B.input_channels);
                add_op(Op::clamp);
                add_op_ctx(Op::clut_A2B, &srcProfile->A2B);
            }

            if (srcProfile->A2B.matrix_channels == 3) {
                add_curve_ops(srcProfile->A2B.matrix_curves, 3);

                static const skcms_Matrix3x4 I = {{
                    {1,0,0,0},
                    {0,1,0,0},
                    {0,0,1,0},
                }};
                if (0 != memcmp(&I, &srcProfile->A2B.matrix, sizeof(I))) {
                    add_op_ctx(Op::matrix_3x4, &srcProfile->A2B.matrix);
                }
            }

            if (srcProfile->A2B.output_channels == 3) {
                add_curve_ops(srcProfile->A2B.output_curves, 3);
            }

            if (srcProfile->pcs == skcms_Signature_Lab) {
                add_op(Op::lab_to_xyz);
            }

        } else if (srcProfile->has_trc && srcProfile->has_toXYZD50) {
            add_curve_ops(srcProfile->trc, 3);
        } else {
            return false;
        }

        assert (srcProfile->has_A2B || srcProfile->has_toXYZD50);

        if (dstProfile->has_B2A) {

            if (!srcProfile->has_A2B) {
                add_op_ctx(Op::matrix_3x3, &srcProfile->toXYZD50);
            }

            if (dstProfile->pcs == skcms_Signature_Lab) {
                add_op(Op::xyz_to_lab);
            }

            if (dstProfile->B2A.input_channels == 3) {
                add_curve_ops(dstProfile->B2A.input_curves, 3);
            }

            if (dstProfile->B2A.matrix_channels == 3) {
                static const skcms_Matrix3x4 I = {{
                    {1,0,0,0},
                    {0,1,0,0},
                    {0,0,1,0},
                }};
                if (0 != memcmp(&I, &dstProfile->B2A.matrix, sizeof(I))) {
                    add_op_ctx(Op::matrix_3x4, &dstProfile->B2A.matrix);
                }

                add_curve_ops(dstProfile->B2A.matrix_curves, 3);
            }

            if (dstProfile->B2A.output_channels) {
                add_op(Op::clamp);
                add_op_ctx(Op::clut_B2A, &dstProfile->B2A);

                add_curve_ops(dstProfile->B2A.output_curves,
                              (int)dstProfile->B2A.output_channels);
            }
        } else {

            static const skcms_Matrix3x3 I = {{
                { 1.0f, 0.0f, 0.0f },
                { 0.0f, 1.0f, 0.0f },
                { 0.0f, 0.0f, 1.0f },
            }};
            const skcms_Matrix3x3* to_xyz = srcProfile->has_A2B ? &I : &srcProfile->toXYZD50;

            if (0 != memcmp(&dstProfile->toXYZD50, to_xyz, sizeof(skcms_Matrix3x3))) {

                from_xyz = skcms_Matrix3x3_concat(&from_xyz, to_xyz);
                add_op_ctx(Op::matrix_3x3, &from_xyz);
            }

            OpAndArg oa[3];
            int numOps = select_curve_ops(dst_curves, 3, oa);
            for (int index = 0; index < numOps; ++index) {
                assert(oa[index].op != Op::table_r &&
                       oa[index].op != Op::table_g &&
                       oa[index].op != Op::table_b &&
                       oa[index].op != Op::table_a);
                add_op_ctx(oa[index].op, oa[index].arg);
            }
        }
    }

    if (dstFmt < skcms_PixelFormat_RGB_hhh) {
        add_op(Op::clamp);
    }

    if (dstProfile->data_color_space == skcms_Signature_CMYK) {

        add_op(Op::invert);

        dstAlpha = skcms_AlphaFormat_Unpremul;
    }

    if (dstAlpha == skcms_AlphaFormat_Opaque) {
        add_op(Op::force_opaque);
    } else if (dstAlpha == skcms_AlphaFormat_PremulAsEncoded) {
        add_op(Op::premul);
    }
    if (dstFmt & 1) {
        add_op(Op::swap_rb);
    }
    switch (dstFmt >> 1) {
        default: return false;
        case skcms_PixelFormat_A_8             >> 1: add_op(Op::store_a8);         break;
        case skcms_PixelFormat_G_8             >> 1: add_op(Op::store_g8);         break;
        case skcms_PixelFormat_GA_88           >> 1: add_op(Op::store_ga88);       break;
        case skcms_PixelFormat_ABGR_4444       >> 1: add_op(Op::store_4444);       break;
        case skcms_PixelFormat_RGB_565         >> 1: add_op(Op::store_565);        break;
        case skcms_PixelFormat_RGB_888         >> 1: add_op(Op::store_888);        break;
        case skcms_PixelFormat_RGBA_8888       >> 1: add_op(Op::store_8888);       break;
        case skcms_PixelFormat_RGBA_1010102    >> 1: add_op(Op::store_1010102);    break;
        case skcms_PixelFormat_RGB_161616LE    >> 1: add_op(Op::store_161616LE);   break;
        case skcms_PixelFormat_RGBA_16161616LE >> 1: add_op(Op::store_16161616LE); break;
        case skcms_PixelFormat_RGB_161616BE    >> 1: add_op(Op::store_161616BE);   break;
        case skcms_PixelFormat_RGBA_16161616BE >> 1: add_op(Op::store_16161616BE); break;
        case skcms_PixelFormat_RGB_hhh_Norm    >> 1: add_op(Op::store_hhh);        break;
        case skcms_PixelFormat_RGBA_hhhh_Norm  >> 1: add_op(Op::store_hhhh);       break;
        case skcms_PixelFormat_RGB_101010x_XR  >> 1: add_op(Op::store_101010x_XR); break;
        case skcms_PixelFormat_RGB_hhh         >> 1: add_op(Op::store_hhh);        break;
        case skcms_PixelFormat_RGBA_hhhh       >> 1: add_op(Op::store_hhhh);       break;
        case skcms_PixelFormat_RGB_fff         >> 1: add_op(Op::store_fff);        break;
        case skcms_PixelFormat_RGBA_ffff       >> 1: add_op(Op::store_ffff);       break;

        case skcms_PixelFormat_RGBA_8888_sRGB >> 1:
            add_op_ctx(Op::tf_rgb, skcms_sRGB_Inverse_TransferFunction());
            add_op(Op::store_8888);
            break;
    }

    assert(ops      <= program + ARRAY_COUNT(program));
    assert(contexts <= context + ARRAY_COUNT(context));

    auto run = baseline::run_program;
    switch (cpu_type()) {
        case CpuType::SKX:
            #if !defined(SKCMS_DISABLE_SKX)
                run = skx::run_program;
                break;
            #endif

        case CpuType::HSW:
            #if !defined(SKCMS_DISABLE_HSW)
                run = hsw::run_program;
                break;
            #endif

        case CpuType::Baseline:
            break;
    }

    run(program, context, ops - program, (const char*)src, (char*)dst, n, src_bpp,dst_bpp);
    return true;
}

static void assert_usable_as_destination(const skcms_ICCProfile* profile) {
#if defined(NDEBUG)
    (void)profile;
#else
    skcms_Matrix3x3 fromXYZD50;
    skcms_TransferFunction invR, invG, invB;
    assert(prep_for_destination(profile, &fromXYZD50, &invR, &invG, &invB));
#endif
}

bool skcms_MakeUsableAsDestination(skcms_ICCProfile* profile) {
    if (!profile->has_B2A) {
        skcms_Matrix3x3 fromXYZD50;
        if (!profile->has_trc || !profile->has_toXYZD50
            || !skcms_Matrix3x3_invert(&profile->toXYZD50, &fromXYZD50)) {
            return false;
        }

        skcms_TransferFunction tf[3];
        for (int i = 0; i < 3; i++) {
            skcms_TransferFunction inv;
            if (profile->trc[i].table_entries == 0
                && skcms_TransferFunction_invert(&profile->trc[i].parametric, &inv)) {
                tf[i] = profile->trc[i].parametric;
                continue;
            }

            float max_error;

            if (!skcms_ApproximateCurve(&profile->trc[i], &tf[i], &max_error)) {
                return false;
            }
        }

        for (int i = 0; i < 3; ++i) {
            profile->trc[i].table_entries = 0;
            profile->trc[i].parametric = tf[i];
        }
    }
    assert_usable_as_destination(profile);
    return true;
}

bool skcms_MakeUsableAsDestinationWithSingleCurve(skcms_ICCProfile* profile) {

    skcms_ICCProfile result = *profile;
    result.has_B2A = false;
    if (!skcms_MakeUsableAsDestination(&result)) {
        return false;
    }

    int best_tf = 0;
    float min_max_error = INFINITY_;
    for (int i = 0; i < 3; i++) {
        skcms_TransferFunction inv;
        if (!skcms_TransferFunction_invert(&result.trc[i].parametric, &inv)) {
            return false;
        }

        float err = 0;
        for (int j = 0; j < 3; ++j) {
            err = fmaxf_(err, skcms_MaxRoundtripError(&profile->trc[j], &inv));
        }
        if (min_max_error > err) {
            min_max_error = err;
            best_tf = i;
        }
    }

    for (int i = 0; i < 3; i++) {
        result.trc[i].parametric = result.trc[best_tf].parametric;
    }

    *profile = result;
    assert_usable_as_destination(profile);
    return true;
}

#include <assert.h>
#include <float.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#if defined(__ARM_NEON)
    #include <arm_neon.h>
#elif defined(__SSE__)
    #include <immintrin.h>

    #if defined(__clang__)

        #include <smmintrin.h>
    #endif
#elif defined(__loongarch_sx)
    #include <lsxintrin.h>
#endif

namespace skcms_private {
namespace baseline {

#if defined(SKCMS_PORTABLE)

    #define N 1
    template <typename T> using V = T;
#else

    #define N 4
    template <typename T> using V = skcms_private::Vec<N,T>;
#endif

using F   = V<float>;
using I32 = V<int32_t>;
using U64 = V<uint64_t>;
using U32 = V<uint32_t>;
using U16 = V<uint16_t>;
using U8  = V<uint8_t>;

#if defined(__GNUC__) && !defined(__clang__)

    static constexpr F F0 = F() + 0.0f,
                       F1 = F() + 1.0f,
                       FInfBits = F() + 0x7f800000;
#else
    static constexpr F F0 = 0.0f,
                       F1 = 1.0f,
                       FInfBits = 0x7f800000;
#endif

#if !defined(USING_AVX)      && N == 8 && defined(__AVX__)
    #define  USING_AVX
#endif
#if !defined(USING_AVX_F16C) && defined(USING_AVX) && defined(__F16C__)
    #define  USING_AVX_F16C
#endif
#if !defined(USING_AVX2)     && defined(USING_AVX) && defined(__AVX2__)
    #define  USING_AVX2
#endif
#if !defined(USING_AVX512F)  && N == 16 && defined(__AVX512F__) && defined(__AVX512DQ__)
    #define  USING_AVX512F
#endif

#if N > 1 && defined(__ARM_NEON)
    #define USING_NEON

    #if defined(__clang__)

        #if __ARM_FP & 2
            #define USING_NEON_F16C
        #endif
    #elif defined(__GNUC__)

        #if defined(__ARM_FP16_FORMAT_IEEE)
            #define USING_NEON_F16C
        #endif
    #endif
#endif

#if defined(USING_NEON) && defined(__clang__)
    #pragma clang diagnostic ignored "-Wvector-conversion"
#endif

#if defined(__SSE__) && defined(__GNUC__)
    #if !defined(__has_warning)
        #pragma GCC diagnostic ignored "-Wpsabi"
    #elif __has_warning("-Wpsabi")
        #pragma GCC diagnostic ignored "-Wpsabi"
    #endif
#endif

#if defined(__clang__) || defined(__GNUC__)
    #define SI static inline __attribute__((always_inline))
#else
    #define SI static inline
#endif

template <typename T, typename P>
SI T load(const P* ptr) {
    T val;
    memcpy(&val, ptr, sizeof(val));
    return val;
}
template <typename T, typename P>
SI void store(P* ptr, const T& val) {
    memcpy(ptr, &val, sizeof(val));
}

template <typename D, typename S>
SI D cast(const S& v) {
#if N == 1
    return (D)v;
#elif defined(__clang__)
    return __builtin_convertvector(v, D);
#else
    D d;
    for (int i = 0; i < N; i++) {
        d[i] = v[i];
    }
    return d;
#endif
}

template <typename D, typename S>
SI D bit_pun(const S& v) {
    static_assert(sizeof(D) == sizeof(v), "");
    return load<D>(&v);
}

SI U32 to_fixed(F f) {  return (U32)cast<I32>(f + 0.5f); }

#if N == 1
    #define if_then_else(cond, t, e) ((cond) ? (t) : (e))
#else
    template <typename C, typename T>
    SI T if_then_else(C cond, T t, T e) {
        return bit_pun<T>( ( cond & bit_pun<C>(t)) |
                           (~cond & bit_pun<C>(e)) );
    }
#endif

SI F F_from_Half(U16 half) {
#if defined(USING_NEON_F16C)
    return vcvt_f32_f16((float16x4_t)half);
#elif defined(USING_AVX512F)
    return (F)_mm512_cvtph_ps((__m256i)half);
#elif defined(USING_AVX_F16C)
    typedef int16_t __attribute__((vector_size(16))) I16;
    return __builtin_ia32_vcvtph2ps256((I16)half);
#else
    U32 wide = cast<U32>(half);

    U32 s  = wide & 0x8000,
        em = wide ^ s;

    F norm = bit_pun<F>( (s<<16) + (em<<13) + ((127-15)<<23) );

    return if_then_else(em < 0x0400, F0, norm);
#endif
}

#if defined(__clang__)

    __attribute__((no_sanitize("unsigned-integer-overflow")))
#endif
SI U16 Half_from_F(F f) {
#if defined(USING_NEON_F16C)
    return (U16)vcvt_f16_f32(f);
#elif defined(USING_AVX512F)
    return (U16)_mm512_cvtps_ph((__m512 )f, _MM_FROUND_CUR_DIRECTION );
#elif defined(USING_AVX_F16C)
    return (U16)__builtin_ia32_vcvtps2ph256(f, 0x04);
#else

    U32 sem = bit_pun<U32>(f),
        s   = sem & 0x80000000,
         em = sem ^ s;

    return cast<U16>(if_then_else(em < 0x38800000, (U32)F0
                                                 , (s>>16) + (em>>13) - ((127-15)<<10)));
#endif
}

#if defined(USING_NEON)
    SI U16 swap_endian_16(U16 v) {
        return (U16)vrev16_u8((uint8x8_t) v);
    }
#endif

SI U64 swap_endian_16x4(const U64& rgba) {
    return (rgba & 0x00ff00ff00ff00ff) << 8
         | (rgba & 0xff00ff00ff00ff00) >> 8;
}

#if defined(USING_NEON)
    SI F min_(F x, F y) { return (F)vminq_f32((float32x4_t)x, (float32x4_t)y); }
    SI F max_(F x, F y) { return (F)vmaxq_f32((float32x4_t)x, (float32x4_t)y); }
#elif defined(__loongarch_sx)
    SI F min_(F x, F y) { return (F)__lsx_vfmin_s(x, y); }
    SI F max_(F x, F y) { return (F)__lsx_vfmax_s(x, y); }
#else
    SI F min_(F x, F y) { return if_then_else(x > y, y, x); }
    SI F max_(F x, F y) { return if_then_else(x < y, y, x); }
#endif

SI F floor_(F x) {
#if N == 1
    return floorf_(x);
#elif defined(__aarch64__)
    return vrndmq_f32(x);
#elif defined(USING_AVX512F)

    return _mm512_mask_floor_ps(x, (__mmask16)-1, x);
#elif defined(USING_AVX)
    return __builtin_ia32_roundps256(x, 0x01);
#elif defined(__SSE4_1__)
    return _mm_floor_ps(x);
#elif defined(__loongarch_sx)
    return __lsx_vfrintrm_s((__m128)x);
#else

    F roundtrip = cast<F>(cast<I32>(x));

    return roundtrip - if_then_else(roundtrip > x, F1, F0);

#endif
}

SI F approx_log2(F x) {

    I32 bits = bit_pun<I32>(x);

    F e = cast<F>(bits) * (1.0f / (1<<23));

    F m = bit_pun<F>( (bits & 0x007fffff) | 0x3f000000 );

    return e - 124.225514990f
             -   1.498030302f*m
             -   1.725879990f/(0.3520887068f + m);
}

SI F approx_log(F x) {
    const float ln2 = 0.69314718f;
    return ln2 * approx_log2(x);
}

SI F approx_exp2(F x) {
    F fract = x - floor_(x);

    F fbits = (1.0f * (1<<23)) * (x + 121.274057500f
                                    -   1.490129070f*fract
                                    +  27.728023300f/(4.84252568f - fract));
    I32 bits = cast<I32>(min_(max_(fbits, F0), FInfBits));

    return bit_pun<F>(bits);
}

SI F approx_pow(F x, float y) {
    return if_then_else((x == F0) | (x == F1), x
                                             , approx_exp2(approx_log2(x) * y));
}

SI F approx_exp(F x) {
    const float log2_e = 1.4426950408889634074f;
    return approx_exp2(log2_e * x);
}

SI F strip_sign(F x, U32* sign) {
    U32 bits = bit_pun<U32>(x);
    *sign = bits & 0x80000000;
    return bit_pun<F>(bits ^ *sign);
}

SI F apply_sign(F x, U32 sign) {
    return bit_pun<F>(sign | bit_pun<U32>(x));
}

SI F apply_tf(const skcms_TransferFunction* tf, F x) {

    U32 sign;
    x = strip_sign(x, &sign);

    F v = if_then_else(x < tf->d,            tf->c*x + tf->f
                                , approx_pow(tf->a*x + tf->b, tf->g) + tf->e);

    return apply_sign(v, sign);
}

SI F apply_gamma(const skcms_TransferFunction* tf, F x) {
    U32 sign;
    x = strip_sign(x, &sign);
    return apply_sign(approx_pow(x, tf->g), sign);
}

SI F apply_pq(const skcms_TransferFunction* tf, F x) {
    U32 bits = bit_pun<U32>(x),
        sign = bits & 0x80000000;
    x = bit_pun<F>(bits ^ sign);

    F v = approx_pow(max_(tf->a + tf->b * approx_pow(x, tf->c), F0)
                       / (tf->d + tf->e * approx_pow(x, tf->c)),
                     tf->f);

    return bit_pun<F>(sign | bit_pun<U32>(v));
}

SI F apply_hlg(const skcms_TransferFunction* tf, F x) {
    const float R = tf->a, G = tf->b,
                a = tf->c, b = tf->d, c = tf->e,
                K = tf->f + 1;
    U32 bits = bit_pun<U32>(x),
        sign = bits & 0x80000000;
    x = bit_pun<F>(bits ^ sign);

    F v = if_then_else(x*R <= 1, approx_pow(x*R, G)
                               , approx_exp((x-c)*a) + b);

    return K*bit_pun<F>(sign | bit_pun<U32>(v));
}

SI F apply_hlginv(const skcms_TransferFunction* tf, F x) {
    const float R = tf->a, G = tf->b,
                a = tf->c, b = tf->d, c = tf->e,
                K = tf->f + 1;
    U32 bits = bit_pun<U32>(x),
        sign = bits & 0x80000000;
    x = bit_pun<F>(bits ^ sign);
    x /= K;

    F v = if_then_else(x <= 1, R * approx_pow(x, G)
                             , a * approx_log(x - b) + c);

    return bit_pun<F>(sign | bit_pun<U32>(v));
}

template <typename T, typename P>
SI T load_3(const P* p) {
#if N == 1
    return (T)p[0];
#elif N == 4
    return T{p[ 0],p[ 3],p[ 6],p[ 9]};
#elif N == 8
    return T{p[ 0],p[ 3],p[ 6],p[ 9], p[12],p[15],p[18],p[21]};
#elif N == 16
    return T{p[ 0],p[ 3],p[ 6],p[ 9], p[12],p[15],p[18],p[21],
             p[24],p[27],p[30],p[33], p[36],p[39],p[42],p[45]};
#endif
}

template <typename T, typename P>
SI T load_4(const P* p) {
#if N == 1
    return (T)p[0];
#elif N == 4
    return T{p[ 0],p[ 4],p[ 8],p[12]};
#elif N == 8
    return T{p[ 0],p[ 4],p[ 8],p[12], p[16],p[20],p[24],p[28]};
#elif N == 16
    return T{p[ 0],p[ 4],p[ 8],p[12], p[16],p[20],p[24],p[28],
             p[32],p[36],p[40],p[44], p[48],p[52],p[56],p[60]};
#endif
}

template <typename T, typename P>
SI void store_3(P* p, const T& v) {
#if N == 1
    p[0] = v;
#elif N == 4
    p[ 0] = v[ 0]; p[ 3] = v[ 1]; p[ 6] = v[ 2]; p[ 9] = v[ 3];
#elif N == 8
    p[ 0] = v[ 0]; p[ 3] = v[ 1]; p[ 6] = v[ 2]; p[ 9] = v[ 3];
    p[12] = v[ 4]; p[15] = v[ 5]; p[18] = v[ 6]; p[21] = v[ 7];
#elif N == 16
    p[ 0] = v[ 0]; p[ 3] = v[ 1]; p[ 6] = v[ 2]; p[ 9] = v[ 3];
    p[12] = v[ 4]; p[15] = v[ 5]; p[18] = v[ 6]; p[21] = v[ 7];
    p[24] = v[ 8]; p[27] = v[ 9]; p[30] = v[10]; p[33] = v[11];
    p[36] = v[12]; p[39] = v[13]; p[42] = v[14]; p[45] = v[15];
#endif
}

template <typename T, typename P>
SI void store_4(P* p, const T& v) {
#if N == 1
    p[0] = v;
#elif N == 4
    p[ 0] = v[ 0]; p[ 4] = v[ 1]; p[ 8] = v[ 2]; p[12] = v[ 3];
#elif N == 8
    p[ 0] = v[ 0]; p[ 4] = v[ 1]; p[ 8] = v[ 2]; p[12] = v[ 3];
    p[16] = v[ 4]; p[20] = v[ 5]; p[24] = v[ 6]; p[28] = v[ 7];
#elif N == 16
    p[ 0] = v[ 0]; p[ 4] = v[ 1]; p[ 8] = v[ 2]; p[12] = v[ 3];
    p[16] = v[ 4]; p[20] = v[ 5]; p[24] = v[ 6]; p[28] = v[ 7];
    p[32] = v[ 8]; p[36] = v[ 9]; p[40] = v[10]; p[44] = v[11];
    p[48] = v[12]; p[52] = v[13]; p[56] = v[14]; p[60] = v[15];
#endif
}

SI U8 gather_8(const uint8_t* p, I32 ix) {
#if N == 1
    U8 v = p[ix];
#elif N == 4
    U8 v = { p[ix[0]], p[ix[1]], p[ix[2]], p[ix[3]] };
#elif N == 8
    U8 v = { p[ix[0]], p[ix[1]], p[ix[2]], p[ix[3]],
             p[ix[4]], p[ix[5]], p[ix[6]], p[ix[7]] };
#elif N == 16
    U8 v = { p[ix[ 0]], p[ix[ 1]], p[ix[ 2]], p[ix[ 3]],
             p[ix[ 4]], p[ix[ 5]], p[ix[ 6]], p[ix[ 7]],
             p[ix[ 8]], p[ix[ 9]], p[ix[10]], p[ix[11]],
             p[ix[12]], p[ix[13]], p[ix[14]], p[ix[15]] };
#endif
    return v;
}

SI U16 gather_16(const uint8_t* p, I32 ix) {

    auto load_16 = [p](int i) {
        return load<uint16_t>(p + 2*i);
    };
#if N == 1
    U16 v = load_16(ix);
#elif N == 4
    U16 v = { load_16(ix[0]), load_16(ix[1]), load_16(ix[2]), load_16(ix[3]) };
#elif N == 8
    U16 v = { load_16(ix[0]), load_16(ix[1]), load_16(ix[2]), load_16(ix[3]),
              load_16(ix[4]), load_16(ix[5]), load_16(ix[6]), load_16(ix[7]) };
#elif N == 16
    U16 v = { load_16(ix[ 0]), load_16(ix[ 1]), load_16(ix[ 2]), load_16(ix[ 3]),
              load_16(ix[ 4]), load_16(ix[ 5]), load_16(ix[ 6]), load_16(ix[ 7]),
              load_16(ix[ 8]), load_16(ix[ 9]), load_16(ix[10]), load_16(ix[11]),
              load_16(ix[12]), load_16(ix[13]), load_16(ix[14]), load_16(ix[15]) };
#endif
    return v;
}

SI U32 gather_32(const uint8_t* p, I32 ix) {

    auto load_32 = [p](int i) {
        return load<uint32_t>(p + 4*i);
    };
#if N == 1
    U32 v = load_32(ix);
#elif N == 4
    U32 v = { load_32(ix[0]), load_32(ix[1]), load_32(ix[2]), load_32(ix[3]) };
#elif N == 8
    U32 v = { load_32(ix[0]), load_32(ix[1]), load_32(ix[2]), load_32(ix[3]),
              load_32(ix[4]), load_32(ix[5]), load_32(ix[6]), load_32(ix[7]) };
#elif N == 16
    U32 v = { load_32(ix[ 0]), load_32(ix[ 1]), load_32(ix[ 2]), load_32(ix[ 3]),
              load_32(ix[ 4]), load_32(ix[ 5]), load_32(ix[ 6]), load_32(ix[ 7]),
              load_32(ix[ 8]), load_32(ix[ 9]), load_32(ix[10]), load_32(ix[11]),
              load_32(ix[12]), load_32(ix[13]), load_32(ix[14]), load_32(ix[15]) };
#endif

    return v;
}

SI U32 gather_24(const uint8_t* p, I32 ix) {

    p -= 1;

    auto load_24_32 = [p](int i) {
        return load<uint32_t>(p + 3*i);
    };

#if N == 1
    U32 v = load_24_32(ix);
#elif N == 4
    U32 v = { load_24_32(ix[0]), load_24_32(ix[1]), load_24_32(ix[2]), load_24_32(ix[3]) };
#elif N == 8 && !defined(USING_AVX2)
    U32 v = { load_24_32(ix[0]), load_24_32(ix[1]), load_24_32(ix[2]), load_24_32(ix[3]),
              load_24_32(ix[4]), load_24_32(ix[5]), load_24_32(ix[6]), load_24_32(ix[7]) };
#elif N == 8
    (void)load_24_32;

    const int* p4 = bit_pun<const int*>(p);
    I32 zero = { 0, 0, 0, 0,  0, 0, 0, 0},
        mask = {-1,-1,-1,-1, -1,-1,-1,-1};
    #if defined(__clang__)
        U32 v = (U32)__builtin_ia32_gatherd_d256(zero, p4, 3*ix, mask, 1);
    #elif defined(__GNUC__)
        U32 v = (U32)__builtin_ia32_gathersiv8si(zero, p4, 3*ix, mask, 1);
    #endif
#elif N == 16
    (void)load_24_32;

    const int* p4 = bit_pun<const int*>(p);
    U32 v = (U32)_mm512_i32gather_epi32((__m512i)(3*ix), p4, 1);
#endif

    return v >> 8;
}

#if !defined(__arm__)
    SI void gather_48(const uint8_t* p, I32 ix, U64* v) {

        p -= 2;

        auto load_48_64 = [p](int i) {
            return load<uint64_t>(p + 6*i);
        };

    #if N == 1
        *v = load_48_64(ix);
    #elif N == 4
        *v = U64{
            load_48_64(ix[0]), load_48_64(ix[1]), load_48_64(ix[2]), load_48_64(ix[3]),
        };
    #elif N == 8 && !defined(USING_AVX2)
        *v = U64{
            load_48_64(ix[0]), load_48_64(ix[1]), load_48_64(ix[2]), load_48_64(ix[3]),
            load_48_64(ix[4]), load_48_64(ix[5]), load_48_64(ix[6]), load_48_64(ix[7]),
        };
    #elif N == 8
        (void)load_48_64;
        typedef int32_t   __attribute__((vector_size(16))) Half_I32;
        typedef long long __attribute__((vector_size(32))) Half_I64;

        const long long int* p8 = bit_pun<const long long int*>(p);

        Half_I64 zero = { 0, 0, 0, 0},
                 mask = {-1,-1,-1,-1};

        ix *= 6;
        Half_I32 ix_lo = { ix[0], ix[1], ix[2], ix[3] },
                 ix_hi = { ix[4], ix[5], ix[6], ix[7] };

        #if defined(__clang__)
            Half_I64 lo = (Half_I64)__builtin_ia32_gatherd_q256(zero, p8, ix_lo, mask, 1),
                     hi = (Half_I64)__builtin_ia32_gatherd_q256(zero, p8, ix_hi, mask, 1);
        #elif defined(__GNUC__)
            Half_I64 lo = (Half_I64)__builtin_ia32_gathersiv4di(zero, p8, ix_lo, mask, 1),
                     hi = (Half_I64)__builtin_ia32_gathersiv4di(zero, p8, ix_hi, mask, 1);
        #endif
        store((char*)v +  0, lo);
        store((char*)v + 32, hi);
    #elif N == 16
        (void)load_48_64;
        const long long int* p8 = bit_pun<const long long int*>(p);
        __m512i lo = _mm512_i32gather_epi64(_mm512_extracti32x8_epi32((__m512i)(6*ix), 0), p8, 1),
                hi = _mm512_i32gather_epi64(_mm512_extracti32x8_epi32((__m512i)(6*ix), 1), p8, 1);
        store((char*)v +  0, lo);
        store((char*)v + 64, hi);
    #endif

        *v >>= 16;
    }
#endif

SI F F_from_U8(U8 v) {
    return cast<F>(v) * (1/255.0f);
}

SI F F_from_U16_BE(U16 v) {

    U16 lo = (v >> 8),
        hi = (v << 8) & 0xffff;
    return cast<F>(lo|hi) * (1/65535.0f);
}

SI U16 U16_from_F(F v) {

    return cast<U16>(cast<V<float>>(v) * 65535 + 0.5f);
}

SI F minus_1_ulp(F v) {
    return bit_pun<F>( bit_pun<U32>(v) - 1 );
}

SI F table(const skcms_Curve* curve, F v) {

    F ix = max_(F0, min_(v, F1)) * (float)(curve->table_entries - 1);

    I32 lo = cast<I32>(            ix      ),
        hi = cast<I32>(minus_1_ulp(ix+1.0f));
    F t = ix - cast<F>(lo);

    F l,h;
    if (curve->table_8) {
        l = F_from_U8(gather_8(curve->table_8, lo));
        h = F_from_U8(gather_8(curve->table_8, hi));
    } else {
        l = F_from_U16_BE(gather_16(curve->table_16, lo));
        h = F_from_U16_BE(gather_16(curve->table_16, hi));
    }
    return l + (h-l)*t;
}

SI void sample_clut_8(const uint8_t* grid_8, I32 ix, F* r, F* g, F* b) {
    U32 rgb = gather_24(grid_8, ix);

    *r = cast<F>((rgb >>  0) & 0xff) * (1/255.0f);
    *g = cast<F>((rgb >>  8) & 0xff) * (1/255.0f);
    *b = cast<F>((rgb >> 16) & 0xff) * (1/255.0f);
}

SI void sample_clut_8(const uint8_t* grid_8, I32 ix, F* r, F* g, F* b, F* a) {

    U32 rgba = gather_32(grid_8, ix);

    *r = cast<F>((rgba >>  0) & 0xff) * (1/255.0f);
    *g = cast<F>((rgba >>  8) & 0xff) * (1/255.0f);
    *b = cast<F>((rgba >> 16) & 0xff) * (1/255.0f);
    *a = cast<F>((rgba >> 24) & 0xff) * (1/255.0f);
}

SI void sample_clut_16(const uint8_t* grid_16, I32 ix, F* r, F* g, F* b) {
#if defined(__arm__) || defined(__loongarch_sx)

    *r = F_from_U16_BE(gather_16(grid_16, 3*ix+0));
    *g = F_from_U16_BE(gather_16(grid_16, 3*ix+1));
    *b = F_from_U16_BE(gather_16(grid_16, 3*ix+2));
#else

    U64 rgb;
    gather_48(grid_16, ix, &rgb);
    rgb = swap_endian_16x4(rgb);

    *r = cast<F>((rgb >>  0) & 0xffff) * (1/65535.0f);
    *g = cast<F>((rgb >> 16) & 0xffff) * (1/65535.0f);
    *b = cast<F>((rgb >> 32) & 0xffff) * (1/65535.0f);
#endif
}

SI void sample_clut_16(const uint8_t* grid_16, I32 ix, F* r, F* g, F* b, F* a) {

    *r = F_from_U16_BE(gather_16(grid_16, 4*ix+0));
    *g = F_from_U16_BE(gather_16(grid_16, 4*ix+1));
    *b = F_from_U16_BE(gather_16(grid_16, 4*ix+2));
    *a = F_from_U16_BE(gather_16(grid_16, 4*ix+3));
}

static void clut(uint32_t input_channels, uint32_t output_channels,
                 const uint8_t grid_points[4], const uint8_t* grid_8, const uint8_t* grid_16,
                 F* r, F* g, F* b, F* a) {

    const int dim = (int)input_channels;
    assert (0 < dim && dim <= 4);
    assert (output_channels == 3 ||
            output_channels == 4);

    I32 index [8];
    F   weight[8];

    const F inputs[] = { *r,*g,*b,*a };
    for (int i = dim-1, stride = 1; i >= 0; i--) {

        F x = inputs[i] * (float)(grid_points[i] - 1);

        I32 lo = cast<I32>(            x      ),
            hi = cast<I32>(minus_1_ulp(x+1.0f));

        index[i+0] = lo * stride;
        index[i+4] = hi * stride;
        stride *= grid_points[i];

        F t = x - cast<F>(lo);
        weight[i+0] = 1-t;
        weight[i+4] = t;
    }

    *r = *g = *b = F0;
    if (output_channels == 4) {
        *a = F0;
    }

    for (int combo = 0; combo < (1<<dim); combo++) {

        I32 ix = index [0 + (combo&1)*4];
        F    w = weight[0 + (combo&1)*4];

        switch ((dim-1)&3) {
            case 3: ix += index [3 + (combo&8)/2];
                    w  *= weight[3 + (combo&8)/2];
                    SKCMS_FALLTHROUGH;

            case 2: ix += index [2 + (combo&4)*1];
                    w  *= weight[2 + (combo&4)*1];
                    SKCMS_FALLTHROUGH;

            case 1: ix += index [1 + (combo&2)*2];
                    w  *= weight[1 + (combo&2)*2];
        }

        F R,G,B,A=F0;
        if (output_channels == 3) {
            if (grid_8) { sample_clut_8 (grid_8 ,ix, &R,&G,&B); }
            else        { sample_clut_16(grid_16,ix, &R,&G,&B); }
        } else {
            if (grid_8) { sample_clut_8 (grid_8 ,ix, &R,&G,&B,&A); }
            else        { sample_clut_16(grid_16,ix, &R,&G,&B,&A); }
        }
        *r += w*R;
        *g += w*G;
        *b += w*B;
        *a += w*A;
    }
}

static void clut(const skcms_A2B* a2b, F* r, F* g, F* b, F a) {
    clut(a2b->input_channels, a2b->output_channels,
         a2b->grid_points, a2b->grid_8, a2b->grid_16,
         r,g,b,&a);
}
static void clut(const skcms_B2A* b2a, F* r, F* g, F* b, F* a) {
    clut(b2a->input_channels, b2a->output_channels,
         b2a->grid_points, b2a->grid_8, b2a->grid_16,
         r,g,b,a);
}

struct NoCtx {};

struct Ctx {
    const void* fArg;
    operator NoCtx()                    { return NoCtx{}; }
    template <typename T> operator T*() { return (const T*)fArg; }
};

#define STAGE_PARAMS(MAYBE_REF) SKCMS_MAYBE_UNUSED const char* src, \
                                SKCMS_MAYBE_UNUSED char* dst,       \
                                SKCMS_MAYBE_UNUSED F MAYBE_REF r,   \
                                SKCMS_MAYBE_UNUSED F MAYBE_REF g,   \
                                SKCMS_MAYBE_UNUSED F MAYBE_REF b,   \
                                SKCMS_MAYBE_UNUSED F MAYBE_REF a,   \
                                SKCMS_MAYBE_UNUSED int i

#if SKCMS_HAS_MUSTTAIL

    struct StageList;
    using StageFn = void (*)(StageList stages, const void** ctx, STAGE_PARAMS());
    struct StageList {
        const StageFn* fn;
    };

    #define DECLARE_STAGE(name, arg, CALL_NEXT)                                 \
        SI void Exec_##name##_k(arg, STAGE_PARAMS(&));                          \
                                                                                \
        SI void Exec_##name(StageList list, const void** ctx, STAGE_PARAMS()) { \
            Exec_##name##_k(Ctx{*ctx}, src, dst, r, g, b, a, i);                \
            ++list.fn; ++ctx;                                                   \
            CALL_NEXT;                                                          \
        }                                                                       \
                                                                                \
        SI void Exec_##name##_k(arg, STAGE_PARAMS(&))

    #define STAGE(name, arg)                                                                \
        DECLARE_STAGE(name, arg, [[clang::musttail]] return (*list.fn)(list, ctx, src, dst, \
                                                                       r, g, b, a, i))

    #define FINAL_STAGE(name, arg) \
        DECLARE_STAGE(name, arg, )

#else

    #define DECLARE_STAGE(name, arg)                            \
        SI void Exec_##name##_k(arg, STAGE_PARAMS(&));          \
                                                                \
        SI void Exec_##name(const void* ctx, STAGE_PARAMS(&)) { \
            Exec_##name##_k(Ctx{ctx}, src, dst, r, g, b, a, i); \
        }                                                       \
                                                                \
        SI void Exec_##name##_k(arg, STAGE_PARAMS(&))

    #define STAGE(name, arg)       DECLARE_STAGE(name, arg)
    #define FINAL_STAGE(name, arg) DECLARE_STAGE(name, arg)

#endif

STAGE(load_a8, NoCtx) {
    a = F_from_U8(load<U8>(src + 1*i));
}

STAGE(load_g8, NoCtx) {
    r = g = b = F_from_U8(load<U8>(src + 1*i));
}

STAGE(load_ga88, NoCtx) {
    U16 u16 = load<U16>(src + 2 * i);
    r = g = b = cast<F>((u16 >> 0) & 0xff) * (1 / 255.0f);
            a = cast<F>((u16 >> 8) & 0xff) * (1 / 255.0f);
}

STAGE(load_4444, NoCtx) {
    U16 abgr = load<U16>(src + 2*i);

    r = cast<F>((abgr >> 12) & 0xf) * (1/15.0f);
    g = cast<F>((abgr >>  8) & 0xf) * (1/15.0f);
    b = cast<F>((abgr >>  4) & 0xf) * (1/15.0f);
    a = cast<F>((abgr >>  0) & 0xf) * (1/15.0f);
}

STAGE(load_565, NoCtx) {
    U16 rgb = load<U16>(src + 2*i);

    r = cast<F>(rgb & (uint16_t)(31<< 0)) * (1.0f / (31<< 0));
    g = cast<F>(rgb & (uint16_t)(63<< 5)) * (1.0f / (63<< 5));
    b = cast<F>(rgb & (uint16_t)(31<<11)) * (1.0f / (31<<11));
}

STAGE(load_888, NoCtx) {
    const uint8_t* rgb = (const uint8_t*)(src + 3*i);
#if defined(USING_NEON)

    uint8x8x3_t v = {{ vdup_n_u8(0), vdup_n_u8(0), vdup_n_u8(0) }};
    v = vld3_lane_u8(rgb+0, v, 0);
    v = vld3_lane_u8(rgb+3, v, 2);
    v = vld3_lane_u8(rgb+6, v, 4);
    v = vld3_lane_u8(rgb+9, v, 6);

    r = cast<F>((U16)v.val[0]) * (1/255.0f);
    g = cast<F>((U16)v.val[1]) * (1/255.0f);
    b = cast<F>((U16)v.val[2]) * (1/255.0f);
#else
    r = cast<F>(load_3<U32>(rgb+0) ) * (1/255.0f);
    g = cast<F>(load_3<U32>(rgb+1) ) * (1/255.0f);
    b = cast<F>(load_3<U32>(rgb+2) ) * (1/255.0f);
#endif
}

STAGE(load_8888, NoCtx) {
    U32 rgba = load<U32>(src + 4*i);

    r = cast<F>((rgba >>  0) & 0xff) * (1/255.0f);
    g = cast<F>((rgba >>  8) & 0xff) * (1/255.0f);
    b = cast<F>((rgba >> 16) & 0xff) * (1/255.0f);
    a = cast<F>((rgba >> 24) & 0xff) * (1/255.0f);
}

STAGE(load_1010102, NoCtx) {
    U32 rgba = load<U32>(src + 4*i);

    r = cast<F>((rgba >>  0) & 0x3ff) * (1/1023.0f);
    g = cast<F>((rgba >> 10) & 0x3ff) * (1/1023.0f);
    b = cast<F>((rgba >> 20) & 0x3ff) * (1/1023.0f);
    a = cast<F>((rgba >> 30) & 0x3  ) * (1/   3.0f);
}

STAGE(load_101010x_XR, NoCtx) {
    static constexpr float min = -0.752941f;
    static constexpr float max = 1.25098f;
    static constexpr float range = max - min;
    U32 rgba = load<U32>(src + 4*i);
    r = cast<F>((rgba >>  0) & 0x3ff) * (1/1023.0f) * range + min;
    g = cast<F>((rgba >> 10) & 0x3ff) * (1/1023.0f) * range + min;
    b = cast<F>((rgba >> 20) & 0x3ff) * (1/1023.0f) * range + min;
}

STAGE(load_10101010_XR, NoCtx) {
    static constexpr float min = -0.752941f;
    static constexpr float max = 1.25098f;
    static constexpr float range = max - min;
    U64 rgba = load<U64>(src + 8 * i);
    r = cast<F>((rgba >>  (0+6)) & 0x3ff) * (1/1023.0f) * range + min;
    g = cast<F>((rgba >> (16+6)) & 0x3ff) * (1/1023.0f) * range + min;
    b = cast<F>((rgba >> (32+6)) & 0x3ff) * (1/1023.0f) * range + min;
    a = cast<F>((rgba >> (48+6)) & 0x3ff) * (1/1023.0f) * range + min;
}

STAGE(load_161616LE, NoCtx) {
    uintptr_t ptr = (uintptr_t)(src + 6*i);
    assert( (ptr & 1) == 0 );
    const uint16_t* rgb = (const uint16_t*)ptr;
#if defined(USING_NEON)
    uint16x4x3_t v = vld3_u16(rgb);
    r = cast<F>((U16)v.val[0]) * (1/65535.0f);
    g = cast<F>((U16)v.val[1]) * (1/65535.0f);
    b = cast<F>((U16)v.val[2]) * (1/65535.0f);
#else
    r = cast<F>(load_3<U32>(rgb+0)) * (1/65535.0f);
    g = cast<F>(load_3<U32>(rgb+1)) * (1/65535.0f);
    b = cast<F>(load_3<U32>(rgb+2)) * (1/65535.0f);
#endif
}

STAGE(load_16161616LE, NoCtx) {
    uintptr_t ptr = (uintptr_t)(src + 8*i);
    assert( (ptr & 1) == 0 );
    const uint16_t* rgba = (const uint16_t*)ptr;
#if defined(USING_NEON)
    uint16x4x4_t v = vld4_u16(rgba);
    r = cast<F>((U16)v.val[0]) * (1/65535.0f);
    g = cast<F>((U16)v.val[1]) * (1/65535.0f);
    b = cast<F>((U16)v.val[2]) * (1/65535.0f);
    a = cast<F>((U16)v.val[3]) * (1/65535.0f);
#else
    U64 px = load<U64>(rgba);

    r = cast<F>((px >>  0) & 0xffff) * (1/65535.0f);
    g = cast<F>((px >> 16) & 0xffff) * (1/65535.0f);
    b = cast<F>((px >> 32) & 0xffff) * (1/65535.0f);
    a = cast<F>((px >> 48) & 0xffff) * (1/65535.0f);
#endif
}

STAGE(load_161616BE, NoCtx) {
    uintptr_t ptr = (uintptr_t)(src + 6*i);
    assert( (ptr & 1) == 0 );
    const uint16_t* rgb = (const uint16_t*)ptr;
#if defined(USING_NEON)
    uint16x4x3_t v = vld3_u16(rgb);
    r = cast<F>(swap_endian_16((U16)v.val[0])) * (1/65535.0f);
    g = cast<F>(swap_endian_16((U16)v.val[1])) * (1/65535.0f);
    b = cast<F>(swap_endian_16((U16)v.val[2])) * (1/65535.0f);
#else
    U32 R = load_3<U32>(rgb+0),
        G = load_3<U32>(rgb+1),
        B = load_3<U32>(rgb+2);

    r = cast<F>((R & 0x00ff)<<8 | (R & 0xff00)>>8) * (1/65535.0f);
    g = cast<F>((G & 0x00ff)<<8 | (G & 0xff00)>>8) * (1/65535.0f);
    b = cast<F>((B & 0x00ff)<<8 | (B & 0xff00)>>8) * (1/65535.0f);
#endif
}

STAGE(load_16161616BE, NoCtx) {
    uintptr_t ptr = (uintptr_t)(src + 8*i);
    assert( (ptr & 1) == 0 );
    const uint16_t* rgba = (const uint16_t*)ptr;
#if defined(USING_NEON)
    uint16x4x4_t v = vld4_u16(rgba);
    r = cast<F>(swap_endian_16((U16)v.val[0])) * (1/65535.0f);
    g = cast<F>(swap_endian_16((U16)v.val[1])) * (1/65535.0f);
    b = cast<F>(swap_endian_16((U16)v.val[2])) * (1/65535.0f);
    a = cast<F>(swap_endian_16((U16)v.val[3])) * (1/65535.0f);
#else
    U64 px = swap_endian_16x4(load<U64>(rgba));

    r = cast<F>((px >>  0) & 0xffff) * (1/65535.0f);
    g = cast<F>((px >> 16) & 0xffff) * (1/65535.0f);
    b = cast<F>((px >> 32) & 0xffff) * (1/65535.0f);
    a = cast<F>((px >> 48) & 0xffff) * (1/65535.0f);
#endif
}

STAGE(load_hhh, NoCtx) {
    uintptr_t ptr = (uintptr_t)(src + 6*i);
    assert( (ptr & 1) == 0 );
    const uint16_t* rgb = (const uint16_t*)ptr;
#if defined(USING_NEON)
    uint16x4x3_t v = vld3_u16(rgb);
    U16 R = (U16)v.val[0],
        G = (U16)v.val[1],
        B = (U16)v.val[2];
#else
    U16 R = load_3<U16>(rgb+0),
        G = load_3<U16>(rgb+1),
        B = load_3<U16>(rgb+2);
#endif
    r = F_from_Half(R);
    g = F_from_Half(G);
    b = F_from_Half(B);
}

STAGE(load_hhhh, NoCtx) {
    uintptr_t ptr = (uintptr_t)(src + 8*i);
    assert( (ptr & 1) == 0 );
    const uint16_t* rgba = (const uint16_t*)ptr;
#if defined(USING_NEON)
    uint16x4x4_t v = vld4_u16(rgba);
    U16 R = (U16)v.val[0],
        G = (U16)v.val[1],
        B = (U16)v.val[2],
        A = (U16)v.val[3];
#else
    U64 px = load<U64>(rgba);
    U16 R = cast<U16>((px >>  0) & 0xffff),
        G = cast<U16>((px >> 16) & 0xffff),
        B = cast<U16>((px >> 32) & 0xffff),
        A = cast<U16>((px >> 48) & 0xffff);
#endif
    r = F_from_Half(R);
    g = F_from_Half(G);
    b = F_from_Half(B);
    a = F_from_Half(A);
}

STAGE(load_fff, NoCtx) {
    uintptr_t ptr = (uintptr_t)(src + 12*i);
    assert( (ptr & 3) == 0 );
    const float* rgb = (const float*)ptr;
#if defined(USING_NEON)
    float32x4x3_t v = vld3q_f32(rgb);
    r = (F)v.val[0];
    g = (F)v.val[1];
    b = (F)v.val[2];
#else
    r = load_3<F>(rgb+0);
    g = load_3<F>(rgb+1);
    b = load_3<F>(rgb+2);
#endif
}

STAGE(load_ffff, NoCtx) {
    uintptr_t ptr = (uintptr_t)(src + 16*i);
    assert( (ptr & 3) == 0 );
    const float* rgba = (const float*)ptr;
#if defined(USING_NEON)
    float32x4x4_t v = vld4q_f32(rgba);
    r = (F)v.val[0];
    g = (F)v.val[1];
    b = (F)v.val[2];
    a = (F)v.val[3];
#else
    r = load_4<F>(rgba+0);
    g = load_4<F>(rgba+1);
    b = load_4<F>(rgba+2);
    a = load_4<F>(rgba+3);
#endif
}

STAGE(swap_rb, NoCtx) {
    F t = r;
    r = b;
    b = t;
}

STAGE(clamp, NoCtx) {
    r = max_(F0, min_(r, F1));
    g = max_(F0, min_(g, F1));
    b = max_(F0, min_(b, F1));
    a = max_(F0, min_(a, F1));
}

STAGE(invert, NoCtx) {
    r = F1 - r;
    g = F1 - g;
    b = F1 - b;
    a = F1 - a;
}

STAGE(force_opaque, NoCtx) {
    a = F1;
}

STAGE(premul, NoCtx) {
    r *= a;
    g *= a;
    b *= a;
}

STAGE(unpremul, NoCtx) {
    F scale = if_then_else(F1 / a < INFINITY_, F1 / a, F0);
    r *= scale;
    g *= scale;
    b *= scale;
}

STAGE(matrix_3x3, const skcms_Matrix3x3* matrix) {
    const float* m = &matrix->vals[0][0];

    F R = m[0]*r + m[1]*g + m[2]*b,
      G = m[3]*r + m[4]*g + m[5]*b,
      B = m[6]*r + m[7]*g + m[8]*b;

    r = R;
    g = G;
    b = B;
}

STAGE(matrix_3x4, const skcms_Matrix3x4* matrix) {
    const float* m = &matrix->vals[0][0];

    F R = m[0]*r + m[1]*g + m[ 2]*b + m[ 3],
      G = m[4]*r + m[5]*g + m[ 6]*b + m[ 7],
      B = m[8]*r + m[9]*g + m[10]*b + m[11];

    r = R;
    g = G;
    b = B;
}

STAGE(lab_to_xyz, NoCtx) {

    F L = r * 100.0f,
      A = g * 255.0f - 128.0f,
      B = b * 255.0f - 128.0f;

    F Y = (L + 16.0f) * (1/116.0f),
      X = Y + A*(1/500.0f),
      Z = Y - B*(1/200.0f);

    X = if_then_else(X*X*X > 0.008856f, X*X*X, (X - (16/116.0f)) * (1/7.787f));
    Y = if_then_else(Y*Y*Y > 0.008856f, Y*Y*Y, (Y - (16/116.0f)) * (1/7.787f));
    Z = if_then_else(Z*Z*Z > 0.008856f, Z*Z*Z, (Z - (16/116.0f)) * (1/7.787f));

    r = X * 0.9642f;
    g = Y          ;
    b = Z * 0.8249f;
}

STAGE(xyz_to_lab, NoCtx) {
    F X = r * (1/0.9642f),
      Y = g,
      Z = b * (1/0.8249f);

    X = if_then_else(X > 0.008856f, approx_pow(X, 1/3.0f), X*7.787f + (16/116.0f));
    Y = if_then_else(Y > 0.008856f, approx_pow(Y, 1/3.0f), Y*7.787f + (16/116.0f));
    Z = if_then_else(Z > 0.008856f, approx_pow(Z, 1/3.0f), Z*7.787f + (16/116.0f));

    F L = Y*116.0f - 16.0f,
      A = (X-Y)*500.0f,
      B = (Y-Z)*200.0f;

    r = L * (1/100.f);
    g = (A + 128.0f) * (1/255.0f);
    b = (B + 128.0f) * (1/255.0f);
}

STAGE(gamma_r, const skcms_TransferFunction* tf) { r = apply_gamma(tf, r); }
STAGE(gamma_g, const skcms_TransferFunction* tf) { g = apply_gamma(tf, g); }
STAGE(gamma_b, const skcms_TransferFunction* tf) { b = apply_gamma(tf, b); }
STAGE(gamma_a, const skcms_TransferFunction* tf) { a = apply_gamma(tf, a); }

STAGE(gamma_rgb, const skcms_TransferFunction* tf) {
    r = apply_gamma(tf, r);
    g = apply_gamma(tf, g);
    b = apply_gamma(tf, b);
}

STAGE(tf_r, const skcms_TransferFunction* tf) { r = apply_tf(tf, r); }
STAGE(tf_g, const skcms_TransferFunction* tf) { g = apply_tf(tf, g); }
STAGE(tf_b, const skcms_TransferFunction* tf) { b = apply_tf(tf, b); }
STAGE(tf_a, const skcms_TransferFunction* tf) { a = apply_tf(tf, a); }

STAGE(tf_rgb, const skcms_TransferFunction* tf) {
    r = apply_tf(tf, r);
    g = apply_tf(tf, g);
    b = apply_tf(tf, b);
}

STAGE(pq_r, const skcms_TransferFunction* tf) { r = apply_pq(tf, r); }
STAGE(pq_g, const skcms_TransferFunction* tf) { g = apply_pq(tf, g); }
STAGE(pq_b, const skcms_TransferFunction* tf) { b = apply_pq(tf, b); }
STAGE(pq_a, const skcms_TransferFunction* tf) { a = apply_pq(tf, a); }

STAGE(pq_rgb, const skcms_TransferFunction* tf) {
    r = apply_pq(tf, r);
    g = apply_pq(tf, g);
    b = apply_pq(tf, b);
}

STAGE(hlg_r, const skcms_TransferFunction* tf) { r = apply_hlg(tf, r); }
STAGE(hlg_g, const skcms_TransferFunction* tf) { g = apply_hlg(tf, g); }
STAGE(hlg_b, const skcms_TransferFunction* tf) { b = apply_hlg(tf, b); }
STAGE(hlg_a, const skcms_TransferFunction* tf) { a = apply_hlg(tf, a); }

STAGE(hlg_rgb, const skcms_TransferFunction* tf) {
    r = apply_hlg(tf, r);
    g = apply_hlg(tf, g);
    b = apply_hlg(tf, b);
}

STAGE(hlginv_r, const skcms_TransferFunction* tf) { r = apply_hlginv(tf, r); }
STAGE(hlginv_g, const skcms_TransferFunction* tf) { g = apply_hlginv(tf, g); }
STAGE(hlginv_b, const skcms_TransferFunction* tf) { b = apply_hlginv(tf, b); }
STAGE(hlginv_a, const skcms_TransferFunction* tf) { a = apply_hlginv(tf, a); }

STAGE(hlginv_rgb, const skcms_TransferFunction* tf) {
    r = apply_hlginv(tf, r);
    g = apply_hlginv(tf, g);
    b = apply_hlginv(tf, b);
}

STAGE(table_r, const skcms_Curve* curve) { r = table(curve, r); }
STAGE(table_g, const skcms_Curve* curve) { g = table(curve, g); }
STAGE(table_b, const skcms_Curve* curve) { b = table(curve, b); }
STAGE(table_a, const skcms_Curve* curve) { a = table(curve, a); }

STAGE(clut_A2B, const skcms_A2B* a2b) {
    clut(a2b, &r,&g,&b,a);

    if (a2b->input_channels == 4) {

        a = F1;
    }
}

STAGE(clut_B2A, const skcms_B2A* b2a) {
    clut(b2a, &r,&g,&b,&a);
}

FINAL_STAGE(store_a8, NoCtx) {
    store(dst + 1*i, cast<U8>(to_fixed(a * 255)));
}

FINAL_STAGE(store_g8, NoCtx) {

    store(dst + 1*i, cast<U8>(to_fixed(g * 255)));
}

FINAL_STAGE(store_ga88, NoCtx) {

    store<U16>(dst + 2*i, cast<U16>(to_fixed(g * 255) << 0 )
                        | cast<U16>(to_fixed(a * 255) << 8 ));
}

FINAL_STAGE(store_4444, NoCtx) {
    store<U16>(dst + 2*i, cast<U16>(to_fixed(r * 15) << 12)
                        | cast<U16>(to_fixed(g * 15) <<  8)
                        | cast<U16>(to_fixed(b * 15) <<  4)
                        | cast<U16>(to_fixed(a * 15) <<  0));
}

FINAL_STAGE(store_565, NoCtx) {
    store<U16>(dst + 2*i, cast<U16>(to_fixed(r * 31) <<  0 )
                        | cast<U16>(to_fixed(g * 63) <<  5 )
                        | cast<U16>(to_fixed(b * 31) << 11 ));
}

FINAL_STAGE(store_888, NoCtx) {
    uint8_t* rgb = (uint8_t*)dst + 3*i;
#if defined(USING_NEON)

    U16 R = cast<U16>(to_fixed(r * 255)),
        G = cast<U16>(to_fixed(g * 255)),
        B = cast<U16>(to_fixed(b * 255));

    uint8x8x3_t v = {{ (uint8x8_t)R, (uint8x8_t)G, (uint8x8_t)B }};
    vst3_lane_u8(rgb+0, v, 0);
    vst3_lane_u8(rgb+3, v, 2);
    vst3_lane_u8(rgb+6, v, 4);
    vst3_lane_u8(rgb+9, v, 6);
#else
    store_3(rgb+0, cast<U8>(to_fixed(r * 255)) );
    store_3(rgb+1, cast<U8>(to_fixed(g * 255)) );
    store_3(rgb+2, cast<U8>(to_fixed(b * 255)) );
#endif
}

FINAL_STAGE(store_8888, NoCtx) {
    store(dst + 4*i, cast<U32>(to_fixed(r * 255)) <<  0
                   | cast<U32>(to_fixed(g * 255)) <<  8
                   | cast<U32>(to_fixed(b * 255)) << 16
                   | cast<U32>(to_fixed(a * 255)) << 24);
}

FINAL_STAGE(store_101010x_XR, NoCtx) {
    static constexpr float min = -0.752941f;
    static constexpr float max = 1.25098f;
    static constexpr float range = max - min;
    store(dst + 4*i, cast<U32>(to_fixed(((r - min) / range) * 1023)) <<  0
                   | cast<U32>(to_fixed(((g - min) / range) * 1023)) << 10
                   | cast<U32>(to_fixed(((b - min) / range) * 1023)) << 20);
}

FINAL_STAGE(store_1010102, NoCtx) {
    store(dst + 4*i, cast<U32>(to_fixed(r * 1023)) <<  0
                   | cast<U32>(to_fixed(g * 1023)) << 10
                   | cast<U32>(to_fixed(b * 1023)) << 20
                   | cast<U32>(to_fixed(a *    3)) << 30);
}

FINAL_STAGE(store_161616LE, NoCtx) {
    uintptr_t ptr = (uintptr_t)(dst + 6*i);
    assert( (ptr & 1) == 0 );
    uint16_t* rgb = (uint16_t*)ptr;
#if defined(USING_NEON)
    uint16x4x3_t v = {{
        (uint16x4_t)U16_from_F(r),
        (uint16x4_t)U16_from_F(g),
        (uint16x4_t)U16_from_F(b),
    }};
    vst3_u16(rgb, v);
#else
    store_3(rgb+0, U16_from_F(r));
    store_3(rgb+1, U16_from_F(g));
    store_3(rgb+2, U16_from_F(b));
#endif

}

FINAL_STAGE(store_16161616LE, NoCtx) {
    uintptr_t ptr = (uintptr_t)(dst + 8*i);
    assert( (ptr & 1) == 0 );
    uint16_t* rgba = (uint16_t*)ptr;
#if defined(USING_NEON)
    uint16x4x4_t v = {{
        (uint16x4_t)U16_from_F(r),
        (uint16x4_t)U16_from_F(g),
        (uint16x4_t)U16_from_F(b),
        (uint16x4_t)U16_from_F(a),
    }};
    vst4_u16(rgba, v);
#else
    U64 px = cast<U64>(to_fixed(r * 65535)) <<  0
           | cast<U64>(to_fixed(g * 65535)) << 16
           | cast<U64>(to_fixed(b * 65535)) << 32
           | cast<U64>(to_fixed(a * 65535)) << 48;
    store(rgba, px);
#endif
}

FINAL_STAGE(store_161616BE, NoCtx) {
    uintptr_t ptr = (uintptr_t)(dst + 6*i);
    assert( (ptr & 1) == 0 );
    uint16_t* rgb = (uint16_t*)ptr;
#if defined(USING_NEON)
    uint16x4x3_t v = {{
        (uint16x4_t)swap_endian_16(cast<U16>(U16_from_F(r))),
        (uint16x4_t)swap_endian_16(cast<U16>(U16_from_F(g))),
        (uint16x4_t)swap_endian_16(cast<U16>(U16_from_F(b))),
    }};
    vst3_u16(rgb, v);
#else
    U32 R = to_fixed(r * 65535),
        G = to_fixed(g * 65535),
        B = to_fixed(b * 65535);
    store_3(rgb+0, cast<U16>((R & 0x00ff) << 8 | (R & 0xff00) >> 8) );
    store_3(rgb+1, cast<U16>((G & 0x00ff) << 8 | (G & 0xff00) >> 8) );
    store_3(rgb+2, cast<U16>((B & 0x00ff) << 8 | (B & 0xff00) >> 8) );
#endif

}

FINAL_STAGE(store_16161616BE, NoCtx) {
    uintptr_t ptr = (uintptr_t)(dst + 8*i);
    assert( (ptr & 1) == 0 );
    uint16_t* rgba = (uint16_t*)ptr;
#if defined(USING_NEON)
    uint16x4x4_t v = {{
        (uint16x4_t)swap_endian_16(cast<U16>(U16_from_F(r))),
        (uint16x4_t)swap_endian_16(cast<U16>(U16_from_F(g))),
        (uint16x4_t)swap_endian_16(cast<U16>(U16_from_F(b))),
        (uint16x4_t)swap_endian_16(cast<U16>(U16_from_F(a))),
    }};
    vst4_u16(rgba, v);
#else
    U64 px = cast<U64>(to_fixed(r * 65535)) <<  0
           | cast<U64>(to_fixed(g * 65535)) << 16
           | cast<U64>(to_fixed(b * 65535)) << 32
           | cast<U64>(to_fixed(a * 65535)) << 48;
    store(rgba, swap_endian_16x4(px));
#endif
}

FINAL_STAGE(store_hhh, NoCtx) {
    uintptr_t ptr = (uintptr_t)(dst + 6*i);
    assert( (ptr & 1) == 0 );
    uint16_t* rgb = (uint16_t*)ptr;

    U16 R = Half_from_F(r),
        G = Half_from_F(g),
        B = Half_from_F(b);
#if defined(USING_NEON)
    uint16x4x3_t v = {{
        (uint16x4_t)R,
        (uint16x4_t)G,
        (uint16x4_t)B,
    }};
    vst3_u16(rgb, v);
#else
    store_3(rgb+0, R);
    store_3(rgb+1, G);
    store_3(rgb+2, B);
#endif
}

FINAL_STAGE(store_hhhh, NoCtx) {
    uintptr_t ptr = (uintptr_t)(dst + 8*i);
    assert( (ptr & 1) == 0 );
    uint16_t* rgba = (uint16_t*)ptr;

    U16 R = Half_from_F(r),
        G = Half_from_F(g),
        B = Half_from_F(b),
        A = Half_from_F(a);
#if defined(USING_NEON)
    uint16x4x4_t v = {{
        (uint16x4_t)R,
        (uint16x4_t)G,
        (uint16x4_t)B,
        (uint16x4_t)A,
    }};
    vst4_u16(rgba, v);
#else
    store(rgba, cast<U64>(R) <<  0
              | cast<U64>(G) << 16
              | cast<U64>(B) << 32
              | cast<U64>(A) << 48);
#endif
}

FINAL_STAGE(store_fff, NoCtx) {
    uintptr_t ptr = (uintptr_t)(dst + 12*i);
    assert( (ptr & 3) == 0 );
    float* rgb = (float*)ptr;
#if defined(USING_NEON)
    float32x4x3_t v = {{
        (float32x4_t)r,
        (float32x4_t)g,
        (float32x4_t)b,
    }};
    vst3q_f32(rgb, v);
#else
    store_3(rgb+0, r);
    store_3(rgb+1, g);
    store_3(rgb+2, b);
#endif
}

FINAL_STAGE(store_ffff, NoCtx) {
    uintptr_t ptr = (uintptr_t)(dst + 16*i);
    assert( (ptr & 3) == 0 );
    float* rgba = (float*)ptr;
#if defined(USING_NEON)
    float32x4x4_t v = {{
        (float32x4_t)r,
        (float32x4_t)g,
        (float32x4_t)b,
        (float32x4_t)a,
    }};
    vst4q_f32(rgba, v);
#else
    store_4(rgba+0, r);
    store_4(rgba+1, g);
    store_4(rgba+2, b);
    store_4(rgba+3, a);
#endif
}

#if SKCMS_HAS_MUSTTAIL

    SI void exec_stages(StageFn* stages, const void** contexts, const char* src, char* dst, int i) {
        (*stages)({stages}, contexts, src, dst, F0, F0, F0, F1, i);
    }

#else

    static void exec_stages(const Op* ops, const void** contexts,
                            const char* src, char* dst, int i) {
        F r = F0, g = F0, b = F0, a = F1;
        while (true) {
            switch (*ops++) {
#define M(name) case Op::name: Exec_##name(*contexts++, src, dst, r, g, b, a, i); break;
                SKCMS_WORK_OPS(M)
#undef M
#define M(name) case Op::name: Exec_##name(*contexts++, src, dst, r, g, b, a, i); return;
                SKCMS_STORE_OPS(M)
#undef M
            }
        }
    }

#endif

void run_program(const Op* program, const void** contexts, SKCMS_MAYBE_UNUSED ptrdiff_t programSize,
                 const char* src, char* dst, int n,
                 const size_t src_bpp, const size_t dst_bpp) {
#if SKCMS_HAS_MUSTTAIL

    StageFn stages[32];
    assert(programSize <= ARRAY_COUNT(stages));

    static constexpr StageFn kStageFns[] = {
#define M(name) &Exec_##name,
        SKCMS_WORK_OPS(M)
        SKCMS_STORE_OPS(M)
#undef M
    };

    for (ptrdiff_t index = 0; index < programSize; ++index) {
        stages[index] = kStageFns[(int)program[index]];
    }
#else

    const Op* stages = program;
#endif

    int i = 0;
    while (n >= N) {
        exec_stages(stages, contexts, src, dst, i);
        i += N;
        n -= N;
    }
    if (n > 0) {
        char tmp[4*4*N] = {0};

        memcpy(tmp, (const char*)src + (size_t)i*src_bpp, (size_t)n*src_bpp);
        exec_stages(stages, contexts, tmp, tmp, 0);
        memcpy((char*)dst + (size_t)i*dst_bpp, tmp, (size_t)n*dst_bpp);
    }
}

}
}
