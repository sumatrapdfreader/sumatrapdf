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
//

#include "testcms2.h"

// --------------------------------------------------------------------------------------------------
// Auxiliary, duplicate a context and mark the block as non-debug because in this case the allocator
// and deallocator have different context owners
// --------------------------------------------------------------------------------------------------

static
cmsContext DupContext(cmsContext src, void* Data)
{
    cmsContext cpy = cmsDupContext(src, Data);

    DebugMemDontCheckThis(cpy);

    return cpy;
}

// --------------------------------------------------------------------------------------------------
// Simple context functions
// --------------------------------------------------------------------------------------------------

// Allocation order
cmsInt32Number CheckAllocContext(void)
{
     cmsContext c1, c2, c3, c4;

     c1 = cmsCreateContext(NULL, NULL);                 // This creates a context by using the normal malloc
     DebugMemDontCheckThis(c1);
     cmsDeleteContext(c1);

     c2 = cmsCreateContext(PluginMemHandler(), NULL);   // This creates a context by using the debug malloc
     DebugMemDontCheckThis(c2);
     cmsDeleteContext(c2);

     c1 = cmsCreateContext(NULL, NULL);
     DebugMemDontCheckThis(c1);

     c2 = cmsCreateContext(PluginMemHandler(), NULL);
     DebugMemDontCheckThis(c2);

     cmsPlugin(c1, PluginMemHandler()); // Now the context have custom allocators

     c3 = DupContext(c1, NULL);
     c4 = DupContext(c2, NULL);

     cmsDeleteContext(c1);  // Should be deleted by using nomal malloc
     cmsDeleteContext(c2);  // Should be deleted by using debug malloc
     cmsDeleteContext(c3);  // Should be deleted by using nomal malloc
     cmsDeleteContext(c4);  // Should be deleted by using debug malloc

     return 1;
}

// Test the very basic context capabilities
cmsInt32Number CheckSimpleContext(void)
{
    int a = 1;
    int b = 32;
    cmsInt32Number rc = 0;

    cmsContext c1, c2, c3;

    // This function creates a context with a special
    // memory manager that check allocation
    c1 = WatchDogContext(&a);
    cmsDeleteContext(c1);

    c1 = WatchDogContext(&a);

    // Let's check duplication
    c2 = DupContext(c1, NULL);
    c3 = DupContext(c2, NULL);

    // User data should have been propagated
    rc = (*(int*) cmsGetContextUserData(c3)) == 1 ;

    // Free resources
    cmsDeleteContext(c1);
    cmsDeleteContext(c2);
    cmsDeleteContext(c3);

    if (!rc) {
        Fail("Creation of user data failed");
        return 0;
    }

    // Back to create 3 levels of inherance
    c1 = cmsCreateContext(NULL, &a);
    DebugMemDontCheckThis(c1);

    c2 = DupContext(c1, NULL);
    c3 = DupContext(c2, &b);

    rc = (*(int*) cmsGetContextUserData(c3)) == 32 ;

    cmsDeleteContext(c1);
    cmsDeleteContext(c2);
    cmsDeleteContext(c3);

    if (!rc) {
        Fail("Modification of user data failed");
        return 0;
    }

    // All seems ok
    return rc;
}




// --------------------------------------------------------------------------------------------------
//Alarm color functions
// --------------------------------------------------------------------------------------------------

// This function tests the alarm codes across contexts
cmsInt32Number CheckAlarmColorsContext(void)
{
    cmsInt32Number rc = 0;
    const cmsUInt16Number codes[] = {0x0000, 0x1111, 0x2222, 0x3333, 0x4444, 0x5555, 0x6666, 0x7777, 0x8888, 0x9999, 0xaaaa, 0xbbbb, 0xcccc, 0xdddd, 0xeeee, 0xffff};
    cmsUInt16Number out[16];
    cmsContext c1, c2, c3;
    int i;

    c1 = WatchDogContext(NULL);

    cmsSetAlarmCodes(c1, codes);
    c2 = DupContext(c1, NULL);
    c3 = DupContext(c2, NULL);

    cmsGetAlarmCodes(c3, out);

    rc = 1;
    for (i=0; i < 16; i++) {
        if (out[i] != codes[i]) {
            Fail("Bad alarm code %x != %x", out[i], codes[i]);
            rc = 0;
            break;
        }
    }

    cmsDeleteContext(c1);
    cmsDeleteContext(c2);
    cmsDeleteContext(c3);

    return rc;
}


// --------------------------------------------------------------------------------------------------
//Adaptation state functions
// --------------------------------------------------------------------------------------------------

// Similar to the previous, but for adaptation state
cmsInt32Number CheckAdaptationStateContext(void)
{
    cmsInt32Number rc = 0;
    cmsContext c1, c2, c3;
    cmsFloat64Number old1, old2;

    old1 =  cmsSetAdaptationState(NULL, -1);

    c1 = WatchDogContext(NULL);

    cmsSetAdaptationState(c1, 0.7);

    c2 = DupContext(c1, NULL);
    c3 = DupContext(c2, NULL);

    rc = IsGoodVal("Adaptation state", cmsSetAdaptationState(c3, -1), 0.7, 0.001);

    cmsDeleteContext(c1);
    cmsDeleteContext(c2);
    cmsDeleteContext(c3);

    old2 =  cmsSetAdaptationState(NULL, -1);

    if (old1 != old2) {
        Fail("Adaptation state has changed");
        return 0;
    }

    return rc;
}

// --------------------------------------------------------------------------------------------------
// Interpolation plugin check: A fake 1D and 3D interpolation will be used to test the functionality.
// --------------------------------------------------------------------------------------------------

// This fake interpolation takes always the closest lower node in the interpolation table for 1D
static
void Fake1Dfloat(cmsContext ContextID, const cmsFloat32Number Value[],
                    cmsFloat32Number Output[],
                    const cmsInterpParams* p)
{
       cmsFloat32Number val2;
       int cell;
       const cmsFloat32Number* LutTable = (const cmsFloat32Number*) p ->Table;

       // Clip upper values
       if (Value[0] >= 1.0) {
           Output[0] = LutTable[p -> Domain[0]];
           return;
       }

       val2 = p -> Domain[0] * Value[0];
       cell = (int) floor(val2);
       Output[0] =  LutTable[cell] ;
}

