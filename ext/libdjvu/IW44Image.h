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

#ifndef IW44IMAGE_H_
#define IW44IMAGE_H_
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#if NEED_GNUG_PRAGMAS
# pragma interface
#endif


/** @name IW44Image.h

    Files #"IW44Image.h"# and #"IW44Image.cpp"# implement the DjVu IW44 wavelet
    scheme for the compression of gray-level images (see class \Ref{IWBitmap})
    and color images (see class \Ref{IWPixmap}).  Programs \Ref{c44} and
    \Ref{d44} demonstrate how to encode and decode IW44 files.

    {\bf IW44 File Structure} --- The IW44 files are structured according to
    the EA IFF85 specifications (see \Ref{IFFByteStream.h}).  Gray level IW44
    Images consist of a single #"FORM:BM44"# chunk composed of an arbitrary
    number of #"BM44"# data chunks.  Color IW44 Images consist of a single
    #"FORM:PM44"# chunk composed of an arbitrary number of #"PM44"# data
    chunks.  The successive #"PM44"# or #"BM44"# data chunks contain
    successive refinements of the encoded image.  Each chunk contains a
    certain number of ``data slices''.  The first chunk also contains a small
    image header.  You can use program \Ref{djvuinfo} to display all this
    structural information:
    \begin{verbatim}
    % djvuinfo lag.iw4
    lag.iw4:
      FORM:PM44 [62598] 
        PM44 [10807]              #1 - 74 slices - v1.2 (color) - 684x510
        PM44 [23583]              #2 - 13 slices 
        PM44 [28178]              #3 - 10 slices 
    \end{verbatim}

    {\bf Embedded IW44 Images} --- These IW44 data chunks can also appear within
    other contexts.  Files representing a DjVu page, for instance, consist of
    a single #"FORM:DJVU"# composite chunk.  This composite chunk may contain
    #"BG44"# chunks encoding the background layer and #"FG44"# chunks encoding
    the foreground color layer.  These #"BG44"# and #"FG44"# chunks are
    actually regular IW44 data chunks with a different chunk identifier.  This
    information too can be displayed using program \Ref{djvuinfo}.
    \begin{verbatim}
    % djvuinfo graham1.djvu 
    graham1.djvu:
      FORM:DJVU [32553] 
        INFO [5]            3156x2325, version 17
        Sjbz [17692] 
        BG44 [2570]         #1 - 74 slices - v1.2 (color) - 1052x775
        FG44 [1035]         #1 - 100 slices - v1.2 (color) - 263x194
        BG44 [3048]         #2 - 10 slices 
        BG44 [894]          #3 - 4 slices 
        BG44 [7247]         #4 - 9 slices 
    \end{verbatim}

    {\bf Performance} --- The main design objective for the DjVu wavelets
    consisted of allowing progressive rendering and smooth scrolling of large
    images with limited memory requirements.  Decoding functions process the
    compressed data and update a memory efficient representation of the
    wavelet coefficients.  Imaging function then can quickly render an
    arbitrary segment of the image using the available data.  Both process can
    be carried out in two threads of execution.  This design plays an
    important role in the DjVu system.  We have investigated various
    state-of-the-art wavelet compression schemes: although these schemes may
    achieve slightly smaller file sizes, the decoding functions did not even
    approach our requirements.  

    The IW44 wavelets satisfy these requirements today. It performs very well
    for quality settings resulting in high compression ratios.  It should not
    be used for quasi-lossless compression because certain design choices
    deliberately sacrifice the IW44 quasi-lossless performance in order to
    improve the image quality at high compression ratios.

    Little care however has been taken to make the IW44 encoder memory
    efficient.  This code uses two copies of the wavelet coefficient data
    structure (one for the raw coefficients, one for the quantized
    coefficients).  A more sophisticated implementation should considerably
    reduce the memory requirements.

    {\bf Masking} --- When we create a DjVu image, we often know that certain
    pixels of the background image are going to be covered by foreground
    objects like text or drawings.  The DjVu IW44 wavelet decomposition
    routine can use an optional bilevel image named the mask.  Every non zero
    pixel in the mask means the value of the corresponding pixel in the
    background image is irrelevant.  The wavelet decomposition code will
    replace these masked pixels by a color value whose coding cost is minimal
    (see \URL{http://www.research.att.com/~leonb/DJVU/mask}).

    {\bf ToDo} --- There are many improvements to be made.  Besides better
    quantization algorithms (such as trellis quantization and bitrate
    allocation), we should allow for more wavelet transforms.  These
    improvements may be implemented in future version, if (and only if) they
    can meet our decoding constraints.  Future versions will probably split
    file #"IW44Image.cpp"# which currently contains everything.
 
    @memo
    Wavelet encoded images.
    @author
    L\'eon Bottou <leonb@research.att.com>

// From: Leon Bottou, 1/31/2002
// Lizardtech has split the corresponding cpp file into a decoder and an encoder.
// Only superficial changes.  The meat is mine.

*/
//@{


