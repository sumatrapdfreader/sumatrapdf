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

#include "GBitmap.h"
#include "ByteStream.h"
#include "GRect.h"
#include "GString.h"
#include "GThreads.h"
#include "GException.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

// - Author: Leon Bottou, 05/1997


#ifdef HAVE_NAMESPACES
namespace DJVU {
# ifdef NOT_DEFINED // Just to fool emacs c++ mode
}
#endif
#endif

// ----- constructor and destructor

GBitmap::~GBitmap()
{
}

void
GBitmap::destroy(void)
{
  gbytes_data.resize(0);
  bytes = 0;
  grle.resize(0);
  grlerows.resize(0);
  rlelength = 0;
}

GBitmap::GBitmap()
  : nrows(0), ncolumns(0), border(0), 
    bytes_per_row(0), grays(0), bytes(0), gbytes_data(bytes_data), 
    grle(rle), grlerows(rlerows), rlelength(0),
    monitorptr(0)
{
}

GBitmap::GBitmap(int nrows, int ncolumns, int border)
  : nrows(0), ncolumns(0), border(0), 
    bytes_per_row(0), grays(0), bytes(0), gbytes_data(bytes_data), 
    grle(rle), grlerows(rlerows), rlelength(0),
    monitorptr(0)
{
  G_TRY
  { 
    init(nrows, ncolumns, border);
  }
  G_CATCH_ALL
  {
    destroy();
    G_RETHROW;
  }
  G_ENDCATCH;
}

GBitmap::GBitmap(ByteStream &ref, int border)
  : nrows(0), ncolumns(0), border(0), 
    bytes_per_row(0), grays(0), bytes(0), gbytes_data(bytes_data),
    grle(rle), grlerows(rlerows), rlelength(0),
    monitorptr(0)
{
  G_TRY
  { 
    init(ref, border);
  }
  G_CATCH_ALL
  {
    destroy();
    G_RETHROW;
  }
  G_ENDCATCH;
}

GBitmap::GBitmap(const GBitmap &ref)
  : nrows(0), ncolumns(0), border(0), 
    bytes_per_row(0), grays(0), bytes(0), gbytes_data(bytes_data), 
    grle(rle), grlerows(rlerows), rlelength(0),
    monitorptr(0)
{
  G_TRY
  { 
    init(ref, ref.border);
  }
  G_CATCH_ALL
  {
    destroy();
    G_RETHROW;
  }
  G_ENDCATCH;
}

GBitmap::GBitmap(const GBitmap &ref, int border)
  : nrows(0), ncolumns(0), border(0), 
    bytes_per_row(0), grays(0), bytes(0), gbytes_data(bytes_data),
    grle(rle), grlerows(rlerows), rlelength(0),
    monitorptr(0)
{
  G_TRY
  { 
    init(ref, border);
  }
  G_CATCH_ALL
  {
    destroy();
    G_RETHROW;
  }
  G_ENDCATCH;
}


GBitmap::GBitmap(const GBitmap &ref, const GRect &rect, int border)
  : nrows(0), ncolumns(0), border(0), 
    bytes_per_row(0), grays(0), bytes(0), gbytes_data(bytes_data),
    grle(rle), grlerows(rlerows), rlelength(0),
    monitorptr(0)
{
  G_TRY
  { 
    init(ref, rect, border);
  }
  G_CATCH_ALL
  {
    destroy();
    G_RETHROW;
  }
  G_ENDCATCH;
}






// ----- initialization

void 
GBitmap::init(int arows, int acolumns, int aborder)
{
  size_t np = arows * (acolumns + aborder) + aborder;
  if (arows != (unsigned short) arows ||
      acolumns != (unsigned short) acolumns ||
      acolumns + aborder != (unsigned short)(acolumns + aborder) ||
      (arows > 0 && (np-aborder)/(size_t)arows!=(size_t)(acolumns+aborder)) )
    G_THROW("GBitmap: image size exceeds maximum (corrupted file?)");
  GMonitorLock lock(monitor());
  destroy();
  grays = 2;
  nrows = arows;
  ncolumns = acolumns;
  border = aborder;
  bytes_per_row = ncolumns + border;
  int npixels = nrows * bytes_per_row + border;
  gzerobuffer=zeroes(bytes_per_row + border);
  if (npixels > 0) 
    {
      gbytes_data.resize(npixels);
      gbytes_data.clear();
      bytes = bytes_data;
    }
}


void 
GBitmap::init(const GBitmap &ref, int aborder)
{
  GMonitorLock lock(monitor());
  if (this != &ref) 
    {
      GMonitorLock lock(ref.monitor());
      init(ref.nrows, ref.ncolumns, aborder);
      grays = ref.grays;
      unsigned char *row = bytes_data+border;
      for (int n=0; n<nrows; n++, row+=bytes_per_row)
      memcpy( (void*)row, (void*)ref[n],  ncolumns );
    }
  else if (aborder > border)
    {
      minborder(aborder);
    }
}


