//
//  Little cms
//  Copyright (C) 1998-2011 Marti Maria
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

#include "lcms2mt.h"
#include <math.h>



static
double VecDist(cmsUInt8Number bin[3], cmsUInt8Number bout[3])
{
       double rdist, gdist, bdist;

       rdist = fabs((double) bout[0] - bin[0]);
       gdist = fabs((double) bout[1] - bin[1]);
       bdist = fabs((double) bout[2] - bin[2]);

       return (sqrt((rdist*rdist + gdist*gdist + bdist*bdist)));
}


int main(int  argc, char* argv[])
{

    int r, g, b;
    cmsUInt8Number RGB[3], RGB_OUT[3];
    cmsHTRANSFORM xform;
    cmsHPROFILE hProfile;
    double err, SumX=0, SumX2=0, Peak = 0, n = 0;


    if (argc != 2) {
        printf("roundtrip <RGB icc profile>\n");
        return 1;
    }

    hProfile = cmsOpenProfileFromFile(argv[1], "r");
    if (hProfile == NULL)
    {
        printf("invalid profile\n");
        return 1;
    }

    xform = cmsCreateTransform(hProfile,TYPE_RGB_8, hProfile, TYPE_RGB_8, INTENT_RELATIVE_COLORIMETRIC, cmsFLAGS_NOOPTIMIZE);
    if (xform == NULL)
    {
        printf("Not a valid RGB profile\n");
        return 1;
    }

    for (r=0; r< 256; r++) {
        printf("%d  \r", r);
        for (g=0; g < 256; g++) {
            for (b=0; b < 256; b++) {

                RGB[0] = r;
                RGB[1] = g;
                RGB[2] = b;

                cmsDoTransform(xform, RGB, RGB_OUT, 1);

                err = VecDist(RGB, RGB_OUT);

                SumX  += err;
                SumX2 += err * err;
                n += 1.0;
                if (err > Peak)
                    Peak = err;

            }
        }
    }

    printf("Average %g\n", SumX / n);
    printf("Max %g\n", Peak);
    printf("Std  %g\n", sqrt((n*SumX2 - SumX * SumX) / (n*(n-1))));
    cmsCloseProfile(hProfile);
    cmsDeleteTransform(xform);

    return 0;
}