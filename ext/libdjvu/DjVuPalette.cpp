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

#include "GException.h"
#include "ByteStream.h"
#include "BSByteStream.h"
#include "DjVuPalette.h"

#include <stddef.h>
#include <stdlib.h>
#include <math.h>


#ifdef HAVE_NAMESPACES
namespace DJVU {
# ifdef NOT_DEFINED // Just to fool emacs c++ mode
}
#endif
#endif


#define CUBEBITS  4
#define CUBESIDE  (1<<CUBEBITS)
#define CUBESIZE  (CUBESIDE*CUBESIDE*CUBESIDE)

#define RMUL 5
#define GMUL 9
#define BMUL 2
#define SMUL (RMUL+GMUL+BMUL)

#define MAXPALETTESIZE 65535 // Limit for a 16 bit unsigned read.

#define fmin fltmin // clash with existing fmin

inline unsigned char 
umax(unsigned char a, unsigned char b) 
{ return (a>b) ? a : b; }

inline unsigned char 
umin(unsigned char a, unsigned char b) 
{ return (a>b) ? b : a; }

inline float 
fmin(float a, float b) 
{ return (a>b) ? b : a; }



// ------- DJVUPALETTE


DjVuPalette::DjVuPalette()
  : mask(0), hist(0), pmap(0)
{
}

DjVuPalette::~DjVuPalette()
{
  delete hist;
  delete pmap;
}

DjVuPalette& 
DjVuPalette::operator=(const DjVuPalette &ref)
{
  if (this != &ref)
    {
      delete hist;
      delete pmap;
      mask = 0;
      palette = ref.palette;
      colordata = ref.colordata;
    }
  return *this;
}

DjVuPalette::DjVuPalette(const DjVuPalette &ref)
  : mask(0), hist(0), pmap(0)
{
  this->operator=(ref);
}



// -------- HISTOGRAM ALLOCATION

void
DjVuPalette::allocate_hist()
{
  if (! hist)
    {
      hist = new GMap<int,int>;
      mask = 0;
    }
  else
    {
      GMap<int,int> *old = hist;
      hist = new GMap<int,int>;
      mask = (mask<<1)|(0x010101);
      for (GPosition p = *old; p; ++p)
        {
          int k = old->key(p);
          int w = (*old)[p];
          (*hist)[k | mask] += w;
        }
      delete old;
    }
}


// -------- PALETTE COMPUTATION


#ifndef NEED_DECODER_ONLY

struct PData
{
  unsigned char p[3];
  int w;
};

struct PBox 
{
  PData *data;
  int colors;
  int boxsize;
  int sum;
};
int
DjVuPalette::bcomp (const void *a, const void *b)
{
  return ((PData*)a)->p[0] - ((PData*)b)->p[0];
}

int

DjVuPalette::gcomp (const void *a, const void *b)
{
  return ((PData*)a)->p[1] - ((PData*)b)->p[1];
}

int
DjVuPalette::rcomp (const void *a, const void *b)
{
  return ((PData*)a)->p[2] - ((PData*)b)->p[2];
}

int
DjVuPalette::lcomp (const void *a, const void *b)
{
  unsigned char *aa = ((PColor*)a)->p;
  unsigned char *bb = ((PColor*)b)->p;
  if (aa[3] != bb[3])
    return aa[3]-bb[3];
  else if (aa[2] != bb[2])
    return aa[2]-bb[2];
  else if (aa[1] != bb[1])
    return aa[1]=bb[1];
  else
    return aa[0]-bb[0];
}

