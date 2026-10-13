/*====================================================================*
 -  Copyright (C) 2001-2016 Leptonica.  All rights reserved.
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

/*
 * writefile.c
 *
 *     Set jpeg quality for pixWrite() and pixWriteMem()
 *        l_int32     l_jpegSetQuality()
 *
 *     Set global variable LeptDebugOK for writing to named temp files
 *        l_int32     setLeptDebugOK()
 *
 *     High-level procedures for writing images to file:
 *        l_int32     pixaWriteFiles()
 *        l_int32     pixWriteDebug()
 *        l_int32     pixWrite()
 *        l_int32     pixWriteAutoFormat()
 *        l_int32     pixWriteStream()
 *        l_int32     pixWriteImpliedFormat()
 *
 *     Selection of output format if default is requested
 *        l_int32     pixChooseOutputFormat()
 *        l_int32     getImpliedFileFormat()
 *        l_int32     getFormatFromExtension()
 *        l_int32     pixGetAutoFormat()
 *        const char *getFormatExtension()
 *
 *     Write to memory
 *        l_int32     pixWriteMem()
 *
 *     Image display for debugging
 *        l_int32     l_fileDisplay()
 *        l_int32     pixDisplay()
 *        l_int32     pixDisplayWithTitle()
 *        PIX        *pixMakeColorSquare()
 *        void        l_chooseDisplayProg()
 *
 *     Change format for missing library
 *        void        changeFormatForMissingLib()
 *
 *     Nonfunctional stub of pix output for debugging
 *        l_int32     pixDisplayWrite()
 *
 *  Supported file formats:
 *  (1) Writing is supported without any external libraries:
 *          bmp
 *          pnm   (including pbm, pgm, etc)
 *          spix  (raw serialized)
 *  (2) Writing is supported with installation of external libraries:
 *          png
 *          jpg   (standard jfif version)
 *          tiff  (including most varieties of compression)
 *          gif
 *          webp
 *          jp2 (jpeg2000)
 *  (3) Writing is supported through special interfaces:
 *          ps (PostScript, in psio1.c, psio2.c):
 *              level 1 (uncompressed)
 *              level 2 (g4 and dct encoding: requires tiff, jpg)
 *              level 3 (g4, dct and flate encoding: requires tiff, jpg, zlib)
 *          pdf (PDF, in pdfio.c):
 *              level 1 (g4 and dct encoding: requires tiff, jpg)
 *              level 2 (g4, dct and flate encoding: requires tiff, jpg, zlib)
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include <string.h>
#include "allheaders.h"

    /* Set defaults for the display program (xv, xli, xzgv, open, irfanview)
     * that is invoked by pixDisplay()  */
#ifdef _WIN32
static l_int32  var_DISPLAY_PROG = L_DISPLAY_WITH_IV;  /* default */
#elif  defined(__APPLE__)
static l_int32  var_DISPLAY_PROG = L_DISPLAY_WITH_OPEN;  /* default */
#else
static l_int32  var_DISPLAY_PROG = L_DISPLAY_WITH_XZGV;  /* default */
#endif  /* _WIN32 */

#define Bufsize 512
static const l_int32  MaxDisplayWidth = 1000;
static const l_int32  MaxDisplayHeight = 800;
static const l_int32  MaxSizeForPng = 200;

    /* PostScript output for printing */
static const l_float32  DefaultScaling = 1.0;

    /* Global array of image file format extension names.                */
    /* This is in 1-1 correspondence with format enum in imageio.h.      */
    /* The empty string at the end represents the serialized format,     */
    /* which has no recognizable extension name, but the array must      */
    /* be padded to agree with the format enum.                          */
    /* (Note on 'const': The size of the array can't be defined 'const'  */
    /* because that makes it static.  The 'const' in the definition of   */
    /* the array refers to the strings in the array; the ptr to the      */
    /* array is not const and can be used 'extern' in other files.)      */
LEPT_DLL l_int32  NumImageFileFormatExtensions = 20;  /* array size */
LEPT_DLL const char *ImageFileFormatExtensions[] =
         {"unknown",
          "bmp",
          "jpg",
          "png",
          "tif",
          "tif",
          "tif",
          "tif",
          "tif",
          "tif",
          "tif",
          "pnm",
          "ps",
          "gif",
          "jp2",
          "webp",
          "pdf",
          "tif",
          "default",
          ""};

    /* Local map of image file name extension to output format.
     * Note that the extension string always includes a '.'  */
struct ExtensionMap
{
    char     extension[16];
    l_int32  format;
};
static const struct ExtensionMap extension_map[] =
                            { { ".bmp",      IFF_BMP       },
                              { ".jpg",      IFF_JFIF_JPEG },
                              { ".jpeg",     IFF_JFIF_JPEG },
                              { ".JPG",      IFF_JFIF_JPEG },
                              { ".png",      IFF_PNG       },
                              { ".tif",      IFF_TIFF      },
                              { ".tiff",     IFF_TIFF      },
                              { ".tiffg4",   IFF_TIFF_G4   },
                              { ".pbm",      IFF_PNM       },
                              { ".pgm",      IFF_PNM       },
                              { ".pnm",      IFF_PNM       },
                              { ".gif",      IFF_GIF       },
                              { ".jp2",      IFF_JP2       },
                              { ".j2k",      IFF_JP2       },
                              { ".ps",       IFF_PS        },
                              { ".pdf",      IFF_LPDF      },
                              { ".webp",     IFF_WEBP      } };


