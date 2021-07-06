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

// Optimization for matrix-shaper in 8 bits. Numbers are operated in n.14 signed, tables are stored in 1.14 fixed

#include "fast_float_internal.h"

typedef cmsInt32Number cmsS1Fixed14Number;   // Note that this may hold more than 16 bits!

#define DOUBLE_TO_1FIXED14(x) ((cmsS1Fixed14Number) floor((x) * 16384.0 + 0.5))

// This is the private data container used by this optimization
typedef struct {

    // Alignment makes it faster

    cmsS1Fixed14Number Mat[4][4];     // n.14 to n.14 (needs a saturation after that)

    void * real_ptr;

    cmsS1Fixed14Number Shaper1R[256];  // from 0..255 to 1.14  (0.0...1.0)
    cmsS1Fixed14Number Shaper1G[256];
    cmsS1Fixed14Number Shaper1B[256];

    cmsUInt8Number Shaper2R[0x4001];    // 1.14 to 0..255
    cmsUInt8Number Shaper2G[0x4001];
    cmsUInt8Number Shaper2B[0x4001];

} XMatShaper8Data;


static
XMatShaper8Data* malloc_aligned(cmsContext ContextID)
{
    cmsUInt8Number* real_ptr = (cmsUInt8Number*) _cmsMallocZero(ContextID, sizeof(XMatShaper8Data) + 32);
    cmsUInt8Number* aligned = (cmsUInt8Number*) (((uintptr_t)real_ptr + 16) & ~0xf);
    XMatShaper8Data* p = (XMatShaper8Data*) aligned;

    p ->real_ptr = real_ptr;
    return p;
}

static
void free_aligned(cmsContext ContextID, XMatShaper8Data* a)
{
    _cmsFree(ContextID, a->real_ptr);
}


// Free the private data container
static
void  FreeMatShaper(cmsContext ContextID, void* Data)
{
    UNUSED_PARAMETER(ContextID);

    if (Data != NULL) free_aligned(ContextID, (XMatShaper8Data*) Data);
}


// This table converts from 8 bits to 1.14 after applying the curve
static
void FillFirstShaper(cmsContext ContextID, cmsS1Fixed14Number* Table, cmsToneCurve* Curve)
{
    int i;
    cmsFloat32Number R, y;

    for (i=0; i < 256; i++) {

        R   = (cmsFloat32Number) (i / 255.0);
        y   = cmsEvalToneCurveFloat(ContextID, Curve, R);

        Table[i] = DOUBLE_TO_1FIXED14(y);
    }
}


// This table converts form 1.14 (being 0x4000 the last entry) to 8 bits after applying the curve
static
void FillSecondShaper(cmsContext ContextID, cmsUInt8Number* Table, cmsToneCurve* Curve)
{
    int i;
    cmsFloat32Number R, Val;
    cmsInt32Number w;

    for (i=0; i < 0x4001; i++) {

        R   = (cmsFloat32Number) (i / 16384.0f);
        Val = cmsEvalToneCurveFloat(ContextID, Curve, R);
        w = (cmsInt32Number) (Val * 255.0f + 0.5f);
        if (w < 0) w = 0;
        if (w > 255) w = 255;

        Table[i] = (cmsInt8Number) w;

    }
}

// Compute the matrix-shaper structure
static
XMatShaper8Data* SetMatShaper(cmsContext ContextID, cmsToneCurve* Curve1[3], cmsMAT3* Mat, cmsVEC3* Off, cmsToneCurve* Curve2[3])
{
    XMatShaper8Data* p;
    int i, j;

    // Allocate a big chuck of memory to store precomputed tables
    p = malloc_aligned(ContextID);
    if (p == NULL) return FALSE;

    // Precompute tables
    FillFirstShaper(ContextID, p ->Shaper1R, Curve1[0]);
    FillFirstShaper(ContextID, p ->Shaper1G, Curve1[1]);
    FillFirstShaper(ContextID, p ->Shaper1B, Curve1[2]);

    FillSecondShaper(ContextID, p ->Shaper2R, Curve2[0]);
    FillSecondShaper(ContextID, p ->Shaper2G, Curve2[1]);
    FillSecondShaper(ContextID, p ->Shaper2B, Curve2[2]);


    // Convert matrix to nFixed14. Note that those values may take more than 16 bits as
    for (i=0; i < 3; i++) {
        for (j=0; j < 3; j++) {
            p ->Mat[j][i] = DOUBLE_TO_1FIXED14(Mat->v[i].n[j]);
        }
    }

    for (i=0; i < 3; i++) {

        if (Off == NULL) {

            p->Mat[3][i] = DOUBLE_TO_1FIXED14(0.5);
        }
        else {
            p->Mat[3][i] = DOUBLE_TO_1FIXED14(Off->n[i] + 0.5);
        }
    }


    return p;
}

