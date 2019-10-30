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
// THIS SOFTWARE IS PROVIDED "AS-IS" AND WITHOUT WARRANTY OF ANY KIND,
// EXPRESS, IMPLIED OR OTHERWISE, INCLUDING WITHOUT LIMITATION, ANY
// WARRANTY OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
//
// IN NO EVENT SHALL MARTI MARIA BE LIABLE FOR ANY SPECIAL, INCIDENTAL,
// INDIRECT OR CONSEQUENTIAL DAMAGES OF ANY KIND,
// OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
// WHETHER OR NOT ADVISED OF THE POSSIBILITY OF DAMAGE, AND ON ANY THEORY OF
// LIABILITY, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
// OF THIS SOFTWARE.
//
// Version 1.12


#include "lcms.h"


typedef struct {
				cmsHPROFILE   hLab;
				cmsHPROFILE   hRGB;
				cmsHTRANSFORM Lab2RGB;
				cmsHTRANSFORM RGB2Lab;

				} CARGO, FAR* LPCARGO;


	 
 

// Our space will be CIE primaries plus a gamma of 4.5

static
int Forward(register WORD In[], register WORD Out[], register LPVOID Cargo)
{	
	LPCARGO C = (LPCARGO) Cargo;	
	WORD RGB[3];
    cmsCIELab Lab;

    cmsLabEncoded2Float(&Lab, In);

	printf("%g %g %g\n", Lab.L, Lab.a, Lab.b);

	cmsDoTransform(C ->Lab2RGB, In, &RGB, 1);


	Out[0] = 0xFFFF - RGB[0]; // Our CMY is negative of RGB
	Out[1] = 0xFFFF - RGB[1]; 
	Out[2] = 0xFFFF - RGB[2]; 
	
	
	return TRUE;

}


static
int Reverse(register WORD In[], register WORD Out[], register LPVOID Cargo)
{	

	LPCARGO C = (LPCARGO) Cargo;	
	WORD RGB[3];
  
	RGB[0] = 0xFFFF - In[0];
	RGB[1] = 0xFFFF - In[1];
	RGB[2] = 0xFFFF - In[2];

	cmsDoTransform(C ->RGB2Lab, &RGB, Out, 1);
	
	return TRUE;

}



static
void InitCargo(LPCARGO Cargo)
{
	

	Cargo -> hLab = cmsCreateLabProfile(NULL);
	Cargo -> hRGB = cmsCreate_sRGBProfile();  
	
	Cargo->Lab2RGB = cmsCreateTransform(Cargo->hLab, TYPE_Lab_16, 
									    Cargo ->hRGB, TYPE_RGB_16,
										INTENT_RELATIVE_COLORIMETRIC, 
										cmsFLAGS_NOTPRECALC);

	Cargo->RGB2Lab = cmsCreateTransform(Cargo ->hRGB, TYPE_RGB_16, 
										Cargo ->hLab, TYPE_Lab_16, 
										INTENT_RELATIVE_COLORIMETRIC, 
										cmsFLAGS_NOTPRECALC);
}




static
void FreeCargo(LPCARGO Cargo)
{
	cmsDeleteTransform(Cargo ->Lab2RGB);
	cmsDeleteTransform(Cargo ->RGB2Lab);
	cmsCloseProfile(Cargo ->hLab);
	cmsCloseProfile(Cargo ->hRGB);
}

	
	
	
int main(void)
{
	LPLUT AToB0, BToA0;	
	CARGO Cargo;
	cmsHPROFILE hProfile;
	
	fprintf(stderr, "Creating lcmscmy.icm...");	
	
	InitCargo(&Cargo);

	hProfile = cmsCreateLabProfile(NULL);
	

    AToB0 = cmsAllocLUT();
	BToA0 = cmsAllocLUT();

	cmsAlloc3DGrid(AToB0, 25, 3, 3);
	cmsAlloc3DGrid(BToA0, 25, 3, 3);
	
	
	cmsSample3DGrid(AToB0, Reverse, &Cargo, 0);
	cmsSample3DGrid(BToA0, Forward, &Cargo, 0);
	
	
    cmsAddTag(hProfile, icSigAToB0Tag, AToB0);
	cmsAddTag(hProfile, icSigBToA0Tag, BToA0);

	cmsSetColorSpace(hProfile, icSigCmyData);
	cmsSetDeviceClass(hProfile, icSigOutputClass);

	cmsAddTag(hProfile, icSigProfileDescriptionTag, "CMY ");
    cmsAddTag(hProfile, icSigCopyrightTag,          "Copyright (c) HP, 2007. All rights reserved.");
    cmsAddTag(hProfile, icSigDeviceMfgDescTag,      "Little cms");    
    cmsAddTag(hProfile, icSigDeviceModelDescTag,    "CMY space");

	_cmsSaveProfile(hProfile, "lcmscmy.icm");
	
	
	cmsFreeLUT(AToB0);
	cmsFreeLUT(BToA0);
	cmsCloseProfile(hProfile);	
	FreeCargo(&Cargo);
	fprintf(stderr, "Done.\n");



	return 0;
}