void 
GBitmap::init(const GBitmap &ref, const GRect &rect, int border)
{
  GMonitorLock lock(monitor());
  // test bitmap physical equality
  if (this == &ref)
    {
      GBitmap tmp;
      tmp.grays = grays;
      tmp.border = border;
      tmp.bytes_per_row = bytes_per_row;
      tmp.ncolumns = ncolumns;
      tmp.nrows = nrows;
      tmp.bytes = bytes;
      tmp.gbytes_data.swap(gbytes_data);
      tmp.grle.swap(grle);
      bytes = 0 ;
      init(tmp, rect, border);
    }
  else
    {
      GMonitorLock lock(ref.monitor());
      // create empty bitmap
      init(rect.height(), rect.width(), border);
      grays = ref.grays;
      // compute destination rectangle
      GRect rect2(0, 0, ref.columns(), ref.rows() );
      rect2.intersect(rect2, rect);
      rect2.translate(-rect.xmin, -rect.ymin);
      // copy bits
      if (! rect2.isempty())
        {
          for (int y=rect2.ymin; y<rect2.ymax; y++)
            {
              unsigned char *dst = (*this)[y];
              const unsigned char *src = ref[y+rect.ymin] + rect.xmin;
              for (int x=rect2.xmin; x<rect2.xmax; x++)
                dst[x] = src[x];
            }
        }
    }
}


void 
GBitmap::init(ByteStream &ref, int aborder)
{
  GMonitorLock lock(monitor());
  // Get magic number
  char magic[2];
  magic[0] = magic[1] = 0;
  ref.readall((void*)magic, sizeof(magic));
  char lookahead = '\n';
  int acolumns = read_integer(lookahead, ref);
  int arows = read_integer(lookahead, ref);
  int maxval = 1;
  init(arows, acolumns, aborder);
  // go reading file
  if (magic[0]=='P')
    {
      switch(magic[1])
        {
        case '1':
          grays = 2;
          read_pbm_text(ref); 
          return;
        case '2':
          maxval = read_integer(lookahead, ref);
          if (maxval > 65535)
            G_THROW("Cannot read PGM with depth greater than 16 bits.");
          grays = (maxval>255 ? 256 : maxval+1);
          read_pgm_text(ref, maxval); 
          return;
        case '4':
          grays = 2;
          read_pbm_raw(ref); 
          return;
        case '5':
          maxval = read_integer(lookahead, ref);
          if (maxval > 65535)
            G_THROW("Cannot read PGM with depth greater than 16 bits.");
          grays = (maxval>255 ? 256 : maxval+1);
          read_pgm_raw(ref, maxval); 
          return;
        }
    }
  else if (magic[0]=='R')
    {
      switch(magic[1])
        {
        case '4':
          grays = 2;
          read_rle_raw(ref); 
          return;
        }
    }
  G_THROW( ERR_MSG("GBitmap.bad_format") );
}

void
GBitmap::donate_data(unsigned char *data, int w, int h)
{
  destroy();
  grays = 2;
  nrows = h;
  ncolumns = w;
  border = 0;
  bytes_per_row = w;
  gbytes_data.replace(data,w*h);
  bytes = bytes_data;
  rlelength = 0;
}

void
GBitmap::donate_rle(unsigned char *rledata, unsigned int rledatalen, int w, int h)
{
  destroy();
  grays = 2;
  nrows = h;
  ncolumns = w;
  border = 0;
  bytes_per_row = w;
//  rle = rledata;
  grle.replace(rledata,rledatalen);
  rlelength = rledatalen;
}


unsigned char *
GBitmap::take_data(size_t &offset)
{
  GMonitorLock lock(monitor());
  unsigned char *ret = bytes_data;
  if (ret) offset = (size_t)border;
  bytes_data=0;
  return ret;
}

const unsigned char *
GBitmap::get_rle(unsigned int &rle_length)
{
  if(!rle)
    compress();
  rle_length=rlelength;
  return rle; 
}

// ----- compression


void 
GBitmap::compress()
{
  if (grays > 2)
    G_THROW( ERR_MSG("GBitmap.cant_compress") );
  GMonitorLock lock(monitor());
  if (bytes)
    {
      grle.resize(0);
      grlerows.resize(0);
      rlelength = encode(rle,grle);
      if (rlelength)
        {
          gbytes_data.resize(0);
          bytes = 0;
        }
    }
}

void
GBitmap::uncompress()
{
  GMonitorLock lock(monitor());
  if (!bytes && rle)
    decode(rle);
}



unsigned int 
GBitmap::get_memory_usage() const
{
  unsigned long usage = sizeof(GBitmap);
  if (bytes)
    usage += nrows * bytes_per_row + border;
  if (rle)
    usage += rlelength;
  return usage;
}


void 
GBitmap::minborder(int minimum)
{
  if (border < minimum)
    {
      GMonitorLock lock(monitor());
      if (border < minimum)
        {
          if (bytes)
            {
              GBitmap tmp(*this, minimum);
              bytes_per_row = tmp.bytes_per_row;
              tmp.gbytes_data.swap(gbytes_data);
              bytes = bytes_data;
              tmp.bytes = 0;
            }
          border = minimum;
          gzerobuffer=zeroes(border + ncolumns + border);
        }
    }
}


