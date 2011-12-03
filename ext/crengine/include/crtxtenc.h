/** \file crtxtenc.h
    \brief character encoding utils

    CoolReader Engine

    (c) Vadim Lopatin, 2000-2006
    This source code is distributed under the terms of
    GNU General Public License.
    See LICENSE file for details.

*/

#ifndef __CRTXTENC_H_INCLUDED__
#define __CRTXTENC_H_INCLUDED__

#include "lvtypes.h"
#include <stdio.h>
#include "lvstring.h"


enum char_encoding_type {
    ce_unknown = 0,
    ce_utf8 = 1,
    ce_utf16_be = 2,
    ce_utf16_le = 3,
    ce_utf32_be = 4,
    ce_utf32_le = 5,
    ce_8bit_cp = 6,
};

#define CRENC_ID_UNKNOWN      ce_unknown
#define CRENC_ID_UTF8         ce_utf8
#define CRENC_ID_UTF16_LE     ce_utf16_le
#define CRENC_ID_UTF16_BE     ce_utf16_be
#define CRENC_ID_UTF32_LE     ce_utf32_le
#define CRENC_ID_UTF32_BE     ce_utf32_be
#define CRENC_ID_8BIT_START   ce_8bit_cp

int CREncodingNameToId( const lChar16 * name );
const char * CREncodingIdToName( int id );

/**
    \brief Searches for 8-bit encoding to unicode conversion table by encoding name.

    Conversion table is table of 128 unicode characters corresponding to 8-bit
    encoding characters 128..255. enc_table[0] is unicode value for character
    128 in 8-bit encoding.

    \param encoding_name is name of encoding, i.e. "utf-8", "windows-1251"

    \return pointer to conversion table if found, NULL otherwise
*/
const lChar16 * GetCharsetByte2UnicodeTable( const lChar16 * encoding_name );
const lChar16 * GetCharsetByte2UnicodeTableById( int id );
const lChar8 ** GetCharsetUnicode2ByteTable( const lChar16 * encoding_name );
/// get conversion table for upper 128 characters of codepage, by codepage number
const lChar16 * GetCharsetByte2UnicodeTable( int codepage );
/// returns "cp1251" for 1251, etc. for supported codepages
const lChar16 * GetCharsetName( int codepage );
/// convert language id to codepage number (MS)
int langToCodepage( int lang );

/**
    \brief Autodetects encoding of text data in buffer.

    \param buf is buffer with text data to autodetect
    \param buf_size is size of data in buffer, bytes
    \param cp_name is buffer to store autodetected name of encoding, i.e. "utf-8", "windows-1251"
    \param lang_name is buffer to store autodetected name of language, i.e. "en", "ru"

    \return non-zero on success
*/
int AutodetectCodePage( const unsigned char * buf, int buf_size, char * cp_name, char * lang_name );
/**
    \brief Autodetects encoding of text data in buffer, only using ByteOrderMark or Utf-8 validity detection.

    \param buf is buffer with text data to autodetect
    \param buf_size is size of data in buffer, bytes
    \param cp_name is buffer to store autodetected name of encoding, i.e. "utf-8", "windows-1251"
    \param lang_name is buffer to store autodetected name of language, i.e. "en", "ru"

    \return non-zero on success
*/
int AutodetectCodePageUtf( const unsigned char * buf, int buf_size, char * cp_name, char * lang_name );

/**
    \brief checks whether data buffer is valid utf-8 stream

    \param buf is buffer with text data to autodetect
    \param buf_size is size of data in buffer, bytes

    \return true if buffer has valid utf-8 data
*/
bool isValidUtf8Data( const unsigned char * buf, int buf_size );

void MakeStatsForFile( const char * fname, const char * cp_name, const char * lang_name, int index, FILE * f, lString8 & list );


#endif
