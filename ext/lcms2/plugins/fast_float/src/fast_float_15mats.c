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

// Optimization for matrix-shaper in 15 bits. Numbers are operated in 1.15 usigned,

#include "fast_float_internal.h"

// An storage capable to keep 1.15 signed and some extra precission.
// Actually I use 32 bits integer (signed)
typedef cmsInt32Number cmsS1Fixed15Number;

// Conversion to fixed. Note we don't use floor to get proper sign roundoff
#define DOUBLE_TO_1FIXED15(x) ((cmsS1Fixed15Number) ((double) (x) * 0x8000 + 0.5))

// This is the private data container used by this optimization
typedef struct {

       cmsS1Fixed15Number Mat[3][3];
       cmsS1Fixed15Number Off[3];

       // Precalculated tables for first shaper (375 Kb in total of both shapers)
       cmsUInt16Number Shaper1R[MAX_NODES_IN_CURVE];
       cmsUInt16Number Shaper1G[MAX_NODES_IN_CURVE];
       cmsUInt16Number Shaper1B[MAX_NODES_IN_CURVE];

       // Second shaper
       cmsUInt16Number Shaper2R[MAX_NODES_IN_CURVE];
       cmsUInt16Number Shaper2G[MAX_NODES_IN_CURVE];
       cmsUInt16Number Shaper2B[MAX_NODES_IN_CURVE];

       // A flag for fast operation if identity
       cmsBool IdentityMat;

       // Poits to the raw, unaligned memory
       void * real_ptr;


} XMatShaperData;

// A special malloc that returns memory aligned to DWORD boundary. Aligned memory access is way faster than unaligned
// reference to the real block is kept for later free
static XMatShaperData* malloc_aligned(cmsContext ContextID)
{
       cmsUInt8Number* real_ptr = (cmsUInt8Number*)_cmsMallocZero(ContextID, sizeof(XMatShaperData) + 32);
       cmsUInt8Number* aligned = (cmsUInt8Number*)(((uintptr_t)real_ptr + 16) & ~0xf);
       XMatShaperData* p = (XMatShaperData*)aligned;

       p->real_ptr = real_ptr;
       return p;
}


// Free the private data container
static
void  FreeMatShaper(cmsContext ContextID, void* Data)
{

       XMatShaperData* p = (XMatShaperData*)Data;
       if (p != NULL)
              _cmsFree(ContextID, p->real_ptr);
}


// This table converts from 8 bits to 1.14 after applying the curve
static
void FillShaper(cmsContext ContextID, cmsUInt16Number* Table, cmsToneCurve* Curve)
{
       int i;
       cmsFloat32Number R, y;

       for (i = 0; i < MAX_NODES_IN_CURVE; i++) {

              R = (cmsFloat32Number)i / (cmsFloat32Number) (MAX_NODES_IN_CURVE - 1);
              y = cmsEvalToneCurveFloat(ContextID, Curve, R);

              Table[i] = (cmsUInt16Number) DOUBLE_TO_1FIXED15(y);
       }
}


// Compute the matrix-shaper structure
static
XMatShaperData* SetMatShaper(cmsContext ContextID, cmsToneCurve* Curve1[3], cmsMAT3* Mat, cmsVEC3* Off, cmsToneCurve* Curve2[3], cmsBool IdentityMat)
{
       XMatShaperData* p;
       int i, j;

       // Allocate a big chuck of memory to store precomputed tables
       p = malloc_aligned(ContextID);
       if (p == NULL) return FALSE;

       p->IdentityMat = IdentityMat;

       // Precompute tables
       FillShaper(ContextID, p->Shaper1R, Curve1[0]);
       FillShaper(ContextID, p->Shaper1G, Curve1[1]);
       FillShaper(ContextID, p->Shaper1B, Curve1[2]);

       FillShaper(ContextID, p->Shaper2R, Curve2[0]);
       FillShaper(ContextID, p->Shaper2G, Curve2[1]);
       FillShaper(ContextID, p->Shaper2B, Curve2[2]);

       // Convert matrix to nFixed14. Note that those values may take more than 16 bits if negative
       for (i = 0; i < 3; i++) {
              for (j = 0; j < 3; j++) {

                     p->Mat[i][j] = DOUBLE_TO_1FIXED15(Mat->v[i].n[j]);
              }
       }


       for (i = 0; i < 3; i++) {

              if (Off == NULL) {

                     p->Off[i] = 0x4000;

              }
              else {
                     p->Off[i] = DOUBLE_TO_1FIXED15(Off->n[i]) + 0x4000;

              }
       }


       return p;
}