#define NMONITORS 8
static GMonitor monitors[NMONITORS];

void
GBitmap::share()
{
  if (!monitorptr)
    {
      size_t x = (size_t)this;
      monitorptr = &monitors[(x^(x>>5)) % NMONITORS];
    }
}


// ----- gray levels

void
GBitmap::set_grays(int ngrays)
{
  if (ngrays<2 || ngrays>256)
    G_THROW( ERR_MSG("GBitmap.bad_levels") );
  // set gray levels
  GMonitorLock lock(monitor());
  grays = ngrays;
  if (ngrays>2 && !bytes)
    uncompress();
}

void 
GBitmap::change_grays(int ngrays)
{
  GMonitorLock lock(monitor());
  // set number of grays
  int ng = ngrays - 1;
  int og = grays - 1;
  set_grays(ngrays);
  // setup conversion table
  unsigned char conv[256];
  for (int i=0; i<256; i++)
    {
      if (i > og)
        conv[i] = ng;
      else
        conv[i] = (i*ng+og/2)/og;
    }
  // perform conversion
  for (int row=0; row<nrows; row++)
    {
      unsigned char *p = (*this)[row];
      for (int n=0; n<ncolumns; n++)
        p[n] = conv[ p[n] ];
    }
}

void 
GBitmap::binarize_grays(int threshold)
{
  GMonitorLock lock(monitor());
  if (bytes)
    for (int row=0; row<nrows; row++)
      {
        unsigned char *p = (*this)[row];
        for(unsigned char const * const pend=p+ncolumns;p<pend;++p)
        {
          *p = (*p>threshold) ? 1 : 0;
        }
      }
  grays = 2;
}


// ----- additive blitting

#undef min
#undef max

static inline int
min(int x, int y) 
{ 
  return (x < y ? x : y);
}

static inline int
max(int x, int y) 
{ 
  return (x > y ? x : y);
}

void 
GBitmap::blit(const GBitmap *bm, int x, int y)
{
  // Check boundaries
  if ((x >= ncolumns)              || 
      (y >= nrows)                 ||
      (x + (int)bm->columns() < 0) || 
      (y + (int)bm->rows() < 0)     )
    return;

  // Perform blit
  GMonitorLock lock1(monitor());
  GMonitorLock lock2(bm->monitor());
  if (bm->bytes)
    {
      if (!bytes_data)
        uncompress();
      // Blit from bitmap
      const unsigned char *srow = bm->bytes + bm->border;
      unsigned char *drow = bytes_data + border + y*bytes_per_row + x;
      for (int sr = 0; sr < bm->nrows; sr++)
        {
          if (sr+y>=0 && sr+y<nrows) 
            {
              int sc = max(0, -x);
              int sc1 = min(bm->ncolumns, ncolumns-x);
              while (sc < sc1)
                {
                  drow[sc] += srow[sc];
                  sc += 1;
                }
            }
          srow += bm->bytes_per_row;
          drow += bytes_per_row;
        }
    }
  else if (bm->rle)
    {
      if (!bytes_data)
        uncompress();
      // Blit from rle
      const unsigned char *runs = bm->rle;
      unsigned char *drow = bytes_data + border + y*bytes_per_row + x;
      int sr = bm->nrows - 1;
      drow += sr * bytes_per_row;
      int sc = 0;
      char p = 0;
      while (sr >= 0)
        {
          const int z = read_run(runs);
          if (sc+z > bm->ncolumns)
            G_THROW( ERR_MSG("GBitmap.lost_sync") );
          int nc = sc + z;
          if (p && sr+y>=0 && sr+y<nrows) 
            {
              if (sc + x < 0) 
                sc = min(-x, nc); 
              while (sc < nc && sc + x<ncolumns)
                drow[sc++] += 1;
            }
          sc = nc;
          p = 1 - p;
          if (sc >= bm->ncolumns) 
            {
              p = 0;
              sc = 0;
              drow -= bytes_per_row;
              sr -= 1; 
            }
        }
    }
}



