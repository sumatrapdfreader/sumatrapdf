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

// - Author: Leon Bottou, 08/1998
// From: Leon Bottou, 1/31/2002
// Lizardtech has split this file into a decoder and an encoder.
// Only superficial changes.  The meat is mine.

#define IW44IMAGE_IMPLIMENTATION /* */



#include "IW44Image.h"
#include "ZPCodec.h"
#include "GBitmap.h"
#include "GPixmap.h"
#include "IFFByteStream.h"
#include "GRect.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "MMX.h"
#undef IWTRANSFORM_TIMER
#ifdef IWTRANSFORM_TIMER
#include "GOS.h"
#endif

#include <assert.h>
#include <string.h>
#include <math.h>

#ifndef NEED_DECODER_ONLY


#ifdef HAVE_NAMESPACES
namespace DJVU {
# ifdef NOT_DEFINED // Just to fool emacs c++ mode
}
#endif
#endif

#define IWALLOCSIZE    4080
#define IWCODEC_MAJOR     1
#define IWCODEC_MINOR     2
#define DECIBEL_PRUNE   5.0


//////////////////////////////////////////////////////
// WAVELET DECOMPOSITION CONSTANTS
//////////////////////////////////////////////////////

// Parameters for IW44 wavelet.
// - iw_norm: norm of all wavelets (for db estimation)
// - iw_shift: scale applied before decomposition


static const float iw_norm[16] = {
  2.627989e+03F,
  1.832893e+02F, 1.832959e+02F, 5.114690e+01F,
  4.583344e+01F, 4.583462e+01F, 1.279225e+01F,
  1.149671e+01F, 1.149712e+01F, 3.218888e+00F,
  2.999281e+00F, 2.999476e+00F, 8.733161e-01F,
  1.074451e+00F, 1.074511e+00F, 4.289318e-01F
};

static const int iw_shift  = 6;
// static const int iw_round  = (1<<(iw_shift-1));

static const struct { int start; int size; }  
bandbuckets[] = 
{
  // Code first bucket and number of buckets in each band
  { 0, 1 }, // -- band zero contains all lores info
  { 1, 1 }, { 2, 1 }, { 3, 1 }, 
  { 4, 4 }, { 8, 4 }, { 12,4 }, 
  { 16,16 }, { 32,16 }, { 48,16 }, 
};


/** IW44 encoded gray-level image.  This class provided functions for managing
    a gray level image represented as a collection of IW44 wavelet
    coefficients.  The coefficients are stored in a memory efficient data
    structure.  Member function \Ref{get_bitmap} renders an arbitrary segment
    of the image into a \Ref{GBitmap}.  Member functions \Ref{decode_iff} and
    \Ref{encode_iff} read and write DjVu IW44 files (see \Ref{IW44Image.h}).
    Both the copy constructor and the copy operator are declared as private
    members. It is therefore not possible to make multiple copies of instances
    of this class. */

class IWBitmap::Encode : public IWBitmap
{
public:
  /// Destructor
  virtual ~Encode(void);
  /** Null constructor.  Constructs an empty IWBitmap object. This object does
      not contain anything meaningful. You must call function \Ref{init},
      \Ref{decode_iff} or \Ref{decode_chunk} to populate the wavelet
      coefficient data structure. */
  Encode(void);
  /** Initializes an IWBitmap with image #bm#.  This constructor
      performs the wavelet decomposition of image #bm# and records the
      corresponding wavelet coefficient.  Argument #mask# is an optional
      bilevel image specifying the masked pixels (see \Ref{IW44Image.h}). */
  void init(const GBitmap &bm, const GP<GBitmap> mask=0);
  // CODER
  /** Encodes one data chunk into ByteStream #bs#.  Parameter #parms# controls
      how much data is generated.  The chunk data is written to ByteStream
      #bs# with no IFF header.  Successive calls to #encode_chunk# encode
      successive chunks.  You must call #close_codec# after encoding the last
      chunk of a file. */
  virtual int  encode_chunk(GP<ByteStream> gbs, const IWEncoderParms &parms);
  /** Writes a gray level image into DjVu IW44 file.  This function creates a
      composite chunk (identifier #FORM:BM44#) composed of #nchunks# chunks
      (identifier #BM44#).  Data for each chunk is generated with
      #encode_chunk# using the corresponding parameters in array #parms#. */
  virtual void encode_iff(IFFByteStream &iff, int nchunks, const IWEncoderParms *parms);
  /** Resets the encoder/decoder state.  The first call to #decode_chunk# or
      #encode_chunk# initializes the coder for encoding or decoding.  Function
      #close_codec# must be called after processing the last chunk in order to
      reset the coder and release the associated memory. */
  virtual void close_codec(void);
protected:
  Codec::Encode *ycodec_enc;
};

/** IW44 encoded color image. This class provided functions for managing a
    color image represented as a collection of IW44 wavelet coefficients.  The
    coefficients are stored in a memory efficient data structure.  Member
    function \Ref{get_pixmap} renders an arbitrary segment of the image into a
    \Ref{GPixmap}.  Member functions \Ref{decode_iff} and \Ref{encode_iff}
    read and write DjVu IW44 files (see \Ref{IW44Image.h}).  Both the copy
    constructor and the copy operator are declared as private members. It is
    therefore not possible to make multiple copies of instances of this
    class. */

class IWPixmap::Encode : public IWPixmap
{
public:
  enum CRCBMode { 
    CRCBnone=IW44Image::CRCBnone, 
    CRCBhalf=IW44Image::CRCBhalf, 
    CRCBnormal=IW44Image::CRCBnormal, 
    CRCBfull=IW44Image::CRCBfull };
  /// Destructor
  virtual ~Encode(void);
  /** Null constructor.  Constructs an empty IWPixmap object. This object does
      not contain anything meaningful. You must call function \Ref{init},
      \Ref{decode_iff} or \Ref{decode_chunk} to populate the wavelet
      coefficient data structure. */
  Encode(void);
  /** Initializes an IWPixmap with color image #bm#.  This constructor
      performs the wavelet decomposition of image #bm# and records the
      corresponding wavelet coefficient.  Argument #mask# is an optional
      bilevel image specifying the masked pixels (see \Ref{IW44Image.h}).
      Argument #crcbmode# specifies how the chrominance information should be
      encoded (see \Ref{CRCBMode}). */
  void init(const GPixmap &bm, const GP<GBitmap> mask=0, CRCBMode crcbmode=CRCBnormal);
  // CODER
  /** Encodes one data chunk into ByteStream #bs#.  Parameter #parms# controls
      how much data is generated.  The chunk data is written to ByteStream
      #bs# with no IFF header.  Successive calls to #encode_chunk# encode
      successive chunks.  You must call #close_codec# after encoding the last
      chunk of a file. */
  virtual int  encode_chunk(GP<ByteStream> gbs, const IWEncoderParms &parms);
  /** Writes a color image into a DjVu IW44 file.  This function creates a
      composite chunk (identifier #FORM:PM44#) composed of #nchunks# chunks
      (identifier #PM44#).  Data for each chunk is generated with
      #encode_chunk# using the corresponding parameters in array #parms#. */
  virtual void encode_iff(IFFByteStream &iff, int nchunks, const IWEncoderParms *parms);
  /** Resets the encoder/decoder state.  The first call to #decode_chunk# or
      #encode_chunk# initializes the coder for encoding or decoding.  Function
      #close_codec# must be called after processing the last chunk in order to
      reset the coder and release the associated memory. */
  virtual void close_codec(void);
protected:
  Codec::Encode *ycodec_enc, *cbcodec_enc, *crcodec_enc;
};

