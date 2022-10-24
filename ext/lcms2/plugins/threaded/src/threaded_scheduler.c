//---------------------------------------------------------------------------------
//
//  Little Color Management System, multithreaded extensions
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

// The scheduler is responsible to split the work in several portions in a way that each
// portion can be calculated by a different thread. All loacking is already done by lcms 
// mutexes, and memory should not overlap.
void  _cmsThrScheduler(struct _cmstransform_struct* CMMcargo,
                       const void* InputBuffer,
                       void* OutputBuffer,
                       cmsUInt32Number PixelsPerLine,
                       cmsUInt32Number LineCount,
                       const cmsStride* Stride)
{
    cmsContext ContextID = cmsGetTransformContextID(CMMcargo);
    _cmsTransform2Fn worker = _cmsGetTransformWorker(CMMcargo);
    cmsInt32Number   MaxWorkers = _cmsGetTransformMaxWorkers(CMMcargo);

    // flags are not actually being used
    // cmsUInt32Number  flags = _cmsGetTransformWorkerFlags(CMMcargo);

    _cmsWorkSlice master;
    _cmsWorkSlice* slices;
    cmsStride FixedStride = *Stride;
    cmsHANDLE* handles;

    //  Count the number of threads needed for this job. MaxWorkers is the upper limit or -1 to auto
    cmsUInt32Number nSlices = _cmsThrCountSlices(CMMcargo, MaxWorkers, PixelsPerLine, LineCount, &FixedStride);
    
    // Abort early if no threaded code
    if (nSlices <= 1) {

        worker(CMMcargo, InputBuffer, OutputBuffer, PixelsPerLine, LineCount, Stride);
        return;
    }

    // Setup master thread
    master.CMMcargo = CMMcargo;
    master.InputBuffer = InputBuffer;
    master.OutputBuffer = OutputBuffer;
    master.PixelsPerLine = PixelsPerLine;
    master.LineCount = LineCount;
    master.Stride = &FixedStride;

    // Create memory for the slices
    slices  = (_cmsWorkSlice*)_cmsCalloc(ContextID, nSlices, sizeof(_cmsWorkSlice));
    handles = (cmsHANDLE*) _cmsCalloc(ContextID, nSlices, sizeof(cmsHANDLE));

    if (slices == NULL || handles == NULL)
    {
        if (slices)  _cmsFree(ContextID, slices);
        if (handles) _cmsFree(ContextID, handles);

        // Out of memory in this case only can come from a corruption, but we do the work anyway
        worker(CMMcargo, InputBuffer, OutputBuffer, PixelsPerLine, LineCount, Stride);
        return;
    }

    // All seems ok so far
    if (_cmsThrSplitWork(&master, nSlices, slices))
    {
        // Work is splitted. Create threads
        cmsUInt32Number i;

        for (i = 1; i < nSlices; i++)
        {
            handles[i] = _cmsThrCreateWorker(ContextID, worker, &slices[i]);
        }

        // Do our portion of work 
        worker(CMMcargo, slices[0].InputBuffer, slices[0].OutputBuffer, 
            slices[0].PixelsPerLine, slices[0].LineCount, slices[0].Stride);

        // Wait until all threads are finished
        for (i = 1; i < nSlices; i++)
        {
            _cmsThrJoinWorker(ContextID, handles[i]);
        }
    }
    else
    {
        // Not able to split the work, so don't thread
        worker(CMMcargo, InputBuffer, OutputBuffer, PixelsPerLine, LineCount, Stride);
    }

    _cmsFree(ContextID, slices);
    _cmsFree(ContextID, handles);
}

