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

#include "MMRDecoder.h"
#include "JB2Image.h"
#include "ByteStream.h"
#include "GBitmap.h"


#ifdef HAVE_NAMESPACES
namespace DJVU {
# ifdef NOT_DEFINED // Just to fool emacs c++ mode
}
#endif
#endif


// ----------------------------------------
// MMR CODEBOOKS

static const char invalid_mmr_data[]= ERR_MSG("MMRDecoder.bad_data");

struct VLCode 
{
  unsigned short code;
  short codelen;
  short value;
};

enum MMRMode
{ 
  P=0, H=1, V0=2, VR1=3, VR2=4, VR3=5, VL1=6, VL2=7, VL3=8 
};

static const VLCode mrcodes[] =
{   // Codes on 7 bits
  // 7 bit codes
  { 0x08,   4,    P }, // 0001
  { 0x10,   3,    H }, // 001
  { 0x40,   1,   V0 }, // 1
  { 0x30,   3,  VR1 }, // 011
  { 0x06,   6,  VR2 }, // 000011
  { 0x03,   7,  VR3 }, // 0000011
  { 0x20,   3,  VL1 }, // 010
  { 0x04,   6,  VL2 }, // 000010
  { 0x02,   7,  VL3 }, // 0000010
  { 0x00,   0,   -1 }  // Illegal entry
};


