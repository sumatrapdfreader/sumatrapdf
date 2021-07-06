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

#include "utils.h"

#ifndef _MSC_VER
#    include <unistd.h>
#endif

#ifdef CMS_IS_WINDOWS_
#    include <io.h>
#endif

#define MAX_INPUT_BUFFER 4096

// Global options

static cmsBool           InHexa                 = FALSE;
static cmsBool           GamutCheck             = FALSE;
static cmsBool           Width16                = FALSE;
static cmsBool           BlackPointCompensation = FALSE;
static cmsBool           lIsDeviceLink          = FALSE;
static cmsBool           lQuantize              = FALSE;
static cmsBool           lUnbounded             = TRUE;
static cmsBool           lIsFloat               = TRUE;

static cmsUInt32Number   Intent           = INTENT_PERCEPTUAL;
static cmsUInt32Number   ProofingIntent   = INTENT_PERCEPTUAL;

static int PrecalcMode  = 0;

// --------------------------------------------------------------

static char *cInProf   = NULL;
static char *cOutProf  = NULL;
static char *cProofing = NULL;

static char *IncludePart = NULL;

static cmsHANDLE hIT8in = NULL;        // CGATS input
static cmsHANDLE hIT8out = NULL;       // CGATS output

static char CGATSPatch[1024];   // Actual Patch Name
static char CGATSoutFilename[cmsMAX_PATH];

static int nMaxPatches;

static cmsHTRANSFORM hTrans, hTransXYZ, hTransLab;
static cmsBool InputNamedColor = FALSE;

static cmsColorSpaceSignature InputColorSpace, OutputColorSpace;

static cmsNAMEDCOLORLIST* InputColorant = NULL;
static cmsNAMEDCOLORLIST* OutputColorant = NULL;

static cmsFloat64Number InputRange, OutputRange;


// isatty replacement
#ifdef _MSC_VER
#define xisatty(x) _isatty( _fileno( (x) ) )
#else
#define xisatty(x) isatty( fileno( (x) ) )
#endif

//---------------------------------------------------------------------------------------------------

// Print usage to stderr
static
void Help(void)
{

    fprintf(stderr, "usage: transicc [flags] [CGATS input] [CGATS output]\n\n");

    fprintf(stderr, "flags:\n\n");
    fprintf(stderr, "-v<0..3> - Verbosity level\n");

    fprintf(stderr, "-e[op] - Encoded representation of numbers\n");
    fprintf(stderr, "\t-w - use 16 bits\n");
    fprintf(stderr, "\t-x - Hexadecimal\n\n");

    fprintf(stderr, "-s - bounded mode (clip negatives and highlights)\n");
    fprintf(stderr, "-q - Quantize (round decimals)\n\n");

    fprintf(stderr, "-i<profile> - Input profile (defaults to sRGB)\n");
    fprintf(stderr, "-o<profile> - Output profile (defaults to sRGB)\n");
    fprintf(stderr, "-l<profile> - Transform by device-link profile\n");

    PrintBuiltins();

    PrintRenderingIntents(NULL);

    fprintf(stderr, "\n");

    fprintf(stderr, "-d<0..1> - Observer adaptation state (abs.col. only)\n\n");

    fprintf(stderr, "-b - Black point compensation\n");

    fprintf(stderr, "-c<0,1,2,3> Precalculates transform (0=Off, 1=Normal, 2=Hi-res, 3=LoRes)\n\n");
    fprintf(stderr, "-n - Terse output, intended for pipe usage\n");

    fprintf(stderr, "-p<profile> - Soft proof profile\n");
    fprintf(stderr, "-m<0,1,2,3> - Soft proof intent\n");
    fprintf(stderr, "-g - Marks out-of-gamut colors on softproof\n\n");



    fprintf(stderr, "This program is intended to be a demo of the Little CMS\n"
        "color engine. Both lcms and this program are open source.\n"
        "You can obtain both in source code at https://www.littlecms.com\n"
        "For suggestions, comments, bug reports etc. send mail to\n"
        "info@littlecms.com\n\n");

}



// The toggles stuff

