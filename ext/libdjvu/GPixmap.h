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

#ifndef _GPIXMAP_H_
#define _GPIXMAP_H_
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#if NEED_GNUG_PRAGMAS
# pragma interface
#endif

/** @name GPixmap.h

    Files #"GPixmap.h"# and #"GPixmap.cpp"# implement class \Ref{GPixmap}.
    Instances of this class represent color images.  Each RGB pixel is
    represented by structure \Ref{GPixel}. The ``bottom left'' coordinate system
    is used consistently in the DjVu library.  Line zero of a GPixmap is the
    bottom line in the color image.  Pixels are organized from left to right
    within each line.
    
    {\bf ToDo} --- More sophisticated color correction schemes. 
    
    @memo
    Generic support for color images.
    @author
    L\'eon Bottou <leonb@research.att.com>
*/
//@{


#include "GSmartPointer.h"

#ifdef HAVE_NAMESPACES
namespace DJVU {
# ifdef NOT_DEFINED // Just to fool emacs c++ mode
}
#endif
#endif


class GBitmap;
class GRect;
class ByteStream;


/** Color pixel as a RGB triple.  
    The colors are represented using three bytes named #r#, #g# and #b#.  The
    value of these bytes represent additive amounts of light.  Color white is
    represented by setting all three bytes to #255#.  Color black is
    represented by setting all three bytes to #0#.  This convention should not
    be confused with the convention adopted for class \Ref{GBitmap} where the
    pixel values represent an ink level.  */

struct DJVUAPI GPixel
{
  /** Blue component. */
  unsigned char b;
  /** Green component. */
  unsigned char g;
  /** Red component. */
  unsigned char r;
  /** Returns true iff colors are identical. */
  friend int operator==(const GPixel & p1, const GPixel & p2);
  /** Returns true iff colors are different. */
  friend int operator!=(const GPixel & p1, const GPixel & p2);
  /** Returns a hash code for the color. */
  friend unsigned int hash(const GPixel &p);
  /** @name Predefined colors. */
  //@{ 
  /// GPixel::WHITE is initialized to #rgb:255/255/255#.
  static const GPixel WHITE; 
  /// GPixel::BLACK is initialized to #rgb:0/0/0#.
  static const GPixel BLACK; 
  /// GPixel::BLUE is initialized to #rgb:0/0/255#.
  static const GPixel BLUE;  
  /// GPixel::GREEN is initialized to #rgb:0/255/0#.
  static const GPixel GREEN; 
  /// GPixel::RED is initialized to #rgb:255/0/0#.
  static const GPixel RED;
  //@}
};


/** RGB Color images.  
    Instances of class #GPixmap# represent color images as a two dimensional
    array of pixels \Ref{GPixel}.  The bracket operator returns a pointer to
    the pixels composing one line of the image.  This pointer can be used as
    an array to read or write the pixels of this particular line.  Following
    the general convention of the DjVu Reference Library, line zero is always
    the bottom line of the image.
 */

class DJVUAPI GPixmap : public GPEnabled
{
protected:
  GPixmap(void);
  GPixmap(int nrows, int ncolumns, const GPixel *filler=0);
  GPixmap(const GBitmap &ref);
  GPixmap(const GBitmap &ref, const GRect &rect);
  GPixmap(const GPixmap &ref);
  GPixmap(const GPixmap &ref, const GRect &rect);
  GPixmap(ByteStream &ref);

public:
  /// Virtual destructor.
  virtual ~GPixmap();

  void destroy(void);
  /** @name Construction. */
  //@{
  /** Creates an empty GBitmap object.  The returned GPixmap has zero rows
      and zero columns.  Use function \Ref{init} to change the size of the
      image. */
  static GP<GPixmap> create(void) {return new GPixmap();}

  /** Creates a GPixmap with #nrows# rows and #ncolumns# columns.  When the
      optional argument #filler# is specified, all pixels are initialized 
      with the corresponding color. */
  static GP<GPixmap> create(
    const int nrows, const int ncolumns, const GPixel *filler=0)
  { return new GPixmap(nrows,ncolumns,filler); }

