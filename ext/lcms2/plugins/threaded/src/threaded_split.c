//---------------------------------------------------------------------------------
//
//  Little Color Management System, fast floating point extensions
//  Copyright (c) 1998-2022 Marti Maria Saguer, all rights reserved
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


#include "threaded_internal.h"


// Returns true component size
cmsINLINE cmsUInt32Number ComponentSize(cmsUInt32Number format)
{
    cmsUInt32Number BytesPerComponent = T_BYTES(format);

    // For double, the T_BYTES field is zero
    if (BytesPerComponent == 0)
        BytesPerComponent = sizeof(cmsUInt64Number);

    return BytesPerComponent;
}

// Returns bytes from one pixel to the next
cmsINLINE cmsUInt32Number PixelSpacing(cmsUInt32Number format)
{
    if (T_PLANAR(format))
        return ComponentSize(format);
    else
        return ComponentSize(format) * (T_CHANNELS(format) + T_EXTRA(format));
}

// macro is not portable
cmsINLINE cmsUInt32Number minimum(cmsUInt32Number a, cmsUInt32Number b)
{
    return a < b ? a : b;
}


// Memory of block depends of planar or chunky. If lines is 1, then the stride does not contain
// information and we have to calculate the size. If lines > 1, then we can take line size from stride.
// if planar, total memory is number of planes per plane stride. If chunky memory is number of lines per
// line size. If line size is zero, then it should be computed.
static
cmsUInt32Number MemSize(cmsUInt32Number format, 
                        cmsUInt32Number PixelsPerLine, 
                        cmsUInt32Number LineCount, 
                        cmsUInt32Number* BytesPerLine,
                        cmsUInt32Number BytesPerPlane)
{
    if (T_PLANAR(format)) {

        if (*BytesPerLine == 0) {
            
            *BytesPerLine = ComponentSize(format) * PixelsPerLine;
        }

        return (T_CHANNELS(format) + T_EXTRA(format)) * BytesPerPlane;
    }
    else
    {
        if (*BytesPerLine == 0) {

            *BytesPerLine = ComponentSize(format) * (T_CHANNELS(format) + T_EXTRA(format)) * PixelsPerLine;
        }

        return LineCount * *BytesPerLine;
    }
}

// Compute how many workers to use. Repairs Stride if any missing member
cmsUInt32Number _cmsThrCountSlices(struct _cmstransform_struct* CMMcargo, cmsInt32Number MaxWorkers,
                                   cmsUInt32Number PixelsPerLine, cmsUInt32Number LineCount, 
                                   cmsStride* Stride)
{	    
    cmsInt32Number MaxInputMem,  MaxOutputMem;
    cmsInt32Number WorkerCount;

    cmsInt32Number MaxCPUs = _cmsThrIdealThreadCount();

    if (MaxWorkers == CMS_THREADED_GUESS_MAX_THREADS) {
        MaxWorkers = MaxCPUs;
    }
    else
    {
        // We allow large number of threads, but this is not going to work well. Warn it. 
        if (MaxWorkers > MaxCPUs) {
            cmsSignalError(NULL, cmsERROR_RANGE,
                "Warning: too many threads for actual processor (CPUs=%s, asked=%d)", MaxCPUs, MaxWorkers);
        }
    }

    MaxInputMem = MemSize(cmsGetTransformInputFormat((cmsHTRANSFORM)CMMcargo),
                         PixelsPerLine, LineCount, &Stride->BytesPerLineIn, Stride->BytesPerPlaneIn);                         

    MaxOutputMem = MemSize(cmsGetTransformOutputFormat((cmsHTRANSFORM)CMMcargo),
                         PixelsPerLine, LineCount, &Stride->BytesPerLineOut, Stride->BytesPerPlaneOut);
    
    // Each thread takes 128K at least
    WorkerCount = (MaxInputMem + MaxOutputMem) / (128 * 1024);

    if (WorkerCount < 1)
        WorkerCount = 1;
    else
        if (WorkerCount > MaxWorkers)
            WorkerCount = MaxWorkers;

    return WorkerCount;
}

