//C-  -*- C++ -*-
//C- -------------------------------------------------------------------
//C- DjVuLibre-3.5
//C- Copyright (c) 2002  Leon Bottou and Yann Le Cun.
//C- Copyright (c) 2001  AT&T
//C-
//C- This software is subject to, and may be distributed under, the
//C- GNU General Public License, either Version 2 of the license,
//C- or (at your option) any later version. The license should have
//C- accompanied the software or you may obtain a copy of the license
//C- from the Free Software Foundation at http://www.fsf.org .
//C-
//C- This program is distributed in the hope that it will be useful,
//C- but WITHOUT ANY WARRANTY; without even the implied warranty of
//C- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//C- GNU General Public License for more details.
//C- 
//C- DjVuLibre-3.5 is derived from the DjVu(r) Reference Library from
//C- Lizardtech Software.  Lizardtech Software has authorized us to
//C- replace the original DjVu(r) Reference Library notice by the following
//C- text (see doc/lizard2002.djvu and doc/lizardtech2007.djvu):
//C-
//C-  ------------------------------------------------------------------
//C- | DjVu (r) Reference Library (v. 3.5)
//C- | Copyright (c) 1999-2001 LizardTech, Inc. All Rights Reserved.
//C- | The DjVu Reference Library is protected by U.S. Pat. No.
//C- | 6,058,214 and patents pending.
//C- |
//C- | This software is subject to, and may be distributed under, the
//C- | GNU General Public License, either Version 2 of the license,
//C- | or (at your option) any later version. The license should have
//C- | accompanied the software or you may obtain a copy of the license
//C- | from the Free Software Foundation at http://www.fsf.org .
//C- |
//C- | The computer code originally released by LizardTech under this
//C- | license and unmodified by other parties is deemed "the LIZARDTECH
//C- | ORIGINAL CODE."  Subject to any third party intellectual property
//C- | claims, LizardTech grants recipient a worldwide, royalty-free, 
//C- | non-exclusive license to make, use, sell, or otherwise dispose of 
//C- | the LIZARDTECH ORIGINAL CODE or of programs derived from the 
//C- | LIZARDTECH ORIGINAL CODE in compliance with the terms of the GNU 
//C- | General Public License.   This grant only confers the right to 
//C- | infringe patent claims underlying the LIZARDTECH ORIGINAL CODE to 
//C- | the extent such infringement is reasonably necessary to enable 
//C- | recipient to make, have made, practice, sell, or otherwise dispose 
//C- | of the LIZARDTECH ORIGINAL CODE (or portions thereof) and not to 
//C- | any greater extent that may be necessary to utilize further 
//C- | modifications or combinations.
//C- |
//C- | The LIZARDTECH ORIGINAL CODE is provided "AS IS" WITHOUT WARRANTY
//C- | OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
//C- | TO ANY WARRANTY OF NON-INFRINGEMENT, OR ANY IMPLIED WARRANTY OF
//C- | MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
//C- +------------------------------------------------------------------

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#if NEED_GNUG_PRAGMAS
# pragma implementation
#endif

// -- Implements class PIXMAP
// Author: Leon Bottou 07/1997



#include "GPixmap.h"

#include "GString.h"
#include "GException.h"
#include "ByteStream.h"
#include "GRect.h"
#include "GBitmap.h"
#include "GThreads.h"
#include "Arrays.h"
#include "JPEGDecoder.h"

#include <stddef.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>


#ifdef HAVE_NAMESPACES
namespace DJVU {
# ifdef NOT_DEFINED // Just to fool emacs c++ mode
}
#endif
#endif



//////////////////////////////////////////////////
// ------- predefined colors
//////////////////////////////////////////////////


const GPixel GPixel::WHITE = { 255, 255, 255 };
const GPixel GPixel::BLACK = {   0,   0,   0 };
const GPixel GPixel::BLUE  = { 255,   0,   0 };
const GPixel GPixel::GREEN = {   0, 255,   0 };
const GPixel GPixel::RED   = {   0,   0, 255 };


//////////////////////////////////////////////////
// ----- utilities
//////////////////////////////////////////////////


static const GPixel *
new_gray_ramp(int grays,GPixel *ramp)
{
  int color = 0xff0000;
  int decrement = color / (grays-1);
  for (int i=0; i<grays; i++)
    {
      int level = color >> 16;
      ramp[i].b = level;
      ramp[i].g = level;
      ramp[i].r = level;
      color -= decrement;
    }
  return ramp;
}


static inline int
mini(int x, int y) 
{ 
  return (x < y ? x : y);
}


static inline int
maxi(int x, int y) 
{ 
  return (x > y ? x : y);
}


static inline void 
euclidian_ratio(int a, int b, int &q, int &r)
{
  q = a / b;
  r = a - b*q;
  if (r < 0)
  {
    q -= 1;
    r += b;
  }
}


//////////////////////////////////////////////////
// global lock used by some rare operations
//////////////////////////////////////////////////

static GMonitor &pixmap_monitor() {
  static GMonitor xpixmap_monitor;
  return xpixmap_monitor;
}


//////////////////////////////////////////////////
// constructors and destructors
//////////////////////////////////////////////////


GPixmap::~GPixmap()
{
  delete [] pixels_data;
}

void
GPixmap::destroy(void)
{
  delete [] pixels_data;
  pixels = pixels_data = 0;
}

GPixmap::GPixmap()
: nrows(0), ncolumns(0), pixels(0), pixels_data(0)
{
}

GPixmap::GPixmap(int nrows, int ncolumns, const GPixel *filler)
: nrows(0), ncolumns(0), pixels(0), pixels_data(0)
{
  G_TRY
  {
    init(nrows, ncolumns, filler);
  }
  G_CATCH_ALL
  {
	destroy();
	G_RETHROW;
  }
  G_ENDCATCH;
}

GPixmap::GPixmap(ByteStream &bs)
: nrows(0), ncolumns(0), pixels(0), pixels_data(0)
{
  G_TRY
  {
	init(bs);
  }
  G_CATCH_ALL
  {
	destroy();
	G_RETHROW;
  }
  G_ENDCATCH;
}

GPixmap::GPixmap(const GBitmap &ref)
: nrows(0), ncolumns(0), pixels(0), pixels_data(0)
{
  G_TRY
  {
	init(ref, 0);
  }
  G_CATCH_ALL
  {
	destroy();
	G_RETHROW;
  }
  G_ENDCATCH;
}

GPixmap::GPixmap(const GBitmap &ref, const GRect &rect)
: nrows(0), ncolumns(0), pixels(0), pixels_data(0)
{
  G_TRY
  {
    init(ref, rect, 0);
  }
  G_CATCH_ALL
  {
	destroy();
	G_RETHROW;
  }
  G_ENDCATCH;
}

GPixmap::GPixmap(const GPixmap &ref)
: nrows(0), ncolumns(0), pixels(0), pixels_data(0)
{
  G_TRY
  {
    init(ref);
  }
  G_CATCH_ALL
  {
	destroy();
	G_RETHROW;
  }
  G_ENDCATCH;
}

GPixmap::GPixmap(const GPixmap &ref, const GRect &rect)
: nrows(0), ncolumns(0), pixels(0), pixels_data(0)
{
  G_TRY
  {
	init(ref, rect);
  }
  G_CATCH_ALL
  {
	destroy();
	G_RETHROW;
  }
  G_ENDCATCH;
}



//////////////////////////////////////////////////
// Initialization
//////////////////////////////////////////////////


