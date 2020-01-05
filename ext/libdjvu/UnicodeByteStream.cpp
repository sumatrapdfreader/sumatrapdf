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

#include "UnicodeByteStream.h"
#include "ByteStream.h"


#ifdef HAVE_NAMESPACES
namespace DJVU {
# ifdef NOT_DEFINED // Just to fool emacs c++ mode
}
#endif
#endif

UnicodeByteStream::UnicodeByteStream(const UnicodeByteStream &uni)
: bs(uni.bs), buffer(uni.buffer), bufferpos(uni.bufferpos), linesread(0)
{
  startpos=bs->tell();
}

UnicodeByteStream::UnicodeByteStream(
  GP<ByteStream> ibs,const GStringRep::EncodeType et)
: bs(ibs), bufferpos(0), linesread(0)
{
  buffer=GUTF8String::create(0,0,et);
  startpos=bs->tell();
}

UnicodeByteStream::~UnicodeByteStream()
{}

static int
CountLines(const GUTF8String &str)
{
  int retval=0;
  static const unsigned long lf='\n';
  for(int pos=0;(pos=str.search(lf,pos)+1)>0;++retval)
    EMPTY_LOOP;
  return retval;
}

void
UnicodeByteStream::set_encodetype(const GStringRep::EncodeType et)
{
  seek(startpos,SEEK_SET);
  bufferpos=0;
  buffer=GUTF8String::create(0,0,et);
}

void
UnicodeByteStream::set_encoding(const GUTF8String &xencoding)
{
  seek(startpos,SEEK_SET);
  bufferpos=0;
  buffer=GUTF8String::create(0,0,xencoding);
}

size_t
UnicodeByteStream::read(void *buf, size_t size)
{
  bufferpos=0;
  const int retval=bs->read(buf,size);
  if(retval)
  {
    buffer=GUTF8String::create(
      (unsigned char const *)buf,retval,buffer.get_remainder());
  }else
  {
    buffer=GUTF8String::create(0,0,buffer.get_remainder());
  }
  return retval;
}

size_t
UnicodeByteStream::write(const void *buf, size_t size)
{
  bufferpos=0;
  buffer=GUTF8String::create(0,0,buffer.get_remainder());
  return bs->write(buf,size);
}

long 
UnicodeByteStream::tell(void) const
{
  return bs->tell();
}

UnicodeByteStream & 
UnicodeByteStream::operator=(UnicodeByteStream &uni)
{
  bs=uni.bs;
  bufferpos=uni.bufferpos;
  buffer=uni.buffer;
  return *this;
}

int 
UnicodeByteStream::seek
(long offset, int whence, bool nothrow)
{
  int retval=bs->seek(offset,whence,nothrow);
  bufferpos=0;
  buffer=GUTF8String::create(0,0,buffer.get_remainder());
  return retval;
}

void 
UnicodeByteStream::flush(void)
{
  bs->flush();
  bufferpos=0;
  buffer=GUTF8String::create(0,0,buffer.get_remainder());
}



GUTF8String
UnicodeByteStream::gets(
  size_t const t,unsigned long const stopat,bool const inclusive)
{
  GUTF8String retval;
  unsigned int len=buffer.length()-bufferpos;
  if(!len)
  {
    int i;
    char *buf;
  	static const size_t bufsize=327680;
    GPBuffer<char> gbuf(buf,bufsize);
    while((i=read(buf,bufsize)>0))
    {
      if((len=buffer.length()-bufferpos))
        break;
    }
  }
  if(len)
  {
    int i=buffer.search((char)stopat,bufferpos);
    if(i>=0)
    {
      if(inclusive)
      {
        ++i;
      }
      if(t&&(i>(int)t+bufferpos))
      {
        i=t+bufferpos;
      }
      if(i>bufferpos)
      {
        retval=buffer.substr(bufferpos,i-bufferpos);
      }
      bufferpos=i;
      linesread+=CountLines(retval);
    }else
    {
      retval=buffer.substr(bufferpos,len);
      bufferpos=buffer.length();
      linesread+=CountLines(retval);
      retval+=gets(t?(t-(i-bufferpos)):0,stopat,inclusive);
    }
  }
  return retval;
}

