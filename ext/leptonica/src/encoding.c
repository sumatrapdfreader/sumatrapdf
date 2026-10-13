/*====================================================================*
 -  Copyright (C) 2001 Leptonica.  All rights reserved.
 -  This software is distributed in the hope that it will be
 -  useful, but with NO WARRANTY OF ANY KIND.
 -  No author or distributor accepts responsibility to anyone for the
 -  consequences of using this software, or for whether it serves any
 -  particular purpose or works at all, unless he or she says so in
 -  writing.  Everyone is granted permission to copy, modify and
 -  redistribute this source code, for commercial or non-commercial
 -  purposes, with the following restrictions: (1) the origin of this
 -  source code must not be misrepresented; (2) modified versions must
 -  be plainly marked as such; and (3) this notice may not be removed
 -  or altered from any source or modified source distribution.
 *====================================================================*/

/*
 *  encodings.c
 *
 *    Base64
 *        char           *encodeBase64()
 *        l_uint8        *decodeBase64()
 *        static l_int32  isBase64()
 *        static l_int32 *genReverseTab64()
 *        static void     byteConvert3to4()
 *        static void     byteConvert4to3()
 *
 *    Ascii85
 *        char           *encodeAscii85()
 *        l_uint8        *decodeAscii85()
 *        static l_int32  convertChunkToAscii85()
 *
 *        char           *encodeAscii85WithComp()
 *        l_uint8        *decodeAscii85WithComp()
 *
 *    String reformatting for base 64 encoded data
 *        char           *reformatPacked64()
 *
 *  Base64 encoding is useful for encding binary data in a restricted set of
 *  64 printable ascii symbols, that includes the 62 alphanumerics and '+'
 *  and '/'.  Notably it does not include quotes, so that base64 encoded
 *  strings can be used in situations where quotes are used for formatting.
 *  64 symbols was chosen because it is the smallest number that can be used
 *  in 4-for-3 byte encoding of binary data:
 *         log2(64) / log2(256) = 0.75 = 3/4
 *
 *  Ascii85 encoding is used in PostScript and some pdf files for
 *  representing binary data (for example, a compressed image) in printable
 *  ascii symbols.  It has a dictionary of 85 symbols; 85 was chosen because
 *  it is the smallest number that can be used in 5-for-4 byte encoding
 *  of binary data (256 possible input values).  This can be seen from
 *  the max information content in such a sequence:
 *         log2(84) / log2(256) = 0.799 < 4/5
 *         log2(85) / log2(256) = 0.801 > 4/5
 */

#ifdef HAVE_CONFIG_H
#include <config_auto.h>
#endif  /* HAVE_CONFIG_H */

#include <ctype.h>
#include <string.h>
#include "allheaders.h"

    /* Base64 encoding table in string representation */
static const l_int32  MAX_BASE64_LINE   = 72;  /* max line length base64 */
static const char *tablechar64 =
             "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
             "abcdefghijklmnopqrstuvwxyz"
             "0123456789+/";

static l_int32 isBase64(char);
static l_int32 *genReverseTab64(void);
static void byteConvert3to4(l_uint8 *in3, l_uint8 *out4);
static void byteConvert4to3(l_uint8 *in4, l_uint8 *out3);

    /* Ascii85 encoding */
static const l_int32  MAX_ASCII85_LINE   = 64;  /* max line length ascii85 */
static const l_uint32  power85[5] = {1,
                                     85,
                                     85 * 85,
                                     85 * 85 * 85,
                                     85 * 85 * 85 * 85};

static l_int32 convertChunkToAscii85(const l_uint8 *inarray, size_t insize,
                                     l_int32 *pindex, char *outbuf,
                                     l_int32 *pnbout);

/*-------------------------------------------------------------*
 *      Utility for encoding and decoding data with base64     *
 *-------------------------------------------------------------*/
/*!
 * \brief   encodeBase64()
 *
 * \param[in]    inarray     input binary data
 * \param[in]    insize      number of bytes in input array
 * \param[out]   poutsize    number of bytes in output char array
 * \return  chara with MAX_BASE64_LINE characters + \n in each line
 *
 * <pre>
 * Notes:
 *      (1) The input character data is unrestricted binary.
 *          The output encoded data consists of the 64 characters
 *          in the base64 set, plus newlines and the pad character '='.
 * </pre>
 */
