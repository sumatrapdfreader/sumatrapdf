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

#ifndef _GBITMAP_H_
#define _GBITMAP_H_
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#if NEED_GNUG_PRAGMAS
# pragma interface
#endif


#include "GSmartPointer.h"
#ifndef NDEBUG
#include "GException.h"
#endif

#ifdef HAVE_NAMESPACES
namespace DJVU {
# ifdef NOT_DEFINED // Just to fool emacs c++ mode
}
#endif
#endif


class GRect;
class GMonitor;
class ByteStream;

/** @name GBitmap.h

    Files #"GBitmap.h"# and #"GBitmap.cpp"# implement class \Ref{GBitmap}.
    Instances of this class represent bilevel or gray-level images. The
    ``bottom left'' coordinate system is used consistently in the DjVu library.
    Line zero of a bitmap is the bottom line in the bitmap.  Pixels are
    organized from left to right within each line.  As suggested by its name,
    class #GBitmap# was initially a class for bilevel images only.  It was
    extended to handle gray-level images when arose the need to render
    anti-aliased images.  This class has been a misnomer since then.

    {\bf ToDo} --- Class #GBitmap# can internally represent bilevel images
    using a run-length encoded representation.  Some algorithms may benefit
    from a direct access to this run information.

    @memo
    Generic support for bilevel and gray-level images.
    @author
    L\'eon Bottou <leonb@research.att.com>

 */
//@{


/** Bilevel and gray-level images.  Instances of class #GBitmap# represent
    bilevel or gray-level images.  Images are usually represented using one
    byte per pixel.  Value zero represents a white pixel.  A value equal to
    the number of gray levels minus one represents a black pixel.  The number
    of gray levels is returned by the function \Ref{get_grays} and can be
    manipulated by the functions \Ref{set_grays} and \Ref{change_grays}.

    The bracket operator returns a pointer to the bytes composing one line of
    the image.  This pointer can be used to read or write the image pixels.
    Line zero represents the bottom line of the image.

    The memory organization is setup in such a way that you can safely read a
    few pixels located in a small border surrounding all four sides of the
    image.  The width of this border can be modified using the function
    \Ref{minborder}.  The border pixels are initialized to zero and therefore
    represent white pixels.  You should never write anything into border
    pixels because they are shared between images and between lines.  */

class DJVUAPI GBitmap : public GPEnabled
{
protected:
  GBitmap(void);
  GBitmap(int nrows, int ncolumns, int border=0);
  GBitmap(const GBitmap &ref);
  GBitmap(const GBitmap &ref, int border);
  GBitmap(const GBitmap &ref, const GRect &rect, int border=0);
  GBitmap(ByteStream &ref, int border=0);
public:
  virtual ~GBitmap();
  void destroy(void);
  /** @name Construction. */
  //@{
  /** Constructs an empty GBitmap object.  The returned GBitmap has zero rows
      and zero columns.  Use function \Ref{init} to change the size of the
      image. */
  static GP<GBitmap> create(void) {return new GBitmap;}

  /** Constructs a GBitmap with #nrows# rows and #ncolumns# columns.  All
      pixels are initialized to white. The optional argument #border#
      specifies the size of the optional border of white pixels surrounding
      the image.  The number of gray levels is initially set to #2#.  */
  static GP<GBitmap> create(const int nrows, const int ncolumns, const int border=0)
  {return new GBitmap(nrows,ncolumns, border); }

  /** Copy constructor. Constructs a GBitmap by replicating the size, the
      border and the contents of GBitmap #ref#. */
  static GP<GBitmap> create(const GBitmap &ref)
  {return new GBitmap(ref);}

  /** Constructs a GBitmap by copying the contents of GBitmap #ref#.  
      Argument #border# specifies the width of the optional border. */
  static GP<GBitmap> create(const GBitmap &ref, const int border)
  { return new GBitmap(ref,border); }

  /** Constructs a GBitmap by copying a rectangular segment #rect# of GBitmap
      #ref#.  The optional argument #border# specifies the size of the
      optional border of white pixels surrounding the image. */
  static GP<GBitmap> create(const GBitmap &ref, const GRect &rect, const int border=0)
  { return new GBitmap(ref,rect,border); }

