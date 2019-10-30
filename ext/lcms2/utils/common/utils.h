
//---------------------------------------------------------------------------------
//
//  Little Color Management System
//  Copyright (c) 1998-2017 Marti Maria Saguer
//
// Permission is hereby granted, free of charge, to any person obtaining
// a copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the Software
// is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
//---------------------------------------------------------------------------------

#ifndef _lcms_utils_h

// Deal with Microsoft's attempt at deprecating C standard runtime functions
#ifdef _MSC_VER
#    if (_MSC_VER >= 1400)
#      ifndef _CRT_SECURE_NO_DEPRECATE
#        define _CRT_SECURE_NO_DEPRECATE
#      endif
#      ifndef _CRT_SECURE_NO_WARNINGS
#        define _CRT_SECURE_NO_WARNINGS
#      endif
#    endif
#endif

#include "lcms2mt.h"

#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <wchar.h>

// Avoid warnings

#define UTILS_UNUSED_PARAMETER(x) ((void)x)

// Init the utility functions

void InitUtils(cmsContext ContextID, const char* PName);

// Fatal Error (print the message and exit(1))---------------------------------------------

extern int Verbose;

void FatalError(const char *frm, ...);

// xgetopt() interface -------------------------------------------------------------

extern int   xoptind;
extern char *xoptarg;
extern int   xopterr;
extern char  SW;

int xgetopt(int argc, char *argv[], char *optionS);

// The stock profile utility -------------------------------------------------------

cmsHPROFILE OpenStockProfile(cmsContext ContextID, const char* File);

// The print info utility ----------------------------------------------------------

void PrintProfileInformation(cmsContext ContextID, cmsHPROFILE h);

// ---------------------------------------------------------------------------------

void PrintRenderingIntents(cmsContext ContextID);
void PrintBuiltins(void);

// ---------------------------------------------------------------------------------

cmsBool SaveMemoryBlock(const cmsUInt8Number* Buffer, cmsUInt32Number dwLen, const char* Filename);

// ---------------------------------------------------------------------------------

// Return a pixel type on depending on the number of channels
int PixelTypeFromChanCount(int ColorChannels);

// ------------------------------------------------------------------------------

// Return number of channels of pixel type
int ChanCountFromPixelType(int ColorChannels);

#define _lcms_utils_h
#endif
