#ifndef _lcms2mt_H

#include <stdio.h>

#include <limits.h>
#include <time.h>
#include <stddef.h>

#ifndef CMS_USE_CPP_API
#   ifdef __cplusplus
#       if __cplusplus >= 201703L
#            define CMS_NO_REGISTER_KEYWORD 1
#       endif
extern "C" {
#   endif
#endif

#define LCMS_VERSION              (2190 - 2000)

#define LCMS2MT_VERSION_MIN (0)
#define LCMS2MT_VERSION_MAX (999)

#ifndef CMS_BASIC_TYPES_ALREADY_DEFINED

typedef unsigned char        cmsUInt8Number;
typedef signed char          cmsInt8Number;

#if CHAR_BIT != 8
#  error "Unable to find 8 bit type, unsupported compiler"
#endif

typedef float                cmsFloat32Number;
typedef double               cmsFloat64Number;

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

#if defined(CMS_NO_REGISTER_KEYWORD)
#  define CMSREGISTER
#else
#  define CMSREGISTER register
#endif

#ifdef CMS_DONT_USE_INT64
    typedef cmsUInt32Number      cmsUInt64Number[2];
    typedef cmsInt32Number       cmsInt64Number[2];
#   if defined(CMS_LARGE_FILE_SUPPORT)
#      error "You need int64 for large file support"
#   endif
#endif

typedef cmsUInt32Number      cmsSignature;
typedef cmsUInt16Number      cmsU8Fixed8Number;
typedef cmsInt32Number       cmsS15Fixed16Number;
typedef cmsUInt32Number      cmsU16Fixed16Number;

typedef int                  cmsBool;

#if defined (_WIN32) || defined(_WIN64) || defined(WIN32) || defined(_WIN32_)
#  define CMS_IS_WINDOWS_ 1
#endif

#ifdef _MSC_VER
#  define CMS_IS_WINDOWS_ 1
#endif

#ifdef __BORLANDC__
#  define CMS_IS_WINDOWS_ 1
#endif

#ifdef CMS_USE_BIG_ENDIAN

#  if CMS_USE_BIG_ENDIAN == 0
#    undef CMS_USE_BIG_ENDIAN
#  endif

#else

#  ifdef WORDS_BIGENDIAN
#    define CMS_USE_BIG_ENDIAN 1
#  else

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
#  endif

#  if defined(_HOST_BIG_ENDIAN) || defined(__BIG_ENDIAN__)
#    define CMS_USE_BIG_ENDIAN      1
#  endif

#endif

#if defined(CMS_IS_WINDOWS_) && !defined(__GNUC__)
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
#else
#  if defined(HAVE_FUNC_ATTRIBUTE_VISIBILITY) && !defined(CMS_NO_VISIBILITY)
#     define CMSEXPORT
#     define CMSAPI    __attribute__((visibility("default")))
#else
# define CMSEXPORT
# define CMSAPI
#endif
#endif

#ifdef HasTHREADS
# if HasTHREADS == 1
#    undef CMS_NO_PTHREADS
# else
#    define CMS_NO_PTHREADS 1
# endif
#endif

#ifdef LCMS2MT_PREFIX

#define LCMS2MT_XCAT(A,B) A##B
#define LCMS2MT_CAT(A,B) LCMS2MT_XCAT(A,B)
#define LCMS2MT_PREF(B) LCMS2MT_CAT(LCMS2MT_PREFIX,B)

#define _cms15Fixed16toDouble                    LCMS2MT_PREF(_cms15Fixed16toDouble)
#define _cms8Fixed8toDouble                      LCMS2MT_PREF(_cms8Fixed8toDouble)
#define cmsAdaptToIlluminant                     LCMS2MT_PREF(cmsAdaptToIlluminant)
#define _cmsAdjustEndianess16                    LCMS2MT_PREF(_cmsAdjustEndianess16)
#define _cmsAdjustEndianess32                    LCMS2MT_PREF(_cmsAdjustEndianess32)
#define _cmsAdjustEndianess64                    LCMS2MT_PREF(_cmsAdjustEndianess64)
#define cmsAllocNamedColorList                   LCMS2MT_PREF(cmsAllocNamedColorList)
#define cmsAllocProfileSequenceDescription       LCMS2MT_PREF(cmsAllocProfileSequenceDescription)
#define cmsAppendNamedColor                      LCMS2MT_PREF(cmsAppendNamedColor)
#define cmsBFDdeltaE                             LCMS2MT_PREF(cmsBFDdeltaE)
#define cmsBuildGamma                            LCMS2MT_PREF(cmsBuildGamma)
#define cmsBuildParametricToneCurve              LCMS2MT_PREF(cmsBuildParametricToneCurve)
#define cmsBuildSegmentedToneCurve               LCMS2MT_PREF(cmsBuildSegmentedToneCurve)
#define cmsBuildTabulatedToneCurve16             LCMS2MT_PREF(cmsBuildTabulatedToneCurve16)
#define cmsBuildTabulatedToneCurveFloat          LCMS2MT_PREF(cmsBuildTabulatedToneCurveFloat)
#define _cmsCalloc                               LCMS2MT_PREF(_cmsCalloc)
#define cmsChannelsOf                            LCMS2MT_PREF(cmsChannelsOf)
#define cmsChannelsOfColorSpace                  LCMS2MT_PREF(cmsChannelsOfColorSpace)
#define cmsCIE2000DeltaE                         LCMS2MT_PREF(cmsCIE2000DeltaE)
#define cmsCIE94DeltaE                           LCMS2MT_PREF(cmsCIE94DeltaE)
#define cmsCIECAM02Done                          LCMS2MT_PREF(cmsCIECAM02Done)
#define cmsCIECAM02Forward                       LCMS2MT_PREF(cmsCIECAM02Forward)
#define cmsCIECAM02Init                          LCMS2MT_PREF(cmsCIECAM02Init)
#define cmsCIECAM02Reverse                       LCMS2MT_PREF(cmsCIECAM02Reverse)
#define cmsCloseIOhandler                        LCMS2MT_PREF(cmsCloseIOhandler)
#define cmsCloseProfile                          LCMS2MT_PREF(cmsCloseProfile)
#define cmsCMCdeltaE                             LCMS2MT_PREF(cmsCMCdeltaE)
#define cmsCreate_sRGBProfile                    LCMS2MT_PREF(cmsCreate_sRGBProfile)
#define cmsCreateBCHSWabstractProfile            LCMS2MT_PREF(cmsCreateBCHSWabstractProfile)
#define cmsCreateExtendedTransform               LCMS2MT_PREF(cmsCreateExtendedTransform)
#define cmsCreateGrayProfile                     LCMS2MT_PREF(cmsCreateGrayProfile)
#define cmsCreateInkLimitingDeviceLink           LCMS2MT_PREF(cmsCreateInkLimitingDeviceLink)
#define cmsCreateLab2Profile                     LCMS2MT_PREF(cmsCreateLab2Profile)
#define cmsCreateLab4Profile                     LCMS2MT_PREF(cmsCreateLab4Profile)
#define cmsCreateLinearizationDeviceLink         LCMS2MT_PREF(cmsCreateLinearizationDeviceLink)
#define cmsCreateMultiprofileTransform           LCMS2MT_PREF(cmsCreateMultiprofileTransform)
#define cmsCreateNULLProfile                     LCMS2MT_PREF(cmsCreateNULLProfile)
#define cmsCreateProfilePlaceholder              LCMS2MT_PREF(cmsCreateProfilePlaceholder)
#define cmsCreateProofingTransform               LCMS2MT_PREF(cmsCreateProofingTransform)
#define cmsCreateRGBProfile                      LCMS2MT_PREF(cmsCreateRGBProfile)
#define cmsCreateTransform                       LCMS2MT_PREF(cmsCreateTransform)
#define cmsCreateXYZProfile                      LCMS2MT_PREF(cmsCreateXYZProfile)
#define cmsD50_xyY                               LCMS2MT_PREF(cmsD50_xyY)
#define cmsD50_XYZ                               LCMS2MT_PREF(cmsD50_XYZ)
#define _cmsDecodeDateTimeNumber                 LCMS2MT_PREF(_cmsDecodeDateTimeNumber)
#define _cmsDefaultICCintents                    LCMS2MT_PREF(_cmsDefaultICCintents)
#define cmsDeleteTransform                       LCMS2MT_PREF(cmsDeleteTransform)
#define cmsDeltaE                                LCMS2MT_PREF(cmsDeltaE)
#define cmsDetectBlackPoint                      LCMS2MT_PREF(cmsDetectBlackPoint)
#define cmsDetectDestinationBlackPoint           LCMS2MT_PREF(cmsDetectDestinationBlackPoint)
#define cmsDetectTAC                             LCMS2MT_PREF(cmsDetectTAC)
#define cmsDesaturateLab                         LCMS2MT_PREF(cmsDesaturateLab)
#define cmsDoTransform                           LCMS2MT_PREF(cmsDoTransform)
#define cmsDoTransformStride                     LCMS2MT_PREF(cmsDoTransformStride)
#define cmsDoTransformLineStride                 LCMS2MT_PREF(cmsDoTransformLineStride)
#define _cmsDoubleTo15Fixed16                    LCMS2MT_PREF(_cmsDoubleTo15Fixed16)
#define _cmsDoubleTo8Fixed8                      LCMS2MT_PREF(_cmsDoubleTo8Fixed8)
#define _cmsDupMem                               LCMS2MT_PREF(_cmsDupMem)
#define cmsDupNamedColorList                     LCMS2MT_PREF(cmsDupNamedColorList)
#define cmsDupProfileSequenceDescription         LCMS2MT_PREF(cmsDupProfileSequenceDescription)
#define cmsDupToneCurve                          LCMS2MT_PREF(cmsDupToneCurve)
#define _cmsEncodeDateTimeNumber                 LCMS2MT_PREF(_cmsEncodeDateTimeNumber)
#define cmsEstimateGamma                         LCMS2MT_PREF(cmsEstimateGamma)
#define cmsGetToneCurveEstimatedTableEntries     LCMS2MT_PREF(cmsGetToneCurveEstimatedTableEntries)
#define cmsGetToneCurveEstimatedTable            LCMS2MT_PREF(cmsGetToneCurveEstimatedTable)
#define cmsEvalToneCurve16                       LCMS2MT_PREF(cmsEvalToneCurve16)
#define cmsEvalToneCurveFloat                    LCMS2MT_PREF(cmsEvalToneCurveFloat)
#define cmsfilelength                            LCMS2MT_PREF(cmsfilelength)
#define cmsFloat2LabEncoded                      LCMS2MT_PREF(cmsFloat2LabEncoded)
#define cmsFloat2LabEncodedV2                    LCMS2MT_PREF(cmsFloat2LabEncodedV2)
#define cmsFloat2XYZEncoded                      LCMS2MT_PREF(cmsFloat2XYZEncoded)
#define cmsFormatterForColorspaceOfProfile       LCMS2MT_PREF(cmsFormatterForColorspaceOfProfile)
#define cmsFormatterForPCSOfProfile              LCMS2MT_PREF(cmsFormatterForPCSOfProfile)
#define _cmsFree                                 LCMS2MT_PREF(_cmsFree)
#define cmsFreeNamedColorList                    LCMS2MT_PREF(cmsFreeNamedColorList)
#define cmsFreeProfileSequenceDescription        LCMS2MT_PREF(cmsFreeProfileSequenceDescription)
#define cmsFreeToneCurve                         LCMS2MT_PREF(cmsFreeToneCurve)
#define cmsFreeToneCurveTriple                   LCMS2MT_PREF(cmsFreeToneCurveTriple)
#define cmsGBDAlloc                              LCMS2MT_PREF(cmsGBDAlloc)
#define cmsGBDFree                               LCMS2MT_PREF(cmsGBDFree)
#define cmsGDBAddPoint                           LCMS2MT_PREF(cmsGDBAddPoint)
#define cmsGDBCheckPoint                         LCMS2MT_PREF(cmsGDBCheckPoint)
#define cmsGDBCompute                            LCMS2MT_PREF(cmsGDBCompute)
#define cmsGetAlarmCodes                         LCMS2MT_PREF(cmsGetAlarmCodes)
#define cmsGetColorSpace                         LCMS2MT_PREF(cmsGetColorSpace)
#define cmsGetDeviceClass                        LCMS2MT_PREF(cmsGetDeviceClass)
#define cmsGetEncodedICCversion                  LCMS2MT_PREF(cmsGetEncodedICCversion)
#define cmsGetHeaderAttributes                   LCMS2MT_PREF(cmsGetHeaderAttributes)
#define cmsGetHeaderCreationDateTime             LCMS2MT_PREF(cmsGetHeaderCreationDateTime)
#define cmsGetHeaderFlags                        LCMS2MT_PREF(cmsGetHeaderFlags)
#define cmsGetHeaderManufacturer                 LCMS2MT_PREF(cmsGetHeaderManufacturer)
#define cmsGetHeaderModel                        LCMS2MT_PREF(cmsGetHeaderModel)
#define cmsGetHeaderProfileID                    LCMS2MT_PREF(cmsGetHeaderProfileID)
#define cmsGetHeaderRenderingIntent              LCMS2MT_PREF(cmsGetHeaderRenderingIntent)
#define cmsGetNamedColorList                     LCMS2MT_PREF(cmsGetNamedColorList)
#define cmsGetPCS                                LCMS2MT_PREF(cmsGetPCS)
#define cmsGetPostScriptColorResource            LCMS2MT_PREF(cmsGetPostScriptColorResource)
#define cmsGetPostScriptCRD                      LCMS2MT_PREF(cmsGetPostScriptCRD)
#define cmsGetPostScriptCSA                      LCMS2MT_PREF(cmsGetPostScriptCSA)
#define cmsGetProfileInfo                        LCMS2MT_PREF(cmsGetProfileInfo)
#define cmsGetProfileInfoASCII                   LCMS2MT_PREF(cmsGetProfileInfoASCII)
#define cmsGetProfileInfoUTF8                    LCMS2MT_PREF(cmsGetProfileInfoUTF8)
#define cmsGetProfileVersion                     LCMS2MT_PREF(cmsGetProfileVersion)
#define cmsGetSupportedIntents                   LCMS2MT_PREF(cmsGetSupportedIntents)
#define cmsGetTagCount                           LCMS2MT_PREF(cmsGetTagCount)
#define cmsGetTagSignature                       LCMS2MT_PREF(cmsGetTagSignature)
#define _cmsICCcolorSpace                        LCMS2MT_PREF(_cmsICCcolorSpace)
#define _cmsIOPrintf                             LCMS2MT_PREF(_cmsIOPrintf)
#define cmsIsCLUT                                LCMS2MT_PREF(cmsIsCLUT)
#define cmsIsIntentSupported                     LCMS2MT_PREF(cmsIsIntentSupported)
#define cmsIsMatrixShaper                        LCMS2MT_PREF(cmsIsMatrixShaper)
#define cmsIsTag                                 LCMS2MT_PREF(cmsIsTag)
#define cmsIsToneCurveDescending                 LCMS2MT_PREF(cmsIsToneCurveDescending)
#define cmsIsToneCurveLinear                     LCMS2MT_PREF(cmsIsToneCurveLinear)
#define cmsIsToneCurveMonotonic                  LCMS2MT_PREF(cmsIsToneCurveMonotonic)
#define cmsIsToneCurveMultisegment               LCMS2MT_PREF(cmsIsToneCurveMultisegment)
#define cmsGetToneCurveParametricType            LCMS2MT_PREF(cmsGetToneCurveParametricType)
#define cmsIT8Alloc                              LCMS2MT_PREF(cmsIT8Alloc)
#define cmsIT8DefineDblFormat                    LCMS2MT_PREF(cmsIT8DefineDblFormat)
#define cmsIT8EnumDataFormat                     LCMS2MT_PREF(cmsIT8EnumDataFormat)
#define cmsIT8EnumProperties                     LCMS2MT_PREF(cmsIT8EnumProperties)
#define cmsIT8EnumPropertyMulti                  LCMS2MT_PREF(cmsIT8EnumPropertyMulti)
#define cmsIT8Free                               LCMS2MT_PREF(cmsIT8Free)
#define cmsIT8GetData                            LCMS2MT_PREF(cmsIT8GetData)
#define cmsIT8GetDataDbl                         LCMS2MT_PREF(cmsIT8GetDataDbl)
#define cmsIT8FindDataFormat                     LCMS2MT_PREF(cmsIT8FindDataFormat)
#define cmsIT8GetDataRowCol                      LCMS2MT_PREF(cmsIT8GetDataRowCol)
#define cmsIT8GetDataRowColDbl                   LCMS2MT_PREF(cmsIT8GetDataRowColDbl)
#define cmsIT8GetPatchName                       LCMS2MT_PREF(cmsIT8GetPatchName)
#define cmsIT8GetPatchByName                     LCMS2MT_PREF(cmsIT8GetPatchByName)
#define cmsIT8GetProperty                        LCMS2MT_PREF(cmsIT8GetProperty)
#define cmsIT8GetPropertyDbl                     LCMS2MT_PREF(cmsIT8GetPropertyDbl)
#define cmsIT8GetPropertyMulti                   LCMS2MT_PREF(cmsIT8GetPropertyMulti)
#define cmsIT8GetSheetType                       LCMS2MT_PREF(cmsIT8GetSheetType)
#define cmsIT8LoadFromFile                       LCMS2MT_PREF(cmsIT8LoadFromFile)
#define cmsIT8LoadFromMem                        LCMS2MT_PREF(cmsIT8LoadFromMem)
#define cmsIT8SaveToFile                         LCMS2MT_PREF(cmsIT8SaveToFile)
#define cmsIT8SaveToMem                          LCMS2MT_PREF(cmsIT8SaveToMem)
#define cmsIT8SetComment                         LCMS2MT_PREF(cmsIT8SetComment)
#define cmsIT8SetData                            LCMS2MT_PREF(cmsIT8SetData)
#define cmsIT8SetDataDbl                         LCMS2MT_PREF(cmsIT8SetDataDbl)
#define cmsIT8SetDataFormat                      LCMS2MT_PREF(cmsIT8SetDataFormat)
#define cmsIT8SetDataRowCol                      LCMS2MT_PREF(cmsIT8SetDataRowCol)
#define cmsIT8SetDataRowColDbl                   LCMS2MT_PREF(cmsIT8SetDataRowColDbl)
#define cmsIT8SetPropertyDbl                     LCMS2MT_PREF(cmsIT8SetPropertyDbl)
#define cmsIT8SetPropertyHex                     LCMS2MT_PREF(cmsIT8SetPropertyHex)
#define cmsIT8SetPropertyStr                     LCMS2MT_PREF(cmsIT8SetPropertyStr)
#define cmsIT8SetPropertyMulti                   LCMS2MT_PREF(cmsIT8SetPropertyMulti)
#define cmsIT8SetPropertyUncooked                LCMS2MT_PREF(cmsIT8SetPropertyUncooked)
#define cmsIT8SetSheetType                       LCMS2MT_PREF(cmsIT8SetSheetType)
#define cmsIT8SetTable                           LCMS2MT_PREF(cmsIT8SetTable)
#define cmsIT8SetTableByLabel                    LCMS2MT_PREF(cmsIT8SetTableByLabel)
#define cmsIT8SetIndexColumn                     LCMS2MT_PREF(cmsIT8SetIndexColumn)
#define cmsIT8TableCount                         LCMS2MT_PREF(cmsIT8TableCount)
#define cmsJoinToneCurve                         LCMS2MT_PREF(cmsJoinToneCurve)
#define cmsLab2LCh                               LCMS2MT_PREF(cmsLab2LCh)
#define cmsLab2XYZ                               LCMS2MT_PREF(cmsLab2XYZ)
#define cmsLabEncoded2Float                      LCMS2MT_PREF(cmsLabEncoded2Float)
#define cmsLabEncoded2FloatV2                    LCMS2MT_PREF(cmsLabEncoded2FloatV2)
#define cmsLCh2Lab                               LCMS2MT_PREF(cmsLCh2Lab)
#define _cmsLCMScolorSpace                       LCMS2MT_PREF(_cmsLCMScolorSpace)
#define cmsLinkTag                               LCMS2MT_PREF(cmsLinkTag)
#define cmsTagLinkedTo                           LCMS2MT_PREF(cmsTagLinkedTo)
#define cmsPipelineAlloc                         LCMS2MT_PREF(cmsPipelineAlloc)
#define cmsPipelineCat                           LCMS2MT_PREF(cmsPipelineCat)
#define cmsPipelineCheckAndRetreiveStages        LCMS2MT_PREF(cmsPipelineCheckAndRetreiveStages)
#define cmsPipelineDup                           LCMS2MT_PREF(cmsPipelineDup)
#define cmsPipelineStageCount                    LCMS2MT_PREF(cmsPipelineStageCount)
#define cmsPipelineEval16                        LCMS2MT_PREF(cmsPipelineEval16)
#define cmsPipelineEvalFloat                     LCMS2MT_PREF(cmsPipelineEvalFloat)
#define cmsPipelineEvalReverseFloat              LCMS2MT_PREF(cmsPipelineEvalReverseFloat)
#define cmsPipelineFree                          LCMS2MT_PREF(cmsPipelineFree)
#define cmsPipelineGetPtrToFirstStage            LCMS2MT_PREF(cmsPipelineGetPtrToFirstStage)
#define cmsPipelineGetPtrToLastStage             LCMS2MT_PREF(cmsPipelineGetPtrToLastStage)
#define cmsPipelineInputChannels                 LCMS2MT_PREF(cmsPipelineInputChannels)
#define cmsPipelineInsertStage                   LCMS2MT_PREF(cmsPipelineInsertStage)
#define cmsPipelineOutputChannels                LCMS2MT_PREF(cmsPipelineOutputChannels)
#define cmsPipelineSetSaveAs8bitsFlag            LCMS2MT_PREF(cmsPipelineSetSaveAs8bitsFlag)
#define _cmsPipelineSetOptimizationParameters    LCMS2MT_PREF(_cmsPipelineSetOptimizationParameters)
#define cmsPipelineUnlinkStage                   LCMS2MT_PREF(cmsPipelineUnlinkStage)
#define _cmsMalloc                               LCMS2MT_PREF(_cmsMalloc)
#define _cmsMallocZero                           LCMS2MT_PREF(_cmsMallocZero)
#define _cmsMAT3eval                             LCMS2MT_PREF(_cmsMAT3eval)
#define _cmsMAT3identity                         LCMS2MT_PREF(_cmsMAT3identity)
#define _cmsMAT3inverse                          LCMS2MT_PREF(_cmsMAT3inverse)
#define _cmsMAT3isIdentity                       LCMS2MT_PREF(_cmsMAT3isIdentity)
#define _cmsMAT3per                              LCMS2MT_PREF(_cmsMAT3per)
#define _cmsMAT3solve                            LCMS2MT_PREF(_cmsMAT3solve)
#define cmsMD5computeID                          LCMS2MT_PREF(cmsMD5computeID)
#define cmsMLUalloc                              LCMS2MT_PREF(cmsMLUalloc)
#define cmsMLUdup                                LCMS2MT_PREF(cmsMLUdup)
#define cmsMLUfree                               LCMS2MT_PREF(cmsMLUfree)
#define cmsMLUgetASCII                           LCMS2MT_PREF(cmsMLUgetASCII)
#define cmsMLUgetTranslation                     LCMS2MT_PREF(cmsMLUgetTranslation)
#define cmsMLUgetWide                            LCMS2MT_PREF(cmsMLUgetWide)
#define cmsMLUgetUTF8                            LCMS2MT_PREF(cmsMLUgetUTF8)
#define cmsMLUsetASCII                           LCMS2MT_PREF(cmsMLUsetASCII)
#define cmsMLUsetWide                            LCMS2MT_PREF(cmsMLUsetWide)
#define cmsMLUsetUTF8                            LCMS2MT_PREF(cmsMLUsetUTF8)
#define cmsStageAllocCLut16bit                   LCMS2MT_PREF(cmsStageAllocCLut16bit)
#define cmsStageAllocCLut16bitGranular           LCMS2MT_PREF(cmsStageAllocCLut16bitGranular)
#define cmsStageAllocCLutFloat                   LCMS2MT_PREF(cmsStageAllocCLutFloat)
#define cmsStageAllocCLutFloatGranular           LCMS2MT_PREF(cmsStageAllocCLutFloatGranular)
#define cmsStageAllocToneCurves                  LCMS2MT_PREF(cmsStageAllocToneCurves)
#define cmsStageAllocIdentity                    LCMS2MT_PREF(cmsStageAllocIdentity)
#define cmsStageAllocMatrix                      LCMS2MT_PREF(cmsStageAllocMatrix)
#define _cmsStageAllocPlaceholder                LCMS2MT_PREF(_cmsStageAllocPlaceholder)
#define cmsStageDup                              LCMS2MT_PREF(cmsStageDup)
#define cmsStageFree                             LCMS2MT_PREF(cmsStageFree)
#define cmsStageNext                             LCMS2MT_PREF(cmsStageNext)
#define cmsStageInputChannels                    LCMS2MT_PREF(cmsStageInputChannels)
#define cmsStageOutputChannels                   LCMS2MT_PREF(cmsStageOutputChannels)
#define cmsStageSampleCLut16bit                  LCMS2MT_PREF(cmsStageSampleCLut16bit)
#define cmsStageSampleCLutFloat                  LCMS2MT_PREF(cmsStageSampleCLutFloat)
#define cmsStageType                             LCMS2MT_PREF(cmsStageType)
#define cmsStageData                             LCMS2MT_PREF(cmsStageData)
#define cmsNamedColorCount                       LCMS2MT_PREF(cmsNamedColorCount)
#define cmsNamedColorIndex                       LCMS2MT_PREF(cmsNamedColorIndex)
#define cmsNamedColorInfo                        LCMS2MT_PREF(cmsNamedColorInfo)
#define cmsOpenIOhandlerFromFile                 LCMS2MT_PREF(cmsOpenIOhandlerFromFile)
#define cmsOpenIOhandlerFromMem                  LCMS2MT_PREF(cmsOpenIOhandlerFromMem)
#define cmsOpenIOhandlerFromNULL                 LCMS2MT_PREF(cmsOpenIOhandlerFromNULL)
#define cmsOpenIOhandlerFromStream               LCMS2MT_PREF(cmsOpenIOhandlerFromStream)
#define cmsOpenProfileFromFile                   LCMS2MT_PREF(cmsOpenProfileFromFile)
#define cmsOpenProfileFromIOhandler              LCMS2MT_PREF(cmsOpenProfileFromIOhandler)
#define cmsOpenProfileFromIOhandler2             LCMS2MT_PREF(cmsOpenProfileFromIOhandler2)
#define cmsOpenProfileFromMem                    LCMS2MT_PREF(cmsOpenProfileFromMem)
#define cmsOpenProfileFromStream                 LCMS2MT_PREF(cmsOpenProfileFromStream)
#define cmsCreateDeviceLinkFromCubeFile		 LCMS2MT_PREF(cmsCreateDeviceLinkFromCubeFile)
#define cmsPlugin                                LCMS2MT_PREF(cmsPlugin)
#define _cmsRead15Fixed16Number                  LCMS2MT_PREF(_cmsRead15Fixed16Number)
#define _cmsReadAlignment                        LCMS2MT_PREF(_cmsReadAlignment)
#define _cmsReadFloat32Number                    LCMS2MT_PREF(_cmsReadFloat32Number)
#define cmsReadRawTag                            LCMS2MT_PREF(cmsReadRawTag)
#define cmsReadTag                               LCMS2MT_PREF(cmsReadTag)
#define _cmsReadTypeBase                         LCMS2MT_PREF(_cmsReadTypeBase)
#define _cmsReadUInt16Array                      LCMS2MT_PREF(_cmsReadUInt16Array)
#define _cmsReadUInt16Number                     LCMS2MT_PREF(_cmsReadUInt16Number)
#define _cmsReadUInt32Number                     LCMS2MT_PREF(_cmsReadUInt32Number)
#define _cmsReadUInt64Number                     LCMS2MT_PREF(_cmsReadUInt64Number)
#define _cmsReadUInt8Number                      LCMS2MT_PREF(_cmsReadUInt8Number)
#define _cmsReadXYZNumber                        LCMS2MT_PREF(_cmsReadXYZNumber)
#define _cmsRealloc                              LCMS2MT_PREF(_cmsRealloc)
#define cmsReverseToneCurve                      LCMS2MT_PREF(cmsReverseToneCurve)
#define cmsReverseToneCurveEx                    LCMS2MT_PREF(cmsReverseToneCurveEx)
#define cmsSaveProfileToFile                     LCMS2MT_PREF(cmsSaveProfileToFile)
#define cmsSaveProfileToIOhandler                LCMS2MT_PREF(cmsSaveProfileToIOhandler)
#define cmsSaveProfileToMem                      LCMS2MT_PREF(cmsSaveProfileToMem)
#define cmsSaveProfileToStream                   LCMS2MT_PREF(cmsSaveProfileToStream)
#define cmsSetAdaptationState                    LCMS2MT_PREF(cmsSetAdaptationState)
#define cmsSetAlarmCodes                         LCMS2MT_PREF(cmsSetAlarmCodes)
#define cmsSetColorSpace                         LCMS2MT_PREF(cmsSetColorSpace)
#define cmsSetDeviceClass                        LCMS2MT_PREF(cmsSetDeviceClass)
#define cmsSetEncodedICCversion                  LCMS2MT_PREF(cmsSetEncodedICCversion)
#define cmsSetHeaderAttributes                   LCMS2MT_PREF(cmsSetHeaderAttributes)
#define cmsSetHeaderFlags                        LCMS2MT_PREF(cmsSetHeaderFlags)
#define cmsSetHeaderManufacturer                 LCMS2MT_PREF(cmsSetHeaderManufacturer)
#define cmsSetHeaderModel                        LCMS2MT_PREF(cmsSetHeaderModel)
#define cmsSetHeaderProfileID                    LCMS2MT_PREF(cmsSetHeaderProfileID)
#define cmsSetHeaderRenderingIntent              LCMS2MT_PREF(cmsSetHeaderRenderingIntent)
#define cmsSetLogErrorHandler                    LCMS2MT_PREF(cmsSetLogErrorHandler)
#define cmsSetPCS                                LCMS2MT_PREF(cmsSetPCS)
#define cmsSetProfileVersion                     LCMS2MT_PREF(cmsSetProfileVersion)
#define cmsSignalError                           LCMS2MT_PREF(cmsSignalError)
#define cmsSmoothToneCurve                       LCMS2MT_PREF(cmsSmoothToneCurve)
#define cmsstrcasecmp                            LCMS2MT_PREF(cmsstrcasecmp)
#define cmsTempFromWhitePoint                    LCMS2MT_PREF(cmsTempFromWhitePoint)
#define cmsTransform2DeviceLink                  LCMS2MT_PREF(cmsTransform2DeviceLink)
#define cmsUnregisterPlugins                     LCMS2MT_PREF(cmsUnregisterPlugins)
#define _cmsVEC3cross                            LCMS2MT_PREF(_cmsVEC3cross)
#define _cmsVEC3distance                         LCMS2MT_PREF(_cmsVEC3distance)
#define _cmsVEC3dot                              LCMS2MT_PREF(_cmsVEC3dot)
#define _cmsVEC3init                             LCMS2MT_PREF(_cmsVEC3init)
#define _cmsVEC3length                           LCMS2MT_PREF(_cmsVEC3length)
#define _cmsVEC3minus                            LCMS2MT_PREF(_cmsVEC3minus)
#define cmsWhitePointFromTemp                    LCMS2MT_PREF(cmsWhitePointFromTemp)
#define _cmsWrite15Fixed16Number                 LCMS2MT_PREF(_cmsWrite15Fixed16Number)
#define _cmsWriteAlignment                       LCMS2MT_PREF(_cmsWriteAlignment)
#define _cmsWriteFloat32Number                   LCMS2MT_PREF(_cmsWriteFloat32Number)
#define cmsWriteRawTag                           LCMS2MT_PREF(cmsWriteRawTag)
#define cmsWriteTag                              LCMS2MT_PREF(cmsWriteTag)
#define _cmsWriteTypeBase                        LCMS2MT_PREF(_cmsWriteTypeBase)
#define _cmsWriteUInt16Array                     LCMS2MT_PREF(_cmsWriteUInt16Array)
#define _cmsWriteUInt16Number                    LCMS2MT_PREF(_cmsWriteUInt16Number)
#define _cmsWriteUInt32Number                    LCMS2MT_PREF(_cmsWriteUInt32Number)
#define _cmsWriteUInt64Number                    LCMS2MT_PREF(_cmsWriteUInt64Number)
#define _cmsWriteUInt8Number                     LCMS2MT_PREF(_cmsWriteUInt8Number)
#define _cmsWriteXYZNumber                       LCMS2MT_PREF(_cmsWriteXYZNumber)
#define cmsxyY2XYZ                               LCMS2MT_PREF(cmsxyY2XYZ)
#define cmsXYZ2Lab                               LCMS2MT_PREF(cmsXYZ2Lab)
#define cmsXYZ2xyY                               LCMS2MT_PREF(cmsXYZ2xyY)
#define cmsXYZEncoded2Float                      LCMS2MT_PREF(cmsXYZEncoded2Float)
#define cmsSliceSpace16                          LCMS2MT_PREF(cmsSliceSpace16)
#define cmsSliceSpaceFloat                       LCMS2MT_PREF(cmsSliceSpaceFloat)
#define cmsCloneTransformChangingFormats         LCMS2MT_PREF(cmsCloneTransformChangingFormats)
#define cmsDictAlloc                             LCMS2MT_PREF(cmsDictAlloc)
#define cmsDictFree                              LCMS2MT_PREF(cmsDictFree)
#define cmsDictDup                               LCMS2MT_PREF(cmsDictDup)
#define cmsDictAddEntry                          LCMS2MT_PREF(cmsDictAddEntry)
#define cmsDictGetEntryList                      LCMS2MT_PREF(cmsDictGetEntryList)
#define cmsDictNextEntry                         LCMS2MT_PREF(cmsDictNextEntry)
#define _cmsGetTransformUserData                 LCMS2MT_PREF(_cmsGetTransformUserData)
#define _cmsSetTransformUserData                 LCMS2MT_PREF(_cmsSetTransformUserData)
#define _cmsGetTransformFormatters16             LCMS2MT_PREF(_cmsGetTransformFormatters16)
#define _cmsGetTransformFormattersFloat          LCMS2MT_PREF(_cmsGetTransformFormattersFloat)
#define cmsGetHeaderCreator                      LCMS2MT_PREF(cmsGetHeaderCreator)
#define cmsPlugin                                LCMS2MT_PREF(cmsPlugin)
#define cmsGetTransformInputFormat               LCMS2MT_PREF(cmsGetTransformInputFormat)
#define cmsGetTransformOutputFormat              LCMS2MT_PREF(cmsGetTransformOutputFormat)
#define cmsCreateContext                         LCMS2MT_PREF(cmsCreateContext)
#define cmsDupContext                            LCMS2MT_PREF(cmsDupContext)
#define cmsDeleteContext                         LCMS2MT_PREF(cmsDeleteContext)
#define cmsGetContextUserData                    LCMS2MT_PREF(cmsGetContextUserData)
#define cmsUnregisterPlugins                     LCMS2MT_PREF(cmsUnregisterPlugins)
#define cmsSetAlarmCodes                         LCMS2MT_PREF(cmsSetAlarmCodes)
#define cmsGetAlarmCodes                         LCMS2MT_PREF(cmsGetAlarmCodes)
#define cmsSetAdaptationState                    LCMS2MT_PREF(cmsSetAdaptationState)
#define cmsSetLogErrorHandler                    LCMS2MT_PREF(cmsSetLogErrorHandler)
#define cmsGetSupportedIntents                   LCMS2MT_PREF(cmsGetSupportedIntents)
#define cmsMLUtranslationsCount                  LCMS2MT_PREF(cmsMLUtranslationsCount)
#define cmsMLUtranslationsCodes                  LCMS2MT_PREF(cmsMLUtranslationsCodes)
#define _cmsCreateMutex                          LCMS2MT_PREF(_cmsCreateMutex)
#define _cmsDestroyMutex                         LCMS2MT_PREF(_cmsDestroyMutex)
#define _cmsLockMutex                            LCMS2MT_PREF(_cmsLockMutex)
#define _cmsUnlockMutex                          LCMS2MT_PREF(_cmsUnlockMutex)
#define cmsGetProfileIOhandler                   LCMS2MT_PREF(cmsGetProfileIOhandler)
#define cmsGetEncodedCMMversion                  LCMS2MT_PREF(cmsGetEncodedCMMversion)
#define _cmsFloat2Half                           LCMS2MT_PREF(_cmsFloat2Half)
#define _cmsHalf2Float                           LCMS2MT_PREF(_cmsHalf2Float)
#define _cmsFreeInterpParams                     LCMS2MT_PREF(_cmsFreeInterpParams)
#define _cmsGetFormatter                         LCMS2MT_PREF(_cmsGetFormatter)
#define _cmsGetTransformFormatters16             LCMS2MT_PREF(_cmsGetTransformFormatters16)
#define _cmsGetTransformFormattersFloat          LCMS2MT_PREF(_cmsGetTransformFormattersFloat)
#define _cmsQuantizeVal                          LCMS2MT_PREF(_cmsQuantizeVal)
#define _cmsReadDevicelinkLUT                    LCMS2MT_PREF(_cmsReadDevicelinkLUT)
#define _cmsReadInputLUT                         LCMS2MT_PREF(_cmsReadInputLUT)
#define _cmsReadOutputLUT                        LCMS2MT_PREF(_cmsReadOutputLUT)
#define _cmsStageAllocIdentityCLut               LCMS2MT_PREF(_cmsStageAllocIdentityCLut)
#define _cmsStageAllocIdentityCurves             LCMS2MT_PREF(_cmsStageAllocIdentityCurves)
#define _cmsStageAllocLab2XYZ                    LCMS2MT_PREF(_cmsStageAllocLab2XYZ)
#define _cmsStageAllocLabV2ToV4                  LCMS2MT_PREF(_cmsStageAllocLabV2ToV4)
#define _cmsStageAllocLabV4ToV2                  LCMS2MT_PREF(_cmsStageAllocLabV4ToV2)
#define _cmsStageAllocNamedColor                 LCMS2MT_PREF(_cmsStageAllocNamedColor)
#define _cmsStageAllocXYZ2Lab                    LCMS2MT_PREF(_cmsStageAllocXYZ2Lab)
#define cmsMD5add                                LCMS2MT_PREF(cmsMD5add)
#define cmsMD5alloc                              LCMS2MT_PREF(cmsMD5alloc)
#define cmsMD5finish                             LCMS2MT_PREF(cmsMD5finish)
#define _cmsComputeInterpParams                  LCMS2MT_PREF(_cmsComputeInterpParams)
#define cmsGetToneCurveSegment                   LCMS2MT_PREF(cmsGetToneCurveSegment)
#define cmsDetectRGBProfileGamma                 LCMS2MT_PREF(cmsDetectRGBProfileGamma)
#define _cmsOptimizePipeline                     LCMS2MT_PREF(_cmsOptimizePipeline)
#define _cmsReasonableGridpointsByColorspace     LCMS2MT_PREF(_cmsReasonableGridpointsByColorspace)
#define _cmsGetTransformFlags                    LCMS2MT_PREF(_cmsGetTransformFlags)
#define _cmsGetTransformWorker                   LCMS2MT_PREF(_cmsGetTransformWorker)
#define _cmsGetTransformMaxWorkers               LCMS2MT_PREF(_cmsGetTransformMaxWorkers)
#define _cmsGetTransformWorkerFlags              LCMS2MT_PREF(_cmsGetTransformWorkerFlags)

#define _cmsQuickFloor                           LCMS2MT_PREF(_cmsQuickFloor)
#define _cmsQuickFloorWord                       LCMS2MT_PREF(_cmsQuickFloorWord)
#define _cmsQuickSaturateWord                    LCMS2MT_PREF(_cmsQuickSaturateWord)
#define _cmsQuickSaturateByte                    LCMS2MT_PREF(_cmsQuickSaturateByte)
#define _cmsHandleExtraChannels                  LCMS2MT_PREF(_cmsHandleExtraChannels)
#define _cmsAllocIntentsPluginChunk              LCMS2MT_PREF(_cmsAllocIntentsPluginChunk)
#define _cmsLinkProfiles                         LCMS2MT_PREF(_cmsLinkProfiles)
#define _cmsRegisterRenderingIntentPlugin        LCMS2MT_PREF(_cmsRegisterRenderingIntentPlugin)
#define _cmsAllocLogErrorChunk                   LCMS2MT_PREF(_cmsAllocLogErrorChunk)
#define _cmsAllocMemPluginChunk                  LCMS2MT_PREF(_cmsAllocMemPluginChunk)
#define _cmsAllocMutexPluginChunk                LCMS2MT_PREF(_cmsAllocMutexPluginChunk)
#define _cmsAllocParallelizationPluginChunk      LCMS2MT_PREF(_cmsAllocParallelizationPluginChunk)
#define _cmsCreateSubAlloc                       LCMS2MT_PREF(_cmsCreateSubAlloc)
#define _cmsInstallAllocFunctions                LCMS2MT_PREF(_cmsInstallAllocFunctions)
#define _cmsRegisterMemHandlerPlugin             LCMS2MT_PREF(_cmsRegisterMemHandlerPlugin)
#define _cmsRegisterMutexPlugin                  LCMS2MT_PREF(_cmsRegisterMutexPlugin)
#define _cmsRegisterParallelizationPlugin        LCMS2MT_PREF(_cmsRegisterParallelizationPlugin)
#define _cmsSubAlloc                             LCMS2MT_PREF(_cmsSubAlloc)
#define _cmsSubAllocDestroy                      LCMS2MT_PREF(_cmsSubAllocDestroy)
#define _cmsSubAllocDup                          LCMS2MT_PREF(_cmsSubAllocDup)
#define _cmsTagSignature2String                  LCMS2MT_PREF(_cmsTagSignature2String)
#define _cmsAllocCurvesPluginChunk               LCMS2MT_PREF(_cmsAllocCurvesPluginChunk)
#define _cmsRegisterParametricCurvesPlugin       LCMS2MT_PREF(_cmsRegisterParametricCurvesPlugin)
#define _cmsBuildKToneCurve                      LCMS2MT_PREF(_cmsBuildKToneCurve)
#define _cmsChain2Lab                            LCMS2MT_PREF(_cmsChain2Lab)
#define _cmsCreateGamutCheckPipeline             LCMS2MT_PREF(_cmsCreateGamutCheckPipeline)
#define _cmsAllocInterpPluginChunk               LCMS2MT_PREF(_cmsAllocInterpPluginChunk)
#define _cmsComputeInterpParamsEx                LCMS2MT_PREF(_cmsComputeInterpParamsEx)
#define _cmsRegisterInterpPlugin                 LCMS2MT_PREF(_cmsRegisterInterpPlugin)
#define _cmsSetInterpolationRoutine              LCMS2MT_PREF(_cmsSetInterpolationRoutine)
#define _cmsGetTagTrueType                       LCMS2MT_PREF(_cmsGetTagTrueType)
#define _cmsReadHeader                           LCMS2MT_PREF(_cmsReadHeader)
#define _cmsSearchTag                            LCMS2MT_PREF(_cmsSearchTag)
#define _cmsWriteHeader                          LCMS2MT_PREF(_cmsWriteHeader)
#define _cmsCompileProfileSequence               LCMS2MT_PREF(_cmsCompileProfileSequence)
#define _cmsReadCHAD                             LCMS2MT_PREF(_cmsReadCHAD)
#define _cmsReadMediaWhitePoint                  LCMS2MT_PREF(_cmsReadMediaWhitePoint)
#define _cmsReadProfileSequence                  LCMS2MT_PREF(_cmsReadProfileSequence)
#define _cmsWriteProfileSequence                 LCMS2MT_PREF(_cmsWriteProfileSequence)
#define _cmsStageAllocLabPrelin                  LCMS2MT_PREF(_cmsStageAllocLabPrelin)
#define _cmsStageAllocLabV2ToV4curves            LCMS2MT_PREF(_cmsStageAllocLabV2ToV4curves)
#define _cmsStageClipNegatives                   LCMS2MT_PREF(_cmsStageClipNegatives)
#define _cmsStageGetPtrToCurveSet                LCMS2MT_PREF(_cmsStageGetPtrToCurveSet)
#define _cmsStageNormalizeFromLabFloat           LCMS2MT_PREF(_cmsStageNormalizeFromLabFloat)
#define _cmsStageNormalizeFromXyzFloat           LCMS2MT_PREF(_cmsStageNormalizeFromXyzFloat)
#define _cmsStageNormalizeToLabFloat             LCMS2MT_PREF(_cmsStageNormalizeToLabFloat)
#define _cmsStageNormalizeToXyzFloat             LCMS2MT_PREF(_cmsStageNormalizeToXyzFloat)
#define _cmsAllocOptimizationPluginChunk         LCMS2MT_PREF(_cmsAllocOptimizationPluginChunk)
#define _cmsLutIsIdentity                        LCMS2MT_PREF(_cmsLutIsIdentity)
#define _cmsRegisterOptimizationPlugin           LCMS2MT_PREF(_cmsRegisterOptimizationPlugin)
#define _cmsAllocFormattersPluginChunk           LCMS2MT_PREF(_cmsAllocFormattersPluginChunk)
#define _cmsFormatterIs8bit                      LCMS2MT_PREF(_cmsFormatterIs8bit)
#define _cmsFormatterIsFloat                     LCMS2MT_PREF(_cmsFormatterIsFloat)
#define _cmsRegisterFormattersPlugin             LCMS2MT_PREF(_cmsRegisterFormattersPlugin)
#define _cmsEndPointsBySpace                     LCMS2MT_PREF(_cmsEndPointsBySpace)
#define _cmsAdjustReferenceCount                 LCMS2MT_PREF(_cmsAdjustReferenceCount)
#define _cmsContextGetClientChunk                LCMS2MT_PREF(_cmsContextGetClientChunk)
#define _cmsGetContext                           LCMS2MT_PREF(_cmsGetContext)
#define _cmsGetTime                              LCMS2MT_PREF(_cmsGetTime)
#define _cmsPluginMalloc                         LCMS2MT_PREF(_cmsPluginMalloc)
#define IsIdentity                               LCMS2MT_PREF(IsIdentity)
#define Type_MHC2_Dup                            LCMS2MT_PREF(Type_MHC2_Dup)
#define Type_VideoSignal_Dup                     LCMS2MT_PREF(Type_VideoSignal_Dup)
#define _cmsAllocMPETypePluginChunk              LCMS2MT_PREF(_cmsAllocMPETypePluginChunk)
#define _cmsAllocTagPluginChunk                  LCMS2MT_PREF(_cmsAllocTagPluginChunk)
#define _cmsAllocTagTypePluginChunk              LCMS2MT_PREF(_cmsAllocTagTypePluginChunk)
#define _cmsGetTagDescriptor                     LCMS2MT_PREF(_cmsGetTagDescriptor)
#define _cmsGetTagTypeHandler                    LCMS2MT_PREF(_cmsGetTagTypeHandler)
#define _cmsRegisterMultiProcessElementPlugin    LCMS2MT_PREF(_cmsRegisterMultiProcessElementPlugin)
#define _cmsRegisterTagPlugin                    LCMS2MT_PREF(_cmsRegisterTagPlugin)
#define _cmsRegisterTagTypePlugin                LCMS2MT_PREF(_cmsRegisterTagTypePlugin)
#define cmsCreate_OkLabProfile                   LCMS2MT_PREF(cmsCreate_OkLabProfile)
#define _cmsAdaptationMatrix                     LCMS2MT_PREF(_cmsAdaptationMatrix)
#define _cmsBuildRGB2XYZtransferMatrix           LCMS2MT_PREF(_cmsBuildRGB2XYZtransferMatrix)
#define _cmsAllocAdaptationStateChunk            LCMS2MT_PREF(_cmsAllocAdaptationStateChunk)
#define _cmsAllocAlarmCodesChunk                 LCMS2MT_PREF(_cmsAllocAlarmCodesChunk)
#define _cmsAllocTransformPluginChunk            LCMS2MT_PREF(_cmsAllocTransformPluginChunk)
#define _cmsFindFormatter                        LCMS2MT_PREF(_cmsFindFormatter)
#define _cmsRegisterTransformPlugin              LCMS2MT_PREF(_cmsRegisterTransformPlugin)

#define _cmsIntentsPluginChunk                   LCMS2MT_PREF(_cmsIntentsPluginChunk)
#define _cmsLogErrorChunk                        LCMS2MT_PREF(_cmsLogErrorChunk)
#define _cmsMemPluginChunk                       LCMS2MT_PREF(_cmsMemPluginChunk)
#define _cmsMutexPluginChunk                     LCMS2MT_PREF(_cmsMutexPluginChunk)
#define _cmsParallelizationPluginChunk           LCMS2MT_PREF(_cmsParallelizationPluginChunk)
#define _cmsCurvesPluginChunk                    LCMS2MT_PREF(_cmsCurvesPluginChunk)
#define _cmsInterpPluginChunk                    LCMS2MT_PREF(_cmsInterpPluginChunk)
#define _cmsOptimizationPluginChunk              LCMS2MT_PREF(_cmsOptimizationPluginChunk)
#define _cmsFormattersPluginChunk                LCMS2MT_PREF(_cmsFormattersPluginChunk)
#define _cmsMPETypePluginChunk                   LCMS2MT_PREF(_cmsMPETypePluginChunk)
#define _cmsTagPluginChunk                       LCMS2MT_PREF(_cmsTagPluginChunk)
#define _cmsTagTypePluginChunk                   LCMS2MT_PREF(_cmsTagTypePluginChunk)
#define _cmsAdaptationStateChunk                 LCMS2MT_PREF(_cmsAdaptationStateChunk)
#define _cmsAlarmCodesChunk                      LCMS2MT_PREF(_cmsAlarmCodesChunk)
#define _cmsTransformPluginChunk                 LCMS2MT_PREF(_cmsTransformPluginChunk)

#endif

#define cmsMAX_PATH     256

#ifndef FALSE
#       define FALSE 0
#endif
#ifndef TRUE
#       define TRUE  1
#endif

#define cmsD50X  0.9642
#define cmsD50Y  1.0
#define cmsD50Z  0.8249

#define cmsPERCEPTUAL_BLACK_X  0.00336
#define cmsPERCEPTUAL_BLACK_Y  0.0034731
#define cmsPERCEPTUAL_BLACK_Z  0.00287

#define cmsMagicNumber  0x61637370
#define lcmsSignature   0x6c636d73

typedef enum {
    cmsSigChromaticityType                  = 0x6368726D,
    cmsSigcicpType                          = 0x63696370,
    cmsSigColorantOrderType                 = 0x636C726F,
    cmsSigColorantTableType                 = 0x636C7274,
    cmsSigCrdInfoType                       = 0x63726469,
    cmsSigCurveType                         = 0x63757276,
    cmsSigDataType                          = 0x64617461,
    cmsSigDictType                          = 0x64696374,
    cmsSigDateTimeType                      = 0x6474696D,
    cmsSigDeviceSettingsType                = 0x64657673,
    cmsSigLut16Type                         = 0x6d667432,
    cmsSigLut8Type                          = 0x6d667431,
    cmsSigLutAtoBType                       = 0x6d414220,
    cmsSigLutBtoAType                       = 0x6d424120,
    cmsSigMeasurementType                   = 0x6D656173,
    cmsSigMultiLocalizedUnicodeType         = 0x6D6C7563,
    cmsSigMultiProcessElementType           = 0x6D706574,
    cmsSigNamedColorType                    = 0x6E636f6C,
    cmsSigNamedColor2Type                   = 0x6E636C32,
    cmsSigParametricCurveType               = 0x70617261,
    cmsSigProfileSequenceDescType           = 0x70736571,
    cmsSigProfileSequenceIdType             = 0x70736964,
    cmsSigResponseCurveSet16Type            = 0x72637332,
    cmsSigS15Fixed16ArrayType               = 0x73663332,
    cmsSigScreeningType                     = 0x7363726E,
    cmsSigSignatureType                     = 0x73696720,
    cmsSigTextType                          = 0x74657874,
    cmsSigTextDescriptionType               = 0x64657363,
    cmsSigU16Fixed16ArrayType               = 0x75663332,
    cmsSigUcrBgType                         = 0x62666420,
    cmsSigUInt16ArrayType                   = 0x75693136,
    cmsSigUInt32ArrayType                   = 0x75693332,
    cmsSigUInt64ArrayType                   = 0x75693634,
    cmsSigUInt8ArrayType                    = 0x75693038,
    cmsSigVcgtType                          = 0x76636774,
    cmsSigViewingConditionsType             = 0x76696577,
    cmsSigXYZType                           = 0x58595A20,
    cmsSigMHC2Type                          = 0x4D484332

} cmsTagTypeSignature;

typedef enum {
    cmsSigAToB0Tag                          = 0x41324230,
    cmsSigAToB1Tag                          = 0x41324231,
    cmsSigAToB2Tag                          = 0x41324232,
    cmsSigBlueColorantTag                   = 0x6258595A,
    cmsSigBlueMatrixColumnTag               = 0x6258595A,
    cmsSigBlueTRCTag                        = 0x62545243,
    cmsSigBToA0Tag                          = 0x42324130,
    cmsSigBToA1Tag                          = 0x42324131,
    cmsSigBToA2Tag                          = 0x42324132,
    cmsSigCalibrationDateTimeTag            = 0x63616C74,
    cmsSigCharTargetTag                     = 0x74617267,
    cmsSigChromaticAdaptationTag            = 0x63686164,
    cmsSigChromaticityTag                   = 0x6368726D,
    cmsSigColorantOrderTag                  = 0x636C726F,
    cmsSigColorantTableTag                  = 0x636C7274,
    cmsSigColorantTableOutTag               = 0x636C6F74,
    cmsSigColorimetricIntentImageStateTag   = 0x63696973,
    cmsSigCopyrightTag                      = 0x63707274,
    cmsSigCrdInfoTag                        = 0x63726469,
    cmsSigDataTag                           = 0x64617461,
    cmsSigDateTimeTag                       = 0x6474696D,
    cmsSigDeviceMfgDescTag                  = 0x646D6E64,
    cmsSigDeviceModelDescTag                = 0x646D6464,
    cmsSigDeviceSettingsTag                 = 0x64657673,
    cmsSigDToB0Tag                          = 0x44324230,
    cmsSigDToB1Tag                          = 0x44324231,
    cmsSigDToB2Tag                          = 0x44324232,
    cmsSigDToB3Tag                          = 0x44324233,
    cmsSigBToD0Tag                          = 0x42324430,
    cmsSigBToD1Tag                          = 0x42324431,
    cmsSigBToD2Tag                          = 0x42324432,
    cmsSigBToD3Tag                          = 0x42324433,
    cmsSigGamutTag                          = 0x67616D74,
    cmsSigGrayTRCTag                        = 0x6b545243,
    cmsSigGreenColorantTag                  = 0x6758595A,
    cmsSigGreenMatrixColumnTag              = 0x6758595A,
    cmsSigGreenTRCTag                       = 0x67545243,
    cmsSigLuminanceTag                      = 0x6C756d69,
    cmsSigMeasurementTag                    = 0x6D656173,
    cmsSigMediaBlackPointTag                = 0x626B7074,
    cmsSigMediaWhitePointTag                = 0x77747074,
    cmsSigNamedColorTag                     = 0x6E636f6C,
    cmsSigNamedColor2Tag                    = 0x6E636C32,
    cmsSigOutputResponseTag                 = 0x72657370,
    cmsSigPerceptualRenderingIntentGamutTag = 0x72696730,
    cmsSigPreview0Tag                       = 0x70726530,
    cmsSigPreview1Tag                       = 0x70726531,
    cmsSigPreview2Tag                       = 0x70726532,
    cmsSigProfileDescriptionTag             = 0x64657363,
    cmsSigProfileDescriptionMLTag           = 0x6473636d,
    cmsSigProfileSequenceDescTag            = 0x70736571,
    cmsSigProfileSequenceIdTag              = 0x70736964,
    cmsSigPs2CRD0Tag                        = 0x70736430,
    cmsSigPs2CRD1Tag                        = 0x70736431,
    cmsSigPs2CRD2Tag                        = 0x70736432,
    cmsSigPs2CRD3Tag                        = 0x70736433,
    cmsSigPs2CSATag                         = 0x70733273,
    cmsSigPs2RenderingIntentTag             = 0x70733269,
    cmsSigRedColorantTag                    = 0x7258595A,
    cmsSigRedMatrixColumnTag                = 0x7258595A,
    cmsSigRedTRCTag                         = 0x72545243,
    cmsSigSaturationRenderingIntentGamutTag = 0x72696732,
    cmsSigScreeningDescTag                  = 0x73637264,
    cmsSigScreeningTag                      = 0x7363726E,
    cmsSigTechnologyTag                     = 0x74656368,
    cmsSigUcrBgTag                          = 0x62666420,
    cmsSigViewingCondDescTag                = 0x76756564,
    cmsSigViewingConditionsTag              = 0x76696577,
    cmsSigVcgtTag                           = 0x76636774,
    cmsSigMetaTag                           = 0x6D657461,
    cmsSigcicpTag                           = 0x63696370,
    cmsSigArgyllArtsTag                     = 0x61727473,
    cmsSigMHC2Tag                           = 0x4D484332

} cmsTagSignature;

typedef enum {
    cmsSigDigitalCamera                     = 0x6463616D,
    cmsSigFilmScanner                       = 0x6673636E,
    cmsSigReflectiveScanner                 = 0x7273636E,
    cmsSigInkJetPrinter                     = 0x696A6574,
    cmsSigThermalWaxPrinter                 = 0x74776178,
    cmsSigElectrophotographicPrinter        = 0x6570686F,
    cmsSigElectrostaticPrinter              = 0x65737461,
    cmsSigDyeSublimationPrinter             = 0x64737562,
    cmsSigPhotographicPaperPrinter          = 0x7270686F,
    cmsSigFilmWriter                        = 0x6670726E,
    cmsSigVideoMonitor                      = 0x7669646D,
    cmsSigVideoCamera                       = 0x76696463,
    cmsSigProjectionTelevision              = 0x706A7476,
    cmsSigCRTDisplay                        = 0x43525420,
    cmsSigPMDisplay                         = 0x504D4420,
    cmsSigAMDisplay                         = 0x414D4420,
    cmsSigPhotoCD                           = 0x4B504344,
    cmsSigPhotoImageSetter                  = 0x696D6773,
    cmsSigGravure                           = 0x67726176,
    cmsSigOffsetLithography                 = 0x6F666673,
    cmsSigSilkscreen                        = 0x73696C6B,
    cmsSigFlexography                       = 0x666C6578,
    cmsSigMotionPictureFilmScanner          = 0x6D706673,
    cmsSigMotionPictureFilmRecorder         = 0x6D706672,
    cmsSigDigitalMotionPictureCamera        = 0x646D7063,
    cmsSigDigitalCinemaProjector            = 0x64636A70

} cmsTechnologySignature;

typedef enum {
    cmsSigXYZData                           = 0x58595A20,
    cmsSigLabData                           = 0x4C616220,
    cmsSigLuvData                           = 0x4C757620,
    cmsSigYCbCrData                         = 0x59436272,
    cmsSigYxyData                           = 0x59787920,
    cmsSigRgbData                           = 0x52474220,
    cmsSigGrayData                          = 0x47524159,
    cmsSigHsvData                           = 0x48535620,
    cmsSigHlsData                           = 0x484C5320,
    cmsSigCmykData                          = 0x434D594B,
    cmsSigCmyData                           = 0x434D5920,
    cmsSigMCH1Data                          = 0x4D434831,
    cmsSigMCH2Data                          = 0x4D434832,
    cmsSigMCH3Data                          = 0x4D434833,
    cmsSigMCH4Data                          = 0x4D434834,
    cmsSigMCH5Data                          = 0x4D434835,
    cmsSigMCH6Data                          = 0x4D434836,
    cmsSigMCH7Data                          = 0x4D434837,
    cmsSigMCH8Data                          = 0x4D434838,
    cmsSigMCH9Data                          = 0x4D434839,
    cmsSigMCHAData                          = 0x4D434841,
    cmsSigMCHBData                          = 0x4D434842,
    cmsSigMCHCData                          = 0x4D434843,
    cmsSigMCHDData                          = 0x4D434844,
    cmsSigMCHEData                          = 0x4D434845,
    cmsSigMCHFData                          = 0x4D434846,
    cmsSigNamedData                         = 0x6e6d636c,
    cmsSig1colorData                        = 0x31434C52,
    cmsSig2colorData                        = 0x32434C52,
    cmsSig3colorData                        = 0x33434C52,
    cmsSig4colorData                        = 0x34434C52,
    cmsSig5colorData                        = 0x35434C52,
    cmsSig6colorData                        = 0x36434C52,
    cmsSig7colorData                        = 0x37434C52,
    cmsSig8colorData                        = 0x38434C52,
    cmsSig9colorData                        = 0x39434C52,
    cmsSig10colorData                       = 0x41434C52,
    cmsSig11colorData                       = 0x42434C52,
    cmsSig12colorData                       = 0x43434C52,
    cmsSig13colorData                       = 0x44434C52,
    cmsSig14colorData                       = 0x45434C52,
    cmsSig15colorData                       = 0x46434C52,
    cmsSigLuvKData                          = 0x4C75764B

} cmsColorSpaceSignature;

typedef enum {
    cmsSigInputClass                        = 0x73636E72,
    cmsSigDisplayClass                      = 0x6D6E7472,
    cmsSigOutputClass                       = 0x70727472,
    cmsSigLinkClass                         = 0x6C696E6B,
    cmsSigAbstractClass                     = 0x61627374,
    cmsSigColorSpaceClass                   = 0x73706163,
    cmsSigNamedColorClass                   = 0x6e6d636c,

    cmsSigColorEncodingSpaceClass           = 0x63656E63,
    cmsSigMultiplexIdentificationClass      = 0x6D696420,
    cmsSigMultiplexLinkClass                = 0x6d6c6e6b,
    cmsSigMultiplexVisualizationClass       = 0x6d766973

} cmsProfileClassSignature;

typedef enum {
    cmsSigMacintosh                         = 0x4150504C,
    cmsSigMicrosoft                         = 0x4D534654,
    cmsSigSolaris                           = 0x53554E57,
    cmsSigSGI                               = 0x53474920,
    cmsSigTaligent                          = 0x54474E54,
    cmsSigUnices                            = 0x2A6E6978

} cmsPlatformSignature;

#define  cmsSigPerceptualReferenceMediumGamut         0x70726d67

#define  cmsSigSceneColorimetryEstimates              0x73636F65
#define  cmsSigSceneAppearanceEstimates               0x73617065
#define  cmsSigFocalPlaneColorimetryEstimates         0x66706365
#define  cmsSigReflectionHardcopyOriginalColorimetry  0x72686F63
#define  cmsSigReflectionPrintOutputColorimetry       0x72706F63

typedef enum {
    cmsSigCurveSetElemType              = 0x63767374,
    cmsSigMatrixElemType                = 0x6D617466,
    cmsSigCLutElemType                  = 0x636C7574,

    cmsSigBAcsElemType                  = 0x62414353,
    cmsSigEAcsElemType                  = 0x65414353,

    cmsSigXYZ2LabElemType               = 0x6C327820,
    cmsSigLab2XYZElemType               = 0x78326C20,
    cmsSigNamedColorElemType            = 0x6E636C20,
    cmsSigLabV2toV4                     = 0x32203420,
    cmsSigLabV4toV2                     = 0x34203220,

    cmsSigIdentityElemType              = 0x69646E20,

    cmsSigLab2FloatPCS                  = 0x64326C20,
    cmsSigFloatPCS2Lab                  = 0x6C326420,
    cmsSigXYZ2FloatPCS                  = 0x64327820,
    cmsSigFloatPCS2XYZ                  = 0x78326420,
    cmsSigClipNegativesElemType         = 0x636c7020

} cmsStageSignature;

typedef enum {

    cmsSigFormulaCurveSeg               = 0x70617266,
    cmsSigSampledCurveSeg               = 0x73616D66,
    cmsSigSegmentedCurve                = 0x63757266

} cmsCurveSegSignature;

#define  cmsSigStatusA                    0x53746141
#define  cmsSigStatusE                    0x53746145
#define  cmsSigStatusI                    0x53746149
#define  cmsSigStatusT                    0x53746154
#define  cmsSigStatusM                    0x5374614D
#define  cmsSigDN                         0x444E2020
#define  cmsSigDNP                        0x444E2050
#define  cmsSigDNN                        0x444E4E20
#define  cmsSigDNNP                       0x444E4E50

#define cmsReflective     0
#define cmsTransparency   1
#define cmsGlossy         0
#define cmsMatte          2

typedef struct {
    cmsUInt32Number len;
    cmsUInt32Number flag;
    cmsUInt8Number  data[1];

} cmsICCData;

typedef struct {
    cmsUInt16Number      year;
    cmsUInt16Number      month;
    cmsUInt16Number      day;
    cmsUInt16Number      hours;
    cmsUInt16Number      minutes;
    cmsUInt16Number      seconds;

} cmsDateTimeNumber;

typedef struct {
    cmsS15Fixed16Number  X;
    cmsS15Fixed16Number  Y;
    cmsS15Fixed16Number  Z;

} cmsEncodedXYZNumber;

typedef union {
    cmsUInt8Number       ID8[16];
    cmsUInt16Number      ID16[8];
    cmsUInt32Number      ID32[4];

} cmsProfileID;

typedef struct {
    cmsUInt32Number              size;
    cmsSignature                 cmmId;
    cmsUInt32Number              version;
    cmsProfileClassSignature     deviceClass;
    cmsColorSpaceSignature       colorSpace;
    cmsColorSpaceSignature       pcs;
    cmsDateTimeNumber            date;
    cmsSignature                 magic;
    cmsPlatformSignature         platform;
    cmsUInt32Number              flags;
    cmsSignature                 manufacturer;
    cmsUInt32Number              model;
    cmsUInt64Number              attributes;
    cmsUInt32Number              renderingIntent;
    cmsEncodedXYZNumber          illuminant;
    cmsSignature                 creator;
    cmsProfileID                 profileID;
    cmsInt8Number                reserved[28];

} cmsICCHeader;

typedef struct {
    cmsTagTypeSignature  sig;
    cmsInt8Number        reserved[4];

} cmsTagBase;

typedef struct {
    cmsTagSignature      sig;
    cmsUInt32Number      offset;
    cmsUInt32Number      size;

} cmsTagEntry;

typedef void* cmsHANDLE ;
typedef void* cmsHPROFILE;
typedef void* cmsHTRANSFORM;

#define cmsMAXCHANNELS  16
#define cmsMAXEXTRACHANNELS  (63+cmsMAXCHANNELS)

#define PREMUL_SH(m)           ((m) << 26)
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

#define T_PREMUL(m)           (((m)>>26)&1)
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

#define PT_ANY       0

#define PT_GRAY      3
#define PT_RGB       4
#define PT_CMY       5
#define PT_CMYK      6
#define PT_YCbCr     7
#define PT_YUV       8
#define PT_XYZ       9
#define PT_Lab       10
#define PT_YUVK      11
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
#define PT_LabV2     30

#ifndef TYPE_RGB_8

#define TYPE_GRAY_8            (COLORSPACE_SH(PT_GRAY)|CHANNELS_SH(1)|BYTES_SH(1))
#define TYPE_GRAY_8_REV        (COLORSPACE_SH(PT_GRAY)|CHANNELS_SH(1)|BYTES_SH(1)|FLAVOR_SH(1))
#define TYPE_GRAY_16           (COLORSPACE_SH(PT_GRAY)|CHANNELS_SH(1)|BYTES_SH(2))
#define TYPE_GRAY_16_REV       (COLORSPACE_SH(PT_GRAY)|CHANNELS_SH(1)|BYTES_SH(2)|FLAVOR_SH(1))
#define TYPE_GRAY_16_SE        (COLORSPACE_SH(PT_GRAY)|CHANNELS_SH(1)|BYTES_SH(2)|ENDIAN16_SH(1))
#define TYPE_GRAYA_8           (COLORSPACE_SH(PT_GRAY)|EXTRA_SH(1)|CHANNELS_SH(1)|BYTES_SH(1))
#define TYPE_GRAYA_8_PREMUL    (COLORSPACE_SH(PT_GRAY)|EXTRA_SH(1)|CHANNELS_SH(1)|BYTES_SH(1)|PREMUL_SH(1))
#define TYPE_GRAYA_16          (COLORSPACE_SH(PT_GRAY)|EXTRA_SH(1)|CHANNELS_SH(1)|BYTES_SH(2))
#define TYPE_GRAYA_16_PREMUL   (COLORSPACE_SH(PT_GRAY)|EXTRA_SH(1)|CHANNELS_SH(1)|BYTES_SH(2)|PREMUL_SH(1))
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
#define TYPE_RGBA_8_PREMUL     (COLORSPACE_SH(PT_RGB)|EXTRA_SH(1)|CHANNELS_SH(3)|BYTES_SH(1)|PREMUL_SH(1))
#define TYPE_RGBA_8_PLANAR     (COLORSPACE_SH(PT_RGB)|EXTRA_SH(1)|CHANNELS_SH(3)|BYTES_SH(1)|PLANAR_SH(1))
#define TYPE_RGBA_16           (COLORSPACE_SH(PT_RGB)|EXTRA_SH(1)|CHANNELS_SH(3)|BYTES_SH(2))
#define TYPE_RGBA_16_PREMUL    (COLORSPACE_SH(PT_RGB)|EXTRA_SH(1)|CHANNELS_SH(3)|BYTES_SH(2)|PREMUL_SH(1))
#define TYPE_RGBA_16_PLANAR    (COLORSPACE_SH(PT_RGB)|EXTRA_SH(1)|CHANNELS_SH(3)|BYTES_SH(2)|PLANAR_SH(1))
#define TYPE_RGBA_16_SE        (COLORSPACE_SH(PT_RGB)|EXTRA_SH(1)|CHANNELS_SH(3)|BYTES_SH(2)|ENDIAN16_SH(1))

#define TYPE_ARGB_8            (COLORSPACE_SH(PT_RGB)|EXTRA_SH(1)|CHANNELS_SH(3)|BYTES_SH(1)|SWAPFIRST_SH(1))
#define TYPE_ARGB_8_PREMUL     (COLORSPACE_SH(PT_RGB)|EXTRA_SH(1)|CHANNELS_SH(3)|BYTES_SH(1)|SWAPFIRST_SH(1)|PREMUL_SH(1))
#define TYPE_ARGB_8_PLANAR     (COLORSPACE_SH(PT_RGB)|EXTRA_SH(1)|CHANNELS_SH(3)|BYTES_SH(1)|SWAPFIRST_SH(1)|PLANAR_SH(1))
#define TYPE_ARGB_16           (COLORSPACE_SH(PT_RGB)|EXTRA_SH(1)|CHANNELS_SH(3)|BYTES_SH(2)|SWAPFIRST_SH(1))
#define TYPE_ARGB_16_PREMUL    (COLORSPACE_SH(PT_RGB)|EXTRA_SH(1)|CHANNELS_SH(3)|BYTES_SH(2)|SWAPFIRST_SH(1)|PREMUL_SH(1))

#define TYPE_ABGR_8            (COLORSPACE_SH(PT_RGB)|EXTRA_SH(1)|CHANNELS_SH(3)|BYTES_SH(1)|DOSWAP_SH(1))
#define TYPE_ABGR_8_PREMUL     (COLORSPACE_SH(PT_RGB)|EXTRA_SH(1)|CHANNELS_SH(3)|BYTES_SH(1)|DOSWAP_SH(1)|PREMUL_SH(1))
#define TYPE_ABGR_8_PLANAR     (COLORSPACE_SH(PT_RGB)|EXTRA_SH(1)|CHANNELS_SH(3)|BYTES_SH(1)|DOSWAP_SH(1)|PLANAR_SH(1))
#define TYPE_ABGR_16           (COLORSPACE_SH(PT_RGB)|EXTRA_SH(1)|CHANNELS_SH(3)|BYTES_SH(2)|DOSWAP_SH(1))
#define TYPE_ABGR_16_PREMUL    (COLORSPACE_SH(PT_RGB)|EXTRA_SH(1)|CHANNELS_SH(3)|BYTES_SH(2)|DOSWAP_SH(1)|PREMUL_SH(1))
#define TYPE_ABGR_16_PLANAR    (COLORSPACE_SH(PT_RGB)|EXTRA_SH(1)|CHANNELS_SH(3)|BYTES_SH(2)|DOSWAP_SH(1)|PLANAR_SH(1))
#define TYPE_ABGR_16_SE        (COLORSPACE_SH(PT_RGB)|EXTRA_SH(1)|CHANNELS_SH(3)|BYTES_SH(2)|DOSWAP_SH(1)|ENDIAN16_SH(1))

#define TYPE_BGRA_8            (COLORSPACE_SH(PT_RGB)|EXTRA_SH(1)|CHANNELS_SH(3)|BYTES_SH(1)|DOSWAP_SH(1)|SWAPFIRST_SH(1))
#define TYPE_BGRA_8_PREMUL     (COLORSPACE_SH(PT_RGB)|EXTRA_SH(1)|CHANNELS_SH(3)|BYTES_SH(1)|DOSWAP_SH(1)|SWAPFIRST_SH(1)|PREMUL_SH(1))
#define TYPE_BGRA_8_PLANAR     (COLORSPACE_SH(PT_RGB)|EXTRA_SH(1)|CHANNELS_SH(3)|BYTES_SH(1)|DOSWAP_SH(1)|SWAPFIRST_SH(1)|PLANAR_SH(1))
#define TYPE_BGRA_16           (COLORSPACE_SH(PT_RGB)|EXTRA_SH(1)|CHANNELS_SH(3)|BYTES_SH(2)|DOSWAP_SH(1)|SWAPFIRST_SH(1))
#define TYPE_BGRA_16_PREMUL    (COLORSPACE_SH(PT_RGB)|EXTRA_SH(1)|CHANNELS_SH(3)|BYTES_SH(2)|DOSWAP_SH(1)|SWAPFIRST_SH(1)|PREMUL_SH(1))
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

#define TYPE_XYZ_16            (COLORSPACE_SH(PT_XYZ)|CHANNELS_SH(3)|BYTES_SH(2))
#define TYPE_Lab_8             (COLORSPACE_SH(PT_Lab)|CHANNELS_SH(3)|BYTES_SH(1))
#define TYPE_LabV2_8           (COLORSPACE_SH(PT_LabV2)|CHANNELS_SH(3)|BYTES_SH(1))

#define TYPE_ALab_8            (COLORSPACE_SH(PT_Lab)|CHANNELS_SH(3)|BYTES_SH(1)|EXTRA_SH(1)|SWAPFIRST_SH(1))
#define TYPE_ALabV2_8          (COLORSPACE_SH(PT_LabV2)|CHANNELS_SH(3)|BYTES_SH(1)|EXTRA_SH(1)|SWAPFIRST_SH(1))
#define TYPE_Lab_16            (COLORSPACE_SH(PT_Lab)|CHANNELS_SH(3)|BYTES_SH(2))
#define TYPE_LabV2_16          (COLORSPACE_SH(PT_LabV2)|CHANNELS_SH(3)|BYTES_SH(2))
#define TYPE_Yxy_16            (COLORSPACE_SH(PT_Yxy)|CHANNELS_SH(3)|BYTES_SH(2))

#define TYPE_YCbCr_8           (COLORSPACE_SH(PT_YCbCr)|CHANNELS_SH(3)|BYTES_SH(1))
#define TYPE_YCbCr_8_PLANAR    (COLORSPACE_SH(PT_YCbCr)|CHANNELS_SH(3)|BYTES_SH(1)|PLANAR_SH(1))
#define TYPE_YCbCr_16          (COLORSPACE_SH(PT_YCbCr)|CHANNELS_SH(3)|BYTES_SH(2))
#define TYPE_YCbCr_16_PLANAR   (COLORSPACE_SH(PT_YCbCr)|CHANNELS_SH(3)|BYTES_SH(2)|PLANAR_SH(1))
#define TYPE_YCbCr_16_SE       (COLORSPACE_SH(PT_YCbCr)|CHANNELS_SH(3)|BYTES_SH(2)|ENDIAN16_SH(1))

#define TYPE_YUV_8             (COLORSPACE_SH(PT_YUV)|CHANNELS_SH(3)|BYTES_SH(1))
#define TYPE_YUV_8_PLANAR      (COLORSPACE_SH(PT_YUV)|CHANNELS_SH(3)|BYTES_SH(1)|PLANAR_SH(1))
#define TYPE_YUV_16            (COLORSPACE_SH(PT_YUV)|CHANNELS_SH(3)|BYTES_SH(2))
#define TYPE_YUV_16_PLANAR     (COLORSPACE_SH(PT_YUV)|CHANNELS_SH(3)|BYTES_SH(2)|PLANAR_SH(1))
#define TYPE_YUV_16_SE         (COLORSPACE_SH(PT_YUV)|CHANNELS_SH(3)|BYTES_SH(2)|ENDIAN16_SH(1))

#define TYPE_HLS_8             (COLORSPACE_SH(PT_HLS)|CHANNELS_SH(3)|BYTES_SH(1))
#define TYPE_HLS_8_PLANAR      (COLORSPACE_SH(PT_HLS)|CHANNELS_SH(3)|BYTES_SH(1)|PLANAR_SH(1))
#define TYPE_HLS_16            (COLORSPACE_SH(PT_HLS)|CHANNELS_SH(3)|BYTES_SH(2))
#define TYPE_HLS_16_PLANAR     (COLORSPACE_SH(PT_HLS)|CHANNELS_SH(3)|BYTES_SH(2)|PLANAR_SH(1))
#define TYPE_HLS_16_SE         (COLORSPACE_SH(PT_HLS)|CHANNELS_SH(3)|BYTES_SH(2)|ENDIAN16_SH(1))

#define TYPE_HSV_8             (COLORSPACE_SH(PT_HSV)|CHANNELS_SH(3)|BYTES_SH(1))
#define TYPE_HSV_8_PLANAR      (COLORSPACE_SH(PT_HSV)|CHANNELS_SH(3)|BYTES_SH(1)|PLANAR_SH(1))
#define TYPE_HSV_16            (COLORSPACE_SH(PT_HSV)|CHANNELS_SH(3)|BYTES_SH(2))
#define TYPE_HSV_16_PLANAR     (COLORSPACE_SH(PT_HSV)|CHANNELS_SH(3)|BYTES_SH(2)|PLANAR_SH(1))
#define TYPE_HSV_16_SE         (COLORSPACE_SH(PT_HSV)|CHANNELS_SH(3)|BYTES_SH(2)|ENDIAN16_SH(1))

#define TYPE_NAMED_COLOR_INDEX (CHANNELS_SH(1)|BYTES_SH(2))

#define TYPE_XYZ_FLT          (FLOAT_SH(1)|COLORSPACE_SH(PT_XYZ)|CHANNELS_SH(3)|BYTES_SH(4))
#define TYPE_Lab_FLT          (FLOAT_SH(1)|COLORSPACE_SH(PT_Lab)|CHANNELS_SH(3)|BYTES_SH(4))
#define TYPE_LabA_FLT         (FLOAT_SH(1)|COLORSPACE_SH(PT_Lab)|EXTRA_SH(1)|CHANNELS_SH(3)|BYTES_SH(4))
#define TYPE_GRAY_FLT         (FLOAT_SH(1)|COLORSPACE_SH(PT_GRAY)|CHANNELS_SH(1)|BYTES_SH(4))
#define TYPE_GRAYA_FLT        (FLOAT_SH(1)|COLORSPACE_SH(PT_GRAY)|CHANNELS_SH(1)|BYTES_SH(4)|EXTRA_SH(1))
#define TYPE_GRAYA_FLT_PREMUL (FLOAT_SH(1)|COLORSPACE_SH(PT_GRAY)|CHANNELS_SH(1)|BYTES_SH(4)|EXTRA_SH(1)|PREMUL_SH(1))
#define TYPE_RGB_FLT          (FLOAT_SH(1)|COLORSPACE_SH(PT_RGB)|CHANNELS_SH(3)|BYTES_SH(4))

#define TYPE_RGBA_FLT         (FLOAT_SH(1)|COLORSPACE_SH(PT_RGB)|EXTRA_SH(1)|CHANNELS_SH(3)|BYTES_SH(4))
#define TYPE_RGBA_FLT_PREMUL  (FLOAT_SH(1)|COLORSPACE_SH(PT_RGB)|EXTRA_SH(1)|CHANNELS_SH(3)|BYTES_SH(4)|PREMUL_SH(1))
#define TYPE_ARGB_FLT         (FLOAT_SH(1)|COLORSPACE_SH(PT_RGB)|EXTRA_SH(1)|CHANNELS_SH(3)|BYTES_SH(4)|SWAPFIRST_SH(1))
#define TYPE_ARGB_FLT_PREMUL  (FLOAT_SH(1)|COLORSPACE_SH(PT_RGB)|EXTRA_SH(1)|CHANNELS_SH(3)|BYTES_SH(4)|SWAPFIRST_SH(1)|PREMUL_SH(1))
#define TYPE_BGR_FLT          (FLOAT_SH(1)|COLORSPACE_SH(PT_RGB)|CHANNELS_SH(3)|BYTES_SH(4)|DOSWAP_SH(1))
#define TYPE_BGRA_FLT         (FLOAT_SH(1)|COLORSPACE_SH(PT_RGB)|EXTRA_SH(1)|CHANNELS_SH(3)|BYTES_SH(4)|DOSWAP_SH(1)|SWAPFIRST_SH(1))
#define TYPE_BGRA_FLT_PREMUL  (FLOAT_SH(1)|COLORSPACE_SH(PT_RGB)|EXTRA_SH(1)|CHANNELS_SH(3)|BYTES_SH(4)|DOSWAP_SH(1)|SWAPFIRST_SH(1)|PREMUL_SH(1))
#define TYPE_ABGR_FLT         (FLOAT_SH(1)|COLORSPACE_SH(PT_RGB)|EXTRA_SH(1)|CHANNELS_SH(3)|BYTES_SH(4)|DOSWAP_SH(1))
#define TYPE_ABGR_FLT_PREMUL  (FLOAT_SH(1)|COLORSPACE_SH(PT_RGB)|EXTRA_SH(1)|CHANNELS_SH(3)|BYTES_SH(4)|DOSWAP_SH(1)|PREMUL_SH(1))

#define TYPE_CMYK_FLT         (FLOAT_SH(1)|COLORSPACE_SH(PT_CMYK)|CHANNELS_SH(4)|BYTES_SH(4))

#define TYPE_XYZ_DBL          (FLOAT_SH(1)|COLORSPACE_SH(PT_XYZ)|CHANNELS_SH(3)|BYTES_SH(0))
#define TYPE_Lab_DBL          (FLOAT_SH(1)|COLORSPACE_SH(PT_Lab)|CHANNELS_SH(3)|BYTES_SH(0))
#define TYPE_GRAY_DBL         (FLOAT_SH(1)|COLORSPACE_SH(PT_GRAY)|CHANNELS_SH(1)|BYTES_SH(0))
#define TYPE_RGB_DBL          (FLOAT_SH(1)|COLORSPACE_SH(PT_RGB)|CHANNELS_SH(3)|BYTES_SH(0))
#define TYPE_BGR_DBL          (FLOAT_SH(1)|COLORSPACE_SH(PT_RGB)|CHANNELS_SH(3)|BYTES_SH(0)|DOSWAP_SH(1))
#define TYPE_CMYK_DBL         (FLOAT_SH(1)|COLORSPACE_SH(PT_CMYK)|CHANNELS_SH(4)|BYTES_SH(0))
#define TYPE_OKLAB_DBL        (FLOAT_SH(1)|COLORSPACE_SH(PT_MCH3)|CHANNELS_SH(3)|BYTES_SH(0))

#define TYPE_GRAY_HALF_FLT    (FLOAT_SH(1)|COLORSPACE_SH(PT_GRAY)|CHANNELS_SH(1)|BYTES_SH(2))
#define TYPE_RGB_HALF_FLT     (FLOAT_SH(1)|COLORSPACE_SH(PT_RGB)|CHANNELS_SH(3)|BYTES_SH(2))
#define TYPE_CMYK_HALF_FLT    (FLOAT_SH(1)|COLORSPACE_SH(PT_CMYK)|CHANNELS_SH(4)|BYTES_SH(2))

#define TYPE_RGBA_HALF_FLT    (FLOAT_SH(1)|COLORSPACE_SH(PT_RGB)|EXTRA_SH(1)|CHANNELS_SH(3)|BYTES_SH(2))
#define TYPE_ARGB_HALF_FLT    (FLOAT_SH(1)|COLORSPACE_SH(PT_RGB)|EXTRA_SH(1)|CHANNELS_SH(3)|BYTES_SH(2)|SWAPFIRST_SH(1))
#define TYPE_BGR_HALF_FLT     (FLOAT_SH(1)|COLORSPACE_SH(PT_RGB)|CHANNELS_SH(3)|BYTES_SH(2)|DOSWAP_SH(1))
#define TYPE_BGRA_HALF_FLT    (FLOAT_SH(1)|COLORSPACE_SH(PT_RGB)|EXTRA_SH(1)|CHANNELS_SH(3)|BYTES_SH(2)|DOSWAP_SH(1)|SWAPFIRST_SH(1))
#define TYPE_ABGR_HALF_FLT    (FLOAT_SH(1)|COLORSPACE_SH(PT_RGB)|CHANNELS_SH(3)|BYTES_SH(2)|DOSWAP_SH(1))

#endif

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
        cmsUInt32Number  Observer;
        cmsCIEXYZ        Backing;
        cmsUInt32Number  Geometry;
        cmsFloat64Number Flare;
        cmsUInt32Number  IlluminantType;

    } cmsICCMeasurementConditions;

