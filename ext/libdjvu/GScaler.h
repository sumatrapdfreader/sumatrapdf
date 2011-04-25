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

#ifndef _GSCALER_H_
#define _GSCALER_H_
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#if NEED_GNUG_PRAGMAS
# pragma interface
#endif

// From: Leon Bottou, 1/31/2002
// Almost equal to my initial code.

#include "GException.h"
#include "GRect.h"
#include "GBitmap.h"
#include "GPixmap.h"

#ifdef HAVE_NAMESPACES
namespace DJVU {
# ifdef NOT_DEFINED // Just to fool emacs c++ mode
}
#endif
#endif


/** @name GScaler.h 

    Files #"GScaler.h"# and #"GScaler.cpp"# implement a fast bilinear
    interpolation scheme to rescale a \Ref{GBitmap} or a \Ref{GPixmap}.
    Common setup functions are implemented by the base class \Ref{GScaler}.
    The actual function for rescaling a gray level image is implemented by
    class \Ref{GBitmapScaler}.  The actual function for rescaling a color
    image is implemented by class \Ref{GPixmapScaler}.

    {\bf Remark} --- The bilinear interpolation code relies on fixed precision
    tables.  It becomes suboptimal when upsampling (i.e. zooming into) an
    image by a factor greater than eight.  High contrast images displayed at
    high magnification may contain visible jaggies.

    @memo
    Rescaling images with bilinear interpolation.
    @author
    L\'eon Bottou <leonb@research.att.com>
*/
//@{


/** Base class for GBitmapScaler and GPixmapScaler.  This base class
    implements the common elements of class \Ref{GBitmapScaler} and
    \Ref{GPixmapScaler}.  Functions \Ref{set_input_size} and
    \Ref{set_output_size} are used to specify the size of the input image and
    the size of the output image.  Functions \Ref{set_horz_ratio} and
    \Ref{set_vert_ratio} may be used to override the scaling ratios computed
    from the image sizes.  You can then call function \Ref{get_input_rect} to
    know which pixels of the input image are necessary to compute a specified
    rectangular zone of the output image.  The actual computation is then
    performed by calling function #scale# in class \Ref{GBitmapScaler} and
    \Ref{GPixmapScaler}.  
*/
class DJVUAPI GScaler  : public GPEnabled
{
protected:  
  GScaler();
public:
  virtual ~GScaler();
  /** Sets the size of the input image. Argument #w# (resp. #h#) contains the
      horizontal (resp. vertical) size of the input image.  This size is used
      to initialize the internal data structures of the scaler object. */
  void set_input_size(int w, int h);
  /** Sets the size of the output image. Argument #w# (resp. #h#) contains the
      horizontal (resp. vertical) size of the output image. This size is used
      to initialize the internal data structures of the scaler object. */
  void set_output_size(int w, int h);
  /** Sets the horizontal scaling ratio #numer/denom#.  This function may be
      used to force an exact scaling ratio.  The scaling ratios are otherwise
      derived from the sizes of the input and output images. */
  void set_horz_ratio(int numer, int denom);
  /** Sets the vertical scaling ratio to #numer/denom#.  This function may be
      used to force an exact scaling ratio.  The scaling ratios are otherwise
      derived from the sizes of the input and output images. */
  void set_vert_ratio(int numer, int denom);
  /** Computes which input pixels are required to compute specified output
      pixels.  Let us assume that we only need a part of the output
      image. This part is defined by rectangle #desired_output#.  Only a part
      of the input image is necessary to compute the output pixels.  Function
      #get_input_rect# computes the coordinates of that part of the input
      image, and stores them into rectangle #required_input#.  */
  void get_input_rect( const GRect &desired_output, GRect &required_input );
protected:
  // The sizes
  int inw, inh;
  int xshift, yshift;
  int redw, redh;
  int outw, outh;
  // Fixed point coordinates
  int *vcoord;
  GPBuffer<int> gvcoord;
  int *hcoord;
  GPBuffer<int> ghcoord;
  // Helper
  void make_rectangles(const GRect &desired, GRect &red, GRect &inp);
};



/** Fast rescaling code for gray level images.  This class augments the base
    class \Ref{GScaler} with a function for rescaling gray level
    images.  Function \Ref{GBitmapScaler::scale} computes an arbitrary segment
    of the output image given the corresponding pixels in the input image.

    {\bf Example} --- The following functions returns an gray level image
    (sixteen gray levels, size #nw# by #nh#) containing a rescaled version of
    the input image #in#.
    \begin{verbatim}
    GBitmap *rescale_bitmap(const GBitmap &in, int nw, int nh)
    {
      int w = in.columns();       // Get input width
      int h = in.raws();          // Get output width
      GBitmapScaler scaler(w,h,nw,nh);  // Creates bitmap scaler
      GRect desired(0,0,nw,nh);   // Desired output = complete bitmap
      GRect provided(0,0,w,h);    // Provided input = complete bitmap
      GBitmap *out = new GBitmap;
      scaler.scale(provided, in, desired, *out);  // Rescale
      out->change_grays(16);      // Reduce to 16 gray levels
      return out;
    }
    \end{verbatim} */