static
void HandleSwitches(cmsContext ContextID, int argc, char *argv[])
{
    int s;

    while ((s = xgetopt(argc, argv,
        "bBC:c:d:D:eEgGI:i:L:l:m:M:nNO:o:p:P:QqSsT:t:V:v:WwxX!:-:")) != EOF) {

    switch (s){

        case '-':
            if (strcmp(xoptarg, "help") == 0)
            {
                Help();
                exit(0);
            }
            else
            {
                FatalError("Unknown option - run without args to see valid ones.\n");
            }
            break;

        case '!':
            IncludePart = xoptarg;
            break;

        case 'b':
        case 'B':
            BlackPointCompensation = TRUE;
            break;

        case 'c':
        case 'C':
            PrecalcMode = atoi(xoptarg);
            if (PrecalcMode < 0 || PrecalcMode > 3)
                FatalError("Unknown precalc mode '%d'", PrecalcMode);
            break;

        case 'd':
        case 'D': {
            cmsFloat64Number ObserverAdaptationState = atof(xoptarg);
            if (ObserverAdaptationState < 0 ||
                ObserverAdaptationState > 1.0)
                FatalError("Adaptation states should be between 0 and 1");

            cmsSetAdaptationState(ContextID, ObserverAdaptationState);
                  }
                  break;

        case 'e':
        case 'E':
            lIsFloat = FALSE;
            break;

        case 'g':
        case 'G':
            GamutCheck = TRUE;
            break;

        case 'i':
        case 'I':
            if (lIsDeviceLink)
                FatalError("icctrans: Device-link already specified");

            cInProf = xoptarg;
            break;

        case 'l':
        case 'L':
            cInProf = xoptarg;
            lIsDeviceLink = TRUE;
            break;

            // No extra intents for proofing
        case 'm':
        case 'M':
            ProofingIntent = atoi(xoptarg);
            if (ProofingIntent > 3)
                FatalError("Unknown Proofing Intent '%d'", ProofingIntent);
            break;

            // For compatibility
        case 'n':
        case 'N':
            Verbose = 0;
            break;

            // Output profile
        case 'o':
        case 'O':
            if (lIsDeviceLink)
                FatalError("icctrans: Device-link already specified");
            cOutProf = xoptarg;
            break;

            // Proofing profile
        case 'p':
        case 'P':
            cProofing = xoptarg;
            break;

            // Quantize (get rid of decimals)
        case 'q':
        case 'Q':
            lQuantize = TRUE;
            break;

            // Inhibit unbounded mode
        case 's':
        case 'S':
               lUnbounded = FALSE;
               break;

            // The intent
        case 't':
        case 'T':
            Intent = atoi(xoptarg);
            break;

            // Verbosity level
        case 'V':
        case 'v':
            Verbose = atoi(xoptarg);
            if (Verbose < 0 || Verbose > 3) {
                FatalError("Unknown verbosity level '%d'", Verbose);
            }
            break;

            // Wide (16 bits)
        case 'W':
        case 'w':
            Width16 = TRUE;
            break;

            // Hexadecimal
        case 'x':
        case 'X':
            InHexa = TRUE;
            break;

        default:
            FatalError("Unknown option - run without args to see valid ones.\n");
            }
    }


    // If output CGATS involved, switch to float
    if ((argc - xoptind) > 2) {
        lIsFloat = TRUE;
    }
}



static
void SetRange(cmsFloat64Number range, cmsBool IsInput)
{
    if (IsInput)
        InputRange = range;
    else
        OutputRange = range;
}

// Populate a named color list with usual component names.
// I am using the first Colorant channel to store the range, but it works since
// this space is not used anyway.
static
cmsNAMEDCOLORLIST* ComponentNames(cmsContext ContextID, cmsColorSpaceSignature space, cmsBool IsInput)
{
    cmsNAMEDCOLORLIST* out;
    int i, n;
    char Buffer[cmsMAX_PATH];

    out = cmsAllocNamedColorList(0, 12, cmsMAXCHANNELS, "", "");
    if (out == NULL) return NULL;

    switch (space) {

    case cmsSigXYZData:
        SetRange(100, IsInput);
        cmsAppendNamedColor(ContextID, out, "X", NULL, NULL);
        cmsAppendNamedColor(ContextID, out, "Y", NULL, NULL);
        cmsAppendNamedColor(ContextID, out, "Z", NULL, NULL);
        break;

    case cmsSigLabData:
        SetRange(1, IsInput);
        cmsAppendNamedColor(ContextID, out, "L*", NULL, NULL);
        cmsAppendNamedColor(ContextID, out, "a*", NULL, NULL);
        cmsAppendNamedColor(ContextID, out, "b*", NULL, NULL);
        break;

    case cmsSigLuvData:
        SetRange(1, IsInput);
        cmsAppendNamedColor(ContextID, out, "L", NULL, NULL);
        cmsAppendNamedColor(ContextID, out, "u", NULL, NULL);
        cmsAppendNamedColor(ContextID, out, "v", NULL, NULL);
        break;

    case cmsSigYCbCrData:
        SetRange(255, IsInput);
        cmsAppendNamedColor(ContextID, out, "Y", NULL, NULL );
        cmsAppendNamedColor(ContextID, out, "Cb", NULL, NULL);
        cmsAppendNamedColor(ContextID, out, "Cr", NULL, NULL);
        break;


    case cmsSigYxyData:
        SetRange(1, IsInput);
        cmsAppendNamedColor(ContextID, out, "Y", NULL, NULL);
        cmsAppendNamedColor(ContextID, out, "x", NULL, NULL);
        cmsAppendNamedColor(ContextID, out, "y", NULL, NULL);
        break;

    case cmsSigRgbData:
        SetRange(255, IsInput);
        cmsAppendNamedColor(ContextID, out, "R", NULL, NULL);
        cmsAppendNamedColor(ContextID, out, "G", NULL, NULL);
        cmsAppendNamedColor(ContextID, out, "B", NULL, NULL);
        break;

    case cmsSigGrayData:
        SetRange(255, IsInput);
        cmsAppendNamedColor(ContextID, out, "G", NULL, NULL);
        break;

    case cmsSigHsvData:
        SetRange(255, IsInput);
        cmsAppendNamedColor(ContextID, out, "H", NULL, NULL);
        cmsAppendNamedColor(ContextID, out, "s", NULL, NULL);
        cmsAppendNamedColor(ContextID, out, "v", NULL, NULL);
        break;

    case cmsSigHlsData:
        SetRange(255, IsInput);
        cmsAppendNamedColor(ContextID, out, "H", NULL, NULL);
        cmsAppendNamedColor(ContextID, out, "l", NULL, NULL);
        cmsAppendNamedColor(ContextID, out, "s", NULL, NULL);
        break;

    case cmsSigCmykData:
        SetRange(1, IsInput);
        cmsAppendNamedColor(ContextID, out, "C", NULL, NULL);
        cmsAppendNamedColor(ContextID, out, "M", NULL, NULL);
        cmsAppendNamedColor(ContextID, out, "Y", NULL, NULL);
        cmsAppendNamedColor(ContextID, out, "K", NULL, NULL);
        break;

    case cmsSigCmyData:
        SetRange(1, IsInput);
        cmsAppendNamedColor(ContextID, out, "C", NULL, NULL);
        cmsAppendNamedColor(ContextID, out, "M", NULL, NULL);
        cmsAppendNamedColor(ContextID, out, "Y", NULL, NULL);
        break;

    default:

        SetRange(1, IsInput);

        n = cmsChannelsOf(ContextID, space);

        for (i=0; i < n; i++) {

            sprintf(Buffer, "Channel #%d", i + 1);
            cmsAppendNamedColor(ContextID, out, Buffer, NULL, NULL);
        }
    }

    return out;

}