char *
encodeBase64(const l_uint8 *inarray,
             l_int32        insize,
             l_int32       *poutsize)
{
char          *chara;
const l_uint8 *bytea;
l_uint8        array3[3], array4[4];
l_int32        outsize, i, j, index, linecount;

    if (!poutsize)
        return (char *)ERROR_PTR("&outsize not defined", __func__, NULL);
    *poutsize = 0;
    if (!inarray)
        return (char *)ERROR_PTR("inarray not defined", __func__, NULL);
    if (insize <= 0)
        return (char *)ERROR_PTR("insize not > 0", __func__, NULL);

        /* The output array is padded to a multiple of 4 bytes, not
         * counting the newlines.  We just need to allocate a large
         * enough array, and add 4 bytes to make sure it is big enough. */
    outsize = 4 * ((insize + 2) / 3);  /* without newlines */
    outsize += outsize / MAX_BASE64_LINE + 4;  /* with the newlines */
    if ((chara = (char *)LEPT_CALLOC(outsize, sizeof(char))) == NULL)
        return (char *)ERROR_PTR("chara not made", __func__, NULL);

        /* Read all the input data, and convert in sets of 3 input
         * bytes --> 4 output bytes. */
    i = index = linecount = 0;
    bytea = inarray;
    while (insize--) {
        if (linecount == MAX_BASE64_LINE) {
            chara[index++] = '\n';
            linecount = 0;
        }
        array3[i++] = *bytea++;
        if (i == 3) {  /* convert 3 to 4 and save */
            byteConvert3to4(array3, array4);
            for (j = 0; j < 4; j++)
                chara[index++] = tablechar64[array4[j]];
            i = 0;
            linecount += 4;
        }
    }

        /* Suppose 1 or 2 bytes has been read but not yet processed.
         * If 1 byte has been read, this will generate 2 bytes of
         * output, with 6 bits to the first byte and 2 bits to the second.
         * We will add two bytes of '=' for padding.
         * If 2 bytes has been read, this will generate 3 bytes of output,
         * with 6 bits to the first 2 bytes and 4 bits to the third, and
         * we add a fourth padding byte ('='). */
    if (i > 0) {  /* left-over 1 or 2 input bytes */
        for (j = i; j < 3; j++)
            array3[j] = '\0';  /* zero the remaining input bytes */
        byteConvert3to4(array3, array4);
        for (j = 0; j <= i; j++)
            chara[index++] = tablechar64[array4[j]];
        for (j = i + 1; j < 4; j++)
            chara[index++] = '=';
    }
    *poutsize = index;

    return chara;
}


/*!
 * \brief   decodeBase64()
 *
 * \param[in]    inarray    input encoded char data, with 72 chars/line)
 * \param[in]    insize     number of bytes in input array
 * \param[out]   poutsize   number of bytes in output byte array
 * \return  bytea decoded byte data, or NULL on error
 *
 * <pre>
 * Notes:
 *      (1) The input character data should have only 66 different characters:
 *          The 64 character set for base64 encoding, plus the pad
 *          character '=' and newlines for formatting with fixed line
 *          lengths.  If there are any other characters, the decoder
 *          will declare the input data to be invalid and return NULL.
 *      (2) The decoder ignores newlines and, for a valid input string,
 *          stops reading input when a pad byte is found.
 * </pre>
 */
