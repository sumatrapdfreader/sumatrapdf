//---------------------------------------------------------------------------------
//
//  Little Color Management System
//  Copyright (c) 1998-2021 Marti Maria Saguer
//
// Permission is hereby granted, free of charge, to any person obtaining
// a copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the Software
// is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
//---------------------------------------------------------------------------------
//
// Version 2.13alpha
//

#ifndef _lcms2mt_H

// ********** Configuration toggles ****************************************

// Uncomment this one if you are using big endian machines
// #define CMS_USE_BIG_ENDIAN   1

// Uncomment this one if your compiler/machine does NOT support the
// "long long" type.
// #define CMS_DONT_USE_INT64        1

// Uncomment this if your compiler doesn't work with fast floor function
// #define CMS_DONT_USE_FAST_FLOOR 1

// Uncomment this line if you want lcms to use the black point tag in profile,
// if commented, lcms will compute the black point by its own.
// It is safer to leave it commented out
// #define CMS_USE_PROFILE_BLACK_POINT_TAG    1

// Uncomment this line if you are compiling as C++ and want a C++ API
// #define CMS_USE_CPP_API

// Uncomment this line if you need strict CGATS syntax. Makes CGATS files to
// require "KEYWORD" on undefined identifiers, keep it commented out unless needed
// #define CMS_STRICT_CGATS  1

// Uncomment to get rid of the tables for "half" float support
// #define CMS_NO_HALF_SUPPORT 1

// Uncomment to get rid of pthreads/windows dependency
// #define CMS_NO_PTHREADS  1

// Uncomment this for special windows mutex initialization (see lcms2_internal.h)
// #define CMS_RELY_ON_WINDOWS_STATIC_MUTEX_INIT

// Uncomment this to remove the "register" storage class
// #define CMS_NO_REGISTER_KEYWORD 1

// ********** End of configuration toggles ******************************

// Needed for streams
#include <stdio.h>

// Needed for portability (C99 per 7.1.2)
#include <limits.h>
#include <time.h>
#include <stddef.h>