class DJVUAPI GBitmapScaler : public GScaler
{
protected:
  GBitmapScaler(void);
  GBitmapScaler(int inw, int inh, int outw, int outh);
public:
  /// Virtual destructor.
  virtual ~GBitmapScaler();

  /** Creates an empty GBitmapScaler. You must call functions
      \Ref{GScaler::set_input_size} and \Ref{GScaler::set_output_size} before
      calling any of the scaling functions. */
  static GP<GBitmapScaler> create(void) {return new GBitmapScaler(); }

  /** Creates a GBitmapScaler. The size of the input image is given by
      #inw# and #inh#.  This function internally calls
      \Ref{GScaler::set_input_size} and \Ref{GScaler::set_output_size}. The
      size of the output image is given by #outw# and #outh#.  . */
  static GP<GBitmapScaler> create(
    const int inw, const int inh, const int outw, const int outh)
  { return new GBitmapScaler(inw,inh,outw,outh); }

  /** Computes a segment of the rescaled output image.  The GBitmap object
      #output# is overwritten with the segment of the output image specified
      by the rectangle #desired_output#.  The rectangle #provided_input#
      specifies which segment of the input image is provided by the GBitmap
      object #input#.  An exception \Ref{GException} is thrown if the
      rectangle #provided_input# is smaller then the rectangle
      #required_input# returned by function \Ref{GScaler::get_input_rect}.
      Note that the output image always contain 256 gray levels. You may want
      to use function \Ref{GBitmap::change_grays} to reduce the number of gray
      levels. */
  void scale( const GRect &provided_input, const GBitmap &input,
              const GRect &desired_output, GBitmap &output );
protected:
  // Helpers
  unsigned char *get_line(int, const GRect &, const GRect &, const GBitmap &);
  // Temporaries
  unsigned char *lbuffer;
  GPBuffer<unsigned char> glbuffer;
  unsigned char *conv;
  GPBuffer<unsigned char> gconv;
  unsigned char *p1;
  GPBuffer<unsigned char> gp1;
  unsigned char *p2;
  GPBuffer<unsigned char> gp2;
  int l1;
  int l2;
};


/** Fast rescaling code for color images.  This class augments the base class
    \Ref{GScaler} with a function for rescaling color images.  Function
    \Ref{GPixmapScaler::scale} computes an arbitrary segment of the output
    image given the corresponding pixels in the input image.

    {\bf Example} --- The following functions returns a color image
    of size #nw# by #nh# containing a rescaled version of
    the input image #in#.
    \begin{verbatim}
    GPixmap *rescale_pixmap(const GPixmap &in, int nw, int nh)
    {
      int w = in.columns();       // Get input width
      int h = in.raws();          // Get output width
      GPixmapScaler scaler(w,h,nw,nh);  // Creates bitmap scaler
      GRect desired(0,0,nw,nh);   // Desired output = complete image
      GRect provided(0,0,w,h);    // Provided input = complete image
      GPixmap *out = new GPixmap;
      scaler.scale(provided, in, desired, *out);  // Rescale
      return out;
    }
    \end{verbatim}

 */
class DJVUAPI GPixmapScaler : public GScaler
{
protected:
  GPixmapScaler(void);
  GPixmapScaler(int inw, int inh, int outw, int outh);
public:
  /// Virtual destructor.
  virtual ~GPixmapScaler();

  /** Creates an empty GPixmapScaler. You must call functions
      \Ref{GScaler::set_input_size} and \Ref{GScaler::set_output_size} before
      calling any of the scaling functions. */
  static GP<GPixmapScaler> create(void) {return new GPixmapScaler(); }

  /** Creates a GPixmapScaler. The size of the input image is given by
      #inw# and #inh#.  This function internally calls
      \Ref{GScaler::set_input_size} and \Ref{GScaler::set_output_size}. The
      size of the output image is given by #outw# and #outh#.  . */
  static GP<GPixmapScaler> create(
    const int inw, const int inh, const int outw, const int outh)
  { return new GPixmapScaler(inw,inh,outw,outh); }

  /** Computes a segment of the rescaled output image.  The pixmap #output# is
      overwritten with the segment of the output image specified by the
      rectangle #desired_output#.  The rectangle #provided_input# specifies
      which segment of the input image is provided in the pixmap #input#.  An
      exception \Ref{GException} is thrown if the rectangle #provided_input#
      is smaller then the rectangle #required_input# returned by function
      \Ref{GScaler::get_input_rect}. */
  void scale( const GRect &provided_input, const GPixmap &input,
              const GRect &desired_output, GPixmap &output );
protected:
  // Helpers
  GPixel *get_line(int, const GRect &, const GRect &, const GPixmap &);
  // Temporaries
  GPixel *lbuffer;
  GPBuffer<GPixel> glbuffer;
  GPixel *p1;
  GPBuffer<GPixel> gp1;
  GPixel *p2;
  GPBuffer<GPixel> gp2;
  int    l1;
  int    l2;
};





//@}
    



// -------- END

#ifdef HAVE_NAMESPACES
}
# ifndef NOT_USING_DJVU_NAMESPACE
using namespace DJVU;
# endif
#endif
#endif