// Creates all needed color transforms
static
cmsBool OpenTransforms(cmsContext ContextID)
{
    cmsHPROFILE hInput, hOutput, hProof;
    cmsUInt32Number dwIn, dwOut, dwFlags;
    cmsNAMEDCOLORLIST* List;
    int i;

    // We don't need cache
    dwFlags = cmsFLAGS_NOCACHE;

    if (lIsDeviceLink) {

        hInput  = OpenStockProfile(0, cInProf);
        if (hInput == NULL) return FALSE;
        hOutput = NULL;
        hProof  = NULL;

        if (cmsGetDeviceClass(ContextID, hInput) == cmsSigNamedColorClass) {
            OutputColorSpace  = cmsGetColorSpace(ContextID, hInput);
            InputColorSpace = cmsGetPCS(ContextID, hInput);
        }
        else {
            InputColorSpace  = cmsGetColorSpace(ContextID, hInput);
            OutputColorSpace = cmsGetPCS(ContextID, hInput);
        }

        // Read colorant tables if present
        if (cmsIsTag(ContextID, hInput, cmsSigColorantTableTag)) {
            List = cmsReadTag(ContextID, hInput, cmsSigColorantTableTag);
            InputColorant = cmsDupNamedColorList(ContextID, List);
            InputRange = 1;
        }
        else InputColorant = ComponentNames(ContextID, InputColorSpace, TRUE);

        if (cmsIsTag(ContextID, hInput, cmsSigColorantTableOutTag)){

            List = cmsReadTag(ContextID, hInput, cmsSigColorantTableOutTag);
            OutputColorant = cmsDupNamedColorList(ContextID, List);
            OutputRange = 1;
        }
        else OutputColorant = ComponentNames(ContextID, OutputColorSpace, FALSE);

    }
    else {

        hInput  = OpenStockProfile(0, cInProf);
        if (hInput == NULL) return FALSE;

        hOutput = OpenStockProfile(0, cOutProf);
        if (hOutput == NULL) return FALSE;
        hProof  = NULL;


        if (cmsGetDeviceClass(ContextID, hInput) == cmsSigLinkClass ||
            cmsGetDeviceClass(ContextID, hOutput) == cmsSigLinkClass)
            FatalError("Use -l flag for devicelink profiles!\n");


        InputColorSpace   = cmsGetColorSpace(ContextID, hInput);
        OutputColorSpace  = cmsGetColorSpace(ContextID, hOutput);

        // Read colorant tables if present
        if (cmsIsTag(ContextID, hInput, cmsSigColorantTableTag)) {
            List = cmsReadTag(ContextID, hInput, cmsSigColorantTableTag);
            InputColorant = cmsDupNamedColorList(ContextID, List);
            if (cmsNamedColorCount(ContextID, InputColorant) <= 3)
                SetRange(255, TRUE);
            else
                SetRange(1, TRUE);  // Inks are already divided by 100 in the formatter

        }
        else InputColorant = ComponentNames(ContextID, InputColorSpace, TRUE);

        if (cmsIsTag(ContextID, hOutput, cmsSigColorantTableTag)){

            List = cmsReadTag(ContextID, hOutput, cmsSigColorantTableTag);
            OutputColorant = cmsDupNamedColorList(ContextID, List);
            if (cmsNamedColorCount(ContextID, OutputColorant) <= 3)
                SetRange(255, FALSE);
            else
                SetRange(1, FALSE);  // Inks are already divided by 100 in the formatter
        }
        else OutputColorant = ComponentNames(ContextID, OutputColorSpace, FALSE);


        if (cProofing != NULL) {

            hProof = OpenStockProfile(0, cProofing);
            if (hProof == NULL) return FALSE;
            dwFlags |= cmsFLAGS_SOFTPROOFING;
        }
    }

    // Print information on profiles
    if (Verbose > 2) {

        printf("Profile:\n");
        PrintProfileInformation(ContextID, hInput);

        if (hOutput) {

            printf("Output profile:\n");
            PrintProfileInformation(ContextID, hOutput);
        }

        if (hProof != NULL) {
            printf("Proofing profile:\n");
            PrintProfileInformation(ContextID, hProof);
        }
    }


    // Input is always in floating point
    dwIn  = cmsFormatterForColorspaceOfProfile(ContextID, hInput, 0, TRUE);

    if (lIsDeviceLink) {

        dwOut = cmsFormatterForPCSOfProfile(ContextID, hInput, lIsFloat ? 0 : 2, lIsFloat);
    }
    else {

        // 16 bits or floating point (only on output)
        dwOut = cmsFormatterForColorspaceOfProfile(ContextID, hOutput, lIsFloat ? 0 : 2, lIsFloat);
    }

    // For named color, there is a specialized formatter
    if (cmsGetDeviceClass(ContextID, hInput) == cmsSigNamedColorClass) {

        dwIn = TYPE_NAMED_COLOR_INDEX;
        InputNamedColor = TRUE;
    }

    // Precision mode
    switch (PrecalcMode) {

       case 0: dwFlags |= cmsFLAGS_NOOPTIMIZE; break;
       case 2: dwFlags |= cmsFLAGS_HIGHRESPRECALC; break;
       case 3: dwFlags |= cmsFLAGS_LOWRESPRECALC; break;
       case 1: break;

       default:
           FatalError("Unknown precalculation mode '%d'", PrecalcMode);
    }


    if (BlackPointCompensation)
        dwFlags |= cmsFLAGS_BLACKPOINTCOMPENSATION;


    if (GamutCheck) {

        cmsUInt16Number Alarm[cmsMAXCHANNELS];

        if (hProof == NULL)
            FatalError("I need proofing profile -p for gamut checking!");

        for (i=0; i < cmsMAXCHANNELS; i++)
            Alarm[i] = 0xFFFF;

        cmsSetAlarmCodes(ContextID, Alarm);
        dwFlags |= cmsFLAGS_GAMUTCHECK;
    }


    // The main transform
    hTrans = cmsCreateProofingTransform(ContextID, hInput,  dwIn, hOutput, dwOut, hProof, Intent, ProofingIntent, dwFlags);

    if (hProof) cmsCloseProfile(ContextID, hProof);

    if (hTrans == NULL) return FALSE;


    // PCS Dump if requested
    hTransXYZ = NULL; hTransLab = NULL;

    if (hOutput && Verbose > 1) {

        cmsHPROFILE hXYZ = cmsCreateXYZProfile(ContextID);
        cmsHPROFILE hLab = cmsCreateLab4Profile(ContextID, NULL);

        hTransXYZ = cmsCreateTransform(ContextID, hInput, dwIn, hXYZ,  lIsFloat ? TYPE_XYZ_DBL : TYPE_XYZ_16, Intent, cmsFLAGS_NOCACHE);
        if (hTransXYZ == NULL) return FALSE;

        hTransLab = cmsCreateTransform(ContextID, hInput, dwIn, hLab,  lIsFloat? TYPE_Lab_DBL : TYPE_Lab_16, Intent, cmsFLAGS_NOCACHE);
        if (hTransLab == NULL) return FALSE;

        cmsCloseProfile(ContextID, hXYZ);
        cmsCloseProfile(ContextID, hLab);
    }

    if (hInput) cmsCloseProfile(ContextID, hInput);
    if (hOutput) cmsCloseProfile(ContextID, hOutput);

    return TRUE;
}