/*---------------------------------------------------------------------*
 *           Set jpeg quality for pixWrite() and pixWriteMem()         *
 *---------------------------------------------------------------------*/
    /* Parameter that controls jpeg quality for high-level calls. */
static l_int32  var_JPEG_QUALITY = 75;   /* default */

/*!
 * \brief   l_jpegSetQuality()
 *
 * \param[in]    new_quality    1 - 100; 75 is default; 0 defaults to 75
 * \return       prev           previous quality
 *
 * <pre>
 * Notes:
 *      (1) This variable is used in pixWriteStream() and pixWriteMem(),
 *          to control the jpeg quality.  The default is 75.
 *      (2) It returns the previous quality, so for example:
 *           l_int32  prev = l_jpegSetQuality(85);  //sets to 85
 *           pixWriteStream(...);
 *           l_jpegSetQuality(prev);   // resets to previous value
 *      (3) On error, logs a message and does not change the variable.
 */
l_int32
l_jpegSetQuality(l_int32  new_quality)
{
l_int32  prevq, newq;

    prevq = var_JPEG_QUALITY;
    newq = (new_quality == 0) ? 75 : new_quality;
    if (newq < 1 || newq > 100)
        L_ERROR("invalid jpeg quality; unchanged\n", __func__);
    else
        var_JPEG_QUALITY = newq;
    return prevq;
}


/*----------------------------------------------------------------------*
 *    Set global variable LeptDebugOK for writing to named temp files   *
 *----------------------------------------------------------------------*/
LEPT_DLL l_int32 LeptDebugOK = 0;  /* default value */
/*!
 * \brief   setLeptDebugOK()
 *
 * \param[in]    allow     TRUE (1) or FALSE (0)
 * \return       void
 *
 * <pre>
 * Notes:
 *      (1) This sets or clears the global variable LeptDebugOK, to
 *          control writing files in a temp directory with names that
 *          are compiled in.
 *      (2) The default in the library distribution is 0.  Call with
 *          %allow = 1 for development and debugging.
 */
void
setLeptDebugOK(l_int32  allow)
{
    if (allow != 0) allow = 1;
    LeptDebugOK = allow;
}


/*---------------------------------------------------------------------*
 *           Top-level procedures for writing images to file           *
 *---------------------------------------------------------------------*/
/*!
 * \brief   pixaWriteFiles()
 *
 * \param[in]    rootname
 * \param[in]    pixa
 * \param[in]    format  defined in imageio.h; see notes for default
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Use %format = IFF_DEFAULT to decide the output format
 *          individually for each pix.
 * </pre>
 */
l_ok
pixaWriteFiles(const char  *rootname,
               PIXA        *pixa,
               l_int32      format)
{
char     bigbuf[Bufsize];
l_int32  i, n, pixformat;
PIX     *pix;

    if (!rootname)
        return ERROR_INT("rootname not defined", __func__, 1);
    if (!pixa)
        return ERROR_INT("pixa not defined", __func__, 1);
    if (format < 0 || format == IFF_UNKNOWN ||
        format >= NumImageFileFormatExtensions)
        return ERROR_INT("invalid format", __func__, 1);

    n = pixaGetCount(pixa);
    for (i = 0; i < n; i++) {
        pix = pixaGetPix(pixa, i, L_CLONE);
        if (format == IFF_DEFAULT)
            pixformat = pixChooseOutputFormat(pix);
        else
            pixformat = format;
        snprintf(bigbuf, Bufsize, "%s%03d.%s", rootname, i,
                 ImageFileFormatExtensions[pixformat]);
        pixWrite(bigbuf, pix, pixformat);
        pixDestroy(&pix);
    }

    return 0;
}


/*!
 * \brief   pixWriteDebug()
 *
 * \param[in]    fname
 * \param[in]    pix
 * \param[in]    format  defined in imageio.h
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Debug version, intended for use in the library when writing
 *          to files in a temp directory with names that are compiled in.
 *          This is used instead of pixWrite() for all such library calls.
 *      (2) The global variable LeptDebugOK defaults to 0, and can be set
 *          or cleared by the function setLeptDebugOK().
 * </pre>
 */
l_ok
pixWriteDebug(const char  *fname,
              PIX         *pix,
              l_int32      format)
{
    if (LeptDebugOK) {
        return pixWrite(fname, pix, format);
    } else {
        L_INFO("write to named temp file %s is disabled\n", __func__, fname);
        return 0;
    }
}


