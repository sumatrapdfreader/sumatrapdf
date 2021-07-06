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

#define PRELINEARIZATION_POINTS 4096

// Optimization for 8 bits, 3 inputs only
typedef struct {

    const cmsInterpParams* p;   // Tetrahedrical interpolation parameters. This is a not-owned pointer.

    cmsUInt16Number rx[256], ry[256], rz[256];
    cmsUInt32Number X0[256], Y0[256], Z0[256];  // Precomputed nodes and offsets for 8-bit input data


} Performance8Data;


// Precomputes tables for 8-bit on input devicelink.
static
Performance8Data* Performance8alloc(cmsContext ContextID, const cmsInterpParams* p, cmsToneCurve* G[3])
{
    int i;
    cmsUInt16Number Input[3];
    cmsS15Fixed16Number v1, v2, v3;
    Performance8Data* p8;

    p8 = (Performance8Data*) _cmsMallocZero(ContextID, sizeof(Performance8Data));
    if (p8 == NULL) return NULL;

    // Since this only works for 8 bit input, values comes always as x * 257,
    // we can safely take msb byte (x << 8 + x)
    for (i=0; i < 256; i++) {

        if (G != NULL) {

            // Get 16-bit representation
            Input[0] = cmsEvalToneCurve16(ContextID, G[0], FROM_8_TO_16(i));
            Input[1] = cmsEvalToneCurve16(ContextID, G[1], FROM_8_TO_16(i));
            Input[2] = cmsEvalToneCurve16(ContextID, G[2], FROM_8_TO_16(i));
        }
        else {
            Input[0] = FROM_8_TO_16(i);
            Input[1] = FROM_8_TO_16(i);
            Input[2] = FROM_8_TO_16(i);
        }

        // Move to 0..1.0 in fixed domain
        v1 = _cmsToFixedDomain(Input[0] * p -> Domain[0]);
        v2 = _cmsToFixedDomain(Input[1] * p -> Domain[1]);
        v3 = _cmsToFixedDomain(Input[2] * p -> Domain[2]);

        // Store the precalculated table of nodes
        p8 ->X0[i] = (p->opta[2] * FIXED_TO_INT(v1));
        p8 ->Y0[i] = (p->opta[1] * FIXED_TO_INT(v2));
        p8 ->Z0[i] = (p->opta[0] * FIXED_TO_INT(v3));

        // Store the precalculated table of offsets
        p8 ->rx[i] = (cmsUInt16Number) FIXED_REST_TO_INT(v1);
        p8 ->ry[i] = (cmsUInt16Number) FIXED_REST_TO_INT(v2);
        p8 ->rz[i] = (cmsUInt16Number) FIXED_REST_TO_INT(v3);
    }


    p8 ->p = p;

    return p8;
}

static
void Performance8free(cmsContext ContextID, void* ptr)
{
    _cmsFree(ContextID, ptr);
}


// Sampler implemented by another LUT. This is a clean way to precalculate the devicelink 3D CLUT for
// almost any transform. We use floating point precision and then convert from floating point to 16 bits.
static
int XFormSampler16(cmsContext ContextID, CMSREGISTER const cmsUInt16Number In[], CMSREGISTER cmsUInt16Number Out[], CMSREGISTER void* Cargo)
{
    // Evaluate in 16 bits
    cmsPipelineEval16(ContextID, In, Out, (cmsPipeline*) Cargo);

    // Always succeed
    return TRUE;
}


// A optimized interpolation for 8-bit input.
#define DENS(i,j,k) (LutTable[(i)+(j)+(k)+OutChan])

