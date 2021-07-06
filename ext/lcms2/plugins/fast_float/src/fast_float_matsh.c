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

// Optimization for matrix-shaper in float

#include "fast_float_internal.h"


// This is the private data container used by this optimization
typedef struct {


    cmsFloat32Number Mat[3][3];
    cmsFloat32Number Off[3];

    cmsFloat32Number Shaper1R[MAX_NODES_IN_CURVE];
    cmsFloat32Number Shaper1G[MAX_NODES_IN_CURVE];
    cmsFloat32Number Shaper1B[MAX_NODES_IN_CURVE];

    cmsFloat32Number Shaper2R[MAX_NODES_IN_CURVE];
    cmsFloat32Number Shaper2G[MAX_NODES_IN_CURVE];
    cmsFloat32Number Shaper2B[MAX_NODES_IN_CURVE];

    cmsBool UseOff;

    void * real_ptr;

} VXMatShaperFloatData;


static
VXMatShaperFloatData* malloc_aligned(cmsContext ContextID)
{
    cmsUInt8Number* real_ptr = (cmsUInt8Number*) _cmsMallocZero(ContextID, sizeof(VXMatShaperFloatData) + 32);
    cmsUInt8Number* aligned = (cmsUInt8Number*) (((uintptr_t)real_ptr + 16) & ~0xf);
    VXMatShaperFloatData* p = (VXMatShaperFloatData*) aligned;

    p ->real_ptr = real_ptr;
    return p;
}



// Free the private data container
static
void  FreeMatShaper(cmsContext ContextID, void* Data)
{
       VXMatShaperFloatData* d = (VXMatShaperFloatData*)Data;

       if (d != NULL)
              _cmsFree(ContextID, d->real_ptr);
}


static
void FillShaper(cmsContext ContextID, cmsFloat32Number* Table, cmsToneCurve* Curve)
{
    int i;
    cmsFloat32Number R;

    for (i = 0; i < MAX_NODES_IN_CURVE; i++) {

           R = (cmsFloat32Number) i / (cmsFloat32Number) (MAX_NODES_IN_CURVE - 1);

        Table[i] = cmsEvalToneCurveFloat(ContextID, Curve, R);
    }
}


// Compute the matrix-shaper structure
static
VXMatShaperFloatData* SetMatShaper(cmsContext ContextID, cmsToneCurve* Curve1[3], cmsMAT3* Mat, cmsVEC3* Off, cmsToneCurve* Curve2[3])
{
    VXMatShaperFloatData* p;
    int i, j;

    // Allocate a big chuck of memory to store precomputed tables
    p = malloc_aligned(ContextID);
    if (p == NULL) return FALSE;


    // Precompute tables
    FillShaper(ContextID, p->Shaper1R, Curve1[0]);
    FillShaper(ContextID, p->Shaper1G, Curve1[1]);
    FillShaper(ContextID, p->Shaper1B, Curve1[2]);

    FillShaper(ContextID, p->Shaper2R, Curve2[0]);
    FillShaper(ContextID, p->Shaper2G, Curve2[1]);
    FillShaper(ContextID, p->Shaper2B, Curve2[2]);


    for (i=0; i < 3; i++) {
        for (j=0; j < 3; j++) {
               p->Mat[i][j] = (cmsFloat32Number) Mat->v[i].n[j];
        }
    }


    for (i = 0; i < 3; i++) {

           if (Off == NULL) {

                  p->UseOff = FALSE;
                  p->Off[i] = 0.0;
           }
           else {
                  p->UseOff = TRUE;
                  p->Off[i] = (cmsFloat32Number)Off->n[i];

           }
    }


    return p;
}



// A fast matrix-shaper evaluator for floating point
static
void MatShaperFloat(cmsContext ContextID, struct _cmstransform_struct *CMMcargo,
                        const void* Input,
                        void* Output,
                        cmsUInt32Number PixelsPerLine,
                        cmsUInt32Number LineCount,
                        const cmsStride* Stride)
{
    VXMatShaperFloatData* p = (VXMatShaperFloatData*) _cmsGetTransformUserData(CMMcargo);
    cmsFloat32Number l1, l2, l3;
    cmsFloat32Number r, g, b;
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

    cmsUInt32Number nchans, nalpha;
    cmsUInt32Number strideIn, strideOut;

    _cmsComputeComponentIncrements(cmsGetTransformInputFormat(ContextID, (cmsHTRANSFORM)CMMcargo), Stride->BytesPerPlaneIn, &nchans, &nalpha, SourceStartingOrder, SourceIncrements);
    _cmsComputeComponentIncrements(cmsGetTransformOutputFormat(ContextID, (cmsHTRANSFORM)CMMcargo), Stride->BytesPerPlaneOut, &nchans, &nalpha, DestStartingOrder, DestIncrements);

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

            r = flerp(p->Shaper1R, *(cmsFloat32Number*)rin);
            g = flerp(p->Shaper1G, *(cmsFloat32Number*)gin);
            b = flerp(p->Shaper1B, *(cmsFloat32Number*)bin);

            l1 = p->Mat[0][0] * r + p->Mat[0][1] * g + p->Mat[0][2] * b;
            l2 = p->Mat[1][0] * r + p->Mat[1][1] * g + p->Mat[1][2] * b;
            l3 = p->Mat[2][0] * r + p->Mat[2][1] * g + p->Mat[2][2] * b;

            if (p->UseOff) {

                l1 += p->Off[0];
                l2 += p->Off[1];
                l3 += p->Off[2];
            }

            *(cmsFloat32Number*)rout = flerp(p->Shaper2R, l1);
            *(cmsFloat32Number*)gout = flerp(p->Shaper2G, l2);
            *(cmsFloat32Number*)bout = flerp(p->Shaper2B, l3);

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



cmsBool OptimizeFloatMatrixShaper(cmsContext ContextID,
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


    // Apply only to floating-point cases
    if (!T_FLOAT(*InputFormat) || !T_FLOAT(*OutputFormat)) return FALSE;

    // Only works on RGB to RGB and gray to gray
    if ( !( (T_CHANNELS(*InputFormat) == 3 && T_CHANNELS(*OutputFormat) == 3))  &&
         !( (T_CHANNELS(*InputFormat) == 1 && T_CHANNELS(*OutputFormat) == 1))) return FALSE;

    // Only works on float
    if (T_BYTES(*InputFormat) != 4 || T_BYTES(*OutputFormat) != 4) return FALSE;

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

           OptimizeFloatByJoiningCurves(ContextID, TransformFn, UserData, FreeUserData, &Dest, InputFormat, OutputFormat, dwFlags);
    }
    else {
        _cmsStageToneCurvesData* mpeC1 = (_cmsStageToneCurvesData*) cmsStageData(ContextID, Curve1);
        _cmsStageToneCurvesData* mpeC2 = (_cmsStageToneCurvesData*) cmsStageData(ContextID, Curve2);

        // In this particular optimization, caché does not help as it takes more time to deal with
        // the cachthat with the pixel handling
        *dwFlags |= cmsFLAGS_NOCACHE;

        // Setup the optimizarion routines
        *UserData = SetMatShaper(ContextID, mpeC1 ->TheCurves, &res, (cmsVEC3*) Data2 ->Offset, mpeC2->TheCurves);
        *FreeUserData = FreeMatShaper;

        *TransformFn = (_cmsTransformFn)MatShaperFloat;
    }

    *dwFlags &= ~cmsFLAGS_CAN_CHANGE_FORMATTER;
    cmsPipelineFree(ContextID, Src);
    *Lut = Dest;
    return TRUE;
}