static const VLCode wcodes[] = {    
  // 13 bit codes
  { 0x06a0,  8,    0 }, // 00110101
  { 0x0380,  6,    1 }, // 000111
  { 0x0e00,  4,    2 }, // 0111
  { 0x1000,  4,    3 }, // 1000
  { 0x1600,  4,    4 }, // 1011
  { 0x1800,  4,    5 }, // 1100
  { 0x1c00,  4,    6 }, // 1110
  { 0x1e00,  4,    7 }, // 1111
  { 0x1300,  5,    8 }, // 10011
  { 0x1400,  5,    9 }, // 10100
  { 0x0700,  5,   10 }, // 00111
  { 0x0800,  5,   11 }, // 01000
  { 0x0400,  6,   12 }, // 001000
  { 0x0180,  6,   13 }, // 000011
  { 0x1a00,  6,   14 }, // 110100
  { 0x1a80,  6,   15 }, // 110101
  { 0x1500,  6,   16 }, // 101010
  { 0x1580,  6,   17 }, // 101011
  { 0x09c0,  7,   18 }, // 0100111
  { 0x0300,  7,   19 }, // 0001100
  { 0x0200,  7,   20 }, // 0001000
  { 0x05c0,  7,   21 }, // 0010111
  { 0x00c0,  7,   22 }, // 0000011
  { 0x0100,  7,   23 }, // 0000100
  { 0x0a00,  7,   24 }, // 0101000
  { 0x0ac0,  7,   25 }, // 0101011
  { 0x04c0,  7,   26 }, // 0010011
  { 0x0900,  7,   27 }, // 0100100
  { 0x0600,  7,   28 }, // 0011000
  { 0x0040,  8,   29 }, // 00000010
  { 0x0060,  8,   30 }, // 00000011
  { 0x0340,  8,   31 }, // 00011010
  { 0x0360,  8,   32 }, // 00011011
  { 0x0240,  8,   33 }, // 00010010
  { 0x0260,  8,   34 }, // 00010011
  { 0x0280,  8,   35 }, // 00010100
  { 0x02a0,  8,   36 }, // 00010101
  { 0x02c0,  8,   37 }, // 00010110
  { 0x02e0,  8,   38 }, // 00010111
  { 0x0500,  8,   39 }, // 00101000
  { 0x0520,  8,   40 }, // 00101001
  { 0x0540,  8,   41 }, // 00101010
  { 0x0560,  8,   42 }, // 00101011
  { 0x0580,  8,   43 }, // 00101100
  { 0x05a0,  8,   44 }, // 00101101
  { 0x0080,  8,   45 }, // 00000100
  { 0x00a0,  8,   46 }, // 00000101
  { 0x0140,  8,   47 }, // 00001010
  { 0x0160,  8,   48 }, // 00001011
  { 0x0a40,  8,   49 }, // 01010010
  { 0x0a60,  8,   50 }, // 01010011
  { 0x0a80,  8,   51 }, // 01010100
  { 0x0aa0,  8,   52 }, // 01010101
  { 0x0480,  8,   53 }, // 00100100
  { 0x04a0,  8,   54 }, // 00100101
  { 0x0b00,  8,   55 }, // 01011000
  { 0x0b20,  8,   56 }, // 01011001
  { 0x0b40,  8,   57 }, // 01011010
  { 0x0b60,  8,   58 }, // 01011011
  { 0x0940,  8,   59 }, // 01001010
  { 0x0960,  8,   60 }, // 01001011
  { 0x0640,  8,   61 }, // 00110010
  { 0x0660,  8,   62 }, // 00110011
  { 0x0680,  8,   63 }, // 00110100
  { 0x1b00,  5,   64 }, // 11011
  { 0x1200,  5,  128 }, // 10010
  { 0x0b80,  6,  192 }, // 010111
  { 0x0dc0,  7,  256 }, // 0110111
  { 0x06c0,  8,  320 }, // 00110110
  { 0x06e0,  8,  384 }, // 00110111
  { 0x0c80,  8,  448 }, // 01100100
  { 0x0ca0,  8,  512 }, // 01100101
  { 0x0d00,  8,  576 }, // 01101000
  { 0x0ce0,  8,  640 }, // 01100111
  { 0x0cc0,  9,  704 }, // 011001100
  { 0x0cd0,  9,  768 }, // 011001101
  { 0x0d20,  9,  832 }, // 011010010
  { 0x0d30,  9,  896 }, // 011010011
  { 0x0d40,  9,  960 }, // 011010100
  { 0x0d50,  9, 1024 }, // 011010101
  { 0x0d60,  9, 1088 }, // 011010110
  { 0x0d70,  9, 1152 }, // 011010111
  { 0x0d80,  9, 1216 }, // 011011000
  { 0x0d90,  9, 1280 }, // 011011001
  { 0x0da0,  9, 1344 }, // 011011010
  { 0x0db0,  9, 1408 }, // 011011011
  { 0x0980,  9, 1472 }, // 010011000
  { 0x0990,  9, 1536 }, // 010011001
  { 0x09a0,  9, 1600 }, // 010011010
  { 0x0c00,  6, 1664 }, // 011000  (what did they think?)
  { 0x09b0,  9, 1728 }, // 010011011
  { 0x0020, 11, 1792 }, // 00000001000
  { 0x0030, 11, 1856 }, // 00000001100
  { 0x0034, 11, 1920 }, // 00000001101
  { 0x0024, 12, 1984 }, // 000000010010
  { 0x0026, 12, 2048 }, // 000000010011
  { 0x0028, 12, 2112 }, // 000000010100
  { 0x002a, 12, 2176 }, // 000000010101
  { 0x002c, 12, 2240 }, // 000000010110
  { 0x002e, 12, 2304 }, // 000000010111
  { 0x0038, 12, 2368 }, // 000000011100
  { 0x003a, 12, 2432 }, // 000000011101
  { 0x003c, 12, 2496 }, // 000000011110
  { 0x003e, 12, 2560 }, // 000000011111
  { 0x0000,  0,   -1 }  // Illegal entry
};