typedef struct {
        cmsCIEXYZ       IlluminantXYZ;
        cmsCIEXYZ       SurroundXYZ;
        cmsUInt32Number IlluminantType;

    } cmsICCViewingConditions;

typedef struct {
    cmsUInt8Number  ColourPrimaries;
    cmsUInt8Number  TransferCharacteristics;
    cmsUInt8Number  MatrixCoefficients;
    cmsUInt8Number  VideoFullRangeFlag;

} cmsVideoSignalType;

typedef struct {
    cmsUInt32Number   CurveEntries;
    cmsFloat64Number* RedCurve;
    cmsFloat64Number* GreenCurve;
    cmsFloat64Number* BlueCurve;

    cmsFloat64Number  MinLuminance;
    cmsFloat64Number  PeakLuminance;

    cmsFloat64Number XYZ2XYZmatrix[3][4];

} cmsMHC2Type;

CMSAPI int               CMSEXPORT cmsGetEncodedCMMversion(void);

CMSAPI int               CMSEXPORT cmsstrcasecmp(const char* s1, const char* s2);

#ifdef CMS_LARGE_FILE_SUPPORT
CMSAPI long long int     CMSEXPORT cmsfilelength(FILE* f);
#else
CMSAPI long int          CMSEXPORT cmsfilelength(FILE* f);
#endif