l_uint8 *
decodeBase64(const char  *inarray,
             l_int32      insize,
             l_int32     *poutsize)
{
char      inchar;
l_uint8  *bytea;
l_uint8   array3[3], array4[4];
l_int32  *rtable64;
l_int32   i, j, outsize, in_index, out_index;

    if (!poutsize)
        return (l_uint8 *)ERROR_PTR("&outsize not defined", __func__, NULL);
    *poutsize = 0;
    if (!inarray)
        return (l_uint8 *)ERROR_PTR("inarray not defined", __func__, NULL);
    if (insize <= 0)
        return (l_uint8 *)ERROR_PTR("insize not > 0", __func__, NULL);

        /* Validate the input data */
    for (i = 0; i < insize; i++) {
        inchar = inarray[i];
        if (inchar == '\n') continue;
        if (isBase64(inchar) == 0 && inchar != '=')
            return (l_uint8 *)ERROR_PTR("invalid char in inarray",
                                        __func__, NULL);
    }

        /* The input array typically is made with a newline every
         * MAX_BASE64_LINE input bytes.  However, as a printed string, the
         * newlines would be stripped.  So when we allocate the output
         * array, assume the input array is all data, but strip
         * out the newlines during decoding.  This guarantees that
         * the allocated array is large enough. */
    outsize = 3 * ((insize + 3) / 4) + 4;
    if ((bytea = (l_uint8 *)LEPT_CALLOC(outsize, sizeof(l_uint8))) == NULL)
        return (l_uint8 *)ERROR_PTR("bytea not made", __func__, NULL);

        /* The number of encoded input data bytes is always a multiple of 4.
         * Read all the data, until you reach either the end or
         * the first pad character '='.  The data is processed in
         * units of 4 input bytes, generating 3 output decoded bytes
         * of binary data.  Newlines are ignored.  If there are no
         * pad bytes, i == 0 at the end of this section. */
    rtable64 = genReverseTab64();
    i = in_index = out_index = 0;
    for (in_index = 0; in_index < insize; in_index++) {
        inchar = inarray[in_index];
        if (inchar == '\n') continue;
        if (inchar == '=') break;
        array4[i++] = rtable64[(unsigned char)inchar];
        if (i < 4) {
            continue;
        } else {  /* i == 4; convert 4 to 3 and save */
            byteConvert4to3(array4, array3);
            for (j = 0; j < 3; j++)
                bytea[out_index++] = array3[j];
            i = 0;
        }
    }

        /* If i > 0, we ran into pad bytes ('=').  If i == 2, there are
         * two input pad bytes and one output data byte.  If i == 3,
         * there is one input pad byte and two output data bytes. */
    if (i > 0) {
        for (j = i; j < 4; j++)
            array4[j] = '\0';  /* zero the remaining input bytes */
        byteConvert4to3(array4, array3);
        for (j = 0; j < i - 1; j++)
            bytea[out_index++] = array3[j];
    }
    *poutsize = out_index;

    LEPT_FREE(rtable64);
    return bytea;
}


/*!
 * \brief   isBase64()
 */
static l_int32
isBase64(char  c)
{
    return (isalnum(((int)c)) || ((c) == '+') || ((c) == '/')) ? 1 : 0;
}

/*!
 * \brief   genReverseTab64()
 */
static l_int32 *
genReverseTab64(void)
{
l_int32   i;
l_int32  *rtable64;

    rtable64 = (l_int32 *)LEPT_CALLOC(128, sizeof(l_int32));
    for (i = 0; i < 64; i++) {
        rtable64[(unsigned char)tablechar64[i]] = i;
    }
    return rtable64;
}

/*!
 * \brief   byteConvert3to4()
 */
static void
byteConvert3to4(l_uint8  *in3,
                l_uint8  *out4)
{
    out4[0] = in3[0] >> 2;
    out4[1] = ((in3[0] & 0x03) << 4) | (in3[1] >> 4);
    out4[2] = ((in3[1] & 0x0f) << 2) | (in3[2] >> 6);
    out4[3] = in3[2] & 0x3f;
    return;
}

/*!
 * \brief   byteConvert4to3()
 */
static void
byteConvert4to3(l_uint8  *in4,
                l_uint8  *out3)
{
    out3[0] = (in4[0] << 2) | (in4[1] >> 4);
    out3[1] = ((in4[1] & 0x0f) << 4) | (in4[2] >> 2);
    out3[2] = ((in4[2] & 0x03) << 6) | in4[3];
    return;
}


/*-------------------------------------------------------------*
 *      Utility for encoding and decoding data with ascii85    *
 *-------------------------------------------------------------*/
/*!
 * \brief   encodeAscii85()
 *
 * \param[in]    inarray    input data
 * \param[in]    insize     number of bytes in input array
 * \param[out]   poutsize   number of bytes in output char array
 * \return  chara with 64 characters + \n in each line
 *
 * <pre>
 * Notes:
 *      (1) Ghostscript has a stack break if the last line of
 *          data only has a '>', so we avoid the problem by
 *          always putting '~>' on the last line.
 * </pre>
 */