// A fast matrix-shaper evaluator for 15 bits. This is a bit ticky since I'm using 1.15 signed fixed point.
static
void MatShaperXform(cmsContext ContextID,
                    struct _cmstransform_struct *CMMcargo,
                    const void* Input,
                    void* Output,
                    cmsUInt32Number PixelsPerLine,
                    cmsUInt32Number LineCount,
                    const cmsStride* Stride)
{
       XMatShaperData* p = (XMatShaperData*)_cmsGetTransformUserData(CMMcargo);

       cmsS1Fixed15Number l1, l2, l3;

       cmsS1Fixed15Number r, g, b;
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

                     // Across first shaper, which also converts to 1.15 fixed point.
                     r = p->Shaper1R[*(cmsUInt16Number*)rin];
                     g = p->Shaper1G[*(cmsUInt16Number*)gin];
                     b = p->Shaper1B[*(cmsUInt16Number*)bin];

                     if (p->IdentityMat)
                     {
                            l1 = r; l2 = g; l3 = b;
                     }
                     else
                     {
                            // Evaluate the matrix in 1.14 fixed point
                            l1 = (p->Mat[0][0] * r + p->Mat[0][1] * g + p->Mat[0][2] * b + p->Off[0]) >> 15;
                            l2 = (p->Mat[1][0] * r + p->Mat[1][1] * g + p->Mat[1][2] * b + p->Off[1]) >> 15;
                            l3 = (p->Mat[2][0] * r + p->Mat[2][1] * g + p->Mat[2][2] * b + p->Off[2]) >> 15;
                     }

                     // Now we have to clip to 0..1.0 range
                     ri = (l1 < 0) ? 0 : ((l1 > 0x8000) ? 0x8000 : l1);
                     gi = (l2 < 0) ? 0 : ((l2 > 0x8000) ? 0x8000 : l2);
                     bi = (l3 < 0) ? 0 : ((l3 > 0x8000) ? 0x8000 : l3);


                     // And across second shaper,
                     *(cmsUInt16Number*)rout = p->Shaper2R[ri];
                     *(cmsUInt16Number*)gout = p->Shaper2G[gi];
                     *(cmsUInt16Number*)bout = p->Shaper2B[bi];


                     // Handle alpha
                     if (ain) {
                            memmove(aout, ain, 2);
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



//  15 bits on input allows matrix-shaper boost up a little bit
cmsBool OptimizeMatrixShaper15(cmsContext ContextID,
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

       // Only works on RGB to RGB and gray

       if (!(T_CHANNELS(*InputFormat) == 3 && T_CHANNELS(*OutputFormat) == 3)) return FALSE;

       // Only works on 15 bit to 15 bit
       if (T_BYTES(*InputFormat) != 2 || T_BYTES(*OutputFormat) != 2 ||
              T_BIT15(*InputFormat) == 0 || T_BIT15(*OutputFormat) == 0) return FALSE;

       // Seems suitable, proceed
       Src = *Lut;

       // Check for shaper-matrix-matrix-shaper structure, that is what this optimizer stands for
       if (!cmsPipelineCheckAndRetreiveStages(ContextID, Src, 4,
              cmsSigCurveSetElemType, cmsSigMatrixElemType, cmsSigMatrixElemType, cmsSigCurveSetElemType,
              &Curve1, &Matrix1, &Matrix2, &Curve2)) return FALSE;

       nChans = T_CHANNELS(*InputFormat);

       // Get both matrices, which are 3x3
       Data1 = (_cmsStageMatrixData*)cmsStageData(ContextID, Matrix1);
       Data2 = (_cmsStageMatrixData*)cmsStageData(ContextID, Matrix2);

       // Input offset should be zero
       if (Data1->Offset != NULL) return FALSE;

       // Multiply both matrices to get the result
       _cmsMAT3per(ContextID, &res, (cmsMAT3*)Data2->Double, (cmsMAT3*)Data1->Double);

       // Now the result is in res + Data2 -> Offset. Maybe is a plain identity?
       IdentityMat = FALSE;
       if (_cmsMAT3isIdentity(ContextID, &res) && Data2->Offset == NULL) {

              // We can get rid of full matrix
              IdentityMat = TRUE;
       }


       // Allocate an empty LUT
       Dest = cmsPipelineAlloc(ContextID, nChans, nChans);
       if (!Dest) return FALSE;

       // Assamble the new LUT
       cmsPipelineInsertStage(ContextID, Dest, cmsAT_BEGIN, cmsStageDup(ContextID, Curve1));

       if (!IdentityMat) {

              cmsPipelineInsertStage(ContextID, Dest, cmsAT_END,
                     cmsStageAllocMatrix(ContextID, 3, 3, (const cmsFloat64Number*)&res, Data2->Offset));
       }

       cmsPipelineInsertStage(ContextID, Dest, cmsAT_END, cmsStageDup(ContextID, Curve2));

       {
              _cmsStageToneCurvesData* mpeC1 = (_cmsStageToneCurvesData*)cmsStageData(ContextID, Curve1);
              _cmsStageToneCurvesData* mpeC2 = (_cmsStageToneCurvesData*)cmsStageData(ContextID, Curve2);

              // In this particular optimization, caché does not help as it takes more time to deal with
              // the caché that with the pixel handling
              *dwFlags |= cmsFLAGS_NOCACHE;

              // Setup the optimizarion routines
              *UserData = SetMatShaper(ContextID, mpeC1->TheCurves, &res, (cmsVEC3*)Data2->Offset, mpeC2->TheCurves, IdentityMat);
              *FreeUserData = FreeMatShaper;

              *TransformFn = (_cmsTransformFn)MatShaperXform;
       }


       cmsPipelineFree(ContextID, Src);
       *dwFlags &= ~cmsFLAGS_CAN_CHANGE_FORMATTER;
       *Lut = Dest;
       return TRUE;
}
