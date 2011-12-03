/*******************************************************

   CoolReader Engine 

   lvdrawbuf.cpp:  Gray bitmap buffer class

   (c) Vadim Lopatin, 2000-2006
   This source code is distributed under the terms of
   GNU General Public License

   See LICENSE file for details

*******************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "../include/lvdrawbuf.h"

void LVDrawBuf::RoundRect( int x0, int y0, int x1, int y1, int borderWidth, int radius, lUInt32 color, int cornerFlags )
{
    FillRect( x0 + ((cornerFlags&1)?radius:0), y0, x1-1-((cornerFlags&2)?radius:0), y0+borderWidth, color );
    FillRect( x0, y0 + ((cornerFlags&1)?radius:0), x0+borderWidth, y1-1-((cornerFlags&4)?radius:0), color );
    FillRect( x1-borderWidth, y0 + ((cornerFlags&2)?radius:0), x1, y1-((cornerFlags&8)?radius:0), color );
    FillRect( x0 + ((cornerFlags&4)?radius:0), y1-borderWidth, x1-((cornerFlags&8)?radius:0), y1, color );
    // TODO: draw rounded corners
}

static lUInt32 rgbToGray( lUInt32 color )
{
    lUInt32 r = (0xFF0000 & color) >> 16;
    lUInt32 g = (0x00FF00 & color) >> 8;
    lUInt32 b = (0x0000FF & color) >> 0;
    return ((r + g + g + b)>>2) & 0xFF;
}

static lUInt8 rgbToGray( lUInt32 color, int bpp )
{
    lUInt32 r = (0xFF0000 & color) >> 16;
    lUInt32 g = (0x00FF00 & color) >> 8;
    lUInt32 b = (0x0000FF & color) >> 0;
    return (lUInt8)(((r + g + g + b)>>2) & (((1<<bpp)-1)<<(8-bpp)));
}

static lUInt16 rgb565(int r, int g, int b) {
	// rrrr rggg gggb bbbb
	return (lUInt16)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | ((g & 0xF8) >> 3));
}

static lUInt8 rgbToGrayMask( lUInt32 color, int bpp )
{
    switch ( bpp ) {
    case DRAW_BUF_1_BPP:
        color = rgbToGray(color) >> 7;
        color = (color&1) ? 0xFF : 0x00;
        break;
    case DRAW_BUF_2_BPP:
        color = rgbToGray(color) >> 6;
        color &= 3;
        color |= (color << 2) | (color << 4) | (color << 6);
        break;
    default:
    //case DRAW_BUF_3_BPP: // 8 colors
    //case DRAW_BUF_4_BPP: // 16 colors
    //case DRAW_BUF_8_BPP: // 256 colors
        // return 8 bit as is
        color = rgbToGray(color);
        color &= ((1<<bpp)-1)<<(8-bpp);
        return (lUInt8)color;
    }
    return (lUInt8)color;
}

static void ApplyAlphaRGB( lUInt32 &dst, lUInt32 src, lUInt32 alpha )
{
    if ( alpha==0 )
        dst = src;
    else if ( alpha<255 ) {
        src &= 0xFFFFFF;
        lUInt32 opaque = 256 - alpha;
        lUInt32 n1 = (((dst & 0xFF00FF) * alpha + (src & 0xFF00FF) * opaque) >> 8) & 0xFF00FF;
        lUInt32 n2 = (((dst & 0x00FF00) * alpha + (src & 0x00FF00) * opaque) >> 8) & 0x00FF00;
        dst = n1 | n2;
    }
}

static void ApplyAlphaRGB565( lUInt16 &dst, lUInt16 src, lUInt32 alpha )
{
    if ( alpha==0 )
        dst = src;
    else if ( alpha<255 ) {
        lUInt32 opaque = 256 - alpha;
        lUInt32 r = (((dst & 0xF800) * alpha + (src & 0xF800) * opaque) >> 8) & 0xF800;
        lUInt32 g = (((dst & 0x07E0) * alpha + (src & 0x7E00) * opaque) >> 8) & 0x07E0;
        lUInt32 b = (((dst & 0x0018) * alpha + (src & 0x0018) * opaque) >> 8) & 0x0018;
        dst = (lUInt16)(r | g | b);
    }
}

static void ApplyAlphaGray( lUInt8 &dst, lUInt8 src, lUInt32 alpha, int bpp )
{
    if ( alpha==0 )
        dst = src;
    else if ( alpha<255 ) {
        int mask = ((1<<bpp)-1) << (8-bpp);
        src &= mask;
        lUInt32 opaque = 256 - alpha;
        lUInt32 n1 = ((dst * alpha + src * opaque)>>8 ) & mask;
        dst = (lUInt8)n1;
    }
}

static const short dither_2bpp_4x4[] = {
    5, 13,  8,  16,
    9,  1,  12,  4,
    7, 15,  6,  14,
    11, 3,  10,  2,
};

static const short dither_2bpp_8x8[] = {
0, 32, 12, 44, 2, 34, 14, 46, 
48, 16, 60, 28, 50, 18, 62, 30, 
8, 40, 4, 36, 10, 42, 6, 38, 
56, 24, 52, 20, 58, 26, 54, 22, 
3, 35, 15, 47, 1, 33, 13, 45, 
51, 19, 63, 31, 49, 17, 61, 29, 
11, 43, 7, 39, 9, 41, 5, 37, 
59, 27, 55, 23, 57, 25, 53, 21, 
};

// returns byte with higher significant bits, lower bits are 0
lUInt32 DitherNBitColor( lUInt32 color, lUInt32 x, lUInt32 y, int bits )
{
    int mask = ((1<<bits)-1)<<(8-bits);
    // gray = (r + 2*g + b)>>2
    //int cl = ((((color>>16) & 255) + ((color>>(8-1)) & (255<<1)) + ((color) & 255)) >> 2) & 255;
    int cl = ((((color>>16) & 255) + ((color>>(8-1)) & (255<<1)) + ((color) & 255)) >> 2) & 255;
    int white = (1<<bits) - 1;
    int precision = white;
    if (cl<precision)
        return 0;
    else if (cl>=255-precision)
        return mask;
    //int d = dither_2bpp_4x4[(x&3) | ( (y&3) << 2 )] - 1;
    // dither = 0..63
    int d = dither_2bpp_8x8[(x&7) | ( (y&7) << 3 )] - 1;
    int shift = bits-2;
    cl = ( (cl<<shift) + d - 32 ) >> shift;
    if ( cl>255 )
        cl = 255;
    if ( cl<0 )
        cl = 0;
    return cl & mask;
}

lUInt32 Dither2BitColor( lUInt32 color, lUInt32 x, lUInt32 y )
{
    int cl = ((((color>>16) & 255) + ((color>>8) & 255) + ((color) & 255)) * (256/3)) >> 8;
    if (cl<5)
        return 0;
    else if (cl>=250)
        return 3;
    //int d = dither_2bpp_4x4[(x&3) | ( (y&3) << 2 )] - 1;
    int d = dither_2bpp_8x8[(x&7) | ( (y&7) << 3 )] - 1;

    cl = ( cl + d - 32 );
    if (cl<5)
        return 0;
    else if (cl>=250)
        return 3;
    return (cl >> 6) & 3;
}

lUInt32 Dither1BitColor( lUInt32 color, lUInt32 x, lUInt32 y )
{
    int cl = ((((color>>16) & 255) + ((color>>8) & 255) + ((color) & 255)) * (256/3)) >> 8;
    if (cl<16)
        return 0;
    else if (cl>=240)
        return 1;
    //int d = dither_2bpp_4x4[(x&3) | ( (y&3) << 2 )] - 1;
    int d = dither_2bpp_8x8[(x&7) | ( (y&7) << 3 )] - 1;

    cl = ( cl + d - 32 );
    if (cl<5)
        return 0;
    else if (cl>=250)
        return 1;
    return (cl >> 7) & 1;
}

static lUInt8 revByteBits1( lUInt8 b )
{
    return ( (b&1)<<7 )
        |  ( (b&2)<<5 )
        |  ( (b&4)<<3 )
        |  ( (b&8)<<1 )
        |  ( (b&16)>>1 )
        |  ( (b&32)>>3 )
        |  ( (b&64)>>4 )
        |  ( (b&128)>>5 );
}

lUInt8 revByteBits2( lUInt8 b )
{
    return ( (b&0x03)<<6 )
        |  ( (b&0x0C)<<2 )
        |  ( (b&0x30)>>2 )
        |  ( (b&0xC0)>>6 );
}

/// rotates buffer contents by specified angle
void LVGrayDrawBuf::Rotate( cr_rotate_angle_t angle )
{
    if ( angle==CR_ROTATE_ANGLE_0 )
        return;
    int sz = (_rowsize * _dy);
    if ( angle==CR_ROTATE_ANGLE_180 ) {
        if ( _bpp==DRAW_BUF_1_BPP ) {
            for ( int i=sz/2-1; i>=0; i-- ) {
                lUInt8 tmp = revByteBits1( _data[i] );
                _data[i] = revByteBits1( _data[sz-i-1] );
                _data[sz-i-1] = tmp;
            }
        } else if ( _bpp==DRAW_BUF_2_BPP ) {
            for ( int i=sz/2-1; i>=0; i-- ) {
                lUInt8 tmp = revByteBits2( _data[i] );
                _data[i] = revByteBits2( _data[sz-i-1] );
                _data[sz-i-1] = tmp;
            }
        } else { // DRAW_BUF_3_BPP, DRAW_BUF_4_BPP, DRAW_BUF_8_BPP
            lUInt8 * buf = (lUInt8 *) _data;
            for ( int i=sz/2-1; i>=0; i-- ) {
                lUInt8 tmp = buf[i];
                buf[i] = buf[sz-i-1];
                buf[sz-i-1] = tmp;
            }
        }
        return;
    }
    int newrowsize = _bpp<=2 ? (_dy * _bpp + 7) / 8 : _dy;
    sz = (newrowsize * _dx);
    lUInt8 * dst = (lUInt8 *)malloc(sz);
    memset( dst, 0, sz );
    for ( int y=0; y<_dy; y++ ) {
        lUInt8 * src = _data + _rowsize*y;
        int dstx, dsty;
        for ( int x=0; x<_dx; x++ ) {
            if ( angle==CR_ROTATE_ANGLE_90 ) {
                dstx = _dy-1-y;
                dsty = x;
            } else {
                dstx = y;
                dsty = _dx-1-x;
            }
            if ( _bpp==DRAW_BUF_1_BPP ) {
                lUInt8 px = (src[ x >> 3 ] << (x&7)) & 0x80;
                lUInt8 * dstrow = dst + newrowsize * dsty;
                dstrow[ dstx >> 3 ] |= (px >> (dstx&7));
            } else if (_bpp==DRAW_BUF_2_BPP ) {
                lUInt8 px = (src[ x >> 2 ] << ((x&3)<<1)) & 0xC0;
                lUInt8 * dstrow = dst + newrowsize * dsty;
                dstrow[ dstx >> 2 ] |= (px >> ((dstx&3)<<1));
            } else { // DRAW_BUF_3_BPP, DRAW_BUF_4_BPP, DRAW_BUF_8_BPP
                lUInt8 * dstrow = dst + newrowsize * dsty;
                dstrow[ dstx ] = src[ x ];
            }
        }
    }
    free( _data );
    _data = dst;
    int tmp = _dx;
    _dx = _dy;
    _dy = tmp;
    _rowsize = newrowsize;
}

/// rotates buffer contents by specified angle
void LVColorDrawBuf::Rotate( cr_rotate_angle_t angle )
{
    if ( angle==CR_ROTATE_ANGLE_0 )
        return;
    if ( _bpp==16 ) {
        int sz = (_dx * _dy);
        if ( angle==CR_ROTATE_ANGLE_180 ) {
            lUInt16 * buf = (lUInt16 *) _data;
            for ( int i=sz/2-1; i>=0; i-- ) {
                lUInt16 tmp = buf[i];
                buf[i] = buf[sz-i-1];
                buf[sz-i-1] = tmp;
            }
            return;
        }
        int newrowsize = _dy * 2;
        sz = (_dx * newrowsize);
        lUInt16 * dst = (lUInt16*) malloc( sz );
    #if !defined(__SYMBIAN32__) && defined(_WIN32)
        bool cw = angle!=CR_ROTATE_ANGLE_90;
    #else
        bool cw = angle==CR_ROTATE_ANGLE_90;
    #endif
        for ( int y=0; y<_dy; y++ ) {
            lUInt16 * src = (lUInt16*)_data + _dx*y;
            int nx, ny;
            if ( cw ) {
                nx = _dy - 1 - y;
            } else {
                nx = y;
            }
            for ( int x=0; x<_dx; x++ ) {
                if ( cw ) {
                    ny = x;
                } else {
                    ny = _dx - 1 - x;
                }
                dst[ _dy*ny + nx ] = src[ x ];
            }
        }
    #if !defined(__SYMBIAN32__) && defined(_WIN32)
        memcpy( _data, dst, sz );
        free( dst );
    #else
        free( _data );
        _data = (lUInt8*)dst;
    #endif
        int tmp = _dx;
        _dx = _dy;
        _dy = tmp;
        _rowsize = newrowsize;
    } else {
        int sz = (_dx * _dy);
        if ( angle==CR_ROTATE_ANGLE_180 ) {
            lUInt32 * buf = (lUInt32 *) _data;
            for ( int i=sz/2-1; i>=0; i-- ) {
                lUInt32 tmp = buf[i];
                buf[i] = buf[sz-i-1];
                buf[sz-i-1] = tmp;
            }
            return;
        }
        int newrowsize = _dy * 4;
        sz = (_dx * newrowsize);
        lUInt32 * dst = (lUInt32*) malloc( sz );
    #if !defined(__SYMBIAN32__) && defined(_WIN32)
        bool cw = angle!=CR_ROTATE_ANGLE_90;
    #else
        bool cw = angle==CR_ROTATE_ANGLE_90;
    #endif
        for ( int y=0; y<_dy; y++ ) {
            lUInt32 * src = (lUInt32*)_data + _dx*y;
            int nx, ny;
            if ( cw ) {
                nx = _dy - 1 - y;
            } else {
                nx = y;
            }
            for ( int x=0; x<_dx; x++ ) {
                if ( cw ) {
                    ny = x;
                } else {
                    ny = _dx - 1 - x;
                }
                dst[ _dy*ny + nx ] = src[ x ];
            }
        }
    #if !defined(__SYMBIAN32__) && defined(_WIN32)
        memcpy( _data, dst, sz );
        free( dst );
    #else
        free( _data );
        _data = (lUInt8*)dst;
    #endif
        int tmp = _dx;
        _dx = _dy;
        _dy = tmp;
        _rowsize = newrowsize;
    }
}

class LVImageScaledDrawCallback : public LVImageDecoderCallback
{
private:
    LVImageSourceRef src;
    LVBaseDrawBuf * dst;
    int dst_x;
    int dst_y;
    int dst_dx;
    int dst_dy;
    int src_dx;
    int src_dy;
    int * xmap;
    int * ymap;
    bool dither;
public:
    static int * GenMap( int src_len, int dst_len )
    {
        int  * map = new int[ dst_len ];
        for (int i=0; i<dst_len; i++)
        {
            map[ i ] = i * src_len / dst_len;
        }
        return map;
    }
    LVImageScaledDrawCallback(LVBaseDrawBuf * dstbuf, LVImageSourceRef img, int x, int y, int width, int height, bool dith )
    : src(img), dst(dstbuf), dst_x(x), dst_y(y), dst_dx(width), dst_dy(height), xmap(0), ymap(0), dither(dith)
    {
        src_dx = img->GetWidth();
        src_dy = img->GetHeight();
        if ( src_dx != dst_dx )
            xmap = GenMap( src_dx, dst_dx );
        if ( src_dy != dst_dy )
            ymap = GenMap( src_dy, dst_dy );
    }
    virtual ~LVImageScaledDrawCallback()
    {
        if (xmap)
            delete[] xmap;
        if (ymap)
            delete[] ymap;
    }
    virtual void OnStartDecode( LVImageSource * )
    {
    }
    virtual bool OnLineDecoded( LVImageSource *, int y, lUInt32 * data )
    {
        //fprintf( stderr, "l_%d ", y );
        int yy = y;
        int yy2 = y+1;
        if ( ymap ) 
        {
            int yy0 = (y - 1) * dst_dy / src_dy;
            yy = y * dst_dy / src_dy;
            yy2 = (y+1) * dst_dy / src_dy;
            if ( yy == yy0 )
            {
                //fprintf( stderr, "skip_dup " );
                //return true; // skip duplicate drawing
            }
            if ( yy2 > dst_dy )
                yy2 = dst_dy;
        }
        lvRect clip;
        dst->GetClipRect( &clip );
        for ( ;yy<yy2; yy++ )
        {
            if ( yy+dst_y<clip.top || yy+dst_y>=clip.bottom )
                continue;
            int bpp = dst->GetBitsPerPixel();
            if ( bpp >= 24 )
            {
                lUInt32 * row = (lUInt32 *)dst->GetScanLine( yy + dst_y );
                row += dst_x;
                for (int x=0; x<dst_dx; x++)
                {
                    lUInt32 cl = data[xmap ? xmap[x] : x];
                    int xx = x + dst_x;
                    lUInt32 alpha = (cl >> 24)&0xFF;
                    if ( xx<clip.left || xx>=clip.right || alpha==0xFF )
                        continue;
                    if ( !alpha )
                        row[ x ] = cl;
                    else
                        ApplyAlphaRGB( row[x], cl, alpha );
                }
            }
            else if ( bpp == 16 )
            {
                lUInt16 * row = (lUInt16 *)dst->GetScanLine( yy + dst_y );
                row += dst_x;
                for (int x=0; x<dst_dx; x++)
                {
                    lUInt32 cl = data[xmap ? xmap[x] : x];
                    int xx = x + dst_x;
                    lUInt32 alpha = (cl >> 24)&0xFF;
                    if ( xx<clip.left || xx>=clip.right || alpha==0xFF )
                        continue;
                    if ( alpha<16 ) {
                        row[ x ] = rgb888to565( cl );
                    } else if (alpha<0xF0) {
                        lUInt32 v = rgb565to888(row[x]);
                        ApplyAlphaRGB( v, cl, alpha );
                        row[x] = rgb888to565(v);
                    }
                }
            }
            else if ( bpp > 2 ) // 3,4,8 bpp
            {
                lUInt8 * row = (lUInt8 *)dst->GetScanLine( yy + dst_y );
                row += dst_x;
                for (int x=0; x<dst_dx; x++)
                {
                    lUInt32 cl = data[xmap ? xmap[x] : x];
                    int xx = x + dst_x;
                    lUInt32 alpha = (cl >> 24)&0xFF;
                    if ( xx<clip.left || xx>=clip.right || alpha==0xFF )
                        continue;
                    if ( alpha ) {
                        lUInt32 origColor = row[x];
                        if ( bpp==3 ) {
                            origColor = origColor & 0xE0;
                            origColor = origColor | (origColor>>3) | (origColor>>6);
                        } else {
                            origColor = origColor & 0xF0;
                            origColor = origColor | (origColor>>4);
                        }
                        origColor = origColor | (origColor<<8) | (origColor<<16);
                        ApplyAlphaRGB( origColor, cl, alpha );
                        cl = origColor;
                    }

                    lUInt8 dcl;
                    if ( dither && bpp < 8) {
#if (GRAY_INVERSE==1)
                        dcl = (lUInt8)DitherNBitColor( cl^0xFFFFFF, x, yy, bpp );
#else
                        dcl = (lUInt8)DitherNBitColor( cl, x, yy, bpp );
#endif
                    } else {
                        dcl = rgbToGray( cl, bpp );
                    }
                    row[ x ] = dcl;
                    // ApplyAlphaGray( row[x], dcl, alpha, bpp );
                }
            }
            else if ( bpp == 2 )
            {
                //fprintf( stderr, "." );
                lUInt8 * row = (lUInt8 *)dst->GetScanLine( yy+dst_y );
                //row += dst_x;
                for (int x=0; x<dst_dx; x++)
                {
                    lUInt32 cl = data[xmap ? xmap[x] : x];
                    int xx = x + dst_x;
                    lUInt32 alpha = (cl >> 24)&0xFF;
                    if ( xx<clip.left || xx>=clip.right || alpha==0xFF )
                        continue;

                    int byteindex = (xx >> 2);
                    int bitindex = (3-(xx & 3))<<1;
                    lUInt8 mask = 0xC0 >> (6 - bitindex);

                    if ( alpha ) {
                        lUInt32 origColor = (row[ byteindex ] & mask)>>bitindex;
                        origColor = origColor | (origColor<<2);
                        origColor = origColor | (origColor<<4);
                        origColor = origColor | (origColor<<8) | (origColor<<16);
                        ApplyAlphaRGB( origColor, cl, alpha );
                        cl = origColor;
                    }

                    lUInt32 dcl = 0;
                    if ( dither ) {
#if (GRAY_INVERSE==1)
                        dcl = Dither2BitColor( cl, x, yy ) ^ 3;
#else
                        dcl = Dither2BitColor( cl, x, yy );
#endif
                    } else {
                        dcl = rgbToGrayMask( cl, 2 ) & 3;
                    }
                    dcl = dcl << bitindex;
                    row[ byteindex ] = (lUInt8)((row[ byteindex ] & (~mask)) | dcl);
                }
            }
            else if ( bpp == 1 )
            {
                //fprintf( stderr, "." );
                lUInt8 * row = (lUInt8 *)dst->GetScanLine( yy+dst_y );
                //row += dst_x;
                for (int x=0; x<dst_dx; x++)
                {
                    lUInt32 cl = data[xmap ? xmap[x] : x];
                    int xx = x + dst_x;
                    lUInt32 alpha = (cl >> 24)&0xFF;
                    if ( xx<clip.left || xx>=clip.right || alpha&0x80 )
                        continue;
                    lUInt32 dcl = 0;
                    if ( dither ) {
#if (GRAY_INVERSE==1)
                        dcl = Dither1BitColor( cl, x, yy ) ^ 1;
#else
                        dcl = Dither1BitColor( cl, x, yy ) ^ 0;
#endif
                    } else {
                        dcl = rgbToGrayMask( cl, 1 ) & 1;
                    }
                    int byteindex = (xx >> 3);
                    int bitindex = ((xx & 7));
                    lUInt8 mask = 0x80 >> (bitindex);
                    dcl = dcl << (7-bitindex);
                    row[ byteindex ] = (lUInt8)((row[ byteindex ] & (~mask)) | dcl);
                }
            }
            else
            {
                return false;
            }
        }
        return true;
    }
    virtual void OnEndDecode( LVImageSource *, bool )
    {
    }
};


int  LVBaseDrawBuf::GetWidth()
{ 
    return _dx;
}

int  LVBaseDrawBuf::GetHeight()
{ 
    return _dy; 
}

int  LVGrayDrawBuf::GetBitsPerPixel()
{ 
    return _bpp;
}

void LVGrayDrawBuf::Draw( LVImageSourceRef img, int x, int y, int width, int height, bool dither )
{
    //fprintf( stderr, "LVGrayDrawBuf::Draw( img(%d, %d), %d, %d, %d, %d\n", img->GetWidth(), img->GetHeight(), x, y, width, height );
    if ( width<=0 || height<=0 )
        return;
    LVImageScaledDrawCallback drawcb( this, img, x, y, width, height, dither );
    img->Decode( &drawcb );
}


/// get pixel value
lUInt32 LVGrayDrawBuf::GetPixel( int x, int y )
{
    if (x<0 || y<0 || x>=_dx || y>=_dy)
        return 0;
    lUInt8 * line = GetScanLine(y);
    if (_bpp==1) {
        // 1bpp
        if ( line[x>>3] & (0x80>>(x&7)) )
            return 1;
        else
            return 0;
    } else if (_bpp==2) {
        return (line[x>>2] >> (6-((x&3)<<1))) & 3;
    } else { // 3, 4, 8
        return line[x];
    }
}

void LVGrayDrawBuf::Clear( lUInt32 color )
{
    color = rgbToGrayMask( color, _bpp );
#if (GRAY_INVERSE==1)
    color ^= 0xFF;
#endif
    memset( _data, color, _rowsize * _dy );
//    for (int i = _rowsize * _dy - 1; i>0; i--)
//    {
//        _data[i] = (lUInt8)color;
//    }
    SetClipRect( NULL );
}

void LVGrayDrawBuf::FillRect( int x0, int y0, int x1, int y1, lUInt32 color32 )
{
    if (x0<_clip.left)
        x0 = _clip.left;
    if (y0<_clip.top)
        y0 = _clip.top;
    if (x1>_clip.right)
        x1 = _clip.right;
    if (y1>_clip.bottom)
        y1 = _clip.bottom;
    if (x0>=x1 || y0>=y1)
        return;
    lUInt8 color = rgbToGrayMask( color32, _bpp );
#if (GRAY_INVERSE==1)
    color ^= 0xFF;
#endif
    lUInt8 * line = GetScanLine(y0);
    for (int y=y0; y<y1; y++)
    {
        if (_bpp==1) {
            for (int x=x0; x<x1; x++)
            {
                lUInt8 mask = 0x80 >> (x&7);
                int index = x >> 3;
                line[index] = (lUInt8)((line[index] & ~mask) | (color & mask));
            }
        } else if (_bpp==2) {
            for (int x=x0; x<x1; x++)
            {
                lUInt8 mask = 0xC0 >> ((x&3)<<1);
                int index = x >> 2;
                line[index] = (lUInt8)((line[index] & ~mask) | (color & mask));
            }
        } else { // 3, 4, 8
            for (int x=x0; x<x1; x++)
                line[x] = color;
        }
        line += _rowsize;
    }
}

void LVGrayDrawBuf::FillRectPattern( int x0, int y0, int x1, int y1, lUInt32 color032, lUInt32 color132, lUInt8 * pattern )
{
    if (x0<_clip.left)
        x0 = _clip.left;
    if (y0<_clip.top)
        y0 = _clip.top;
    if (x1>_clip.right)
        x1 = _clip.right;
    if (y1>_clip.bottom)
        y1 = _clip.bottom;
    if (x0>=x1 || y0>=y1)
        return;
    lUInt8 color0 = rgbToGrayMask( color032, _bpp );
    lUInt8 color1 = rgbToGrayMask( color132, _bpp );
    lUInt8 * line = GetScanLine(y0);
    for (int y=y0; y<y1; y++)
    {
        lUInt8 patternMask = pattern[y & 3];
        if (_bpp==1) {
            for (int x=x0; x<x1; x++)
            {
                lUInt8 patternBit = (patternMask << (x&7)) & 0x80;
                lUInt8 mask = 0x80 >> (x&7);
                int index = x >> 3;
                line[index] = (lUInt8)((line[index] & ~mask) | ((patternBit?color1:color0) & mask));
            }
        } else if (_bpp==2) {
            for (int x=x0; x<x1; x++)
            {
                lUInt8 patternBit = (patternMask << (x&7)) & 0x80;
                lUInt8 mask = 0xC0 >> ((x&3)<<1);
                int index = x >> 2;
                line[index] = (lUInt8)((line[index] & ~mask) | ((patternBit?color1:color0) & mask));
            }
        } else {
            for (int x=x0; x<x1; x++)
            {
                lUInt8 patternBit = (patternMask << (x&7)) & 0x80;
                line[x] = patternBit ? color1 : color0;
            }
        }
        line += _rowsize;
    }
}

static const lUInt8 fill_masks1[5] = {0x00, 0x3, 0x0f, 0x3f, 0xff};
static const lUInt8 fill_masks2[4] = {0x00, 0xc0, 0xf0, 0xfc};

#define INVERT_PRSERVE_GRAYS

#ifdef INVERT_PRSERVE_GRAYS
static const lUInt8 inverted_bytes[] = {
    0xff, 0xfd, 0xfe, 0xfc, 0xf7, 0xf5, 0xf6, 0xf4, 0xfb, 0xf9, 0xfa, 0xf8, 0xf3, 0xf1,
    0xf2, 0xf0, 0xdf, 0xdd, 0xde, 0xdc, 0xd7, 0xd5, 0xd6, 0xd4, 0xdb, 0xd9, 0xda, 0xd8,
    0xd3, 0xd1, 0xd2, 0xd0, 0xef, 0xed, 0xee, 0xec, 0xe7, 0xe5, 0xe6, 0xe4, 0xeb, 0xe9,
    0xea, 0xe8, 0xe3, 0xe1, 0xe2, 0xe0, 0xcf, 0xcd, 0xce, 0xcc, 0xc7, 0xc5, 0xc6, 0xc4,
    0xcb, 0xc9, 0xca, 0xc8, 0xc3, 0xc1, 0xc2, 0xc0, 0x7f, 0x7d, 0x7e, 0x7c, 0x77, 0x75,
    0x76, 0x74, 0x7b, 0x79, 0x7a, 0x78, 0x73, 0x71, 0x72, 0x70, 0x5f, 0x5d, 0x5e, 0x5c,
    0x57, 0x55, 0x56, 0x54, 0x5b, 0x59, 0x5a, 0x58, 0x53, 0x51, 0x52, 0x50, 0x6f, 0x6d,
    0x6e, 0x6c, 0x67, 0x65, 0x66, 0x64, 0x6b, 0x69, 0x6a, 0x68, 0x63, 0x61, 0x62, 0x60,
    0x4f, 0x4d, 0x4e, 0x4c, 0x47, 0x45, 0x46, 0x44, 0x4b, 0x49, 0x4a, 0x48, 0x43, 0x41,
    0x42, 0x40, 0xbf, 0xbd, 0xbe, 0xbc, 0xb7, 0xb5, 0xb6, 0xb4, 0xbb, 0xb9, 0xba, 0xb8,
    0xb3, 0xb1, 0xb2, 0xb0, 0x9f, 0x9d, 0x9e, 0x9c, 0x97, 0x95, 0x96, 0x94, 0x9b, 0x99,
    0x9a, 0x98, 0x93, 0x91, 0x92, 0x90, 0xaf, 0xad, 0xae, 0xac, 0xa7, 0xa5, 0xa6, 0xa4,
    0xab, 0xa9, 0xaa, 0xa8, 0xa3, 0xa1, 0xa2, 0xa0, 0x8f, 0x8d, 0x8e, 0x8c, 0x87, 0x85,
    0x86, 0x84, 0x8b, 0x89, 0x8a, 0x88, 0x83, 0x81, 0x82, 0x80, 0x3f, 0x3d, 0x3e, 0x3c,
    0x37, 0x35, 0x36, 0x34, 0x3b, 0x39, 0x3a, 0x38, 0x33, 0x31, 0x32, 0x30, 0x1f, 0x1d,
    0x1e, 0x1c, 0x17, 0x15, 0x16, 0x14, 0x1b, 0x19, 0x1a, 0x18, 0x13, 0x11, 0x12, 0x10,
    0x2f, 0x2d, 0x2e, 0x2c, 0x27, 0x25, 0x26, 0x24, 0x2b, 0x29, 0x2a, 0x28, 0x23, 0x21,
    0x22, 0x20, 0xf, 0xd, 0xe, 0xc, 0x7, 0x5, 0x6, 0x4, 0xb, 0x9, 0xa, 0x8,
    0x3, 0x1, 0x2, 0x0
};
#define GET_INVERTED_BYTE(x) inverted_bytes[x]
#else
#define GET_INVERTED_BYTE(x) ~(x)
#endif
void LVGrayDrawBuf::InvertRect(int x0, int y0, int x1, int y1)
{
    if (x0<_clip.left)
        x0 = _clip.left;
    if (y0<_clip.top)
        y0 = _clip.top;
    if (x1>_clip.right)
        x1 = _clip.right;
    if (y1>_clip.bottom)
        y1 = _clip.bottom;
    if (x0>=x1 || y0>=y1)
        return;

	if (_bpp==1) {
		; //TODO: implement for 1 bit
	} else if (_bpp==2) {
                lUInt8 * line = GetScanLine(y0) + (x0 >> 2);
		lUInt16 before = 4 - (x0 & 3); // number of pixels before byte boundary
		if (before == 4)
			before = 0;
		lUInt16 w = (x1 - x0 - before);
		lUInt16 after  = (w & 3); // number of pixels after byte boundary
		w >>= 2;
		before = fill_masks1[before];
		after = fill_masks2[after];
		for (int y = y0; y < y1; y++) {
			lUInt8 *dst  = line;
			if (before) {
				lUInt8 color = GET_INVERTED_BYTE(dst[0]);
				dst[0] = ((dst[0] & ~before) | (color & before));
				dst++;
			}
			for (int x = 0; x < w; x++) {
				dst[x] = GET_INVERTED_BYTE(dst[x]);
			}
			dst += w;
			if (after) {
				lUInt8 color = GET_INVERTED_BYTE(dst[0]);
				dst[0] = ((dst[0] & ~after) | (color & after));
			}
			line += _rowsize;
		}
        }
#if 0
        else if (_bpp == 4) { // 3, 4, 8
            lUInt8 * line = GetScanLine(y0);
            for (int y=y0; y<y1; y++) {
                for (int x=x0; x<x1; x++) {
                    lUInt8 value = line[x];
                    if (value == 0 || value == 0xF0)
                        line[x] = ~value;
                }
                line += _rowsize;
            }
        }
#endif
        else { // 3, 4, 8
            lUInt8 * line = GetScanLine(y0);
            for (int y=y0; y<y1; y++) {
                for (int x=x0; x<x1; x++)
                    line[x] = ~line[x];
                line += _rowsize;
            }
        }
}

void LVGrayDrawBuf::Resize( int dx, int dy )
{
    _dx = dx;
    _dy = dy;
    _rowsize = _bpp<=2 ? (_dx * _bpp + 7) / 8 : _dx;
    if ( !_ownData ) {
        _data = NULL;
        _ownData = false;
    }
    if ( dx && dy )
    {
        _data = cr_realloc(_data, _rowsize * _dy);
    }
    else if (_data)
    {
        free(_data);
        _data = NULL;
    }
    Clear(0);
    SetClipRect( NULL );
}

/// returns white pixel value
lUInt32 LVGrayDrawBuf::GetWhiteColor()
{
    return 0xFFFFFF;
    /*
#if (GRAY_INVERSE==1)
    return 0;
#else
    return (1<<_bpp) - 1;
#endif
    */
}
/// returns black pixel value
lUInt32 LVGrayDrawBuf::GetBlackColor()
{
    return 0;
    /*
#if (GRAY_INVERSE==1)
    return (1<<_bpp) - 1;
#else
    return 0;
#endif
    */
}