#ifndef CMS_USE_CPP_API
#   ifdef __cplusplus
extern "C" {
#   endif
#endif

// Version/release
// Vanilla LCMS2 uses values from 2000-2120. This is
// used as an unsigned number. We want any attempt to
// use OUR numbers with a mainline LCMS to fail, so
// we have to go under 2000-2100. Let's subtract
// 2000 from the mainline release.
#define LCMS_VERSION              (2120 - 2000)

// We expect any LCMS2MT release to fall within the
// following range.
#define LCMS2MT_VERSION_MIN (0)
#define LCMS2MT_VERSION_MAX (999)

// I will give the chance of redefining basic types for compilers that are not fully C99 compliant
#ifndef CMS_BASIC_TYPES_ALREADY_DEFINED

// Base types
typedef unsigned char        cmsUInt8Number;   // That is guaranteed by the C99 spec
typedef signed char          cmsInt8Number;    // That is guaranteed by the C99 spec

#if CHAR_BIT != 8
#  error "Unable to find 8 bit type, unsupported compiler"
#endif

// IEEE float storage numbers
typedef float                cmsFloat32Number;
typedef double               cmsFloat64Number;

// 16-bit base types
#if (USHRT_MAX == 65535U)
 typedef unsigned short      cmsUInt16Number;
#elif (UINT_MAX == 65535U)
 typedef unsigned int        cmsUInt16Number;
#else
#  error "Unable to find 16 bits unsigned type, unsupported compiler"
#endif

#if (SHRT_MAX == 32767)
  typedef  short             cmsInt16Number;
#elif (INT_MAX == 32767)
  typedef  int               cmsInt16Number;
#else
#  error "Unable to find 16 bits signed type, unsupported compiler"
#endif

// 32-bit base type
#if (UINT_MAX == 4294967295U)
 typedef unsigned int        cmsUInt32Number;
#elif (ULONG_MAX == 4294967295U)
 typedef unsigned long       cmsUInt32Number;
#else
#  error "Unable to find 32 bit unsigned type, unsupported compiler"
#endif

#if (INT_MAX == +2147483647)
 typedef  int                cmsInt32Number;
#elif (LONG_MAX == +2147483647)
 typedef  long               cmsInt32Number;
#else
#  error "Unable to find 32 bit signed type, unsupported compiler"
#endif

// 64-bit base types
#ifndef CMS_DONT_USE_INT64
#  if (ULONG_MAX  == 18446744073709551615U)
    typedef unsigned long   cmsUInt64Number;
#  elif (ULLONG_MAX == 18446744073709551615U)
      typedef unsigned long long   cmsUInt64Number;
#  else
#     define CMS_DONT_USE_INT64 1
#  endif
#  if (LONG_MAX == +9223372036854775807)
      typedef  long          cmsInt64Number;
#  elif (LLONG_MAX == +9223372036854775807)
      typedef  long long     cmsInt64Number;
#  else
#     define CMS_DONT_USE_INT64 1
#  endif
#endif
#endif

// Handle "register" keyword
#if defined(CMS_NO_REGISTER_KEYWORD) && !defined(CMS_DLL) && !defined(CMS_DLL_BUILD)
#  define CMSREGISTER
#else
#  define CMSREGISTER register
#endif

// In the case 64 bit numbers are not supported by the compiler
#ifdef CMS_DONT_USE_INT64
    typedef cmsUInt32Number      cmsUInt64Number[2];
    typedef cmsInt32Number       cmsInt64Number[2];
#endif

// Derivative types
typedef cmsUInt32Number      cmsSignature;
typedef cmsUInt16Number      cmsU8Fixed8Number;
typedef cmsInt32Number       cmsS15Fixed16Number;
typedef cmsUInt32Number      cmsU16Fixed16Number;

// Boolean type, which will be using the native integer
typedef int                  cmsBool;

// Try to detect windows
#if defined (_WIN32) || defined(_WIN64) || defined(WIN32) || defined(_WIN32_)
#  define CMS_IS_WINDOWS_ 1
#endif

#ifdef _MSC_VER
#  define CMS_IS_WINDOWS_ 1
#endif

#ifdef __BORLANDC__
#  define CMS_IS_WINDOWS_ 1
#endif

// Try to detect big endian platforms. This list can be endless, so primarily rely on the configure script
// on Unix-like systems, and allow it to be set on the compiler command line using
// -DCMS_USE_BIG_ENDIAN or something similar
#ifdef CMS_USE_BIG_ENDIAN // set at compiler command line takes overall precedence

#  if CMS_USE_BIG_ENDIAN == 0
#    undef CMS_USE_BIG_ENDIAN
#  endif

#else // CMS_USE_BIG_ENDIAN

#  ifdef WORDS_BIGENDIAN // set by configure (or explicitly on compiler command line)
#    define CMS_USE_BIG_ENDIAN 1
#  else // WORDS_BIGENDIAN
// Fall back to platform/compiler specific tests
#    if defined(__sgi__) || defined(__sgi) || defined(sparc)
#      define CMS_USE_BIG_ENDIAN      1
#    endif

#    if defined(__s390__) || defined(__s390x__)
#      define CMS_USE_BIG_ENDIAN   1
#    endif

#    ifdef macintosh
#      ifdef __BIG_ENDIAN__
#        define CMS_USE_BIG_ENDIAN      1
#      endif
#      ifdef __LITTLE_ENDIAN__
#        undef CMS_USE_BIG_ENDIAN
#      endif
#    endif
#  endif  // WORDS_BIGENDIAN

#  if defined(_HOST_BIG_ENDIAN) || defined(__BIG_ENDIAN__)
#    define CMS_USE_BIG_ENDIAN      1
#  endif

#endif  // CMS_USE_BIG_ENDIAN


// Calling convention -- this is hardly platform and compiler dependent
#ifdef CMS_IS_WINDOWS_
#  if defined(CMS_DLL) || defined(CMS_DLL_BUILD)
#     ifdef __BORLANDC__
#        define CMSEXPORT       __stdcall _export
#        define CMSAPI
#     else
#        define CMSEXPORT      __stdcall
#        ifdef CMS_DLL_BUILD
#            define CMSAPI    __declspec(dllexport)
#        else
#           define CMSAPI     __declspec(dllimport)
#       endif
#     endif
#  else
#       define CMSEXPORT
#       define CMSAPI
#  endif
#else  // not Windows
#  ifdef HAVE_FUNC_ATTRIBUTE_VISIBILITY
#     define CMSEXPORT
#     define CMSAPI    __attribute__((visibility("default")))
#else
# define CMSEXPORT
# define CMSAPI
#endif
#endif  // CMS_IS_WINDOWS_

#ifdef HasTHREADS
# if HasTHREADS == 1
#    undef CMS_NO_PTHREADS
# else
#    define CMS_NO_PTHREADS 1
# endif
#endif

// Some common definitions
#define cmsMAX_PATH     256

#ifndef FALSE
#       define FALSE 0
#endif
#ifndef TRUE
#       define TRUE  1
#endif

// D50 XYZ normalized to Y=1.0
#define cmsD50X  0.9642
#define cmsD50Y  1.0
#define cmsD50Z  0.8249

// V4 perceptual black
#define cmsPERCEPTUAL_BLACK_X  0.00336
#define cmsPERCEPTUAL_BLACK_Y  0.0034731
#define cmsPERCEPTUAL_BLACK_Z  0.00287

// Definitions in ICC spec
#define cmsMagicNumber  0x61637370     // 'acsp'
#define lcmsSignature   0x6c636d73     // 'lcms'


// Base ICC type definitions
typedef enum {
    cmsSigChromaticityType                  = 0x6368726D,  // 'chrm'
    cmsSigColorantOrderType                 = 0x636C726F,  // 'clro'
    cmsSigColorantTableType                 = 0x636C7274,  // 'clrt'
    cmsSigCrdInfoType                       = 0x63726469,  // 'crdi'
    cmsSigCurveType                         = 0x63757276,  // 'curv'
    cmsSigDataType                          = 0x64617461,  // 'data'
    cmsSigDictType                          = 0x64696374,  // 'dict'
    cmsSigDateTimeType                      = 0x6474696D,  // 'dtim'
    cmsSigDeviceSettingsType                = 0x64657673,  // 'devs'
    cmsSigLut16Type                         = 0x6d667432,  // 'mft2'
    cmsSigLut8Type                          = 0x6d667431,  // 'mft1'
    cmsSigLutAtoBType                       = 0x6d414220,  // 'mAB '
    cmsSigLutBtoAType                       = 0x6d424120,  // 'mBA '
    cmsSigMeasurementType                   = 0x6D656173,  // 'meas'
    cmsSigMultiLocalizedUnicodeType         = 0x6D6C7563,  // 'mluc'
    cmsSigMultiProcessElementType           = 0x6D706574,  // 'mpet'
    cmsSigNamedColorType                    = 0x6E636f6C,  // 'ncol' -- DEPRECATED!
    cmsSigNamedColor2Type                   = 0x6E636C32,  // 'ncl2'
    cmsSigParametricCurveType               = 0x70617261,  // 'para'
    cmsSigProfileSequenceDescType           = 0x70736571,  // 'pseq'
    cmsSigProfileSequenceIdType             = 0x70736964,  // 'psid'
    cmsSigResponseCurveSet16Type            = 0x72637332,  // 'rcs2'
    cmsSigS15Fixed16ArrayType               = 0x73663332,  // 'sf32'
    cmsSigScreeningType                     = 0x7363726E,  // 'scrn'
    cmsSigSignatureType                     = 0x73696720,  // 'sig '
    cmsSigTextType                          = 0x74657874,  // 'text'
    cmsSigTextDescriptionType               = 0x64657363,  // 'desc'
    cmsSigU16Fixed16ArrayType               = 0x75663332,  // 'uf32'
    cmsSigUcrBgType                         = 0x62666420,  // 'bfd '
    cmsSigUInt16ArrayType                   = 0x75693136,  // 'ui16'
    cmsSigUInt32ArrayType                   = 0x75693332,  // 'ui32'
    cmsSigUInt64ArrayType                   = 0x75693634,  // 'ui64'
    cmsSigUInt8ArrayType                    = 0x75693038,  // 'ui08'
    cmsSigVcgtType                          = 0x76636774,  // 'vcgt'
    cmsSigViewingConditionsType             = 0x76696577,  // 'view'
    cmsSigXYZType                           = 0x58595A20   // 'XYZ '


} cmsTagTypeSignature;

// Base ICC tag definitions
typedef enum {
    cmsSigAToB0Tag                          = 0x41324230,  // 'A2B0'
    cmsSigAToB1Tag                          = 0x41324231,  // 'A2B1'
    cmsSigAToB2Tag                          = 0x41324232,  // 'A2B2'
    cmsSigBlueColorantTag                   = 0x6258595A,  // 'bXYZ'
    cmsSigBlueMatrixColumnTag               = 0x6258595A,  // 'bXYZ'
    cmsSigBlueTRCTag                        = 0x62545243,  // 'bTRC'
    cmsSigBToA0Tag                          = 0x42324130,  // 'B2A0'
    cmsSigBToA1Tag                          = 0x42324131,  // 'B2A1'
    cmsSigBToA2Tag                          = 0x42324132,  // 'B2A2'
    cmsSigCalibrationDateTimeTag            = 0x63616C74,  // 'calt'
    cmsSigCharTargetTag                     = 0x74617267,  // 'targ'
    cmsSigChromaticAdaptationTag            = 0x63686164,  // 'chad'
    cmsSigChromaticityTag                   = 0x6368726D,  // 'chrm'
    cmsSigColorantOrderTag                  = 0x636C726F,  // 'clro'
    cmsSigColorantTableTag                  = 0x636C7274,  // 'clrt'
    cmsSigColorantTableOutTag               = 0x636C6F74,  // 'clot'
    cmsSigColorimetricIntentImageStateTag   = 0x63696973,  // 'ciis'
    cmsSigCopyrightTag                      = 0x63707274,  // 'cprt'
    cmsSigCrdInfoTag                        = 0x63726469,  // 'crdi'
    cmsSigDataTag                           = 0x64617461,  // 'data'
    cmsSigDateTimeTag                       = 0x6474696D,  // 'dtim'
    cmsSigDeviceMfgDescTag                  = 0x646D6E64,  // 'dmnd'
    cmsSigDeviceModelDescTag                = 0x646D6464,  // 'dmdd'
    cmsSigDeviceSettingsTag                 = 0x64657673,  // 'devs'
    cmsSigDToB0Tag                          = 0x44324230,  // 'D2B0'
    cmsSigDToB1Tag                          = 0x44324231,  // 'D2B1'
    cmsSigDToB2Tag                          = 0x44324232,  // 'D2B2'
    cmsSigDToB3Tag                          = 0x44324233,  // 'D2B3'
    cmsSigBToD0Tag                          = 0x42324430,  // 'B2D0'
    cmsSigBToD1Tag                          = 0x42324431,  // 'B2D1'
    cmsSigBToD2Tag                          = 0x42324432,  // 'B2D2'
    cmsSigBToD3Tag                          = 0x42324433,  // 'B2D3'
    cmsSigGamutTag                          = 0x67616D74,  // 'gamt'
    cmsSigGrayTRCTag                        = 0x6b545243,  // 'kTRC'
    cmsSigGreenColorantTag                  = 0x6758595A,  // 'gXYZ'
    cmsSigGreenMatrixColumnTag              = 0x6758595A,  // 'gXYZ'
    cmsSigGreenTRCTag                       = 0x67545243,  // 'gTRC'
    cmsSigLuminanceTag                      = 0x6C756d69,  // 'lumi'
    cmsSigMeasurementTag                    = 0x6D656173,  // 'meas'
    cmsSigMediaBlackPointTag                = 0x626B7074,  // 'bkpt'
    cmsSigMediaWhitePointTag                = 0x77747074,  // 'wtpt'
    cmsSigNamedColorTag                     = 0x6E636f6C,  // 'ncol' // Deprecated by the ICC
    cmsSigNamedColor2Tag                    = 0x6E636C32,  // 'ncl2'
    cmsSigOutputResponseTag                 = 0x72657370,  // 'resp'
    cmsSigPerceptualRenderingIntentGamutTag = 0x72696730,  // 'rig0'
    cmsSigPreview0Tag                       = 0x70726530,  // 'pre0'
    cmsSigPreview1Tag                       = 0x70726531,  // 'pre1'
    cmsSigPreview2Tag                       = 0x70726532,  // 'pre2'
    cmsSigProfileDescriptionTag             = 0x64657363,  // 'desc'
    cmsSigProfileDescriptionMLTag           = 0x6473636d,  // 'dscm'
    cmsSigProfileSequenceDescTag            = 0x70736571,  // 'pseq'
    cmsSigProfileSequenceIdTag              = 0x70736964,  // 'psid'
    cmsSigPs2CRD0Tag                        = 0x70736430,  // 'psd0'
    cmsSigPs2CRD1Tag                        = 0x70736431,  // 'psd1'
    cmsSigPs2CRD2Tag                        = 0x70736432,  // 'psd2'
    cmsSigPs2CRD3Tag                        = 0x70736433,  // 'psd3'
    cmsSigPs2CSATag                         = 0x70733273,  // 'ps2s'
    cmsSigPs2RenderingIntentTag             = 0x70733269,  // 'ps2i'
    cmsSigRedColorantTag                    = 0x7258595A,  // 'rXYZ'
    cmsSigRedMatrixColumnTag                = 0x7258595A,  // 'rXYZ'
    cmsSigRedTRCTag                         = 0x72545243,  // 'rTRC'
    cmsSigSaturationRenderingIntentGamutTag = 0x72696732,  // 'rig2'
    cmsSigScreeningDescTag                  = 0x73637264,  // 'scrd'
    cmsSigScreeningTag                      = 0x7363726E,  // 'scrn'
    cmsSigTechnologyTag                     = 0x74656368,  // 'tech'
    cmsSigUcrBgTag                          = 0x62666420,  // 'bfd '
    cmsSigViewingCondDescTag                = 0x76756564,  // 'vued'
    cmsSigViewingConditionsTag              = 0x76696577,  // 'view'
    cmsSigVcgtTag                           = 0x76636774,  // 'vcgt'
    cmsSigMetaTag                           = 0x6D657461,  // 'meta'
    cmsSigArgyllArtsTag                     = 0x61727473   // 'arts'

} cmsTagSignature;


// ICC Technology tag
typedef enum {
    cmsSigDigitalCamera                     = 0x6463616D,  // 'dcam'
    cmsSigFilmScanner                       = 0x6673636E,  // 'fscn'
    cmsSigReflectiveScanner                 = 0x7273636E,  // 'rscn'
    cmsSigInkJetPrinter                     = 0x696A6574,  // 'ijet'
    cmsSigThermalWaxPrinter                 = 0x74776178,  // 'twax'
    cmsSigElectrophotographicPrinter        = 0x6570686F,  // 'epho'
    cmsSigElectrostaticPrinter              = 0x65737461,  // 'esta'
    cmsSigDyeSublimationPrinter             = 0x64737562,  // 'dsub'
    cmsSigPhotographicPaperPrinter          = 0x7270686F,  // 'rpho'
    cmsSigFilmWriter                        = 0x6670726E,  // 'fprn'
    cmsSigVideoMonitor                      = 0x7669646D,  // 'vidm'
    cmsSigVideoCamera                       = 0x76696463,  // 'vidc'
    cmsSigProjectionTelevision              = 0x706A7476,  // 'pjtv'
    cmsSigCRTDisplay                        = 0x43525420,  // 'CRT '
    cmsSigPMDisplay                         = 0x504D4420,  // 'PMD '
    cmsSigAMDisplay                         = 0x414D4420,  // 'AMD '
    cmsSigPhotoCD                           = 0x4B504344,  // 'KPCD'
    cmsSigPhotoImageSetter                  = 0x696D6773,  // 'imgs'
    cmsSigGravure                           = 0x67726176,  // 'grav'
    cmsSigOffsetLithography                 = 0x6F666673,  // 'offs'
    cmsSigSilkscreen                        = 0x73696C6B,  // 'silk'
    cmsSigFlexography                       = 0x666C6578,  // 'flex'
    cmsSigMotionPictureFilmScanner          = 0x6D706673,  // 'mpfs'
    cmsSigMotionPictureFilmRecorder         = 0x6D706672,  // 'mpfr'
    cmsSigDigitalMotionPictureCamera        = 0x646D7063,  // 'dmpc'
    cmsSigDigitalCinemaProjector            = 0x64636A70   // 'dcpj'

} cmsTechnologySignature;


// ICC Color spaces
typedef enum {
    cmsSigXYZData                           = 0x58595A20,  // 'XYZ '
    cmsSigLabData                           = 0x4C616220,  // 'Lab '
    cmsSigLuvData                           = 0x4C757620,  // 'Luv '
    cmsSigYCbCrData                         = 0x59436272,  // 'YCbr'
    cmsSigYxyData                           = 0x59787920,  // 'Yxy '
    cmsSigRgbData                           = 0x52474220,  // 'RGB '
    cmsSigGrayData                          = 0x47524159,  // 'GRAY'
    cmsSigHsvData                           = 0x48535620,  // 'HSV '
    cmsSigHlsData                           = 0x484C5320,  // 'HLS '
    cmsSigCmykData                          = 0x434D594B,  // 'CMYK'
    cmsSigCmyData                           = 0x434D5920,  // 'CMY '
    cmsSigMCH1Data                          = 0x4D434831,  // 'MCH1'
    cmsSigMCH2Data                          = 0x4D434832,  // 'MCH2'
    cmsSigMCH3Data                          = 0x4D434833,  // 'MCH3'
    cmsSigMCH4Data                          = 0x4D434834,  // 'MCH4'
    cmsSigMCH5Data                          = 0x4D434835,  // 'MCH5'
    cmsSigMCH6Data                          = 0x4D434836,  // 'MCH6'
    cmsSigMCH7Data                          = 0x4D434837,  // 'MCH7'
    cmsSigMCH8Data                          = 0x4D434838,  // 'MCH8'
    cmsSigMCH9Data                          = 0x4D434839,  // 'MCH9'
    cmsSigMCHAData                          = 0x4D434841,  // 'MCHA'
    cmsSigMCHBData                          = 0x4D434842,  // 'MCHB'
    cmsSigMCHCData                          = 0x4D434843,  // 'MCHC'
    cmsSigMCHDData                          = 0x4D434844,  // 'MCHD'
    cmsSigMCHEData                          = 0x4D434845,  // 'MCHE'
    cmsSigMCHFData                          = 0x4D434846,  // 'MCHF'
    cmsSigNamedData                         = 0x6e6d636c,  // 'nmcl'
    cmsSig1colorData                        = 0x31434C52,  // '1CLR'
    cmsSig2colorData                        = 0x32434C52,  // '2CLR'
    cmsSig3colorData                        = 0x33434C52,  // '3CLR'
    cmsSig4colorData                        = 0x34434C52,  // '4CLR'
    cmsSig5colorData                        = 0x35434C52,  // '5CLR'
    cmsSig6colorData                        = 0x36434C52,  // '6CLR'
    cmsSig7colorData                        = 0x37434C52,  // '7CLR'
    cmsSig8colorData                        = 0x38434C52,  // '8CLR'
    cmsSig9colorData                        = 0x39434C52,  // '9CLR'
    cmsSig10colorData                       = 0x41434C52,  // 'ACLR'
    cmsSig11colorData                       = 0x42434C52,  // 'BCLR'
    cmsSig12colorData                       = 0x43434C52,  // 'CCLR'
    cmsSig13colorData                       = 0x44434C52,  // 'DCLR'
    cmsSig14colorData                       = 0x45434C52,  // 'ECLR'
    cmsSig15colorData                       = 0x46434C52,  // 'FCLR'
    cmsSigLuvKData                          = 0x4C75764B   // 'LuvK'

} cmsColorSpaceSignature;

// ICC Profile Class
typedef enum {
    cmsSigInputClass                        = 0x73636E72,  // 'scnr'
    cmsSigDisplayClass                      = 0x6D6E7472,  // 'mntr'
    cmsSigOutputClass                       = 0x70727472,  // 'prtr'
    cmsSigLinkClass                         = 0x6C696E6B,  // 'link'
    cmsSigAbstractClass                     = 0x61627374,  // 'abst'
    cmsSigColorSpaceClass                   = 0x73706163,  // 'spac'
    cmsSigNamedColorClass                   = 0x6e6d636c   // 'nmcl'

} cmsProfileClassSignature;

// ICC Platforms
typedef enum {
    cmsSigMacintosh                         = 0x4150504C,  // 'APPL'
    cmsSigMicrosoft                         = 0x4D534654,  // 'MSFT'
    cmsSigSolaris                           = 0x53554E57,  // 'SUNW'
    cmsSigSGI                               = 0x53474920,  // 'SGI '
    cmsSigTaligent                          = 0x54474E54,  // 'TGNT'
    cmsSigUnices                            = 0x2A6E6978   // '*nix'   // From argyll -- Not official

} cmsPlatformSignature;

// Reference gamut
#define  cmsSigPerceptualReferenceMediumGamut         0x70726d67  //'prmg'

// For cmsSigColorimetricIntentImageStateTag
#define  cmsSigSceneColorimetryEstimates              0x73636F65  //'scoe'
#define  cmsSigSceneAppearanceEstimates               0x73617065  //'sape'
#define  cmsSigFocalPlaneColorimetryEstimates         0x66706365  //'fpce'
#define  cmsSigReflectionHardcopyOriginalColorimetry  0x72686F63  //'rhoc'
#define  cmsSigReflectionPrintOutputColorimetry       0x72706F63  //'rpoc'

// Multi process elements types
typedef enum {
    cmsSigCurveSetElemType              = 0x63767374,  //'cvst'
    cmsSigMatrixElemType                = 0x6D617466,  //'matf'
    cmsSigCLutElemType                  = 0x636C7574,  //'clut'

    cmsSigBAcsElemType                  = 0x62414353,  // 'bACS'
    cmsSigEAcsElemType                  = 0x65414353,  // 'eACS'

    // Custom from here, not in the ICC Spec
    cmsSigXYZ2LabElemType               = 0x6C327820,  // 'l2x '
    cmsSigLab2XYZElemType               = 0x78326C20,  // 'x2l '
    cmsSigNamedColorElemType            = 0x6E636C20,  // 'ncl '
    cmsSigLabV2toV4                     = 0x32203420,  // '2 4 '
    cmsSigLabV4toV2                     = 0x34203220,  // '4 2 '

    // Identities
    cmsSigIdentityElemType              = 0x69646E20,  // 'idn '

    // Float to floatPCS
    cmsSigLab2FloatPCS                  = 0x64326C20,  // 'd2l '
    cmsSigFloatPCS2Lab                  = 0x6C326420,  // 'l2d '
    cmsSigXYZ2FloatPCS                  = 0x64327820,  // 'd2x '
    cmsSigFloatPCS2XYZ                  = 0x78326420,  // 'x2d '
    cmsSigClipNegativesElemType         = 0x636c7020   // 'clp '

} cmsStageSignature;

// Types of CurveElements
typedef enum {

    cmsSigFormulaCurveSeg               = 0x70617266, // 'parf'
    cmsSigSampledCurveSeg               = 0x73616D66, // 'samf'
    cmsSigSegmentedCurve                = 0x63757266  // 'curf'

} cmsCurveSegSignature;

// Used in ResponseCurveType
#define  cmsSigStatusA                    0x53746141 //'StaA'
#define  cmsSigStatusE                    0x53746145 //'StaE'
#define  cmsSigStatusI                    0x53746149 //'StaI'
#define  cmsSigStatusT                    0x53746154 //'StaT'
#define  cmsSigStatusM                    0x5374614D //'StaM'
#define  cmsSigDN                         0x444E2020 //'DN  '
#define  cmsSigDNP                        0x444E2050 //'DN P'
#define  cmsSigDNN                        0x444E4E20 //'DNN '
#define  cmsSigDNNP                       0x444E4E50 //'DNNP'

// Device attributes, currently defined values correspond to the low 4 bytes
// of the 8 byte attribute quantity
#define cmsReflective     0
#define cmsTransparency   1
#define cmsGlossy         0
#define cmsMatte          2

// Common structures in ICC tags
typedef struct {
    cmsUInt32Number len;
    cmsUInt32Number flag;
    cmsUInt8Number  data[1];

} cmsICCData;

// ICC date time
typedef struct {
    cmsUInt16Number      year;
    cmsUInt16Number      month;
    cmsUInt16Number      day;
    cmsUInt16Number      hours;
    cmsUInt16Number      minutes;
    cmsUInt16Number      seconds;

} cmsDateTimeNumber;

// ICC XYZ
typedef struct {
    cmsS15Fixed16Number  X;
    cmsS15Fixed16Number  Y;
    cmsS15Fixed16Number  Z;

} cmsEncodedXYZNumber;


// Profile ID as computed by MD5 algorithm
typedef union {
    cmsUInt8Number       ID8[16];
    cmsUInt16Number      ID16[8];
    cmsUInt32Number      ID32[4];

} cmsProfileID;


// ----------------------------------------------------------------------------------------------
// ICC profile internal base types. Strictly, shouldn't be declared in this header, but maybe
// somebody want to use this info for accessing profile header directly, so here it is.

// Profile header -- it is 32-bit aligned, so no issues are expected on alignment
typedef struct {
    cmsUInt32Number              size;           // Profile size in bytes
    cmsSignature                 cmmId;          // CMM for this profile
    cmsUInt32Number              version;        // Format version number
    cmsProfileClassSignature     deviceClass;    // Type of profile
    cmsColorSpaceSignature       colorSpace;     // Color space of data
    cmsColorSpaceSignature       pcs;            // PCS, XYZ or Lab only
    cmsDateTimeNumber            date;           // Date profile was created
    cmsSignature                 magic;          // Magic Number to identify an ICC profile
    cmsPlatformSignature         platform;       // Primary Platform
    cmsUInt32Number              flags;          // Various bit settings
    cmsSignature                 manufacturer;   // Device manufacturer
    cmsUInt32Number              model;          // Device model number
    cmsUInt64Number              attributes;     // Device attributes
    cmsUInt32Number              renderingIntent;// Rendering intent
    cmsEncodedXYZNumber          illuminant;     // Profile illuminant
    cmsSignature                 creator;        // Profile creator
    cmsProfileID                 profileID;      // Profile ID using MD5
    cmsInt8Number                reserved[28];   // Reserved for future use

} cmsICCHeader;

// ICC base tag
typedef struct {
    cmsTagTypeSignature  sig;
    cmsInt8Number        reserved[4];

} cmsTagBase;

// A tag entry in directory
typedef struct {
    cmsTagSignature      sig;            // The tag signature
    cmsUInt32Number      offset;         // Start of tag
    cmsUInt32Number      size;           // Size in bytes

} cmsTagEntry;

// ----------------------------------------------------------------------------------------------

// Little CMS specific typedefs

typedef void* cmsHANDLE ;              // Generic handle
typedef void* cmsHPROFILE;             // Opaque typedefs to hide internals
typedef void* cmsHTRANSFORM;

#define cmsMAXCHANNELS  16             // Maximum number of channels in ICC profiles
#define cmsMAXEXTRACHANNELS  (63+cmsMAXCHANNELS)        // Maximum number of channels + 'extra' channels supported in links

// Format of pixel is defined by one cmsUInt32Number, using bit fields as follows
//
//                        2  2222 1111  1111 11
//                        4  3210 9876  5432 1098  7654 3210
//                        E  EEEE EAOT  TTTT YFPX  SCCC CBBB
//
//            E: Extra samples
//            A: Floating point -- With this flag we can differentiate 16 bits as float and as int
//            O: Optimized -- previous optimization already returns the final 8-bit value
//            T: Pixeltype
//            F: Flavor  0=MinIsBlack(Chocolate) 1=MinIsWhite(Vanilla)
//            P: Planar? 0=Chunky, 1=Planar
//            X: swap 16 bps endianness?
//            S: Do swap? ie, BGR, KYMC
//            C: Channels (Samples per pixel)
//            B: bytes per sample
//            Y: Swap first - changes ABGR to BGRA and KCMY to CMYK

#define EXTRA_SH(e)            ((e) << 19)
#define FLOAT_SH(a)            ((a) << 18)
#define OPTIMIZED_SH(s)        ((s) << 17)
#define COLORSPACE_SH(s)       ((s) << 12)
#define SWAPFIRST_SH(s)        ((s) << 11)
#define FLAVOR_SH(s)           ((s) << 10)
#define PLANAR_SH(p)           ((p) << 9)
#define ENDIAN16_SH(e)         ((e) << 8)
#define DOSWAP_SH(e)           ((e) << 7)
#define CHANNELS_SH(c)         ((c) << 3)
#define BYTES_SH(b)            (b)

// These macros unpack format specifiers into integers
#define T_EXTRA(e)            (((e)>>19)&63)
#define T_FLOAT(a)            (((a)>>18)&1)
#define T_OPTIMIZED(o)        (((o)>>17)&1)
#define T_COLORSPACE(s)       (((s)>>12)&31)
#define T_SWAPFIRST(s)        (((s)>>11)&1)
#define T_FLAVOR(s)           (((s)>>10)&1)
#define T_PLANAR(p)           (((p)>>9)&1)
#define T_ENDIAN16(e)         (((e)>>8)&1)
#define T_DOSWAP(e)           (((e)>>7)&1)
#define T_CHANNELS(c)         (((c)>>3)&15)
#define T_BYTES(b)            ((b)&7)


// Pixel types
#define PT_ANY       0    // Don't check colorspace
                          // 1 & 2 are reserved
#define PT_GRAY      3
#define PT_RGB       4
#define PT_CMY       5
#define PT_CMYK      6
#define PT_YCbCr     7
#define PT_YUV       8      // Lu'v'
#define PT_XYZ       9
#define PT_Lab       10
#define PT_YUVK      11     // Lu'v'K
#define PT_HSV       12
#define PT_HLS       13
#define PT_Yxy       14

#define PT_MCH1      15
#define PT_MCH2      16
#define PT_MCH3      17
#define PT_MCH4      18
#define PT_MCH5      19
#define PT_MCH6      20
#define PT_MCH7      21
#define PT_MCH8      22
#define PT_MCH9      23
#define PT_MCH10     24
#define PT_MCH11     25
#define PT_MCH12     26
#define PT_MCH13     27
#define PT_MCH14     28
#define PT_MCH15     29

#define PT_LabV2     30     // Identical to PT_Lab, but using the V2 old encoding

// Some (not all!) representations

#ifndef TYPE_RGB_8      // TYPE_RGB_8 is a very common identifier, so don't include ours
                        // if user has it already defined.

#define TYPE_GRAY_8            (COLORSPACE_SH(PT_GRAY)|CHANNELS_SH(1)|BYTES_SH(1))
#define TYPE_GRAY_8_REV        (COLORSPACE_SH(PT_GRAY)|CHANNELS_SH(1)|BYTES_SH(1)|FLAVOR_SH(1))
#define TYPE_GRAY_16           (COLORSPACE_SH(PT_GRAY)|CHANNELS_SH(1)|BYTES_SH(2))
#define TYPE_GRAY_16_REV       (COLORSPACE_SH(PT_GRAY)|CHANNELS_SH(1)|BYTES_SH(2)|FLAVOR_SH(1))
#define TYPE_GRAY_16_SE        (COLORSPACE_SH(PT_GRAY)|CHANNELS_SH(1)|BYTES_SH(2)|ENDIAN16_SH(1))
#define TYPE_GRAYA_8           (COLORSPACE_SH(PT_GRAY)|EXTRA_SH(1)|CHANNELS_SH(1)|BYTES_SH(1))
#define TYPE_GRAYA_16          (COLORSPACE_SH(PT_GRAY)|EXTRA_SH(1)|CHANNELS_SH(1)|BYTES_SH(2))
#define TYPE_GRAYA_16_SE       (COLORSPACE_SH(PT_GRAY)|EXTRA_SH(1)|CHANNELS_SH(1)|BYTES_SH(2)|ENDIAN16_SH(1))
#define TYPE_GRAYA_8_PLANAR    (COLORSPACE_SH(PT_GRAY)|EXTRA_SH(1)|CHANNELS_SH(1)|BYTES_SH(1)|PLANAR_SH(1))
#define TYPE_GRAYA_16_PLANAR   (COLORSPACE_SH(PT_GRAY)|EXTRA_SH(1)|CHANNELS_SH(1)|BYTES_SH(2)|PLANAR_SH(1))

#define TYPE_RGB_8             (COLORSPACE_SH(PT_RGB)|CHANNELS_SH(3)|BYTES_SH(1))
#define TYPE_RGB_8_PLANAR      (COLORSPACE_SH(PT_RGB)|CHANNELS_SH(3)|BYTES_SH(1)|PLANAR_SH(1))
#define TYPE_BGR_8             (COLORSPACE_SH(PT_RGB)|CHANNELS_SH(3)|BYTES_SH(1)|DOSWAP_SH(1))
#define TYPE_BGR_8_PLANAR      (COLORSPACE_SH(PT_RGB)|CHANNELS_SH(3)|BYTES_SH(1)|DOSWAP_SH(1)|PLANAR_SH(1))
#define TYPE_RGB_16            (COLORSPACE_SH(PT_RGB)|CHANNELS_SH(3)|BYTES_SH(2))
#define TYPE_RGB_16_PLANAR     (COLORSPACE_SH(PT_RGB)|CHANNELS_SH(3)|BYTES_SH(2)|PLANAR_SH(1))
#define TYPE_RGB_16_SE         (COLORSPACE_SH(PT_RGB)|CHANNELS_SH(3)|BYTES_SH(2)|ENDIAN16_SH(1))
#define TYPE_BGR_16            (COLORSPACE_SH(PT_RGB)|CHANNELS_SH(3)|BYTES_SH(2)|DOSWAP_SH(1))
#define TYPE_BGR_16_PLANAR     (COLORSPACE_SH(PT_RGB)|CHANNELS_SH(3)|BYTES_SH(2)|DOSWAP_SH(1)|PLANAR_SH(1))
#define TYPE_BGR_16_SE         (COLORSPACE_SH(PT_RGB)|CHANNELS_SH(3)|BYTES_SH(2)|DOSWAP_SH(1)|ENDIAN16_SH(1))

#define TYPE_RGBA_8            (COLORSPACE_SH(PT_RGB)|EXTRA_SH(1)|CHANNELS_SH(3)|BYTES_SH(1))
#define TYPE_RGBA_8_PLANAR     (COLORSPACE_SH(PT_RGB)|EXTRA_SH(1)|CHANNELS_SH(3)|BYTES_SH(1)|PLANAR_SH(1))
#define TYPE_RGBA_16           (COLORSPACE_SH(PT_RGB)|EXTRA_SH(1)|CHANNELS_SH(3)|BYTES_SH(2))
#define TYPE_RGBA_16_PLANAR    (COLORSPACE_SH(PT_RGB)|EXTRA_SH(1)|CHANNELS_SH(3)|BYTES_SH(2)|PLANAR_SH(1))
#define TYPE_RGBA_16_SE        (COLORSPACE_SH(PT_RGB)|EXTRA_SH(1)|CHANNELS_SH(3)|BYTES_SH(2)|ENDIAN16_SH(1))

#define TYPE_ARGB_8            (COLORSPACE_SH(PT_RGB)|EXTRA_SH(1)|CHANNELS_SH(3)|BYTES_SH(1)|SWAPFIRST_SH(1))
#define TYPE_ARGB_8_PLANAR     (COLORSPACE_SH(PT_RGB)|EXTRA_SH(1)|CHANNELS_SH(3)|BYTES_SH(1)|SWAPFIRST_SH(1)|PLANAR_SH(1))
#define TYPE_ARGB_16           (COLORSPACE_SH(PT_RGB)|EXTRA_SH(1)|CHANNELS_SH(3)|BYTES_SH(2)|SWAPFIRST_SH(1))

#define TYPE_ABGR_8            (COLORSPACE_SH(PT_RGB)|EXTRA_SH(1)|CHANNELS_SH(3)|BYTES_SH(1)|DOSWAP_SH(1))
#define TYPE_ABGR_8_PLANAR     (COLORSPACE_SH(PT_RGB)|EXTRA_SH(1)|CHANNELS_SH(3)|BYTES_SH(1)|DOSWAP_SH(1)|PLANAR_SH(1))
#define TYPE_ABGR_16           (COLORSPACE_SH(PT_RGB)|EXTRA_SH(1)|CHANNELS_SH(3)|BYTES_SH(2)|DOSWAP_SH(1))
#define TYPE_ABGR_16_PLANAR    (COLORSPACE_SH(PT_RGB)|EXTRA_SH(1)|CHANNELS_SH(3)|BYTES_SH(2)|DOSWAP_SH(1)|PLANAR_SH(1))
#define TYPE_ABGR_16_SE        (COLORSPACE_SH(PT_RGB)|EXTRA_SH(1)|CHANNELS_SH(3)|BYTES_SH(2)|DOSWAP_SH(1)|ENDIAN16_SH(1))

#define TYPE_BGRA_8            (COLORSPACE_SH(PT_RGB)|EXTRA_SH(1)|CHANNELS_SH(3)|BYTES_SH(1)|DOSWAP_SH(1)|SWAPFIRST_SH(1))
#define TYPE_BGRA_8_PLANAR     (COLORSPACE_SH(PT_RGB)|EXTRA_SH(1)|CHANNELS_SH(3)|BYTES_SH(1)|DOSWAP_SH(1)|SWAPFIRST_SH(1)|PLANAR_SH(1))
#define TYPE_BGRA_16           (COLORSPACE_SH(PT_RGB)|EXTRA_SH(1)|CHANNELS_SH(3)|BYTES_SH(2)|DOSWAP_SH(1)|SWAPFIRST_SH(1))
#define TYPE_BGRA_16_SE        (COLORSPACE_SH(PT_RGB)|EXTRA_SH(1)|CHANNELS_SH(3)|BYTES_SH(2)|ENDIAN16_SH(1)|DOSWAP_SH(1)|SWAPFIRST_SH(1))

#define TYPE_CMY_8             (COLORSPACE_SH(PT_CMY)|CHANNELS_SH(3)|BYTES_SH(1))
#define TYPE_CMY_8_PLANAR      (COLORSPACE_SH(PT_CMY)|CHANNELS_SH(3)|BYTES_SH(1)|PLANAR_SH(1))
#define TYPE_CMY_16            (COLORSPACE_SH(PT_CMY)|CHANNELS_SH(3)|BYTES_SH(2))
#define TYPE_CMY_16_PLANAR     (COLORSPACE_SH(PT_CMY)|CHANNELS_SH(3)|BYTES_SH(2)|PLANAR_SH(1))
#define TYPE_CMY_16_SE         (COLORSPACE_SH(PT_CMY)|CHANNELS_SH(3)|BYTES_SH(2)|ENDIAN16_SH(1))

#define TYPE_CMYK_8            (COLORSPACE_SH(PT_CMYK)|CHANNELS_SH(4)|BYTES_SH(1))
#define TYPE_CMYKA_8           (COLORSPACE_SH(PT_CMYK)|EXTRA_SH(1)|CHANNELS_SH(4)|BYTES_SH(1))
#define TYPE_CMYK_8_REV        (COLORSPACE_SH(PT_CMYK)|CHANNELS_SH(4)|BYTES_SH(1)|FLAVOR_SH(1))
#define TYPE_YUVK_8            TYPE_CMYK_8_REV
#define TYPE_CMYK_8_PLANAR     (COLORSPACE_SH(PT_CMYK)|CHANNELS_SH(4)|BYTES_SH(1)|PLANAR_SH(1))
#define TYPE_CMYK_16           (COLORSPACE_SH(PT_CMYK)|CHANNELS_SH(4)|BYTES_SH(2))
#define TYPE_CMYK_16_REV       (COLORSPACE_SH(PT_CMYK)|CHANNELS_SH(4)|BYTES_SH(2)|FLAVOR_SH(1))
#define TYPE_YUVK_16           TYPE_CMYK_16_REV
#define TYPE_CMYK_16_PLANAR    (COLORSPACE_SH(PT_CMYK)|CHANNELS_SH(4)|BYTES_SH(2)|PLANAR_SH(1))
#define TYPE_CMYK_16_SE        (COLORSPACE_SH(PT_CMYK)|CHANNELS_SH(4)|BYTES_SH(2)|ENDIAN16_SH(1))

#define TYPE_KYMC_8            (COLORSPACE_SH(PT_CMYK)|CHANNELS_SH(4)|BYTES_SH(1)|DOSWAP_SH(1))
#define TYPE_KYMC_16           (COLORSPACE_SH(PT_CMYK)|CHANNELS_SH(4)|BYTES_SH(2)|DOSWAP_SH(1))
#define TYPE_KYMC_16_SE        (COLORSPACE_SH(PT_CMYK)|CHANNELS_SH(4)|BYTES_SH(2)|DOSWAP_SH(1)|ENDIAN16_SH(1))

#define TYPE_KCMY_8            (COLORSPACE_SH(PT_CMYK)|CHANNELS_SH(4)|BYTES_SH(1)|SWAPFIRST_SH(1))
#define TYPE_KCMY_8_REV        (COLORSPACE_SH(PT_CMYK)|CHANNELS_SH(4)|BYTES_SH(1)|FLAVOR_SH(1)|SWAPFIRST_SH(1))
#define TYPE_KCMY_16           (COLORSPACE_SH(PT_CMYK)|CHANNELS_SH(4)|BYTES_SH(2)|SWAPFIRST_SH(1))
#define TYPE_KCMY_16_REV       (COLORSPACE_SH(PT_CMYK)|CHANNELS_SH(4)|BYTES_SH(2)|FLAVOR_SH(1)|SWAPFIRST_SH(1))
#define TYPE_KCMY_16_SE        (COLORSPACE_SH(PT_CMYK)|CHANNELS_SH(4)|BYTES_SH(2)|ENDIAN16_SH(1)|SWAPFIRST_SH(1))

#define TYPE_CMYK5_8           (COLORSPACE_SH(PT_MCH5)|CHANNELS_SH(5)|BYTES_SH(1))
#define TYPE_CMYK5_16          (COLORSPACE_SH(PT_MCH5)|CHANNELS_SH(5)|BYTES_SH(2))
#define TYPE_CMYK5_16_SE       (COLORSPACE_SH(PT_MCH5)|CHANNELS_SH(5)|BYTES_SH(2)|ENDIAN16_SH(1))
#define TYPE_KYMC5_8           (COLORSPACE_SH(PT_MCH5)|CHANNELS_SH(5)|BYTES_SH(1)|DOSWAP_SH(1))
#define TYPE_KYMC5_16          (COLORSPACE_SH(PT_MCH5)|CHANNELS_SH(5)|BYTES_SH(2)|DOSWAP_SH(1))
#define TYPE_KYMC5_16_SE       (COLORSPACE_SH(PT_MCH5)|CHANNELS_SH(5)|BYTES_SH(2)|DOSWAP_SH(1)|ENDIAN16_SH(1))
#define TYPE_CMYK6_8           (COLORSPACE_SH(PT_MCH6)|CHANNELS_SH(6)|BYTES_SH(1))
#define TYPE_CMYK6_8_PLANAR    (COLORSPACE_SH(PT_MCH6)|CHANNELS_SH(6)|BYTES_SH(1)|PLANAR_SH(1))
#define TYPE_CMYK6_16          (COLORSPACE_SH(PT_MCH6)|CHANNELS_SH(6)|BYTES_SH(2))
#define TYPE_CMYK6_16_PLANAR   (COLORSPACE_SH(PT_MCH6)|CHANNELS_SH(6)|BYTES_SH(2)|PLANAR_SH(1))
#define TYPE_CMYK6_16_SE       (COLORSPACE_SH(PT_MCH6)|CHANNELS_SH(6)|BYTES_SH(2)|ENDIAN16_SH(1))
#define TYPE_CMYK7_8           (COLORSPACE_SH(PT_MCH7)|CHANNELS_SH(7)|BYTES_SH(1))
#define TYPE_CMYK7_16          (COLORSPACE_SH(PT_MCH7)|CHANNELS_SH(7)|BYTES_SH(2))
#define TYPE_CMYK7_16_SE       (COLORSPACE_SH(PT_MCH7)|CHANNELS_SH(7)|BYTES_SH(2)|ENDIAN16_SH(1))
#define TYPE_KYMC7_8           (COLORSPACE_SH(PT_MCH7)|CHANNELS_SH(7)|BYTES_SH(1)|DOSWAP_SH(1))
#define TYPE_KYMC7_16          (COLORSPACE_SH(PT_MCH7)|CHANNELS_SH(7)|BYTES_SH(2)|DOSWAP_SH(1))
#define TYPE_KYMC7_16_SE       (COLORSPACE_SH(PT_MCH7)|CHANNELS_SH(7)|BYTES_SH(2)|DOSWAP_SH(1)|ENDIAN16_SH(1))
#define TYPE_CMYK8_8           (COLORSPACE_SH(PT_MCH8)|CHANNELS_SH(8)|BYTES_SH(1))
#define TYPE_CMYK8_16          (COLORSPACE_SH(PT_MCH8)|CHANNELS_SH(8)|BYTES_SH(2))
#define TYPE_CMYK8_16_SE       (COLORSPACE_SH(PT_MCH8)|CHANNELS_SH(8)|BYTES_SH(2)|ENDIAN16_SH(1))
#define TYPE_KYMC8_8           (COLORSPACE_SH(PT_MCH8)|CHANNELS_SH(8)|BYTES_SH(1)|DOSWAP_SH(1))
#define TYPE_KYMC8_16          (COLORSPACE_SH(PT_MCH8)|CHANNELS_SH(8)|BYTES_SH(2)|DOSWAP_SH(1))
#define TYPE_KYMC8_16_SE       (COLORSPACE_SH(PT_MCH8)|CHANNELS_SH(8)|BYTES_SH(2)|DOSWAP_SH(1)|ENDIAN16_SH(1))
#define TYPE_CMYK9_8           (COLORSPACE_SH(PT_MCH9)|CHANNELS_SH(9)|BYTES_SH(1))
#define TYPE_CMYK9_16          (COLORSPACE_SH(PT_MCH9)|CHANNELS_SH(9)|BYTES_SH(2))
#define TYPE_CMYK9_16_SE       (COLORSPACE_SH(PT_MCH9)|CHANNELS_SH(9)|BYTES_SH(2)|ENDIAN16_SH(1))
#define TYPE_KYMC9_8           (COLORSPACE_SH(PT_MCH9)|CHANNELS_SH(9)|BYTES_SH(1)|DOSWAP_SH(1))
#define TYPE_KYMC9_16          (COLORSPACE_SH(PT_MCH9)|CHANNELS_SH(9)|BYTES_SH(2)|DOSWAP_SH(1))
#define TYPE_KYMC9_16_SE       (COLORSPACE_SH(PT_MCH9)|CHANNELS_SH(9)|BYTES_SH(2)|DOSWAP_SH(1)|ENDIAN16_SH(1))
#define TYPE_CMYK10_8          (COLORSPACE_SH(PT_MCH10)|CHANNELS_SH(10)|BYTES_SH(1))
#define TYPE_CMYK10_16         (COLORSPACE_SH(PT_MCH10)|CHANNELS_SH(10)|BYTES_SH(2))
#define TYPE_CMYK10_16_SE      (COLORSPACE_SH(PT_MCH10)|CHANNELS_SH(10)|BYTES_SH(2)|ENDIAN16_SH(1))
#define TYPE_KYMC10_8          (COLORSPACE_SH(PT_MCH10)|CHANNELS_SH(10)|BYTES_SH(1)|DOSWAP_SH(1))
#define TYPE_KYMC10_16         (COLORSPACE_SH(PT_MCH10)|CHANNELS_SH(10)|BYTES_SH(2)|DOSWAP_SH(1))
#define TYPE_KYMC10_16_SE      (COLORSPACE_SH(PT_MCH10)|CHANNELS_SH(10)|BYTES_SH(2)|DOSWAP_SH(1)|ENDIAN16_SH(1))
#define TYPE_CMYK11_8          (COLORSPACE_SH(PT_MCH11)|CHANNELS_SH(11)|BYTES_SH(1))
#define TYPE_CMYK11_16         (COLORSPACE_SH(PT_MCH11)|CHANNELS_SH(11)|BYTES_SH(2))
#define TYPE_CMYK11_16_SE      (COLORSPACE_SH(PT_MCH11)|CHANNELS_SH(11)|BYTES_SH(2)|ENDIAN16_SH(1))
#define TYPE_KYMC11_8          (COLORSPACE_SH(PT_MCH11)|CHANNELS_SH(11)|BYTES_SH(1)|DOSWAP_SH(1))
#define TYPE_KYMC11_16         (COLORSPACE_SH(PT_MCH11)|CHANNELS_SH(11)|BYTES_SH(2)|DOSWAP_SH(1))
#define TYPE_KYMC11_16_SE      (COLORSPACE_SH(PT_MCH11)|CHANNELS_SH(11)|BYTES_SH(2)|DOSWAP_SH(1)|ENDIAN16_SH(1))
#define TYPE_CMYK12_8          (COLORSPACE_SH(PT_MCH12)|CHANNELS_SH(12)|BYTES_SH(1))
#define TYPE_CMYK12_16         (COLORSPACE_SH(PT_MCH12)|CHANNELS_SH(12)|BYTES_SH(2))
#define TYPE_CMYK12_16_SE      (COLORSPACE_SH(PT_MCH12)|CHANNELS_SH(12)|BYTES_SH(2)|ENDIAN16_SH(1))
#define TYPE_KYMC12_8          (COLORSPACE_SH(PT_MCH12)|CHANNELS_SH(12)|BYTES_SH(1)|DOSWAP_SH(1))
#define TYPE_KYMC12_16         (COLORSPACE_SH(PT_MCH12)|CHANNELS_SH(12)|BYTES_SH(2)|DOSWAP_SH(1))
#define TYPE_KYMC12_16_SE      (COLORSPACE_SH(PT_MCH12)|CHANNELS_SH(12)|BYTES_SH(2)|DOSWAP_SH(1)|ENDIAN16_SH(1))

// Colorimetric
#define TYPE_XYZ_16            (COLORSPACE_SH(PT_XYZ)|CHANNELS_SH(3)|BYTES_SH(2))
#define TYPE_Lab_8             (COLORSPACE_SH(PT_Lab)|CHANNELS_SH(3)|BYTES_SH(1))
#define TYPE_LabV2_8           (COLORSPACE_SH(PT_LabV2)|CHANNELS_SH(3)|BYTES_SH(1))

#define TYPE_ALab_8            (COLORSPACE_SH(PT_Lab)|CHANNELS_SH(3)|BYTES_SH(1)|EXTRA_SH(1)|SWAPFIRST_SH(1))
#define TYPE_ALabV2_8          (COLORSPACE_SH(PT_LabV2)|CHANNELS_SH(3)|BYTES_SH(1)|EXTRA_SH(1)|SWAPFIRST_SH(1))
#define TYPE_Lab_16            (COLORSPACE_SH(PT_Lab)|CHANNELS_SH(3)|BYTES_SH(2))
#define TYPE_LabV2_16          (COLORSPACE_SH(PT_LabV2)|CHANNELS_SH(3)|BYTES_SH(2))
#define TYPE_Yxy_16            (COLORSPACE_SH(PT_Yxy)|CHANNELS_SH(3)|BYTES_SH(2))

// YCbCr
#define TYPE_YCbCr_8           (COLORSPACE_SH(PT_YCbCr)|CHANNELS_SH(3)|BYTES_SH(1))
#define TYPE_YCbCr_8_PLANAR    (COLORSPACE_SH(PT_YCbCr)|CHANNELS_SH(3)|BYTES_SH(1)|PLANAR_SH(1))
#define TYPE_YCbCr_16          (COLORSPACE_SH(PT_YCbCr)|CHANNELS_SH(3)|BYTES_SH(2))
#define TYPE_YCbCr_16_PLANAR   (COLORSPACE_SH(PT_YCbCr)|CHANNELS_SH(3)|BYTES_SH(2)|PLANAR_SH(1))
#define TYPE_YCbCr_16_SE       (COLORSPACE_SH(PT_YCbCr)|CHANNELS_SH(3)|BYTES_SH(2)|ENDIAN16_SH(1))

// YUV
#define TYPE_YUV_8             (COLORSPACE_SH(PT_YUV)|CHANNELS_SH(3)|BYTES_SH(1))
#define TYPE_YUV_8_PLANAR      (COLORSPACE_SH(PT_YUV)|CHANNELS_SH(3)|BYTES_SH(1)|PLANAR_SH(1))
#define TYPE_YUV_16            (COLORSPACE_SH(PT_YUV)|CHANNELS_SH(3)|BYTES_SH(2))
#define TYPE_YUV_16_PLANAR     (COLORSPACE_SH(PT_YUV)|CHANNELS_SH(3)|BYTES_SH(2)|PLANAR_SH(1))
#define TYPE_YUV_16_SE         (COLORSPACE_SH(PT_YUV)|CHANNELS_SH(3)|BYTES_SH(2)|ENDIAN16_SH(1))

// HLS
#define TYPE_HLS_8             (COLORSPACE_SH(PT_HLS)|CHANNELS_SH(3)|BYTES_SH(1))
#define TYPE_HLS_8_PLANAR      (COLORSPACE_SH(PT_HLS)|CHANNELS_SH(3)|BYTES_SH(1)|PLANAR_SH(1))
#define TYPE_HLS_16            (COLORSPACE_SH(PT_HLS)|CHANNELS_SH(3)|BYTES_SH(2))
#define TYPE_HLS_16_PLANAR     (COLORSPACE_SH(PT_HLS)|CHANNELS_SH(3)|BYTES_SH(2)|PLANAR_SH(1))
#define TYPE_HLS_16_SE         (COLORSPACE_SH(PT_HLS)|CHANNELS_SH(3)|BYTES_SH(2)|ENDIAN16_SH(1))

// HSV
#define TYPE_HSV_8             (COLORSPACE_SH(PT_HSV)|CHANNELS_SH(3)|BYTES_SH(1))
#define TYPE_HSV_8_PLANAR      (COLORSPACE_SH(PT_HSV)|CHANNELS_SH(3)|BYTES_SH(1)|PLANAR_SH(1))
#define TYPE_HSV_16            (COLORSPACE_SH(PT_HSV)|CHANNELS_SH(3)|BYTES_SH(2))
#define TYPE_HSV_16_PLANAR     (COLORSPACE_SH(PT_HSV)|CHANNELS_SH(3)|BYTES_SH(2)|PLANAR_SH(1))
#define TYPE_HSV_16_SE         (COLORSPACE_SH(PT_HSV)|CHANNELS_SH(3)|BYTES_SH(2)|ENDIAN16_SH(1))

// Named color index. Only 16 bits allowed (don't check colorspace)
#define TYPE_NAMED_COLOR_INDEX (CHANNELS_SH(1)|BYTES_SH(2))

// Float formatters.
#define TYPE_XYZ_FLT          (FLOAT_SH(1)|COLORSPACE_SH(PT_XYZ)|CHANNELS_SH(3)|BYTES_SH(4))
#define TYPE_Lab_FLT          (FLOAT_SH(1)|COLORSPACE_SH(PT_Lab)|CHANNELS_SH(3)|BYTES_SH(4))
#define TYPE_LabA_FLT         (FLOAT_SH(1)|COLORSPACE_SH(PT_Lab)|EXTRA_SH(1)|CHANNELS_SH(3)|BYTES_SH(4))
#define TYPE_GRAY_FLT         (FLOAT_SH(1)|COLORSPACE_SH(PT_GRAY)|CHANNELS_SH(1)|BYTES_SH(4))
#define TYPE_RGB_FLT          (FLOAT_SH(1)|COLORSPACE_SH(PT_RGB)|CHANNELS_SH(3)|BYTES_SH(4))

#define TYPE_RGBA_FLT         (FLOAT_SH(1)|COLORSPACE_SH(PT_RGB)|EXTRA_SH(1)|CHANNELS_SH(3)|BYTES_SH(4))
#define TYPE_ARGB_FLT         (FLOAT_SH(1)|COLORSPACE_SH(PT_RGB)|EXTRA_SH(1)|CHANNELS_SH(3)|BYTES_SH(4)|SWAPFIRST_SH(1))
#define TYPE_BGR_FLT          (FLOAT_SH(1)|COLORSPACE_SH(PT_RGB)|CHANNELS_SH(3)|BYTES_SH(4)|DOSWAP_SH(1))
#define TYPE_BGRA_FLT         (FLOAT_SH(1)|COLORSPACE_SH(PT_RGB)|EXTRA_SH(1)|CHANNELS_SH(3)|BYTES_SH(4)|DOSWAP_SH(1)|SWAPFIRST_SH(1))
#define TYPE_ABGR_FLT         (FLOAT_SH(1)|COLORSPACE_SH(PT_RGB)|EXTRA_SH(1)|CHANNELS_SH(3)|BYTES_SH(4)|DOSWAP_SH(1))

#define TYPE_CMYK_FLT         (FLOAT_SH(1)|COLORSPACE_SH(PT_CMYK)|CHANNELS_SH(4)|BYTES_SH(4))

// Floating point formatters.
// NOTE THAT 'BYTES' FIELD IS SET TO ZERO ON DLB because 8 bytes overflows the bitfield
#define TYPE_XYZ_DBL          (FLOAT_SH(1)|COLORSPACE_SH(PT_XYZ)|CHANNELS_SH(3)|BYTES_SH(0))
#define TYPE_Lab_DBL          (FLOAT_SH(1)|COLORSPACE_SH(PT_Lab)|CHANNELS_SH(3)|BYTES_SH(0))
#define TYPE_GRAY_DBL         (FLOAT_SH(1)|COLORSPACE_SH(PT_GRAY)|CHANNELS_SH(1)|BYTES_SH(0))
#define TYPE_RGB_DBL          (FLOAT_SH(1)|COLORSPACE_SH(PT_RGB)|CHANNELS_SH(3)|BYTES_SH(0))
#define TYPE_BGR_DBL          (FLOAT_SH(1)|COLORSPACE_SH(PT_RGB)|CHANNELS_SH(3)|BYTES_SH(0)|DOSWAP_SH(1))
#define TYPE_CMYK_DBL         (FLOAT_SH(1)|COLORSPACE_SH(PT_CMYK)|CHANNELS_SH(4)|BYTES_SH(0))

// IEEE 754-2008 "half"
#define TYPE_GRAY_HALF_FLT    (FLOAT_SH(1)|COLORSPACE_SH(PT_GRAY)|CHANNELS_SH(1)|BYTES_SH(2))
#define TYPE_RGB_HALF_FLT     (FLOAT_SH(1)|COLORSPACE_SH(PT_RGB)|CHANNELS_SH(3)|BYTES_SH(2))
#define TYPE_RGBA_HALF_FLT    (FLOAT_SH(1)|COLORSPACE_SH(PT_RGB)|EXTRA_SH(1)|CHANNELS_SH(3)|BYTES_SH(2))
#define TYPE_CMYK_HALF_FLT    (FLOAT_SH(1)|COLORSPACE_SH(PT_CMYK)|CHANNELS_SH(4)|BYTES_SH(2))

#define TYPE_RGBA_HALF_FLT    (FLOAT_SH(1)|COLORSPACE_SH(PT_RGB)|EXTRA_SH(1)|CHANNELS_SH(3)|BYTES_SH(2))
#define TYPE_ARGB_HALF_FLT    (FLOAT_SH(1)|COLORSPACE_SH(PT_RGB)|EXTRA_SH(1)|CHANNELS_SH(3)|BYTES_SH(2)|SWAPFIRST_SH(1))
#define TYPE_BGR_HALF_FLT     (FLOAT_SH(1)|COLORSPACE_SH(PT_RGB)|CHANNELS_SH(3)|BYTES_SH(2)|DOSWAP_SH(1))
#define TYPE_BGRA_HALF_FLT    (FLOAT_SH(1)|COLORSPACE_SH(PT_RGB)|EXTRA_SH(1)|CHANNELS_SH(3)|BYTES_SH(2)|DOSWAP_SH(1)|SWAPFIRST_SH(1))
#define TYPE_ABGR_HALF_FLT    (FLOAT_SH(1)|COLORSPACE_SH(PT_RGB)|CHANNELS_SH(3)|BYTES_SH(2)|DOSWAP_SH(1))

#endif

// Colorspaces
typedef struct {
        cmsFloat64Number X;
        cmsFloat64Number Y;
        cmsFloat64Number Z;

    } cmsCIEXYZ;

typedef struct {
        cmsFloat64Number x;
        cmsFloat64Number y;
        cmsFloat64Number Y;

    } cmsCIExyY;

typedef struct {
        cmsFloat64Number L;
        cmsFloat64Number a;
        cmsFloat64Number b;

    } cmsCIELab;

typedef struct {
        cmsFloat64Number L;
        cmsFloat64Number C;
        cmsFloat64Number h;

    } cmsCIELCh;

typedef struct {
        cmsFloat64Number J;
        cmsFloat64Number C;
        cmsFloat64Number h;

    } cmsJCh;

typedef struct {
        cmsCIEXYZ  Red;
        cmsCIEXYZ  Green;
        cmsCIEXYZ  Blue;

    } cmsCIEXYZTRIPLE;

typedef struct {
        cmsCIExyY  Red;
        cmsCIExyY  Green;
        cmsCIExyY  Blue;

    } cmsCIExyYTRIPLE;

// Illuminant types for structs below
#define cmsILLUMINANT_TYPE_UNKNOWN 0x0000000
#define cmsILLUMINANT_TYPE_D50     0x0000001
#define cmsILLUMINANT_TYPE_D65     0x0000002
#define cmsILLUMINANT_TYPE_D93     0x0000003
#define cmsILLUMINANT_TYPE_F2      0x0000004
#define cmsILLUMINANT_TYPE_D55     0x0000005
#define cmsILLUMINANT_TYPE_A       0x0000006
#define cmsILLUMINANT_TYPE_E       0x0000007
#define cmsILLUMINANT_TYPE_F8      0x0000008

typedef struct {
        cmsUInt32Number  Observer;    // 0 = unknown, 1=CIE 1931, 2=CIE 1964
        cmsCIEXYZ        Backing;     // Value of backing
        cmsUInt32Number  Geometry;    // 0=unknown, 1=45/0, 0/45 2=0d, d/0
        cmsFloat64Number Flare;       // 0..1.0
        cmsUInt32Number  IlluminantType;

    } cmsICCMeasurementConditions;

typedef struct {
        cmsCIEXYZ       IlluminantXYZ;   // Not the same struct as CAM02,
        cmsCIEXYZ       SurroundXYZ;     // This is for storing the tag
        cmsUInt32Number IlluminantType;  // viewing condition

    } cmsICCViewingConditions;

// Get LittleCMS version (for shared objects) -----------------------------------------------------------------------------

CMSAPI int               CMSEXPORT cmsGetEncodedCMMversion(void);

// Support of non-standard functions --------------------------------------------------------------------------------------

CMSAPI int               CMSEXPORT cmsstrcasecmp(const char* s1, const char* s2);
CMSAPI long int          CMSEXPORT cmsfilelength(FILE* f);


// Context handling --------------------------------------------------------------------------------------------------------

// Each context holds its owns globals and its own plug-ins. There is a global context with the id = 0 for lecacy compatibility
// though using the global context is not recommended. Proper context handling makes lcms more thread-safe.

typedef struct _cmsContext_struct* cmsContext;

CMSAPI cmsContext       CMSEXPORT cmsCreateContext(void* Plugin, void* UserData);
CMSAPI void             CMSEXPORT cmsDeleteContext(cmsContext ContextID);
CMSAPI cmsContext       CMSEXPORT cmsDupContext(cmsContext ContextID, void* NewUserData);
CMSAPI void*            CMSEXPORT cmsGetContextUserData(cmsContext ContextID);

// Plug-In registering  --------------------------------------------------------------------------------------------------

CMSAPI cmsBool           CMSEXPORT cmsPlugin(cmsContext ContextID, void* Plugin);
CMSAPI void              CMSEXPORT cmsUnregisterPlugins(cmsContext ContextID);

// Error logging ----------------------------------------------------------------------------------------------------------

// There is no error handling at all. When a function fails, it returns proper value.
// For example, all create functions does return NULL on failure. Other may return FALSE.
// It may be interesting, for the developer, to know why the function is failing.
// for that reason, lcms2 does offer a logging function. This function will get
// an ENGLISH string with some clues on what is going wrong. You can show this
// info to the end user if you wish, or just create some sort of log on disk.
// The logging function should NOT terminate the program, as this obviously can leave
// unfreed resources. It is the programmer's responsibility to check each function
// return code to make sure it didn't fail.

#define cmsERROR_UNDEFINED                    0
#define cmsERROR_FILE                         1
#define cmsERROR_RANGE                        2
#define cmsERROR_INTERNAL                     3
#define cmsERROR_NULL                         4
#define cmsERROR_READ                         5
#define cmsERROR_SEEK                         6
#define cmsERROR_WRITE                        7
#define cmsERROR_UNKNOWN_EXTENSION            8
#define cmsERROR_COLORSPACE_CHECK             9
#define cmsERROR_ALREADY_DEFINED              10
#define cmsERROR_BAD_SIGNATURE                11
#define cmsERROR_CORRUPTION_DETECTED          12
#define cmsERROR_NOT_SUITABLE                 13

// Error logger is called with the ContextID when a message is raised. This gives the
// chance to know which thread is responsible of the warning and any environment associated
// with it. Non-multithreading applications may safely ignore this parameter.
// Note that under certain special circumstances, ContextID may be NULL.
typedef void  (* cmsLogErrorHandlerFunction)(cmsContext ContextID, cmsUInt32Number ErrorCode, const char *Text);

// Allows user to set any specific logger
CMSAPI void              CMSEXPORT cmsSetLogErrorHandler(cmsContext ContextID, cmsLogErrorHandlerFunction Fn);

// Conversions --------------------------------------------------------------------------------------------------------------

// Returns pointers to constant structs
CMSAPI const cmsCIEXYZ*  CMSEXPORT cmsD50_XYZ(cmsContext ContextID);
CMSAPI const cmsCIExyY*  CMSEXPORT cmsD50_xyY(cmsContext ContextID);

// Colorimetric space conversions
CMSAPI void              CMSEXPORT cmsXYZ2xyY(cmsContext ContextID, cmsCIExyY* Dest, const cmsCIEXYZ* Source);
CMSAPI void              CMSEXPORT cmsxyY2XYZ(cmsContext ContextID, cmsCIEXYZ* Dest, const cmsCIExyY* Source);
CMSAPI void              CMSEXPORT cmsXYZ2Lab(cmsContext ContextID, const cmsCIEXYZ* WhitePoint, cmsCIELab* Lab, const cmsCIEXYZ* xyz);
CMSAPI void              CMSEXPORT cmsLab2XYZ(cmsContext ContextID, const cmsCIEXYZ* WhitePoint, cmsCIEXYZ* xyz, const cmsCIELab* Lab);
CMSAPI void              CMSEXPORT cmsLab2LCh(cmsContext ContextID, cmsCIELCh*LCh, const cmsCIELab* Lab);
CMSAPI void              CMSEXPORT cmsLCh2Lab(cmsContext ContextID, cmsCIELab* Lab, const cmsCIELCh* LCh);

// Encoding /Decoding on PCS
CMSAPI void              CMSEXPORT cmsLabEncoded2Float(cmsContext ContextID, cmsCIELab* Lab, const cmsUInt16Number wLab[3]);
CMSAPI void              CMSEXPORT cmsLabEncoded2FloatV2(cmsContext ContextID, cmsCIELab* Lab, const cmsUInt16Number wLab[3]);
CMSAPI void              CMSEXPORT cmsFloat2LabEncoded(cmsContext ContextID, cmsUInt16Number wLab[3], const cmsCIELab* Lab);
CMSAPI void              CMSEXPORT cmsFloat2LabEncodedV2(cmsContext ContextID, cmsUInt16Number wLab[3], const cmsCIELab* Lab);
CMSAPI void              CMSEXPORT cmsXYZEncoded2Float(cmsContext ContextID, cmsCIEXYZ* fxyz, const cmsUInt16Number XYZ[3]);
CMSAPI void              CMSEXPORT cmsFloat2XYZEncoded(cmsContext ContextID, cmsUInt16Number XYZ[3], const cmsCIEXYZ* fXYZ);

// DeltaE metrics
CMSAPI cmsFloat64Number  CMSEXPORT cmsDeltaE(cmsContext ContextID, const cmsCIELab* Lab1, const cmsCIELab* Lab2);
CMSAPI cmsFloat64Number  CMSEXPORT cmsCIE94DeltaE(cmsContext ContextID, const cmsCIELab* Lab1, const cmsCIELab* Lab2);
CMSAPI cmsFloat64Number  CMSEXPORT cmsBFDdeltaE(cmsContext ContextID, const cmsCIELab* Lab1, const cmsCIELab* Lab2);
CMSAPI cmsFloat64Number  CMSEXPORT cmsCMCdeltaE(cmsContext ContextID, const cmsCIELab* Lab1, const cmsCIELab* Lab2, cmsFloat64Number l, cmsFloat64Number c);
CMSAPI cmsFloat64Number  CMSEXPORT cmsCIE2000DeltaE(cmsContext ContextID, const cmsCIELab* Lab1, const cmsCIELab* Lab2, cmsFloat64Number Kl, cmsFloat64Number Kc, cmsFloat64Number Kh);

// Temperature <-> Chromaticity (Black body)
CMSAPI cmsBool           CMSEXPORT cmsWhitePointFromTemp(cmsContext ContextID, cmsCIExyY* WhitePoint, cmsFloat64Number  TempK);
CMSAPI cmsBool           CMSEXPORT cmsTempFromWhitePoint(cmsContext ContextID, cmsFloat64Number* TempK, const cmsCIExyY* WhitePoint);

// Chromatic adaptation
CMSAPI cmsBool           CMSEXPORT cmsAdaptToIlluminant(cmsContext ContextID, cmsCIEXYZ* Result, const cmsCIEXYZ* SourceWhitePt,
                                                                           const cmsCIEXYZ* Illuminant,
                                                                           const cmsCIEXYZ* Value);

// CIECAM02 ---------------------------------------------------------------------------------------------------

// Viewing conditions. Please note those are CAM model viewing conditions, and not the ICC tag viewing
// conditions, which I'm naming cmsICCViewingConditions to make differences evident. Unfortunately, the tag
// cannot deal with surround La, Yb and D value so is basically useless to store CAM02 viewing conditions.


#define AVG_SURROUND       1
#define DIM_SURROUND       2
#define DARK_SURROUND      3
#define CUTSHEET_SURROUND  4

#define D_CALCULATE        (-1)

typedef struct {
    cmsCIEXYZ        whitePoint;
    cmsFloat64Number Yb;
    cmsFloat64Number La;
    cmsUInt32Number  surround;
    cmsFloat64Number D_value;

    } cmsViewingConditions;

CMSAPI cmsHANDLE         CMSEXPORT cmsCIECAM02Init(cmsContext ContextID, const cmsViewingConditions* pVC);
CMSAPI void              CMSEXPORT cmsCIECAM02Done(cmsContext ContextID, cmsHANDLE hModel);
CMSAPI void              CMSEXPORT cmsCIECAM02Forward(cmsContext ContextID, cmsHANDLE hModel, const cmsCIEXYZ* pIn, cmsJCh* pOut);
CMSAPI void              CMSEXPORT cmsCIECAM02Reverse(cmsContext ContextID, cmsHANDLE hModel, const cmsJCh* pIn,    cmsCIEXYZ* pOut);


// Tone curves -----------------------------------------------------------------------------------------

// This describes a curve segment. For a table of supported types, see the manual. User can increase the number of
// available types by using a proper plug-in. Parametric segments allow 10 parameters at most

typedef struct {
    cmsFloat32Number   x0, x1;           // Domain; for x0 < x <= x1
    cmsInt32Number     Type;             // Parametric type, Type == 0 means sampled segment. Negative values are reserved
    cmsFloat64Number   Params[10];       // Parameters if Type != 0
    cmsUInt32Number    nGridPoints;      // Number of grid points if Type == 0
    cmsFloat32Number*  SampledPoints;    // Points to an array of floats if Type == 0

} cmsCurveSegment;

// The internal representation is none of your business.
typedef struct _cms_curve_struct cmsToneCurve;

CMSAPI cmsToneCurve*     CMSEXPORT cmsBuildSegmentedToneCurve(cmsContext ContextID, cmsUInt32Number nSegments, const cmsCurveSegment Segments[]);
CMSAPI cmsToneCurve*     CMSEXPORT cmsBuildParametricToneCurve(cmsContext ContextID, cmsInt32Number Type, const cmsFloat64Number Params[]);
CMSAPI cmsToneCurve*     CMSEXPORT cmsBuildGamma(cmsContext ContextID, cmsFloat64Number Gamma);
CMSAPI cmsToneCurve*     CMSEXPORT cmsBuildTabulatedToneCurve16(cmsContext ContextID, cmsUInt32Number nEntries, const cmsUInt16Number values[]);
CMSAPI cmsToneCurve*     CMSEXPORT cmsBuildTabulatedToneCurveFloat(cmsContext ContextID, cmsUInt32Number nEntries, const cmsFloat32Number values[]);
CMSAPI void              CMSEXPORT cmsFreeToneCurve(cmsContext ContextID, cmsToneCurve* Curve);
CMSAPI void              CMSEXPORT cmsFreeToneCurveTriple(cmsContext ContextID, cmsToneCurve* Curve[3]);
CMSAPI cmsToneCurve*     CMSEXPORT cmsDupToneCurve(cmsContext ContextID, const cmsToneCurve* Src);
CMSAPI cmsToneCurve*     CMSEXPORT cmsReverseToneCurve(cmsContext ContextID, const cmsToneCurve* InGamma);
CMSAPI cmsToneCurve*     CMSEXPORT cmsReverseToneCurveEx(cmsContext ContextID, cmsUInt32Number nResultSamples, const cmsToneCurve* InGamma);
CMSAPI cmsToneCurve*     CMSEXPORT cmsJoinToneCurve(cmsContext ContextID, const cmsToneCurve* X,  const cmsToneCurve* Y, cmsUInt32Number nPoints);
CMSAPI cmsBool           CMSEXPORT cmsSmoothToneCurve(cmsContext ContextID, cmsToneCurve* Tab, cmsFloat64Number lambda);
CMSAPI cmsFloat32Number  CMSEXPORT cmsEvalToneCurveFloat(cmsContext ContextID, const cmsToneCurve* Curve, cmsFloat32Number v);
CMSAPI cmsUInt16Number   CMSEXPORT cmsEvalToneCurve16(cmsContext ContextID, const cmsToneCurve* Curve, cmsUInt16Number v);
CMSAPI cmsBool           CMSEXPORT cmsIsToneCurveMultisegment(cmsContext ContextID, const cmsToneCurve* InGamma);
CMSAPI cmsBool           CMSEXPORT cmsIsToneCurveLinear(cmsContext ContextID, const cmsToneCurve* Curve);
CMSAPI cmsBool           CMSEXPORT cmsIsToneCurveMonotonic(cmsContext ContextID, const cmsToneCurve* t);
CMSAPI cmsBool           CMSEXPORT cmsIsToneCurveDescending(cmsContext ContextID, const cmsToneCurve* t);
CMSAPI cmsInt32Number    CMSEXPORT cmsGetToneCurveParametricType(cmsContext ContextID, const cmsToneCurve* t);
CMSAPI cmsFloat64Number  CMSEXPORT cmsEstimateGamma(cmsContext ContextID, const cmsToneCurve* t, cmsFloat64Number Precision);
CMSAPI cmsFloat64Number* CMSEXPORT cmsGetToneCurveParams(cmsContext ContextID, const cmsToneCurve* t);

// Tone curve tabular estimation
CMSAPI cmsUInt32Number         CMSEXPORT cmsGetToneCurveEstimatedTableEntries(cmsContext ContextID, const cmsToneCurve* t);
CMSAPI const cmsUInt16Number*  CMSEXPORT cmsGetToneCurveEstimatedTable(cmsContext ContextID, const cmsToneCurve* t);


// Implements pipelines of multi-processing elements -------------------------------------------------------------

// Nothing to see here, move along
typedef struct _cmsPipeline_struct cmsPipeline;
typedef struct _cmsStage_struct cmsStage;

// Those are hi-level pipelines
CMSAPI cmsPipeline*      CMSEXPORT cmsPipelineAlloc(cmsContext ContextID, cmsUInt32Number InputChannels, cmsUInt32Number OutputChannels);
CMSAPI void              CMSEXPORT cmsPipelineFree(cmsContext ContextID, cmsPipeline* lut);
CMSAPI cmsPipeline*      CMSEXPORT cmsPipelineDup(cmsContext ContextID, const cmsPipeline* Orig);

CMSAPI cmsUInt32Number   CMSEXPORT cmsPipelineInputChannels(cmsContext ContextID, const cmsPipeline* lut);
CMSAPI cmsUInt32Number   CMSEXPORT cmsPipelineOutputChannels(cmsContext ContextID, const cmsPipeline* lut);

CMSAPI cmsUInt32Number   CMSEXPORT cmsPipelineStageCount(cmsContext ContextID, const cmsPipeline* lut);
CMSAPI cmsStage*         CMSEXPORT cmsPipelineGetPtrToFirstStage(cmsContext ContextID, const cmsPipeline* lut);
CMSAPI cmsStage*         CMSEXPORT cmsPipelineGetPtrToLastStage(cmsContext ContextID, const cmsPipeline* lut);

CMSAPI void              CMSEXPORT cmsPipelineEval16(cmsContext ContextID, const cmsUInt16Number In[], cmsUInt16Number Out[], const cmsPipeline* lut);
CMSAPI void              CMSEXPORT cmsPipelineEvalFloat(cmsContext ContextID, const cmsFloat32Number In[], cmsFloat32Number Out[], const cmsPipeline* lut);
CMSAPI cmsBool           CMSEXPORT cmsPipelineEvalReverseFloat(cmsContext ContextID, cmsFloat32Number Target[], cmsFloat32Number Result[], cmsFloat32Number Hint[], const cmsPipeline* lut);
CMSAPI cmsBool           CMSEXPORT cmsPipelineCat(cmsContext ContextID, cmsPipeline* l1, const cmsPipeline* l2);
CMSAPI cmsBool           CMSEXPORT cmsPipelineSetSaveAs8bitsFlag(cmsContext ContextID, cmsPipeline* lut, cmsBool On);

// Where to place/locate the stages in the pipeline chain
typedef enum { cmsAT_BEGIN, cmsAT_END } cmsStageLoc;

CMSAPI cmsBool           CMSEXPORT cmsPipelineInsertStage(cmsContext ContextID, cmsPipeline* lut, cmsStageLoc loc, cmsStage* mpe);
CMSAPI void              CMSEXPORT cmsPipelineUnlinkStage(cmsContext ContextID, cmsPipeline* lut, cmsStageLoc loc, cmsStage** mpe);

// This function is quite useful to analyze the structure of a Pipeline and retrieve the Stage elements
// that conform the Pipeline. It should be called with the Pipeline, the number of expected elements and
// then a list of expected types followed with a list of double pointers to Stage elements. If
// the function founds a match with current pipeline, it fills the pointers and returns TRUE
// if not, returns FALSE without touching anything.
CMSAPI cmsBool           CMSEXPORT cmsPipelineCheckAndRetreiveStages(cmsContext ContextID, const cmsPipeline* Lut, cmsUInt32Number n, ...);

// Matrix has double precision and CLUT has only float precision. That is because an ICC profile can encode
// matrices with far more precision that CLUTS
CMSAPI cmsStage*         CMSEXPORT cmsStageAllocIdentity(cmsContext ContextID, cmsUInt32Number nChannels);
CMSAPI cmsStage*         CMSEXPORT cmsStageAllocToneCurves(cmsContext ContextID, cmsUInt32Number nChannels, cmsToneCurve* const Curves[]);
CMSAPI cmsStage*         CMSEXPORT cmsStageAllocMatrix(cmsContext ContextID, cmsUInt32Number Rows, cmsUInt32Number Cols, const cmsFloat64Number* Matrix, const cmsFloat64Number* Offset);

CMSAPI cmsStage*         CMSEXPORT cmsStageAllocCLut16bit(cmsContext ContextID, cmsUInt32Number nGridPoints, cmsUInt32Number inputChan, cmsUInt32Number outputChan, const cmsUInt16Number* Table);
CMSAPI cmsStage*         CMSEXPORT cmsStageAllocCLutFloat(cmsContext ContextID, cmsUInt32Number nGridPoints, cmsUInt32Number inputChan, cmsUInt32Number outputChan, const cmsFloat32Number* Table);

CMSAPI cmsStage*         CMSEXPORT cmsStageAllocCLut16bitGranular(cmsContext ContextID, const cmsUInt32Number clutPoints[], cmsUInt32Number inputChan, cmsUInt32Number outputChan, const cmsUInt16Number* Table);
CMSAPI cmsStage*         CMSEXPORT cmsStageAllocCLutFloatGranular(cmsContext ContextID, const cmsUInt32Number clutPoints[], cmsUInt32Number inputChan, cmsUInt32Number outputChan, const cmsFloat32Number* Table);

CMSAPI cmsStage*         CMSEXPORT cmsStageDup(cmsContext ContextID, cmsStage* mpe);
CMSAPI void              CMSEXPORT cmsStageFree(cmsContext ContextID, cmsStage* mpe);
CMSAPI cmsStage*         CMSEXPORT cmsStageNext(cmsContext ContextID, const cmsStage* mpe);

CMSAPI cmsUInt32Number   CMSEXPORT cmsStageInputChannels(cmsContext ContextID, const cmsStage* mpe);
CMSAPI cmsUInt32Number   CMSEXPORT cmsStageOutputChannels(cmsContext ContextID, const cmsStage* mpe);
CMSAPI cmsStageSignature CMSEXPORT cmsStageType(cmsContext ContextID, const cmsStage* mpe);
CMSAPI void*             CMSEXPORT cmsStageData(cmsContext ContextID, const cmsStage* mpe);

// Sampling
typedef cmsInt32Number (* cmsSAMPLER16)   (cmsContext ContextID,
                                           CMSREGISTER const cmsUInt16Number In[],
                                           CMSREGISTER cmsUInt16Number Out[],
                                           CMSREGISTER void * Cargo);

typedef cmsInt32Number (* cmsSAMPLERFLOAT)(cmsContext ContextID,
                                           CMSREGISTER const cmsFloat32Number In[],
                                           CMSREGISTER cmsFloat32Number Out[],
                                           CMSREGISTER void * Cargo);

// Use this flag to prevent changes being written to destination
#define SAMPLER_INSPECT     0x01000000

// For CLUT only
CMSAPI cmsBool           CMSEXPORT cmsStageSampleCLut16bit(cmsContext ContextID, cmsStage* mpe, cmsSAMPLER16 Sampler, void* Cargo, cmsUInt32Number dwFlags);
CMSAPI cmsBool           CMSEXPORT cmsStageSampleCLutFloat(cmsContext ContextID, cmsStage* mpe, cmsSAMPLERFLOAT Sampler, void* Cargo, cmsUInt32Number dwFlags);

// Slicers
CMSAPI cmsBool           CMSEXPORT cmsSliceSpace16(cmsContext ContextID, cmsUInt32Number nInputs, const cmsUInt32Number clutPoints[],
                                                   cmsSAMPLER16 Sampler, void * Cargo);

CMSAPI cmsBool           CMSEXPORT cmsSliceSpaceFloat(cmsContext ContextID, cmsUInt32Number nInputs, const cmsUInt32Number clutPoints[],
                                                   cmsSAMPLERFLOAT Sampler, void * Cargo);

// Multilocalized Unicode management ---------------------------------------------------------------------------------------

typedef struct _cms_MLU_struct cmsMLU;

#define  cmsNoLanguage "\0\0"
#define  cmsNoCountry  "\0\0"

CMSAPI cmsMLU*           CMSEXPORT cmsMLUalloc(cmsContext ContextID, cmsUInt32Number nItems);
CMSAPI void              CMSEXPORT cmsMLUfree(cmsContext ContextID, cmsMLU* mlu);
CMSAPI cmsMLU*           CMSEXPORT cmsMLUdup(cmsContext ContextID, const cmsMLU* mlu);

CMSAPI cmsBool           CMSEXPORT cmsMLUsetASCII(cmsContext ContextID, cmsMLU* mlu,
                                                  const char LanguageCode[3], const char CountryCode[3],
                                                  const char* ASCIIString);
CMSAPI cmsBool           CMSEXPORT cmsMLUsetWide(cmsContext ContextID, cmsMLU* mlu,
                                                  const char LanguageCode[3], const char CountryCode[3],
                                                  const wchar_t* WideString);

CMSAPI cmsUInt32Number   CMSEXPORT cmsMLUgetASCII(cmsContext ContextID, const cmsMLU* mlu,
                                                  const char LanguageCode[3], const char CountryCode[3],
                                                  char* Buffer,    cmsUInt32Number BufferSize);

CMSAPI cmsUInt32Number   CMSEXPORT cmsMLUgetWide(cmsContext ContextID, const cmsMLU* mlu,
                                                 const char LanguageCode[3], const char CountryCode[3],
                                                 wchar_t* Buffer, cmsUInt32Number BufferSize);

CMSAPI cmsBool           CMSEXPORT cmsMLUgetTranslation(cmsContext ContextID, const cmsMLU* mlu,
                                                         const char LanguageCode[3], const char CountryCode[3],
                                                         char ObtainedLanguage[3], char ObtainedCountry[3]);

CMSAPI cmsUInt32Number   CMSEXPORT cmsMLUtranslationsCount(cmsContext ContextID, const cmsMLU* mlu);

CMSAPI cmsBool           CMSEXPORT cmsMLUtranslationsCodes(cmsContext ContextID, const cmsMLU* mlu,
                                                             cmsUInt32Number idx,
                                                             char LanguageCode[3],
                                                             char CountryCode[3]);

// Undercolorremoval & black generation -------------------------------------------------------------------------------------

typedef struct {
        cmsToneCurve* Ucr;
        cmsToneCurve* Bg;
        cmsMLU*       Desc;

} cmsUcrBg;

// Screening ----------------------------------------------------------------------------------------------------------------

#define cmsPRINTER_DEFAULT_SCREENS     0x0001
#define cmsFREQUENCE_UNITS_LINES_CM    0x0000
#define cmsFREQUENCE_UNITS_LINES_INCH  0x0002

#define cmsSPOT_UNKNOWN         0
#define cmsSPOT_PRINTER_DEFAULT 1
#define cmsSPOT_ROUND           2
#define cmsSPOT_DIAMOND         3
#define cmsSPOT_ELLIPSE         4
#define cmsSPOT_LINE            5
#define cmsSPOT_SQUARE          6
#define cmsSPOT_CROSS           7

typedef struct {
    cmsFloat64Number  Frequency;
    cmsFloat64Number  ScreenAngle;
    cmsUInt32Number   SpotShape;

} cmsScreeningChannel;

typedef struct {
    cmsUInt32Number Flag;
    cmsUInt32Number nChannels;
    cmsScreeningChannel Channels[cmsMAXCHANNELS];

} cmsScreening;


// Named color -----------------------------------------------------------------------------------------------------------------

typedef struct _cms_NAMEDCOLORLIST_struct cmsNAMEDCOLORLIST;

CMSAPI cmsNAMEDCOLORLIST* CMSEXPORT cmsAllocNamedColorList(cmsContext ContextID,
                                                           cmsUInt32Number n,
                                                           cmsUInt32Number ColorantCount,
                                                           const char* Prefix, const char* Suffix);

CMSAPI void               CMSEXPORT cmsFreeNamedColorList(cmsContext ContextID, cmsNAMEDCOLORLIST* v);
CMSAPI cmsNAMEDCOLORLIST* CMSEXPORT cmsDupNamedColorList(cmsContext ContextID, const cmsNAMEDCOLORLIST* v);
CMSAPI cmsBool            CMSEXPORT cmsAppendNamedColor(cmsContext ContextID, cmsNAMEDCOLORLIST* v, const char* Name,
                                                            cmsUInt16Number PCS[3],
                                                            cmsUInt16Number Colorant[cmsMAXCHANNELS]);

CMSAPI cmsUInt32Number    CMSEXPORT cmsNamedColorCount(cmsContext ContextID, const cmsNAMEDCOLORLIST* v);
CMSAPI cmsInt32Number     CMSEXPORT cmsNamedColorIndex(cmsContext ContextID, const cmsNAMEDCOLORLIST* v, const char* Name);

CMSAPI cmsBool            CMSEXPORT cmsNamedColorInfo(cmsContext ContextID,
                                                      const cmsNAMEDCOLORLIST* NamedColorList, cmsUInt32Number nColor,
                                                      char* Name,
                                                      char* Prefix,
                                                      char* Suffix,
                                                      cmsUInt16Number* PCS,
                                                      cmsUInt16Number* Colorant);

// Retrieve named color list from transform
CMSAPI cmsNAMEDCOLORLIST* CMSEXPORT cmsGetNamedColorList(cmsHTRANSFORM xform);

// Profile sequence -----------------------------------------------------------------------------------------------------

// Profile sequence descriptor. Some fields come from profile sequence descriptor tag, others
// come from Profile Sequence Identifier Tag
typedef struct {

    cmsSignature           deviceMfg;
    cmsSignature           deviceModel;
    cmsUInt64Number        attributes;
    cmsTechnologySignature technology;
    cmsProfileID           ProfileID;
    cmsMLU*                Manufacturer;
    cmsMLU*                Model;
    cmsMLU*                Description;

} cmsPSEQDESC;

typedef struct {

    cmsUInt32Number n;
    cmsPSEQDESC*    seq;

} cmsSEQ;

CMSAPI cmsSEQ*           CMSEXPORT cmsAllocProfileSequenceDescription(cmsContext ContextID, cmsUInt32Number n);
CMSAPI cmsSEQ*           CMSEXPORT cmsDupProfileSequenceDescription(cmsContext ContextID, const cmsSEQ* pseq);
CMSAPI void              CMSEXPORT cmsFreeProfileSequenceDescription(cmsContext ContextID, cmsSEQ* pseq);

// Dictionaries --------------------------------------------------------------------------------------------------------

typedef struct _cmsDICTentry_struct {

    struct _cmsDICTentry_struct* Next;

    cmsMLU *DisplayName;
    cmsMLU *DisplayValue;
    wchar_t* Name;
    wchar_t* Value;

} cmsDICTentry;

CMSAPI cmsHANDLE           CMSEXPORT cmsDictAlloc(cmsContext ContextID);
CMSAPI void                CMSEXPORT cmsDictFree(cmsContext ContextID, cmsHANDLE hDict);
CMSAPI cmsHANDLE           CMSEXPORT cmsDictDup(cmsContext ContextID, cmsHANDLE hDict);

CMSAPI cmsBool             CMSEXPORT cmsDictAddEntry(cmsContext ContextID, cmsHANDLE hDict, const wchar_t* Name, const wchar_t* Value, const cmsMLU *DisplayName, const cmsMLU *DisplayValue);
CMSAPI const cmsDICTentry* CMSEXPORT cmsDictGetEntryList(cmsContext ContextID, cmsHANDLE hDict);
CMSAPI const cmsDICTentry* CMSEXPORT cmsDictNextEntry(cmsContext ContextID, const cmsDICTentry* e);

// Access to Profile data ----------------------------------------------------------------------------------------------
CMSAPI cmsHPROFILE       CMSEXPORT cmsCreateProfilePlaceholder(cmsContext ContextID);

CMSAPI cmsInt32Number    CMSEXPORT cmsGetTagCount(cmsContext ContextID, cmsHPROFILE hProfile);
CMSAPI cmsTagSignature   CMSEXPORT cmsGetTagSignature(cmsContext ContextID, cmsHPROFILE hProfile, cmsUInt32Number n);
CMSAPI cmsBool           CMSEXPORT cmsIsTag(cmsContext ContextID, cmsHPROFILE hProfile, cmsTagSignature sig);

// Read and write pre-formatted data
CMSAPI void*             CMSEXPORT cmsReadTag(cmsContext ContextID, cmsHPROFILE hProfile, cmsTagSignature sig);
CMSAPI cmsBool           CMSEXPORT cmsWriteTag(cmsContext ContextID, cmsHPROFILE hProfile, cmsTagSignature sig, const void* data);
CMSAPI cmsBool           CMSEXPORT cmsLinkTag(cmsContext ContextID, cmsHPROFILE hProfile, cmsTagSignature sig, cmsTagSignature dest);
CMSAPI cmsTagSignature   CMSEXPORT cmsTagLinkedTo(cmsContext ContextID, cmsHPROFILE hProfile, cmsTagSignature sig);

// Read and write raw data
CMSAPI cmsUInt32Number   CMSEXPORT cmsReadRawTag(cmsContext ContextID, cmsHPROFILE hProfile, cmsTagSignature sig, void* Buffer, cmsUInt32Number BufferSize);
CMSAPI cmsBool           CMSEXPORT cmsWriteRawTag(cmsContext ContextID, cmsHPROFILE hProfile, cmsTagSignature sig, const void* data, cmsUInt32Number Size);

// Access header data
#define cmsEmbeddedProfileFalse    0x00000000
#define cmsEmbeddedProfileTrue     0x00000001
#define cmsUseAnywhere             0x00000000
#define cmsUseWithEmbeddedDataOnly 0x00000002

CMSAPI cmsUInt32Number   CMSEXPORT cmsGetHeaderFlags(cmsContext ContextID, cmsHPROFILE hProfile);
CMSAPI void              CMSEXPORT cmsGetHeaderAttributes(cmsContext ContextID, cmsHPROFILE hProfile, cmsUInt64Number* Flags);
CMSAPI void              CMSEXPORT cmsGetHeaderProfileID(cmsContext ContextID, cmsHPROFILE hProfile, cmsUInt8Number* ProfileID);
CMSAPI cmsBool           CMSEXPORT cmsGetHeaderCreationDateTime(cmsContext ContextID, cmsHPROFILE hProfile, struct tm *Dest);
CMSAPI cmsUInt32Number   CMSEXPORT cmsGetHeaderRenderingIntent(cmsContext ContextID, cmsHPROFILE hProfile);

CMSAPI void              CMSEXPORT cmsSetHeaderFlags(cmsContext ContextID, cmsHPROFILE hProfile, cmsUInt32Number Flags);
CMSAPI cmsUInt32Number   CMSEXPORT cmsGetHeaderManufacturer(cmsContext ContextID, cmsHPROFILE hProfile);
CMSAPI void              CMSEXPORT cmsSetHeaderManufacturer(cmsContext ContextID, cmsHPROFILE hProfile, cmsUInt32Number manufacturer);
CMSAPI cmsUInt32Number   CMSEXPORT cmsGetHeaderCreator(cmsContext ContextID, cmsHPROFILE hProfile);
CMSAPI cmsUInt32Number   CMSEXPORT cmsGetHeaderModel(cmsContext ContextID, cmsHPROFILE hProfile);
CMSAPI void              CMSEXPORT cmsSetHeaderModel(cmsContext ContextID, cmsHPROFILE hProfile, cmsUInt32Number model);
CMSAPI void              CMSEXPORT cmsSetHeaderAttributes(cmsContext ContextID, cmsHPROFILE hProfile, cmsUInt64Number Flags);
CMSAPI void              CMSEXPORT cmsSetHeaderProfileID(cmsContext ContextID, cmsHPROFILE hProfile, cmsUInt8Number* ProfileID);
CMSAPI void              CMSEXPORT cmsSetHeaderRenderingIntent(cmsContext ContextID, cmsHPROFILE hProfile, cmsUInt32Number RenderingIntent);

CMSAPI cmsColorSpaceSignature
                         CMSEXPORT cmsGetPCS(cmsContext ContextID, cmsHPROFILE hProfile);
CMSAPI void              CMSEXPORT cmsSetPCS(cmsContext ContextID, cmsHPROFILE hProfile, cmsColorSpaceSignature pcs);
CMSAPI cmsColorSpaceSignature
                         CMSEXPORT cmsGetColorSpace(cmsContext ContextID, cmsHPROFILE hProfile);
CMSAPI void              CMSEXPORT cmsSetColorSpace(cmsContext ContextID, cmsHPROFILE hProfile, cmsColorSpaceSignature sig);
CMSAPI cmsProfileClassSignature
                         CMSEXPORT cmsGetDeviceClass(cmsContext ContextID, cmsHPROFILE hProfile);
CMSAPI void              CMSEXPORT cmsSetDeviceClass(cmsContext ContextID, cmsHPROFILE hProfile, cmsProfileClassSignature sig);
CMSAPI void              CMSEXPORT cmsSetProfileVersion(cmsContext ContextID, cmsHPROFILE hProfile, cmsFloat64Number Version);
CMSAPI cmsFloat64Number  CMSEXPORT cmsGetProfileVersion(cmsContext ContextID, cmsHPROFILE hProfile);

CMSAPI cmsUInt32Number   CMSEXPORT cmsGetEncodedICCversion(cmsContext ContextID, cmsHPROFILE hProfile);
CMSAPI void              CMSEXPORT cmsSetEncodedICCversion(cmsContext ContextID, cmsHPROFILE hProfile, cmsUInt32Number Version);

// How profiles may be used
#define LCMS_USED_AS_INPUT      0
#define LCMS_USED_AS_OUTPUT     1
#define LCMS_USED_AS_PROOF      2

CMSAPI cmsBool           CMSEXPORT cmsIsIntentSupported(cmsContext ContextID, cmsHPROFILE hProfile, cmsUInt32Number Intent, cmsUInt32Number UsedDirection);
CMSAPI cmsBool           CMSEXPORT cmsIsMatrixShaper(cmsContext ContextID, cmsHPROFILE hProfile);
CMSAPI cmsBool           CMSEXPORT cmsIsCLUT(cmsContext ContextID, cmsHPROFILE hProfile, cmsUInt32Number Intent, cmsUInt32Number UsedDirection);

// Translate form/to our notation to ICC
CMSAPI cmsColorSpaceSignature   CMSEXPORT _cmsICCcolorSpace(cmsContext ContextID, int OurNotation);
CMSAPI int                      CMSEXPORT _cmsLCMScolorSpace(cmsContext ContextID, cmsColorSpaceSignature ProfileSpace);

CMSAPI cmsUInt32Number   CMSEXPORT cmsChannelsOf(cmsContext ContextID, cmsColorSpaceSignature ColorSpace);

// Build a suitable formatter for the colorspace of this profile. nBytes=1 means 8 bits, nBytes=2 means 16 bits.
CMSAPI cmsUInt32Number   CMSEXPORT cmsFormatterForColorspaceOfProfile(cmsContext ContextID, cmsHPROFILE hProfile, cmsUInt32Number nBytes, cmsBool lIsFloat);
CMSAPI cmsUInt32Number   CMSEXPORT cmsFormatterForPCSOfProfile(cmsContext ContextID, cmsHPROFILE hProfile, cmsUInt32Number nBytes, cmsBool lIsFloat);


// Localized info
typedef enum {
             cmsInfoDescription  = 0,
             cmsInfoManufacturer = 1,
             cmsInfoModel        = 2,
             cmsInfoCopyright    = 3
} cmsInfoType;

CMSAPI cmsUInt32Number   CMSEXPORT cmsGetProfileInfo(cmsContext ContextID, cmsHPROFILE hProfile, cmsInfoType Info,
                                                            const char LanguageCode[3], const char CountryCode[3],
                                                            wchar_t* Buffer, cmsUInt32Number BufferSize);

CMSAPI cmsUInt32Number   CMSEXPORT cmsGetProfileInfoASCII(cmsContext ContextID, cmsHPROFILE hProfile, cmsInfoType Info,
                                                            const char LanguageCode[3], const char CountryCode[3],
                                                            char* Buffer, cmsUInt32Number BufferSize);

// IO handlers ----------------------------------------------------------------------------------------------------------

typedef struct _cms_io_handler cmsIOHANDLER;

CMSAPI cmsIOHANDLER*     CMSEXPORT cmsOpenIOhandlerFromFile(cmsContext ContextID, const char* FileName, const char* AccessMode);
CMSAPI cmsIOHANDLER*     CMSEXPORT cmsOpenIOhandlerFromStream(cmsContext ContextID, FILE* Stream);
CMSAPI cmsIOHANDLER*     CMSEXPORT cmsOpenIOhandlerFromMem(cmsContext ContextID, void *Buffer, cmsUInt32Number size, const char* AccessMode);
CMSAPI cmsIOHANDLER*     CMSEXPORT cmsOpenIOhandlerFromNULL(cmsContext ContextID);
CMSAPI cmsIOHANDLER*     CMSEXPORT cmsGetProfileIOhandler(cmsContext ContextID, cmsHPROFILE hProfile);
CMSAPI cmsBool           CMSEXPORT cmsCloseIOhandler(cmsContext ContextID, cmsIOHANDLER* io);

// MD5 message digest --------------------------------------------------------------------------------------------------

CMSAPI cmsBool           CMSEXPORT cmsMD5computeID(cmsContext ContextID, cmsHPROFILE hProfile);

// Profile high level functions ------------------------------------------------------------------------------------------

CMSAPI cmsHPROFILE      CMSEXPORT cmsOpenProfileFromFile(cmsContext ContextID, const char *ICCProfile, const char *sAccess);
CMSAPI cmsHPROFILE      CMSEXPORT cmsOpenProfileFromStream(cmsContext ContextID, FILE* ICCProfile, const char* sAccess);
CMSAPI cmsHPROFILE      CMSEXPORT cmsOpenProfileFromMem(cmsContext ContextID, const void * MemPtr, cmsUInt32Number dwSize);
CMSAPI cmsHPROFILE      CMSEXPORT cmsOpenProfileFromIOhandler(cmsContext ContextID, cmsIOHANDLER* io);
CMSAPI cmsHPROFILE      CMSEXPORT cmsOpenProfileFromIOhandler2(cmsContext ContextID, cmsIOHANDLER* io, cmsBool write);
CMSAPI cmsBool          CMSEXPORT cmsCloseProfile(cmsContext ContextID, cmsHPROFILE hProfile);

CMSAPI cmsBool          CMSEXPORT cmsSaveProfileToFile(cmsContext ContextID, cmsHPROFILE hProfile, const char* FileName);
CMSAPI cmsBool          CMSEXPORT cmsSaveProfileToStream(cmsContext ContextID, cmsHPROFILE hProfile, FILE* Stream);
CMSAPI cmsBool          CMSEXPORT cmsSaveProfileToMem(cmsContext ContextID, cmsHPROFILE hProfile, void *MemPtr, cmsUInt32Number* BytesNeeded);
CMSAPI cmsUInt32Number  CMSEXPORT cmsSaveProfileToIOhandler(cmsContext ContextID, cmsHPROFILE hProfile, cmsIOHANDLER* io);

// Predefined virtual profiles ------------------------------------------------------------------------------------------

CMSAPI cmsHPROFILE      CMSEXPORT cmsCreateRGBProfile(cmsContext ContextID,
                                                const cmsCIExyY* WhitePoint,
                                                const cmsCIExyYTRIPLE* Primaries,
                                                cmsToneCurve* const TransferFunction[3]);

CMSAPI cmsHPROFILE      CMSEXPORT cmsCreateGrayProfile(cmsContext ContextID,
                                                 const cmsCIExyY* WhitePoint,
                                                 const cmsToneCurve* TransferFunction);

CMSAPI cmsHPROFILE      CMSEXPORT cmsCreateLinearizationDeviceLink(cmsContext ContextID,
                                                                   cmsColorSpaceSignature ColorSpace,
                                                                   cmsToneCurve* const TransferFunctions[]);

CMSAPI cmsHPROFILE      CMSEXPORT cmsCreateInkLimitingDeviceLink(cmsContext ContextID,
                                                                 cmsColorSpaceSignature ColorSpace,
                                                                 cmsFloat64Number Limit);

CMSAPI cmsHPROFILE      CMSEXPORT cmsCreateLab2Profile(cmsContext ContextID,
                                                 const cmsCIExyY* WhitePoint);
CMSAPI cmsHPROFILE      CMSEXPORT cmsCreateLab4Profile(cmsContext ContextID,
                                                 const cmsCIExyY* WhitePoint);

CMSAPI cmsHPROFILE      CMSEXPORT cmsCreateXYZProfile(cmsContext ContextID);

CMSAPI cmsHPROFILE      CMSEXPORT cmsCreate_sRGBProfile(cmsContext ContextID);

CMSAPI cmsHPROFILE      CMSEXPORT cmsCreateBCHSWabstractProfile(cmsContext ContextID,
                                                                cmsUInt32Number nLUTPoints,
                                                                cmsFloat64Number Bright,
                                                                cmsFloat64Number Contrast,
                                                                cmsFloat64Number Hue,
                                                                cmsFloat64Number Saturation,
                                                                cmsUInt32Number TempSrc,
                                                                cmsUInt32Number TempDest);

CMSAPI cmsHPROFILE      CMSEXPORT cmsCreateNULLProfile(cmsContext ContextID);

// Converts a transform to a devicelink profile
CMSAPI cmsHPROFILE      CMSEXPORT cmsTransform2DeviceLink(cmsContext ContextID,
                                                          cmsHTRANSFORM hTransform,
                                                          cmsFloat64Number Version,
                                                          cmsUInt32Number dwFlags);

// Intents ----------------------------------------------------------------------------------------------

// ICC Intents
#define INTENT_PERCEPTUAL                              0
#define INTENT_RELATIVE_COLORIMETRIC                   1
#define INTENT_SATURATION                              2
#define INTENT_ABSOLUTE_COLORIMETRIC                   3

// Non-ICC intents
#define INTENT_PRESERVE_K_ONLY_PERCEPTUAL             10
#define INTENT_PRESERVE_K_ONLY_RELATIVE_COLORIMETRIC  11
#define INTENT_PRESERVE_K_ONLY_SATURATION             12
#define INTENT_PRESERVE_K_PLANE_PERCEPTUAL            13
#define INTENT_PRESERVE_K_PLANE_RELATIVE_COLORIMETRIC 14
#define INTENT_PRESERVE_K_PLANE_SATURATION            15

// Call with NULL as parameters to get the intent count
CMSAPI cmsUInt32Number  CMSEXPORT cmsGetSupportedIntents(cmsContext ContextID,
                                                         cmsUInt32Number nMax,
                                                         cmsUInt32Number* Codes,
                                                         char** Descriptions);

// Flags

#define cmsFLAGS_NOCACHE                  0x0040    // Inhibit 1-pixel cache
#define cmsFLAGS_NOOPTIMIZE               0x0100    // Inhibit optimizations
#define cmsFLAGS_NULLTRANSFORM            0x0200    // Don't transform anyway

// Proofing flags
#define cmsFLAGS_GAMUTCHECK               0x1000    // Out of Gamut alarm
#define cmsFLAGS_SOFTPROOFING             0x4000    // Do softproofing

// Misc
#define cmsFLAGS_BLACKPOINTCOMPENSATION   0x2000
#define cmsFLAGS_NOWHITEONWHITEFIXUP      0x0004    // Don't fix scum dot
#define cmsFLAGS_HIGHRESPRECALC           0x0400    // Use more memory to give better accuracy
#define cmsFLAGS_LOWRESPRECALC            0x0800    // Use less memory to minimize resources

// For devicelink creation
#define cmsFLAGS_8BITS_DEVICELINK         0x0008   // Create 8 bits devicelinks
#define cmsFLAGS_GUESSDEVICECLASS         0x0020   // Guess device class (for transform2devicelink)
#define cmsFLAGS_KEEP_SEQUENCE            0x0080   // Keep profile sequence for devicelink creation

// Specific to a particular optimizations
#define cmsFLAGS_FORCE_CLUT               0x0002    // Force CLUT optimization
#define cmsFLAGS_CLUT_POST_LINEARIZATION  0x0001    // create postlinearization tables if possible
#define cmsFLAGS_CLUT_PRE_LINEARIZATION   0x0010    // create prelinearization tables if possible

// Specific to unbounded mode
#define cmsFLAGS_NONEGATIVES              0x8000    // Prevent negative numbers in floating point transforms

// Copy alpha channels when transforming
#define cmsFLAGS_COPY_ALPHA               0x04000000 // Alpha channels are copied on cmsDoTransform()

// Unpremultiply/premultiply by final alpha value when transforming
#define cmsFLAGS_PREMULT                  0x08000000 // Data is multiplied by final alpha channel on cmsDoTransform()

// Fine-tune control over number of gridpoints
#define cmsFLAGS_GRIDPOINTS(n)           (((n) & 0xFF) << 16)

// CRD special
#define cmsFLAGS_NODEFAULTRESOURCEDEF     0x01000000

// Transforms ---------------------------------------------------------------------------------------------------

CMSAPI cmsHTRANSFORM    CMSEXPORT cmsCreateTransform(cmsContext ContextID,
                                                  cmsHPROFILE Input,
                                                  cmsUInt32Number InputFormat,
                                                  cmsHPROFILE Output,
                                                  cmsUInt32Number OutputFormat,
                                                  cmsUInt32Number Intent,
                                                  cmsUInt32Number dwFlags);

CMSAPI cmsHTRANSFORM    CMSEXPORT cmsCreateProofingTransform(cmsContext ContextID,
                                                  cmsHPROFILE Input,
                                                  cmsUInt32Number InputFormat,
                                                  cmsHPROFILE Output,
                                                  cmsUInt32Number OutputFormat,
                                                  cmsHPROFILE Proofing,
                                                  cmsUInt32Number Intent,
                                                  cmsUInt32Number ProofingIntent,
                                                  cmsUInt32Number dwFlags);

CMSAPI cmsHTRANSFORM    CMSEXPORT cmsCreateMultiprofileTransform(cmsContext ContextID,
                                                  cmsHPROFILE hProfiles[],
                                                  cmsUInt32Number nProfiles,
                                                  cmsUInt32Number InputFormat,
                                                  cmsUInt32Number OutputFormat,
                                                  cmsUInt32Number Intent,
                                                  cmsUInt32Number dwFlags);


CMSAPI cmsHTRANSFORM    CMSEXPORT cmsCreateExtendedTransform(cmsContext ContextID,
                                                   cmsUInt32Number nProfiles, cmsHPROFILE hProfiles[],
                                                   cmsBool  BPC[],
                                                   cmsUInt32Number Intents[],
                                                   cmsFloat64Number AdaptationStates[],
                                                   cmsHPROFILE hGamutProfile,
                                                   cmsUInt32Number nGamutPCSposition,
                                                   cmsUInt32Number InputFormat,
                                                   cmsUInt32Number OutputFormat,
                                                   cmsUInt32Number dwFlags);

CMSAPI void             CMSEXPORT cmsDeleteTransform(cmsContext ContextID, cmsHTRANSFORM hTransform);

CMSAPI void             CMSEXPORT cmsDoTransform(cmsContext ContextID,
                                                 cmsHTRANSFORM Transform,
                                                 const void * InputBuffer,
                                                 void * OutputBuffer,
                                                 cmsUInt32Number Size);

CMSAPI void             CMSEXPORT cmsDoTransformStride(cmsContext ContextID,      // Deprecated
                                                 cmsHTRANSFORM Transform,
                                                 const void * InputBuffer,
                                                 void * OutputBuffer,
                                                 cmsUInt32Number Size,
                                                 cmsUInt32Number Stride);

CMSAPI void             CMSEXPORT cmsDoTransformLineStride(cmsContext ContextID,
                                                 cmsHTRANSFORM  Transform,
                                                 const void* InputBuffer,
                                                 void* OutputBuffer,
                                                 cmsUInt32Number PixelsPerLine,
                                                 cmsUInt32Number LineCount,
                                                 cmsUInt32Number BytesPerLineIn,
                                                 cmsUInt32Number BytesPerLineOut,
                                                 cmsUInt32Number BytesPerPlaneIn,
                                                 cmsUInt32Number BytesPerPlaneOut);


CMSAPI void             CMSEXPORT cmsSetAlarmCodes(cmsContext ContextID,
                                             const cmsUInt16Number AlarmCodes[cmsMAXCHANNELS]);
CMSAPI void             CMSEXPORT cmsGetAlarmCodes(cmsContext ContextID,
                                                   cmsUInt16Number AlarmCodes[cmsMAXCHANNELS]);



// Adaptation state for absolute colorimetric intent
CMSAPI cmsFloat64Number CMSEXPORT cmsSetAdaptationState(cmsContext ContextID, cmsFloat64Number d);


// Grab the input/output formats
CMSAPI cmsUInt32Number CMSEXPORT cmsGetTransformInputFormat(cmsContext ContextID, cmsHTRANSFORM hTransform);
CMSAPI cmsUInt32Number CMSEXPORT cmsGetTransformOutputFormat(cmsContext ContextID, cmsHTRANSFORM hTransform);

cmsHTRANSFORM cmsCloneTransformChangingFormats(cmsContext ContextID,
                                               const cmsHTRANSFORM hTransform,
                                               cmsUInt32Number InputFormat,
                                               cmsUInt32Number OutputFormat);


// PostScript ColorRenderingDictionary and ColorSpaceArray ----------------------------------------------------

typedef enum { cmsPS_RESOURCE_CSA, cmsPS_RESOURCE_CRD } cmsPSResourceType;

// lcms2 unified method to access postscript color resources
CMSAPI cmsUInt32Number  CMSEXPORT cmsGetPostScriptColorResource(cmsContext ContextID,
                                                                cmsPSResourceType Type,
                                                                cmsHPROFILE hProfile,
                                                                cmsUInt32Number Intent,
                                                                cmsUInt32Number dwFlags,
                                                                cmsIOHANDLER* io);

CMSAPI cmsUInt32Number  CMSEXPORT cmsGetPostScriptCSA(cmsContext ContextID, cmsHPROFILE hProfile, cmsUInt32Number Intent, cmsUInt32Number dwFlags, void* Buffer, cmsUInt32Number dwBufferLen);
CMSAPI cmsUInt32Number  CMSEXPORT cmsGetPostScriptCRD(cmsContext ContextID, cmsHPROFILE hProfile, cmsUInt32Number Intent, cmsUInt32Number dwFlags, void* Buffer, cmsUInt32Number dwBufferLen);


// IT8.7 / CGATS.17-200x handling -----------------------------------------------------------------------------

CMSAPI cmsHANDLE        CMSEXPORT cmsIT8Alloc(cmsContext ContextID);
CMSAPI void             CMSEXPORT cmsIT8Free(cmsContext ContextID, cmsHANDLE hIT8);

// Tables
CMSAPI cmsUInt32Number  CMSEXPORT cmsIT8TableCount(cmsContext ContextID, cmsHANDLE hIT8);
CMSAPI cmsInt32Number   CMSEXPORT cmsIT8SetTable(cmsContext ContextID, cmsHANDLE hIT8, cmsUInt32Number nTable);

// Persistence
CMSAPI cmsHANDLE        CMSEXPORT cmsIT8LoadFromFile(cmsContext ContextID, const char* cFileName);
CMSAPI cmsHANDLE        CMSEXPORT cmsIT8LoadFromMem(cmsContext ContextID, const void *Ptr, cmsUInt32Number len);
// CMSAPI cmsHANDLE        CMSEXPORT cmsIT8LoadFromIOhandler(cmsContext ContextID, cmsIOHANDLER* io);

CMSAPI cmsBool          CMSEXPORT cmsIT8SaveToFile(cmsContext ContextID, cmsHANDLE hIT8, const char* cFileName);
CMSAPI cmsBool          CMSEXPORT cmsIT8SaveToMem(cmsContext ContextID, cmsHANDLE hIT8, void *MemPtr, cmsUInt32Number* BytesNeeded);

// Properties
CMSAPI const char*      CMSEXPORT cmsIT8GetSheetType(cmsContext ContextID, cmsHANDLE hIT8);
CMSAPI cmsBool          CMSEXPORT cmsIT8SetSheetType(cmsContext ContextID, cmsHANDLE hIT8, const char* Type);

CMSAPI cmsBool          CMSEXPORT cmsIT8SetComment(cmsContext ContextID, cmsHANDLE hIT8, const char* cComment);

CMSAPI cmsBool          CMSEXPORT cmsIT8SetPropertyStr(cmsContext ContextID, cmsHANDLE hIT8, const char* cProp, const char *Str);
CMSAPI cmsBool          CMSEXPORT cmsIT8SetPropertyDbl(cmsContext ContextID, cmsHANDLE hIT8, const char* cProp, cmsFloat64Number Val);
CMSAPI cmsBool          CMSEXPORT cmsIT8SetPropertyHex(cmsContext ContextID, cmsHANDLE hIT8, const char* cProp, cmsUInt32Number Val);
CMSAPI cmsBool          CMSEXPORT cmsIT8SetPropertyMulti(cmsContext ContextID, cmsHANDLE hIT8, const char* Key, const char* SubKey, const char *Buffer);
CMSAPI cmsBool          CMSEXPORT cmsIT8SetPropertyUncooked(cmsContext ContextID, cmsHANDLE hIT8, const char* Key, const char* Buffer);


CMSAPI const char*      CMSEXPORT cmsIT8GetProperty(cmsContext ContextID, cmsHANDLE hIT8, const char* cProp);
CMSAPI cmsFloat64Number CMSEXPORT cmsIT8GetPropertyDbl(cmsContext ContextID, cmsHANDLE hIT8, const char* cProp);
CMSAPI const char*      CMSEXPORT cmsIT8GetPropertyMulti(cmsContext ContextID, cmsHANDLE hIT8, const char* Key, const char *SubKey);
CMSAPI cmsUInt32Number  CMSEXPORT cmsIT8EnumProperties(cmsContext ContextID, cmsHANDLE hIT8, char ***PropertyNames);
CMSAPI cmsUInt32Number  CMSEXPORT cmsIT8EnumPropertyMulti(cmsContext ContextID, cmsHANDLE hIT8, const char* cProp, const char ***SubpropertyNames);

// Datasets
CMSAPI const char*      CMSEXPORT cmsIT8GetDataRowCol(cmsContext ContextID, cmsHANDLE hIT8, int row, int col);
CMSAPI cmsFloat64Number CMSEXPORT cmsIT8GetDataRowColDbl(cmsContext ContextID, cmsHANDLE hIT8, int row, int col);

CMSAPI cmsBool          CMSEXPORT cmsIT8SetDataRowCol(cmsContext ContextID, cmsHANDLE hIT8, int row, int col,
                                                const char* Val);

CMSAPI cmsBool          CMSEXPORT cmsIT8SetDataRowColDbl(cmsContext ContextID, cmsHANDLE hIT8, int row, int col,
                                                cmsFloat64Number Val);

CMSAPI const char*      CMSEXPORT cmsIT8GetData(cmsContext ContextID, cmsHANDLE hIT8, const char* cPatch, const char* cSample);


CMSAPI cmsFloat64Number CMSEXPORT cmsIT8GetDataDbl(cmsContext ContextID, cmsHANDLE hIT8, const char* cPatch, const char* cSample);

CMSAPI cmsBool          CMSEXPORT cmsIT8SetData(cmsContext ContextID, cmsHANDLE hIT8, const char* cPatch,
                                                const char* cSample,
                                                const char *Val);

CMSAPI cmsBool          CMSEXPORT cmsIT8SetDataDbl(cmsContext ContextID, cmsHANDLE hIT8, const char* cPatch,
                                                const char* cSample,
                                                cmsFloat64Number Val);

CMSAPI int              CMSEXPORT cmsIT8FindDataFormat(cmsContext ContextID, cmsHANDLE hIT8, const char* cSample);
CMSAPI cmsBool          CMSEXPORT cmsIT8SetDataFormat(cmsContext ContextID, cmsHANDLE hIT8, int n, const char *Sample);
CMSAPI int              CMSEXPORT cmsIT8EnumDataFormat(cmsContext ContextID, cmsHANDLE hIT8, char ***SampleNames);

CMSAPI const char*      CMSEXPORT cmsIT8GetPatchName(cmsContext ContextID, cmsHANDLE hIT8, int nPatch, char* buffer);
CMSAPI int              CMSEXPORT cmsIT8GetPatchByName(cmsContext ContextID, cmsHANDLE hIT8, const char *cPatch);

// The LABEL extension
CMSAPI int              CMSEXPORT cmsIT8SetTableByLabel(cmsContext ContextID, cmsHANDLE hIT8, const char* cSet, const char* cField, const char* ExpectedType);

CMSAPI cmsBool          CMSEXPORT cmsIT8SetIndexColumn(cmsContext ContextID, cmsHANDLE hIT8, const char* cSample);

// Formatter for double
CMSAPI void             CMSEXPORT cmsIT8DefineDblFormat(cmsContext ContextID, cmsHANDLE hIT8, const char* Formatter);

// Gamut boundary description routines ------------------------------------------------------------------------------

CMSAPI cmsHANDLE        CMSEXPORT cmsGBDAlloc(cmsContext ContextID);
CMSAPI void             CMSEXPORT cmsGBDFree(cmsContext ContextID, cmsHANDLE hGBD);
CMSAPI cmsBool          CMSEXPORT cmsGDBAddPoint(cmsContext ContextID, cmsHANDLE hGBD, const cmsCIELab* Lab);
CMSAPI cmsBool          CMSEXPORT cmsGDBCompute(cmsContext ContextID, cmsHANDLE  hGDB, cmsUInt32Number dwFlags);
CMSAPI cmsBool          CMSEXPORT cmsGDBCheckPoint(cmsContext ContextID, cmsHANDLE hGBD, const cmsCIELab* Lab);

// Feature detection  ----------------------------------------------------------------------------------------------

// Estimate the black point
CMSAPI cmsBool          CMSEXPORT cmsDetectBlackPoint(cmsContext ContextID, cmsCIEXYZ* BlackPoint, cmsHPROFILE hProfile, cmsUInt32Number Intent, cmsUInt32Number dwFlags);
CMSAPI cmsBool          CMSEXPORT cmsDetectDestinationBlackPoint(cmsContext ContextID, cmsCIEXYZ* BlackPoint, cmsHPROFILE hProfile, cmsUInt32Number Intent, cmsUInt32Number dwFlags);

// Estimate total area coverage
CMSAPI cmsFloat64Number CMSEXPORT cmsDetectTAC(cmsContext ContextID, cmsHPROFILE hProfile);

// Estimate gamma space, alwasys positive. Returns -1 on error.
CMSAPI cmsFloat64Number CMSEXPORT cmsDetectRGBProfileGamma(cmsContext ContextID, cmsHPROFILE hProfile, cmsFloat64Number thereshold);

// Poor man's gamut mapping
CMSAPI cmsBool          CMSEXPORT cmsDesaturateLab(cmsContext ContextID, cmsCIELab* Lab,
                                                   double amax, double amin,
                                                   double bmax, double bmin);

#ifndef CMS_USE_CPP_API
#   ifdef __cplusplus
    }
#   endif
#endif

#define _lcms2mt_H
#endif
