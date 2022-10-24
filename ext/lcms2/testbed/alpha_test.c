

#include <stdio.h>
#include <stdint.h>
#include <math.h>

#include "lcms2_internal.h"

/**
* Premultiplied alpha. This conversion generates irreversible information loss.
* 
* 8 bits:
*	prgb = rgb * (alpha/255) 
*	rgb  = prgb * (255 / alpha)
* 
* 16 bits:
*	prgb = rgb * (alpha/65535)
*	rgb  = prgb * (65535/alpha)
*
*/
uint8_t to_premul8_float(uint8_t rgb8, uint8_t a8)
{	
    double alpha_factor, rgb;

    if (a8 == 0) return rgb8;
    alpha_factor = (double) a8 / 255.0;
    rgb = ((double) rgb8 * alpha_factor);

    return (uint8_t)round(rgb);
}

uint8_t from_premul8_float(uint8_t rgb8, uint8_t a8)
{
    double alpha_factor, rgb;

    if (a8 == 0) return rgb8;
    alpha_factor = 255.0 / (double)a8;
    rgb = ((double)rgb8 * alpha_factor);
    if (rgb > 255.0) rgb = 255.0;
    return (uint8_t)round(rgb);
}

uint16_t to_premul16_float(uint16_t rgb16, uint16_t a16)
{
    double alpha_factor, rgb;

    if (a16 == 0) return rgb16;
    alpha_factor = (double)a16 / 65535.0;
    rgb = ((double)rgb16 * alpha_factor);
    return (uint16_t)round(rgb);
}

uint16_t from_premul16_float(uint16_t rgb16, uint16_t a16)
{
    double alpha_factor, rgb;

    if (a16 == 0) return rgb16;
    alpha_factor = 65535.0 / (double)a16;
    rgb = ((double)rgb16 * alpha_factor);
    if (rgb > 65535.0) rgb = 65535.0;
    return (uint16_t)round(rgb);
}


/**
** Optimized versions
* 
* alpha_factor goes 0..1.0 in 1.15 fixed point format
*		(a16 / 0xffff) which equals to _cmsToFixedDomain() inline (15.16)
* 
*  rgb 16.0 fixed point x alpha factor 1.15 = (a*b + 0x8000) >> 15
* 
*/
uint16_t to_premul16(uint16_t rgb16, uint16_t a16)
{
    uint32_t alpha_factor, rgb;

    if (a16 == 0) return rgb16;	
    alpha_factor = _cmsToFixedDomain(a16);
    rgb = ((uint32_t) rgb16 * alpha_factor + 0x8000) >> 16;

    return (uint16_t)rgb;
}

uint16_t from_premul16(uint16_t rgb16, uint16_t a16)
{
    uint32_t alpha_factor, rgb;

    if (a16 == 0) return rgb16;
    alpha_factor = _cmsToFixedDomain(a16);    
    rgb = (((uint32_t) rgb16) << 16) / alpha_factor;
    if (rgb > 0xffff) rgb = 0xffff;

    return (uint16_t)rgb;
}


uint8_t to_premul8(uint8_t rgb8, uint8_t a8)
{
    uint32_t alpha_factor, rgb;

    if (a8 == 0) return rgb8;
    alpha_factor = _cmsToFixedDomain(FROM_8_TO_16(a8));
    rgb = ((uint32_t)rgb8 * alpha_factor + 0x8000) >> 16;
    return (uint8_t)rgb;
}


uint8_t from_premul8(uint8_t rgb8, uint8_t a8)
{
    uint32_t alpha_factor, rgb;

    if (a8 == 0) return rgb8;
    alpha_factor = _cmsToFixedDomain(FROM_8_TO_16(a8));
    rgb = (((uint32_t)rgb8) << 16) / alpha_factor;
    if (rgb > 0xff) rgb = 0xff;
    return (uint8_t)rgb;
}


