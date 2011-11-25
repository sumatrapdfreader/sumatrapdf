// lvfonttest.cpp

#include "stdafx.h"
#include "lvfonttest.h"

void DrawBuf2DC(CDC & dc, int x, int y, draw_buf_t * buf, COLORREF * palette, int scale )
{

    COLORREF * m_drawpixels;
    CDC m_drawdc;
    CBitmap m_drawbmp;


    int buf_width = buf->bytesPerRow*(8 / buf->bitsPerPixel);
   BITMAPINFO bmi;
   memset( &bmi, 0, sizeof(bmi) );
   bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
   bmi.bmiHeader.biWidth = buf_width * scale;
   bmi.bmiHeader.biHeight = buf->height * scale;
   bmi.bmiHeader.biPlanes = 1;
   bmi.bmiHeader.biBitCount = 32;
   bmi.bmiHeader.biCompression = BI_RGB;
   bmi.bmiHeader.biSizeImage = 0;
   bmi.bmiHeader.biXPelsPerMeter = 1024;
   bmi.bmiHeader.biYPelsPerMeter = 1024;
   bmi.bmiHeader.biClrUsed = 0;
   bmi.bmiHeader.biClrImportant = 0;

   HBITMAP hbmp = CreateDIBSection( NULL, &bmi, DIB_RGB_COLORS, (void**)(&m_drawpixels), NULL, 0 );
   m_drawbmp.Attach(hbmp);
   m_drawdc.CreateCompatibleDC(NULL);
   m_drawdc.SelectObject(&m_drawbmp);

    
   int pixelsPerByte = (8 / buf->bitsPerPixel);
   int mask = (1<<buf->bitsPerPixel) - 1;
   for (int yy=0; yy<buf->height; yy++)
   {
      unsigned char * src = buf->data + yy*buf->bytesPerRow;
      for (int yyi = 0; yyi<scale; yyi++)
      {
         COLORREF * dst = m_drawpixels + ( (buf->height*scale-(yy*scale + yyi)-1) )*buf_width*scale;
         for (int xx=0; xx<buf->bytesPerRow; xx++)
         {
            unsigned int b = src[xx];
            int x0 = 0;
            for (int shift = 8-buf->bitsPerPixel; x0<pixelsPerByte; shift -= buf->bitsPerPixel, x0++ )
            {
               COLORREF * px = dst + (xx*pixelsPerByte + x0)*scale;
               for (int xxi=0; xxi<scale; xxi++)
               {
                  px[xxi] = palette[(b >> shift)&mask];
               }
            }
         }
      }
   }
   dc.BitBlt( x, y, buf_width*scale, buf->height*scale, &m_drawdc, 0, 0, SRCCOPY );
}