LVGrayDrawBuf::LVGrayDrawBuf(int dx, int dy, int bpp, void * auxdata )
    : LVBaseDrawBuf(), _bpp(bpp), _ownData(true)
{
    _dx = dx;
    _dy = dy;
    _bpp = bpp;
    _rowsize = (bpp<=2) ? (_dx * _bpp + 7) / 8 : _dx;

    _backgroundColor = GetWhiteColor();
    _textColor = GetBlackColor();

    if ( auxdata ) {
        _data = (lUInt8 *) auxdata;
        _ownData = false;
    } else if (_dx && _dy) {
        _data = (lUInt8 *) malloc(_rowsize * _dy);
        Clear(0);
    }
    SetClipRect( NULL );
}

LVGrayDrawBuf::~LVGrayDrawBuf()
{
    if (_data && _ownData )
        free( _data );
}

void LVGrayDrawBuf::Draw( int x, int y, const lUInt8 * bitmap, int width, int height, lUInt32 * )
{
    //int buf_width = _dx; /* 2bpp */
    int initial_height = height;
    int bx = 0;
    int by = 0;
    int xx;
    int bmp_width = width;
    lUInt8 * dst;
    lUInt8 * dstline;
    const lUInt8 * src;
    int      shift, shift0;

    if (x<_clip.left)
    {
        width += x-_clip.left;
        bx -= x-_clip.left;
        x = _clip.left;
        if (width<=0)
            return;
    }
    if (y<_clip.top)
    {
        height += y-_clip.top;
        by -= y-_clip.top;
        y = _clip.top;
        if (_hidePartialGlyphs && height<=initial_height/2) // HIDE PARTIAL VISIBLE GLYPHS
            return;
        if (height<=0)
            return;
    }
    if (x + width > _clip.right)
    {
        width = _clip.right - x;
    }
    if (width<=0)
        return;
    if (y + height > _clip.bottom)
    {
        if (_hidePartialGlyphs && height<=initial_height/2) // HIDE PARTIAL VISIBLE GLYPHS
            return;
        int clip_bottom = _clip.bottom;
        if ( _hidePartialGlyphs )
            clip_bottom = this->_dy;
        if ( y+height > clip_bottom)
            height = clip_bottom - y;
    }
    if (height<=0)
        return;

    int bytesPerRow = _rowsize;
    if ( _bpp==2 ) {
        dstline = _data + bytesPerRow*y + (x >> 2);
        shift0 = (x & 3);
    } else if (_bpp==1) {
        dstline = _data + bytesPerRow*y + (x >> 3);
        shift0 = (x & 7);
    } else {
        dstline = _data + bytesPerRow*y + x;
        shift0 = 0;// not used
    }
    dst = dstline;
    xx = width;

    bitmap += bx + by*bmp_width;
    shift = shift0;


    lUInt8 color = rgbToGrayMask(GetTextColor(), _bpp);
//    bool white = (color & 0x80) ?
//#if (GRAY_INVERSE==1)
//            false : true;
//#else
//            true : false;
//#endif
    for (;height;height--)
    {
        src = bitmap;

        if ( _bpp==2 ) {
            // foreground color
            lUInt8 cl = (lUInt8)(rgbToGray(GetTextColor()) >> 6); // 0..3
            //cl ^= 0x03;
            for (xx = width; xx>0; --xx)
            {
                lUInt8 opaque = (*src >> 4) & 0x0F; // 0..15
                if ( opaque>0x3 ) {
                    int shift2 = shift<<1;
                    int shift2i = 6-shift2;
                    lUInt8 mask = 0xC0 >> shift2;
                    lUInt8 dstcolor;
                    if ( opaque>=0xC ) {
                        dstcolor = cl;
                    } else {
                        lUInt8 bgcolor = ((*dst)>>shift2i)&3; // 0..3
                        dstcolor = ((opaque*cl + (15-opaque)*bgcolor)>>4)&3;
                    }
                    *dst = (*dst & ~mask) | (dstcolor<<shift2i);
                }
                src++;
                /* next pixel */
                if (!(++shift & 3))
                {
                    shift = 0;
                    dst++;
                }
            }
        } else if ( _bpp==1 ) {
            for (xx = width; xx>0; --xx)
            {
#if (GRAY_INVERSE==1)
                *dst |= (( (*src++) & 0x80 ) >> ( shift ));
#else
                *dst &= ~(( ((*src++) & 0x80) ) >> ( shift ));
#endif
                /* next pixel */
                if (!(++shift & 7))
                {
                    shift = 0;
                    dst++;
                }
            }
        } else { // 3,4,8
            int mask = ((1<<_bpp)-1)<<(8-_bpp);
            for (xx = width; xx>0; --xx)
            {
                lUInt8 b = (*src++);
                if ( b ) {
                    if ( b>=mask )
                        *dst = color;
                    else {
                        int alpha = 256 - b;
                        ApplyAlphaGray( *dst, color, alpha, _bpp );
                    }
                }
                dst++;
            }
        }
        /* new dest line */
        bitmap += bmp_width;
        dstline += bytesPerRow;
        dst = dstline;
        shift = shift0;
    }
}