void 
GBitmap::blit(const GBitmap *bm, int xh, int yh, int subsample)
{
  // Use code when no subsampling is necessary
  if (subsample == 1)
    {
      blit(bm, xh, yh);
      return;
    }

  // Check boundaries
  if ((xh >= ncolumns * subsample) || 
      (yh >= nrows * subsample)    ||
      (xh + (int)bm->columns() < 0)   || 
      (yh + (int)bm->rows() < 0)     )
    return;

  // Perform subsampling blit
  GMonitorLock lock1(monitor());
  GMonitorLock lock2(bm->monitor());
  if (bm->bytes)
    {
      if (!bytes_data)
        uncompress();
      // Blit from bitmap
      int dr, dr1, zdc, zdc1;
      euclidian_ratio(yh, subsample, dr, dr1);
      euclidian_ratio(xh, subsample, zdc, zdc1);
      const unsigned char *srow = bm->bytes + bm->border;
      unsigned char *drow = bytes_data + border + dr*bytes_per_row;
      for (int sr = 0; sr < bm->nrows; sr++)
        {
          if (dr>=0 && dr<nrows) 
            {
              int dc = zdc;
              int dc1 = zdc1;
              for (int sc=0; sc < bm->ncolumns; sc++) 
                {
                  if (dc>=0 && dc<ncolumns)
                    drow[dc] += srow[sc];
                  if (++dc1 >= subsample) 
                    {
                      dc1 = 0;
                      dc += 1;
                    }
                }
            }
          // next line in source
          srow += bm->bytes_per_row;
          // next line fraction in destination
          if (++dr1 >= subsample)
            {
              dr1 = 0;
              dr += 1;
              drow += bytes_per_row;
            }
        }
    }
  else if (bm->rle)
    {
      if (!bytes_data)
        uncompress();
      // Blit from rle
      int dr, dr1, zdc, zdc1;
      euclidian_ratio(yh+bm->nrows-1, subsample, dr, dr1);
      euclidian_ratio(xh, subsample, zdc, zdc1);
      const unsigned char *runs = bm->rle;
      unsigned char *drow = bytes_data + border + dr*bytes_per_row;
      int sr = bm->nrows -1;
      int sc = 0;
      char p = 0;
      int dc = zdc;
      int dc1 = zdc1;
      while (sr >= 0)
        {
          int z = read_run(runs);
          if (sc+z > bm->ncolumns)
            G_THROW( ERR_MSG("GBitmap.lost_sync") );
          int nc = sc + z;

          if (dr>=0 && dr<nrows)
            while (z>0 && dc<ncolumns)
              {
                int zd = subsample - dc1;
                if (zd > z) 
                  zd = z;
                if (p && dc>=0) 
                  drow[dc] += zd;
                z -= zd;
                dc1 += zd;
                if (dc1 >= subsample)
                  {
                    dc1 = 0;
                    dc += 1;
                  }
              }
          // next fractional row
          sc = nc;
          p = 1 - p;
          if (sc >= bm->ncolumns) 
            {
              sc = 0;
              dc = zdc;
              dc1 = zdc1;
              p = 0;
              sr -= 1; 
              if (--dr1 < 0)
                {
                  dr1 = subsample - 1;
                  dr -= 1;
                  drow -= bytes_per_row;
                }
            }
        }
    }
}



// ------ load bitmaps


unsigned int 
GBitmap::read_integer(char &c, ByteStream &bs)
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
    G_THROW( ERR_MSG("GBitmap.not_int") );
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
GBitmap::read_pbm_text(ByteStream &bs)
{
  unsigned char *row = bytes_data + border;
  row += (nrows-1) * bytes_per_row;
  for (int n = nrows-1; n>=0; n--) 
    {
      for (int c = 0; c<ncolumns; c++) 
        {
          char bit = 0;
          bs.read(&bit,1);
          while (bit==' ' || bit=='\t' || bit=='\r' || bit=='\n')
            { 
              bit=0; 
              bs.read(&bit,1); 
            }
          if (bit=='1')
            row[c] = 1;
          else if (bit=='0')
            row[c] = 0;
          else
            G_THROW( ERR_MSG("GBitmap.bad_PBM") );
        }
      row -= bytes_per_row;
    }
}

void 
GBitmap::read_pgm_text(ByteStream &bs, int maxval)
{
  unsigned char *row = bytes_data + border;
  row += (nrows-1) * bytes_per_row;
  char lookahead = '\n';
  GTArray<unsigned char> ramp(0, maxval);
  for (int i=0; i<=maxval; i++)
    ramp[i] = (i<maxval ? ((grays-1)*(maxval-i) + maxval/2) / maxval : 0);
  for (int n = nrows-1; n>=0; n--) 
    {
      for (int c = 0; c<ncolumns; c++)
        row[c] = ramp[(int)read_integer(lookahead, bs)];
      row -= bytes_per_row;
    }
}

void 
GBitmap::read_pbm_raw(ByteStream &bs)
{
  unsigned char *row = bytes_data + border;
  row += (nrows-1) * bytes_per_row;
  for (int n = nrows-1; n>=0; n--) 
    {
      unsigned char acc = 0;
      unsigned char mask = 0;
      for (int c = 0; c<ncolumns; c++)
        {
          if (!mask) 
            {
              bs.read(&acc, 1);
              mask = (unsigned char)0x80;
            }
          if (acc & mask)
            row[c] = 1;
          else
            row[c] = 0;
          mask >>= 1;
        }
      row -= bytes_per_row;
    }
}

void 
GBitmap::read_pgm_raw(ByteStream &bs, int maxval)
{
  int maxbin = (maxval>255) ? 65536 : 256;
  GTArray<unsigned char> ramp(0, maxbin-1);
  for (int i=0; i<maxbin; i++)
    ramp[i] = (i<maxval ? ((grays-1)*(maxval-i) + maxval/2) / maxval : 0);
  unsigned char *bramp = ramp;
  unsigned char *row = bytes_data + border;
  row += (nrows-1) * bytes_per_row;
  for (int n = nrows-1; n>=0; n--) 
    {
      if (maxbin > 256)
        {
          for (int c = 0; c<ncolumns; c++)
            {
              unsigned char x[2];
              bs.read((void*)&x, 2);
              row[c] = bramp[x[0]*256+x[1]];
            }
        }
      else
        {
          for (int c = 0; c<ncolumns; c++)
            {
              unsigned char x;
              bs.read((void*)&x, 1);
              row[c] = bramp[x];
            }
        }
      row -= bytes_per_row;
    }
}

