#ifndef SUMATRA_A_SKCMS_H
#define SUMATRA_A_SKCMS_H

#ifndef SKCMS_API
    #define SKCMS_API
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct skcms_Matrix3x3 {
    float vals[3][3];
} skcms_Matrix3x3;

SKCMS_API bool            skcms_Matrix3x3_invert(const skcms_Matrix3x3*, skcms_Matrix3x3*);
SKCMS_API skcms_Matrix3x3 skcms_Matrix3x3_concat(const skcms_Matrix3x3*, const skcms_Matrix3x3*);

typedef struct skcms_Matrix3x4 {
    float vals[3][4];
} skcms_Matrix3x4;

typedef struct skcms_TransferFunction {
    float g, a,b,c,d,e,f;
} skcms_TransferFunction;

SKCMS_API float skcms_TransferFunction_eval  (const skcms_TransferFunction*, float);
SKCMS_API bool  skcms_TransferFunction_invert(const skcms_TransferFunction*,
                                              skcms_TransferFunction*);

typedef enum skcms_TFType {
    skcms_TFType_Invalid,
    skcms_TFType_sRGBish,
    skcms_TFType_PQish,
    skcms_TFType_HLGish,
    skcms_TFType_HLGinvish,
} skcms_TFType;

SKCMS_API skcms_TFType skcms_TransferFunction_getType(const skcms_TransferFunction*);

SKCMS_API bool skcms_TransferFunction_makePQish(skcms_TransferFunction*,
                                                float A, float B, float C,
                                                float D, float E, float F);

SKCMS_API bool skcms_TransferFunction_makeScaledHLGish(skcms_TransferFunction*,
                                                       float K, float R, float G,
                                                       float a, float b, float c);

static inline bool skcms_TransferFunction_makeHLGish(skcms_TransferFunction* fn,
                                                     float R, float G,
                                                     float a, float b, float c) {
    return skcms_TransferFunction_makeScaledHLGish(fn, 1.0f, R,G, a,b,c);
}

static inline bool skcms_TransferFunction_makePQ(skcms_TransferFunction* tf) {
    return skcms_TransferFunction_makePQish(tf, -107/128.0f,         1.0f,   32/2523.0f
                                              , 2413/128.0f, -2392/128.0f, 8192/1305.0f);
}

static inline bool skcms_TransferFunction_makeHLG(skcms_TransferFunction* tf) {
    return skcms_TransferFunction_makeHLGish(tf, 2.0f, 2.0f
                                               , 1/0.17883277f, 0.28466892f, 0.55991073f);
}

SKCMS_API bool skcms_TransferFunction_isSRGBish(const skcms_TransferFunction*);
SKCMS_API bool skcms_TransferFunction_isPQish  (const skcms_TransferFunction*);
SKCMS_API bool skcms_TransferFunction_isHLGish (const skcms_TransferFunction*);

typedef union skcms_Curve {
    struct {

        uint32_t alias_of_table_entries;
        skcms_TransferFunction parametric;
    };
    struct {
        uint32_t table_entries;
        const uint8_t* table_8;
        const uint8_t* table_16;
    };
} skcms_Curve;

typedef struct skcms_A2B {

    skcms_Curve     input_curves[4];
    const uint8_t*  grid_8;
    const uint8_t*  grid_16;
    uint32_t        input_channels;
    uint8_t         grid_points[4];

    skcms_Curve     matrix_curves[3];
    skcms_Matrix3x4 matrix;
    uint32_t        matrix_channels;

    uint32_t        output_channels;
    skcms_Curve     output_curves[3];
} skcms_A2B;

typedef struct skcms_B2A {

    skcms_Curve     input_curves[3];
    uint32_t        input_channels;

    uint32_t        matrix_channels;
    skcms_Curve     matrix_curves[3];
    skcms_Matrix3x4 matrix;

    skcms_Curve     output_curves[4];
    const uint8_t*  grid_8;
    const uint8_t*  grid_16;
    uint8_t         grid_points[4];
    uint32_t        output_channels;
} skcms_B2A;

typedef struct skcms_CICP {
    uint8_t color_primaries;
    uint8_t transfer_characteristics;
    uint8_t matrix_coefficients;
    uint8_t video_full_range_flag;
} skcms_CICP;