void LVBaseDrawBuf::SetClipRect( const lvRect * clipRect )
{
    if (clipRect)
    {
        _clip = *clipRect;
        if (_clip.left<0)
            _clip.left = 0;
        if (_clip.top<0)
            _clip.top = 0;
        if (_clip.right>_dx)
            _clip.right = _dx;
        if (_clip.bottom > _dy)
            _clip.bottom = _dy;
    }
    else
    {
        _clip.top = 0;
        _clip.left = 0;
        _clip.right = _dx;
        _clip.bottom = _dy;
    }
}

lUInt8 * LVGrayDrawBuf::GetScanLine( int y )
{
    return _data + _rowsize*y;
}

void LVGrayDrawBuf::Invert()
{
    int sz = _rowsize * _dy;
    for (int i=sz-1; i>=0; i--)
        _data[i] = ~_data[i];
}

void LVGrayDrawBuf::ConvertToBitmap(bool flgDither)
{
    if (_bpp==1)
        return;
    // TODO: implement for byte per pixel mode
    int sz = GetRowSize();
    lUInt8 * bitmap = (lUInt8*) malloc( sizeof(lUInt8) * sz );
    memset( bitmap, 0, sz );
    if (flgDither)
    {
        static const lUInt8 cmap[4][4] = {
            { 0, 0, 0, 0},
            { 0, 0, 1, 0},
            { 0, 1, 0, 1},
            { 1, 1, 1, 1},
        };
        for (int y=0; y<_dy; y++)
        {
            lUInt8 * src = GetScanLine(y);
            lUInt8 * dst = bitmap + ((_dx+7)/8)*y;
            for (int x=0; x<_dx; x++) {
                int cl = (src[x>>2] >> (6-((x&3)*2)))&3;
                cl = cmap[cl][ (x&1) + ((y&1)<<1) ];
                if (cmap[cl][ (x&1) + ((y&1)<<1) ])
                    dst[x>>3] |= 0x80>>(x&7);
            }
        }
    }
    else
    {
        for (int y=0; y<_dy; y++)
        {
            lUInt8 * src = GetScanLine(y);
            lUInt8 * dst = bitmap + ((_dx+7)/8)*y;
            for (int x=0; x<_dx; x++) {
                int cl = (src[x>>2] >> (7-((x&3)*2)))&1;
                if (cl)
                    dst[x>>3] |= 0x80>>(x&7);
            }
        }
    }
    free( _data );
    _data = bitmap;
    _bpp = 1;
    _rowsize = (_dx+7)/8;
}