typedef struct _cmsContext_struct* cmsContext;

CMSAPI cmsContext       CMSEXPORT cmsCreateContext(void* Plugin, void* UserData);
CMSAPI void             CMSEXPORT cmsDeleteContext(cmsContext ContextID);
CMSAPI cmsContext       CMSEXPORT cmsDupContext(cmsContext ContextID, void* NewUserData);
CMSAPI void*            CMSEXPORT cmsGetContextUserData(cmsContext ContextID);

CMSAPI cmsBool           CMSEXPORT cmsPlugin(cmsContext ContextID, void* Plugin);
CMSAPI void              CMSEXPORT cmsUnregisterPlugins(cmsContext ContextID);

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

typedef void  (* cmsLogErrorHandlerFunction)(cmsContext ContextID, cmsUInt32Number ErrorCode, const char *Text);

CMSAPI void              CMSEXPORT cmsSetLogErrorHandler(cmsContext ContextID, cmsLogErrorHandlerFunction Fn);

CMSAPI const cmsCIEXYZ*  CMSEXPORT cmsD50_XYZ(cmsContext ContextID);
CMSAPI const cmsCIExyY*  CMSEXPORT cmsD50_xyY(cmsContext ContextID);

CMSAPI void              CMSEXPORT cmsXYZ2xyY(cmsContext ContextID, cmsCIExyY* Dest, const cmsCIEXYZ* Source);
CMSAPI void              CMSEXPORT cmsxyY2XYZ(cmsContext ContextID, cmsCIEXYZ* Dest, const cmsCIExyY* Source);
CMSAPI void              CMSEXPORT cmsXYZ2Lab(cmsContext ContextID, const cmsCIEXYZ* WhitePoint, cmsCIELab* Lab, const cmsCIEXYZ* xyz);
CMSAPI void              CMSEXPORT cmsLab2XYZ(cmsContext ContextID, const cmsCIEXYZ* WhitePoint, cmsCIEXYZ* xyz, const cmsCIELab* Lab);
CMSAPI void              CMSEXPORT cmsLab2LCh(cmsContext ContextID, cmsCIELCh*LCh, const cmsCIELab* Lab);
CMSAPI void              CMSEXPORT cmsLCh2Lab(cmsContext ContextID, cmsCIELab* Lab, const cmsCIELCh* LCh);