#include "GSmartPointer.h"
#include "ZPCodec.h"


#ifdef HAVE_NAMESPACES
namespace DJVU {
# ifdef NOT_DEFINED // Just to fool emacs c++ mode
}
#endif
#endif

class GRect;
class IFFByteStream;
class ByteStream;
class GBitmap;
class GPixmap;



/** IW44 encoding parameters.  
    This data structure gathers the quality specification parameters needed
    for encoding each chunk of an IW44 file.  Chunk data is generated until
    meeting either the slice target, the size target or the decibel target.  */

struct DJVUAPI IWEncoderParms 
{
  /** Slice target.  Data generation for the current chunk stops if the total
      number of slices (in this chunk and all the previous chunks) reaches
      value #slice#.  The default value #0# has a special meaning: data will
      be generated regardless of the number of slices in the file. */
  int    slices;
  /** Size target.  Data generation for the current chunk stops if the total
      data size (in this chunk and all the previous chunks), expressed in
      bytes, reaches value #size#.  The default value #0# has a special
      meaning: data will be generated regardless of the file size. */
  int    bytes;
  /** Decibel target.  Data generation for the current chunk stops if the
      estimated luminance error, expressed in decibels, reaches value
      #decibel#.  The default value #0# has a special meaning: data will be
      generated regardless of the estimated luminance error.  Specifying value
      #0# in fact shortcuts the computation of the estimated luminance error
      and sensibly speeds up the encoding process.  */
  float  decibels;
  /** Constructor. Initializes the structure with the default values. */
  IWEncoderParms(void);
};



/** IW44 encoded gray-level and color images.  This class acts as a base for
    images represented as a collection of IW44 wavelet coefficients.  The
    coefficients are stored in a memory efficient data structure.  Member
    function \Ref{get_bitmap} renders an arbitrary segment of the image into
    a \Ref{GBitmap}.  Member functions \Ref{decode_iff} and \Ref{encode_iff}
    read and write DjVu IW44 files (see \Ref{IW44Image.h}).  Both the copy
    constructor and the copy operator are declared as private members. It is
    therefore not possible to make multiple copies of instances of this
    class. */