static
void PerformanceEval8(cmsContext ContextID,
                      struct _cmstransform_struct *CMMcargo,
                      const void* Input,
                      void* Output,
                      cmsUInt32Number PixelsPerLine,
                      cmsUInt32Number LineCount,
                      const cmsStride* Stride)
{

       cmsUInt8Number         r, g, b;
       cmsS15Fixed16Number    rx, ry, rz;
       cmsS15Fixed16Number    c0, c1, c2, c3, Rest;
       cmsUInt32Number        OutChan, TotalPlusAlpha;
       cmsS15Fixed16Number    X0, X1, Y0, Y1, Z0, Z1;
       Performance8Data*      p8 = (Performance8Data*)_cmsGetTransformUserData(CMMcargo);
       const cmsInterpParams* p = p8->p;
       cmsUInt32Number        TotalOut = p->nOutputs;
       const cmsUInt16Number* LutTable = (const cmsUInt16Number*)p->Table;

       cmsUInt8Number* out[cmsMAXCHANNELS];
       cmsUInt16Number res16;

       cmsUInt32Number i, ii;

       cmsUInt32Number SourceStartingOrder[cmsMAXCHANNELS];
       cmsUInt32Number SourceIncrements[cmsMAXCHANNELS];
       cmsUInt32Number DestStartingOrder[cmsMAXCHANNELS];
       cmsUInt32Number DestIncrements[cmsMAXCHANNELS];

       const cmsUInt8Number* rin;
       const cmsUInt8Number* gin;
       const cmsUInt8Number* bin;
       const cmsUInt8Number* ain = NULL;

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

              TotalPlusAlpha = TotalOut;
              if (ain) TotalPlusAlpha++;

              for (OutChan = 0; OutChan < TotalPlusAlpha; OutChan++) {
                     out[OutChan] = (cmsUInt8Number*)Output + DestStartingOrder[OutChan] + strideOut;
              }


              for (ii = 0; ii < PixelsPerLine; ii++) {

                     r = *rin; g = *gin; b = *bin;

                     rin += SourceIncrements[0];
                     gin += SourceIncrements[1];
                     bin += SourceIncrements[2];

                     X0 = X1 = p8->X0[r];
                     Y0 = Y1 = p8->Y0[g];
                     Z0 = Z1 = p8->Z0[b];

                     rx = p8->rx[r];
                     ry = p8->ry[g];
                     rz = p8->rz[b];

                     X1 = X0 + ((rx == 0) ? 0 : p->opta[2]);
                     Y1 = Y0 + ((ry == 0) ? 0 : p->opta[1]);
                     Z1 = Z0 + ((rz == 0) ? 0 : p->opta[0]);


                     // These are the 6 Tetrahedral
                     for (OutChan = 0; OutChan < TotalOut; OutChan++) {

                            c0 = DENS(X0, Y0, Z0);

                            if (rx >= ry && ry >= rz)
                            {
                                   c1 = DENS(X1, Y0, Z0) - c0;
                                   c2 = DENS(X1, Y1, Z0) - DENS(X1, Y0, Z0);
                                   c3 = DENS(X1, Y1, Z1) - DENS(X1, Y1, Z0);
                            }
                            else
                                   if (rx >= rz && rz >= ry)
                                   {
                                          c1 = DENS(X1, Y0, Z0) - c0;
                                          c2 = DENS(X1, Y1, Z1) - DENS(X1, Y0, Z1);
                                          c3 = DENS(X1, Y0, Z1) - DENS(X1, Y0, Z0);
                                   }
                                   else
                                          if (rz >= rx && rx >= ry)
                                          {
                                                 c1 = DENS(X1, Y0, Z1) - DENS(X0, Y0, Z1);
                                                 c2 = DENS(X1, Y1, Z1) - DENS(X1, Y0, Z1);
                                                 c3 = DENS(X0, Y0, Z1) - c0;
                                          }
                                          else
                                                 if (ry >= rx && rx >= rz)
                                                 {
                                                        c1 = DENS(X1, Y1, Z0) - DENS(X0, Y1, Z0);
                                                        c2 = DENS(X0, Y1, Z0) - c0;
                                                        c3 = DENS(X1, Y1, Z1) - DENS(X1, Y1, Z0);
                                                 }
                                                 else
                                                        if (ry >= rz && rz >= rx)
                                                        {
                                                               c1 = DENS(X1, Y1, Z1) - DENS(X0, Y1, Z1);
                                                               c2 = DENS(X0, Y1, Z0) - c0;
                                                               c3 = DENS(X0, Y1, Z1) - DENS(X0, Y1, Z0);
                                                        }
                                                        else
                                                               if (rz >= ry && ry >= rx)
                                                               {
                                                                      c1 = DENS(X1, Y1, Z1) - DENS(X0, Y1, Z1);
                                                                      c2 = DENS(X0, Y1, Z1) - DENS(X0, Y0, Z1);
                                                                      c3 = DENS(X0, Y0, Z1) - c0;
                                                               }
                                                               else  {
                                                                      c1 = c2 = c3 = 0;
                                                               }


                                                               Rest = c1 * rx + c2 * ry + c3 * rz + 0x8001;
                                                               res16 = (cmsUInt16Number)c0 + ((Rest + (Rest >> 16)) >> 16);

                                                               *out[OutChan] = FROM_16_TO_8(res16);
                                                               out[OutChan] += DestIncrements[OutChan];

                     }

                     if (ain) {
                         *out[TotalOut] = *ain;
                         out[TotalOut] += DestIncrements[TotalOut];
                     }

              }

              strideIn += Stride->BytesPerLineIn;
              strideOut += Stride->BytesPerLineOut;
       }
}

#undef DENS