int
DjVuPalette::compute_palette(int maxcolors, int minboxsize)
{
  if (!hist)
    G_THROW( ERR_MSG("DjVuPalette.no_color") );
  if (maxcolors<1 || maxcolors>MAXPALETTESIZE)
    G_THROW( ERR_MSG("DjVuPalette.many_colors") );
  
  // Paul Heckbert: "Color Image Quantization for Frame Buffer Display", 
  // SIGGRAPH '82 Proceedings, page 297.  (also in ppmquant)
  
  // Collect histogram colors
  int sum = 0;
  int ncolors = 0;
  GTArray<PData> pdata;
  { // extra nesting for windows
    for (GPosition p = *hist; p; ++p)
    {
      pdata.touch(ncolors);
      PData &data = pdata[ncolors++];
      int k = hist->key(p);
      data.p[0] = (k>>16) & 0xff;
      data.p[1] = (k>>8) & 0xff;
      data.p[2] = (k) & 0xff;
      data.w = (*hist)[p];
      sum += data.w;
    }
  }
  // Create first box
  GList<PBox> boxes;
  PBox newbox;
  newbox.data = pdata;
  newbox.colors = ncolors;
  newbox.boxsize = 256;
  newbox.sum = sum;
  boxes.append(newbox);
  // Repeat spliting boxes
  while (boxes.size() < maxcolors)
    {
      // Find suitable box
      GPosition p;
      for (p=boxes; p; ++p)
        if (boxes[p].colors>=2 && boxes[p].boxsize>minboxsize) 
          break;
      if (! p)
        break;
      // Find box boundaries
      PBox &splitbox = boxes[p];
      unsigned char pmax[3];
      unsigned char pmin[3];
      pmax[0] = pmin[0] = splitbox.data->p[0];
      pmax[1] = pmin[1] = splitbox.data->p[1];
      pmax[2] = pmin[2] = splitbox.data->p[2];
      { // extra nesting for windows
        for (int j=1; j<splitbox.colors; j++)
        {
          pmax[0] = umax(pmax[0], splitbox.data[j].p[0]);
          pmax[1] = umax(pmax[1], splitbox.data[j].p[1]);
          pmax[2] = umax(pmax[2], splitbox.data[j].p[2]);
          pmin[0] = umin(pmin[0], splitbox.data[j].p[0]);
          pmin[1] = umin(pmin[1], splitbox.data[j].p[1]);
          pmin[2] = umin(pmin[2], splitbox.data[j].p[2]);
        }
      }
      // Determine split direction and sort
      int bl = pmax[0]-pmin[0]; 
      int gl = pmax[1]-pmin[1];
      int rl = pmax[2]-pmin[2];
      splitbox.boxsize = (bl>gl ? (rl>bl ? rl : bl) : (rl>gl ? rl : gl));
      if (splitbox.boxsize <= minboxsize)
        continue;
      if (gl == splitbox.boxsize)
        qsort(splitbox.data, splitbox.colors, sizeof(PData), gcomp);
      else if (rl == splitbox.boxsize)
        qsort(splitbox.data, splitbox.colors, sizeof(PData), rcomp);
      else
        qsort(splitbox.data, splitbox.colors, sizeof(PData), bcomp);
      // Find median
      int lowercolors = 0;
      int lowersum = 0;
      while (lowercolors<splitbox.colors-1 && lowersum+lowersum<splitbox.sum)
        lowersum += splitbox.data[lowercolors++].w;
      // Compute new boxes
      newbox.data = splitbox.data + lowercolors;
      newbox.colors = splitbox.colors - lowercolors;
      newbox.sum = splitbox.sum - lowersum;
      splitbox.colors = lowercolors;
      splitbox.sum = lowersum;
      // Insert boxes at proper location
      GPosition q;
      for (q=p; q; ++q)
        if (boxes[q].sum < newbox.sum)
          break;
      boxes.insert_before(q, newbox);
      for (q=p; q; ++q)
        if (boxes[q].sum < splitbox.sum)
          break;
      boxes.insert_before(q, boxes, p);
    }
  // Fill palette array
  ncolors = 0;
  palette.empty();
  palette.resize(0,boxes.size()-1);
  { // extra nesting for windows
    for (GPosition p=boxes; p; ++p)
    {
      PBox &box = boxes[p];
      // Compute box representative color
      float bsum = 0;
      float gsum = 0;
      float rsum = 0;
      for (int j=0; j<box.colors; j++)
        {
          float w = (float)box.data[j].w;
          bsum += box.data[j].p[0] * w;
          gsum += box.data[j].p[1] * w;
          rsum += box.data[j].p[2] * w;
        }
      PColor &color = palette[ncolors++];
      color.p[0] = (unsigned char) fmin(255, bsum/box.sum);
      color.p[1] = (unsigned char) fmin(255, gsum/box.sum);
      color.p[2] = (unsigned char) fmin(255, rsum/box.sum);
      color.p[3] = ( color.p[0]*BMUL + color.p[1]*GMUL + color.p[2]*RMUL) / SMUL;
    }
  }
  // Save dominant color
  PColor dcolor = palette[0];
  // Sort palette colors in luminance order
  qsort((PColor*)palette, ncolors, sizeof(PColor), lcomp);
  // Clear invalid data
  colordata.empty();
  delete pmap;
  pmap = 0;
  // Return dominant color
  return color_to_index_slow(dcolor.p);
}



