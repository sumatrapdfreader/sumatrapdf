/*******************************************************

   CoolReader Engine C-compatible API

   lvbmpbuf.cpp:  Gray bitmap buffer

   (c) Vadim Lopatin, 2000-2006
   This source code is distributed under the terms of
   GNU General Public License
   See LICENSE file for details

*******************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "../include/lvbmpbuf.h"

void lvdrawbufInit( draw_buf_t * buf, int bitsPerPixel, int width, int height, lUInt8 * data )
{
   int pixelsPerByte = (8 / bitsPerPixel);
   buf->data = data;
   buf->height = height;
   buf->bitsPerPixel = bitsPerPixel;
   buf->bytesPerRow = (width + (pixelsPerByte-1))/pixelsPerByte;
}

void lvdrawbufAlloc( draw_buf_t * buf, int bitsPerPixel, int width, int height )
{
   int pixelsPerByte = (8 / bitsPerPixel);
   buf->height = height;
   buf->bitsPerPixel = bitsPerPixel;
   buf->bytesPerRow = (width + (pixelsPerByte-1))/pixelsPerByte;
   buf->data = (lUInt8 *) malloc(buf->bytesPerRow*height);
}

void lvdrawbufFree( draw_buf_t * buf )
{
   buf->height = 0;
   buf->bitsPerPixel = 0;
   buf->bytesPerRow = 0;
   if (buf->data)
      free(buf->data);
   buf->data = NULL;
}

void lvdrawbufFill( draw_buf_t * buf, lUInt8 pixel )
{
   int sz = buf->height * buf->bytesPerRow;
   memset( buf->data, pixel, sz );
}

/* Fill rectangle with specified data */
void lvdrawbufFillRect( draw_buf_t * buf, int x0, int y0, int x1, int y1, unsigned char pixel )
{
    int width = buf->bytesPerRow << 2;
    if ( y0<0 )
        y0 = 0;
    if ( x0<0 )
        x0 = 0;
    if ( x1>=width )
        x1 = width - 1;
    if ( y1>=buf->height )
        y1 = buf->height - 1;
    if ( x0 >= x1 || y0 >= y1 )
        return;
    for ( int y=y0; y<y1; y++)
    {
        lUInt8 * pLine = buf->data + buf->bytesPerRow * y;
        for ( int x=x0; x<x1; x++)
        {
            lUInt8 * p = pLine + (x >> 2);
            int inx = (x & 3) << 1;
            lUInt8 mask = 0xC0 >> inx;
            *p = (*p & ~mask) | ( pixel << (6-inx) );
        }
    }
}

void lvdrawbufDraw2( draw_buf_t * buf, int x, int y, const lUInt8 * bitmap, int width, int height )
{
    int buf_width = buf->bytesPerRow << 2; /* 2bpp */
    int bx = 0;
    int by = 0;
    int xx;
    int bmp_width = width;
    lUInt8 * dst;
    lUInt8 * dstline;
    int      shift, shift0;
    int      srcshift;
    int      srcskip;
    int      srcdata;
    int      srccount;

    if (x<0)
    {
        width += x;
        bx -= x;
        x = 0;
        if (width<=0)
            return;
    }
    if (y<0)
    {
        height += y;
        by -= y;
        y = 0;
        if (height<=0)
            return;
    }
    if (x + width > buf_width)
    {
        width = buf_width - x;
    }
    if (width<=0)
        return;
    if (y + height > buf->height)
    {
        height = buf->height - y;
    }
    if (height<=0)
        return;
    dstline = buf->data + buf->bytesPerRow*y + (x >> 2);
    dst = dstline;
    shift0 = (x & 3);
    xx = width;
    srcskip = by*bmp_width + bx;
    bitmap += srcskip >> 2;
    srcshift = (srcskip & 3);
    srcskip = 0;
    shift = shift0;
    /* newline */
    for (;;)
    {
        /* skip source pixels if necessary */
        if (srcskip)
        {
            bitmap += (srcskip + srcshift) >> 2;
            srcshift = (srcskip + srcshift) & 3;
            srcskip = 0;
        }
        srcdata = ((*bitmap)<<(srcshift<<1)) & 0xC0;
        srccount = 1;
        if (!(++srcshift & 3))
        {
            srcshift = 0;
            bitmap++;
        }
        *dst |= srcdata >> (shift<<1);
        /* next pixel */
        if (!(++shift & 3))
        {
            shift = 0;
            dst++;
        }
        if ( --xx == 0 )
        {
            if ( --height == 0 )
                break;
            /* new dest line */
            dstline += buf->bytesPerRow;
            dst = dstline;
            shift = shift0;
            xx = width;
            srcskip = bmp_width - width;
        }
    }
}