char *
encodeAscii85(const l_uint8  *inarray,
              size_t          insize,
              size_t         *poutsize)
{
char    *chara;
char     outbuf[8];
l_int32  maxsize, i, index, linecount, nbout, eof;
size_t   outindex;

    if (!poutsize)
        return (char *)ERROR_PTR("&outsize not defined", __func__, NULL);
    *poutsize = 0;
    if (!inarray)
        return (char *)ERROR_PTR("inarray not defined", __func__, NULL);
    if (insize <= 0)
        return (char *)ERROR_PTR("insize not > 0", __func__, NULL);

        /* Accumulate results in char array */
    maxsize = (l_int32)(80. + (insize * 5. / 4.) *
                        (1. + 2. / MAX_ASCII85_LINE));
    if ((chara = (char *)LEPT_CALLOC(maxsize, sizeof(char))) == NULL)
        return (char *)ERROR_PTR("chara not made", __func__, NULL);

    linecount = 0;
    index = 0;
    outindex = 0;
    while (1) {
        eof = convertChunkToAscii85(inarray, insize, &index, outbuf, &nbout);
        for (i = 0; i < nbout; i++) {
            chara[outindex++] = outbuf[i];
            linecount++;
            if (linecount >= MAX_ASCII85_LINE) {
                chara[outindex++] = '\n';
                linecount = 0;
            }
        }
        if (eof == TRUE) {
            if (linecount != 0)
                chara[outindex++] = '\n';
            chara[outindex++] = '~';
            chara[outindex++] = '>';
            chara[outindex++] = '\n';
            break;
        }
    }

    *poutsize = outindex;
    return chara;
}


/*!
 * \brief   convertChunkToAscii85()
 *
 * \param[in]    inarray    input data
 * \param[in]    insize     number of bytes in input array
 * \param[out]   pindex     use and -- ptr
 * \param[in]    outbuf     holds 8 ascii chars; we use no more than 7
 * \param[out]   pnbsout    number of bytes written to outbuf
 * \return  boolean for eof 0 if more data, 1 if end of file
 *
 * <pre>
 * Notes:
 *      (1) Attempts to read 4 bytes and write 5.
 *      (2) Writes 1 byte if the value is 0.
 * </pre>
 */
static l_int32
convertChunkToAscii85(const l_uint8 *inarray,
                      size_t         insize,
                      l_int32       *pindex,
                      char          *outbuf,
                      l_int32       *pnbout)
{
l_uint8   inbyte;
l_uint32  inword, val;
l_int32   eof, index, nread, nbout, i;

    eof = FALSE;
    index = *pindex;
    nread = L_MIN(4, (insize - index));
    if (insize == index + nread)
        eof = TRUE;
    *pindex += nread;  /* save new index */

        /* Read input data and save in l_uint32 */
    inword = 0;
    for (i = 0; i < nread; i++) {
        inbyte = inarray[index + i];
        inword += (l_uint32)inbyte << (8 * (3 - i));
    }

#if 0
    lept_stderr("index = %d, nread = %d\n", index, nread);
    lept_stderr("inword = %x\n", inword);
    lept_stderr("eof = %d\n", eof);
#endif

        /* Special case: output 1 byte only */
    if (inword == 0) {
        outbuf[0] = 'z';
        nbout = 1;
    } else { /* output nread + 1 bytes */
        for (i = 4; i >= 4 - nread; i--) {
            val = inword / power85[i];
            outbuf[4 - i] = (l_uint8)(val + '!');
            inword -= val * power85[i];
        }
        nbout = nread + 1;
    }
    *pnbout = nbout;

    return eof;
}


/*!
 * \brief   decodeAscii85()
 *
 * \param[in]    inarray     ascii85 input data
 * \param[in]    insize      number of bytes in input array
 * \param[out]   poutsize    number of bytes in output l_uint8 array
 * \return  outarray binary
 *
 * <pre>
 * Notes:
 *      (1) We assume the data is properly encoded, so we do not check
 *          for invalid characters or the final '>' character.
 *      (2) We permit whitespace to be added to the encoding in an
 *          arbitrary way.
 * </pre>
 */