static const VLCode bcodes[] = {
  // 13 bit codes
  { 0x01b8, 10,    0 }, // 0000110111
  { 0x0800,  3,    1 }, // 010
  { 0x1800,  2,    2 }, // 11
  { 0x1000,  2,    3 }, // 10
  { 0x0c00,  3,    4 }, // 011
  { 0x0600,  4,    5 }, // 0011
  { 0x0400,  4,    6 }, // 0010
  { 0x0300,  5,    7 }, // 00011
  { 0x0280,  6,    8 }, // 000101
  { 0x0200,  6,    9 }, // 000100
  { 0x0100,  7,   10 }, // 0000100
  { 0x0140,  7,   11 }, // 0000101
  { 0x01c0,  7,   12 }, // 0000111
  { 0x0080,  8,   13 }, // 00000100
  { 0x00e0,  8,   14 }, // 00000111
  { 0x0180,  9,   15 }, // 000011000
  { 0x00b8, 10,   16 }, // 0000010111
  { 0x00c0, 10,   17 }, // 0000011000
  { 0x0040, 10,   18 }, // 0000001000
  { 0x019c, 11,   19 }, // 00001100111
  { 0x01a0, 11,   20 }, // 00001101000
  { 0x01b0, 11,   21 }, // 00001101100
  { 0x00dc, 11,   22 }, // 00000110111
  { 0x00a0, 11,   23 }, // 00000101000
  { 0x005c, 11,   24 }, // 00000010111
  { 0x0060, 11,   25 }, // 00000011000
  { 0x0194, 12,   26 }, // 000011001010
  { 0x0196, 12,   27 }, // 000011001011
  { 0x0198, 12,   28 }, // 000011001100
  { 0x019a, 12,   29 }, // 000011001101
  { 0x00d0, 12,   30 }, // 000001101000
  { 0x00d2, 12,   31 }, // 000001101001
  { 0x00d4, 12,   32 }, // 000001101010
  { 0x00d6, 12,   33 }, // 000001101011
  { 0x01a4, 12,   34 }, // 000011010010
  { 0x01a6, 12,   35 }, // 000011010011
  { 0x01a8, 12,   36 }, // 000011010100
  { 0x01aa, 12,   37 }, // 000011010101
  { 0x01ac, 12,   38 }, // 000011010110
  { 0x01ae, 12,   39 }, // 000011010111
  { 0x00d8, 12,   40 }, // 000001101100
  { 0x00da, 12,   41 }, // 000001101101
  { 0x01b4, 12,   42 }, // 000011011010
  { 0x01b6, 12,   43 }, // 000011011011
  { 0x00a8, 12,   44 }, // 000001010100
  { 0x00aa, 12,   45 }, // 000001010101
  { 0x00ac, 12,   46 }, // 000001010110
  { 0x00ae, 12,   47 }, // 000001010111
  { 0x00c8, 12,   48 }, // 000001100100
  { 0x00ca, 12,   49 }, // 000001100101
  { 0x00a4, 12,   50 }, // 000001010010
  { 0x00a6, 12,   51 }, // 000001010011
  { 0x0048, 12,   52 }, // 000000100100
  { 0x006e, 12,   53 }, // 000000110111
  { 0x0070, 12,   54 }, // 000000111000
  { 0x004e, 12,   55 }, // 000000100111
  { 0x0050, 12,   56 }, // 000000101000
  { 0x00b0, 12,   57 }, // 000001011000
  { 0x00b2, 12,   58 }, // 000001011001
  { 0x0056, 12,   59 }, // 000000101011
  { 0x0058, 12,   60 }, // 000000101100
  { 0x00b4, 12,   61 }, // 000001011010
  { 0x00cc, 12,   62 }, // 000001100110
  { 0x00ce, 12,   63 }, // 000001100111
  { 0x0078, 10,   64 }, // 0000001111
  { 0x0190, 12,  128 }, // 000011001000
  { 0x0192, 12,  192 }, // 000011001001
  { 0x00b6, 12,  256 }, // 000001011011
  { 0x0066, 12,  320 }, // 000000110011
  { 0x0068, 12,  384 }, // 000000110100
  { 0x006a, 12,  448 }, // 000000110101
  { 0x006c, 13,  512 }, // 0000001101100
  { 0x006d, 13,  576 }, // 0000001101101
  { 0x004a, 13,  640 }, // 0000001001010
  { 0x004b, 13,  704 }, // 0000001001011
  { 0x004c, 13,  768 }, // 0000001001100
  { 0x004d, 13,  832 }, // 0000001001101
  { 0x0072, 13,  896 }, // 0000001110010
  { 0x0073, 13,  960 }, // 0000001110011
  { 0x0074, 13, 1024 }, // 0000001110100
  { 0x0075, 13, 1088 }, // 0000001110101
  { 0x0076, 13, 1152 }, // 0000001110110
  { 0x0077, 13, 1216 }, // 0000001110111
  { 0x0052, 13, 1280 }, // 0000001010010
  { 0x0053, 13, 1344 }, // 0000001010011
  { 0x0054, 13, 1408 }, // 0000001010100
  { 0x0055, 13, 1472 }, // 0000001010101
  { 0x005a, 13, 1536 }, // 0000001011010
  { 0x005b, 13, 1600 }, // 0000001011011
  { 0x0064, 13, 1664 }, // 0000001100100
  { 0x0065, 13, 1728 }, // 0000001100101
  { 0x0020, 11, 1792 }, // 00000001000
  { 0x0030, 11, 1856 }, // 00000001100
  { 0x0034, 11, 1920 }, // 00000001101
  { 0x0024, 12, 1984 }, // 000000010010
  { 0x0026, 12, 2048 }, // 000000010011
  { 0x0028, 12, 2112 }, // 000000010100
  { 0x002a, 12, 2176 }, // 000000010101
  { 0x002c, 12, 2240 }, // 000000010110
  { 0x002e, 12, 2304 }, // 000000010111
  { 0x0038, 12, 2368 }, // 000000011100
  { 0x003a, 12, 2432 }, // 000000011101
  { 0x003c, 12, 2496 }, // 000000011110
  { 0x003e, 12, 2560 }, // 000000011111
  { 0x0000,  0,   -1 }  // Illegal entry
};




