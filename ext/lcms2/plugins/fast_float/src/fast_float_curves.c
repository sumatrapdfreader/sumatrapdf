//---------------------------------------------------------------------------------
//
//  Little Color Management System, fast floating point extensions
//  Copyright (c) 1998-2020 Marti Maria Saguer, all rights reserved
//
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//---------------------------------------------------------------------------------

#include "fast_float_internal.h"

// Curves, optimization is valid for floating point curves
typedef struct {

    cmsFloat32Number CurveR[MAX_NODES_IN_CURVE];
    cmsFloat32Number CurveG[MAX_NODES_IN_CURVE];
    cmsFloat32Number CurveB[MAX_NODES_IN_CURVE];

    void* real_ptr;

} CurvesFloatData;



// A special malloc that returns memory aligned to DWORD boundary. Aligned memory access is way faster than unaligned
// reference to the real block is kept for later free
static CurvesFloatData* malloc_aligned(cmsContext ContextID)
{
       cmsUInt8Number* real_ptr = (cmsUInt8Number*)_cmsMallocZero(ContextID, sizeof(CurvesFloatData) + 32);
       cmsUInt8Number* aligned = (cmsUInt8Number*)(((uintptr_t)real_ptr + 16) & ~0xf);
       CurvesFloatData* p = (CurvesFloatData*)aligned;

       p->real_ptr = real_ptr;

       return p;
}

// Free the private data container
static void free_aligned(cmsContext ContextID, void* Data)
{
       CurvesFloatData* p = (CurvesFloatData*)Data;
       if (p != NULL)
              _cmsFree(ContextID, p->real_ptr);
}

// Evaluator for float curves. This are just 1D tables

static void FastEvaluateFloatRGBCurves(cmsContext ContextID,
                                        struct _cmstransform_struct *CMMcargo,
                                        const void* Input,
                                        void* Output,
                                        cmsUInt32Number PixelsPerLine,
                                        cmsUInt32Number LineCount,
                                        const cmsStride* Stride)
{
    cmsUInt32Number i, ii;
	cmsUInt32Number SourceStartingOrder[cmsMAXCHANNELS];
	cmsUInt32Number SourceIncrements[cmsMAXCHANNELS];
	cmsUInt32Number DestStartingOrder[cmsMAXCHANNELS];
	cmsUInt32Number DestIncrements[cmsMAXCHANNELS];

    const cmsUInt8Number* rin;
    const cmsUInt8Number* gin;
    const cmsUInt8Number* bin;
    const cmsUInt8Number* ain = NULL;

    cmsUInt8Number* rout;
    cmsUInt8Number* gout;
    cmsUInt8Number* bout;
    cmsUInt8Number* aout = NULL;

    cmsUInt32Number InputFormat  = cmsGetTransformInputFormat(ContextID, (cmsHTRANSFORM) CMMcargo);
    cmsUInt32Number OutputFormat = cmsGetTransformOutputFormat(ContextID, (cmsHTRANSFORM) CMMcargo);

    CurvesFloatData* Data = (CurvesFloatData*)  _cmsGetTransformUserData(CMMcargo);

    cmsUInt32Number nchans, nalpha;
    cmsUInt32Number strideIn, strideOut;

    _cmsComputeComponentIncrements(InputFormat,  Stride->BytesPerPlaneIn, &nchans, &nalpha, SourceStartingOrder, SourceIncrements);
    _cmsComputeComponentIncrements(OutputFormat, Stride->BytesPerPlaneOut, &nchans, &nalpha, DestStartingOrder, DestIncrements);

    if (!(_cmsGetTransformFlags((cmsHTRANSFORM)CMMcargo) & cmsFLAGS_COPY_ALPHA))
        nalpha = 0;


    strideIn = strideOut = 0;
    for (i = 0; i < LineCount; i++) {

        rin = (const cmsUInt8Number*)Input + SourceStartingOrder[0] + strideIn;
        gin = (const cmsUInt8Number*)Input + SourceStartingOrder[1] + strideIn;
        bin = (const cmsUInt8Number*)Input + SourceStartingOrder[2] + strideIn;

        if (nalpha)
            ain = (const cmsUInt8Number*)Input + SourceStartingOrder[3] + strideIn;

        rout = (cmsUInt8Number*)Output + DestStartingOrder[0] + strideOut;
        gout = (cmsUInt8Number*)Output + DestStartingOrder[1] + strideOut;
        bout = (cmsUInt8Number*)Output + DestStartingOrder[2] + strideOut;

        if (nalpha)
            aout = (cmsUInt8Number*)Output + DestStartingOrder[3] + strideOut;


        for (ii = 0; ii < PixelsPerLine; ii++) {

            *(cmsFloat32Number*)rout = flerp(Data->CurveR, *(cmsFloat32Number*)rin);
            *(cmsFloat32Number*)gout = flerp(Data->CurveG, *(cmsFloat32Number*)gin);
            *(cmsFloat32Number*)bout = flerp(Data->CurveB, *(cmsFloat32Number*)bin);

            rin += SourceIncrements[0];
            gin += SourceIncrements[1];
            bin += SourceIncrements[2];

            rout += DestIncrements[0];
            gout += DestIncrements[1];
            bout += DestIncrements[2];

            if (ain)
            {
                *(cmsFloat32Number*)aout = *(cmsFloat32Number*)ain;
                ain += SourceIncrements[3];
                aout += DestIncrements[3];
            }
        }

        strideIn += Stride->BytesPerLineIn;
        strideOut += Stride->BytesPerLineOut;
    }
}

