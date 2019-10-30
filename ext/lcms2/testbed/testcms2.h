//---------------------------------------------------------------------------------
//
//  Little Color Management System
//  Copyright (c) 1998-2014 Marti Maria Saguer
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

#ifdef _MSC_VER
#    define _CRT_SECURE_NO_WARNINGS 1
#     include "crtdbg.h"
#     include <io.h>
#endif

#include "lcms2_internal.h"

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
cmsInt32Number CheckSimpleContext(void);
cmsInt32Number CheckAllocContext(void);
cmsInt32Number CheckAlarmColorsContext(void);
cmsInt32Number CheckAdaptationStateContext(void);
cmsInt32Number CheckInterp1DPlugin(void);
cmsInt32Number CheckInterp3DPlugin(void);
cmsInt32Number CheckParametricCurvePlugin(void);
cmsInt32Number CheckFormattersPlugin(void);
cmsInt32Number CheckTagTypePlugin(void);
cmsInt32Number CheckMPEPlugin(void);
cmsInt32Number CheckOptimizationPlugin(void);
cmsInt32Number CheckIntentPlugin(void);
cmsInt32Number CheckTransformPlugin(void);
cmsInt32Number CheckMutexPlugin(void);


cmsInt32Number CheckOptimizationPluginLeak(void);

// Zoo
void CheckProfileZOO(cmsContext ContextID);

#endif