void 
GPixmap::init(int arows, int acolumns, const GPixel *filler)
{
  size_t np = arows * acolumns;
  if (arows != (unsigned short) arows ||
      acolumns != (unsigned short) acolumns ||
      (arows>0 && np/(size_t)arows!=(size_t)acolumns) )
    G_THROW("GPixmap: image size exceeds maximum (corrupted file?)");
  destroy();
  nrows = arows;
  ncolumns = acolumns;
  nrowsize = acolumns;
  int npix = nrows * nrowsize;
  if (npix > 0)
  {
    pixels = pixels_data = new GPixel[npix];
    if (filler)
    { 
      while (--npix>=0) 
        pixels_data[npix] = *filler;
    }
  }
}


void 
GPixmap::init(const GBitmap &ref, const GPixel *userramp)
{
  init(ref.rows(), ref.columns(), 0);
  GPixel *xramp;
  GPBuffer<GPixel> gxramp(xramp);
  if (nrows>0 && ncolumns>0)
  {
    // Create pixel ramp
    const GPixel *ramp = userramp;
    if (!userramp)
	{
          gxramp.resize(256);
          gxramp.clear();
	  ramp = new_gray_ramp(ref.get_grays(),xramp);
	}
    // Copy pixels
    for (int y=0; y<nrows; y++)
    {
      GPixel *dst = (*this)[y];
      const unsigned char *src = ref[y];
      for (int x=0; x<ncolumns; x++)
        dst[x] = ramp[ src[x] ];
    }
    // Free ramp
//    if (!userramp)
//      delete [] (GPixel*)ramp;
  }
}


void 
GPixmap::init(const GBitmap &ref, const GRect &rect, const GPixel *userramp)
{
  init(rect.height(), rect.width(), 0);
  // compute destination rectangle
  GRect rect2(0, 0, ref.columns(), ref.rows() );
  rect2.intersect(rect2, rect);
  rect2.translate(-rect.xmin, -rect.ymin);
  // copy bits
  if (! rect2.isempty())
  {
    GPixel *xramp;
    GPBuffer<GPixel> gxramp(xramp);
    // allocate ramp
    const GPixel *ramp = userramp;
    if (!userramp)
	{
	  gxramp.resize(256);
          gxramp.clear();
          ramp = new_gray_ramp(ref.get_grays(),xramp);
	}
    // copy pixels
    for (int y=rect2.ymin; y<rect2.ymax; y++)
    {
      GPixel *dst = (*this)[y];
      const unsigned char *src = ref[y+rect.ymin] + rect.xmin;
      for (int x=rect2.xmin; x<rect2.xmax; x++)
        dst[x] = ramp[ src[x] ];
    }
    // free ramp
//    if (!userramp)
//      delete [] (GPixel*) ramp;
  }
}


void 
GPixmap::init(const GPixmap &ref)
{
  init(ref.rows(), ref.columns(), 0);
  if (nrows>0 && ncolumns>0)
  {
    for (int y=0; y<nrows; y++)
    {
      GPixel *dst = (*this)[y];
      const GPixel *src = ref[y];
      for (int x=0; x<ncolumns; x++)
        dst[x] = src[x];
    }
  }
}


void 
GPixmap::init(const GPixmap &ref, const GRect &rect)
{
  init(rect.height(), rect.width(), 0);
  // compute destination rectangle
  GRect rect2(0, 0, ref.columns(), ref.rows() );
  rect2.intersect(rect2, rect);
  rect2.translate(-rect.xmin, -rect.ymin);
  // copy bits
  if (! rect2.isempty())
  {
    for (int y=rect2.ymin; y<rect2.ymax; y++)
    {
      GPixel *dst = (*this)[y];
      const GPixel *src = ref[y+rect.ymin] + rect.xmin;
      for (int x=rect2.xmin; x<rect2.xmax; x++)
        dst[x] = src[x];
    }
  }
}


void 
GPixmap::donate_data(GPixel *data, int w, int h)
{
  destroy();
  nrows = h;
  ncolumns = w;
  nrowsize = w;
  pixels_data=pixels=data;
}


GPixel *
GPixmap::take_data(size_t &offset)
{
  GPixel *ret = pixels_data;
  pixels_data = 0;
  offset = 0;
  return ret;
}



//////////////////////////////////////////////////
// Save and load ppm files
//////////////////////////////////////////////////


static unsigned int 
read_integer(char &c, ByteStream &bs)
{
  unsigned int x = 0;
  // eat blank before integer
  while (c==' ' || c=='\t' || c=='\r' || c=='\n' || c=='#') 
    {
      if (c=='#') 
        do { } while (bs.read(&c,1) && c!='\n' && c!='\r');
      c = 0; 
      bs.read(&c, 1);
    }
  // check integer
  if (c<'0' || c>'9')
    G_THROW( ERR_MSG("GPixmap.no_int") );
  // eat integer
  while (c>='0' && c<='9') 
    {
      x = x*10 + c - '0';
      c = 0;
      bs.read(&c, 1);
    }
  return x;
}


void 
GPixmap::init(ByteStream &bs)
{
  // Read header
  bool raw = false;
  bool grey = false;
  int magic = bs.read16();
  GP<GBitmap> bm;
  switch (magic)
    {
    case ('P'<<8)+'2':
      grey = true;
      break;
    case ('P'<<8)+'3':
      break;
    case ('P'<<8)+'5':
      raw = grey = true;
      /* FALLTHRU */
    case ('P'<<8)+'6':
      raw = true;
      break;
    case ('P'<<8)+'1':
    case ('P'<<8)+'4': 
      bs.seek(0L);
      bm = GBitmap::create(bs); 
      init(*bm);
      return;
    default:
#ifdef NEED_JPEG_DECODER
      bs.seek(0L);
      JPEGDecoder::decode(bs,*this);
      return;
#else
      
      G_THROW( ERR_MSG("GPixmap.unk_PPM") );
#endif
    }
  // Read image size
  char lookahead = '\n';
  int bytesperrow = 0;
  int bytespercomp = 1;
  int acolumns = read_integer(lookahead, bs);
  int arows = read_integer(lookahead, bs);
  int maxval = read_integer(lookahead, bs);
  if (maxval > 65535)
    G_THROW("Cannot read PPM with depth greater than 48 bits.");
  if (maxval > 255)
    bytespercomp = 2;
  init(arows, acolumns, 0);
  // Prepare ramp
  GTArray<unsigned char> ramp;
  int maxbin = 1 << (8 * bytespercomp);
  ramp.resize(0, maxbin-1);
  for (int i=0; i<maxbin; i++)
    ramp[i] = (i<maxval ? (255*i + maxval/2) / maxval : 255);
  unsigned char *bramp = ramp;
  // Read image data
  if (raw && grey)
    {
      bytesperrow = ncolumns * bytespercomp;
      GTArray<unsigned char> line(bytesperrow);
      for (int y=nrows-1; y>=0; y--) 
        {
          GPixel *p = (*this)[y];
          unsigned char *g = &line[0];
          if ( bs.readall((void*)g, bytesperrow) < (size_t)bytesperrow)
            G_THROW( ByteStream::EndOfFile );
          if (bytespercomp <= 1)
            {
              for (int x=0; x<ncolumns; x+=1, g+=1)
                p[x].r = p[x].g = p[x].b = bramp[g[0]];
            }
          else
            {
              for (int x=0; x<ncolumns; x+=1, g+=2)
                p[x].r = p[x].g = p[x].b = bramp[g[0]*256+g[1]];
            }
        }
    }
  else if (raw)
    {
      bytesperrow = ncolumns * bytespercomp * 3;
      GTArray<unsigned char> line(bytesperrow);
      for (int y=nrows-1; y>=0; y--) 
        {
          GPixel *p = (*this)[y];
          unsigned char *rgb = &line[0];
          if ( bs.readall((void*)rgb, bytesperrow) < (size_t)bytesperrow)
            G_THROW( ByteStream::EndOfFile );
          if (bytespercomp <= 1)
            {
              for (int x=0; x<ncolumns; x+=1, rgb+=3)
                {
                  p[x].r = bramp[rgb[0]];
                  p[x].g = bramp[rgb[1]];
                  p[x].b = bramp[rgb[2]];
                }
            }
          else
            for (int x=0; x<ncolumns; x+=1, rgb+=6)
              {
                p[x].r = bramp[rgb[0]*256+rgb[1]];
                p[x].g = bramp[rgb[2]*256+rgb[3]];
                p[x].b = bramp[rgb[4]*256+rgb[5]];
              }
        }
    }
  else
    {
      for (int y=nrows-1; y>=0; y--) 
        {
          GPixel *p = (*this)[y];
          for (int x=0; x<ncolumns; x++)
            if (grey)
              {
                p[x].g = p[x].b = p[x].r = ramp[(int)read_integer(lookahead, bs)];
              }
            else
              {
                p[x].r = ramp[(int)read_integer(lookahead, bs)];
                p[x].g = ramp[(int)read_integer(lookahead, bs)];
                p[x].b = ramp[(int)read_integer(lookahead, bs)];
              }
        }
    }
}


