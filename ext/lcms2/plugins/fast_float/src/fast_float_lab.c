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


#define SIGMOID_POINTS 1024

// Optimization for floating point tetrahedral interpolation  using Lab as indexing space
typedef struct {

    cmsContext ContextID;
    const cmsInterpParams* p;   // Tetrahedrical interpolation parameters. This is a not-owned pointer.

    cmsFloat32Number sigmoidIn[SIGMOID_POINTS];   // to apply to a*/b* axis on indexing
    cmsFloat32Number sigmoidOut[SIGMOID_POINTS];  // the curve above, inverted.

} LabCLUTdata;


typedef struct {

    LabCLUTdata* data;
    cmsPipeline* original;

} ResamplingContainer;

/**
* Predefined tone curve
*/
#define TYPE_SIGMOID  109


// Floating-point version of 1D interpolation
cmsINLINE cmsFloat32Number LinLerp1D(cmsFloat32Number Value, const cmsFloat32Number* LutTable)
{
    if (Value >= 1.0f)
    {
        return LutTable[SIGMOID_POINTS - 1];
    }
    else
        if (Value <= 0)
        {
            return LutTable[0];
        }
        else
        {
            cmsFloat32Number y1, y0;
            cmsFloat32Number rest;
            int cell0, cell1;

            Value *= (SIGMOID_POINTS - 1);

            cell0 = _cmsQuickFloor(Value);
            cell1 = cell0 + 1;

            rest = Value - cell0;

            y0 = LutTable[cell0];
            y1 = LutTable[cell1];

            return y0 + (y1 - y0) * rest;
        }
}

static
void tabulateSigmoid(cmsContext ContextID, cmsInt32Number type, cmsFloat32Number table[], cmsInt32Number tablePoints)
{
    const cmsFloat64Number sigmoidal_slope = 2.5;
    cmsToneCurve* original;
    cmsInt32Number i;

    memset(table, 0, sizeof(cmsFloat32Number) * tablePoints);
    original = cmsBuildParametricToneCurve(ContextID, type, &sigmoidal_slope);
    if (original != NULL)
    {
        for (i = 0; i < tablePoints; i++)
        {
            cmsFloat32Number v = (cmsFloat32Number)i / (cmsFloat32Number)(tablePoints - 1);

            table[i] = fclamp(cmsEvalToneCurveFloat(ContextID, original, v));
        }

        cmsFreeToneCurve(ContextID, original);
    }
}


// Allocates container and curves
static
LabCLUTdata* LabCLUTAlloc(cmsContext ContextID, const cmsInterpParams* p)
{
    LabCLUTdata* fd;

    fd = (LabCLUTdata*) _cmsMallocZero(ContextID, sizeof(LabCLUTdata));
    if (fd == NULL) return NULL;

    fd ->ContextID = ContextID;
    fd ->p = p;

    tabulateSigmoid(ContextID, +TYPE_SIGMOID, fd->sigmoidIn, SIGMOID_POINTS);
    tabulateSigmoid(ContextID, -TYPE_SIGMOID, fd->sigmoidOut, SIGMOID_POINTS);

    return fd;
}

static
void LabCLUTFree(cmsContext ContextID, void* v)
{
    _cmsFree(ContextID, v);
}

// Sampler implemented by another LUT.
static
int XFormSampler(cmsContext ContextID, CMSREGISTER const cmsFloat32Number In[], CMSREGISTER cmsFloat32Number Out[], CMSREGISTER void* Cargo)
{
    ResamplingContainer* container = (ResamplingContainer*)Cargo;
    cmsFloat32Number linearized[3];

    // Apply inverse sigmoid
    linearized[0] = In[0];
    linearized[1] = LinLerp1D(In[1], container->data->sigmoidOut);
    linearized[2] = LinLerp1D(In[2], container->data->sigmoidOut);

    cmsPipelineEvalFloat(ContextID, linearized, Out, container->original);
    return TRUE;
}


// To prevent out of bounds indexing
cmsINLINE cmsFloat32Number fclamp128(cmsFloat32Number v)
{
    return ((v < -128) || isnan(v)) ? -128.0f : (v > 128.0f ? 128.0f : v);
}

cmsINLINE cmsFloat32Number fclamp100(cmsFloat32Number v)
{
    return ((v < 1.0e-9f) || isnan(v)) ? 0.0f : (v > 100.0f ? 100.0f : v);
}

// A optimized interpolation for Lab.
#define DENS(i,j,k) (LutTable[(i)+(j)+(k)+OutChan])

static
void LabCLUTEval(cmsContext ContextID,
                        struct _cmstransform_struct* CMMcargo,
                        const void* Input,
                        void* Output,
                        cmsUInt32Number PixelsPerLine,
                        cmsUInt32Number LineCount,
                        const cmsStride* Stride)

