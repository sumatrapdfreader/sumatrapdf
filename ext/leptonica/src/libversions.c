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
 * \file libversions.c
 * <pre>
 *
 *       Image library version number
 *           char      *getImagelibVersions()
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include "allheaders.h"

#if HAVE_LIBGIF
#include "gif_lib.h"
#endif

#if HAVE_LIBJPEG
/* jpeglib.h includes jconfig.h, which makes the error of setting
 *   #define HAVE_STDLIB_H
 * which conflicts with config_auto.h (where it is set to 1) and results
 * for some gcc compiler versions in a warning.  The conflict is harmless
 * but we suppress it by undefining the variable. */
#undef HAVE_STDLIB_H
#include "jpeglib.h"
#include "jerror.h"
#endif

#if HAVE_LIBPNG
#include "png.h"
#endif

#if HAVE_LIBTIFF
#include "tiffio.h"
#endif

#if HAVE_LIBZ
#include "zlib.h"
#endif

#if HAVE_LIBWEBP
#include "webp/encode.h"
#endif

#if HAVE_LIBJP2K
  #ifdef LIBJP2K_HEADER
    #include LIBJP2K_HEADER
  #else
    #include <openjpeg.h>
  #endif
#endif


/*---------------------------------------------------------------------*
 *                    Image Library Version number                     *
 *---------------------------------------------------------------------*/
/*!
 * \brief   getImagelibVersions()
 *
 * <pre>
 * Notes:
 *      (1) This returns a string of version numbers; e.g.,
 *            libgif 5.1.4
 *            libjpeg 8b (libjpeg-turbo 2.0.3)
 *            libpng 1.6.37
 *            libtiff 4.1.0
 *            zlib 1.2.11
 *            libwebp 0.6.1
 *            libopenjp2 2.5.0
 *      (2) The caller must free the memory.
 * </pre>
 */
char *
getImagelibVersions(void)
{
#if HAVE_LIBGIF || HAVE_LIBJPEG
char     buf[128];
#endif
l_int32  first = TRUE;
#if HAVE_LIBJPEG || HAVE_LIBTIFF
char    *versionNumP;
char    *nextTokenP;
#endif
char    *versionStrP = NULL;

#if HAVE_LIBGIF
    first = FALSE;
    stringJoinIP(&versionStrP, "libgif ");
  #ifdef GIFLIB_MAJOR
    snprintf(buf, sizeof(buf), "%d.%d.%d", GIFLIB_MAJOR, GIFLIB_MINOR,
             GIFLIB_RELEASE);
  #else
    stringCopy(buf, "4.1.6(?)", sizeof(buf));
  #endif
    stringJoinIP(&versionStrP, buf);
#endif  /* HAVE_LIBGIF */

#if HAVE_LIBJPEG
    {
    struct jpeg_compress_struct  cinfo = { 0 };
    struct jpeg_error_mgr        err = { 0 };
    char                         buffer[JMSG_LENGTH_MAX];
    cinfo.err = jpeg_std_error(&err);
    err.msg_code = JMSG_VERSION;
    (*err.format_message) ((j_common_ptr ) &cinfo, buffer);

    if (!first) stringJoinIP(&versionStrP, " : ");
    first = FALSE;
    stringJoinIP(&versionStrP, "libjpeg ");
    versionNumP = strtokSafe(buffer, " ", &nextTokenP);
    stringJoinIP(&versionStrP, versionNumP);
    LEPT_FREE(versionNumP);

  #if defined(LIBJPEG_TURBO_VERSION)
        /* To stringify the result of expansion of a macro argument,
         * you must use two levels of macros.  See:
         *   https://gcc.gnu.org/onlinedocs/cpp/Stringification.html  */
    #define l_xstr(s) l_str(s)
    #define l_str(s) #s
    snprintf(buf, sizeof(buf), " (libjpeg-turbo %s)",
             l_xstr(LIBJPEG_TURBO_VERSION));
    stringJoinIP(&versionStrP, buf);
  #endif  /* LIBJPEG_TURBO_VERSION */
    }
#endif  /* HAVE_LIBJPEG */

#if HAVE_LIBPNG
    if (!first) stringJoinIP(&versionStrP, " : ");
    first = FALSE;
    stringJoinIP(&versionStrP, "libpng ");
    stringJoinIP(&versionStrP, png_get_libpng_ver(NULL));
#endif  /* HAVE_LIBPNG */

#if HAVE_LIBTIFF
    if (!first) stringJoinIP(&versionStrP, " : ");
    first = FALSE;
    stringJoinIP(&versionStrP, "libtiff ");
    versionNumP = strtokSafe((char *)TIFFGetVersion(), " \n", &nextTokenP);
    LEPT_FREE(versionNumP);
    versionNumP = strtokSafe(NULL, " \n", &nextTokenP);
    LEPT_FREE(versionNumP);
    versionNumP = strtokSafe(NULL, " \n", &nextTokenP);
    stringJoinIP(&versionStrP, versionNumP);
    LEPT_FREE(versionNumP);
#endif  /* HAVE_LIBTIFF */

#if HAVE_LIBZ
    if (!first) stringJoinIP(&versionStrP, " : ");
    first = FALSE;
    stringJoinIP(&versionStrP, "zlib ");
    stringJoinIP(&versionStrP, ZLIB_VERSION);
#endif  /* HAVE_LIBZ */

#if HAVE_LIBWEBP
    {
    l_int32 val;
    char buf[32];
    if (!first) stringJoinIP(&versionStrP, " : ");
    first = FALSE;
    stringJoinIP(&versionStrP, "libwebp ");
    val = WebPGetEncoderVersion();
    snprintf(buf, sizeof(buf), "%d.%d.%d", val >> 16, (val >> 8) & 0xff,
             val & 0xff);
    stringJoinIP(&versionStrP, buf);
    }
#endif  /* HAVE_LIBWEBP */

#if HAVE_LIBJP2K
    {
    const char *version;
    if (!first) stringJoinIP(&versionStrP, " : ");
    first = FALSE;
    stringJoinIP(&versionStrP, "libopenjp2 ");
    version = opj_version();
    stringJoinIP(&versionStrP, version);
    }
#endif  /* HAVE_LIBJP2K */

    return versionStrP;
}