  /** Creates a GPixmap by copying the gray level image #ref#.
      The constructed GPixmap has the same size as #ref#.  The pixels
      are initialized with shades of grays copied from #ref#. */
  static GP<GPixmap> create(const GBitmap &ref)
  { return new GPixmap(ref); }

  /** Creates a GPixmap by copying the rectangle #rect# of the gray level
      image #ref#.  The constructed GPixmap has the same size as rectangle
      #rect#.  The pixels are initialized with shades of grays converted from
      the ink levels represented in #ref#.  This conversion depends on the
      number of gray levels in #ref#. */
  static GP<GPixmap> create(const GBitmap &ref, const GRect &rect)
  { return new GPixmap(ref,rect); }

  /** Copy constructors. Creates a GPixmap by replicating the size and the
      contents of GPixmap #ref#. */
  static GP<GPixmap> create(const GPixmap &ref)
  { return new GPixmap(ref); }

  /** Creates a GPixmap by copying the rectangle #rect# of the color image #ref#.
      The constructed GPixmap has the same size as rectangle #rect#.
      The pixels are initialized with colors copied from #ref#. */
  static GP<GPixmap> create(const GPixmap &ref, const GRect &rect)
  { return new GPixmap(ref,rect); }

  /** Creates a GPixmap by reading PPM data from ByteStream #ref#.
      See \Ref{PNM and RLE file formats} for more information. */
  static GP<GPixmap> create(ByteStream &ref)
  { return new GPixmap(ref); }

  //@}

  /** @name Initialization. */
  //@{
  /** Resets the GPixmap to #nrows# rows and #ncolumns# columns.  When the
      optional argument #filler# is specified, all pixels are initialized with
      the corresponding color.  The previous content of the GPixmap is discarded. */
  void init(int nrows, int ncolumns,  const GPixel *filler=0);
  /** Resets the GPixmap by copying the size and the contents of the color
      image #ref#.  The previous content of the GPixmap is discarded. */
  void init(const GPixmap &ref);
  /** Resets the GPixmap by copying the rectangle #rect# of the color image #ref#.
      The previous content of the GPixmap is discarded. */
  void init(const GPixmap &ref, const GRect &rect);
  /** Resets the GPixmap by copying the size and the contents of the gray 
      level image #ref#.  The optional argument #ramp# is an array of 256 
      pixel values used for mapping the gray levels to color values. 
      Setting #ramp# to zero selects a linear ramp of shades of gray. */
  void init(const GBitmap &ref, const GPixel *ramp=0);
  /** Resets the GPixmap by copying the rectangle #rect# of the gray level
      image #ref#.  The optional argument #ramp# is an array of 256 pixel
      values used for mapping the gray levels to color values.  Setting #ramp#
      to zero selects a linear ramp computed according to the maximal number
      of gray levels in #ref#. */
  void init(const GBitmap &ref, const GRect &rect, const GPixel *ramp=0);
  /** Resets the GPixmap by reading PPM data from ByteStream #ref#.  See
      \Ref{PNM and RLE file formats} for more information. */
  void init(ByteStream &ref);
  /** Resets the GPixmap by copying the gray level image #ref#.  The pixels
      are initialized with shades of grays copied from #ref#. */
  GPixmap& operator=(const GBitmap &ref);
  /** Copy operator. Resets the GPixmap by copying the size and the contents
      of the color image #ref#.  The previous content of the GPixmap is
      discarded. */
  GPixmap& operator=(const GPixmap &ref);
  //@}