void 
GPixmap::save_ppm(ByteStream &bs, int raw) const
{
  GUTF8String head;
  head.format("P%c\n%d %d\n255\n", (raw ? '6' : '3'), ncolumns, nrows);
  bs.writall((void*)(const char *)head, head.length());
  if (raw)
    {
      int rowsize = ncolumns+ncolumns+ncolumns;
      GTArray<unsigned char> xrgb(rowsize);
      for (int y=nrows-1; y>=0; y--) 
        {
          const GPixel *p = (*this)[y];
          unsigned char *d = xrgb;
          for (int x=0; x<ncolumns; x++) 
            {
              *d++ = p[x].r;
              *d++ = p[x].g;
              *d++ = p[x].b;
            }
          bs.writall((void*)(unsigned char*)xrgb, ncolumns * 3);
        }
    }
  else
    {
      for (int y=nrows-1; y>=0; y--) 
        {
          const GPixel *p = (*this)[y];
          unsigned char eol='\n';
          for (int x=0; x<ncolumns; )
            {
              head.format("%d %d %d  ", p[x].r, p[x].g, p[x].b);
              bs.writall((void*)(const char *)head, head.length());
              x += 1;
              if (x==ncolumns || (x&0x7)==0) 
                bs.write((void*)&eol, 1);          
            }
        }
    }
}




//////////////////////////////////////////////////
// Color correction
//////////////////////////////////////////////////


static void
color_correction_table(double gamma, GPixel white,
                       unsigned char gtable[256][3] )
{
  // Check argument
  if (gamma<0.1 || gamma>10.0)
    G_THROW( ERR_MSG("GPixmap.bad_param") );
  if (gamma<1.001 && gamma>0.999 && white==GPixel::WHITE)
    {
      // Trivial correction
      for (int i=0; i<256; i++)
        gtable[i][0] = gtable[i][1] = gtable[i][2] = i;
    }
  else
    {
      // Must compute the correction
      for (int i=0; i<256; i++)
        {
          double x = (double)(i)/255.0;
#ifdef BEZIERGAMMA
          double t = ( sqrt(1.0+(gamma*gamma-1.0)*x) - 1.0 ) / (gamma - 1.0);
          x = ( (1.0 - gamma)*t + 2.0 * gamma ) * t / (gamma + 1.0);
#else
          x = pow(x, 1.0/gamma);        
#endif
          gtable[i][0] = (int) floor(white.b * x + 0.5);
          gtable[i][1] = (int) floor(white.g * x + 0.5);
          gtable[i][2] = (int) floor(white.r * x + 0.5);
        }
      // Make sure that min and max values are exactly black or white
      gtable[0][0] = 0;
      gtable[0][1] = 0;
      gtable[0][2] = 0;
      gtable[255][0] = white.b;
      gtable[255][1] = white.g;
      gtable[255][2] = white.r;
    }
}

static void
color_correction_table_cache(double gamma, GPixel white,
                             unsigned char gtable[256][3] )
{
  // Compute color correction table
  if (gamma<1.001 && gamma>0.999 && white==GPixel::WHITE)
    {
      color_correction_table(gamma, white, gtable);
    }
  else
    {
      static double lgamma = -1.0;
      static GPixel lwhite = GPixel::BLACK;
      static unsigned char ctable[256][3];
      GMonitorLock lock(&pixmap_monitor());
      if (gamma != lgamma || white != lwhite)
        {
          color_correction_table(gamma, white, ctable);
          lgamma = gamma;
          lwhite = white;
        }
      memcpy(gtable, ctable, 256*3*sizeof(unsigned char));
    }
}

void 
GPixmap::color_correct(double gamma_correction, GPixel white)
{
  // Trivial corrections
  if (gamma_correction>0.999 && gamma_correction<1.001 && white==GPixel::WHITE)
    return;
  // Compute correction table
  unsigned char gtable[256][3];
  color_correction_table_cache(gamma_correction, white, gtable);
  // Perform correction
  for (int y=0; y<nrows; y++)
  {
    GPixel *pix = (*this)[y];
    for (int x=0; x<ncolumns; x++, pix++)
    {
      pix->b = gtable[ pix->b ][0];
      pix->g = gtable[ pix->g ][1];
      pix->r = gtable[ pix->r ][2];
    }
  }
}

void 
GPixmap::color_correct(double gamma_correction)
{
  // Trivial corrections
  if (gamma_correction<=0.999 || gamma_correction>=1.001)
    color_correct(gamma_correction, GPixel::WHITE);
}


void 
GPixmap::color_correct(double gamma_correction, GPixel white,
                       GPixel *pix, int npixels)
{
  // Trivial corrections
  if (gamma_correction>0.999 && gamma_correction<1.001 && white==GPixel::WHITE)
    return;
  // Compute correction table
  unsigned char gtable[256][3];
  color_correction_table_cache(gamma_correction, white, gtable);
  // Perform correction
  while (--npixels>=0)
    {
      pix->b = gtable[pix->b][0];
      pix->g = gtable[pix->g][1];
      pix->r = gtable[pix->r][2];
      pix++;
    }
}


void 
GPixmap::color_correct(double gamma_correction, GPixel *pix, int npixels)
{
  // Trivial corrections
  if (gamma_correction<=0.999 || gamma_correction>=1.001)
    color_correct(gamma_correction,GPixel::WHITE,pix,npixels);
}


//////////////////////////////////////////////////
// Dithering
//////////////////////////////////////////////////


