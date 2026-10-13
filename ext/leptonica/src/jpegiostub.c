/*====================================================================*
 -  Copyright (C) 2001 Leptonica.  All rights reserved.
 -
 -  Redistribution and use in source and binary forms, with or without
 -  modification, are permitted provided that the following conditions
 -  are met:
 -  1. Redistributions of source code must retain the above copyright
 -     notice, this list of conditions and the following disclaimer.
 -  2. Redistributions in binary form must reproduce the above
 -     copyright notice, this list of conditions and the following
 -     disclaimer in the documentation and/or other materials
 -     provided with the distribution.
 -
 -  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 -  ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 -  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 -  A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL ANY
 -  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 -  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 -  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 -  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 -  OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 -  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 -  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *====================================================================*/

/*!
 * \file jpegiostub.c
 * <pre>
 *
 *     Stubs for jpegio.c functions
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include "allheaders.h"

/* --------------------------------------------*/
#if  !HAVE_LIBJPEG   /* defined in environ.h */
/* --------------------------------------------*/

/* ----------------------------------------------------------------------*/

PIX * pixReadJpeg(const char *filename, l_int32 cmflag, l_int32 reduction,
                  l_int32 *pnwarn, l_int32 hint)
{
    return (PIX * )ERROR_PTR("function not present", __func__, NULL);
}

/* ----------------------------------------------------------------------*/

PIX * pixReadStreamJpeg(FILE *fp, l_int32 cmflag, l_int32 reduction,
                        l_int32 *pnwarn, l_int32 hint)
{
    return (PIX * )ERROR_PTR("function not present", __func__, NULL);
}

/* ----------------------------------------------------------------------*/

l_ok readHeaderJpeg(const char *filename, l_int32 *pw, l_int32 *ph,
                    l_int32 *pspp, l_int32 *pycck, l_int32 *pcmyk)
{
    return ERROR_INT("function not present", __func__, 1);
}

/* ----------------------------------------------------------------------*/

l_ok freadHeaderJpeg(FILE *fp, l_int32 *pw, l_int32 *ph,
                     l_int32 *pspp, l_int32 *pycck, l_int32 *pcmyk)
{
    return ERROR_INT("function not present", __func__, 1);
}

/* ----------------------------------------------------------------------*/

l_int32 fgetJpegResolution(FILE *fp, l_int32 *pxres, l_int32 *pyres)
{
    return ERROR_INT("function not present", __func__, 1);
}

/* ----------------------------------------------------------------------*/

l_int32 fgetJpegComment(FILE *fp, l_uint8 **pcomment)
{
    return ERROR_INT("function not present", __func__, 1);
}

/* ----------------------------------------------------------------------*/

l_ok pixWriteJpeg(const char *filename, PIX *pix, l_int32 quality,
                  l_int32 progressive)
{
    return ERROR_INT("function not present", __func__, 1);
}

/* ----------------------------------------------------------------------*/

l_ok pixWriteStreamJpeg(FILE *fp, PIX *pix, l_int32 quality,
                        l_int32 progressive)
{
    return ERROR_INT("function not present", __func__, 1);
}

/* ----------------------------------------------------------------------*/

PIX * pixReadMemJpeg(const l_uint8 *cdata, size_t size, l_int32 cmflag,
                     l_int32 reduction, l_int32 *pnwarn, l_int32 hint)
{
    return (PIX * )ERROR_PTR("function not present", __func__, NULL);
}

/* ----------------------------------------------------------------------*/

l_ok readHeaderMemJpeg(const l_uint8 *cdata, size_t size,
                       l_int32 *pw, l_int32 *ph, l_int32 *pspp,
                       l_int32 *pycck, l_int32 *pcmyk)
{
    return ERROR_INT("function not present", __func__, 1);
}

/* ----------------------------------------------------------------------*/

l_ok readResolutionMemJpeg(const l_uint8 *data, size_t size,
                           l_int32 *pxres, l_int32 *pyres)
{
    return ERROR_INT("function not present", __func__, 1);
}

/* ----------------------------------------------------------------------*/

l_ok pixWriteMemJpeg(l_uint8 **pdata, size_t *psize, PIX *pix,
                     l_int32 quality, l_int32 progressive)
{
    return ERROR_INT("function not present", __func__, 1);
}

/* ----------------------------------------------------------------------*/

l_ok pixSetChromaSampling(PIX *pix, l_int32 sampling)
{
    return ERROR_INT("function not present", __func__, 1);
}

/* ----------------------------------------------------------------------*/

/* --------------------------------------------*/
#endif  /* !HAVE_LIBJPEG */
/* --------------------------------------------*/