class DJVUAPI IW44Image : public GPEnabled
{
public:
  /** Chrominance processing selector.  The following constants may be used as
      argument to the following \Ref{IWPixmap} constructor to indicate how the
      chrominance information should be processed. There are four possible values:
      \begin{description}
      \item[CRCBnone:] The wavelet transform will discard the chrominance 
           information and only keep the luminance. The image will show in shades of gray.
      \item[CRCBhalf:] The wavelet transform will process the chrominance at only 
           half the image resolution. This option creates smaller files but may create
           artifacts in highly colored images.
      \item[CRCBnormal:] The wavelet transform will process the chrominance at full 
           resolution. This is the default.
      \item[CRCBfull:] The wavelet transform will process the chrominance at full 
           resolution. This option also disables the chrominance encoding delay
           (see \Ref{parm_crcbdelay}) which usually reduces the bitrate associated with the
           chrominance information.
      \end{description} */
  enum CRCBMode { 
    CRCBnone, 
    CRCBhalf, 
    CRCBnormal, 
    CRCBfull };
  class Transform;
  class Map;
  class Block;
  class Codec;
  struct Alloc;
  struct PrimaryHeader;
  struct SecondaryHeader;
  struct TertiaryHeader;
  enum ImageType {
    GRAY=false,
    COLOR=true };
protected:
  IW44Image(void);
public:
  /** Null constructor.  Constructs an empty IW44Image object. This object does
      not contain anything meaningful. You must call function \Ref{init},
      \Ref{decode_iff} or \Ref{decode_chunk} to populate the wavelet
      coefficient data structure. You may not use \Ref{encode_iff} or 
      \Ref{encode_chunk}. */
  static GP<IW44Image> create_decode(const ImageType itype=COLOR);
  /** Null constructor.  Constructs an empty IW44Image object. This object does
      not contain anything meaningful. You must call function \Ref{init},
      \Ref{decode_iff} or \Ref{decode_chunk} to populate the wavelet
      coefficient data structure.  You may then use \Ref{encode_iff}
      and \Ref{encode_chunk}. */
  static GP<IW44Image> create_encode(const ImageType itype=COLOR);
  // virtual destructor
  virtual ~IW44Image();
  /** Initializes an IWBitmap with image #bm#.  This constructor
      performs the wavelet decomposition of image #bm# and records the
      corresponding wavelet coefficient.  Argument #mask# is an optional
      bilevel image specifying the masked pixels (see \Ref{IW44Image.h}). */
  static GP<IW44Image> create_encode(const GBitmap &bm, const GP<GBitmap> mask=0);
  /** Initializes an IWPixmap with color image #bm#.  This constructor
      performs the wavelet decomposition of image #bm# and records the
      corresponding wavelet coefficient.  Argument #mask# is an optional
      bilevel image specifying the masked pixels (see \Ref{IW44Image.h}).
      Argument #crcbmode# specifies how the chrominance information should be
      encoded (see \Ref{CRCBMode}). */
  static GP<IW44Image> create_encode(const GPixmap &bm, const GP<GBitmap> mask=0, CRCBMode crcbmode=CRCBnormal);
  // ACCESS
  /** Returns the width of the IWBitmap image. */
  int get_width(void) const;
  /** Returns the height of the IWBitmap image. */
  int get_height(void) const;
  /** Reconstructs the complete image.  The reconstructed image
      is then returned as a GBitmap object. */
  virtual GP<GBitmap> get_bitmap(void) {return 0;}
  /** Reconstructs a segment of the image at a given scale.  The subsampling
      ratio #subsample# must be a power of two between #1# and #32#.  Argument
      #rect# specifies which segment of the subsampled image should be
      reconstructed.  The reconstructed image is returned as a GBitmap object
      whose size is equal to the size of the rectangle #rect#. */
  virtual GP<GBitmap> get_bitmap(int subsample, const GRect &rect) {return 0;}
  /** Reconstructs the complete image.  The reconstructed image
      is then returned as a GPixmap object. */
  virtual GP<GPixmap> get_pixmap(void) {return 0;}
  /** Reconstructs a segment of the image at a given scale.  The subsampling
      ratio #subsample# must be a power of two between #1# and #32#.  Argument
      #rect# specifies which segment of the subsampled image should be
      reconstructed.  The reconstructed image is returned as a GPixmap object
      whose size is equal to the size of the rectangle #rect#. */
  virtual GP<GPixmap> get_pixmap(int subsample, const GRect &rect) {return 0;}
  /** Returns the amount of memory used by the wavelet coefficients.  This
      amount of memory is expressed in bytes. */
  virtual unsigned int get_memory_usage(void) const = 0;
  /** Returns the filling ratio of the internal data structure.  Wavelet
      coefficients are stored in a sparse array.  This function tells what
      percentage of bins have been effectively allocated. */
  virtual int get_percent_memory(void) const = 0;
  // CODER
  /** Encodes one data chunk into ByteStream #bs#.  Parameter #parms# controls
      how much data is generated.  The chunk data is written to ByteStream
      #bs# with no IFF header.  Successive calls to #encode_chunk# encode
      successive chunks.  You must call #close_codec# after encoding the last
      chunk of a file. */
  virtual int  encode_chunk(GP<ByteStream> gbs, const IWEncoderParms &parms);
  /** Writes a gray level image into DjVu IW44 file.  This function creates a
      composite chunk (identifier #FORM:BM44# or #FORM:PM44#) composed of
      #nchunks# chunks (identifier #BM44# or #PM44#).  Data for each chunk is
      generated with #encode_chunk# using the corresponding parameters in
      array #parms#. */
  virtual void encode_iff(IFFByteStream &iff, int nchunks, const IWEncoderParms *parms);
  // DECODER
  /** Decodes one data chunk from ByteStream #bs#.  Successive calls to
      #decode_chunk# decode successive chunks.  You must call #close_codec#
      after decoding the last chunk of a file.  Note that function
      #get_bitmap# and #decode_chunk# may be called simultaneously from two
      execution threads. */
  virtual int  decode_chunk(GP<ByteStream> gbs) = 0;
  /** This function enters a composite chunk (identifier #FORM:BM44#, or
      #FORM:PM44#), and decodes a maximum of #maxchunks# data chunks
      (identifier #BM44#).  Data for each chunk is processed using the
      function #decode_chunk#. */
  virtual void decode_iff(IFFByteStream &iff, int maxchunks=999) = 0;
  // MISCELLANEOUS
  /** Resets the encoder/decoder state.  The first call to #decode_chunk# or
      #encode_chunk# initializes the coder for encoding or decoding.  Function
      #close_codec# must be called after processing the last chunk in order to
      reset the coder and release the associated memory. */
  virtual void close_codec(void) = 0;
  /** Returns the chunk serial number.  This function returns the serial
      number of the last chunk encoded with #encode_chunk# or decoded with
      #decode_chunk#. The first chunk always has serial number #1#. Successive
      chunks have increasing serial numbers.  Value #0# is returned if this
      function is called before calling #encode_chunk# or #decode_chunk# or
      after calling #close_codec#. */
  virtual int get_serial(void) = 0;
  /** Sets the chrominance delay parameter.  This function can be called
      before encoding the first color IW44 data chunk.  Parameter #parm# is an
      encoding delay which reduces the bitrate associated with the
      chrominance information. The default chrominance encoding delay is 10. */
  virtual int  parm_crcbdelay(const int parm) {return parm;}
  /** Sets the #dbfrac# parameter.  This function can be called before
      encoding the first IW44 data chunk.  Parameter #frac# modifies the
      decibel estimation algorithm in such a way that the decibel target only
      pertains to the average error of the fraction #frac# of the most
      misrepresented 32x32 pixel blocks.  Setting arguments #frac# to #1.0#
      restores the normal behavior.  */
  virtual void parm_dbfrac(float frac) = 0;
protected:
  // Parameter
  float db_frac;
  // Data
  Map *ymap, *cbmap, *crmap;
  int cslice;
  int cserial;
  int cbytes;
private:
  // Disable assignment semantic
  IW44Image(const IW44Image &ref);
  IW44Image& operator=(const IW44Image &ref);
};