int 
DjVuPalette::compute_pixmap_palette(const GPixmap &pm, int ncolors, int minboxsize)
{
  // Prepare histogram
  histogram_clear();
  { // extra nesting for windows
    for (int j=0; j<(int)pm.rows(); j++)
    {
      const GPixel *p = pm[j];
      for (int i=0; i<(int)pm.columns(); i++)
        histogram_add(p[i], 1);
    }
  }
  // Compute palette
  return compute_palette(ncolors, minboxsize);
}


#endif




// -------- QUANTIZATION


void
DjVuPalette::allocate_pmap()
{
  if (! pmap)
    pmap = new GMap<int,int>;
}

int 
DjVuPalette::color_to_index_slow(const unsigned char *bgr)
{
  PColor *pal = palette;
  const int ncolors = palette.size();
  if (! ncolors)
    G_THROW( ERR_MSG("DjVuPalette.not_init") );
  // Should be able to do better
  int found = 0;
  int founddist = 3*256*256;
  { // extra nesting for windows
    for (int i=0; i<ncolors; i++)
    {
      int bd = bgr[0] - pal[i].p[0];
      int gd = bgr[1] - pal[i].p[1];
      int rd = bgr[2] - pal[i].p[2];
      int dist = (bd*bd)+(gd*gd)+(rd*rd);
      if (dist < founddist)
        {
          found = i;
          founddist = dist;
        }
    }
  }
  // Store in pmap
  if (pmap && pmap->size()<0x8000)
    {
      int key = (bgr[0]<<16)|(bgr[1]<<8)|(bgr[2]);
      (*pmap)[key] = found;
    }
  // Return
  return found;
}


#ifndef NEED_DECODER_ONLY

void 
DjVuPalette::quantize(GPixmap &pm)
{
  { // extra nesting for windows
    for (int j=0; j<(int)pm.rows(); j++)
    {
      GPixel *p = pm[j];
      for (int i=0; i<(int)pm.columns(); i++)
        index_to_color(color_to_index(p[i]), p[i]);
    }
  }
}

int 
DjVuPalette::compute_palette_and_quantize(GPixmap &pm, int maxcolors, int minboxsize)
{
  int result = compute_pixmap_palette(pm, maxcolors, minboxsize);
  quantize(pm);
  return result;
}

void 
DjVuPalette::color_correct(double corr)
{
  const int palettesize = palette.size();
  if (palettesize > 0)
    {
      // Copy colors
      int i;
      GTArray<GPixel> pix(0,palettesize-1);
      GPixel *r = pix;
      PColor *q = palette;
      for (i=0; i<palettesize; i++) 
        {
          r[i].b = q[i].p[0];
          r[i].g = q[i].p[1];
          r[i].r = q[i].p[2];
        }
      // Apply color correction
      GPixmap::color_correct(corr, r, palettesize);
      // Restore colors
      for (i=0; i<palettesize; i++) 
        {
          q[i].p[0] = r[i].b;
          q[i].p[1] = r[i].g;
          q[i].p[2] = r[i].r;
        }
    }
}