void lvdrawbufDrawUnpacked( draw_buf_t * buf, int x, int y, const lUInt8 * bitmap, int width, int height )
{
    int buf_width = buf->bytesPerRow << 2; /* 2bpp */
    int bx = 0;
    int by = 0;
    int xx;
    int bmp_width = width;
    lUInt8 * dst;
    lUInt8 * dstline;
    const lUInt8 * src;
    int      shift, shift0;

    if (x<0)
    {
        width += x;
        bx -= x;
        x = 0;
        if (width<=0)
            return;
    }
    if (y<0)
    {
        height += y;
        by -= y;
        y = 0;
        if (height<=0)
            return;
    }
    if (x + width > buf_width)
    {
        width = buf_width - x;
    }
    if (width<=0)
        return;
    if (y + height > buf->height)
    {
        height = buf->height - y;
    }
    if (height<=0)
        return;

    dstline = buf->data + buf->bytesPerRow*y + (x >> 2);
    dst = dstline;
    shift0 = (x & 3);
    xx = width;

    bitmap += bx + by*bmp_width;
    shift = shift0;

    for (;height;height--)
    {
        src = bitmap;

        for (xx = width; xx>0; --xx)
        {
            *dst |= (*src++) >> (shift<<1);
            /* next pixel */
            if (!(++shift & 3))
            {
                shift = 0;
                dst++;
            }
        }
        /* new dest line */
        bitmap += bmp_width;
        dstline += buf->bytesPerRow;
        dst = dstline;
        shift = shift0;
    }
}

void lvdrawbufDrawPacked( draw_buf_t * buf, int x, int y, const lUInt8 * bitmap, int width, int height, const hrle_decode_info_t * table )
{
    int buf_width = buf->bytesPerRow << 2; /* 2bpp */
    int bx = 0;
    int by = 0;
    int xx;
    int bmp_width = width;
    lUInt16  b;
    lUInt8 * dst;
    lUInt8 * dstline;
    hrle_decode_table_t code;
    int      shift, shift0;
    int      srcshift;
    int      srcskip;
    int      srcdata = 0;
    int      srccount;
    int      inx;

    if (x<0)
    {
        width += x;
        bx -= x;
        x = 0;
        if (width<=0)
            return;
    }
    if (y<0)
    {
        height += y;
        by -= y;
        y = 0;
        if (height<=0)
            return;
    }
    if (x + width > buf_width)
    {
        width = buf_width - x;
    }
    if (width<=0)
        return;
    if (y + height > buf->height)
    {
        height = buf->height - y;
    }
    if (height<=0)
        return;
    dstline = buf->data + buf->bytesPerRow*y + (x >> 2);
    dst = dstline;
    shift0 = (x & 3);
    xx = width;
    srcskip = by*bmp_width + bx;
    //bitmap += srcskip >> 2;
    srcshift = 0; //(srcskip & 3);
    srcskip = 0;
    shift = shift0;
    srccount = 0;
    /* newline */
    for (;;)
    {
        /* read source symbols */
        if (!srccount)
        {
            b = (((lUInt16)(bitmap[0]))<<8) | (bitmap[1]);
            inx = (b >> (16 - table->bitcount - srcshift)) & table->rightmask;
            code = table->table[inx];
            srcdata = code.value << 6;
            srccount = code.count;
            srcshift += code.codelen;
            if (srcshift & 8)
            {
                srcshift &= 7;
                bitmap++;
            }
        }
        /* skip source pixels if necessary */
        if (srcskip)
        {
            if (srcskip>=srccount)
            {
                srcskip -= srccount;
                srccount = 0;
                continue;
            }
            srccount -= srcskip;
        }

        *dst |= srcdata >> (shift<<1);
        --srccount;

        /* next pixel */
        if (!(++shift & 3))
        {
            shift = 0;
            dst++;
        }
        if ( --xx == 0 )
        {
            if ( --height == 0 )
                break;
            /* new dest line */
            dstline += buf->bytesPerRow;
            dst = dstline;
            shift = shift0;
            xx = width;
            srcskip = bmp_width - width;
        }
    }
}