// This fake interpolation just uses scrambled negated indexes for output
static
void Fake3D16(cmsContext ContextID, register const cmsUInt16Number Input[],
              register cmsUInt16Number Output[],
              register const struct _cms_interp_struc* p)
{
       Output[0] =  0xFFFF - Input[2];
       Output[1] =  0xFFFF - Input[1];
       Output[2] =  0xFFFF - Input[0];
}

// The factory chooses interpolation routines on depending on certain conditions.
cmsInterpFunction my_Interpolators_Factory(cmsContext ContextID, cmsUInt32Number nInputChannels,
                                           cmsUInt32Number nOutputChannels,
                                           cmsUInt32Number dwFlags)
{
    cmsInterpFunction Interpolation;
    cmsBool  IsFloat = (dwFlags & CMS_LERP_FLAGS_FLOAT);

    // Initialize the return to zero as a non-supported mark
    memset(&Interpolation, 0, sizeof(Interpolation));

    // For 1D to 1D and floating point
    if (nInputChannels == 1 && nOutputChannels == 1 && IsFloat) {

        Interpolation.LerpFloat = Fake1Dfloat;
    }
    else
    if (nInputChannels == 3 && nOutputChannels == 3 && !IsFloat) {

        // For 3D to 3D and 16 bits
        Interpolation.Lerp16 = Fake3D16;
    }

    // Here is the interpolation
    return Interpolation;
}

// Interpolation plug-in
static
cmsPluginInterpolation InterpPluginSample = {

    { cmsPluginMagicNumber, 2060, cmsPluginInterpolationSig, NULL },
    my_Interpolators_Factory
};


// This is the check code for 1D interpolation plug-in
cmsInt32Number CheckInterp1DPlugin(void)
{
    cmsToneCurve* Sampled1D = NULL;
    cmsContext ctx = NULL;
    cmsContext cpy = NULL;
    const cmsFloat32Number tab[] = { 0.0f, 0.10f, 0.20f, 0.30f, 0.40f, 0.50f, 0.60f, 0.70f, 0.80f, 0.90f, 1.00f };  // A straight line

    // 1st level context
    ctx = WatchDogContext(NULL);
    if (ctx == NULL) {
        Fail("Cannot create context");
        goto Error;
    }

    cmsPlugin(ctx, &InterpPluginSample);

    cpy = DupContext(ctx, NULL);
     if (cpy == NULL) {
        Fail("Cannot create context (2)");
        goto Error;
    }

    Sampled1D = cmsBuildTabulatedToneCurveFloat(cpy, 11, tab);
    if (Sampled1D == NULL) {
        Fail("Cannot create tone curve (1)");
        goto Error;
    }

    // Do some interpolations with the plugin
    if (!IsGoodVal("0.10", cmsEvalToneCurveFloat(cpy, Sampled1D, 0.10f), 0.10, 0.01)) goto Error;
    if (!IsGoodVal("0.13", cmsEvalToneCurveFloat(cpy, Sampled1D, 0.13f), 0.10, 0.01)) goto Error;
    if (!IsGoodVal("0.55", cmsEvalToneCurveFloat(cpy, Sampled1D, 0.55f), 0.50, 0.01)) goto Error;
    if (!IsGoodVal("0.9999", cmsEvalToneCurveFloat(cpy, Sampled1D, 0.9999f), 0.90, 0.01)) goto Error;

    cmsFreeToneCurve(cpy, Sampled1D);
    cmsDeleteContext(ctx);
    cmsDeleteContext(cpy);

    // Now in global context
    Sampled1D = cmsBuildTabulatedToneCurveFloat(NULL, 11, tab);
    if (Sampled1D == NULL) {
        Fail("Cannot create tone curve (2)");
        goto Error;
    }

    // Now without the plug-in
    if (!IsGoodVal("0.10", cmsEvalToneCurveFloat(NULL, Sampled1D, 0.10f), 0.10, 0.001)) goto Error;
    if (!IsGoodVal("0.13", cmsEvalToneCurveFloat(NULL, Sampled1D, 0.13f), 0.13, 0.001)) goto Error;
    if (!IsGoodVal("0.55", cmsEvalToneCurveFloat(NULL, Sampled1D, 0.55f), 0.55, 0.001)) goto Error;
    if (!IsGoodVal("0.9999", cmsEvalToneCurveFloat(NULL, Sampled1D, 0.9999f), 0.9999, 0.001)) goto Error;

    cmsFreeToneCurve(NULL, Sampled1D);
    return 1;

Error:
    if (ctx != NULL) cmsDeleteContext(ctx);
     if (cpy != NULL) cmsDeleteContext(ctx);
    if (Sampled1D != NULL) cmsFreeToneCurve(NULL, Sampled1D);
    return 0;

}

