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

// On Visual Studio, use debug CRT
#ifdef _MSC_VER
#     include "crtdbg.h"
#     include <io.h>
#endif

// A single check. Returns 1 if success, 0 if failed
typedef cmsInt32Number (*TestFn)(void);

// A parametric Tone curve test function
typedef cmsFloat32Number (* dblfnptr)(cmsFloat32Number x, const cmsFloat64Number Params[]);

// Some globals to keep track of error
#define TEXT_ERROR_BUFFER_SIZE  4096

static char ReasonToFailBuffer[TEXT_ERROR_BUFFER_SIZE];
static char SubTestBuffer[TEXT_ERROR_BUFFER_SIZE];
static cmsInt32Number TotalTests = 0, TotalFail = 0;
static cmsBool TrappedError;
static cmsInt32Number SimultaneousErrors;


#define cmsmin(a, b) (((a) < (b)) ? (a) : (b))

// Die, a fatal unexpected error is detected!
void Die(const char* Reason, ...)
{
    va_list args;
    va_start(args, Reason);
    vsprintf(ReasonToFailBuffer, Reason, args);
    va_end(args);
    printf("\n%s\n", ReasonToFailBuffer);
    fflush(stdout);
    exit(1);
}

// Memory management replacement -----------------------------------------------------------------------------


// This is just a simple plug-in for malloc, free and realloc to keep track of memory allocated,
// maximum requested as a single block and maximum allocated at a given time. Results are printed at the end
static cmsUInt32Number SingleHit, MaxAllocated=0, TotalMemory=0;

// I'm hiding the size before the block. This is a well-known technique and probably the blocks coming from
// malloc are built in a way similar to that, but I do on my own to be portable.
typedef struct {
    cmsUInt32Number KeepSize;
    cmsContext      WhoAllocated;
    cmsUInt32Number DontCheck;

    union {
        cmsUInt64Number HiSparc;

        // '_cmsMemoryBlock' block is prepended by the
        // allocator for any requested size. Thus, union holds
        // "widest" type to guarantee proper '_cmsMemoryBlock'
        // alignment for any requested size.

    } alignment;


} _cmsMemoryBlock;

#define SIZE_OF_MEM_HEADER (sizeof(_cmsMemoryBlock))

// This is a fake thread descriptor used to check thread integrity.
// Basically it returns a different threadID each time it is called.
// Then the memory management replacement functions does check if each
// free() is being called with same ContextID used on malloc()
static
cmsContext DbgThread(void)
{
    static cmsUInt32Number n = 1;

    return (cmsContext) (void*)(n++ % 0xff0);
}

// The allocate routine
static
void* DebugMalloc(cmsContext ContextID, cmsUInt32Number size)
{
    _cmsMemoryBlock* blk;

    if (size <= 0) {
       Die("malloc requested with zero bytes");
    }

    TotalMemory += size;

    if (TotalMemory > MaxAllocated)
        MaxAllocated = TotalMemory;

    if (size > SingleHit)
        SingleHit = size;

    blk = (_cmsMemoryBlock*) malloc(size + SIZE_OF_MEM_HEADER);
    if (blk == NULL) return NULL;

    blk ->KeepSize = size;
    blk ->WhoAllocated = ContextID;
    blk ->DontCheck = 0;

    return (void*) ((cmsUInt8Number*) blk + SIZE_OF_MEM_HEADER);
}


// The free routine
static
void  DebugFree(cmsContext ContextID, void *Ptr)
{
    _cmsMemoryBlock* blk;

    if (Ptr == NULL) {
        Die("NULL free (which is a no-op in C, but may be an clue of something going wrong)");
    }

    blk = (_cmsMemoryBlock*) (((cmsUInt8Number*) Ptr) - SIZE_OF_MEM_HEADER);
    TotalMemory -= blk ->KeepSize;

    if (blk ->WhoAllocated != ContextID && !blk->DontCheck) {
        Die("Trying to free memory allocated by a different thread");
    }

    free(blk);
}


// Reallocate, just a malloc, a copy and a free in this case.
static
void * DebugRealloc(cmsContext ContextID, void* Ptr, cmsUInt32Number NewSize)
{
    _cmsMemoryBlock* blk;
    void*  NewPtr;
    cmsUInt32Number max_sz;

    NewPtr = DebugMalloc(ContextID, NewSize);
    if (Ptr == NULL) return NewPtr;

    blk = (_cmsMemoryBlock*) (((cmsUInt8Number*) Ptr) - SIZE_OF_MEM_HEADER);
    max_sz = blk -> KeepSize > NewSize ? NewSize : blk ->KeepSize;
    memmove(NewPtr, Ptr, max_sz);
    DebugFree(ContextID, Ptr);

    return NewPtr;
}

// Let's know the totals
static
void DebugMemPrintTotals(void)
{
    printf("[Memory statistics]\n");
    printf("Allocated = %u MaxAlloc = %u Single block hit = %u\n", TotalMemory, MaxAllocated, SingleHit);
}


void DebugMemDontCheckThis(void *Ptr)
{
     _cmsMemoryBlock* blk = (_cmsMemoryBlock*) (((cmsUInt8Number*) Ptr) - SIZE_OF_MEM_HEADER);

     blk ->DontCheck = 1;
}


// Memory string
static
const char* MemStr(cmsUInt32Number size)
{
    static char Buffer[1024];

    if (size > 1024*1024) {
        sprintf(Buffer, "%g Mb", (cmsFloat64Number) size / (1024.0*1024.0));
    }
    else
        if (size > 1024) {
            sprintf(Buffer, "%g Kb", (cmsFloat64Number) size / 1024.0);
        }
        else
            sprintf(Buffer, "%g bytes", (cmsFloat64Number) size);

    return Buffer;
}


void TestMemoryLeaks(cmsBool ok)
{
    if (TotalMemory > 0)
        printf("Ok, but %s are left!\n", MemStr(TotalMemory));
    else {
        if (ok) printf("Ok.\n");
    }
}

// Here we go with the plug-in declaration
static cmsPluginMemHandler DebugMemHandler = {{ cmsPluginMagicNumber, 2060, cmsPluginMemHandlerSig, NULL },
                                               DebugMalloc, DebugFree, DebugRealloc, NULL, NULL, NULL };

// Returns a pointer to the memhandler plugin
void* PluginMemHandler(void)
{
    return (void*) &DebugMemHandler;
}

cmsContext WatchDogContext(void* usr)
{
    cmsContext ctx;

    ctx = cmsCreateContext(&DebugMemHandler, usr);

    if (ctx == NULL)
        Die("Unable to create memory managed context");

    DebugMemDontCheckThis(ctx);
    return ctx;
}



static
void FatalErrorQuit(cmsContext ContextID, cmsUInt32Number ErrorCode, const char *Text)
{
    Die(Text);

    cmsUNUSED_PARAMETER(ContextID);
    cmsUNUSED_PARAMETER(ErrorCode);
}


void ResetFatalError(cmsContext ContextID)
{
    cmsSetLogErrorHandler(ContextID, FatalErrorQuit);
}


// Print a dot for gauging
void Dot(void)
{
    fprintf(stdout, "."); fflush(stdout);
}

void Say(const char* str)
{
    fprintf(stdout, "%s", str); fflush(stdout);
}


// Keep track of the reason to fail

void Fail(const char* frm, ...)
{
    va_list args;
    va_start(args, frm);
    vsprintf(ReasonToFailBuffer, frm, args);
    va_end(args);
}

// Keep track of subtest

void SubTest(const char* frm, ...)
{
    va_list args;

    Dot();
    va_start(args, frm);
    vsprintf(SubTestBuffer, frm, args);
    va_end(args);
}

// The check framework
static
void Check(const char* Title, TestFn Fn)
{
    printf("Checking %s ...", Title);
    fflush(stdout);

    ReasonToFailBuffer[0] = 0;
    SubTestBuffer[0] = 0;
    TrappedError = FALSE;
    SimultaneousErrors = 0;
    TotalTests++;

    if (Fn() && !TrappedError) {

        // It is a good place to check memory
        TestMemoryLeaks(TRUE);

    }
    else {
        printf("FAIL!\n");

        if (SubTestBuffer[0])
            printf("%s: [%s]\n\t%s\n", Title, SubTestBuffer, ReasonToFailBuffer);
        else
            printf("%s:\n\t%s\n", Title, ReasonToFailBuffer);

        if (SimultaneousErrors > 1)
               printf("\tMore than one (%d) errors were reported\n", SimultaneousErrors);

        TotalFail++;
    }
    fflush(stdout);
}

// Dump a tone curve, for easy diagnostic
void DumpToneCurve(cmsToneCurve* gamma, const char* FileName)
{
    cmsHANDLE hIT8;
    cmsUInt32Number i;

    hIT8 = cmsIT8Alloc(DbgThread());

    cmsIT8SetPropertyDbl(DbgThread(), hIT8, "NUMBER_OF_FIELDS", 2);
    cmsIT8SetPropertyDbl(DbgThread(), hIT8, "NUMBER_OF_SETS", gamma ->nEntries);

    cmsIT8SetDataFormat(DbgThread(), hIT8, 0, "SAMPLE_ID");
    cmsIT8SetDataFormat(DbgThread(), hIT8, 1, "VALUE");

    for (i=0; i < gamma ->nEntries; i++) {
        char Val[30];

        sprintf(Val, "%u", i);
        cmsIT8SetDataRowCol(DbgThread(), hIT8, i, 0, Val);
        sprintf(Val, "0x%x", gamma ->Table16[i]);
        cmsIT8SetDataRowCol(DbgThread(), hIT8, i, 1, Val);
    }

    cmsIT8SaveToFile(DbgThread(), hIT8, FileName);
    cmsIT8Free(DbgThread(), hIT8);
}

// -------------------------------------------------------------------------------------------------


// Used to perform several checks.
// The space used is a clone of a well-known commercial
// color space which I will name "Above RGB"
static
cmsHPROFILE Create_AboveRGB(void)
{
    cmsToneCurve* Curve[3];
    cmsHPROFILE hProfile;
    cmsCIExyY D65;
    cmsCIExyYTRIPLE Primaries = {{0.64, 0.33, 1 },
                                 {0.21, 0.71, 1 },
                                 {0.15, 0.06, 1 }};

    Curve[0] = Curve[1] = Curve[2] = cmsBuildGamma(DbgThread(), 2.19921875);

    cmsWhitePointFromTemp(DbgThread(), &D65, 6504);
    hProfile = cmsCreateRGBProfile(DbgThread(), &D65, &Primaries, Curve);
    cmsFreeToneCurve(DbgThread(), Curve[0]);

    return hProfile;
}

// A gamma-2.2 gray space
static
cmsHPROFILE Create_Gray22(void)
{
    cmsHPROFILE hProfile;
    cmsToneCurve* Curve = cmsBuildGamma(DbgThread(), 2.2);
    if (Curve == NULL) return NULL;

    hProfile = cmsCreateGrayProfile(DbgThread(), cmsD50_xyY(DbgThread()), Curve);
    cmsFreeToneCurve(DbgThread(), Curve);

    return hProfile;
}

// A gamma-3.0 gray space
static
cmsHPROFILE Create_Gray30(void)
{
    cmsHPROFILE hProfile;
    cmsToneCurve* Curve = cmsBuildGamma(DbgThread(), 3.0);
    if (Curve == NULL) return NULL;

    hProfile = cmsCreateGrayProfile(DbgThread(), cmsD50_xyY(DbgThread()), Curve);
    cmsFreeToneCurve(DbgThread(), Curve);

    return hProfile;
}


static
cmsHPROFILE Create_GrayLab(void)
{
    cmsHPROFILE hProfile;
    cmsToneCurve* Curve = cmsBuildGamma(DbgThread(), 1.0);
    if (Curve == NULL) return NULL;

    hProfile = cmsCreateGrayProfile(DbgThread(), cmsD50_xyY(DbgThread()), Curve);
    cmsFreeToneCurve(DbgThread(), Curve);

    cmsSetPCS(DbgThread(), hProfile, cmsSigLabData);
    return hProfile;
}

// A CMYK devicelink that adds gamma 3.0 to each channel
static
cmsHPROFILE Create_CMYK_DeviceLink(void)
{
    cmsHPROFILE hProfile;
    cmsToneCurve* Tab[4];
    cmsToneCurve* Curve = cmsBuildGamma(DbgThread(), 3.0);
    if (Curve == NULL) return NULL;

    Tab[0] = Curve;
    Tab[1] = Curve;
    Tab[2] = Curve;
    Tab[3] = Curve;

    hProfile = cmsCreateLinearizationDeviceLink(DbgThread(), cmsSigCmykData, Tab);
    if (hProfile == NULL) return NULL;

    cmsFreeToneCurve(DbgThread(), Curve);

    return hProfile;
}


// Create a fake CMYK profile, without any other requeriment that being coarse CMYK.
// DON'T USE THIS PROFILE FOR ANYTHING, IT IS USELESS BUT FOR TESTING PURPOSES.
typedef struct {

    cmsHTRANSFORM hLab2sRGB;
    cmsHTRANSFORM sRGB2Lab;
    cmsHTRANSFORM hIlimit;

} FakeCMYKParams;

static
cmsFloat64Number Clip(cmsFloat64Number v)
{
    if (v < 0) return 0;
    if (v > 1) return 1;

    return v;
}

static
cmsInt32Number ForwardSampler(cmsContext ContextID, register const cmsUInt16Number In[], cmsUInt16Number Out[], void* Cargo)
{
    FakeCMYKParams* p = (FakeCMYKParams*) Cargo;
    cmsFloat64Number rgb[3], cmyk[4];
    cmsFloat64Number c, m, y, k;

    cmsDoTransform(DbgThread(), p ->hLab2sRGB, In, rgb, 1);

    c = 1 - rgb[0];
    m = 1 - rgb[1];
    y = 1 - rgb[2];

    k = (c < m ? cmsmin(c, y) : cmsmin(m, y));

    // NONSENSE WARNING!: I'm doing this just because this is a test
    // profile that may have ink limit up to 400%. There is no UCR here
    // so the profile is basically useless for anything but testing.

    cmyk[0] = c;
    cmyk[1] = m;
    cmyk[2] = y;
    cmyk[3] = k;

    cmsDoTransform(DbgThread(), p ->hIlimit, cmyk, Out, 1);

    return 1;
}


static
cmsInt32Number ReverseSampler(cmsContext ContextID, register const cmsUInt16Number In[], register cmsUInt16Number Out[], register void* Cargo)
{
    FakeCMYKParams* p = (FakeCMYKParams*) Cargo;
    cmsFloat64Number c, m, y, k, rgb[3];

    c = In[0] / 65535.0;
    m = In[1] / 65535.0;
    y = In[2] / 65535.0;
    k = In[3] / 65535.0;

    if (k == 0) {

        rgb[0] = Clip(1 - c);
        rgb[1] = Clip(1 - m);
        rgb[2] = Clip(1 - y);
    }
    else
        if (k == 1) {

            rgb[0] = rgb[1] = rgb[2] = 0;
        }
        else {

            rgb[0] = Clip((1 - c) * (1 - k));
            rgb[1] = Clip((1 - m) * (1 - k));
            rgb[2] = Clip((1 - y) * (1 - k));
        }

        cmsDoTransform(DbgThread(), p ->sRGB2Lab, rgb, Out, 1);
        return 1;
}



static
cmsHPROFILE CreateFakeCMYK(cmsFloat64Number InkLimit, cmsBool lUseAboveRGB)
{
    cmsHPROFILE hICC;
    cmsPipeline* AToB0, *BToA0;
    cmsStage* CLUT;
    cmsContext ContextID;
    FakeCMYKParams p;
    cmsHPROFILE hLab, hsRGB, hLimit;
    cmsUInt32Number cmykfrm;

    ContextID = DbgThread();

    if (lUseAboveRGB)
        hsRGB = Create_AboveRGB();
    else
       hsRGB  = cmsCreate_sRGBProfile(ContextID);

    hLab   = cmsCreateLab4Profile(ContextID, NULL);
    hLimit = cmsCreateInkLimitingDeviceLink(ContextID, cmsSigCmykData, InkLimit);

    cmykfrm = FLOAT_SH(1) | BYTES_SH(0)|CHANNELS_SH(4);
    p.hLab2sRGB = cmsCreateTransform(ContextID, hLab,  TYPE_Lab_16,  hsRGB, TYPE_RGB_DBL, INTENT_PERCEPTUAL, cmsFLAGS_NOOPTIMIZE|cmsFLAGS_NOCACHE);
    p.sRGB2Lab  = cmsCreateTransform(ContextID, hsRGB, TYPE_RGB_DBL, hLab,  TYPE_Lab_16,  INTENT_PERCEPTUAL, cmsFLAGS_NOOPTIMIZE|cmsFLAGS_NOCACHE);
    p.hIlimit   = cmsCreateTransform(ContextID, hLimit, cmykfrm, NULL, TYPE_CMYK_16, INTENT_PERCEPTUAL, cmsFLAGS_NOOPTIMIZE|cmsFLAGS_NOCACHE);

    cmsCloseProfile(ContextID, hLab); cmsCloseProfile(ContextID, hsRGB); cmsCloseProfile(ContextID, hLimit);

    hICC = cmsCreateProfilePlaceholder(ContextID);
    if (!hICC) return NULL;

    cmsSetProfileVersion(ContextID, hICC, 4.3);

    cmsSetDeviceClass(ContextID, hICC, cmsSigOutputClass);
    cmsSetColorSpace(ContextID, hICC,  cmsSigCmykData);
    cmsSetPCS(ContextID, hICC,         cmsSigLabData);

    BToA0 = cmsPipelineAlloc(ContextID, 3, 4);
    if (BToA0 == NULL) return 0;
    CLUT = cmsStageAllocCLut16bit(ContextID, 17, 3, 4, NULL);
    if (CLUT == NULL) return 0;
    if (!cmsStageSampleCLut16bit(ContextID, CLUT, ForwardSampler, &p, 0)) return 0;

    cmsPipelineInsertStage(ContextID, BToA0, cmsAT_BEGIN, _cmsStageAllocIdentityCurves(ContextID, 3));
    cmsPipelineInsertStage(ContextID, BToA0, cmsAT_END, CLUT);
    cmsPipelineInsertStage(ContextID, BToA0, cmsAT_END, _cmsStageAllocIdentityCurves(ContextID, 4));

    if (!cmsWriteTag(ContextID, hICC, cmsSigBToA0Tag, (void*) BToA0)) return 0;
    cmsPipelineFree(ContextID, BToA0);

    AToB0 = cmsPipelineAlloc(ContextID, 4, 3);
    if (AToB0 == NULL) return 0;
    CLUT = cmsStageAllocCLut16bit(ContextID, 17, 4, 3, NULL);
    if (CLUT == NULL) return 0;
    if (!cmsStageSampleCLut16bit(ContextID, CLUT, ReverseSampler, &p, 0)) return 0;

    cmsPipelineInsertStage(ContextID, AToB0, cmsAT_BEGIN, _cmsStageAllocIdentityCurves(ContextID, 4));
    cmsPipelineInsertStage(ContextID, AToB0, cmsAT_END, CLUT);
    cmsPipelineInsertStage(ContextID, AToB0, cmsAT_END, _cmsStageAllocIdentityCurves(ContextID, 3));

    if (!cmsWriteTag(ContextID, hICC, cmsSigAToB0Tag, (void*) AToB0)) return 0;
    cmsPipelineFree(ContextID, AToB0);

    cmsDeleteTransform(ContextID, p.hLab2sRGB);
    cmsDeleteTransform(ContextID, p.sRGB2Lab);
    cmsDeleteTransform(ContextID, p.hIlimit);

    cmsLinkTag(ContextID, hICC, cmsSigAToB1Tag, cmsSigAToB0Tag);
    cmsLinkTag(ContextID, hICC, cmsSigAToB2Tag, cmsSigAToB0Tag);
    cmsLinkTag(ContextID, hICC, cmsSigBToA1Tag, cmsSigBToA0Tag);
    cmsLinkTag(ContextID, hICC, cmsSigBToA2Tag, cmsSigBToA0Tag);

    return hICC;
}


// Does create several profiles for latter use------------------------------------------------------------------------------------------------

static
cmsInt32Number OneVirtual(cmsHPROFILE h, const char* SubTestTxt, const char* FileName)
{
    SubTest(SubTestTxt);
    if (h == NULL) return 0;

    if (!cmsSaveProfileToFile(DbgThread(), h, FileName)) return 0;
    cmsCloseProfile(DbgThread(), h);

    h = cmsOpenProfileFromFile(DbgThread(), FileName, "r");
    if (h == NULL) return 0;

    cmsCloseProfile(DbgThread(), h);
    return 1;
}



// This test checks the ability of lcms2 to save its built-ins as valid profiles.
// It does not check the functionality of such profiles
static
cmsInt32Number CreateTestProfiles(void)
{
    cmsHPROFILE h;

    h = cmsCreate_sRGBProfile(DbgThread());
    if (!OneVirtual(h, "sRGB profile", "sRGBlcms2.icc")) return 0;

    // ----

    h = Create_AboveRGB();
    if (!OneVirtual(h, "aRGB profile", "aRGBlcms2.icc")) return 0;

    // ----

    h = Create_Gray22();
    if (!OneVirtual(h, "Gray profile", "graylcms2.icc")) return 0;

    // ----

    h = Create_Gray30();
    if (!OneVirtual(h, "Gray 3.0 profile", "gray3lcms2.icc")) return 0;

    // ----

    h = Create_GrayLab();
    if (!OneVirtual(h, "Gray Lab profile", "glablcms2.icc")) return 0;

    // ----

    h = Create_CMYK_DeviceLink();
    if (!OneVirtual(h, "Linearization profile", "linlcms2.icc")) return 0;

    // -------
    h = cmsCreateInkLimitingDeviceLink(DbgThread(), cmsSigCmykData, 150);
    if (h == NULL) return 0;
    if (!OneVirtual(h, "Ink-limiting profile", "limitlcms2.icc")) return 0;

    // ------

    h = cmsCreateLab2Profile(DbgThread(), NULL);
    if (!OneVirtual(h, "Lab 2 identity profile", "labv2lcms2.icc")) return 0;

    // ----

    h = cmsCreateLab4Profile(DbgThread(), NULL);
    if (!OneVirtual(h, "Lab 4 identity profile", "labv4lcms2.icc")) return 0;

    // ----

    h = cmsCreateXYZProfile(DbgThread());
    if (!OneVirtual(h, "XYZ identity profile", "xyzlcms2.icc")) return 0;

    // ----

    h = cmsCreateNULLProfile(DbgThread());
    if (!OneVirtual(h, "NULL profile", "nullcms2.icc")) return 0;

    // ---

    h = cmsCreateBCHSWabstractProfile(DbgThread(), 17, 0, 0, 0, 0, 5000, 6000);
    if (!OneVirtual(h, "BCHS profile", "bchslcms2.icc")) return 0;

    // ---

    h = CreateFakeCMYK(300, FALSE);
    if (!OneVirtual(h, "Fake CMYK profile", "lcms2cmyk.icc")) return 0;

    // ---

    h = cmsCreateBCHSWabstractProfile(DbgThread(), 17, 0, 1.2, 0, 3, 5000, 5000);
    if (!OneVirtual(h, "Brightness", "brightness.icc")) return 0;
    return 1;
}

static
void RemoveTestProfiles(void)
{
    remove("sRGBlcms2.icc");
    remove("aRGBlcms2.icc");
    remove("graylcms2.icc");
    remove("gray3lcms2.icc");
    remove("linlcms2.icc");
    remove("limitlcms2.icc");
    remove("labv2lcms2.icc");
    remove("labv4lcms2.icc");
    remove("xyzlcms2.icc");
    remove("nullcms2.icc");
    remove("bchslcms2.icc");
    remove("lcms2cmyk.icc");
    remove("glablcms2.icc");
    remove("lcms2link.icc");
    remove("lcms2link2.icc");
    remove("brightness.icc");
}

// -------------------------------------------------------------------------------------------------

// Check the size of basic types. If this test fails, nothing is going to work anyway
static
cmsInt32Number CheckBaseTypes(void)
{
    // Ignore warnings about conditional expression
#ifdef _MSC_VER
#pragma warning(disable: 4127)
#endif

    if (sizeof(cmsUInt8Number) != 1) return 0;
    if (sizeof(cmsInt8Number) != 1) return 0;
    if (sizeof(cmsUInt16Number) != 2) return 0;
    if (sizeof(cmsInt16Number) != 2) return 0;
    if (sizeof(cmsUInt32Number) != 4) return 0;
    if (sizeof(cmsInt32Number) != 4) return 0;
    if (sizeof(cmsUInt64Number) != 8) return 0;
    if (sizeof(cmsInt64Number) != 8) return 0;
    if (sizeof(cmsFloat32Number) != 4) return 0;
    if (sizeof(cmsFloat64Number) != 8) return 0;
    if (sizeof(cmsSignature) != 4) return 0;
    if (sizeof(cmsU8Fixed8Number) != 2) return 0;
    if (sizeof(cmsS15Fixed16Number) != 4) return 0;
    if (sizeof(cmsU16Fixed16Number) != 4) return 0;

    return 1;
}

// -------------------------------------------------------------------------------------------------


// Are we little or big endian?  From Harbison&Steele.
static
cmsInt32Number CheckEndianness(void)
{
    cmsInt32Number BigEndian, IsOk;
    union {
        long l;
        char c[sizeof (long)];
    } u;

    u.l = 1;
    BigEndian = (u.c[sizeof (long) - 1] == 1);

#ifdef CMS_USE_BIG_ENDIAN
    IsOk = BigEndian;
#else
    IsOk = !BigEndian;
#endif

    if (!IsOk) {
        Die("\nOOOPPSS! You have CMS_USE_BIG_ENDIAN toggle misconfigured!\n\n"
            "Please, edit lcms2mt.h and %s the CMS_USE_BIG_ENDIAN toggle.\n", BigEndian? "uncomment" : "comment");
        return 0;
    }

    return 1;
}

// Check quick floor
static
cmsInt32Number CheckQuickFloor(void)
{
    if ((_cmsQuickFloor(1.234) != 1) ||
        (_cmsQuickFloor(32767.234) != 32767) ||
        (_cmsQuickFloor(-1.234) != -2) ||
        (_cmsQuickFloor(-32767.1) != -32768)) {

            Die("\nOOOPPSS! _cmsQuickFloor() does not work as expected in your machine!\n\n"
                "Please, edit lcms2mt.h and uncomment the CMS_DONT_USE_FAST_FLOOR toggle.\n");
            return 0;

    }

    return 1;
}

// Quick floor restricted to word
static
cmsInt32Number CheckQuickFloorWord(void)
{
    cmsUInt32Number i;

    for (i=0; i < 65535; i++) {

        if (_cmsQuickFloorWord((cmsFloat64Number) i + 0.1234) != i) {

            Die("\nOOOPPSS! _cmsQuickFloorWord() does not work as expected in your machine!\n\n"
                "Please, edit lcms2mt.h and uncomment the CMS_DONT_USE_FAST_FLOOR toggle.\n");
            return 0;
        }
    }

    return 1;
}

// -------------------------------------------------------------------------------------------------

// Precision stuff.

// On 15.16 fixed point, this is the maximum we can obtain. Remember ICC profiles have storage limits on this number
#define FIXED_PRECISION_15_16 (1.0 / 65535.0)

// On 8.8 fixed point, that is the max we can obtain.
#define FIXED_PRECISION_8_8 (1.0 / 255.0)

// On cmsFloat32Number type, this is the precision we expect
#define FLOAT_PRECISSION      (0.00001)

static cmsFloat64Number MaxErr;
static cmsFloat64Number AllowedErr = FIXED_PRECISION_15_16;

cmsBool IsGoodVal(const char *title, cmsFloat64Number in, cmsFloat64Number out, cmsFloat64Number max)
{
    cmsFloat64Number Err = fabs(in - out);

    if (Err > MaxErr) MaxErr = Err;

        if ((Err > max )) {

              Fail("(%s): Must be %f, But is %f ", title, in, out);
              return FALSE;
              }

       return TRUE;
}


cmsBool  IsGoodFixed15_16(const char *title, cmsFloat64Number in, cmsFloat64Number out)
{
    return IsGoodVal(title, in, out, FIXED_PRECISION_15_16);
}


cmsBool  IsGoodFixed8_8(const char *title, cmsFloat64Number in, cmsFloat64Number out)
{
    return IsGoodVal(title, in, out, FIXED_PRECISION_8_8);
}

cmsBool  IsGoodWord(const char *title, cmsUInt16Number in, cmsUInt16Number out)
{
    if ((abs(in - out) > 0 )) {

        Fail("(%s): Must be %x, But is %x ", title, in, out);
        return FALSE;
    }

    return TRUE;
}

cmsBool  IsGoodWordPrec(const char *title, cmsUInt16Number in, cmsUInt16Number out, cmsUInt16Number maxErr)
{
    if ((abs(in - out) > maxErr )) {

        Fail("(%s): Must be %x, But is %x ", title, in, out);
        return FALSE;
    }

    return TRUE;
}

// Fixed point ----------------------------------------------------------------------------------------------

static
cmsInt32Number TestSingleFixed15_16(cmsFloat64Number d)
{
    cmsS15Fixed16Number f = _cmsDoubleTo15Fixed16(DbgThread(), d);
    cmsFloat64Number RoundTrip = _cms15Fixed16toDouble(DbgThread(), f);
    cmsFloat64Number Error     = fabs(d - RoundTrip);

    return ( Error <= FIXED_PRECISION_15_16);
}

static
cmsInt32Number CheckFixedPoint15_16(void)
{
    if (!TestSingleFixed15_16(1.0)) return 0;
    if (!TestSingleFixed15_16(2.0)) return 0;
    if (!TestSingleFixed15_16(1.23456)) return 0;
    if (!TestSingleFixed15_16(0.99999)) return 0;
    if (!TestSingleFixed15_16(0.1234567890123456789099999)) return 0;
    if (!TestSingleFixed15_16(-1.0)) return 0;
    if (!TestSingleFixed15_16(-2.0)) return 0;
    if (!TestSingleFixed15_16(-1.23456)) return 0;
    if (!TestSingleFixed15_16(-1.1234567890123456789099999)) return 0;
    if (!TestSingleFixed15_16(+32767.1234567890123456789099999)) return 0;
    if (!TestSingleFixed15_16(-32767.1234567890123456789099999)) return 0;
    return 1;
}

static
cmsInt32Number TestSingleFixed8_8(cmsFloat64Number d)
{
    cmsS15Fixed16Number f = _cmsDoubleTo8Fixed8(DbgThread(), d);
    cmsFloat64Number RoundTrip = _cms8Fixed8toDouble(DbgThread(), (cmsUInt16Number) f);
    cmsFloat64Number Error     = fabs(d - RoundTrip);

    return ( Error <= FIXED_PRECISION_8_8);
}

static
cmsInt32Number CheckFixedPoint8_8(void)
{
    if (!TestSingleFixed8_8(1.0)) return 0;
    if (!TestSingleFixed8_8(2.0)) return 0;
    if (!TestSingleFixed8_8(1.23456)) return 0;
    if (!TestSingleFixed8_8(0.99999)) return 0;
    if (!TestSingleFixed8_8(0.1234567890123456789099999)) return 0;
    if (!TestSingleFixed8_8(+255.1234567890123456789099999)) return 0;

    return 1;
}

// D50 constant --------------------------------------------------------------------------------------------

static
cmsInt32Number CheckD50Roundtrip(void)
{
    cmsFloat64Number cmsD50X_2 =  0.96420288;
    cmsFloat64Number cmsD50Y_2 =  1.0;
    cmsFloat64Number cmsD50Z_2 = 0.82490540;

    cmsS15Fixed16Number xe = _cmsDoubleTo15Fixed16(DbgThread(), cmsD50X);
    cmsS15Fixed16Number ye = _cmsDoubleTo15Fixed16(DbgThread(), cmsD50Y);
    cmsS15Fixed16Number ze = _cmsDoubleTo15Fixed16(DbgThread(), cmsD50Z);

    cmsFloat64Number x =  _cms15Fixed16toDouble(DbgThread(), xe);
    cmsFloat64Number y =  _cms15Fixed16toDouble(DbgThread(), ye);
    cmsFloat64Number z =  _cms15Fixed16toDouble(DbgThread(), ze);

    double dx = fabs(cmsD50X - x);
    double dy = fabs(cmsD50Y - y);
    double dz = fabs(cmsD50Z - z);

    double euc = sqrt(dx*dx + dy*dy + dz* dz);

    if (euc > 1E-5) {

        Fail("D50 roundtrip |err| > (%f) ", euc);
        return 0;
    }

    xe = _cmsDoubleTo15Fixed16(DbgThread(), cmsD50X_2);
    ye = _cmsDoubleTo15Fixed16(DbgThread(), cmsD50Y_2);
    ze = _cmsDoubleTo15Fixed16(DbgThread(), cmsD50Z_2);

    x =  _cms15Fixed16toDouble(DbgThread(), xe);
    y =  _cms15Fixed16toDouble(DbgThread(), ye);
    z =  _cms15Fixed16toDouble(DbgThread(), ze);

    dx = fabs(cmsD50X_2 - x);
    dy = fabs(cmsD50Y_2 - y);
    dz = fabs(cmsD50Z_2 - z);

    euc = sqrt(dx*dx + dy*dy + dz* dz);

    if (euc > 1E-5) {

        Fail("D50 roundtrip |err| > (%f) ", euc);
        return 0;
    }


    return 1;
}

// Linear interpolation -----------------------------------------------------------------------------------------------

// Since prime factors of 65535 (FFFF) are,
//
//            0xFFFF = 3 * 5 * 17 * 257
//
// I test tables of 2, 4, 6, and 18 points, that will be exact.

static
void BuildTable(cmsInt32Number n, cmsUInt16Number Tab[], cmsBool  Descending)
{
    cmsInt32Number i;

    for (i=0; i < n; i++) {
        cmsFloat64Number v = (cmsFloat64Number) ((cmsFloat64Number) 65535.0 * i ) / (n-1);

        Tab[Descending ? (n - i - 1) : i ] = (cmsUInt16Number) floor(v + 0.5);
    }
}

// A single function that does check 1D interpolation
// nNodesToCheck = number on nodes to check
// Down = Create decreasing tables
// Reverse = Check reverse interpolation
// max_err = max allowed error

static
cmsInt32Number Check1D(cmsInt32Number nNodesToCheck, cmsBool  Down, cmsInt32Number max_err)
{
    cmsUInt32Number i;
    cmsUInt16Number in, out;
    cmsInterpParams* p;
    cmsUInt16Number* Tab;

    Tab = (cmsUInt16Number*) malloc(sizeof(cmsUInt16Number)* nNodesToCheck);
    if (Tab == NULL) return 0;

    p = _cmsComputeInterpParams(DbgThread(), nNodesToCheck, 1, 1, Tab, CMS_LERP_FLAGS_16BITS);
    if (p == NULL) return 0;

    BuildTable(nNodesToCheck, Tab, Down);

    for (i=0; i <= 0xffff; i++) {

        in = (cmsUInt16Number) i;
        out = 0;

        p ->Interpolation.Lerp16(DbgThread(), &in, &out, p);

        if (Down) out = 0xffff - out;

        if (abs(out - in) > max_err) {

            Fail("(%dp): Must be %x, But is %x : ", nNodesToCheck, in, out);
            _cmsFreeInterpParams(DbgThread(), p);
            free(Tab);
            return 0;
        }
    }

    _cmsFreeInterpParams(DbgThread(), p);
    free(Tab);
    return 1;
}


static
cmsInt32Number Check1DLERP2(void)
{
    return Check1D(2, FALSE, 0);
}


static
cmsInt32Number Check1DLERP3(void)
{
    return Check1D(3, FALSE, 1);
}


static
cmsInt32Number Check1DLERP4(void)
{
    return Check1D(4, FALSE, 0);
}

static
cmsInt32Number Check1DLERP6(void)
{
    return Check1D(6, FALSE, 0);
}

static
cmsInt32Number Check1DLERP18(void)
{
    return Check1D(18, FALSE, 0);
}


static
cmsInt32Number Check1DLERP2Down(void)
{
    return Check1D(2, TRUE, 0);
}


static
cmsInt32Number Check1DLERP3Down(void)
{
    return Check1D(3, TRUE, 1);
}

static
cmsInt32Number Check1DLERP6Down(void)
{
    return Check1D(6, TRUE, 0);
}

static
cmsInt32Number Check1DLERP18Down(void)
{
    return Check1D(18, TRUE, 0);
}

static
cmsInt32Number ExhaustiveCheck1DLERP(void)
{
    cmsUInt32Number j;

    printf("\n");
    for (j=10; j <= 4096; j++) {

        if ((j % 10) == 0) printf("%u    \r", j);

        if (!Check1D(j, FALSE, 1)) return 0;
    }

    printf("\rResult is ");
    return 1;
}

static
cmsInt32Number ExhaustiveCheck1DLERPDown(void)
{
    cmsUInt32Number j;

    printf("\n");
    for (j=10; j <= 4096; j++) {

        if ((j % 10) == 0) printf("%u    \r", j);

        if (!Check1D(j, TRUE, 1)) return 0;
    }


    printf("\rResult is ");
    return 1;
}



// 3D interpolation -------------------------------------------------------------------------------------------------

static
cmsInt32Number Check3DinterpolationFloatTetrahedral(void)
{
    cmsInterpParams* p;
    cmsInt32Number i;
    cmsFloat32Number In[3], Out[3];
    cmsFloat32Number FloatTable[] = { //R     G    B

        0,    0,   0,     // B=0,G=0,R=0
        0,    0,  .25,    // B=1,G=0,R=0

        0,   .5,    0,    // B=0,G=1,R=0
        0,   .5,  .25,    // B=1,G=1,R=0

        1,    0,    0,    // B=0,G=0,R=1
        1,    0,  .25,    // B=1,G=0,R=1

        1,    .5,   0,    // B=0,G=1,R=1
        1,    .5,  .25    // B=1,G=1,R=1

    };

    p = _cmsComputeInterpParams(DbgThread(), 2, 3, 3, FloatTable, CMS_LERP_FLAGS_FLOAT);


    MaxErr = 0.0;
     for (i=0; i < 0xffff; i++) {

       In[0] = In[1] = In[2] = (cmsFloat32Number) ( (cmsFloat32Number) i / 65535.0F);

        p ->Interpolation.LerpFloat(DbgThread(), In, Out, p);

       if (!IsGoodFixed15_16("Channel 1", Out[0], In[0])) goto Error;
       if (!IsGoodFixed15_16("Channel 2", Out[1], (cmsFloat32Number) In[1] / 2.F)) goto Error;
       if (!IsGoodFixed15_16("Channel 3", Out[2], (cmsFloat32Number) In[2] / 4.F)) goto Error;
     }

    if (MaxErr > 0) printf("|Err|<%lf ", MaxErr);
    _cmsFreeInterpParams(DbgThread(), p);
    return 1;

Error:
    _cmsFreeInterpParams(DbgThread(), p);
    return 0;
}

static
cmsInt32Number Check3DinterpolationFloatTrilinear(void)
{
    cmsInterpParams* p;
    cmsInt32Number i;
    cmsFloat32Number In[3], Out[3];
    cmsFloat32Number FloatTable[] = { //R     G    B

        0,    0,   0,     // B=0,G=0,R=0
        0,    0,  .25,    // B=1,G=0,R=0

        0,   .5,    0,    // B=0,G=1,R=0
        0,   .5,  .25,    // B=1,G=1,R=0

        1,    0,    0,    // B=0,G=0,R=1
        1,    0,  .25,    // B=1,G=0,R=1

        1,    .5,   0,    // B=0,G=1,R=1
        1,    .5,  .25    // B=1,G=1,R=1

    };

    p = _cmsComputeInterpParams(DbgThread(), 2, 3, 3, FloatTable, CMS_LERP_FLAGS_FLOAT|CMS_LERP_FLAGS_TRILINEAR);

    MaxErr = 0.0;
     for (i=0; i < 0xffff; i++) {

       In[0] = In[1] = In[2] = (cmsFloat32Number) ( (cmsFloat32Number) i / 65535.0F);

        p ->Interpolation.LerpFloat(DbgThread(), In, Out, p);

       if (!IsGoodFixed15_16("Channel 1", Out[0], In[0])) goto Error;
       if (!IsGoodFixed15_16("Channel 2", Out[1], (cmsFloat32Number) In[1] / 2.F)) goto Error;
       if (!IsGoodFixed15_16("Channel 3", Out[2], (cmsFloat32Number) In[2] / 4.F)) goto Error;
     }

    if (MaxErr > 0) printf("|Err|<%lf ", MaxErr);
    _cmsFreeInterpParams(DbgThread(), p);
    return 1;

Error:
    _cmsFreeInterpParams(DbgThread(), p);
    return 0;

}

static
cmsInt32Number Check3DinterpolationTetrahedral16(void)
{
    cmsInterpParams* p;
    cmsInt32Number i;
    cmsUInt16Number In[3], Out[3];
    cmsUInt16Number Table[] = {

        0,    0,   0,
        0,    0,   0xffff,

        0,    0xffff,    0,
        0,    0xffff,    0xffff,

        0xffff,    0,    0,
        0xffff,    0,    0xffff,

        0xffff,    0xffff,   0,
        0xffff,    0xffff,   0xffff
    };

    p = _cmsComputeInterpParams(DbgThread(), 2, 3, 3, Table, CMS_LERP_FLAGS_16BITS);

    MaxErr = 0.0;
     for (i=0; i < 0xffff; i++) {

       In[0] = In[1] = In[2] = (cmsUInt16Number) i;

        p ->Interpolation.Lerp16(DbgThread(), In, Out, p);

       if (!IsGoodWord("Channel 1", Out[0], In[0])) goto Error;
       if (!IsGoodWord("Channel 2", Out[1], In[1])) goto Error;
       if (!IsGoodWord("Channel 3", Out[2], In[2])) goto Error;
     }

    if (MaxErr > 0) printf("|Err|<%lf ", MaxErr);
    _cmsFreeInterpParams(DbgThread(), p);
    return 1;

Error:
    _cmsFreeInterpParams(DbgThread(), p);
    return 0;
}

static
cmsInt32Number Check3DinterpolationTrilinear16(void)
{
    cmsInterpParams* p;
    cmsInt32Number i;
    cmsUInt16Number In[3], Out[3];
    cmsUInt16Number Table[] = {

        0,    0,   0,
        0,    0,   0xffff,

        0,    0xffff,    0,
        0,    0xffff,    0xffff,

        0xffff,    0,    0,
        0xffff,    0,    0xffff,

        0xffff,    0xffff,   0,
        0xffff,    0xffff,   0xffff
    };

    p = _cmsComputeInterpParams(DbgThread(), 2, 3, 3, Table, CMS_LERP_FLAGS_TRILINEAR);

    MaxErr = 0.0;
     for (i=0; i < 0xffff; i++) {

       In[0] = In[1] = In[2] = (cmsUInt16Number) i;

        p ->Interpolation.Lerp16(DbgThread(), In, Out, p);

       if (!IsGoodWord("Channel 1", Out[0], In[0])) goto Error;
       if (!IsGoodWord("Channel 2", Out[1], In[1])) goto Error;
       if (!IsGoodWord("Channel 3", Out[2], In[2])) goto Error;
     }

    if (MaxErr > 0) printf("|Err|<%lf ", MaxErr);
    _cmsFreeInterpParams(DbgThread(), p);
    return 1;

Error:
    _cmsFreeInterpParams(DbgThread(), p);
    return 0;
}


static
cmsInt32Number ExaustiveCheck3DinterpolationFloatTetrahedral(void)
{
    cmsInterpParams* p;
    cmsInt32Number r, g, b;
    cmsFloat32Number In[3], Out[3];
    cmsFloat32Number FloatTable[] = { //R     G    B

        0,    0,   0,     // B=0,G=0,R=0
        0,    0,  .25,    // B=1,G=0,R=0

        0,   .5,    0,    // B=0,G=1,R=0
        0,   .5,  .25,    // B=1,G=1,R=0

        1,    0,    0,    // B=0,G=0,R=1
        1,    0,  .25,    // B=1,G=0,R=1

        1,    .5,   0,    // B=0,G=1,R=1
        1,    .5,  .25    // B=1,G=1,R=1

    };

    p = _cmsComputeInterpParams(DbgThread(), 2, 3, 3, FloatTable, CMS_LERP_FLAGS_FLOAT);

    MaxErr = 0.0;
    for (r=0; r < 0xff; r++)
        for (g=0; g < 0xff; g++)
            for (b=0; b < 0xff; b++)
        {

            In[0] = (cmsFloat32Number) r / 255.0F;
            In[1] = (cmsFloat32Number) g / 255.0F;
            In[2] = (cmsFloat32Number) b / 255.0F;


        p ->Interpolation.LerpFloat(DbgThread(), In, Out, p);

       if (!IsGoodFixed15_16("Channel 1", Out[0], In[0])) goto Error;
       if (!IsGoodFixed15_16("Channel 2", Out[1], (cmsFloat32Number) In[1] / 2.F)) goto Error;
       if (!IsGoodFixed15_16("Channel 3", Out[2], (cmsFloat32Number) In[2] / 4.F)) goto Error;
     }

    if (MaxErr > 0) printf("|Err|<%lf ", MaxErr);
    _cmsFreeInterpParams(DbgThread(), p);
    return 1;

Error:
    _cmsFreeInterpParams(DbgThread(), p);
    return 0;
}

static
cmsInt32Number ExaustiveCheck3DinterpolationFloatTrilinear(void)
{
    cmsInterpParams* p;
    cmsInt32Number r, g, b;
    cmsFloat32Number In[3], Out[3];
    cmsFloat32Number FloatTable[] = { //R     G    B

        0,    0,   0,     // B=0,G=0,R=0
        0,    0,  .25,    // B=1,G=0,R=0

        0,   .5,    0,    // B=0,G=1,R=0
        0,   .5,  .25,    // B=1,G=1,R=0

        1,    0,    0,    // B=0,G=0,R=1
        1,    0,  .25,    // B=1,G=0,R=1

        1,    .5,   0,    // B=0,G=1,R=1
        1,    .5,  .25    // B=1,G=1,R=1

    };

    p = _cmsComputeInterpParams(DbgThread(), 2, 3, 3, FloatTable, CMS_LERP_FLAGS_FLOAT|CMS_LERP_FLAGS_TRILINEAR);

    MaxErr = 0.0;
    for (r=0; r < 0xff; r++)
        for (g=0; g < 0xff; g++)
            for (b=0; b < 0xff; b++)
            {

                In[0] = (cmsFloat32Number) r / 255.0F;
                In[1] = (cmsFloat32Number) g / 255.0F;
                In[2] = (cmsFloat32Number) b / 255.0F;


                p ->Interpolation.LerpFloat(DbgThread(), In, Out, p);

                if (!IsGoodFixed15_16("Channel 1", Out[0], In[0])) goto Error;
                if (!IsGoodFixed15_16("Channel 2", Out[1], (cmsFloat32Number) In[1] / 2.F)) goto Error;
                if (!IsGoodFixed15_16("Channel 3", Out[2], (cmsFloat32Number) In[2] / 4.F)) goto Error;
            }

    if (MaxErr > 0) printf("|Err|<%lf ", MaxErr);
    _cmsFreeInterpParams(DbgThread(), p);
    return 1;

Error:
    _cmsFreeInterpParams(DbgThread(), p);
    return 0;

}

static
cmsInt32Number ExhaustiveCheck3DinterpolationTetrahedral16(void)
{
    cmsInterpParams* p;
    cmsInt32Number r, g, b;
    cmsUInt16Number In[3], Out[3];
    cmsUInt16Number Table[] = {

        0,    0,   0,
        0,    0,   0xffff,

        0,    0xffff,    0,
        0,    0xffff,    0xffff,

        0xffff,    0,    0,
        0xffff,    0,    0xffff,

        0xffff,    0xffff,   0,
        0xffff,    0xffff,   0xffff
    };

    p = _cmsComputeInterpParams(DbgThread(), 2, 3, 3, Table, CMS_LERP_FLAGS_16BITS);

    for (r=0; r < 0xff; r++)
        for (g=0; g < 0xff; g++)
            for (b=0; b < 0xff; b++)
        {
            In[0] = (cmsUInt16Number) r ;
            In[1] = (cmsUInt16Number) g ;
            In[2] = (cmsUInt16Number) b ;


        p ->Interpolation.Lerp16(DbgThread(), In, Out, p);

       if (!IsGoodWord("Channel 1", Out[0], In[0])) goto Error;
       if (!IsGoodWord("Channel 2", Out[1], In[1])) goto Error;
       if (!IsGoodWord("Channel 3", Out[2], In[2])) goto Error;
     }

    _cmsFreeInterpParams(DbgThread(), p);
    return 1;

Error:
    _cmsFreeInterpParams(DbgThread(), p);
    return 0;
}

static
cmsInt32Number ExhaustiveCheck3DinterpolationTrilinear16(void)
{
    cmsInterpParams* p;
    cmsInt32Number r, g, b;
    cmsUInt16Number In[3], Out[3];
    cmsUInt16Number Table[] = {

        0,    0,   0,
        0,    0,   0xffff,

        0,    0xffff,    0,
        0,    0xffff,    0xffff,

        0xffff,    0,    0,
        0xffff,    0,    0xffff,

        0xffff,    0xffff,   0,
        0xffff,    0xffff,   0xffff
    };

    p = _cmsComputeInterpParams(DbgThread(), 2, 3, 3, Table, CMS_LERP_FLAGS_TRILINEAR);

    for (r=0; r < 0xff; r++)
        for (g=0; g < 0xff; g++)
            for (b=0; b < 0xff; b++)
        {
            In[0] = (cmsUInt16Number) r ;
            In[1] = (cmsUInt16Number)g ;
            In[2] = (cmsUInt16Number)b ;


        p ->Interpolation.Lerp16(DbgThread(), In, Out, p);

       if (!IsGoodWord("Channel 1", Out[0], In[0])) goto Error;
       if (!IsGoodWord("Channel 2", Out[1], In[1])) goto Error;
       if (!IsGoodWord("Channel 3", Out[2], In[2])) goto Error;
     }


    _cmsFreeInterpParams(DbgThread(), p);
    return 1;

Error:
    _cmsFreeInterpParams(DbgThread(), p);
    return 0;
}

// Check reverse interpolation on LUTS. This is right now exclusively used by K preservation algorithm
static
cmsInt32Number CheckReverseInterpolation3x3(void)
{
 cmsPipeline* Lut;
 cmsStage* clut;
 cmsFloat32Number Target[4], Result[4], Hint[4];
 cmsFloat32Number err, max;
 cmsInt32Number i;
 cmsUInt16Number Table[] = {

        0,    0,   0,                 // 0 0 0
        0,    0,   0xffff,            // 0 0 1

        0,    0xffff,    0,           // 0 1 0
        0,    0xffff,    0xffff,      // 0 1 1

        0xffff,    0,    0,           // 1 0 0
        0xffff,    0,    0xffff,      // 1 0 1

        0xffff,    0xffff,   0,       // 1 1 0
        0xffff,    0xffff,   0xffff,  // 1 1 1
    };



   Lut = cmsPipelineAlloc(DbgThread(), 3, 3);

   clut = cmsStageAllocCLut16bit(DbgThread(), 2, 3, 3, Table);
   cmsPipelineInsertStage(DbgThread(), Lut, cmsAT_BEGIN, clut);

   Target[0] = 0; Target[1] = 0; Target[2] = 0;
   Hint[0] = 0; Hint[1] = 0; Hint[2] = 0;
   cmsPipelineEvalReverseFloat(DbgThread(), Target, Result, NULL, Lut);
   if (Result[0] != 0 || Result[1] != 0 || Result[2] != 0){

       Fail("Reverse interpolation didn't find zero");
       goto Error;
   }

   // Transverse identity
   max = 0;
   for (i=0; i <= 100; i++) {

       cmsFloat32Number in = i / 100.0F;

       Target[0] = in; Target[1] = 0; Target[2] = 0;
       cmsPipelineEvalReverseFloat(DbgThread(), Target, Result, Hint, Lut);

       err = fabsf(in - Result[0]);
       if (err > max) max = err;

       memcpy(Hint, Result, sizeof(Hint));
   }

    cmsPipelineFree(DbgThread(), Lut);
    return (max <= FLOAT_PRECISSION);

Error:
    cmsPipelineFree(DbgThread(), Lut);
    return 0;
}


static
cmsInt32Number CheckReverseInterpolation4x3(void)
{
 cmsPipeline* Lut;
 cmsStage* clut;
 cmsFloat32Number Target[4], Result[4], Hint[4];
 cmsFloat32Number err, max;
 cmsInt32Number i;

 // 4 -> 3, output gets 3 first channels copied
 cmsUInt16Number Table[] = {

        0,         0,         0,          //  0 0 0 0   = ( 0, 0, 0)
        0,         0,         0,          //  0 0 0 1   = ( 0, 0, 0)

        0,         0,         0xffff,     //  0 0 1 0   = ( 0, 0, 1)
        0,         0,         0xffff,     //  0 0 1 1   = ( 0, 0, 1)

        0,         0xffff,    0,          //  0 1 0 0   = ( 0, 1, 0)
        0,         0xffff,    0,          //  0 1 0 1   = ( 0, 1, 0)

        0,         0xffff,    0xffff,     //  0 1 1 0    = ( 0, 1, 1)
        0,         0xffff,    0xffff,     //  0 1 1 1    = ( 0, 1, 1)

        0xffff,    0,         0,          //  1 0 0 0    = ( 1, 0, 0)
        0xffff,    0,         0,          //  1 0 0 1    = ( 1, 0, 0)

        0xffff,    0,         0xffff,     //  1 0 1 0    = ( 1, 0, 1)
        0xffff,    0,         0xffff,     //  1 0 1 1    = ( 1, 0, 1)

        0xffff,    0xffff,    0,          //  1 1 0 0    = ( 1, 1, 0)
        0xffff,    0xffff,    0,          //  1 1 0 1    = ( 1, 1, 0)

        0xffff,    0xffff,    0xffff,     //  1 1 1 0    = ( 1, 1, 1)
        0xffff,    0xffff,    0xffff,     //  1 1 1 1    = ( 1, 1, 1)
    };


   Lut = cmsPipelineAlloc(DbgThread(), 4, 3);

   clut = cmsStageAllocCLut16bit(DbgThread(), 2, 4, 3, Table);
   cmsPipelineInsertStage(DbgThread(), Lut, cmsAT_BEGIN, clut);

   // Check if the LUT is behaving as expected
   SubTest("4->3 feasibility");
   for (i=0; i <= 100; i++) {

       Target[0] = i / 100.0F;
       Target[1] = Target[0];
       Target[2] = 0;
       Target[3] = 12;

       cmsPipelineEvalFloat(DbgThread(), Target, Result, Lut);

       if (!IsGoodFixed15_16("0", Target[0], Result[0])) goto Error;
       if (!IsGoodFixed15_16("1", Target[1], Result[1])) goto Error;
       if (!IsGoodFixed15_16("2", Target[2], Result[2])) goto Error;
   }

   SubTest("4->3 zero");
   Target[0] = 0;
   Target[1] = 0;
   Target[2] = 0;

   // This one holds the fixed K
   Target[3] = 0;

   // This is our hint (which is a big lie in this case)
   Hint[0] = 0.1F; Hint[1] = 0.1F; Hint[2] = 0.1F;

   cmsPipelineEvalReverseFloat(DbgThread(), Target, Result, Hint, Lut);

   if (Result[0] != 0 || Result[1] != 0 || Result[2] != 0 || Result[3] != 0){

       Fail("Reverse interpolation didn't find zero");
       goto Error;
   }

   SubTest("4->3 find CMY");
   max = 0;
   for (i=0; i <= 100; i++) {

       cmsFloat32Number in = i / 100.0F;

       Target[0] = in; Target[1] = 0; Target[2] = 0;
       cmsPipelineEvalReverseFloat(DbgThread(), Target, Result, Hint, Lut);

       err = fabsf(in - Result[0]);
       if (err > max) max = err;

       memcpy(Hint, Result, sizeof(Hint));
   }

    cmsPipelineFree(DbgThread(), Lut);
    return (max <= FLOAT_PRECISSION);

Error:
    cmsPipelineFree(DbgThread(), Lut);
    return 0;
}



// Check all interpolation.

static
cmsUInt16Number Fn8D1(cmsUInt16Number a1, cmsUInt16Number a2, cmsUInt16Number a3, cmsUInt16Number a4,
                      cmsUInt16Number a5, cmsUInt16Number a6, cmsUInt16Number a7, cmsUInt16Number a8,
                      cmsUInt32Number m)
{
    return (cmsUInt16Number) ((a1 + a2 + a3 + a4 + a5 + a6 + a7 + a8) / m);
}


static
cmsUInt16Number Fn8D2(cmsUInt16Number a1, cmsUInt16Number a2, cmsUInt16Number a3, cmsUInt16Number a4,
                      cmsUInt16Number a5, cmsUInt16Number a6, cmsUInt16Number a7, cmsUInt16Number a8,
                      cmsUInt32Number m)
{
    return (cmsUInt16Number) ((a1 + 3* a2 + 3* a3 + a4 + a5 + a6 + a7 + a8 ) / (m + 4));
}


static
cmsUInt16Number Fn8D3(cmsUInt16Number a1, cmsUInt16Number a2, cmsUInt16Number a3, cmsUInt16Number a4,
                      cmsUInt16Number a5, cmsUInt16Number a6, cmsUInt16Number a7, cmsUInt16Number a8,
                      cmsUInt32Number m)
{
    return (cmsUInt16Number) ((3*a1 + 2*a2 + 3*a3 + a4 + a5 + a6 + a7 + a8) / (m + 5));
}




static
cmsInt32Number Sampler3D(cmsContext ContextID, register const cmsUInt16Number In[],
               register cmsUInt16Number Out[],
               register void * Cargo)
{

    Out[0] = Fn8D1(In[0], In[1], In[2], 0, 0, 0, 0, 0, 3);
    Out[1] = Fn8D2(In[0], In[1], In[2], 0, 0, 0, 0, 0, 3);
    Out[2] = Fn8D3(In[0], In[1], In[2], 0, 0, 0, 0, 0, 3);

    return 1;

    cmsUNUSED_PARAMETER(Cargo);

}

static
cmsInt32Number Sampler4D(cmsContext ContextID, register const cmsUInt16Number In[],
               register cmsUInt16Number Out[],
               register void * Cargo)
{

    Out[0] = Fn8D1(In[0], In[1], In[2], In[3], 0, 0, 0, 0, 4);
    Out[1] = Fn8D2(In[0], In[1], In[2], In[3], 0, 0, 0, 0, 4);
    Out[2] = Fn8D3(In[0], In[1], In[2], In[3], 0, 0, 0, 0, 4);

    return 1;

    cmsUNUSED_PARAMETER(Cargo);
}

static
cmsInt32Number Sampler5D(cmsContext ContextID, register const cmsUInt16Number In[],
               register cmsUInt16Number Out[],
               register void * Cargo)
{

    Out[0] = Fn8D1(In[0], In[1], In[2], In[3], In[4], 0, 0, 0, 5);
    Out[1] = Fn8D2(In[0], In[1], In[2], In[3], In[4], 0, 0, 0, 5);
    Out[2] = Fn8D3(In[0], In[1], In[2], In[3], In[4], 0, 0, 0, 5);

    return 1;

    cmsUNUSED_PARAMETER(Cargo);
}

static
cmsInt32Number Sampler6D(cmsContext ContextID, register const cmsUInt16Number In[],
               register cmsUInt16Number Out[],
               register void * Cargo)
{

    Out[0] = Fn8D1(In[0], In[1], In[2], In[3], In[4], In[5], 0, 0, 6);
    Out[1] = Fn8D2(In[0], In[1], In[2], In[3], In[4], In[5], 0, 0, 6);
    Out[2] = Fn8D3(In[0], In[1], In[2], In[3], In[4], In[5], 0, 0, 6);

    return 1;

    cmsUNUSED_PARAMETER(Cargo);
}

static
cmsInt32Number Sampler7D(cmsContext ContextID, register const cmsUInt16Number In[],
               register cmsUInt16Number Out[],
               register void * Cargo)
{

    Out[0] = Fn8D1(In[0], In[1], In[2], In[3], In[4], In[5], In[6], 0, 7);
    Out[1] = Fn8D2(In[0], In[1], In[2], In[3], In[4], In[5], In[6], 0, 7);
    Out[2] = Fn8D3(In[0], In[1], In[2], In[3], In[4], In[5], In[6], 0, 7);

    return 1;

    cmsUNUSED_PARAMETER(Cargo);
}

static
cmsInt32Number Sampler8D(cmsContext ContextID, register const cmsUInt16Number In[],
               register cmsUInt16Number Out[],
               register void * Cargo)
{

    Out[0] = Fn8D1(In[0], In[1], In[2], In[3], In[4], In[5], In[6], In[7], 8);
    Out[1] = Fn8D2(In[0], In[1], In[2], In[3], In[4], In[5], In[6], In[7], 8);
    Out[2] = Fn8D3(In[0], In[1], In[2], In[3], In[4], In[5], In[6], In[7], 8);

    return 1;

    cmsUNUSED_PARAMETER(Cargo);
}

static
cmsBool CheckOne3D(cmsPipeline* lut, cmsUInt16Number a1, cmsUInt16Number a2, cmsUInt16Number a3)
{
    cmsUInt16Number In[3], Out1[3], Out2[3];

    In[0] = a1; In[1] = a2; In[2] = a3;

    // This is the interpolated value
    cmsPipelineEval16(DbgThread(), In, Out1, lut);

    // This is the real value
    Sampler3D(DbgThread(), In, Out2, NULL);

    // Let's see the difference

    if (!IsGoodWordPrec("Channel 1", Out1[0], Out2[0], 2)) return FALSE;
    if (!IsGoodWordPrec("Channel 2", Out1[1], Out2[1], 2)) return FALSE;
    if (!IsGoodWordPrec("Channel 3", Out1[2], Out2[2], 2)) return FALSE;

    return TRUE;
}

static
cmsBool CheckOne4D(cmsPipeline* lut, cmsUInt16Number a1, cmsUInt16Number a2, cmsUInt16Number a3, cmsUInt16Number a4)
{
    cmsUInt16Number In[4], Out1[3], Out2[3];

    In[0] = a1; In[1] = a2; In[2] = a3; In[3] = a4;

    // This is the interpolated value
    cmsPipelineEval16(DbgThread(), In, Out1, lut);

    // This is the real value
    Sampler4D(DbgThread(), In, Out2, NULL);

    // Let's see the difference

    if (!IsGoodWordPrec("Channel 1", Out1[0], Out2[0], 2)) return FALSE;
    if (!IsGoodWordPrec("Channel 2", Out1[1], Out2[1], 2)) return FALSE;
    if (!IsGoodWordPrec("Channel 3", Out1[2], Out2[2], 2)) return FALSE;

    return TRUE;
}

static
cmsBool CheckOne5D(cmsPipeline* lut, cmsUInt16Number a1, cmsUInt16Number a2,
                                     cmsUInt16Number a3, cmsUInt16Number a4, cmsUInt16Number a5)
{
    cmsUInt16Number In[5], Out1[3], Out2[3];

    In[0] = a1; In[1] = a2; In[2] = a3; In[3] = a4; In[4] = a5;

    // This is the interpolated value
    cmsPipelineEval16(DbgThread(), In, Out1, lut);

    // This is the real value
    Sampler5D(DbgThread(), In, Out2, NULL);

    // Let's see the difference

    if (!IsGoodWordPrec("Channel 1", Out1[0], Out2[0], 2)) return FALSE;
    if (!IsGoodWordPrec("Channel 2", Out1[1], Out2[1], 2)) return FALSE;
    if (!IsGoodWordPrec("Channel 3", Out1[2], Out2[2], 2)) return FALSE;

    return TRUE;
}

static
cmsBool CheckOne6D(cmsPipeline* lut, cmsUInt16Number a1, cmsUInt16Number a2,
                                     cmsUInt16Number a3, cmsUInt16Number a4,
                                     cmsUInt16Number a5, cmsUInt16Number a6)
{
    cmsUInt16Number In[6], Out1[3], Out2[3];

    In[0] = a1; In[1] = a2; In[2] = a3; In[3] = a4; In[4] = a5; In[5] = a6;

    // This is the interpolated value
    cmsPipelineEval16(DbgThread(), In, Out1, lut);

    // This is the real value
    Sampler6D(DbgThread(), In, Out2, NULL);

    // Let's see the difference

    if (!IsGoodWordPrec("Channel 1", Out1[0], Out2[0], 2)) return FALSE;
    if (!IsGoodWordPrec("Channel 2", Out1[1], Out2[1], 2)) return FALSE;
    if (!IsGoodWordPrec("Channel 3", Out1[2], Out2[2], 2)) return FALSE;

    return TRUE;
}


static
cmsBool CheckOne7D(cmsPipeline* lut, cmsUInt16Number a1, cmsUInt16Number a2,
                                     cmsUInt16Number a3, cmsUInt16Number a4,
                                     cmsUInt16Number a5, cmsUInt16Number a6,
                                     cmsUInt16Number a7)
{
    cmsUInt16Number In[7], Out1[3], Out2[3];

    In[0] = a1; In[1] = a2; In[2] = a3; In[3] = a4; In[4] = a5; In[5] = a6; In[6] = a7;

    // This is the interpolated value
    cmsPipelineEval16(DbgThread(), In, Out1, lut);

    // This is the real value
    Sampler7D(DbgThread(), In, Out2, NULL);

    // Let's see the difference

    if (!IsGoodWordPrec("Channel 1", Out1[0], Out2[0], 2)) return FALSE;
    if (!IsGoodWordPrec("Channel 2", Out1[1], Out2[1], 2)) return FALSE;
    if (!IsGoodWordPrec("Channel 3", Out1[2], Out2[2], 2)) return FALSE;

    return TRUE;
}


static
cmsBool CheckOne8D(cmsPipeline* lut, cmsUInt16Number a1, cmsUInt16Number a2,
                                     cmsUInt16Number a3, cmsUInt16Number a4,
                                     cmsUInt16Number a5, cmsUInt16Number a6,
                                     cmsUInt16Number a7, cmsUInt16Number a8)
{
    cmsUInt16Number In[8], Out1[3], Out2[3];

    In[0] = a1; In[1] = a2; In[2] = a3; In[3] = a4; In[4] = a5; In[5] = a6; In[6] = a7; In[7] = a8;

    // This is the interpolated value
    cmsPipelineEval16(DbgThread(), In, Out1, lut);

    // This is the real value
    Sampler8D(DbgThread(), In, Out2, NULL);

    // Let's see the difference

    if (!IsGoodWordPrec("Channel 1", Out1[0], Out2[0], 2)) return FALSE;
    if (!IsGoodWordPrec("Channel 2", Out1[1], Out2[1], 2)) return FALSE;
    if (!IsGoodWordPrec("Channel 3", Out1[2], Out2[2], 2)) return FALSE;

    return TRUE;
}


static
cmsInt32Number Check3Dinterp(void)
{
    cmsPipeline* lut;
    cmsStage* mpe;

    lut = cmsPipelineAlloc(DbgThread(), 3, 3);
    mpe = cmsStageAllocCLut16bit(DbgThread(), 9, 3, 3, NULL);
    cmsStageSampleCLut16bit(DbgThread(), mpe, Sampler3D, NULL, 0);
    cmsPipelineInsertStage(DbgThread(), lut, cmsAT_BEGIN, mpe);

    // Check accuracy

    if (!CheckOne3D(lut, 0, 0, 0)) return 0;
    if (!CheckOne3D(lut, 0xffff, 0xffff, 0xffff)) return 0;

    if (!CheckOne3D(lut, 0x8080, 0x8080, 0x8080)) return 0;
    if (!CheckOne3D(lut, 0x0000, 0xFE00, 0x80FF)) return 0;
    if (!CheckOne3D(lut, 0x1111, 0x2222, 0x3333)) return 0;
    if (!CheckOne3D(lut, 0x0000, 0x0012, 0x0013)) return 0;
    if (!CheckOne3D(lut, 0x3141, 0x1415, 0x1592)) return 0;
    if (!CheckOne3D(lut, 0xFF00, 0xFF01, 0xFF12)) return 0;

    cmsPipelineFree(DbgThread(), lut);

    return 1;
}

static
cmsInt32Number Check3DinterpGranular(void)
{
    cmsPipeline* lut;
    cmsStage* mpe;
    cmsUInt32Number Dimensions[] = { 7, 8, 9 };

    lut = cmsPipelineAlloc(DbgThread(), 3, 3);
    mpe = cmsStageAllocCLut16bitGranular(DbgThread(), Dimensions, 3, 3, NULL);
    cmsStageSampleCLut16bit(DbgThread(), mpe, Sampler3D, NULL, 0);
    cmsPipelineInsertStage(DbgThread(), lut, cmsAT_BEGIN, mpe);

    // Check accuracy

    if (!CheckOne3D(lut, 0, 0, 0)) return 0;
    if (!CheckOne3D(lut, 0xffff, 0xffff, 0xffff)) return 0;

    if (!CheckOne3D(lut, 0x8080, 0x8080, 0x8080)) return 0;
    if (!CheckOne3D(lut, 0x0000, 0xFE00, 0x80FF)) return 0;
    if (!CheckOne3D(lut, 0x1111, 0x2222, 0x3333)) return 0;
    if (!CheckOne3D(lut, 0x0000, 0x0012, 0x0013)) return 0;
    if (!CheckOne3D(lut, 0x3141, 0x1415, 0x1592)) return 0;
    if (!CheckOne3D(lut, 0xFF00, 0xFF01, 0xFF12)) return 0;

    cmsPipelineFree(DbgThread(), lut);

    return 1;
}


static
cmsInt32Number Check4Dinterp(void)
{
    cmsPipeline* lut;
    cmsStage* mpe;

    lut = cmsPipelineAlloc(DbgThread(), 4, 3);
    mpe = cmsStageAllocCLut16bit(DbgThread(), 9, 4, 3, NULL);
    cmsStageSampleCLut16bit(DbgThread(), mpe, Sampler4D, NULL, 0);
    cmsPipelineInsertStage(DbgThread(), lut, cmsAT_BEGIN, mpe);

    // Check accuracy

    if (!CheckOne4D(lut, 0, 0, 0, 0)) return 0;
    if (!CheckOne4D(lut, 0xffff, 0xffff, 0xffff, 0xffff)) return 0;

    if (!CheckOne4D(lut, 0x8080, 0x8080, 0x8080, 0x8080)) return 0;
    if (!CheckOne4D(lut, 0x0000, 0xFE00, 0x80FF, 0x8888)) return 0;
    if (!CheckOne4D(lut, 0x1111, 0x2222, 0x3333, 0x4444)) return 0;
    if (!CheckOne4D(lut, 0x0000, 0x0012, 0x0013, 0x0014)) return 0;
    if (!CheckOne4D(lut, 0x3141, 0x1415, 0x1592, 0x9261)) return 0;
    if (!CheckOne4D(lut, 0xFF00, 0xFF01, 0xFF12, 0xFF13)) return 0;

    cmsPipelineFree(DbgThread(), lut);

    return 1;
}



static
cmsInt32Number Check4DinterpGranular(void)
{
    cmsPipeline* lut;
    cmsStage* mpe;
    cmsUInt32Number Dimensions[] = { 9, 8, 7, 6 };

    lut = cmsPipelineAlloc(DbgThread(), 4, 3);
    mpe = cmsStageAllocCLut16bitGranular(DbgThread(), Dimensions, 4, 3, NULL);
    cmsStageSampleCLut16bit(DbgThread(), mpe, Sampler4D, NULL, 0);
    cmsPipelineInsertStage(DbgThread(), lut, cmsAT_BEGIN, mpe);

    // Check accuracy

    if (!CheckOne4D(lut, 0, 0, 0, 0)) return 0;
    if (!CheckOne4D(lut, 0xffff, 0xffff, 0xffff, 0xffff)) return 0;

    if (!CheckOne4D(lut, 0x8080, 0x8080, 0x8080, 0x8080)) return 0;
    if (!CheckOne4D(lut, 0x0000, 0xFE00, 0x80FF, 0x8888)) return 0;
    if (!CheckOne4D(lut, 0x1111, 0x2222, 0x3333, 0x4444)) return 0;
    if (!CheckOne4D(lut, 0x0000, 0x0012, 0x0013, 0x0014)) return 0;
    if (!CheckOne4D(lut, 0x3141, 0x1415, 0x1592, 0x9261)) return 0;
    if (!CheckOne4D(lut, 0xFF00, 0xFF01, 0xFF12, 0xFF13)) return 0;

    cmsPipelineFree(DbgThread(), lut);

    return 1;
}


static
cmsInt32Number Check5DinterpGranular(void)
{
    cmsPipeline* lut;
    cmsStage* mpe;
    cmsUInt32Number Dimensions[] = { 3, 2, 2, 2, 2 };

    lut = cmsPipelineAlloc(DbgThread(), 5, 3);
    mpe = cmsStageAllocCLut16bitGranular(DbgThread(), Dimensions, 5, 3, NULL);
    cmsStageSampleCLut16bit(DbgThread(), mpe, Sampler5D, NULL, 0);
    cmsPipelineInsertStage(DbgThread(), lut, cmsAT_BEGIN, mpe);

    // Check accuracy

    if (!CheckOne5D(lut, 0, 0, 0, 0, 0)) return 0;
    if (!CheckOne5D(lut, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff)) return 0;

    if (!CheckOne5D(lut, 0x8080, 0x8080, 0x8080, 0x8080, 0x1234)) return 0;
    if (!CheckOne5D(lut, 0x0000, 0xFE00, 0x80FF, 0x8888, 0x8078)) return 0;
    if (!CheckOne5D(lut, 0x1111, 0x2222, 0x3333, 0x4444, 0x1455)) return 0;
    if (!CheckOne5D(lut, 0x0000, 0x0012, 0x0013, 0x0014, 0x2333)) return 0;
    if (!CheckOne5D(lut, 0x3141, 0x1415, 0x1592, 0x9261, 0x4567)) return 0;
    if (!CheckOne5D(lut, 0xFF00, 0xFF01, 0xFF12, 0xFF13, 0xF344)) return 0;

    cmsPipelineFree(DbgThread(), lut);

    return 1;
}

static
cmsInt32Number Check6DinterpGranular(void)
{
    cmsPipeline* lut;
    cmsStage* mpe;
    cmsUInt32Number Dimensions[] = { 4, 3, 3, 2, 2, 2 };

    lut = cmsPipelineAlloc(DbgThread(), 6, 3);
    mpe = cmsStageAllocCLut16bitGranular(DbgThread(), Dimensions, 6, 3, NULL);
    cmsStageSampleCLut16bit(DbgThread(), mpe, Sampler6D, NULL, 0);
    cmsPipelineInsertStage(DbgThread(), lut, cmsAT_BEGIN, mpe);

    // Check accuracy

    if (!CheckOne6D(lut, 0, 0, 0, 0, 0, 0)) return 0;
    if (!CheckOne6D(lut, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff)) return 0;

    if (!CheckOne6D(lut, 0x8080, 0x8080, 0x8080, 0x8080, 0x1234, 0x1122)) return 0;
    if (!CheckOne6D(lut, 0x0000, 0xFE00, 0x80FF, 0x8888, 0x8078, 0x2233)) return 0;
    if (!CheckOne6D(lut, 0x1111, 0x2222, 0x3333, 0x4444, 0x1455, 0x3344)) return 0;
    if (!CheckOne6D(lut, 0x0000, 0x0012, 0x0013, 0x0014, 0x2333, 0x4455)) return 0;
    if (!CheckOne6D(lut, 0x3141, 0x1415, 0x1592, 0x9261, 0x4567, 0x5566)) return 0;
    if (!CheckOne6D(lut, 0xFF00, 0xFF01, 0xFF12, 0xFF13, 0xF344, 0x6677)) return 0;

    cmsPipelineFree(DbgThread(), lut);

    return 1;
}

static
cmsInt32Number Check7DinterpGranular(void)
{
    cmsPipeline* lut;
    cmsStage* mpe;
    cmsUInt32Number Dimensions[] = { 4, 3, 3, 2, 2, 2, 2 };

    lut = cmsPipelineAlloc(DbgThread(), 7, 3);
    mpe = cmsStageAllocCLut16bitGranular(DbgThread(), Dimensions, 7, 3, NULL);
    cmsStageSampleCLut16bit(DbgThread(), mpe, Sampler7D, NULL, 0);
    cmsPipelineInsertStage(DbgThread(), lut, cmsAT_BEGIN, mpe);

    // Check accuracy

    if (!CheckOne7D(lut, 0, 0, 0, 0, 0, 0, 0)) return 0;
    if (!CheckOne7D(lut, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff)) return 0;

    if (!CheckOne7D(lut, 0x8080, 0x8080, 0x8080, 0x8080, 0x1234, 0x1122, 0x0056)) return 0;
    if (!CheckOne7D(lut, 0x0000, 0xFE00, 0x80FF, 0x8888, 0x8078, 0x2233, 0x0088)) return 0;
    if (!CheckOne7D(lut, 0x1111, 0x2222, 0x3333, 0x4444, 0x1455, 0x3344, 0x1987)) return 0;
    if (!CheckOne7D(lut, 0x0000, 0x0012, 0x0013, 0x0014, 0x2333, 0x4455, 0x9988)) return 0;
    if (!CheckOne7D(lut, 0x3141, 0x1415, 0x1592, 0x9261, 0x4567, 0x5566, 0xfe56)) return 0;
    if (!CheckOne7D(lut, 0xFF00, 0xFF01, 0xFF12, 0xFF13, 0xF344, 0x6677, 0xbabe)) return 0;

    cmsPipelineFree(DbgThread(), lut);

    return 1;
}


static
cmsInt32Number Check8DinterpGranular(void)
{
    cmsPipeline* lut;
    cmsStage* mpe;
    cmsUInt32Number Dimensions[] = { 4, 3, 3, 2, 2, 2, 2, 2 };

    lut = cmsPipelineAlloc(DbgThread(), 8, 3);
    mpe = cmsStageAllocCLut16bitGranular(DbgThread(), Dimensions, 8, 3, NULL);
    cmsStageSampleCLut16bit(DbgThread(), mpe, Sampler8D, NULL, 0);
    cmsPipelineInsertStage(DbgThread(), lut, cmsAT_BEGIN, mpe);

    // Check accuracy

    if (!CheckOne8D(lut, 0, 0, 0, 0, 0, 0, 0, 0)) return 0;
    if (!CheckOne8D(lut, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff)) return 0;

    if (!CheckOne8D(lut, 0x8080, 0x8080, 0x8080, 0x8080, 0x1234, 0x1122, 0x0056, 0x0011)) return 0;
    if (!CheckOne8D(lut, 0x0000, 0xFE00, 0x80FF, 0x8888, 0x8078, 0x2233, 0x0088, 0x2020)) return 0;
    if (!CheckOne8D(lut, 0x1111, 0x2222, 0x3333, 0x4444, 0x1455, 0x3344, 0x1987, 0x4532)) return 0;
    if (!CheckOne8D(lut, 0x0000, 0x0012, 0x0013, 0x0014, 0x2333, 0x4455, 0x9988, 0x1200)) return 0;
    if (!CheckOne8D(lut, 0x3141, 0x1415, 0x1592, 0x9261, 0x4567, 0x5566, 0xfe56, 0x6666)) return 0;
    if (!CheckOne8D(lut, 0xFF00, 0xFF01, 0xFF12, 0xFF13, 0xF344, 0x6677, 0xbabe, 0xface)) return 0;

    cmsPipelineFree(DbgThread(), lut);

    return 1;
}

// Colorimetric conversions -------------------------------------------------------------------------------------------------

// Lab to LCh and back should be performed at 1E-12 accuracy at least
static
cmsInt32Number CheckLab2LCh(void)
{
    cmsInt32Number l, a, b;
    cmsFloat64Number dist, Max = 0;
    cmsCIELab Lab, Lab2;
    cmsCIELCh LCh;

    for (l=0; l <= 100; l += 10) {

        for (a=-128; a <= +128; a += 8) {

            for (b=-128; b <= 128; b += 8) {

                Lab.L = l;
                Lab.a = a;
                Lab.b = b;

                cmsLab2LCh(DbgThread(), &LCh, &Lab);
                cmsLCh2Lab(DbgThread(), &Lab2, &LCh);

                dist = cmsDeltaE(DbgThread(), &Lab, &Lab2);
                if (dist > Max) Max = dist;
            }
        }
    }

    return Max < 1E-12;
}

// Lab to LCh and back should be performed at 1E-12 accuracy at least
static
cmsInt32Number CheckLab2XYZ(void)
{
    cmsInt32Number l, a, b;
    cmsFloat64Number dist, Max = 0;
    cmsCIELab Lab, Lab2;
    cmsCIEXYZ XYZ;

    for (l=0; l <= 100; l += 10) {

        for (a=-128; a <= +128; a += 8) {

            for (b=-128; b <= 128; b += 8) {

                Lab.L = l;
                Lab.a = a;
                Lab.b = b;

                cmsLab2XYZ(DbgThread(), NULL, &XYZ, &Lab);
                cmsXYZ2Lab(DbgThread(), NULL, &Lab2, &XYZ);

                dist = cmsDeltaE(DbgThread(), &Lab, &Lab2);
                if (dist > Max) Max = dist;

            }
        }
    }

    return Max < 1E-12;
}

// Lab to xyY and back should be performed at 1E-12 accuracy at least
static
cmsInt32Number CheckLab2xyY(void)
{
    cmsInt32Number l, a, b;
    cmsFloat64Number dist, Max = 0;
    cmsCIELab Lab, Lab2;
    cmsCIEXYZ XYZ;
    cmsCIExyY xyY;

    for (l=0; l <= 100; l += 10) {

        for (a=-128; a <= +128; a += 8) {

            for (b=-128; b <= 128; b += 8) {

                Lab.L = l;
                Lab.a = a;
                Lab.b = b;

                cmsLab2XYZ(DbgThread(), NULL, &XYZ, &Lab);
                cmsXYZ2xyY(DbgThread(), &xyY, &XYZ);
                cmsxyY2XYZ(DbgThread(), &XYZ, &xyY);
                cmsXYZ2Lab(DbgThread(), NULL, &Lab2, &XYZ);

                dist = cmsDeltaE(DbgThread(), &Lab, &Lab2);
                if (dist > Max) Max = dist;

            }
        }
    }

    return Max < 1E-12;
}


static
cmsInt32Number CheckLabV2encoding(void)
{
    cmsInt32Number n2, i, j;
    cmsUInt16Number Inw[3], aw[3];
    cmsCIELab Lab;

    n2=0;

    for (j=0; j < 65535; j++) {

        Inw[0] = Inw[1] = Inw[2] = (cmsUInt16Number) j;

        cmsLabEncoded2FloatV2(DbgThread(), &Lab, Inw);
        cmsFloat2LabEncodedV2(DbgThread(), aw, &Lab);

        for (i=0; i < 3; i++) {

        if (aw[i] != j) {
            n2++;
        }
        }

    }

    return (n2 == 0);
}

static
cmsInt32Number CheckLabV4encoding(void)
{
    cmsInt32Number n2, i, j;
    cmsUInt16Number Inw[3], aw[3];
    cmsCIELab Lab;

    n2=0;

    for (j=0; j < 65535; j++) {

        Inw[0] = Inw[1] = Inw[2] = (cmsUInt16Number) j;

        cmsLabEncoded2Float(DbgThread(), &Lab, Inw);
        cmsFloat2LabEncoded(DbgThread(), aw, &Lab);

        for (i=0; i < 3; i++) {

        if (aw[i] != j) {
            n2++;
        }
        }

    }

    return (n2 == 0);
}


// BlackBody -----------------------------------------------------------------------------------------------------

static
cmsInt32Number CheckTemp2CHRM(void)
{
    cmsInt32Number j;
    cmsFloat64Number d, v, Max = 0;
    cmsCIExyY White;

    for (j=4000; j < 25000; j++) {

        cmsWhitePointFromTemp(DbgThread(), &White, j);
        if (!cmsTempFromWhitePoint(DbgThread(), &v, &White)) return 0;

        d = fabs(v - j);
        if (d > Max) Max = d;
    }

    // 100 degree is the actual resolution
    return (Max < 100);
}



// Tone curves -----------------------------------------------------------------------------------------------------

static
cmsInt32Number CheckGammaEstimation(cmsToneCurve* c, cmsFloat64Number g)
{
    cmsFloat64Number est = cmsEstimateGamma(DbgThread(), c, 0.001);

    SubTest("Gamma estimation");
    if (fabs(est - g) > 0.001) return 0;
    return 1;
}

static
cmsInt32Number CheckGammaCreation16(void)
{
    cmsToneCurve* LinGamma = cmsBuildGamma(DbgThread(), 1.0);
    cmsInt32Number i;
    cmsUInt16Number in, out;

    for (i=0; i < 0xffff; i++) {

        in = (cmsUInt16Number) i;
        out = cmsEvalToneCurve16(DbgThread(), LinGamma, in);
        if (in != out) {
            Fail("(lin gamma): Must be %x, But is %x : ", in, out);
            cmsFreeToneCurve(DbgThread(), LinGamma);
            return 0;
        }
    }

    if (!CheckGammaEstimation(LinGamma, 1.0)) return 0;

    cmsFreeToneCurve(DbgThread(), LinGamma);
    return 1;

}

static
cmsInt32Number CheckGammaCreationFlt(void)
{
    cmsToneCurve* LinGamma = cmsBuildGamma(DbgThread(), 1.0);
    cmsInt32Number i;
    cmsFloat32Number in, out;

    for (i=0; i < 0xffff; i++) {

        in = (cmsFloat32Number) (i / 65535.0);
        out = cmsEvalToneCurveFloat(DbgThread(), LinGamma, in);
        if (fabs(in - out) > (1/65535.0)) {
            Fail("(lin gamma): Must be %f, But is %f : ", in, out);
            cmsFreeToneCurve(DbgThread(), LinGamma);
            return 0;
        }
    }

    if (!CheckGammaEstimation(LinGamma, 1.0)) return 0;
    cmsFreeToneCurve(DbgThread(), LinGamma);
    return 1;
}

// Curve curves using a single power function
// Error is given in 0..ffff counts
static
cmsInt32Number CheckGammaFloat(cmsFloat64Number g)
{
    cmsToneCurve* Curve = cmsBuildGamma(DbgThread(), g);
    cmsInt32Number i;
    cmsFloat32Number in, out;
    cmsFloat64Number val, Err;

    MaxErr = 0.0;
    for (i=0; i < 0xffff; i++) {

        in = (cmsFloat32Number) (i / 65535.0);
        out = cmsEvalToneCurveFloat(DbgThread(), Curve, in);
        val = pow((cmsFloat64Number) in, g);

        Err = fabs( val - out);
        if (Err > MaxErr) MaxErr = Err;
    }

    if (MaxErr > 0) printf("|Err|<%lf ", MaxErr * 65535.0);

    if (!CheckGammaEstimation(Curve, g)) return 0;

    cmsFreeToneCurve(DbgThread(), Curve);
    return 1;
}

static cmsInt32Number CheckGamma18(void)
{
    return CheckGammaFloat(1.8);
}

static cmsInt32Number CheckGamma22(void)
{
    return CheckGammaFloat(2.2);
}

static cmsInt32Number CheckGamma30(void)
{
    return CheckGammaFloat(3.0);
}


// Check table-based gamma functions
static
cmsInt32Number CheckGammaFloatTable(cmsFloat64Number g)
{
    cmsFloat32Number Values[1025];
    cmsToneCurve* Curve;
    cmsInt32Number i;
    cmsFloat32Number in, out;
    cmsFloat64Number val, Err;

    for (i=0; i <= 1024; i++) {

        in = (cmsFloat32Number) (i / 1024.0);
        Values[i] = powf(in, (float) g);
    }

    Curve = cmsBuildTabulatedToneCurveFloat(DbgThread(), 1025, Values);

    MaxErr = 0.0;
    for (i=0; i <= 0xffff; i++) {

        in = (cmsFloat32Number) (i / 65535.0);
        out = cmsEvalToneCurveFloat(DbgThread(), Curve, in);
        val = pow(in, g);

        Err = fabs(val - out);
        if (Err > MaxErr) MaxErr = Err;
    }

    if (MaxErr > 0) printf("|Err|<%lf ", MaxErr * 65535.0);

    if (!CheckGammaEstimation(Curve, g)) return 0;

    cmsFreeToneCurve(DbgThread(), Curve);
    return 1;
}


static cmsInt32Number CheckGamma18Table(void)
{
    return CheckGammaFloatTable(1.8);
}

static cmsInt32Number CheckGamma22Table(void)
{
    return CheckGammaFloatTable(2.2);
}

static cmsInt32Number CheckGamma30Table(void)
{
    return CheckGammaFloatTable(3.0);
}

// Create a curve from a table (which is a pure gamma function) and check it against the pow function.
static
cmsInt32Number CheckGammaWordTable(cmsFloat64Number g)
{
    cmsUInt16Number Values[1025];
    cmsToneCurve* Curve;
    cmsInt32Number i;
    cmsFloat32Number in, out;
    cmsFloat64Number val, Err;

    for (i=0; i <= 1024; i++) {

        in = (cmsFloat32Number) (i / 1024.0);
        Values[i] = (cmsUInt16Number) floor(pow(in, g) * 65535.0 + 0.5);
    }

    Curve = cmsBuildTabulatedToneCurve16(DbgThread(), 1025, Values);

    MaxErr = 0.0;
    for (i=0; i <= 0xffff; i++) {

        in = (cmsFloat32Number) (i / 65535.0);
        out = cmsEvalToneCurveFloat(DbgThread(), Curve, in);
        val = pow(in, g);

        Err = fabs(val - out);
        if (Err > MaxErr) MaxErr = Err;
    }

    if (MaxErr > 0) printf("|Err|<%lf ", MaxErr * 65535.0);

    if (!CheckGammaEstimation(Curve, g)) return 0;

    cmsFreeToneCurve(DbgThread(), Curve);
    return 1;
}

static cmsInt32Number CheckGamma18TableWord(void)
{
    return CheckGammaWordTable(1.8);
}

static cmsInt32Number CheckGamma22TableWord(void)
{
    return CheckGammaWordTable(2.2);
}

static cmsInt32Number CheckGamma30TableWord(void)
{
    return CheckGammaWordTable(3.0);
}


// Curve joining test. Joining two high-gamma of 3.0 curves should
// give something like linear
static
cmsInt32Number CheckJointCurves(void)
{
    cmsToneCurve *Forward, *Reverse, *Result;
    cmsBool  rc;

    Forward = cmsBuildGamma(DbgThread(), 3.0);
    Reverse = cmsBuildGamma(DbgThread(), 3.0);

    Result = cmsJoinToneCurve(DbgThread(), Forward, Reverse, 256);

    cmsFreeToneCurve(DbgThread(), Forward); cmsFreeToneCurve(DbgThread(), Reverse);

    rc = cmsIsToneCurveLinear(DbgThread(), Result);
    cmsFreeToneCurve(DbgThread(), Result);

    if (!rc)
        Fail("Joining same curve twice does not result in a linear ramp");

    return rc;
}


// Create a gamma curve by cheating the table
static
cmsToneCurve* GammaTableLinear(cmsInt32Number nEntries, cmsBool Dir)
{
    cmsInt32Number i;
    cmsToneCurve* g = cmsBuildTabulatedToneCurve16(DbgThread(), nEntries, NULL);

    for (i=0; i < nEntries; i++) {

        cmsInt32Number v = _cmsQuantizeVal(i, nEntries);

        if (Dir)
            g->Table16[i] = (cmsUInt16Number) v;
        else
            g->Table16[i] = (cmsUInt16Number) (0xFFFF - v);
    }

    return g;
}


static
cmsInt32Number CheckJointCurvesDescending(void)
{
    cmsToneCurve *Forward, *Reverse, *Result;
    cmsInt32Number i, rc;

     Forward = cmsBuildGamma(DbgThread(), 2.2);

    // Fake the curve to be table-based

    for (i=0; i < 4096; i++)
        Forward ->Table16[i] = 0xffff - Forward->Table16[i];
    Forward ->Segments[0].Type = 0;

    Reverse = cmsReverseToneCurve(DbgThread(), Forward);

    Result = cmsJoinToneCurve(DbgThread(), Reverse, Reverse, 256);

    cmsFreeToneCurve(DbgThread(), Forward);
    cmsFreeToneCurve(DbgThread(), Reverse);

    rc = cmsIsToneCurveLinear(DbgThread(), Result);
    cmsFreeToneCurve(DbgThread(), Result);

    return rc;
}


static
cmsInt32Number CheckFToneCurvePoint(cmsToneCurve* c, cmsUInt16Number Point, cmsInt32Number Value)
{
    cmsInt32Number Result;

    Result = cmsEvalToneCurve16(DbgThread(), c, Point);

    return (abs(Value - Result) < 2);
}

static
cmsInt32Number CheckReverseDegenerated(void)
{
    cmsToneCurve* p, *g;
    cmsUInt16Number Tab[16];

    Tab[0] = 0;
    Tab[1] = 0;
    Tab[2] = 0;
    Tab[3] = 0;
    Tab[4] = 0;
    Tab[5] = 0x5555;
    Tab[6] = 0x6666;
    Tab[7] = 0x7777;
    Tab[8] = 0x8888;
    Tab[9] = 0x9999;
    Tab[10]= 0xffff;
    Tab[11]= 0xffff;
    Tab[12]= 0xffff;
    Tab[13]= 0xffff;
    Tab[14]= 0xffff;
    Tab[15]= 0xffff;

    p = cmsBuildTabulatedToneCurve16(DbgThread(), 16, Tab);
    g = cmsReverseToneCurve(DbgThread(), p);

    // Now let's check some points
    if (!CheckFToneCurvePoint(g, 0x5555, 0x5555)) return 0;
    if (!CheckFToneCurvePoint(g, 0x7777, 0x7777)) return 0;

    // First point for zero
    if (!CheckFToneCurvePoint(g, 0x0000, 0x4444)) return 0;

    // Last point
    if (!CheckFToneCurvePoint(g, 0xFFFF, 0xFFFF)) return 0;

    cmsFreeToneCurve(DbgThread(), p);
    cmsFreeToneCurve(DbgThread(), g);

    return 1;
}


// Build a parametric sRGB-like curve
static
cmsToneCurve* Build_sRGBGamma(void)
{
    cmsFloat64Number Parameters[5];

    Parameters[0] = 2.4;
    Parameters[1] = 1. / 1.055;
    Parameters[2] = 0.055 / 1.055;
    Parameters[3] = 1. / 12.92;
    Parameters[4] = 0.04045;    // d

    return cmsBuildParametricToneCurve(DbgThread(), 4, Parameters);
}



// Join two gamma tables in floating point format. Result should be a straight line
static
cmsToneCurve* CombineGammaFloat(cmsToneCurve* g1, cmsToneCurve* g2)
{
    cmsUInt16Number Tab[256];
    cmsFloat32Number f;
    cmsInt32Number i;

    for (i=0; i < 256; i++) {

        f = (cmsFloat32Number) i / 255.0F;
        f = cmsEvalToneCurveFloat(DbgThread(), g2, cmsEvalToneCurveFloat(DbgThread(), g1, f));

        Tab[i] = (cmsUInt16Number) floor(f * 65535.0 + 0.5);
    }

    return  cmsBuildTabulatedToneCurve16(DbgThread(), 256, Tab);
}

// Same of anterior, but using quantized tables
static
cmsToneCurve* CombineGamma16(cmsToneCurve* g1, cmsToneCurve* g2)
{
    cmsUInt16Number Tab[256];

    cmsInt32Number i;

    for (i=0; i < 256; i++) {

        cmsUInt16Number wValIn;

        wValIn = _cmsQuantizeVal(i, 256);
        Tab[i] = cmsEvalToneCurve16(DbgThread(), g2, cmsEvalToneCurve16(DbgThread(), g1, wValIn));
    }

    return  cmsBuildTabulatedToneCurve16(DbgThread(), 256, Tab);
}

static
cmsInt32Number CheckJointFloatCurves_sRGB(void)
{
    cmsToneCurve *Forward, *Reverse, *Result;
    cmsBool  rc;

    Forward = Build_sRGBGamma();
    Reverse = cmsReverseToneCurve(DbgThread(), Forward);
    Result = CombineGammaFloat(Forward, Reverse);
    cmsFreeToneCurve(DbgThread(), Forward); cmsFreeToneCurve(DbgThread(), Reverse);

    rc = cmsIsToneCurveLinear(DbgThread(), Result);
    cmsFreeToneCurve(DbgThread(), Result);

    return rc;
}

static
cmsInt32Number CheckJoint16Curves_sRGB(void)
{
    cmsToneCurve *Forward, *Reverse, *Result;
    cmsBool  rc;

    Forward = Build_sRGBGamma();
    Reverse = cmsReverseToneCurve(DbgThread(), Forward);
    Result = CombineGamma16(Forward, Reverse);
    cmsFreeToneCurve(DbgThread(), Forward); cmsFreeToneCurve(DbgThread(), Reverse);

    rc = cmsIsToneCurveLinear(DbgThread(), Result);
    cmsFreeToneCurve(DbgThread(), Result);

    return rc;
}

// sigmoidal curve f(x) = (1-x^g) ^(1/g)

static
cmsInt32Number CheckJointCurvesSShaped(void)
{
    cmsFloat64Number p = 3.2;
    cmsToneCurve *Forward, *Reverse, *Result;
    cmsInt32Number rc;

    Forward = cmsBuildParametricToneCurve(DbgThread(), 108, &p);
    Reverse = cmsReverseToneCurve(DbgThread(), Forward);
    Result = cmsJoinToneCurve(DbgThread(), Forward, Forward, 4096);

    cmsFreeToneCurve(DbgThread(), Forward);
    cmsFreeToneCurve(DbgThread(), Reverse);

    rc = cmsIsToneCurveLinear(DbgThread(), Result);
    cmsFreeToneCurve(DbgThread(), Result);
    return rc;
}


// --------------------------------------------------------------------------------------------------------

// Implementation of some tone curve functions
static
cmsFloat32Number Gamma(cmsFloat32Number x, const cmsFloat64Number Params[])
{
    return (cmsFloat32Number) pow(x, Params[0]);
}

static
cmsFloat32Number CIE122(cmsFloat32Number x, const cmsFloat64Number Params[])

{
    cmsFloat64Number e, Val;

    if (x >= -Params[2] / Params[1]) {

        e = Params[1]*x + Params[2];

        if (e > 0)
            Val = pow(e, Params[0]);
        else
            Val = 0;
    }
    else
        Val = 0;

    return (cmsFloat32Number) Val;
}

static
cmsFloat32Number IEC61966_3(cmsFloat32Number x, const cmsFloat64Number Params[])
{
    cmsFloat64Number e, Val;

    if (x >= -Params[2] / Params[1]) {

        e = Params[1]*x + Params[2];

        if (e > 0)
            Val = pow(e, Params[0]) + Params[3];
        else
            Val = 0;
    }
    else
        Val = Params[3];

    return (cmsFloat32Number) Val;
}

static
cmsFloat32Number IEC61966_21(cmsFloat32Number x, const cmsFloat64Number Params[])
{
    cmsFloat64Number e, Val;

    if (x >= Params[4]) {

        e = Params[1]*x + Params[2];

        if (e > 0)
            Val = pow(e, Params[0]);
        else
            Val = 0;
    }
    else
        Val = x * Params[3];

    return (cmsFloat32Number) Val;
}

static
cmsFloat32Number param_5(cmsFloat32Number x, const cmsFloat64Number Params[])
{
    cmsFloat64Number e, Val;
    // Y = (aX + b)^Gamma + e | X >= d
    // Y = cX + f             | else
    if (x >= Params[4]) {

        e = Params[1]*x + Params[2];
        if (e > 0)
            Val = pow(e, Params[0]) + Params[5];
        else
            Val = 0;
    }
    else
        Val = x*Params[3] + Params[6];

    return (cmsFloat32Number) Val;
}

static
cmsFloat32Number param_6(cmsFloat32Number x, const cmsFloat64Number Params[])
{
    cmsFloat64Number e, Val;

    e = Params[1]*x + Params[2];
    if (e > 0)
        Val = pow(e, Params[0]) + Params[3];
    else
        Val = 0;

    return (cmsFloat32Number) Val;
}

static
cmsFloat32Number param_7(cmsFloat32Number x, const cmsFloat64Number Params[])
{
    cmsFloat64Number Val;


    Val = Params[1]*log10(Params[2] * pow(x, Params[0]) + Params[3]) + Params[4];

    return (cmsFloat32Number) Val;
}


static
cmsFloat32Number param_8(cmsFloat32Number x, const cmsFloat64Number Params[])
{
    cmsFloat64Number Val;

    Val = (Params[0] * pow(Params[1], Params[2] * x + Params[3]) + Params[4]);

    return (cmsFloat32Number) Val;
}


static
cmsFloat32Number sigmoidal(cmsFloat32Number x, const cmsFloat64Number Params[])
{
    cmsFloat64Number Val;

    Val = pow(1.0 - pow(1 - x, 1/Params[0]), 1/Params[0]);

    return (cmsFloat32Number) Val;
}


static
cmsBool CheckSingleParametric(const char* Name, dblfnptr fn, cmsInt32Number Type, const cmsFloat64Number Params[])
{
    cmsInt32Number i;
    cmsToneCurve* tc;
    cmsToneCurve* tc_1;
    char InverseText[256];

    tc = cmsBuildParametricToneCurve(DbgThread(), Type, Params);
    tc_1 = cmsBuildParametricToneCurve(DbgThread(), -Type, Params);

    for (i=0; i <= 1000; i++) {

        cmsFloat32Number x = (cmsFloat32Number) i / 1000;
        cmsFloat32Number y_fn, y_param, x_param, y_param2;

        y_fn = fn(x, Params);
        y_param = cmsEvalToneCurveFloat(DbgThread(), tc, x);
        x_param = cmsEvalToneCurveFloat(DbgThread(), tc_1, y_param);

        y_param2 = fn(x_param, Params);

        if (!IsGoodVal(Name, y_fn, y_param, FIXED_PRECISION_15_16))
            goto Error;

        sprintf(InverseText, "Inverse %s", Name);
        if (!IsGoodVal(InverseText, y_fn, y_param2, FIXED_PRECISION_15_16))
            goto Error;
    }

    cmsFreeToneCurve(DbgThread(), tc);
    cmsFreeToneCurve(DbgThread(), tc_1);
    return TRUE;

Error:
    cmsFreeToneCurve(DbgThread(), tc);
    cmsFreeToneCurve(DbgThread(), tc_1);
    return FALSE;
}

// Check against some known values
static
cmsInt32Number CheckParametricToneCurves(void)
{
    cmsFloat64Number Params[10];

     // 1) X = Y ^ Gamma

     Params[0] = 2.2;

     if (!CheckSingleParametric("Gamma", Gamma, 1, Params)) return 0;

     // 2) CIE 122-1966
     // Y = (aX + b)^Gamma  | X >= -b/a
     // Y = 0               | else

     Params[0] = 2.2;
     Params[1] = 1.5;
     Params[2] = -0.5;

     if (!CheckSingleParametric("CIE122-1966", CIE122, 2, Params)) return 0;

     // 3) IEC 61966-3
     // Y = (aX + b)^Gamma | X <= -b/a
     // Y = c              | else

     Params[0] = 2.2;
     Params[1] = 1.5;
     Params[2] = -0.5;
     Params[3] = 0.3;


     if (!CheckSingleParametric("IEC 61966-3", IEC61966_3, 3, Params)) return 0;

     // 4) IEC 61966-2.1 (sRGB)
     // Y = (aX + b)^Gamma | X >= d
     // Y = cX             | X < d

     Params[0] = 2.4;
     Params[1] = 1. / 1.055;
     Params[2] = 0.055 / 1.055;
     Params[3] = 1. / 12.92;
     Params[4] = 0.04045;

     if (!CheckSingleParametric("IEC 61966-2.1", IEC61966_21, 4, Params)) return 0;


     // 5) Y = (aX + b)^Gamma + e | X >= d
     // Y = cX + f             | else

     Params[0] = 2.2;
     Params[1] = 0.7;
     Params[2] = 0.2;
     Params[3] = 0.3;
     Params[4] = 0.1;
     Params[5] = 0.5;
     Params[6] = 0.2;

     if (!CheckSingleParametric("param_5", param_5, 5, Params)) return 0;

     // 6) Y = (aX + b) ^ Gamma + c

     Params[0] = 2.2;
     Params[1] = 0.7;
     Params[2] = 0.2;
     Params[3] = 0.3;

     if (!CheckSingleParametric("param_6", param_6, 6, Params)) return 0;

     // 7) Y = a * log (b * X^Gamma + c) + d

     Params[0] = 2.2;
     Params[1] = 0.9;
     Params[2] = 0.9;
     Params[3] = 0.02;
     Params[4] = 0.1;

     if (!CheckSingleParametric("param_7", param_7, 7, Params)) return 0;

     // 8) Y = a * b ^ (c*X+d) + e

     Params[0] = 0.9;
     Params[1] = 0.9;
     Params[2] = 1.02;
     Params[3] = 0.1;
     Params[4] = 0.2;

     if (!CheckSingleParametric("param_8", param_8, 8, Params)) return 0;

     // 108: S-Shaped: (1 - (1-x)^1/g)^1/g

     Params[0] = 1.9;
     if (!CheckSingleParametric("sigmoidal", sigmoidal, 108, Params)) return 0;

     // All OK

     return 1;
}

// LUT checks ------------------------------------------------------------------------------

static
cmsInt32Number CheckLUTcreation(void)
{
    cmsPipeline* lut;
    cmsPipeline* lut2;
    cmsInt32Number n1, n2;

    lut = cmsPipelineAlloc(DbgThread(), 1, 1);
    n1 = cmsPipelineStageCount(DbgThread(), lut);
    lut2 = cmsPipelineDup(DbgThread(), lut);
    n2 = cmsPipelineStageCount(DbgThread(), lut2);

    cmsPipelineFree(DbgThread(), lut);
    cmsPipelineFree(DbgThread(), lut2);

    return (n1 == 0) && (n2 == 0);
}

// Create a MPE for a identity matrix
static
void AddIdentityMatrix(cmsPipeline* lut)
{
    const cmsFloat64Number Identity[] = { 1, 0, 0,
                          0, 1, 0,
                          0, 0, 1,
                          0, 0, 0 };

    cmsPipelineInsertStage(DbgThread(), lut, cmsAT_END, cmsStageAllocMatrix(DbgThread(), 3, 3, Identity, NULL));
}

// Create a MPE for identity cmsFloat32Number CLUT
static
void AddIdentityCLUTfloat(cmsPipeline* lut)
{
    const cmsFloat32Number  Table[] = {

        0,    0,    0,
        0,    0,    1.0,

        0,    1.0,    0,
        0,    1.0,    1.0,

        1.0,    0,    0,
        1.0,    0,    1.0,

        1.0,    1.0,    0,
        1.0,    1.0,    1.0
    };

    cmsPipelineInsertStage(DbgThread(), lut, cmsAT_END, cmsStageAllocCLutFloat(DbgThread(), 2, 3, 3, Table));
}

// Create a MPE for identity cmsFloat32Number CLUT
static
void AddIdentityCLUT16(cmsPipeline* lut)
{
    const cmsUInt16Number Table[] = {

        0,    0,    0,
        0,    0,    0xffff,

        0,    0xffff,    0,
        0,    0xffff,    0xffff,

        0xffff,    0,    0,
        0xffff,    0,    0xffff,

        0xffff,    0xffff,    0,
        0xffff,    0xffff,    0xffff
    };


    cmsPipelineInsertStage(DbgThread(), lut, cmsAT_END, cmsStageAllocCLut16bit(DbgThread(), 2, 3, 3, Table));
}


// Create a 3 fn identity curves

static
void Add3GammaCurves(cmsPipeline* lut, cmsFloat64Number Curve)
{
    cmsToneCurve* id = cmsBuildGamma(DbgThread(), Curve);
    cmsToneCurve* id3[3];

    id3[0] = id;
    id3[1] = id;
    id3[2] = id;

    cmsPipelineInsertStage(DbgThread(), lut, cmsAT_END, cmsStageAllocToneCurves(DbgThread(), 3, id3));

    cmsFreeToneCurve(DbgThread(), id);
}


static
cmsInt32Number CheckFloatLUT(cmsPipeline* lut)
{
    cmsInt32Number n1, i, j;
    cmsFloat32Number Inf[3], Outf[3];

    n1=0;

    for (j=0; j < 65535; j++) {

        cmsInt32Number af[3];

        Inf[0] = Inf[1] = Inf[2] = (cmsFloat32Number) j / 65535.0F;
        cmsPipelineEvalFloat(DbgThread(), Inf, Outf, lut);

        af[0] = (cmsInt32Number) floor(Outf[0]*65535.0 + 0.5);
        af[1] = (cmsInt32Number) floor(Outf[1]*65535.0 + 0.5);
        af[2] = (cmsInt32Number) floor(Outf[2]*65535.0 + 0.5);

        for (i=0; i < 3; i++) {

            if (af[i] != j) {
                n1++;
            }
        }

    }

    return (n1 == 0);
}


static
cmsInt32Number Check16LUT(cmsPipeline* lut)
{
    cmsInt32Number n2, i, j;
    cmsUInt16Number Inw[3], Outw[3];

    n2=0;

    for (j=0; j < 65535; j++) {

        cmsInt32Number aw[3];

        Inw[0] = Inw[1] = Inw[2] = (cmsUInt16Number) j;
        cmsPipelineEval16(DbgThread(), Inw, Outw, lut);
        aw[0] = Outw[0];
        aw[1] = Outw[1];
        aw[2] = Outw[2];

        for (i=0; i < 3; i++) {

        if (aw[i] != j) {
            n2++;
        }
        }

    }

    return (n2 == 0);
}


// Check any LUT that is linear
static
cmsInt32Number CheckStagesLUT(cmsPipeline* lut, cmsInt32Number ExpectedStages)
{

    cmsInt32Number nInpChans, nOutpChans, nStages;

    nInpChans  = cmsPipelineInputChannels(DbgThread(), lut);
    nOutpChans = cmsPipelineOutputChannels(DbgThread(), lut);
    nStages    = cmsPipelineStageCount(DbgThread(), lut);

    return (nInpChans == 3) && (nOutpChans == 3) && (nStages == ExpectedStages);
}


static
cmsInt32Number CheckFullLUT(cmsPipeline* lut, cmsInt32Number ExpectedStages)
{
    cmsInt32Number rc = CheckStagesLUT(lut, ExpectedStages) && Check16LUT(lut) && CheckFloatLUT(lut);

    cmsPipelineFree(DbgThread(), lut);
    return rc;
}


static
cmsInt32Number Check1StageLUT(void)
{
    cmsPipeline* lut = cmsPipelineAlloc(DbgThread(), 3, 3);

    AddIdentityMatrix(lut);
    return CheckFullLUT(lut, 1);
}



static
cmsInt32Number Check2StageLUT(void)
{
    cmsPipeline* lut = cmsPipelineAlloc(DbgThread(), 3, 3);

    AddIdentityMatrix(lut);
    AddIdentityCLUTfloat(lut);

    return CheckFullLUT(lut, 2);
}

static
cmsInt32Number Check2Stage16LUT(void)
{
    cmsPipeline* lut = cmsPipelineAlloc(DbgThread(), 3, 3);

    AddIdentityMatrix(lut);
    AddIdentityCLUT16(lut);

    return CheckFullLUT(lut, 2);
}



static
cmsInt32Number Check3StageLUT(void)
{
    cmsPipeline* lut = cmsPipelineAlloc(DbgThread(), 3, 3);

    AddIdentityMatrix(lut);
    AddIdentityCLUTfloat(lut);
    Add3GammaCurves(lut, 1.0);

    return CheckFullLUT(lut, 3);
}

static
cmsInt32Number Check3Stage16LUT(void)
{
    cmsPipeline* lut = cmsPipelineAlloc(DbgThread(), 3, 3);

    AddIdentityMatrix(lut);
    AddIdentityCLUT16(lut);
    Add3GammaCurves(lut, 1.0);

    return CheckFullLUT(lut, 3);
}



static
cmsInt32Number Check4StageLUT(void)
{
    cmsPipeline* lut = cmsPipelineAlloc(DbgThread(), 3, 3);

    AddIdentityMatrix(lut);
    AddIdentityCLUTfloat(lut);
    Add3GammaCurves(lut, 1.0);
    AddIdentityMatrix(lut);

    return CheckFullLUT(lut, 4);
}

static
cmsInt32Number Check4Stage16LUT(void)
{
    cmsPipeline* lut = cmsPipelineAlloc(DbgThread(), 3, 3);

    AddIdentityMatrix(lut);
    AddIdentityCLUT16(lut);
    Add3GammaCurves(lut, 1.0);
    AddIdentityMatrix(lut);

    return CheckFullLUT(lut, 4);
}

static
cmsInt32Number Check5StageLUT(void)
{
    cmsPipeline* lut = cmsPipelineAlloc(DbgThread(), 3, 3);

    AddIdentityMatrix(lut);
    AddIdentityCLUTfloat(lut);
    Add3GammaCurves(lut, 1.0);
    AddIdentityMatrix(lut);
    Add3GammaCurves(lut, 1.0);

    return CheckFullLUT(lut, 5);
}


static
cmsInt32Number Check5Stage16LUT(void)
{
    cmsPipeline* lut = cmsPipelineAlloc(DbgThread(), 3, 3);

    AddIdentityMatrix(lut);
    AddIdentityCLUT16(lut);
    Add3GammaCurves(lut, 1.0);
    AddIdentityMatrix(lut);
    Add3GammaCurves(lut, 1.0);

    return CheckFullLUT(lut, 5);
}

static
cmsInt32Number Check6StageLUT(void)
{
    cmsPipeline* lut = cmsPipelineAlloc(DbgThread(), 3, 3);

    AddIdentityMatrix(lut);
    Add3GammaCurves(lut, 1.0);
    AddIdentityCLUTfloat(lut);
    Add3GammaCurves(lut, 1.0);
    AddIdentityMatrix(lut);
    Add3GammaCurves(lut, 1.0);

    return CheckFullLUT(lut, 6);
}

static
cmsInt32Number Check6Stage16LUT(void)
{
    cmsPipeline* lut = cmsPipelineAlloc(DbgThread(), 3, 3);

    AddIdentityMatrix(lut);
    Add3GammaCurves(lut, 1.0);
    AddIdentityCLUT16(lut);
    Add3GammaCurves(lut, 1.0);
    AddIdentityMatrix(lut);
    Add3GammaCurves(lut, 1.0);

    return CheckFullLUT(lut, 6);
}


static
cmsInt32Number CheckLab2LabLUT(void)
{
    cmsPipeline* lut = cmsPipelineAlloc(DbgThread(), 3, 3);
    cmsInt32Number rc;

    cmsPipelineInsertStage(DbgThread(), lut, cmsAT_END, _cmsStageAllocLab2XYZ(DbgThread()));
    cmsPipelineInsertStage(DbgThread(), lut, cmsAT_END, _cmsStageAllocXYZ2Lab(DbgThread()));

    rc = CheckFloatLUT(lut) && CheckStagesLUT(lut, 2);

    cmsPipelineFree(DbgThread(), lut);

    return rc;
}


static
cmsInt32Number CheckXYZ2XYZLUT(void)
{
    cmsPipeline* lut = cmsPipelineAlloc(DbgThread(), 3, 3);
    cmsInt32Number rc;

    cmsPipelineInsertStage(DbgThread(), lut, cmsAT_END, _cmsStageAllocXYZ2Lab(DbgThread()));
    cmsPipelineInsertStage(DbgThread(), lut, cmsAT_END, _cmsStageAllocLab2XYZ(DbgThread()));

    rc = CheckFloatLUT(lut) && CheckStagesLUT(lut, 2);

    cmsPipelineFree(DbgThread(), lut);

    return rc;
}



static
cmsInt32Number CheckLab2LabMatLUT(void)
{
    cmsPipeline* lut = cmsPipelineAlloc(DbgThread(), 3, 3);
    cmsInt32Number rc;

    cmsPipelineInsertStage(DbgThread(), lut, cmsAT_END, _cmsStageAllocLab2XYZ(DbgThread()));
    AddIdentityMatrix(lut);
    cmsPipelineInsertStage(DbgThread(), lut, cmsAT_END, _cmsStageAllocXYZ2Lab(DbgThread()));

    rc = CheckFloatLUT(lut) && CheckStagesLUT(lut, 3);

    cmsPipelineFree(DbgThread(), lut);

    return rc;
}

static
cmsInt32Number CheckNamedColorLUT(void)
{
    cmsPipeline* lut = cmsPipelineAlloc(DbgThread(), 3, 3);
    cmsNAMEDCOLORLIST* nc;
    cmsInt32Number i,j, rc = 1, n2;
    cmsUInt16Number PCS[3];
    cmsUInt16Number Colorant[cmsMAXCHANNELS];
    char Name[255];
    cmsUInt16Number Inw[3], Outw[3];



    nc = cmsAllocNamedColorList(DbgThread(), 256, 3, "pre", "post");
    if (nc == NULL) return 0;

    for (i=0; i < 256; i++) {

        PCS[0] = PCS[1] = PCS[2] = (cmsUInt16Number) i;
        Colorant[0] = Colorant[1] = Colorant[2] = Colorant[3] = (cmsUInt16Number) i;

        sprintf(Name, "#%d", i);
        if (!cmsAppendNamedColor(DbgThread(), nc, Name, PCS, Colorant)) { rc = 0; break; }
    }

    cmsPipelineInsertStage(DbgThread(), lut, cmsAT_END, _cmsStageAllocNamedColor(DbgThread(), nc, FALSE));

    cmsFreeNamedColorList(DbgThread(), nc);
    if (rc == 0) return 0;

    n2=0;

    for (j=0; j < 256; j++) {

        Inw[0] = (cmsUInt16Number) j;

        cmsPipelineEval16(DbgThread(), Inw, Outw, lut);
        for (i=0; i < 3; i++) {

            if (Outw[i] != j) {
                n2++;
            }
        }

    }

    cmsPipelineFree(DbgThread(), lut);
    return (n2 == 0);
}



// --------------------------------------------------------------------------------------------

// A lightweight test of multilocalized unicode structures.

static
cmsInt32Number CheckMLU(void)
{
    cmsMLU* mlu, *mlu2, *mlu3;
    char Buffer[256], Buffer2[256];
    cmsInt32Number rc = 1;
    cmsInt32Number i;
    cmsHPROFILE h= NULL;

    // Allocate a MLU structure, no preferred size
    mlu = cmsMLUalloc(DbgThread(), 0);

    // Add some localizations
    cmsMLUsetWide(DbgThread(), mlu, "en", "US", L"Hello, world");
    cmsMLUsetWide(DbgThread(), mlu, "es", "ES", L"Hola, mundo");
    cmsMLUsetWide(DbgThread(), mlu, "fr", "FR", L"Bonjour, le monde");
    cmsMLUsetWide(DbgThread(), mlu, "ca", "CA", L"Hola, mon");


    // Check the returned string for each language

    cmsMLUgetASCII(DbgThread(), mlu, "en", "US", Buffer, 256);
    if (strcmp(Buffer, "Hello, world") != 0) rc = 0;


    cmsMLUgetASCII(DbgThread(), mlu, "es", "ES", Buffer, 256);
    if (strcmp(Buffer, "Hola, mundo") != 0) rc = 0;


    cmsMLUgetASCII(DbgThread(), mlu, "fr", "FR", Buffer, 256);
    if (strcmp(Buffer, "Bonjour, le monde") != 0) rc = 0;


    cmsMLUgetASCII(DbgThread(), mlu, "ca", "CA", Buffer, 256);
    if (strcmp(Buffer, "Hola, mon") != 0) rc = 0;

    if (rc == 0)
        Fail("Unexpected string '%s'", Buffer);

    // So far, so good.
    cmsMLUfree(DbgThread(), mlu);

    // Now for performance, allocate an empty struct
    mlu = cmsMLUalloc(DbgThread(), 0);

    // Fill it with several thousands of different lenguages
    for (i=0; i < 4096; i++) {

        char Lang[3];

        Lang[0] = (char) (i % 255);
        Lang[1] = (char) (i / 255);
        Lang[2] = 0;

        sprintf(Buffer, "String #%i", i);
        cmsMLUsetASCII(DbgThread(), mlu, Lang, Lang, Buffer);
    }

    // Duplicate it
    mlu2 = cmsMLUdup(DbgThread(), mlu);

    // Get rid of original
    cmsMLUfree(DbgThread(), mlu);

    // Check all is still in place
    for (i=0; i < 4096; i++) {

        char Lang[3];

        Lang[0] = (char)(i % 255);
        Lang[1] = (char)(i / 255);
        Lang[2] = 0;

        cmsMLUgetASCII(DbgThread(), mlu2, Lang, Lang, Buffer2, 256);
        sprintf(Buffer, "String #%i", i);

        if (strcmp(Buffer, Buffer2) != 0) { rc = 0; break; }
    }

    if (rc == 0)
        Fail("Unexpected string '%s'", Buffer2);

    // Check profile IO

    h = cmsOpenProfileFromFile(DbgThread(), "mlucheck.icc", "w");

    cmsSetProfileVersion(DbgThread(), h, 4.3);

    cmsWriteTag(DbgThread(), h, cmsSigProfileDescriptionTag, mlu2);
    cmsCloseProfile(DbgThread(), h);
    cmsMLUfree(DbgThread(), mlu2);


    h = cmsOpenProfileFromFile(DbgThread(), "mlucheck.icc", "r");

    mlu3 = (cmsMLU *) cmsReadTag(DbgThread(), h, cmsSigProfileDescriptionTag);
    if (mlu3 == NULL) { Fail("Profile didn't get the MLU\n"); rc = 0; goto Error; }

    // Check all is still in place
    for (i=0; i < 4096; i++) {

        char Lang[3];

        Lang[0] = (char) (i % 255);
        Lang[1] = (char) (i / 255);
        Lang[2] = 0;

        cmsMLUgetASCII(DbgThread(), mlu3, Lang, Lang, Buffer2, 256);
        sprintf(Buffer, "String #%i", i);

        if (strcmp(Buffer, Buffer2) != 0) { rc = 0; break; }
    }

    if (rc == 0) Fail("Unexpected string '%s'", Buffer2);

Error:

    if (h != NULL) cmsCloseProfile(DbgThread(), h);
    remove("mlucheck.icc");

    return rc;
}


// A lightweight test of named color structures.
static
cmsInt32Number CheckNamedColorList(void)
{
    cmsNAMEDCOLORLIST* nc = NULL, *nc2;
    cmsInt32Number i, j, rc=1;
    char Name[cmsMAX_PATH];
    cmsUInt16Number PCS[3];
    cmsUInt16Number Colorant[cmsMAXCHANNELS];
    char CheckName[cmsMAX_PATH];
    cmsUInt16Number CheckPCS[3];
    cmsUInt16Number CheckColorant[cmsMAXCHANNELS];
    cmsHPROFILE h;

    nc = cmsAllocNamedColorList(DbgThread(), 0, 4, "prefix", "suffix");
    if (nc == NULL) return 0;

    for (i=0; i < 4096; i++) {


        PCS[0] = PCS[1] = PCS[2] = (cmsUInt16Number) i;
        Colorant[0] = Colorant[1] = Colorant[2] = Colorant[3] = (cmsUInt16Number) (4096 - i);

        sprintf(Name, "#%d", i);
        if (!cmsAppendNamedColor(DbgThread(), nc, Name, PCS, Colorant)) { rc = 0; break; }
    }

    for (i=0; i < 4096; i++) {

        CheckPCS[0] = CheckPCS[1] = CheckPCS[2] = (cmsUInt16Number) i;
        CheckColorant[0] = CheckColorant[1] = CheckColorant[2] = CheckColorant[3] = (cmsUInt16Number) (4096 - i);

        sprintf(CheckName, "#%d", i);
        if (!cmsNamedColorInfo(DbgThread(), nc, i, Name, NULL, NULL, PCS, Colorant)) { rc = 0; goto Error; }


        for (j=0; j < 3; j++) {
            if (CheckPCS[j] != PCS[j]) { rc = 0; Fail("Invalid PCS"); goto Error; }
        }

        for (j=0; j < 4; j++) {
            if (CheckColorant[j] != Colorant[j]) { rc = 0; Fail("Invalid Colorant"); goto Error; };
        }

        if (strcmp(Name, CheckName) != 0) {rc = 0; Fail("Invalid Name"); goto Error; };
    }

    h = cmsOpenProfileFromFile(DbgThread(), "namedcol.icc", "w");
    if (h == NULL) return 0;
    if (!cmsWriteTag(DbgThread(), h, cmsSigNamedColor2Tag, nc)) return 0;
    cmsCloseProfile(DbgThread(), h);
    cmsFreeNamedColorList(DbgThread(), nc);
    nc = NULL;

    h = cmsOpenProfileFromFile(DbgThread(), "namedcol.icc", "r");
    nc2 = (cmsNAMEDCOLORLIST *) cmsReadTag(DbgThread(), h, cmsSigNamedColor2Tag);

    if (cmsNamedColorCount(DbgThread(), nc2) != 4096) { rc = 0; Fail("Invalid count"); goto Error; }

    i = cmsNamedColorIndex(DbgThread(), nc2, "#123");
    if (i != 123) { rc = 0; Fail("Invalid index"); goto Error; }


    for (i=0; i < 4096; i++) {

        CheckPCS[0] = CheckPCS[1] = CheckPCS[2] = (cmsUInt16Number) i;
        CheckColorant[0] = CheckColorant[1] = CheckColorant[2] = CheckColorant[3] = (cmsUInt16Number) (4096 - i);

        sprintf(CheckName, "#%d", i);
        if (!cmsNamedColorInfo(DbgThread(), nc2, i, Name, NULL, NULL, PCS, Colorant)) { rc = 0; goto Error; }


        for (j=0; j < 3; j++) {
            if (CheckPCS[j] != PCS[j]) { rc = 0; Fail("Invalid PCS"); goto Error; }
        }

        for (j=0; j < 4; j++) {
            if (CheckColorant[j] != Colorant[j]) { rc = 0; Fail("Invalid Colorant"); goto Error; };
        }

        if (strcmp(Name, CheckName) != 0) {rc = 0; Fail("Invalid Name"); goto Error; };
    }

    cmsCloseProfile(DbgThread(), h);
    remove("namedcol.icc");

Error:
    if (nc != NULL) cmsFreeNamedColorList(DbgThread(), nc);
    return rc;
}



// ----------------------------------------------------------------------------------------------------------

// Formatters

static cmsBool  FormatterFailed;

static
void CheckSingleFormatter16(cmsContext id, cmsUInt32Number Type, const char* Text)
{
    cmsUInt16Number Values[cmsMAXCHANNELS];
    cmsUInt8Number Buffer[1024];
    cmsFormatter f, b;
    cmsInt32Number i, j, nChannels, bytes;
    _cmsTRANSFORM info;

    // Already failed?
    if (FormatterFailed) return;

    memset(&info, 0, sizeof(info));
    info.OutputFormat = info.InputFormat = Type;

    // Go forth and back
    f = _cmsGetFormatter(id, Type,  cmsFormatterInput, CMS_PACK_FLAGS_16BITS);
    b = _cmsGetFormatter(id, Type,  cmsFormatterOutput, CMS_PACK_FLAGS_16BITS);

    if (f.Fmt16 == NULL || b.Fmt16 == NULL) {
        Fail("no formatter for %s", Text);
        FormatterFailed = TRUE;

        // Useful for debug
        f = _cmsGetFormatter(id, Type,  cmsFormatterInput, CMS_PACK_FLAGS_16BITS);
        b = _cmsGetFormatter(id, Type,  cmsFormatterOutput, CMS_PACK_FLAGS_16BITS);
        return;
    }

    nChannels = T_CHANNELS(Type);
    bytes     = T_BYTES(Type);

    for (j=0; j < 5; j++) {

        for (i=0; i < nChannels; i++) {
            Values[i] = (cmsUInt16Number) (i+j);
            // For 8-bit
            if (bytes == 1)
                Values[i] <<= 8;
        }

    b.Fmt16(DbgThread(), &info, Values, Buffer, 2);
    memset(Values, 0, sizeof(Values));
    f.Fmt16(DbgThread(), &info, Values, Buffer, 2);

    for (i=0; i < nChannels; i++) {
        if (bytes == 1)
            Values[i] >>= 8;

        if (Values[i] != i+j) {

            Fail("%s failed", Text);
            FormatterFailed = TRUE;

            // Useful for debug
            for (i=0; i < nChannels; i++) {
                Values[i] = (cmsUInt16Number) (i+j);
                // For 8-bit
                if (bytes == 1)
                    Values[i] <<= 8;
            }

            b.Fmt16(DbgThread(), &info, Values, Buffer, 1);
            f.Fmt16(DbgThread(), &info, Values, Buffer, 1);
            return;
        }
    }
    }
}

#define C(a) CheckSingleFormatter16(0, a, #a)


// Check all formatters
static
cmsInt32Number CheckFormatters16(void)
{
    FormatterFailed = FALSE;

   C( TYPE_GRAY_8            );
   C( TYPE_GRAY_8_REV        );
   C( TYPE_GRAY_16           );
   C( TYPE_GRAY_16_REV       );
   C( TYPE_GRAY_16_SE        );
   C( TYPE_GRAYA_8           );
   C( TYPE_GRAYA_16          );
   C( TYPE_GRAYA_16_SE       );
   C( TYPE_GRAYA_8_PLANAR    );
   C( TYPE_GRAYA_16_PLANAR   );
   C( TYPE_RGB_8             );
   C( TYPE_RGB_8_PLANAR      );
   C( TYPE_BGR_8             );
   C( TYPE_BGR_8_PLANAR      );
   C( TYPE_RGB_16            );
   C( TYPE_RGB_16_PLANAR     );
   C( TYPE_RGB_16_SE         );
   C( TYPE_BGR_16            );
   C( TYPE_BGR_16_PLANAR     );
   C( TYPE_BGR_16_SE         );
   C( TYPE_RGBA_8            );
   C( TYPE_RGBA_8_PLANAR     );
   C( TYPE_RGBA_16           );
   C( TYPE_RGBA_16_PLANAR    );
   C( TYPE_RGBA_16_SE        );
   C( TYPE_ARGB_8            );
   C( TYPE_ARGB_8_PLANAR     );
   C( TYPE_ARGB_16           );
   C( TYPE_ABGR_8            );
   C( TYPE_ABGR_8_PLANAR     );
   C( TYPE_ABGR_16           );
   C( TYPE_ABGR_16_PLANAR    );
   C( TYPE_ABGR_16_SE        );
   C( TYPE_BGRA_8            );
   C( TYPE_BGRA_8_PLANAR     );
   C( TYPE_BGRA_16           );
   C( TYPE_BGRA_16_SE        );
   C( TYPE_CMY_8             );
   C( TYPE_CMY_8_PLANAR      );
   C( TYPE_CMY_16            );
   C( TYPE_CMY_16_PLANAR     );
   C( TYPE_CMY_16_SE         );
   C( TYPE_CMYK_8            );
   C( TYPE_CMYKA_8           );
   C( TYPE_CMYK_8_REV        );
   C( TYPE_YUVK_8            );
   C( TYPE_CMYK_8_PLANAR     );
   C( TYPE_CMYK_16           );
   C( TYPE_CMYK_16_REV       );
   C( TYPE_YUVK_16           );
   C( TYPE_CMYK_16_PLANAR    );
   C( TYPE_CMYK_16_SE        );
   C( TYPE_KYMC_8            );
   C( TYPE_KYMC_16           );
   C( TYPE_KYMC_16_SE        );
   C( TYPE_KCMY_8            );
   C( TYPE_KCMY_8_REV        );
   C( TYPE_KCMY_16           );
   C( TYPE_KCMY_16_REV       );
   C( TYPE_KCMY_16_SE        );
   C( TYPE_CMYK5_8           );
   C( TYPE_CMYK5_16          );
   C( TYPE_CMYK5_16_SE       );
   C( TYPE_KYMC5_8           );
   C( TYPE_KYMC5_16          );
   C( TYPE_KYMC5_16_SE       );
   C( TYPE_CMYK6_8          );
   C( TYPE_CMYK6_8_PLANAR   );
   C( TYPE_CMYK6_16         );
   C( TYPE_CMYK6_16_PLANAR  );
   C( TYPE_CMYK6_16_SE      );
   C( TYPE_CMYK7_8           );
   C( TYPE_CMYK7_16          );
   C( TYPE_CMYK7_16_SE       );
   C( TYPE_KYMC7_8           );
   C( TYPE_KYMC7_16          );
   C( TYPE_KYMC7_16_SE       );
   C( TYPE_CMYK8_8           );
   C( TYPE_CMYK8_16          );
   C( TYPE_CMYK8_16_SE       );
   C( TYPE_KYMC8_8           );
   C( TYPE_KYMC8_16          );
   C( TYPE_KYMC8_16_SE       );
   C( TYPE_CMYK9_8           );
   C( TYPE_CMYK9_16          );
   C( TYPE_CMYK9_16_SE       );
   C( TYPE_KYMC9_8           );
   C( TYPE_KYMC9_16          );
   C( TYPE_KYMC9_16_SE       );
   C( TYPE_CMYK10_8          );
   C( TYPE_CMYK10_16         );
   C( TYPE_CMYK10_16_SE      );
   C( TYPE_KYMC10_8          );
   C( TYPE_KYMC10_16         );
   C( TYPE_KYMC10_16_SE      );
   C( TYPE_CMYK11_8          );
   C( TYPE_CMYK11_16         );
   C( TYPE_CMYK11_16_SE      );
   C( TYPE_KYMC11_8          );
   C( TYPE_KYMC11_16         );
   C( TYPE_KYMC11_16_SE      );
   C( TYPE_CMYK12_8          );
   C( TYPE_CMYK12_16         );
   C( TYPE_CMYK12_16_SE      );
   C( TYPE_KYMC12_8          );
   C( TYPE_KYMC12_16         );
   C( TYPE_KYMC12_16_SE      );
   C( TYPE_XYZ_16            );
   C( TYPE_Lab_8             );
   C( TYPE_ALab_8            );
   C( TYPE_Lab_16            );
   C( TYPE_Yxy_16            );
   C( TYPE_YCbCr_8           );
   C( TYPE_YCbCr_8_PLANAR    );
   C( TYPE_YCbCr_16          );
   C( TYPE_YCbCr_16_PLANAR   );
   C( TYPE_YCbCr_16_SE       );
   C( TYPE_YUV_8             );
   C( TYPE_YUV_8_PLANAR      );
   C( TYPE_YUV_16            );
   C( TYPE_YUV_16_PLANAR     );
   C( TYPE_YUV_16_SE         );
   C( TYPE_HLS_8             );
   C( TYPE_HLS_8_PLANAR      );
   C( TYPE_HLS_16            );
   C( TYPE_HLS_16_PLANAR     );
   C( TYPE_HLS_16_SE         );
   C( TYPE_HSV_8             );
   C( TYPE_HSV_8_PLANAR      );
   C( TYPE_HSV_16            );
   C( TYPE_HSV_16_PLANAR     );
   C( TYPE_HSV_16_SE         );

   C( TYPE_XYZ_FLT  );
   C( TYPE_Lab_FLT  );
   C( TYPE_GRAY_FLT );
   C( TYPE_RGB_FLT  );
   C( TYPE_BGR_FLT  );
   C( TYPE_CMYK_FLT );
   C( TYPE_LabA_FLT );
   C( TYPE_RGBA_FLT );
   C( TYPE_ARGB_FLT );
   C( TYPE_BGRA_FLT );
   C( TYPE_ABGR_FLT );


   C( TYPE_XYZ_DBL  );
   C( TYPE_Lab_DBL  );
   C( TYPE_GRAY_DBL );
   C( TYPE_RGB_DBL  );
   C( TYPE_BGR_DBL  );
   C( TYPE_CMYK_DBL );

   C( TYPE_LabV2_8  );
   C( TYPE_ALabV2_8 );
   C( TYPE_LabV2_16 );

#ifndef CMS_NO_HALF_SUPPORT

   C( TYPE_GRAY_HALF_FLT );
   C( TYPE_RGB_HALF_FLT  );
   C( TYPE_CMYK_HALF_FLT );
   C( TYPE_RGBA_HALF_FLT );

   C( TYPE_RGBA_HALF_FLT );
   C( TYPE_ARGB_HALF_FLT );
   C( TYPE_BGR_HALF_FLT  );
   C( TYPE_BGRA_HALF_FLT );
   C( TYPE_ABGR_HALF_FLT );

#endif

   return FormatterFailed == 0 ? 1 : 0;
}
#undef C

static
void CheckSingleFormatterFloat(cmsUInt32Number Type, const char* Text)
{
    cmsFloat32Number Values[cmsMAXCHANNELS];
    cmsUInt8Number Buffer[1024];
    cmsFormatter f, b;
    cmsInt32Number i, j, nChannels;
    _cmsTRANSFORM info;

    // Already failed?
    if (FormatterFailed) return;

    memset(&info, 0, sizeof(info));
    info.OutputFormat = info.InputFormat = Type;

    // Go forth and back
    f = _cmsGetFormatter(0, Type,  cmsFormatterInput, CMS_PACK_FLAGS_FLOAT);
    b = _cmsGetFormatter(0, Type,  cmsFormatterOutput, CMS_PACK_FLAGS_FLOAT);

    if (f.FmtFloat == NULL || b.FmtFloat == NULL) {
        Fail("no formatter for %s", Text);
        FormatterFailed = TRUE;

        // Useful for debug
        f = _cmsGetFormatter(0, Type,  cmsFormatterInput, CMS_PACK_FLAGS_FLOAT);
        b = _cmsGetFormatter(0, Type,  cmsFormatterOutput, CMS_PACK_FLAGS_FLOAT);
        return;
    }

    nChannels = T_CHANNELS(Type);

    for (j=0; j < 5; j++) {

        for (i=0; i < nChannels; i++) {
            Values[i] = (cmsFloat32Number) (i+j);
        }

        b.FmtFloat(DbgThread(), &info, Values, Buffer, 1);
        memset(Values, 0, sizeof(Values));
        f.FmtFloat(DbgThread(), &info, Values, Buffer, 1);

        for (i=0; i < nChannels; i++) {

            cmsFloat64Number delta = fabs(Values[i] - ( i+j));

            if (delta > 0.000000001) {

                Fail("%s failed", Text);
                FormatterFailed = TRUE;

                // Useful for debug
                for (i=0; i < nChannels; i++) {
                    Values[i] = (cmsFloat32Number) (i+j);
                }

                b.FmtFloat(DbgThread(), &info, Values, Buffer, 1);
                f.FmtFloat(DbgThread(), &info, Values, Buffer, 1);
                return;
            }
        }
    }
}

#define C(a) CheckSingleFormatterFloat(a, #a)

static
cmsInt32Number CheckFormattersFloat(void)
{
    FormatterFailed = FALSE;

    C( TYPE_XYZ_FLT  );
    C( TYPE_Lab_FLT  );
    C( TYPE_GRAY_FLT );
    C( TYPE_RGB_FLT  );
    C( TYPE_BGR_FLT  );
    C( TYPE_CMYK_FLT );

    C( TYPE_LabA_FLT );
    C( TYPE_RGBA_FLT );

    C( TYPE_ARGB_FLT );
    C( TYPE_BGRA_FLT );
    C( TYPE_ABGR_FLT );

    C( TYPE_XYZ_DBL  );
    C( TYPE_Lab_DBL  );
    C( TYPE_GRAY_DBL );
    C( TYPE_RGB_DBL  );
    C( TYPE_BGR_DBL  );
    C( TYPE_CMYK_DBL );
    C( TYPE_XYZ_FLT );

#ifndef CMS_NO_HALF_SUPPORT
   C( TYPE_GRAY_HALF_FLT );
   C( TYPE_RGB_HALF_FLT  );
   C( TYPE_CMYK_HALF_FLT );
   C( TYPE_RGBA_HALF_FLT );

   C( TYPE_RGBA_HALF_FLT );
   C( TYPE_ARGB_HALF_FLT );
   C( TYPE_BGR_HALF_FLT  );
   C( TYPE_BGRA_HALF_FLT );
   C( TYPE_ABGR_HALF_FLT );
#endif




   return FormatterFailed == 0 ? 1 : 0;
}
#undef C

#ifndef CMS_NO_HALF_SUPPORT

// Check half float
#define my_isfinite(x) ((x) != (x))
static
cmsInt32Number CheckFormattersHalf(void)
{
    int i, j;


    for (i=0; i < 0xffff; i++) {

        cmsFloat32Number f = _cmsHalf2Float((cmsUInt16Number) i);

        if (!my_isfinite(f))  {

            j = _cmsFloat2Half(f);

            if (i != j) {
                Fail("%d != %d in Half float support!\n", i, j);
                return 0;
            }
        }
    }

    return 1;
}

#endif

static
cmsInt32Number CheckOneRGB(cmsHTRANSFORM xform, cmsUInt16Number R, cmsUInt16Number G, cmsUInt16Number B, cmsUInt16Number Ro, cmsUInt16Number Go, cmsUInt16Number Bo)
{
    cmsUInt16Number RGB[3];
    cmsUInt16Number Out[3];

    RGB[0] = R;
    RGB[1] = G;
    RGB[2] = B;

    cmsDoTransform(DbgThread(), xform, RGB, Out, 1);

    return IsGoodWord("R", Ro , Out[0]) &&
           IsGoodWord("G", Go , Out[1]) &&
           IsGoodWord("B", Bo , Out[2]);
}

// Check known values going from sRGB to XYZ
static
cmsInt32Number CheckOneRGB_double(cmsHTRANSFORM xform, cmsFloat64Number R, cmsFloat64Number G, cmsFloat64Number B, cmsFloat64Number Ro, cmsFloat64Number Go, cmsFloat64Number Bo)
{
    cmsFloat64Number RGB[3];
    cmsFloat64Number Out[3];

    RGB[0] = R;
    RGB[1] = G;
    RGB[2] = B;

    cmsDoTransform(DbgThread(), xform, RGB, Out, 1);

    return IsGoodVal("R", Ro , Out[0], 0.01) &&
           IsGoodVal("G", Go , Out[1], 0.01) &&
           IsGoodVal("B", Bo , Out[2], 0.01);
}


static
cmsInt32Number CheckChangeBufferFormat(void)
{
    cmsHPROFILE hsRGB = cmsCreate_sRGBProfile(DbgThread());
    cmsHTRANSFORM xform;
    cmsHTRANSFORM xform2;


    xform = cmsCreateTransform(DbgThread(), hsRGB, TYPE_RGB_16, hsRGB, TYPE_RGB_16, INTENT_PERCEPTUAL, 0);
    cmsCloseProfile(DbgThread(), hsRGB);
    if (xform == NULL) return 0;


    if (!CheckOneRGB(xform, 0, 0, 0, 0, 0, 0)) return 0;
    if (!CheckOneRGB(xform, 120, 0, 0, 120, 0, 0)) return 0;
    if (!CheckOneRGB(xform, 0, 222, 255, 0, 222, 255)) return 0;

    xform2 = cmsCloneTransformChangingFormats(DbgThread(), xform, TYPE_BGR_16, TYPE_RGB_16);
    if (!xform2) return 0;

    if (!CheckOneRGB(xform2, 0, 0, 123, 123, 0, 0)) return 0;
    if (!CheckOneRGB(xform2, 154, 234, 0, 0, 234, 154)) return 0;

    cmsDeleteTransform(DbgThread(),xform2);
    xform2 = cmsCloneTransformChangingFormats(DbgThread(), xform, TYPE_RGB_DBL, TYPE_RGB_DBL);
    if (!xform2) return 0;

    if (!CheckOneRGB_double(xform2, 0.20, 0, 0, 0.20, 0, 0)) return 0;
    if (!CheckOneRGB_double(xform2, 0, 0.9, 1, 0, 0.9, 1)) return 0;

    cmsDeleteTransform(DbgThread(),xform2);
    cmsDeleteTransform(DbgThread(),xform);

return 1;
}


// Write tag testbed ----------------------------------------------------------------------------------------

static
cmsInt32Number CheckXYZ(cmsInt32Number Pass, cmsHPROFILE hProfile, cmsTagSignature tag)
{
    cmsCIEXYZ XYZ, *Pt;


    switch (Pass) {

        case 1:

            XYZ.X = 1.0; XYZ.Y = 1.1; XYZ.Z = 1.2;
            return cmsWriteTag(DbgThread(), hProfile, tag, &XYZ);

        case 2:
            Pt = (cmsCIEXYZ *) cmsReadTag(DbgThread(), hProfile, tag);
            if (Pt == NULL) return 0;
            return IsGoodFixed15_16("X", 1.0, Pt ->X) &&
                   IsGoodFixed15_16("Y", 1.1, Pt->Y) &&
                   IsGoodFixed15_16("Z", 1.2, Pt -> Z);

        default:
            return 0;
    }
}


static
cmsInt32Number CheckGamma(cmsInt32Number Pass, cmsHPROFILE hProfile, cmsTagSignature tag)
{
    cmsToneCurve *g, *Pt;
    cmsInt32Number rc;

    switch (Pass) {

        case 1:

            g = cmsBuildGamma(DbgThread(), 1.0);
            rc = cmsWriteTag(DbgThread(), hProfile, tag, g);
            cmsFreeToneCurve(DbgThread(), g);
            return rc;

        case 2:
            Pt = (cmsToneCurve *) cmsReadTag(DbgThread(), hProfile, tag);
            if (Pt == NULL) return 0;
            return cmsIsToneCurveLinear(DbgThread(), Pt);

        default:
            return 0;
    }
}

static
cmsInt32Number CheckTextSingle(cmsInt32Number Pass, cmsHPROFILE hProfile, cmsTagSignature tag)
{
    cmsMLU *m, *Pt;
    cmsInt32Number rc;
    char Buffer[256];


    switch (Pass) {

    case 1:
        m = cmsMLUalloc(DbgThread(), 0);
        cmsMLUsetASCII(DbgThread(), m, cmsNoLanguage, cmsNoCountry, "Test test");
        rc = cmsWriteTag(DbgThread(), hProfile, tag, m);
        cmsMLUfree(DbgThread(), m);
        return rc;

    case 2:
        Pt = (cmsMLU *) cmsReadTag(DbgThread(), hProfile, tag);
        if (Pt == NULL) return 0;
        cmsMLUgetASCII(DbgThread(), Pt, cmsNoLanguage, cmsNoCountry, Buffer, 256);
        if (strcmp(Buffer, "Test test") != 0) return FALSE;
        return TRUE;

    default:
        return 0;
    }
}


static
cmsInt32Number CheckText(cmsInt32Number Pass, cmsHPROFILE hProfile, cmsTagSignature tag)
{
    cmsMLU *m, *Pt;
    cmsInt32Number rc;
    char Buffer[256];


    switch (Pass) {

        case 1:
            m = cmsMLUalloc(DbgThread(), 0);
            cmsMLUsetASCII(DbgThread(), m, cmsNoLanguage, cmsNoCountry, "Test test");
            cmsMLUsetASCII(DbgThread(), m, "en",  "US",  "1 1 1 1");
            cmsMLUsetASCII(DbgThread(), m, "es",  "ES",  "2 2 2 2");
            cmsMLUsetASCII(DbgThread(), m, "ct",  "ES",  "3 3 3 3");
            cmsMLUsetASCII(DbgThread(), m, "en",  "GB",  "444444444");
            rc = cmsWriteTag(DbgThread(), hProfile, tag, m);
            cmsMLUfree(DbgThread(), m);
            return rc;

        case 2:
            Pt = (cmsMLU *) cmsReadTag(DbgThread(), hProfile, tag);
            if (Pt == NULL) return 0;
            cmsMLUgetASCII(DbgThread(), Pt, cmsNoLanguage, cmsNoCountry, Buffer, 256);
            if (strcmp(Buffer, "Test test") != 0) return FALSE;
            cmsMLUgetASCII(DbgThread(), Pt, "en", "US", Buffer, 256);
            if (strcmp(Buffer, "1 1 1 1") != 0) return FALSE;
            cmsMLUgetASCII(DbgThread(), Pt, "es", "ES", Buffer, 256);
            if (strcmp(Buffer, "2 2 2 2") != 0) return FALSE;
            cmsMLUgetASCII(DbgThread(), Pt, "ct", "ES", Buffer, 256);
            if (strcmp(Buffer, "3 3 3 3") != 0) return FALSE;
            cmsMLUgetASCII(DbgThread(), Pt, "en", "GB",  Buffer, 256);
            if (strcmp(Buffer, "444444444") != 0) return FALSE;
            return TRUE;

        default:
            return 0;
    }
}

static
cmsInt32Number CheckData(cmsInt32Number Pass,  cmsHPROFILE hProfile, cmsTagSignature tag)
{
    cmsICCData *Pt;
    cmsICCData d = { 1, 0, { '?' }};
    cmsInt32Number rc;


    switch (Pass) {

        case 1:
            rc = cmsWriteTag(DbgThread(), hProfile, tag, &d);
            return rc;

        case 2:
            Pt = (cmsICCData *) cmsReadTag(DbgThread(), hProfile, tag);
            if (Pt == NULL) return 0;
            return (Pt ->data[0] == '?') && (Pt ->flag == 0) && (Pt ->len == 1);

        default:
            return 0;
    }
}


static
cmsInt32Number CheckSignature(cmsInt32Number Pass,  cmsHPROFILE hProfile, cmsTagSignature tag)
{
    cmsTagSignature *Pt, Holder;

    switch (Pass) {

        case 1:
            Holder = (cmsTagSignature) cmsSigPerceptualReferenceMediumGamut;
            return cmsWriteTag(DbgThread(), hProfile, tag, &Holder);

        case 2:
            Pt = (cmsTagSignature *) cmsReadTag(DbgThread(), hProfile, tag);
            if (Pt == NULL) return 0;
            return *Pt == cmsSigPerceptualReferenceMediumGamut;

        default:
            return 0;
    }
}


static
cmsInt32Number CheckDateTime(cmsInt32Number Pass,  cmsHPROFILE hProfile, cmsTagSignature tag)
{
    struct tm *Pt, Holder;

    switch (Pass) {

        case 1:

            Holder.tm_hour = 1;
            Holder.tm_min = 2;
            Holder.tm_sec = 3;
            Holder.tm_mday = 4;
            Holder.tm_mon = 5;
            Holder.tm_year = 2009 - 1900;
            return cmsWriteTag(DbgThread(), hProfile, tag, &Holder);

        case 2:
            Pt = (struct tm *) cmsReadTag(DbgThread(), hProfile, tag);
            if (Pt == NULL) return 0;

            return (Pt ->tm_hour == 1 &&
                Pt ->tm_min == 2 &&
                Pt ->tm_sec == 3 &&
                Pt ->tm_mday == 4 &&
                Pt ->tm_mon == 5 &&
                Pt ->tm_year == 2009 - 1900);

        default:
            return 0;
    }

}


static
cmsInt32Number CheckNamedColor(cmsInt32Number Pass,  cmsHPROFILE hProfile, cmsTagSignature tag, cmsInt32Number max_check, cmsBool  colorant_check)
{
    cmsNAMEDCOLORLIST* nc;
    cmsInt32Number i, j, rc;
    char Name[255];
    cmsUInt16Number PCS[3];
    cmsUInt16Number Colorant[cmsMAXCHANNELS];
    char CheckName[255];
    cmsUInt16Number CheckPCS[3];
    cmsUInt16Number CheckColorant[cmsMAXCHANNELS];

    switch (Pass) {

    case 1:

        nc = cmsAllocNamedColorList(DbgThread(), 0, 4, "prefix", "suffix");
        if (nc == NULL) return 0;

        for (i=0; i < max_check; i++) {

            PCS[0] = PCS[1] = PCS[2] = (cmsUInt16Number) i;
            Colorant[0] = Colorant[1] = Colorant[2] = Colorant[3] = (cmsUInt16Number) (max_check - i);

            sprintf(Name, "#%d", i);
            if (!cmsAppendNamedColor(DbgThread(), nc, Name, PCS, Colorant)) { Fail("Couldn't append named color"); return 0; }
        }

        rc = cmsWriteTag(DbgThread(), hProfile, tag, nc);
        cmsFreeNamedColorList(DbgThread(), nc);
        return rc;

    case 2:

        nc = (cmsNAMEDCOLORLIST *) cmsReadTag(DbgThread(), hProfile, tag);
        if (nc == NULL) return 0;

        for (i=0; i < max_check; i++) {

            CheckPCS[0] = CheckPCS[1] = CheckPCS[2] = (cmsUInt16Number) i;
            CheckColorant[0] = CheckColorant[1] = CheckColorant[2] = CheckColorant[3] = (cmsUInt16Number) (max_check - i);

            sprintf(CheckName, "#%d", i);
            if (!cmsNamedColorInfo(DbgThread(), nc, i, Name, NULL, NULL, PCS, Colorant)) { Fail("Invalid string"); return 0; }


            for (j=0; j < 3; j++) {
                if (CheckPCS[j] != PCS[j]) {  Fail("Invalid PCS"); return 0; }
            }

            // This is only used on named color list
            if (colorant_check) {

            for (j=0; j < 4; j++) {
                if (CheckColorant[j] != Colorant[j]) { Fail("Invalid Colorant"); return 0; };
            }
            }

            if (strcmp(Name, CheckName) != 0) { Fail("Invalid Name");  return 0; };
        }
        return 1;


    default: return 0;
    }
}


static
cmsInt32Number CheckLUT(cmsInt32Number Pass,  cmsHPROFILE hProfile, cmsTagSignature tag)
{
    cmsPipeline* Lut, *Pt;
    cmsInt32Number rc;


    switch (Pass) {

        case 1:

            Lut = cmsPipelineAlloc(DbgThread(), 3, 3);
            if (Lut == NULL) return 0;

            // Create an identity LUT
            cmsPipelineInsertStage(DbgThread(), Lut, cmsAT_BEGIN, _cmsStageAllocIdentityCurves(DbgThread(), 3));
            cmsPipelineInsertStage(DbgThread(), Lut, cmsAT_END, _cmsStageAllocIdentityCLut(DbgThread(), 3));
            cmsPipelineInsertStage(DbgThread(), Lut, cmsAT_END, _cmsStageAllocIdentityCurves(DbgThread(), 3));

            rc =  cmsWriteTag(DbgThread(), hProfile, tag, Lut);
            cmsPipelineFree(DbgThread(), Lut);
            return rc;

        case 2:
            Pt = (cmsPipeline *) cmsReadTag(DbgThread(), hProfile, tag);
            if (Pt == NULL) return 0;

            // Transform values, check for identity
            return Check16LUT(Pt);

        default:
            return 0;
    }
}

static
cmsInt32Number CheckCHAD(cmsInt32Number Pass,  cmsHPROFILE hProfile, cmsTagSignature tag)
{
    cmsFloat64Number *Pt;
    cmsFloat64Number CHAD[] = { 0, .1, .2, .3, .4, .5, .6, .7, .8 };
    cmsInt32Number i;

    switch (Pass) {

        case 1:
            return cmsWriteTag(DbgThread(), hProfile, tag, CHAD);


        case 2:
            Pt = (cmsFloat64Number *) cmsReadTag(DbgThread(), hProfile, tag);
            if (Pt == NULL) return 0;

            for (i=0; i < 9; i++) {
                if (!IsGoodFixed15_16("CHAD", Pt[i], CHAD[i])) return 0;
            }

            return 1;

        default:
            return 0;
    }
}

static
cmsInt32Number CheckChromaticity(cmsInt32Number Pass,  cmsHPROFILE hProfile, cmsTagSignature tag)
{
    cmsCIExyYTRIPLE *Pt, c = { {0, .1, 1 }, { .3, .4, 1 }, { .6, .7, 1 }};

    switch (Pass) {

        case 1:
            return cmsWriteTag(DbgThread(), hProfile, tag, &c);


        case 2:
            Pt = (cmsCIExyYTRIPLE *) cmsReadTag(DbgThread(), hProfile, tag);
            if (Pt == NULL) return 0;

            if (!IsGoodFixed15_16("xyY", Pt ->Red.x, c.Red.x)) return 0;
            if (!IsGoodFixed15_16("xyY", Pt ->Red.y, c.Red.y)) return 0;
            if (!IsGoodFixed15_16("xyY", Pt ->Green.x, c.Green.x)) return 0;
            if (!IsGoodFixed15_16("xyY", Pt ->Green.y, c.Green.y)) return 0;
            if (!IsGoodFixed15_16("xyY", Pt ->Blue.x, c.Blue.x)) return 0;
            if (!IsGoodFixed15_16("xyY", Pt ->Blue.y, c.Blue.y)) return 0;
            return 1;

        default:
            return 0;
    }
}


static
cmsInt32Number CheckColorantOrder(cmsInt32Number Pass,  cmsHPROFILE hProfile, cmsTagSignature tag)
{
    cmsUInt8Number *Pt, c[cmsMAXCHANNELS];
    cmsInt32Number i;

    switch (Pass) {

        case 1:
            for (i=0; i < cmsMAXCHANNELS; i++) c[i] = (cmsUInt8Number) (cmsMAXCHANNELS - i - 1);
            return cmsWriteTag(DbgThread(), hProfile, tag, c);


        case 2:
            Pt = (cmsUInt8Number *) cmsReadTag(DbgThread(), hProfile, tag);
            if (Pt == NULL) return 0;

            for (i=0; i < cmsMAXCHANNELS; i++) {
                if (Pt[i] != ( cmsMAXCHANNELS - i - 1 )) return 0;
            }
            return 1;

        default:
            return 0;
    }
}

static
cmsInt32Number CheckMeasurement(cmsInt32Number Pass,  cmsHPROFILE hProfile, cmsTagSignature tag)
{
    cmsICCMeasurementConditions *Pt, m;

    switch (Pass) {

        case 1:
            m.Backing.X = 0.1;
            m.Backing.Y = 0.2;
            m.Backing.Z = 0.3;
            m.Flare = 1.0;
            m.Geometry = 1;
            m.IlluminantType = cmsILLUMINANT_TYPE_D50;
            m.Observer = 1;
            return cmsWriteTag(DbgThread(), hProfile, tag, &m);


        case 2:
            Pt = (cmsICCMeasurementConditions *) cmsReadTag(DbgThread(), hProfile, tag);
            if (Pt == NULL) return 0;

            if (!IsGoodFixed15_16("Backing", Pt ->Backing.X, 0.1)) return 0;
            if (!IsGoodFixed15_16("Backing", Pt ->Backing.Y, 0.2)) return 0;
            if (!IsGoodFixed15_16("Backing", Pt ->Backing.Z, 0.3)) return 0;
            if (!IsGoodFixed15_16("Flare",   Pt ->Flare, 1.0)) return 0;

            if (Pt ->Geometry != 1) return 0;
            if (Pt ->IlluminantType != cmsILLUMINANT_TYPE_D50) return 0;
            if (Pt ->Observer != 1) return 0;
            return 1;

        default:
            return 0;
    }
}


static
cmsInt32Number CheckUcrBg(cmsInt32Number Pass,  cmsHPROFILE hProfile, cmsTagSignature tag)
{
    cmsUcrBg *Pt, m;
    cmsInt32Number rc;
    char Buffer[256];

    switch (Pass) {

        case 1:
            m.Ucr = cmsBuildGamma(DbgThread(), 2.4);
            m.Bg  = cmsBuildGamma(DbgThread(), -2.2);
            m.Desc = cmsMLUalloc(DbgThread(), 1);
            cmsMLUsetASCII(DbgThread(), m.Desc,  cmsNoLanguage, cmsNoCountry, "test UCR/BG");
            rc = cmsWriteTag(DbgThread(), hProfile, tag, &m);
            cmsMLUfree(DbgThread(), m.Desc);
            cmsFreeToneCurve(DbgThread(), m.Bg);
            cmsFreeToneCurve(DbgThread(), m.Ucr);
            return rc;


        case 2:
            Pt = (cmsUcrBg *) cmsReadTag(DbgThread(), hProfile, tag);
            if (Pt == NULL) return 0;

            cmsMLUgetASCII(DbgThread(), Pt ->Desc, cmsNoLanguage, cmsNoCountry, Buffer, 256);
            if (strcmp(Buffer, "test UCR/BG") != 0) return 0;
            return 1;

        default:
            return 0;
    }
}


static
cmsInt32Number CheckCRDinfo(cmsInt32Number Pass,  cmsHPROFILE hProfile, cmsTagSignature tag)
{
    cmsMLU *mlu;
    char Buffer[256];
    cmsInt32Number rc;

    switch (Pass) {

        case 1:
            mlu = cmsMLUalloc(DbgThread(), 5);

            cmsMLUsetWide(DbgThread(), mlu,  "PS", "nm", L"test postscript");
            cmsMLUsetWide(DbgThread(), mlu,  "PS", "#0", L"perceptual");
            cmsMLUsetWide(DbgThread(), mlu,  "PS", "#1", L"relative_colorimetric");
            cmsMLUsetWide(DbgThread(), mlu,  "PS", "#2", L"saturation");
            cmsMLUsetWide(DbgThread(), mlu,  "PS", "#3", L"absolute_colorimetric");
            rc = cmsWriteTag(DbgThread(), hProfile, tag, mlu);
            cmsMLUfree(DbgThread(), mlu);
            return rc;


        case 2:
            mlu = (cmsMLU*) cmsReadTag(DbgThread(), hProfile, tag);
            if (mlu == NULL) return 0;



             cmsMLUgetASCII(DbgThread(), mlu, "PS", "nm", Buffer, 256);
             if (strcmp(Buffer, "test postscript") != 0) return 0;


             cmsMLUgetASCII(DbgThread(), mlu, "PS", "#0", Buffer, 256);
             if (strcmp(Buffer, "perceptual") != 0) return 0;


             cmsMLUgetASCII(DbgThread(), mlu, "PS", "#1", Buffer, 256);
             if (strcmp(Buffer, "relative_colorimetric") != 0) return 0;


             cmsMLUgetASCII(DbgThread(), mlu, "PS", "#2", Buffer, 256);
             if (strcmp(Buffer, "saturation") != 0) return 0;


             cmsMLUgetASCII(DbgThread(), mlu, "PS", "#3", Buffer, 256);
             if (strcmp(Buffer, "absolute_colorimetric") != 0) return 0;
             return 1;

        default:
            return 0;
    }
}


static
cmsToneCurve *CreateSegmentedCurve(void)
{
    cmsCurveSegment Seg[3];
    cmsFloat32Number Sampled[2] = { 0, 1};

    Seg[0].Type = 6;
    Seg[0].Params[0] = 1;
    Seg[0].Params[1] = 0;
    Seg[0].Params[2] = 0;
    Seg[0].Params[3] = 0;
    Seg[0].x0 = -1E22F;
    Seg[0].x1 = 0;

    Seg[1].Type = 0;
    Seg[1].nGridPoints = 2;
    Seg[1].SampledPoints = Sampled;
    Seg[1].x0 = 0;
    Seg[1].x1 = 1;

    Seg[2].Type = 6;
    Seg[2].Params[0] = 1;
    Seg[2].Params[1] = 0;
    Seg[2].Params[2] = 0;
    Seg[2].Params[3] = 0;
    Seg[2].x0 = 1;
    Seg[2].x1 = 1E22F;

    return cmsBuildSegmentedToneCurve(DbgThread(), 3, Seg);
}


static
cmsInt32Number CheckMPE(cmsInt32Number Pass,  cmsHPROFILE hProfile, cmsTagSignature tag)
{
    cmsPipeline* Lut, *Pt;
    cmsToneCurve* G[3];
    cmsInt32Number rc;

    switch (Pass) {

        case 1:

            Lut = cmsPipelineAlloc(DbgThread(), 3, 3);

            cmsPipelineInsertStage(DbgThread(), Lut, cmsAT_BEGIN, _cmsStageAllocLabV2ToV4(DbgThread()));
            cmsPipelineInsertStage(DbgThread(), Lut, cmsAT_END, _cmsStageAllocLabV4ToV2(DbgThread()));
            AddIdentityCLUTfloat(Lut);

            G[0] = G[1] = G[2] = CreateSegmentedCurve();
            cmsPipelineInsertStage(DbgThread(), Lut, cmsAT_END, cmsStageAllocToneCurves(DbgThread(), 3, G));
            cmsFreeToneCurve(DbgThread(), G[0]);

            rc = cmsWriteTag(DbgThread(), hProfile, tag, Lut);
            cmsPipelineFree(DbgThread(), Lut);
            return rc;

        case 2:
            Pt = (cmsPipeline *) cmsReadTag(DbgThread(), hProfile, tag);
            if (Pt == NULL) return 0;
            return CheckFloatLUT(Pt);

        default:
            return 0;
    }
}


static
cmsInt32Number CheckScreening(cmsInt32Number Pass,  cmsHPROFILE hProfile, cmsTagSignature tag)
{
    cmsScreening *Pt, sc;
    cmsInt32Number rc;

    switch (Pass) {

        case 1:

            sc.Flag = 0;
            sc.nChannels = 1;
            sc.Channels[0].Frequency = 2.0;
            sc.Channels[0].ScreenAngle = 3.0;
            sc.Channels[0].SpotShape = cmsSPOT_ELLIPSE;

            rc = cmsWriteTag(DbgThread(), hProfile, tag, &sc);
            return rc;


        case 2:
            Pt = (cmsScreening *) cmsReadTag(DbgThread(), hProfile, tag);
            if (Pt == NULL) return 0;

            if (Pt ->nChannels != 1) return 0;
            if (Pt ->Flag      != 0) return 0;
            if (!IsGoodFixed15_16("Freq", Pt ->Channels[0].Frequency, 2.0)) return 0;
            if (!IsGoodFixed15_16("Angle", Pt ->Channels[0].ScreenAngle, 3.0)) return 0;
            if (Pt ->Channels[0].SpotShape != cmsSPOT_ELLIPSE) return 0;
            return 1;

        default:
            return 0;
    }
}


static
cmsBool CheckOneStr(cmsMLU* mlu, cmsInt32Number n)
{
    char Buffer[256], Buffer2[256];


    cmsMLUgetASCII(DbgThread(), mlu, "en", "US", Buffer, 255);
    sprintf(Buffer2, "Hello, world %d", n);
    if (strcmp(Buffer, Buffer2) != 0) return FALSE;


    cmsMLUgetASCII(DbgThread(), mlu, "es", "ES", Buffer, 255);
    sprintf(Buffer2, "Hola, mundo %d", n);
    if (strcmp(Buffer, Buffer2) != 0) return FALSE;

    return TRUE;
}


static
void SetOneStr(cmsMLU** mlu, wchar_t* s1, wchar_t* s2)
{
    *mlu = cmsMLUalloc(DbgThread(), 0);
    cmsMLUsetWide(DbgThread(), *mlu, "en", "US", s1);
    cmsMLUsetWide(DbgThread(), *mlu, "es", "ES", s2);
}


static
cmsInt32Number CheckProfileSequenceTag(cmsInt32Number Pass,  cmsHPROFILE hProfile)
{
    cmsSEQ* s;
    cmsInt32Number i;

    switch (Pass) {

    case 1:

        s = cmsAllocProfileSequenceDescription(DbgThread(), 3);
        if (s == NULL) return 0;

        SetOneStr(&s -> seq[0].Manufacturer, L"Hello, world 0", L"Hola, mundo 0");
        SetOneStr(&s -> seq[0].Model, L"Hello, world 0", L"Hola, mundo 0");
        SetOneStr(&s -> seq[1].Manufacturer, L"Hello, world 1", L"Hola, mundo 1");
        SetOneStr(&s -> seq[1].Model, L"Hello, world 1", L"Hola, mundo 1");
        SetOneStr(&s -> seq[2].Manufacturer, L"Hello, world 2", L"Hola, mundo 2");
        SetOneStr(&s -> seq[2].Model, L"Hello, world 2", L"Hola, mundo 2");


#ifdef CMS_DONT_USE_INT64
        s ->seq[0].attributes[0] = cmsTransparency|cmsMatte;
        s ->seq[0].attributes[1] = 0;
#else
        s ->seq[0].attributes = cmsTransparency|cmsMatte;
#endif

#ifdef CMS_DONT_USE_INT64
        s ->seq[1].attributes[0] = cmsReflective|cmsMatte;
        s ->seq[1].attributes[1] = 0;
#else
        s ->seq[1].attributes = cmsReflective|cmsMatte;
#endif

#ifdef CMS_DONT_USE_INT64
        s ->seq[2].attributes[0] = cmsTransparency|cmsGlossy;
        s ->seq[2].attributes[1] = 0;
#else
        s ->seq[2].attributes = cmsTransparency|cmsGlossy;
#endif

        if (!cmsWriteTag(DbgThread(), hProfile, cmsSigProfileSequenceDescTag, s)) return 0;
        cmsFreeProfileSequenceDescription(DbgThread(), s);
        return 1;

    case 2:

        s = (cmsSEQ *) cmsReadTag(DbgThread(), hProfile, cmsSigProfileSequenceDescTag);
        if (s == NULL) return 0;

        if (s ->n != 3) return 0;

#ifdef CMS_DONT_USE_INT64
        if (s ->seq[0].attributes[0] != (cmsTransparency|cmsMatte)) return 0;
        if (s ->seq[0].attributes[1] != 0) return 0;
#else
        if (s ->seq[0].attributes != (cmsTransparency|cmsMatte)) return 0;
#endif

#ifdef CMS_DONT_USE_INT64
        if (s ->seq[1].attributes[0] != (cmsReflective|cmsMatte)) return 0;
        if (s ->seq[1].attributes[1] != 0) return 0;
#else
        if (s ->seq[1].attributes != (cmsReflective|cmsMatte)) return 0;
#endif

#ifdef CMS_DONT_USE_INT64
        if (s ->seq[2].attributes[0] != (cmsTransparency|cmsGlossy)) return 0;
        if (s ->seq[2].attributes[1] != 0) return 0;
#else
        if (s ->seq[2].attributes != (cmsTransparency|cmsGlossy)) return 0;
#endif

        // Check MLU
        for (i=0; i < 3; i++) {

            if (!CheckOneStr(s -> seq[i].Manufacturer, i)) return 0;
            if (!CheckOneStr(s -> seq[i].Model, i)) return 0;
        }
        return 1;

    default:
        return 0;
    }
}


static
cmsInt32Number CheckProfileSequenceIDTag(cmsInt32Number Pass,  cmsHPROFILE hProfile)
{
    cmsSEQ* s;
    cmsInt32Number i;

    switch (Pass) {

    case 1:

        s = cmsAllocProfileSequenceDescription(DbgThread(), 3);
        if (s == NULL) return 0;

        memcpy(s ->seq[0].ProfileID.ID8, "0123456789ABCDEF", 16);
        memcpy(s ->seq[1].ProfileID.ID8, "1111111111111111", 16);
        memcpy(s ->seq[2].ProfileID.ID8, "2222222222222222", 16);


        SetOneStr(&s -> seq[0].Description, L"Hello, world 0", L"Hola, mundo 0");
        SetOneStr(&s -> seq[1].Description, L"Hello, world 1", L"Hola, mundo 1");
        SetOneStr(&s -> seq[2].Description, L"Hello, world 2", L"Hola, mundo 2");

        if (!cmsWriteTag(DbgThread(), hProfile, cmsSigProfileSequenceIdTag, s)) return 0;
        cmsFreeProfileSequenceDescription(DbgThread(), s);
        return 1;

    case 2:

        s = (cmsSEQ *) cmsReadTag(DbgThread(), hProfile, cmsSigProfileSequenceIdTag);
        if (s == NULL) return 0;

        if (s ->n != 3) return 0;

        if (memcmp(s ->seq[0].ProfileID.ID8, "0123456789ABCDEF", 16) != 0) return 0;
        if (memcmp(s ->seq[1].ProfileID.ID8, "1111111111111111", 16) != 0) return 0;
        if (memcmp(s ->seq[2].ProfileID.ID8, "2222222222222222", 16) != 0) return 0;

        for (i=0; i < 3; i++) {

            if (!CheckOneStr(s -> seq[i].Description, i)) return 0;
        }

        return 1;

    default:
        return 0;
    }
}


static
cmsInt32Number CheckICCViewingConditions(cmsInt32Number Pass,  cmsHPROFILE hProfile)
{
    cmsICCViewingConditions* v;
    cmsICCViewingConditions  s;

    switch (Pass) {

        case 1:
            s.IlluminantType = 1;
            s.IlluminantXYZ.X = 0.1;
            s.IlluminantXYZ.Y = 0.2;
            s.IlluminantXYZ.Z = 0.3;
            s.SurroundXYZ.X = 0.4;
            s.SurroundXYZ.Y = 0.5;
            s.SurroundXYZ.Z = 0.6;

            if (!cmsWriteTag(DbgThread(), hProfile, cmsSigViewingConditionsTag, &s)) return 0;
            return 1;

        case 2:
            v = (cmsICCViewingConditions *) cmsReadTag(DbgThread(), hProfile, cmsSigViewingConditionsTag);
            if (v == NULL) return 0;

            if (v ->IlluminantType != 1) return 0;
            if (!IsGoodVal("IlluminantXYZ.X", v ->IlluminantXYZ.X, 0.1, 0.001)) return 0;
            if (!IsGoodVal("IlluminantXYZ.Y", v ->IlluminantXYZ.Y, 0.2, 0.001)) return 0;
            if (!IsGoodVal("IlluminantXYZ.Z", v ->IlluminantXYZ.Z, 0.3, 0.001)) return 0;

            if (!IsGoodVal("SurroundXYZ.X", v ->SurroundXYZ.X, 0.4, 0.001)) return 0;
            if (!IsGoodVal("SurroundXYZ.Y", v ->SurroundXYZ.Y, 0.5, 0.001)) return 0;
            if (!IsGoodVal("SurroundXYZ.Z", v ->SurroundXYZ.Z, 0.6, 0.001)) return 0;

            return 1;

        default:
            return 0;
    }

}


static
cmsInt32Number CheckVCGT(cmsInt32Number Pass,  cmsHPROFILE hProfile)
{
    cmsToneCurve* Curves[3];
    cmsToneCurve** PtrCurve;

     switch (Pass) {

        case 1:
            Curves[0] = cmsBuildGamma(DbgThread(), 1.1);
            Curves[1] = cmsBuildGamma(DbgThread(), 2.2);
            Curves[2] = cmsBuildGamma(DbgThread(), 3.4);

            if (!cmsWriteTag(DbgThread(), hProfile, cmsSigVcgtTag, Curves)) return 0;

            cmsFreeToneCurveTriple(DbgThread(), Curves);
            return 1;


        case 2:

             PtrCurve = (cmsToneCurve **) cmsReadTag(DbgThread(), hProfile, cmsSigVcgtTag);
             if (PtrCurve == NULL) return 0;
             if (!IsGoodVal("VCGT R", cmsEstimateGamma(DbgThread(), PtrCurve[0], 0.01), 1.1, 0.001)) return 0;
             if (!IsGoodVal("VCGT G", cmsEstimateGamma(DbgThread(), PtrCurve[1], 0.01), 2.2, 0.001)) return 0;
             if (!IsGoodVal("VCGT B", cmsEstimateGamma(DbgThread(), PtrCurve[2], 0.01), 3.4, 0.001)) return 0;
             return 1;

        default:;
    }

    return 0;
}


// Only one of the two following may be used, as they share the same tag
static
cmsInt32Number CheckDictionary16(cmsInt32Number Pass,  cmsHPROFILE hProfile)
{
      cmsHANDLE hDict;
      const cmsDICTentry* e;
      switch (Pass) {

        case 1:
            hDict = cmsDictAlloc(DbgThread());
            cmsDictAddEntry(DbgThread(), hDict, L"Name0",  NULL, NULL, NULL);
            cmsDictAddEntry(DbgThread(), hDict, L"Name1",  L"", NULL, NULL);
            cmsDictAddEntry(DbgThread(), hDict, L"Name",  L"String", NULL, NULL);
            cmsDictAddEntry(DbgThread(), hDict, L"Name2", L"12",    NULL, NULL);
            if (!cmsWriteTag(DbgThread(), hProfile, cmsSigMetaTag, hDict)) return 0;
            cmsDictFree(DbgThread(), hDict);
            return 1;


        case 2:

             hDict = cmsReadTag(DbgThread(), hProfile, cmsSigMetaTag);
             if (hDict == NULL) return 0;
             e = cmsDictGetEntryList(DbgThread(), hDict);
             if (memcmp(e ->Name, L"Name2", sizeof(wchar_t) * 5) != 0) return 0;
             if (memcmp(e ->Value, L"12",  sizeof(wchar_t) * 2) != 0) return 0;
             e = cmsDictNextEntry(DbgThread(), e);
             if (memcmp(e ->Name, L"Name", sizeof(wchar_t) * 4) != 0) return 0;
             if (memcmp(e ->Value, L"String",  sizeof(wchar_t) * 5) != 0) return 0;
             e = cmsDictNextEntry(DbgThread(), e);
             if (memcmp(e ->Name, L"Name1", sizeof(wchar_t) *5) != 0) return 0;
             if (e ->Value == NULL) return 0;
             if (*e->Value != 0) return 0;
             e = cmsDictNextEntry(DbgThread(), e);
             if (memcmp(e ->Name, L"Name0", sizeof(wchar_t) * 5) != 0) return 0;
             if (e ->Value != NULL) return 0;
             return 1;


        default:;
    }

    return 0;
}



static
cmsInt32Number CheckDictionary24(cmsInt32Number Pass,  cmsHPROFILE hProfile)
{
    cmsHANDLE hDict;
    const cmsDICTentry* e;
    cmsMLU* DisplayName;
    char Buffer[256];
    cmsInt32Number rc = 1;

    switch (Pass) {

    case 1:
        hDict = cmsDictAlloc(DbgThread());

        DisplayName = cmsMLUalloc(DbgThread(), 0);

        cmsMLUsetWide(DbgThread(), DisplayName, "en", "US", L"Hello, world");
        cmsMLUsetWide(DbgThread(), DisplayName, "es", "ES", L"Hola, mundo");
        cmsMLUsetWide(DbgThread(), DisplayName, "fr", "FR", L"Bonjour, le monde");
        cmsMLUsetWide(DbgThread(), DisplayName, "ca", "CA", L"Hola, mon");

        cmsDictAddEntry(DbgThread(), hDict, L"Name",  L"String", DisplayName, NULL);
        cmsMLUfree(DbgThread(), DisplayName);

        cmsDictAddEntry(DbgThread(), hDict, L"Name2", L"12",    NULL, NULL);
        if (!cmsWriteTag(DbgThread(), hProfile, cmsSigMetaTag, hDict)) return 0;
        cmsDictFree(DbgThread(), hDict);

        return 1;


    case 2:

        hDict = cmsReadTag(DbgThread(), hProfile, cmsSigMetaTag);
        if (hDict == NULL) return 0;

        e = cmsDictGetEntryList(DbgThread(), hDict);
        if (memcmp(e ->Name, L"Name2", sizeof(wchar_t) * 5) != 0) return 0;
        if (memcmp(e ->Value, L"12",  sizeof(wchar_t) * 2) != 0) return 0;
        e = cmsDictNextEntry(DbgThread(), e);
        if (memcmp(e ->Name, L"Name", sizeof(wchar_t) * 4) != 0) return 0;
        if (memcmp(e ->Value, L"String",  sizeof(wchar_t) * 5) != 0) return 0;

        cmsMLUgetASCII(DbgThread(), e->DisplayName, "en", "US", Buffer, 256);
        if (strcmp(Buffer, "Hello, world") != 0) rc = 0;


        cmsMLUgetASCII(DbgThread(), e->DisplayName, "es", "ES", Buffer, 256);
        if (strcmp(Buffer, "Hola, mundo") != 0) rc = 0;


        cmsMLUgetASCII(DbgThread(), e->DisplayName, "fr", "FR", Buffer, 256);
        if (strcmp(Buffer, "Bonjour, le monde") != 0) rc = 0;


        cmsMLUgetASCII(DbgThread(), e->DisplayName, "ca", "CA", Buffer, 256);
        if (strcmp(Buffer, "Hola, mon") != 0) rc = 0;

        if (rc == 0)
            Fail("Unexpected string '%s'", Buffer);
        return 1;

    default:;
    }

    return 0;
}

static
cmsInt32Number CheckRAWtags(cmsInt32Number Pass,  cmsHPROFILE hProfile)
{
    char Buffer[7];

    switch (Pass) {

        case 1:
            return cmsWriteRawTag(DbgThread(), hProfile, (cmsTagSignature) 0x31323334, "data123", 7);

        case 2:
            if (!cmsReadRawTag(DbgThread(), hProfile, (cmsTagSignature) 0x31323334, Buffer, 7)) return 0;

            if (strncmp(Buffer, "data123", 7) != 0) return 0;
            return 1;

        default:
            return 0;
    }
}


// This is a very big test that checks every single tag
static
cmsInt32Number CheckProfileCreation(void)
{
    cmsHPROFILE h;
    cmsInt32Number Pass;

    h = cmsCreateProfilePlaceholder(DbgThread());
    if (h == NULL) return 0;

    cmsSetProfileVersion(DbgThread(), h, 4.3);
    if (cmsGetTagCount(DbgThread(), h) != 0) { Fail("Empty profile with nonzero number of tags"); goto Error; }
    if (cmsIsTag(DbgThread(), h, cmsSigAToB0Tag)) { Fail("Found a tag in an empty profile"); goto Error; }

    cmsSetColorSpace(DbgThread(), h, cmsSigRgbData);
    if (cmsGetColorSpace(DbgThread(), h) !=  cmsSigRgbData) { Fail("Unable to set colorspace"); goto Error; }

    cmsSetPCS(DbgThread(), h, cmsSigLabData);
    if (cmsGetPCS(DbgThread(), h) !=  cmsSigLabData) { Fail("Unable to set colorspace"); goto Error; }

    cmsSetDeviceClass(DbgThread(), h, cmsSigDisplayClass);
    if (cmsGetDeviceClass(DbgThread(), h) != cmsSigDisplayClass) { Fail("Unable to set deviceclass"); goto Error; }

    cmsSetHeaderRenderingIntent(DbgThread(), h, INTENT_SATURATION);
    if (cmsGetHeaderRenderingIntent(DbgThread(), h) != INTENT_SATURATION) { Fail("Unable to set rendering intent"); goto Error; }

    for (Pass = 1; Pass <= 2; Pass++) {

        SubTest("Tags holding XYZ");

        if (!CheckXYZ(Pass, h, cmsSigBlueColorantTag)) goto Error;
        if (!CheckXYZ(Pass, h, cmsSigGreenColorantTag)) goto Error;
        if (!CheckXYZ(Pass, h, cmsSigRedColorantTag)) goto Error;
        if (!CheckXYZ(Pass, h, cmsSigMediaBlackPointTag)) goto Error;
        if (!CheckXYZ(Pass, h, cmsSigMediaWhitePointTag)) goto Error;
        if (!CheckXYZ(Pass, h, cmsSigLuminanceTag)) goto Error;

        SubTest("Tags holding curves");

        if (!CheckGamma(Pass, h, cmsSigBlueTRCTag)) goto Error;
        if (!CheckGamma(Pass, h, cmsSigGrayTRCTag)) goto Error;
        if (!CheckGamma(Pass, h, cmsSigGreenTRCTag)) goto Error;
        if (!CheckGamma(Pass, h, cmsSigRedTRCTag)) goto Error;

        SubTest("Tags holding text");

        if (!CheckTextSingle(Pass, h, cmsSigCharTargetTag)) goto Error;
        if (!CheckTextSingle(Pass, h, cmsSigScreeningDescTag)) goto Error;

        if (!CheckText(Pass, h, cmsSigCopyrightTag)) goto Error;
        if (!CheckText(Pass, h, cmsSigProfileDescriptionTag)) goto Error;
        if (!CheckText(Pass, h, cmsSigDeviceMfgDescTag)) goto Error;
        if (!CheckText(Pass, h, cmsSigDeviceModelDescTag)) goto Error;
        if (!CheckText(Pass, h, cmsSigViewingCondDescTag)) goto Error;



        SubTest("Tags holding cmsICCData");

        if (!CheckData(Pass, h, cmsSigPs2CRD0Tag)) goto Error;
        if (!CheckData(Pass, h, cmsSigPs2CRD1Tag)) goto Error;
        if (!CheckData(Pass, h, cmsSigPs2CRD2Tag)) goto Error;
        if (!CheckData(Pass, h, cmsSigPs2CRD3Tag)) goto Error;
        if (!CheckData(Pass, h, cmsSigPs2CSATag)) goto Error;
        if (!CheckData(Pass, h, cmsSigPs2RenderingIntentTag)) goto Error;

        SubTest("Tags holding signatures");

        if (!CheckSignature(Pass, h, cmsSigColorimetricIntentImageStateTag)) goto Error;
        if (!CheckSignature(Pass, h, cmsSigPerceptualRenderingIntentGamutTag)) goto Error;
        if (!CheckSignature(Pass, h, cmsSigSaturationRenderingIntentGamutTag)) goto Error;
        if (!CheckSignature(Pass, h, cmsSigTechnologyTag)) goto Error;

        SubTest("Tags holding date_time");

        if (!CheckDateTime(Pass, h, cmsSigCalibrationDateTimeTag)) goto Error;
        if (!CheckDateTime(Pass, h, cmsSigDateTimeTag)) goto Error;

        SubTest("Tags holding named color lists");

        if (!CheckNamedColor(Pass, h, cmsSigColorantTableTag, 15, FALSE)) goto Error;
        if (!CheckNamedColor(Pass, h, cmsSigColorantTableOutTag, 15, FALSE)) goto Error;
        if (!CheckNamedColor(Pass, h, cmsSigNamedColor2Tag, 4096, TRUE)) goto Error;

        SubTest("Tags holding LUTs");

        if (!CheckLUT(Pass, h, cmsSigAToB0Tag)) goto Error;
        if (!CheckLUT(Pass, h, cmsSigAToB1Tag)) goto Error;
        if (!CheckLUT(Pass, h, cmsSigAToB2Tag)) goto Error;
        if (!CheckLUT(Pass, h, cmsSigBToA0Tag)) goto Error;
        if (!CheckLUT(Pass, h, cmsSigBToA1Tag)) goto Error;
        if (!CheckLUT(Pass, h, cmsSigBToA2Tag)) goto Error;
        if (!CheckLUT(Pass, h, cmsSigPreview0Tag)) goto Error;
        if (!CheckLUT(Pass, h, cmsSigPreview1Tag)) goto Error;
        if (!CheckLUT(Pass, h, cmsSigPreview2Tag)) goto Error;
        if (!CheckLUT(Pass, h, cmsSigGamutTag)) goto Error;

        SubTest("Tags holding CHAD");
        if (!CheckCHAD(Pass, h, cmsSigChromaticAdaptationTag)) goto Error;

        SubTest("Tags holding Chromaticity");
        if (!CheckChromaticity(Pass, h, cmsSigChromaticityTag)) goto Error;

        SubTest("Tags holding colorant order");
        if (!CheckColorantOrder(Pass, h, cmsSigColorantOrderTag)) goto Error;

        SubTest("Tags holding measurement");
        if (!CheckMeasurement(Pass, h, cmsSigMeasurementTag)) goto Error;

        SubTest("Tags holding CRD info");
        if (!CheckCRDinfo(Pass, h, cmsSigCrdInfoTag)) goto Error;

        SubTest("Tags holding UCR/BG");
        if (!CheckUcrBg(Pass, h, cmsSigUcrBgTag)) goto Error;

        SubTest("Tags holding MPE");
        if (!CheckMPE(Pass, h, cmsSigDToB0Tag)) goto Error;
        if (!CheckMPE(Pass, h, cmsSigDToB1Tag)) goto Error;
        if (!CheckMPE(Pass, h, cmsSigDToB2Tag)) goto Error;
        if (!CheckMPE(Pass, h, cmsSigDToB3Tag)) goto Error;
        if (!CheckMPE(Pass, h, cmsSigBToD0Tag)) goto Error;
        if (!CheckMPE(Pass, h, cmsSigBToD1Tag)) goto Error;
        if (!CheckMPE(Pass, h, cmsSigBToD2Tag)) goto Error;
        if (!CheckMPE(Pass, h, cmsSigBToD3Tag)) goto Error;

        SubTest("Tags using screening");
        if (!CheckScreening(Pass, h, cmsSigScreeningTag)) goto Error;

        SubTest("Tags holding profile sequence description");
        if (!CheckProfileSequenceTag(Pass, h)) goto Error;
        if (!CheckProfileSequenceIDTag(Pass, h)) goto Error;

        SubTest("Tags holding ICC viewing conditions");
        if (!CheckICCViewingConditions(Pass, h)) goto Error;

        SubTest("VCGT tags");
        if (!CheckVCGT(Pass, h)) goto Error;

        SubTest("RAW tags");
        if (!CheckRAWtags(Pass, h)) goto Error;

        SubTest("Dictionary meta tags");
        // if (!CheckDictionary16(Pass, h)) goto Error;
        if (!CheckDictionary24(Pass, h)) goto Error;

        if (Pass == 1) {
            cmsSaveProfileToFile(DbgThread(), h, "alltags.icc");
            cmsCloseProfile(DbgThread(), h);
            h = cmsOpenProfileFromFile(DbgThread(), "alltags.icc", "r");
        }

    }

    /*
    Not implemented (by design):

    cmsSigDataTag                           = 0x64617461,  // 'data'  -- Unused
    cmsSigDeviceSettingsTag                 = 0x64657673,  // 'devs'  -- Unused
    cmsSigNamedColorTag                     = 0x6E636f6C,  // 'ncol'  -- Don't use this one, deprecated by ICC
    cmsSigOutputResponseTag                 = 0x72657370,  // 'resp'  -- Possible patent on this
    */

    cmsCloseProfile(DbgThread(), h);
    remove("alltags.icc");
    return 1;

Error:
    cmsCloseProfile(DbgThread(), h);
    remove("alltags.icc");
    return 0;
}


// Thanks to Christopher James Halse Rogers for the bugfixing and providing this test
static
cmsInt32Number CheckVersionHeaderWriting(void)
{
    cmsHPROFILE h;
    int index;
    float test_versions[] = {
      2.3f,
      4.08f,
      4.09f,
      4.3f
    };

    for (index = 0; index < sizeof(test_versions)/sizeof(test_versions[0]); index++) {

      h = cmsCreateProfilePlaceholder(DbgThread());
      if (h == NULL) return 0;

      cmsSetProfileVersion(DbgThread(), h, test_versions[index]);

      cmsSaveProfileToFile(DbgThread(), h, "versions.icc");
      cmsCloseProfile(DbgThread(), h);

      h = cmsOpenProfileFromFile(DbgThread(), "versions.icc", "r");

      // Only the first 3 digits are significant
      if (fabs(cmsGetProfileVersion(DbgThread(), h) - test_versions[index]) > 0.005) {
        Fail("Version failed to round-trip: wrote %.2f, read %.2f",
             test_versions[index], cmsGetProfileVersion(DbgThread(), h));
        return 0;
      }

      cmsCloseProfile(DbgThread(), h);
      remove("versions.icc");
    }
    return 1;
}


// Test on Richard Hughes "crayons.icc"
static
cmsInt32Number CheckMultilocalizedProfile(void)
{
    cmsHPROFILE hProfile;
    cmsMLU *Pt;
    char Buffer[256];

    hProfile = cmsOpenProfileFromFile(DbgThread(), "crayons.icc", "r");

    Pt = (cmsMLU *) cmsReadTag(DbgThread(), hProfile, cmsSigProfileDescriptionTag);
    cmsMLUgetASCII(DbgThread(), Pt, "en", "GB", Buffer, 256);
    if (strcmp(Buffer, "Crayon Colours") != 0) return FALSE;
    cmsMLUgetASCII(DbgThread(), Pt, "en", "US", Buffer, 256);
    if (strcmp(Buffer, "Crayon Colors") != 0) return FALSE;

    cmsCloseProfile(DbgThread(), hProfile);

    return TRUE;
}


// Error reporting  -------------------------------------------------------------------------------------------------------


static
void ErrorReportingFunction(cmsContext ContextID, cmsUInt32Number ErrorCode, const char *Text)
{
    TrappedError = TRUE;
    SimultaneousErrors++;
    strncpy(ReasonToFailBuffer, Text, TEXT_ERROR_BUFFER_SIZE-1);

    cmsUNUSED_PARAMETER(ContextID);
    cmsUNUSED_PARAMETER(ErrorCode);
}


static
cmsInt32Number CheckBadProfiles(void)
{
    cmsHPROFILE h;

    h = cmsOpenProfileFromFile(DbgThread(), "IDoNotExist.icc", "r");
    if (h != NULL) {
        cmsCloseProfile(DbgThread(), h);
        return 0;
    }

    h = cmsOpenProfileFromFile(DbgThread(), "IAmIllFormed*.icc", "r");
    if (h != NULL) {
        cmsCloseProfile(DbgThread(), h);
        return 0;
    }

    // No profile name given
    h = cmsOpenProfileFromFile(DbgThread(), "", "r");
    if (h != NULL) {
        cmsCloseProfile(DbgThread(), h);
        return 0;
    }

    h = cmsOpenProfileFromFile(DbgThread(), "..", "r");
    if (h != NULL) {
        cmsCloseProfile(DbgThread(), h);
        return 0;
    }

    h = cmsOpenProfileFromFile(DbgThread(), "IHaveBadAccessMode.icc", "@");
    if (h != NULL) {
        cmsCloseProfile(DbgThread(), h);
        return 0;
    }

    h = cmsOpenProfileFromFile(DbgThread(), "bad.icc", "r");
    if (h != NULL) {
        cmsCloseProfile(DbgThread(), h);
        return 0;
    }

     h = cmsOpenProfileFromFile(DbgThread(), "toosmall.icc", "r");
    if (h != NULL) {
        cmsCloseProfile(DbgThread(), h);
        return 0;
    }

    h = cmsOpenProfileFromMem(DbgThread(), NULL, 3);
    if (h != NULL) {
        cmsCloseProfile(DbgThread(), h);
        return 0;
    }

    h = cmsOpenProfileFromMem(DbgThread(), "123", 3);
    if (h != NULL) {
        cmsCloseProfile(DbgThread(), h);
        return 0;
    }

    if (SimultaneousErrors != 9) return 0;

    return 1;
}


static
cmsInt32Number CheckErrReportingOnBadProfiles(void)
{
    cmsInt32Number rc;

    cmsSetLogErrorHandler(DbgThread(), ErrorReportingFunction);
    rc = CheckBadProfiles();
    cmsSetLogErrorHandler(DbgThread(), FatalErrorQuit);

    // Reset the error state
    TrappedError = FALSE;
    return rc;
}


static
cmsInt32Number CheckBadTransforms(void)
{
    cmsHPROFILE h1 = cmsCreate_sRGBProfile(DbgThread());
    cmsHTRANSFORM x1;

    x1 = cmsCreateTransform(DbgThread(), NULL, 0, NULL, 0, 0, 0);
    if (x1 != NULL) {
        cmsDeleteTransform(DbgThread(), x1);
        return 0;
    }



    x1 = cmsCreateTransform(DbgThread(), h1, TYPE_RGB_8, h1, TYPE_RGB_8, 12345, 0);
    if (x1 != NULL) {
        cmsDeleteTransform(DbgThread(), x1);
        return 0;
    }

    x1 = cmsCreateTransform(DbgThread(), h1, TYPE_CMYK_8, h1, TYPE_RGB_8, 0, 0);
    if (x1 != NULL) {
        cmsDeleteTransform(DbgThread(), x1);
        return 0;
    }

    x1 = cmsCreateTransform(DbgThread(), h1, TYPE_RGB_8, h1, TYPE_CMYK_8, 1, 0);
    if (x1 != NULL) {
        cmsDeleteTransform(DbgThread(), x1);
        return 0;
    }

    // sRGB does its output as XYZ!
    x1 = cmsCreateTransform(DbgThread(), h1, TYPE_RGB_8, NULL, TYPE_Lab_8, 1, 0);
    if (x1 != NULL) {
        cmsDeleteTransform(DbgThread(), x1);
        return 0;
    }

    cmsCloseProfile(DbgThread(), h1);


    {

    cmsHPROFILE hp1 = cmsOpenProfileFromFile(DbgThread(),  "test1.icc", "r");
    cmsHPROFILE hp2 = cmsCreate_sRGBProfile(DbgThread());

    x1 = cmsCreateTransform(DbgThread(), hp1, TYPE_BGR_8, hp2, TYPE_BGR_8, INTENT_PERCEPTUAL, 0);

    cmsCloseProfile(DbgThread(), hp1); cmsCloseProfile(DbgThread(), hp2);
    if (x1 != NULL) {
        cmsDeleteTransform(DbgThread(), x1);
        return 0;
    }
    }

    return 1;

}

static
cmsInt32Number CheckErrReportingOnBadTransforms(void)
{
    cmsInt32Number rc;

    cmsSetLogErrorHandler(DbgThread(), ErrorReportingFunction);
    rc = CheckBadTransforms();
    cmsSetLogErrorHandler(DbgThread(), FatalErrorQuit);

    // Reset the error state
    TrappedError = FALSE;
    return rc;
}




// ---------------------------------------------------------------------------------------------------------

// Check a linear xform
static
cmsInt32Number Check8linearXFORM(cmsHTRANSFORM xform, cmsInt32Number nChan)
{
    cmsInt32Number n2, i, j;
    cmsUInt8Number Inw[cmsMAXCHANNELS], Outw[cmsMAXCHANNELS];

    n2=0;

    for (j=0; j < 0xFF; j++) {

        memset(Inw, j, sizeof(Inw));
        cmsDoTransform(DbgThread(), xform, Inw, Outw, 1);

        for (i=0; i < nChan; i++) {

           cmsInt32Number dif = abs(Outw[i] - j);
           if (dif > n2) n2 = dif;

        }
    }

   // We allow 2 contone of difference on 8 bits
    if (n2 > 2) {

        Fail("Differences too big (%x)", n2);
        return 0;
    }

    return 1;
}

static
cmsInt32Number Compare8bitXFORM(cmsHTRANSFORM xform1, cmsHTRANSFORM xform2, cmsInt32Number nChan)
{
    cmsInt32Number n2, i, j;
    cmsUInt8Number Inw[cmsMAXCHANNELS], Outw1[cmsMAXCHANNELS], Outw2[cmsMAXCHANNELS];;

    n2=0;

    for (j=0; j < 0xFF; j++) {

        memset(Inw, j, sizeof(Inw));
        cmsDoTransform(DbgThread(), xform1, Inw, Outw1, 1);
        cmsDoTransform(DbgThread(), xform2, Inw, Outw2, 1);

        for (i=0; i < nChan; i++) {

           cmsInt32Number dif = abs(Outw2[i] - Outw1[i]);
           if (dif > n2) n2 = dif;

        }
    }

   // We allow 2 contone of difference on 8 bits
    if (n2 > 2) {

        Fail("Differences too big (%x)", n2);
        return 0;
    }


    return 1;
}


// Check a linear xform
static
cmsInt32Number Check16linearXFORM(cmsHTRANSFORM xform, cmsInt32Number nChan)
{
    cmsInt32Number n2, i, j;
    cmsUInt16Number Inw[cmsMAXCHANNELS], Outw[cmsMAXCHANNELS];

    n2=0;
    for (j=0; j < 0xFFFF; j++) {

        for (i=0; i < nChan; i++) Inw[i] = (cmsUInt16Number) j;

        cmsDoTransform(DbgThread(), xform, Inw, Outw, 1);

        for (i=0; i < nChan; i++) {

           cmsInt32Number dif = abs(Outw[i] - j);
           if (dif > n2) n2 = dif;

        }


   // We allow 2 contone of difference on 16 bits
    if (n2 > 0x200) {

        Fail("Differences too big (%x)", n2);
        return 0;
    }
    }

    return 1;
}

static
cmsInt32Number Compare16bitXFORM(cmsHTRANSFORM xform1, cmsHTRANSFORM xform2, cmsInt32Number nChan)
{
    cmsInt32Number n2, i, j;
    cmsUInt16Number Inw[cmsMAXCHANNELS], Outw1[cmsMAXCHANNELS], Outw2[cmsMAXCHANNELS];;

    n2=0;

    for (j=0; j < 0xFFFF; j++) {

        for (i=0; i < nChan; i++) Inw[i] = (cmsUInt16Number) j;

        cmsDoTransform(DbgThread(), xform1, Inw, Outw1, 1);
        cmsDoTransform(DbgThread(), xform2, Inw, Outw2, 1);

        for (i=0; i < nChan; i++) {

           cmsInt32Number dif = abs(Outw2[i] - Outw1[i]);
           if (dif > n2) n2 = dif;

        }
    }

   // We allow 2 contone of difference on 16 bits
    if (n2 > 0x200) {

        Fail("Differences too big (%x)", n2);
        return 0;
    }


    return 1;
}


// Check a linear xform
static
cmsInt32Number CheckFloatlinearXFORM(cmsHTRANSFORM xform, cmsInt32Number nChan)
{
    cmsInt32Number i, j;
    cmsFloat32Number In[cmsMAXCHANNELS], Out[cmsMAXCHANNELS];

    for (j=0; j < 0xFFFF; j++) {

        for (i=0; i < nChan; i++) In[i] = (cmsFloat32Number) (j / 65535.0);;

        cmsDoTransform(DbgThread(), xform, In, Out, 1);

        for (i=0; i < nChan; i++) {

           // We allow no difference in floating point
            if (!IsGoodFixed15_16("linear xform cmsFloat32Number", Out[i], (cmsFloat32Number) (j / 65535.0)))
                return 0;
        }
    }

    return 1;
}


// Check a linear xform
static
cmsInt32Number CompareFloatXFORM(cmsHTRANSFORM xform1, cmsHTRANSFORM xform2, cmsInt32Number nChan)
{
    cmsInt32Number i, j;
    cmsFloat32Number In[cmsMAXCHANNELS], Out1[cmsMAXCHANNELS], Out2[cmsMAXCHANNELS];

    for (j=0; j < 0xFFFF; j++) {

        for (i=0; i < nChan; i++) In[i] = (cmsFloat32Number) (j / 65535.0);;

        cmsDoTransform(DbgThread(), xform1, In, Out1, 1);
        cmsDoTransform(DbgThread(), xform2, In, Out2, 1);

        for (i=0; i < nChan; i++) {

           // We allow no difference in floating point
            if (!IsGoodFixed15_16("linear xform cmsFloat32Number", Out1[i], Out2[i]))
                return 0;
        }

    }

    return 1;
}


// Curves only transforms ----------------------------------------------------------------------------------------

static
cmsInt32Number CheckCurvesOnlyTransforms(void)
{

    cmsHTRANSFORM xform1, xform2;
    cmsHPROFILE h1, h2, h3;
    cmsToneCurve* c1, *c2, *c3;
    cmsInt32Number rc = 1;


    c1 = cmsBuildGamma(DbgThread(), 2.2);
    c2 = cmsBuildGamma(DbgThread(), 1/2.2);
    c3 = cmsBuildGamma(DbgThread(), 4.84);

    h1 = cmsCreateLinearizationDeviceLink(DbgThread(), cmsSigGrayData, &c1);
    h2 = cmsCreateLinearizationDeviceLink(DbgThread(), cmsSigGrayData, &c2);
    h3 = cmsCreateLinearizationDeviceLink(DbgThread(), cmsSigGrayData, &c3);

    SubTest("Gray float optimizeable transform");
    xform1 = cmsCreateTransform(DbgThread(), h1, TYPE_GRAY_FLT, h2, TYPE_GRAY_FLT, INTENT_PERCEPTUAL, 0);
    rc &= CheckFloatlinearXFORM(xform1, 1);
    cmsDeleteTransform(DbgThread(), xform1);
    if (rc == 0) goto Error;

    SubTest("Gray 8 optimizeable transform");
    xform1 = cmsCreateTransform(DbgThread(), h1, TYPE_GRAY_8, h2, TYPE_GRAY_8, INTENT_PERCEPTUAL, 0);
    rc &= Check8linearXFORM(xform1, 1);
    cmsDeleteTransform(DbgThread(), xform1);
    if (rc == 0) goto Error;

    SubTest("Gray 16 optimizeable transform");
    xform1 = cmsCreateTransform(DbgThread(), h1, TYPE_GRAY_16, h2, TYPE_GRAY_16, INTENT_PERCEPTUAL, 0);
    rc &= Check16linearXFORM(xform1, 1);
    cmsDeleteTransform(DbgThread(), xform1);
    if (rc == 0) goto Error;

    SubTest("Gray float non-optimizeable transform");
    xform1 = cmsCreateTransform(DbgThread(), h1, TYPE_GRAY_FLT, h1, TYPE_GRAY_FLT, INTENT_PERCEPTUAL, 0);
    xform2 = cmsCreateTransform(DbgThread(), h3, TYPE_GRAY_FLT, NULL, TYPE_GRAY_FLT, INTENT_PERCEPTUAL, 0);

    rc &= CompareFloatXFORM(xform1, xform2, 1);
    cmsDeleteTransform(DbgThread(), xform1);
    cmsDeleteTransform(DbgThread(), xform2);
    if (rc == 0) goto Error;

    SubTest("Gray 8 non-optimizeable transform");
    xform1 = cmsCreateTransform(DbgThread(), h1, TYPE_GRAY_8, h1, TYPE_GRAY_8, INTENT_PERCEPTUAL, 0);
    xform2 = cmsCreateTransform(DbgThread(), h3, TYPE_GRAY_8, NULL, TYPE_GRAY_8, INTENT_PERCEPTUAL, 0);

    rc &= Compare8bitXFORM(xform1, xform2, 1);
    cmsDeleteTransform(DbgThread(), xform1);
    cmsDeleteTransform(DbgThread(), xform2);
    if (rc == 0) goto Error;


    SubTest("Gray 16 non-optimizeable transform");
    xform1 = cmsCreateTransform(DbgThread(), h1, TYPE_GRAY_16, h1, TYPE_GRAY_16, INTENT_PERCEPTUAL, 0);
    xform2 = cmsCreateTransform(DbgThread(), h3, TYPE_GRAY_16, NULL, TYPE_GRAY_16, INTENT_PERCEPTUAL, 0);

    rc &= Compare16bitXFORM(xform1, xform2, 1);
    cmsDeleteTransform(DbgThread(), xform1);
    cmsDeleteTransform(DbgThread(), xform2);
    if (rc == 0) goto Error;

Error:

    cmsCloseProfile(DbgThread(), h1); cmsCloseProfile(DbgThread(), h2); cmsCloseProfile(DbgThread(), h3);
    cmsFreeToneCurve(DbgThread(), c1); cmsFreeToneCurve(DbgThread(), c2); cmsFreeToneCurve(DbgThread(), c3);

    return rc;
}



// Lab to Lab trivial transforms ----------------------------------------------------------------------------------------

static cmsFloat64Number MaxDE;

static
cmsInt32Number CheckOneLab(cmsHTRANSFORM xform, cmsFloat64Number L, cmsFloat64Number a, cmsFloat64Number b)
{
    cmsCIELab In, Out;
    cmsFloat64Number dE;

    In.L = L; In.a = a; In.b = b;
    cmsDoTransform(DbgThread(), xform, &In, &Out, 1);

    dE = cmsDeltaE(DbgThread(), &In, &Out);

    if (dE > MaxDE) MaxDE = dE;

    if (MaxDE >  0.003) {
        Fail("dE=%f Lab1=(%f, %f, %f)\n\tLab2=(%f %f %f)", MaxDE, In.L, In.a, In.b, Out.L, Out.a, Out.b);
        cmsDoTransform(DbgThread(), xform, &In, &Out, 1);
        return 0;
    }

    return 1;
}

// Check several Lab, slicing at non-exact values. Precision should be 16 bits. 50x50x50 checks aprox.
static
cmsInt32Number CheckSeveralLab(cmsHTRANSFORM xform)
{
    cmsInt32Number L, a, b;

    MaxDE = 0;
    for (L=0; L < 65536; L += 1311) {

        for (a = 0; a < 65536; a += 1232) {

            for (b = 0; b < 65536; b += 1111) {

                if (!CheckOneLab(xform, (L * 100.0) / 65535.0,
                                        (a  / 257.0) - 128, (b / 257.0) - 128))
                    return 0;
            }

        }

    }
    return 1;
}


static
cmsInt32Number OneTrivialLab(cmsHPROFILE hLab1, cmsHPROFILE hLab2, const char* txt)
{
    cmsHTRANSFORM xform;
    cmsInt32Number rc;

    SubTest(txt);
    xform = cmsCreateTransform(DbgThread(), hLab1, TYPE_Lab_DBL, hLab2, TYPE_Lab_DBL, INTENT_RELATIVE_COLORIMETRIC, 0);
    cmsCloseProfile(DbgThread(), hLab1); cmsCloseProfile(DbgThread(), hLab2);

    rc = CheckSeveralLab(xform);
    cmsDeleteTransform(DbgThread(), xform);
    return rc;
}


static
cmsInt32Number CheckFloatLabTransforms(void)
{
    return OneTrivialLab(cmsCreateLab4Profile(DbgThread(), NULL), cmsCreateLab4Profile(DbgThread(), NULL),  "Lab4/Lab4") &&
           OneTrivialLab(cmsCreateLab2Profile(DbgThread(), NULL), cmsCreateLab2Profile(DbgThread(), NULL),  "Lab2/Lab2") &&
           OneTrivialLab(cmsCreateLab4Profile(DbgThread(), NULL), cmsCreateLab2Profile(DbgThread(), NULL),  "Lab4/Lab2") &&
           OneTrivialLab(cmsCreateLab2Profile(DbgThread(), NULL), cmsCreateLab4Profile(DbgThread(), NULL),  "Lab2/Lab4");
}


static
cmsInt32Number CheckEncodedLabTransforms(void)
{
    cmsHTRANSFORM xform;
    cmsUInt16Number In[3];
    cmsCIELab Lab;
    cmsCIELab White = { 100, 0, 0 };
    cmsHPROFILE hLab1 = cmsCreateLab4Profile(DbgThread(), NULL);
    cmsHPROFILE hLab2 = cmsCreateLab4Profile(DbgThread(), NULL);


    xform = cmsCreateTransform(DbgThread(), hLab1, TYPE_Lab_16, hLab2, TYPE_Lab_DBL, INTENT_RELATIVE_COLORIMETRIC, 0);
    cmsCloseProfile(DbgThread(), hLab1); cmsCloseProfile(DbgThread(), hLab2);

    In[0] = 0xFFFF;
    In[1] = 0x8080;
    In[2] = 0x8080;

    cmsDoTransform(DbgThread(), xform, In, &Lab, 1);

    if (cmsDeltaE(DbgThread(), &Lab, &White) > 0.0001) return 0;
    cmsDeleteTransform(DbgThread(), xform);

    hLab1 = cmsCreateLab2Profile(DbgThread(), NULL);
    hLab2 = cmsCreateLab4Profile(DbgThread(), NULL);

    xform = cmsCreateTransform(DbgThread(), hLab1, TYPE_LabV2_16, hLab2, TYPE_Lab_DBL, INTENT_RELATIVE_COLORIMETRIC, 0);
    cmsCloseProfile(DbgThread(), hLab1); cmsCloseProfile(DbgThread(), hLab2);


    In[0] = 0xFF00;
    In[1] = 0x8000;
    In[2] = 0x8000;

    cmsDoTransform(DbgThread(), xform, In, &Lab, 1);

    if (cmsDeltaE(DbgThread(), &Lab, &White) > 0.0001) return 0;

    cmsDeleteTransform(DbgThread(), xform);

    hLab2 = cmsCreateLab2Profile(DbgThread(), NULL);
    hLab1 = cmsCreateLab4Profile(DbgThread(), NULL);

    xform = cmsCreateTransform(DbgThread(), hLab1, TYPE_Lab_DBL, hLab2, TYPE_LabV2_16, INTENT_RELATIVE_COLORIMETRIC, 0);
    cmsCloseProfile(DbgThread(), hLab1); cmsCloseProfile(DbgThread(), hLab2);

    Lab.L = 100;
    Lab.a = 0;
    Lab.b = 0;

    cmsDoTransform(DbgThread(), xform, &Lab, In, 1);
    if (In[0] != 0xFF00 ||
        In[1] != 0x8000 ||
        In[2] != 0x8000) return 0;

    cmsDeleteTransform(DbgThread(), xform);

    hLab1 = cmsCreateLab4Profile(DbgThread(), NULL);
    hLab2 = cmsCreateLab4Profile(DbgThread(), NULL);

    xform = cmsCreateTransform(DbgThread(), hLab1, TYPE_Lab_DBL, hLab2, TYPE_Lab_16, INTENT_RELATIVE_COLORIMETRIC, 0);
    cmsCloseProfile(DbgThread(), hLab1); cmsCloseProfile(DbgThread(), hLab2);

    Lab.L = 100;
    Lab.a = 0;
    Lab.b = 0;

    cmsDoTransform(DbgThread(), xform, &Lab, In, 1);

    if (In[0] != 0xFFFF ||
        In[1] != 0x8080 ||
        In[2] != 0x8080) return 0;

    cmsDeleteTransform(DbgThread(), xform);

    return 1;
}

static
cmsInt32Number CheckStoredIdentities(void)
{
    cmsHPROFILE hLab, hLink, h4, h2;
    cmsHTRANSFORM xform;
    cmsInt32Number rc = 1;

    hLab  = cmsCreateLab4Profile(DbgThread(), NULL);
    xform = cmsCreateTransform(DbgThread(), hLab, TYPE_Lab_8, hLab, TYPE_Lab_8, 0, 0);

    hLink = cmsTransform2DeviceLink(NULL, xform, 3.4, 0);
    cmsSaveProfileToFile(DbgThread(), hLink, "abstractv2.icc");
    cmsCloseProfile(DbgThread(), hLink);

    hLink = cmsTransform2DeviceLink(NULL, xform, 4.3, 0);
    cmsSaveProfileToFile(DbgThread(), hLink, "abstractv4.icc");
    cmsCloseProfile(DbgThread(), hLink);

    cmsDeleteTransform(DbgThread(), xform);
    cmsCloseProfile(DbgThread(), hLab);

    h4 = cmsOpenProfileFromFile(DbgThread(), "abstractv4.icc", "r");

    xform = cmsCreateTransform(DbgThread(), h4, TYPE_Lab_DBL, h4, TYPE_Lab_DBL, INTENT_RELATIVE_COLORIMETRIC, 0);

    SubTest("V4");
    rc &= CheckSeveralLab(xform);

    cmsDeleteTransform(DbgThread(), xform);
    cmsCloseProfile(DbgThread(), h4);
    if (!rc) goto Error;


    SubTest("V2");
    h2 = cmsOpenProfileFromFile(DbgThread(), "abstractv2.icc", "r");

    xform = cmsCreateTransform(DbgThread(), h2, TYPE_Lab_DBL, h2, TYPE_Lab_DBL, INTENT_RELATIVE_COLORIMETRIC, 0);
    rc &= CheckSeveralLab(xform);
    cmsDeleteTransform(DbgThread(), xform);
    cmsCloseProfile(DbgThread(), h2);
    if (!rc) goto Error;


    SubTest("V2 -> V4");
    h2 = cmsOpenProfileFromFile(DbgThread(), "abstractv2.icc", "r");
    h4 = cmsOpenProfileFromFile(DbgThread(), "abstractv4.icc", "r");

    xform = cmsCreateTransform(DbgThread(), h4, TYPE_Lab_DBL, h2, TYPE_Lab_DBL, INTENT_RELATIVE_COLORIMETRIC, 0);
    rc &= CheckSeveralLab(xform);
    cmsDeleteTransform(DbgThread(), xform);
    cmsCloseProfile(DbgThread(), h2);
    cmsCloseProfile(DbgThread(), h4);

    SubTest("V4 -> V2");
    h2 = cmsOpenProfileFromFile(DbgThread(), "abstractv2.icc", "r");
    h4 = cmsOpenProfileFromFile(DbgThread(), "abstractv4.icc", "r");

    xform = cmsCreateTransform(DbgThread(), h2, TYPE_Lab_DBL, h4, TYPE_Lab_DBL, INTENT_RELATIVE_COLORIMETRIC, 0);
    rc &= CheckSeveralLab(xform);
    cmsDeleteTransform(DbgThread(), xform);
    cmsCloseProfile(DbgThread(), h2);
    cmsCloseProfile(DbgThread(), h4);

Error:
    remove("abstractv2.icc");
    remove("abstractv4.icc");
    return rc;

}



// Check a simple xform from a matrix profile to itself. Test floating point accuracy.
static
cmsInt32Number CheckMatrixShaperXFORMFloat(void)
{
    cmsHPROFILE hAbove, hSRGB;
    cmsHTRANSFORM xform;
    cmsInt32Number rc1, rc2;

    hAbove = Create_AboveRGB();
    xform = cmsCreateTransform(DbgThread(), hAbove, TYPE_RGB_FLT, hAbove, TYPE_RGB_FLT,  INTENT_RELATIVE_COLORIMETRIC, 0);
    cmsCloseProfile(DbgThread(), hAbove);
    rc1 = CheckFloatlinearXFORM(xform, 3);
    cmsDeleteTransform(DbgThread(), xform);

    hSRGB = cmsCreate_sRGBProfile(DbgThread());
    xform = cmsCreateTransform(DbgThread(), hSRGB, TYPE_RGB_FLT, hSRGB, TYPE_RGB_FLT,  INTENT_RELATIVE_COLORIMETRIC, 0);
    cmsCloseProfile(DbgThread(), hSRGB);
    rc2 = CheckFloatlinearXFORM(xform, 3);
    cmsDeleteTransform(DbgThread(), xform);


    return rc1 && rc2;
}

// Check a simple xform from a matrix profile to itself. Test 16 bits accuracy.
static
cmsInt32Number CheckMatrixShaperXFORM16(void)
{
    cmsHPROFILE hAbove, hSRGB;
    cmsHTRANSFORM xform;
    cmsInt32Number rc1, rc2;

    hAbove = Create_AboveRGB();
    xform = cmsCreateTransform(DbgThread(), hAbove, TYPE_RGB_16, hAbove, TYPE_RGB_16,  INTENT_RELATIVE_COLORIMETRIC, 0);
    cmsCloseProfile(DbgThread(), hAbove);

    rc1 = Check16linearXFORM(xform, 3);
    cmsDeleteTransform(DbgThread(), xform);

    hSRGB = cmsCreate_sRGBProfile(DbgThread());
    xform = cmsCreateTransform(DbgThread(), hSRGB, TYPE_RGB_16, hSRGB, TYPE_RGB_16,  INTENT_RELATIVE_COLORIMETRIC, 0);
    cmsCloseProfile(DbgThread(), hSRGB);
    rc2 = Check16linearXFORM(xform, 3);
    cmsDeleteTransform(DbgThread(), xform);

    return rc1 && rc2;

}


// Check a simple xform from a matrix profile to itself. Test 8 bits accuracy.
static
cmsInt32Number CheckMatrixShaperXFORM8(void)
{
    cmsHPROFILE hAbove, hSRGB;
    cmsHTRANSFORM xform;
    cmsInt32Number rc1, rc2;

    hAbove = Create_AboveRGB();
    xform = cmsCreateTransform(DbgThread(), hAbove, TYPE_RGB_8, hAbove, TYPE_RGB_8,  INTENT_RELATIVE_COLORIMETRIC, 0);
    cmsCloseProfile(DbgThread(), hAbove);
    rc1 = Check8linearXFORM(xform, 3);
    cmsDeleteTransform(DbgThread(), xform);

    hSRGB = cmsCreate_sRGBProfile(DbgThread());
    xform = cmsCreateTransform(DbgThread(), hSRGB, TYPE_RGB_8, hSRGB, TYPE_RGB_8,  INTENT_RELATIVE_COLORIMETRIC, 0);
    cmsCloseProfile(DbgThread(), hSRGB);
    rc2 = Check8linearXFORM(xform, 3);
    cmsDeleteTransform(DbgThread(), xform);


    return rc1 && rc2;
}


// TODO: Check LUT based to LUT based transforms for CMYK






// -----------------------------------------------------------------------------------------------------------------


// Check known values going from sRGB to XYZ
static
cmsInt32Number CheckOneRGB_f(cmsHTRANSFORM xform, cmsInt32Number R, cmsInt32Number G, cmsInt32Number B, cmsFloat64Number X, cmsFloat64Number Y, cmsFloat64Number Z, cmsFloat64Number err)
{
    cmsFloat32Number RGB[3];
    cmsFloat64Number Out[3];

    RGB[0] = (cmsFloat32Number) (R / 255.0);
    RGB[1] = (cmsFloat32Number) (G / 255.0);
    RGB[2] = (cmsFloat32Number) (B / 255.0);

    cmsDoTransform(DbgThread(), xform, RGB, Out, 1);

    return IsGoodVal("X", X , Out[0], err) &&
           IsGoodVal("Y", Y , Out[1], err) &&
           IsGoodVal("Z", Z , Out[2], err);
}

static
cmsInt32Number Chack_sRGB_Float(void)
{
    cmsHPROFILE hsRGB, hXYZ, hLab;
    cmsHTRANSFORM xform1, xform2;
    cmsInt32Number rc;


    hsRGB = cmsCreate_sRGBProfile(DbgThread());
    hXYZ  = cmsCreateXYZProfile(DbgThread());
    hLab  = cmsCreateLab4Profile(DbgThread(), NULL);

    xform1 =  cmsCreateTransform(DbgThread(), hsRGB, TYPE_RGB_FLT, hXYZ, TYPE_XYZ_DBL,
                                INTENT_RELATIVE_COLORIMETRIC, 0);

    xform2 =  cmsCreateTransform(DbgThread(), hsRGB, TYPE_RGB_FLT, hLab, TYPE_Lab_DBL,
                                INTENT_RELATIVE_COLORIMETRIC, 0);
    cmsCloseProfile(DbgThread(), hsRGB);
    cmsCloseProfile(DbgThread(), hXYZ);
    cmsCloseProfile(DbgThread(), hLab);

    MaxErr = 0;

    // Xform 1 goes from 8 bits to XYZ,
    rc  = CheckOneRGB_f(xform1, 1, 1, 1,        0.0002927, 0.0003035,  0.000250,  0.0001);
    rc  &= CheckOneRGB_f(xform1, 127, 127, 127, 0.2046329, 0.212230,   0.175069,  0.0001);
    rc  &= CheckOneRGB_f(xform1, 12, 13, 15,    0.0038364, 0.0039928,  0.003853,  0.0001);
    rc  &= CheckOneRGB_f(xform1, 128, 0, 0,     0.0941240, 0.0480256,  0.003005,  0.0001);
    rc  &= CheckOneRGB_f(xform1, 190, 25, 210,  0.3204592, 0.1605926,  0.468213,  0.0001);

    // Xform 2 goes from 8 bits to Lab, we allow 0.01 error max
    rc  &= CheckOneRGB_f(xform2, 1, 1, 1,       0.2741748, 0, 0,                   0.01);
    rc  &= CheckOneRGB_f(xform2, 127, 127, 127, 53.192776, 0, 0,                   0.01);
    rc  &= CheckOneRGB_f(xform2, 190, 25, 210,  47.052136, 74.565610, -56.883274,  0.01);
    rc  &= CheckOneRGB_f(xform2, 128, 0, 0,     26.164701, 48.478171, 39.4384713,  0.01);

    cmsDeleteTransform(DbgThread(), xform1);
    cmsDeleteTransform(DbgThread(), xform2);
    return rc;
}


// ---------------------------------------------------

static
cmsBool GetProfileRGBPrimaries(cmsHPROFILE hProfile,
                                cmsCIEXYZTRIPLE *result,
                                cmsUInt32Number intent)
{
    cmsHPROFILE hXYZ;
    cmsHTRANSFORM hTransform;
    cmsFloat64Number rgb[3][3] = {{1., 0., 0.},
    {0., 1., 0.},
    {0., 0., 1.}};

    hXYZ = cmsCreateXYZProfile(DbgThread());
    if (hXYZ == NULL) return FALSE;

    hTransform = cmsCreateTransform(DbgThread(), hProfile, TYPE_RGB_DBL, hXYZ, TYPE_XYZ_DBL,
        intent, cmsFLAGS_NOCACHE | cmsFLAGS_NOOPTIMIZE);
    cmsCloseProfile(DbgThread(), hXYZ);
    if (hTransform == NULL) return FALSE;

    cmsDoTransform(DbgThread(), hTransform, rgb, result, 3);
    cmsDeleteTransform(DbgThread(), hTransform);
    return TRUE;
}


static
int CheckRGBPrimaries(void)
{
    cmsHPROFILE hsRGB;
    cmsCIEXYZTRIPLE tripXYZ;
    cmsCIExyYTRIPLE tripxyY;
    cmsBool result;

    cmsSetAdaptationState(DbgThread(), 0);
    hsRGB = cmsCreate_sRGBProfile(DbgThread());
    if (!hsRGB) return 0;

    result = GetProfileRGBPrimaries(hsRGB, &tripXYZ,
        INTENT_ABSOLUTE_COLORIMETRIC);

    cmsCloseProfile(DbgThread(), hsRGB);
    if (!result) return 0;

    cmsXYZ2xyY(DbgThread(), &tripxyY.Red, &tripXYZ.Red);
    cmsXYZ2xyY(DbgThread(), &tripxyY.Green, &tripXYZ.Green);
    cmsXYZ2xyY(DbgThread(), &tripxyY.Blue, &tripXYZ.Blue);

    /* valus were taken from
    http://en.wikipedia.org/wiki/RGB_color_spaces#Specifications */

    if (!IsGoodFixed15_16("xRed", tripxyY.Red.x, 0.64) ||
        !IsGoodFixed15_16("yRed", tripxyY.Red.y, 0.33) ||
        !IsGoodFixed15_16("xGreen", tripxyY.Green.x, 0.30) ||
        !IsGoodFixed15_16("yGreen", tripxyY.Green.y, 0.60) ||
        !IsGoodFixed15_16("xBlue", tripxyY.Blue.x, 0.15) ||
        !IsGoodFixed15_16("yBlue", tripxyY.Blue.y, 0.06)) {
            Fail("One or more primaries are wrong.");
            return FALSE;
    }

    return TRUE;
}


// -----------------------------------------------------------------------------------------------------------------

// This function will check CMYK -> CMYK transforms. It uses FOGRA29 and SWOP ICC profiles

static
cmsInt32Number CheckCMYK(cmsInt32Number Intent, const char *Profile1, const char* Profile2)
{
    cmsHPROFILE hSWOP  = cmsOpenProfileFromFile(DbgThread(), Profile1, "r");
    cmsHPROFILE hFOGRA = cmsOpenProfileFromFile(DbgThread(), Profile2, "r");
    cmsHTRANSFORM xform, swop_lab, fogra_lab;
    cmsFloat32Number CMYK1[4], CMYK2[4];
    cmsCIELab Lab1, Lab2;
    cmsHPROFILE hLab;
    cmsFloat64Number DeltaL, Max;
    cmsInt32Number i;

    hLab = cmsCreateLab4Profile(DbgThread(), NULL);

    xform = cmsCreateTransform(DbgThread(), hSWOP, TYPE_CMYK_FLT, hFOGRA, TYPE_CMYK_FLT, Intent, 0);

    swop_lab = cmsCreateTransform(DbgThread(), hSWOP,   TYPE_CMYK_FLT, hLab, TYPE_Lab_DBL, Intent, 0);
    fogra_lab = cmsCreateTransform(DbgThread(), hFOGRA, TYPE_CMYK_FLT, hLab, TYPE_Lab_DBL, Intent, 0);

    Max = 0;
    for (i=0; i <= 100; i++) {

        CMYK1[0] = 10;
        CMYK1[1] = 20;
        CMYK1[2] = 30;
        CMYK1[3] = (cmsFloat32Number) i;

        cmsDoTransform(DbgThread(), swop_lab, CMYK1, &Lab1, 1);
        cmsDoTransform(DbgThread(), xform, CMYK1, CMYK2, 1);
        cmsDoTransform(DbgThread(), fogra_lab, CMYK2, &Lab2, 1);

        DeltaL = fabs(Lab1.L - Lab2.L);

        if (DeltaL > Max) Max = DeltaL;
    }


    cmsDeleteTransform(DbgThread(), xform);


    xform = cmsCreateTransform(DbgThread(),  hFOGRA, TYPE_CMYK_FLT, hSWOP, TYPE_CMYK_FLT, Intent, 0);

    for (i=0; i <= 100; i++) {
        CMYK1[0] = 10;
        CMYK1[1] = 20;
        CMYK1[2] = 30;
        CMYK1[3] = (cmsFloat32Number) i;

        cmsDoTransform(DbgThread(), fogra_lab, CMYK1, &Lab1, 1);
        cmsDoTransform(DbgThread(), xform, CMYK1, CMYK2, 1);
        cmsDoTransform(DbgThread(), swop_lab, CMYK2, &Lab2, 1);

        DeltaL = fabs(Lab1.L - Lab2.L);

        if (DeltaL > Max) Max = DeltaL;
    }


    cmsCloseProfile(DbgThread(), hSWOP);
    cmsCloseProfile(DbgThread(), hFOGRA);
    cmsCloseProfile(DbgThread(), hLab);

    cmsDeleteTransform(DbgThread(), xform);
    cmsDeleteTransform(DbgThread(), swop_lab);
    cmsDeleteTransform(DbgThread(), fogra_lab);

    return Max < 3.0;
}

static
cmsInt32Number CheckCMYKRoundtrip(void)
{
    return CheckCMYK(INTENT_RELATIVE_COLORIMETRIC, "test1.icc", "test1.icc");
}


static
cmsInt32Number CheckCMYKPerceptual(void)
{
    return CheckCMYK(INTENT_PERCEPTUAL, "test1.icc", "test2.icc");
}



static
cmsInt32Number CheckCMYKRelCol(void)
{
    return CheckCMYK(INTENT_RELATIVE_COLORIMETRIC, "test1.icc", "test2.icc");
}



static
cmsInt32Number CheckKOnlyBlackPreserving(void)
{
    cmsHPROFILE hSWOP  = cmsOpenProfileFromFile(DbgThread(), "test1.icc", "r");
    cmsHPROFILE hFOGRA = cmsOpenProfileFromFile(DbgThread(), "test2.icc", "r");
    cmsHTRANSFORM xform, swop_lab, fogra_lab;
    cmsFloat32Number CMYK1[4], CMYK2[4];
    cmsCIELab Lab1, Lab2;
    cmsHPROFILE hLab;
    cmsFloat64Number DeltaL, Max;
    cmsInt32Number i;

    hLab = cmsCreateLab4Profile(DbgThread(), NULL);

    xform = cmsCreateTransform(DbgThread(), hSWOP, TYPE_CMYK_FLT, hFOGRA, TYPE_CMYK_FLT, INTENT_PRESERVE_K_ONLY_PERCEPTUAL, 0);

    swop_lab = cmsCreateTransform(DbgThread(), hSWOP,   TYPE_CMYK_FLT, hLab, TYPE_Lab_DBL, INTENT_PERCEPTUAL, 0);
    fogra_lab = cmsCreateTransform(DbgThread(), hFOGRA, TYPE_CMYK_FLT, hLab, TYPE_Lab_DBL, INTENT_PERCEPTUAL, 0);

    Max = 0;

    for (i=0; i <= 100; i++) {
        CMYK1[0] = 0;
        CMYK1[1] = 0;
        CMYK1[2] = 0;
        CMYK1[3] = (cmsFloat32Number) i;

        // SWOP CMYK to Lab1
        cmsDoTransform(DbgThread(), swop_lab, CMYK1, &Lab1, 1);

        // SWOP To FOGRA using black preservation
        cmsDoTransform(DbgThread(), xform, CMYK1, CMYK2, 1);

        // Obtained FOGRA CMYK to Lab2
        cmsDoTransform(DbgThread(), fogra_lab, CMYK2, &Lab2, 1);

        // We care only on L*
        DeltaL = fabs(Lab1.L - Lab2.L);

        if (DeltaL > Max) Max = DeltaL;
    }


    cmsDeleteTransform(DbgThread(), xform);

    // dL should be below 3.0


    // Same, but FOGRA to SWOP
    xform = cmsCreateTransform(DbgThread(), hFOGRA, TYPE_CMYK_FLT, hSWOP, TYPE_CMYK_FLT, INTENT_PRESERVE_K_ONLY_PERCEPTUAL, 0);

    for (i=0; i <= 100; i++) {
        CMYK1[0] = 0;
        CMYK1[1] = 0;
        CMYK1[2] = 0;
        CMYK1[3] = (cmsFloat32Number) i;

        cmsDoTransform(DbgThread(), fogra_lab, CMYK1, &Lab1, 1);
        cmsDoTransform(DbgThread(), xform, CMYK1, CMYK2, 1);
        cmsDoTransform(DbgThread(), swop_lab, CMYK2, &Lab2, 1);

        DeltaL = fabs(Lab1.L - Lab2.L);

        if (DeltaL > Max) Max = DeltaL;
    }


    cmsCloseProfile(DbgThread(), hSWOP);
    cmsCloseProfile(DbgThread(), hFOGRA);
    cmsCloseProfile(DbgThread(), hLab);

    cmsDeleteTransform(DbgThread(), xform);
    cmsDeleteTransform(DbgThread(), swop_lab);
    cmsDeleteTransform(DbgThread(), fogra_lab);

    return Max < 3.0;
}

static
cmsInt32Number CheckKPlaneBlackPreserving(void)
{
    cmsHPROFILE hSWOP  = cmsOpenProfileFromFile(DbgThread(), "test1.icc", "r");
    cmsHPROFILE hFOGRA = cmsOpenProfileFromFile(DbgThread(), "test2.icc", "r");
    cmsHTRANSFORM xform, swop_lab, fogra_lab;
    cmsFloat32Number CMYK1[4], CMYK2[4];
    cmsCIELab Lab1, Lab2;
    cmsHPROFILE hLab;
    cmsFloat64Number DeltaE, Max;
    cmsInt32Number i;

    hLab = cmsCreateLab4Profile(DbgThread(), NULL);

    xform = cmsCreateTransform(DbgThread(), hSWOP, TYPE_CMYK_FLT, hFOGRA, TYPE_CMYK_FLT, INTENT_PERCEPTUAL, 0);

    swop_lab = cmsCreateTransform(DbgThread(), hSWOP,  TYPE_CMYK_FLT, hLab, TYPE_Lab_DBL, INTENT_PERCEPTUAL, 0);
    fogra_lab = cmsCreateTransform(DbgThread(), hFOGRA, TYPE_CMYK_FLT, hLab, TYPE_Lab_DBL, INTENT_PERCEPTUAL, 0);

    Max = 0;

    for (i=0; i <= 100; i++) {
        CMYK1[0] = 0;
        CMYK1[1] = 0;
        CMYK1[2] = 0;
        CMYK1[3] = (cmsFloat32Number) i;

        cmsDoTransform(DbgThread(), swop_lab, CMYK1, &Lab1, 1);
        cmsDoTransform(DbgThread(), xform, CMYK1, CMYK2, 1);
        cmsDoTransform(DbgThread(), fogra_lab, CMYK2, &Lab2, 1);

        DeltaE = cmsDeltaE(DbgThread(), &Lab1, &Lab2);

        if (DeltaE > Max) Max = DeltaE;
    }


    cmsDeleteTransform(DbgThread(), xform);

    xform = cmsCreateTransform(DbgThread(),  hFOGRA, TYPE_CMYK_FLT, hSWOP, TYPE_CMYK_FLT, INTENT_PRESERVE_K_PLANE_PERCEPTUAL, 0);

    for (i=0; i <= 100; i++) {
        CMYK1[0] = 30;
        CMYK1[1] = 20;
        CMYK1[2] = 10;
        CMYK1[3] = (cmsFloat32Number) i;

        cmsDoTransform(DbgThread(), fogra_lab, CMYK1, &Lab1, 1);
        cmsDoTransform(DbgThread(), xform, CMYK1, CMYK2, 1);
        cmsDoTransform(DbgThread(), swop_lab, CMYK2, &Lab2, 1);

        DeltaE = cmsDeltaE(DbgThread(), &Lab1, &Lab2);

        if (DeltaE > Max) Max = DeltaE;
    }

    cmsDeleteTransform(DbgThread(), xform);



    cmsCloseProfile(DbgThread(), hSWOP);
    cmsCloseProfile(DbgThread(), hFOGRA);
    cmsCloseProfile(DbgThread(), hLab);


    cmsDeleteTransform(DbgThread(), swop_lab);
    cmsDeleteTransform(DbgThread(), fogra_lab);

    return Max < 30.0;
}


// ------------------------------------------------------------------------------------------------------


static
cmsInt32Number CheckProofingXFORMFloat(void)
{
    cmsHPROFILE hAbove;
    cmsHTRANSFORM xform;
    cmsInt32Number rc;

    hAbove = Create_AboveRGB();
    xform =  cmsCreateProofingTransform(DbgThread(), hAbove, TYPE_RGB_FLT, hAbove, TYPE_RGB_FLT, hAbove,
                                INTENT_RELATIVE_COLORIMETRIC, INTENT_RELATIVE_COLORIMETRIC, cmsFLAGS_SOFTPROOFING);
    cmsCloseProfile(DbgThread(), hAbove);
    rc = CheckFloatlinearXFORM(xform, 3);
    cmsDeleteTransform(DbgThread(), xform);
    return rc;
}

static
cmsInt32Number CheckProofingXFORM16(void)
{
    cmsHPROFILE hAbove;
    cmsHTRANSFORM xform;
    cmsInt32Number rc;

    hAbove = Create_AboveRGB();
    xform =  cmsCreateProofingTransform(DbgThread(), hAbove, TYPE_RGB_16, hAbove, TYPE_RGB_16, hAbove,
                                INTENT_RELATIVE_COLORIMETRIC, INTENT_RELATIVE_COLORIMETRIC, cmsFLAGS_SOFTPROOFING|cmsFLAGS_NOCACHE);
    cmsCloseProfile(DbgThread(), hAbove);
    rc = Check16linearXFORM(xform, 3);
    cmsDeleteTransform(DbgThread(), xform);
    return rc;
}


static
cmsInt32Number CheckGamutCheck(void)
{
        cmsHPROFILE hSRGB, hAbove;
        cmsHTRANSFORM xform;
        cmsInt32Number rc;
        cmsUInt16Number Alarm[16] = { 0xDEAD, 0xBABE, 0xFACE };

        // Set alarm codes to fancy values so we could check the out of gamut condition
        cmsSetAlarmCodes(DbgThread(), Alarm);

        // Create the profiles
        hSRGB  = cmsCreate_sRGBProfile(DbgThread());
        hAbove = Create_AboveRGB();

        if (hSRGB == NULL || hAbove == NULL) return 0;  // Failed

        SubTest("Gamut check on floating point");

        // Create a gamut checker in the same space. No value should be out of gamut
        xform = cmsCreateProofingTransform(DbgThread(), hAbove, TYPE_RGB_FLT, hAbove, TYPE_RGB_FLT, hAbove,
                                INTENT_RELATIVE_COLORIMETRIC, INTENT_RELATIVE_COLORIMETRIC, cmsFLAGS_GAMUTCHECK);


        if (!CheckFloatlinearXFORM(xform, 3)) {
            cmsCloseProfile(DbgThread(), hSRGB);
            cmsCloseProfile(DbgThread(), hAbove);
            cmsDeleteTransform(DbgThread(), xform);
            Fail("Gamut check on same profile failed");
            return 0;
        }

        cmsDeleteTransform(DbgThread(), xform);

        SubTest("Gamut check on 16 bits");

        xform = cmsCreateProofingTransform(DbgThread(), hAbove, TYPE_RGB_16, hAbove, TYPE_RGB_16, hSRGB,
                                INTENT_RELATIVE_COLORIMETRIC, INTENT_RELATIVE_COLORIMETRIC, cmsFLAGS_GAMUTCHECK);

        cmsCloseProfile(DbgThread(), hSRGB);
        cmsCloseProfile(DbgThread(), hAbove);

        rc = Check16linearXFORM(xform, 3);

        cmsDeleteTransform(DbgThread(), xform);

        return rc;
}



// -------------------------------------------------------------------------------------------------------------------

static
cmsInt32Number CheckBlackPoint(void)
{
    cmsHPROFILE hProfile;
    cmsCIEXYZ Black;
    cmsCIELab Lab;

    hProfile  = cmsOpenProfileFromFile(DbgThread(), "test5.icc", "r");
    cmsDetectDestinationBlackPoint(DbgThread(), &Black, hProfile, INTENT_RELATIVE_COLORIMETRIC, 0);
    cmsCloseProfile(DbgThread(), hProfile);


    hProfile = cmsOpenProfileFromFile(DbgThread(), "test1.icc", "r");
    cmsDetectDestinationBlackPoint(DbgThread(), &Black, hProfile, INTENT_RELATIVE_COLORIMETRIC, 0);
    cmsXYZ2Lab(DbgThread(), NULL, &Lab, &Black);
    cmsCloseProfile(DbgThread(), hProfile);

    hProfile = cmsOpenProfileFromFile(DbgThread(), "lcms2cmyk.icc", "r");
    cmsDetectDestinationBlackPoint(DbgThread(), &Black, hProfile, INTENT_RELATIVE_COLORIMETRIC, 0);
    cmsXYZ2Lab(DbgThread(), NULL, &Lab, &Black);
    cmsCloseProfile(DbgThread(), hProfile);

    hProfile = cmsOpenProfileFromFile(DbgThread(), "test2.icc", "r");
    cmsDetectDestinationBlackPoint(DbgThread(), &Black, hProfile, INTENT_RELATIVE_COLORIMETRIC, 0);
    cmsXYZ2Lab(DbgThread(), NULL, &Lab, &Black);
    cmsCloseProfile(DbgThread(), hProfile);

    hProfile = cmsOpenProfileFromFile(DbgThread(), "test1.icc", "r");
    cmsDetectDestinationBlackPoint(DbgThread(), &Black, hProfile, INTENT_PERCEPTUAL, 0);
    cmsXYZ2Lab(DbgThread(), NULL, &Lab, &Black);
    cmsCloseProfile(DbgThread(), hProfile);

    return 1;
}


static
cmsInt32Number CheckOneTAC(cmsFloat64Number InkLimit)
{
    cmsHPROFILE h;
    cmsFloat64Number d;

    h =CreateFakeCMYK(InkLimit, TRUE);
    cmsSaveProfileToFile(DbgThread(), h, "lcmstac.icc");
    cmsCloseProfile(DbgThread(), h);

    h = cmsOpenProfileFromFile(DbgThread(), "lcmstac.icc", "r");
    d = cmsDetectTAC(DbgThread(), h);
    cmsCloseProfile(DbgThread(), h);

    remove("lcmstac.icc");

    if (fabs(d - InkLimit) > 5) return 0;

    return 1;
}


static
cmsInt32Number CheckTAC(void)
{
    if (!CheckOneTAC(180)) return 0;
    if (!CheckOneTAC(220)) return 0;
    if (!CheckOneTAC(286)) return 0;
    if (!CheckOneTAC(310)) return 0;
    if (!CheckOneTAC(330)) return 0;

    return 1;
}

// -------------------------------------------------------------------------------------------------------


#define NPOINTS_IT8 10  // (17*17*17*17)

static
cmsInt32Number CheckCGATS(void)
{
    cmsHANDLE  it8;
    cmsInt32Number i;

    SubTest("IT8 creation");
    it8 = cmsIT8Alloc(DbgThread());
    if (it8 == NULL) return 0;

    cmsIT8SetSheetType(DbgThread(), it8, "LCMS/TESTING");
    cmsIT8SetPropertyStr(DbgThread(), it8, "ORIGINATOR",   "1 2 3 4");
    cmsIT8SetPropertyUncooked(DbgThread(), it8, "DESCRIPTOR",   "1234");
    cmsIT8SetPropertyStr(DbgThread(), it8, "MANUFACTURER", "3");
    cmsIT8SetPropertyDbl(DbgThread(), it8, "CREATED",      4);
    cmsIT8SetPropertyDbl(DbgThread(), it8, "SERIAL",       5);
    cmsIT8SetPropertyHex(DbgThread(), it8, "MATERIAL",     0x123);

    cmsIT8SetPropertyDbl(DbgThread(), it8, "NUMBER_OF_SETS", NPOINTS_IT8);
    cmsIT8SetPropertyDbl(DbgThread(), it8, "NUMBER_OF_FIELDS", 4);

    cmsIT8SetDataFormat(DbgThread(), it8, 0, "SAMPLE_ID");
    cmsIT8SetDataFormat(DbgThread(), it8, 1, "RGB_R");
    cmsIT8SetDataFormat(DbgThread(), it8, 2, "RGB_G");
    cmsIT8SetDataFormat(DbgThread(), it8, 3, "RGB_B");

    SubTest("Table creation");
    for (i=0; i < NPOINTS_IT8; i++) {

          char Patch[20];

          sprintf(Patch, "P%d", i);

          cmsIT8SetDataRowCol(DbgThread(), it8, i, 0, Patch);
          cmsIT8SetDataRowColDbl(DbgThread(), it8, i, 1, i);
          cmsIT8SetDataRowColDbl(DbgThread(), it8, i, 2, i);
          cmsIT8SetDataRowColDbl(DbgThread(), it8, i, 3, i);
    }

    SubTest("Save to file");
    cmsIT8SaveToFile(DbgThread(), it8, "TEST.IT8");
    cmsIT8Free(DbgThread(), it8);

    SubTest("Load from file");
    it8 = cmsIT8LoadFromFile(DbgThread(), "TEST.IT8");
    if (it8 == NULL) return 0;

    SubTest("Save again file");
    cmsIT8SaveToFile(DbgThread(), it8, "TEST.IT8");
    cmsIT8Free(DbgThread(), it8);


    SubTest("Load from file (II)");
    it8 = cmsIT8LoadFromFile(DbgThread(), "TEST.IT8");
    if (it8 == NULL) return 0;


     SubTest("Change prop value");
    if (cmsIT8GetPropertyDbl(DbgThread(), it8, "DESCRIPTOR") != 1234) {

        return 0;
    }


    cmsIT8SetPropertyDbl(DbgThread(), it8, "DESCRIPTOR", 5678);
    if (cmsIT8GetPropertyDbl(DbgThread(), it8, "DESCRIPTOR") != 5678) {

        return 0;
    }

     SubTest("Positive numbers");
    if (cmsIT8GetDataDbl(DbgThread(), it8, "P3", "RGB_G") != 3) {

        return 0;
    }


     SubTest("Positive exponent numbers");
     cmsIT8SetPropertyDbl(DbgThread(), it8, "DBL_PROP", 123E+12);
     if ((cmsIT8GetPropertyDbl(DbgThread(), it8, "DBL_PROP") - 123E+12) > 1 ) {

        return 0;
    }

    SubTest("Negative exponent numbers");
    cmsIT8SetPropertyDbl(DbgThread(), it8, "DBL_PROP_NEG", 123E-45);
     if ((cmsIT8GetPropertyDbl(DbgThread(), it8, "DBL_PROP_NEG") - 123E-45) > 1E-45 ) {

        return 0;
    }


    SubTest("Negative numbers");
    cmsIT8SetPropertyDbl(DbgThread(), it8, "DBL_NEG_VAL", -123);
    if ((cmsIT8GetPropertyDbl(DbgThread(), it8, "DBL_NEG_VAL")) != -123 ) {

        return 0;
    }

    cmsIT8Free(DbgThread(), it8);

    remove("TEST.IT8");
    return 1;

}


static
cmsInt32Number CheckCGATS2(void)
{
    cmsHANDLE handle;
    const cmsUInt8Number junk[] = { 0x0, 0xd, 0xd, 0xa, 0x20, 0xd, 0x20, 0x20, 0x20, 0x3a, 0x31, 0x3d, 0x3d, 0x3d, 0x3d };

    handle = cmsIT8LoadFromMem(0, (const void*)junk, sizeof(junk));
    if (handle)
        cmsIT8Free(DbgThread(), handle);

    return 1;
}


static
cmsInt32Number CheckCGATS_Overflow(void)
{
    cmsHANDLE handle;
    const cmsUInt8Number junk[] = { "@\nA 1.e2147483648\n" };

    handle = cmsIT8LoadFromMem(0, (const void*)junk, sizeof(junk));
    if (handle)
        cmsIT8Free(DbgThread(), handle);

    return 1;
}

// Create CSA/CRD

static
void GenerateCSA(const char* cInProf, const char* FileName)
{
    cmsHPROFILE hProfile;
    cmsUInt32Number n;
    char* Buffer;
    cmsContext BuffThread = DbgThread();
    FILE* o;


    if (cInProf == NULL)
        hProfile = cmsCreateLab4Profile(DbgThread(), NULL);
    else
        hProfile = cmsOpenProfileFromFile(DbgThread(), cInProf, "r");

    n = cmsGetPostScriptCSA(DbgThread(), hProfile, 0, 0, NULL, 0);
    if (n == 0) return;

    Buffer = (char*) _cmsMalloc(BuffThread, n + 1);
    cmsGetPostScriptCSA(DbgThread(), hProfile, 0, 0, Buffer, n);
    Buffer[n] = 0;

    if (FileName != NULL) {
        o = fopen(FileName, "wb");
        fwrite(Buffer, n, 1, o);
        fclose(o);
    }

    _cmsFree(BuffThread, Buffer);
    cmsCloseProfile(DbgThread(), hProfile);
    if (FileName != NULL)
        remove(FileName);
}


static
void GenerateCRD(const char* cOutProf, const char* FileName)
{
    cmsHPROFILE hProfile;
    cmsUInt32Number n;
    char* Buffer;
    cmsUInt32Number dwFlags = 0;
    cmsContext BuffThread = DbgThread();


    if (cOutProf == NULL)
        hProfile = cmsCreateLab4Profile(DbgThread(), NULL);
    else
        hProfile = cmsOpenProfileFromFile(DbgThread(), cOutProf, "r");

    n = cmsGetPostScriptCRD(DbgThread(), hProfile, 0, dwFlags, NULL, 0);
    if (n == 0) return;

    Buffer = (char*) _cmsMalloc(BuffThread, n + 1);
    cmsGetPostScriptCRD(DbgThread(), hProfile, 0, dwFlags, Buffer, n);
    Buffer[n] = 0;

    if (FileName != NULL) {
        FILE* o = fopen(FileName, "wb");
        fwrite(Buffer, n, 1, o);
        fclose(o);
    }

    _cmsFree(BuffThread, Buffer);
    cmsCloseProfile(DbgThread(), hProfile);
    if (FileName != NULL)
        remove(FileName);
}

static
cmsInt32Number CheckPostScript(void)
{
    GenerateCSA("test5.icc", "sRGB_CSA.ps");
    GenerateCSA("aRGBlcms2.icc", "aRGB_CSA.ps");
    GenerateCSA("test4.icc", "sRGBV4_CSA.ps");
    GenerateCSA("test1.icc", "SWOP_CSA.ps");
    GenerateCSA(NULL, "Lab_CSA.ps");
    GenerateCSA("graylcms2.icc", "gray_CSA.ps");

    GenerateCRD("test5.icc", "sRGB_CRD.ps");
    GenerateCRD("aRGBlcms2.icc", "aRGB_CRD.ps");
    GenerateCRD(NULL, "Lab_CRD.ps");
    GenerateCRD("test1.icc", "SWOP_CRD.ps");
    GenerateCRD("test4.icc", "sRGBV4_CRD.ps");
    GenerateCRD("graylcms2.icc", "gray_CRD.ps");

    return 1;
}


static
cmsInt32Number CheckGray(cmsHTRANSFORM xform, cmsUInt8Number g, double L)
{
    cmsCIELab Lab;

    cmsDoTransform(DbgThread(), xform, &g, &Lab, 1);

    if (!IsGoodVal("a axis on gray", 0, Lab.a, 0.001)) return 0;
    if (!IsGoodVal("b axis on gray", 0, Lab.b, 0.001)) return 0;

    return IsGoodVal("Gray value", L, Lab.L, 0.01);
}

static
cmsInt32Number CheckInputGray(void)
{
    cmsHPROFILE hGray = Create_Gray22();
    cmsHPROFILE hLab  = cmsCreateLab4Profile(DbgThread(), NULL);
    cmsHTRANSFORM xform;

    if (hGray == NULL || hLab == NULL) return 0;

    xform = cmsCreateTransform(DbgThread(), hGray, TYPE_GRAY_8, hLab, TYPE_Lab_DBL, INTENT_RELATIVE_COLORIMETRIC, 0);
    cmsCloseProfile(DbgThread(), hGray); cmsCloseProfile(DbgThread(), hLab);

    if (!CheckGray(xform, 0, 0)) return 0;
    if (!CheckGray(xform, 125, 52.768)) return 0;
    if (!CheckGray(xform, 200, 81.069)) return 0;
    if (!CheckGray(xform, 255, 100.0)) return 0;

    cmsDeleteTransform(DbgThread(), xform);
    return 1;
}

static
cmsInt32Number CheckLabInputGray(void)
{
    cmsHPROFILE hGray = Create_GrayLab();
    cmsHPROFILE hLab  = cmsCreateLab4Profile(DbgThread(), NULL);
    cmsHTRANSFORM xform;

    if (hGray == NULL || hLab == NULL) return 0;

    xform = cmsCreateTransform(DbgThread(), hGray, TYPE_GRAY_8, hLab, TYPE_Lab_DBL, INTENT_RELATIVE_COLORIMETRIC, 0);
    cmsCloseProfile(DbgThread(), hGray); cmsCloseProfile(DbgThread(), hLab);

    if (!CheckGray(xform, 0, 0)) return 0;
    if (!CheckGray(xform, 125, 49.019)) return 0;
    if (!CheckGray(xform, 200, 78.431)) return 0;
    if (!CheckGray(xform, 255, 100.0)) return 0;

    cmsDeleteTransform(DbgThread(), xform);
    return 1;
}


static
cmsInt32Number CheckOutGray(cmsHTRANSFORM xform, double L, cmsUInt8Number g)
{
    cmsCIELab Lab;
    cmsUInt8Number g_out;

    Lab.L = L;
    Lab.a = 0;
    Lab.b = 0;

    cmsDoTransform(DbgThread(), xform, &Lab, &g_out, 1);

    return IsGoodVal("Gray value", g, (double) g_out, 0.01);
}

static
cmsInt32Number CheckOutputGray(void)
{
    cmsHPROFILE hGray = Create_Gray22();
    cmsHPROFILE hLab  = cmsCreateLab4Profile(DbgThread(), NULL);
    cmsHTRANSFORM xform;

    if (hGray == NULL || hLab == NULL) return 0;

    xform = cmsCreateTransform(DbgThread(), hLab, TYPE_Lab_DBL, hGray, TYPE_GRAY_8, INTENT_RELATIVE_COLORIMETRIC, 0);
    cmsCloseProfile(DbgThread(), hGray); cmsCloseProfile(DbgThread(), hLab);

    if (!CheckOutGray(xform, 0, 0)) return 0;
    if (!CheckOutGray(xform, 100, 255)) return 0;

    if (!CheckOutGray(xform, 20, 52)) return 0;
    if (!CheckOutGray(xform, 50, 118)) return 0;


    cmsDeleteTransform(DbgThread(), xform);
    return 1;
}


static
cmsInt32Number CheckLabOutputGray(void)
{
    cmsHPROFILE hGray = Create_GrayLab();
    cmsHPROFILE hLab  = cmsCreateLab4Profile(DbgThread(), NULL);
    cmsHTRANSFORM xform;
    cmsInt32Number i;

    if (hGray == NULL || hLab == NULL) return 0;

    xform = cmsCreateTransform(DbgThread(), hLab, TYPE_Lab_DBL, hGray, TYPE_GRAY_8, INTENT_RELATIVE_COLORIMETRIC, 0);
    cmsCloseProfile(DbgThread(), hGray); cmsCloseProfile(DbgThread(), hLab);

    if (!CheckOutGray(xform, 0, 0)) return 0;
    if (!CheckOutGray(xform, 100, 255)) return 0;

    for (i=0; i < 100; i++) {

        cmsUInt8Number g;

        g = (cmsUInt8Number) floor(i * 255.0 / 100.0 + 0.5);

        if (!CheckOutGray(xform, i, g)) return 0;
    }


    cmsDeleteTransform(DbgThread(), xform);
    return 1;
}


static
cmsInt32Number CheckV4gamma(void)
{
    cmsHPROFILE h;
    cmsUInt16Number Lin[] = {0, 0xffff};
    cmsToneCurve*g = cmsBuildTabulatedToneCurve16(DbgThread(), 2, Lin);

    h = cmsOpenProfileFromFile(DbgThread(), "v4gamma.icc", "w");
    if (h == NULL) return 0;


    cmsSetProfileVersion(DbgThread(), h, 4.3);

    if (!cmsWriteTag(DbgThread(), h, cmsSigGrayTRCTag, g)) return 0;
    cmsCloseProfile(DbgThread(), h);

    cmsFreeToneCurve(DbgThread(), g);
    remove("v4gamma.icc");
    return 1;
}

// cmsBool cmsGBDdumpVRML(cmsHANDLE hGBD, const char* fname);

// Gamut descriptor routines
static
cmsInt32Number CheckGBD(void)
{
    cmsCIELab Lab;
    cmsHANDLE  h;
    cmsInt32Number L, a, b;
    cmsUInt32Number r1, g1, b1;
    cmsHPROFILE hLab, hsRGB;
    cmsHTRANSFORM xform;

    h = cmsGBDAlloc(DbgThread());
    if (h == NULL) return 0;

    // Fill all Lab gamut as valid
    SubTest("Filling RAW gamut");

    for (L=0; L <= 100; L += 10)
        for (a = -128; a <= 128; a += 5)
            for (b = -128; b <= 128; b += 5) {

                Lab.L = L;
                Lab.a = a;
                Lab.b = b;
                if (!cmsGDBAddPoint(DbgThread(), h, &Lab)) return 0;
            }

    // Complete boundaries
    SubTest("computing Lab gamut");
    if (!cmsGDBCompute(DbgThread(), h, 0)) return 0;


    // All points should be inside gamut
    SubTest("checking Lab gamut");
    for (L=10; L <= 90; L += 25)
        for (a = -120; a <= 120; a += 25)
            for (b = -120; b <= 120; b += 25) {

                Lab.L = L;
                Lab.a = a;
                Lab.b = b;
                if (!cmsGDBCheckPoint(DbgThread(), h, &Lab)) {
                    return 0;
                }
            }
    cmsGBDFree(DbgThread(), h);


    // Now for sRGB
    SubTest("checking sRGB gamut");
    h = cmsGBDAlloc(DbgThread());
    hsRGB = cmsCreate_sRGBProfile(DbgThread());
    hLab  = cmsCreateLab4Profile(DbgThread(), NULL);

    xform = cmsCreateTransform(DbgThread(), hsRGB, TYPE_RGB_8, hLab, TYPE_Lab_DBL, INTENT_RELATIVE_COLORIMETRIC, cmsFLAGS_NOCACHE);
    cmsCloseProfile(DbgThread(), hsRGB); cmsCloseProfile(DbgThread(), hLab);


    for (r1=0; r1 < 256; r1 += 5) {
        for (g1=0; g1 < 256; g1 += 5)
            for (b1=0; b1 < 256; b1 += 5) {


                cmsUInt8Number rgb[3];

                rgb[0] = (cmsUInt8Number) r1;
                rgb[1] = (cmsUInt8Number) g1;
                rgb[2] = (cmsUInt8Number) b1;

                cmsDoTransform(DbgThread(), xform, rgb, &Lab, 1);

                // if (fabs(Lab.b) < 20 && Lab.a > 0) continue;

                if (!cmsGDBAddPoint(DbgThread(), h, &Lab)) {
                    cmsGBDFree(DbgThread(), h);
                    return 0;
                }


            }
    }


    if (!cmsGDBCompute(DbgThread(), h, 0)) return 0;
    // cmsGBDdumpVRML(h, "c:\\colormaps\\lab.wrl");

    for (r1=10; r1 < 200; r1 += 10) {
        for (g1=10; g1 < 200; g1 += 10)
            for (b1=10; b1 < 200; b1 += 10) {


                cmsUInt8Number rgb[3];

                rgb[0] = (cmsUInt8Number) r1;
                rgb[1] = (cmsUInt8Number) g1;
                rgb[2] = (cmsUInt8Number) b1;

                cmsDoTransform(DbgThread(), xform, rgb, &Lab, 1);
                if (!cmsGDBCheckPoint(DbgThread(), h, &Lab)) {

                    cmsDeleteTransform(DbgThread(), xform);
                    cmsGBDFree(DbgThread(), h);
                    return 0;
                }
            }
    }


    cmsDeleteTransform(DbgThread(), xform);
    cmsGBDFree(DbgThread(), h);

    SubTest("checking LCh chroma ring");
    h = cmsGBDAlloc(DbgThread());


    for (r1=0; r1 < 360; r1++) {

        cmsCIELCh LCh;

        LCh.L = 70;
        LCh.C = 60;
        LCh.h = r1;

        cmsLCh2Lab(DbgThread(), &Lab, &LCh);
        if (!cmsGDBAddPoint(DbgThread(), h, &Lab)) {
                    cmsGBDFree(DbgThread(), h);
                    return 0;
                }
    }


    if (!cmsGDBCompute(DbgThread(), h, 0)) return 0;

    cmsGBDFree(DbgThread(), h);

    return 1;
}


static
int CheckMD5(void)
{
    _cmsICCPROFILE* h;
    cmsHPROFILE pProfile = cmsOpenProfileFromFile(DbgThread(), "sRGBlcms2.icc", "r");
    cmsProfileID ProfileID1, ProfileID2, ProfileID3, ProfileID4;

    h =(_cmsICCPROFILE*) pProfile;
    if (cmsMD5computeID(DbgThread(), pProfile)) cmsGetHeaderProfileID(DbgThread(), pProfile, ProfileID1.ID8);
    if (cmsMD5computeID(DbgThread(), pProfile)) cmsGetHeaderProfileID(DbgThread(), pProfile,ProfileID2.ID8);

    cmsCloseProfile(DbgThread(), pProfile);


    pProfile = cmsOpenProfileFromFile(DbgThread(), "sRGBlcms2.icc", "r");

    h =(_cmsICCPROFILE*) pProfile;
    if (cmsMD5computeID(DbgThread(), pProfile)) cmsGetHeaderProfileID(DbgThread(), pProfile, ProfileID3.ID8);
    if (cmsMD5computeID(DbgThread(), pProfile)) cmsGetHeaderProfileID(DbgThread(), pProfile,ProfileID4.ID8);

    cmsCloseProfile(DbgThread(), pProfile);

    return ((memcmp(ProfileID1.ID8, ProfileID3.ID8, sizeof(ProfileID1)) == 0) &&
            (memcmp(ProfileID2.ID8, ProfileID4.ID8, sizeof(ProfileID2)) == 0));
}



static
int CheckLinking(void)
{
    cmsHPROFILE h;
    cmsPipeline * pipeline;
    cmsStage *stageBegin, *stageEnd;

    // Create a CLUT based profile
     h = cmsCreateInkLimitingDeviceLink(DbgThread(), cmsSigCmykData, 150);

     // link a second tag
     cmsLinkTag(DbgThread(), h, cmsSigAToB1Tag, cmsSigAToB0Tag);

     // Save the linked devicelink
    if (!cmsSaveProfileToFile(DbgThread(), h, "lcms2link.icc")) return 0;
    cmsCloseProfile(DbgThread(), h);

    // Now open the profile and read the pipeline
    h = cmsOpenProfileFromFile(DbgThread(), "lcms2link.icc", "r");
    if (h == NULL) return 0;

    pipeline = (cmsPipeline*) cmsReadTag(DbgThread(), h, cmsSigAToB1Tag);
    if (pipeline == NULL)
    {
        return 0;
    }

    pipeline = cmsPipelineDup(DbgThread(), pipeline);

    // extract stage from pipe line
    cmsPipelineUnlinkStage(DbgThread(), pipeline, cmsAT_BEGIN, &stageBegin);
    cmsPipelineUnlinkStage(DbgThread(), pipeline, cmsAT_END,   &stageEnd);
    cmsPipelineInsertStage(DbgThread(), pipeline, cmsAT_END,    stageEnd);
    cmsPipelineInsertStage(DbgThread(), pipeline, cmsAT_BEGIN,  stageBegin);

    if (cmsTagLinkedTo(DbgThread(), h, cmsSigAToB1Tag) != cmsSigAToB0Tag) return 0;

    cmsWriteTag(DbgThread(), h, cmsSigAToB0Tag, pipeline);
    cmsPipelineFree(DbgThread(), pipeline);

    if (!cmsSaveProfileToFile(DbgThread(), h, "lcms2link2.icc")) return 0;
    cmsCloseProfile(DbgThread(), h);


    return 1;

}

//  TestMPE
//
//  Created by Paul Miller on 30/08/2016.
//
static
cmsHPROFILE IdentityMatrixProfile( cmsColorSpaceSignature dataSpace)
{
    cmsContext ctx = 0;
    cmsVEC3 zero = {{0,0,0}};
    cmsMAT3 identity;
    cmsPipeline* forward;
    cmsPipeline* reverse;
    cmsHPROFILE identityProfile = cmsCreateProfilePlaceholder( ctx);


    cmsSetProfileVersion(DbgThread(), identityProfile, 4.3);

    cmsSetDeviceClass(DbgThread(),  identityProfile,     cmsSigColorSpaceClass);
    cmsSetColorSpace(DbgThread(), identityProfile,       dataSpace);
    cmsSetPCS(DbgThread(), identityProfile,              cmsSigXYZData);

    cmsSetHeaderRenderingIntent(DbgThread(), identityProfile,  INTENT_RELATIVE_COLORIMETRIC);

    cmsWriteTag(DbgThread(), identityProfile, cmsSigMediaWhitePointTag, cmsD50_XYZ(DbgThread()));



    _cmsMAT3identity(DbgThread(),  &identity);

    // build forward transform.... (RGB to PCS)
    forward = cmsPipelineAlloc( 0, 3, 3);
    cmsPipelineInsertStage(DbgThread(),  forward, cmsAT_END, cmsStageAllocMatrix( ctx, 3, 3, (cmsFloat64Number*)&identity, (cmsFloat64Number*)&zero));
    cmsWriteTag(DbgThread(),  identityProfile, cmsSigDToB1Tag, forward);

    cmsPipelineFree(DbgThread(), forward);

    reverse = cmsPipelineAlloc( 0, 3, 3);
    cmsPipelineInsertStage(DbgThread(),  reverse, cmsAT_END, cmsStageAllocMatrix( ctx, 3, 3, (cmsFloat64Number*)&identity, (cmsFloat64Number*)&zero));
    cmsWriteTag(DbgThread(),  identityProfile, cmsSigBToD1Tag, reverse);

    cmsPipelineFree(DbgThread(), reverse);

    return identityProfile;
}

static
cmsInt32Number CheckFloatXYZ(void)
{
    cmsHPROFILE input;
    cmsHPROFILE xyzProfile = cmsCreateXYZProfile(DbgThread());
    cmsHTRANSFORM xform;
    cmsFloat32Number in[4];
    cmsFloat32Number out[4];

    in[0] = 1.0;
    in[1] = 1.0;
    in[2] = 1.0;
    in[3] = 0.5;

    // RGB to XYZ
    input = IdentityMatrixProfile( cmsSigRgbData);

    xform = cmsCreateTransform(DbgThread(), input, TYPE_RGB_FLT, xyzProfile, TYPE_XYZ_FLT, INTENT_RELATIVE_COLORIMETRIC, 0);
    cmsCloseProfile(DbgThread(), input);

    cmsDoTransform(DbgThread(),  xform, in, out, 1);
    cmsDeleteTransform(DbgThread(),  xform);

    if (!IsGoodVal("Float RGB->XYZ", in[0], out[0], FLOAT_PRECISSION) ||
        !IsGoodVal("Float RGB->XYZ", in[1], out[1], FLOAT_PRECISSION) ||
        !IsGoodVal("Float RGB->XYZ", in[2], out[2], FLOAT_PRECISSION))
           return 0;


    // XYZ to XYZ
    input = IdentityMatrixProfile( cmsSigXYZData);

    xform = cmsCreateTransform(DbgThread(), input, TYPE_XYZ_FLT, xyzProfile, TYPE_XYZ_FLT, INTENT_RELATIVE_COLORIMETRIC, 0);
    cmsCloseProfile(DbgThread(), input);

    cmsDoTransform(DbgThread(),  xform, in, out, 1);


    cmsDeleteTransform(DbgThread(),  xform);

     if (!IsGoodVal("Float XYZ->XYZ", in[0], out[0], FLOAT_PRECISSION) ||
         !IsGoodVal("Float XYZ->XYZ", in[1], out[1], FLOAT_PRECISSION) ||
         !IsGoodVal("Float XYZ->XYZ", in[2], out[2], FLOAT_PRECISSION))
           return 0;


    input = IdentityMatrixProfile( cmsSigXYZData);

#   define TYPE_XYZA_FLT          (FLOAT_SH(1)|COLORSPACE_SH(PT_XYZ)|EXTRA_SH(1)|CHANNELS_SH(3)|BYTES_SH(4))

    xform = cmsCreateTransform(DbgThread(), input, TYPE_XYZA_FLT, xyzProfile, TYPE_XYZA_FLT, INTENT_RELATIVE_COLORIMETRIC, cmsFLAGS_COPY_ALPHA);
    cmsCloseProfile(DbgThread(), input);

    cmsDoTransform(DbgThread(), xform, in, out, 1);


    cmsDeleteTransform(DbgThread(),  xform);

     if (!IsGoodVal("Float XYZA->XYZA", in[0], out[0], FLOAT_PRECISSION) ||
         !IsGoodVal("Float XYZA->XYZA", in[1], out[1], FLOAT_PRECISSION) ||
         !IsGoodVal("Float XYZA->XYZA", in[2], out[2], FLOAT_PRECISSION) ||
         !IsGoodVal("Float XYZA->XYZA", in[3], out[3], FLOAT_PRECISSION))
           return 0;


    // XYZ to RGB
    input = IdentityMatrixProfile( cmsSigRgbData);

    xform = cmsCreateTransform(DbgThread(), xyzProfile, TYPE_XYZ_FLT, input, TYPE_RGB_FLT, INTENT_RELATIVE_COLORIMETRIC, 0);
    cmsCloseProfile(DbgThread(), input);

    cmsDoTransform(DbgThread(),  xform, in, out, 1);

    cmsDeleteTransform(DbgThread(),  xform);

       if (!IsGoodVal("Float XYZ->RGB", in[0], out[0], FLOAT_PRECISSION) ||
           !IsGoodVal("Float XYZ->RGB", in[1], out[1], FLOAT_PRECISSION) ||
           !IsGoodVal("Float XYZ->RGB", in[2], out[2], FLOAT_PRECISSION))
           return 0;


    // Now the optimizer should remove a stage

    // XYZ to RGB
    input = IdentityMatrixProfile( cmsSigRgbData);

    xform = cmsCreateTransform(DbgThread(), input, TYPE_RGB_FLT, input, TYPE_RGB_FLT, INTENT_RELATIVE_COLORIMETRIC, 0);
    cmsCloseProfile(DbgThread(), input);

    cmsDoTransform(DbgThread(),  xform, in, out, 1);

    cmsDeleteTransform(DbgThread(),  xform);

       if (!IsGoodVal("Float RGB->RGB", in[0], out[0], FLOAT_PRECISSION) ||
           !IsGoodVal("Float RGB->RGB", in[1], out[1], FLOAT_PRECISSION) ||
           !IsGoodVal("Float RGB->RGB", in[2], out[2], FLOAT_PRECISSION))
           return 0;

    cmsCloseProfile(DbgThread(), xyzProfile);


    return 1;
}


/*
Bug reported

        1)
        sRGB built-in V4.3 -> Lab identity built-in V4.3
        Flags: "cmsFLAGS_NOCACHE", "cmsFLAGS_NOOPTIMIZE"
        Input format: TYPE_RGBA_FLT
        Output format: TYPE_LabA_FLT

        2) and back
        Lab identity built-in V4.3 -> sRGB built-in V4.3
        Flags: "cmsFLAGS_NOCACHE", "cmsFLAGS_NOOPTIMIZE"
        Input format: TYPE_LabA_FLT
        Output format: TYPE_RGBA_FLT

*/
static
cmsInt32Number ChecksRGB2LabFLT(void)
{
    cmsHPROFILE hSRGB = cmsCreate_sRGBProfile(DbgThread());
    cmsHPROFILE hLab  = cmsCreateLab4Profile(DbgThread(), NULL);

    cmsHTRANSFORM xform1 = cmsCreateTransform(DbgThread(), hSRGB, TYPE_RGBA_FLT, hLab, TYPE_LabA_FLT, 0, cmsFLAGS_NOCACHE|cmsFLAGS_NOOPTIMIZE);
    cmsHTRANSFORM xform2 = cmsCreateTransform(DbgThread(), hLab, TYPE_LabA_FLT, hSRGB, TYPE_RGBA_FLT, 0, cmsFLAGS_NOCACHE|cmsFLAGS_NOOPTIMIZE);

    cmsFloat32Number RGBA1[4], RGBA2[4], LabA[4];
    int i;


    for (i = 0; i <= 100; i++)
    {
        RGBA1[0] = i / 100.0F;
        RGBA1[1] = i / 100.0F;
        RGBA1[2] = i / 100.0F;
        RGBA1[3] = 0;

        cmsDoTransform(DbgThread(), xform1, RGBA1, LabA,  1);
        cmsDoTransform(DbgThread(), xform2, LabA, RGBA2, 1);

        if (!IsGoodVal("Float RGB->RGB", RGBA1[0], RGBA2[0], FLOAT_PRECISSION) ||
            !IsGoodVal("Float RGB->RGB", RGBA1[1], RGBA2[1], FLOAT_PRECISSION) ||
            !IsGoodVal("Float RGB->RGB", RGBA1[2], RGBA2[2], FLOAT_PRECISSION))
            return 0;
    }


    cmsDeleteTransform(DbgThread(), xform1);
    cmsDeleteTransform(DbgThread(), xform2);
    cmsCloseProfile(DbgThread(), hSRGB);
    cmsCloseProfile(DbgThread(), hLab);

    return 1;
}

/*
 * parametric curve for Rec709
 */
static
double Rec709(double L)
{
    if (L <0.018) return 4.5*L;
    else
    {
          double a = 1.099* pow(L, 0.45);

          a = a - 0.099;
          return a;
    }
}


static
cmsInt32Number CheckParametricRec709(void)
{
    cmsFloat64Number params[7];
    cmsToneCurve* t;
    int i;

    params[0] = 0.45; /* y */
    params[1] = pow(1.099, 1.0 / 0.45); /* a */
    params[2] = 0.0; /* b */
    params[3] = 4.5; /* c */
    params[4] = 0.018; /* d */
    params[5] = -0.099; /* e */
    params[6] = 0.0; /* f */

    t = cmsBuildParametricToneCurve (NULL, 5, params);


    for (i=0; i < 256; i++)
    {
        cmsFloat32Number n = (cmsFloat32Number) i / 255.0F;
        cmsUInt16Number f1 = (cmsUInt16Number) floor(255.0 * cmsEvalToneCurveFloat(DbgThread(), t, n) + 0.5);
        cmsUInt16Number f2 = (cmsUInt16Number) floor(255.0*Rec709((double) i / 255.0) + 0.5);

        if (f1 != f2)
        {
            cmsFreeToneCurve(DbgThread(), t);
            return 0;
        }
    }

    cmsFreeToneCurve(DbgThread(), t);
    return 1;
}


#define kNumPoints  10

typedef cmsFloat32Number(*Function)(cmsFloat32Number x);

static cmsFloat32Number StraightLine( cmsFloat32Number x)
{
    return (cmsFloat32Number) (0.1 + 0.9 * x);
}

static cmsInt32Number TestCurve( const char* label, cmsToneCurve* curve, Function fn)
{
    cmsInt32Number ok = 1;
    int i;
    for (i = 0; i < kNumPoints*3; i++) {

        cmsFloat32Number x = (cmsFloat32Number)i / (kNumPoints*3 - 1);
        cmsFloat32Number expectedY = fn(x);
        cmsFloat32Number out = cmsEvalToneCurveFloat(DbgThread(),  curve, x);

        if (!IsGoodVal(label, expectedY, out, FLOAT_PRECISSION)) {
            ok = 0;
        }
    }
    return ok;
}

static
cmsInt32Number CheckFloatSamples(void)
{
    cmsFloat32Number y[kNumPoints];
    int i;
    cmsToneCurve *curve;
    cmsInt32Number ok;

    for (i = 0; i < kNumPoints; i++) {
        cmsFloat32Number x = (cmsFloat32Number)i / (kNumPoints-1);

        y[i] = StraightLine(x);
    }

    curve = cmsBuildTabulatedToneCurveFloat(NULL, kNumPoints, y);
    ok = TestCurve( "Float Samples", curve, StraightLine);
    cmsFreeToneCurve(DbgThread(), curve);

    return ok;
}

static
cmsInt32Number CheckFloatSegments(void)
{
    cmsInt32Number ok = 1;
    int i;
    cmsToneCurve *curve;

    cmsFloat32Number y[ kNumPoints];

    // build a segmented curve with a sampled section...
    cmsCurveSegment Seg[3];

    // Initialize segmented curve part up to 0.1
    Seg[0].x0 = -1e22f;      // -infinity
    Seg[0].x1 = 0.1f;
    Seg[0].Type = 6;             // Y = (a * X + b) ^ Gamma + c
    Seg[0].Params[0] = 1.0f;     // gamma
    Seg[0].Params[1] = 0.9f;     // a
    Seg[0].Params[2] = 0.0f;        // b
    Seg[0].Params[3] = 0.1f;     // c
    Seg[0].Params[4] = 0.0f;

    // From zero to 1
    Seg[1].x0 = 0.1f;
    Seg[1].x1 = 0.9f;
    Seg[1].Type = 0;

    Seg[1].nGridPoints = kNumPoints;
    Seg[1].SampledPoints = y;

    for (i = 0; i < kNumPoints; i++) {
        cmsFloat32Number x = (cmsFloat32Number) (0.1 + ((cmsFloat32Number)i / (kNumPoints-1)) * (0.9 - 0.1));
        y[i] = StraightLine(x);
    }

    // from 1 to +infinity
    Seg[2].x0 = 0.9f;
    Seg[2].x1 = 1e22f;   // +infinity
    Seg[2].Type = 6;

    Seg[2].Params[0] = 1.0f;
    Seg[2].Params[1] = 0.9f;
    Seg[2].Params[2] = 0.0f;
    Seg[2].Params[3] = 0.1f;
    Seg[2].Params[4] = 0.0f;

    curve = cmsBuildSegmentedToneCurve(0, 3, Seg);

    ok = TestCurve( "Float Segmented Curve", curve, StraightLine);

    cmsFreeToneCurve(DbgThread(), curve);

    return ok;
}


static
cmsInt32Number CheckReadRAW(void)
{
    cmsInt32Number tag_size, tag_size1;
    char buffer[4];
    cmsHPROFILE hProfile;


    SubTest("RAW read on on-disk");
    hProfile = cmsOpenProfileFromFile(DbgThread(), "test1.icc", "r");

    if (hProfile == NULL)
        return 0;

    tag_size = cmsReadRawTag(DbgThread(), hProfile, cmsSigGamutTag, buffer, 4);
    tag_size1 = cmsReadRawTag(DbgThread(), hProfile, cmsSigGamutTag, NULL, 0);

    cmsCloseProfile(DbgThread(), hProfile);

    if (tag_size != 4)
        return 0;

    if (tag_size1 != 37009)
        return 0;

    SubTest("RAW read on in-memory created profiles");
    hProfile = cmsCreate_sRGBProfile(DbgThread());
    tag_size = cmsReadRawTag(DbgThread(), hProfile, cmsSigGreenColorantTag, buffer, 4);
    tag_size1 = cmsReadRawTag(DbgThread(), hProfile, cmsSigGreenColorantTag, NULL, 0);

    cmsCloseProfile(DbgThread(), hProfile);

    if (tag_size != 4)
        return 0;
    if (tag_size1 != 20)
        return 0;

    return 1;
}


static
cmsInt32Number CheckMeta(void)
{
    char *data;
    cmsHANDLE dict;
    cmsHPROFILE p;
    cmsUInt32Number clen;
    FILE *fp;
    int rc;

    /* open file */
    p = cmsOpenProfileFromFile(DbgThread(), "ibm-t61.icc", "r");
    if (p == NULL) return 0;

    /* read dictionary, but don't do anything with the value */
    //COMMENT OUT THE NEXT TWO LINES AND IT WORKS FINE!!!
    dict = cmsReadTag(DbgThread(), p, cmsSigMetaTag);
    if (dict == NULL) return 0;

    /* serialize profile to memory */
    rc = cmsSaveProfileToMem(DbgThread(), p, NULL, &clen);
    if (!rc) return 0;

    data = (char*) malloc(clen);
    rc = cmsSaveProfileToMem(DbgThread(), p, data, &clen);
    if (!rc) return 0;

    /* write the memory blob to a file */
    //NOTE: The crash does not happen if cmsSaveProfileToFile() is used */
    fp = fopen("new.icc", "wb");
    fwrite(data, 1, clen, fp);
    fclose(fp);
    free(data);

    cmsCloseProfile(DbgThread(), p);

    /* open newly created file and read metadata */
    p = cmsOpenProfileFromFile(DbgThread(), "new.icc", "r");
    //ERROR: Bad dictionary Name/Value
    //ERROR: Corrupted tag 'meta'
    //test: test.c:59: main: Assertion `dict' failed.
    dict = cmsReadTag(DbgThread(), p, cmsSigMetaTag);
   if (dict == NULL) return 0;

   cmsCloseProfile(DbgThread(), p);
    return 1;
}


// Bug on applying null transforms on floating point buffers
static
cmsInt32Number CheckFloatNULLxform(void)
{
    int i;
    cmsFloat32Number in[10] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
    cmsFloat32Number out[10];

    cmsHTRANSFORM xform = cmsCreateTransform(DbgThread(), NULL, TYPE_GRAY_FLT, NULL, TYPE_GRAY_FLT, INTENT_PERCEPTUAL, cmsFLAGS_NULLTRANSFORM);

    if (xform == NULL) {
        Fail("Unable to create float null transform");
        return 0;
    }

    cmsDoTransform(DbgThread(), xform, in, out, 10);

    cmsDeleteTransform(DbgThread(), xform);
    for (i=0; i < 10; i++) {

        if (!IsGoodVal("float nullxform", in[i], out[i], 0.001)) {

            return 0;
        }
    }

    return 1;
}

static
cmsInt32Number CheckRemoveTag(void)
{
    cmsHPROFILE p;
    cmsMLU *mlu;
    int ret;

    p = cmsCreate_sRGBProfile(NULL);

    /* set value */
    mlu = cmsMLUalloc (NULL, 1);
    ret = cmsMLUsetASCII(DbgThread(), mlu, "en", "US", "bar");
    if (!ret) return 0;

    ret = cmsWriteTag(DbgThread(), p, cmsSigDeviceMfgDescTag, mlu);
    if (!ret) return 0;

    cmsMLUfree(DbgThread(), mlu);

    /* remove the tag  */
    ret = cmsWriteTag(DbgThread(), p, cmsSigDeviceMfgDescTag, NULL);
    if (!ret) return 0;

    /* THIS EXPLODES */
    cmsCloseProfile(DbgThread(), p);
    return 1;
}


static
cmsInt32Number CheckMatrixSimplify(void)
{

       cmsHPROFILE pIn;
       cmsHPROFILE pOut;
       cmsHTRANSFORM t;
       unsigned char buf[3] = { 127, 32, 64 };


       pIn = cmsCreate_sRGBProfile(DbgThread());
       pOut = cmsOpenProfileFromFile(DbgThread(), "ibm-t61.icc", "r");
       if (pIn == NULL || pOut == NULL)
              return 0;

       t = cmsCreateTransform(DbgThread(), pIn, TYPE_RGB_8, pOut, TYPE_RGB_8, INTENT_PERCEPTUAL, 0);
       cmsDoTransformStride(DbgThread(), t, buf, buf, 1, 1);
       cmsDeleteTransform(DbgThread(), t);
       cmsCloseProfile(DbgThread(), pIn);
       cmsCloseProfile(DbgThread(), pOut);


       return buf[0] == 144 && buf[1] == 0 && buf[2] == 69;
}



static
cmsInt32Number CheckTransformLineStride(void)
{

       cmsHPROFILE pIn;
       cmsHPROFILE pOut;
       cmsHTRANSFORM t;

       // Our buffer is formed by 4 RGB8 lines, each line is 2 pixels wide plus a padding of one byte

       cmsUInt8Number buf1[]= { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0,
                                0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0,
                                0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0,
                                0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0, };

       // Our buffer2 is formed by 4 RGBA lines, each line is 2 pixels wide plus a padding of one byte

       cmsUInt8Number buf2[] = { 0xff, 0xff, 0xff, 1, 0xff, 0xff, 0xff, 1, 0,
                                 0xff, 0xff, 0xff, 1, 0xff, 0xff, 0xff, 1, 0,
                                 0xff, 0xff, 0xff, 1, 0xff, 0xff, 0xff, 1, 0,
                                 0xff, 0xff, 0xff, 1, 0xff, 0xff, 0xff, 1, 0};

       // Our buffer3 is formed by 4 RGBA16 lines, each line is 2 pixels wide plus a padding of two bytes

       cmsUInt16Number buf3[] = { 0xffff, 0xffff, 0xffff, 0x0101, 0xffff, 0xffff, 0xffff, 0x0101, 0,
                                  0xffff, 0xffff, 0xffff, 0x0101, 0xffff, 0xffff, 0xffff, 0x0101, 0,
                                  0xffff, 0xffff, 0xffff, 0x0101, 0xffff, 0xffff, 0xffff, 0x0101, 0,
                                  0xffff, 0xffff, 0xffff, 0x0101, 0xffff, 0xffff, 0xffff, 0x0101, 0 };

       cmsUInt8Number out[1024];


       memset(out, 0, sizeof(out));
       pIn = cmsCreate_sRGBProfile(DbgThread());
       pOut = cmsOpenProfileFromFile(DbgThread(),  "ibm-t61.icc", "r");
       if (pIn == NULL || pOut == NULL)
              return 0;

       t = cmsCreateTransform(DbgThread(), pIn, TYPE_RGB_8, pOut, TYPE_RGB_8, INTENT_PERCEPTUAL, cmsFLAGS_COPY_ALPHA);

       cmsDoTransformLineStride(DbgThread(), t, buf1, out, 2, 4, 7, 7, 0, 0);
       cmsDeleteTransform(DbgThread(), t);

       if (memcmp(out, buf1, sizeof(buf1)) != 0) {
              Fail("Failed transform line stride on RGB8");
              cmsCloseProfile(DbgThread(), pIn);
              cmsCloseProfile(DbgThread(), pOut);
              return 0;
       }

       memset(out, 0, sizeof(out));

       t = cmsCreateTransform(DbgThread(), pIn, TYPE_RGBA_8, pOut, TYPE_RGBA_8, INTENT_PERCEPTUAL, cmsFLAGS_COPY_ALPHA);

       cmsDoTransformLineStride(DbgThread(), t, buf2, out, 2, 4, 9, 9, 0, 0);

       cmsDeleteTransform(DbgThread(), t);


       if (memcmp(out, buf2, sizeof(buf2)) != 0) {
              cmsCloseProfile(DbgThread(), pIn);
              cmsCloseProfile(DbgThread(), pOut);
              Fail("Failed transform line stride on RGBA8");
              return 0;
       }

       memset(out, 0, sizeof(out));

       t = cmsCreateTransform(DbgThread(), pIn, TYPE_RGBA_16, pOut, TYPE_RGBA_16, INTENT_PERCEPTUAL, cmsFLAGS_COPY_ALPHA);

       cmsDoTransformLineStride(DbgThread(), t, buf3, out, 2, 4, 18, 18, 0, 0);

       cmsDeleteTransform(DbgThread(), t);

       if (memcmp(out, buf3, sizeof(buf3)) != 0) {
              cmsCloseProfile(DbgThread(), pIn);
              cmsCloseProfile(DbgThread(), pOut);
              Fail("Failed transform line stride on RGBA16");
              return 0;
       }


       memset(out, 0, sizeof(out));


       // From 8 to 16
       t = cmsCreateTransform(DbgThread(), pIn, TYPE_RGBA_8, pOut, TYPE_RGBA_16, INTENT_PERCEPTUAL, cmsFLAGS_COPY_ALPHA);

       cmsDoTransformLineStride(DbgThread(), t, buf2, out, 2, 4, 9, 18, 0, 0);

       cmsDeleteTransform(DbgThread(), t);

       if (memcmp(out, buf3, sizeof(buf3)) != 0) {
              cmsCloseProfile(DbgThread(), pIn);
              cmsCloseProfile(DbgThread(), pOut);
              Fail("Failed transform line stride on RGBA16");
              return 0;
       }



       cmsCloseProfile(DbgThread(), pIn);
       cmsCloseProfile(DbgThread(), pOut);

       return 1;
}


static
int CheckPlanar8opt(void)
{
    cmsHPROFILE aboveRGB = Create_AboveRGB();
    cmsHPROFILE sRGB = cmsCreate_sRGBProfile(DbgThread());

    cmsHTRANSFORM transform = cmsCreateTransform(DbgThread(),
        sRGB, TYPE_RGB_8_PLANAR,
        aboveRGB, TYPE_RGB_8_PLANAR,
        INTENT_PERCEPTUAL, 0);

    cmsDeleteTransform(DbgThread(), transform);
    cmsCloseProfile(DbgThread(), aboveRGB);
    cmsCloseProfile(DbgThread(), sRGB);

    return 1;
}

/**
* Bug reported & fixed. Thanks to Kornel Lesinski for spotting this.
*/
static
int CheckSE(void)
{
    cmsHPROFILE input_profile = Create_AboveRGB();
    cmsHPROFILE output_profile = cmsCreate_sRGBProfile(DbgThread());

    cmsHTRANSFORM tr = cmsCreateTransform(DbgThread(), input_profile, TYPE_RGBA_8, output_profile, TYPE_RGBA_16_SE, INTENT_RELATIVE_COLORIMETRIC, cmsFLAGS_COPY_ALPHA);

    cmsUInt8Number rgba[4] = { 40, 41, 41, 0xfa };
    cmsUInt16Number out[4];

    cmsDoTransform(DbgThread(), tr, rgba, out, 1);
    cmsCloseProfile(DbgThread(), input_profile);
    cmsCloseProfile(DbgThread(), output_profile);
    cmsDeleteTransform(DbgThread(), tr);

    if (out[0] != 0xf622 || out[1] != 0x7f24 || out[2] != 0x7f24)
        return 0;

    return 1;
}

/**
* Bug reported.
*/
static
int CheckForgedMPE(void)
{
    cmsUInt32Number i;
    cmsHPROFILE srcProfile;
    cmsHPROFILE dstProfile;
    cmsColorSpaceSignature srcCS;
    cmsUInt32Number nSrcComponents;
    cmsUInt32Number srcFormat;
    cmsUInt32Number intent = 0;
    cmsUInt32Number flags = 0;
    cmsHTRANSFORM hTransform;
    cmsUInt8Number output[4];

    srcProfile = cmsOpenProfileFromFile(DbgThread(), "bad_mpe.icc", "r");
    if (!srcProfile)
        return 0;

    dstProfile = cmsCreate_sRGBProfile(DbgThread());
    if (!dstProfile) {
        cmsCloseProfile(DbgThread(), srcProfile);
        return 0;
    }

    srcCS = cmsGetColorSpace(DbgThread(), srcProfile);
    nSrcComponents = cmsChannelsOf(DbgThread(), srcCS);

    if (srcCS == cmsSigLabData) {
        srcFormat =
            COLORSPACE_SH(PT_Lab) | CHANNELS_SH(nSrcComponents) | BYTES_SH(0);
    }
    else {
        srcFormat =
            COLORSPACE_SH(PT_ANY) | CHANNELS_SH(nSrcComponents) | BYTES_SH(1);
    }

    cmsSetLogErrorHandler(DbgThread(), ErrorReportingFunction);

    hTransform = cmsCreateTransform(DbgThread(), srcProfile, srcFormat, dstProfile,
        TYPE_BGR_8, intent, flags);
    cmsCloseProfile(DbgThread(), srcProfile);
    cmsCloseProfile(DbgThread(), dstProfile);

    cmsSetLogErrorHandler(DbgThread(), FatalErrorQuit);

    // Should report error
    if (!TrappedError) return 0;

    TrappedError = FALSE;

    // Transform should NOT be created
    if (!hTransform) return 1;

    // Never should reach here
    if (T_BYTES(srcFormat) == 0) {  // 0 means double
        double input[128];
        for (i = 0; i < nSrcComponents; i++)
            input[i] = 0.5f;
        cmsDoTransform(DbgThread(), hTransform, input, output, 1);
    }
    else {
        cmsUInt8Number input[128];
        for (i = 0; i < nSrcComponents; i++)
            input[i] = 128;
        cmsDoTransform(DbgThread(), hTransform, input, output, 1);
    }
    cmsDeleteTransform(DbgThread(), hTransform);

    return 0;
}

/**
* What the self test is trying to do is creating a proofing transform
* with gamut check, so we can getting the coverage of one profile of
* another, i.e. to approximate the gamut intersection. e.g.
* Thanks to Richard Hughes for providing the test
*/
static
int CheckProofingIntersection(void)
{
    cmsHPROFILE profile_null, hnd1, hnd2;
    cmsHTRANSFORM transform;

    hnd1 = cmsCreate_sRGBProfile(DbgThread());
    hnd2 = Create_AboveRGB();

    profile_null = cmsCreateNULLProfile(DbgThread());
    transform = cmsCreateProofingTransform(DbgThread(),
        hnd1,
        TYPE_RGB_FLT,
        profile_null,
        TYPE_GRAY_FLT,
        hnd2,
        INTENT_ABSOLUTE_COLORIMETRIC,
        INTENT_ABSOLUTE_COLORIMETRIC,
        cmsFLAGS_GAMUTCHECK |
        cmsFLAGS_SOFTPROOFING);

    cmsCloseProfile(DbgThread(), hnd1);
    cmsCloseProfile(DbgThread(), hnd2);
    cmsCloseProfile(DbgThread(), profile_null);

    // Failed?
    if (transform == NULL) return 0;

    cmsDeleteTransform(DbgThread(), transform);
    return 1;
}

// --------------------------------------------------------------------------------------------------
// P E R F O R M A N C E   C H E C K S
// --------------------------------------------------------------------------------------------------


typedef struct {cmsUInt8Number r, g, b, a;}    Scanline_rgba8;
typedef struct {cmsUInt16Number r, g, b, a;}   Scanline_rgba16;
typedef struct {cmsFloat32Number r, g, b, a;}  Scanline_rgba32;
typedef struct {cmsUInt8Number r, g, b;}       Scanline_rgb8;
typedef struct {cmsUInt16Number r, g, b;}      Scanline_rgb16;
typedef struct {cmsFloat32Number r, g, b;}     Scanline_rgb32;


static
void TitlePerformance(const char* Txt)
{
    printf("%-45s: ", Txt); fflush(stdout);
}

static
void PrintPerformance(cmsUInt32Number Bytes, cmsUInt32Number SizeOfPixel, cmsFloat64Number diff)
{
    cmsFloat64Number seconds  = (cmsFloat64Number) diff / CLOCKS_PER_SEC;
    cmsFloat64Number mpix_sec = Bytes / (1024.0*1024.0*seconds*SizeOfPixel);

    printf("%#4.3g MPixel/sec.\n", mpix_sec);
    fflush(stdout);
}


static
void SpeedTest32bits(const char * Title, cmsHPROFILE hlcmsProfileIn, cmsHPROFILE hlcmsProfileOut, cmsInt32Number Intent)
{
    cmsInt32Number r, g, b, j;
    clock_t atime;
    cmsFloat64Number diff;
    cmsHTRANSFORM hlcmsxform;
    Scanline_rgba32 *In;
    cmsUInt32Number Mb;
    cmsUInt32Number Interval = 4; // Power of 2 number to increment r,g,b values by in the loops to keep the test duration practically short
    cmsUInt32Number NumPixels;

    if (hlcmsProfileIn == NULL || hlcmsProfileOut == NULL)
        Die("Unable to open profiles");

    hlcmsxform  = cmsCreateTransform(DbgThread(), hlcmsProfileIn, TYPE_RGBA_FLT,
        hlcmsProfileOut, TYPE_RGBA_FLT, Intent, cmsFLAGS_NOCACHE);
    cmsCloseProfile(DbgThread(), hlcmsProfileIn);
    cmsCloseProfile(DbgThread(), hlcmsProfileOut);

    NumPixels = 256 / Interval * 256 / Interval * 256 / Interval;
    Mb = NumPixels * sizeof(Scanline_rgba32);

    In = (Scanline_rgba32 *) malloc(Mb);

    j = 0;
    for (r=0; r < 256; r += Interval)
        for (g=0; g < 256; g += Interval)
            for (b=0; b < 256; b += Interval) {

                In[j].r = r / 256.0f;
                In[j].g = g / 256.0f;
                In[j].b = b / 256.0f;
                In[j].a = (In[j].r + In[j].g + In[j].b) / 3;

                j++;
            }


    TitlePerformance(Title);

    atime = clock();

    cmsDoTransform(DbgThread(), hlcmsxform, In, In, NumPixels);

    diff = clock() - atime;
    free(In);

    PrintPerformance(Mb, sizeof(Scanline_rgba32), diff);
    cmsDeleteTransform(DbgThread(), hlcmsxform);

}


static
void SpeedTest16bits(const char * Title, cmsHPROFILE hlcmsProfileIn, cmsHPROFILE hlcmsProfileOut, cmsInt32Number Intent)
{
    cmsInt32Number r, g, b, j;
    clock_t atime;
    cmsFloat64Number diff;
    cmsHTRANSFORM hlcmsxform;
    Scanline_rgb16 *In;
    cmsUInt32Number Mb;

    if (hlcmsProfileIn == NULL || hlcmsProfileOut == NULL)
        Die("Unable to open profiles");

    hlcmsxform  = cmsCreateTransform(DbgThread(), hlcmsProfileIn, TYPE_RGB_16,
        hlcmsProfileOut, TYPE_RGB_16, Intent, cmsFLAGS_NOCACHE);
    cmsCloseProfile(DbgThread(), hlcmsProfileIn);
    cmsCloseProfile(DbgThread(), hlcmsProfileOut);

    Mb = 256*256*256 * sizeof(Scanline_rgb16);

    In = (Scanline_rgb16*) malloc(Mb);

    j = 0;
    for (r=0; r < 256; r++)
        for (g=0; g < 256; g++)
            for (b=0; b < 256; b++) {

                In[j].r = (cmsUInt16Number) ((r << 8) | r);
                In[j].g = (cmsUInt16Number) ((g << 8) | g);
                In[j].b = (cmsUInt16Number) ((b << 8) | b);

                j++;
            }


    TitlePerformance(Title);

    atime = clock();

    cmsDoTransform(DbgThread(), hlcmsxform, In, In, 256*256*256);

    diff = clock() - atime;
    free(In);

    PrintPerformance(Mb, sizeof(Scanline_rgb16), diff);
    cmsDeleteTransform(DbgThread(), hlcmsxform);

}


static
void SpeedTest32bitsCMYK(const char * Title, cmsHPROFILE hlcmsProfileIn, cmsHPROFILE hlcmsProfileOut)
{
    cmsInt32Number r, g, b, j;
    clock_t atime;
    cmsFloat64Number diff;
    cmsHTRANSFORM hlcmsxform;
    Scanline_rgba32 *In;
    cmsUInt32Number Mb;
    cmsUInt32Number Interval = 4; // Power of 2 number to increment r,g,b values by in the loops to keep the test duration practically short
    cmsUInt32Number NumPixels;

    if (hlcmsProfileIn == NULL || hlcmsProfileOut == NULL)
        Die("Unable to open profiles");

    hlcmsxform  = cmsCreateTransform(DbgThread(), hlcmsProfileIn, TYPE_CMYK_FLT,
        hlcmsProfileOut, TYPE_CMYK_FLT, INTENT_PERCEPTUAL, cmsFLAGS_NOCACHE);
    cmsCloseProfile(DbgThread(), hlcmsProfileIn);
    cmsCloseProfile(DbgThread(), hlcmsProfileOut);

    NumPixels = 256 / Interval * 256 / Interval * 256 / Interval;
    Mb = NumPixels * sizeof(Scanline_rgba32);

    In = (Scanline_rgba32 *) malloc(Mb);

    j = 0;
    for (r=0; r < 256; r += Interval)
        for (g=0; g < 256; g += Interval)
            for (b=0; b < 256; b += Interval) {

                In[j].r = r / 256.0f;
                In[j].g = g / 256.0f;
                In[j].b = b / 256.0f;
                In[j].a = (In[j].r + In[j].g + In[j].b) / 3;

                j++;
            }


    TitlePerformance(Title);

    atime = clock();

    cmsDoTransform(DbgThread(), hlcmsxform, In, In, NumPixels);

    diff = clock() - atime;

    free(In);

    PrintPerformance(Mb, sizeof(Scanline_rgba32), diff);

    cmsDeleteTransform(DbgThread(), hlcmsxform);

}


static
void SpeedTest16bitsCMYK(const char * Title, cmsHPROFILE hlcmsProfileIn, cmsHPROFILE hlcmsProfileOut)
{
    cmsInt32Number r, g, b, j;
    clock_t atime;
    cmsFloat64Number diff;
    cmsHTRANSFORM hlcmsxform;
    Scanline_rgba16 *In;
    cmsUInt32Number Mb;

    if (hlcmsProfileIn == NULL || hlcmsProfileOut == NULL)
        Die("Unable to open profiles");

    hlcmsxform  = cmsCreateTransform(DbgThread(), hlcmsProfileIn, TYPE_CMYK_16,
        hlcmsProfileOut, TYPE_CMYK_16, INTENT_PERCEPTUAL,  cmsFLAGS_NOCACHE);
    cmsCloseProfile(DbgThread(), hlcmsProfileIn);
    cmsCloseProfile(DbgThread(), hlcmsProfileOut);

    Mb = 256*256*256*sizeof(Scanline_rgba16);

    In = (Scanline_rgba16*) malloc(Mb);

    j = 0;
    for (r=0; r < 256; r++)
        for (g=0; g < 256; g++)
            for (b=0; b < 256; b++) {

                In[j].r = (cmsUInt16Number) ((r << 8) | r);
                In[j].g = (cmsUInt16Number) ((g << 8) | g);
                In[j].b = (cmsUInt16Number) ((b << 8) | b);
                In[j].a = 0;

                j++;
            }


    TitlePerformance(Title);

    atime = clock();

    cmsDoTransform(DbgThread(), hlcmsxform, In, In, 256*256*256);

    diff = clock() - atime;

    free(In);

    PrintPerformance(Mb, sizeof(Scanline_rgba16), diff);

    cmsDeleteTransform(DbgThread(), hlcmsxform);

}


static
void SpeedTest8bits(const char * Title, cmsHPROFILE hlcmsProfileIn, cmsHPROFILE hlcmsProfileOut, cmsInt32Number Intent)
{
    cmsInt32Number r, g, b, j;
    clock_t atime;
    cmsFloat64Number diff;
    cmsHTRANSFORM hlcmsxform;
    Scanline_rgb8 *In;
    cmsUInt32Number Mb;

    if (hlcmsProfileIn == NULL || hlcmsProfileOut == NULL)
        Die("Unable to open profiles");

    hlcmsxform  = cmsCreateTransform(DbgThread(), hlcmsProfileIn, TYPE_RGB_8,
                            hlcmsProfileOut, TYPE_RGB_8, Intent, cmsFLAGS_NOCACHE);
    cmsCloseProfile(DbgThread(), hlcmsProfileIn);
    cmsCloseProfile(DbgThread(), hlcmsProfileOut);

    Mb = 256*256*256*sizeof(Scanline_rgb8);

    In = (Scanline_rgb8*) malloc(Mb);

    j = 0;
    for (r=0; r < 256; r++)
        for (g=0; g < 256; g++)
            for (b=0; b < 256; b++) {

        In[j].r = (cmsUInt8Number) r;
        In[j].g = (cmsUInt8Number) g;
        In[j].b = (cmsUInt8Number) b;

        j++;
    }

    TitlePerformance(Title);

    atime = clock();

    cmsDoTransform(DbgThread(), hlcmsxform, In, In, 256*256*256);

    diff = clock() - atime;

    free(In);

    PrintPerformance(Mb, sizeof(Scanline_rgb8), diff);

    cmsDeleteTransform(DbgThread(), hlcmsxform);

}


static
void SpeedTest8bitsCMYK(const char * Title, cmsHPROFILE hlcmsProfileIn, cmsHPROFILE hlcmsProfileOut)
{
    cmsInt32Number r, g, b, j;
    clock_t atime;
    cmsFloat64Number diff;
    cmsHTRANSFORM hlcmsxform;
    Scanline_rgba8 *In;
    cmsUInt32Number Mb;

    if (hlcmsProfileIn == NULL || hlcmsProfileOut == NULL)
        Die("Unable to open profiles");

    hlcmsxform  = cmsCreateTransform(DbgThread(), hlcmsProfileIn, TYPE_CMYK_8,
                        hlcmsProfileOut, TYPE_CMYK_8, INTENT_PERCEPTUAL, cmsFLAGS_NOCACHE);
    cmsCloseProfile(DbgThread(), hlcmsProfileIn);
    cmsCloseProfile(DbgThread(), hlcmsProfileOut);

    Mb = 256*256*256*sizeof(Scanline_rgba8);

    In = (Scanline_rgba8*) malloc(Mb);

    j = 0;
    for (r=0; r < 256; r++)
        for (g=0; g < 256; g++)
            for (b=0; b < 256; b++) {

        In[j].r = (cmsUInt8Number) r;
        In[j].g = (cmsUInt8Number) g;
        In[j].b = (cmsUInt8Number) b;
        In[j].a = (cmsUInt8Number) 0;

        j++;
    }

    TitlePerformance(Title);

    atime = clock();

    cmsDoTransform(DbgThread(), hlcmsxform, In, In, 256*256*256);

    diff = clock() - atime;

    free(In);

    PrintPerformance(Mb, sizeof(Scanline_rgba8), diff);


    cmsDeleteTransform(DbgThread(), hlcmsxform);

}


static
void SpeedTest32bitsGray(const char * Title, cmsHPROFILE hlcmsProfileIn, cmsHPROFILE hlcmsProfileOut, cmsInt32Number Intent)
{
    cmsInt32Number r, g, b, j;
    clock_t atime;
    cmsFloat64Number diff;
    cmsHTRANSFORM hlcmsxform;
    cmsFloat32Number *In;
    cmsUInt32Number Mb;
    cmsUInt32Number Interval = 4; // Power of 2 number to increment r,g,b values by in the loops to keep the test duration practically short
    cmsUInt32Number NumPixels;

    if (hlcmsProfileIn == NULL || hlcmsProfileOut == NULL)
        Die("Unable to open profiles");

    hlcmsxform  = cmsCreateTransform(DbgThread(), hlcmsProfileIn,
        TYPE_GRAY_FLT, hlcmsProfileOut, TYPE_GRAY_FLT, Intent, cmsFLAGS_NOCACHE);
    cmsCloseProfile(DbgThread(), hlcmsProfileIn);
    cmsCloseProfile(DbgThread(), hlcmsProfileOut);

    NumPixels = 256 / Interval * 256 / Interval * 256 / Interval;
    Mb = NumPixels * sizeof(cmsFloat32Number);

    In = (cmsFloat32Number*) malloc(Mb);

    j = 0;
    for (r = 0; r < 256; r += Interval)
        for (g = 0; g < 256; g += Interval)
            for (b = 0; b < 256; b += Interval) {

                In[j] = ((r + g + b) / 768.0f);

                j++;
            }

    TitlePerformance(Title);

    atime = clock();

    cmsDoTransform(DbgThread(), hlcmsxform, In, In, NumPixels);

    diff = clock() - atime;
    free(In);

    PrintPerformance(Mb, sizeof(cmsFloat32Number), diff);
    cmsDeleteTransform(DbgThread(), hlcmsxform);
}


static
void SpeedTest16bitsGray(const char * Title, cmsHPROFILE hlcmsProfileIn, cmsHPROFILE hlcmsProfileOut, cmsInt32Number Intent)
{
    cmsInt32Number r, g, b, j;
    clock_t atime;
    cmsFloat64Number diff;
    cmsHTRANSFORM hlcmsxform;
    cmsUInt16Number *In;
    cmsUInt32Number Mb;

    if (hlcmsProfileIn == NULL || hlcmsProfileOut == NULL)
        Die("Unable to open profiles");

    hlcmsxform  = cmsCreateTransform(DbgThread(), hlcmsProfileIn,
        TYPE_GRAY_16, hlcmsProfileOut, TYPE_GRAY_16, Intent, cmsFLAGS_NOCACHE);
    cmsCloseProfile(DbgThread(), hlcmsProfileIn);
    cmsCloseProfile(DbgThread(), hlcmsProfileOut);
    Mb = 256*256*256 * sizeof(cmsUInt16Number);

    In = (cmsUInt16Number *) malloc(Mb);

    j = 0;
    for (r=0; r < 256; r++)
        for (g=0; g < 256; g++)
            for (b=0; b < 256; b++) {

                In[j] = (cmsUInt16Number) ((r + g + b) / 3);

                j++;
            }

    TitlePerformance(Title);

    atime = clock();

    cmsDoTransform(DbgThread(), hlcmsxform, In, In, 256*256*256);

    diff = clock() - atime;
    free(In);

    PrintPerformance(Mb, sizeof(cmsUInt16Number), diff);
    cmsDeleteTransform(DbgThread(), hlcmsxform);
}


static
void SpeedTest8bitsGray(const char * Title, cmsHPROFILE hlcmsProfileIn, cmsHPROFILE hlcmsProfileOut, cmsInt32Number Intent)
{
    cmsInt32Number r, g, b, j;
    clock_t atime;
    cmsFloat64Number diff;
    cmsHTRANSFORM hlcmsxform;
    cmsUInt8Number *In;
    cmsUInt32Number Mb;


    if (hlcmsProfileIn == NULL || hlcmsProfileOut == NULL)
        Die("Unable to open profiles");

    hlcmsxform  = cmsCreateTransform(DbgThread(), hlcmsProfileIn,
        TYPE_GRAY_8, hlcmsProfileOut, TYPE_GRAY_8, Intent, cmsFLAGS_NOCACHE);
    cmsCloseProfile(DbgThread(), hlcmsProfileIn);
    cmsCloseProfile(DbgThread(), hlcmsProfileOut);
    Mb = 256*256*256;

    In = (cmsUInt8Number*) malloc(Mb);

    j = 0;
    for (r=0; r < 256; r++)
        for (g=0; g < 256; g++)
            for (b=0; b < 256; b++) {

                In[j] = (cmsUInt8Number) r;

                j++;
            }

    TitlePerformance(Title);

    atime = clock();

    cmsDoTransform(DbgThread(), hlcmsxform, In, In, 256*256*256);

    diff = clock() - atime;
    free(In);

    PrintPerformance(Mb, sizeof(cmsUInt8Number), diff);
    cmsDeleteTransform(DbgThread(), hlcmsxform);
}


static
cmsHPROFILE CreateCurves(void)
{
    cmsToneCurve* Gamma = cmsBuildGamma(DbgThread(), 1.1);
    cmsToneCurve* Transfer[3];
    cmsHPROFILE h;

    Transfer[0] = Transfer[1] = Transfer[2] = Gamma;
    h = cmsCreateLinearizationDeviceLink(DbgThread(), cmsSigRgbData, Transfer);

    cmsFreeToneCurve(DbgThread(), Gamma);

    return h;
}


static
void SpeedTest(void)
{
    printf("\n\nP E R F O R M A N C E   T E S T S\n");
    printf(    "=================================\n\n");
    fflush(stdout);

    SpeedTest8bits("8 bits on CLUT profiles",
        cmsOpenProfileFromFile(DbgThread(), "test5.icc", "r"),
        cmsOpenProfileFromFile(DbgThread(), "test3.icc", "r"),
        INTENT_PERCEPTUAL);

    SpeedTest16bits("16 bits on CLUT profiles",
        cmsOpenProfileFromFile(DbgThread(), "test5.icc", "r"),
        cmsOpenProfileFromFile(DbgThread(), "test3.icc", "r"), INTENT_PERCEPTUAL);

    SpeedTest32bits("32 bits on CLUT profiles",
        cmsOpenProfileFromFile(DbgThread(), "test5.icc", "r"),
        cmsOpenProfileFromFile(DbgThread(), "test3.icc", "r"), INTENT_PERCEPTUAL);

    printf("\n");

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

    SpeedTest8bits("8 bits on Matrix-Shaper profiles",
        cmsOpenProfileFromFile(DbgThread(), "test5.icc", "r"),
        cmsOpenProfileFromFile(DbgThread(), "aRGBlcms2.icc", "r"),
        INTENT_PERCEPTUAL);

    SpeedTest16bits("16 bits on Matrix-Shaper profiles",
       cmsOpenProfileFromFile(DbgThread(), "test5.icc", "r"),
        cmsOpenProfileFromFile(DbgThread(), "aRGBlcms2.icc", "r"),
        INTENT_PERCEPTUAL);

    SpeedTest32bits("32 bits on Matrix-Shaper profiles",
       cmsOpenProfileFromFile(DbgThread(), "test5.icc", "r"),
        cmsOpenProfileFromFile(DbgThread(), "aRGBlcms2.icc", "r"),
        INTENT_PERCEPTUAL);

    printf("\n");

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

    SpeedTest8bits("8 bits on SAME Matrix-Shaper profiles",
        cmsOpenProfileFromFile(DbgThread(), "test5.icc", "r"),
        cmsOpenProfileFromFile(DbgThread(), "test5.icc", "r"),
        INTENT_PERCEPTUAL);

    SpeedTest16bits("16 bits on SAME Matrix-Shaper profiles",
        cmsOpenProfileFromFile(DbgThread(), "aRGBlcms2.icc", "r"),
        cmsOpenProfileFromFile(DbgThread(), "aRGBlcms2.icc", "r"),
        INTENT_PERCEPTUAL);

    SpeedTest32bits("32 bits on SAME Matrix-Shaper profiles",
        cmsOpenProfileFromFile(DbgThread(), "aRGBlcms2.icc", "r"),
        cmsOpenProfileFromFile(DbgThread(), "aRGBlcms2.icc", "r"),
        INTENT_PERCEPTUAL);

    printf("\n");

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

    SpeedTest8bits("8 bits on Matrix-Shaper profiles (AbsCol)",
       cmsOpenProfileFromFile(DbgThread(), "test5.icc", "r"),
       cmsOpenProfileFromFile(DbgThread(), "aRGBlcms2.icc", "r"),
        INTENT_ABSOLUTE_COLORIMETRIC);

    SpeedTest16bits("16 bits on Matrix-Shaper profiles (AbsCol)",
       cmsOpenProfileFromFile(DbgThread(), "test5.icc", "r"),
       cmsOpenProfileFromFile(DbgThread(), "aRGBlcms2.icc", "r"),
        INTENT_ABSOLUTE_COLORIMETRIC);

    SpeedTest32bits("32 bits on Matrix-Shaper profiles (AbsCol)",
       cmsOpenProfileFromFile(DbgThread(), "test5.icc", "r"),
       cmsOpenProfileFromFile(DbgThread(), "aRGBlcms2.icc", "r"),
        INTENT_ABSOLUTE_COLORIMETRIC);

    printf("\n");

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

    SpeedTest8bits("8 bits on curves",
        CreateCurves(),
        CreateCurves(),
        INTENT_PERCEPTUAL);

    SpeedTest16bits("16 bits on curves",
        CreateCurves(),
        CreateCurves(),
        INTENT_PERCEPTUAL);

    SpeedTest32bits("32 bits on curves",
        CreateCurves(),
        CreateCurves(),
        INTENT_PERCEPTUAL);

    printf("\n");

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

    SpeedTest8bitsCMYK("8 bits on CMYK profiles",
        cmsOpenProfileFromFile(DbgThread(), "test1.icc", "r"),
        cmsOpenProfileFromFile(DbgThread(), "test2.icc", "r"));

    SpeedTest16bitsCMYK("16 bits on CMYK profiles",
        cmsOpenProfileFromFile(DbgThread(), "test1.icc", "r"),
        cmsOpenProfileFromFile(DbgThread(), "test2.icc", "r"));

    SpeedTest32bitsCMYK("32 bits on CMYK profiles",
        cmsOpenProfileFromFile(DbgThread(), "test1.icc", "r"),
        cmsOpenProfileFromFile(DbgThread(), "test2.icc", "r"));

    printf("\n");

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

    SpeedTest8bitsGray("8 bits on gray-to gray",
        cmsOpenProfileFromFile(DbgThread(), "gray3lcms2.icc", "r"),
        cmsOpenProfileFromFile(DbgThread(), "graylcms2.icc", "r"), INTENT_RELATIVE_COLORIMETRIC);

    SpeedTest16bitsGray("16 bits on gray-to gray",
        cmsOpenProfileFromFile(DbgThread(), "gray3lcms2.icc", "r"),
        cmsOpenProfileFromFile(DbgThread(), "graylcms2.icc", "r"), INTENT_RELATIVE_COLORIMETRIC);

    SpeedTest32bitsGray("32 bits on gray-to gray",
        cmsOpenProfileFromFile(DbgThread(), "gray3lcms2.icc", "r"),
        cmsOpenProfileFromFile(DbgThread(), "graylcms2.icc", "r"), INTENT_RELATIVE_COLORIMETRIC);

    printf("\n");

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

    SpeedTest8bitsGray("8 bits on gray-to-lab gray",
        cmsOpenProfileFromFile(DbgThread(), "graylcms2.icc", "r"),
        cmsOpenProfileFromFile(DbgThread(), "glablcms2.icc", "r"), INTENT_RELATIVE_COLORIMETRIC);

    SpeedTest16bitsGray("16 bits on gray-to-lab gray",
        cmsOpenProfileFromFile(DbgThread(), "graylcms2.icc", "r"),
        cmsOpenProfileFromFile(DbgThread(), "glablcms2.icc", "r"), INTENT_RELATIVE_COLORIMETRIC);

    SpeedTest32bitsGray("32 bits on gray-to-lab gray",
        cmsOpenProfileFromFile(DbgThread(), "graylcms2.icc", "r"),
        cmsOpenProfileFromFile(DbgThread(), "glablcms2.icc", "r"), INTENT_RELATIVE_COLORIMETRIC);

    printf("\n");

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

    SpeedTest8bitsGray("8 bits on SAME gray-to-gray",
        cmsOpenProfileFromFile(DbgThread(), "graylcms2.icc", "r"),
        cmsOpenProfileFromFile(DbgThread(), "graylcms2.icc", "r"), INTENT_PERCEPTUAL);

    SpeedTest16bitsGray("16 bits on SAME gray-to-gray",
        cmsOpenProfileFromFile(DbgThread(), "graylcms2.icc", "r"),
        cmsOpenProfileFromFile(DbgThread(), "graylcms2.icc", "r"), INTENT_PERCEPTUAL);

    SpeedTest32bitsGray("32 bits on SAME gray-to-gray",
        cmsOpenProfileFromFile(DbgThread(), "graylcms2.icc", "r"),
        cmsOpenProfileFromFile(DbgThread(), "graylcms2.icc", "r"), INTENT_PERCEPTUAL);

    printf("\n");
}


// -----------------------------------------------------------------------------------------------------


// Print the supported intents
static
void PrintSupportedIntents(void)
{
    cmsUInt32Number n, i;
    cmsUInt32Number Codes[200];
    char* Descriptions[200];

    n = cmsGetSupportedIntents(DbgThread(), 200, Codes, Descriptions);

    printf("Supported intents:\n");
    for (i=0; i < n; i++) {
        printf("\t%u - %s\n", Codes[i], Descriptions[i]);
    }
    printf("\n");
}



// ---------------------------------------------------------------------------------------

#ifdef LCMS_FAST_EXTENSIONS
    void* cmsFast8Bitextensions(void);
#endif

int main(int argc, char* argv[])
{
    cmsInt32Number Exhaustive = 0;
    cmsInt32Number DoSpeedTests = 1;
    cmsInt32Number DoCheckTests = 1;
    cmsInt32Number DoPluginTests = 1;
    cmsInt32Number DoZooTests = 0;

#ifdef _MSC_VER
    _CrtSetDbgFlag ( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif


    // First of all, check for the right header
   if (cmsGetEncodedCMMversion() != LCMS_VERSION) {
          Die("Oops, you are mixing header and shared lib!\nHeader version reports to be '%d' and shared lib '%d'\n", LCMS_VERSION, cmsGetEncodedCMMversion());
   }

    printf("LittleCMS %2.2f test bed %s %s\n\n", LCMS_VERSION / 1000.0, __DATE__, __TIME__);

    if ((argc == 2) && strcmp(argv[1], "--exhaustive") == 0) {

        Exhaustive = 1;
        printf("Running exhaustive tests (will take a while...)\n\n");
    }

#ifdef LCMS_FAST_EXTENSIONS
   printf("Installing fast 8 bit extension ...");
   cmsPlugin(cmsFast8Bitextensions());
   printf("done.\n");
#endif


    printf("Installing debug memory plug-in ... ");
    cmsPlugin(DbgThread(), &DebugMemHandler);
    printf("done.\n");

    printf("Installing error logger ... ");
    cmsSetLogErrorHandler(DbgThread(), FatalErrorQuit);
    printf("done.\n");

    PrintSupportedIntents();

    Check("Base types", CheckBaseTypes);
    Check("endianness", CheckEndianness);
    Check("quick floor", CheckQuickFloor);
    Check("quick floor word", CheckQuickFloorWord);
    Check("Fixed point 15.16 representation", CheckFixedPoint15_16);
    Check("Fixed point 8.8 representation", CheckFixedPoint8_8);
    Check("D50 roundtrip", CheckD50Roundtrip);

    // Create utility profiles
    if (DoCheckTests || DoSpeedTests)
        Check("Creation of test profiles", CreateTestProfiles);

    if (DoCheckTests) {

    // Forward 1D interpolation
    Check("1D interpolation in 2pt tables", Check1DLERP2);
    Check("1D interpolation in 3pt tables", Check1DLERP3);
    Check("1D interpolation in 4pt tables", Check1DLERP4);
    Check("1D interpolation in 6pt tables", Check1DLERP6);
    Check("1D interpolation in 18pt tables", Check1DLERP18);
    Check("1D interpolation in descending 2pt tables", Check1DLERP2Down);
    Check("1D interpolation in descending 3pt tables", Check1DLERP3Down);
    Check("1D interpolation in descending 6pt tables", Check1DLERP6Down);
    Check("1D interpolation in descending 18pt tables", Check1DLERP18Down);

    if (Exhaustive) {

        Check("1D interpolation in n tables", ExhaustiveCheck1DLERP);
        Check("1D interpolation in descending tables", ExhaustiveCheck1DLERPDown);
    }

    // Forward 3D interpolation
    Check("3D interpolation Tetrahedral (float) ", Check3DinterpolationFloatTetrahedral);
    Check("3D interpolation Trilinear (float) ", Check3DinterpolationFloatTrilinear);
    Check("3D interpolation Tetrahedral (16) ", Check3DinterpolationTetrahedral16);
    Check("3D interpolation Trilinear (16) ", Check3DinterpolationTrilinear16);

    if (Exhaustive) {

        Check("Exhaustive 3D interpolation Tetrahedral (float) ", ExaustiveCheck3DinterpolationFloatTetrahedral);
        Check("Exhaustive 3D interpolation Trilinear  (float) ", ExaustiveCheck3DinterpolationFloatTrilinear);
        Check("Exhaustive 3D interpolation Tetrahedral (16) ", ExhaustiveCheck3DinterpolationTetrahedral16);
        Check("Exhaustive 3D interpolation Trilinear (16) ", ExhaustiveCheck3DinterpolationTrilinear16);
    }

    Check("Reverse interpolation 3 -> 3", CheckReverseInterpolation3x3);
    Check("Reverse interpolation 4 -> 3", CheckReverseInterpolation4x3);


    // High dimensionality interpolation

    Check("3D interpolation", Check3Dinterp);
    Check("3D interpolation with granularity", Check3DinterpGranular);
    Check("4D interpolation", Check4Dinterp);
    Check("4D interpolation with granularity", Check4DinterpGranular);
    Check("5D interpolation with granularity", Check5DinterpGranular);
    Check("6D interpolation with granularity", Check6DinterpGranular);
    Check("7D interpolation with granularity", Check7DinterpGranular);
    Check("8D interpolation with granularity", Check8DinterpGranular);

    // Encoding of colorspaces
    Check("Lab to LCh and back (float only) ", CheckLab2LCh);
    Check("Lab to XYZ and back (float only) ", CheckLab2XYZ);
    Check("Lab to xyY and back (float only) ", CheckLab2xyY);
    Check("Lab V2 encoding", CheckLabV2encoding);
    Check("Lab V4 encoding", CheckLabV4encoding);

    // BlackBody
    Check("Blackbody radiator", CheckTemp2CHRM);

    // Tone curves
    Check("Linear gamma curves (16 bits)", CheckGammaCreation16);
    Check("Linear gamma curves (float)", CheckGammaCreationFlt);

    Check("Curve 1.8 (float)", CheckGamma18);
    Check("Curve 2.2 (float)", CheckGamma22);
    Check("Curve 3.0 (float)", CheckGamma30);

    Check("Curve 1.8 (table)", CheckGamma18Table);
    Check("Curve 2.2 (table)", CheckGamma22Table);
    Check("Curve 3.0 (table)", CheckGamma30Table);

    Check("Curve 1.8 (word table)", CheckGamma18TableWord);
    Check("Curve 2.2 (word table)", CheckGamma22TableWord);
    Check("Curve 3.0 (word table)", CheckGamma30TableWord);

    Check("Parametric curves", CheckParametricToneCurves);

    Check("Join curves", CheckJointCurves);
    Check("Join curves descending", CheckJointCurvesDescending);
    Check("Join curves degenerated", CheckReverseDegenerated);
    Check("Join curves sRGB (Float)", CheckJointFloatCurves_sRGB);
    Check("Join curves sRGB (16 bits)", CheckJoint16Curves_sRGB);
    Check("Join curves sigmoidal", CheckJointCurvesSShaped);

    // LUT basics
    Check("LUT creation & dup", CheckLUTcreation);
    Check("1 Stage LUT ", Check1StageLUT);
    Check("2 Stage LUT ", Check2StageLUT);
    Check("2 Stage LUT (16 bits)", Check2Stage16LUT);
    Check("3 Stage LUT ", Check3StageLUT);
    Check("3 Stage LUT (16 bits)", Check3Stage16LUT);
    Check("4 Stage LUT ", Check4StageLUT);
    Check("4 Stage LUT (16 bits)", Check4Stage16LUT);
    Check("5 Stage LUT ", Check5StageLUT);
    Check("5 Stage LUT (16 bits) ", Check5Stage16LUT);
    Check("6 Stage LUT ", Check6StageLUT);
    Check("6 Stage LUT (16 bits) ", Check6Stage16LUT);

    // LUT operation
    Check("Lab to Lab LUT (float only) ", CheckLab2LabLUT);
    Check("XYZ to XYZ LUT (float only) ", CheckXYZ2XYZLUT);
    Check("Lab to Lab MAT LUT (float only) ", CheckLab2LabMatLUT);
    Check("Named Color LUT", CheckNamedColorLUT);
    Check("Usual formatters", CheckFormatters16);
    Check("Floating point formatters", CheckFormattersFloat);

#ifndef CMS_NO_HALF_SUPPORT
    Check("HALF formatters", CheckFormattersHalf);
#endif
    // ChangeBuffersFormat
    Check("ChangeBuffersFormat", CheckChangeBufferFormat);

    // MLU
    Check("Multilocalized Unicode", CheckMLU);

    // Named color
    Check("Named color lists", CheckNamedColorList);

    // Profile I/O (this one is huge!)
    Check("Profile creation", CheckProfileCreation);
    Check("Header version", CheckVersionHeaderWriting);
    Check("Multilocalized profile", CheckMultilocalizedProfile);

    // Error reporting
    Check("Error reporting on bad profiles", CheckErrReportingOnBadProfiles);
    Check("Error reporting on bad transforms", CheckErrReportingOnBadTransforms);

    // Transforms
    Check("Curves only transforms", CheckCurvesOnlyTransforms);
    Check("Float Lab->Lab transforms", CheckFloatLabTransforms);
    Check("Encoded Lab->Lab transforms", CheckEncodedLabTransforms);
    Check("Stored identities", CheckStoredIdentities);

    Check("Matrix-shaper transform (float)",   CheckMatrixShaperXFORMFloat);
    Check("Matrix-shaper transform (16 bits)", CheckMatrixShaperXFORM16);
    Check("Matrix-shaper transform (8 bits)",  CheckMatrixShaperXFORM8);

    Check("Primaries of sRGB", CheckRGBPrimaries);

    // Known values
    Check("Known values across matrix-shaper", Chack_sRGB_Float);
    Check("Gray input profile", CheckInputGray);
    Check("Gray Lab input profile", CheckLabInputGray);
    Check("Gray output profile", CheckOutputGray);
    Check("Gray Lab output profile", CheckLabOutputGray);

    Check("Matrix-shaper proofing transform (float)",   CheckProofingXFORMFloat);
    Check("Matrix-shaper proofing transform (16 bits)",  CheckProofingXFORM16);

    Check("Gamut check", CheckGamutCheck);

    Check("CMYK roundtrip on perceptual transform",   CheckCMYKRoundtrip);

    Check("CMYK perceptual transform",   CheckCMYKPerceptual);
    // Check("CMYK rel.col. transform",   CheckCMYKRelCol);

    Check("Black ink only preservation", CheckKOnlyBlackPreserving);
    Check("Black plane preservation", CheckKPlaneBlackPreserving);


    Check("Deciding curve types", CheckV4gamma);

    Check("Black point detection", CheckBlackPoint);
    Check("TAC detection", CheckTAC);

    Check("CGATS parser", CheckCGATS);
    Check("CGATS parser on junk", CheckCGATS2);
    Check("CGATS parser on overflow", CheckCGATS_Overflow);
    Check("PostScript generator", CheckPostScript);
    Check("Segment maxima GBD", CheckGBD);
    Check("MD5 digest", CheckMD5);
    Check("Linking", CheckLinking);
    Check("floating point tags on XYZ", CheckFloatXYZ);
    Check("RGB->Lab->RGB with alpha on FLT", ChecksRGB2LabFLT);
    Check("Parametric curve on Rec709", CheckParametricRec709);
    Check("Floating Point sampled curve with non-zero start", CheckFloatSamples);
    Check("Floating Point segmented curve with short sampled segment", CheckFloatSegments);
    Check("Read RAW portions", CheckReadRAW);
    Check("Check MetaTag", CheckMeta);
    Check("Null transform on floats", CheckFloatNULLxform);
    Check("Set free a tag", CheckRemoveTag);
    Check("Matrix simplification", CheckMatrixSimplify);
    Check("Planar 8 optimization", CheckPlanar8opt);
    Check("Swap endian feature", CheckSE);
    Check("Transform line stride RGB", CheckTransformLineStride);
    Check("Forged MPE profile", CheckForgedMPE);
    Check("Proofing intersection", CheckProofingIntersection);
    }

    if (DoPluginTests)
    {

        Check("Context memory handling", CheckAllocContext);
        Check("Simple context functionality", CheckSimpleContext);
        Check("Alarm codes context", CheckAlarmColorsContext);
        Check("Adaptation state context", CheckAdaptationStateContext);
        Check("1D interpolation plugin", CheckInterp1DPlugin);
        Check("3D interpolation plugin", CheckInterp3DPlugin);
        Check("Parametric curve plugin", CheckParametricCurvePlugin);
        Check("Formatters plugin",       CheckFormattersPlugin);
        Check("Tag type plugin",         CheckTagTypePlugin);
        Check("MPE type plugin",         CheckMPEPlugin);
        Check("Optimization plugin",     CheckOptimizationPlugin);
        Check("Rendering intent plugin", CheckIntentPlugin);
        Check("Full transform plugin",   CheckTransformPlugin);
        Check("Mutex plugin",            CheckMutexPlugin);

    }


    if (DoSpeedTests)
        SpeedTest();


#ifdef CMS_IS_WINDOWS_
    if (DoZooTests)
         CheckProfileZOO(NULL);
#endif

    DebugMemPrintTotals();

    cmsUnregisterPlugins(DbgThread());

    // Cleanup
    if (DoCheckTests || DoSpeedTests)
        RemoveTestProfiles();

   return TotalFail;
}