void
GPixmap::ordered_666_dither(int xmin, int ymin)
{
  static unsigned char quantize[256+0x33+0x33];
  static unsigned char *quant = quantize + 0x33;
  static char  dither_ok = 0;
  static short dither[16][16] = 
  {
    {   0,192, 48,240, 12,204, 60,252,  3,195, 51,243, 15,207, 63,255 },
    { 128, 64,176,112,140, 76,188,124,131, 67,179,115,143, 79,191,127 },
    {  32,224, 16,208, 44,236, 28,220, 35,227, 19,211, 47,239, 31,223 },
    { 160, 96,144, 80,172,108,156, 92,163, 99,147, 83,175,111,159, 95 },
    {   8,200, 56,248,  4,196, 52,244, 11,203, 59,251,  7,199, 55,247 },
    { 136, 72,184,120,132, 68,180,116,139, 75,187,123,135, 71,183,119 },
    {  40,232, 24,216, 36,228, 20,212, 43,235, 27,219, 39,231, 23,215 },
    { 168,104,152, 88,164,100,148, 84,171,107,155, 91,167,103,151, 87 },
    {   2,194, 50,242, 14,206, 62,254,  1,193, 49,241, 13,205, 61,253 },
    { 130, 66,178,114,142, 78,190,126,129, 65,177,113,141, 77,189,125 },
    {  34,226, 18,210, 46,238, 30,222, 33,225, 17,209, 45,237, 29,221 },
    { 162, 98,146, 82,174,110,158, 94,161, 97,145, 81,173,109,157, 93 },
    {  10,202, 58,250,  6,198, 54,246,  9,201, 57,249,  5,197, 53,245 },
    { 138, 74,186,122,134, 70,182,118,137, 73,185,121,133, 69,181,117 },
    {  42,234, 26,218, 38,230, 22,214, 41,233, 25,217, 37,229, 21,213 },
    { 170,106,154, 90,166,102,150, 86,169,105,153, 89,165,101,149, 85 }
  };
  // Prepare tables
  if (!dither_ok)
  {
    int i, j;
    for (i=0; i<16; i++)
      for (j=0; j<16; j++)
        dither[i][j] = ((255 - 2*dither[i][j]) * 0x33) / 512;    
    j = -0x33;
    for (i=0x19; i<256; i+=0x33)
      while (j <= i)
        quant[j++] = i-0x19;
    assert(i-0x19 == 0xff);
    while (j< 256+0x33)
      quant[j++] = i-0x19;
    dither_ok = 1;
  }
  // Go dithering
  for (int y=0; y<nrows; y++)
  {
    GPixel *pix = (*this)[y];
    for (int x=0; x<ncolumns; x++, pix++)
    {
      pix->r = quant[ pix->r + dither[(x+xmin+0)&0xf][(y+ymin+0)&0xf] ];
      pix->g = quant[ pix->g + dither[(x+xmin+5)&0xf][(y+ymin+11)&0xf] ];
      pix->b = quant[ pix->b + dither[(x+xmin+11)&0xf][(y+ymin+5)&0xf] ];
    }
  }
}

void
GPixmap::ordered_32k_dither(int xmin, int ymin)
{
  static unsigned char quantize[256+8+8];
  static unsigned char *quant = quantize + 8;
  static char  dither_ok = 0;
  static short dither[16][16] = 
  {
    {   0,192, 48,240, 12,204, 60,252,  3,195, 51,243, 15,207, 63,255 },
    { 128, 64,176,112,140, 76,188,124,131, 67,179,115,143, 79,191,127 },
    {  32,224, 16,208, 44,236, 28,220, 35,227, 19,211, 47,239, 31,223 },
    { 160, 96,144, 80,172,108,156, 92,163, 99,147, 83,175,111,159, 95 },
    {   8,200, 56,248,  4,196, 52,244, 11,203, 59,251,  7,199, 55,247 },
    { 136, 72,184,120,132, 68,180,116,139, 75,187,123,135, 71,183,119 },
    {  40,232, 24,216, 36,228, 20,212, 43,235, 27,219, 39,231, 23,215 },
    { 168,104,152, 88,164,100,148, 84,171,107,155, 91,167,103,151, 87 },
    {   2,194, 50,242, 14,206, 62,254,  1,193, 49,241, 13,205, 61,253 },
    { 130, 66,178,114,142, 78,190,126,129, 65,177,113,141, 77,189,125 },
    {  34,226, 18,210, 46,238, 30,222, 33,225, 17,209, 45,237, 29,221 },
    { 162, 98,146, 82,174,110,158, 94,161, 97,145, 81,173,109,157, 93 },
    {  10,202, 58,250,  6,198, 54,246,  9,201, 57,249,  5,197, 53,245 },
    { 138, 74,186,122,134, 70,182,118,137, 73,185,121,133, 69,181,117 },
    {  42,234, 26,218, 38,230, 22,214, 41,233, 25,217, 37,229, 21,213 },
    { 170,106,154, 90,166,102,150, 86,169,105,153, 89,165,101,149, 85 }
  };
  // Prepare tables
  if (!dither_ok)
  {
    int i, j;
    for (i=0; i<16; i++)
      for (j=0; j<16; j++)
        dither[i][j] = ((255 - 2*dither[i][j]) * 8) / 512;    
    j = -8;
    for (i=3; i<256; i+=8)
      while (j <= i)
        quant[j++] = i;
    while (j<256+8)
      quant[j++] = 0xff;
    dither_ok = 1;
  }
  // Go dithering
  for (int y=0; y<nrows; y++)
  {
    GPixel *pix = (*this)[y];
    for (int x=0; x<ncolumns; x++, pix++)
    {
      pix->r = quant[ pix->r + dither[(x+xmin+0)&0xf][(y+ymin+0)&0xf] ];
      pix->g = quant[ pix->g + dither[(x+xmin+5)&0xf][(y+ymin+11)&0xf] ];
      pix->b = quant[ pix->b + dither[(x+xmin+11)&0xf][(y+ymin+5)&0xf] ];
    }
  }
}


//////////////////////////////////////////////////
// Upsample Downsample
//////////////////////////////////////////////////


void  
GPixmap::downsample(const GPixmap *src, int factor, const GRect *pdr)
{
  // check arguments
  GRect rect(0, 0, (src->columns()+factor-1)/factor, (src->rows()+factor-1)/factor);
  if (pdr != 0)
  {
    if (pdr->xmin < rect.xmin || 
        pdr->ymin < rect.ymin || 
        pdr->xmax > rect.xmax || 
        pdr->ymax > rect.ymax  )
      G_THROW( ERR_MSG("GPixmap.overflow1") );
    rect = *pdr;
  }

  // precompute inverse map
  static int invmap[256];
  static int invmapok = 0;
  if (! invmapok)
  {
    invmapok = 1;
    for (int i=1; i<(int)(sizeof(invmap)/sizeof(int)); i++)
      invmap[i] = 0x10000 / i;
  }
  
  // initialise pixmap
  init(rect.height(), rect.width(), 0);

  // determine starting and ending points in source rectangle
  int sy = rect.ymin * factor;
  int sxz = rect.xmin * factor;


  // loop over source rows
  const GPixel *sptr = (*src)[sy];
  GPixel *dptr = (*this)[0];
  for (int y=0; y<nrows; y++)
  {
    int sx = sxz;
    // loop over source columns
    for (int x=0; x<ncolumns; x++)
    {
      int r=0, g=0, b=0, s=0;
      // compute average bounds
      const GPixel *ksptr = sptr;
      int lsy = sy + factor;
      if (lsy > (int)src->rows())
        lsy = (int)src->rows();
      int lsx = sx + factor;
      if (lsx > (int)src->columns())
        lsx = (int)src->columns();
      // compute average
      for (int rsy=sy; rsy<lsy; rsy++)
      {
        for (int rsx = sx; rsx<lsx; rsx++)
        {
          r += ksptr[rsx].r;
          g += ksptr[rsx].g;
          b += ksptr[rsx].b;
          s += 1;
        }
        ksptr += src->rowsize();
      }
      // set pixel color
      if (s >= (int)(sizeof(invmap)/sizeof(int)))
      {
        dptr[x].r = r / s;
        dptr[x].g = g / s;
        dptr[x].b = b / s;
      }
      else
      {
        dptr[x].r = (r*invmap[s] + 0x8000) >> 16;
        dptr[x].g = (g*invmap[s] + 0x8000) >> 16;
        dptr[x].b = (b*invmap[s] + 0x8000) >> 16;
      }
      // next column
      sx = sx + factor;
    }
    // next row
    sy = sy + factor;
    sptr = sptr + factor * src->rowsize();
    dptr = dptr + rowsize();
  }
}

