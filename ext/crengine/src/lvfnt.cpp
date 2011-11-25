/*******************************************************

   CoolReader Engine C-compatible API

   lvfnt.cpp:  Unicode Antialiased Bitmap Font

   (c) Vadim Lopatin, 2000-2006
   This source code is distributed under the terms of
   GNU General Public License
   See LICENSE file for details

*******************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "../include/lvfnt.h"

#define MIN_FONT_SIZE 2048
#define MAX_FONT_SIZE 0x100000

#ifndef __cplusplus
int lvfontIsUnicodeSpace( lChar16 code )
{
    /* TODO: add other space codes here */
    return (code==0x0020);
}
#endif

const lvfont_header_t * lvfontGetHeader( const lvfont_handle pfont )
{
    return (const tag_lvfont_header *) pfont;
}

const hrle_decode_info_t * lvfontGetDecodeTable( const lvfont_handle pfont )
{
    return (hrle_decode_info_t *) 
        ((const lUInt8*)pfont + ((const tag_lvfont_header *) pfont)->decodeTableOffset);
}

const lvfont_glyph_t * lvfontGetGlyph( const lvfont_handle pfont, lUInt16 code )
{
    const tag_lvfont_header * hdr = (const tag_lvfont_header *) pfont;
    if ( code>hdr->maxCode )
        return NULL;
    lUInt32 rangeoffset = hdr->rangesOffset[(code>>6) & 0x3FF ];
    if ( rangeoffset == 0 || rangeoffset>hdr->fileSize )
        return NULL;
    const lvfont_range_t * range = (const lvfont_range_t *)
        ( ((const char *)pfont) + rangeoffset );
    lUInt32 glyphoffset = range->glyphsOffset[code & 0x3F];
    if ( glyphoffset == 0 || glyphoffset>hdr->fileSize )
        return NULL;
    return (const lvfont_glyph_t *)
        ( ((const char *)range) + glyphoffset );
}

int lvfontOpen( const char * fname, lvfont_handle * pfont )
{
    static lvByteOrderConv cnv;

    FILE * f = fopen( fname, "rb" );
    if (f == NULL)
        return 0;
    fseek( f, 0, SEEK_END );
    lUInt32 sz = ftell(f);
    if (sz < MIN_FONT_SIZE || sz > MAX_FONT_SIZE )
    {
        fclose(f);
        return 0;
    }
    *pfont = (lvfont_handle) malloc( sz );
    fseek( f, 0, SEEK_SET );
    fread( *pfont, sz, 1, f );
    fclose(f);
    tag_lvfont_header * hdr = (tag_lvfont_header *)lvfontGetHeader( *pfont );

    cnv.lsf( &hdr->fileSize );

    if (   hdr->fileSize != sz 
        || hdr->magic[0]!='L'
        || hdr->magic[1]!='F'
        || hdr->magic[2]!='N'
        || hdr->magic[3]!='T'
        || hdr->version[0]!='1'
        || hdr->version[1]!='.'
        || hdr->version[2]!='0'
        || hdr->version[3]!='0' )
    {
        free( *pfont );
        return 0;
    }
    if (cnv.msf()) {
        cnv.rev( &hdr->minCode );
        cnv.rev( &hdr->maxCode );
        cnv.rev( &hdr->decodeTableOffset );
        int ntables = (hdr->maxCode >> 6);
        for ( int i=0; i<ntables; i++ ) {
            cnv.rev( &hdr->rangesOffset[i] );
            int rangeoffset = hdr->rangesOffset[i];
            if ( rangeoffset<=0 || rangeoffset>(int)sz )
                continue;
            lvfont_range_t * range = (lvfont_range_t *)
                ( ((const char *)*pfont) + rangeoffset );

            for ( int j=0; j<64; j++ ) {
                cnv.rev( &range->glyphsOffset[j] );
                int glyphoffset = range->glyphsOffset[j];
                if ( glyphoffset<=0 || glyphoffset+rangeoffset>=(int)sz )
                    continue;
                lvfont_glyph_t * glyph =  (lvfont_glyph_t *)
                    ( ((const char *)range) + glyphoffset );
                cnv.rev( &glyph->glyphSize );
            }

        }
    }
    return 1;
}

