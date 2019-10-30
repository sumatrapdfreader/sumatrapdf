//
//  Little cms
//  Copyright (C) 1998-2003 Marti Maria
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


#include "lcms.h"



static
int Forward(register WORD In[], register WORD Out[], register LPVOID Cargo)
{	
    cmsCIELab Lab;


    cmsLabEncoded2Float(&Lab, In);

	if (fabs(Lab.a) < 3 && fabs(Lab.b) < 3) {
		
		double L_01 = Lab.L / 100.0;
	    WORD K;

		if (L_01 > 1) L_01 = 1;
		K = (WORD) floor(L_01* 65535.0 + 0.5);

		Out[0] = Out[1] = Out[2] = K; 
	}
	else {
		Out[0] = 0xFFFF; Out[1] = 0; Out[2] = 0; 
	}

	return TRUE;
}




	
int main(int argc, char *argv[])
{
	LPLUT BToA0;
	cmsHPROFILE hProfile;

	fprintf(stderr, "Creating interpol2.icc...");

	unlink("interpol2.icc");
	hProfile = cmsOpenProfileFromFile("interpol2.icc", "w8");


    BToA0 = cmsAllocLUT();

	cmsAlloc3DGrid(BToA0, 17, 3, 3);
	    
	cmsSample3DGrid(BToA0, Forward, NULL, 0);
			
    cmsAddTag(hProfile, icSigBToA0Tag, BToA0);
	                                
	cmsSetColorSpace(hProfile, icSigRgbData);
    cmsSetPCS(hProfile, icSigLabData);
    cmsSetDeviceClass(hProfile, icSigOutputClass);

	cmsAddTag(hProfile, icSigProfileDescriptionTag, "Interpolation test");
    cmsAddTag(hProfile, icSigCopyrightTag,          "Copyright (c) HP 2007. All rights reserved.");
    cmsAddTag(hProfile, icSigDeviceMfgDescTag,      "Little cms");    
    cmsAddTag(hProfile, icSigDeviceModelDescTag,    "Interpolation test profile");

	
	cmsCloseProfile(hProfile);
    
	cmsFreeLUT(BToA0);
	
	fprintf(stderr, "Done.\n");

	return 0;
}