// Checks the 3D interpolation
cmsInt32Number CheckInterp3DPlugin(void)
{

    cmsPipeline* p;
    cmsStage* clut;
    cmsContext ctx;
    cmsUInt16Number In[3], Out[3];
    cmsUInt16Number identity[] = {

       0,       0,       0,
       0,       0,       0xffff,
       0,       0xffff,  0,
       0,       0xffff,  0xffff,
       0xffff,  0,       0,
       0xffff,  0,       0xffff,
       0xffff,  0xffff,  0,
       0xffff,  0xffff,  0xffff
    };


    ctx = WatchDogContext(NULL);
    if (ctx == NULL) {
        Fail("Cannot create context");
       return 0;
    }

    cmsPlugin(ctx, &InterpPluginSample);

    p =  cmsPipelineAlloc(ctx, 3, 3);
    clut = cmsStageAllocCLut16bit(ctx, 2, 3, 3, identity);
    cmsPipelineInsertStage(ctx, p, cmsAT_BEGIN, clut);

    // Do some interpolations with the plugin

    In[0] = 0; In[1] = 0; In[2] = 0;
    cmsPipelineEval16(ctx, In, Out, p);

    if (!IsGoodWord("0", Out[0], 0xFFFF - 0)) goto Error;
    if (!IsGoodWord("1", Out[1], 0xFFFF - 0)) goto Error;
    if (!IsGoodWord("2", Out[2], 0xFFFF - 0)) goto Error;

    In[0] = 0x1234; In[1] = 0x5678; In[2] = 0x9ABC;
    cmsPipelineEval16(ctx, In, Out, p);

    if (!IsGoodWord("0", 0xFFFF - 0x9ABC, Out[0])) goto Error;
    if (!IsGoodWord("1", 0xFFFF - 0x5678, Out[1])) goto Error;
    if (!IsGoodWord("2", 0xFFFF - 0x1234, Out[2])) goto Error;

    cmsPipelineFree(ctx, p);
    cmsDeleteContext(ctx);

    // Now without the plug-in

    p =  cmsPipelineAlloc(NULL, 3, 3);
    clut = cmsStageAllocCLut16bit(NULL, 2, 3, 3, identity);
    cmsPipelineInsertStage(NULL, p, cmsAT_BEGIN, clut);

    In[0] = 0; In[1] = 0; In[2] = 0;
    cmsPipelineEval16(NULL, In, Out, p);

    if (!IsGoodWord("0", 0, Out[0])) goto Error;
    if (!IsGoodWord("1", 0, Out[1])) goto Error;
    if (!IsGoodWord("2", 0, Out[2])) goto Error;

    In[0] = 0x1234; In[1] = 0x5678; In[2] = 0x9ABC;
    cmsPipelineEval16(NULL, In, Out, p);

    if (!IsGoodWord("0", 0x1234, Out[0])) goto Error;
    if (!IsGoodWord("1", 0x5678, Out[1])) goto Error;
    if (!IsGoodWord("2", 0x9ABC, Out[2])) goto Error;

    cmsPipelineFree(NULL, p);
    return 1;

Error:
    cmsPipelineFree(NULL, p);
    return 0;
}

// --------------------------------------------------------------------------------------------------
// Parametric curve plugin check: sin(x)/cos(x) function will be used to test the functionality.
// --------------------------------------------------------------------------------------------------

#define TYPE_SIN  1000
#define TYPE_COS  1010
#define TYPE_TAN  1020
#define TYPE_709  709

static cmsFloat64Number my_fns(cmsContext ContextID, cmsInt32Number Type,
                        const cmsFloat64Number Params[],
                        cmsFloat64Number R)
{
    cmsFloat64Number Val;
    switch (Type) {

    case TYPE_SIN:
        Val = Params[0]* sin(R * M_PI);
        break;

    case -TYPE_SIN:
        Val = asin(R) / (M_PI * Params[0]);
        break;

    case TYPE_COS:
        Val = Params[0]* cos(R * M_PI);
        break;

    case -TYPE_COS:
        Val = acos(R) / (M_PI * Params[0]);
        break;

    default: return -1.0;

     }

   return Val;
}

static
cmsFloat64Number my_fns2(cmsContext ContextID, cmsInt32Number Type,
                        const cmsFloat64Number Params[],
                        cmsFloat64Number R)
{
    cmsFloat64Number Val;
    switch (Type) {

    case TYPE_TAN:
        Val = Params[0]* tan(R * M_PI);
        break;

    case -TYPE_TAN:
        Val = atan(R) / (M_PI * Params[0]);
        break;

     default: return -1.0;
     }

   return Val;
}


static double Rec709Math(cmsContext ContextID, int Type, const double Params[], double R)
{
    double Fun = 0;

    switch (Type)
    {
    case 709:

        if (R <= (Params[3]*Params[4])) Fun = R / Params[3];
        else Fun = pow(((R - Params[2])/Params[1]), Params[0]);
        break;

    case -709:

        if (R <= Params[4]) Fun = R * Params[3];
        else Fun = Params[1] * pow(R, (1/Params[0])) + Params[2];
        break;
    }
    return Fun;
}


// Add nonstandard TRC curves -> Rec709

cmsPluginParametricCurves Rec709Plugin = {

    { cmsPluginMagicNumber, 2060, cmsPluginParametricCurveSig, NULL },

    1, {TYPE_709}, {5}, Rec709Math

};


static
cmsPluginParametricCurves CurvePluginSample = {
    { cmsPluginMagicNumber, 2060, cmsPluginParametricCurveSig, NULL },

    2,                       // nFunctions
    { TYPE_SIN, TYPE_COS },  // Function Types
    { 1, 1 },                // ParameterCount
    my_fns                   // Evaluator
};

static
cmsPluginParametricCurves CurvePluginSample2 = {
    { cmsPluginMagicNumber, 2060, cmsPluginParametricCurveSig, NULL },

    1,                       // nFunctions
    { TYPE_TAN},             // Function Types
    { 1 },                   // ParameterCount
    my_fns2                  // Evaluator
};