#ifdef IW44IMAGE_IMPLIMENTATION

/*x IW44 encoded gray-level image.  This class provided functions for managing
    a gray level image represented as a collection of IW44 wavelet
    coefficients.  The coefficients are stored in a memory efficient data
    structure.  Member function \Ref{get_bitmap} renders an arbitrary segment
    of the image into a \Ref{GBitmap}.  Member functions \Ref{decode_iff} and
    \Ref{encode_iff} read and write DjVu IW44 files (see \Ref{IW44Image.h}).
    Both the copy constructor and the copy operator are declared as private
    members. It is therefore not possible to make multiple copies of instances
    of this class. */

class DJVUAPI IWBitmap : public IW44Image
{
public:
  friend class IW44Image;
  class Encode;
protected:
  /*x Null constructor.  Constructs an empty IWBitmap object. This object does
      not contain anything meaningful. You must call function \Ref{init},
      \Ref{decode_iff} or \Ref{decode_chunk} to populate the wavelet
      coefficient data structure. */
  IWBitmap(void);
public:
  //x virtual destructor
  virtual ~IWBitmap();
  //x ACCESS
  /*x Reconstructs the complete image.  The reconstructed image
      is then returned as a GBitmap object. */
  virtual GP<GBitmap> get_bitmap(void);
  /*x Reconstructs a segment of the image at a given scale.  The subsampling
      ratio #subsample# must be a power of two between #1# and #32#.  Argument
      #rect# specifies which segment of the subsampled image should be
      reconstructed.  The reconstructed image is returned as a GBitmap object
      whose size is equal to the size of the rectangle #rect#. */
  virtual GP<GBitmap> get_bitmap(int subsample, const GRect &rect);
  /*x Returns the amount of memory used by the wavelet coefficients.  This
      amount of memory is expressed in bytes. */
  virtual unsigned int get_memory_usage(void) const;
  /*x Returns the filling ratio of the internal data structure.  Wavelet
      coefficients are stored in a sparse array.  This function tells what
      percentage of bins have been effectively allocated. */
  virtual int get_percent_memory(void) const;
  // DECODER
  /*x Decodes one data chunk from ByteStream #bs#.  Successive calls to
      #decode_chunk# decode successive chunks.  You must call #close_codec#
      after decoding the last chunk of a file.  Note that function
      #get_bitmap# and #decode_chunk# may be called simultaneously from two
      execution threads. */
  virtual int  decode_chunk(GP<ByteStream> gbs);
  /*x Reads a DjVu IW44 file as a gray level image.  This function enters a
      composite chunk (identifier #FORM:BM44#), and decodes a maximum of
      #maxchunks# data chunks (identifier #BM44#).  Data for each chunk is
      processed using the function #decode_chunk#. */
  virtual void decode_iff(IFFByteStream &iff, int maxchunks=999);
  // MISCELLANEOUS
  /*x Resets the encoder/decoder state.  The first call to #decode_chunk# or
      #encode_chunk# initializes the coder for encoding or decoding.  Function
      #close_codec# must be called after processing the last chunk in order to
      reset the coder and release the associated memory. */
  virtual void close_codec(void);
  /*x Returns the chunk serial number.  This function returns the serial
      number of the last chunk encoded with #encode_chunk# or decoded with
      #decode_chunk#. The first chunk always has serial number #1#. Successive
      chunks have increasing serial numbers.  Value #0# is returned if this
      function is called before calling #encode_chunk# or #decode_chunk# or
      after calling #close_codec#. */
  virtual int get_serial(void);
  /*x Sets the #dbfrac# parameter.  This function can be called before
      encoding the first IW44 data chunk.  Parameter #frac# modifies the
      decibel estimation algorithm in such a way that the decibel target only
      pertains to the average error of the fraction #frac# of the most
      misrepresented 32x32 pixel blocks.  Setting arguments #frac# to #1.0#
      restores the normal behavior.  */
  virtual void parm_dbfrac(float frac);
private:
  Codec *ycodec;
};