  /** Constructs a GBitmap by reading PBM, PGM or RLE data from ByteStream
      #ref# into this GBitmap. The optional argument #border# specifies the
      size of the optional border of white pixels surrounding the image.  See
      \Ref{PNM and RLE file formats} for more information.  */
  static GP<GBitmap> create(ByteStream &ref, const int border=0)
  { return new GBitmap(ref,border); }

  //@}

  /** @name Initialization. */
  //@{
  /** Resets this GBitmap size to #nrows# rows and #ncolumns# columns and sets
      all pixels to white.  The optional argument #border# specifies the size
      of the optional border of white pixels surrounding the image.  The
      number of gray levels is initialized to #2#. */
  void init(int nrows, int ncolumns, int border=0);
  /** Initializes this GBitmap with the contents of the GBitmap #ref#.  The
      optional argument #border# specifies the size of the optional border of
      white pixels surrounding the image. */
  void init(const GBitmap &ref, int border=0);
  /** Initializes this GBitmap with a rectangular segment #rect# of GBitmap
      #ref#.  The optional argument #border# specifies the size of the
      optional border of white pixels surrounding the image. */
  void init(const GBitmap &ref, const GRect &rect, int border=0);
  /** Reads PBM, PGM or RLE data from ByteStream #ref# into this GBitmap.  The
      previous content of the GBitmap object is lost. The optional argument
      #border# specifies the size of the optional border of white pixels
      surrounding the image. See \Ref{PNM and RLE file formats} for more
      information. */
  void init(ByteStream &ref, int border=0);
  /** Assignment operator. Initializes this GBitmap by copying the size, the
      border and the contents of GBitmap #ref#. */
  GBitmap& operator=(const GBitmap &ref);
  /** Initializes all the GBitmap pixels to value #value#. */
  void fill(unsigned char value);
  //@}

  /** @name Accessing the pixels. */
  //@{
  /** Returns the number of rows (the image height). */
  unsigned int rows() const;
  /** Returns the number of columns (the image width). */
  unsigned int columns() const;
  /** Returns a constant pointer to the first byte of row #row#.
      This pointer can be used as an array to read the row elements. */
  const unsigned char *operator[] (int row) const;
  /** Returns a pointer to the first byte of row #row#.
      This pointer can be used as an array to read or write the row elements. */
  unsigned char *operator[] (int row);
  /** Returns the size of a row in memory (in pixels).  This number is equal
      to the difference between pointers to pixels located in the same column
      in consecutive rows.  This difference can be larger than the number of
      columns in the image. */
  unsigned int rowsize() const;
  /** Makes sure that the border is at least #minimum# pixels large.  This
      function does nothing it the border width is already larger than
      #minimum#.  Otherwise it reorganizes the data in order to provide a
      border of #minimum# pixels. */
  void minborder(int minimum);
  //@}

  /** @name Managing gray levels. */
  //@{
  /** Returns the number of gray levels. 
      Value #2# denotes a bilevel image. */
  int  get_grays() const;
  /** Sets the number of gray levels without changing the pixels.
      Argument #grays# must be in range #2# to #256#. */
  void set_grays(int grays);
  /** Changes the number of gray levels.  The argument #grays# must be in the
      range #2# to #256#.  All the pixel values are then rescaled and clipped
      in range #0# to #grays-1#. */
  void change_grays(int grays);
  /** Binarizes a gray level image using a threshold.  The number of gray
      levels is reduced to #2# as in a bilevel image.  All pixels whose value
      was strictly greater than #threshold# are set to black. All other pixels
      are set to white. */
  void binarize_grays(int threshold=0);
  //@}