// --------------------------------------------------------------------------------------------------
// In this test, the DupContext function will be checked as well
// --------------------------------------------------------------------------------------------------
cmsInt32Number CheckParametricCurvePlugin(void)
{
    cmsContext ctx = NULL;
    cmsContext cpy = NULL;
    cmsToneCurve* sinus;
    cmsToneCurve* cosinus;
    cmsToneCurve* tangent;
    cmsToneCurve* reverse_sinus;
    cmsToneCurve* reverse_cosinus;
    cmsFloat64Number scale = 1.0;

    ctx = WatchDogContext(NULL);

    cmsPlugin(ctx, &CurvePluginSample);

    cpy = DupContext(ctx, NULL);

    cmsPlugin(cpy, &CurvePluginSample2);

    sinus = cmsBuildParametricToneCurve(cpy, TYPE_SIN, &scale);
    cosinus = cmsBuildParametricToneCurve(cpy, TYPE_COS, &scale);
    tangent = cmsBuildParametricToneCurve(cpy, TYPE_TAN, &scale);
    reverse_sinus = cmsReverseToneCurve(cpy, sinus);
    reverse_cosinus = cmsReverseToneCurve(cpy, cosinus);


     if (!IsGoodVal("0.10", cmsEvalToneCurveFloat(cpy, sinus, 0.10f), sin(0.10 * M_PI) , 0.001)) goto Error;
     if (!IsGoodVal("0.60", cmsEvalToneCurveFloat(cpy, sinus, 0.60f), sin(0.60* M_PI), 0.001)) goto Error;
     if (!IsGoodVal("0.90", cmsEvalToneCurveFloat(cpy, sinus, 0.90f), sin(0.90* M_PI), 0.001)) goto Error;

     if (!IsGoodVal("0.10", cmsEvalToneCurveFloat(cpy, cosinus, 0.10f), cos(0.10* M_PI), 0.001)) goto Error;
     if (!IsGoodVal("0.60", cmsEvalToneCurveFloat(cpy, cosinus, 0.60f), cos(0.60* M_PI), 0.001)) goto Error;
     if (!IsGoodVal("0.90", cmsEvalToneCurveFloat(cpy, cosinus, 0.90f), cos(0.90* M_PI), 0.001)) goto Error;

     if (!IsGoodVal("0.10", cmsEvalToneCurveFloat(cpy, tangent, 0.10f), tan(0.10* M_PI), 0.001)) goto Error;
     if (!IsGoodVal("0.60", cmsEvalToneCurveFloat(cpy, tangent, 0.60f), tan(0.60* M_PI), 0.001)) goto Error;
     if (!IsGoodVal("0.90", cmsEvalToneCurveFloat(cpy, tangent, 0.90f), tan(0.90* M_PI), 0.001)) goto Error;


     if (!IsGoodVal("0.10", cmsEvalToneCurveFloat(cpy, reverse_sinus, 0.10f), asin(0.10)/M_PI, 0.001)) goto Error;
     if (!IsGoodVal("0.60", cmsEvalToneCurveFloat(cpy, reverse_sinus, 0.60f), asin(0.60)/M_PI, 0.001)) goto Error;
     if (!IsGoodVal("0.90", cmsEvalToneCurveFloat(cpy, reverse_sinus, 0.90f), asin(0.90)/M_PI, 0.001)) goto Error;

     if (!IsGoodVal("0.10", cmsEvalToneCurveFloat(cpy, reverse_cosinus, 0.10f), acos(0.10)/M_PI, 0.001)) goto Error;
     if (!IsGoodVal("0.60", cmsEvalToneCurveFloat(cpy, reverse_cosinus, 0.60f), acos(0.60)/M_PI, 0.001)) goto Error;
     if (!IsGoodVal("0.90", cmsEvalToneCurveFloat(cpy, reverse_cosinus, 0.90f), acos(0.90)/M_PI, 0.001)) goto Error;

     cmsFreeToneCurve(cpy, sinus);
     cmsFreeToneCurve(cpy, cosinus);
     cmsFreeToneCurve(cpy, tangent);
     cmsFreeToneCurve(cpy, reverse_sinus);
     cmsFreeToneCurve(cpy, reverse_cosinus);

     cmsDeleteContext(ctx);
     cmsDeleteContext(cpy);

     return 1;

Error:

     cmsFreeToneCurve(cpy, sinus);
     cmsFreeToneCurve(cpy, reverse_sinus);
     cmsFreeToneCurve(cpy, cosinus);
     cmsFreeToneCurve(cpy, reverse_cosinus);

     if (ctx != NULL) cmsDeleteContext(ctx);
     if (cpy != NULL) cmsDeleteContext(cpy);
     return 0;
}

// --------------------------------------------------------------------------------------------------
// formatters plugin check: 5-6-5 RGB format
// --------------------------------------------------------------------------------------------------

// We define this special type as 0 bytes not float, and set the upper bit

#define TYPE_RGB_565  (COLORSPACE_SH(PT_RGB)|CHANNELS_SH(3)|BYTES_SH(0) | (1 << 23))

cmsUInt8Number* my_Unroll565(cmsContext ContextID, register struct _cmstransform_struct* nfo,
                            register cmsUInt16Number wIn[],
                            register cmsUInt8Number* accum,
                            register cmsUInt32Number Stride)
{
    cmsUInt16Number pixel = *(cmsUInt16Number*) accum;  // Take whole pixel

    double r = floor(((double) (pixel & 31) * 65535.0) / 31.0 + 0.5);
    double g = floor((((pixel >> 5) & 63) * 65535.0) / 63.0 + 0.5);
    double b = floor((((pixel >> 11) & 31) * 65535.0) / 31.0 + 0.5);

    wIn[2] = (cmsUInt16Number) r;
    wIn[1] = (cmsUInt16Number) g;
    wIn[0] = (cmsUInt16Number) b;

    return accum + 2;
}

cmsUInt8Number* my_Pack565(cmsContext ContextID, register _cmsTRANSFORM* info,
                           register cmsUInt16Number wOut[],
                           register cmsUInt8Number* output,
                           register cmsUInt32Number Stride)
{

    register cmsUInt16Number pixel;
    int r, g, b;

    r = (int) floor(( wOut[2] * 31) / 65535.0 + 0.5);
    g = (int) floor(( wOut[1] * 63) / 65535.0 + 0.5);
    b = (int) floor(( wOut[0] * 31) / 65535.0 + 0.5);


    pixel = (r & 31)  | (( g & 63) << 5) | ((b & 31) << 11);


    *(cmsUInt16Number*) output = pixel;
    return output + 2;
}


cmsFormatter my_FormatterFactory(cmsContext ContextID, cmsUInt32Number Type,
                                  cmsFormatterDirection Dir,
                                  cmsUInt32Number dwFlags)
{
    cmsFormatter Result = { NULL };

    if ((Type == TYPE_RGB_565) &&
        !(dwFlags & CMS_PACK_FLAGS_FLOAT) &&
        (Dir == cmsFormatterInput)) {
            Result.Fmt16 = my_Unroll565;
    }
    return Result;
}