  /** @name Accessing pixels. */
  //@{
  /** Returns the number of rows (the image height). */
  unsigned int rows() const;
  /** Returns the number of columns (the image width). */
  unsigned int columns() const;
  /** Returns a constant pointer to the first GPixel in row #row#.  This
      pointer can be used as an array to read the row elements. */
  const GPixel * operator[] (int row) const;
  /** Returns a pointer to the first GPixel in row #row#.  This pointer can be
      used as an array to read or write the row elements. */
  GPixel * operator[] (int row);
  /** Returns the length (in pixels) of a row in memory.  This number is equal
      to the difference between pointers to pixels located in the same column
      in consecutive rows.  This difference may be larger than the number of
      columns in the image. */
  unsigned int rowsize() const;
  //@}

  /** @name Resampling images. */
  //@{
  /** Resets this GPixmap with a subsampled segment of color image #src#.
      This function conceptually rescales image #src# by a factor #1:factor#,
      and copies rectangle #rect# of the subsampled image into the current GPixmap.
      The full subsampled image is copied if #rect# is a null pointer.
      Both operations are however performed together for efficiency reasons.
      Subsampling works by averaging the colors of the source pixels located
      in small squares of size #factor# times #factor#. */
  void downsample(const GPixmap *src, int factor, const GRect *rect=0);
  /** Resets this GPixmap with a oversampled segment of color image #src#.
      This function conceptually rescales image #src# by a factor #factor:1#,
      and copies rectangle #rect# of the oversampled image into the current
      GPixmap.  The full oversampled image is copied if #rect# is a null
      pointer.  Both operations are however performed together for efficiency
      reasons.  Oversampling works by replicating the color of the source
      pixels into squares of size #factor# times #factor#. */
  void upsample(const GPixmap *src, int factor, const GRect *rect=0);
  /** Resets this GPixmap with a rescaled segment of #src# (zoom 75%).  This
      function conceptually rescales image #src# by a factor #3:4#, and copies
      rectangle #rect# of the rescaled image into the current GPixmap.  The
      full rescaled image is copied if #rect# is a null pointer.  Both
      operations are however performed together for efficiency reasons.  This
      function has been superseded by class \Ref{GPixmapScaler}. */
  void downsample43(const GPixmap *src, const GRect *rect=0); 
  /** Resets this GPixmap with a rescaled segment of #src# (zoom 150%).  This
      function conceptually rescales image #src# by a factor #3:2# and copies
      rectangle #rect# of the rescaled image into the current GPixmap.  The
      full rescaled image is copied if #rect# is a null pointer.  Both
      operations are however performed together for efficiency reasons.  This
      function has been superseded by class \Ref{GPixmapScaler}. */
  void upsample23(const GPixmap *src, const GRect *rect=0);
  //@}

  /** @name Blitting and applying stencils.  
      These function is essential for rendering DjVu images.  The elementary
      functions are \Ref{attenuate} and \Ref{blit}.  The combined functions
      \Ref{blend} and \Ref{stencil} should be viewed as optimizations.  */
  //@{
  /** Attenuates the color image in preparation for a blit.  
      Bitmap #bm# is positionned at location #x#,#y# over this color image.
      The matching color image pixels are then multiplied by #1.0-Alpha# where
      #Alpha# denotes the gray value, in range #[0,1]#, represented by the
      corresponding pixel of bitmap #bm#. */
  void attenuate(const GBitmap *bm, int x, int y);
  /** Blits solid color #color# through transparency mask #bm#.  
      Bitmap #bm# is positionned at location #x#,#y# over this color image.
      The matching color image pixels are then modified by adding color
      #color# multiplied by #Alpha#, where #Alpha# denotes the gray value, in
      range #[0,1]#, represented by the corresponding pixel of bitmap #bm#. */
  void blit(const GBitmap *bm, int x, int y, const GPixel *color);
  /** Blits pixmap #color# through transparency mask #bm#.
      Bitmap #bm# is positionned at location #x#,#y# over this color image.
      The matching color image pixels are then modified by adding the
      corresponding pixel color in pixmap #color#, multiplied by #Alpha#,
      where #Alpha# denotes the gray value, in range #[0,1]#, represented by
      the corresponding pixel of bitmap #bm#. */
  void blit(const GBitmap *bm, int x, int y, const GPixmap *color);
  /** Performs alpha blending. This function is similar to first calling
      \Ref{attenuate} with alpha map #bm# and then calling \Ref{blit} with
      alpha map #bm# and color map #color#. Both operations are performed
      together for efficiency reasons. */
  void blend(const GBitmap *bm, int x, int y, const GPixmap *color);
  /** Resample color pixmap and performs color corrected alpha blending.  This
      function conceptually computes an intermediate color image by first
      upsampling the GPixmap #pm# by a factor #pms:1# (see \Ref{upsample}),
      extracting the sub-image designated by rectangle #pmr# and applying
      color correction #corr# (see \Ref{color_correct}).  This intermediate
      color image is then blended into this pixel map according to the alpha
      map #bm# (see \Ref{blend}). */
  void stencil(const GBitmap *bm, 
               const GPixmap *pm, int pms, 
               const GRect *pmr, double corr, GPixel white);
  void stencil(const GBitmap *bm, 
               const GPixmap *pm, int pms, 
               const GRect *pmr, double corr=1.0);
  //@}
  
