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

// lcms internal
CMSAPI cmsBool  CMSEXPORT _cmsOptimizePipeline(cmsContext ContextID,
                              cmsPipeline** Lut,
                              cmsUInt32Number  Intent,
                              cmsUInt32Number* InputFormat,
                              cmsUInt32Number* OutputFormat,
                              cmsUInt32Number* dwFlags);


// Optimization for 16 bits, 3 inputs only
typedef struct {

    const cmsInterpParams* p;   // Tetrahedrical interpolation parameters. This is a not-owned pointer.

} Performance16Data;


// Precomputes tables for 16-bit on input devicelink.
static
Performance16Data* Performance16alloc(cmsContext ContextID, const cmsInterpParams* p)
{
    Performance16Data* p16;

    p16 = (Performance16Data*) _cmsMallocZero(ContextID, sizeof(Performance16Data));
    if (p16 == NULL) return NULL;

    p16 ->p = p;

    return p16;
}

static
void Performance16free(cmsContext ContextID, void* ptr)
{
    _cmsFree(ContextID, ptr);
}

/**
* Because cmsChangeBuffersFormat, we have to allow this code to output data in either 8 or 16 bits.
* The increments are already computed correctly, but the data may change. So, we use a macro to
* increase xput
*/
#define TO_OUTPUT_16(d,v)  do { *(cmsUInt16Number*) (d) = v; } while(0)
#define TO_OUTPUT_8(d,v)   do { *(cmsUInt8Number*) (d) = FROM_16_TO_8(v); } while(0)

#define TO_OUTPUT(d,v) do { if (out16) TO_OUTPUT_16(d,v); else TO_OUTPUT_8(d,v); } while(0)

#define FROM_INPUT(v) (in16 ? (*((const cmsUInt16Number*)(v))) : FROM_8_TO_16(*((const cmsUInt8Number*)(v))))

