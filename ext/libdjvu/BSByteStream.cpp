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

// - Author: Leon Bottou, 07/1998

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "BSByteStream.h"
#undef BSORT_TIMER
#ifdef BSORT_TIMER
#include "GOS.h"
#endif


#ifdef HAVE_NAMESPACES
namespace DJVU {
# ifdef NOT_DEFINED // Just to fool emacs c++ mode
}
#endif
#endif

class BSByteStream::Decode : public BSByteStream
{
public:
  /** Creates a Static object for allocating the memory area of
      length #sz# starting at address #buffer#. */
  Decode(GP<ByteStream> bs);
  ~Decode();
  void init(void);
  // Virtual functions
  virtual size_t read(void *buffer, size_t sz);
  virtual void flush(void);
protected:
  unsigned int decode(void);
private:
  bool eof;
};

// ========================================
// --- Assertion

#define ASSERT(expr) do{if(!(expr))G_THROW("assertion ("#expr") failed");}while(0)

// ========================================
// --- Construction

BSByteStream::BSByteStream(GP<ByteStream> xbs)
: offset(0), bptr(0), blocksize(0), size(0), bs(xbs),
  gbs(xbs), gdata(data,0)
{
  // Initialize context array
  memset(ctx, 0, sizeof(ctx));
}

BSByteStream::~BSByteStream() {}

BSByteStream::Decode::Decode(GP<ByteStream> xbs)
: BSByteStream(xbs), eof(false) {}

void
BSByteStream::Decode::init(void)
{
  gzp=ZPCodec::create(gbs,false,true);
}

BSByteStream::Decode::~Decode() {}

GP<ByteStream>
BSByteStream::create(GP<ByteStream> xbs)
{
  BSByteStream::Decode *rbs=new BSByteStream::Decode(xbs);
  GP<ByteStream> retval=rbs;
  rbs->init();
  return retval;
}

void 
BSByteStream::Decode::flush()
{
  size = bptr = 0;
}

// ========================================
// -- Decoding


static int 
decode_raw(ZPCodec &zp, int bits)
{
  int n = 1;
  const int m = (1<<bits);
  while (n < m)
    {
      const int b = zp.decoder();
      n = (n<<1) | b;
    }
  return n - m;
}

static inline int 
decode_binary(ZPCodec &zp, BitContext *ctx, int bits)
{
  int n = 1;
  int m = (1<<bits);
  ctx = ctx - 1;
  while (n < m)
    {
      int b = zp.decoder(ctx[n]);
      n = (n<<1) | b;
    }
  return n - m;
}


static inline void
assignmtf(unsigned char xmtf[256])
{
  static const unsigned char mtf[256]={
    0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
    0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,
    0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,
    0x18,0x19,0x1A,0x1B,0x1C,0x1D,0x1E,0x1F,
    0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,
    0x28,0x29,0x2A,0x2B,0x2C,0x2D,0x2E,0x2F,
    0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,
    0x38,0x39,0x3A,0x3B,0x3C,0x3D,0x3E,0x3F,
    0x40,0x41,0x42,0x43,0x44,0x45,0x46,0x47,
    0x48,0x49,0x4A,0x4B,0x4C,0x4D,0x4E,0x4F,
    0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57,
    0x58,0x59,0x5A,0x5B,0x5C,0x5D,0x5E,0x5F,
    0x60,0x61,0x62,0x63,0x64,0x65,0x66,0x67,
    0x68,0x69,0x6A,0x6B,0x6C,0x6D,0x6E,0x6F,
    0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,
    0x78,0x79,0x7A,0x7B,0x7C,0x7D,0x7E,0x7F,
    0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,
    0x88,0x89,0x8A,0x8B,0x8C,0x8D,0x8E,0x8F,
    0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x97,
    0x98,0x99,0x9A,0x9B,0x9C,0x9D,0x9E,0x9F,
    0xA0,0xA1,0xA2,0xA3,0xA4,0xA5,0xA6,0xA7,
    0xA8,0xA9,0xAA,0xAB,0xAC,0xAD,0xAE,0xAF,
    0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,
    0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF,
    0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,
    0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF,
    0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,
    0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF,
    0xE0,0xE1,0xE2,0xE3,0xE4,0xE5,0xE6,0xE7,
    0xE8,0xE9,0xEA,0xEB,0xEC,0xED,0xEE,0xEF,
    0xF0,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,
    0xF8,0xF9,0xFA,0xFB,0xFC,0xFD,0xFE,0xFF};
  memcpy(xmtf,mtf,sizeof(mtf));
}
  
