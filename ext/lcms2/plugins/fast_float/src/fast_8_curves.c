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

// Curves, optimization is valid for 8 bits only
typedef struct {

    int nCurves;
    cmsUInt8Number Curves[cmsMAXCHANNELS][256];

} Curves8Data;


// Evaluator for RGB 8-bit curves. This are just 1D tables
static void FastEvaluateRGBCurves8(cmsContext ContextID,
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

       cmsUInt32Number nalpha, strideIn, strideOut;

       Curves8Data* Data = (Curves8Data*)_cmsGetTransformUserData(CMMcargo);

       _cmsComputeComponentIncrements(cmsGetTransformInputFormat(ContextID, (cmsHTRANSFORM)CMMcargo), Stride->BytesPerPlaneIn, NULL, &nalpha, SourceStartingOrder, SourceIncrements);
       _cmsComputeComponentIncrements(cmsGetTransformOutputFormat(ContextID, (cmsHTRANSFORM)CMMcargo), Stride->BytesPerPlaneOut, NULL, &nalpha, DestStartingOrder, DestIncrements);

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


                     *rout = Data->Curves[0][*rin];
                     *gout = Data->Curves[1][*gin];
                     *bout = Data->Curves[2][*bin];

                     // Handle alpha
                     if (ain) {
                           *aout = *ain;
                     }

                     rin += SourceIncrements[0];
                     gin += SourceIncrements[1];
                     bin += SourceIncrements[2];
                     if (ain) ain += SourceIncrements[3];

                     rout += DestIncrements[0];
                     gout += DestIncrements[1];
                     bout += DestIncrements[2];
                     if (aout) aout += DestIncrements[3];
              }

              strideIn += Stride->BytesPerLineIn;
              strideOut += Stride->BytesPerLineOut;
       }
}


// Do nothing but arrange the format. RGB
static void FastRGBIdentity8(cmsContext ContextID,
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

       cmsUInt32Number nalpha, strideIn, strideOut;

       _cmsComputeComponentIncrements(cmsGetTransformInputFormat(ContextID, (cmsHTRANSFORM)CMMcargo), Stride->BytesPerPlaneIn, NULL, &nalpha, SourceStartingOrder, SourceIncrements);
       _cmsComputeComponentIncrements(cmsGetTransformOutputFormat(ContextID, (cmsHTRANSFORM)CMMcargo), Stride->BytesPerPlaneOut, NULL, &nalpha, DestStartingOrder, DestIncrements);

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


                     *rout = *rin;
                     *gout = *gin;
                     *bout = *bin;

                     // Handle alpha
                     if (ain) {
                            *aout = *ain;
                     }

                     rin += SourceIncrements[0];
                     gin += SourceIncrements[1];
                     bin += SourceIncrements[2];
                     if (ain) ain += SourceIncrements[3];

                     rout += DestIncrements[0];
                     gout += DestIncrements[1];
                     bout += DestIncrements[2];
                     if (aout) aout += DestIncrements[3];
              }

              strideIn += Stride->BytesPerLineIn;
              strideOut += Stride->BytesPerLineOut;
       }
}



// Evaluate 1 channel only
static void FastEvaluateGrayCurves8(cmsContext ContextID,
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

       const cmsUInt8Number* gin;
       const cmsUInt8Number* ain = NULL;

       cmsUInt8Number* gout;
       cmsUInt8Number* aout = NULL;

       cmsUInt32Number nalpha, strideIn, strideOut;

       Curves8Data* Data = (Curves8Data*)_cmsGetTransformUserData(CMMcargo);

       _cmsComputeComponentIncrements(cmsGetTransformInputFormat(ContextID, (cmsHTRANSFORM)CMMcargo), Stride->BytesPerPlaneIn, NULL, &nalpha, SourceStartingOrder, SourceIncrements);
       _cmsComputeComponentIncrements(cmsGetTransformOutputFormat(ContextID, (cmsHTRANSFORM)CMMcargo), Stride->BytesPerPlaneOut, NULL, &nalpha, DestStartingOrder, DestIncrements);

       if (!(_cmsGetTransformFlags((cmsHTRANSFORM)CMMcargo) & cmsFLAGS_COPY_ALPHA))
           nalpha = 0;

       strideIn = strideOut = 0;
       for (i = 0; i < LineCount; i++) {

              gin = (const cmsUInt8Number*)Input + SourceStartingOrder[0] + strideIn;
              if (nalpha)
                     ain = (const cmsUInt8Number*)Input + SourceStartingOrder[1] + strideIn;

              gout = (cmsUInt8Number*)Output + DestStartingOrder[0] + strideOut;
              if (nalpha)
                     aout = (cmsUInt8Number*)Output + DestStartingOrder[1] + strideOut;

              for (ii = 0; ii < PixelsPerLine; ii++) {

                     *gout = Data->Curves[0][*gin];

                     // Handle alpha
                     if (ain) {
                            *aout = *ain;
                     }

                     gin += SourceIncrements[0];

                     if (ain) ain += SourceIncrements[1];

                     gout += DestIncrements[0];

                     if (aout) aout += DestIncrements[1];
              }

              strideIn += Stride->BytesPerLineIn;
              strideOut += Stride->BytesPerLineOut;
       }
}