CMSAPI void              CMSEXPORT cmsLabEncoded2Float(cmsContext ContextID, cmsCIELab* Lab, const cmsUInt16Number wLab[3]);
CMSAPI void              CMSEXPORT cmsLabEncoded2FloatV2(cmsContext ContextID, cmsCIELab* Lab, const cmsUInt16Number wLab[3]);
CMSAPI void              CMSEXPORT cmsFloat2LabEncoded(cmsContext ContextID, cmsUInt16Number wLab[3], const cmsCIELab* Lab);
CMSAPI void              CMSEXPORT cmsFloat2LabEncodedV2(cmsContext ContextID, cmsUInt16Number wLab[3], const cmsCIELab* Lab);
CMSAPI void              CMSEXPORT cmsXYZEncoded2Float(cmsContext ContextID, cmsCIEXYZ* fxyz, const cmsUInt16Number XYZ[3]);
CMSAPI void              CMSEXPORT cmsFloat2XYZEncoded(cmsContext ContextID, cmsUInt16Number XYZ[3], const cmsCIEXYZ* fXYZ);

CMSAPI cmsFloat64Number  CMSEXPORT cmsDeltaE(cmsContext ContextID, const cmsCIELab* Lab1, const cmsCIELab* Lab2);
CMSAPI cmsFloat64Number  CMSEXPORT cmsCIE94DeltaE(cmsContext ContextID, const cmsCIELab* Lab1, const cmsCIELab* Lab2);
CMSAPI cmsFloat64Number  CMSEXPORT cmsBFDdeltaE(cmsContext ContextID, const cmsCIELab* Lab1, const cmsCIELab* Lab2);
CMSAPI cmsFloat64Number  CMSEXPORT cmsCMCdeltaE(cmsContext ContextID, const cmsCIELab* Lab1, const cmsCIELab* Lab2, cmsFloat64Number l, cmsFloat64Number c);
CMSAPI cmsFloat64Number  CMSEXPORT cmsCIE2000DeltaE(cmsContext ContextID, const cmsCIELab* Lab1, const cmsCIELab* Lab2, cmsFloat64Number Kl, cmsFloat64Number Kc, cmsFloat64Number Kh);