typedef struct skcms_ICCProfile {
    const uint8_t* buffer;

    uint32_t size;
    uint32_t data_color_space;
    uint32_t pcs;
    uint32_t tag_count;

    skcms_Curve            trc[3];

    skcms_Matrix3x3        toXYZD50;

    skcms_A2B              A2B;

    skcms_B2A              B2A;

    skcms_CICP             CICP;

    bool                   has_trc;
    bool                   has_toXYZD50;
    bool                   has_A2B;
    bool                   has_B2A;
    bool                   has_CICP;
} skcms_ICCProfile;

SKCMS_API const skcms_ICCProfile* skcms_sRGB_profile(void);

SKCMS_API const skcms_ICCProfile* skcms_XYZD50_profile(void);

SKCMS_API const skcms_TransferFunction* skcms_sRGB_TransferFunction(void);
SKCMS_API const skcms_TransferFunction* skcms_sRGB_Inverse_TransferFunction(void);
SKCMS_API const skcms_TransferFunction* skcms_Identity_TransferFunction(void);

SKCMS_API bool skcms_ApproximatelyEqualProfiles(const skcms_ICCProfile* A,
                                                const skcms_ICCProfile* B);

SKCMS_API bool skcms_AreApproximateInverses(const skcms_Curve* curve,
                                            const skcms_TransferFunction* inv_tf);

SKCMS_API bool skcms_TRCs_AreApproximateInverse(const skcms_ICCProfile* profile,
                                                const skcms_TransferFunction* inv_tf);

SKCMS_API bool skcms_ParseWithA2BPriority(const void*, size_t,
                                          const int priority[], int priorities,
                                          skcms_ICCProfile*);

static inline bool skcms_Parse(const void* buf, size_t len, skcms_ICCProfile* profile) {

    const int priority[] = {0,1};
    return skcms_ParseWithA2BPriority(buf, len,
                                      priority, sizeof(priority)/sizeof(*priority),
                                      profile);
}

SKCMS_API bool skcms_ApproximateCurve(const skcms_Curve* curve,
                                      skcms_TransferFunction* approx,
                                      float* max_error);

SKCMS_API bool skcms_GetCHAD(const skcms_ICCProfile*, skcms_Matrix3x3*);
SKCMS_API bool skcms_GetWTPT(const skcms_ICCProfile*, float xyz[3]);

SKCMS_API int skcms_GetInputChannelCount(const skcms_ICCProfile*);

enum {

    skcms_Signature_CMYK = 0x434D594B,
    skcms_Signature_Gray = 0x47524159,
    skcms_Signature_RGB  = 0x52474220,

    skcms_Signature_Lab  = 0x4C616220,
    skcms_Signature_XYZ  = 0x58595A20,

    skcms_Signature_CIELUV = 0x4C757620,
    skcms_Signature_YCbCr  = 0x59436272,
    skcms_Signature_CIEYxy = 0x59787920,
    skcms_Signature_HSV    = 0x48535620,
    skcms_Signature_HLS    = 0x484C5320,
    skcms_Signature_CMY    = 0x434D5920,
    skcms_Signature_2CLR   = 0x32434C52,
    skcms_Signature_3CLR   = 0x33434C52,
    skcms_Signature_4CLR   = 0x34434C52,
    skcms_Signature_5CLR   = 0x35434C52,
    skcms_Signature_6CLR   = 0x36434C52,
    skcms_Signature_7CLR   = 0x37434C52,
    skcms_Signature_8CLR   = 0x38434C52,
    skcms_Signature_9CLR   = 0x39434C52,
    skcms_Signature_10CLR  = 0x41434C52,
    skcms_Signature_11CLR  = 0x42434C52,
    skcms_Signature_12CLR  = 0x43434C52,
    skcms_Signature_13CLR  = 0x44434C52,
    skcms_Signature_14CLR  = 0x45434C52,
    skcms_Signature_15CLR  = 0x46434C52,
};