class IW44Image::Map::Encode : public IW44Image::Map // DJVU_CLASS
{
public:
  Encode(const int w, const int h) : Map(w,h) {}
  // creation (from image)
  void create(const signed char *img8, int imgrowsize, 
              const signed char *msk8=0, int mskrowsize=0);
  // slash resolution
  void slashres(int res);
};

class IW44Image::Codec::Encode : public IW44Image::Codec
{
public:
  Encode(IW44Image::Map &map);
  // Coding
  virtual int code_slice(ZPCodec &zp);
  float estimate_decibel(float frac);
  // Data
  void encode_buckets(ZPCodec &zp, int bit, int band,
    IW44Image::Block &blk, IW44Image::Block &eblk, int fbucket, int nbucket);
  int encode_prepare(int band, int fbucket, int nbucket, IW44Image::Block &blk, IW44Image::Block &eblk);
  IW44Image::Map emap;
};

IW44Image::Codec::Encode::Encode(IW44Image::Map &map)
: Codec(map), emap(map.iw,map.ih) {}

//////////////////////////////////////////////////////
/** IW44Image::Transform::Encode
*/

class IW44Image::Transform::Encode : IW44Image::Transform
{
 public:
 // WAVELET TRANSFORM
  /** Forward transform. */
  static void forward(short *p, int w, int h, int rowsize, int begin, int end);
  
  // COLOR TRANSFORM
  /** Extracts Y */
  static void RGB_to_Y(const GPixel *p, int w, int h, int rowsize, 
                       signed char *out, int outrowsize);
  /** Extracts Cb */
  static void RGB_to_Cb(const GPixel *p, int w, int h, int rowsize, 
                        signed char *out, int outrowsize);
  /** Extracts Cr */
  static void RGB_to_Cr(const GPixel *p, int w, int h, int rowsize, 
                        signed char *out, int outrowsize);
};


//////////////////////////////////////////////////////
// MMX IMPLEMENTATION HELPERS
//////////////////////////////////////////////////////


// Note:
// MMX implementation for vertical transforms only.
// Speedup is basically related to faster memory transfer
// The IW44 transform is not CPU bound, it is memory bound.

#ifdef MMX

static const short w9[]  = {9,9,9,9};
static const short w1[]  = {1,1,1,1};
static const int   d8[]  = {8,8};
static const int   d16[] = {16,16};


static inline void
mmx_fv_1 ( short* &q, short* e, int s, int s3 )
{
  while (q<e && (((size_t)q)&0x7))
    {
      int a = (int)q[-s] + (int)q[s];
      int b = (int)q[-s3] + (int)q[s3];
      *q -= (((a<<3)+a-b+8)>>4);
      q++;
    }
  while (q+3<e)
    {
      MMXar( movq,       q-s,mm0);  // MM0=[ b3, b2, b1, b0 ]
      MMXar( movq,       q+s,mm2);  // MM2=[ c3, c2, c1, c0 ]
      MMXrr( movq,       mm0,mm1);  
      MMXrr( punpcklwd,  mm2,mm0);  // MM0=[ c1, b1, c0, b0 ]
      MMXrr( punpckhwd,  mm2,mm1);  // MM1=[ c3, b3, c2, b2 ]
      MMXar( pmaddwd,    w9,mm0);   // MM0=[ (c1+b1)*9, (c0+b0)*9 ]
      MMXar( pmaddwd,    w9,mm1);   // MM1=[ (c3+b3)*9, (c2+b2)*9 ]
      MMXar( movq,       q-s3,mm2);
      MMXar( movq,       q+s3,mm4);
      MMXrr( movq,       mm2,mm3);
      MMXrr( punpcklwd,  mm4,mm2);  // MM2=[ d1, a1, d0, a0 ]
      MMXrr( punpckhwd,  mm4,mm3);  // MM3=[ d3, a3, d2, a2 ]
      MMXar( pmaddwd,    w1,mm2);   // MM2=[ (a1+d1)*1, (a0+d0)*1 ]
      MMXar( pmaddwd,    w1,mm3);   // MM3=[ (a3+d3)*1, (a2+d2)*1 ]
      MMXar( paddd,      d8,mm0);
      MMXar( paddd,      d8,mm1);
      MMXrr( psubd,      mm2,mm0);  // MM0=[ (c1+b1)*9-a1-d1+8, ...
      MMXrr( psubd,      mm3,mm1);  // MM1=[ (c3+b3)*9-a3-d3+8, ...
      MMXir( psrad,      4,mm0);
      MMXar( movq,       q,mm7);    // MM7=[ p3,p2,p1,p0 ]
      MMXir( psrad,      4,mm1);
      MMXrr( packssdw,   mm1,mm0);  // MM0=[ x3,x2,x1,x0 ]
      MMXrr( psubw,      mm0,mm7);  // MM7=[ p3-x3, p2-x2, ... ]
      MMXra( movq,       mm7,q);
#if defined(_MSC_VER) && defined(_DEBUG)
      MMXemms;
#endif
      q += 4;
    }
}

static inline void
mmx_fv_2 ( short* &q, short* e, int s, int s3 )
{
  while (q<e && (((size_t)q)&0x7))
    {
      int a = (int)q[-s] + (int)q[s];
      int b = (int)q[-s3] + (int)q[s3];
      *q += (((a<<3)+a-b+16)>>5);
      q ++;
    }
  while (q+3<e)
    {
      MMXar( movq,       q-s,mm0);  // MM0=[ b3, b2, b1, b0 ]
      MMXar( movq,       q+s,mm2);  // MM2=[ c3, c2, c1, c0 ]
      MMXrr( movq,       mm0,mm1);  
      MMXrr( punpcklwd,  mm2,mm0);  // MM0=[ c1, b1, c0, b0 ]
      MMXrr( punpckhwd,  mm2,mm1);  // MM1=[ c3, b3, c2, b2 ]
      MMXar( pmaddwd,    w9,mm0);   // MM0=[ (c1+b1)*9, (c0+b0)*9 ]
      MMXar( pmaddwd,    w9,mm1);   // MM1=[ (c3+b3)*9, (c2+b2)*9 ]
      MMXar( movq,       q-s3,mm2);
      MMXar( movq,       q+s3,mm4);
      MMXrr( movq,       mm2,mm3);
      MMXrr( punpcklwd,  mm4,mm2);  // MM2=[ d1, a1, d0, a0 ]
      MMXrr( punpckhwd,  mm4,mm3);  // MM3=[ d3, a3, d2, a2 ]
      MMXar( pmaddwd,    w1,mm2);   // MM2=[ (a1+d1)*1, (a0+d0)*1 ]
      MMXar( pmaddwd,    w1,mm3);   // MM3=[ (a3+d3)*1, (a2+d2)*1 ]
      MMXar( paddd,      d16,mm0);
      MMXar( paddd,      d16,mm1);
      MMXrr( psubd,      mm2,mm0);  // MM0=[ (c1+b1)*9-a1-d1+8, ...
      MMXrr( psubd,      mm3,mm1);  // MM1=[ (c3+b3)*9-a3-d3+8, ...
      MMXir( psrad,      5,mm0);
      MMXar( movq,       q,mm7);    // MM7=[ p3,p2,p1,p0 ]
      MMXir( psrad,      5,mm1);
      MMXrr( packssdw,   mm1,mm0);  // MM0=[ x3,x2,x1,x0 ]
      MMXrr( paddw,      mm0,mm7);  // MM7=[ p3+x3, p2+x2, ... ]
      MMXra( movq,       mm7,q);
#if defined(_MSC_VER) && defined(_DEBUG)
      MMXemms;
#endif
      q += 4;
    }
}