// Curves that contain wide empty areas are not optimizeable
static
cmsBool IsDegenerated(cmsContext ContextID, const cmsToneCurve* g)
{
    int i, Zeros = 0, Poles = 0;
    int nEntries = cmsGetToneCurveEstimatedTableEntries(ContextID, g);
    const cmsUInt16Number* Table16 = cmsGetToneCurveEstimatedTable(ContextID, g);

    for (i=0; i < nEntries; i++) {

        if (Table16[i] == 0x0000) Zeros++;
        if (Table16[i] == 0xffff) Poles++;
    }

    if (Zeros == 1 && Poles == 1) return FALSE;  // For linear tables
    if (Zeros > (nEntries / 4)) return TRUE;  // Degenerated, mostly zeros
    if (Poles > (nEntries / 4)) return TRUE;  // Degenerated, mostly poles

    return FALSE;
}



// Normalize endpoints by slope limiting max and min. This assures endpoints as well.
// Descending curves are handled as well.
static
void SlopeLimiting(cmsUInt16Number* Table16, int nEntries)
{
    int BeginVal, EndVal;

    int AtBegin = (int) floor((cmsFloat64Number)nEntries * 0.02 + 0.5);   // Cutoff at 2%
    int AtEnd   = nEntries - AtBegin - 1;                                  // And 98%
    cmsFloat64Number Val, Slope, beta;
    int i;


    if (Table16[0] > Table16[nEntries-1]) {
        BeginVal = 0xffff; EndVal = 0;
    }
    else {
        BeginVal = 0; EndVal = 0xffff;
    }

    // Compute slope and offset for begin of curve
    Val   = Table16[AtBegin];
    Slope = (Val - BeginVal) / AtBegin;
    beta  = Val - Slope * AtBegin;

    for (i=0; i < AtBegin; i++)
        Table16[i] = _cmsSaturateWord(i * Slope + beta);

    // Compute slope and offset for the end
    Val   = Table16[AtEnd];
    Slope = (EndVal - Val) / AtBegin;   // AtBegin holds the X interval, which is same in both cases
    beta  = Val - Slope * AtEnd;

    for (i = AtEnd; i < (int) nEntries; i++)
        Table16[i] = _cmsSaturateWord(i * Slope + beta);
}


// --------------------------------------------------------------------------------------------------------------