typedef enum skcms_PixelFormat {
    skcms_PixelFormat_A_8,
    skcms_PixelFormat_A_8_,
    skcms_PixelFormat_G_8,
    skcms_PixelFormat_G_8_,
    skcms_PixelFormat_GA_88,
    skcms_PixelFormat_GA_88_,

    skcms_PixelFormat_RGB_565,
    skcms_PixelFormat_BGR_565,

    skcms_PixelFormat_ABGR_4444,
    skcms_PixelFormat_ARGB_4444,

    skcms_PixelFormat_RGB_888,
    skcms_PixelFormat_BGR_888,
    skcms_PixelFormat_RGBA_8888,
    skcms_PixelFormat_BGRA_8888,
    skcms_PixelFormat_RGBA_8888_sRGB,
    skcms_PixelFormat_BGRA_8888_sRGB,

    skcms_PixelFormat_RGBA_1010102,
    skcms_PixelFormat_BGRA_1010102,

    skcms_PixelFormat_RGB_161616LE,
    skcms_PixelFormat_BGR_161616LE,
    skcms_PixelFormat_RGBA_16161616LE,
    skcms_PixelFormat_BGRA_16161616LE,

    skcms_PixelFormat_RGB_161616BE,
    skcms_PixelFormat_BGR_161616BE,
    skcms_PixelFormat_RGBA_16161616BE,
    skcms_PixelFormat_BGRA_16161616BE,

    skcms_PixelFormat_RGB_hhh_Norm,
    skcms_PixelFormat_BGR_hhh_Norm,
    skcms_PixelFormat_RGBA_hhhh_Norm,
    skcms_PixelFormat_BGRA_hhhh_Norm,

    skcms_PixelFormat_RGB_hhh,
    skcms_PixelFormat_BGR_hhh,
    skcms_PixelFormat_RGBA_hhhh,
    skcms_PixelFormat_BGRA_hhhh,

    skcms_PixelFormat_RGB_fff,
    skcms_PixelFormat_BGR_fff,
    skcms_PixelFormat_RGBA_ffff,
    skcms_PixelFormat_BGRA_ffff,

    skcms_PixelFormat_RGB_101010x_XR,
    skcms_PixelFormat_BGR_101010x_XR,
    skcms_PixelFormat_RGBA_10101010_XR,
    skcms_PixelFormat_BGRA_10101010_XR,
} skcms_PixelFormat;

typedef enum skcms_AlphaFormat {
    skcms_AlphaFormat_Opaque,

    skcms_AlphaFormat_Unpremul,

    skcms_AlphaFormat_PremulAsEncoded,

} skcms_AlphaFormat;

SKCMS_API bool skcms_Transform(const void*             src,
                               skcms_PixelFormat       srcFmt,
                               skcms_AlphaFormat       srcAlpha,
                               const skcms_ICCProfile* srcProfile,
                               void*                   dst,
                               skcms_PixelFormat       dstFmt,
                               skcms_AlphaFormat       dstAlpha,
                               const skcms_ICCProfile* dstProfile,
                               size_t                  npixels);

SKCMS_API bool skcms_MakeUsableAsDestination(skcms_ICCProfile* profile);

SKCMS_API bool skcms_MakeUsableAsDestinationWithSingleCurve(skcms_ICCProfile* profile);

SKCMS_API bool skcms_AdaptToXYZD50(float wx, float wy,
                                   skcms_Matrix3x3* toXYZD50);

SKCMS_API bool skcms_PrimariesToXYZD50(float rx, float ry,
                                       float gx, float gy,
                                       float bx, float by,
                                       float wx, float wy,
                                       skcms_Matrix3x3* toXYZD50);

SKCMS_API void skcms_DisableRuntimeCPUDetection(void);

static inline void skcms_Init(skcms_ICCProfile* p) {
    memset(p, 0, sizeof(*p));
    p->data_color_space = skcms_Signature_RGB;
    p->pcs = skcms_Signature_XYZ;
}

static inline void skcms_SetTransferFunction(skcms_ICCProfile* p,
                                             const skcms_TransferFunction* tf) {
    p->has_trc = true;
    for (int i = 0; i < 3; ++i) {
        p->trc[i].table_entries = 0;
        p->trc[i].parametric = *tf;
    }
}

static inline void skcms_SetXYZD50(skcms_ICCProfile* p, const skcms_Matrix3x3* m) {
    p->has_toXYZD50 = true;
    p->toXYZD50 = *m;
}

#ifdef __cplusplus
}
#endif

#endif