// Free open resources
static
void CloseTransforms(cmsContext ContextID)
{
    if (InputColorant) cmsFreeNamedColorList(ContextID, InputColorant);
    if (OutputColorant) cmsFreeNamedColorList(ContextID, OutputColorant);

    if (hTrans) cmsDeleteTransform(ContextID, hTrans);
    if (hTransLab) cmsDeleteTransform(ContextID, hTransLab);
    if (hTransXYZ) cmsDeleteTransform(ContextID, hTransXYZ);

}

// ---------------------------------------------------------------------------------------------------

// Get input from user
static
void GetLine(cmsContext ContextID, char* Buffer, const char* frm, ...)
{
    int res;
    va_list args;

    va_start(args, frm);

    do {
        if (xisatty(stdin))
            vfprintf(stderr, frm, args);

        res = scanf("%4095s", Buffer);

        if (res < 0 || toupper(Buffer[0]) == 'Q') { // Quit?

            CloseTransforms(ContextID);

            if (xisatty(stdin))
                fprintf(stderr, "Done.\n");

            exit(0);
        }
    } while (res == 0);

    va_end(args);
}


// Print a value which is given in double floating point
static
void PrintFloatResults(cmsContext ContextID, cmsFloat64Number Value[])
{
    cmsUInt32Number i, n;
    char ChannelName[cmsMAX_PATH];
    cmsFloat64Number v;

    n = cmsChannelsOf(ContextID, OutputColorSpace);
    for (i=0; i < n; i++) {

        if (OutputColorant != NULL) {

            cmsNamedColorInfo(ContextID, OutputColorant, i, ChannelName, NULL, NULL, NULL, NULL);
        }
        else {
            OutputRange = 1;
            sprintf(ChannelName, "Channel #%u", i + 1);
        }

        v = (cmsFloat64Number) Value[i]* OutputRange;

        if (lQuantize)
            v = floor(v + 0.5);

        if (!lUnbounded) {

               if (v < 0)
                      v = 0;
               if (v > OutputRange)
                      v = OutputRange;
        }

        if (Verbose <= 0)
            printf("%.4f ", v);
        else
            printf("%s=%.4f ", ChannelName, v);
    }

    printf("\n");
}


