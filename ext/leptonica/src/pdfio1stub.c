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
 * \file pdfio1stub.c
 * <pre>
 *
 *     Stubs for pdfio1.c functions
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include "allheaders.h"

/* --------------------------------------------*/
#if  !USE_PDFIO   /* defined in environ.h */
/* --------------------------------------------*/

/* ----------------------------------------------------------------------*/

l_ok convertFilesToPdf(const char *dirname, const char *substr,
                       l_int32 res, l_float32 scalefactor,
                       l_int32 type, l_int32 quality,
                       const char *title, const char *fileout)
{
    return ERROR_INT("function not present", __func__, 1);
}

/* ----------------------------------------------------------------------*/

l_ok saConvertFilesToPdf(SARRAY *sa, l_int32 res, l_float32 scalefactor,
                         l_int32 type, l_int32 quality,
                         const char *title, const char *fileout)
{
    return ERROR_INT("function not present", __func__, 1);
}

/* ----------------------------------------------------------------------*/

l_ok saConvertFilesToPdfData(SARRAY *sa, l_int32 res,
                             l_float32 scalefactor, l_int32 type,
                             l_int32 quality, const char *title,
                             l_uint8 **pdata, size_t *pnbytes)
{
    return ERROR_INT("function not present", __func__, 1);
}

/* ----------------------------------------------------------------------*/

l_ok selectDefaultPdfEncoding(PIX *pix, l_int32 *ptype)
{
    return ERROR_INT("function not present", __func__, 1);
}

/* ----------------------------------------------------------------------*/

l_ok convertUnscaledFilesToPdf(const char *dirname, const char *substr,
                               const char *title, const char *fileout)
{
    return ERROR_INT("function not present", __func__, 1);
}

/* ----------------------------------------------------------------------*/

l_ok saConvertUnscaledFilesToPdf(SARRAY *sa, const char *title,
                                 const char *fileout)
{
    return ERROR_INT("function not present", __func__, 1);
}

/* ----------------------------------------------------------------------*/

l_ok saConvertUnscaledFilesToPdfData(SARRAY *sa, const char *title,
                                     l_uint8 **pdata, size_t *pnbytes)
{
    return ERROR_INT("function not present", __func__, 1);
}

/* ----------------------------------------------------------------------*/

l_ok convertUnscaledToPdfData(const char *fname, const char *title,
                              l_uint8 **pdata, size_t *pnbytes)
{
    return ERROR_INT("function not present", __func__, 1);
}

/* ----------------------------------------------------------------------*/

l_ok pixaConvertToPdf(PIXA *pixa, l_int32 res, l_float32 scalefactor,
                      l_int32 type, l_int32 quality,
                      const char *title, const char *fileout)
{
    return ERROR_INT("function not present", __func__, 1);
}

/* ----------------------------------------------------------------------*/

l_ok pixaConvertToPdfData(PIXA *pixa, l_int32 res, l_float32 scalefactor,
                          l_int32 type, l_int32 quality, const char *title,
                          l_uint8 **pdata, size_t *pnbytes)
{
    return ERROR_INT("function not present", __func__, 1);
}

/* ----------------------------------------------------------------------*/

l_ok convertToPdf(const char *filein,
                  l_int32 type, l_int32 quality,
                  const char *fileout,
                     l_int32 x, l_int32 y, l_int32 res,
                     const char *title,
                     L_PDF_DATA **plpd, l_int32 position)
{
    return ERROR_INT("function not present", __func__, 1);
}

/* ----------------------------------------------------------------------*/

l_ok convertImageDataToPdf(l_uint8 *imdata, size_t size,
                           l_int32 type, l_int32 quality,
                           const char *fileout,
                           l_int32 x, l_int32 y, l_int32 res,
                           const char *title,
                           L_PDF_DATA **plpd, l_int32 position)
{
    return ERROR_INT("function not present", __func__, 1);
}

/* ----------------------------------------------------------------------*/

l_ok convertToPdfData(const char *filein,
                      l_int32 type, l_int32 quality,
                      l_uint8 **pdata, size_t *pnbytes,
                      l_int32 x, l_int32 y, l_int32 res,
                      const char *title,
                      L_PDF_DATA **plpd, l_int32 position)
{
    return ERROR_INT("function not present", __func__, 1);
}

/* ----------------------------------------------------------------------*/