void  
GPixmap::upsample(const GPixmap *src, int factor, const GRect *pdr)
{
  // check arguments
  GRect rect(0, 0, src->columns()*factor, src->rows()*factor);
  if (pdr != 0)
  {
    if (pdr->xmin < rect.xmin || 
        pdr->ymin < rect.ymin || 
        pdr->xmax > rect.xmax || 
        pdr->ymax > rect.ymax  )
      G_THROW( ERR_MSG("GPixmap.overflow2") );
    rect = *pdr;
  }
  // initialise pixmap
  init(rect.height(), rect.width(), 0);
  // compute starting point in source rectangle
  int sy, sy1, sxz, sx1z;
  euclidian_ratio(rect.ymin, factor, sy, sy1);
  euclidian_ratio(rect.xmin, factor, sxz, sx1z);
  // loop over rows
  const GPixel *sptr = (*src)[sy];
  GPixel *dptr = (*this)[0];
  for (int y=0; y<nrows; y++)
  {
    // loop over columns
    int sx = sxz;
    int sx1 = sx1z;
    for (int x=0; x<ncolumns; x++)
    {
      dptr[x] = sptr[sx];
      // next column
      if (++sx1 >= factor)
      {
        sx1 = 0;
        sx += 1;
      }
    }
    // next row
    dptr += rowsize();
    if (++sy1 >= factor)
    {
      sy1 = 0;
      sptr += src->rowsize();
    }
  }
}


static inline void
downsample_4x4_to_3x3 (const GPixel *s, int sadd, GPixel *d, int dadd)
{
  const GPixel *x = s;
  const GPixel *y = x + sadd;
  d[0].b = ( 11*x[0].b + 2*(x[1].b + y[0].b ) + y[1].b  + 8) >> 4;
  d[0].g = ( 11*x[0].g + 2*(x[1].g + y[0].g ) + y[1].g  + 8) >> 4;
  d[0].r = ( 11*x[0].r + 2*(x[1].r + y[0].r ) + y[1].r  + 8) >> 4;
  d[1].b = ( 7*(x[1].b + x[2].b) + y[1].b + y[2].b + 8 )     >> 4;
  d[1].g = ( 7*(x[1].g + x[2].g) + y[1].g + y[2].g + 8 )     >> 4;
  d[1].r = ( 7*(x[1].r + x[2].r) + y[1].r + y[2].r + 8 )     >> 4;
  d[2].b = ( 11*x[3].b + 2*(x[2].b + y[3].b ) + y[2].b  + 8) >> 4;
  d[2].g = ( 11*x[3].g + 2*(x[2].g + y[3].g ) + y[2].g  + 8) >> 4;
  d[2].r = ( 11*x[3].r + 2*(x[2].r + y[3].r ) + y[2].r  + 8) >> 4;
  d = d + dadd;
  x = x + sadd + sadd;
  d[0].b = ( 7*(x[0].b + y[0].b) + x[1].b + y[1].b + 8 )     >> 4;
  d[0].g = ( 7*(x[0].g + y[0].g) + x[1].g + y[1].g + 8 )     >> 4;
  d[0].r = ( 7*(x[0].r + y[0].r) + x[1].r + y[1].r + 8 )     >> 4;
  d[1].b = ( x[2].b + y[2].b + x[1].b + y[1].b + 2 )         >> 2;
  d[1].g = ( x[2].g + y[2].g + x[1].g + y[1].g + 2 )         >> 2;
  d[1].r = ( x[2].r + y[2].r + x[1].r + y[1].r + 2 )         >> 2;
  d[2].b = ( 7*(x[3].b + y[3].b) + x[2].b + y[2].b + 8 )     >> 4;
  d[2].g = ( 7*(x[3].g + y[3].g) + x[2].g + y[2].g + 8 )     >> 4;
  d[2].r = ( 7*(x[3].r + y[3].r) + x[2].r + y[2].r + 8 )     >> 4;
  d = d + dadd;
  y = y + sadd + sadd;
  d[0].b = ( 11*y[0].b + 2*(y[1].b + x[0].b ) + x[1].b  + 8) >> 4;
  d[0].g = ( 11*y[0].g + 2*(y[1].g + x[0].g ) + x[1].g  + 8) >> 4;
  d[0].r = ( 11*y[0].r + 2*(y[1].r + x[0].r ) + x[1].r  + 8) >> 4;
  d[1].b = ( 7*(y[1].b + y[2].b) + x[1].b + x[2].b + 8 )     >> 4;
  d[1].g = ( 7*(y[1].g + y[2].g) + x[1].g + x[2].g + 8 )     >> 4;
  d[1].r = ( 7*(y[1].r + y[2].r) + x[1].r + x[2].r + 8 )     >> 4;
  d[2].b = ( 11*y[3].b + 2*(y[2].b + x[3].b ) + x[2].b  + 8) >> 4;
  d[2].g = ( 11*y[3].g + 2*(y[2].g + x[3].g ) + x[2].g  + 8) >> 4;
  d[2].r = ( 11*y[3].r + 2*(y[2].r + x[3].r ) + x[2].r  + 8) >> 4;
}


static inline void
upsample_2x2_to_3x3 (const GPixel *s, int sadd, GPixel *d, int dadd)
{
  const GPixel *x = s;
  const GPixel *y = x + sadd;
  d[0] = x[0];
  d[1].b = (x[0].b + x[1].b + 1) >> 1;
  d[1].g = (x[0].g + x[1].g + 1) >> 1;
  d[1].r = (x[0].r + x[1].r + 1) >> 1;
  d[2] = x[1];
  d = d + dadd;
  d[0].b = (x[0].b + y[0].b + 1) >> 1;
  d[0].g = (x[0].g + y[0].g + 1) >> 1;
  d[0].r = (x[0].r + y[0].r + 1) >> 1;
  d[1].b = (x[0].b + y[0].b + x[1].b + y[1].b + 2) >> 2;
  d[1].g = (x[0].g + y[0].g + x[1].g + y[1].g + 2) >> 2;
  d[1].r = (x[0].r + y[0].r + x[1].r + y[1].r + 2) >> 2;
  d[2].b = (x[1].b + y[1].b + 1) >> 1;
  d[2].g = (x[1].g + y[1].g + 1) >> 1;
  d[2].r = (x[1].r + y[1].r + 1) >> 1;
  d = d + dadd;
  d[0] = y[0];
  d[1].b = (y[0].b + y[1].b + 1) >> 1;
  d[1].g = (y[0].g + y[1].g + 1) >> 1;
  d[1].r = (y[0].r + y[1].r + 1) >> 1;
  d[2] = y[1];
}