// ----------------------------------------
// SOURCE OF BITS

#define VLSBUFSIZE    64

class MMRDecoder::VLSource : public GPEnabled
{
protected:
  VLSource(GP<ByteStream> &inp);
  void init(const bool striped);
public:
  // Initializes a bit source on a bytestream
  static GP<VLSource> create(GP<ByteStream> &inp, const bool striped);

  // Synchronize on the next stripe
  void nextstripe(void);
  // Returns a 32 bits integer with at least the 
  // next sixteen code bits in the high order bits.
  inline unsigned int peek(void);
  // Ensures that next #peek()# contains at least
  // the next 24 code bits.
  void preload(void);
  // Consumes #n# bits.
  void shift(const int n);
private:
  GP<ByteStream> ginp;
  ByteStream &inp;
  unsigned char buffer[ VLSBUFSIZE ];
  unsigned int codeword;
  int lowbits;
  int bufpos;
  int bufmax;
  int readmax;
};

MMRDecoder::VLSource::VLSource(GP<ByteStream> &xinp)
: ginp(xinp), inp(*ginp), codeword(0), 
  lowbits(0), bufpos(0), bufmax(0),
  readmax(-1)
{}

void
MMRDecoder::VLSource::init(const bool striped)
{
  if (striped)
    readmax = inp.read32();
  lowbits = 32;
  preload();
}

GP<MMRDecoder::VLSource>
MMRDecoder::VLSource::create(GP<ByteStream> &inp, const bool striped)
{
  VLSource *src=new VLSource(inp);
  GP<VLSource> retval=src;
  src->init(striped);
  return retval;
}

void 
MMRDecoder::VLSource::shift(const int n)
{ 
  codeword<<=n;
  lowbits+=n;
  if (lowbits>=16)
    preload();
}

inline unsigned int
MMRDecoder::VLSource::peek(void)
{
  return codeword;
}


void
MMRDecoder::VLSource::nextstripe(void)
{
  while (readmax>0)
    {
      int size = sizeof(buffer);
      if (readmax < size) 
        size = readmax;
      inp.readall(buffer, size);
      readmax -= size;
    }
  bufpos = bufmax = 0;
  memset(buffer,0,sizeof(buffer));
  readmax = inp.read32();
  codeword = 0; 
  lowbits = 32;
  preload();
}

void
MMRDecoder::VLSource::preload(void)
{
  while (lowbits>=8) 
    {
      if (bufpos >= bufmax) 
	{
          // Refill buffer
	  bufpos = bufmax = 0;
          int size = sizeof(buffer);
          if (readmax>=0 && readmax<size) 
            size = readmax;
          if (size>0)
            bufmax = inp.read((void*)buffer, size);
          readmax -= bufmax;
	  if (bufmax <= 0)
            return;
	}
      lowbits -= 8;
      codeword |= buffer[bufpos++] << lowbits;
    }
}



// ----------------------------------------
// VARIABLE LENGTH CODES



class MMRDecoder::VLTable : public GPEnabled
{
protected:
  VLTable(const VLCode *codes);
  void init(const int nbits);
public:
  // Construct a VLTable given a codebook with #nbits# long codes.
  static GP<VLTable> create(VLCode const * const codes, const int nbits);

  // Reads one symbol from a VLSource
  int decode(MMRDecoder::VLSource *src);

  const VLCode *code;
  int codewordshift;
  unsigned char *index;
  GPBuffer<unsigned char> gindex;
};

