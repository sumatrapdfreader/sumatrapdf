//---------------------------------------------------------------------------------
//
//  Little Color Management System
//  Copyright (c) 1998-2020 Marti Maria Saguer
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
//

#ifndef TESTCMS2_H
#define TESTCMS2_H

#include "lcms2_internal.h"

#ifdef LCMS_FAST_EXTENSIONS
#   include "fast_float_internal.h"
#endif

// On Visual Studio, use debug CRT
#ifdef _MSC_VER
#    include "crtdbg.h"
#endif

#ifdef CMS_IS_WINDOWS_
#    include <io.h>
#endif

#define cmsmin(a, b) (((a) < (b)) ? (a) : (b))

// Used to mark special pointers
void DebugMemDontCheckThis(void *Ptr);


cmsBool IsGoodVal(const char *title, cmsFloat64Number in, cmsFloat64Number out, cmsFloat64Number max);
cmsBool IsGoodFixed15_16(const char *title, cmsFloat64Number in, cmsFloat64Number out);
cmsBool IsGoodFixed8_8(const char *title, cmsFloat64Number in, cmsFloat64Number out);
cmsBool IsGoodWord(const char *title, cmsUInt16Number in, cmsUInt16Number out);
cmsBool IsGoodWordPrec(const char *title, cmsUInt16Number in, cmsUInt16Number out, cmsUInt16Number maxErr);

void* PluginMemHandler(void);
cmsContext WatchDogContext(void* usr);

void ResetFatalError(cmsContext ContextID);
void Die(const char* Reason, ...);
void Dot(void);
void Fail(const char* frm, ...);
void SubTest(const char* frm, ...);
void TestMemoryLeaks(cmsBool ok);
void Say(const char* str);

// Plug-in tests
cmsInt32Number CheckSimpleContext(cmsContext ContextID);
cmsInt32Number CheckAllocContext(cmsContext ContextID);
cmsInt32Number CheckAlarmColorsContext(cmsContext ContextID);
cmsInt32Number CheckAdaptationStateContext(cmsContext ContextID);
cmsInt32Number CheckInterp1DPlugin(cmsContext ContextID);
cmsInt32Number CheckInterp3DPlugin(cmsContext ContextID);
cmsInt32Number CheckParametricCurvePlugin(cmsContext ContextID);
cmsInt32Number CheckFormattersPlugin(cmsContext ContextID);
cmsInt32Number CheckTagTypePlugin(cmsContext ContextID);
cmsInt32Number CheckMPEPlugin(cmsContext ContextID);
cmsInt32Number CheckOptimizationPlugin(cmsContext ContextID);
cmsInt32Number CheckIntentPlugin(cmsContext ContextID);
cmsInt32Number CheckTransformPlugin(cmsContext ContextID);
cmsInt32Number CheckMutexPlugin(cmsContext ContextID);
cmsInt32Number CheckMethodPackDoublesFromFloat(cmsContext ContextID);


// Zoo
void CheckProfileZOO(cmsContext ContextID);

#endif