void 
GBitmap::read_rle_raw(ByteStream &bs)
{
  // interpret runs data
  unsigned char h;
  unsigned char p = 0;
  unsigned char *row = bytes_data + border;
  int n = nrows - 1;
  row += n * bytes_per_row;
  int c = 0;
  while (n >= 0)
    {
      if (bs.read(&h, 1) <= 0)
        G_THROW( ByteStream::EndOfFile );
      int x = h;
      if (x >= (int)RUNOVERFLOWVALUE)
        {
          if (bs.read(&h, 1) <= 0)
            G_THROW( ByteStream::EndOfFile );
          x = h + ((x - (int)RUNOVERFLOWVALUE) << 8);
        }
      if (c+x > ncolumns)
        G_THROW( ERR_MSG("GBitmap.lost_sync") );
      while (x-- > 0)
        row[c++] = p;
      p = 1 - p;
      if (c >= ncolumns) 
        {
          c = 0;
          p = 0;
          row -= bytes_per_row;
          n -= 1; 
        }
    }
}


// ------ save bitmaps

void 
GBitmap::save_pbm(ByteStream &bs, int raw)
{
  // check arguments
  if (grays > 2)
    G_THROW( ERR_MSG("GBitmap.cant_make_PBM") );
  GMonitorLock lock(monitor());
  // header
  {
    GUTF8String head;
    head.format("P%c\n%d %d\n", (raw ? '4' : '1'), ncolumns, nrows);
    bs.writall((void*)(const char *)head, head.length());
  }
  // body
  if(raw)
  {
    if(!rle)
      compress();
    const unsigned char *runs=rle;
    const unsigned char * const runs_end=rle+rlelength;
    const int count=(ncolumns+7)>>3;
    unsigned char *buf;
    GPBuffer<unsigned char> gbuf(buf,count);
    while(runs<runs_end)
    {
      rle_get_bitmap(ncolumns,runs,buf,false);
      bs.writall(buf,count);
    }
  }else
  {
    if (!bytes)
      uncompress();
    const unsigned char *row = bytes + border;
    int n = nrows - 1;
    row += n * bytes_per_row;
    while (n >= 0)
    {
      unsigned char eol='\n';
      for (int c=0; c<ncolumns;)
      {
        unsigned char bit= (row[c] ? '1' : '0');
        bs.write((void*)&bit, 1);
        c += 1;
        if (c==ncolumns || (c&(int)RUNMSBMASK)==0) 
          bs.write((void*)&eol, 1);          
       }
      // next row
      row -= bytes_per_row;
      n -= 1;
    }
  }
}

void 
GBitmap::save_pgm(ByteStream &bs, int raw)
{
  // checks
  GMonitorLock lock(monitor());
  if (!bytes)
    uncompress();
  // header
  GUTF8String head;
  head.format("P%c\n%d %d\n%d\n", (raw ? '5' : '2'), ncolumns, nrows, grays-1);
  bs.writall((void*)(const char *)head, head.length());
  // body
  const unsigned char *row = bytes + border;
  int n = nrows - 1;
  row += n * bytes_per_row;
  while (n >= 0)
    {
      if (raw)
        {
          for (int c=0; c<ncolumns; c++)
            {
              char x = grays - 1 - row[c];
              bs.write((void*)&x, 1);
            }
        }
      else 
        {
          unsigned char eol='\n';
          for (int c=0; c<ncolumns; )
            {
              head.format("%d ", grays - 1 - row[c]);
              bs.writall((void*)(const char *)head, head.length());
              c += 1;
              if (c==ncolumns || (c&0x1f)==0) 
                bs.write((void*)&eol, 1);          
            }
        }
      row -= bytes_per_row;
      n -= 1;
    }
}

void 
GBitmap::save_rle(ByteStream &bs)
{
  // checks
  if (ncolumns==0 || nrows==0)
    G_THROW( ERR_MSG("GBitmap.not_init") );
  GMonitorLock lock(monitor());
  if (grays > 2)
    G_THROW( ERR_MSG("GBitmap.cant_make_PBM") );
  // header
  GUTF8String head;
  head.format("R4\n%d %d\n", ncolumns, nrows);
  bs.writall((void*)(const char *)head, head.length());
  // body
  if (rle)
    {
      bs.writall((void*)rle, rlelength);
    }
  else
    {
      unsigned char *runs = 0;
      GPBuffer<unsigned char> gruns(runs);
      int size = encode(runs,gruns);
      bs.writall((void*)runs, size);
    }
}


// ------ runs