cmsFormatter my_FormatterFactory2(cmsContext ContextID, cmsUInt32Number Type,
                                  cmsFormatterDirection Dir,
                                  cmsUInt32Number dwFlags)
{
    cmsFormatter Result = { NULL };

    if ((Type == TYPE_RGB_565) &&
        !(dwFlags & CMS_PACK_FLAGS_FLOAT) &&
        (Dir == cmsFormatterOutput)) {
            Result.Fmt16 = my_Pack565;
    }
    return Result;
}

static
cmsPluginFormatters FormattersPluginSample = { {cmsPluginMagicNumber,
                                2060,
                                cmsPluginFormattersSig,
                                NULL},
                                my_FormatterFactory };



static
cmsPluginFormatters FormattersPluginSample2 = { {cmsPluginMagicNumber,
                                2060,
                                cmsPluginFormattersSig,
                                NULL},
                                my_FormatterFactory2 };


cmsInt32Number CheckFormattersPlugin(void)
{
    cmsContext ctx = WatchDogContext(NULL);
    cmsContext cpy;
    cmsHTRANSFORM xform;
    cmsUInt16Number stream[]= { 0xffffU, 0x1234U, 0x0000U, 0x33ddU };
    cmsUInt16Number result[4];
    int i;

    cmsPlugin(ctx, &FormattersPluginSample);

    cpy = DupContext(ctx, NULL);

    cmsPlugin(cpy, &FormattersPluginSample2);

    xform = cmsCreateTransform(cpy, NULL, TYPE_RGB_565, NULL, TYPE_RGB_565, INTENT_PERCEPTUAL, cmsFLAGS_NULLTRANSFORM);

    cmsDoTransform(cpy, xform, stream, result, 4);

    cmsDeleteTransform(cpy, xform);
    cmsDeleteContext(ctx);
    cmsDeleteContext(cpy);

    for (i=0; i < 4; i++)
        if (stream[i] != result[i]) return 0;

    return 1;
}

// --------------------------------------------------------------------------------------------------
// TagTypePlugin plugin check
// --------------------------------------------------------------------------------------------------

#define SigIntType      ((cmsTagTypeSignature)  0x74747448)   //   'tttH'
#define SigInt          ((cmsTagSignature)  0x74747448)       //   'tttH'

static
void *Type_int_Read(cmsContext ContextID, struct _cms_typehandler_struct* self,
 			    cmsIOHANDLER* io,
               cmsUInt32Number* nItems,
               cmsUInt32Number SizeOfTag)
{
    cmsUInt32Number* Ptr = (cmsUInt32Number*) _cmsMalloc(ContextID, sizeof(cmsUInt32Number));
    if (Ptr == NULL) return NULL;
    if (!_cmsReadUInt32Number(ContextID, io, Ptr)) return NULL;
    *nItems = 1;
    return Ptr;
}

static
cmsBool Type_int_Write(cmsContext ContextID, struct _cms_typehandler_struct* self,
                        cmsIOHANDLER* io,
                        void* Ptr, cmsUInt32Number nItems)
{
    return _cmsWriteUInt32Number(ContextID, io, *(cmsUInt32Number*) Ptr);
}

static
void* Type_int_Dup(cmsContext ContextID, struct _cms_typehandler_struct* self,
                   const void *Ptr, cmsUInt32Number n)
{
    return _cmsDupMem(ContextID, Ptr, n * sizeof(cmsUInt32Number));
}

void Type_int_Free(cmsContext ContextID, struct _cms_typehandler_struct* self,
                   void* Ptr)
{
    _cmsFree(ContextID, Ptr);
}


static cmsPluginTag HiddenTagPluginSample = {

    { cmsPluginMagicNumber, 2060, cmsPluginTagSig, NULL},
    SigInt,  {  1, 1, { SigIntType }, NULL }
};

static cmsPluginTagType TagTypePluginSample = {

     { cmsPluginMagicNumber, 2060, cmsPluginTagTypeSig,  (cmsPluginBase*) &HiddenTagPluginSample},
     { SigIntType, Type_int_Read, Type_int_Write, Type_int_Dup, Type_int_Free, 0 }
};


cmsInt32Number CheckTagTypePlugin(void)
{
    cmsContext ctx = NULL;
    cmsContext cpy = NULL;
    cmsHPROFILE h = NULL;
    cmsUInt32Number myTag = 1234;
    cmsUInt32Number rc = 0;
    char* data = NULL;
    cmsUInt32Number *ptr = NULL;
    cmsUInt32Number clen = 0;

    ctx = WatchDogContext(NULL);
    cmsPlugin(ctx, &TagTypePluginSample);

    cpy = DupContext(ctx, NULL);

    h = cmsCreateProfilePlaceholder(cpy);
    if (h == NULL) {
        Fail("Create placeholder failed");
        goto Error;
    }


    if (!cmsWriteTag(cpy, h, SigInt, &myTag)) {
        Fail("Plug-in failed");
        goto Error;
    }

    rc = cmsSaveProfileToMem(cpy, h, NULL, &clen);
    if (!rc) {
        Fail("Fetch mem size failed");
        goto Error;
    }


    data = (char*) malloc(clen);
    if (data == NULL) {
        Fail("malloc failed ?!?");
        goto Error;
    }


    rc = cmsSaveProfileToMem(cpy, h, data, &clen);
    if (!rc) {
        Fail("Save to mem failed");
        goto Error;
    }

    cmsCloseProfile(cpy, h);

    cmsSetLogErrorHandler(cpy, NULL);
    h = cmsOpenProfileFromMem(cpy, data, clen);
    if (h == NULL) {
        Fail("Open profile failed");
        goto Error;
    }

    ptr = (cmsUInt32Number*) cmsReadTag(cpy, h, SigInt);
    if (ptr != NULL) {

        Fail("read tag/context switching failed");
        goto Error;
    }

    cmsCloseProfile(cpy, h);
    ResetFatalError(cpy);

    h = cmsOpenProfileFromMem(cpy, data, clen);
    if (h == NULL) {
        Fail("Open profile from mem failed");
        goto Error;
    }

    // Get rid of data
    free(data); data = NULL;

    ptr = (cmsUInt32Number*) cmsReadTag(cpy, h, SigInt);
    if (ptr == NULL) {
        Fail("Read tag/conext switching failed (2)");
        return 0;
    }

    rc = (*ptr == 1234);

    cmsCloseProfile(cpy, h);
    cmsDeleteContext(ctx);
    cmsDeleteContext(cpy);

    return rc;

Error:

    if (h != NULL) cmsCloseProfile(cpy, h);
    if (ctx != NULL) cmsDeleteContext(ctx);
    if (cpy != NULL) cmsDeleteContext(cpy);
    if (data) free(data);

    return 0;
}