// Get a named-color index
static
cmsUInt16Number GetIndex(cmsContext ContextID)
{
    char Buffer[4096], Name[cmsMAX_PATH], Prefix[40], Suffix[40];
    int index, max;
    const cmsNAMEDCOLORLIST* NamedColorList;

    NamedColorList = cmsGetNamedColorList(hTrans);
    if (NamedColorList == NULL) return 0;

    max = cmsNamedColorCount(ContextID, NamedColorList)-1;

    GetLine(ContextID, Buffer, "Color index (0..%d)? ", max);
    index = atoi(Buffer);

    if (index > max)
        FatalError("Named color %d out of range!", index);

    cmsNamedColorInfo(ContextID, NamedColorList, index, Name, Prefix, Suffix, NULL, NULL);

    printf("\n%s %s %s\n", Prefix, Name, Suffix);

    return (cmsUInt16Number) index;
}

// Read values from a text file or terminal
static
void TakeFloatValues(cmsContext ContextID, cmsFloat64Number Float[])
{
    cmsUInt32Number i, n;
    char ChannelName[cmsMAX_PATH];
    char Buffer[4096];

    if (xisatty(stdin))
        fprintf(stderr, "\nEnter values, 'q' to quit\n");

    if (InputNamedColor) {

        // This is named color index, which is always cmsUInt16Number
        cmsUInt16Number index = GetIndex(ContextID);
        memcpy(Float, &index, sizeof(cmsUInt16Number));
        return;
    }

    n = cmsChannelsOf(ContextID, InputColorSpace);
    for (i=0; i < n; i++) {

        if (InputColorant) {
            cmsNamedColorInfo(ContextID, InputColorant, i, ChannelName, NULL, NULL, NULL, NULL);
        }
        else {
            InputRange = 1;
            sprintf(ChannelName, "Channel #%u", i+1);
        }

        GetLine(ContextID, Buffer, "%s? ", ChannelName);

        Float[i] = (cmsFloat64Number) atof(Buffer) / InputRange;
    }

    if (xisatty(stdin))
        fprintf(stderr, "\n");
}

static
void PrintPCSFloat(cmsContext ContextID, cmsFloat64Number Input[])
{
    if (Verbose > 1 && hTransXYZ && hTransLab) {

        cmsCIEXYZ XYZ = { 0, 0, 0 };
        cmsCIELab Lab = { 0, 0, 0 };

        if (hTransXYZ) cmsDoTransform(ContextID, hTransXYZ, Input, &XYZ, 1);
        if (hTransLab) cmsDoTransform(ContextID, hTransLab, Input, &Lab, 1);

        printf("[PCS] Lab=(%.4f,%.4f,%.4f) XYZ=(%.4f,%.4f,%.4f)\n", Lab.L, Lab.a, Lab.b,
            XYZ.X * 100.0, XYZ.Y * 100.0, XYZ.Z * 100.0);

    }
}




// -----------------------------------------------------------------------------------------------

static
void PrintEncodedResults(cmsContext ContextID, cmsUInt16Number Encoded[])
{
    cmsUInt32Number i, n;
    char ChannelName[cmsMAX_PATH];
    cmsUInt32Number v;

    n = cmsChannelsOf(ContextID, OutputColorSpace);
    for (i=0; i < n; i++) {

        if (OutputColorant != NULL) {

            cmsNamedColorInfo(ContextID, OutputColorant, i, ChannelName, NULL, NULL, NULL, NULL);
        }
        else {
            sprintf(ChannelName, "Channel #%u", i + 1);
        }

        if (Verbose > 0)
            printf("%s=", ChannelName);

        v = Encoded[i];

        if (InHexa) {

            if (Width16)
                printf("0x%04X ", (int) floor(v + .5));
            else
                printf("0x%02X ", (int) floor(v / 257. + .5));

        } else {

            if (Width16)
                printf("%d ", (int) floor(v + .5));
            else
                printf("%d ", (int) floor(v / 257. + .5));
        }

    }

    printf("\n");
}

// Print XYZ/Lab values on verbose mode

static
void PrintPCSEncoded(cmsContext ContextID, cmsFloat64Number Input[])
{
    if (Verbose > 1 && hTransXYZ && hTransLab) {

        cmsUInt16Number XYZ[3], Lab[3];

        if (hTransXYZ) cmsDoTransform(ContextID, hTransXYZ, Input, XYZ, 1);
        if (hTransLab) cmsDoTransform(ContextID, hTransLab, Input, Lab, 1);

        printf("[PCS] Lab=(0x%04X,0x%04X,0x%04X) XYZ=(0x%04X,0x%04X,0x%04X)\n", Lab[0], Lab[1], Lab[2],
            XYZ[0], XYZ[1], XYZ[2]);

    }
}


// --------------------------------------------------------------------------------------



// Take a value from IT8 and scale it accordly to fill a cmsUInt16Number (0..FFFF)

static
cmsFloat64Number GetIT8Val(cmsContext ContextID, const char* Name, cmsFloat64Number Max)
{
    const char* Val = cmsIT8GetData(ContextID, hIT8in, CGATSPatch, Name);

    if (Val == NULL)
        FatalError("Field '%s' not found", Name);

    return atof(Val) / Max;

}


// Read input values from CGATS file.