void lvdrawbufDraw( draw_buf_t * buf, int x, int y, const lUInt8 * bitmap, int numRows, int bytesPerRow )
{
   int pixelsPerByte = (8 / buf->bitsPerPixel);
   int x0 = x / pixelsPerByte;
   int dx = x % pixelsPerByte;
   int shift = dx * buf->bitsPerPixel;
   const lUInt8 * src;
   lUInt8 * dst;
   lUInt32 b;
   for (int yy=0; yy<numRows; yy++)
   {
      if (y+yy>=0 && y+yy<buf->height)
      {
         dst = buf->data + buf->bytesPerRow * (y+yy) + x0;
         src = bitmap + bytesPerRow * yy;
         for (int xx=0; xx<bytesPerRow; xx++)
         {
            if (xx+x0>=0 && xx+x0<buf->bytesPerRow)
            {
               b = (((lUInt32)src[xx]) << (8-shift));
               dst[xx] |= (lUInt8)( (b >> 8) & 0xFF );
               if ( xx+x0+1<buf->bytesPerRow )
                  dst[xx+1] |= (lUInt8)( (b) & 0xFF );
            }
         }
      }
   }
}

/*
   Draw text string into buffer (logical OR)
   x, y: coordinates where to draw
   text: string to draw
   len: number of chars from text to draw
*/
/*
void lvdrawbufDrawText( draw_buf_t * buf, int x, int y, const lvfont_handle pfont, 
                       const lChar16 * text, int len, lChar16 def_char )
{
   const lvfont_glyph_t * glyph;
   int baseline = lvfontGetHeader( pfont )->fontBaseline;
   const hrle_decode_info_t * pDecodeTable = lvfontGetDecodeTable( pfont );
   while (len)
   {
      if (len==1 || *text != UNICODE_SOFT_HYPHEN_CODE)
      {
          glyph = lvfontGetGlyph( pfont, *text );
          if (!glyph)
              glyph = lvfontGetGlyph( pfont, def_char ); // substitute char 
          if (glyph)
          {
              //lvdrawbufDraw( buf, x + glyph->originX, y + baseline - glyph->originY, glyph->glyph, glyph->blackBoxY, glyph->rowBytes );
              lvdrawbufDrawPacked( buf, x + glyph->originX, 
                  y + baseline - glyph->originY, glyph->glyph, 
                  glyph->blackBoxX, glyph->blackBoxY, pDecodeTable );
              //lvdrawbufDraw2( buf, x + glyph->originX, y + baseline - glyph->originY, glyph->glyph, glyph->blackBoxX, glyph->blackBoxY );
              x += glyph->width;
          }
      }
      else if (*text != UNICODE_SOFT_HYPHEN_CODE)
      {
          len = len;
      }
      len--;
      text++;
   }
}
*/
void lvdrawbufDrawText( draw_buf_t * buf, int x, int y, const lvfont_handle pfont, 
                       const lChar16 * text, int len, lChar16 def_char )
{
    static lUInt8 glyph_buf[16384];
    const lvfont_glyph_t * glyph;
    int baseline = lvfontGetHeader( pfont )->fontBaseline;
    const hrle_decode_info_t * pDecodeTable = lvfontGetDecodeTable( pfont );
    while (len)
    {
      if (len==1 || *text != UNICODE_SOFT_HYPHEN_CODE)
      {
          glyph = lvfontGetGlyph( pfont, *text );
          if (!glyph)
              glyph = lvfontGetGlyph( pfont, def_char ); /* substitute char */
          if (glyph)
          {
              lvfontUnpackGlyph( glyph->glyph, pDecodeTable, glyph_buf, glyph->blackBoxX*glyph->blackBoxY );
              lvdrawbufDrawUnpacked( buf, x + glyph->originX,
                  y + baseline - glyph->originY, glyph_buf,
                  glyph->blackBoxX, glyph->blackBoxY );
              x += glyph->width;
          }
      }
      else if (*text != UNICODE_SOFT_HYPHEN_CODE)
      {
          len = len;
      }
      len--;
      text++;
    }
}