XMLByteStream::XMLByteStream(UnicodeByteStream &uni)
: UnicodeByteStream(uni) {}

XMLByteStream::XMLByteStream(GP<ByteStream> &ibs) 
: UnicodeByteStream(ibs,GStringRep::XOTHER)
{}

GP<XMLByteStream>
XMLByteStream::create(GP<ByteStream> ibs) 
{
  XMLByteStream *xml=new XMLByteStream(ibs);
  GP<XMLByteStream> retval=xml;
  xml->init();
  return retval;
}

void
XMLByteStream::init(void)
{
  unsigned char buf[4];
  GP<ByteStream> ibs=bs;
  bufferpos=0;
  bs->readall(buf,sizeof(buf));
  const unsigned int i=(buf[0]<<8)+buf[1];
  switch(i)
  {
    case 0x0000:
    {
      const unsigned int j=(buf[2]<<8)+buf[3];
      switch(j)
      {
        case 0x003C:
        {
          buffer=GUTF8String::create(buf,sizeof(buf),GStringRep::XUCS4BE);
          break;
        }
        case 0x3C00:
        {
          buffer=GUTF8String::create(buf,sizeof(buf),GStringRep::XUCS4_2143);
          break;
        }
        case 0xFEFF:
        {
          buffer=GUTF8String::create(0,0,GStringRep::XUCS4BE);
          startpos+=sizeof(buf);
          break;
        }
        case 0xFFFE:
        {
          buffer=GUTF8String::create(0,0,GStringRep::XUCS4_2143);
          startpos+=sizeof(buf);
          break;
        }
        default:
        {
          buffer=GUTF8String::create(buf,sizeof(buf),GStringRep::XUTF8);
          break;
        }
      }
      break;
    }
    case 0x003C:
    {
      const unsigned int j=(buf[2]<<8)+buf[3];
      switch(j)
      {
        case 0x0000:
          buffer=GUTF8String::create(buf,sizeof(buf),GStringRep::XUCS4_3412);
          break;
        case 0x003F:
          buffer=GUTF8String::create(buf,sizeof(buf),GStringRep::XUTF16BE);
          break;
        default:
          buffer=GUTF8String::create(buf,sizeof(buf),GStringRep::XUTF8);
          break;
      }
      break;
    }
    case 0x3C00:
    {
      const unsigned int j=(buf[2]<<8)+buf[3];
      switch(j)
      {
        case 0x0000:
          buffer=GUTF8String::create(buf,sizeof(buf),GStringRep::XUCS4LE);
          break;
        case 0x3F00:
          buffer=GUTF8String::create(buf,sizeof(buf),GStringRep::XUTF16LE);
          break;
        default:
          buffer=GUTF8String::create(buf,sizeof(buf),GStringRep::XUTF8);
          break;
      }
      break;
    }
    case 0x4C6F:
    {
      const unsigned int j=(buf[2]<<8)+buf[3];
      buffer=GUTF8String::create(buf,sizeof(buf),
         (j == 0xA794)?(GStringRep::XEBCDIC):(GStringRep::XUTF8));
      break;
    }
    case 0xFFFE:
    {
      buffer=GUTF8String::create(buf+2,sizeof(buf)-2,GStringRep::XUTF16LE);
      startpos+=2;
      break;
    }
    case 0xFEFF:
    {
      buffer=GUTF8String::create(buf+2,sizeof(buf)-2,GStringRep::XUTF16BE);
      startpos+=2;
      break;
    }
    case 0xEFBB:
    {
      if(buf[2] == 0xBF)
      {
        buffer=GUTF8String::create(buf+3,sizeof(buf)-3,GStringRep::XUTF8);
        startpos+=3;
      }else
      {
        buffer=GUTF8String::create(buf,sizeof(buf),GStringRep::XUTF8);
      }
      break;
    }
    case 0x3C3F:
    default:
    {
      buffer=GUTF8String::create(buf,sizeof(buf),GStringRep::XUTF8);
      break;
    }
  }
  bs=ibs;
}

XMLByteStream::~XMLByteStream()
{}


#ifdef HAVE_NAMESPACES
}
# ifndef NOT_USING_DJVU_NAMESPACE
using namespace DJVU;
# endif
#endif