GP<MMRDecoder::VLTable>
MMRDecoder::VLTable::create(VLCode const * const codes, const int nbits)
{
  VLTable *table=new VLTable(codes);
  GP<VLTable> retval=table;
  table->init(nbits);
  return retval;
}

inline int
MMRDecoder::VLTable::decode(MMRDecoder::VLSource *src)    
{ 
  const VLCode &c = code[ index[ src->peek() >> codewordshift ] ];
  src->shift(c.codelen); 
  return c.value; 
}

MMRDecoder::VLTable::VLTable(const VLCode *codes)
: code(codes), codewordshift(0), gindex(index,0)
{}

void
MMRDecoder::VLTable::init(const int nbits)
{
  // count entries
  int ncodes = 0;
  while (code[ncodes].codelen)
    ncodes++;
  // check arguments
  if (nbits<=1 || nbits>16)
    G_THROW(invalid_mmr_data);
  if (ncodes>=256)
    G_THROW(invalid_mmr_data);
  codewordshift = 32 - nbits;
  // allocate table
  int size = (1<<nbits);
  gindex.resize(size);
  gindex.set(ncodes);
  // process codes
  for (int i=0; i<ncodes; i++) {
    const int c = code[i].code;
    const int b = code[i].codelen;
    if(b<=0 || b>nbits)
    {
      G_THROW(invalid_mmr_data);
    }
    // fill table entries whose index high bits are code.
    int n = c + (1<<(nbits-b));
    while ( --n >= c ) {
      if(index[n] != ncodes)
       G_THROW( ERR_MSG("MMRDecoder.bad_codebook") );
      index[n] = i;
    }
  }
}

// ----------------------------------------
// MMR DECODER



MMRDecoder::~MMRDecoder() {}

MMRDecoder::MMRDecoder( const int xwidth, const int xheight )
: width(xwidth), height(xheight), lineno(0), 
  striplineno(0), rowsperstrip(0), gline(line,width+8),
  glineruns(lineruns,width+4), gprevruns(prevruns,width+4)
{
  gline.clear();
  glineruns.clear();
  gprevruns.clear();
  lineruns[0] = width;
  prevruns[0] = width;
}

void
MMRDecoder::init(GP<ByteStream> gbs, const bool striped)
{
  rowsperstrip = (striped ? gbs->read16() : height);
  src = VLSource::create(gbs, striped);
  mrtable = VLTable::create(mrcodes, 7);
  btable = VLTable::create(bcodes, 13);
  wtable = VLTable::create(wcodes, 13);
}

GP<MMRDecoder> 
MMRDecoder::create( GP<ByteStream> gbs, const int width,
  const int height, const bool striped )
{
  MMRDecoder *mmr=new MMRDecoder(width,height);
  GP<MMRDecoder> retval=mmr;
  mmr->init(gbs,striped);
  return retval;
}

