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

// Optimization for matrix-shaper in 8 bits using SSE2 intrinsics

#include "fast_float_internal.h"


#ifndef CMS_DONT_USE_SSE2

#ifdef _MSC_VER
#include <intrin.h>
#else
#include <x86intrin.h>
#include <cpuid.h>
#endif

#include <emmintrin.h>


// This is the private data container used by this optimization
typedef struct {

    // This is for SSE, MUST be aligned at 16 bit boundary

    cmsFloat32Number Mat[4][4];     // n.14 to n.14 (needs a saturation after that)

    void * real_ptr;

    cmsContext ContextID;

    cmsFloat32Number Shaper1R[256];  // from 0..255 to 1.14  (0.0...1.0)
    cmsFloat32Number Shaper1G[256];
    cmsFloat32Number Shaper1B[256];

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
void FreeMatShaper(cmsContext ContextID, void* Data)
{
    UNUSED_PARAMETER(ContextID);

    if (Data != NULL) free_aligned(ContextID, (XMatShaper8Data*) Data);
}


// This table converts from 8 bits to 1.14 after applying the curve
static
void FillFirstShaper(cmsContext ContextID, cmsFloat32Number* Table, cmsToneCurve* Curve)
{
    cmsInt32Number i;
    cmsFloat32Number R;

    for (i = 0; i < 256; i++) {

        R = (cmsFloat32Number)(i / 255.0);
        Table[i] = cmsEvalToneCurveFloat(ContextID, Curve, R);
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


    // Convert matrix to float
    for (i=0; i < 3; i++) {
        for (j=0; j < 3; j++) {
            p ->Mat[j][i] = (cmsFloat32Number) Mat->v[i].n[j];
        }
    }

    // Roundoff
    for (i=0; i < 3; i++) {

        if (Off == NULL) {

            p->Mat[3][i] = 0.0f;
        }
        else {
            p->Mat[3][i] = (cmsFloat32Number)Off->n[i];
        }
    }


    return p;
}

// A fast matrix-shaper evaluator for 8 bits.
static
void MatShaperXform8SSE(cmsContext ContextID,
                     struct _cmstransform_struct *CMMcargo,
                     const void* Input,
                     void* Output,
                     cmsUInt32Number PixelsPerLine,
                     cmsUInt32Number LineCount,
                     const cmsStride* Stride)
{
    XMatShaper8Data* p = (XMatShaper8Data*) _cmsGetTransformUserData(CMMcargo);

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

    __m128 mat0 = _mm_load_ps(p->Mat[0]);
    __m128 mat1 = _mm_load_ps(p->Mat[1]);
    __m128 mat2 = _mm_load_ps(p->Mat[2]);
    __m128 mat3 = _mm_load_ps(p->Mat[3]);

    __m128 zero = _mm_setzero_ps();
    __m128 one = _mm_set1_ps(1.0f);
    __m128 scale = _mm_set1_ps((cmsFloat32Number)0x4000);

    cmsUInt8Number buffer[32];
    cmsUInt32Number* output_index = (cmsUInt32Number*)(((uintptr_t)buffer + 16) & ~0xf);


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

           /**
           * Prefetch
           */
           __m128 rvector = _mm_set1_ps(p->Shaper1R[*rin]);
           __m128 gvector = _mm_set1_ps(p->Shaper1G[*gin]);
           __m128 bvector = _mm_set1_ps(p->Shaper1B[*bin]);

           for (ii = 0; ii < PixelsPerLine; ii++) {

               __m128 el1 = _mm_mul_ps(rvector, mat0);
               __m128 el2 = _mm_mul_ps(gvector, mat1);
               __m128 el3 = _mm_mul_ps(bvector, mat2);

               __m128 sum = _mm_add_ps(el1, _mm_add_ps(el2, _mm_add_ps(el3, mat3)));

               __m128 out = _mm_min_ps(_mm_max_ps(sum, zero), one);

               out = _mm_mul_ps(out, scale);

               /**
               * Rounding and converting to index.
               * Actually this is a costly instruction that may be blocking performance
               */
               _mm_store_si128((__m128i*)output_index, _mm_cvtps_epi32(out));


               // Handle alpha
               if (ain) {
                   *aout = *ain;
               }

               rin += SourceIncrements[0];
               gin += SourceIncrements[1];
               bin += SourceIncrements[2];
               if (ain) ain += SourceIncrements[3];

               /**
               * Take next value whilst store is being performed
               */
               if (ii < PixelsPerLine - 1)
               {
                   rvector = _mm_set1_ps(p->Shaper1R[*rin]);
                   gvector = _mm_set1_ps(p->Shaper1G[*gin]);
                   bvector = _mm_set1_ps(p->Shaper1B[*bin]);
               }

               *rout = p->Shaper2R[output_index[0]];
               *gout = p->Shaper2G[output_index[1]];
               *bout = p->Shaper2B[output_index[2]];

               rout += DestIncrements[0];
               gout += DestIncrements[1];
               bout += DestIncrements[2];
               if (aout) aout += DestIncrements[3];
           }

           strideIn += Stride->BytesPerLineIn;
           strideOut += Stride->BytesPerLineOut;
    }
}


static
cmsBool IsSSE2Available(void)
{
#ifdef _MSC_VER
    int cpuinfo[4];

    __cpuid(cpuinfo, 1);
    if (!(cpuinfo[3] & (1 << 26))) return FALSE;
    return TRUE;

#else
  unsigned int level = 1u;
  unsigned int eax, ebx, ecx, edx;
  unsigned int bits = (1u << 26);
  unsigned int max = __get_cpuid_max(0, NULL);
  if (level > max) {
    return FALSE;
  }
  __cpuid_count(level, 0, eax, ebx, ecx, edx);
  return (edx & bits) == bits;
#endif
}


//  8 bits on input allows matrix-shaper boost up a little bit
cmsBool Optimize8MatrixShaperSSE(cmsContext ContextID,
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

    // Check for SSE2 support
    if (!(IsSSE2Available())) return FALSE;

    // Only works on 3 to 3, probably RGB
    if ( !( (T_CHANNELS(*InputFormat) == 3 && T_CHANNELS(*OutputFormat) == 3) ) ) return FALSE;

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
    Dest =  cmsPipelineAlloc(ContextID, nChans, nChans);
    if (!Dest) return FALSE;

    // Assamble the new LUT
    cmsPipelineInsertStage(ContextID, Dest, cmsAT_BEGIN, cmsStageDup(ContextID, Curve1));

    if (!IdentityMat) {

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

        // In this particular optimization, cache does not help as it takes more time to deal with
        // the cache that with the pixel handling
        *dwFlags |= cmsFLAGS_NOCACHE;


        // Setup the optimizarion routines
        *UserData = SetMatShaper(ContextID, mpeC1 ->TheCurves, &res, (cmsVEC3*) Data2 ->Offset, mpeC2->TheCurves);
        *FreeUserData = FreeMatShaper;

        *TransformFn = (_cmsTransformFn) MatShaperXform8SSE;
    }

    *dwFlags &= ~cmsFLAGS_CAN_CHANGE_FORMATTER;
    cmsPipelineFree(ContextID, Src);
    *Lut = Dest;
    return TRUE;
}

#endif
