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

// The Plug-in entry point
static cmsPluginParalellization Plugin = {

  { cmsPluginMagicNumber, REQUIRED_LCMS_VERSION, cmsPluginParalellizationSig, NULL },

  CMS_THREADED_GUESS_MAX_THREADS,
  0,
  _cmsThrScheduler  
};

// This is the main plug-in installer. 
void* CMSEXPORT cmsThreadedExtensions(cmsInt32Number max_threads, cmsUInt32Number flags)
{
    Plugin.MaxWorkers = max_threads;
    Plugin.WorkerFlags = flags;

    return (void*)&Plugin;
}