void
GBitmap::makerows(
  int nrows, const int ncolumns, unsigned char *runs, unsigned char *rlerows[])
{
  while (nrows-- > 0)
  {
    rlerows[nrows] = runs;
    int c;
    for(c=0;c<ncolumns;c+=GBitmap::read_run(runs))
      EMPTY_LOOP;
    if (c > ncolumns)
      G_THROW( ERR_MSG("GBitmap.lost_sync2") );
  }
}


void
GBitmap::rle_get_bitmap (
  const int ncolumns,
  const unsigned char *&runs,
  unsigned char *bitmap,
  const bool invert )
{
  const int obyte_def=invert?0xff:0;
  const int obyte_ndef=invert?0:0xff;
  int mask=0x80,obyte=0;
  for(int c=ncolumns;c > 0 ;)
  {
    int x=read_run(runs);
    c-=x;
    while((x--)>0)
    {
      if(!(mask>>=1))
      {
        *(bitmap++) = obyte^obyte_def;
        obyte=0;
        mask=0x80;
        for(;x>=8;x-=8)
        {
          *(bitmap++)=obyte_def;
        }
      }
    }
    if(c>0)
    {
      int x=read_run(runs);
      c-=x;
      while((x--)>0)
      {
        obyte|=mask;
        if(!(mask>>=1))
        {
          *(bitmap++)=obyte^obyte_def;
          obyte=0;
          mask=0x80;
          for(;(x>8);x-=8)
            *(bitmap++)=obyte_ndef;
        }
      }
    }
  }
  if(mask != 0x80)
  {
    *(bitmap++)=obyte^obyte_def;
  }
}

int 
GBitmap::rle_get_bits(int rowno, unsigned char *bits) const
{
  GMonitorLock lock(monitor());
  if (!rle)
    return 0;
  if (rowno<0 || rowno>=nrows)
    return 0;
  if (!rlerows)
  {
    const_cast<GPBuffer<unsigned char *> &>(grlerows).resize(nrows);
    makerows(nrows,ncolumns,rle,const_cast<unsigned char **>(rlerows));
  }
  int n = 0;
  int p = 0;
  int c = 0;
  unsigned char *runs = rlerows[rowno];
  while (c < ncolumns)
    {
      const int x=read_run(runs);
      if ((c+=x)>ncolumns)
        c = ncolumns;
      while (n<c)
        bits[n++] = p;
      p = 1-p;
    }
  return n;
}


int 
GBitmap::rle_get_runs(int rowno, int *rlens) const
{
  GMonitorLock lock(monitor());
  if (!rle)
    return 0;
  if (rowno<0 || rowno>=nrows)
    return 0;
  if (!rlerows)
  {
    const_cast<GPBuffer<unsigned char *> &>(grlerows).resize(nrows);
    makerows(nrows,ncolumns,rle,const_cast<unsigned char **>(rlerows));
  }
  int n = 0;
  int d = 0;
  int c = 0;
  unsigned char *runs = rlerows[rowno];
  while (c < ncolumns)
    {
      const int x=read_run(runs);
      if (n>0 && !x)
        {
          n--;
          d = d-rlens[n];
        }
      else 
        {
          rlens[n++] = (c+=x)-d;
          d = c;
        }
    }
  return n;
}


int 
GBitmap::rle_get_rect(GRect &rect) const
{
  GMonitorLock lock(monitor());
  if (!rle) 
    return 0;
  int area = 0;
  unsigned char *runs = rle;
  rect.xmin = ncolumns;
  rect.ymin = nrows;
  rect.xmax = 0;
  rect.ymax = 0;
  int r = nrows;
  while (--r >= 0)
    {
      int p = 0;
      int c = 0;
      int n = 0;
      while (c < ncolumns)
        {
          const int x=read_run(runs);
          if(x)
            {
              if (p)
                {
                  if (c < rect.xmin) 
                    rect.xmin = c;
                  if ((c += x) > rect.xmax) 
                    rect.xmax = c-1;
                  n += x;
                }
              else
                {
                  c += x;
                }
            }
          p = 1-p;
        }
      area += n;
      if (n)
        {
          rect.ymin = r;
          if (r > rect.ymax) 
            rect.ymax = r;
        }
    }
  if (area==0)
    rect.clear();
  return area;
}



// ------ helpers

int
GBitmap::encode(unsigned char *&pruns,GPBuffer<unsigned char> &gpruns) const
{
  // uncompress rle information
  if (nrows==0 || ncolumns==0)
  {
    gpruns.resize(0);
    return 0;
  }
  if (!bytes)
    {
      unsigned char *runs;
      GPBuffer<unsigned char> gruns(runs,rlelength);
      memcpy((void*)runs, rle, rlelength);
      gruns.swap(gpruns);
      return rlelength;
    }
  gpruns.resize(0);
  // create run array
  int pos = 0;
  int maxpos = 1024 + ncolumns + ncolumns;
  unsigned char *runs;
  GPBuffer<unsigned char> gruns(runs,maxpos);
  // encode bitmap as rle
  const unsigned char *row = bytes + border;
  int n = nrows - 1;
  row += n * bytes_per_row;
  while (n >= 0)
    {
      if (maxpos < pos+ncolumns+ncolumns+2)
        {
          maxpos += 1024 + ncolumns + ncolumns;
          gruns.resize(maxpos);
        }

      unsigned char *runs_pos=runs+pos;
      const unsigned char * const runs_pos_start=runs_pos;
      append_line(runs_pos,row,ncolumns);
      pos+=(size_t)runs_pos-(size_t)runs_pos_start;
      row -= bytes_per_row;
      n -= 1;
    }
  // return result
  gruns.resize(pos);
  gpruns.swap(gruns);
  return pos;
}