/*x IW44 encoded color image. This class provided functions for managing a
    color image represented as a collection of IW44 wavelet coefficients.  The
    coefficients are stored in a memory efficient data structure.  Member
    function \Ref{get_pixmap} renders an arbitrary segment of the image into a
    \Ref{GPixmap}.  Member functions \Ref{decode_iff} and \Ref{encode_iff}
    read and write DjVu IW44 files (see \Ref{IW44Image.h}).  Both the copy
    constructor and the copy operator are declared as private members. It is
    therefore not possible to make multiple copies of instances of this
    class. */

class DJVUAPI IWPixmap : public IW44Image
{
public:
  friend class IW44Image;
protected:
  class Encode;
  /*x Null constructor.  Constructs an empty IWPixmap object. This object does
      not contain anything meaningful. You must call function \Ref{init},
      \Ref{decode_iff} or \Ref{decode_chunk} to populate the wavelet
      coefficient data structure. */
  IWPixmap(void);
public:
  // virtual destructor
  virtual ~IWPixmap();
  // ACCESS
  /*x Reconstructs the complete image.  The reconstructed image
      is then returned as a GPixmap object. */
  virtual GP<GPixmap> get_pixmap(void);
  /*x Reconstructs a segment of the image at a given scale.  The subsampling
      ratio #subsample# must be a power of two between #1# and #32#.  Argument
      #rect# specifies which segment of the subsampled image should be
      reconstructed.  The reconstructed image is returned as a GPixmap object
      whose size is equal to the size of the rectangle #rect#. */
  virtual GP<GPixmap> get_pixmap(int subsample, const GRect &rect);
  /*x Returns the amount of memory used by the wavelet coefficients.  This
      amount of memory is expressed in bytes. */
  virtual unsigned int get_memory_usage(void) const;
  /*x Returns the filling ratio of the internal data structure.  Wavelet
      coefficients are stored in a sparse array.  This function tells what
      percentage of bins have been effectively allocated. */
  virtual int get_percent_memory(void) const;
  // DECODER
  /*x Decodes one data chunk from ByteStream #bs#.  Successive calls to
      #decode_chunk# decode successive chunks.  You must call #close_codec#
      after decoding the last chunk of a file.  Note that function
      #get_bitmap# and #decode_chunk# may be called simultaneously from two
      execution threads. */
  virtual int  decode_chunk(GP<ByteStream> gbs);
  /*x Reads a DjVu IW44 file as a color image.  This function enters a
      composite chunk (identifier #FORM:PM44# or #FORM:BM44#), and decodes a
      maximum of #maxchunks# data chunks (identifier #PM44# or #BM44#).  Data
      for each chunk is processed using the function #decode_chunk#. */
  virtual void decode_iff(IFFByteStream &iff, int maxchunks=999);
  // MISCELLANEOUS
  /*x Resets the encoder/decoder state.  The first call to #decode_chunk# or
      #encode_chunk# initializes the coder for encoding or decoding.  Function
      #close_codec# must be called after processing the last chunk in order to
      reset the coder and release the associated memory. */
  virtual void close_codec(void);
  /*x Returns the chunk serial number.  This function returns the serial
      number of the last chunk encoded with #encode_chunk# or decoded with
      #decode_chunk#. The first chunk always has serial number #1#. Successive
      chunks have increasing serial numbers.  Value #0# is returned if this
      function is called before calling #encode_chunk# or #decode_chunk# or
      after calling #close_codec#. */
  virtual int  get_serial(void);
  /*x Sets the chrominance delay parameter.  This function can be called
      before encoding the first IW44 data chunk.  Parameter #parm# is an
      encoding delay which reduces the bitrate associated with the
      chrominance information. The default chrominance encoding delay is 10. */
  virtual int  parm_crcbdelay(const int parm);
  /*x Sets the #dbfrac# parameter.  This function can be called before
      encoding the first IW44 data chunk.  Parameter #frac# modifies the
      decibel estimation algorithm in such a way that the decibel target only
      pertains to the average error of the fraction #frac# of the most
      misrepresented 32x32 pixel blocks.  Setting arguments #frac# to #1.0#
      restores the normal behavior.  */
  virtual void parm_dbfrac(float frac);
protected:
  // Parameter
  int   crcb_delay;
  int   crcb_half;
  // Data
private:
  Codec *ycodec, *cbcodec, *crcodec;
};