  /** @name Optimizing the memory usage.  
      The amount of memory used by bilevel images can be reduced using
      function \Ref{compress}, which encodes the image using a run-length
      encoding scheme.  The bracket operator decompresses the image on demand.
      A few highly optimized functions (e.g. \Ref{blit}) can use a run-length
      encoded bitmap without decompressing it.  There are unfortunate locking
      issues associated with this capability (c.f. \Ref{share} and
      \Ref{monitor}). */
  //@{
  /** Reduces the memory required for a bilevel image by using a run-length
      encoded representation.  Functions that need to access the pixel array
      will decompress the image on demand. */
  void compress();
  /** Decodes run-length encoded bitmaps and recreate the pixel array.
      This function is usually called by #operator[]# when needed. */
  void uncompress();
  /** Returns the number of bytes allocated for this image. */
  unsigned int get_memory_usage() const;
  /** Returns a possibly null pointer to a \Ref{GMonitor} for this bitmap.
      You should use this monitor to ensure that the data representation of the 
      bitmap will not change while you are using it.  We suggest using
      class \Ref{GMonitorLock} which properly handles null monitor pointers. */
  GMonitor *monitor() const;
  /** Associates a \Ref{GMonitor} with this bitmap. This function should be
      called on all bitmaps susceptible of being simultaneously used by
      several threads.  It will make sure that function \Ref{monitor} returns
      a pointer to a suitable monitor for this bitmap. */
  void share();
  //@}

  /** @name Accessing RLE data.
      The next functions are useful for processing bilevel images
      encoded using the run length encoding scheme.  These functions always return
      zero if the bitmap is not RLE encoded.  Function \Ref{compress} must
      be used to ensure that the bitmap is RLE encoded.  */
  //@{
  /** Gets the pixels for line #rowno#.  One line of pixel is stored as
      #unsigned char# values into array #bits#.  Each pixel is either 1 or 0.
      The array must be large enough to hold the whole line.  The number of
      pixels is returned. */

  int rle_get_bits(int rowno, unsigned char *bits) const;

  /** Gets the bitmap line rle data passed.  One line of pixel is stored one
      with 8 bits per #unsigned char# in an array.  The array must be large
      enough to hold the whole line.  */

  static void rle_get_bitmap(const int ncolumns,const unsigned char *&runs,
    unsigned char *bitmap, const bool invert );

  /** Gets the lengths of all runs in line #rowno#.  The array #rlens# must be
      large enough to accomodate #w+2# integers where #w# is the number of
      columns in the image.  These integers represent the lengths of
      consecutive runs of alternatively white or black pixels.  Lengths can be
      zero in order to allow for lines starting with black pixels.  This
      function returns the total number of runs in the line. */
  int rle_get_runs(int rowno, int *rlens) const;
  /** Gets the smallest rectangle enclosing black pixels.
      Rectangle rect gives the coordinates of the smallest rectangle
      containing all black pixels. Returns the number of black pixels. */
  int rle_get_rect(GRect &rect) const;
  //@}

  /** @name Additive Blit.  
      The blit functions are designed to efficiently construct an anti-aliased
      image by copying smaller images at predefined locations.  The image of a
      page, for instance, is composed by copying the images of characters at
      predefined locations.  These functions are fairly optimized.  They can
      directly use compressed GBitmaps (see \Ref{compress}).  We consider in
      this section that each GBitmap comes with a coordinate system defined as
      follows.  Position (#0#,#0#) corresponds to the bottom left corner of
      the bottom left pixel.  Position (#1#,#1#) corresponds to the top right
      corner of the bottom left pixel, which is also the bottom left corner of
      the second pixel of the second row.  Position (#w#,#h#), where #w# and
      #h# denote the size of the GBitmap, corresponds to the top right corner
      of the top right pixel. */