void 
GBitmap::decode(unsigned char *runs)
{
  // initialize pixel array
  if (nrows==0 || ncolumns==0)
    G_THROW( ERR_MSG("GBitmap.not_init") );
  bytes_per_row = ncolumns + border;
  if (runs==0)
    G_THROW( ERR_MSG("GBitmap.null_arg") );
  size_t npixels = nrows * bytes_per_row + border;
  if (!bytes_data)
  {
    gbytes_data.resize(npixels);
    bytes = bytes_data;
  }
  gbytes_data.clear();
  gzerobuffer=zeroes(bytes_per_row + border);
  // interpret runs data
  int c, n;
  unsigned char p = 0;
  unsigned char *row = bytes_data + border;
  n = nrows - 1;
  row += n * bytes_per_row;
  c = 0;
  while (n >= 0)
    {
      int x = read_run(runs);
      if (c+x > ncolumns)
        G_THROW( ERR_MSG("GBitmap.lost_sync2") );
      while (x-- > 0)
        row[c++] = p;
      p = 1 - p;
      if (c >= ncolumns) 
        {
          c = 0;
          p = 0;
          row -= bytes_per_row;
          n -= 1; 
        }
    }
  // Free rle data possibly attached to this bitmap
  grle.resize(0);
  grlerows.resize(0);
  rlelength = 0;
#ifndef NDEBUG
  check_border();
#endif
}

class GBitmap::ZeroBuffer : public GPEnabled
{
public:
  ZeroBuffer(const unsigned int zerosize);
  unsigned char *zerobuffer;
  GPBuffer<unsigned char> gzerobuffer;
};

GBitmap::ZeroBuffer::ZeroBuffer(const unsigned int zerosize)
: gzerobuffer(zerobuffer,zerosize)
{
  gzerobuffer.clear();
  GBitmap::zerobuffer=zerobuffer;
  GBitmap::zerosize=zerosize;
}

static const unsigned char static_zerobuffer[]=
{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 32
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 64
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 96 
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 128 
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 160
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 192
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 234
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 256
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 288
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 320
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 352 
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 384 
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 416
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 448
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 480
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 512
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 544
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 576
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 608 
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 640 
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 672
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 704
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 736
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 768
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 800
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 832
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 864 
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 896 
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 928
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 960
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 992
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 1024
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 1024+32
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 1024+64
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 1024+96 
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 1024+128 
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 1024+160
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 1024+192
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 1024+234
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 1024+256
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 1024+288
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 1024+320
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 1024+352 
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 1024+384 
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 1024+416
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 1024+448
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 1024+480
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 1024+512
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 1024+544
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 1024+576
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 1024+608 
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 1024+640 
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 1024+672
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 1024+704
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 1024+736
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 1024+768
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 1024+800
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 1024+832
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 1024+864 
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 1024+896 
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 1024+928
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 1024+960
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 1024+992
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 2048
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 2048+32
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 2048+64
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 2048+96 
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 2048+128 
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 2048+160
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 2048+192
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 2048+234
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 2048+256
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 2048+288
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 2048+320
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 2048+352 
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 2048+384 
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 2048+416
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 2048+448
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 2048+480
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 2048+512
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 2048+544
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 2048+576
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 2048+608 
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 2048+640 
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 2048+672
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 2048+704
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 2048+736
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 2048+768
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 2048+800
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 2048+832
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 2048+864 
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 2048+896 
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 2048+928
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 2048+960
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 2048+992
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 3072
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 3072+32
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 3072+64
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 3072+96 
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 3072+128 
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 3072+160
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 3072+192
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 3072+234
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 3072+256
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 3072+288
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 3072+320
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 3072+352 
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 3072+384 
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 3072+416
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 3072+448
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 3072+480
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 3072+512
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 3072+544
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 3072+576
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 3072+608 
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 3072+640 
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 3072+672
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 3072+704
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 3072+736
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 3072+768
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 3072+800
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 3072+832
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 3072+864 
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 3072+896 
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 3072+928
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 3072+960
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 3072+992
 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}; // 4096

int GBitmap::zerosize = sizeof(static_zerobuffer);
unsigned char *GBitmap::zerobuffer=const_cast<unsigned char *>(static_zerobuffer);

GP<GBitmap::ZeroBuffer>
GBitmap::zeroes(int required)
{
  GMonitorLock lock(&monitors[0]); // any monitor would do
  static GP<GBitmap::ZeroBuffer> gzerobuffer;
  if (zerosize < required)
  {
    int z;
    for(z=zerosize;z<required;z <<= 1)
      EMPTY_LOOP;
    z=(z+0xfff)&(~0xfff);
    gzerobuffer=new GBitmap::ZeroBuffer((unsigned int)z);
  }
  return gzerobuffer;
}


