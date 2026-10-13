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
 * \file  bootnumgen1.c
 * <pre>
 *
 *   Function for generating prog/recog/digits/bootnum1.pa from an
 *   encoded, gzipped and serialized string.
 *
 *   This was generated using the stringcode utility, slightly edited,
 *   and then merged into a single file.
 *
 *   The code and encoded strings were made using the stringcode utility:
 *
 *       L_STRCODE  *strc;
 *       strc = strcodeCreate(101);   // arbitrary integer
 *       strcodeGenerate(strc, "recog/digits/bootnum1.pa", "PIXA");
 *       strcodeFinalize(&strc, ".");
 *
 *   The two output files, autogen.101.c and autogen.101.h, were
 *   then slightly edited and merged into this file.
 *
 *   Call this way:
 *       PIXA  *pixa = l_bootnum_gen1();   (C)
 *       Pixa  *pixa = l_bootnum_gen1();   (C++)
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
static const char *l_bootnum1 =
    "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";

/*---------------------------------------------------------------------*/
/*                      Auto-generated deserializer                    */
/*---------------------------------------------------------------------*/
/*!
 * \brief   l_bootnum_gen1()
 *
 * \return   pixa  of labeled digits
 *
 * <pre>
 * Call this way:
 *      PIXA  *pixa = l_bootnum_gen1();   (C)
 *      Pixa  *pixa = l_bootnum_gen1();   (C++)
 * </pre>
 */
PIXA *
l_bootnum_gen1(void)
{
l_uint8  *data1, *data2;
l_int32   size1;
size_t    size2;
PIXA     *pixa;

        /* Unencode selected string, write to file, and read it */
    data1 = decodeBase64(l_bootnum1, strlen(l_bootnum1), &size1);
    data2 = zlibUncompress(data1, size1, &size2);
    pixa = pixaReadMem(data2, size2);
    lept_free(data1);
    lept_free(data2);
    return pixa;
}