  //@{
  /** Performs an additive blit of the GBitmap #bm#.  The GBitmap #bm# is
      first positioned above the current GBitmap in such a way that position
      (#u#,#v#) in GBitmap #bm# corresponds to position (#u#+#x#,#v#+#y#) in
      the current GBitmap.  The value of each pixel in GBitmap #bm# is then
      added to the value of the corresponding pixel in the current GBitmap.
      
      {\bf Example}: Assume for instance that the current GBitmap is initially
      white (all pixels have value zero).  This operation copies the pixel
      values of GBitmap #bm# at position (#x#,#y#) into the current GBitmap.
      Note that function #blit# does not change the number of gray levels in
      the current GBitmap.  You may have to call \Ref{set_grays} to specify
      how the pixel values should be interpreted. */
  void blit(const GBitmap *bm, int x, int y);
  /** Performs an additive blit of the GBitmap #bm# with anti-aliasing.  The
      GBitmap #bm# is first positioned above the current GBitmap in such a
      way that position (#u#,#v#) in GBitmap #bm# corresponds to position
      (#u#+#x#/#subsample#,#v#+#y#/#subsample#) in the current GBitmap.  This
      mapping results in a contraction of GBitmap #bm# by a factor
      #subsample#.  Each pixel of the current GBitmap can be covered by a
      maximum of #subsample^2# pixels of GBitmap #bm#.  The value of
      each pixel in GBitmap #bm# is then added to the value of the
      corresponding pixel in the current GBitmap.

      {\bf Example}: Assume for instance that the current GBitmap is initially
      white (all pixels have value zero).  Each pixel of the current GBitmap
      then contains the sum of the gray levels of the corresponding pixels in
      GBitmap #bm#.  There are up to #subsample*subsample# such pixels.  If
      for instance GBitmap #bm# is a bilevel image (pixels can be #0# or #1#),
      the pixels of the current GBitmap can take values in range #0# to
      #subsample*subsample#.  Note that function #blit# does not change the
      number of gray levels in the current GBitmap.  You must call
      \Ref{set_grays} to indicate that there are #subsample^2+1# gray
      levels.  Since there is at most 256 gray levels, this also means that
      #subsample# should never be greater than #15#.

      {\bf Remark}: Arguments #x# and #y# do not represent a position in the
      coordinate system of the current GBitmap.  According to the above
      discussion, the position is (#x/subsample#,#y/subsample#).  In other
      words, you can position the blit with a sub-pixel resolution.  The
      resulting anti-aliasing changes are paramount to the image quality. */
  void blit(const GBitmap *shape, int x, int y, int subsample);
  //@}
  
  /** @name Saving images.  
      The following functions write PBM, PGM and RLE files.  PBM and PGM are
      well known formats for bilevel and gray-level images.  The RLE is a
      simple run-length encoding scheme for bilevel images. These files can be
      read using the ByteStream based constructor or initialization function.
      See \Ref{PNM and RLE file formats} for more information. */
  //@{
  /** Saves the image into ByteStream #bs# using the PBM format.  Argument
      #raw# selects the ``Raw PBM'' (1) or the ``Ascii PBM'' (0) format.  The
      image is saved as a bilevel image.  All non zero pixels are considered
      black pixels. See section \Ref{PNM and RLE file formats}. */
  void save_pbm(ByteStream &bs, int raw=1);
  /** Saves the image into ByteStream #bs# using the PGM format.  Argument
      #raw# selects the ``Raw PGM'' (1) or the ``Ascii PGM'' (0) format.  The
      image is saved as a gray level image.  See section
      \Ref{PNM and RLE file formats}. */
  void save_pgm(ByteStream &bs, int raw=1);
  /** Saves the image into ByteStream #bs# using the RLE file format.
      The image is saved as a bilevel image. All non zero pixels are
      considered black pixels. See section \Ref{PNM and RLE file formats}. */
  void save_rle(ByteStream &bs);
  //@}