//=======================================================
// 32-bit RGB buffer
//=======================================================

/// invert image
void  LVColorDrawBuf::Invert()
{
}

/// get buffer bits per pixel
int  LVColorDrawBuf::GetBitsPerPixel()
{
    return _bpp;
}

void LVColorDrawBuf::Draw( LVImageSourceRef img, int x, int y, int width, int height, bool dither )
{
    //fprintf( stderr, "LVColorDrawBuf::Draw( img(%d, %d), %d, %d, %d, %d\n", img->GetWidth(), img->GetHeight(), x, y, width, height );
    LVImageScaledDrawCallback drawcb( this, img, x, y, width, height, dither );
    img->Decode( &drawcb );
}

/// fills buffer with specified color
void LVColorDrawBuf::Clear( lUInt32 color )
{
    if ( _bpp==16 ) {
        lUInt16 cl16 = rgb888to565(color);
        for (int y=0; y<_dy; y++)
        {
            lUInt16 * line = (lUInt16 *)GetScanLine(y);
            for (int x=0; x<_dx; x++)
            {
                line[x] = cl16;
            }
        }
    } else {
        for (int y=0; y<_dy; y++)
        {
            lUInt32 * line = (lUInt32 *)GetScanLine(y);
            for (int x=0; x<_dx; x++)
            {
                line[x] = color;
            }
        }
    }
}