// Do nothing but arrange the RGB format.
static void FastFloatRGBIdentity(cmsContext ContextID,
                                struct _cmstransform_struct *CMMcargo,
                                        const void* Input,
                                        void* Output,
                                        cmsUInt32Number PixelsPerLine,
                                        cmsUInt32Number LineCount,
                                        const cmsStride* Stride)
{
    cmsUInt32Number i, ii;
	cmsUInt32Number SourceStartingOrder[cmsMAXCHANNELS];
	cmsUInt32Number SourceIncrements[cmsMAXCHANNELS];
    cmsUInt32Number DestStartingOrder[cmsMAXCHANNELS];
    cmsUInt32Number DestIncrements[cmsMAXCHANNELS];
    const cmsUInt8Number* rin;
    const cmsUInt8Number* gin;
    const cmsUInt8Number* bin;
    const cmsUInt8Number* ain = NULL;
    cmsUInt8Number* rout;
    cmsUInt8Number* gout;
    cmsUInt8Number* bout;
    cmsUInt8Number* aout = NULL;

    cmsUInt32Number InputFormat  = cmsGetTransformInputFormat(ContextID, (cmsHTRANSFORM) CMMcargo);
    cmsUInt32Number OutputFormat = cmsGetTransformOutputFormat(ContextID, (cmsHTRANSFORM) CMMcargo);

    cmsUInt32Number nchans, nalpha;
    cmsUInt32Number strideIn, strideOut;

    _cmsComputeComponentIncrements(InputFormat,  Stride->BytesPerPlaneIn,  &nchans, &nalpha, SourceStartingOrder, SourceIncrements);
    _cmsComputeComponentIncrements(OutputFormat, Stride->BytesPerPlaneOut, &nchans, &nalpha, DestStartingOrder, DestIncrements);

    if (!(_cmsGetTransformFlags((cmsHTRANSFORM)CMMcargo) & cmsFLAGS_COPY_ALPHA))
        nalpha = 0;

    strideIn = strideOut = 0;
    for (i = 0; i < LineCount; i++) {

    rin = (const cmsUInt8Number*)Input + SourceStartingOrder[0] + strideIn;
    gin = (const cmsUInt8Number*)Input + SourceStartingOrder[1] + strideIn;
    bin = (const cmsUInt8Number*)Input + SourceStartingOrder[2] + strideIn;

    if (nalpha)
        ain = (const cmsUInt8Number*)Input + SourceStartingOrder[3] + strideIn;


    rout = (cmsUInt8Number*)Output + DestStartingOrder[0] + strideOut;
    gout = (cmsUInt8Number*)Output + DestStartingOrder[1] + strideOut;
    bout = (cmsUInt8Number*)Output + DestStartingOrder[2] + strideOut;

    if (nalpha)
        aout = (cmsUInt8Number*)Output + DestStartingOrder[3] + strideOut;


    for (ii=0; ii < PixelsPerLine; ii++) {

        *(cmsFloat32Number*)rout = *(cmsFloat32Number*)rin;
        *(cmsFloat32Number*)gout = *(cmsFloat32Number*)gin;
        *(cmsFloat32Number*)bout = *(cmsFloat32Number*)bin;

        rin += SourceIncrements[0];
        gin += SourceIncrements[1];
        bin += SourceIncrements[2];

        rout += DestIncrements[0];
        gout += DestIncrements[1];
        bout += DestIncrements[2];


        if (ain)
        {
            *(cmsFloat32Number*)aout = *(cmsFloat32Number*)ain;
            ain += SourceIncrements[3];
            aout += DestIncrements[3];
        }
    }

    strideIn += Stride->BytesPerLineIn;
    strideOut += Stride->BytesPerLineOut;
    }
}

