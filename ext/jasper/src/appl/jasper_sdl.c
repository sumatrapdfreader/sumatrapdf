/* Jasper-based sdl image display utility

   Copyright (C) 2004-2005 Artifex Software, Inc.

   Licensed under the GNU GPL
*/

/** includes **/

#include <stdio.h>
#include <stdlib.h>

#include <jasper/jasper.h>
#include <SDL.h>

#define MAX(x,y) (((x)>(y))?(x):(y))

/** prototypes **/

int open_image(const char * filename);

int dump_jas_image(jas_image_t *image);
int copy_jas_image(jas_image_t * image, SDL_Surface * window);
int display_jas_image(jas_image_t * image);

void wait_close(void);


/** implementation **/

int
dump_jas_image(jas_image_t *image)
{
    int i, numcmpts = jas_image_numcmpts(image);
    int clrspc = jas_image_clrspc(image);
    const char *csname = "unrecognized vendor space";
 
    if (image == NULL) return 1;
 
    printf("image is %d x %d",
        jas_image_width(image), jas_image_height(image));
 
    /* sort the colorspace */
    if jas_clrspc_isunknown(clrspc) csname = "unknown";
    else switch (clrspc) {
        case JAS_CLRSPC_CIEXYZ: csname = "CIE XYZ"; break;
        case JAS_CLRSPC_CIELAB: csname = "CIE Lab"; break;
        case JAS_CLRSPC_SGRAY: csname = "calibrated grayscale"; break;
        case JAS_CLRSPC_SRGB: csname = "sRGB"; break;
        case JAS_CLRSPC_SYCBCR: csname = "calibrated YCbCr"; break;
        case JAS_CLRSPC_GENGRAY: csname = "generic gray"; break;
        case JAS_CLRSPC_GENRGB: csname = "generic RGB"; break;
        case JAS_CLRSPC_GENYCBCR: csname = "generic YCbCr"; break;
    }
    printf(" colorspace is %s (family %d, member %d)\n",
        csname, jas_clrspc_fam(clrspc), jas_clrspc_mbr(clrspc));
                                                                                
    for (i = 0; i < numcmpts; i++) {
        int type = jas_image_cmpttype(image, i);
        const char *opacity = (type & JAS_IMAGE_CT_OPACITY) ? " opacity" : "";
        const char *name = "unrecognized";
        const char *issigned = "";
        if (jas_clrspc_fam(clrspc) == JAS_CLRSPC_FAM_GRAY)
            name = "gray";
        else if (jas_clrspc_fam(clrspc) == JAS_CLRSPC_FAM_RGB)
            switch (JAS_IMAGE_CT_COLOR(type)) {
                case JAS_IMAGE_CT_RGB_R: name = "red"; break;
                case JAS_IMAGE_CT_RGB_G: name = "green"; break;
                case JAS_IMAGE_CT_RGB_B: name = "blue"; break;
                case JAS_IMAGE_CT_UNKNOWN:
                default:
                    name = "unknown";
            }
        else if (jas_clrspc_fam(clrspc) == JAS_CLRSPC_FAM_YCBCR)
            switch (JAS_IMAGE_CT_COLOR(type)) {
                case JAS_IMAGE_CT_YCBCR_Y: name = "luminance Y"; break;
                case JAS_IMAGE_CT_YCBCR_CB: name = "chrominance Cb"; break;
                case JAS_IMAGE_CT_YCBCR_CR: name = "chrominance Cr"; break;
                case JAS_IMAGE_CT_UNKNOWN:
                default:
                    name = "unknown";
            }
        if (jas_image_cmptsgnd(image, i))
            issigned = ", signed";
        printf("  component %d: type %d '%s%s' (%d bits%s)",
            i, type, name, opacity, jas_image_cmptprec(image, i), issigned);
        printf(" grid step (%d,%d) offset (%d,%d)\n",
            jas_image_cmpthstep(image, i), jas_image_cmptvstep(image, i),
            jas_image_cmpttlx(image, i), jas_image_cmpttly(image, i));
    }
                                                                                
    return 0;
}

