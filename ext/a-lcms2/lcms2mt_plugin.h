#ifndef _lcms2mt_plugin_H

#ifdef _MSC_VER
#    if (_MSC_VER >= 1400)
#      ifndef _CRT_SECURE_NO_DEPRECATE
#        define _CRT_SECURE_NO_DEPRECATE
#      endif
#      ifndef _CRT_SECURE_NO_WARNINGS
#        define _CRT_SECURE_NO_WARNINGS
#      endif
#    endif
#endif

#ifndef _lcms2mt_H
#include "lcms2mt.h"
#endif

#include <stdlib.h>
#include <math.h>
#include <stdarg.h>
#include <memory.h>
#include <string.h>

#ifndef CMS_USE_CPP_API
#   ifdef __cplusplus
extern "C" {
#   endif
#endif

#define VX      0
#define VY      1
#define VZ      2

typedef struct {
    cmsFloat64Number n[3];

    } cmsVEC3;

typedef struct {
    cmsVEC3 v[3];

    } cmsMAT3;

CMSAPI void               CMSEXPORT _cmsVEC3init(cmsContext ContextID, cmsVEC3* r, cmsFloat64Number x, cmsFloat64Number y, cmsFloat64Number z);
CMSAPI void               CMSEXPORT _cmsVEC3minus(cmsContext ContextID, cmsVEC3* r, const cmsVEC3* a, const cmsVEC3* b);
CMSAPI void               CMSEXPORT _cmsVEC3cross(cmsContext ContextID, cmsVEC3* r, const cmsVEC3* u, const cmsVEC3* v);
CMSAPI cmsFloat64Number   CMSEXPORT _cmsVEC3dot(cmsContext ContextID, const cmsVEC3* u, const cmsVEC3* v);
CMSAPI cmsFloat64Number   CMSEXPORT _cmsVEC3length(cmsContext ContextID, const cmsVEC3* a);
CMSAPI cmsFloat64Number   CMSEXPORT _cmsVEC3distance(cmsContext ContextID, const cmsVEC3* a, const cmsVEC3* b);

CMSAPI void               CMSEXPORT _cmsMAT3identity(cmsContext ContextID, cmsMAT3* a);
CMSAPI cmsBool            CMSEXPORT _cmsMAT3isIdentity(cmsContext ContextID, const cmsMAT3* a);
CMSAPI void               CMSEXPORT _cmsMAT3per(cmsContext ContextID, cmsMAT3* r, const cmsMAT3* a, const cmsMAT3* b);
CMSAPI cmsBool            CMSEXPORT _cmsMAT3inverse(cmsContext ContextID, const cmsMAT3* a, cmsMAT3* b);
CMSAPI cmsBool            CMSEXPORT _cmsMAT3solve(cmsContext ContextID, cmsVEC3* x, cmsMAT3* a, cmsVEC3* b);
CMSAPI void               CMSEXPORT _cmsMAT3eval(cmsContext ContextID, cmsVEC3* r, const cmsMAT3* a, const cmsVEC3* v);

CMSAPI cmsHANDLE          CMSEXPORT cmsMD5alloc(cmsContext ContextID);
CMSAPI void               CMSEXPORT cmsMD5add(cmsHANDLE Handle, const cmsUInt8Number* buf, cmsUInt32Number len);
CMSAPI void               CMSEXPORT cmsMD5finish(cmsContext ContextID, cmsProfileID* ProfileID, cmsHANDLE Handle);

CMSAPI void               CMSEXPORT  cmsSignalError(cmsContext ContextID, cmsUInt32Number ErrorCode, const char *ErrorText, ...);

CMSAPI void*              CMSEXPORT _cmsMalloc(cmsContext ContextID, cmsUInt32Number size);
CMSAPI void*              CMSEXPORT _cmsMallocZero(cmsContext ContextID, cmsUInt32Number size);
CMSAPI void*              CMSEXPORT _cmsCalloc(cmsContext ContextID, cmsUInt32Number num, cmsUInt32Number size);
CMSAPI void*              CMSEXPORT _cmsRealloc(cmsContext ContextID, void* Ptr, cmsUInt32Number NewSize);
CMSAPI void               CMSEXPORT _cmsFree(cmsContext ContextID, void* Ptr);
CMSAPI void*              CMSEXPORT _cmsDupMem(cmsContext ContextID, const void* Org, cmsUInt32Number size);

struct _cms_io_handler {

    void* stream;

    cmsUInt32Number   UsedSpace;
    cmsUInt32Number   ReportedSize;
    char              PhysicalFile[cmsMAX_PATH];

    cmsUInt32Number   (* Read)(cmsContext ContextID, struct _cms_io_handler* iohandler, void *Buffer,
                                                                  cmsUInt32Number size,
                                                                  cmsUInt32Number count);
    cmsBool           (* Seek)(cmsContext ContextID, struct _cms_io_handler* iohandler, cmsUInt32Number offset);
    cmsBool           (* Close)(cmsContext ContextID, struct _cms_io_handler* iohandler);
    cmsUInt32Number   (* Tell)(cmsContext ContextID, struct _cms_io_handler* iohandler);
    cmsBool           (* Write)(cmsContext ContextID, struct _cms_io_handler* iohandler, cmsUInt32Number size,
                                                                   const void* Buffer);
};

CMSAPI cmsUInt16Number   CMSEXPORT  _cmsAdjustEndianess16(cmsUInt16Number Word);
CMSAPI cmsUInt32Number   CMSEXPORT  _cmsAdjustEndianess32(cmsUInt32Number Value);
CMSAPI void              CMSEXPORT  _cmsAdjustEndianess64(cmsUInt64Number* Result, cmsUInt64Number* QWord);

CMSAPI cmsBool           CMSEXPORT  _cmsReadUInt8Number(cmsContext ContextID, cmsIOHANDLER* io,  cmsUInt8Number* n);
CMSAPI cmsBool           CMSEXPORT  _cmsReadUInt16Number(cmsContext ContextID, cmsIOHANDLER* io, cmsUInt16Number* n);
CMSAPI cmsBool           CMSEXPORT  _cmsReadUInt32Number(cmsContext ContextID, cmsIOHANDLER* io, cmsUInt32Number* n);
CMSAPI cmsBool           CMSEXPORT  _cmsReadFloat32Number(cmsContext ContextID, cmsIOHANDLER* io, cmsFloat32Number* n);
CMSAPI cmsBool           CMSEXPORT  _cmsReadUInt64Number(cmsContext ContextID, cmsIOHANDLER* io, cmsUInt64Number* n);
CMSAPI cmsBool           CMSEXPORT  _cmsRead15Fixed16Number(cmsContext ContextID, cmsIOHANDLER* io, cmsFloat64Number* n);
CMSAPI cmsBool           CMSEXPORT  _cmsReadXYZNumber(cmsContext ContextID, cmsIOHANDLER* io, cmsCIEXYZ* XYZ);
CMSAPI cmsBool           CMSEXPORT  _cmsReadUInt16Array(cmsContext ContextID, cmsIOHANDLER* io, cmsUInt32Number n, cmsUInt16Number* Array);

CMSAPI cmsBool           CMSEXPORT  _cmsWriteUInt8Number(cmsContext ContextID, cmsIOHANDLER* io, cmsUInt8Number n);
CMSAPI cmsBool           CMSEXPORT  _cmsWriteUInt16Number(cmsContext ContextID, cmsIOHANDLER* io, cmsUInt16Number n);
CMSAPI cmsBool           CMSEXPORT  _cmsWriteUInt32Number(cmsContext ContextID, cmsIOHANDLER* io, cmsUInt32Number n);
CMSAPI cmsBool           CMSEXPORT  _cmsWriteFloat32Number(cmsContext ContextID, cmsIOHANDLER* io, cmsFloat32Number n);
CMSAPI cmsBool           CMSEXPORT  _cmsWriteUInt64Number(cmsContext ContextID, cmsIOHANDLER* io, cmsUInt64Number* n);
CMSAPI cmsBool           CMSEXPORT  _cmsWrite15Fixed16Number(cmsContext ContextID, cmsIOHANDLER* io, cmsFloat64Number n);
CMSAPI cmsBool           CMSEXPORT  _cmsWriteXYZNumber(cmsContext ContextID, cmsIOHANDLER* io, const cmsCIEXYZ* XYZ);
CMSAPI cmsBool           CMSEXPORT  _cmsWriteUInt16Array(cmsContext ContextID, cmsIOHANDLER* io, cmsUInt32Number n, const cmsUInt16Number* Array);

typedef struct {
    cmsTagTypeSignature  sig;
    cmsInt8Number        reserved[4];

} _cmsTagBase;

CMSAPI cmsTagTypeSignature  CMSEXPORT _cmsReadTypeBase(cmsContext ContextID, cmsIOHANDLER* io);
CMSAPI cmsBool              CMSEXPORT _cmsWriteTypeBase(cmsContext ContextID, cmsIOHANDLER* io, cmsTagTypeSignature sig);

CMSAPI cmsBool             CMSEXPORT _cmsReadAlignment(cmsContext ContextID, cmsIOHANDLER* io);
CMSAPI cmsBool             CMSEXPORT _cmsWriteAlignment(cmsContext ContextID, cmsIOHANDLER* io);

CMSAPI cmsBool             CMSEXPORT _cmsIOPrintf(cmsContext ContextID, cmsIOHANDLER* io, const char* frm, ...);

CMSAPI cmsFloat64Number    CMSEXPORT _cms8Fixed8toDouble(cmsContext ContextID, cmsUInt16Number fixed8);
CMSAPI cmsUInt16Number     CMSEXPORT _cmsDoubleTo8Fixed8(cmsContext ContextID, cmsFloat64Number val);

CMSAPI cmsFloat64Number    CMSEXPORT _cms15Fixed16toDouble(cmsContext ContextID, cmsS15Fixed16Number fix32);
CMSAPI cmsS15Fixed16Number CMSEXPORT _cmsDoubleTo15Fixed16(cmsContext ContextID, cmsFloat64Number v);

CMSAPI void                CMSEXPORT _cmsEncodeDateTimeNumber(cmsContext ContextID, cmsDateTimeNumber *Dest, const struct tm *Source);
CMSAPI void                CMSEXPORT _cmsDecodeDateTimeNumber(cmsContext ContextID, const cmsDateTimeNumber *Source, struct tm *Dest);

typedef void     (* _cmsFreeUserDataFn)(cmsContext ContextID, void* Data);
typedef void*    (* _cmsDupUserDataFn)(cmsContext ContextID, const void* Data);

#define cmsPluginMagicNumber                 0x61637070

#define cmsPluginMemHandlerSig               0x6D656D48
#define cmsPluginInterpolationSig            0x696E7048
#define cmsPluginParametricCurveSig          0x70617248
#define cmsPluginFormattersSig               0x66726D48
#define cmsPluginTagTypeSig                  0x74797048
#define cmsPluginTagSig                      0x74616748
#define cmsPluginRenderingIntentSig          0x696E7448
#define cmsPluginMultiProcessElementSig      0x6D706548
#define cmsPluginOptimizationSig             0x6F707448
#define cmsPluginTransformSig                0x7A666D48
#define cmsPluginMutexSig                    0x6D747A48
#define cmsPluginParalellizationSig          0x70726C48

typedef struct _cmsPluginBaseStruct {

        cmsUInt32Number                Magic;
        cmsUInt32Number                ExpectedVersion;
        cmsUInt32Number                Type;
        struct _cmsPluginBaseStruct*   Next;

} cmsPluginBase;

#define MAX_TYPES_IN_LCMS_PLUGIN    20

typedef void* (* _cmsMallocFnPtrType)(cmsContext ContextID, cmsUInt32Number size);
typedef void  (* _cmsFreeFnPtrType)(cmsContext ContextID, void *Ptr);
typedef void* (* _cmsReallocFnPtrType)(cmsContext ContextID, void* Ptr, cmsUInt32Number NewSize);

typedef void* (* _cmsMalloZerocFnPtrType)(cmsContext ContextID, cmsUInt32Number size);
typedef void* (* _cmsCallocFnPtrType)(cmsContext ContextID, cmsUInt32Number num, cmsUInt32Number size);
typedef void* (* _cmsDupFnPtrType)(cmsContext ContextID, const void* Org, cmsUInt32Number size);

typedef struct {

        cmsPluginBase base;

        _cmsMallocFnPtrType  MallocPtr;
        _cmsFreeFnPtrType    FreePtr;
        _cmsReallocFnPtrType ReallocPtr;

       _cmsMalloZerocFnPtrType MallocZeroPtr;
       _cmsCallocFnPtrType     CallocPtr;
       _cmsDupFnPtrType        DupPtr;

} cmsPluginMemHandler;

struct _cms_interp_struc;

typedef void (* _cmsInterpFn16)(cmsContext ContextID,
                                CMSREGISTER const cmsUInt16Number Input[],
                                CMSREGISTER cmsUInt16Number Output[],
                                CMSREGISTER const struct _cms_interp_struc* p);

typedef void (* _cmsInterpFnFloat)(cmsContext ContextID, cmsFloat32Number const Input[],
                                   cmsFloat32Number Output[],
                                   const struct _cms_interp_struc* p);

typedef union {
    _cmsInterpFn16       Lerp16;
    _cmsInterpFnFloat    LerpFloat;
} cmsInterpFunction;

#define CMS_LERP_FLAGS_16BITS             0x0000
#define CMS_LERP_FLAGS_FLOAT              0x0001
#define CMS_LERP_FLAGS_TRILINEAR          0x0100

#define MAX_INPUT_DIMENSIONS 15

typedef struct _cms_interp_struc {

    cmsUInt32Number dwFlags;
    cmsUInt32Number nInputs;
    cmsUInt32Number nOutputs;

    cmsUInt32Number nSamples[MAX_INPUT_DIMENSIONS];
    cmsUInt32Number Domain[MAX_INPUT_DIMENSIONS];

    cmsUInt32Number opta[MAX_INPUT_DIMENSIONS];

    const void *Table;
    cmsInterpFunction Interpolation;

 } cmsInterpParams;

typedef cmsInterpFunction (* cmsInterpFnFactory)(cmsContext ContextID, cmsUInt32Number nInputChannels, cmsUInt32Number nOutputChannels, cmsUInt32Number dwFlags);

typedef struct {
    cmsPluginBase base;

    cmsInterpFnFactory InterpolatorsFactory;

} cmsPluginInterpolation;

typedef  cmsFloat64Number (* cmsParametricCurveEvaluator)(cmsContext ContextID, cmsInt32Number Type, const cmsFloat64Number Params[10], cmsFloat64Number R);

typedef struct {
    cmsPluginBase base;

    cmsUInt32Number nFunctions;
    cmsUInt32Number FunctionTypes[MAX_TYPES_IN_LCMS_PLUGIN];
    cmsUInt32Number ParameterCount[MAX_TYPES_IN_LCMS_PLUGIN];

    cmsParametricCurveEvaluator    Evaluator;

} cmsPluginParametricCurves;

struct _cmstransform_struct;

typedef cmsUInt8Number* (* cmsFormatter16)(cmsContext ContextID,
                                           CMSREGISTER struct _cmstransform_struct* CMMcargo,
                                           CMSREGISTER cmsUInt16Number Values[],
                                           CMSREGISTER cmsUInt8Number* Buffer,
                                           CMSREGISTER cmsUInt32Number Stride);

typedef cmsUInt8Number* (* cmsFormatterFloat)(cmsContext ContextID, struct _cmstransform_struct* CMMcargo,
                                              cmsFloat32Number Values[],
                                              cmsUInt8Number*  Buffer,
                                              cmsUInt32Number  Stride);

typedef union {
    cmsFormatter16    Fmt16;
    cmsFormatterFloat FmtFloat;

} cmsFormatter;

#define CMS_PACK_FLAGS_16BITS       0x0000
#define CMS_PACK_FLAGS_FLOAT        0x0001

typedef enum { cmsFormatterInput=0, cmsFormatterOutput=1 } cmsFormatterDirection;

typedef cmsFormatter (* cmsFormatterFactory)(cmsContext ContextID, cmsUInt32Number Type,
                                             cmsFormatterDirection Dir,
                                             cmsUInt32Number dwFlags);

typedef struct {
    cmsPluginBase          base;
    cmsFormatterFactory    FormattersFactory;

} cmsPluginFormatters;

typedef struct _cms_typehandler_struct {

        cmsTagTypeSignature Signature;

        void *   (* ReadPtr)(cmsContext ContextID, struct _cms_typehandler_struct* self,
                             cmsIOHANDLER*      io,
                             cmsUInt32Number*   nItems,
                             cmsUInt32Number    SizeOfTag);

        cmsBool  (* WritePtr)(cmsContext ContextID, struct _cms_typehandler_struct* self,
                              cmsIOHANDLER*     io,
                              void*             Ptr,
                              cmsUInt32Number   nItems);

        void*   (* DupPtr)(cmsContext ContextID, struct _cms_typehandler_struct* self,
                           const void *Ptr,
                           cmsUInt32Number n);

        void    (* FreePtr)(cmsContext ContextID, struct _cms_typehandler_struct* self,
                            void *Ptr);

        cmsUInt32Number  ICCVersion;

} cmsTagTypeHandler;

typedef struct {
        cmsPluginBase      base;
        cmsTagTypeHandler  Handler;

} cmsPluginTagType;

typedef struct {

    cmsUInt32Number     ElemCount;

    cmsUInt32Number     nSupportedTypes;
    cmsTagTypeSignature SupportedTypes[MAX_TYPES_IN_LCMS_PLUGIN];

    cmsTagTypeSignature (* DecideType)(cmsContext ContextID, cmsFloat64Number ICCVersion, const void *Data);

} cmsTagDescriptor;

typedef struct {
    cmsPluginBase    base;

    cmsTagSignature  Signature;
    cmsTagDescriptor Descriptor;

} cmsPluginTag;

typedef cmsPipeline* (* cmsIntentFn)( cmsContext       ContextID,
                                      cmsUInt32Number  nProfiles,
                                      cmsUInt32Number  Intents[],
                                      cmsHPROFILE      hProfiles[],
                                      cmsBool          BPC[],
                                      cmsFloat64Number AdaptationStates[],
                                      cmsUInt32Number  dwFlags);

typedef struct {
    cmsPluginBase     base;
    cmsUInt32Number   Intent;
    cmsIntentFn       Link;
    char              Description[256];

} cmsPluginRenderingIntent;

CMSAPI cmsPipeline*  CMSEXPORT _cmsDefaultICCintents(cmsContext       ContextID,
                                                     cmsUInt32Number  nProfiles,
                                                     cmsUInt32Number  Intents[],
                                                     cmsHPROFILE      hProfiles[],
                                                     cmsBool          BPC[],
                                                     cmsFloat64Number AdaptationStates[],
                                                     cmsUInt32Number  dwFlags);

typedef void (* _cmsStageEvalFn)     (cmsContext ContextID, const cmsFloat32Number In[], cmsFloat32Number Out[], const cmsStage* mpe);
typedef void*(* _cmsStageDupElemFn)  (cmsContext ContextID, cmsStage* mpe);
typedef void (* _cmsStageFreeElemFn) (cmsContext ContextID, cmsStage* mpe);

CMSAPI cmsStage* CMSEXPORT _cmsStageAllocPlaceholder(cmsContext ContextID,
                                cmsStageSignature     Type,
                                cmsUInt32Number       InputChannels,
                                cmsUInt32Number       OutputChannels,
                                _cmsStageEvalFn       EvalPtr,
                                _cmsStageDupElemFn    DupElemPtr,
                                _cmsStageFreeElemFn   FreePtr,
                                void*                 Data);
typedef struct {
      cmsPluginBase     base;
      cmsTagTypeHandler Handler;

}  cmsPluginMultiProcessElement;

typedef struct {
    cmsUInt32Number nCurves;
    cmsToneCurve**  TheCurves;

} _cmsStageToneCurvesData;

typedef struct {
    cmsFloat64Number*  Double;
    cmsFloat64Number*  Offset;

} _cmsStageMatrixData;

typedef struct {

    union {
        cmsUInt16Number*  T;
        cmsFloat32Number* TFloat;

    } Tab;

    cmsInterpParams* Params;
    cmsUInt32Number  nEntries;
    cmsBool          HasFloatValues;

} _cmsStageCLutData;

typedef cmsBool  (* _cmsOPToptimizeFn)(cmsContext ContextID,
                                       cmsPipeline** Lut,
                                       cmsUInt32Number  Intent,
                                       cmsUInt32Number* InputFormat,
                                       cmsUInt32Number* OutputFormat,
                                       cmsUInt32Number* dwFlags);

typedef void (* _cmsPipelineEval16Fn)(cmsContext ContextID,
                                     CMSREGISTER const cmsUInt16Number In[],
                                     CMSREGISTER cmsUInt16Number Out[],
                                     const void* Data);

typedef void (* _cmsPipelineEvalFloatFn)(cmsContext ContextID,
                                         const cmsFloat32Number In[],
                                         cmsFloat32Number Out[],
                                         const void* Data);

CMSAPI void CMSEXPORT _cmsPipelineSetOptimizationParameters(
                                               cmsContext ContextID,
                                               cmsPipeline* Lut,
                                               _cmsPipelineEval16Fn Eval16,
                                               void* PrivateData,
                                               _cmsFreeUserDataFn FreePrivateDataFn,
                                               _cmsDupUserDataFn DupPrivateDataFn);

typedef struct {
      cmsPluginBase     base;

      _cmsOPToptimizeFn  OptimizePtr;

}  cmsPluginOptimization;

typedef struct {
       cmsUInt32Number BytesPerLineIn;
       cmsUInt32Number BytesPerLineOut;
       cmsUInt32Number BytesPerPlaneIn;
       cmsUInt32Number BytesPerPlaneOut;

} cmsStride;

typedef void     (* _cmsTransformFn)(cmsContext ContextID, struct _cmstransform_struct *CMMcargo,
                                     const void* InputBuffer,
                                     void* OutputBuffer,
                                     cmsUInt32Number Size,
                                     cmsUInt32Number Stride);

typedef void     (*_cmsTransform2Fn)(cmsContext ContextID, struct _cmstransform_struct *CMMcargo,
                                     const void* InputBuffer,
                                     void* OutputBuffer,
                                     cmsUInt32Number PixelsPerLine,
                                     cmsUInt32Number LineCount,
                                     const cmsStride* Stride);

typedef cmsBool  (* _cmsTransformFactory)(cmsContext ContextID, _cmsTransformFn* xform,
                                         void** UserData,
                                         _cmsFreeUserDataFn* FreePrivateDataFn,
                                         cmsPipeline** Lut,
                                         cmsUInt32Number* InputFormat,
                                         cmsUInt32Number* OutputFormat,
                                         cmsUInt32Number* dwFlags);

typedef cmsBool  (* _cmsTransform2Factory)(cmsContext ContextID, _cmsTransform2Fn* xform,
                                         void** UserData,
                                         _cmsFreeUserDataFn* FreePrivateDataFn,
                                         cmsPipeline** Lut,
                                         cmsUInt32Number* InputFormat,
                                         cmsUInt32Number* OutputFormat,
                                         cmsUInt32Number* dwFlags);

CMSAPI void   CMSEXPORT _cmsSetTransformUserData(struct _cmstransform_struct *CMMcargo, void* ptr, _cmsFreeUserDataFn FreePrivateDataFn);
CMSAPI void * CMSEXPORT _cmsGetTransformUserData(struct _cmstransform_struct *CMMcargo);

CMSAPI void   CMSEXPORT _cmsGetTransformFormatters16   (struct _cmstransform_struct *CMMcargo, cmsFormatter16* FromInput, cmsFormatter16* ToOutput);
CMSAPI void   CMSEXPORT _cmsGetTransformFormattersFloat(struct _cmstransform_struct *CMMcargo, cmsFormatterFloat* FromInput, cmsFormatterFloat* ToOutput);

CMSAPI cmsUInt32Number CMSEXPORT _cmsGetTransformFlags(struct _cmstransform_struct* CMMcargo);

typedef struct {
      cmsPluginBase     base;

      union {
             _cmsTransformFactory        legacy_xform;
             _cmsTransform2Factory       xform;
      } factories;

}  cmsPluginTransform;

typedef void*    (* _cmsCreateMutexFnPtrType)(cmsContext ContextID);
typedef void     (* _cmsDestroyMutexFnPtrType)(cmsContext ContextID, void* mtx);
typedef cmsBool  (* _cmsLockMutexFnPtrType)(cmsContext ContextID, void* mtx);
typedef void     (* _cmsUnlockMutexFnPtrType)(cmsContext ContextID, void* mtx);

typedef struct {
      cmsPluginBase     base;

     _cmsCreateMutexFnPtrType  CreateMutexPtr;
     _cmsDestroyMutexFnPtrType DestroyMutexPtr;
     _cmsLockMutexFnPtrType    LockMutexPtr;
     _cmsUnlockMutexFnPtrType  UnlockMutexPtr;

}  cmsPluginMutex;

CMSAPI void*   CMSEXPORT _cmsCreateMutex(cmsContext ContextID);
CMSAPI void    CMSEXPORT _cmsDestroyMutex(cmsContext ContextID, void* mtx);
CMSAPI cmsBool CMSEXPORT _cmsLockMutex(cmsContext ContextID, void* mtx);
CMSAPI void    CMSEXPORT _cmsUnlockMutex(cmsContext ContextID, void* mtx);

CMSAPI _cmsTransform2Fn CMSEXPORT _cmsGetTransformWorker(struct _cmstransform_struct* CMMcargo);
CMSAPI cmsInt32Number   CMSEXPORT _cmsGetTransformMaxWorkers(struct _cmstransform_struct* CMMcargo);
CMSAPI cmsUInt32Number  CMSEXPORT _cmsGetTransformWorkerFlags(struct _cmstransform_struct* CMMcargo);

#define CMS_GUESS_MAX_WORKERS -1

typedef struct {
    cmsPluginBase       base;

    cmsInt32Number      MaxWorkers;
    cmsUInt32Number     WorkerFlags;
    _cmsTransform2Fn    SchedulerFn;

}  cmsPluginParalellization;

#ifndef CMS_USE_CPP_API
#   ifdef __cplusplus
    }
#   endif
#endif

#define _lcms2mt_plugin_H
#endif