// --------------------------------------------------------------------------------------------------
// MPE plugin check:
// --------------------------------------------------------------------------------------------------
#define SigNegateType ((cmsStageSignature)0x6E202020)

static
void EvaluateNegate(cmsContext ContextID, const cmsFloat32Number In[],
                     cmsFloat32Number Out[],
                     const cmsStage *mpe)
{
    Out[0] = 1.0f - In[0];
    Out[1] = 1.0f - In[1];
    Out[2] = 1.0f - In[2];
}

static
cmsStage* StageAllocNegate(cmsContext ContextID)
{
    return _cmsStageAllocPlaceholder(ContextID,
                 SigNegateType, 3, 3, EvaluateNegate,
                 NULL, NULL, NULL);
}

static
void *Type_negate_Read(cmsContext ContextID, struct _cms_typehandler_struct* self,
 			    cmsIOHANDLER* io,
                cmsUInt32Number* nItems,
                cmsUInt32Number SizeOfTag)
{
    cmsUInt16Number   Chans;
    if (!_cmsReadUInt16Number(ContextID, io, &Chans)) return NULL;
    if (Chans != 3) return NULL;

    *nItems = 1;
    return StageAllocNegate(ContextID);
}

static
cmsBool Type_negate_Write(cmsContext ContextID, struct _cms_typehandler_struct* self,
                        cmsIOHANDLER* io,
                        void* Ptr, cmsUInt32Number nItems)
{

    if (!_cmsWriteUInt16Number(ContextID, io, 3)) return FALSE;
    return TRUE;
}

static
cmsPluginMultiProcessElement MPEPluginSample = {

    {cmsPluginMagicNumber, 2060, cmsPluginMultiProcessElementSig, NULL},

    { (cmsTagTypeSignature) SigNegateType, Type_negate_Read, Type_negate_Write, NULL, NULL, 0 }
};


cmsInt32Number CheckMPEPlugin(void)
{
    cmsContext ctx = NULL;
    cmsContext cpy = NULL;
    cmsHPROFILE h = NULL;
    cmsUInt32Number myTag = 1234;
    cmsUInt32Number rc = 0;
    char* data = NULL;
    cmsUInt32Number clen = 0;
    cmsFloat32Number In[3], Out[3];
    cmsPipeline* pipe;

    ctx = WatchDogContext(NULL);
    cmsPlugin(ctx, &MPEPluginSample);

    cpy =  DupContext(ctx, NULL);

    h = cmsCreateProfilePlaceholder(cpy);
    if (h == NULL) {
        Fail("Create placeholder failed");
        goto Error;
    }

    pipe = cmsPipelineAlloc(cpy, 3, 3);
    cmsPipelineInsertStage(cpy, pipe, cmsAT_BEGIN, StageAllocNegate(cpy));


    In[0] = 0.3f; In[1] = 0.2f; In[2] = 0.9f;
    cmsPipelineEvalFloat(cpy, In, Out, pipe);

    rc = (IsGoodVal("0", Out[0], 1.0-In[0], 0.001) &&
           IsGoodVal("1", Out[1], 1.0-In[1], 0.001) &&
           IsGoodVal("2", Out[2], 1.0-In[2], 0.001));

    if (!rc) {
        Fail("Pipeline failed");
        goto Error;
    }

    if (!cmsWriteTag(cpy, h, cmsSigDToB3Tag, pipe)) {
        Fail("Plug-in failed");
        goto Error;
    }

    // This cleans the stage as well
    cmsPipelineFree(cpy, pipe);

    rc = cmsSaveProfileToMem(cpy, h, NULL, &clen);
    if (!rc) {
        Fail("Fetch mem size failed");
        goto Error;
    }


    data = (char*) malloc(clen);
    if (data == NULL) {
        Fail("malloc failed ?!?");
        goto Error;
    }


    rc = cmsSaveProfileToMem(cpy, h, data, &clen);
    if (!rc) {
        Fail("Save to mem failed");
        goto Error;
    }

    cmsCloseProfile(cpy, h);

    cmsSetLogErrorHandler(cpy, NULL);
    h = cmsOpenProfileFromMem(cpy, data, clen);
    if (h == NULL) {
        Fail("Open profile failed");
        goto Error;
    }

    pipe = (cmsPipeline*) cmsReadTag(cpy, h, cmsSigDToB3Tag);
    if (pipe != NULL) {

        // Unsupported stage, should fail
        Fail("read tag/context switching failed");
        goto Error;
    }

    cmsCloseProfile(cpy, h);

    ResetFatalError(cpy);

    h = cmsOpenProfileFromMem(cpy, data, clen);
    if (h == NULL) {
        Fail("Open profile from mem failed");
        goto Error;
    }

    // Get rid of data
    free(data); data = NULL;

    pipe = (cmsPipeline*) cmsReadTag(cpy, h, cmsSigDToB3Tag);
    if (pipe == NULL) {
        Fail("Read tag/conext switching failed (2)");
        return 0;
    }

    // Evaluate for negation
    In[0] = 0.3f; In[1] = 0.2f; In[2] = 0.9f;
    cmsPipelineEvalFloat(cpy, In, Out, pipe);

     rc = (IsGoodVal("0", Out[0], 1.0-In[0], 0.001) &&
           IsGoodVal("1", Out[1], 1.0-In[1], 0.001) &&
           IsGoodVal("2", Out[2], 1.0-In[2], 0.001));

    cmsCloseProfile(cpy, h);
    cmsDeleteContext(ctx);
    cmsDeleteContext(cpy);
    return rc;

Error:

    if (h != NULL) cmsCloseProfile(ctx, h);
    if (ctx != NULL) cmsDeleteContext(ctx);
    if (cpy != NULL) cmsDeleteContext(cpy);
    if (data) free(data);

    return 0;
}