/*!
 * \brief   pixWrite()
 *
 * \param[in]    fname
 * \param[in]    pix
 * \param[in]    format  defined in imageio.h
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) Open for write using binary mode (with the "b" flag)
 *          to avoid having Windows automatically translate the NL
 *          into CRLF, which corrupts image files.  On non-Windows
 *          systems this flag should be ignored, per ISO C90.
 *          Thanks to Dave Bryan for pointing this out.
 *      (2) If the default image format IFF_DEFAULT is requested:
 *          use the input format if known; otherwise, use a lossless format.
 *      (3) The default jpeg quality is 75.  For some other value,
 *          Use l_jpegSetQuality().
 * </pre>
 */
l_ok
pixWrite(const char  *fname,
         PIX         *pix,
         l_int32      format)
{
l_int32  ret;
FILE    *fp;

    if (!pix)
        return ERROR_INT("pix not defined", __func__, 1);
    if (!fname)
        return ERROR_INT("fname not defined", __func__, 1);

    if ((fp = fopenWriteStream(fname, "wb+")) == NULL)
        return ERROR_INT_1("stream not opened", fname, __func__, 1);

    ret = pixWriteStream(fp, pix, format);
    fclose(fp);
    if (ret)
        return ERROR_INT_1("pix not written to stream", fname, __func__, 1);
    return 0;
}


/*!
 * \brief   pixWriteAutoFormat()
 *
 * \param[in]    filename
 * \param[in]    pix
 * \return  0 if OK; 1 on error
 */
l_ok
pixWriteAutoFormat(const char  *filename,
                   PIX         *pix)
{
l_int32  format;

    if (!pix)
        return ERROR_INT("pix not defined", __func__, 1);
    if (!filename)
        return ERROR_INT("filename not defined", __func__, 1);

    if (pixGetAutoFormat(pix, &format))
        return ERROR_INT("auto format not returned", __func__, 1);
    return pixWrite(filename, pix, format);
}


/*!
 * \brief   pixWriteStream()
 *
 * \param[in]    fp file stream
 * \param[in]    pix
 * \param[in]    format
 * \return  0 if OK; 1 on error.
 */
l_ok
pixWriteStream(FILE    *fp,
               PIX     *pix,
               l_int32  format)
{
    if (!fp)
        return ERROR_INT("stream not defined", __func__, 1);
    if (!pix)
        return ERROR_INT("pix not defined", __func__, 1);

    if (format == IFF_DEFAULT)
        format = pixChooseOutputFormat(pix);

        /* Use bmp format for testing if library for requested
         * format for jpeg, png or tiff is not available */
    changeFormatForMissingLib(&format);

    switch(format)
    {
    case IFF_BMP:
        pixWriteStreamBmp(fp, pix);
        break;

    case IFF_JFIF_JPEG:   /* default quality; baseline sequential */
        return pixWriteStreamJpeg(fp, pix, var_JPEG_QUALITY, 0);

    case IFF_PNG:   /* no gamma value stored */
        return pixWriteStreamPng(fp, pix, 0.0);

    case IFF_TIFF:           /* uncompressed */
    case IFF_TIFF_PACKBITS:  /* compressed, binary only */
    case IFF_TIFF_RLE:       /* compressed, binary only */
    case IFF_TIFF_G3:        /* compressed, binary only */
    case IFF_TIFF_G4:        /* compressed, binary only */
    case IFF_TIFF_LZW:       /* compressed, all depths */
    case IFF_TIFF_ZIP:       /* compressed, all depths */
    case IFF_TIFF_JPEG:      /* compressed, 8 bpp gray and 32 bpp rgb */
        return pixWriteStreamTiff(fp, pix, format);

    case IFF_PNM:
        return pixWriteStreamPnm(fp, pix);

    case IFF_PS:
        return pixWriteStreamPS(fp, pix, NULL, 0, DefaultScaling);

    case IFF_GIF:
        return pixWriteStreamGif(fp, pix);

    case IFF_JP2:
        return pixWriteStreamJp2k(fp, pix, 34, 0, L_JP2_CODEC, 0, 0);

    case IFF_WEBP:
        return pixWriteStreamWebP(fp, pix, 80, 0);

    case IFF_LPDF:
        return pixWriteStreamPdf(fp, pix, 0, NULL);

    case IFF_SPIX:
        return pixWriteStreamSpix(fp, pix);

    default:
        return ERROR_INT("unknown format", __func__, 1);
    }

    return 0;
}


/*!
 * \brief   pixWriteImpliedFormat()
 *
 * \param[in]    filename
 * \param[in]    pix
 * \param[in]    quality iff JPEG; 1 - 100, 0 for default
 * \param[in]    progressive iff JPEG; 0 for baseline seq., 1 for progressive
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This determines the output format from the filename extension.
 *      (2) The last two args are ignored except for requests for jpeg files.
 *      (3) The jpeg default quality is 75.
 * </pre>
 */