CMSAPI cmsBool           CMSEXPORT cmsWhitePointFromTemp(cmsContext ContextID, cmsCIExyY* WhitePoint, cmsFloat64Number  TempK);
CMSAPI cmsBool           CMSEXPORT cmsTempFromWhitePoint(cmsContext ContextID, cmsFloat64Number* TempK, const cmsCIExyY* WhitePoint);

CMSAPI cmsBool           CMSEXPORT cmsAdaptToIlluminant(cmsContext ContextID, cmsCIEXYZ* Result, const cmsCIEXYZ* SourceWhitePt,
                                                                           const cmsCIEXYZ* Illuminant,
                                                                           const cmsCIEXYZ* Value);

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

typedef struct {
    cmsFloat32Number   x0, x1;
    cmsInt32Number     Type;
    cmsFloat64Number   Params[10];
    cmsUInt32Number    nGridPoints;
    cmsFloat32Number*  SampledPoints;

} cmsCurveSegment;

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
CMSAPI cmsUInt16Number   CMSEXPORT cmsEvalToneCurve16(const cmsContext ContextID, const cmsToneCurve* Curve, cmsUInt16Number v);
CMSAPI cmsBool           CMSEXPORT cmsIsToneCurveMultisegment(cmsContext ContextID, const cmsToneCurve* InGamma);
CMSAPI cmsBool           CMSEXPORT cmsIsToneCurveLinear(cmsContext ContextID, const cmsToneCurve* Curve);
CMSAPI cmsBool           CMSEXPORT cmsIsToneCurveMonotonic(cmsContext ContextID, const cmsToneCurve* t);
CMSAPI cmsBool           CMSEXPORT cmsIsToneCurveDescending(cmsContext ContextID, const cmsToneCurve* t);
CMSAPI cmsInt32Number    CMSEXPORT cmsGetToneCurveParametricType(cmsContext ContextID, const cmsToneCurve* t);
CMSAPI cmsFloat64Number  CMSEXPORT cmsEstimateGamma(cmsContext ContextID, const cmsToneCurve* t, cmsFloat64Number Precision);

