/** \file w32utils.cpp
    \brief misc windows utility functions

    CoolReader Engine


    (c) Vadim Lopatin, 2000-2006
    This source code is distributed under the terms of
    GNU General Public License.

    See LICENSE file for details.

*/

// lvfonttest.cpp

#include "../include/crsetup.h"
#if !defined(__SYMBIAN32__) && defined(_WIN32)

extern "C" {
#include <windows.h>
}
#include "../include/w32utils.h"
#include "../include/lvstream.h"


void DrawBuf2DC(HDC dc, int x, int y, LVDrawBuf * buf, COLORREF * palette, int scale )
{

    COLORREF * drawpixels;
    HDC drawdc;
    HBITMAP drawbmp;

    int buf_width = buf->GetWidth();
    int bytesPerRow = (buf_width * buf->GetBitsPerPixel() + 7) / 8;
    BITMAPINFO bmi;
    memset( &bmi, 0, sizeof(bmi) );
    bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
    bmi.bmiHeader.biWidth = buf_width * scale;
    bmi.bmiHeader.biHeight = 1;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    bmi.bmiHeader.biSizeImage = 0;
    bmi.bmiHeader.biXPelsPerMeter = 1024;
    bmi.bmiHeader.biYPelsPerMeter = 1024;
    bmi.bmiHeader.biClrUsed = 0;
    bmi.bmiHeader.biClrImportant = 0;

    drawbmp = CreateDIBSection( NULL, &bmi, DIB_RGB_COLORS, (void**)(&drawpixels), NULL, 0 );
    drawdc = CreateCompatibleDC(NULL);
    SelectObject(drawdc, drawbmp);

    
    int pixelsPerByte = (8 / buf->GetBitsPerPixel());
    int mask = (1<<buf->GetBitsPerPixel()) - 1;
    for (int yy=0; yy<buf->GetHeight(); yy++)
    {
       unsigned char * src = buf->GetScanLine(yy);
       for (int yyi = 0; yyi<scale; yyi++)
       {
          for (int xx=0; xx<bytesPerRow; xx++)
          {
             unsigned int b = src[xx];
             int x0 = 0;
             for (int shift = 8-buf->GetBitsPerPixel(); x0<pixelsPerByte; shift -= buf->GetBitsPerPixel(), x0++ )
             {
                 int dindex = (xx*pixelsPerByte + x0)*scale;
                 if ( dindex>=buf_width )
                      break;
                 COLORREF * px = drawpixels + dindex;
                 for (int xxi=0; xxi<scale; xxi++)
                 {
                     px[xxi] = palette[(b >> shift)&mask];
                 }
             }
          }
          BitBlt( dc, x, y+yy*scale+yyi, buf_width*scale, 1, drawdc, 0, 0, SRCCOPY );
       }
    }
    DeleteObject( drawbmp );
    DeleteObject( drawdc );
}

void SaveBitmapToFile( const char * fname, LVGrayDrawBuf * bmp )
{
    if (!bmp)
        return;
    LVStreamRef stream = LVOpenFileStream(fname, LVOM_WRITE);
    if (!stream)
        return;
    int rowsize = ((bmp->GetWidth()+1)/2);
    int img_size = rowsize * bmp->GetHeight();
    int padding = rowsize - rowsize;
    BITMAPFILEHEADER fh;
    struct {
        BITMAPINFOHEADER hdr;
        RGBQUAD colors[16];
    } bmi;
    memset(&fh, 0, sizeof(fh));
    memset(&bmi, 0, sizeof(bmi));
    fh.bfType = 0x4D42;
    fh.bfSize = sizeof(fh) + sizeof(bmi) + img_size;
    fh.bfOffBits = sizeof(fh) + sizeof(bmi);
    bmi.hdr.biSize = sizeof(bmi.hdr);
    bmi.hdr.biWidth = bmp->GetWidth();
    bmi.hdr.biHeight = bmp->GetHeight();
    bmi.hdr.biPlanes = 1;
    bmi.hdr.biBitCount = 4;
    bmi.hdr.biCompression = 0;
    bmi.hdr.biSizeImage = img_size;
    bmi.hdr.biXPelsPerMeter = 0xEC4;
    bmi.hdr.biYPelsPerMeter = 0xEC4;
    bmi.hdr.biClrUsed = 16;
    bmi.hdr.biClrImportant = 16;
    static lUInt8 gray[8] = { 0xFF, 0x00, 0x00, 0x00, 0xFF, 0xAA, 0x55, 0x00 };
    lUInt8 * pal = bmp->GetBitsPerPixel()==1?gray+0:gray+4;
    for (int i=0; i<4; i++)
    {
        bmi.colors[i].rgbBlue = pal[i];
        bmi.colors[i].rgbGreen = pal[i];
        bmi.colors[i].rgbRed = pal[i];
    }
    stream->Write( &fh, sizeof(fh), NULL );
    stream->Write( &bmi, sizeof(bmi), NULL );
    static const lUInt8 dummy[3] = {0,0,0};
    for (int y=0; y<bmp->GetHeight(); y++)
    {
        LVArray<lUInt8> row( (bmp->GetWidth()+1)/2, 0 );
        for ( int x=0; x<bmp->GetWidth(); x++)
        {
            int cl = bmp->GetPixel(x, bmp->GetHeight()-1-y);
            //int cl = (src[x/8] >> ((1-(x&3))*2)) & 3;
            row[x/2] = row[x/2] | (cl << ((x&1)?0:4));
        }
        row[0] = 0x11;
        row[1] = 0x11;
        row[2] = 0x22;
        row[3] = 0x22;
        row[4] = 0x33;
        row[5] = 0x33;
        *stream << row;
        if (padding)
            stream->Write( dummy, padding, NULL );
    }
}

#endif
