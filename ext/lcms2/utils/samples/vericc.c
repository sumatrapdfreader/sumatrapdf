//---------------------------------------------------------------------------------
//
//  Little Color Management System
//  Copyright (c) 1998-2010 Marti Maria Saguer
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

#include "lcms2mt.h"
#include <string.h>
#include <math.h>

static
int PrintUsage(void)
{
	fprintf(stderr, "Sets profile version\n\nUsage: vericc --r<version> iccprofile.icc\n"); 
	return 0; 
}

int main(int argc, char *argv[])
{
       cmsHPROFILE hProfile;
	   char* ptr;
	   cmsFloat64Number Version;

	   if (argc != 3)  return PrintUsage();

	   ptr = argv[1];
	   if (strncmp(ptr, "--r", 3) != 0) return PrintUsage();
	   ptr += 3;
	   if (!*ptr) { fprintf(stderr, "Wrong version number\n"); return 1; }

	   Version = atof(ptr); 

	   hProfile = cmsOpenProfileFromFile(argv[2], "r");
	   if (hProfile == NULL) { fprintf(stderr, "'%s': cannot open\n", argv[2]); return 1; }

	   cmsSetProfileVersion(hProfile, Version);
	   cmsSaveProfileToFile(hProfile, "$$tmp.icc");
	   cmsCloseProfile(hProfile);

	   remove(argv[2]);
	   rename("$$tmp.icc", argv[2]);
	   return 0;


}