l_ok convertImageDataToPdfData(l_uint8 *imdata, size_t size,
                               l_int32 type, l_int32 quality,
                               l_uint8 **pdata, size_t *pnbytes,
                               l_int32 x, l_int32 y, l_int32 res,
                               const char *title,
                               L_PDF_DATA **plpd, l_int32 position)
{
    return ERROR_INT("function not present", __func__, 1);
}

/* ----------------------------------------------------------------------*/

l_ok pixConvertToPdf(PIX *pix, l_int32 type, l_int32 quality,
                     const char *fileout,
                     l_int32 x, l_int32 y, l_int32 res,
                     const char *title,
                     L_PDF_DATA **plpd, l_int32 position)
{
    return ERROR_INT("function not present", __func__, 1);
}

/* ----------------------------------------------------------------------*/

l_ok pixWriteStreamPdf(FILE *fp, PIX *pix, l_int32 res, const char *title)
{
    return ERROR_INT("function not present", __func__, 1);
}

/* ----------------------------------------------------------------------*/

l_ok pixWriteMemPdf(l_uint8 **pdata, size_t *pnbytes, PIX *pix,
                    l_int32 res, const char *title)
{
    return ERROR_INT("function not present", __func__, 1);
}

/* ----------------------------------------------------------------------*/

l_ok convertSegmentedFilesToPdf(const char *dirname, const char *substr,
                                l_int32 res, l_int32 type, l_int32 thresh,
                                BOXAA *baa, l_int32 quality,
                                l_float32 scalefactor, const char *title,
                                const char *fileout)
{
    return ERROR_INT("function not present", __func__, 1);
}

/* ----------------------------------------------------------------------*/

BOXAA * convertNumberedMasksToBoxaa(const char *dirname, const char *substr,
                                    l_int32 numpre, l_int32 numpost)
{
    return (BOXAA *)ERROR_PTR("function not present", __func__, NULL);
}

/* ----------------------------------------------------------------------*/

l_ok convertToPdfSegmented(const char *filein, l_int32 res, l_int32 type,
                           l_int32 thresh, BOXA *boxa, l_int32 quality,
                           l_float32 scalefactor, const char *title,
                           const char *fileout)
{
    return ERROR_INT("function not present", __func__, 1);
}

/* ----------------------------------------------------------------------*/

l_ok pixConvertToPdfSegmented(PIX *pixs, l_int32 res, l_int32 type,
                              l_int32 thresh, BOXA *boxa, l_int32 quality,
                              l_float32 scalefactor, const char *title,
                              const char *fileout)
{
    return ERROR_INT("function not present", __func__, 1);
}

/* ----------------------------------------------------------------------*/

l_ok convertToPdfDataSegmented(const char *filein, l_int32 res,
                               l_int32 type, l_int32 thresh, BOXA *boxa,
                               l_int32 quality, l_float32 scalefactor,
                               const char *title,
                               l_uint8 **pdata, size_t *pnbytes)
{
    return ERROR_INT("function not present", __func__, 1);
}

/* ----------------------------------------------------------------------*/

l_ok pixConvertToPdfDataSegmented(PIX *pixs, l_int32 res, l_int32 type,
                                  l_int32 thresh, BOXA *boxa,
                                  l_int32 quality, l_float32 scalefactor,
                                  const char *title,
                                  l_uint8 **pdata, size_t *pnbytes)
{
    return ERROR_INT("function not present", __func__, 1);
}

/* ----------------------------------------------------------------------*/

l_ok concatenatePdf(const char *dirname, const char *substr,
                    const char *fileout)
{
    return ERROR_INT("function not present", __func__, 1);
}

/* ----------------------------------------------------------------------*/

l_ok saConcatenatePdf(SARRAY *sa, const char *fileout)
{
    return ERROR_INT("function not present", __func__, 1);
}

/* ----------------------------------------------------------------------*/

l_ok ptraConcatenatePdf(L_PTRA *pa, const char *fileout)
{
    return ERROR_INT("function not present", __func__, 1);
}

/* ----------------------------------------------------------------------*/

l_ok concatenatePdfToData(const char *dirname, const char *substr,
                          l_uint8 **pdata, size_t *pnbytes)
{
    return ERROR_INT("function not present", __func__, 1);
}

/* ----------------------------------------------------------------------*/

l_ok saConcatenatePdfToData(SARRAY *sa, l_uint8 **pdata, size_t *pnbytes)
{
    return ERROR_INT("function not present", __func__, 1);
}

/* ----------------------------------------------------------------------*/

/* --------------------------------------------*/
#endif  /* !USE_PDFIO */
/* --------------------------------------------*/