#endif /* MMX */

//////////////////////////////////////////////////////
// NEW FILTERS
//////////////////////////////////////////////////////

static void 
filter_fv(short *p, int w, int h, int rowsize, int scale)
{
  int y = 0;
  int s = scale*rowsize;
  int s3 = s+s+s;
  h = (h>0) ? ((h-1)/scale)+1 : 0;
  y += 1;
  p += s;
  while (y-3 < h)
    {
      // 1-Delta
      {
        short *q = p;
        short *e = q+w;
        if (y>=3 && y+3<h)
          {
            // Generic case
#ifdef MMX
            if (scale==1 && MMXControl::mmxflag>0)
              mmx_fv_1(q, e, s, s3);
#endif
            while (q<e)
              {
                int a = (int)q[-s] + (int)q[s];
                int b = (int)q[-s3] + (int)q[s3];
                *q -= (((a<<3)+a-b+8)>>4);
                q += scale;
              }
          }
        else if (y<h)
          {
            // Special cases
            short *q1 = (y+1<h ? q+s : q-s);
            while (q<e)
              {
                int a = (int)q[-s] + (int)(*q1);
                *q -= ((a+1)>>1);
                q += scale;
                q1 += scale;
              }
          }
      }
      // 2-Update
      {
        short *q = p-s3;
        short *e = q+w;
        if (y>=6 && y<h)
          {
            // Generic case
#ifdef MMX
            if (scale==1 && MMXControl::mmxflag>0)
              mmx_fv_2(q, e, s, s3);
#endif
            while (q<e)
              {
                int a = (int)q[-s] + (int)q[s];
                int b = (int)q[-s3] + (int)q[s3];
                *q += (((a<<3)+a-b+16)>>5);
                q += scale;
              }
          }
        else if (y>=3)
          {
            // Special cases
            short *q1 = (y-2<h ? q+s : 0);
            short *q3 = (y<h ? q+s3 : 0);
            if (y>=6)
              {
                while (q<e)
                  {
                    int a = (int)q[-s] + (q1 ? (int)(*q1) : 0);
                    int b = (int)q[-s3] + (q3 ? (int)(*q3) : 0);
                    *q += (((a<<3)+a-b+16)>>5);
                    q += scale;
                    if (q1) q1 += scale;
                    if (q3) q3 += scale;
                  }
              }
            else if (y>=4)
              {
                while (q<e)
                  {
                    int a = (int)q[-s] + (q1 ? (int)(*q1) : 0);
                    int b = (q3 ? (int)(*q3) : 0);
                    *q += (((a<<3)+a-b+16)>>5);
                    q += scale;
                    if (q1) q1 += scale;
                    if (q3) q3 += scale;
                  }
              }
            else
              {
                while (q<e)
                  {
                    int a = (q1 ? (int)(*q1) : 0);
                    int b = (q3 ? (int)(*q3) : 0);
                    *q += (((a<<3)+a-b+16)>>5);
                    q += scale;
                    if (q1) q1 += scale;
                    if (q3) q3 += scale;
                  }
              }
          }
      }
      y += 2;
      p += s+s;
    }
}

static void 
filter_fh(short *p, int w, int h, int rowsize, int scale)
{
  int y = 0;
  int s = scale;
  int s3 = s+s+s;
  rowsize *= scale;
  while (y<h)
    {
      short *q = p+s;
      short *e = p+w;
      int a0=0, a1=0, a2=0, a3=0;
      int b0=0, b1=0, b2=0, b3=0;
      if (q < e)
        {
          // Special case: x=1
          a1 = a2 = a3 = q[-s];
          if (q+s<e)
            a2 = q[s];
          if (q+s3<e)
            a3 = q[s3];
          b3 = q[0] - ((a1+a2+1)>>1);
          q[0] = b3;
          q += s+s;
        }
      while (q+s3 < e)
        {
          // Generic case
          a0=a1; 
          a1=a2; 
          a2=a3;
          a3=q[s3];
          b0=b1; 
          b1=b2; 
          b2=b3;
          b3 = q[0] - ((((a1+a2)<<3)+(a1+a2)-a0-a3+8) >> 4);
          q[0] = b3;
          q[-s3] = q[-s3] + ((((b1+b2)<<3)+(b1+b2)-b0-b3+16) >> 5);
          q += s+s;
        }
      while (q < e)
        {
          // Special case: w-3 <= x < w
          a1=a2; 
          a2=a3;
          b0=b1; 
          b1=b2; 
          b2=b3;
          b3 = q[0] - ((a1+a2+1)>>1);
          q[0] = b3;
          q[-s3] = q[-s3] + ((((b1+b2)<<3)+(b1+b2)-b0-b3+16) >> 5);
          q += s+s;
        }
      while (q-s3 < e)
        {
          // Special case  w <= x < w+3
          b0=b1; 
          b1=b2; 
          b2=b3;
          b3=0;
          if (q-s3 >= p)
            q[-s3] = q[-s3] + ((((b1+b2)<<3)+(b1+b2)-b0-b3+16) >> 5);
          q += s+s;
        }
      y += scale;
      p += rowsize;
    }
}


//////////////////////////////////////////////////////
// WAVELET TRANSFORM 
//////////////////////////////////////////////////////


//----------------------------------------------------
// Function for applying bidimensional IW44 between 
// scale intervals begin(inclusive) and end(exclusive)

void
IW44Image::Transform::Encode::forward(short *p, int w, int h, int rowsize, int begin, int end)
{ 

  // PREPARATION
  filter_begin(w,h);
  // LOOP ON SCALES
  for (int scale=begin; scale<end; scale<<=1)
    {
#ifdef IWTRANSFORM_TIMER
      int tv,th;
      th = tv = GOS::ticks();
#endif
      filter_fh(p, w, h, rowsize, scale);
#ifdef IWTRANSFORM_TIMER
      th = GOS::ticks();
      tv = th - tv;
#endif
      filter_fv(p, w, h, rowsize, scale);
#ifdef IWTRANSFORM_TIMER
      th = GOS::ticks()-th;
      DjVuPrintErrorUTF8("forw%d\tv=%dms h=%dms\n", scale,th,tv);
#endif
    }
  // TERMINATE
  filter_end();
}

//////////////////////////////////////////////////////
// COLOR TRANSFORM 
//////////////////////////////////////////////////////

static const float 
rgb_to_ycc[3][3] = 
{ { 0.304348F,  0.608696F,  0.086956F },      
  { 0.463768F, -0.405797F, -0.057971F },
  {-0.173913F, -0.347826F,  0.521739F } };


/* Extracts Y */
void 
IW44Image::Transform::Encode::RGB_to_Y(const GPixel *p, int w, int h, int rowsize, 
                      signed char *out, int outrowsize)
{
  int rmul[256], gmul[256], bmul[256];
  for (int k=0; k<256; k++)
    {
      rmul[k] = (int)(k*0x10000*rgb_to_ycc[0][0]);
      gmul[k] = (int)(k*0x10000*rgb_to_ycc[0][1]);
      bmul[k] = (int)(k*0x10000*rgb_to_ycc[0][2]);
    }
  for (int i=0; i<h; i++, p+=rowsize, out+=outrowsize)
    {
      const GPixel *p2 = p;
      signed char *out2 = out;
      for (int j=0; j<w; j++,p2++,out2++)
        {
          int y = rmul[p2->r] + gmul[p2->g] + bmul[p2->b] + 32768;
          *out2 = (y>>16) - 128;
        }
    }
}