/// get pixel value
lUInt32 LVColorDrawBuf::GetPixel( int x, int y )
{
    if (!_data || y<0 || x<0 || y>=_dy || x>=_dx)
        return 0;
    if ( _bpp==16 )
        return ((lUInt16*)GetScanLine(y))[x];
    return ((lUInt32*)GetScanLine(y))[x];
}

/// fills rectangle with specified color
void LVColorDrawBuf::FillRect( int x0, int y0, int x1, int y1, lUInt32 color )
{
    if (x0<_clip.left)
        x0 = _clip.left;
    if (y0<_clip.top)
        y0 = _clip.top;
    if (x1>_clip.right)
        x1 = _clip.right;
    if (y1>_clip.bottom)
        y1 = _clip.bottom;
    if (x0>=x1 || y0>=y1)
        return;
    int alpha = (color >> 24) & 0xFF;
    if ( _bpp==16 ) {
        lUInt16 cl16 = rgb888to565(color);
        for (int y=y0; y<y1; y++)
        {
            lUInt16 * line = (lUInt16 *)GetScanLine(y);
            for (int x=x0; x<x1; x++)
            {
                if (alpha)
                    ApplyAlphaRGB565(line[x], color, alpha);
                else
                    line[x] = cl16;
            }
        }
    } else {
        for (int y=y0; y<y1; y++)
        {
            lUInt32 * line = (lUInt32 *)GetScanLine(y);
            for (int x=x0; x<x1; x++)
            {
                if (alpha)
                    ApplyAlphaRGB(line[x], color, alpha);
                else
                    line[x] = color;
            }
        }
    }
}