int copy_jas_image(jas_image_t * image, SDL_Surface * window)
{
    int i,j;
    unsigned char *pixels;
    SDL_Rect rect;
    int ticks = SDL_GetTicks();
    const int bpp = window->format->BytesPerPixel;
    const int width = jas_image_width(image);
    const int height = jas_image_height(image);
    const int r = jas_image_getcmptbytype(image, JAS_IMAGE_CT_RGB_R);
    const int g = jas_image_getcmptbytype(image, JAS_IMAGE_CT_RGB_G);
    const int b = jas_image_getcmptbytype(image, JAS_IMAGE_CT_RGB_B);
    const int shift_r = MAX(jas_image_cmptprec(image, r) - 8, 0);
    const int shift_g = MAX(jas_image_cmptprec(image, g) - 8, 0);
    const int shift_b = MAX(jas_image_cmptprec(image, b) - 8, 0);
    
    rect.x = 0;
    rect.y = 0;
    rect.w = width;
    rect.h = height;

    for (j = 0; j < height; j++) {
	pixels = (unsigned char *)window->pixels + j * window->pitch;
        for (i = 0; i < width; i++) {
            pixels[0] = 0;
            pixels[1] = jas_image_readcmptsample(image, r, i, j) >> shift_r;
            pixels[2] = jas_image_readcmptsample(image, g, i, j) >> shift_g;
            pixels[3] = jas_image_readcmptsample(image, b, i, j) >> shift_b;
            pixels += bpp;
        }
        if (((j & 0xF) == 0) && (ticks - SDL_GetTicks() > 1000)) {
            rect.h = j - rect.y;
            if (SDL_MUSTLOCK(window)) {
                SDL_UnlockSurface(window);
            }
            SDL_UpdateRects(window, 1, &rect);
            if (SDL_MUSTLOCK(window)) {
                if (SDL_LockSurface(window) < 0) {
                    jas_eprintf("Can't lock drawing surface: %s\n", SDL_GetError());
                    return -1;
                }
            }
            rect.y += rect.h;
        }
    }

    return 0;
}

void wait_close(void)
{
    SDL_Event event;

    while (SDL_WaitEvent(&event) != 0) {
        switch (event.type) {
            case SDL_QUIT:
                return;
        }
    }
}

int display_jas_image(jas_image_t * image)
{
    int width, height;
    SDL_Surface *window;
    SDL_Rect bounds;

    width = jas_image_width(image);
    height = jas_image_height(image);
    window = SDL_SetVideoMode(width, height, 32, SDL_SWSURFACE);
    if (window == NULL) {
        jas_eprintf("Unable to open %dx%d image window: %s\n",
            width, height, SDL_GetError());
        return -1;
    }

    bounds.x = 0;
    bounds.y = 0;
    bounds.w = width;
    bounds.h = height;

    if (SDL_MUSTLOCK(window)) {
        if (SDL_LockSurface(window) < 0) {
            jas_eprintf("Can't lock drawing surface: %s\n", SDL_GetError());
            return -1;
        }
    }

    copy_jas_image(image, window);

    if (SDL_MUSTLOCK(window)) {
        SDL_UnlockSurface(window);
    }
    SDL_UpdateRects(window, 1, &bounds);

    wait_close();

    SDL_FreeSurface(window);
    return 0;
}

int open_image(const char * filename)
{
    jas_stream_t *stream;
    jas_image_t *image;

    stream  = jas_stream_fopen(filename, "rb");
    if (stream == NULL) {
        jas_eprintf("error: could not open '%s'\n", filename);
        return -1;
    }
    image = jas_image_decode(stream, -1, NULL);

    jas_stream_close(stream);

    if (image == NULL) {
        jas_eprintf("error: could not decode image data from '%s'\n", filename);
        return -2;
    }

    dump_jas_image(image);

    display_jas_image(image);

    jas_image_destroy(image);

    return 0;
}

int main(int argc, char *argv[])
{
    int i;

    if (jas_init()) {
        jas_eprintf("error: unable to initialize jasper library\n");
        exit(1);
    }
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        jas_eprintf("error: unable to initialize SDL: %s\n", SDL_GetError());
        exit(2);
    }

    for (i = 1; i < argc; i++) {
       open_image(argv[i]);
    }

    SDL_Quit();
    jas_cleanup();

    return 0;
}