l_ok
pixWriteImpliedFormat(const char  *filename,
                      PIX         *pix,
                      l_int32      quality,
                      l_int32      progressive)
{
l_int32  format;

    if (!filename)
        return ERROR_INT("filename not defined", __func__, 1);
    if (!pix)
        return ERROR_INT("pix not defined", __func__, 1);

        /* Determine output format */
    format = getImpliedFileFormat(filename);
    if (format == IFF_UNKNOWN) {
        format = IFF_PNG;
    } else if (format == IFF_TIFF) {
        if (pixGetDepth(pix) == 1)
            format = IFF_TIFF_G4;
        else
#ifdef _WIN32
            format = IFF_TIFF_LZW;  /* poor compression */
#else
            format = IFF_TIFF_ZIP;  /* native Windows tools can't handle this */
#endif  /* _WIN32 */
    }

    if (format == IFF_JFIF_JPEG) {
        quality = L_MIN(quality, 100);
        quality = L_MAX(quality, 0);
        if (progressive != 0 && progressive != 1) {
            progressive = 0;
            L_WARNING("invalid progressive; setting to baseline\n", __func__);
        }
        if (quality == 0)
            quality = 75;
        pixWriteJpeg (filename, pix, quality, progressive);
    } else {
        pixWrite(filename, pix, format);
    }

    return 0;
}


/*---------------------------------------------------------------------*
 *          Selection of output format if default is requested         *
 *---------------------------------------------------------------------*/
/*!
 * \brief   pixChooseOutputFormat()
 *
 * \param[in]    pix
 * \return  output format, or 0 on error
 *
 * <pre>
 * Notes:
 *      (1) This should only be called if the requested format is IFF_DEFAULT.
 *      (2) If the pix wasn't read from a file, its input format value
 *          will be IFF_UNKNOWN, and in that case it is written out
 *          in a compressed but lossless format.
 * </pre>
 */
l_int32
pixChooseOutputFormat(PIX  *pix)
{
l_int32  d, format;

    if (!pix)
        return ERROR_INT("pix not defined", __func__, 0);

    d = pixGetDepth(pix);
    format = pixGetInputFormat(pix);
    if (format == IFF_UNKNOWN) {  /* output lossless */
        if (d == 1)
            format = IFF_TIFF_G4;
        else
            format = IFF_PNG;
    }

    return format;
}


/*!
 * \brief   getImpliedFileFormat()
 *
 * \param[in]    filename
 * \return  output format, or IFF_UNKNOWN on error or invalid extension.
 *
 * <pre>
 * Notes:
 *      (1) This determines the output file format from the extension
 *          of the input filename.
 * </pre>
 */
l_int32
getImpliedFileFormat(const char  *filename)
{
char    *extension;
l_int32  format = IFF_UNKNOWN;

    if (!filename)
        return ERROR_INT("extension not defined", __func__, IFF_UNKNOWN);

    if (splitPathAtExtension (filename, NULL, &extension))
        return IFF_UNKNOWN;

    format = getFormatFromExtension(extension);
    LEPT_FREE(extension);
    return format;
}


/*!
 * \brief   getFormatFromExtension()
 *
 * \param[in]    extension
 * \return  output format, or IFF_UNKNOWN on error or invalid extension.
 *
 * <pre>
 * Notes:
 *      (1) This determines the integer for writing in a format that
 *          corresponds to the image file type extension.  For example,
 *          the integer code corresponding to the extension "jpg" is 2;
 *          it is used to write with jpeg encoding.
 * </pre>
 */
l_int32
getFormatFromExtension(const char  *extension)
{
int      i, numext;
l_int32  format = IFF_UNKNOWN;

    if (!extension)
        return ERROR_INT("extension not defined", __func__, IFF_UNKNOWN);

    numext = sizeof(extension_map) / sizeof(extension_map[0]);
    for (i = 0; i < numext; i++) {
        if (!strcmp(extension, extension_map[i].extension)) {
            format = extension_map[i].format;
            break;
        }
    }
    return format;
}


/*!
 * \brief   pixGetAutoFormat()
 *
 * \param[in]    pix
 * \param[in]    &format
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) The output formats are restricted to tiff, jpeg and png
 *          because these are the most commonly used image formats and
 *          the ones that are typically installed with leptonica.
 *      (2) This decides what compression to use based on the pix.
 *          It chooses tiff-g4 if 1 bpp without a colormap, jpeg with
 *          quality 75 if grayscale, rgb or rgba (where it loses
 *          the alpha layer), and lossless png for all other situations.
 * </pre>
 */
l_ok
pixGetAutoFormat(PIX      *pix,
                 l_int32  *pformat)
{
l_int32   d;
PIXCMAP  *cmap;

    if (!pformat)
        return ERROR_INT("&format not defined", __func__, 0);
    *pformat = IFF_UNKNOWN;
    if (!pix)
        return ERROR_INT("pix not defined", __func__, 0);

    d = pixGetDepth(pix);
    cmap = pixGetColormap(pix);
    if (d == 1 && !cmap) {
        *pformat = IFF_TIFF_G4;
    } else if ((d == 8 && !cmap) || d == 24 || d == 32) {
        *pformat = IFF_JFIF_JPEG;
    } else {
        *pformat = IFF_PNG;
    }

    return 0;
}


/*!
 * \brief   getFormatExtension()
 *
 * \param[in]    format integer
 * \return  extension string, or NULL if format is out of range
 *
 * <pre>
 * Notes:
 *      (1) This string is NOT owned by the caller; it is just a pointer
 *          to a global string.  Do not free it.
 * </pre>
 */