const unsigned short *
MMRDecoder::scanruns(const unsigned short **endptr)
{
  // Check if all lines have been returned
  if (lineno >= height)
    return 0;
  // Check end of stripe
  if ( striplineno == rowsperstrip )
    {
      striplineno=0;
      lineruns[0] = prevruns[0] = width;
      src->nextstripe();
    }
  // Swap run buffers
  unsigned short *pr = lineruns;
  unsigned short *xr = prevruns;
  prevruns = pr;
  lineruns = xr;
  // Loop until scanline is complete
  bool a0color = false;
  int a0,rle,b1;
  for(a0=0,rle=0,b1=*pr++;a0 < width;)
    {
      // Process MMR codes
      const int c=mrtable->decode(src);
      switch ( c )
      {
          /* Pass Mode */
        case P: 
          { 
            b1 += *pr++;
            rle += b1 - a0;
            a0 = b1;
            b1 += *pr++;
            break;
          }
          /* Horizontal Mode */
        case H: 
          { 
            // First run
            VLTable &table1 = *(a0color ? btable : wtable);
            int inc;
            do { inc=table1.decode(src); a0+=inc; rle+=inc; } while (inc>=64);
            *xr = rle; xr++; rle = 0;
            // Second run
            VLTable &table2 = *(!a0color ? btable : wtable);
            do { inc=table2.decode(src); a0+=inc; rle+=inc; } while (inc>=64);
            *xr = rle; xr++; rle = 0;
            break;
          }
          /* Vertical Modes */
        case V0:
        case VR3:
        case VR2:
        case VR1:
        case VL3:
        case VL2:
        case VL1:
        {
          int inc=b1;
          switch ( c )
          {
          case V0:
            inc = b1;
            b1 += *pr++;
            break;
          case VR3:
            inc = b1+3;
            b1 += *pr++;
            break;
          case VR2:
            inc = b1+2;
            b1 += *pr++;
            break;
          case VR1:
            inc = b1+1;
            b1 += *pr++;
            break;
          case VL3:
            inc = b1-3;
            b1 -= *--pr;
            break;
          case VL2:
            inc = b1-2;
            b1 -= *--pr;
            break;
          case VL1:
            inc = b1-1;
            b1 -= *--pr;
            break;
          }
          *xr = inc+rle-a0;
          xr++;
          a0 = inc;
          rle = 0;
          a0color = !a0color;
          break;
        }
          /* Uncommon modes */
        default: 
          {
            src->preload();
            unsigned int m = src->peek();
            // -- Could be EOFB ``000000000001000000000001''
            //    TIFF6 says that all remaining lines are white
            if ((m & 0xffffff00) == 0x00100100)
              {
                lineno = height;
                return 0;
              }
            // -- Could be UNCOMPRESSED ``0000001111''
            //    TIFF6 says people should not do this.
            //    RFC1314 says people should do this.
            else if ((m & 0xffc00000) == 0x03c00000)
              {
#ifdef MMRDECODER_REFUSES_UNCOMPRESSED
                G_THROW( ERR_MSG("MMRDecoder.cant_process") );
#else
                // ---THE-FOLLOWING-CODE-IS-POORLY-TESTED---
                src->shift(10);
                while ((m = (src->peek() & 0xfc000000)))
                  {
                    if (m == 0x04000000)       // 000001
                      {
                        src->shift(6);
                        if (a0color)
                        {
                          *xr = rle;
                          xr++;
                          rle = 0;
                          a0color = !a0color;
                        }
                        rle += 5;
                        a0 += 5;
                      }
                    else                       // 000010 to 111111 
                      { 
                        src->shift(1);
                        if (a0color == !(m & 0x80000000))
                        {
                          *xr = rle;
                          xr++;
                          rle = 0;
                          a0color = !a0color;
                        }
                        rle++;
                        a0++;
                      }
                    if (a0 > width)
                      G_THROW(invalid_mmr_data);
                  }
                // Analyze uncompressed termination code.
                m = src->peek() & 0xff000000;  
                src->shift(8);
                if ( (m & 0xfe000000) != 0x02000000 )
                  G_THROW(invalid_mmr_data);
                if (rle)
                {
                  *xr = rle;
                  xr++;
                  rle = 0;
                  a0color = !a0color;
                }                  
                if (a0color == !(m & 0x01000000))
                {
                  *xr = rle;
                  xr++;
                  rle = 0;
                  a0color = !a0color;
                }
                // Cross fingers and proceed ...
                break;
#endif
              }
            // -- Unknown MMR code.
            G_THROW(invalid_mmr_data);
          }
      }
      // Next reference run
      for(;b1<=a0 && b1<width;pr+=2)
      {
        b1 += pr[0]+pr[1];
      }
    }
  // Final P must be followed by V0 (they say!)
  if (rle > 0)
  {
    if (mrtable->decode(src) != V0)
    {
      G_THROW(invalid_mmr_data);
    }
  }
  if (rle > 0)
  {
    *xr = rle;
    xr++;
  }
  // At this point we should have A0 equal to WIDTH
  // But there are buggy files around (Kofax!)
  // and we are not the CCITT police.
  if (a0 > width) 
    {
      while (a0 > width && xr > lineruns)
        a0 -= *--xr;
      if (a0 < width)
      {
        *xr = width-a0;
        xr++;
      }
    }
  /* Increment and return */
  if (endptr) 
    *endptr = xr;
  xr[0] = 0;
  xr[1] = 0;
  lineno ++;
  striplineno ++;
  return lineruns;
}



