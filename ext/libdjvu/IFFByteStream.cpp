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

// -- Implementation of IFFByteStream
// - Author: Leon Bottou, 06/1998

// From: Leon Bottou, 1/31/2002
// This has been changed by Lizardtech to fit better 
// with their re-implementation of ByteStreams.

#include <assert.h>
#include "IFFByteStream.h"


#ifdef HAVE_NAMESPACES
namespace DJVU {
# ifdef NOT_DEFINED // Just to fool emacs c++ mode
}
#endif
#endif


// Constructor
IFFByteStream::IFFByteStream(const GP<ByteStream> &xbs,const int xpos)
  : ByteStream::Wrapper(xbs), ctx(0), dir(0)
{
  offset = seekto = xpos;
  has_magic_att = false;
  has_magic_sdjv = false;
}

// Destructor
IFFByteStream::~IFFByteStream()
{
  while (ctx)
    close_chunk();
}

GP<IFFByteStream>
IFFByteStream::create(const GP<ByteStream> &bs)
{
  const int pos=bs->tell();
  return new IFFByteStream(bs,pos);
}


// IFFByteStream::ready
// -- indicates if bytestream is ready for reading
//    returns number of bytes available

int 
IFFByteStream::ready()
{
  if (ctx && dir < 0)
    return ctx->offEnd - offset;
  else if (ctx)
    return 1;
  else
    return 0;
}


// IFFByteStream::composite
// -- indicates if bytestream is ready for putting or getting chunks

int 
IFFByteStream::composite()
{
  if (ctx && !ctx->bComposite)
    return 0;
  else
    return 1;
}




// IFFByteStream::check_id
// -- checks if an id is legal

int 
IFFByteStream::check_id(const char *id)
{
  int i;
  // check absence of null bytes
  for (i=0; i<4; i++)
    if (id[i]<0x20 || id[i]>0x7e)
      return -1;
  // check composite chunks
  static const char *szComposite[] = { "FORM", "LIST", "PROP", "CAT ", 0 };
  for (i=0; szComposite[i]; i++) 
    if (!memcmp(id, szComposite[i], 4))
      return 1;
  // check reserved chunks
  static const char *szReserved[] = { "FOR", "LIS", "CAT", 0 };
  for (i=0; szReserved[i]; i++) 
    if (!memcmp(id, szReserved[i], 3) && id[3]>='1' && id[3]<='9')
      return -1;
  // regular chunk
  return 0;
}



// IFFByteStream::get_chunk
// -- get next chunk header

int  
IFFByteStream::get_chunk(GUTF8String &chkid, int *rawoffsetptr, int *rawsizeptr)
{
  int bytes;
  char buffer[8];
  
  // Check that we are allowed to read a chunk
  if (dir > 0)
    G_THROW( ERR_MSG("IFFByteStream.read_write") );
  if (ctx && !ctx->bComposite)
    G_THROW( ERR_MSG("IFFByteStream.not_ready") );
  dir = -1;

  // Seek to end of previous chunk if necessary
  if (seekto > offset)
    {
      bs->seek(seekto);
      offset = seekto;
    }

  // Skip padding byte
  if (ctx && offset == ctx->offEnd)
    return 0;
  if (offset & 1)
    {
      bytes = bs->read( (void*)buffer, 1);
      if (bytes==0 && !ctx)
        return 0;
      offset += bytes;
    }
  
  // Record raw offset
  int rawoffset = offset;
  
  // Read chunk id (skipping magic sequences inserted here to make
  // DjVu files recognizable.)
  for(;;)
  {
    if (ctx && offset == ctx->offEnd)
      return 0;
    if (ctx && offset+4 > ctx->offEnd)
      G_THROW( ERR_MSG("IFFByteStream.corrupt_end") );
    bytes = bs->readall( (void*)&buffer[0], 4);
    offset = seekto = offset + bytes;
    if (bytes==0 && !ctx)
      return 0;
    if (bytes != 4)
      G_THROW(ByteStream::EndOfFile);
    if (buffer[0] == 'S' && buffer[1] == 'D' &&
        buffer[2] == 'J' && buffer[3] == 'V' )
      {
        has_magic_sdjv = true;
        continue;
      }
    else if (buffer[0] == 'A' && buffer[1] == 'T' &&
             buffer[2] == '&' && buffer[3] == 'T' )
      {
        has_magic_att=true;
        continue;
      }
    else
      break;
  }
  
  // Read chunk size
  if (ctx && offset+4 > ctx->offEnd)
    G_THROW( ERR_MSG("IFFByteStream.corrupt_end2") );
  bytes = bs->readall( (void*)&buffer[4], 4);
  offset = seekto = offset + bytes;
  if (bytes != 4)
    G_THROW( ByteStream::EndOfFile );
  long size = ((unsigned char)buffer[4]<<24) |
              ((unsigned char)buffer[5]<<16) |
              ((unsigned char)buffer[6]<<8)  |
              ((unsigned char)buffer[7]);
  if (ctx && offset+size > ctx->offEnd)
    G_THROW( ERR_MSG("IFFByteStream.corrupt_mangled") );
  
  // Check if composite 
  int composite = check_id(buffer);
  if (composite < 0)
    G_THROW( ERR_MSG("IFFByteStream.corrupt_id") );
  
  // Read secondary id of composite chunk
  if (composite)
  {
    if (ctx && ctx->offEnd<offset+4)
      G_THROW( ERR_MSG("IFFByteStream.corrupt_header") );
    bytes = bs->readall( (void*)&buffer[4], 4);
    offset += bytes;
    if (bytes != 4)
      G_THROW( ByteStream::EndOfFile );
    if (check_id(&buffer[4]))
      G_THROW( ERR_MSG("IFFByteStream.corrupt_2nd_id") );
  }

  // Create context record
  IFFContext *nctx = new IFFContext;
  G_TRY
  {
    nctx->next = ctx;
    nctx->offStart = seekto;
    nctx->offEnd = seekto + size;
    if (composite)
    {
      memcpy( (void*)(nctx->idOne), (void*)&buffer[0], 4);
      memcpy( (void*)(nctx->idTwo), (void*)&buffer[4], 4);
      nctx->bComposite = 1;
    }
    else
    {
      memcpy( (void*)(nctx->idOne), (void*)&buffer[0], 4);
      memset( (void*)(nctx->idTwo), 0, 4);
      nctx->bComposite = 0;
    }
  }
  G_CATCH_ALL
  {
    delete nctx;
    G_RETHROW;
  }
  G_ENDCATCH;
  
  // Install context record
  ctx = nctx;
  chkid = GUTF8String(ctx->idOne, 4);
  if (composite)
    chkid = chkid + ":" + GUTF8String(ctx->idTwo, 4);

  // Return
  if (rawoffsetptr)
    *rawoffsetptr = rawoffset;
  if (rawsizeptr)
    *rawsizeptr = ( ctx->offEnd - rawoffset + 1) & ~0x1;
  return size;
}