l_uint8 *
decodeAscii85(const char *inarray,
              size_t      insize,
              size_t     *poutsize)
{
char        inc;
const char *pin;
l_uint8     val;
l_uint8    *outa;
l_int32     maxsize, ocount, bytecount, index;
l_uint32    oword;

    if (!poutsize)
        return (l_uint8 *)ERROR_PTR("&outsize not defined", __func__, NULL);
    *poutsize = 0;
    if (!inarray)
        return (l_uint8 *)ERROR_PTR("inarray not defined", __func__, NULL);
    if (insize <= 0)
        return (l_uint8 *)ERROR_PTR("insize not > 0", __func__, NULL);

        /* Accumulate results in outa */
    maxsize = (l_int32)(80. + (insize * 4. / 5.));  /* plenty big */
    if ((outa = (l_uint8 *)LEPT_CALLOC(maxsize, sizeof(l_uint8))) == NULL)
        return (l_uint8 *)ERROR_PTR("outa not made", __func__, NULL);

    pin = inarray;
    ocount = 0;  /* byte index into outa */
    oword = 0;
    for (index = 0, bytecount = 0; index < insize; index++, pin++) {
        inc = *pin;

        if (inc == ' ' || inc == '\t' || inc == '\n' ||
            inc == '\f' || inc == '\r' || inc == '\v')  /* ignore white space */
            continue;

        val = inc - '!';
        if (val < 85) {
            oword = oword * 85 + val;
            if (bytecount < 4) {
                bytecount++;
            } else {  /* we have all 5 input chars for the oword */
                outa[ocount] = (oword >> 24) & 0xff;
                outa[ocount + 1] = (oword >> 16) & 0xff;
                outa[ocount + 2] = (oword >> 8) & 0xff;
                outa[ocount + 3] = oword & 0xff;
                ocount += 4;
                bytecount = 0;
                oword = 0;
            }
        } else if (inc == 'z' && bytecount == 0) {
            outa[ocount] = 0;
            outa[ocount + 1] = 0;
            outa[ocount + 2] = 0;
            outa[ocount + 3] = 0;
            ocount += 4;
        } else if (inc == '~') {  /* end of data */
            L_INFO(" %d extra bytes output\n", __func__, bytecount - 1);
            switch (bytecount) {
            case 0:   /* normal eof */
            case 1:   /* error */
                break;
            case 2:   /* 1 extra byte */
                oword = oword * power85[3] + 0xffffff;
                outa[ocount] = (oword >> 24) & 0xff;
                break;
            case 3:   /* 2 extra bytes */
                oword = oword * power85[2] + 0xffff;
                outa[ocount] = (oword >> 24) & 0xff;
                outa[ocount + 1] = (oword >> 16) & 0xff;
                break;
            case 4:   /* 3 extra bytes */
                oword = oword * 85 + 0xff;
                outa[ocount] = (oword >> 24) & 0xff;
                outa[ocount + 1] = (oword >> 16) & 0xff;
                outa[ocount + 2] = (oword >> 8) & 0xff;
                break;
            }
            if (bytecount > 1)
                ocount += (bytecount - 1);
            break;
        }
    }
    *poutsize = ocount;

    return outa;
}


/*!
 * \brief   encodeAscii85WithComp)
 *
 * \param[in]    indata     input binary data
 * \param[in]    insize     number of bytes in input data
 * \param[out]   poutsize   number of bytes in output string
 * \return  outstr with 64 characters + \n in each line
 *
 * <pre>
 * Notes:
 *      (1) Compress the input data; then encode ascii85.  For ascii
 *          input, a first compression step will significantly reduce
 *          the final encoded output size.
 * </pre>
 */
char *
encodeAscii85WithComp(const l_uint8  *indata,
                      size_t          insize,
                      size_t         *poutsize)
{
char     *outstr;
size_t    size1;
l_uint8  *data1;

    if (!poutsize)
        return (char *)ERROR_PTR("&outsize not defined", __func__, NULL);
    *poutsize = 0;
    if (!indata)
        return (char *)ERROR_PTR("indata not defined", __func__, NULL);

    if ((data1 = zlibCompress(indata, insize, &size1)) == NULL)
        return (char *)ERROR_PTR("data1 not made", __func__, NULL);
    outstr = encodeAscii85(data1, size1, poutsize);
    LEPT_FREE(data1);
    return outstr;
}


/*!
 * \brief   decodeAscii85WithComp()
 *
 * \param[in]    instr       ascii85 input data string
 * \param[in]    insize      number of bytes in input data
 * \param[out]   poutsize    number of bytes in output binary data
 * \return  outdata   binary data before compression and ascii85 encoding
 *
 * <pre>
 * Notes:
 *      (1) We assume the input data has been zlib compressed and then
 *          properly encoded, so we reverse the procedure.  This is the
 *          inverse of encodeAscii85WithComp().
 *      (2) Set %insize == 0 to use strlen(%instr).
 * </pre>
 */
