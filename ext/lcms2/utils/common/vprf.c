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


int Verbose = 0;

static char ProgramName[256] = "";

void FatalError(const char *frm, ...)
{
    va_list args;

    va_start(args, frm);
    fprintf(stderr, "[%s fatal error]: ", ProgramName);
    vfprintf(stderr, frm, args);
    fprintf(stderr, "\n");
    va_end(args);

    exit(1);
}

// Show errors to the end user (unless quiet option)
static
void MyErrorLogHandler(cmsContext ContextID, cmsUInt32Number ErrorCode, const char *Text)
{
    if (Verbose >= 0)
        fprintf(stderr, "[%s]: %s\n", ProgramName, Text);

    UTILS_UNUSED_PARAMETER(ErrorCode);
    UTILS_UNUSED_PARAMETER(ContextID);
}


void InitUtils(cmsContext ContextID, const char* PName)
{
      strncpy(ProgramName, PName, sizeof(ProgramName));
      ProgramName[sizeof(ProgramName)-1] = 0;

      cmsSetLogErrorHandler(ContextID, MyErrorLogHandler);
}


// Virtual profiles are handled here.
cmsHPROFILE OpenStockProfile(cmsContext ContextID, const char* File)
{
       if (!File)
            return cmsCreate_sRGBProfile(ContextID);

       if (cmsstrcasecmp(File, "*Lab2") == 0)
                return cmsCreateLab2Profile(ContextID, NULL);

       if (cmsstrcasecmp(File, "*Lab4") == 0)
                return cmsCreateLab4Profile(ContextID, NULL);

       if (cmsstrcasecmp(File, "*Lab") == 0)
                return cmsCreateLab4Profile(ContextID, NULL);

       if (cmsstrcasecmp(File, "*LabD65") == 0) {

           cmsCIExyY D65xyY;

           cmsWhitePointFromTemp(ContextID,  &D65xyY, 6504);
           return cmsCreateLab4Profile(ContextID, &D65xyY);
       }

       if (cmsstrcasecmp(File, "*XYZ") == 0)
                return cmsCreateXYZProfile(ContextID);

       if (cmsstrcasecmp(File, "*Gray22") == 0) {

           cmsToneCurve* Curve = cmsBuildGamma(ContextID, 2.2);
           cmsHPROFILE hProfile = cmsCreateGrayProfile(ContextID, cmsD50_xyY(ContextID), Curve);
           cmsFreeToneCurve(ContextID, Curve);
           return hProfile;
       }

        if (cmsstrcasecmp(File, "*Gray30") == 0) {

           cmsToneCurve* Curve = cmsBuildGamma(ContextID, 3.0);
           cmsHPROFILE hProfile = cmsCreateGrayProfile(ContextID, cmsD50_xyY(ContextID), Curve);
           cmsFreeToneCurve(ContextID, Curve);
           return hProfile;
       }

       if (cmsstrcasecmp(File, "*srgb") == 0)
                return cmsCreate_sRGBProfile(ContextID);

       if (cmsstrcasecmp(File, "*null") == 0)
                return cmsCreateNULLProfile(ContextID);


       if (cmsstrcasecmp(File, "*Lin2222") == 0) {

            cmsToneCurve*  Gamma = cmsBuildGamma(0, 2.2);
            cmsToneCurve*  Gamma4[4];
            cmsHPROFILE hProfile;

            Gamma4[0] = Gamma4[1] = Gamma4[2] = Gamma4[3] = Gamma;
            hProfile = cmsCreateLinearizationDeviceLink(ContextID, cmsSigCmykData, Gamma4);
            cmsFreeToneCurve(ContextID, Gamma);
            return hProfile;
       }


        return cmsOpenProfileFromFile(ContextID, File, "r");
}

// Help on available built-ins
void PrintBuiltins(void)
{
     fprintf(stderr, "\nBuilt-in profiles:\n\n");
     fprintf(stderr, "\t*Lab2  -- D50-based v2 CIEL*a*b\n"
                     "\t*Lab4  -- D50-based v4 CIEL*a*b\n"
                     "\t*Lab   -- D50-based v4 CIEL*a*b\n"
                     "\t*XYZ   -- CIE XYZ (PCS)\n"
                     "\t*sRGB  -- sRGB color space\n"
                     "\t*Gray22 - Monochrome of Gamma 2.2\n"
                     "\t*Gray30 - Monochrome of Gamma 3.0\n"
                     "\t*null   - Monochrome black for all input\n"
                     "\t*Lin2222- CMYK linearization of gamma 2.2 on each channel\n\n");
}


// Auxiliary for printing information on profile
static
void PrintInfo(cmsContext ContextID, cmsHPROFILE h, cmsInfoType Info)
{
    char* text;
    int len;

    len = cmsGetProfileInfoASCII(ContextID, h, Info, "en", "US", NULL, 0);
    if (len == 0) return;

    text = (char*) malloc(len * sizeof(char));
    if (text == NULL) return;

    cmsGetProfileInfoASCII(ContextID, h, Info, "en", "US", text, len);

    if (strlen(text) > 0)
        printf("%s\n", text);

    free(text);
}