/// fills rectangle with specified color
void LVColorDrawBuf::FillRectPattern( int x0, int y0, int x1, int y1, lUInt32 color0, lUInt32 color1, lUInt8 * pattern )
{
    if (x0<_clip.left)
        x0 = _clip.left;
    if (y0<_clip.top)
        y0 = _clip.top;
    if (x1>_clip.right)
        x1 = _clip.right;
    if (y1>_clip.bottom)
        y1 = _clip.bottom;
    if (x0>=x1 || y0>=y1)
        return;
    if ( _bpp==16 ) {
        lUInt16 cl16_0 = rgb888to565(color0);
        lUInt16 cl16_1 = rgb888to565(color1);
        for (int y=y0; y<y1; y++)
        {
            lUInt8 patternMask = pattern[y & 3];
            lUInt16 * line = (lUInt16 *)GetScanLine(y);
            for (int x=x0; x<x1; x++)
            {
                lUInt8 patternBit = (patternMask << (x&7)) & 0x80;
                line[x] = patternBit ? cl16_1 : cl16_0;
            }
        }
    } else {
        for (int y=y0; y<y1; y++)
        {
            lUInt8 patternMask = pattern[y & 3];
            lUInt32 * line = (lUInt32 *)GetScanLine(y);
            for (int x=x0; x<x1; x++)
            {
                lUInt8 patternBit = (patternMask << (x&7)) & 0x80;
                line[x] = patternBit ? color1 : color0;
            }
        }
    }
}

/// sets new size
void LVColorDrawBuf::Resize( int dx, int dy )
{
    if ( dx==_dx && dy==_dy ) {
    	//CRLog::trace("LVColorDrawBuf::Resize : no resize, not changed");
    	return;
    }
    if ( !_ownData ) {
    	//CRLog::trace("LVColorDrawBuf::Resize : no resize, own data");
        return;
    }
    //CRLog::trace("LVColorDrawBuf::Resize : resizing %d x %d to %d x %d", _dx, _dy, dx, dy);
    // delete old bitmap
    if ( _dx>0 && _dy>0 && _data )
    {
#if !defined(__SYMBIAN32__) && defined(_WIN32)
        if (_drawbmp)
            DeleteObject( _drawbmp );
        if (_drawdc)
            DeleteObject( _drawdc );
        _drawbmp = NULL;
        _drawdc = NULL;
#else
        free(_data);
#endif
        _data = NULL;
        _rowsize = 0;
        _dx = 0;
        _dy = 0;
    }
    
    if (dx>0 && dy>0) 
    {
        // create new bitmap
        _dx = dx;
        _dy = dy;
        _rowsize = dx*(_bpp>>3);
#if !defined(__SYMBIAN32__) && defined(_WIN32)
        BITMAPINFO bmi;
        memset( &bmi, 0, sizeof(bmi) );
        bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
        bmi.bmiHeader.biWidth = _dx;
        bmi.bmiHeader.biHeight = _dy;
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;
        bmi.bmiHeader.biSizeImage = 0;
        bmi.bmiHeader.biXPelsPerMeter = 1024;
        bmi.bmiHeader.biYPelsPerMeter = 1024;
        bmi.bmiHeader.biClrUsed = 0;
        bmi.bmiHeader.biClrImportant = 0;
    
        _drawbmp = CreateDIBSection( NULL, &bmi, DIB_RGB_COLORS, (void**)(&_data), NULL, 0 );
        _drawdc = CreateCompatibleDC(NULL);
        SelectObject(_drawdc, _drawbmp);
#else
        _data = (lUInt8 *)malloc( (_bpp>>3) * _dx * _dy);
#endif
        memset( _data, 0, _rowsize * _dy );
    }
    SetClipRect( NULL );
}
void LVColorDrawBuf::InvertRect(int x0, int y0, int x1, int y1)
{
	
}

/// draws bitmap (1 byte per pixel) using specified palette
void LVColorDrawBuf::Draw( int x, int y, const lUInt8 * bitmap, int width, int height, lUInt32 * palette )
{
    //int buf_width = _dx; /* 2bpp */
    int initial_height = height;
    int bx = 0;
    int by = 0;
    int xx;
    int bmp_width = width;
    lUInt32 bmpcl = palette?palette[0]:GetTextColor();
    const lUInt8 * src;

    if (x<_clip.left)
    {
        width += x-_clip.left;
        bx -= x-_clip.left;
        x = _clip.left;
        if (width<=0)
            return;
    }
    if (y<_clip.top)
    {
        height += y-_clip.top;
        by -= y-_clip.top;
        y = _clip.top;
        if (_hidePartialGlyphs && height<=initial_height/2) // HIDE PARTIAL VISIBLE GLYPHS
            return;
        if (height<=0)
            return;
    }
    if (x + width > _clip.right)
    {
        width = _clip.right - x;
    }
    if (width<=0)
        return;
    if (y + height > _clip.bottom)
    {
        if (_hidePartialGlyphs && height<=initial_height/2) // HIDE PARTIAL VISIBLE GLYPHS
            return;
        int clip_bottom = _clip.bottom;
        if (_hidePartialGlyphs )
            clip_bottom = this->_dy;
        if ( y+height > clip_bottom)
            height = clip_bottom - y;
    }
    if (height<=0)
        return;

    xx = width;

    bitmap += bx + by*bmp_width;

    if ( _bpp==16 ) {

        lUInt16 bmpcl16 = rgb888to565(bmpcl);

        lUInt16 * dst;
        lUInt16 * dstline;


        for (;height;height--)
        {
            src = bitmap;
            dstline = ((lUInt16*)GetScanLine(y++)) + x;
            dst = dstline;

            for (xx = width; xx>0; --xx)
            {
                lUInt32 opaque = ((*(src++))>>4)&0x0F;
                if ( opaque>=0xF )
                    *dst = bmpcl16;
                else if ( opaque>0 ) {
                    lUInt32 alpha = 0xF-opaque;
                    lUInt16 cl1 = (lUInt16)(((alpha*((*dst)&0xF81F) + opaque*(bmpcl16&0xF81F))>>4) & 0xF81F);
                    lUInt16 cl2 = (lUInt16)(((alpha*((*dst)&0x07E0) + opaque*(bmpcl16&0x07E0))>>4) & 0x07E0);
                    *dst = cl1 | cl2;
                }
                /* next pixel */
                dst++;
            }
            /* new dest line */
            bitmap += bmp_width;
        }

    } else {


        lUInt32 * dst;
        lUInt32 * dstline;


        for (;height;height--)
        {
            src = bitmap;
            dstline = ((lUInt32*)GetScanLine(y++)) + x;
            dst = dstline;

            for (xx = width; xx>0; --xx)
            {
                lUInt32 opaque = ((*(src++))>>1)&0x7F;
                if ( opaque>=0x78 )
                    *dst = bmpcl;
                else if ( opaque>0 ) {
                    lUInt32 alpha = 0x7F-opaque;
                    lUInt32 cl1 = ((alpha*((*dst)&0xFF00FF) + opaque*(bmpcl&0xFF00FF))>>7) & 0xFF00FF;
                    lUInt32 cl2 = ((alpha*((*dst)&0x00FF00) + opaque*(bmpcl&0x00FF00))>>7) & 0x00FF00;
                    *dst = cl1 | cl2;
                }
                /* next pixel */
                dst++;
            }
            /* new dest line */
            bitmap += bmp_width;
        }
    }
}

