//---------------------------------------------------------------------------------
//
//  Little Color Management System, multithreaded extensions
//  Copyright (c) 1998-2022 Marti Maria Saguer, all rights reserved
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

#ifndef _THREADED_INTERNAL_H
#define _THREADED_INTERNAL_H

#include "lcms2_threaded.h"

// This plugin requires lcms 2.14 or greater
#define REQUIRED_LCMS_VERSION 2140

// Unused parameter warning suppression
#define UNUSED_PARAMETER(x) ((void)x) 

// For testbed
#define CMSCHECKPOINT CMSAPI

// The specification for "inline" is section 6.7.4 of the C99 standard (ISO/IEC 9899:1999).
// unfortunately VisualC++ does not conform that
#if defined(_MSC_VER) || defined(__BORLANDC__)
#   define cmsINLINE __inline
#else
#   define cmsINLINE static inline
#endif

// Holds all parameters for a threadable transform fragment
typedef struct {

	struct _cmstransform_struct* CMMcargo;

	const void* InputBuffer;
	void* OutputBuffer;

	cmsUInt32Number  PixelsPerLine;
	cmsUInt32Number  LineCount;
	const cmsStride* Stride;

} _cmsWorkSlice;

// Count the number of threads needed for this job
cmsUInt32Number _cmsThrCountSlices(struct _cmstransform_struct* CMMcargo, cmsInt32Number MaxWorkers, 
								   cmsUInt32Number PixelsPerLine, cmsUInt32Number LineCount, 
								   cmsStride* Stride);

// Split work following several expert rules
cmsBool		    _cmsThrSplitWork(const _cmsWorkSlice* master, cmsInt32Number nslices, _cmsWorkSlice slices[]);

// Thread primitives
cmsHANDLE       _cmsThrCreateWorker(cmsContext ContextID, _cmsTransform2Fn worker, _cmsWorkSlice* param);
void            _cmsThrJoinWorker(cmsContext ContextID, cmsHANDLE hWorker);
cmsInt32Number  _cmsThrIdealThreadCount(void);

// The scheduler
void  _cmsThrScheduler(struct _cmstransform_struct* CMMcargo,
				       const void* InputBuffer,
				       void* OutputBuffer,
				       cmsUInt32Number PixelsPerLine,
				       cmsUInt32Number LineCount,
				       const cmsStride* Stride);
#endif