// A fast matrix-shaper evaluator for 8 bits. This is a bit ticky since I'm using 1.14 signed fixed point
// to accomplish some performance. Actually it takes 256x3 16 bits tables and 16385 x 3 tables of 8 bits,
// in total about 50K, and the performance boost is huge!

static
void MatShaperXform8(cmsContext ContextID,
                     struct _cmstransform_struct *CMMcargo,
                     const void* Input,
                     void* Output,
                     cmsUInt32Number PixelsPerLine,
                     cmsUInt32Number LineCount,
                     const cmsStride* Stride)
{
    XMatShaper8Data* p = (XMatShaper8Data*) _cmsGetTransformUserData(CMMcargo);

    cmsS1Fixed14Number l1, l2, l3;
    cmsS1Fixed14Number r, g, b;
    cmsUInt32Number ri, gi, bi;
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

                  // Across first shaper, which also converts to 1.14 fixed point. 16 bits guaranteed.
                  r = p->Shaper1R[*rin];
                  g = p->Shaper1G[*gin];
                  b = p->Shaper1B[*bin];

                  // Evaluate the matrix in 1.14 fixed point
                  l1 = (p->Mat[0][0] * r + p->Mat[1][0] * g + p->Mat[2][0] * b + p->Mat[3][0]) >> 14;
                  l2 = (p->Mat[0][1] * r + p->Mat[1][1] * g + p->Mat[2][1] * b + p->Mat[3][1]) >> 14;
                  l3 = (p->Mat[0][2] * r + p->Mat[1][2] * g + p->Mat[2][2] * b + p->Mat[3][2]) >> 14;


                  // Now we have to clip to 0..1.0 range
                  ri = (l1 < 0) ? 0 : ((l1 > 0x4000) ? 0x4000 : l1);
                  gi = (l2 < 0) ? 0 : ((l2 > 0x4000) ? 0x4000 : l2);
                  bi = (l3 < 0) ? 0 : ((l3 > 0x4000) ? 0x4000 : l3);


                  // And across second shaper,
                  *rout = p->Shaper2R[ri];
                  *gout = p->Shaper2G[gi];
                  *bout = p->Shaper2B[bi];

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


//  8 bits on input allows matrix-shaper boost up a little bit
cmsBool Optimize8MatrixShaper(    cmsContext ContextID,
                                  _cmsTransformFn* TransformFn,
                                  void** UserData,
                                  _cmsFreeUserDataFn* FreeUserData,
                                  cmsPipeline** Lut,
                                  cmsUInt32Number* InputFormat,
                                  cmsUInt32Number* OutputFormat,
                                  cmsUInt32Number* dwFlags)
{
    cmsStage* Curve1, *Curve2;
    cmsStage* Matrix1, *Matrix2;
    _cmsStageMatrixData* Data1;
    _cmsStageMatrixData* Data2;
    cmsMAT3 res;
    cmsBool IdentityMat = FALSE;
    cmsPipeline* Dest, *Src;
    cmsUInt32Number nChans;
    cmsFloat64Number factor = 1.0;

    // Only works on RGB to RGB and gray to gray

    if ( !( (T_CHANNELS(*InputFormat) == 3 && T_CHANNELS(*OutputFormat) == 3) ||
            (T_CHANNELS(*InputFormat) == 1 && T_CHANNELS(*OutputFormat) == 1) )) return FALSE;

    // Only works on 8 bit input
    if (T_BYTES(*InputFormat) != 1 || T_BYTES(*OutputFormat) != 1) return FALSE;

    // Seems suitable, proceed
    Src = *Lut;

    // Check for shaper-matrix-matrix-shaper structure, that is what this optimizer stands for
    if (!cmsPipelineCheckAndRetreiveStages(ContextID, Src, 4,
        cmsSigCurveSetElemType, cmsSigMatrixElemType, cmsSigMatrixElemType, cmsSigCurveSetElemType,
        &Curve1, &Matrix1, &Matrix2, &Curve2)) return FALSE;

    nChans    = T_CHANNELS(*InputFormat);

    // Get both matrices, which are 3x3
    Data1 = (_cmsStageMatrixData*) cmsStageData(ContextID, Matrix1);
    Data2 = (_cmsStageMatrixData*) cmsStageData(ContextID, Matrix2);

    // Input offset should be zero
    if (Data1 ->Offset != NULL) return FALSE;

    if (cmsStageInputChannels(ContextID, Matrix1) == 1 && cmsStageOutputChannels(ContextID, Matrix2) == 1)
    {
        // This is a gray to gray. Just multiply
         factor = Data1->Double[0]*Data2->Double[0] +
                  Data1->Double[1]*Data2->Double[1] +
                  Data1->Double[2]*Data2->Double[2];

        if (fabs(1 - factor) < (1.0 / 65535.0)) IdentityMat = TRUE;
    }
    else
    {
        // Multiply both matrices to get the result
        _cmsMAT3per(ContextID, &res, (cmsMAT3*) Data2 ->Double, (cmsMAT3*) Data1 ->Double);

        // Now the result is in res + Data2 -> Offset. Maybe is a plain identity?
        IdentityMat = FALSE;
        if (_cmsMAT3isIdentity(ContextID, &res) && Data2 ->Offset == NULL) {

            // We can get rid of full matrix
            IdentityMat = TRUE;
        }
    }

      // Allocate an empty LUT
    Dest =  cmsPipelineAlloc(ContextID, nChans, nChans);
    if (!Dest) return FALSE;

    // Assamble the new LUT
    cmsPipelineInsertStage(ContextID, Dest, cmsAT_BEGIN, cmsStageDup(ContextID, Curve1));

    if (!IdentityMat) {

        if (nChans == 1)
             cmsPipelineInsertStage(ContextID, Dest, cmsAT_END,
                    cmsStageAllocMatrix(ContextID, 1, 1, (const cmsFloat64Number*) &factor, Data2->Offset));
        else
            cmsPipelineInsertStage(ContextID, Dest, cmsAT_END,
                    cmsStageAllocMatrix(ContextID, 3, 3, (const cmsFloat64Number*) &res, Data2 ->Offset));
    }


    cmsPipelineInsertStage(ContextID, Dest, cmsAT_END, cmsStageDup(ContextID, Curve2));

    // If identity on matrix, we can further optimize the curves, so call the join curves routine
    if (IdentityMat) {

      Optimize8ByJoiningCurves(ContextID, TransformFn, UserData, FreeUserData, &Dest, InputFormat, OutputFormat, dwFlags);
    }
    else {
        _cmsStageToneCurvesData* mpeC1 = (_cmsStageToneCurvesData*) cmsStageData(ContextID, Curve1);
        _cmsStageToneCurvesData* mpeC2 = (_cmsStageToneCurvesData*) cmsStageData(ContextID, Curve2);

        // In this particular optimization, caché does not help as it takes more time to deal with
        // the caché that with the pixel handling
        *dwFlags |= cmsFLAGS_NOCACHE;


        // Setup the optimizarion routines
        *UserData = SetMatShaper(ContextID, mpeC1 ->TheCurves, &res, (cmsVEC3*) Data2 ->Offset, mpeC2->TheCurves);
        *FreeUserData = FreeMatShaper;

        *TransformFn = (_cmsTransformFn) MatShaperXform8;
    }

    *dwFlags &= ~cmsFLAGS_CAN_CHANGE_FORMATTER;
    cmsPipelineFree(ContextID, Src);
    *Lut = Dest;
    return TRUE;
}