/*x IW44Transform.
*/
class IW44Image::Transform
{
public:
  class Decode;
  class Encode;
protected:
  static void filter_begin(int w, int h);
  static void filter_end(void);
};

struct GPixel;
class IW44Image::Transform::Decode : public IW44Image::Transform
{
public:
 // WAVELET TRANSFORM
  /*x Forward transform. */
  static void backward(short *p, int w, int h, int rowsize, int begin, int end);
  
  // COLOR TRANSFORM
  /*x Converts YCbCr to RGB. */
  static void YCbCr_to_RGB(GPixel *p, int w, int h, int rowsize);
};

//---------------------------------------------------------------
// *** Class IW44Image::Block [declaration]
// Represents a block of 32x32 coefficients after zigzagging and scaling


class IW44Image::Block // DJVU_CLASS
{
public:
  // creating
  Block(void);
  // accessing scaled coefficients
  short get(int n) const;
  void  set(int n, int val, IW44Image::Map *map);
  // converting from liftblock
  void  read_liftblock(const short *coeff, IW44Image::Map *map);
  void  write_liftblock(short *coeff, int bmin=0, int bmax=64) const;
  // sparse array access
  const short* data(int n) const;
  short* data(int n, IW44Image::Map *map);
  void   zero(int n);
  // sparse representation
private:
  short **(pdata[4]);
};

//---------------------------------------------------------------
// *** Class IW44Image::Map [declaration]
// Represents all the blocks of an image

class IW44Image::Map // DJVU_CLASS
{
public:
  class Encode;