static void FastGrayIdentity8(cmsContext ContextID,
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

       const cmsUInt8Number* gin;
       const cmsUInt8Number* ain = NULL;

       cmsUInt8Number* gout;
       cmsUInt8Number* aout = NULL;

       cmsUInt32Number nalpha, strideIn, strideOut;

       _cmsComputeComponentIncrements(cmsGetTransformInputFormat(ContextID, (cmsHTRANSFORM)CMMcargo), Stride->BytesPerPlaneIn, NULL, &nalpha, SourceStartingOrder, SourceIncrements);
       _cmsComputeComponentIncrements(cmsGetTransformOutputFormat(ContextID, (cmsHTRANSFORM)CMMcargo), Stride->BytesPerPlaneOut, NULL, &nalpha, DestStartingOrder, DestIncrements);

       if (!(_cmsGetTransformFlags((cmsHTRANSFORM)CMMcargo) & cmsFLAGS_COPY_ALPHA))
           nalpha = 0;

       strideIn = strideOut = 0;
       for (i = 0; i < LineCount; i++) {

              gin = (const cmsUInt8Number*)Input + SourceStartingOrder[0] + strideIn;
              if (nalpha)
                     ain = (const cmsUInt8Number*)Input + SourceStartingOrder[1] + strideIn;

              gout = (cmsUInt8Number*)Output + DestStartingOrder[0] + strideOut;
              if (nalpha)
                     aout = (cmsUInt8Number*)Output + DestStartingOrder[1] + strideOut;

              for (ii = 0; ii < PixelsPerLine; ii++) {

                     *gout = *gin;

                     // Handle alpha
                     if (ain) {
                            *aout = *ain;
                     }

                     gin += SourceIncrements[0];

                     if (ain) ain += SourceIncrements[1];

                     gout += DestIncrements[0];

                     if (aout) aout += DestIncrements[1];
              }

              strideIn += Stride->BytesPerLineIn;
              strideOut += Stride->BytesPerLineOut;
       }
}





// Try to see if the curves are linear
static
cmsBool AllCurvesAreLinear(Curves8Data* data)
{
    int i, j;

    for (i=0; i < 3; i++) {
        for (j = 0; j < 256; j++) {
            if (data ->Curves[i][j] != j) return FALSE;
        }
    }

    return TRUE;
}


static
Curves8Data* ComputeCompositeCurves(cmsContext ContextID, cmsUInt32Number nChan,  cmsPipeline* Src)
{
    cmsUInt32Number i, j;
    cmsFloat32Number InFloat[3], OutFloat[3];

    Curves8Data* Data = (Curves8Data*) _cmsMallocZero(ContextID, sizeof(Curves8Data));
    if (Data == NULL) return NULL;

    // Create target curves
    for (i=0; i < 256; i++) {

        for (j=0; j <nChan; j++)
            InFloat[j] = (cmsFloat32Number) ((cmsFloat64Number) i / 255.0);

        cmsPipelineEvalFloat(ContextID, InFloat, OutFloat, Src);

        for (j=0; j < nChan; j++)
            Data -> Curves[j][i] = FROM_16_TO_8(_cmsSaturateWord(OutFloat[j] * 65535.0));
    }

    return Data;
}


// If the target LUT holds only curves, the optimization procedure is to join all those
// curves together. That only works on curves and does not work on matrices.
// Any number of channels up to 16
cmsBool Optimize8ByJoiningCurves(cmsContext ContextID,
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
    Curves8Data* Data;
    cmsUInt32Number nChans;

    // This is a loosy optimization! does not apply in floating-point cases
    if (T_FLOAT(*InputFormat) || T_FLOAT(*OutputFormat)) return FALSE;

    // Only on 8-bit
    if (T_BYTES(*InputFormat) != 1 ||  T_BYTES(*OutputFormat) != 1) return FALSE;

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
    *FreeUserData = _cmsFree;

    // Maybe the curves are linear at the end
    if (nChans == 1)
      *TransformFn = (_cmsTransformFn)(AllCurvesAreLinear(Data) ? FastGrayIdentity8 : FastEvaluateGrayCurves8);
    else
        *TransformFn = (_cmsTransformFn)(AllCurvesAreLinear(Data) ? FastRGBIdentity8 : FastEvaluateRGBCurves8);

    return TRUE;

}