  /** @name Manipulating colors. */
  //@{
  /** Dithers the image to 216 colors.  This function applies an ordered
      dithering algorithm to reduce the image to 216 predefined colors.  These
      predefined colors are located on a color cube of 6x6x6 colors: the color
      RGB coordinates can only take the following values: #0#, #51#, #102#,
      #163#, #214# or #255#.  This is useful for displaying images on a device
      supporting a maximum of 256 colors. Arguments #xmin# and #ymin# control
      the position of the dithering grids.  This is useful for dithering tiled
      images. Arguments #xmin# and #ymin# must be the position of the bottom
      left corner of the tile contained in this GPixmap. Properly setting
      these arguments eliminates dithering artifacts on the tile
      boundaries. */
  void ordered_666_dither(int xmin=0, int ymin=0);
  /** Dithers the image to 32768 colors.  This function applies an ordered
      dithering algorithm to reduce the image to 32768 predefined colors.
      These predefined colors are located on a color cube of 32x32x32 colors:
      the color RGB coordinates can only take values in which the three least
      significant bits are set to #1#.  This is useful for displaying images
      with less than 24 bits per pixel.  Arguments #xmin# and #ymin# control
      the position of the dithering grids.  This is useful for dithering tiled
      images. Arguments #xmin# and #ymin# must be the position of the bottom
      left corner of the tile contained in this GPixmap. Properly setting
      these arguments eliminates dithering artifacts on the tile
      boundaries. */
  void ordered_32k_dither(int xmin=0, int ymin=0);
  /** Applies a luminance gamma correction factor of #corr#.  
      Values greater than #1.0# make the image brighter.  
      Values smaller than #1.0# make the image darker.  
      The documentation of program \Ref{ppmcoco} explains how to
      properly use this function. */
  void color_correct(double corr);
  void color_correct(double corr, GPixel white);
  /** Applies a luminance gamma correction to an array of pixels. 
      This function is {\em static} and does not modify this pixmap. */
  static void color_correct(double corr, GPixel *pix, int npix);
  static void color_correct(double corr, GPixel white, GPixel *pix, int npix);

  //@}
  
  /** @name Miscellaneous. */
  //@{
  /** Returns the number of bytes allocated for this image. */
  inline unsigned int get_memory_usage() const;
  /** Saves the image into ByteStream #bs# using the PPM format.
      Argument #raw# selects the ``Raw PPM'' (1) or the ``Ascii PPM'' (0) format.
      See \Ref{PNM and RLE file formats} for more information. */
  void save_ppm(ByteStream &bs, int raw=1) const;
  //@}