static
void PerformanceEval16(cmsContext ContextID,
                      struct _cmstransform_struct *CMMcargo,
                      const void* Input,
                      void* Output,
                      cmsUInt32Number PixelsPerLine,
                      cmsUInt32Number LineCount,
                      const cmsStride* Stride)
{

       cmsUInt16Number        r, g, b;
       int                    x0, y0, z0;
       cmsS15Fixed16Number    rx, ry, rz;
       cmsS15Fixed16Number    fx, fy, fz;
       cmsS15Fixed16Number    c0, c1, c2, c3, Rest;
       cmsUInt32Number        OutChan, TotalPlusAlpha;
       cmsS15Fixed16Number    X0, X1, Y0, Y1, Z0, Z1;
       Performance16Data*     p16 = (Performance16Data*)_cmsGetTransformUserData(CMMcargo);
       const cmsInterpParams* p = p16->p;
       cmsUInt32Number        TotalOut = p->nOutputs;
       const cmsUInt16Number* BaseTable = (const cmsUInt16Number*)p->Table;
       const cmsUInt16Number* LutTable;

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

       int    in16, out16;  // Used by macros!

       cmsUInt32Number nalpha, strideIn, strideOut;

       cmsUInt32Number dwInFormat = cmsGetTransformInputFormat(ContextID, (cmsHTRANSFORM)CMMcargo);
       cmsUInt32Number dwOutFormat = cmsGetTransformOutputFormat(ContextID, (cmsHTRANSFORM)CMMcargo);

       _cmsComputeComponentIncrements(dwInFormat, Stride->BytesPerPlaneIn, NULL, &nalpha, SourceStartingOrder, SourceIncrements);
       _cmsComputeComponentIncrements(dwOutFormat, Stride->BytesPerPlaneOut, NULL, &nalpha, DestStartingOrder, DestIncrements);

       in16  = (T_BYTES(dwInFormat) == 2);
       out16 = (T_BYTES(dwOutFormat) == 2);

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

                  r = FROM_INPUT(rin);
                  g = FROM_INPUT(gin);
                  b = FROM_INPUT(bin);

                  rin += SourceIncrements[0];
                  gin += SourceIncrements[1];
                  bin += SourceIncrements[2];

                  fx = _cmsToFixedDomain((int)r * p->Domain[0]);
                  fy = _cmsToFixedDomain((int)g * p->Domain[1]);
                  fz = _cmsToFixedDomain((int)b * p->Domain[2]);

                  x0 = FIXED_TO_INT(fx);
                  y0 = FIXED_TO_INT(fy);
                  z0 = FIXED_TO_INT(fz);

                  rx = FIXED_REST_TO_INT(fx);
                  ry = FIXED_REST_TO_INT(fy);
                  rz = FIXED_REST_TO_INT(fz);

                  X0 = p->opta[2] * x0;
                  X1 = (r == 0xFFFFU ? 0 : p->opta[2]);

                  Y0 = p->opta[1] * y0;
                  Y1 = (g == 0xFFFFU ? 0 : p->opta[1]);

                  Z0 = p->opta[0] * z0;
                  Z1 = (b == 0xFFFFU ? 0 : p->opta[0]);


                  LutTable = &BaseTable[X0 + Y0 + Z0];

                  // Output should be computed as x = ROUND_FIXED_TO_INT(_cmsToFixedDomain(Rest))
                  // which expands as: x = (Rest + ((Rest+0x7fff)/0xFFFF) + 0x8000)>>16
                  // This can be replaced by: t = Rest+0x8001, x = (t + (t>>16))>>16
                  // at the cost of being off by one at 7fff and 17ffe.

                  if (rx >= ry) {
                      if (ry >= rz) {
                          Y1 += X1;
                          Z1 += Y1;
                          for (OutChan = 0; OutChan < TotalOut; OutChan++) {
                              c1 = LutTable[X1];
                              c2 = LutTable[Y1];
                              c3 = LutTable[Z1];
                              c0 = *LutTable++;
                              c3 -= c2;
                              c2 -= c1;
                              c1 -= c0;
                              Rest = c1 * rx + c2 * ry + c3 * rz + 0x8001;
                              res16 = (cmsUInt16Number)c0 + ((Rest + (Rest >> 16)) >> 16);
                              TO_OUTPUT(out[OutChan], res16);
                              out[OutChan] += DestIncrements[OutChan];
                          }
                      }
                      else if (rz >= rx) {
                          X1 += Z1;
                          Y1 += X1;
                          for (OutChan = 0; OutChan < TotalOut; OutChan++) {
                              c1 = LutTable[X1];
                              c2 = LutTable[Y1];
                              c3 = LutTable[Z1];
                              c0 = *LutTable++;
                              c2 -= c1;
                              c1 -= c3;
                              c3 -= c0;
                              Rest = c1 * rx + c2 * ry + c3 * rz + 0x8001;
                              res16 = (cmsUInt16Number)c0 + ((Rest + (Rest >> 16)) >> 16);
                              TO_OUTPUT(out[OutChan], res16);
                              out[OutChan] += DestIncrements[OutChan];
                          }
                      }
                      else {
                          Z1 += X1;
                          Y1 += Z1;
                          for (OutChan = 0; OutChan < TotalOut; OutChan++) {
                              c1 = LutTable[X1];
                              c2 = LutTable[Y1];
                              c3 = LutTable[Z1];
                              c0 = *LutTable++;
                              c2 -= c3;
                              c3 -= c1;
                              c1 -= c0;
                              Rest = c1 * rx + c2 * ry + c3 * rz + 0x8001;
                              res16 = (cmsUInt16Number)c0 + ((Rest + (Rest >> 16)) >> 16);
                              TO_OUTPUT(out[OutChan], res16);
                              out[OutChan] += DestIncrements[OutChan];
                          }
                      }
                  }
                  else {
                      if (rx >= rz) {
                          X1 += Y1;
                          Z1 += X1;
                          for (OutChan = 0; OutChan < TotalOut; OutChan++) {
                              c1 = LutTable[X1];
                              c2 = LutTable[Y1];
                              c3 = LutTable[Z1];
                              c0 = *LutTable++;
                              c3 -= c1;
                              c1 -= c2;
                              c2 -= c0;
                              Rest = c1 * rx + c2 * ry + c3 * rz + 0x8001;
                              res16 = (cmsUInt16Number)c0 + ((Rest + (Rest >> 16)) >> 16);
                              TO_OUTPUT(out[OutChan], res16);
                              out[OutChan] += DestIncrements[OutChan];
                          }
                      }
                      else if (ry >= rz) {
                          Z1 += Y1;
                          X1 += Z1;
                          for (OutChan = 0; OutChan < TotalOut; OutChan++) {
                              c1 = LutTable[X1];
                              c2 = LutTable[Y1];
                              c3 = LutTable[Z1];
                              c0 = *LutTable++;
                              c1 -= c3;
                              c3 -= c2;
                              c2 -= c0;
                              Rest = c1 * rx + c2 * ry + c3 * rz + 0x8001;
                              res16 = (cmsUInt16Number)c0 + ((Rest + (Rest >> 16)) >> 16);
                              TO_OUTPUT(out[OutChan], res16);
                              out[OutChan] += DestIncrements[OutChan];
                          }
                      }
                      else {
                          Y1 += Z1;
                          X1 += Y1;
                          for (OutChan = 0; OutChan < TotalOut; OutChan++) {
                              c1 = LutTable[X1];
                              c2 = LutTable[Y1];
                              c3 = LutTable[Z1];
                              c0 = *LutTable++;
                              c1 -= c2;
                              c2 -= c3;
                              c3 -= c0;
                              Rest = c1 * rx + c2 * ry + c3 * rz + 0x8001;
                              res16 = (cmsUInt16Number)c0 + ((Rest + (Rest >> 16)) >> 16);
                              TO_OUTPUT(out[OutChan], res16);
                              out[OutChan] += DestIncrements[OutChan];
                          }
                      }
                  }

                  if (ain)
                  {
                      res16 = *(const cmsUInt16Number*)ain;
                      TO_OUTPUT(out[OutChan], res16);
                      out[TotalOut] += DestIncrements[TotalOut];
                  }

              }

              strideIn += Stride->BytesPerLineIn;
              strideOut += Stride->BytesPerLineOut;
       }
}