// IFFByteStream::put_chunk
// -- write new chunk header

void  
IFFByteStream::put_chunk(const char *chkid, int insert_magic)
{
  int bytes;
  char buffer[8];

  // Check that we are allowed to write a chunk
  if (dir < 0)
    G_THROW( ERR_MSG("IFFByteStream.read_write") );
  if (ctx && !ctx->bComposite)
    G_THROW( ERR_MSG("IFFByteStream.not_ready2") );
  dir = +1;

  // Check primary id
  int composite = check_id(chkid);
  if ((composite<0) || (composite==0 && chkid[4])
      || (composite && (chkid[4]!=':' || check_id(&chkid[5]) || chkid[9])) )
    G_THROW( ERR_MSG("IFFByteStream.bad_chunk") );

  // Write padding byte
  assert(seekto <= offset);
  memset((void*)buffer, 0, 8);
  if (offset & 1)
    offset += bs->write((void*)&buffer[4], 1);

  // Insert magic to make this file recognizable as DjVu
  if (insert_magic)
  {
    // Don't change the way you do the file magic!
    // I rely on these bytes letters in some places
    // (like DjVmFile.cpp and djvm.cpp) -- eaf
    buffer[0]=0x41;
    buffer[1]=0x54;
    buffer[2]=0x26;
    buffer[3]=0x54;
    offset += bs->writall((void*)&buffer[0], 4);
  }

  // Write chunk header
  memcpy((void*)&buffer[0], (void*)&chkid[0], 4);
  bytes = bs->writall((void*)&buffer[0], 8);
  offset = seekto = offset + bytes;
  if (composite)
  {
    memcpy((void*)&buffer[4], (void*)&chkid[5], 4);
    bytes = bs->writall((void*)&buffer[4], 4);
    offset = offset + bytes;    
  }

  // Create new context record
  IFFContext *nctx = new IFFContext;
  G_TRY
  {
    nctx->next = ctx;
    nctx->offStart = seekto;
    nctx->offEnd = 0;
    if (composite)
    {
      memcpy( (void*)(nctx->idOne), (void*)&buffer[0], 4);
      memcpy( (void*)(nctx->idTwo), (void*)&buffer[4], 4);
      nctx->bComposite = 1;
    }
    else
    {
      memcpy( (void*)(nctx->idOne), (void*)&buffer[0], 4);
      memset( (void*)(nctx->idTwo), 0, 4);
      nctx->bComposite = 0;
    }
  }
  G_CATCH_ALL
  {
    delete nctx;
    G_RETHROW;
  }
  G_ENDCATCH; 
  // Install context record and leave
  ctx = nctx;
}