#ifdef min
#undef min
#endif
static inline int min(const int x,const int y) {return (x<y)?x:y;}
#ifdef max
#undef max
#endif
static inline int max(const int x,const int y) {return (x>y)?x:y;}

/* Extracts Cb */
void 
IW44Image::Transform::Encode::RGB_to_Cb(const GPixel *p, int w, int h, int rowsize, 
                       signed char *out, int outrowsize)
{
  int rmul[256], gmul[256], bmul[256];
  for (int k=0; k<256; k++)
    {
      rmul[k] = (int)(k*0x10000*rgb_to_ycc[2][0]);
      gmul[k] = (int)(k*0x10000*rgb_to_ycc[2][1]);
      bmul[k] = (int)(k*0x10000*rgb_to_ycc[2][2]);
    }
  for (int i=0; i<h; i++, p+=rowsize, out+=outrowsize)
    {
      const GPixel *p2 = p;
      signed char *out2 = out;
      for (int j=0; j<w; j++,p2++,out2++)
        {
          int c = rmul[p2->r] + gmul[p2->g] + bmul[p2->b] + 32768;
          *out2 = max(-128, min(127, c>>16));
        }
    }
}

/* Extracts Cr */
void 
IW44Image::Transform::Encode::RGB_to_Cr(const GPixel *p, int w, int h, int rowsize, 
                       signed char *out, int outrowsize)
{
  int rmul[256], gmul[256], bmul[256];
  for (int k=0; k<256; k++)
    {
      rmul[k] = (int)((k*0x10000)*rgb_to_ycc[1][0]);
      gmul[k] = (int)((k*0x10000)*rgb_to_ycc[1][1]);
      bmul[k] = (int)((k*0x10000)*rgb_to_ycc[1][2]);
    }
  for (int i=0; i<h; i++, p+=rowsize, out+=outrowsize)
    {
      const GPixel *p2 = p;
      signed char *out2 = out;
      for (int j=0; j<w; j++,p2++,out2++)
        {
          int c = rmul[p2->r] + gmul[p2->g] + bmul[p2->b] + 32768;
          *out2 = max(-128, min(127, c>>16));
        }
    }
}


//////////////////////////////////////////////////////
// MASKING DECOMPOSITION
//////////////////////////////////////////////////////

//----------------------------------------------------
// Function for applying bidimensional IW44 between 
// scale intervals begin(inclusive) and end(exclusive)
// with a MASK bitmap


static void
interpolate_mask(short *data16, int w, int h, int rowsize,
                 const signed char *mask8, int mskrowsize)
{
  int i,j;
  // count masked bits
  short *count;
  GPBuffer<short> gcount(count,w*h);
  short *cp = count;
  for (i=0; i<h; i++, cp+=w, mask8+=mskrowsize)
    for (j=0; j<w; j++)
      cp[j] = (mask8[j] ? 0 : 0x1000);
  // copy image
  short *sdata;
  GPBuffer<short> gsdata(sdata,w*h);
  short *p = sdata;
  short *q = data16;
  for (i=0; i<h; i++, p+=w, q+=rowsize)
    for (j=0; j<w; j++)
      p[j] = q[j];
  // iterate over resolutions
  int split = 1;
  int scale = 2;
  int again = 1;
  while (again && scale<w && scale<h)
    {
      again = 0;
      p = data16;
      q = sdata;
      cp = count;
      // iterate over block
      for (i=0; i<h; i+=scale, cp+=w*scale, q+=w*scale, p+=rowsize*scale)
        for (j=0; j<w; j+=scale)
          {
            int ii, jj;
            int gotz = 0;
            int gray = 0;
            int npix = 0;
            short *cpp = cp;
            short *qq = q;
            // look around when square goes beyond border
            int istart = i;
            if (istart+split>h)
              {
                istart -= scale;
                cpp -= w*scale;
                qq -= w*scale;
              }
            int jstart = j;
            if (jstart+split>w)
              jstart -= scale;
            // compute gray level
            for (ii=istart; ii<i+scale && ii<h; ii+=split, cpp+=w*split, qq+=w*split)
              for (jj=jstart; jj<j+scale && jj<w; jj+=split)
                {
                  if (cpp[jj]>0) 
                    {
                      npix += cpp[jj];
                      gray += cpp[jj] * qq[jj];
                    } 
                  else if (ii>=i && jj>=j)
                    {
                      gotz = 1;
                    }
                }
            // process result
            if (npix == 0)
              {
                // continue to next resolution
                again = 1;
                cp[j] = 0;
              }
            else
              {
                gray = gray / npix;
                // check whether initial image require fix
                if (gotz)
                  {
                    cpp = cp;
                    qq = p;
                    for (ii=i; ii<i+scale && ii<h; ii+=1, cpp+=w, qq+=rowsize)
                      for (jj=j; jj<j+scale && jj<w; jj+=1)
                        if (cpp[jj] == 0)
                          {
                            qq[jj] = gray;
                            cpp[jj] = 1;
                          }
                  }
                // store average for next iteration
                cp[j] = npix>>2;
                q[j] = gray;
              }
          }
      // double resolution
      split = scale;
      scale = scale+scale;
    }
}


static void
forward_mask(short *data16, int w, int h, int rowsize, int begin, int end,
             const signed char *mask8, int mskrowsize )
{
  int i,j;
  signed char *m;
  short *p;
  short *d;
  // Allocate buffers
  short *sdata;
  GPBuffer<short> gsdata(sdata,w*h);
  signed char *smask;
  GPBuffer<signed char> gsmask(smask,w*h);
  // Copy mask
  m = smask;
  for (i=0; i<h; i+=1, m+=w, mask8+=mskrowsize)
    memcpy((void*)m, (void*)mask8, w);
  // Loop over scale
  for (int scale=begin; scale<end; scale<<=1)
    {
      // Copy data into sdata buffer
      p = data16;
      d = sdata;
      for (i=0; i<h; i+=scale)
        {
          for (j=0; j<w; j+=scale)
            d[j] = p[j];
          p += rowsize * scale;
          d += w * scale;
        }
      // Decompose
      IW44Image::Transform::Encode::forward(sdata, w, h, w, scale, scale+scale);
      // Cancel masked coefficients
      d = sdata;
      m = smask;
      for (i=0; i<h; i+=scale+scale)
        {
          for (j=scale; j<w; j+=scale+scale)
            if (m[j])
              d[j] = 0;
          d += w * scale;
          m += w * scale;
          if (i+scale < h)
            {
              for (j=0; j<w; j+=scale)
                if (m[j])
                  d[j] = 0;
              d += w * scale;
              m += w * scale;
            }
        }
      // Reconstruct
      IW44Image::Transform::Decode::backward(sdata, w, h, w, scale+scale, scale);
      // Correct visible pixels
      p = data16;
      d = sdata;
      m = smask;
      for (i=0; i<h; i+=scale)
        {
          for (j=0; j<w; j+=scale)
            if (! m[j])
              d[j] = p[j];
          p += rowsize*scale;
          m += w*scale;
          d += w*scale;
        }
      // Decompose again (no need to iterate actually!)
      IW44Image::Transform::Encode::forward(sdata, w, h, w, scale, scale+scale);
      // Copy coefficients from sdata buffer
      p = data16;
      d = sdata;
      for (i=0; i<h; i+=scale)
        {
          for (j=0; j<w; j+=scale)
            p[j] = d[j];
          p += rowsize * scale;
          d += w * scale;
        }
      // Compute new mask for next scale
      m = smask;
      signed char *m0 = m;
      signed char *m1 = m;
      for (i=0; i<h; i+=scale+scale)
        {
          m0 = m1;
          if (i+scale < h)
            m1 = m + w*scale;
          for (j=0; j<w; j+=scale+scale)
            if (m[j] && m0[j] && m1[j] && (j<=0 || m[j-scale]) && (j+scale>=w || m[j+scale]))
              m[j] = 1;
            else
              m[j] = 0;
          m = m1 + w*scale;
        }
    }
  // Free buffers
}