static
void TakeCGATSValues(cmsContext ContextID, int nPatch, cmsFloat64Number Float[])
{

    // At first take the name if SAMPLE_ID is present
    if (cmsIT8GetPatchName(ContextID, hIT8in, nPatch, CGATSPatch) == NULL) {
        FatalError("Sorry, I need 'SAMPLE_ID' on input CGATS to operate.");
    }


    // Special handling for named color profiles.
    // Lookup the name in the names database (the transform)

    if (InputNamedColor) {

        const cmsNAMEDCOLORLIST* NamedColorList;
        int index;

        NamedColorList = cmsGetNamedColorList(hTrans);
        if (NamedColorList == NULL)
            FatalError("Malformed named color profile");

        index = cmsNamedColorIndex(ContextID, NamedColorList, CGATSPatch);
        if (index < 0)
            FatalError("Named color '%s' not found in the profile", CGATSPatch);

        Float[0] = index;
        return;
    }

    // Color is not a spot color, proceed.

    switch (InputColorSpace) {

        // Encoding should follow CGATS specification.

    case cmsSigXYZData:
        Float[0] = cmsIT8GetDataDbl(ContextID, hIT8in, CGATSPatch, "XYZ_X") / 100.0;
        Float[1] = cmsIT8GetDataDbl(ContextID, hIT8in, CGATSPatch, "XYZ_Y") / 100.0;
        Float[2] = cmsIT8GetDataDbl(ContextID, hIT8in, CGATSPatch, "XYZ_Z") / 100.0;
        break;

    case cmsSigLabData:
        Float[0] = cmsIT8GetDataDbl(ContextID, hIT8in, CGATSPatch, "LAB_L");
        Float[1] = cmsIT8GetDataDbl(ContextID, hIT8in, CGATSPatch, "LAB_A");
        Float[2] = cmsIT8GetDataDbl(ContextID, hIT8in, CGATSPatch, "LAB_B");
        break;


    case cmsSigRgbData:
        Float[0] = GetIT8Val(ContextID, "RGB_R", 255.0);
        Float[1] = GetIT8Val(ContextID, "RGB_G", 255.0);
        Float[2] = GetIT8Val(ContextID, "RGB_B", 255.0);
        break;

    case cmsSigGrayData:
        Float[0] = GetIT8Val(ContextID, "GRAY", 255.0);
        break;

    case cmsSigCmykData:
        Float[0] = GetIT8Val(ContextID, "CMYK_C", 1.0);
        Float[1] = GetIT8Val(ContextID, "CMYK_M", 1.0);
        Float[2] = GetIT8Val(ContextID, "CMYK_Y", 1.0);
        Float[3] = GetIT8Val(ContextID, "CMYK_K", 1.0);
        break;

    case cmsSigCmyData:
        Float[0] = GetIT8Val(ContextID, "CMY_C", 1.0);
        Float[1] = GetIT8Val(ContextID, "CMY_M", 1.0);
        Float[2] = GetIT8Val(ContextID, "CMY_Y", 1.0);
        break;

    case cmsSig1colorData:
    case cmsSig2colorData:
    case cmsSig3colorData:
    case cmsSig4colorData:
    case cmsSig5colorData:
    case cmsSig6colorData:
    case cmsSig7colorData:
    case cmsSig8colorData:
    case cmsSig9colorData:
    case cmsSig10colorData:
    case cmsSig11colorData:
    case cmsSig12colorData:
    case cmsSig13colorData:
    case cmsSig14colorData:
    case cmsSig15colorData:
        {
            cmsUInt32Number i, n;

            n = cmsChannelsOf(ContextID, InputColorSpace);
            for (i=0; i < n; i++) {

                char Buffer[255];

                sprintf(Buffer, "%uCLR_%u", n, i+1);
                Float[i] = GetIT8Val(ContextID, Buffer, 100.0);
            }

        }
        break;

    default:
        {
            cmsUInt32Number i, n;

            n = cmsChannelsOf(ContextID, InputColorSpace);
            for (i=0; i < n; i++) {

                char Buffer[255];

                sprintf(Buffer, "CHAN_%u", i+1);
                Float[i] = GetIT8Val(ContextID, Buffer, 1.0);
            }

        }
    }

}

static
void SetCGATSfld(cmsContext ContextID, const char* Col, cmsFloat64Number Val)
{
    if (lQuantize)
        Val = floor(Val + 0.5);

    if (!cmsIT8SetDataDbl(ContextID, hIT8out, CGATSPatch, Col, Val)) {
        FatalError("couldn't set '%s' on output cgats '%s'", Col, CGATSoutFilename);
    }
}