  /** @name Stealing or borrowing the memory buffer (advanced). */
  //@{
  /** Steals the memory buffer of a GPixmap.  This function returns the
      address of the memory buffer allocated by this GPixmap object.  The
      offset of the first pixel in the bottom line is written into variable
      #offset#.  Other lines can be accessed using pointer arithmetic (see
      \Ref{rowsize}).  The GPixmap object no longer ``owns'' the buffer: you
      must explicitly de-allocate the buffer using #operator delete []#.  This
      de-allocation should take place after the destruction or the
      re-initialization of the GPixmap object.  This function will return a
      null pointer if the GPixmap object does not ``own'' the buffer in the
      first place.  */
  GPixel *take_data(size_t &offset);
  /** Initializes this GPixmap by borrowing a memory segment.  The GPixmap
      then directly addresses the memory buffer #data# provided by the user.
      This buffer must be large enough to hold #w*h# GPixels.  The GPixmap
      object does not ``own'' the buffer: you must explicitly de-allocate the
      buffer using #operator delete []#.  This de-allocation should take place
      after the destruction or the re-initialization of the GPixmap object.  */
  inline void borrow_data(GPixel &data, int w, int h); 
  /// Identical to the above, but GPixmap will do the delete []. 
  void donate_data(GPixel *data, int w, int h); 
  
  /** Rotates pixmap by 90, 180 or 270 degrees anticlockwise
      and returns a new pixmap, input pixmap is not changed. 
      count can be 1, 2, or 3 for 90, 180, 270 degree rotation.
      It returns the same pixmap if not rotated. */
  GP<GPixmap> rotate(int count=0);

  //@}
  
  // Please ignore these two functions. Their only purpose is to allow
  // DjVu viewer compile w/o errors. eaf. 
  // Is this still useful ?. lyb.
  int get_grays(void) const { return 256; };
  void set_grays(int) {};\
  
protected:
  // data
  unsigned short nrows;
  unsigned short ncolumns;
  unsigned short nrowsize;
  GPixel *pixels;
  GPixel *pixels_data;
  friend class DjVu_PixImage;
};

//@}

// INLINE --------------------------


inline int 
operator==(const GPixel & p1, const GPixel & p2)
{
  return p1.r==p2.r && p1.g==p2.g && p1.b==p2.b;
}

inline int 
operator!=(const GPixel & p1, const GPixel & p2)
{
  return p1.r!=p2.r || p1.g!=p2.g || p1.b!=p2.b;
}

inline unsigned int 
hash(const GPixel &p)
{
  unsigned int x = (p.b<<16)|(p.g<<8)|(p.r);
  return x ^ (p.b<<4) ^ (p.r<<12);
}

inline unsigned int
GPixmap::rows() const
{
  return nrows;
}

inline unsigned int
GPixmap::columns() const
{
  return ncolumns;
}

inline unsigned int
GPixmap::rowsize() const
{
  return nrowsize;
}

inline GPixel *
GPixmap::operator[](int row)
{
  if (row<0 || row>=nrows || !pixels) return 0;
  return &pixels[row * nrowsize];
}

inline const GPixel *
GPixmap::operator[](int row) const
{
  if (row<0 || row>=nrows) return 0;
  return &pixels[row * nrowsize];
}

inline GPixmap & 
GPixmap::operator=(const GBitmap &ref)
{
  init(ref);
  return *this;
}

inline GPixmap & 
GPixmap::operator=(const GPixmap &ref)
{
  init(ref);
  return *this;
}

inline void
GPixmap::borrow_data(GPixel &data, int w, int h)
{
  donate_data(&data,w,h);
  pixels_data=0;
}

//////////////////////////////////////////////////
// Memory usage
//////////////////////////////////////////////////


inline unsigned int 
GPixmap::get_memory_usage() const
{
  return  sizeof(GPixmap)+(nrows * ncolumns * sizeof(GPixel));
}

// ---------------------------------

#ifdef HAVE_NAMESPACES
}
# ifndef NOT_USING_DJVU_NAMESPACE
using namespace DJVU;
# endif
#endif
#endif