unsigned int
BSByteStream::Decode::decode(void)
{
  /////////////////////////////////
  ////////////  Decode input stream
  
  int i;
  // Decode block size
  ZPCodec &zp=*gzp;
  size = decode_raw(zp, 24);
  if (!size)
    return 0;
  if (size>MAXBLOCK*1024)
    G_THROW( ERR_MSG("ByteStream.corrupt") );
  // Allocate
  if ((int)blocksize < size)
    {
      blocksize = size;
      if (data)
      {
        gdata.resize(0);
      }
    }
  if (! data) 
    gdata.resize(blocksize);
  // Decode Estimation Speed
  int fshift = 0;
  if (zp.decoder())
    {
      fshift += 1;
      if (zp.decoder())
        fshift += 1;
    }
  // Prepare Quasi MTF
  static const unsigned char xmtf[256]={
    0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
    0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,
    0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,
    0x18,0x19,0x1A,0x1B,0x1C,0x1D,0x1E,0x1F,
    0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,
    0x28,0x29,0x2A,0x2B,0x2C,0x2D,0x2E,0x2F,
    0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,
    0x38,0x39,0x3A,0x3B,0x3C,0x3D,0x3E,0x3F,
    0x40,0x41,0x42,0x43,0x44,0x45,0x46,0x47,
    0x48,0x49,0x4A,0x4B,0x4C,0x4D,0x4E,0x4F,
    0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57,
    0x58,0x59,0x5A,0x5B,0x5C,0x5D,0x5E,0x5F,
    0x60,0x61,0x62,0x63,0x64,0x65,0x66,0x67,
    0x68,0x69,0x6A,0x6B,0x6C,0x6D,0x6E,0x6F,
    0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,
    0x78,0x79,0x7A,0x7B,0x7C,0x7D,0x7E,0x7F,
    0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,
    0x88,0x89,0x8A,0x8B,0x8C,0x8D,0x8E,0x8F,
    0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x97,
    0x98,0x99,0x9A,0x9B,0x9C,0x9D,0x9E,0x9F,
    0xA0,0xA1,0xA2,0xA3,0xA4,0xA5,0xA6,0xA7,
    0xA8,0xA9,0xAA,0xAB,0xAC,0xAD,0xAE,0xAF,
    0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,
    0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF,
    0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,
    0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF,
    0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,
    0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF,
    0xE0,0xE1,0xE2,0xE3,0xE4,0xE5,0xE6,0xE7,
    0xE8,0xE9,0xEA,0xEB,0xEC,0xED,0xEE,0xEF,
    0xF0,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,
    0xF8,0xF9,0xFA,0xFB,0xFC,0xFD,0xFE,0xFF};
  unsigned char mtf[256];
  memcpy(mtf,xmtf,sizeof(xmtf));
  unsigned int freq[FREQMAX];
  memset(freq,0,sizeof(freq));
  int fadd = 4;
  // Decode
  int mtfno = 3;
  int markerpos = -1;
  for (i=0; i<size; i++)
    {
      int ctxid = CTXIDS-1;
      if (ctxid>mtfno) ctxid=mtfno;
      BitContext *cx = ctx;
      if (zp.decoder(cx[ctxid]))
        { mtfno=0; data[i]=mtf[mtfno]; goto rotate; }
      cx+=CTXIDS;
      if (zp.decoder(cx[ctxid]))
        { mtfno=1; data[i]=mtf[mtfno]; goto rotate; } 
      cx+=CTXIDS;
      if (zp.decoder(cx[0]))
        { mtfno=2+decode_binary(zp,cx+1,1); data[i]=mtf[mtfno]; goto rotate; } 
      cx+=1+1;
      if (zp.decoder(cx[0]))
        { mtfno=4+decode_binary(zp,cx+1,2); data[i]=mtf[mtfno]; goto rotate; } 
      cx+=1+3;
      if (zp.decoder(cx[0]))
        { mtfno=8+decode_binary(zp,cx+1,3); data[i]=mtf[mtfno]; goto rotate; } 
      cx+=1+7;
      if (zp.decoder(cx[0]))
        { mtfno=16+decode_binary(zp,cx+1,4); data[i]=mtf[mtfno]; goto rotate; } 
      cx+=1+15;
      if (zp.decoder(cx[0]))
        { mtfno=32+decode_binary(zp,cx+1,5); data[i]=mtf[mtfno]; goto rotate; } 
      cx+=1+31;
      if (zp.decoder(cx[0]))
        { mtfno=64+decode_binary(zp,cx+1,6); data[i]=mtf[mtfno]; goto rotate; } 
      cx+=1+63;
      if (zp.decoder(cx[0]))
        { mtfno=128+decode_binary(zp,cx+1,7); data[i]=mtf[mtfno]; goto rotate; } 
      mtfno=256;
      data[i]=0;
      markerpos=i;
      continue;
      // Rotate mtf according to empirical frequencies (new!)
    rotate:
      // Adjust frequencies for overflow
      int k;
      fadd = fadd + (fadd>>fshift);
      if (fadd > 0x10000000) 
        {
          fadd    >>= 24;
          freq[0] >>= 24;
          freq[1] >>= 24;
          freq[2] >>= 24;
          freq[3] >>= 24;
          for (k=4; k<FREQMAX; k++)
            freq[k] = freq[k]>>24;
        }
      // Relocate new char according to new freq
      unsigned int fc = fadd;
      if (mtfno < FREQMAX)
        fc += freq[mtfno];
      for (k=mtfno; k>=FREQMAX; k--) 
        mtf[k] = mtf[k-1];
      for (; k>0 && fc>=freq[k-1]; k--)
        {
          mtf[k] = mtf[k-1];
          freq[k] = freq[k-1];
        }
      mtf[k] = data[i];
      freq[k] = fc;
    }
  

  /////////////////////////////////
  ////////// Reconstruct the string
  
  if (markerpos<1 || markerpos>=size)
    G_THROW( ERR_MSG("ByteStream.corrupt") );
  // Allocate pointers
  unsigned int *posn;
  GPBuffer<unsigned int> gposn(posn,blocksize);
  memset(posn, 0, sizeof(unsigned int)*size);
  // Prepare count buffer
  int count[256];
  for (i=0; i<256; i++)
    count[i] = 0;
  // Fill count buffer
  for (i=0; i<markerpos; i++) 
    {
      unsigned char c = data[i];
      posn[i] = (c<<24) | (count[c] & 0xffffff);
      count[c] += 1;
    }
  for (i=markerpos+1; i<size; i++)
    {
      unsigned char c = data[i];
      posn[i] = (c<<24) | (count[c] & 0xffffff);
      count[c] += 1;
    }
  // Compute sorted char positions
  int last = 1;
  for (i=0; i<256; i++)
    {
      int tmp = count[i];
      count[i] = last;
      last += tmp;
    }
  // Undo the sort transform
  i = 0;
  last = size-1;
  while (last>0)
    {
      unsigned int n = posn[i];
      unsigned char c = (posn[i]>>24);
      data[--last] = c;
      i = count[c] + (n & 0xffffff);
    }
  // Free and check
  if (i != markerpos)
    G_THROW( ERR_MSG("ByteStream.corrupt") );
  return size;
}



// ========================================
// -- ByteStream interface



long 
BSByteStream::tell() const
{
  return offset;
}

size_t 
BSByteStream::Decode::read(void *buffer, size_t sz)
{
  if (eof)
    return 0;
  // Loop
  int copied = 0;
  while (sz>0 && !eof)
    {
      // Decode if needed
      if (!size)
        {
          bptr = 0;
          if (! decode()) 
          {
            size = 1 ;
            eof = true;
          }
          size -= 1;
        }
      // Compute remaining
      int bytes = size;
      if (bytes > (int)sz)
        bytes = sz;
      // Transfer
      if (buffer && bytes)
        {
          memcpy(buffer, data+bptr, bytes);
          buffer = (void*)((char*)buffer + bytes);
        }
      size -= bytes;
      bptr += bytes;
      sz -= bytes;
      copied += bytes;
      offset += bytes;
    }
  // Return copied bytes
  return copied;
}


#ifdef HAVE_NAMESPACES
}
# ifndef NOT_USING_DJVU_NAMESPACE
using namespace DJVU;
# endif
#endif
