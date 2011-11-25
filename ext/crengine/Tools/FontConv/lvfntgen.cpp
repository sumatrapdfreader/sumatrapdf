/*******************************************************
              LV Bitmap font implementation
*******************************************************/

#include "StdAfx.h"
#include "lvfntgen.h"


 glyph_range_buf::glyph_range_buf()
     : pos(0)
 {
     memset( &range, 0, sizeof(range) );
     memset( buf, 0, sizeof(buf) );
 }

 lvfont_glyph_t * glyph_range_buf::addGlyph( unsigned short code )
 {
     range.glyphsOffset[code & 0x3F] = pos + sizeof(range);
     return (lvfont_glyph_t *) (buf+pos);
 }

 void glyph_range_buf::commitGlyph()
 {
     lvfont_glyph_t * glyph = (lvfont_glyph_t *)( buf+pos );
     pos += glyph->glyphSize + 8;
 }


 lvfont_glyph_t *  font_gen_buf::addGlyph( unsigned short code )
 {
     int nrange = (code >> 6) & 0x3FF;
     if (!ranges[nrange]) ranges[nrange] = new glyph_range_buf;
     lastRange = ranges[nrange];
     if ( hdr.maxCode < code )
         hdr.maxCode = code;
     return ranges[nrange]->addGlyph( code );
 }

 void font_gen_buf::commitGlyph()
 {
     if (lastRange)
     {
         lastRange->commitGlyph();
         lastRange = NULL;
     }
 }

 bool font_gen_buf::saveToFile( const char * fname )
 {
     FILE * f = fopen( fname, "wb" );
     if (!f)
         return false;
     int sz = 0;
     int rangecount = hdr.maxCode/64 + 1;
     int hdrsz = sizeof(lvfont_header_t) - (1024 - rangecount) * sizeof(unsigned long);
     for (int i=0; i<rangecount; i++)
     {
         if (ranges[i])
         {
             hdr.rangesOffset[i] = hdrsz + decodeTableSize + sz;
             sz += ranges[i]->getSize();
         }
     }
     hdr.fileSize = hdrsz + sz + decodeTableSize;
     hdr.decodeTableOffset = hdrsz;
     fwrite( &hdr, hdrsz, 1, f);
     fwrite( decodeTable, decodeTableSize, 1, f);
     int pos = hdrsz + decodeTableSize;
     for (i=0; i<1024; i++)
     {
         if (ranges[i])
         {
             //ranges[i]->relocate(pos);
             pos += ranges[i]->getSize();
             fwrite( &ranges[i]->range, ranges[i]->getSize(), 1, f );
         }
     }
     fclose(f);
     return true;
 }

 void font_gen_buf::init( int fntSize, int fntBaseline, int bitsPerPixel, const char * fontName, const char * fontCopyright )
 {
     memset( &hdr, 0, sizeof(hdr) );
     hdr.magic[0] = 'L';
     hdr.magic[1] = 'F';
     hdr.magic[2] = 'N';
     hdr.magic[3] = 'T';
     hdr.version[0] = '1';
     hdr.version[1] = '.';
     hdr.version[2] = '0';
     hdr.version[3] = '0';
     hdr.fontHeight = fntSize;
     hdr.fontBaseline = fntBaseline;
     hdr.fontBitsPerPixel = bitsPerPixel;
     hdr.fontMaxWidth = 0;
     hdr.minCode = 0;
     hdr.maxCode = 0;
     strncpy( hdr.fontName, fontName, FONT_NAME_LENGTH-1);
     hdr.fontName[FONT_NAME_LENGTH-1] = 0;
     strncpy( hdr.fontCopyright, fontCopyright, FONT_NAME_LENGTH-1);
     hdr.fontCopyright[FONT_NAME_LENGTH-1];
 }

 void font_gen_buf::setDecodeTable( hrle_decode_info_t * table )
 {
     if (decodeTable)
         free(decodeTable);
     decodeTableSize = sizeof(hrle_decode_info_t)+sizeof(hrle_decode_table_t)*(table->itemcount-1);
     decodeTable = (hrle_decode_info_t*)malloc(decodeTableSize);
     memcpy(decodeTable, table, decodeTableSize);
 }

 font_gen_buf::font_gen_buf()
 : lastRange(NULL), decodeTable(NULL), decodeTableSize(0)
 {
     memset( &hdr, 0, sizeof(hdr) );
     memset( &ranges, 0, sizeof(ranges) );
 }

 font_gen_buf::~font_gen_buf()
 {
     if (decodeTable)
         free(decodeTable);
     for (int i=0; i<1024; i++)
     {
         if (ranges[i]) delete ranges[i];
     }
 }
