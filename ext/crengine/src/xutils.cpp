/** \file xutils.cpp
    \brief misc X Window System utility functions

    CoolReader Engine


    (c) Vadim Lopatin, 2000-2006
    This source code is distributed under the terms of
    GNU General Public License.

    See LICENSE file for details.

*/

#ifdef LINUX

#include <X11/Xlib.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "../include/xutils.h"


//#define DRAW_BY_LINE


static XImage img_template = {
    1,1,        /* int width, height;         size of image (filled in by program)   */
    0,          /* int xoffset;               number of pixels offset in X direction */
    ZPixmap,    /* int format;                XYBitmap, XYPixmap, ZPixmap            */
    NULL,       /* char *data;                pointer to image data                  */
    LSBFirst,   /* int byte_order;            data byte order, LSBFirst, MSBFirst    */
    32,          /* int bitmap_unit;           quant. of scanline 8, 16, 32           */
    LSBFirst,   /* int bitmap_bit_order;      LSBFirst, MSBFirst                     */
    32,          /* int bitmap_pad;            8, 16, 32 either XY or ZPixmap         */
    24,         /* int depth;                 depth of image                         */
    0,          /* int bytes_per_line;        accelarator to next line               */
    32,         /* int bits_per_pixel;        bits per pixel (ZPixmap)               */
    0xff0000,   /* unsigned long red_mask;    bits in z arrangment                   */
    0x00ff00,   /* unsigned long green_mask;                                         */
    0x0000ff    /* unsigned long blue_mask;                                          */
};

MyXImage::MyXImage( int dx, int dy )
{
    _img = (XImage*) malloc( sizeof(XImage)+1024 );
    memcpy( _img, &img_template, sizeof(XImage) );
    _img->width = dx;
    _img->height = dy;
    _img->bytes_per_line = dx*4;
    _img->data = (char*) malloc( _img->bytes_per_line * dy );
    XInitImage( _img );
}    

MyXImage::~MyXImage()
{
    if (_img->data)
        free( _img->data );
    if (_img)
    	free( _img );	
}

unsigned * MyXImage::getScanLine( int y )
{
    assert ( y>=0 && y<_img->height );
    return ((unsigned*)_img->data)+_img->width*y;
}

void MyXImage::fill( unsigned pixel )
{
    unsigned * buf = ((unsigned*)_img->data);
    for (int i=_img->width*_img->height-1; i>=0; --i)
        buf[i] = pixel;
}



/// draw gray bitmap buffer to X drawable
void DrawBuf2Drawable(Display *display, Drawable d, GC gc, int x, int y, LVDrawBuf * buf, unsigned * palette, int scale )
{
    int pixelsPerByte = (8 / buf->GetBitsPerPixel());
    int bytesPerRow = (buf->GetWidth() * buf->GetBitsPerPixel() + 7) / 8;
    int mask = (1<<buf->GetBitsPerPixel()) - 1;
    int width = buf->GetWidth();
    int dwidth = buf->GetWidth()*scale;

#ifdef DRAW_BY_LINE

    MyXImage img( dwidth, 1 );
    unsigned * dest = img.getScanLine( 0 );

    for (int yy=0; yy<buf->GetHeight(); yy++)
    {
        unsigned char * src = buf->GetScanLine(yy);

        for (int yyi = 0; yyi<scale; yyi++)
        {
           if ( buf->GetBitsPerPixel()==2 )
           {
               for (int xx=0; xx<bytesPerRow; xx++)
               {
                  unsigned int b = src[xx];
                  int x0 = 0;
                  for (int shift = 8-buf->GetBitsPerPixel(); x0<pixelsPerByte; shift -= buf->GetBitsPerPixel(), x0++ )
                  {
                     int dindex = (xx*pixelsPerByte + x0)*scale;
                     if ( dindex>=dwidth )
                         break;
                     unsigned * px = dest + dindex;
                     for (int xxi=0; xxi<scale; xxi++)
                     {
                        px[xxi] = palette[(b >> shift)&mask];
                     }
                  }
               }
           } 
           else if ( buf->GetBitsPerPixel()==32 )
           {
               unsigned int * px = dest;
               for (int xx=0; xx<width; xx++)
               {
                  unsigned int b = ((unsigned int *)src)[xx];
                  b = ((b>>16)&255) | (b&0xFF00) | ((b&255)<<16);
                  for (int xxi=0; xxi<scale; xxi++)
                  {
                      *px++ = b;
                  }
               }
           } 
           XPutImage( display, d, gc, img.getXImage(), 0, 0, x, y + yy*scale+yyi, buf->GetWidth()*scale, 1 );
           XFlush( display );
       }
    }

#else //DRAW_BY_LINE
    // draw the whole image at once

    MyXImage img( buf->GetWidth()*scale, buf->GetHeight()*scale );

    for (int yy=0; yy<buf->GetHeight(); yy++)
    {
        unsigned char * src = buf->GetScanLine(yy);

        for (int yyi = 0; yyi<scale; yyi++)
        {
           unsigned * dest = img.getScanLine( yy*scale+yyi );
           if ( buf->GetBitsPerPixel()==2 )
           {
              for (int xx=0; xx<bytesPerRow; xx++)
              {
                 unsigned int b = src[xx];
                 int x0 = 0;
                 for (int shift = 8-buf->GetBitsPerPixel(); x0<pixelsPerByte; shift -= buf->GetBitsPerPixel(), x0++ )
                 {
                    int dindex = (xx*pixelsPerByte + x0)*scale;
                    if ( dindex>=dwidth )
                        break;
                    unsigned * px = dest + dindex;
                    for (int xxi=0; xxi<scale; xxi++)
                    {
                       px[xxi] = palette[(b >> shift)&mask];
                    }
                 }
              }
           }
           else if ( buf->GetBitsPerPixel()==32 )
           {
               unsigned int * px = dest;
               for (int xx=0; xx<width; xx++)
               {
                  unsigned int b = ((unsigned int *)src)[xx];
                  //b = ((b>>16)&255) | (b&0xFF00) | ((b&255)<<16);
                  for (int xxi=0; xxi<scale; xxi++)
                  {
                      *px++ = b;
                  }
               }
           } 
       }
    }
    XPutImage( display, d, gc, img.getXImage(), 0, 0, x, y, buf->GetWidth()*scale, buf->GetHeight()*scale );

#endif //DRAW_BY_LINE
}

#endif
