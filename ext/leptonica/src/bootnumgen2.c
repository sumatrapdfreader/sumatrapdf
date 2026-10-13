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
 * \file  bootnumgen2.c
 * <pre>
 *
 *   Function for generating prog/recog/digits/bootnum2.pa from an
 *   encoded, gzipped and serialized string.
 *
 *   This was generated using the stringcode utility, slightly edited,
 *   and then merged into a single file.
 *
 *   The code and encoded strings were made using the stringcode utility:
 *
 *       L_STRCODE  *strc;
 *       strc = strcodeCreate(102);   // arbitrary integer
 *       strcodeGenerate(strc, "recog/digits/bootnum2.pa", "PIXA");
 *       strcodeFinalize(&strc, ".");
 *
 *   The two output files, autogen.102.c and autogen.102.h, were
 *   then slightly edited and merged into this file.
 *
 *   Call this way:
 *       PIXA  *pixa = l_bootnum_gen2();   (C)
 *       Pixa  *pixa = l_bootnum_gen2();   (C++)
 * </pre>
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include <string.h>
#include "allheaders.h"

/*---------------------------------------------------------------------*/
/*                         Serialized string                           */
/*---------------------------------------------------------------------*/
static const char *l_bootnum2 =
    "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";


/*---------------------------------------------------------------------*/
/*                      Auto-generated deserializer                    */
/*---------------------------------------------------------------------*/
/*!
 * \brief   l_bootnum_gen2()
 *
 * \return   pixa  of labeled digits
 *
 * <pre>
 * Call this way:
 *      PIXA  *pixa = l_bootnum_gen2();   (C)
 *      Pixa  *pixa = l_bootnum_gen2();   (C++)
 * </pre>
 */
PIXA *
l_bootnum_gen2(void)
{
l_uint8  *data1, *data2;
l_int32   size1;
size_t    size2;
PIXA     *pixa;

        /* Unencode selected string, write to file, and read it */
    data1 = decodeBase64(l_bootnum2, strlen(l_bootnum2), &size1);
    data2 = zlibUncompress(data1, size1, &size2);
    pixa = pixaReadMem(data2, size2);
    lept_free(data1);
    lept_free(data2);
    return pixa;
}