// Fills a bitmap with the given value
void 
GBitmap::fill(unsigned char value)
{
  GMonitorLock lock(monitor());
  for(unsigned int y=0; y<rows(); y++)
    {
      unsigned char* bm_y = (*this)[y];
      for(unsigned int x=0; x<columns(); x++)
        bm_y[x] = value;
    }
}


void 
GBitmap::append_long_run(unsigned char *&data, int count)
{
  while (count > MAXRUNSIZE)
    {
      data[0] = data[1] = 0xff;
      data[2] = 0;
      data += 3;
      count -= MAXRUNSIZE;
    }
  if (count < RUNOVERFLOWVALUE)
    {
      data[0] = count;
      data += 1;
    }
  else
    {
      data[0] = (count>>8) + GBitmap::RUNOVERFLOWVALUE;
      data[1] = (count & 0xff);
      data += 2;
    }
}


void
GBitmap::append_line(unsigned char *&data,const unsigned char *row,
                     const int rowlen,bool invert)
{
  const unsigned char *rowend=row+rowlen;
  bool p=!invert;
  while(row<rowend)
    {
      int count=0;
      if ((p=!p)) 
        {
          if(*row)
            for(++count,++row;(row<rowend)&&*row;++count,++row)
            	EMPTY_LOOP;
        } 
      else if(!*row)
        {
          for(++count,++row;(row<rowend)&&!*row;++count,++row)
          	EMPTY_LOOP;
        }
      append_run(data,count);
    }
}

#if 0
static inline int
GetRowTDLRNR(
  GBitmap &bit,const int row, const unsigned char *&startptr,
  const unsigned char *&stopptr)
{
  stopptr=(startptr=bit[row])+bit.columns();
  return 1;
}

static inline int
GetRowTDLRNR(
  GBitmap &bit,const int row, const unsigned char *&startptr,
  const unsigned char *&stopptr)
{
  stopptr=(startptr=bit[row])+bit.columns();
  return 1;
}

static inline int
GetRowTDRLNR(
  GBitmap &bit,const int row, const unsigned char *&startptr,
  const unsigned char *&stopptr)
{
  startptr=(stopptr=bit[row]-1)+bit.columns();
  return -1;
}
#endif // 0

GP<GBitmap> 
GBitmap::rotate(int count)
{
  GP<GBitmap> newbitmap=this;
  count = count & 3;
  if(count)
  {
    if( count & 0x01 )
    {
      newbitmap = new GBitmap(ncolumns, nrows);
    }else
    {
      newbitmap = new GBitmap(nrows, ncolumns);
    }
    GMonitorLock lock(monitor());
    if (!bytes_data)
      uncompress();
    GBitmap &dbitmap = *newbitmap;
    dbitmap.set_grays(grays);
    switch(count)
    {
    case 3: // rotate 90 counter clockwise
      {
        const int lastrow = dbitmap.rows()-1;
        for(int y=0; y<nrows; y++)
        {
          const unsigned char *r=operator[] (y);
          for(int x=0,xnew=lastrow;xnew>=0; x++,xnew--)
          {
            dbitmap[xnew][y] = r[x];
          }
        }
      }
      break;
    case 2: // rotate 180 counter clockwise
      {
        const int lastrow = dbitmap.rows()-1;
        const int lastcolumn = dbitmap.columns()-1;
        for(int y=0,ynew=lastrow;ynew>=0; y++,ynew--)
        {
          const unsigned char *r=operator[] (y);
          unsigned char *d=dbitmap[ynew];
          for(int xnew=lastcolumn;xnew>=0; r++,--xnew)
          {
            d[xnew] = *r;
          }
        }
      }
      break;
    case 1: // rotate 270 counter clockwise
      {
        const int lastcolumn = dbitmap.columns()-1;
        for(int y=0,ynew=lastcolumn;ynew>=0;y++,ynew--)
        {
          const unsigned char *r=operator[] (y);
          for(int x=0; x<ncolumns; x++)
          {
            dbitmap[x][ynew] = r[x];
          }
        }
      }
      break;
    }
    if(grays == 2)
    {
      compress();
      dbitmap.compress();
    }
  }
  return newbitmap;
}

#ifndef NDEBUG
void 
GBitmap::check_border() const
{int col ;
  if (bytes)
    {
      const unsigned char *p = (*this)[-1];
      for (col=-border; col<ncolumns+border; col++)
        if (p[col])
          G_THROW( ERR_MSG("GBitmap.zero_damaged") );
      for (int row=0; row<nrows; row++)
        {
          p = (*this)[row];
          for (col=-border; col<0; col++)
            if (p[col])
              G_THROW( ERR_MSG("GBitmap.left_damaged") );
          for (col=ncolumns; col<ncolumns+border; col++)
            if (p[col])
              G_THROW( ERR_MSG("GBitmap.right_damaged") );
        }
    }
}
#endif


#ifdef HAVE_NAMESPACES
}
# ifndef NOT_USING_DJVU_NAMESPACE
using namespace DJVU;
# endif
#endif