void 
IW44Image::Map::Encode::create(const signed char *img8, int imgrowsize, 
               const signed char *msk8, int mskrowsize )
{
  int i, j;
  // Progress
  DJVU_PROGRESS_TASK(transf,"create iw44 map",3);
  // Allocate decomposition buffer
  short *data16;
  GPBuffer<short> gdata16(data16,bw*bh);
  // Copy pixels
  short *p = data16;
  const signed char *row = img8;
  for (i=0; i<ih; i++)
    {
      for (j=0; j<iw; j++)
        *p++ = (int)(row[j]) << iw_shift;
      row += imgrowsize;
      for (j=iw; j<bw; j++)
        *p++ = 0;
    }
  for (i=ih; i<bh; i++)
    for (j=0; j<bw; j++)
      *p++ = 0;
  // Handle bitmask
  if (msk8)
    {
      // Interpolate pixels below mask
      DJVU_PROGRESS_RUN(transf, 1);
      interpolate_mask(data16, iw, ih, bw, msk8, mskrowsize);
      // Multiscale iterative masked decomposition
      DJVU_PROGRESS_RUN(transf, 3);
      forward_mask(data16, iw, ih, bw, 1, 32, msk8, mskrowsize);
    }
  else
    {
      // Perform traditional decomposition
      DJVU_PROGRESS_RUN(transf, 3);
      IW44Image::Transform::Encode::forward(data16, iw, ih, bw, 1, 32);
    }
  // Copy coefficient into blocks
  p = data16;
  IW44Image::Block *block = blocks;
  for (i=0; i<bh; i+=32)
    {
      for (j=0; j<bw; j+=32)
        {
          short liftblock[1024];
          // transfer coefficients at (p+j) into aligned block
          short *pp = p + j;
          short *pl = liftblock;
          for (int ii=0; ii<32; ii++, pp+=bw)
            for (int jj=0; jj<32; jj++) 
              *pl++ = pp[jj];
          // transfer into IW44Image::Block (apply zigzag and scaling)
          block->read_liftblock(liftblock, this);
          block++;
        }
      // next row of blocks
      p += 32*bw;
    }
}

void 
IW44Image::Map::Encode::slashres(int res)
{
  int minbucket = 1;
  if (res < 2)
    return;
  else if (res < 4)
    minbucket=16;
  else if (res < 8)
    minbucket=4;
  for (int blockno=0; blockno<nb; blockno++)
    for (int buckno=minbucket; buckno<64; buckno++)
      blocks[blockno].zero(buckno);
}

// encode_prepare
// -- compute the states prior to encoding the buckets
int
IW44Image::Codec::Encode::encode_prepare(int band, int fbucket, int nbucket, IW44Image::Block &blk, IW44Image::Block &eblk)
{
  int bbstate = 0;
  // compute state of all coefficients in all buckets
  if (band) 
    {
      // Band other than zero
      int thres = quant_hi[band];
      char *cstate = coeffstate;
      for (int buckno=0; buckno<nbucket; buckno++, cstate+=16)
        {
          const short *pcoeff = blk.data(fbucket+buckno);
          const short *epcoeff = eblk.data(fbucket+buckno);
          int bstatetmp = 0;
          if (! pcoeff)
            {
              bstatetmp = UNK;
              // cstate[i] is not used and does not need initialization
            }
          else if (! epcoeff)
            {
              for (int i=0; i<16; i++)
                {
                  int cstatetmp = UNK;
                  if  ((int)(pcoeff[i])>=thres || (int)(pcoeff[i])<=-thres)
                    cstatetmp = NEW|UNK;
                  cstate[i] = cstatetmp;
                  bstatetmp |= cstatetmp;
                }
            }
          else
            {
              for (int i=0; i<16; i++)
                {
                  int cstatetmp = UNK;
                  if (epcoeff[i])
                    cstatetmp = ACTIVE;
                  else if  ((int)(pcoeff[i])>=thres || (int)(pcoeff[i])<=-thres)
                    cstatetmp = NEW|UNK;
                  cstate[i] = cstatetmp;
                  bstatetmp |= cstatetmp;
                }
            }
          bucketstate[buckno] = bstatetmp;
          bbstate |= bstatetmp;
        }
    }
  else
    {
      // Band zero ( fbucket==0 implies band==zero and nbucket==1 )
      const short *pcoeff = blk.data(0, &map);
      const short *epcoeff = eblk.data(0, &emap);
      char *cstate = coeffstate;
      for (int i=0; i<16; i++)
        {
          int thres = quant_lo[i];
          int cstatetmp = cstate[i];
          if (cstatetmp != ZERO)
            {
              cstatetmp = UNK;
              if (epcoeff[i])
                cstatetmp = ACTIVE;
              else if ((int)(pcoeff[i])>=thres || (int)(pcoeff[i])<=-thres)
                cstatetmp = NEW|UNK;
            }
          cstate[i] = cstatetmp;
          bbstate |= cstatetmp;
        }
      bucketstate[0] = bbstate;
    }
  return bbstate;
}