void lvfontClose( lvfont_handle pfont )
{
    if (pfont)
    {
        free(pfont);
    }
}

lUInt16 lvfontMeasureText( const lvfont_handle pfont, 
                    const lChar16 * text, int len, 
                    lUInt16 * widths,
                    lUInt8 * flags,
                    int max_width,
                    lChar16 def_char
                 )
{
    lUInt16 wsum = 0;
    lUInt16 nchars = 0;
    const lvfont_glyph_t * glyph;
    lUInt16 gwidth = 0;
    lUInt16 hyphwidth = 0;
    lUInt8 bflags;
    int isSpace;
    lChar16 ch;
    int hwStart, hwEnd;

    glyph = lvfontGetGlyph( pfont, UNICODE_SOFT_HYPHEN_CODE );
    hyphwidth = glyph ? glyph->width : 0;

    for ( ; wsum < max_width && nchars < len; nchars++ ) 
    {
        bflags = 0;
        ch = text[nchars];
        isSpace = lvfontIsUnicodeSpace(ch);
        if (isSpace ||  ch == UNICODE_SOFT_HYPHEN_CODE )
            bflags |= LCHAR_ALLOW_WRAP_AFTER;
        if (ch == '-')
            bflags |= LCHAR_DEPRECATED_WRAP_AFTER;
        if (isSpace)
            bflags |= LCHAR_IS_SPACE;
        glyph = lvfontGetGlyph( pfont, ch );
        if (!glyph && def_char)
             glyph = lvfontGetGlyph( pfont, def_char );
        gwidth = glyph ? glyph->width : 0;
        widths[nchars] = wsum + gwidth;
        if ( ch != UNICODE_SOFT_HYPHEN_CODE ) 
            wsum += gwidth; /* don't include hyphens to width */
        flags[nchars] = bflags;
    }

#ifdef __cplusplus
    //try to add hyphen
    for (hwStart=nchars-1; hwStart>0; hwStart--)
    {
        if (lvfontIsUnicodeSpace(text[hwStart]))
        {
            hwStart++;
            break;
        }
    }
    for (hwEnd=nchars; hwEnd<len; hwEnd++)
    {
        lChar16 ch = text[hwEnd];
        if (lvfontIsUnicodeSpace(ch))
            break;
        if (flags[hwEnd-1]&LCHAR_ALLOW_WRAP_AFTER)
            break;
        if (ch=='.' || ch==',' || ch=='!' || ch=='?' || ch=='?')
            break;
        
    }
    HyphMan::hyphenate(text+hwStart, hwEnd-hwStart, widths+hwStart, flags+hwStart, hyphwidth, max_width);
#endif

    return nchars;
}

void lvfontUnpackGlyph( const lUInt8 * packed, const hrle_decode_info_t * table, lUInt8 * unpacked, int unp_size )
{
    lUInt16  b;
    hrle_decode_table_t code;
    int      srcshift;
    int      srcdata;
    int      srccount;
    int inx;

    srcshift = 0; //(srcskip & 3);
    srccount = 0;
    /* newline */
    lUInt8 * unp_end = unpacked + unp_size;
    for (;unpacked<unp_end;)
    {
        // read source
        b = (((lUInt16)(packed[0]))<<8) | (packed[1]);
        inx = (b >> (16 - table->bitcount - srcshift)) & table->rightmask;
        code = table->table[inx];
        srcdata = code.value << 6;
        srccount = code.count;
        srcshift += code.codelen;
        if (srcshift & 8)
        {
            srcshift &= 7;
            packed++;
        }
        // write to dest
        for (;srccount;srccount--)
            *unpacked++ = srcdata;
    }
}