const char *
getFormatExtension(l_int32  format)
{
    if (format < 0 || format >= NumImageFileFormatExtensions)
        return (const char *)ERROR_PTR("invalid format", __func__, NULL);

    return ImageFileFormatExtensions[format];
}


/*---------------------------------------------------------------------*
 *                            Write to memory                          *
 *---------------------------------------------------------------------*/
/*!
 * \brief   pixWriteMem()
 *
 * \param[out]   pdata data of tiff compressed image
 * \param[out]   psize size of returned data
 * \param[in]    pix
 * \param[in]    format  defined in imageio.h
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) On Windows, this will only write tiff and PostScript to memory.
 *          For other formats, it requires open_memstream(3).
 *      (2) PostScript output is uncompressed, in hex ascii.
 *          Most printers support level 2 compression (tiff_g4 for 1 bpp,
 *          jpeg for 8 and 32 bpp).
 *      (3) The default jpeg quality is 75.  For some other value,
 *          Use l_jpegSetQuality().
 * </pre>
 */
l_ok
pixWriteMem(l_uint8  **pdata,
            size_t    *psize,
            PIX       *pix,
            l_int32    format)
{
l_int32  ret;

    if (!pdata)
        return ERROR_INT("&data not defined", __func__, 1 );
    if (!psize)
        return ERROR_INT("&size not defined", __func__, 1 );
    if (!pix)
        return ERROR_INT("&pix not defined", __func__, 1 );

    if (format == IFF_DEFAULT)
        format = pixChooseOutputFormat(pix);

        /* Use bmp format for testing if library for requested
         * format for jpeg, png or tiff is not available */
    changeFormatForMissingLib(&format);

    switch(format)
    {
    case IFF_BMP:
        ret = pixWriteMemBmp(pdata, psize, pix);
        break;

    case IFF_JFIF_JPEG:   /* default quality; baseline sequential */
        ret = pixWriteMemJpeg(pdata, psize, pix, var_JPEG_QUALITY, 0);
        break;

    case IFF_PNG:   /* no gamma value stored */
        ret = pixWriteMemPng(pdata, psize, pix, 0.0);
        break;

    case IFF_TIFF:           /* uncompressed */
    case IFF_TIFF_PACKBITS:  /* compressed, binary only */
    case IFF_TIFF_RLE:       /* compressed, binary only */
    case IFF_TIFF_G3:        /* compressed, binary only */
    case IFF_TIFF_G4:        /* compressed, binary only */
    case IFF_TIFF_LZW:       /* compressed, all depths */
    case IFF_TIFF_ZIP:       /* compressed, all depths */
    case IFF_TIFF_JPEG:      /* compressed, 8 bpp gray or 32 bpp rgb */
        ret = pixWriteMemTiff(pdata, psize, pix, format);
        break;

    case IFF_PNM:
        ret = pixWriteMemPnm(pdata, psize, pix);
        break;

    case IFF_PS:
        ret = pixWriteMemPS(pdata, psize, pix, NULL, 0, DefaultScaling);
        break;

    case IFF_GIF:
        ret = pixWriteMemGif(pdata, psize, pix);
        break;

    case IFF_JP2:
        ret = pixWriteMemJp2k(pdata, psize, pix, 34, 0, 0, 0);
        break;

    case IFF_WEBP:
        ret = pixWriteMemWebP(pdata, psize, pix, 80, 0);
        break;

    case IFF_LPDF:
        ret = pixWriteMemPdf(pdata, psize, pix, 0, NULL);
        break;

    case IFF_SPIX:
        ret = pixWriteMemSpix(pdata, psize, pix);
        break;

    default:
        return ERROR_INT("unknown format", __func__, 1);
    }

    return ret;
}


/*---------------------------------------------------------------------*
 *                      Image display for debugging                    *
 *---------------------------------------------------------------------*/
/*!
 * \brief   l_fileDisplay()
 *
 * \param[in]    fname
 * \param[in]    x, y  location of display frame on the screen
 * \param[in]    scale  scale factor (use 0 to skip display)
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This is a convenient wrapper for displaying image files.
 *      (2) It does nothing unless LeptDebugOK == TRUE.
 *      (2) Set %scale = 0 to disable display.
 *      (3) This downscales 1 bpp to gray.
 * </pre>
 */
l_ok
l_fileDisplay(const char  *fname,
              l_int32      x,
              l_int32      y,
              l_float32    scale)
{
PIX  *pixs, *pixd;

    if (!LeptDebugOK) {
        L_INFO("displaying files is disabled; "
               "use setLeptDebugOK(1) to enable\n", __func__);
        return 0;
    }
    if (scale == 0.0)
        return 0;
    if (scale < 0.0)
        return ERROR_INT("invalid scale factor", __func__, 1);
    if ((pixs = pixRead(fname)) == NULL)
        return ERROR_INT("pixs not read", __func__, 1);

    if (scale == 1.0) {
        pixd = pixClone(pixs);
    } else {
        if (scale < 1.0 && pixGetDepth(pixs) == 1)
            pixd = pixScaleToGray(pixs, scale);
        else
            pixd = pixScale(pixs, scale, scale);
    }
    pixDisplay(pixd, x, y);
    pixDestroy(&pixs);
    pixDestroy(&pixd);
    return 0;
}


