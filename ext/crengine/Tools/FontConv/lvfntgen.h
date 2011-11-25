/*******************************************************
              LV Bitmap font interface
*******************************************************/

#ifndef __LVFNTGEN_H_INCLUDED__
#define __LVFNTGEN_H_INCLUDED__

#include "../../include/lvfnt.h"

struct glyph_range_buf
{
    tag_lvfont_range range;
    unsigned char buf[0x10000];
    int pos;
    glyph_range_buf();

    int getSize() { return (sizeof(range) + pos + 7)/8*8; }

    lvfont_glyph_t * addGlyph( unsigned short code );

    void commitGlyph();

    bool isEmpty() { return pos==0; }
};


struct font_gen_buf
{
    lvfont_header_t   hdr;
    glyph_range_buf * ranges[1024];
    glyph_range_buf * lastRange;
    hrle_decode_info_t * decodeTable;
    int decodeTableSize;

    lvfont_glyph_t *  addGlyph( unsigned short code );
    void commitGlyph();
    void setDecodeTable( hrle_decode_info_t * table );

    bool saveToFile( const char * fname );
    void init( int fntSize, int fntBaseline, int bitsPerPixel, const char * fontName, const char * fontCopyright );
    font_gen_buf();
    ~font_gen_buf();
};

#endif