// --------------------------------------------------------------------------------------------------
// Optimization plugin check:
// --------------------------------------------------------------------------------------------------

static
void FastEvaluateCurves(cmsContext ContextID, register const cmsUInt16Number In[],
                                     register cmsUInt16Number Out[],
                                     register const void* Data)
{
    Out[0] = In[0];
}

static
cmsBool MyOptimize(cmsContext ContextID, cmsPipeline** Lut,
                   cmsUInt32Number  Intent,
                   cmsUInt32Number* InputFormat,
                   cmsUInt32Number* OutputFormat,
                   cmsUInt32Number* dwFlags)
{
    cmsStage* mpe;
     _cmsStageToneCurvesData* Data;

    //  Only curves in this LUT? All are identities?
    for (mpe = cmsPipelineGetPtrToFirstStage(ContextID, *Lut);
         mpe != NULL;
         mpe = cmsStageNext(ContextID, mpe)) {

            if (cmsStageType(ContextID, mpe) != cmsSigCurveSetElemType) return FALSE;

            // Check for identity
            Data = (_cmsStageToneCurvesData*) cmsStageData(ContextID, mpe);
            if (Data ->nCurves != 1) return FALSE;
            if (cmsEstimateGamma(ContextID, Data->TheCurves[0], 0.1) > 1.0) return FALSE;

    }

    *dwFlags |= cmsFLAGS_NOCACHE;
    _cmsPipelineSetOptimizationParameters(ContextID, *Lut, FastEvaluateCurves, NULL, NULL, NULL);

    return TRUE;
}

cmsPluginOptimization OptimizationPluginSample = {

    {cmsPluginMagicNumber, 2060, cmsPluginOptimizationSig, NULL},
    MyOptimize
};


cmsInt32Number CheckOptimizationPlugin(void)
{
    cmsContext ctx = WatchDogContext(NULL);
    cmsContext cpy;
    cmsHTRANSFORM xform;
    cmsUInt8Number In[]= { 10, 20, 30, 40 };
    cmsUInt8Number Out[4];
    cmsToneCurve* Linear[1];
    cmsHPROFILE h;
    int i;

    cmsPlugin(ctx, &OptimizationPluginSample);

    cpy = DupContext(ctx, NULL);

    Linear[0] = cmsBuildGamma(cpy, 1.0);
    h = cmsCreateLinearizationDeviceLink(cpy, cmsSigGrayData, Linear);
    cmsFreeToneCurve(cpy, Linear[0]);

    xform = cmsCreateTransform(cpy, h, TYPE_GRAY_8, h, TYPE_GRAY_8, INTENT_PERCEPTUAL, 0);
    cmsCloseProfile(cpy, h);

    cmsDoTransform(cpy, xform, In, Out, 4);

    cmsDeleteTransform(cpy, xform);
    cmsDeleteContext(ctx);
    cmsDeleteContext(cpy);

    for (i=0; i < 4; i++)
        if (In[i] != Out[i]) return 0;

    return 1;
}


// --------------------------------------------------------------------------------------------------
// Check the intent plug-in
// --------------------------------------------------------------------------------------------------

/*
   This example creates a new rendering intent, at intent number 300, that is identical to perceptual
   intent for all color spaces but gray to gray transforms, in this case it bypasses the data.
   Note that it has to clear all occurrences of intent 300 in the intents array to avoid
   infinite recursion.
*/

#define INTENT_DECEPTIVE   300

static
cmsPipeline*  MyNewIntent(cmsContext      ContextID,
                          cmsUInt32Number nProfiles,
                          cmsUInt32Number TheIntents[],
                          cmsHPROFILE     hProfiles[],
                          cmsBool         BPC[],
                          cmsFloat64Number AdaptationStates[],
                          cmsUInt32Number dwFlags)
{
    cmsPipeline*    Result;
    cmsUInt32Number ICCIntents[256];
    cmsUInt32Number i;

 for (i=0; i < nProfiles; i++)
        ICCIntents[i] = (TheIntents[i] == INTENT_DECEPTIVE) ? INTENT_PERCEPTUAL :
                                                 TheIntents[i];

 if (cmsGetColorSpace(ContextID, hProfiles[0]) != cmsSigGrayData ||
     cmsGetColorSpace(ContextID, hProfiles[nProfiles-1]) != cmsSigGrayData)
           return _cmsDefaultICCintents(ContextID, nProfiles,
                                   ICCIntents, hProfiles,
                                   BPC, AdaptationStates,
                                   dwFlags);

    Result = cmsPipelineAlloc(ContextID, 1, 1);
    if (Result == NULL) return NULL;

    cmsPipelineInsertStage(ContextID, Result, cmsAT_BEGIN,
                            cmsStageAllocIdentity(ContextID, 1));

    return Result;
}

static cmsPluginRenderingIntent IntentPluginSample = {

    {cmsPluginMagicNumber, 2060, cmsPluginRenderingIntentSig, NULL},

    INTENT_DECEPTIVE, MyNewIntent,  "bypass gray to gray rendering intent"
};