#endif


// -------- ENCODE AND DECODE

#define DJVUPALETTEVERSION 0

void
DjVuPalette::encode_rgb_entries(ByteStream &bs) const
{
  const int palettesize = palette.size();
  { // extra nesting for windows
    for (int c=0; c<palettesize; c++)
    {
      unsigned char p[3];
      p[2] = palette[c].p[0];
      p[1] = palette[c].p[1];
      p[0] = palette[c].p[2];
      bs.writall((const void*)p, 3);
    }
  }
}

void 
DjVuPalette::encode(GP<ByteStream> gbs) const
{
  ByteStream &bs=*gbs;
  const int palettesize = palette.size();
  const int datasize = colordata.size();
  // Code version number
  int version = DJVUPALETTEVERSION;
  if (datasize>0) version |= 0x80;
  bs.write8(version);
  // Code palette
  bs.write16(palettesize);
  { // extra nesting for windows
    for (int c=0; c<palettesize; c++)
    {
      unsigned char p[3];
      p[0] = palette[c].p[0];
      p[1] = palette[c].p[1];
      p[2] = palette[c].p[2];
      bs.writall((const void*)p, 3);
    }
  }
  // Code colordata
  if (datasize > 0)
    {
      bs.write24(datasize);
      GP<ByteStream> gbsb=BSByteStream::create(gbs, 50);
      ByteStream &bsb=*gbsb;
      for (int d=0; d<datasize; d++)
        bsb.write16(colordata[d]);
    }
}

void 
DjVuPalette::decode_rgb_entries(ByteStream &bs, const int palettesize)
{
  palette.resize(0,palettesize-1);
  { // extra nesting for windows
    for (int c=0; c<palettesize; c++)
    {
      unsigned char p[3];
      bs.readall((void*)p, 3);
      palette[c].p[0] = p[2];
      palette[c].p[1] = p[1];
      palette[c].p[2] = p[0];
      palette[c].p[3] = (p[0]*BMUL+p[1]*GMUL+p[2]*RMUL)/SMUL;
    }
  }
}

void 
DjVuPalette::decode(GP<ByteStream> gbs)
{
  ByteStream &bs=*gbs;
  // Make sure that everything is clear
  delete hist;
  delete pmap;
  hist = 0;
  pmap = 0;
  mask = 0;
  // Code version
  int version = bs.read8();
  if ( (version & 0x7f) != DJVUPALETTEVERSION)
    G_THROW( ERR_MSG("DjVuPalette.bad_version") );
  // Code palette
  const int palettesize = bs.read16();
  if (palettesize<0 || palettesize>MAXPALETTESIZE)
    G_THROW( ERR_MSG("DjVuPalette.bad_palette") );
  palette.resize(0,palettesize-1);
  { // extra nesting for windows
    for (int c=0; c<palettesize; c++)
    {
      unsigned char p[3];
      bs.readall((void*)p, 3);
      palette[c].p[0] = p[0];
      palette[c].p[1] = p[1];
      palette[c].p[2] = p[2];
      palette[c].p[3] = (p[0]*BMUL+p[1]*GMUL+p[2]*RMUL)/SMUL;
    }
  }
  // Code data
  if (version & 0x80)
    {
      int datasize = bs.read24();
      if (datasize<0)
        G_THROW( ERR_MSG("DjVuPalette.bad_palette") );
      colordata.resize(0,datasize-1);
      GP<ByteStream> gbsb=BSByteStream::create(gbs);
      ByteStream &bsb=*gbsb;
      { // extra nesting for windows
        for (int d=0; d<datasize; d++)
        {
          short s = bsb.read16();
          if (s<0 || s>=palettesize)
            G_THROW( ERR_MSG("DjVuPalette.bad_palette") );        
          colordata[d] = s;
        }
      }
    }
}




#ifdef HAVE_NAMESPACES
}
# ifndef NOT_USING_DJVU_NAMESPACE
using namespace DJVU;
# endif
#endif