void 
IFFByteStream::close_chunk()
{
  // Check that this is ok
  if (!ctx)
    G_THROW( ERR_MSG("IFFByteStream.cant_close") );
  // Patch size field in new chunk
  if (dir > 0)
    {
      ctx->offEnd = offset;
      long size = ctx->offEnd - ctx->offStart;
      char buffer[4];
      buffer[0] = (unsigned char)(size>>24);
      buffer[1] = (unsigned char)(size>>16);
      buffer[2] = (unsigned char)(size>>8);
      buffer[3] = (unsigned char)(size);
      bs->seek(ctx->offStart - 4);
      bs->writall((void*)buffer, 4);
      bs->seek(offset);
    }
  // Arrange for reader to seek at next chunk
  seekto = ctx->offEnd;
  // Remove ctx record
  IFFContext *octx = ctx;
  ctx = octx->next;
  assert(ctx==0 || ctx->bComposite);
  delete octx;
}

// This is the same as above, but adds a seek to the close
// Otherwise an EOF in this chunk won't be reported until we
// try to open the next chunk, which makes error recovery
// very difficult.
void  
IFFByteStream::seek_close_chunk(void)
{
  close_chunk();
  if ((dir <= 0)&&((!ctx)||(ctx->bComposite))&&(seekto > offset))
  {
    bs->seek(seekto);
    offset = seekto;
  }
}

// IFFByteStream::short_id
// Returns the id of the current chunk

void 
IFFByteStream::short_id(GUTF8String &chkid)
{
  if (!ctx)
    G_THROW( ERR_MSG("IFFByteStream.no_chunk_id") );
  if (ctx->bComposite)
    chkid = GUTF8String(ctx->idOne, 4) + ":" + GUTF8String(ctx->idTwo, 4);
  else
    chkid = GUTF8String(ctx->idOne, 4);
}


// IFFByteStream::full_id
// Returns the full chunk id of the current chunk

void 
IFFByteStream::full_id(GUTF8String &chkid)
{
  short_id(chkid);
  if (ctx->bComposite)
    return;
  // Search parent FORM or PROP chunk.
  for (IFFContext *ct = ctx->next; ct; ct=ct->next)
    if (memcmp(ct->idOne, "FOR", 3)==0 || 
        memcmp(ct->idOne, "PRO", 3)==0  )
      {
        chkid = GUTF8String(ct->idTwo, 4) + "." + chkid;
        break;
      }
}



// IFFByteStream::read
// -- read bytes from IFF file chunk

size_t 
IFFByteStream::read(void *buffer, size_t size)
{
  if (! (ctx && dir < 0))
    G_THROW( ERR_MSG("IFFByteStream.not_ready3") );
  // Seek if necessary
  if (seekto > offset) {
    bs->seek(seekto);
    offset = seekto;
  }
  // Ensure that read does not extend beyond chunk
  if (offset > ctx->offEnd)
    G_THROW( ERR_MSG("IFFByteStream.bad_offset") );
  if (offset + (long)size >  ctx->offEnd)
    size = (size_t) (ctx->offEnd - offset);
  // Read bytes
  size_t bytes = bs->read(buffer, size);
  offset += bytes;
  return bytes;
}


// IFFByteStream::write
// -- write bytes to IFF file chunk

size_t 
IFFByteStream::write(const void *buffer, size_t size)
{
  if (! (ctx && dir > 0))
    G_THROW( ERR_MSG("IFFByteStream.not_ready4") );
  if (seekto > offset)
    G_THROW( ERR_MSG("IFFByteStream.cant_write") );
  size_t bytes = bs->write(buffer, size);
  offset += bytes;
  return bytes;
}

// IFFByteStream::tell
// -- tell position

long 
IFFByteStream::tell() const
{
  return (seekto>offset)?seekto:offset;
}

bool
IFFByteStream::compare(IFFByteStream &iff)
{
  bool retval=(&iff == this);
  if(!retval)
  {
    GUTF8String chkid1, chkid2;
    int size;
    while((size=get_chunk(chkid1)) == iff.get_chunk(chkid2))
    {
      if(chkid1 != chkid2)
      {
        break;
      }
      if(!size)
      {
        retval=true;
        break;
      }
      char buf[4096];
      int len;
      while((len=read(buf,sizeof(buf))))
      {
        int s=0;
        char buf2[sizeof(buf)];
        while(s<len)
        {
          const int i=iff.read(buf2+s,len-s);
          if(!i)
            break;
          s+=i;
        }
        if((s != len)||memcmp(buf,buf2,len))
          break;
      }
      if(len)
        break;
      iff.close_chunk();
      close_chunk();
    }
  }
  return retval;
}


#ifdef HAVE_NAMESPACES
}
# ifndef NOT_USING_DJVU_NAMESPACE
using namespace DJVU;
# endif
#endif