l_uint8 *
decodeAscii85WithComp(const char  *instr,
                      size_t       insize,
                      size_t      *poutsize)
{
size_t    size1;
l_uint8  *data1, *outdata;

    if (!poutsize)
        return (l_uint8 *)ERROR_PTR("&outsize not defined", __func__, NULL);
    *poutsize = 0;
    if (!instr)
        return (l_uint8 *)ERROR_PTR("instr not defined", __func__, NULL);

    if (insize == 0) insize = strlen(instr);
    if ((data1 = decodeAscii85(instr, insize, &size1)) == NULL)
        return (l_uint8 *)ERROR_PTR("data1 not made", __func__, NULL);
    outdata = zlibUncompress(data1, size1, poutsize);
    LEPT_FREE(data1);
    return outdata;
}


/*-------------------------------------------------------------*
 *       String reformatting for base 64 encoded data          *
 *-------------------------------------------------------------*/
/*!
 * \brief   reformatPacked64()
 *
 * \param[in]    inarray     base64 encoded string with newlines
 * \param[in]    insize      number of bytes in input array
 * \param[in]    leadspace   number of spaces in each line before the data
 * \param[in]    linechars   number of bytes of data in each line; multiple of 4
 * \param[in]    addquotes   1 to add quotes to each line of data; 0 to skip
 * \param[out]   poutsize    number of bytes in output char array
 * \return  outarray ascii
 *
 * <pre>
 * Notes:
 *      (1) Each line in the output array has %leadspace space characters,
 *          followed optionally by a double-quote, followed by %linechars
 *          bytes of base64 data, followed optionally by a double-quote,
 *          followed by a newline.
 *      (2) This can be used to convert a base64 encoded string to a
 *          string formatted for inclusion in a C source file.
 * </pre>
 */
char *
reformatPacked64(const char *inarray,
                 l_int32     insize,
                 l_int32     leadspace,
                 l_int32     linechars,
                 l_int32     addquotes,
                 l_int32    *poutsize)
{
char    *flata, *outa;
l_int32  i, j, flatindex, flatsize, outindex, nlines, linewithpad, linecount;

    if (!poutsize)
        return (char *)ERROR_PTR("&outsize not defined", __func__, NULL);
    *poutsize = 0;
    if (!inarray)
        return (char *)ERROR_PTR("inarray not defined", __func__, NULL);
    if (insize <= 0)
        return (char *)ERROR_PTR("insize not > 0", __func__, NULL);
    if (leadspace < 0)
        return (char *)ERROR_PTR("leadspace must be >= 0", __func__, NULL);
    if (linechars % 4)
        return (char *)ERROR_PTR("linechars % 4 must be 0", __func__, NULL);

        /* Remove all white space */
    if ((flata = (char *)LEPT_CALLOC(insize, sizeof(char))) == NULL)
        return (char *)ERROR_PTR("flata not made", __func__, NULL);
    for (i = 0, flatindex = 0; i < insize; i++) {
        if (isBase64(inarray[i]) || inarray[i] == '=')
            flata[flatindex++] = inarray[i];
    }

        /* Generate output string */
    flatsize = flatindex;
    nlines = (flatsize + linechars - 1) / linechars;
    linewithpad = leadspace + linechars + 1;  /* including newline */
    if (addquotes) linewithpad += 2;
    if ((outa = (char *)LEPT_CALLOC((size_t)nlines * linewithpad,
                                    sizeof(char))) == NULL) {
        LEPT_FREE(flata);
        return (char *)ERROR_PTR("outa not made", __func__, NULL);
    }
    for (j = 0, outindex = 0; j < leadspace; j++)
        outa[outindex++] = ' ';
    if (addquotes) outa[outindex++] = '"';
    for (i = 0, linecount = 0; i < flatsize; i++) {
        if (linecount == linechars) {
            if (addquotes) outa[outindex++] = '"';
            outa[outindex++] = '\n';
            for (j = 0; j < leadspace; j++)
                outa[outindex++] = ' ';
            if (addquotes) outa[outindex++] = '"';
            linecount = 0;
        }
        outa[outindex++] = flata[i];
        linecount++;
    }
    if (addquotes) outa[outindex++] = '"';
    *poutsize = outindex;

    LEPT_FREE(flata);
    return outa;
}
