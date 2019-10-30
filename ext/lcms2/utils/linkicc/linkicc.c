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

#include "utils.h"

// ---------------------------------------------------------------------------------

static char* Description = "Devicelink profile";
static char* Copyright   = "No copyright, use freely";
static int   Intent = INTENT_PERCEPTUAL;
static char* cOutProf    = "devicelink.icc";
static int   PrecalcMode  = 1;
static int   NumOfGridPoints = 0;

static cmsFloat64Number ObserverAdaptationState = 1.0;  // According ICC 4.2 this is the default

static cmsBool BlackPointCompensation = FALSE;

static cmsFloat64Number InkLimit   = 400;
static cmsBool lUse8bits           = FALSE;
static cmsBool TagResult           = FALSE;
static cmsBool KeepLinearization   = FALSE;
static cmsFloat64Number Version    = 4.3;


// The manual
static
int Help(int level)
{
     switch (level) {

     default:
     case 0:

         fprintf(stderr, "\nlinkicc: Links profiles into a single devicelink.\n");

         fprintf(stderr, "\n");
         fprintf(stderr, "usage: linkicc [flags] <profiles>\n\n");
         fprintf(stderr, "flags:\n\n");
         fprintf(stderr, "%co<profile> - Output devicelink profile. [defaults to 'devicelink.icc']\n", SW);

         PrintRenderingIntents(NULL);

         fprintf(stderr, "%cc<0,1,2> - Precision (0=LowRes, 1=Normal, 2=Hi-res) [defaults to 1]\n", SW);
         fprintf(stderr, "%cn<gridpoints> - Alternate way to set precision, number of CLUT points\n", SW);
         fprintf(stderr, "%cd<description> - description text (quotes can be used)\n", SW);
         fprintf(stderr, "%cy<copyright> - copyright notice (quotes can be used)\n", SW);

         fprintf(stderr, "\n%ck<0..400> - Ink-limiting in %% (CMYK only)\n", SW);
         fprintf(stderr, "%c8 - Creates 8-bit devicelink\n", SW);
         fprintf(stderr, "%cx - Creatively, guess deviceclass of resulting profile.\n", SW);
         fprintf(stderr, "%cb - Black point compensation\n", SW);
         fprintf(stderr, "%ca<0..1> - Observer adaptation state (abs.col. only)\n\n", SW);
         fprintf(stderr, "%cl - Use linearization curves (may affect accuracy)\n", SW);
         fprintf(stderr, "%cr<v.r> - Profile version. (CAUTION: may change the profile implementation)\n", SW);
         fprintf(stderr, "\n");
         fprintf(stderr, "Colorspaces must be paired except Lab/XYZ, that can be interchanged.\n\n");

         fprintf(stderr, "%ch<0,1,2,3> - More help\n", SW);
         break;

     case 1:
         PrintBuiltins();
         break;

     case 2:

         fprintf(stderr, "\nExamples:\n\n"
             "To create 'devicelink.icm' from a.icc to b.icc:\n"
             "\tlinkicc a.icc b.icc\n\n"
             "To create 'out.icc' from sRGB to cmyk.icc:\n"
             "\tlinkicc -o out.icc *sRGB cmyk.icc\n\n"
             "To create a sRGB input profile working in Lab:\n"
             "\tlinkicc -x -o sRGBLab.icc *sRGB *Lab\n\n"
             "To create a XYZ -> sRGB output profile:\n"
             "\tlinkicc -x -o sRGBLab.icc *XYZ *sRGB\n\n"
             "To create a abstract profile doing softproof for cmyk.icc:\n"
             "\tlinkicc -t1 -x -o softproof.icc *Lab cmyk.icc cmyk.icc *Lab\n\n"
             "To create a 'grayer' sRGB input profile:\n"
             "\tlinkicc -x -o grayer.icc *sRGB gray.icc gray.icc *Lab\n\n"
             "To embed ink limiting into a cmyk output profile:\n"
             "\tlinkicc -x -o cmyklimited.icc -k 250 cmyk.icc *Lab\n\n");
         break;

     case 3:

         fprintf(stderr, "This program is intended to be a demo of the little cms\n"
             "engine. Both lcms and this program are freeware. You can\n"
             "obtain both in source code at http://www.littlecms.com\n"
             "For suggestions, comments, bug reports etc. send mail to\n"
             "info@littlecms.com\n\n");
    }

   exit(0);
}