const unsigned char *
MMRDecoder::scanrle(const bool invert, const unsigned char **endptr)
{
  // Obtain run lengths
  const unsigned short *xr = scanruns();
  if (!xr) return 0;
  unsigned char *p=line;
  // Process inversion
  if (invert)
    {
      if (! *xr) 
      {
        xr++;
      }else
      {
        *p = 0; p++;
      }
    }
  // Encode lenghts using the RLE format
  for(int a0=0;a0 < width;)
  {
    int count = *xr++;
    a0 += count;
    GBitmap::append_run(p, count);
  }
  if (endptr)
    *endptr = p;
  p[0] = 0;
  p[1] = 0;
  return line;
}


#if 0
const unsigned char *
MMRDecoder::scanline(void)
{
  // Obtain run lengths
  const unsigned short *xr = scanruns();
  if (!xr) return 0;
  // Allocate data buffer if needed
  unsigned char *p = line;
  // Decode run lengths
  int a0 = 0;
  int a0color = 0;
  while (a0 < width)
    {
      int a1 = a0 + *xr++;
      while (a0<a1 && a0<width)
        line[a0++] = a0color;
      a0color = !a0color;
    }
  return line;
}
#endif




// ----------------------------------------
// MAIN DECODING ROUTINE

bool
MMRDecoder::decode_header(
  ByteStream &inp, int &width, int &height, int &invert)
{
  unsigned long int magic = inp.read32();
  if((magic&0xfffffffc) != 0x4d4d5200)
    G_THROW( ERR_MSG("MMRDecoder.unrecog_header") ); 
  invert = ((magic & 0x1) ? 1 : 0);
  const bool strip =  ((magic & 0x2) ? 1 : 0);
  width = inp.read16();
  height = inp.read16();
  if (width<=0 || height<=0)
    G_THROW( ERR_MSG("MMRDecoder.bad_header") );
  return strip;
}

static inline int MAX(int a, int b) { return a>b ? a : b; }
static inline int MIN(int a, int b) { return a<b ? a : b; }

GP<JB2Image>
MMRDecoder::decode(GP<ByteStream> gbs)
{
  ByteStream &inp=*gbs;
  // Read header
  int width, height, invert;
  const bool striped=decode_header(inp, width, height, invert);
  // Prepare image
  GP<JB2Image> jimg = JB2Image::create();
  jimg->set_dimension(width, height);
  // Choose pertinent blocksize
  int blocksize = MIN(500,MAX(64,MAX(width/17,height/22)));
  int blocksperline = (width+blocksize-1)/blocksize;
  // Prepare decoder
  GP<MMRDecoder> gdcd=MMRDecoder::create(gbs, width, height, striped);
  MMRDecoder &dcd=*gdcd;
  // Loop on JB2 bands
  int line = height-1;
  while (line >= 0)
    {
      int bandline = MIN(blocksize-1,line);
      GPArray<GBitmap> blocks(0,blocksperline-1);
      // Loop on scanlines
      for(; bandline >= 0; bandline--,line--)
      {
        // Decode one scanline
        const unsigned short *s = dcd.scanruns();
        if (s)
        {
	  // Loop on blocks
          int x = 0;
          int b = 0;
          int firstx = 0;
          bool c = !!invert;
          while (x < width)
            {
              int xend = x + *s++;
              while (b<blocksperline)
                {
                  int lastx = MIN(firstx+blocksize,width);
                  if (c)
                    {
                      if (!blocks[b])
                        blocks[b] = GBitmap::create(bandline+1, lastx-firstx);
                      unsigned char *bptr = (*blocks[b])[bandline] - firstx;
                      int x1 = MAX(x,firstx);
                      int x2 = MIN(xend,lastx);
                      while (x1 < x2)
                        bptr[x1++] = 1;
                    }
                  if (xend < lastx)
                    break;
                  firstx = lastx;
                  b ++;
                }
              x = xend;
              c = !c; 
            }
	}
      }
      // Insert blocks into JB2Image
      for (int b=0; b<blocksperline; b++)
	{
	  JB2Shape shape;
	  shape.bits = blocks[b];
	  if (shape.bits) 
	    {
	      shape.parent = -1;
	      shape.bits->compress();
	      JB2Blit blit;
	      blit.left = b*blocksize;
	      blit.bottom = line+1;
	      blit.shapeno = jimg->add_shape(shape);
	      jimg->add_blit(blit);
	    }
	}
    }
  // Return
  return jimg;
}



#ifdef HAVE_NAMESPACES
}
# ifndef NOT_USING_DJVU_NAMESPACE
using namespace DJVU;
# endif
#endif