static inline void
copy_to_partial(int w, int h,
                const GPixel *s, int sadd,
                GPixel *d, int dadd, int xmin, int xmax, int ymin, int ymax)
{
  int y = 0;
  while (y<ymin  && y<h)
    {
      y += 1;
      s += sadd;
      d += dadd;
    }
  while (y<ymax && y<h)
    {
      int x = (xmin>0 ? xmin : 0);
      while (x<w && x<xmax)
        {
          d[x] = s[x];
          x++;
        }
      y += 1;
      s += sadd;
      d += dadd;
    }
}


static inline void
copy_line(const GPixel *s, int smin, int smax,
          GPixel *d, int dmin, int dmax)
{
  int x = dmin;
  while (x < smin) 
  { 
    d[x] = s[smin]; 
    x++; 
  }
  while (x < dmax && x < smax)  
  { 
    d[x] = s[x]; 
    x++; 
  }
  while (x < dmax)              
  {
    d[x] = s[smax-1]; 
    x++; 
  }
}


static inline void
copy_from_partial(int w, int h,
                  const GPixel *s, int sadd, int xmin, int xmax, int ymin, int ymax,
                  GPixel *d, int dadd)
{
  int y = 0;
  s += (ymin>0 ? sadd * ymin : 0);
  while (y<ymin  && y<h)
    {
      copy_line(s, xmin, xmax, d, 0, w);
      y += 1;
      d += dadd;
    }
  while (y<ymax && y<h)
    {
      copy_line(s, xmin, xmax, d, 0, w);
      y += 1;
      s += sadd;
      d += dadd;
    }
  s -= sadd;
  while (y < h)
    {
      copy_line(s, xmin, xmax, d, 0, w);
      y += 1;
      d += dadd;
    }
}





void  
GPixmap::downsample43(const GPixmap *src, const GRect *pdr)
{
  // check arguments
  int srcwidth = src->columns();
  int srcheight = src->rows();
  int destwidth = (srcwidth * 3 + 3 ) / 4;
  int destheight = (srcheight * 3 + 3) / 4;
  GRect rect(0, 0, destwidth, destheight);
  if (pdr != 0)
  {
    if (pdr->xmin < rect.xmin || 
        pdr->ymin < rect.ymin || 
        pdr->xmax > rect.xmax || 
        pdr->ymax > rect.ymax  )
      G_THROW( ERR_MSG("GPixmap.overflow3") );
    rect = *pdr;
    destwidth = rect.width();
    destheight = rect.height();
  }
  // initialize pixmap
  init(destheight, destwidth, 0);

  // compute bounds
  int dxz, dy;   // location of bottomleft block in destination image
  int sxz, sy;   // location of bottomleft block in source image
  euclidian_ratio(rect.ymin, 3, sy, dy);
  euclidian_ratio(rect.xmin, 3, sxz, dxz);
  sxz = 4 * sxz;   
  sy  = 4 * sy;
  dxz = - dxz;
  dy  = - dy;

  // prepare variables
  int sadd = src->rowsize();
  int dadd = this->rowsize();
  const GPixel *sptr = (*src)[0]  + sy * sadd;
  GPixel *dptr = (*this)[0] + dy * dadd;
  int s4add = 4 * sadd;
  int d3add = 3 * dadd;

  // iterate over row blocks
  while (dy < destheight)
  {
    int sx = sxz;
    int dx = dxz;
    // iterate over column blocks
    while (dx < destwidth)
    {
      GPixel xin[16], xout[9];

      if (dx>=0 && dy>=0 && dx+3<=destwidth && dy+3<=destheight)
        {
          if (sx+4<=srcwidth && sy+4<=srcheight)
            {
              downsample_4x4_to_3x3(sptr+sx, sadd, dptr+dx, dadd);
            }
          else
            {
              copy_from_partial(4,4, sptr+sx,sadd,-sx,srcwidth-sx,-sy,srcheight-sy, xin,4);
              downsample_4x4_to_3x3(xin, 4, dptr+dx, dadd);
            }
        }
      else
        {
          if (sx+4<=srcwidth && sy+4<=srcheight)
            {
              downsample_4x4_to_3x3(sptr+sx, sadd, xout, 3);  
              copy_to_partial(3,3, xout, 3, dptr+dx, dadd,-dx,destwidth-dx,-dy,destheight-dy);
            }
          else
            {
              copy_from_partial(4,4, sptr+sx,sadd,-sx,srcwidth-sx,-sy,srcheight-sy, xin,4);
              downsample_4x4_to_3x3(xin, 4, xout, 3);  
              copy_to_partial(3,3, xout,3, dptr+dx,dadd,-dx,destwidth-dx,-dy,destheight-dy);
            }
        }
      // next column
      dx += 3;
      sx += 4;
    }
    // next row
    dy += 3;
    dptr += d3add;
    sy += 4;
    sptr += s4add;
  }
}


void  
GPixmap::upsample23(const GPixmap *src, const GRect *pdr)
{
  // check arguments
  int srcwidth = src->columns();
  int srcheight = src->rows();
  int destwidth = (srcwidth * 3 + 1 ) / 2;
  int destheight = (srcheight * 3 + 1) / 2;
  GRect rect(0, 0, destwidth, destheight);
  if (pdr != 0)
  {
    if (pdr->xmin < rect.xmin || 
        pdr->ymin < rect.ymin || 
        pdr->xmax > rect.xmax || 
        pdr->ymax > rect.ymax  )
      G_THROW( ERR_MSG("GPixmap.overflow4") );
    rect = *pdr;
    destwidth = rect.width();
    destheight = rect.height();
  }
  // initialize pixmap
  init(destheight, destwidth, 0);

  // compute bounds
  int dxz, dy;   // location of bottomleft block in destination image
  int sxz, sy;   // location of bottomleft block in source image
  euclidian_ratio(rect.ymin, 3, sy, dy);
  euclidian_ratio(rect.xmin, 3, sxz, dxz);
  sxz = 2 * sxz;   
  sy  = 2 * sy;
  dxz = - dxz;
  dy  = - dy;

  // prepare variables
  int sadd = src->rowsize();
  int dadd = this->rowsize();
  const GPixel *sptr = (*src)[0]  + sy * sadd;
  GPixel *dptr = (*this)[0] + dy * dadd;
  int s2add = 2 * sadd;
  int d3add = 3 * dadd;

  // iterate over row blocks
  while (dy < destheight)
  {
    int sx = sxz;
    int dx = dxz;
    // iterate over column blocks
    while (dx < destwidth)
    {
      GPixel xin[4], xout[9];

      if (dx>=0 && dy>=0 && dx+3<=destwidth && dy+3<=destheight)
      {
        if (sx+2<=srcwidth && sy+2<=srcheight)
        {
          upsample_2x2_to_3x3( sptr+sx, sadd, dptr+dx, dadd);
        }
        else
        {
          copy_from_partial(2, 2, sptr+sx, sadd, -sx, srcwidth-sx, -sy, srcheight-sy, xin, 2);
          upsample_2x2_to_3x3(xin, 2, dptr+dx, dadd);
        }
      }
      else
      {
        if (sx+2<=srcwidth && sy+2<=srcheight)
        {
          upsample_2x2_to_3x3( sptr+sx, sadd, xout, 3);  
          copy_to_partial(3,3, xout, 3, dptr+dx, dadd, -dx, destwidth-dx, -dy, destheight-dy);
        }
        else
        {
          copy_from_partial(2, 2, sptr+sx, sadd, -sx, srcwidth-sx, -sy, srcheight-sy, xin, 2);
          upsample_2x2_to_3x3(xin, 2, xout, 3);  
          copy_to_partial(3,3, xout, 3, dptr+dx, dadd, -dx, destwidth-dx, -dy, destheight-dy);
        }
      }
      // next column
      dx += 3;
      sx += 2;
    }
    // next row
    dy += 3;
    dptr += d3add;
    sy += 2;
    sptr += s2add;
  }
}