CMSAPI const cmsCurveSegment* CMSEXPORT cmsGetToneCurveSegment(cmsContext ContextID, cmsInt32Number n, const cmsToneCurve* t);

CMSAPI cmsUInt32Number         CMSEXPORT cmsGetToneCurveEstimatedTableEntries(cmsContext ContextID, const cmsToneCurve* t);
CMSAPI const cmsUInt16Number*  CMSEXPORT cmsGetToneCurveEstimatedTable(cmsContext ContextID, const cmsToneCurve* t);

typedef struct _cmsPipeline_struct cmsPipeline;
typedef struct _cmsStage_struct cmsStage;

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

typedef enum { cmsAT_BEGIN, cmsAT_END } cmsStageLoc;

CMSAPI cmsBool           CMSEXPORT cmsPipelineInsertStage(cmsContext ContextID, cmsPipeline* lut, cmsStageLoc loc, cmsStage* mpe);
CMSAPI void              CMSEXPORT cmsPipelineUnlinkStage(cmsContext ContextID, cmsPipeline* lut, cmsStageLoc loc, cmsStage** mpe);

CMSAPI cmsBool           CMSEXPORT cmsPipelineCheckAndRetreiveStages(cmsContext ContextID, const cmsPipeline* Lut, cmsUInt32Number n, ...);

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