cmsInt32Number CheckIntentPlugin(void)
{
    cmsContext ctx = WatchDogContext(NULL);
    cmsContext cpy;
    cmsHTRANSFORM xform;
    cmsHPROFILE h1, h2;
    cmsToneCurve* Linear1;
    cmsToneCurve* Linear2;
    cmsUInt8Number In[]= { 10, 20, 30, 40 };
    cmsUInt8Number Out[4];
    int i;

    cmsPlugin(ctx, &IntentPluginSample);

    cpy  = DupContext(ctx, NULL);

    Linear1 = cmsBuildGamma(cpy, 3.0);
    Linear2 = cmsBuildGamma(cpy, 0.1);
    h1 = cmsCreateLinearizationDeviceLink(cpy, cmsSigGrayData, &Linear1);
    h2 = cmsCreateLinearizationDeviceLink(cpy, cmsSigGrayData, &Linear2);

    cmsFreeToneCurve(cpy, Linear1);
    cmsFreeToneCurve(cpy, Linear2);

    xform = cmsCreateTransform(cpy, h1, TYPE_GRAY_8, h2, TYPE_GRAY_8, INTENT_DECEPTIVE, 0);
    cmsCloseProfile(cpy,h1); cmsCloseProfile(cpy, h2);

    cmsDoTransform(cpy, xform, In, Out, 4);

    cmsDeleteTransform(cpy, xform);
    cmsDeleteContext(cpy);
    cmsDeleteContext(ctx);

    for (i=0; i < 4; i++)
        if (Out[i] != In[i]) return 0;

    return 1;
}


// --------------------------------------------------------------------------------------------------
// Check the full transform plug-in
// --------------------------------------------------------------------------------------------------

// This is a sample intent that only works for gray8 as output, and always returns '42'
static
void TrancendentalTransform(cmsContext ContextID, struct _cmstransform_struct * CMM,
                              const void* InputBuffer,
                              void* OutputBuffer,
                              cmsUInt32Number Size,
                              cmsUInt32Number Stride)
{
    cmsUInt32Number i;

    for (i=0; i < Size; i++)
    {
        ((cmsUInt8Number*) OutputBuffer)[i] = 0x42;
    }

}


cmsBool  TransformFactory(cmsContext ContextID, _cmsTransformFn* xformPtr,
                          void** UserData,
                           _cmsFreeUserDataFn* FreePrivateDataFn,
                           cmsPipeline** Lut,
                           cmsUInt32Number* InputFormat,
                           cmsUInt32Number* OutputFormat,
                           cmsUInt32Number* dwFlags)

{
    if (*OutputFormat == TYPE_GRAY_8)
    {
        // *Lut holds the pipeline to be applied
        *xformPtr = TrancendentalTransform;
        return TRUE;
    }

    return FALSE;
}


// The Plug-in entry point
static cmsPluginTransform FullTransformPluginSample = {

     { cmsPluginMagicNumber, 2060, cmsPluginTransformSig, NULL},

     TransformFactory
};

cmsInt32Number CheckTransformPlugin(void)
{
    cmsContext ctx = WatchDogContext(NULL);
    cmsContext cpy;
    cmsHTRANSFORM xform;
    cmsUInt8Number In[]= { 10, 20, 30, 40 };
    cmsUInt8Number Out[4];
    cmsToneCurve* Linear;
    cmsHPROFILE h;
    int i;

    cmsPlugin(ctx, &FullTransformPluginSample);

    cpy  = DupContext(ctx, NULL);

    Linear = cmsBuildGamma(cpy, 1.0);
    h = cmsCreateLinearizationDeviceLink(cpy, cmsSigGrayData, &Linear);
    cmsFreeToneCurve(cpy, Linear);

    xform = cmsCreateTransform(cpy, h, TYPE_GRAY_8, h, TYPE_GRAY_8, INTENT_PERCEPTUAL, 0);
    cmsCloseProfile(cpy, h);

    cmsDoTransform(cpy, xform, In, Out, 4);

    cmsDeleteTransform(cpy, xform);
    cmsDeleteContext(ctx);
    cmsDeleteContext(cpy);

    for (i=0; i < 4; i++)
        if (Out[i] != 0x42) return 0;

    return 1;
}


// --------------------------------------------------------------------------------------------------
// Check the mutex plug-in
// --------------------------------------------------------------------------------------------------

typedef struct {
    int nlocks;
} MyMtx;


static
void* MyMtxCreate(cmsContext id)
{
   MyMtx* mtx = (MyMtx*) _cmsMalloc(id, sizeof(MyMtx));
   mtx ->nlocks = 0;
   return mtx;
}

static
void MyMtxDestroy(cmsContext id, void* mtx)
{
    MyMtx* mtx_ = (MyMtx*) mtx;

    if (mtx_->nlocks != 0)
        Die("Locks != 0 when setting free a mutex");

    _cmsFree(id, mtx);

}

static
cmsBool MyMtxLock(cmsContext id, void* mtx)
{
    MyMtx* mtx_ = (MyMtx*) mtx;
    mtx_->nlocks++;

    return TRUE;
}

static
void MyMtxUnlock(cmsContext id, void* mtx)
{
    MyMtx* mtx_ = (MyMtx*) mtx;
    mtx_->nlocks--;

}


static cmsPluginMutex MutexPluginSample = {

     { cmsPluginMagicNumber, 2060, cmsPluginMutexSig, NULL},

     MyMtxCreate,  MyMtxDestroy,  MyMtxLock,  MyMtxUnlock
};


cmsInt32Number CheckMutexPlugin(void)
{
    cmsContext ctx = WatchDogContext(NULL);
    cmsContext cpy;
    cmsHTRANSFORM xform;
    cmsUInt8Number In[]= { 10, 20, 30, 40 };
    cmsUInt8Number Out[4];
    cmsToneCurve* Linear;
    cmsHPROFILE h;
    int i;


    cmsPlugin(ctx, &MutexPluginSample);

    cpy  = DupContext(ctx, NULL);

    Linear = cmsBuildGamma(cpy, 1.0);
    h = cmsCreateLinearizationDeviceLink(cpy, cmsSigGrayData, &Linear);
    cmsFreeToneCurve(cpy, Linear);

    xform = cmsCreateTransform(cpy, h, TYPE_GRAY_8, h, TYPE_GRAY_8, INTENT_PERCEPTUAL, 0);
    cmsCloseProfile(cpy, h);

    cmsDoTransform(cpy, xform, In, Out, 4);

    cmsDeleteTransform(cpy, xform);
    cmsDeleteContext(ctx);
    cmsDeleteContext(cpy);

    for (i=0; i < 4; i++)
        if (Out[i] != In[i]) return 0;

    return 1;
}