  // construction
  Map(int w, int h);
  ~Map();
  // image access
  void image(signed char *img8, int rowsize, 
             int pixsep=1, int fast=0);
  void image(int subsample, const GRect &rect, 
             signed char *img8, int rowsize, 
             int pixsep=1, int fast=0);
  // array of blocks
  IW44Image::Block *blocks;
  // geometry
  int iw, ih;
  int bw, bh;
  int nb;
  // coefficient allocation stuff
  short *alloc(int n);
  short **allocp(int n);
  IW44Image::Alloc *chain;
  int top;
  // statistics
  int get_bucket_count(void) const;
  unsigned int get_memory_usage(void) const;
};

//////////////////////////////////////////////////////
// ENCODING/DECODING WAVELET COEFFICIENTS 
//    USING HIERARCHICAL SET DIFFERENCE
//////////////////////////////////////////////////////


//-----------------------------------------------
// Class IW44Image::Codec [declaration+implementation]
// Maintains information shared while encoding or decoding

class IW44Image::Codec 
{
public:
  class Decode;
  class Encode;

protected:
  // Construction
  Codec(IW44Image::Map &map);
public:
  virtual ~Codec();
  // Coding
  int finish_code_slice(ZPCodec &zp);
  virtual int code_slice(ZPCodec &zp) = 0;
  // Data
  IW44Image::Map &map;                  // working map
  // status
  int curband;                  // current band
  int curbit;                   // current bitplane
  // quantization tables
  int quant_hi[10];             // quantization for bands 1 to 9
  int quant_lo[16];             // quantization for band 0.
  // bucket state
  char coeffstate[256];
  char bucketstate[16];
  enum { ZERO   = 1,            // this coeff never hits this bit
         ACTIVE = 2,            // this coeff is already active
         NEW    = 4,            // this coeff is becoming active
         UNK    = 8 };          // this coeff may become active
  // coding context
  BitContext ctxStart [32];
  BitContext ctxBucket[10][8];
  BitContext ctxMant;
  BitContext ctxRoot;
  // helper
  int is_null_slice(int bit, int band);
  int decode_prepare(int fbucket, int nbucket, IW44Image::Block &blk);
  void decode_buckets(ZPCodec &zp, int bit, int band,
    IW44Image::Block &blk, int fbucket, int nbucket);
};

//////////////////////////////////////////////////////
// DEFINITION OF CHUNK HEADERS
//////////////////////////////////////////////////////


struct IW44Image::PrimaryHeader {
  unsigned char serial;
  unsigned char slices;
  void encode(GP<ByteStream> gbs);
  void decode(GP<ByteStream> gbs);
};  

struct IW44Image::SecondaryHeader {
  unsigned char major;
  unsigned char minor;
  void encode(GP<ByteStream> gbs);
  void decode(GP<ByteStream> gbs);
};

struct IW44Image::TertiaryHeader {
  unsigned char xhi, xlo;
  unsigned char yhi, ylo;
  unsigned char crcbdelay;
  void encode(GP<ByteStream> gbs);
  void decode(GP<ByteStream> gbs, int major=1, int minor=2);
};

inline const short* 
IW44Image::Block::data(int n) const
{
  if (! pdata[n>>4])
    return 0;
  return pdata[n>>4][n&15];
}

inline short* 
IW44Image::Block::data(int n, IW44Image::Map *map)
{
  if (! pdata[n>>4])
    pdata[n>>4] = map->allocp(16);
  if (! pdata[n>>4][n &15])
    pdata[n>>4][n &15] = map->alloc(16);
  return pdata[n>>4][n&15];
}

inline short 
IW44Image::Block::get(int n) const
{
  int n1 = (n>>4);
  const short *d = data(n1);
  if (! d)
    return 0;
  return d[n&15];
}

inline void  
IW44Image::Block::set(int n, int val, IW44Image::Map *map)
{
  int n1 = (n>>4);
  short* d = data(n1, map);
  d[n&15] = val;
}

#endif /* IW44IMAGE_IMPLIMENTATION */

//@}


#ifdef HAVE_NAMESPACES
}
# ifndef NOT_USING_DJVU_NAMESPACE
using namespace DJVU;
# endif
#endif
#endif