typedef cmsInt32Number (* cmsSAMPLER16)   (cmsContext ContextID,
                                           CMSREGISTER const cmsUInt16Number In[],
                                           CMSREGISTER cmsUInt16Number Out[],
                                           CMSREGISTER void * Cargo);

typedef cmsInt32Number (* cmsSAMPLERFLOAT)(cmsContext ContextID,
                                           CMSREGISTER const cmsFloat32Number In[],
                                           CMSREGISTER cmsFloat32Number Out[],
                                           CMSREGISTER void * Cargo);

#define SAMPLER_INSPECT     0x01000000

CMSAPI cmsBool           CMSEXPORT cmsStageSampleCLut16bit(cmsContext ContextID, cmsStage* mpe, cmsSAMPLER16 Sampler, void* Cargo, cmsUInt32Number dwFlags);
CMSAPI cmsBool           CMSEXPORT cmsStageSampleCLutFloat(cmsContext ContextID, cmsStage* mpe, cmsSAMPLERFLOAT Sampler, void* Cargo, cmsUInt32Number dwFlags);

CMSAPI cmsBool           CMSEXPORT cmsSliceSpace16(cmsContext ContextID, cmsUInt32Number nInputs, const cmsUInt32Number clutPoints[],
                                                   cmsSAMPLER16 Sampler, void * Cargo);

CMSAPI cmsBool           CMSEXPORT cmsSliceSpaceFloat(cmsContext ContextID, cmsUInt32Number nInputs, const cmsUInt32Number clutPoints[],
                                                   cmsSAMPLERFLOAT Sampler, void * Cargo);

typedef struct _cms_MLU_struct cmsMLU;

#define  cmsNoLanguage    "\0\0"
#define  cmsNoCountry     "\0\0"

#define  cmsV2Unicode     "\xff\xff"

CMSAPI cmsMLU*           CMSEXPORT cmsMLUalloc(cmsContext ContextID, cmsUInt32Number nItems);
CMSAPI void              CMSEXPORT cmsMLUfree(cmsContext ContextID, cmsMLU* mlu);
CMSAPI cmsMLU*           CMSEXPORT cmsMLUdup(cmsContext ContextID, const cmsMLU* mlu);

CMSAPI cmsBool           CMSEXPORT cmsMLUsetASCII(cmsContext ContextID, cmsMLU* mlu,
                                                  const char LanguageCode[3], const char CountryCode[3],
                                                  const char* ASCIIString);
CMSAPI cmsBool           CMSEXPORT cmsMLUsetWide(cmsContext ContextID, cmsMLU* mlu,
                                                  const char LanguageCode[3], const char CountryCode[3],
                                                  const wchar_t* WideString);
CMSAPI cmsBool           CMSEXPORT cmsMLUsetUTF8(cmsContext ContextID, cmsMLU* mlu,
                                                  const char LanguageCode[3], const char CountryCode[3],
                                                  const char* UTF8String);

CMSAPI cmsUInt32Number   CMSEXPORT cmsMLUgetASCII(cmsContext ContextID, const cmsMLU* mlu,
                                                  const char LanguageCode[3], const char CountryCode[3],
                                                  char* Buffer,    cmsUInt32Number BufferSize);

CMSAPI cmsUInt32Number   CMSEXPORT cmsMLUgetWide(cmsContext ContextID, const cmsMLU* mlu,
                                                 const char LanguageCode[3], const char CountryCode[3],
                                                 wchar_t* Buffer, cmsUInt32Number BufferSize);
CMSAPI cmsUInt32Number   CMSEXPORT cmsMLUgetUTF8(cmsContext ContextID, const cmsMLU* mlu,
                                                 const char LanguageCode[3], const char CountryCode[3],
                                                 char* Buffer, cmsUInt32Number BufferSize);

CMSAPI cmsBool           CMSEXPORT cmsMLUgetTranslation(cmsContext ContextID, const cmsMLU* mlu,
                                                         const char LanguageCode[3], const char CountryCode[3],
                                                         char ObtainedLanguage[3], char ObtainedCountry[3]);

CMSAPI cmsUInt32Number   CMSEXPORT cmsMLUtranslationsCount(cmsContext ContextID, const cmsMLU* mlu);

CMSAPI cmsBool           CMSEXPORT cmsMLUtranslationsCodes(cmsContext ContextID, const cmsMLU* mlu,
                                                             cmsUInt32Number idx,
                                                             char LanguageCode[3],
                                                             char CountryCode[3]);

typedef struct {
        cmsToneCurve* Ucr;
        cmsToneCurve* Bg;
        cmsMLU*       Desc;

} cmsUcrBg;

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

CMSAPI cmsNAMEDCOLORLIST* CMSEXPORT cmsGetNamedColorList(cmsHTRANSFORM xform);

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

CMSAPI cmsHPROFILE       CMSEXPORT cmsCreateProfilePlaceholder(cmsContext ContextID);

CMSAPI cmsContext        CMSEXPORT cmsGetProfileContextID(cmsHPROFILE hProfile);
CMSAPI cmsInt32Number    CMSEXPORT cmsGetTagCount(cmsContext ContextID, cmsHPROFILE hProfile);
CMSAPI cmsTagSignature   CMSEXPORT cmsGetTagSignature(cmsContext ContextID, cmsHPROFILE hProfile, cmsUInt32Number n);
CMSAPI cmsBool           CMSEXPORT cmsGetTagOffsetAndSize(cmsHPROFILE hProfile, cmsUInt32Number n, cmsUInt32Number* offset, cmsUInt32Number* size);
CMSAPI cmsBool           CMSEXPORT cmsIsTag(cmsContext ContextID, cmsHPROFILE hProfile, cmsTagSignature sig);

CMSAPI void*             CMSEXPORT cmsReadTag(cmsContext ContextID, cmsHPROFILE hProfile, cmsTagSignature sig);
CMSAPI cmsBool           CMSEXPORT cmsWriteTag(cmsContext ContextID, cmsHPROFILE hProfile, cmsTagSignature sig, const void* data);
CMSAPI cmsBool           CMSEXPORT cmsLinkTag(cmsContext ContextID, cmsHPROFILE hProfile, cmsTagSignature sig, cmsTagSignature dest);
CMSAPI cmsTagSignature   CMSEXPORT cmsTagLinkedTo(cmsContext ContextID, cmsHPROFILE hProfile, cmsTagSignature sig);

CMSAPI cmsUInt32Number   CMSEXPORT cmsReadRawTag(cmsContext ContextID, cmsHPROFILE hProfile, cmsTagSignature sig, void* Buffer, cmsUInt32Number BufferSize);
CMSAPI cmsBool           CMSEXPORT cmsWriteRawTag(cmsContext ContextID, cmsHPROFILE hProfile, cmsTagSignature sig, const void* data, cmsUInt32Number Size);