// encode_buckets
// -- code a sequence of buckets in a given block
void
IW44Image::Codec::Encode::encode_buckets(ZPCodec &zp, int bit, int band, 
                         IW44Image::Block &blk, IW44Image::Block &eblk,
                         int fbucket, int nbucket)
{
  // compute state of all coefficients in all buckets
  int bbstate = encode_prepare(band, fbucket, nbucket, blk, eblk);

  // code root bit
  if ((nbucket<16) || (bbstate&ACTIVE))
    {
      bbstate |= NEW;
    }
  else if (bbstate & UNK)
    {
      zp.encoder( (bbstate&NEW) ? 1 : 0 , ctxRoot);
#ifdef TRACE
      DjVuPrintMessage("bbstate[bit=%d,band=%d] = %d\n", bit, band, bbstate);
#endif
    }
  
  // code bucket bits
  if (bbstate & NEW)
    for (int buckno=0; buckno<nbucket; buckno++)
      {
        // Code bucket bit
        if (bucketstate[buckno] & UNK)
          {
            // Context
            int ctx = 0;
#ifndef NOCTX_BUCKET_UPPER
            if (band>0)
              {
                int k = (fbucket+buckno)<<2;
                const short *b = eblk.data(k>>4);
                if (b)
                  {
                    k = k & 0xf;
                    if (b[k])
                      ctx += 1;
                    if (b[k+1])
                      ctx += 1;
                    if (b[k+2])
                      ctx += 1;
                    if (ctx<3 && b[k+3])
                      ctx += 1;
                  }
              }
#endif
#ifndef NOCTX_BUCKET_ACTIVE
            if (bbstate & ACTIVE)
              ctx |= 4; 
#endif
            // Code
            zp.encoder( (bucketstate[buckno]&NEW) ? 1 : 0, ctxBucket[band][ctx] );
#ifdef TRACE
            DjVuPrintMessage("  bucketstate[bit=%d,band=%d,buck=%d] = %d\n", 
                   bit, band, buckno, bucketstate[buckno] & ~ZERO);
#endif
          }
      }
  
  // code new active coefficient (with their sign)
  if (bbstate & NEW)
    {
      int thres = quant_hi[band];
      char *cstate = coeffstate;
      for (int buckno=0; buckno<nbucket; buckno++, cstate+=16)
        if (bucketstate[buckno] & NEW)
          {
            int i;
#ifndef NOCTX_EXPECT
            int gotcha = 0;
            const int maxgotcha = 7;
            for (i=0; i<16; i++)
              if (cstate[i] & UNK)
                gotcha += 1;
#endif
            const short *pcoeff = blk.data(fbucket+buckno);
            short *epcoeff = eblk.data(fbucket+buckno, &emap);
            // iterate within bucket
            for (i=0; i<16; i++)
              {
                if (cstate[i] & UNK)
                  {
                    // Prepare context
                    int ctx = 0;
#ifndef NOCTX_EXPECT
                    if (gotcha>=maxgotcha)
                      ctx = maxgotcha;
                    else
                      ctx = gotcha;
#endif
#ifndef NOCTX_ACTIVE
                    if (bucketstate[buckno] & ACTIVE)
                      ctx |= 8;
#endif
                    // Code
                    zp.encoder( (cstate[i]&NEW) ? 1 : 0, ctxStart[ctx] );
                    if (cstate[i] & NEW)
                      {
                        // Code sign
                        zp.IWencoder( (pcoeff[i]<0) ? 1 : 0 );
                        // Set encoder state
                        if (band==0)
                          thres = quant_lo[i];
                        epcoeff[i] = thres + (thres>>1);
                      }
#ifndef NOCTX_EXPECT
                    if (cstate[i] & NEW)
                      gotcha = 0;
                    else if (gotcha > 0)
                      gotcha -= 1;
#endif
#ifdef TRACE
                    DjVuPrintMessage("    coeffstate[bit=%d,band=%d,buck=%d,c=%d] = %d\n", 
                           bit, band, buckno, i, cstate[i]);
#endif
                  }
              }
          }
    }

  // code mantissa bits
  if (bbstate & ACTIVE)
    {
      int thres = quant_hi[band];
      char *cstate = coeffstate;
      for (int buckno=0; buckno<nbucket; buckno++, cstate+=16)
        if (bucketstate[buckno] & ACTIVE)
          {
            const short *pcoeff = blk.data(fbucket+buckno);
            short *epcoeff = eblk.data(fbucket+buckno, &emap);
            for (int i=0; i<16; i++)
              if (cstate[i] & ACTIVE)
                {
                  // get coefficient
                  int coeff = pcoeff[i];
                  int ecoeff = epcoeff[i];
                  if (coeff < 0)
                    coeff = -coeff;
                  // get band zero thresholds
                  if (band == 0)
                    thres = quant_lo[i];
                  // compute mantissa bit
                  int pix = 0;
                  if (coeff >= ecoeff)
                    pix = 1;
                  // encode second or lesser mantissa bit
                  if (ecoeff <= 3*thres)
                    zp.encoder(pix, ctxMant);                      
                  else
					  zp.IWencoder(!!pix);
                  // adjust epcoeff
                  epcoeff[i] = ecoeff - (pix ? 0 : thres) + (thres>>1);
                }
          }
    }
}

// IW44Image::Codec::estimate_decibel
// -- estimate encoding error (after code_slice) in decibels.
float
IW44Image::Codec::Encode::estimate_decibel(float frac)
{
  int i,j;
  const float *q;
  // Fill norm arrays
  float norm_lo[16];
  float norm_hi[10];
  // -- lo coefficients
  q = iw_norm;
  for (i=j=0; i<4; j++)
    norm_lo[i++] = *q++;
  for (j=0; j<4; j++)
    norm_lo[i++] = *q;
  q += 1;
  for (j=0; j<4; j++)
    norm_lo[i++] = *q;
  q += 1;
  for (j=0; j<4; j++)
    norm_lo[i++] = *q;
  q += 1;
  // -- hi coefficients
  norm_hi[0] = 0;
  for (j=1; j<10; j++)
    norm_hi[j] = *q++;
  // Initialize mse array
  float *xmse;
  GPBuffer<float> gxmse(xmse,map.nb);
  // Compute mse in each block
  for (int blockno=0; blockno<map.nb; blockno++)
    {
      float mse = 0;
      // Iterate over bands
      for (int bandno=0; bandno<10; bandno++)
        {
          int fbucket = bandbuckets[bandno].start;
          int nbucket = bandbuckets[bandno].size;
          IW44Image::Block &blk = map.blocks[blockno];
          IW44Image::Block &eblk = emap.blocks[blockno];
          float norm = norm_hi[bandno];
          for (int buckno=0; buckno<nbucket; buckno++)
            {
              const short *pcoeff = blk.data(fbucket+buckno);
              const short *epcoeff = eblk.data(fbucket+buckno);
              if (pcoeff)
                {
                  if (epcoeff)
                    {
                      for (i=0; i<16; i++)
                        {
                          if (bandno == 0)
                            norm = norm_lo[i];
                          float delta = (float)(pcoeff[i]<0 ? -pcoeff[i] : pcoeff[i]);
                          delta = delta - epcoeff[i];
                          mse = mse + norm * delta * delta;
                        }
                    }
                  else
                    {
                      for (i=0; i<16; i++)
                        {
                          if (bandno == 0)
                            norm = norm_lo[i];
                          float delta = (float)(pcoeff[i]);
                          mse = mse + norm * delta * delta;
                        }
                    }
                }
            }
        }
      xmse[blockno] = mse / 1024;
    }
  // Compute partition point
  int n = 0;
  int m = map.nb - 1;
  int p = (int)floor(m*(1.0-frac)+0.5);
  p = (p>m ? m : (p<0 ? 0 : p));
  float pivot = 0;
  // Partition array
  while (n < p)
    {
      int l = n;
      int h = m;
      if (xmse[l] > xmse[h]) { float tmp=xmse[l]; xmse[l]=xmse[h]; xmse[h]=tmp; }
      pivot = xmse[(l+h)/2];
      if (pivot < xmse[l]) { float tmp=pivot; pivot=xmse[l]; xmse[l]=tmp; }
      if (pivot > xmse[h]) { float tmp=pivot; pivot=xmse[h]; xmse[h]=tmp; }
      while (l < h)
        {
          if (xmse[l] > xmse[h]) { float tmp=xmse[l]; xmse[l]=xmse[h]; xmse[h]=tmp; }
          while (xmse[l]<pivot || (xmse[l]==pivot && l<h)) l++;
          while (xmse[h]>pivot) h--;
        }
      if (p>=l) 
        n = l;
      else 
        m = l-1;
    }
  // Compute average mse
  float mse = 0;
  for (i=p; i<map.nb; i++)
    mse = mse + xmse[i];
  mse = mse / (map.nb - p);
  // Return
  float factor = 255 << iw_shift;
  float decibel = (float)(10.0 * log ( factor * factor / mse ) / 2.302585125);
  return decibel;
}




//////////////////////////////////////////////////////
// IW44IMAGE ENCODING ROUTINES
//////////////////////////////////////////////////////


void 
IW44Image::PrimaryHeader::encode(GP<ByteStream> gbs)
{
  gbs->write8(serial);
  gbs->write8(slices);
}