// Displays the colorant table
static
void PrintColorantTable(cmsContext ContextID, cmsHPROFILE hInput, cmsTagSignature Sig, const char* Title)
{
    cmsNAMEDCOLORLIST* list;
    int i, n;

    if (cmsIsTag(ContextID, hInput, Sig)) {

        printf("%s:\n", Title);

        list = (cmsNAMEDCOLORLIST*) cmsReadTag(ContextID, hInput, Sig);
        if (list == NULL) {
            printf("(Unavailable)\n");
            return;
        }

        n = cmsNamedColorCount(ContextID, list);
        for (i=0; i < n; i++) {

            char Name[cmsMAX_PATH];

            cmsNamedColorInfo(ContextID, list, i, Name, NULL, NULL, NULL, NULL);
            printf("\t%s\n", Name);
        }

        printf("\n");
    }

}


void PrintProfileInformation(cmsContext ContextID, cmsHPROFILE hInput)
{
    if (hInput == NULL) {
			fprintf(stderr, "*Wrong or corrupted profile*\n");
            return;
    }

    PrintInfo(ContextID, hInput, cmsInfoDescription);
    PrintInfo(ContextID, hInput, cmsInfoManufacturer);
    PrintInfo(ContextID, hInput, cmsInfoModel);
    PrintInfo(ContextID, hInput, cmsInfoCopyright);

    if (Verbose > 2) {

        PrintColorantTable(ContextID, hInput, cmsSigColorantTableTag,    "Input colorant table");
        PrintColorantTable(ContextID, hInput, cmsSigColorantTableOutTag, "Input colorant out table");
    }

    printf("\n");
}

// -----------------------------------------------------------------------------


void PrintRenderingIntents(cmsContext ContextID)
{
    cmsUInt32Number Codes[200];
    char* Descriptions[200];
    cmsUInt32Number n, i;

    fprintf(stderr, "-t<n> rendering intent:\n\n");

    n = cmsGetSupportedIntents(ContextID, 200, Codes, Descriptions);

    for (i=0; i < n; i++) {
        fprintf(stderr, "\t%u - %s\n", Codes[i], Descriptions[i]);
    }
    fprintf(stderr, "\n");
}



// ------------------------------------------------------------------------------

cmsBool SaveMemoryBlock(const cmsUInt8Number* Buffer, cmsUInt32Number dwLen, const char* Filename)
{
    FILE* out = fopen(Filename, "wb");
    if (out == NULL) {
        FatalError("Cannot create '%s'", Filename);
        return FALSE;
    }

    if (fwrite(Buffer, 1, dwLen, out) != dwLen) {
        FatalError("Cannot write %ld bytes to %s", (long) dwLen, Filename);
        return FALSE;
    }

    if (fclose(out) != 0) {
        FatalError("Error flushing file '%s'", Filename);
        return FALSE;
    }

    return TRUE;
}

// ------------------------------------------------------------------------------

// Return a pixel type on depending on the number of channels
int PixelTypeFromChanCount(int ColorChannels)
{
    switch (ColorChannels) {

        case 1: return PT_GRAY;
        case 2: return PT_MCH2;
        case 3: return PT_MCH3;
        case 4: return PT_CMYK;
        case 5: return PT_MCH5;
        case 6: return PT_MCH6;
        case 7: return PT_MCH7;
        case 8: return PT_MCH8;
        case 9: return PT_MCH9;
        case 10: return PT_MCH10;
        case 11: return PT_MCH11;
        case 12: return PT_MCH12;
        case 13: return PT_MCH13;
        case 14: return PT_MCH14;
        case 15: return PT_MCH15;

        default:

            FatalError("What a weird separation of %d channels?!?!", ColorChannels);
            return -1;
    }
}


// ------------------------------------------------------------------------------

// Return number of channels of pixel type
int ChanCountFromPixelType(int ColorChannels)
{
    switch (ColorChannels) {

      case PT_GRAY: return 1;

      case PT_RGB:
      case PT_CMY:
      case PT_Lab:
      case PT_YUV:
      case PT_YCbCr: return 3;

      case PT_CMYK: return 4 ;
      case PT_MCH2: return 2 ;
      case PT_MCH3: return 3 ;
      case PT_MCH4: return 4 ;
      case PT_MCH5: return 5 ;
      case PT_MCH6: return 6 ;
      case PT_MCH7: return 7 ;
      case PT_MCH8: return 8 ;
      case PT_MCH9: return 9 ;
      case PT_MCH10: return 10;
      case PT_MCH11: return 11;
      case PT_MCH12: return 12;
      case PT_MCH13: return 12;
      case PT_MCH14: return 14;
      case PT_MCH15: return 15;

      default:

          FatalError("Unsupported color space of %d channels", ColorChannels);
          return -1;
    }
}
