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

#ifndef _DJVUPALETTE_H_
#define _DJVUPALETTE_H_
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#if NEED_GNUG_PRAGMAS
# pragma interface
#endif


#include "GContainer.h"
#include "GPixmap.h"
#include <string.h>


#ifdef HAVE_NAMESPACES
namespace DJVU {
# ifdef NOT_DEFINED // Just to fool emacs c++ mode
}
#endif
#endif


/** @name DjVuPalette.h
    Files #"DjVuPalette.h"# and #"DjVuPalette.cpp"# implement a single class
    \Ref{DjVuPalette} which provides facilities for computing optimal color
    palettes, coding color palettes, and coding sequences of color indices.
    @memo 
    DjVuPalette header file
    @author: 
    L\'eon Bottou <leonb@research.att.com> */
//@{


/** Computing and coding color palettes and index arrays.
    This class provides facilities for computing optimal color palettes,
    coding color palettes, and coding sequences of color indices.
    
    {\bf Creating a color palette} -- The recipe for creating a color palette
    consists in (a) creating a DjVuPalette object, (b) constructing a color
    histogram using \Ref{histogram_add}, and (c) calling function
    \Ref{compute_palette}.

    {\bf Accessing the color palette} -- Conversion between colors and color
    palette indices is easily achieved with \Ref{color_to_index} and
    \Ref{index_to_color}.  There are also functions for computing a palette
    and quantizing a complete pixmap.

    {\bf Sequences of color indices} -- The DjVuPalette object also contains
    an array \Ref{colordata} optionally containing a sequence of color
    indices.  This array will be encoded and decoded by functions \Ref{encode}
    and \Ref{decode}.  This feature simplifies the implementation of the ``one
    color per symbol'' model in DjVu.

    {\bf Coding color palettes and color indices} -- Two functions
    \Ref{encode} and \Ref{decode} are provided for coding the color palette
    and the array of color indices when appropriate.  */
#ifdef _WIN32_WCE_EMULATION         // Work around odd behavior under WCE Emulation
#define CALLINGCONVENTION __cdecl
#else
#define CALLINGCONVENTION  /* */
#endif

class DJVUAPI DjVuPalette : public GPEnabled
{
protected:
  DjVuPalette(void);
public:
  /// Generic creator
  static GP<DjVuPalette> create(void) {return new DjVuPalette();}

  /// Non-virtual destructor
  ~DjVuPalette();
  // COPY
  DjVuPalette(const DjVuPalette &ref);
  DjVuPalette& operator=(const DjVuPalette &ref);
  // PALETTE COMPUTATION
  /** Resets the color histogram to zero. */
  void histogram_clear();
  /** Adds the color specified by #p# to the histogram.
      Argument #weight# represent the number of pixels with this color. */
  void histogram_add(const GPixel &p, int weight);
  /** Adds the color specified by the triple #bgr# to the histogram.
      Argument #weight# represent the number of pixels with this color. */
  void histogram_add(const unsigned char *bgr, int weight);
  /** Adds the color specified by the weighted triple #bgr# to the histogram.
      Argument #weight# represent the number of pixels with this color.  This
      function will compute the actual color by dividing the elements of the
      #bgr# array by #weight# and then use the unnormalized values to compute
      the average color per bucket.  This is all a way to avoid excessive loss
      of accuracy. */
  void histogram_norm_and_add(const int *bgr, int weight);
  /** Computes an optimal palette for representing an image where colors
      appear according to the histogram.  Argument #maxcolors# is the maximum
      number of colors allowed in the palette (up to 1024).  Argument
      #minboxsize# controls the minimal size of the color cube area affected
      to a color palette entry.  Returns the index of the dominant color. */
  int compute_palette(int maxcolors, int minboxsize=0);
  /** Computes the optimal palette for pixmap #pm#.  This function builds the
      histogram for pixmap #pm# and computes the optimal palette using
      \Ref{compute_palette}. */
  int compute_pixmap_palette(const GPixmap &pm, int ncolors, int minboxsize=0);
  // CONVERSION
  /** Returns the number of colors in the palette. */
  int size() const;
  /** Returns the best palette index for representing color #p#. */
  int color_to_index(const GPixel &p);
  /** Returns the best palette index for representing color #bgr#. */
  int color_to_index(const unsigned char *bgr);
  /** Overwrites #p# with the color located at position #index# in the palette. */
  void index_to_color(int index, GPixel &p) const;
  /** Overwrites #rgb[0..3]# with the color located at 
      position #index# in the palette. */
  void index_to_color(int index, unsigned char *bgr) const;
  /** Quantizes pixmap #pm#. All pixels are replaced by their closest
      approximation available in the palette. */
  void quantize(GPixmap &pm);
  /** Calls \Ref{compute_pixmap_palette} and \Ref{quantize}. */
  int compute_palette_and_quantize(GPixmap &pm, int maxcolors, int minboxsize=0);
  // COLOR CORRECTION
  /** Applies a luminance gamma correction factor of #corr# to the palette
      entries.  Values greater than #1.0# make the image brighter.  Values
      smaller than #1.0# make the image darker.  The documentation of program
      \Ref{ppmcoco} explains how to properly use this function. */
  void color_correct(double corr);
  // COLOR INDEX DATA
  /** Contains an optional sequence of color indices. 
      Function \Ref{encode} and \Ref{decode} also encode and decode this
      sequence when such a sequence is provided. */
  GTArray<short> colordata;
  /** Returns colors from the color index sequence.  Pixel #out# is
      overwritten with the color corresponding to the #nth# element of the
      color sequence \Ref{colordata}. */
  void get_color(int nth, GPixel &out) const;
  // CODING
  /** Writes the palette colors.  This function writes each palette color as a
      RGB triple into bytestream #bs#. */
  void encode_rgb_entries(ByteStream &bs) const;
  /** Reads palette colors.  This function initializes the palette colors by
      reading #palettesize# RGB triples from bytestream #bs#. */
  void decode_rgb_entries(ByteStream &bs, const int palettesize);
  /** Encodes the palette and the color index sequence. This function encodes
      the a version byte, the palette size, the palette colors and the color
      index sequence into bytestream #bs#.  Note that the color histogram is
      never saved. */
  void encode(GP<ByteStream> bs) const;
  /** Initializes the object by reading data from bytestream #bs#.  This
      function reads a version byte, the palette size, the palette and the
      color index sequence from bytestream #bs#.  Note that the color
      histogram is never saved. */
  void decode(GP<ByteStream> bs);

private:
  // Histogram
  int mask;
  GMap<int,int> *hist;
  // Quantization data
  struct PColor { unsigned char p[4]; };
  GTArray<PColor> palette;
  GMap<int,int> *pmap;
  // Helpers
  void allocate_hist();
  void allocate_pmap();
  static int CALLINGCONVENTION bcomp (const void*, const void*);
  static int CALLINGCONVENTION gcomp (const void*, const void*);
  static int CALLINGCONVENTION rcomp (const void*, const void*);
  static int CALLINGCONVENTION lcomp (const void*, const void*);
  int color_to_index_slow(const unsigned char *bgr);
private: // dummy functions
  static void encode(ByteStream *);
  static void decode(ByteStream *);
};