//////////////////////////////////////////////////
// Blitting and attenuating
//////////////////////////////////////////////////


static unsigned char clip[512];
static bool clipok = false;

static void
compute_clip()
{
  clipok = true;
  for (unsigned int i=0; i<sizeof(clip); i++)
    clip[i] = (i<256 ? i : 255);
}


void 
GPixmap::attenuate(const GBitmap *bm, int xpos, int ypos)
{
  // Check
  if (!bm) G_THROW( ERR_MSG("GPixmap.null_alpha") );
  // Compute number of rows and columns
  int xrows = mini(ypos + (int)bm->rows(), nrows) - maxi(0, ypos),
    xcolumns = mini(xpos + (int) bm->columns(), ncolumns) - maxi(0, xpos);
  if(xrows <= 0 || xcolumns <= 0)
    return;
  // Precompute multiplier map
  unsigned int multiplier[256];
  unsigned int maxgray = bm->get_grays() - 1;
  for (unsigned int i=0; i<maxgray ; i++)
    multiplier[i] = 0x10000 * i / maxgray;
  // Compute starting point
  const unsigned char *src = (*bm)[0] - mini(0,ypos)*bm->rowsize()-mini(0,xpos);
  GPixel *dst = (*this)[0] + maxi(0, ypos)*rowsize()+maxi(0, xpos);
  // Loop over rows
  for (int y=0; y<xrows; y++)
    {
      // Loop over columns
      for (int x=0; x<xcolumns; x++)
        {
          unsigned char srcpix = src[x];
          // Perform pixel operation
          if (srcpix > 0)
            {
              if (srcpix >= maxgray)
                {
                  dst[x].b = 0;
                  dst[x].g = 0;
                  dst[x].r = 0;
                }
              else
                {
                  unsigned int level = multiplier[srcpix];
                  dst[x].b -=  (dst[x].b * level) >> 16;
                  dst[x].g -=  (dst[x].g * level) >> 16;
                  dst[x].r -=  (dst[x].r * level) >> 16;
                }
            }
        }
      // Next line
      dst += rowsize();
      src += bm->rowsize();
    }
}


void 
GPixmap::blit(const GBitmap *bm, int xpos, int ypos, const GPixel *color)
{
  // Check
  if (!bm) G_THROW( ERR_MSG("GPixmap.null_alpha") );
  if (!clipok) compute_clip();
  if (!color) return;
  // Compute number of rows and columns
  int xrows = mini(ypos + (int)bm->rows(), nrows) - maxi(0, ypos),
    xcolumns = mini(xpos + (int) bm->columns(), ncolumns) - maxi(0, xpos);
  if(xrows <= 0 || xcolumns <= 0)
    return;
  // Precompute multiplier map
  unsigned int multiplier[256];
  unsigned int maxgray = bm->get_grays() - 1;
  for (unsigned int i=1; i<maxgray ; i++)
    multiplier[i] = 0x10000 * i / maxgray;
  // Cache target color
  unsigned char gr = color->r;
  unsigned char gg = color->g;
  unsigned char gb = color->b;
  // Compute starting point
  const unsigned char *src = (*bm)[0] - mini(0,ypos)*bm->rowsize()-mini(0,xpos);
  GPixel *dst = (*this)[0] + maxi(0, ypos)*rowsize()+maxi(0, xpos);
  // Loop over rows
  for (int y=0; y<xrows; y++)
    {
      // Loop over columns
      for (int x=0; x<xcolumns; x++)
        {
          unsigned char srcpix = src[x];
          // Perform pixel operation
          if (srcpix > 0)
            {
              if (srcpix >= maxgray)
                {
                  dst[x].b = clip[dst[x].b + gb];
                  dst[x].g = clip[dst[x].g + gg];
                  dst[x].r = clip[dst[x].r + gr];
                }
              else
                {
                  unsigned int level = multiplier[srcpix];
                  dst[x].b = clip[dst[x].b + ((gb * level) >> 16)];
                  dst[x].g = clip[dst[x].g + ((gg * level) >> 16)];
                  dst[x].r = clip[dst[x].r + ((gr * level) >> 16)];
                }
            }
        }
      // Next line
      dst += rowsize();
      src += bm->rowsize();
    }
}


void 
GPixmap::blit(const GBitmap *bm, int xpos, int ypos, const GPixmap *color)
{
  // Check
  if (!bm) G_THROW( ERR_MSG("GPixmap.null_alpha") );
  if (!color) G_THROW( ERR_MSG("GPixmap.null_color") );
  if (!clipok) compute_clip();
  if (bm->rows()!=color->rows() || bm->columns()!=color->columns())
    G_THROW( ERR_MSG("GPixmap.diff_size") );
  // Compute number of rows and columns
  int xrows = mini(ypos + (int)bm->rows(), nrows) - maxi(0, ypos),
      xcolumns = mini(xpos + (int) bm->columns(), ncolumns) - maxi(0, xpos);
  if(xrows <= 0 || xcolumns <= 0)
    return;
  // Precompute multiplier map
  unsigned int multiplier[256];
  unsigned int maxgray = bm->get_grays() - 1;
  for (unsigned int i=1; i<maxgray ; i++)
    multiplier[i] = 0x10000 * i / maxgray;
  // Cache target color
  // Compute starting point
  const unsigned char *src = (*bm)[0] - mini(0,ypos)*bm->rowsize()-mini(0,xpos);
  const GPixel *src2 = (*color)[0] + maxi(0, ypos)*color->rowsize()+maxi(0, xpos);
  GPixel *dst = (*this)[0] + maxi(0, ypos)*rowsize()+maxi(0, xpos);
  // Loop over rows
  for (int y=0; y<xrows; y++)
    {
      // Loop over columns
      for (int x=0; x<xcolumns; x++)
        {
          unsigned char srcpix = src[x];
          // Perform pixel operation
          if (srcpix > 0)
            {
              if (srcpix >= maxgray)
                {
                  dst[x].b = clip[dst[x].b + src2[x].b];
                  dst[x].g = clip[dst[x].g + src2[x].g];
                  dst[x].r = clip[dst[x].r + src2[x].r];
                }
              else
                {
                  unsigned int level = multiplier[srcpix];
                  dst[x].b = clip[dst[x].b + ((src2[x].b * level) >> 16)];
                  dst[x].g = clip[dst[x].g + ((src2[x].g * level) >> 16)];
                  dst[x].r = clip[dst[x].r + ((src2[x].r * level) >> 16)];
                }
            }
        }
      // Next line
      dst += rowsize();
      src += bm->rowsize();
      src2 += color->rowsize();
    }
}



