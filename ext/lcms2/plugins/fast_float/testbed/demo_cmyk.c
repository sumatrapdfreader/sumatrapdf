//---------------------------------------------------------------------------------
//
//  Little Color Management System, fast floating point extensions
//  Copyright (c) 1998-2022 Marti Maria Saguer, all rights reserved
//
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//---------------------------------------------------------------------------------

#include "lcms2_fast_float.h"

#include <stdlib.h>
#include <memory.h>

static
void Fail(const char* frm, ...)
{
    va_list args;

    va_start(args, frm);
    vprintf(frm, args);
    va_end(args);
    exit(1);
}



#define ALIGNED_SIZE(a,x)  (((x)+((a) - 1)) & ~((a) - 1))


static
void* make_image(size_t size_x, size_t size_y, cmsBool fill_rgb, cmsUInt32Number* stride_x)
{
    cmsUInt32Number size_x_aligned = ALIGNED_SIZE(16, size_x);
    cmsUInt32Number line_size_in_bytes = size_x_aligned * sizeof(cmsUInt32Number); // RGBA

    cmsUInt8Number* ptr_image = (cmsUInt8Number*) calloc(size_y, line_size_in_bytes);
    
    if (ptr_image == NULL) Fail("Couldn't allocate memory for image");

    if (fill_rgb)
    {
        size_t line;

        for (line = 0; line < size_y; line++)
        {
            cmsUInt32Number* ptr_line = (cmsUInt32Number*)(ptr_image + line_size_in_bytes * line);          
            cmsUInt32Number argb = 0;
            int col;

            for (col = 0; col < size_x; col++)
                *ptr_line++ = argb++;
                
        }
    }

    *stride_x = line_size_in_bytes;
    return (void*) ptr_image;
}

#define SIZE_X 10000
#define SIZE_Y 10000

static
cmsFloat64Number MPixSec(cmsFloat64Number diff)
{
    cmsFloat64Number seconds = (cmsFloat64Number)diff / (cmsFloat64Number)CLOCKS_PER_SEC;
    return (SIZE_X * SIZE_Y) / (1024.0 * 1024.0 * seconds);
}



static
cmsFloat64Number speed_test(void)
{
    clock_t atime;
    cmsFloat64Number diff;
    cmsHPROFILE hProfileIn;
    cmsHPROFILE hProfileOut;   
    cmsHTRANSFORM xform;
    void* image_in;
    void* image_out;
    cmsUInt32Number stride_rgb_x, stride_cmyk_x;


    hProfileIn = cmsOpenProfileFromFile("sRGB Color Space Profile.icm", "r");
    hProfileOut = cmsOpenProfileFromFile("USWebCoatedSWOP.icc", "r");

    if (hProfileIn == NULL || hProfileOut == NULL)
        Fail("Unable to open profiles");

    xform = cmsCreateTransform(hProfileIn, TYPE_RGBA_8, hProfileOut, TYPE_CMYK_8, INTENT_PERCEPTUAL, 0);
    cmsCloseProfile(hProfileIn);
    cmsCloseProfile(hProfileOut);

    
    image_in = make_image(SIZE_X, SIZE_Y, TRUE, &stride_rgb_x);
    image_out = make_image(SIZE_X, SIZE_Y, FALSE, &stride_cmyk_x);

    atime = clock();

    cmsDoTransformLineStride(xform, image_in, image_out, SIZE_X, SIZE_Y, stride_rgb_x, stride_cmyk_x, 0, 0);
    
    diff = clock() - atime;

    free(image_in);
    free(image_out);

    cmsDeleteTransform(xform);
    return MPixSec(diff);
}


int main(void)
{
    cmsFloat64Number without_plugin;
    cmsFloat64Number with_plugin;
    
    fprintf(stdout, "DEMO of littleCMS fast float plugin: RGBA -> CMYK in Megapixels per second\n");  fflush(stdout);

    // filling cache
    fprintf(stdout, "Wait CPU cache to stabilize: ");  fflush(stdout);
    speed_test();
    fprintf(stdout, "Ok\n");

    fprintf(stdout, "Without plugin: ");  fflush(stdout);
    without_plugin = speed_test();
    fprintf(stdout, "%.2f\n", without_plugin); fflush(stdout);

    cmsPlugin(cmsFastFloatExtensions());

    fprintf(stdout, "With plugin: ");  fflush(stdout);
    with_plugin = speed_test();
    fprintf(stdout, "%.2f\n", with_plugin); fflush(stdout);
    
    fprintf(stdout, "x %2.2f\n", (with_plugin/without_plugin)); fflush(stdout);

    return 0;    
}