  /** @name Stealing or borrowing the memory buffer (advanced). */
  //@{
  /** Steals the memory buffer of a GBitmap.  This function returns the
      address of the memory buffer allocated by this GBitmap object.  The
      offset of the first pixel in the bottom line is written into variable
      #offset#.  Other lines can be accessed using pointer arithmetic (see
      \Ref{rowsize}).  The GBitmap object no longer ``owns'' the buffer: you
      must explicitly de-allocate the buffer using #operator delete []#.  This
      de-allocation should take place after the destruction or the
      re-initialization of the GBitmap object.  This function will return a
      null pointer if the GBitmap object does not ``own'' the buffer in the
      first place.  */
  unsigned char *take_data(size_t &offset);
  /** Initializes this GBitmap by borrowing a memory segment.  The GBitmap
      then directly addresses the memory buffer #data# provided by the user.
      This buffer must be large enough to hold #w*h# bytes representing each
      one pixel.  The GBitmap object does not ``own'' the buffer: you must
      explicitly de-allocate the buffer using #operator delete []#.  This
      de-allocation should take place after the destruction or the
      re-initialization of the GBitmap object.  */
  inline void borrow_data(unsigned char &data, int w, int h);
  /** Same as borrow_data, except GBitmap will call #delete[]#. */
  void donate_data(unsigned char *data, int w, int h);
  /** Return a pointer to the rle data. */
  const unsigned char *get_rle(unsigned int &rle_length);
  /** Initializes this GBitmap by setting the size to #h# rows and #w#
      columns, and directly addressing the memory buffer #rledata# provided by
      the user.  This buffer contains #rledatalen# bytes representing the
      bitmap in run length encoded form.  The GBitmap object then ``owns'' the
      buffer (unlike #borrow_data#, but like #donate_data#) and will
      deallocate this buffer when appropriate: you should not deallocate this
      buffer yourself.  The encoding of buffer #rledata# is similar to the
      data segment of the RLE file format (without the header) documented in
      \Ref{PNM and RLE file formats}.  */
  void donate_rle(unsigned char *rledata, unsigned int rledatalen, int w, int h);
  /** Static function for parsing run data.
      This function returns one run length encoded at position #data# 
      and increments the pointer #data# accordingly. */
  static inline int read_run(const unsigned char *&data);
  static inline int read_run(unsigned char *&data);
  /** Static function for generating run data.
      This function encoded run length #count# at position #data#
      and increments the pointer accordingly.  The pointer must
      initially point to a large enough data buffer. */
  static inline void append_run(unsigned char *&data, int count);
  /** Rotates bitmap by 90, 180 or 270 degrees anticlockwise
      and returns a new pixmap, input bitmap is not changed. 
      count can be 1, 2, or 3 for 90, 180, 270 degree rotation.
      It returns the same bitmap if not rotated. 
      The input bitmap will be uncompressed for rotation*/
  GP<GBitmap> rotate(int count=0);
  //@}

// These are constants, but we use enum because that works on older compilers.
  enum {MAXRUNSIZE=0x3fff};
  enum {RUNOVERFLOWVALUE=0xc0};
  enum {RUNMSBMASK=0x3f};
  enum {RUNLSBMASK=0xff};


protected:
  // bitmap components
  unsigned short nrows;
  unsigned short ncolumns;
  unsigned short border;
  unsigned short bytes_per_row;
  unsigned short grays;
  unsigned char  *bytes;
  unsigned char  *bytes_data;
  GPBuffer<unsigned char> gbytes_data;
  unsigned char  *rle;
  GPBuffer<unsigned char> grle;
  unsigned char  **rlerows;
  GPBuffer<unsigned char *> grlerows;
  unsigned int   rlelength;
private:
  GMonitor       *monitorptr;
public:
  class ZeroBuffer;
  friend class ZeroBuffer;
  GP<ZeroBuffer> gzerobuffer; 
private:
  static int zerosize;
  static unsigned char *zerobuffer;
  static GP<ZeroBuffer> zeroes(int ncolumns);
  static unsigned int read_integer(char &lookahead, ByteStream &ref);
  static void euclidian_ratio(int a, int b, int &q, int &r);
  int encode(unsigned char *&pruns,GPBuffer<unsigned char> &gpruns) const;
  void decode(unsigned char *runs);
  void read_pbm_text(ByteStream &ref); 
  void read_pgm_text(ByteStream &ref, int maxval); 
  void read_pbm_raw(ByteStream &ref); 
  void read_pgm_raw(ByteStream &ref, int maxval); 
  void read_rle_raw(ByteStream &ref); 
  static void append_long_run(unsigned char *&data, int count);
  static void append_line(unsigned char *&data,const unsigned char *row,
                          const int rowlen,bool invert=false);
  static void makerows(int,const int, unsigned char *, unsigned char *[]);
  friend class DjVu_Stream;
  friend class DjVu_PixImage;
public:
#ifndef NDEBUG
  void check_border() const;
#endif
};