// The toggles stuff
static
void HandleSwitches(int argc, char *argv[])
{
    int s;

    while ((s = xgetopt(argc,argv,"a:A:BbC:c:D:d:h:H:k:K:lLn:N:O:o:r:R:T:t:V:v:xX8y:Y:")) != EOF) {

    switch (s) {


        case 'a':
        case 'A':
            ObserverAdaptationState = atof(xoptarg);
            if (ObserverAdaptationState < 0 ||
                ObserverAdaptationState > 1.0)
                       FatalError("Adaptation state should be 0..1");
            break;

        case 'b':
        case 'B':
            BlackPointCompensation = TRUE;
           break;

        case 'c':
        case 'C':
            PrecalcMode = atoi(xoptarg);
            if (PrecalcMode < 0 || PrecalcMode > 2) {
                FatalError("Unknown precalc mode '%d'", PrecalcMode);
            }
           break;

       case 'd':
       case 'D':
           // Doing that is correct and safe: Description points to memory allocated in the command line.
           // same for Copyright and output devicelink.
           Description = xoptarg;
           break;

        case 'h':
        case 'H':
            Help(atoi(xoptarg));
            return;

        case 'k':
        case 'K':
            InkLimit = atof(xoptarg);
            if (InkLimit < 0.0 || InkLimit > 400.0) {
                FatalError("Ink limit must be 0%%..400%%");
            }
           break;


        case 'l':
        case 'L': KeepLinearization = TRUE;
           break;

       case 'n':
       case 'N':
           if (PrecalcMode != 1) {
               FatalError("Precalc mode already specified");
           }
           NumOfGridPoints = atoi(xoptarg);
           break;

        case 'o':
        case 'O':
            cOutProf = xoptarg;
           break;


       case 'r':
       case 'R':
          Version = atof(xoptarg);
          if (Version < 2.0 || Version > 4.3) {
              fprintf(stderr, "WARNING: lcms was not aware of this version, tag types may be wrong!\n");
          }
          break;

        case 't':
        case 'T':
            Intent = atoi(xoptarg);  // Will be validated latter on
            break;

        case 'V':
        case 'v':
            Verbose = atoi(xoptarg);
            if (Verbose < 0 || Verbose > 3) {
                FatalError("Unknown verbosity level '%d'", Verbose);
            }
            break;

        case '8':
            lUse8bits = TRUE;
            break;



        case 'y':
        case 'Y':
            Copyright = xoptarg;
            break;



       case 'x':
       case 'X': TagResult = TRUE;
           break;



       default:

           FatalError("Unknown option - run without args to see valid ones.\n");
        }
    }
}

// Set the copyright and description
static
cmsBool SetTextTags(cmsContext ContextID, cmsHPROFILE hProfile)
{
    cmsMLU *DescriptionMLU, *CopyrightMLU;
    cmsBool  rc = FALSE;

    DescriptionMLU  = cmsMLUalloc(ContextID, 1);
    CopyrightMLU    = cmsMLUalloc(ContextID, 1);

    if (DescriptionMLU == NULL || CopyrightMLU == NULL) goto Error;

    if (!cmsMLUsetASCII(ContextID, DescriptionMLU,  "en", "US", Description)) goto Error;
    if (!cmsMLUsetASCII(ContextID, CopyrightMLU,    "en", "US", Copyright)) goto Error;

    if (!cmsWriteTag(ContextID, hProfile, cmsSigProfileDescriptionTag,  DescriptionMLU)) goto Error;
    if (!cmsWriteTag(ContextID, hProfile, cmsSigCopyrightTag,           CopyrightMLU)) goto Error;

    rc = TRUE;

Error:

    if (DescriptionMLU)
        cmsMLUfree(ContextID, DescriptionMLU);
    if (CopyrightMLU)
        cmsMLUfree(ContextID, CopyrightMLU);
    return rc;
}