// Evaluate 1 channel only
static void FastEvaluateFloatGrayCurves(cmsContext ContextID,
                                        struct _cmstransform_struct *CMMcargo,
                                        const void* Input,
                                        void* Output,
                                        cmsUInt32Number PixelsPerLine,
                                        cmsUInt32Number LineCount,
                                        const cmsStride* Stride)
{
    cmsUInt32Number i, ii;
    cmsUInt32Number SourceStartingOrder[cmsMAXCHANNELS];
    cmsUInt32Number SourceIncrements[cmsMAXCHANNELS];
    cmsUInt32Number DestStartingOrder[cmsMAXCHANNELS];
    cmsUInt32Number DestIncrements[cmsMAXCHANNELS];
    const cmsUInt8Number* kin;
    const cmsUInt8Number* ain = NULL;
    cmsUInt8Number* kout;
    cmsUInt8Number* aout = NULL;
    cmsUInt32Number InputFormat  = cmsGetTransformInputFormat(ContextID, (cmsHTRANSFORM) CMMcargo);
    cmsUInt32Number OutputFormat = cmsGetTransformOutputFormat(ContextID, (cmsHTRANSFORM) CMMcargo);

    CurvesFloatData* Data = (CurvesFloatData*)_cmsGetTransformUserData(CMMcargo);

    cmsUInt32Number nchans, nalpha;
    cmsUInt32Number strideIn, strideOut;

    _cmsComputeComponentIncrements(InputFormat, Stride->BytesPerPlaneIn, &nchans, &nalpha, SourceStartingOrder, SourceIncrements);
    _cmsComputeComponentIncrements(OutputFormat, Stride->BytesPerPlaneIn, &nchans, &nalpha, DestStartingOrder, DestIncrements);

    if (!(_cmsGetTransformFlags((cmsHTRANSFORM)CMMcargo) & cmsFLAGS_COPY_ALPHA))
        nalpha = 0;

    strideIn = strideOut = 0;
    for (i = 0; i < LineCount; i++) {

        kin = (const cmsUInt8Number*)Input + SourceStartingOrder[0] + strideIn;
        kout = (cmsUInt8Number*)Output + DestStartingOrder[0] + strideOut;

        if (nalpha)
        {
            ain = (const cmsUInt8Number*)Input + SourceStartingOrder[1];
            aout = (cmsUInt8Number*)Output + DestStartingOrder[1];
        }

        for (ii = 0; ii < PixelsPerLine; ii++) {

            *(cmsFloat32Number*)kout = flerp(Data->CurveR, *(cmsFloat32Number*)kin);

            kin += SourceIncrements[0];
            kout += DestIncrements[0];

            if (ain)
            {
                *(cmsFloat32Number*)aout = *(cmsFloat32Number*)ain;
                ain += SourceIncrements[1];
                aout += DestIncrements[1];
            }
        }

        strideIn += Stride->BytesPerLineIn;
        strideOut += Stride->BytesPerLineOut;
    }
}