/** @name PNM and RLE file formats

    {\bf PNM} --- There are actually three PNM file formats: PBM for bilevel
    images, PGM for gray level images, and PPM for color images.  These
    formats are widely used by popular image manipulation packages such as
    NetPBM \URL{http://www.arc.umn.edu/GVL/Software/netpbm.html} or
    ImageMagick \URL{http://www.wizards.dupont.com/cristy/}.
    
    {\bf RLE} --- The binary RLE file format is a simple run-length encoding
    scheme for storing bilevel images.  Encoding or decoding a RLE encoded
    file is extremely simple. Yet RLE encoded files are usually much smaller
    than the corresponding PBM encoded files.  RLE files always begin with a
    header line composed of:\\
    - the two characters #"R4"#,\\
    - one or more blank characters,\\
    - the number of columns, encoded using characters #"0"# to #"9"#,\\
    - one or more blank characters,\\
    - the number of lines, encoded using characters #"0"# to #"9"#,\\
    - exactly one blank character (usually a line-feed character).

    The rest of the file encodes a sequence of numbers representing the
    lengths of alternating runs of white and black pixels.  Lines are encoded
    starting with the top line and progressing towards the bottom line.  Each
    line starts with a white run. The decoder knows that a line is finished
    when the sum of the run lengths for that line is equal to the number of
    columns in the image.  Numbers in range #0# to #191# are represented by a
    single byte in range #0x00# to #0xbf#.  Numbers in range #192# to #16383#
    are represented by a two byte sequence: the first byte, in range #0xc0# to
    #0xff#, encodes the six most significant bits of the number, the second
    byte encodes the remaining eight bits of the number. This scheme allows
    for runs of length zero, which are useful when a line starts with a black
    pixel, and when a very long run (whose length exceeds #16383#) must be
    split into smaller runs.

    @memo
    Simple image file formats.  */

//@}


// ---------------- IMPLEMENTATION

inline unsigned int
GBitmap::rows() const
{
  return nrows;
}

inline unsigned int
GBitmap::columns() const
{
  return ncolumns;
}

inline unsigned int 
GBitmap::rowsize() const
{
  return bytes_per_row;
}

inline int
GBitmap::get_grays() const
{
  return grays;
}

inline unsigned char *
GBitmap::operator[](int row) 
{
  if (!bytes) 
    uncompress();
  if (row<0 || row>=nrows || !bytes) {
#ifndef NDEBUG
    if (zerosize < bytes_per_row + border)
      G_THROW( ERR_MSG("GBitmap.zero_small") );
#endif
    return zerobuffer + border;
  }
  return &bytes[row * bytes_per_row + border];
}

inline const unsigned char *
GBitmap::operator[](int row) const
{
  if (!bytes) 
    ((GBitmap*)this)->uncompress();
  if (row<0 || row>=nrows || !bytes) {
#ifndef NDEBUG
    if (zerosize < bytes_per_row + border)
      G_THROW( ERR_MSG("GBitmap.zero_small") );
#endif
    return zerobuffer + border;
  }
  return &bytes[row * bytes_per_row + border];
}

inline GBitmap& 
GBitmap::operator=(const GBitmap &ref)
{
  init(ref, ref.border);
  return *this;
}

inline GMonitor *
GBitmap::monitor() const
{
  return monitorptr;
}

inline void 
GBitmap::euclidian_ratio(int a, int b, int &q, int &r)
{
  q = a / b;
  r = a - b*q;
  if (r < 0)
  {
    q -= 1;
    r += b;
  }
}


inline int
GBitmap::read_run(unsigned char *&data)
{
  register int z=*data++;
  return (z>=RUNOVERFLOWVALUE)?
    ((z&~RUNOVERFLOWVALUE)<<8)|(*data++):z;
}

inline int
GBitmap::read_run(const unsigned char *&data)
{
  register int z=*data++;
  return (z>=RUNOVERFLOWVALUE)?
    ((z&~RUNOVERFLOWVALUE)<<8)|(*data++):z;
}

inline void
GBitmap::append_run(unsigned char *&data, int count)
{
  if (count < RUNOVERFLOWVALUE)
    {
      data[0] = count;
      data += 1;
    }
  else if (count <= MAXRUNSIZE)
    {
      data[0] = (count>>8) + GBitmap::RUNOVERFLOWVALUE;
      data[1] = (count & 0xff);
      data += 2;
    }
  else
    {
      append_long_run(data, count);
    }
}


inline void
GBitmap::borrow_data(unsigned char &data,int w,int h)
{
  donate_data(&data,w,h);
  bytes_data=0;
}

// ---------------- THE END

#ifdef HAVE_NAMESPACES
}
# ifndef NOT_USING_DJVU_NAMESPACE
using namespace DJVU;
# endif
#endif
#endif
