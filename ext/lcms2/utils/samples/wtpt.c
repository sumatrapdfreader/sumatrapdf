//
//  Little cms
//  Copyright (C) 1998-2015 Marti Maria
//
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

#include "utils.h"


// The toggles stuff

static cmsBool lShowXYZ = TRUE;
static cmsBool lShowLab = FALSE;
static cmsBool lShowLCh = FALSE;

static
void HandleSwitches(int argc, char *argv[])
{
       int s;

       while ((s = xgetopt(argc, argv, "lcx")) != EOF) {

              switch (s){


              case 'l':
                     lShowLab = TRUE;
                     break;

              case 'c':
                     lShowLCh = TRUE;
                     break;

              case 'x':
                     lShowXYZ = FALSE;
                     break;

              default:

                     FatalError("Unknown option - run without args to see valid ones.\n");
              }
       }
}

static
void Help(void)
{
       fprintf(stderr, "little CMS ICC white point utility - v3 [LittleCMS %2.2f]\n", LCMS_VERSION / 1000.0);

       fprintf(stderr, "usage: wtpt [flags] [<ICC profile>]\n\n");

       fprintf(stderr, "flags:\n\n");
       
       fprintf(stderr, "%cl - CIE Lab\n", SW);
       fprintf(stderr, "%cc - CIE LCh\n", SW);
       fprintf(stderr, "%cx - Don't show XYZ\n", SW);

       fprintf(stderr, "\nIf no parameters are given, then this program will\n");
       fprintf(stderr, "ask for XYZ value of media white. If parameter given, it must be\n");
       fprintf(stderr, "the profile to inspect.\n\n");

       fprintf(stderr, "This program is intended to be a demo of the little cms\n"
              "engine. Both lcms and this program are freeware. You can\n"
              "obtain both in source code at http://www.littlecms.com\n"
              "For suggestions, comments, bug reports etc. send mail to\n"
              "info@littlecms.com\n\n");
       exit(0);
}



static
void ShowWhitePoint(cmsCIEXYZ* WtPt)
{
       cmsCIELab Lab;
       cmsCIELCh LCh;
       cmsCIExyY xyY;


       cmsXYZ2Lab(NULL, &Lab, WtPt);
       cmsLab2LCh(&LCh, &Lab);
       cmsXYZ2xyY(&xyY, WtPt);


       if (lShowXYZ) printf("XYZ=(%3.1f, %3.1f, %3.1f)\n", WtPt->X, WtPt->Y, WtPt->Z);
       if (lShowLab) printf("Lab=(%3.3f, %3.3f, %3.3f)\n", Lab.L, Lab.a, Lab.b);
       if (lShowLCh) printf("LCh=(%3.3f, %3.3f, %3.3f)\n", LCh.L, LCh.C, LCh.h);
       {
              double Ssens = (LCh.C * 100.0 )/ sqrt(LCh.C*LCh.C + LCh.L * LCh.L) ;
              printf("Sens = %f\n", Ssens);
       }

}


int main(int argc, char *argv[])
{
       int nargs;

       InitUtils("wtpt");
       
       HandleSwitches(argc, argv);

       nargs = (argc - xoptind);

       if (nargs != 1)
              Help();

       else {
              cmsCIEXYZ* WtPt;
              cmsHPROFILE hProfile = cmsOpenProfileFromFile(argv[xoptind], "r");  
              if (hProfile == NULL) return 1;

              WtPt = cmsReadTag(hProfile, cmsSigMediaWhitePointTag);
              ShowWhitePoint(WtPt);
              cmsCloseProfile(hProfile);
       }
       
       return 0;
}