void 
IW44Image::SecondaryHeader::encode(GP<ByteStream> gbs)
{
  gbs->write8(major);
  gbs->write8(minor);
}

void 
IW44Image::TertiaryHeader::encode(GP<ByteStream> gbs)
{
  gbs->write8(xhi);
  gbs->write8(xlo);
  gbs->write8(yhi);
  gbs->write8(ylo);
  gbs->write8(crcbdelay);
}



GP<IW44Image>
IW44Image::create_encode(const ImageType itype)
{
  switch(itype)
  {
  case COLOR:
    return new IWPixmap::Encode();
  case GRAY:
    return new IWBitmap::Encode();
  default:
    return 0;
  }
}

GP<IW44Image>
IW44Image::create_encode(const GBitmap &bm, const GP<GBitmap> mask)
{
  IWBitmap::Encode *bit=new IWBitmap::Encode();
  GP<IW44Image> retval=bit;
  bit->init(bm, mask);
  return retval;
}


IWBitmap::Encode::Encode(void)
: IWBitmap(), ycodec_enc(0)
{}

IWBitmap::Encode::~Encode()
{
  close_codec();
}

void
IWBitmap::Encode::init(const GBitmap &bm, const GP<GBitmap> gmask)
{
  // Free
  close_codec();
  delete ymap;
  ymap = 0;
  // Init
  int i, j;
  int w = bm.columns();
  int h = bm.rows();
  int g = bm.get_grays()-1;
  signed char *buffer;
  GPBuffer<signed char> gbuffer(buffer,w*h);
  // Prepare gray level conversion table
  signed char  bconv[256];
  for (i=0; i<256; i++)
    bconv[i] = max(0,min(255,i*255/g)) - 128;
  // Perform decomposition
  // Prepare mask information
  const signed char *msk8 = 0;
  int mskrowsize = 0;
  GBitmap *mask=gmask;
  if (gmask)
  {
    msk8 = (const signed char*)((*mask)[0]);
    mskrowsize = mask->rowsize();
  }
  // Prepare a buffer of signed bytes
  for (i=0; i<h; i++)
    {
      signed char *bufrow = buffer + i*w;
      const unsigned char *bmrow = bm[i];
      for (j=0; j<w; j++)
        bufrow[j] = bconv[bmrow[j]];
    }
  // Create map
  Map::Encode *eymap=new Map::Encode(w,h);
  ymap = eymap;
  eymap->create(buffer, w, msk8, mskrowsize);
}

void 
IWBitmap::Encode::close_codec(void)
{
  delete ycodec_enc;
  ycodec_enc = 0;
  IWBitmap::close_codec();
}

int  
IWBitmap::Encode::encode_chunk(GP<ByteStream> gbs, const IWEncoderParms &parm)
{
  // Check
  if (parm.slices==0 && parm.bytes==0 && parm.decibels==0)
    G_THROW( ERR_MSG("IW44Image.need_stop") );
  if (! ymap)
    G_THROW( ERR_MSG("IW44Image.empty_object") );
  // Open codec
  if (!ycodec_enc)
  {
    cslice = cserial = cbytes = 0;
    ycodec_enc = new Codec::Encode(*ymap);
  }
  // Adjust cbytes
  cbytes += sizeof(struct IW44Image::PrimaryHeader);
  if (cserial == 0)
    cbytes += sizeof(struct IW44Image::SecondaryHeader) + sizeof(struct IW44Image::TertiaryHeader);
  // Prepare zcoded slices
  int flag = 1;
  int nslices = 0;
  GP<ByteStream> gmbs=ByteStream::create();
  ByteStream &mbs=*gmbs;
  DJVU_PROGRESS_TASK(chunk,"encode chunk",parm.slices-cslice);
  {
    float estdb = -1.0;
    GP<ZPCodec> gzp=ZPCodec::create(gmbs, true, true);
    ZPCodec &zp=*gzp;
    while (flag)
      {
        if (parm.decibels>0  && estdb>=parm.decibels)
          break;
        if (parm.bytes>0  && mbs.tell()+cbytes>=parm.bytes)
          break;
        if (parm.slices>0 && nslices+cslice>=parm.slices)
          break;
        DJVU_PROGRESS_RUN(chunk, (1+nslices-cslice)|0xf);
        flag = ycodec_enc->code_slice(zp);
        if (flag && parm.decibels>0.0)
          if (ycodec_enc->curband==0 || estdb>=parm.decibels-DECIBEL_PRUNE)
            estdb = ycodec_enc->estimate_decibel(db_frac);
        nslices++;
      }
  }
  // Write primary header
  struct IW44Image::PrimaryHeader primary;
  primary.serial = cserial;
  primary.slices = nslices;
  primary.encode(gbs);
  // Write auxilliary headers
  if (cserial == 0)
    {
      struct IW44Image::SecondaryHeader secondary;
      secondary.major = IWCODEC_MAJOR + 0x80;
      secondary.minor = IWCODEC_MINOR;
      secondary.encode(gbs);
      struct IW44Image::TertiaryHeader tertiary;
      tertiary.xhi = (ymap->iw >> 8) & 0xff;
      tertiary.xlo = (ymap->iw >> 0) & 0xff;
      tertiary.yhi = (ymap->ih >> 8) & 0xff;
      tertiary.ylo = (ymap->ih >> 0) & 0xff;
      tertiary.crcbdelay = 0;
      tertiary.encode(gbs);
    }
  // Write slices
  mbs.seek(0);
  gbs->copy(mbs);
  // Return
  cbytes  += mbs.tell();
  cslice  += nslices;
  cserial += 1;
  return flag;
}

void 
IWBitmap::Encode::encode_iff(IFFByteStream &iff, int nchunks, const IWEncoderParms *parms)
{
  if (ycodec_enc)
    G_THROW( ERR_MSG("IW44Image.left_open1") );
  int flag = 1;
  iff.put_chunk("FORM:BM44", 1);
  DJVU_PROGRESS_TASK(iff,"encode iff chunk",nchunks);
  for (int i=0; flag && i<nchunks; i++)
    {
      DJVU_PROGRESS_RUN(iff,i+1);
      iff.put_chunk("BM44");
      flag = encode_chunk(iff.get_bytestream(),parms[i]);
      iff.close_chunk();
    }
  iff.close_chunk();
  close_codec();
}

GP<IW44Image>
IW44Image::create_encode(
  const GPixmap &pm, const GP<GBitmap> gmask, CRCBMode crcbmode)
{
  IWPixmap::Encode *pix=new IWPixmap::Encode();
  GP<IW44Image> retval=pix;
  pix->init(pm, gmask,(IWPixmap::Encode::CRCBMode)crcbmode);
  return retval;
}

IWPixmap::Encode::Encode(void)
: IWPixmap(), ycodec_enc(0), cbcodec_enc(0), crcodec_enc(0)
{}

IWPixmap::Encode::~Encode()
{
  close_codec();
}