/*!
 * \brief   pixDisplay()
 *
 * \param[in]    pix 1, 2, 4, 8, 16, 32 bpp
 * \param[in]    x, y  location of display frame on the screen
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) This is debugging code that displays an image on the screen.
 *          It uses a static internal variable to number the output files
 *          written by a single process.  Behavior with a shared library
 *          may be unpredictable.
 *      (2) It does nothing unless LeptDebugOK == TRUE.
 *      (3) It uses these programs to display the image:
 *             On Unix: xzgv, xli, xv or (for apple) open
 *             On Windows: i_view or the application currently registered
 *                         as the default viewer (the browser 'open' action
 *                         based on the extension of the image file).
 *          The display program must be on your $PATH variable.  It is
 *          chosen by setting the global var_DISPLAY_PROG, using
 *          l_chooseDisplayProg().  Default on Unix is xzgv.
 *      (4) Images with dimensions larger than MaxDisplayWidth or
 *          MaxDisplayHeight are downscaled to fit those constraints.
 *          This is particularly important for displaying 1 bpp images
 *          with xv, because xv automatically downscales large images
 *          by subsampling, which looks poor.  For 1 bpp, we use
 *          scale-to-gray to get decent-looking anti-aliased images.
 *          In all cases, we write a temporary file to /tmp/lept/disp,
 *          that is read by the display program.
 *      (5) The temporary file is written as png if, after initial
 *          processing for special cases, any of these obtain:
 *            * pix dimensions are smaller than some thresholds
 *            * pix depth is less than 8 bpp
 *            * pix is colormapped
 *      (6) For spp == 4, we call pixDisplayLayersRGBA() to show 3
 *          versions of the image: the image with a fully opaque
 *          alpha, the alpha, and the image as it would appear with
 *          a white background.
 *      (7) pixDisplay() can be inactivated at runtime by calling:
 *               l_chooseDisplayProg(L_DISPLAY_WITH_NONE);
 * </pre>
 */
l_ok
pixDisplay(PIX     *pixs,
           l_int32  x,
           l_int32  y)
{
    return pixDisplayWithTitle(pixs, x, y, NULL, 1);
}


/*!
 * \brief   pixDisplayWithTitle()
 *
 * \param[in]    pix 1, 2, 4, 8, 16, 32 bpp
 * \param[in]    x, y  location of display frame
 * \param[in]    title [optional] on frame; can be NULL;
 * \param[in]    dispflag 1 to write, else disabled
 * \return  0 if OK; 1 on error
 *
 * <pre>
 * Notes:
 *      (1) See notes for pixDisplay().
 *      (2) This displays the image if dispflag == 1 and the global
 *          var_DISPLAY_PROG != L_DISPLAY_WITH_NONE; otherwise it punts.
 * </pre>
 */