static void FastFloatGrayIdentity(cmsContext ContextID,
                                struct _cmstransform_struct *CMMcargo,
                                        const void* Input,
                                        void* Output,
                                        cmsUInt32Number PixelsPerLine,
                                        cmsUInt32Number LineCount,
                                        const cmsStride* Stride)
{
    cmsUInt32Number i, ii;
    cmsUInt32Number SourceStartingOrder[cmsMAXCHANNELS];
    cmsUInt32Number SourceIncrements[cmsMAXCHANNELS];
    cmsUInt32Number DestStartingOrder[cmsMAXCHANNELS];
    cmsUInt32Number DestIncrements[cmsMAXCHANNELS];

    const cmsUInt8Number* kin;
    const cmsUInt8Number* ain = NULL;
    cmsUInt8Number* kout;
    cmsUInt8Number* aout = NULL;

    cmsUInt32Number InputFormat  = cmsGetTransformInputFormat(ContextID, (cmsHTRANSFORM) CMMcargo);
    cmsUInt32Number OutputFormat = cmsGetTransformOutputFormat(ContextID, (cmsHTRANSFORM) CMMcargo);

    cmsUInt32Number nchans, nalpha;
    cmsUInt32Number strideIn, strideOut;

    _cmsComputeComponentIncrements(InputFormat, Stride->BytesPerPlaneIn, &nchans, &nalpha, SourceStartingOrder, SourceIncrements);
    _cmsComputeComponentIncrements(OutputFormat, Stride->BytesPerPlaneOut, &nchans, &nalpha, DestStartingOrder, DestIncrements);

    if (!(_cmsGetTransformFlags((cmsHTRANSFORM)CMMcargo) & cmsFLAGS_COPY_ALPHA))
        nalpha = 0;

    strideIn = strideOut = 0;
    for (i = 0; i < LineCount; i++) {


        kin = (const cmsUInt8Number*)Input + SourceStartingOrder[0] + strideIn;
        kout = (cmsUInt8Number*)Output + DestStartingOrder[0] + strideOut;

        if (nalpha)
        {
            ain = (const cmsUInt8Number*)Input + SourceStartingOrder[1];
            aout = (cmsUInt8Number*)Output + DestStartingOrder[1];
        }


        for (ii = 0; ii < PixelsPerLine; ii++) {

            *(cmsFloat32Number*)kout = *(cmsFloat32Number*)kin;

            kin += SourceIncrements[0];
            kout += DestIncrements[0];

            if (ain)
            {
                *(cmsFloat32Number*)aout = *(cmsFloat32Number*)ain;
                ain += SourceIncrements[1];
                aout += DestIncrements[1];
            }
        }
        strideIn += Stride->BytesPerLineIn;
        strideOut += Stride->BytesPerLineOut;
    }
}


#define LINEAR_CURVES_EPSILON 0.00001

// Try to see if the curves are linear
static
cmsBool AllRGBCurvesAreLinear(CurvesFloatData* data)
{
       int j;
       cmsFloat32Number expected;

       for (j = 0; j < MAX_NODES_IN_CURVE; j++) {

              expected = (cmsFloat32Number)j / (cmsFloat32Number)(MAX_NODES_IN_CURVE - 1);

              if (fabsf(data->CurveR[j] - expected) > LINEAR_CURVES_EPSILON ||
                     fabsf(data->CurveG[j] - expected) > LINEAR_CURVES_EPSILON ||
                     fabsf(data->CurveB[j] - expected) > LINEAR_CURVES_EPSILON) {
                     return FALSE;
              }
       }

       return TRUE;
}