void
IWPixmap::Encode::init(const GPixmap &pm, const GP<GBitmap> gmask, CRCBMode crcbmode)
{
  /* Free */
  close_codec();
  delete ymap;
  delete cbmap;
  delete crmap;
  ymap = cbmap = crmap = 0;
  /* Create */
  int w = pm.columns();
  int h = pm.rows();
  signed char *buffer;
  GPBuffer<signed char> gbuffer(buffer,w*h);
  // Create maps
  Map::Encode *eymap = new Map::Encode(w,h);
  ymap = eymap;
  // Handle CRCB mode
  switch (crcbmode) 
    {
    case CRCBnone:   crcb_half=1; crcb_delay=-1; break;
    case CRCBhalf:   crcb_half=1; crcb_delay=10; break;        
    case CRCBnormal: crcb_half=0; crcb_delay=10; break;
    case CRCBfull:   crcb_half=0; crcb_delay= 0; break;
    }
  // Prepare mask information
  const signed char *msk8 = 0;
  int mskrowsize = 0;
  GBitmap *mask=gmask;
  if (mask)
  {
    msk8 = (signed char const *)((*mask)[0]);
    mskrowsize = mask->rowsize();
  }
  // Fill buffer with luminance information
  DJVU_PROGRESS_TASK(create,"initialize pixmap",3);
  DJVU_PROGRESS_RUN(create,(crcb_delay>=0 ? 1 : 3));
  Transform::Encode::RGB_to_Y(pm[0], w, h, pm.rowsize(), buffer, w);
  if (crcb_delay < 0)
    {
      // Stupid inversion for gray images
      signed char *e = buffer + w*h;
      for (signed char *b=buffer; b<e; b++)
        *b = 255 - *b;
    }
  // Create YMAP
  eymap->create(buffer, w, msk8, mskrowsize);
  // Create chrominance maps
  if (crcb_delay >= 0)
    {
      Map::Encode *ecbmap = new Map::Encode(w,h);
      cbmap = ecbmap;
      Map::Encode *ecrmap = new Map::Encode(w,h);
      crmap = ecrmap;
      // Process CB information
      DJVU_PROGRESS_RUN(create,2);
      Transform::Encode::RGB_to_Cb(pm[0], w, h, pm.rowsize(), buffer, w);
      ecbmap->create(buffer, w, msk8, mskrowsize);
      // Process CR information
      DJVU_PROGRESS_RUN(create,3);
      Transform::Encode::RGB_to_Cr(pm[0], w, h, pm.rowsize(), buffer, w); 
      ecrmap->create(buffer, w, msk8, mskrowsize);
      // Perform chrominance reduction (CRCBhalf)
      if (crcb_half)
        {
          ecbmap->slashres(2);
          ecrmap->slashres(2);
        }
    }
}

void 
IWPixmap::Encode::encode_iff(IFFByteStream &iff, int nchunks, const IWEncoderParms *parms)
{
  if (ycodec_enc)
    G_THROW( ERR_MSG("IW44Image.left_open3") );
  int flag = 1;
  iff.put_chunk("FORM:PM44", 1);
  DJVU_PROGRESS_TASK(iff,"encode pixmap chunk", nchunks);
  for (int i=0; flag && i<nchunks; i++)
    {
      DJVU_PROGRESS_RUN(iff,i+1);
      iff.put_chunk("PM44");
      flag = encode_chunk(iff.get_bytestream(), parms[i]);
      iff.close_chunk();
    }
  iff.close_chunk();
  close_codec();
}

void 
IWPixmap::Encode::close_codec(void)
{
  delete ycodec_enc;
  delete cbcodec_enc;
  delete crcodec_enc;
  ycodec_enc = crcodec_enc = cbcodec_enc = 0;
  IWPixmap::close_codec();
}

int  
IWPixmap::Encode::encode_chunk(GP<ByteStream> gbs, const IWEncoderParms &parm)
{
  // Check
  if (parm.slices==0 && parm.bytes==0 && parm.decibels==0)
    G_THROW( ERR_MSG("IW44Image.need_stop2") );
  if (!ymap)
    G_THROW( ERR_MSG("IW44Image.empty_object2") );
  // Open
  if (!ycodec_enc)
  {
    cslice = cserial = cbytes = 0;
    ycodec_enc = new Codec::Encode(*ymap);
    if (crmap && cbmap)
    {
      cbcodec_enc = new Codec::Encode(*cbmap);
      crcodec_enc = new Codec::Encode(*crmap);
    }
  }

  // Adjust cbytes
  cbytes += sizeof(struct IW44Image::PrimaryHeader);
  if (cserial == 0)
    cbytes += sizeof(struct IW44Image::SecondaryHeader) + sizeof(struct IW44Image::TertiaryHeader);
  // Prepare zcodec slices
  int flag = 1;
  int nslices = 0;
  GP<ByteStream> gmbs=ByteStream::create();
  ByteStream &mbs=*gmbs;
  DJVU_PROGRESS_TASK(chunk, "encode pixmap chunk", parm.slices-cslice);
  {
    float estdb = -1.0;
    GP<ZPCodec> gzp=ZPCodec::create(gmbs, true, true);
    ZPCodec &zp=*gzp;
    while (flag)
      {
        if (parm.decibels>0  && estdb>=parm.decibels)
          break;
        if (parm.bytes>0  && mbs.tell()+cbytes>=parm.bytes)
          break;
        if (parm.slices>0 && nslices+cslice>=parm.slices)
          break;
        DJVU_PROGRESS_RUN(chunk,(1+nslices-cslice)|0xf);
        flag = ycodec_enc->code_slice(zp);
        if (flag && parm.decibels>0)
          if (ycodec_enc->curband==0 || estdb>=parm.decibels-DECIBEL_PRUNE)
            estdb = ycodec_enc->estimate_decibel(db_frac);
        if (crcodec_enc && cbcodec_enc && cslice+nslices>=crcb_delay)
          {
            flag |= cbcodec_enc->code_slice(zp);
            flag |= crcodec_enc->code_slice(zp);
          }
        nslices++;
      }
  }
  // Write primary header
  struct IW44Image::PrimaryHeader primary;
  primary.serial = cserial;
  primary.slices = nslices;
  primary.encode(gbs);
  // Write secondary header
  if (cserial == 0)
    {
      struct IW44Image::SecondaryHeader secondary;
      secondary.major = IWCODEC_MAJOR;
      secondary.minor = IWCODEC_MINOR;
      if (! (crmap && cbmap))
        secondary.major |= 0x80;
      secondary.encode(gbs);
      struct IW44Image::TertiaryHeader tertiary;
      tertiary.xhi = (ymap->iw >> 8) & 0xff;
      tertiary.xlo = (ymap->iw >> 0) & 0xff;
      tertiary.yhi = (ymap->ih >> 8) & 0xff;
      tertiary.ylo = (ymap->ih >> 0) & 0xff;
      tertiary.crcbdelay = (crcb_half ? 0x00 : 0x80);
      tertiary.crcbdelay |= (crcb_delay>=0 ? crcb_delay : 0x00);
      tertiary.encode(gbs);
    }
  // Write slices
  mbs.seek(0);
  gbs->copy(mbs);
  // Return
  cbytes  += mbs.tell();
  cslice  += nslices;
  cserial += 1;
  return flag;
}

// code_slice
// -- read/write a slice of datafile

int
IW44Image::Codec::Encode::code_slice(ZPCodec &zp)
{
  // Check that code_slice can still run
  if (curbit < 0)
    return 0;
  // Perform coding
  if (! is_null_slice(curbit, curband))
    {
      for (int blockno=0; blockno<map.nb; blockno++)
        {
          const int fbucket = bandbuckets[curband].start;
          const int nbucket = bandbuckets[curband].size;
          encode_buckets(zp, curbit, curband, 
                         map.blocks[blockno], emap.blocks[blockno], 
                         fbucket, nbucket);
        }
    }
  return finish_code_slice(zp);
}



#ifdef HAVE_NAMESPACES
}
# ifndef NOT_USING_DJVU_NAMESPACE
using namespace DJVU;
# endif
#endif
#endif // NEED_DECODER_ONLY