#if !defined(__SYMBIAN32__) && defined(_WIN32)
/// draws buffer content to DC doing color conversion if necessary
void LVGrayDrawBuf::DrawTo( HDC dc, int x, int y, int options, lUInt32 * palette )
{
    if (!dc || !_data)
        return;
    LVColorDrawBuf buf( _dx, 1 );
    lUInt32 * dst = (lUInt32 *)buf.GetScanLine(0);
#if (GRAY_INVERSE==1)
    static lUInt32 def_pal_1bpp[2] = {0xFFFFFF, 0x000000};
    static lUInt32 def_pal_2bpp[4] = {0xFFFFFF, 0xAAAAAA, 0x555555, 0x000000};
#else
    static lUInt32 def_pal_1bpp[2] = {0x000000, 0xFFFFFF};
    static lUInt32 def_pal_2bpp[4] = {0x000000, 0x555555, 0xAAAAAA, 0xFFFFFF};
#endif
	lUInt32 pal[256];
	if ( _bpp<=8 ) {
		int n = 1<<_bpp;
		for ( int i=0; i<n; i++ ) {
			int c = 255 * i / (n-1);
			pal[i] = c | (c<<8) | (c<<16);
		}
	}
    if (!palette)
        palette = (_bpp==1) ? def_pal_1bpp : def_pal_2bpp;
    for (int yy=0; yy<_dy; yy++)
    {
        lUInt8 * src = GetScanLine(yy);
        for (int xx=0; xx<_dx; xx++)
        {
            //
            if (_bpp==1)
            {
                int shift = 7-(xx&7);
                int x0 = xx >> 3;
                dst[xx] = palette[ (src[x0]>>shift) & 1];
            }
            else if (_bpp==2)
            {
                int shift = 6-((xx&3)<<1);
                int x0 = xx >> 2;
                dst[xx] = palette[ (src[x0]>>shift) & 3];
            }
            else // 3,4,8
            {
                int index = (src[xx] >> (8-_bpp)) & ((1<<_bpp)-1);
                dst[xx] = pal[ index ];
            }
        }
        BitBlt( dc, x, y+yy, _dx, 1, buf.GetDC(), 0, 0, SRCCOPY );
    }
}


/// draws buffer content to DC doing color conversion if necessary
void LVColorDrawBuf::DrawTo( HDC dc, int x, int y, int options, lUInt32 * palette )
{
    if (dc!=NULL && _drawdc!=NULL)
        BitBlt( dc, x, y, _dx, _dy, _drawdc, 0, 0, SRCCOPY );
}
#endif

/// draws buffer content to another buffer doing color conversion if necessary
void LVGrayDrawBuf::DrawTo( LVDrawBuf * buf, int x, int y, int options, lUInt32 * palette )
{
    lvRect clip;
    buf->GetClipRect(&clip);

	if ( !(!clip.isEmpty() || buf->GetBitsPerPixel()!=GetBitsPerPixel() || GetWidth()!=buf->GetWidth() || GetHeight()!=buf->GetHeight()) ) {
		// simple copy
        memcpy( buf->GetScanLine(0), GetScanLine(0), GetHeight() * GetRowSize() );
		return;
	}
    int bpp = GetBitsPerPixel();
	if (buf->GetBitsPerPixel() == 32) {
		// support for 32bpp to Gray drawing
	    for (int yy=0; yy<_dy; yy++)
	    {
	        if (y+yy >= clip.top && y+yy < clip.bottom)
	        {
	            lUInt8 * src = (lUInt8 *)GetScanLine(yy);
                lUInt32 * dst = ((lUInt32 *)buf->GetScanLine(y+yy)) + x;
	            if (bpp==1)
	            {
	                int shift = x & 7;
	                for (int xx=0; xx<_dx; xx++)
	                {
	                    if ( x+xx >= clip.left && x+xx < clip.right )
	                    {
	                        lUInt8 cl = (*src << shift) & 0x80;
	                        *dst = cl ? 0xFFFFFF : 0x000000;
	                    }
	                    dst++;
	                    if (++shift >= 8) {
	                    	shift = 0;
		                    src++;
	                    }

	                }
	            }
	            else if (bpp==2)
	            {
	                int shift = x & 3;
	                for (int xx=0; xx<_dx; xx++)
	                {
	                    if ( x+xx >= clip.left && x+xx < clip.right )
	                    {
	                        lUInt32 cl = (*src << (shift<<1)) & 0xC0;
	                        cl = cl | (cl >> 2) | (cl>>4) | (cl>>6);
	                        *dst = cl | (cl << 8) | (cl << 16);
	                    }
	                    dst++;
	                    if (++shift >= 4) {
	                    	shift = 0;
		                    src++;
	                    }

	                }
	            }
	            else
	            {
	            	// byte per pixel
	                for (int xx=0; xx<_dx; xx++)
	                {
	                    if ( x+xx >= clip.left && x+xx < clip.right )
	                    {
	                        lUInt32 cl = *src;
	                        if (bpp == 3) {
	                        	cl &= 0xE0;
	                        	cl = cl | (cl>>3) | (cl>>6);
	                        } else if (bpp == 4) {
	                        	cl &= 0xF0;
	                        	cl = cl | (cl>>4);
	                        }
	                        *dst = cl | (cl << 8) | (cl << 16);
	                    }
	                    dst++;
	                    src++;
	                }
	            }
	        }
	    }
	    return;
	}
	if (buf->GetBitsPerPixel() == 16) {
		// support for 32bpp to Gray drawing
	    for (int yy=0; yy<_dy; yy++)
	    {
	        if (y+yy >= clip.top && y+yy < clip.bottom)
	        {
	            lUInt8 * src = (lUInt8 *)GetScanLine(yy);
                lUInt16 * dst = ((lUInt16 *)buf->GetScanLine(y+yy)) + x;
	            if (bpp==1)
	            {
	                int shift = x & 7;
	                for (int xx=0; xx<_dx; xx++)
	                {
	                    if ( x+xx >= clip.left && x+xx < clip.right )
	                    {
	                        lUInt8 cl = (*src << shift) & 0x80;
	                        *dst = cl ? 0xFFFF : 0x0000;
	                    }
	                    dst++;
	                    if (++shift >= 8) {
	                    	shift = 0;
		                    src++;
	                    }

	                }
	            }
	            else if (bpp==2)
	            {
	                int shift = x & 3;
	                for (int xx=0; xx<_dx; xx++)
	                {
	                    if ( x+xx >= clip.left && x+xx < clip.right )
	                    {
	                        lUInt16 cl = (*src << (shift<<1)) & 0xC0;
	                        cl = cl | (cl >> 2) | (cl>>4) | (cl>>6);
	                        *dst = rgb565(cl, cl, cl);
	                    }
	                    dst++;
	                    if (++shift >= 4) {
	                    	shift = 0;
		                    src++;
	                    }

	                }
	            }
	            else
	            {
	            	// byte per pixel
	                for (int xx=0; xx<_dx; xx++)
	                {
	                    if ( x+xx >= clip.left && x+xx < clip.right )
	                    {
	                        lUInt16 cl = *src;
	                        if (bpp == 3) {
	                        	cl &= 0xE0;
	                        	cl = cl | (cl>>3) | (cl>>6);
	                        } else if (bpp == 4) {
	                        	cl &= 0xF0;
	                        	cl = cl | (cl>>4);
	                        }
	                        *dst = rgb565(cl, cl, cl);
	                    }
	                    dst++;
	                    src++;
	                }
	            }
	        }
	    }
	    return;
	}
	if (buf->GetBitsPerPixel() != bpp)
		return; // not supported yet
    for (int yy=0; yy<_dy; yy++)
    {
        if (y+yy >= clip.top && y+yy < clip.bottom)
        {
            lUInt8 * src = (lUInt8 *)GetScanLine(yy);
            if (bpp==1)
            {
                int shift = x & 7;
                lUInt8 * dst = buf->GetScanLine(y+yy) + (x>>3);
                for (int xx=0; xx<_dx; xx+=8)
                {
                    if ( x+xx >= clip.left && x+xx < clip.right )
                    {
                        //lUInt8 mask = ~((lUInt8)0xC0>>shift);
                        lUInt16 cl = (*src << 8)>>shift;
                        lUInt16 mask = (0xFF00)>>shift;
						lUInt8 c = *dst;
						c &= ~(mask>>8);
						c |= (cl>>8);
                        *dst = c;
						c = *(dst+1);
						c &= ~(mask&0xFF);
						c |= (cl&0xFF);
                        *(dst+1) = c;
                    }    
                    dst++;
                    src++;
                }
            }
            else if (bpp==2)
            {
                int shift = (x & 3) * 2;
                lUInt8 * dst = buf->GetScanLine(y+yy) + (x>>2);
                for (int xx=0; xx<_dx; xx+=4)
                {
                    if ( x+xx >= clip.left && x+xx < clip.right )
                    {
                        //lUInt8 mask = ~((lUInt8)0xC0>>shift);
                        lUInt16 cl = (*src << 8)>>shift;
                        lUInt16 mask = (0xFF00)>>shift;
						lUInt8 c = *dst;
						c &= ~(mask>>8);
						c |= (cl>>8);
                        *dst = c;
						c = *(dst+1);
						c &= ~(mask&0xFF);
						c |= (cl&0xFF);
                        *(dst+1) = c;
                    }    
                    dst++;
                    src++;
                }
            }
            else
            {
                lUInt8 * dst = buf->GetScanLine(y+yy) + x;
                for (int xx=0; xx<_dx; xx++)
                {
                    if ( x+xx >= clip.left && x+xx < clip.right )
                    {
                        *dst = *src;
                    }
                    dst++;
                    src++;
                }
            }
        }
    }
}