{

    LabCLUTdata* pfloat = (LabCLUTdata*)_cmsGetTransformUserData(CMMcargo);

    cmsFloat32Number        l, a, b;
    cmsFloat32Number        px, py, pz;
    int                     x0, y0, z0;
    int                     X0, Y0, Z0, X1, Y1, Z1;
    cmsFloat32Number        rx, ry, rz;
    cmsFloat32Number        c0, c1 = 0, c2 = 0, c3 = 0;
    cmsUInt32Number         OutChan;

    const cmsInterpParams* p = pfloat->p;
    cmsUInt32Number        TotalOut = p->nOutputs;
    cmsUInt32Number        TotalPlusAlpha;
    const cmsFloat32Number* LutTable = (const cmsFloat32Number*)p->Table;

    cmsUInt32Number       i, ii;
    const cmsUInt8Number* lin;
    const cmsUInt8Number* ain;
    const cmsUInt8Number* bin;
    const cmsUInt8Number* xin = NULL;

    cmsUInt8Number* out[cmsMAXCHANNELS];
    cmsUInt32Number SourceStartingOrder[cmsMAXCHANNELS];
    cmsUInt32Number SourceIncrements[cmsMAXCHANNELS];
    cmsUInt32Number DestStartingOrder[cmsMAXCHANNELS];
    cmsUInt32Number DestIncrements[cmsMAXCHANNELS];

    cmsUInt32Number InputFormat = cmsGetTransformInputFormat(ContextID, (cmsHTRANSFORM)CMMcargo);
    cmsUInt32Number OutputFormat = cmsGetTransformOutputFormat(ContextID, (cmsHTRANSFORM)CMMcargo);

    cmsUInt32Number nchans, nalpha;
    cmsUInt32Number strideIn, strideOut;

    _cmsComputeComponentIncrements(InputFormat, Stride->BytesPerPlaneIn, &nchans, &nalpha, SourceStartingOrder, SourceIncrements);
    _cmsComputeComponentIncrements(OutputFormat, Stride->BytesPerPlaneOut, &nchans, &nalpha, DestStartingOrder, DestIncrements);

    if (!(_cmsGetTransformFlags((cmsHTRANSFORM)CMMcargo) & cmsFLAGS_COPY_ALPHA))
        nalpha = 0;

    strideIn = strideOut = 0;
    for (i = 0; i < LineCount; i++) {

        lin = (const cmsUInt8Number*)Input + SourceStartingOrder[0] + strideIn;
        ain = (const cmsUInt8Number*)Input + SourceStartingOrder[1] + strideIn;
        bin = (const cmsUInt8Number*)Input + SourceStartingOrder[2] + strideIn;

        if (nalpha)
            xin = (const cmsUInt8Number*)Input + SourceStartingOrder[3] + strideIn;

        TotalPlusAlpha = TotalOut;
        if (xin) TotalPlusAlpha++;

        for (ii = 0; ii < TotalPlusAlpha; ii++)
            out[ii] = (cmsUInt8Number*)Output + DestStartingOrder[ii] + strideOut;

        for (ii = 0; ii < PixelsPerLine; ii++) {

            // Decode Lab and go across sigmoids on a*/b*
            l = fclamp100( *(cmsFloat32Number*)lin ) / 100.0f;

            a = LinLerp1D((( fclamp128( *(cmsFloat32Number*)ain)) + 128.0f) / 255.0f, pfloat->sigmoidIn);
            b = LinLerp1D((( fclamp128( *(cmsFloat32Number*)bin)) + 128.0f) / 255.0f, pfloat->sigmoidIn);

            lin += SourceIncrements[0];
            ain += SourceIncrements[1];
            bin += SourceIncrements[2];

            px = l * p->Domain[0];
            py = a * p->Domain[1];
            pz = b * p->Domain[2];

            x0 = _cmsQuickFloor(px); rx = (px - (cmsFloat32Number)x0);
            y0 = _cmsQuickFloor(py); ry = (py - (cmsFloat32Number)y0);
            z0 = _cmsQuickFloor(pz); rz = (pz - (cmsFloat32Number)z0);

            X0 = p->opta[2] * x0;
            X1 = X0 + (l >= 1.0f ? 0 : p->opta[2]);

            Y0 = p->opta[1] * y0;
            Y1 = Y0 + (a >= 1.0f ? 0 : p->opta[1]);

            Z0 = p->opta[0] * z0;
            Z1 = Z0 + (b >= 1.0f ? 0 : p->opta[0]);

            for (OutChan = 0; OutChan < TotalOut; OutChan++) {

                // These are the 6 Tetrahedral

                c0 = DENS(X0, Y0, Z0);

                if (rx >= ry && ry >= rz) {

                    c1 = DENS(X1, Y0, Z0) - c0;
                    c2 = DENS(X1, Y1, Z0) - DENS(X1, Y0, Z0);
                    c3 = DENS(X1, Y1, Z1) - DENS(X1, Y1, Z0);

                }
                else
                    if (rx >= rz && rz >= ry) {

                        c1 = DENS(X1, Y0, Z0) - c0;
                        c2 = DENS(X1, Y1, Z1) - DENS(X1, Y0, Z1);
                        c3 = DENS(X1, Y0, Z1) - DENS(X1, Y0, Z0);

                    }
                    else
                        if (rz >= rx && rx >= ry) {

                            c1 = DENS(X1, Y0, Z1) - DENS(X0, Y0, Z1);
                            c2 = DENS(X1, Y1, Z1) - DENS(X1, Y0, Z1);
                            c3 = DENS(X0, Y0, Z1) - c0;

                        }
                        else
                            if (ry >= rx && rx >= rz) {

                                c1 = DENS(X1, Y1, Z0) - DENS(X0, Y1, Z0);
                                c2 = DENS(X0, Y1, Z0) - c0;
                                c3 = DENS(X1, Y1, Z1) - DENS(X1, Y1, Z0);

                            }
                            else
                                if (ry >= rz && rz >= rx) {

                                    c1 = DENS(X1, Y1, Z1) - DENS(X0, Y1, Z1);
                                    c2 = DENS(X0, Y1, Z0) - c0;
                                    c3 = DENS(X0, Y1, Z1) - DENS(X0, Y1, Z0);

                                }
                                else
                                    if (rz >= ry && ry >= rx) {

                                        c1 = DENS(X1, Y1, Z1) - DENS(X0, Y1, Z1);
                                        c2 = DENS(X0, Y1, Z1) - DENS(X0, Y0, Z1);
                                        c3 = DENS(X0, Y0, Z1) - c0;

                                    }
                                    else {
                                        c1 = c2 = c3 = 0;
                                    }

                *(cmsFloat32Number*)(out[OutChan]) = c0 + c1 * rx + c2 * ry + c3 * rz;

                out[OutChan] += DestIncrements[OutChan];
            }

            if (xin)
            {
                *(cmsFloat32Number*) (out[TotalOut]) = *xin;
                out[TotalOut] += DestIncrements[TotalOut];
            }
        }

        strideIn  += Stride->BytesPerLineIn;
        strideOut += Stride->BytesPerLineOut;
    }
}