// Slice input, output for lines
static
void SlicePerLines(const _cmsWorkSlice* master, cmsInt32Number nslices, 
                            cmsInt32Number LinesPerSlice, _cmsWorkSlice slices[])
{
    cmsInt32Number i;
    cmsInt32Number TotalLines = master ->LineCount;

    for (i = 0; i < nslices; i++) {

        const cmsUInt8Number* PtrInput = master->InputBuffer;
        cmsUInt8Number* PtrOutput = master->OutputBuffer;

        cmsInt32Number  lines = minimum(LinesPerSlice, TotalLines);

        memcpy(&slices[i], master, sizeof(_cmsWorkSlice));

        slices[i].InputBuffer  = PtrInput + i * LinesPerSlice * master->Stride->BytesPerLineIn;
        slices[i].OutputBuffer = PtrOutput + i * LinesPerSlice * master->Stride->BytesPerLineOut;

        slices[i].LineCount = lines;
        TotalLines -= lines;
    }

    // Add left lines because rounding
    if (slices > 0) slices[nslices - 1].LineCount += TotalLines;
}

// Per pixels on big blocks of one line
static
void SlicePerPixels(const _cmsWorkSlice* master, cmsInt32Number nslices,
                    cmsInt32Number PixelsPerSlice, _cmsWorkSlice slices[])
{
    cmsInt32Number i;
    cmsInt32Number TotalPixels = master->PixelsPerLine; // As this works on one line only

    cmsUInt32Number PixelSpacingIn = PixelSpacing(cmsGetTransformInputFormat((cmsHTRANSFORM)master->CMMcargo));
    cmsUInt32Number PixelSpacingOut = PixelSpacing(cmsGetTransformOutputFormat((cmsHTRANSFORM)master->CMMcargo));

    for (i = 0; i < nslices; i++) {

        const cmsUInt8Number* PtrInput = master->InputBuffer;
        cmsUInt8Number* PtrOutput = master->OutputBuffer;

        cmsInt32Number pixels = minimum(PixelsPerSlice, TotalPixels);

        memcpy(&slices[i], master, sizeof(_cmsWorkSlice));

        slices[i].InputBuffer = PtrInput + i * PixelsPerSlice * PixelSpacingIn;
        slices[i].OutputBuffer = PtrOutput + i * PixelsPerSlice * PixelSpacingOut;
        slices[i].PixelsPerLine = pixels;

        TotalPixels -= pixels;
    }

    // Add left pixels because rounding
    if (slices > 0) slices[nslices - 1].PixelsPerLine += TotalPixels;
}


// If multiline, assign a number of lines to each thread. This works on chunky and planar. Stride parameters 
// are not changed. In the case of one line, stride chunky is not used and stride planar keeps same.
cmsBool _cmsThrSplitWork(const _cmsWorkSlice* master, cmsInt32Number nslices, _cmsWorkSlice slices[])
{
    
    // Check parameters
    if (master->PixelsPerLine == 0 ||
        master->Stride->BytesPerLineIn == 0 ||
        master->Stride->BytesPerLineOut == 0) return FALSE;

    // Do the splitting depending on lines
    if (master->LineCount <= 1) {

        cmsInt32Number PixelsPerWorker = master->PixelsPerLine / nslices;

        if (PixelsPerWorker <= 0) 
            return FALSE;
        else
            SlicePerPixels(master, nslices, PixelsPerWorker, slices);
    }
    else {
        
        cmsInt32Number LinesPerWorker = master->LineCount / nslices;
        
        if (LinesPerWorker <= 0)
            return FALSE;
        else
            SlicePerLines(master, nslices, LinesPerWorker, slices);
    }

    return TRUE;
}