static
void PutCGATSValues(cmsContext ContextID, cmsFloat64Number Float[])
{
    cmsIT8SetData(ContextID, hIT8out, CGATSPatch, "SAMPLE_ID", CGATSPatch);
    switch (OutputColorSpace) {


    // Encoding should follow CGATS specification.

    case cmsSigXYZData:

        SetCGATSfld(ContextID, "XYZ_X", Float[0] * 100.0);
        SetCGATSfld(ContextID, "XYZ_Y", Float[1] * 100.0);
        SetCGATSfld(ContextID, "XYZ_Z", Float[2] * 100.0);
        break;

    case cmsSigLabData:

        SetCGATSfld(ContextID, "LAB_L", Float[0]);
        SetCGATSfld(ContextID, "LAB_A", Float[1]);
        SetCGATSfld(ContextID, "LAB_B", Float[2]);
        break;


    case cmsSigRgbData:
        SetCGATSfld(ContextID, "RGB_R", Float[0] * 255.0);
        SetCGATSfld(ContextID, "RGB_G", Float[1] * 255.0);
        SetCGATSfld(ContextID, "RGB_B", Float[2] * 255.0);
        break;

    case cmsSigGrayData:
        SetCGATSfld(ContextID, "GRAY", Float[0] * 255.0);
        break;

    case cmsSigCmykData:
        SetCGATSfld(ContextID, "CMYK_C", Float[0]);
        SetCGATSfld(ContextID, "CMYK_M", Float[1]);
        SetCGATSfld(ContextID, "CMYK_Y", Float[2]);
        SetCGATSfld(ContextID, "CMYK_K", Float[3]);
        break;

    case cmsSigCmyData:
        SetCGATSfld(ContextID, "CMY_C", Float[0]);
        SetCGATSfld(ContextID, "CMY_M", Float[1]);
        SetCGATSfld(ContextID, "CMY_Y", Float[2]);
        break;

    case cmsSig1colorData:
    case cmsSig2colorData:
    case cmsSig3colorData:
    case cmsSig4colorData:
    case cmsSig5colorData:
    case cmsSig6colorData:
    case cmsSig7colorData:
    case cmsSig8colorData:
    case cmsSig9colorData:
    case cmsSig10colorData:
    case cmsSig11colorData:
    case cmsSig12colorData:
    case cmsSig13colorData:
    case cmsSig14colorData:
    case cmsSig15colorData:
        {

            cmsUInt32Number i, n;

            n = cmsChannelsOf(ContextID, InputColorSpace);
            for (i=0; i < n; i++) {

                char Buffer[255];

                sprintf(Buffer, "%uCLR_%u", n, i+1);

                SetCGATSfld(ContextID, Buffer, Float[i] * 100.0);
            }
        }
        break;

    default:
        {

            cmsUInt32Number i, n;

            n = cmsChannelsOf(ContextID, InputColorSpace);
            for (i=0; i < n; i++) {

                char Buffer[255];

                sprintf(Buffer, "CHAN_%u", i+1);

                SetCGATSfld(ContextID, Buffer, Float[i]);
            }
        }
    }
}



// Create data format
static
void SetOutputDataFormat(cmsContext ContextID)
{
    cmsIT8DefineDblFormat(ContextID, hIT8out, "%.4g");
    cmsIT8SetPropertyStr(ContextID, hIT8out, "ORIGINATOR", "icctrans");

    if (IncludePart != NULL)
        cmsIT8SetPropertyStr(ContextID, hIT8out, ".INCLUDE", IncludePart);

    cmsIT8SetComment(ContextID, hIT8out, "Data follows");
    cmsIT8SetPropertyDbl(ContextID, hIT8out, "NUMBER_OF_SETS", nMaxPatches);


    switch (OutputColorSpace) {


        // Encoding should follow CGATS specification.

    case cmsSigXYZData:
        cmsIT8SetPropertyDbl(ContextID, hIT8out, "NUMBER_OF_FIELDS", 4);
        cmsIT8SetDataFormat(ContextID, hIT8out, 0, "SAMPLE_ID");
        cmsIT8SetDataFormat(ContextID, hIT8out, 1, "XYZ_X");
        cmsIT8SetDataFormat(ContextID, hIT8out, 2, "XYZ_Y");
        cmsIT8SetDataFormat(ContextID, hIT8out, 3, "XYZ_Z");
        break;

    case cmsSigLabData:
        cmsIT8SetPropertyDbl(ContextID, hIT8out, "NUMBER_OF_FIELDS", 4);
        cmsIT8SetDataFormat(ContextID, hIT8out, 0, "SAMPLE_ID");
        cmsIT8SetDataFormat(ContextID, hIT8out, 1, "LAB_L");
        cmsIT8SetDataFormat(ContextID, hIT8out, 2, "LAB_A");
        cmsIT8SetDataFormat(ContextID, hIT8out, 3, "LAB_B");
        break;


    case cmsSigRgbData:
        cmsIT8SetPropertyDbl(ContextID, hIT8out, "NUMBER_OF_FIELDS", 4);
        cmsIT8SetDataFormat(ContextID, hIT8out, 0, "SAMPLE_ID");
        cmsIT8SetDataFormat(ContextID, hIT8out, 1, "RGB_R");
        cmsIT8SetDataFormat(ContextID, hIT8out, 2, "RGB_G");
        cmsIT8SetDataFormat(ContextID, hIT8out, 3, "RGB_B");
        break;

    case cmsSigGrayData:
        cmsIT8SetPropertyDbl(ContextID, hIT8out, "NUMBER_OF_FIELDS", 2);
        cmsIT8SetDataFormat(ContextID, hIT8out, 0, "SAMPLE_ID");
        cmsIT8SetDataFormat(ContextID, hIT8out, 1, "GRAY");
        break;

    case cmsSigCmykData:
        cmsIT8SetPropertyDbl(ContextID, hIT8out, "NUMBER_OF_FIELDS", 5);
        cmsIT8SetDataFormat(ContextID, hIT8out, 0, "SAMPLE_ID");
        cmsIT8SetDataFormat(ContextID, hIT8out, 1, "CMYK_C");
        cmsIT8SetDataFormat(ContextID, hIT8out, 2, "CMYK_M");
        cmsIT8SetDataFormat(ContextID, hIT8out, 3, "CMYK_Y");
        cmsIT8SetDataFormat(ContextID, hIT8out, 4, "CMYK_K");
        break;

    case cmsSigCmyData:
        cmsIT8SetPropertyDbl(ContextID, hIT8out, "NUMBER_OF_FIELDS", 4);
        cmsIT8SetDataFormat(ContextID, hIT8out, 0, "SAMPLE_ID");
        cmsIT8SetDataFormat(ContextID, hIT8out, 1, "CMY_C");
        cmsIT8SetDataFormat(ContextID, hIT8out, 2, "CMY_M");
        cmsIT8SetDataFormat(ContextID, hIT8out, 3, "CMY_Y");
        break;

    case cmsSig1colorData:
    case cmsSig2colorData:
    case cmsSig3colorData:
    case cmsSig4colorData:
    case cmsSig5colorData:
    case cmsSig6colorData:
    case cmsSig7colorData:
    case cmsSig8colorData:
    case cmsSig9colorData:
    case cmsSig10colorData:
    case cmsSig11colorData:
    case cmsSig12colorData:
    case cmsSig13colorData:
    case cmsSig14colorData:
    case cmsSig15colorData:
        {
            int i, n;
            char Buffer[255];

            n = cmsChannelsOf(ContextID, OutputColorSpace);
            cmsIT8SetPropertyDbl(ContextID, hIT8out, "NUMBER_OF_FIELDS", n+1);
            cmsIT8SetDataFormat(ContextID, hIT8out, 0, "SAMPLE_ID");

            for (i=1; i <= n; i++) {
                sprintf(Buffer, "%dCLR_%d", n, i);
                cmsIT8SetDataFormat(ContextID, hIT8out, i, Buffer);
            }
        }
        break;

    default: {

        int i, n;
        char Buffer[255];

        n = cmsChannelsOf(ContextID, OutputColorSpace);
        cmsIT8SetPropertyDbl(ContextID, hIT8out, "NUMBER_OF_FIELDS", n+1);
        cmsIT8SetDataFormat(ContextID, hIT8out, 0, "SAMPLE_ID");

        for (i=1; i <= n; i++) {
            sprintf(Buffer, "CHAN_%d", i);
            cmsIT8SetDataFormat(ContextID, hIT8out, i, Buffer);
        }
    }
    }
}