/// draws buffer content to another buffer doing color conversion if necessary
void LVColorDrawBuf::DrawTo( LVDrawBuf * buf, int x, int y, int options, lUInt32 * palette )
{
    //
    lvRect clip;
    buf->GetClipRect(&clip);
    int bpp = buf->GetBitsPerPixel();
    for (int yy=0; yy<_dy; yy++)
    {
        if (y+yy >= clip.top && y+yy < clip.bottom)
        {
            if ( _bpp==16 ) {
                lUInt16 * src = (lUInt16 *)GetScanLine(yy);
                if (bpp==1)
                {
                    int shift = x & 7;
                    lUInt8 * dst = buf->GetScanLine(y+yy) + (x>>3);
                    for (int xx=0; xx<_dx; xx++)
                    {
                        if ( x+xx >= clip.left && x+xx < clip.right )
                        {
                            //lUInt8 mask = ~((lUInt8)0xC0>>shift);
    #if (GRAY_INVERSE==1)
                            lUInt8 cl = (((lUInt8)(*src)&0x8000)^0x8000) >> (shift+8);
    #else
                            lUInt8 cl = (((lUInt8)(*src)&0x8000)) >> (shift+8);
    #endif
                            *dst |= cl;
                        }
                        if ( !(shift = (shift+1)&7) )
                            dst++;
                        src++;
                    }
                }
                else if (bpp==2)
                {
                    int shift = x & 3;
                    lUInt8 * dst = buf->GetScanLine(y+yy) + (x>>2);
                    for (int xx=0; xx<_dx; xx++)
                    {
                        if ( x+xx >= clip.left && x+xx < clip.right )
                        {
                            //lUInt8 mask = ~((lUInt8)0xC0>>shift);
    #if (GRAY_INVERSE==1)
                            lUInt8 cl = (((lUInt8)(*src)&0xC000)^0xC000) >> ((shift<<1) + 8);
    #else
                            lUInt8 cl = (((lUInt8)(*src)&0xC000)) >> ((shift<<1) + 8);
    #endif
                            *dst |= cl;
                        }
                        if ( !(shift = (shift+1)&3) )
                            dst++;
                        src++;
                    }
                }
                else if (bpp<=8)
                {
                    lUInt8 * dst = buf->GetScanLine(y+yy) + x;
                    for (int xx=0; xx<_dx; xx++)
                    {
                        if ( x+xx >= clip.left && x+xx < clip.right )
                        {
                            //lUInt8 mask = ~((lUInt8)0xC0>>shift);
                            *dst = (lUInt8)(*src >> 8);
                        }
                        dst++;
                        src++;
                    }
                }
                else if (bpp==16)
                {
                    lUInt16 * dst = ((lUInt16 *)buf->GetScanLine(y+yy)) + x;
                    for (int xx=0; xx<_dx; xx++)
                    {
                        if ( x+xx >= clip.left && x+xx < clip.right )
                        {
                            *dst = *src;
                        }
                        dst++;
                        src++;
                    }
                }
                else if (bpp==32)
                {
                    lUInt32 * dst = ((lUInt32 *)buf->GetScanLine(y+yy)) + x;
                    for (int xx=0; xx<_dx; xx++)
                    {
                        if ( x+xx >= clip.left && x+xx < clip.right )
                        {
                            *dst = rgb565to888( *src );
                        }
                        dst++;
                        src++;
                    }
                }
            } else {
                lUInt32 * src = (lUInt32 *)GetScanLine(yy);
                if (bpp==1)
                {
                    int shift = x & 7;
                    lUInt8 * dst = buf->GetScanLine(y+yy) + (x>>3);
                    for (int xx=0; xx<_dx; xx++)
                    {
                        if ( x+xx >= clip.left && x+xx < clip.right )
                        {
                            //lUInt8 mask = ~((lUInt8)0xC0>>shift);
    #if (GRAY_INVERSE==1)
                            lUInt8 cl = (((lUInt8)(*src)&0x80)^0x80) >> (shift);
    #else
                            lUInt8 cl = (((lUInt8)(*src)&0x80)) >> (shift);
    #endif
                            *dst |= cl;
                        }
                        if ( !(shift = (shift+1)&7) )
                            dst++;
                        src++;
                    }
                }
                else if (bpp==2)
                {
                    int shift = x & 3;
                    lUInt8 * dst = buf->GetScanLine(y+yy) + (x>>2);
                    for (int xx=0; xx<_dx; xx++)
                    {
                        if ( x+xx >= clip.left && x+xx < clip.right )
                        {
                            //lUInt8 mask = ~((lUInt8)0xC0>>shift);
    #if (GRAY_INVERSE==1)
                            lUInt8 cl = (((lUInt8)(*src)&0xC0)^0xC0) >> (shift<<1);
    #else
                            lUInt8 cl = (((lUInt8)(*src)&0xC0)) >> (shift<<1);
    #endif
                            *dst |= cl;
                        }
                        if ( !(shift = (shift+1)&3) )
                            dst++;
                        src++;
                    }
                }
                else if (bpp<=8)
                {
                    lUInt8 * dst = buf->GetScanLine(y+yy) + x;
                    for (int xx=0; xx<_dx; xx++)
                    {
                        if ( x+xx >= clip.left && x+xx < clip.right )
                        {
                            //lUInt8 mask = ~((lUInt8)0xC0>>shift);
                            *dst = (lUInt8)*src;
                        }
                        dst++;
                        src++;
                    }
                }
                else if (bpp==32)
                {
                    lUInt32 * dst = ((lUInt32 *)buf->GetScanLine(y+yy)) + x;
                    for (int xx=0; xx<_dx; xx++)
                    {
                        if ( x+xx >= clip.left && x+xx < clip.right )
                        {
                            *dst = *src;
                        }
                        dst++;
                        src++;
                    }
                }
            }
        }
    }
}

/// returns scanline pointer
lUInt8 * LVColorDrawBuf::GetScanLine( int y )
{
    if (!_data || y<0 || y>=_dy)
        return NULL;
#if !defined(__SYMBIAN32__) && defined(_WIN32)
    return _data + _rowsize * (_dy-1-y);
#else
    return _data + _rowsize * y;
#endif
}

/// returns white pixel value
lUInt32 LVColorDrawBuf::GetWhiteColor()
{
    return 0xFFFFFF;
}
/// returns black pixel value
lUInt32 LVColorDrawBuf::GetBlackColor()
{
    return 0x000000;
}


/// constructor
LVColorDrawBuf::LVColorDrawBuf(int dx, int dy, int bpp)
:     LVBaseDrawBuf()
#if !defined(__SYMBIAN32__) && defined(_WIN32)
    ,_drawdc(NULL)
    ,_drawbmp(NULL)
#endif
    ,_bpp(bpp)
    ,_ownData(true)
{
    _rowsize = dx*(_bpp>>3);
    Resize( dx, dy );
}

/// creates wrapper around external RGBA buffer
LVColorDrawBuf::LVColorDrawBuf(int dx, int dy, lUInt8 * externalBuffer, int bpp )
:     LVBaseDrawBuf()
#if !defined(__SYMBIAN32__) && defined(_WIN32)
    ,_drawdc(NULL)
    ,_drawbmp(NULL)
#endif
    ,_bpp(bpp)
    ,_ownData(false)
{
    _dx = dx;
    _dy = dy;
    _rowsize = dx*(_bpp>>3);
    _data = externalBuffer;
    SetClipRect( NULL );
}

/// destructor
LVColorDrawBuf::~LVColorDrawBuf()
{
	if ( !_ownData )
		return;
#if !defined(__SYMBIAN32__) && defined(_WIN32)
    if (_drawdc)
        DeleteDC(_drawdc);
    if (_drawbmp)
        DeleteObject(_drawbmp);
#else
    if (_data)
        free( _data );
#endif
}

/// convert to 1-bit bitmap
void LVColorDrawBuf::ConvertToBitmap(bool flgDither)
{
}