l_ok
pixDisplayWithTitle(PIX         *pixs,
                    l_int32      x,
                    l_int32      y,
                    const char  *title,
                    l_int32      dispflag)
{
char           *tempname;
char            buffer[Bufsize];
static l_atomic index = 0;  /* caution: not .so safe */
l_int32         w, h, d, spp, maxheight, opaque, threeviews;
l_float32       ratw, rath, ratmin;
PIX            *pix0, *pix1, *pix2;
PIXCMAP        *cmap;
#ifndef _WIN32
l_int32         wt, ht;
#else
char           *pathname;
char            fullpath[_MAX_PATH];
#endif  /* _WIN32 */

    if (!LeptDebugOK) {
        L_INFO("displaying images is disabled;\n      "
               "use setLeptDebugOK(1) to enable\n", __func__);
        return 0;
    }

#ifdef OS_IOS /* iOS 11 does not support system() */
    return ERROR_INT("iOS 11 does not support system()", __func__, 1);
#endif /* OS_IOS */

    if (dispflag != 1 || var_DISPLAY_PROG == L_DISPLAY_WITH_NONE)
        return 0;
    if (!pixs)
        return ERROR_INT("pixs not defined", __func__, 1);

#ifndef _WIN32  /* unix */
    if (var_DISPLAY_PROG != L_DISPLAY_WITH_XZGV &&
        var_DISPLAY_PROG != L_DISPLAY_WITH_XLI &&
        var_DISPLAY_PROG != L_DISPLAY_WITH_XV &&
        var_DISPLAY_PROG != L_DISPLAY_WITH_OPEN)
        return ERROR_INT("invalid unix program chosen for display",
                         __func__, 1);
#else  /* _WIN32 */
    if (var_DISPLAY_PROG != L_DISPLAY_WITH_IV &&
        var_DISPLAY_PROG != L_DISPLAY_WITH_OPEN)
        return ERROR_INT("invalid windows program chosen for display",
                         __func__, 1);
#endif  /* _WIN32 */

        /* Display with three views if either spp = 4 or if colormapped
         * and the alpha component is not fully opaque */
    opaque = TRUE;
    if ((cmap = pixGetColormap(pixs)) != NULL)
        pixcmapIsOpaque(cmap, &opaque);
    spp = pixGetSpp(pixs);
    threeviews = (spp == 4 || !opaque) ? TRUE : FALSE;

        /* If colormapped and not opaque, remove the colormap to RGBA */
    if (!opaque)
        pix0 = pixRemoveColormap(pixs, REMOVE_CMAP_WITH_ALPHA);
    else
        pix0 = pixClone(pixs);

        /* Scale if necessary; this will also remove a colormap */
    pixGetDimensions(pix0, &w, &h, &d);
    maxheight = (threeviews) ? MaxDisplayHeight / 3 : MaxDisplayHeight;
    if (w <= MaxDisplayWidth && h <= maxheight) {
        if (d == 16)  /* take MSB */
            pix1 = pixConvert16To8(pix0, L_MS_BYTE);
        else
            pix1 = pixClone(pix0);
    } else {
        ratw = (l_float32)MaxDisplayWidth / (l_float32)w;
        rath = (l_float32)maxheight / (l_float32)h;
        ratmin = L_MIN(ratw, rath);
        if (ratmin < 0.125 && d == 1)
            pix1 = pixScaleToGray8(pix0);
        else if (ratmin < 0.25 && d == 1)
            pix1 = pixScaleToGray4(pix0);
        else if (ratmin < 0.33 && d == 1)
            pix1 = pixScaleToGray3(pix0);
        else if (ratmin < 0.5 && d == 1)
            pix1 = pixScaleToGray2(pix0);
        else
            pix1 = pixScale(pix0, ratmin, ratmin);
    }
    pixDestroy(&pix0);
    if (!pix1)
        return ERROR_INT("pix1 not made", __func__, 1);

        /* Generate the three views if required */
    if (threeviews)
        pix2 = pixDisplayLayersRGBA(pix1, 0xffffff00, 0);
    else
        pix2 = pixClone(pix1);

    if (index == 0) {  /* erase any existing images */
        lept_rmdir("lept/disp");
        lept_mkdir("lept/disp");
    }

    index++;
    if (pixGetDepth(pix2) < 8 || pixGetColormap(pix2) ||
        (w < MaxSizeForPng && h < MaxSizeForPng)) {
        snprintf(buffer, Bufsize, "/tmp/lept/disp/write.%03d.png", index);
        pixWrite(buffer, pix2, IFF_PNG);
    } else {
        snprintf(buffer, Bufsize, "/tmp/lept/disp/write.%03d.jpg", index);
        pixWrite(buffer, pix2, IFF_JFIF_JPEG);
    }
    tempname = genPathname(buffer, NULL);

#ifndef _WIN32

        /* Unix */
    if (var_DISPLAY_PROG == L_DISPLAY_WITH_XZGV) {
            /* no way to display title */
        pixGetDimensions(pix2, &wt, &ht, NULL);
        snprintf(buffer, Bufsize,
                 "xzgv --geometry %dx%d+%d+%d %s &", wt + 10, ht + 10,
                 x, y, tempname);
    } else if (var_DISPLAY_PROG == L_DISPLAY_WITH_XLI) {
        if (title) {
            snprintf(buffer, Bufsize,
               "xli -dispgamma 1.0 -quiet -geometry +%d+%d -title \"%s\" %s &",
               x, y, title, tempname);
        } else {
            snprintf(buffer, Bufsize,
               "xli -dispgamma 1.0 -quiet -geometry +%d+%d %s &",
               x, y, tempname);
        }
    } else if (var_DISPLAY_PROG == L_DISPLAY_WITH_XV) {
        if (title) {
            snprintf(buffer, Bufsize,
                     "xv -quit -geometry +%d+%d -name \"%s\" %s &",
                     x, y, title, tempname);
        } else {
            snprintf(buffer, Bufsize,
                     "xv -quit -geometry +%d+%d %s &", x, y, tempname);
        }
    } else {  /* L_DISPLAY_WITH_OPEN */
        snprintf(buffer, Bufsize, "open %s &", tempname);
    }
    callSystemDebug(buffer);

#else  /* _WIN32 */

        /* Windows: L_DISPLAY_WITH_IV || L_DISPLAY_WITH_OPEN */
    pathname = genPathname(tempname, NULL);
    _fullpath(fullpath, pathname, sizeof(fullpath));
    if (var_DISPLAY_PROG == L_DISPLAY_WITH_IV) {
        if (title) {
            snprintf(buffer, Bufsize,
                     "i_view32.exe \"%s\" /pos=(%d,%d) /title=\"%s\"",
                     fullpath, x, y, title);
        } else {
            snprintf(buffer, Bufsize, "i_view32.exe \"%s\" /pos=(%d,%d)",
                     fullpath, x, y);
        }
    } else {  /* L_DISPLAY_WITH_OPEN */
        snprintf(buffer, Bufsize, "explorer.exe /open,\"%s\"", fullpath);
    }
    callSystemDebug(buffer);
    LEPT_FREE(pathname);

#endif  /* _WIN32 */

    pixDestroy(&pix1);
    pixDestroy(&pix2);
    LEPT_FREE(tempname);
    return 0;
}