// Open CGATS if specified

static
void OpenCGATSFiles(cmsContext ContextID, int argc, char *argv[])
{
    int nParams = argc - xoptind;

    if (nParams >= 1)  {

        hIT8in = cmsIT8LoadFromFile(0, argv[xoptind]);

        if (hIT8in == NULL)
            FatalError("'%s' is not recognized as a CGATS file", argv[xoptind]);

        nMaxPatches = (int) cmsIT8GetPropertyDbl(ContextID, hIT8in, "NUMBER_OF_SETS");
    }

    if (nParams == 2) {

        hIT8out = cmsIT8Alloc(NULL);
        SetOutputDataFormat(ContextID);
        strncpy(CGATSoutFilename, argv[xoptind+1], cmsMAX_PATH-1);
    }

    if (nParams > 2) FatalError("Too many CGATS files");
}



// The main sink
int main(int argc, char *argv[])
{
    cmsUInt16Number Output[cmsMAXCHANNELS];
    cmsFloat64Number OutputFloat[cmsMAXCHANNELS];
    cmsFloat64Number InputFloat[cmsMAXCHANNELS];
    cmsContext ContextID =  NULL;

    int nPatch = 0;

    fprintf(stderr, "LittleCMS ColorSpace conversion calculator - 5.0 [LittleCMS %2.2f]\n", LCMS_VERSION / 1000.0);
    fprintf(stderr, "Copyright (c) 1998-2020 Marti Maria Saguer. See COPYING file for details.\n");
    fflush(stderr);

    InitUtils(ContextID, "transicc");

    Verbose = 1;

    if (argc == 1) {

        Help();
        return 0;
    }

    HandleSwitches(ContextID, argc, argv);

    // Open profiles, create transforms
    if (!OpenTransforms(ContextID)) return 1;

    // Open CGATS input if specified
    OpenCGATSFiles(ContextID, argc, argv);

    // Main loop: read all values and convert them
    for(;;) {

        if (hIT8in != NULL) {

            if (nPatch >= nMaxPatches) break;
            TakeCGATSValues(ContextID, nPatch++, InputFloat);

        } else {

            if (feof(stdin)) break;
            TakeFloatValues(ContextID, InputFloat);

        }

        if (lIsFloat)
            cmsDoTransform(ContextID, hTrans, InputFloat, OutputFloat, 1);
        else
            cmsDoTransform(ContextID, hTrans, InputFloat, Output, 1);


        if (hIT8out != NULL) {

            PutCGATSValues(ContextID, OutputFloat);
        }
        else {

            if (lIsFloat) {
                PrintFloatResults(ContextID, OutputFloat); PrintPCSFloat(ContextID, InputFloat);
            }
            else {
                PrintEncodedResults(ContextID, Output);   PrintPCSEncoded(ContextID, InputFloat);
            }

        }
    }


    // Cleanup
    CloseTransforms(ContextID);

    if (hIT8in)
        cmsIT8Free(ContextID, hIT8in);

    if (hIT8out) {
        cmsIT8SaveToFile(ContextID, hIT8out, CGATSoutFilename);
        cmsIT8Free(ContextID, hIT8out);
    }

    // All is ok
    return 0;
}