//@}

// ------------ INLINES


inline void 
DjVuPalette::histogram_clear()
{
  delete hist;
  hist = 0;
  mask = 0;
}

inline void 
DjVuPalette::histogram_add(const unsigned char *bgr, int weight)
{
  if (weight > 0)
    {
      if (!hist || hist->size()>=0x4000) 
        allocate_hist();
      int key = (bgr[0]<<16)|(bgr[1]<<8)|(bgr[2])|(mask);
      (*hist)[key] += weight;
    }
}  

inline void 
DjVuPalette::histogram_add(const GPixel &p, int weight)
{
  histogram_add(&p.b, weight);
}

inline void 
DjVuPalette::histogram_norm_and_add(const int *bgr, int weight)
{
  if (weight > 0)
    {
      int p0 = bgr[0]/weight; if (p0>255) p0=255;
      int p1 = bgr[1]/weight; if (p1>255) p1=255;
      int p2 = bgr[2]/weight; if (p2>255) p2=255;
      if (!hist || hist->size()>=0x4000) 
        allocate_hist();
      int key = (p0<<16)|(p1<<8)|(p2)|(mask);
      (*hist)[key] += weight;
    }
}

inline int
DjVuPalette::size() const
{
  return palette.size();
}

inline int 
DjVuPalette::color_to_index(const unsigned char *bgr)
{
  if (! pmap)
    allocate_pmap();
  int key = (bgr[0]<<16)|(bgr[1]<<8)|(bgr[2]);
  GPosition p = pmap->contains(key);
  if ( p)
    return (*pmap)[p];
  return color_to_index_slow(bgr);
}

inline int 
DjVuPalette::color_to_index(const GPixel &p)
{
  return color_to_index(&p.b);
}

inline void 
DjVuPalette::index_to_color(int index, unsigned char *bgr) const
{
  const PColor &color = palette[index];
  bgr[0] = color.p[0];
  bgr[1] = color.p[1];
  bgr[2] = color.p[2];
}

inline void 
DjVuPalette::index_to_color(int index, GPixel &p) const
{
  index_to_color(index, &p.b);
}

inline void
DjVuPalette::get_color(int nth, GPixel &p) const
{
  index_to_color(colordata[nth], p);
}



// ------------ THE END

#ifdef HAVE_NAMESPACES
}
# ifndef NOT_USING_DJVU_NAMESPACE
using namespace DJVU;
# endif
#endif
#endif
      
      
             

    