/*!
 * \brief   pixMakeColorSquare()
 *
 * \param[in]    color      in 0xrrggbb00 format
 * \param[in]    size       in pixels; >= 100; use 0 for default (min size)
 * \param[in]    addlabel   use 1 to display the color component values
 * \param[in]    location   of text: L_ADD_ABOVE, etc; ignored if %addlabel == 0
 * \param[in]    textcolor  of text label; in 0xrrggbb00 format
 * \return  32 bpp rgb pixd if OK; NULL on error
 *
 * <pre>
 * Notes:
 *      (1) If %addlabel == 0, %location and %textcolor are ignored.
 *      (2) To make an array of color squares, use pixDisplayColorArray().
 * </pre>
 */
PIX *
pixMakeColorSquare(l_uint32  color,
                   l_int32   size,
                   l_int32   addlabel,
                   l_int32   location,
                   l_uint32  textcolor)
{
char     buf[32];
l_int32  w, rval, gval, bval;
L_BMF   *bmf;
PIX     *pix1, *pix2;

    w = (size <= 0) ? 100 : size;
    if (addlabel && w < 100) {
        L_WARNING("size too small for label; omitting label\n", __func__);
        addlabel = 0;
    }

    if ((pix1 = pixCreate(w, w, 32)) == NULL)
        return (PIX *)ERROR_PTR("pix1 not madel", __func__, NULL);
    pixSetAllArbitrary(pix1, color);
    if (!addlabel)
        return pix1;

        /* Adding text of color component values */
    if (location != L_ADD_ABOVE && location != L_ADD_AT_TOP &&
        location != L_ADD_AT_BOT && location != L_ADD_BELOW) {
        L_ERROR("invalid location: adding below\n", __func__);
        location = L_ADD_BELOW;
    }
    bmf = bmfCreate(NULL, 4);
    extractRGBValues(color, &rval, &gval, &bval);
    snprintf(buf, sizeof(buf), "%d,%d,%d", rval, gval, bval);
    pix2 = pixAddSingleTextblock(pix1, bmf, buf, textcolor, location, NULL);
    pixDestroy(&pix1);
    bmfDestroy(&bmf);
    return pix2;
}


void
l_chooseDisplayProg(l_int32  selection)
{
    if (selection == L_DISPLAY_WITH_XLI ||
        selection == L_DISPLAY_WITH_XZGV ||
        selection == L_DISPLAY_WITH_XV ||
        selection == L_DISPLAY_WITH_IV ||
        selection == L_DISPLAY_WITH_OPEN) {
        var_DISPLAY_PROG = selection;
    } else {
        L_ERROR("invalid display program\n", "l_chooseDisplayProg");
    }
}


/*---------------------------------------------------------------------*
 *                   Change format for missing lib                     *
 *---------------------------------------------------------------------*/
/*!
 * \brief   changeFormatForMissingLib()
 *
 * \param[in,out]    pformat    addr of requested output image format
 * \return  void
 *
 * <pre>
 * Notes:
 *      (1) This is useful for testing functionality when the library for
 *          the requested output format (jpeg, png or tiff) is not linked.
 *          In that case, the output format is changed to bmp.
 * </pre>
 */
void
changeFormatForMissingLib(l_int32  *pformat)
{
#if !defined(HAVE_LIBJPEG)
    if (*pformat == IFF_JFIF_JPEG) {
        L_WARNING("jpeg library missing; output bmp format\n", __func__);
        *pformat = IFF_BMP;
    }
#endif  /* !defined(HAVE_LIBJPEG) */
#if !defined(HAVE_LIBPNG)
    if (*pformat == IFF_PNG) {
        L_WARNING("png library missing; output bmp format\n", __func__);
        *pformat = IFF_BMP;
    }
#endif  /* !defined(HAVE_LIBPNG) */
#if !defined(HAVE_LIBTIFF)
    if (L_FORMAT_IS_TIFF(*pformat)) {
        L_WARNING("tiff library missing; output bmp format\n", __func__);
        *pformat = IFF_BMP;
    }
#endif  /* !defined(HAVE_LIBTIFF) */
}


/*---------------------------------------------------------------------*
 *                Deprecated pix output for debugging                  *
 *---------------------------------------------------------------------*/
/*!
 * \brief   pixDisplayWrite()
 *
 * \param[in]    pix
 * \param[in]    reduction
 * \return  1 (error)
 *
 * <pre>
 * Notes:
 *      As of 1.80, this is a non-functional stub.
 * </pre>
 */
l_ok
pixDisplayWrite(PIX     *pixs,
                l_int32  reduction)
{
    lept_stderr("\n########################################################\n"
                "  pixDisplayWrite() was last used in tesseract 3.04,"
                "  in Feb 2016.  As of 1.80, it is a non-functional stub\n"
                "########################################################"
                "\n\n\n");
    return 1;
}
