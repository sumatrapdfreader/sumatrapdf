//
//  Little cms
//  Copyright (C) 1998-2010 Marti Maria
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

// Creates a devicelink that decodes TIFF8 Lab files 

#include "lcms2mt.h"
#include <stdlib.h>
#include <math.h>

static
double DecodeAbTIFF(double ab)
{
	if (ab <= 128.)
		ab += 127.;
	else
		ab -= 127.;

	return ab;
}

static
cmsToneCurve* CreateStep(void)
{
	cmsToneCurve* Gamma;
	cmsUInt16Number* Table;
	int i;
	double a;

	Table = calloc(4096, sizeof(cmsUInt16Number));
	if (Table == NULL) return NULL;

	for (i=0; i < 4096; i++) {

		a = (double) i * 255. / 4095.;

		a = DecodeAbTIFF(a);

		Table[i] = (cmsUInt16Number) floor(a * 257. + 0.5);
	}

	Gamma = cmsBuildTabulatedToneCurve16(0, 4096, Table);
	free(Table);

	return Gamma;
}


static
cmsToneCurve* CreateLinear(void)
{
	cmsUInt16Number Linear[2] = { 0, 0xffff };

	return cmsBuildTabulatedToneCurve16(0, 2, Linear);          
}



// Set the copyright and description
static
cmsBool SetTextTags(cmsHPROFILE hProfile)
{
    cmsMLU *DescriptionMLU, *CopyrightMLU;
    cmsBool  rc = FALSE;
  
    DescriptionMLU  = cmsMLUalloc(0, 1);
    CopyrightMLU    = cmsMLUalloc(0, 1);

    if (DescriptionMLU == NULL || CopyrightMLU == NULL) goto Error;

    if (!cmsMLUsetASCII(DescriptionMLU,  "en", "US", "Little cms Tiff8 CIELab")) goto Error;
    if (!cmsMLUsetASCII(CopyrightMLU,    "en", "US", "Copyright (c) Marti Maria, 2010. All rights reserved.")) goto Error;

    if (!cmsWriteTag(hProfile, cmsSigProfileDescriptionTag,  DescriptionMLU)) goto Error;
    if (!cmsWriteTag(hProfile, cmsSigCopyrightTag,           CopyrightMLU)) goto Error;     

    rc = TRUE;

Error:

    if (DescriptionMLU)
        cmsMLUfree(DescriptionMLU);
    if (CopyrightMLU)
        cmsMLUfree(CopyrightMLU);
    return rc;
}


int main(int argc, char *argv[])
{
	cmsHPROFILE hProfile;
	cmsPipeline *AToB0;
	cmsToneCurve* PreLinear[3];
	cmsToneCurve *Lin, *Step;

	fprintf(stderr, "Creating lcmstiff8.icm...");
    
    remove("lcmstiff8.icm");
	hProfile = cmsOpenProfileFromFile("lcmstiff8.icm", "w");

	// Create linearization
	Lin  = CreateLinear();
	Step = CreateStep();

	PreLinear[0] = Lin;
	PreLinear[1] = Step;
	PreLinear[2] = Step;

    AToB0 = cmsPipelineAlloc(0, 3, 3);

	cmsPipelineInsertStage(AToB0, 
		cmsAT_BEGIN, cmsStageAllocToneCurves(0, 3, PreLinear));

	cmsSetColorSpace(hProfile, cmsSigLabData);
	cmsSetPCS(hProfile, cmsSigLabData);
	cmsSetDeviceClass(hProfile, cmsSigLinkClass);
	cmsSetProfileVersion(hProfile, 4.2);

    cmsWriteTag(hProfile, cmsSigAToB0Tag, AToB0);
	
    SetTextTags(hProfile);

	cmsCloseProfile(hProfile);

	cmsFreeToneCurve(Lin);
	cmsFreeToneCurve(Step);
	cmsPipelineFree(AToB0);
		
	fprintf(stderr, "Done.\n");

	return 0;
}