void 
GPixmap::blend(const GBitmap *bm, int xpos, int ypos, const GPixmap *color)
{
  // Check
  if (!bm) G_THROW( ERR_MSG("GPixmap.null_alpha") );
  if (!color) G_THROW( ERR_MSG("GPixmap.null_color") );
  if (!clipok) compute_clip();
  if (bm->rows()!=color->rows() || bm->columns()!=color->columns())
    G_THROW( ERR_MSG("GPixmap.diff_size") );
  // Compute number of rows and columns
  int xrows = mini(ypos + (int)bm->rows(), nrows) - maxi(0, ypos),
      xcolumns = mini(xpos + (int) bm->columns(), ncolumns) - maxi(0, xpos);
  if(xrows <= 0 || xcolumns <= 0)
    return;
  // Precompute multiplier map
  unsigned int multiplier[256];
  unsigned int maxgray = bm->get_grays() - 1;
  for (unsigned int i=1; i<maxgray ; i++)
    multiplier[i] = 0x10000 * i / maxgray;
  // Cache target color
  // Compute starting point
  const unsigned char *src = (*bm)[0] - mini(0,ypos)*bm->rowsize()-mini(0,xpos);
  const GPixel *src2 = (*color)[0] + maxi(0, ypos)*color->rowsize()+maxi(0, xpos);
  GPixel *dst = (*this)[0] + maxi(0, ypos)*rowsize()+maxi(0, xpos);
  // Loop over rows
  for (int y=0; y<xrows; y++)
    {
      // Loop over columns
      for (int x=0; x<xcolumns; x++)
        {
          unsigned char srcpix = src[x];
          // Perform pixel operation
          if (srcpix > 0)
            {
              if (srcpix >= maxgray)
                {
                  dst[x].b = src2[x].b;
                  dst[x].g = src2[x].g;
                  dst[x].r = src2[x].r;
                }
              else
                {
                  unsigned int level = multiplier[srcpix];
                  dst[x].b -= (((int)dst[x].b - (int)src2[x].b) * level) >> 16;
                  dst[x].g -= (((int)dst[x].g - (int)src2[x].g) * level) >> 16;
                  dst[x].r -= (((int)dst[x].r - (int)src2[x].r) * level) >> 16;
                }
            }
        }
      // Next line
      dst += rowsize();
      src += bm->rowsize();
      src2 += color->rowsize();
    }
}




void 
GPixmap::stencil(const GBitmap *bm, 
                const GPixmap *pm, int pms, const GRect *pmr, 
                 double corr, GPixel white)
{
  // Check arguments
  GRect rect(0, 0, pm->columns()*pms, pm->rows()*pms);
  if (pmr != 0)
    {
      if (pmr->xmin < rect.xmin || 
          pmr->ymin < rect.ymin || 
          pmr->xmax > rect.xmax || 
          pmr->ymax > rect.ymax  )
        G_THROW( ERR_MSG("GPixmap.overflow5") );
      rect = *pmr;
    }
  // Compute number of rows
  int xrows = nrows;
  if ((int)bm->rows() < xrows)
    xrows = bm->rows();
  if (rect.height() < xrows)
    xrows = rect.height();
  // Compute number of columns
  int xcolumns = ncolumns;
  if ((int)bm->columns() < xcolumns)
    xcolumns = bm->columns();
  if (rect.width() < xcolumns)
    xcolumns = rect.width();
  // Precompute multiplier map
  unsigned int multiplier[256];
  unsigned int maxgray = bm->get_grays() - 1;
  for (unsigned int i=1; i<maxgray ; i++)
    multiplier[i] = 0x10000 * i / maxgray;
  // Prepare color correction table
  unsigned char gtable[256][3];
  color_correction_table_cache(corr, white, gtable);
  // Compute starting point in blown up foreground pixmap
  int fgy, fgy1, fgxz, fgx1z;
  euclidian_ratio(rect.ymin, pms, fgy, fgy1);
  euclidian_ratio(rect.xmin, pms, fgxz, fgx1z);
  const GPixel *fg = (*pm)[fgy];
  const unsigned char *src = (*bm)[0];
  GPixel *dst = (*this)[0];
  // Loop over rows
  for (int y=0; y<xrows; y++)
  {
    // Loop over columns
    int fgx = fgxz;
    int fgx1 = fgx1z;
    for (int x=0; x<xcolumns; x++)
    {
      unsigned char srcpix = src[x];
      // Perform pixel operation
      if (srcpix > 0)
      {
        if (srcpix >= maxgray)
        {
          dst[x].b = gtable[fg[fgx].b][0];
          dst[x].g = gtable[fg[fgx].g][1];
          dst[x].r = gtable[fg[fgx].r][2];
        }
        else
        {
          unsigned int level = multiplier[srcpix];
          dst[x].b -= (((int)dst[x].b-(int)gtable[fg[fgx].b][0])*level) >> 16;
          dst[x].g -= (((int)dst[x].g-(int)gtable[fg[fgx].g][1])*level) >> 16;
          dst[x].r -= (((int)dst[x].r-(int)gtable[fg[fgx].r][2])*level) >> 16;
        }
      }
      // Next column
      if (++fgx1 >= pms)
      {
        fgx1 = 0;
        fgx += 1;
      }
    }
    // Next line
    dst += rowsize();
    src += bm->rowsize();
    if (++fgy1 >= pms)
    {
      fgy1 = 0;
      fg += pm->rowsize();
    } 
  }
}

void 
GPixmap::stencil(const GBitmap *bm, 
                const GPixmap *pm, int pms, const GRect *pmr, 
                double corr)
{
  stencil(bm, pm, pms, pmr, corr, GPixel::WHITE);
}


GP<GPixmap> GPixmap::rotate(int count)
{
  GP<GPixmap> newpixmap(this);
  count = count & 3;
  if(count)
  {
    if( count&0x01)
      newpixmap = new GPixmap(ncolumns, nrows);
    else
      newpixmap = new GPixmap(nrows, ncolumns);

    GPixmap &dpixmap = *newpixmap;

    GMonitorLock lock(&pixmap_monitor());
    switch(count)
    {
    case 3: //// rotate 90 counter clockwise
        {
            int lastrow = dpixmap.rows()-1;

            for(int y=0; y<nrows; y++)
            {
                const GPixel *r=operator [] (y);
                for(int x=0,xnew=lastrow; xnew>=0; x++,xnew--)
                {
                    dpixmap[xnew][y] = r[x];
                }
            }
        }
        break;
    case 2: //// rotate 180 counter clockwise
        {
            int lastrow = dpixmap.rows()-1;
            int lastcolumn = dpixmap.columns()-1;

            for(int y=0,ynew=lastrow; ynew>=0; y++,ynew--)
            {
                const GPixel *r=operator [] (y);
                GPixel *d=dpixmap[ynew];
                for(int xnew=lastcolumn; xnew>=0; r++,xnew--)
                {
                    d[xnew] = *r;
                }
            }
        }
        break;
    case 1: //// rotate 270 counter clockwise
        {
            int lastcolumn = dpixmap.columns()-1;

            for(int y=0,ynew=lastcolumn; ynew>=0; y++,ynew--)
            {
                const GPixel *r=operator [] (y);
                for(int x=0; x<ncolumns; x++)
                {
                    dpixmap[x][ynew] = r[x];
                }
            }
        }
        break;
    }
  }
  return newpixmap;
}



#ifdef HAVE_NAMESPACES
}
# ifndef NOT_USING_DJVU_NAMESPACE
using namespace DJVU;
# endif
#endif