cmsBool Optimize8BitRGBTransform( cmsContext ContextID,
                                  _cmsTransformFn* TransformFn,
                                  void** UserData,
                                  _cmsFreeUserDataFn* FreeDataFn,
                                  cmsPipeline** Lut,
                                  cmsUInt32Number* InputFormat,
                                  cmsUInt32Number* OutputFormat,
                                  cmsUInt32Number* dwFlags)
{
    cmsPipeline* OriginalLut;
    int nGridPoints;
    cmsToneCurve *Trans[cmsMAXCHANNELS], *TransReverse[cmsMAXCHANNELS];
    cmsUInt32Number t, i, j;
    cmsFloat32Number v, In[cmsMAXCHANNELS], Out[cmsMAXCHANNELS];
    cmsBool lIsSuitable;
    cmsPipeline* OptimizedLUT = NULL, *LutPlusCurves = NULL;
    cmsStage* OptimizedCLUTmpe;
    cmsStage* OptimizedPrelinMpe;
    Performance8Data* p8;
    cmsUInt16Number* MyTable[3];
    _cmsStageCLutData* data;

    // For empty transforms, do nothing
    if (*Lut == NULL) return FALSE;

    // This is a loosy optimization! does not apply in floating-point cases
    if (T_FLOAT(*InputFormat) || T_FLOAT(*OutputFormat)) return FALSE;

    // Only on 8-bit
    if (T_BYTES(*InputFormat) != 1 || T_BYTES(*OutputFormat) != 1) return FALSE;

    // Only on RGB
    if (T_COLORSPACE(*InputFormat)  != PT_RGB) return FALSE;

    OriginalLut = *Lut;

    nGridPoints      = _cmsReasonableGridpointsByColorspace(cmsSigRgbData, *dwFlags);

    // Empty gamma containers
    memset(Trans, 0, sizeof(Trans));
    memset(TransReverse, 0, sizeof(TransReverse));

    MyTable[0] = (cmsUInt16Number*) _cmsMallocZero(ContextID, sizeof(cmsUInt16Number) * PRELINEARIZATION_POINTS);
    MyTable[1] = (cmsUInt16Number*) _cmsMallocZero(ContextID, sizeof(cmsUInt16Number) * PRELINEARIZATION_POINTS);
    MyTable[2] = (cmsUInt16Number*) _cmsMallocZero(ContextID, sizeof(cmsUInt16Number) * PRELINEARIZATION_POINTS);

    if (MyTable[0] == NULL || MyTable[1] == NULL || MyTable[2] == NULL) goto Error;

    // Populate the curves

    for (i=0; i < PRELINEARIZATION_POINTS; i++) {

        v = (cmsFloat32Number) ((cmsFloat64Number) i / (PRELINEARIZATION_POINTS - 1));

        // Feed input with a gray ramp
        for (j=0; j < 3; j++)
            In[j] = v;

        // Evaluate the gray value
        cmsPipelineEvalFloat(ContextID, In, Out, OriginalLut);

        // Store result in curve
        for (j=0; j < 3; j++)
            MyTable[j][i] = _cmsSaturateWord(Out[j] * 65535.0);
    }

    for (t=0; t < 3; t++) {

        SlopeLimiting(MyTable[t], PRELINEARIZATION_POINTS);

        Trans[t] = cmsBuildTabulatedToneCurve16(ContextID, PRELINEARIZATION_POINTS, MyTable[t]);
        if (Trans[t] == NULL) goto Error;

        _cmsFree(ContextID, MyTable[t]);
    }

    // Check for validity
    lIsSuitable = TRUE;
    for (t=0; (lIsSuitable && (t < 3)); t++) {

        // Exclude if non-monotonic
        if (!cmsIsToneCurveMonotonic(ContextID, Trans[t]))
            lIsSuitable = FALSE;

        if (IsDegenerated(ContextID, Trans[t]))
            lIsSuitable = FALSE;
    }

    // If it is not suitable, just quit
    if (!lIsSuitable) goto Error;

    // Invert curves if possible
    for (t = 0; t < cmsPipelineInputChannels(ContextID, OriginalLut); t++) {
        TransReverse[t] = cmsReverseToneCurveEx(ContextID, PRELINEARIZATION_POINTS, Trans[t]);
        if (TransReverse[t] == NULL) goto Error;
    }

    // Now inset the reversed curves at the begin of transform
    LutPlusCurves = cmsPipelineDup(ContextID, OriginalLut);
    if (LutPlusCurves == NULL) goto Error;

    cmsPipelineInsertStage(ContextID, LutPlusCurves, cmsAT_BEGIN, cmsStageAllocToneCurves(ContextID, 3, TransReverse));

    // Create the result LUT
    OptimizedLUT = cmsPipelineAlloc(ContextID, 3, cmsPipelineOutputChannels(ContextID, OriginalLut));
    if (OptimizedLUT == NULL) goto Error;

    OptimizedPrelinMpe = cmsStageAllocToneCurves(ContextID, 3, Trans);

    // Create and insert the curves at the beginning
    cmsPipelineInsertStage(ContextID, OptimizedLUT, cmsAT_BEGIN, OptimizedPrelinMpe);

    // Allocate the CLUT for result
    OptimizedCLUTmpe = cmsStageAllocCLut16bit(ContextID, nGridPoints, 3, cmsPipelineOutputChannels(ContextID, OriginalLut), NULL);

    // Add the CLUT to the destination LUT
    cmsPipelineInsertStage(ContextID, OptimizedLUT, cmsAT_END, OptimizedCLUTmpe);

    // Resample the LUT
    if (!cmsStageSampleCLut16bit(ContextID, OptimizedCLUTmpe, XFormSampler16, (void*) LutPlusCurves, 0)) goto Error;

    // Set the evaluator
    data = (_cmsStageCLutData*) cmsStageData(ContextID, OptimizedCLUTmpe);

    p8 = Performance8alloc(ContextID, data ->Params, Trans);
    if (p8 == NULL) return FALSE;

    // Free resources
    for (t = 0; t <3; t++) {

        if (Trans[t]) cmsFreeToneCurve(ContextID, Trans[t]);
        if (TransReverse[t]) cmsFreeToneCurve(ContextID, TransReverse[t]);
    }

    cmsPipelineFree(ContextID, LutPlusCurves);

    // And return the obtained LUT
    cmsPipelineFree(ContextID, OriginalLut);

    *dwFlags &= ~cmsFLAGS_CAN_CHANGE_FORMATTER;
    *Lut = OptimizedLUT;
    *TransformFn = (_cmsTransformFn)PerformanceEval8;
    *UserData   = p8;
    *FreeDataFn = Performance8free;

    return TRUE;

Error:

    for (t = 0; t < 3; t++) {

        if (Trans[t]) cmsFreeToneCurve(ContextID, Trans[t]);
        if (TransReverse[t]) cmsFreeToneCurve(ContextID, TransReverse[t]);
    }

    if (LutPlusCurves != NULL) cmsPipelineFree(ContextID, LutPlusCurves);
    if (OptimizedLUT != NULL) cmsPipelineFree(ContextID, OptimizedLUT);

    return FALSE;
}