int main(int argc, char *argv[])
{
    int i, nargs, rc;
    cmsHPROFILE Profiles[257];
    cmsHPROFILE hProfile;
    cmsUInt32Number dwFlags;
    cmsHTRANSFORM hTransform = NULL;
    cmsContext ContextID = NULL;

    // Here we are
    fprintf(stderr, "little cms ICC device link generator - v2.2 [LittleCMS %2.2f]\n", LCMS_VERSION / 1000.0);
    fflush(stderr);

    // Initialize
    InitUtils(ContextID, "linkicc");
    rc = 0;

    // Get the options
    HandleSwitches(argc, argv);

    // How many profiles to link?
    nargs = (argc - xoptind);
    if (nargs < 1)
        return Help(0);

    if (nargs > 255) {
        FatalError("Holy profile! what are you trying to do with so many profiles!?");
        goto Cleanup;
    }

    // Open all profiles
    memset(Profiles, 0, sizeof(Profiles));
    for (i=0; i < nargs; i++) {

        Profiles[i] = OpenStockProfile(ContextID, argv[i + xoptind]);
        if (Profiles[i] == NULL) goto Cleanup;

        if (Verbose >= 1) {
            PrintProfileInformation(ContextID, Profiles[i]);
        }
    }

    // Ink limiting
    if (InkLimit != 400.0) {
        cmsColorSpaceSignature EndingColorSpace = cmsGetColorSpace(ContextID, Profiles[nargs-1]);
        Profiles[nargs++] = cmsCreateInkLimitingDeviceLink(ContextID, EndingColorSpace, InkLimit);
    }

    // Set the flags
    dwFlags = cmsFLAGS_KEEP_SEQUENCE;
    switch (PrecalcMode) {

        case 0: dwFlags |= cmsFLAGS_LOWRESPRECALC; break;
        case 2: dwFlags |= cmsFLAGS_HIGHRESPRECALC; break;
        case 1:
            if (NumOfGridPoints > 0)
                dwFlags |= cmsFLAGS_GRIDPOINTS(NumOfGridPoints);
            break;

        default:
            {
                FatalError("Unknown precalculation mode '%d'", PrecalcMode);
                goto Cleanup;
            }
    }

    if (BlackPointCompensation)
        dwFlags |= cmsFLAGS_BLACKPOINTCOMPENSATION;

    if (TagResult)
        dwFlags |= cmsFLAGS_GUESSDEVICECLASS;

    if (KeepLinearization)
        dwFlags |= cmsFLAGS_CLUT_PRE_LINEARIZATION|cmsFLAGS_CLUT_POST_LINEARIZATION;

    if (lUse8bits) dwFlags |= cmsFLAGS_8BITS_DEVICELINK;

     cmsSetAdaptationState(ContextID, ObserverAdaptationState);

    // Create the color transform. Specify 0 for the format is safe as the transform
    // is intended to be used only for the devicelink.
    hTransform = cmsCreateMultiprofileTransform(ContextID, Profiles, nargs, 0, 0, Intent, dwFlags|cmsFLAGS_NOOPTIMIZE);
    if (hTransform == NULL) {
        FatalError("Transform creation failed");
        goto Cleanup;
    }

    hProfile =  cmsTransform2DeviceLink(ContextID, hTransform, Version, dwFlags);
    if (hProfile == NULL) {
        FatalError("Devicelink creation failed");
        goto Cleanup;
    }

    SetTextTags(ContextID, hProfile);
    cmsSetHeaderRenderingIntent(ContextID, hProfile, Intent);

    if (cmsSaveProfileToFile(ContextID, hProfile, cOutProf)) {

        if (Verbose > 0)
            fprintf(stderr, "Ok");
    }
    else
        FatalError("Error saving file!");

    cmsCloseProfile(ContextID, hProfile);


Cleanup:

    if (hTransform != NULL) cmsDeleteTransform(ContextID, hTransform);
    for (i=0; i < nargs; i++) {

        if (Profiles[i] != NULL) cmsCloseProfile(ContextID, Profiles[i]);
    }

    return rc;
}