static
void dif16to(void)
{
    int32_t gpremul, gpremul1;
    int32_t max, max1, max2, a, g;
    
    printf("Premul TO diff\n");
    max = max1 = max2 = 0;
    for (a = 0; a < 65536; a += 255)
        for (g = 0; g < 65536; g++)
        {
            gpremul = to_premul16_float(g, a);
            gpremul1 = to_premul16(g, a);
            
            if (gpremul != gpremul1)
            {
                int32_t dif = abs(gpremul - gpremul1);
                if (dif > max)
                {
                    max = dif;
                    max1 = gpremul;
                    max2 = gpremul1;
                }

            }
        }

    printf("Error max=%d on pre:%d pre1:%d\n", max, max1, max2);

}


static
void dif16from(void)
{
    int32_t gpremul, gpremul1;
    int32_t max, max1, max2, maxa, maxg, a, g;

    printf("Premul FROM diff\n");
    max = max1 = max2 = maxa = maxg = 0;
    for (a = 0; a < 65536; a += 255)
        for (g = 0; g < 65536; g++)
        {
            gpremul = from_premul16_float(g, a);
            gpremul1 = from_premul16(g, a);

            if (gpremul != gpremul1)
            {
                int32_t dif = abs(gpremul - gpremul1);
                if (dif > max)
                {
                    max = dif;
                    max1 = gpremul;
                    max2 = gpremul1;
                    maxa = a;
                    maxg = g;
                }

            }
        }

    printf("Error max=%d on pre:%d pre1:%d (a:%d g:%d)\n", max, max1, max2, maxa, maxg);

    from_premul16_float(maxg, maxa);
    from_premul16(maxg, maxa);
}

static
void dif8to(void)
{
    int32_t gpremul, gpremul1;
    int32_t max, max1, max2, a, g;

    printf("Premul TO8 diff\n");
    max = max1 = max2 = 0;
    for (a = 0; a < 256; a++)
        for (g = 0; g < 256; g++)
        {
            gpremul = to_premul8_float(g, a);
            gpremul1 = to_premul8(g, a);

            if (gpremul != gpremul1)
            {
                int32_t dif = abs(gpremul - gpremul1);
                if (dif > max)
                {
                    max = dif;
                    max1 = gpremul;
                    max2 = gpremul1;
                }

            }
        }

    printf("Error max=%d on pre:%d pre1:%d\n", max, max1, max2);

}


static
void dif8from(void)
{
    int32_t gpremul, gpremul1;
    int32_t max, max1, max2, maxa, maxg, a, g;

    printf("Premul FROM8 diff\n");
    max = max1 = max2 = maxa = maxg = 0;
    for (a = 0; a < 256; a++)
        for (g = 0; g < 256; g++)
        {
            gpremul = from_premul8_float(g, a);
            gpremul1 = from_premul8(g, a);

            if (gpremul != gpremul1)
            {
                int32_t dif = abs(gpremul - gpremul1);
                if (dif > max)
                {
                    max = dif;
                    max1 = gpremul;
                    max2 = gpremul1;
                    maxa = a;
                    maxg = g;
                }

            }
        }

    printf("Error max=%d on pre:%d pre1:%d (a:%d g:%d)\n", max, max1, max2, maxa, maxg);    

    from_premul8_float(maxg, maxa);
    from_premul8(maxg, maxa);
}






static
void toFixedDomain(void)
{
    int32_t g;

    for (g = 0; g < 65536; g++)
    {
        uint32_t a = _cmsToFixedDomain(g);
        uint32_t b = (uint32_t)round(((double)g / 65535.0) * 65536.0);

        if (a != b)
            printf("%d != %d\n", a, b);
    }
}

static
void fromFixedDomain(void)
{
    int32_t g;

    for (g = 0; g <= 65536; g++)
    {
        uint32_t a = _cmsFromFixedDomain(g);
        uint32_t b = (uint32_t)round(((double)g / 65536.0) * 65535.0);

        if (a != b)
            printf("%d != %d\n", a, b);
    }
}

// Check alpha
int main()
{
    toFixedDomain();
    fromFixedDomain();

    dif8from();
    dif8to();

    dif16from();
    dif16to();

    
    return 0;
    

}