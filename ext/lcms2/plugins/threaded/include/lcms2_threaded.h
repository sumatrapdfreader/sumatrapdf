//---------------------------------------------------------------------------------
//
//  Little Color Management System, multithread extensions
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

#ifndef _LCMS2_THREADED_H
#define _LCMS2_THREADED_H

#include "lcms2_plugin.h"

#ifdef CMS_NO_PTHREADS
#error "This plug-in needs pthreads to operate"
#endif

#ifndef CMS_USE_CPP_API
#   ifdef __cplusplus
extern "C" {
#   endif
#endif

#define LCMS2_THREADED_VERSION   1000

// Configuration toggles

// The one and only plug-in entry point. To install this plugin in your code you need to place this in 
// some initialization place. If you want to combine this plug-in with fastfloat, make sure to call 
// the threaded entry point comes last in chain. flags is a reserved field for future use
//
//  cmsPlugin(cmsThreadedExtensions(CMS_THREADED_GUESS_MAX_THREADS, 0));
// 

#define CMS_THREADED_GUESS_MAX_THREADS -1

CMSAPI void* CMSEXPORT cmsThreadedExtensions(cmsInt32Number max_threads, cmsUInt32Number flags);

#ifndef CMS_USE_CPP_API
#   ifdef __cplusplus
    }
#   endif
#endif

#endif