#undef DENS



// --------------------------------------------------------------------------------------------------------------

cmsBool Optimize16BitRGBTransform(cmsContext ContextID,
                                  _cmsTransformFn* TransformFn,
                                  void** UserData,
                                  _cmsFreeUserDataFn* FreeDataFn,
                                  cmsPipeline** Lut,
                                  cmsUInt32Number* InputFormat,
                                  cmsUInt32Number* OutputFormat,
                                  cmsUInt32Number* dwFlags)
{
    Performance16Data* p16;
    _cmsStageCLutData* data;
    cmsUInt32Number newFlags;
    cmsStage* OptimizedCLUTmpe;


    // For empty transforms, do nothing
    if (*Lut == NULL) return FALSE;

    // This is a loosy optimization! does not apply in floating-point cases
    if (T_FLOAT(*InputFormat) || T_FLOAT(*OutputFormat)) return FALSE;

    // Only on 16-bit
    if (T_BYTES(*InputFormat) != 2 || T_BYTES(*OutputFormat) != 2) return FALSE;

    // Only real 16 bits
    if (T_BIT15(*InputFormat) != 0 || T_BIT15(*OutputFormat) != 0) return FALSE;

	// Swap endian is not supported
    if (T_ENDIAN16(*InputFormat) != 0 || T_ENDIAN16(*OutputFormat) != 0) return FALSE;

    // Only on input RGB
    if (T_COLORSPACE(*InputFormat)  != PT_RGB) return FALSE;


    // If this is a matrix-shaper, the default does already a good job
    if (cmsPipelineCheckAndRetreiveStages(ContextID, *Lut, 4,
        cmsSigCurveSetElemType, cmsSigMatrixElemType, cmsSigMatrixElemType, cmsSigCurveSetElemType,
        NULL, NULL, NULL, NULL)) return FALSE;

    if (cmsPipelineCheckAndRetreiveStages(ContextID, *Lut, 2,
        cmsSigCurveSetElemType, cmsSigCurveSetElemType,
        NULL, NULL)) return FALSE;


    newFlags = *dwFlags | cmsFLAGS_FORCE_CLUT;

    if (!_cmsOptimizePipeline(ContextID,
                               Lut,
                               INTENT_PERCEPTUAL,  // Dont care
                               InputFormat,
                               OutputFormat,
                               &newFlags)) return FALSE;

    OptimizedCLUTmpe = cmsPipelineGetPtrToFirstStage(ContextID, *Lut);

    // Set the evaluator
    data = (_cmsStageCLutData*)cmsStageData(ContextID, OptimizedCLUTmpe);

    p16 = Performance16alloc(ContextID, data->Params);
    if (p16 == NULL) return FALSE;

    *TransformFn = (_cmsTransformFn)PerformanceEval16;
    *UserData   = p16;
    *FreeDataFn = Performance16free;
    *InputFormat  |= 0x02000000;
    *OutputFormat |= 0x02000000;
    *dwFlags |= cmsFLAGS_CAN_CHANGE_FORMATTER;

    return TRUE;
}