static
cmsBool KCurveIsLinear(CurvesFloatData* data)
{
       int j;
       cmsFloat32Number expected;

       for (j = 0; j < MAX_NODES_IN_CURVE; j++) {
              expected = (cmsFloat32Number)j / (cmsFloat32Number)(MAX_NODES_IN_CURVE - 1);

              if (fabs(data->CurveR[j] - expected) > LINEAR_CURVES_EPSILON) return FALSE;
       }


       return TRUE;
}


// Create linearization tables with a reasonable number of entries. Precission is about 32 bits.
static
CurvesFloatData* ComputeCompositeCurves(cmsContext ContextID, cmsUInt32Number nChan,  cmsPipeline* Src)
{
    cmsUInt32Number i, j;
    cmsFloat32Number InFloat[3], OutFloat[3];

    CurvesFloatData* Data = malloc_aligned(ContextID);
    if (Data == NULL) return NULL;

    // Create target curves
    for (i = 0; i < MAX_NODES_IN_CURVE; i++) {

        for (j=0; j <nChan; j++)
               InFloat[j] = (cmsFloat32Number)i /  (cmsFloat32Number)(MAX_NODES_IN_CURVE-1);

        cmsPipelineEvalFloat(ContextID, InFloat, OutFloat, Src);

        if (nChan == 1) {

               Data->CurveR[i] = OutFloat[0];
        }
        else {
               Data->CurveR[i] = OutFloat[0];
               Data->CurveG[i] = OutFloat[1];
               Data->CurveB[i] = OutFloat[2];
        }

    }

    return Data;
}


// If the target LUT holds only curves, the optimization procedure is to join all those
// curves together. That only works on curves and does not work on matrices.
cmsBool OptimizeFloatByJoiningCurves(cmsContext ContextID,
                                  _cmsTransformFn* TransformFn,
                                  void** UserData,
                                  _cmsFreeUserDataFn* FreeUserData,
                                  cmsPipeline** Lut,
                                  cmsUInt32Number* InputFormat,
                                  cmsUInt32Number* OutputFormat,
                                  cmsUInt32Number* dwFlags)
{

    cmsPipeline* Src = *Lut;
    cmsStage* mpe;
    CurvesFloatData* Data;
    cmsUInt32Number nChans;

    // Apply only to floating-point cases
    if (!T_FLOAT(*InputFormat) || !T_FLOAT(*OutputFormat)) return FALSE;

    // Only on 8-bit
    if (T_BYTES(*InputFormat) != 4 ||  T_BYTES(*OutputFormat) != 4) return FALSE;

    // Curves need same channels on input and output (despite extra channels may differ)
    nChans = T_CHANNELS(*InputFormat);
    if (nChans != T_CHANNELS(*OutputFormat)) return FALSE;

    // gray and RGB
    if (nChans != 1 && nChans != 3) return FALSE;

    //  Only curves in this LUT?
    for (mpe = cmsPipelineGetPtrToFirstStage(ContextID, Src);
        mpe != NULL;
        mpe = cmsStageNext(ContextID, mpe)) {

            if (cmsStageType(ContextID, mpe) != cmsSigCurveSetElemType) return FALSE;
    }

    Data = ComputeCompositeCurves(ContextID, nChans, Src);

    *dwFlags |= cmsFLAGS_NOCACHE;
    *dwFlags &= ~cmsFLAGS_CAN_CHANGE_FORMATTER;
    *UserData = Data;
    *FreeUserData = free_aligned;

    // Maybe the curves are linear at the end
    if (nChans == 1)
        *TransformFn = (_cmsTransformFn)(KCurveIsLinear(Data) ? FastFloatGrayIdentity : FastEvaluateFloatGrayCurves);
    else
      *TransformFn = (_cmsTransformFn)(AllRGBCurvesAreLinear(Data) ? FastFloatRGBIdentity : FastEvaluateFloatRGBCurves);

    return TRUE;

}