#define cmsEmbeddedProfileFalse    0x00000000
#define cmsEmbeddedProfileTrue     0x00000001
#define cmsUseAnywhere             0x00000000
#define cmsUseWithEmbeddedDataOnly 0x00000002

CMSAPI cmsUInt32Number   CMSEXPORT cmsGetHeaderFlags(cmsContext ContextID, cmsHPROFILE hProfile);
CMSAPI void              CMSEXPORT cmsGetHeaderAttributes(cmsContext ContextID, cmsHPROFILE hProfile, cmsUInt64Number* Flags);
CMSAPI void              CMSEXPORT cmsGetHeaderProfileID(cmsContext ContextID, cmsHPROFILE hProfile, cmsUInt8Number* ProfileID);
CMSAPI cmsBool           CMSEXPORT cmsGetHeaderCreationDateTime(cmsContext ContextID, cmsHPROFILE hProfile, struct tm *Dest);
CMSAPI cmsUInt32Number   CMSEXPORT cmsGetHeaderRenderingIntent(cmsContext ContextID, cmsHPROFILE hProfile);
CMSAPI cmsUInt32Number   CMSEXPORT cmsGetHeaderCMM(cmsContext ContextID, cmsHPROFILE hProfile);

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

#define LCMS_USED_AS_INPUT      0
#define LCMS_USED_AS_OUTPUT     1
#define LCMS_USED_AS_PROOF      2

CMSAPI cmsBool           CMSEXPORT cmsIsIntentSupported(cmsContext ContextID, cmsHPROFILE hProfile, cmsUInt32Number Intent, cmsUInt32Number UsedDirection);
CMSAPI cmsBool           CMSEXPORT cmsIsMatrixShaper(cmsContext ContextID, cmsHPROFILE hProfile);
CMSAPI cmsBool           CMSEXPORT cmsIsCLUT(cmsContext ContextID, cmsHPROFILE hProfile, cmsUInt32Number Intent, cmsUInt32Number UsedDirection);

CMSAPI cmsColorSpaceSignature   CMSEXPORT _cmsICCcolorSpace(cmsContext ContextID, int OurNotation);
CMSAPI int                      CMSEXPORT _cmsLCMScolorSpace(cmsContext ContextID, cmsColorSpaceSignature ProfileSpace);

CMSAPI cmsUInt32Number   CMSEXPORT cmsChannelsOf(cmsContext ContextID, cmsColorSpaceSignature ColorSpace);

CMSAPI cmsInt32Number CMSEXPORT cmsChannelsOfColorSpace(cmsContext ContextID, cmsColorSpaceSignature ColorSpace);

CMSAPI cmsUInt32Number   CMSEXPORT cmsFormatterForColorspaceOfProfile(cmsContext ContextID, cmsHPROFILE hProfile, cmsUInt32Number nBytes, cmsBool lIsFloat);
CMSAPI cmsUInt32Number   CMSEXPORT cmsFormatterForPCSOfProfile(cmsContext ContextID, cmsHPROFILE hProfile, cmsUInt32Number nBytes, cmsBool lIsFloat);

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

CMSAPI cmsUInt32Number  CMSEXPORT cmsGetProfileInfoUTF8(cmsContext ContextID, cmsHPROFILE hProfile, cmsInfoType Info,
                                                            const char LanguageCode[3], const char CountryCode[3],
                                                            char* Buffer, cmsUInt32Number BufferSize);

typedef struct _cms_io_handler cmsIOHANDLER;

CMSAPI cmsIOHANDLER*     CMSEXPORT cmsOpenIOhandlerFromFile(cmsContext ContextID, const char* FileName, const char* AccessMode);
CMSAPI cmsIOHANDLER*     CMSEXPORT cmsOpenIOhandlerFromStream(cmsContext ContextID, FILE* Stream);
CMSAPI cmsIOHANDLER*     CMSEXPORT cmsOpenIOhandlerFromMem(cmsContext ContextID, void *Buffer, cmsUInt32Number size, const char* AccessMode);
CMSAPI cmsIOHANDLER*     CMSEXPORT cmsOpenIOhandlerFromNULL(cmsContext ContextID);
CMSAPI cmsIOHANDLER*     CMSEXPORT cmsGetProfileIOhandler(cmsContext ContextID, cmsHPROFILE hProfile);
CMSAPI cmsBool           CMSEXPORT cmsCloseIOhandler(cmsContext ContextID, cmsIOHANDLER* io);

CMSAPI cmsBool           CMSEXPORT cmsMD5computeID(cmsContext ContextID, cmsHPROFILE hProfile);

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

CMSAPI cmsHPROFILE      CMSEXPORT cmsCreateDeviceLinkFromCubeFile(cmsContext ContextID, const char* cFileName);

CMSAPI cmsHPROFILE      CMSEXPORT cmsCreateLab2Profile(cmsContext ContextID, const cmsCIExyY* WhitePoint);
CMSAPI cmsHPROFILE      CMSEXPORT cmsCreateLab4Profile(cmsContext ContextID, const cmsCIExyY* WhitePoint);

CMSAPI cmsHPROFILE      CMSEXPORT cmsCreateXYZProfile(cmsContext ContextID);

CMSAPI cmsHPROFILE      CMSEXPORT cmsCreate_sRGBProfile(cmsContext ContextID);

CMSAPI cmsHPROFILE      CMSEXPORT cmsCreate_OkLabProfile(cmsContext ctx);

CMSAPI cmsHPROFILE      CMSEXPORT cmsCreateBCHSWabstractProfile(cmsContext ContextID,
                                                             cmsUInt32Number nLUTPoints,
                                                             cmsFloat64Number Bright,
                                                             cmsFloat64Number Contrast,
                                                             cmsFloat64Number Hue,
                                                             cmsFloat64Number Saturation,
                                                             cmsUInt32Number TempSrc,
                                                             cmsUInt32Number TempDest);

CMSAPI cmsHPROFILE      CMSEXPORT cmsCreateNULLProfile(cmsContext ContextID);

CMSAPI cmsHPROFILE      CMSEXPORT cmsTransform2DeviceLink(cmsContext ContextID,
                                                          cmsHTRANSFORM hTransform,
                                                          cmsFloat64Number Version,
                                                          cmsUInt32Number dwFlags);

#define INTENT_PERCEPTUAL                              0
#define INTENT_RELATIVE_COLORIMETRIC                   1
#define INTENT_SATURATION                              2
#define INTENT_ABSOLUTE_COLORIMETRIC                   3

#define INTENT_PRESERVE_K_ONLY_PERCEPTUAL             10
#define INTENT_PRESERVE_K_ONLY_RELATIVE_COLORIMETRIC  11
#define INTENT_PRESERVE_K_ONLY_SATURATION             12
#define INTENT_PRESERVE_K_PLANE_PERCEPTUAL            13
#define INTENT_PRESERVE_K_PLANE_RELATIVE_COLORIMETRIC 14
#define INTENT_PRESERVE_K_PLANE_SATURATION            15

CMSAPI cmsUInt32Number  CMSEXPORT cmsGetSupportedIntents(cmsContext ContextID,
                                                         cmsUInt32Number nMax,
                                                         cmsUInt32Number* Codes,
                                                         char** Descriptions);

#define cmsFLAGS_NOCACHE                  0x0040
#define cmsFLAGS_NOOPTIMIZE               0x0100
#define cmsFLAGS_NULLTRANSFORM            0x0200

#define cmsFLAGS_GAMUTCHECK               0x1000
#define cmsFLAGS_SOFTPROOFING             0x4000

#define cmsFLAGS_BLACKPOINTCOMPENSATION   0x2000
#define cmsFLAGS_NOWHITEONWHITEFIXUP      0x0004
#define cmsFLAGS_HIGHRESPRECALC           0x0400
#define cmsFLAGS_LOWRESPRECALC            0x0800

#define cmsFLAGS_8BITS_DEVICELINK         0x0008
#define cmsFLAGS_GUESSDEVICECLASS         0x0020
#define cmsFLAGS_KEEP_SEQUENCE            0x0080

#define cmsFLAGS_FORCE_CLUT               0x0002
#define cmsFLAGS_CLUT_POST_LINEARIZATION  0x0001
#define cmsFLAGS_CLUT_PRE_LINEARIZATION   0x0010

#define cmsFLAGS_NONEGATIVES              0x8000

#define cmsFLAGS_COPY_ALPHA               0x04000000

#define cmsFLAGS_PREMULT                  0x08000000

#define cmsFLAGS_GRIDPOINTS(n)           (((n) & 0xFF) << 16)

#define cmsFLAGS_NODEFAULTRESOURCEDEF     0x01000000

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

CMSAPI void             CMSEXPORT cmsDoTransformStride(cmsContext ContextID,
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

CMSAPI cmsFloat64Number CMSEXPORT cmsSetAdaptationState(cmsContext ContextID, cmsFloat64Number d);

CMSAPI cmsUInt32Number CMSEXPORT cmsGetTransformInputFormat(cmsContext ContextID, cmsHTRANSFORM hTransform);
CMSAPI cmsUInt32Number CMSEXPORT cmsGetTransformOutputFormat(cmsContext ContextID, cmsHTRANSFORM hTransform);

CMSAPI cmsPipeline*    CMSEXPORT cmsGetTransformPipeline(cmsHTRANSFORM hTransform);
CMSAPI cmsPipeline*    CMSEXPORT cmsGetTransformGamutCheckPipeline(cmsHTRANSFORM hTransform);

CMSAPI cmsNAMEDCOLORLIST* CMSEXPORT cmsGetTransformInputColorants(cmsHTRANSFORM hTransform);
CMSAPI cmsNAMEDCOLORLIST* CMSEXPORT cmsGetTransformOutputColorants(cmsHTRANSFORM hTransform);

CMSAPI cmsBool          CMSEXPORT cmsChangeBuffersFormat(cmsHTRANSFORM hTransform,
                                                         cmsUInt32Number InputFormat,
                                                         cmsUInt32Number OutputFormat);

cmsHTRANSFORM cmsCloneTransformChangingFormats(cmsContext ContextID,
                                               const cmsHTRANSFORM hTransform,
                                               cmsUInt32Number InputFormat,
                                               cmsUInt32Number OutputFormat);

typedef enum { cmsPS_RESOURCE_CSA, cmsPS_RESOURCE_CRD } cmsPSResourceType;

CMSAPI cmsUInt32Number  CMSEXPORT cmsGetPostScriptColorResource(cmsContext ContextID,
                                                                cmsPSResourceType Type,
                                                                cmsHPROFILE hProfile,
                                                                cmsUInt32Number Intent,
                                                                cmsUInt32Number dwFlags,
                                                                cmsIOHANDLER* io);

CMSAPI cmsUInt32Number  CMSEXPORT cmsGetPostScriptCSA(cmsContext ContextID, cmsHPROFILE hProfile, cmsUInt32Number Intent, cmsUInt32Number dwFlags, void* Buffer, cmsUInt32Number dwBufferLen);
CMSAPI cmsUInt32Number  CMSEXPORT cmsGetPostScriptCRD(cmsContext ContextID, cmsHPROFILE hProfile, cmsUInt32Number Intent, cmsUInt32Number dwFlags, void* Buffer, cmsUInt32Number dwBufferLen);

CMSAPI cmsHANDLE        CMSEXPORT cmsIT8Alloc(cmsContext ContextID);
CMSAPI void             CMSEXPORT cmsIT8Free(cmsContext ContextID, cmsHANDLE hIT8);

CMSAPI cmsUInt32Number  CMSEXPORT cmsIT8TableCount(cmsContext ContextID, cmsHANDLE hIT8);
CMSAPI cmsInt32Number   CMSEXPORT cmsIT8SetTable(cmsContext ContextID, cmsHANDLE hIT8, cmsUInt32Number nTable);

CMSAPI cmsHANDLE        CMSEXPORT cmsIT8LoadFromFile(cmsContext ContextID, const char* cFileName);
CMSAPI cmsHANDLE        CMSEXPORT cmsIT8LoadFromMem(cmsContext ContextID, const void *Ptr, cmsUInt32Number len);

CMSAPI cmsBool          CMSEXPORT cmsIT8SaveToFile(cmsContext ContextID, cmsHANDLE hIT8, const char* cFileName);
CMSAPI cmsBool          CMSEXPORT cmsIT8SaveToMem(cmsContext ContextID, cmsHANDLE hIT8, void *MemPtr, cmsUInt32Number* BytesNeeded);

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

CMSAPI int              CMSEXPORT cmsIT8SetTableByLabel(cmsContext ContextID, cmsHANDLE hIT8, const char* cSet, const char* cField, const char* ExpectedType);

CMSAPI cmsBool          CMSEXPORT cmsIT8SetIndexColumn(cmsContext ContextID, cmsHANDLE hIT8, const char* cSample);

CMSAPI void             CMSEXPORT cmsIT8DefineDblFormat(cmsContext ContextID, cmsHANDLE hIT8, const char* Formatter);

CMSAPI cmsHANDLE        CMSEXPORT cmsGBDAlloc(cmsContext ContextID);
CMSAPI void             CMSEXPORT cmsGBDFree(cmsContext ContextID, cmsHANDLE hGBD);
CMSAPI cmsBool          CMSEXPORT cmsGDBAddPoint(cmsContext ContextID, cmsHANDLE hGBD, const cmsCIELab* Lab);
CMSAPI cmsBool          CMSEXPORT cmsGDBCompute(cmsContext ContextID, cmsHANDLE  hGDB, cmsUInt32Number dwFlags);
CMSAPI cmsBool          CMSEXPORT cmsGDBCheckPoint(cmsContext ContextID, cmsHANDLE hGBD, const cmsCIELab* Lab);

CMSAPI cmsBool          CMSEXPORT cmsDetectBlackPoint(cmsContext ContextID, cmsCIEXYZ* BlackPoint, cmsHPROFILE hProfile, cmsUInt32Number Intent, cmsUInt32Number dwFlags);
CMSAPI cmsBool          CMSEXPORT cmsDetectDestinationBlackPoint(cmsContext ContextID, cmsCIEXYZ* BlackPoint, cmsHPROFILE hProfile, cmsUInt32Number Intent, cmsUInt32Number dwFlags);

CMSAPI cmsFloat64Number CMSEXPORT cmsDetectTAC(cmsContext ContextID, cmsHPROFILE hProfile);

CMSAPI cmsFloat64Number CMSEXPORT cmsDetectRGBProfileGamma(cmsContext ContextID, cmsHPROFILE hProfile, cmsFloat64Number thereshold);

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