#undef DENS


/**
* Get from flags
*/
static
int GetGridpoints(cmsUInt32Number dwFlags)
{
    // Already specified?
    if (dwFlags & 0x00FF0000) {
        return (dwFlags >> 16) & 0xFF;
    }

    // HighResPrecalc is maximum resolution
    if (dwFlags & cmsFLAGS_HIGHRESPRECALC) {
        return 66;
    }
    else
        // LowResPrecal is lower resolution
        if (dwFlags & cmsFLAGS_LOWRESPRECALC) {
            return 33;
        }
        else
            return 51;

}

// --------------------------------------------------------------------------------------------------------------

cmsBool OptimizeCLUTLabTransform(cmsContext ContextID,
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
    cmsPipeline* OptimizedLUT = NULL;
    cmsStage* OptimizedCLUTmpe;
    LabCLUTdata* pfloat;
    _cmsStageCLutData* data;
    ResamplingContainer container;


    // For empty transforms, do nothing
    if (*Lut == NULL) return FALSE;

    // Check for floating point only
    if (!T_FLOAT(*InputFormat) || !T_FLOAT(*OutputFormat)) return FALSE;

    // Only on floats
    if (T_BYTES(*InputFormat) != sizeof(cmsFloat32Number) ||
        T_BYTES(*OutputFormat) != sizeof(cmsFloat32Number)) return FALSE;

    if (T_COLORSPACE(*InputFormat) != PT_Lab) return FALSE;

    OriginalLut = *Lut;

    nGridPoints = GetGridpoints(*dwFlags);

    // Create the result LUT
    OptimizedLUT = cmsPipelineAlloc(ContextID, 3, cmsPipelineOutputChannels(ContextID, OriginalLut));
    if (OptimizedLUT == NULL) goto Error;

    // Allocate the CLUT for result
    OptimizedCLUTmpe = cmsStageAllocCLutFloat(ContextID, nGridPoints, 3, cmsPipelineOutputChannels(ContextID, OriginalLut), NULL);

    // Add the CLUT to the destination LUT
    cmsPipelineInsertStage(ContextID, OptimizedLUT, cmsAT_BEGIN, OptimizedCLUTmpe);

    // Set the evaluator, copy parameters
    data = (_cmsStageCLutData*) cmsStageData(ContextID, OptimizedCLUTmpe);

    // Allocate data
    pfloat = LabCLUTAlloc(ContextID, data ->Params);
    if (pfloat == NULL) return FALSE;

    container.data = pfloat;
    container.original = OriginalLut;

    // Resample the LUT
    if (!cmsStageSampleCLutFloat(ContextID, OptimizedCLUTmpe, XFormSampler, (void*)&container, 0)) goto Error;

    // And return the obtained LUT
    cmsPipelineFree(ContextID, OriginalLut);

    *Lut = OptimizedLUT;
    *TransformFn = (_cmsTransformFn)LabCLUTEval;
    *UserData   = pfloat;
    *FreeDataFn = LabCLUTFree;
    *dwFlags &= ~cmsFLAGS_CAN_CHANGE_FORMATTER;
    return TRUE;

Error:

    if (OptimizedLUT != NULL) cmsPipelineFree(ContextID, OptimizedLUT);

    return FALSE;
}
