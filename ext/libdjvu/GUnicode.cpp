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

#include "GString.h"

#include <stddef.h>

#if HAS_ICONV
#include <iconv.h>
#endif

#ifdef HAVE_NAMESPACES
namespace DJVU {
# ifdef NOT_DEFINED // Just to fool emacs c++ mode
}
#endif
#endif

static unsigned char nill=0;

static void const * 
checkmarks(void const * const xbuf,
           unsigned int &bufsize,
           GStringRep::EncodeType &rep)
{
  unsigned char const *buf=(unsigned char const *)xbuf;
  if(bufsize >= 2 || (xbuf && !bufsize && rep != GStringRep::XOTHER))
  {
    const unsigned int s=(((unsigned int)buf[0])<<8)+(unsigned int)buf[1];
    switch(s)
    {
      case 0:
        if((bufsize>=4)||(!bufsize && rep == GStringRep::XUCS4BE)
          ||(!bufsize && rep == GStringRep::XUCS4_2143))
        {
          const unsigned int s=(((unsigned int)buf[2])<<8)+(unsigned int)buf[3];
          if(s == 0xfeff)
          { 
            rep=GStringRep::XUCS4BE;
            buf+=4;
          }else if(s == 0xfffe)
          {
            rep=GStringRep::XUCS4_2143;
            buf+=4;
          }
        }
        break;
      case 0xfffe:
        if(((bufsize>=4)||(!bufsize && rep == GStringRep::XUCS4LE)) 
           && !((unsigned char *)buf)[2] && !((unsigned char *)buf)[3])
        {
          rep=GStringRep::XUCS4LE;
          buf+=4;
        }else
        {
          rep=GStringRep::XUTF16LE;
          buf+=2;
        }
        break;
      case 0xfeff:
        if(((bufsize>=4)||(!bufsize && rep == GStringRep::XUCS4_3412)) 
           && !((unsigned char *)buf)[2] && !((unsigned char *)buf)[3])
        {
          rep=GStringRep::XUCS4_3412;
          buf+=4;
        }else
        {
          rep=GStringRep::XUTF16LE;
          buf+=2;
        }
        break;
      case 0xefbb:
        if(((bufsize>=3)||(!bufsize && GStringRep::XUTF8 == rep))&&(buf[2] == 0xbf))
        {
          rep=GStringRep::XUTF8;
          buf+=3;
        }
        break;
      default:
        break;
    }
  }
  if(buf != xbuf)
  {
    if(bufsize)
    {
      const size_t s=(size_t)xbuf-(size_t)buf;
      if(bufsize> s)
      {
        bufsize-=s;
      }else
      {
        bufsize=0;
        buf=(const unsigned char *)&nill;
      }
    }
  }
  return buf;
}

class GStringRep::Unicode : public GStringRep::UTF8
{
public:
  GP<GStringRep> encoding;
  EncodeType encodetype;
  void *remainder;
  GPBufferBase gremainder;
public:
  Unicode(void);
  /// virtual destructor.
  virtual ~Unicode();

  static GP<GStringRep> create(const unsigned int sz);
  static GP<GStringRep> create(void const * const buf, unsigned int bufsize,
                               const EncodeType, const GP<GStringRep> &encoding);
  static GP<GStringRep> create( void const * const buf,
    unsigned int size, const EncodeType encodetype );
  static GP<GStringRep> create( void const * const buf,
    const unsigned int size, GP<GStringRep> encoding );
  static GP<GStringRep> create( void const * const buf,
    const unsigned int size, const GP<Unicode> &remainder );

protected:
  virtual void set_remainder( void const * const buf, const unsigned int size,
    const EncodeType encodetype );
  virtual void set_remainder( void const * const buf, const unsigned int size,
    const GP<GStringRep> &encoding );
  virtual void set_remainder( const GP<Unicode> &remainder );
  virtual GP<Unicode> get_remainder(void) const;
};
// static uint32_t UTF8toUCS4(unsigned char const *&,void const * const);
static uint32_t xUTF16toUCS4(uint16_t const *&s,void const * const);
static uint32_t UTF16BEtoUCS4(unsigned char const *&s,void const * const);
static uint32_t UTF16LEtoUCS4(unsigned char const *&s,void const * const);
static uint32_t UCS4BEtoUCS4(unsigned char const *&s,void const * const);
static uint32_t UCS4LEtoUCS4(unsigned char const *&s,void const * const);
static uint32_t UCS4_3412toUCS4(unsigned char const *&s,void const * const);
static uint32_t UCS4_2143toUCS4(unsigned char const *&s,void const * const);

GP<GStringRep>
GStringRep::Unicode::create(const unsigned int sz)
{
  GP<GStringRep> gaddr;
  if (sz > 0)
  {
    GStringRep *addr;
    gaddr=(addr=new GStringRep::Unicode);
    addr->data=(char *)(::operator new(sz+1));
    addr->size = sz;
    addr->data[sz] = 0;
  }
  return gaddr;
}

GStringRep::Unicode::Unicode(void)
: encodetype(XUTF8), gremainder(remainder,0,1) {}

GStringRep::Unicode::~Unicode() {}

GP<GStringRep>
GStringRep::Unicode::create(
  void const * const xbuf,
  unsigned int bufsize,
  const EncodeType t,
  const GP<GStringRep> &encoding)
{
  return (encoding->size)
    ?create(xbuf,bufsize,encoding)
    :create(xbuf,bufsize,t);
}

GP<GStringRep>
GStringRep::Unicode::create(
  void const * const xbuf,
  const unsigned int bufsize,
  const GP<Unicode> &xremainder )
{
  Unicode *r=xremainder;
  GP<GStringRep> retval;
  if(r)
  {
    const int s=r->gremainder;
    if(xbuf && bufsize)
    {
      if(s)
      {
        void *buf;
        GPBufferBase gbuf(buf,s+bufsize,1);
        memcpy(buf,r->remainder,s);
        memcpy((void *)((size_t)buf+s),xbuf,bufsize);
        retval=((r->encoding)
          ?create(buf,s+bufsize,r->encoding)
          :create(buf,s+bufsize,r->encodetype));
      }else
      {
        retval=((r->encoding)
          ?create(xbuf,bufsize,r->encoding)
          :create(xbuf,bufsize,r->encodetype));
      }
    }else if(s)
	{
      void *buf;
      GPBufferBase gbuf(buf,s,1);
      memcpy(buf,r->remainder,s);
      retval=((r->encoding)
        ?create(buf,s,r->encoding)
        :create(buf,s,r->encodetype));
	}else
    {
      retval=((r->encoding)
        ?create(0,0,r->encoding)
        :create(0,0,r->encodetype));
    }
  }else
  {
    retval=create(xbuf,bufsize,XUTF8);
  }
  return retval;
}

#if HAS_ICONV
/* This template works around incompatible iconv protoypes */
template<typename _T> inline size_t 
iconv_adaptor(size_t(*iconv_func)(iconv_t, _T, size_t *, char**, size_t*),
              iconv_t cd, char **inbuf, size_t *inbytesleft,
              char **outbuf, size_t *outbytesleft)
{
  return iconv_func (cd, (_T)inbuf, inbytesleft, outbuf, outbytesleft);
}
#endif

GP<GStringRep>
GStringRep::Unicode::create(
  void const * const xbuf,
  unsigned int bufsize,
  GP<GStringRep> encoding)
{
  GP<GStringRep> retval;
  GStringRep *e=encoding;
  if(e)
  {
    e=(encoding=e->upcase());
  }
  if(!e || !e->size)
  {
    retval=create(xbuf,bufsize,XOTHER);
  }else if(!e->cmp("UTF8") || !e->cmp("UTF-8"))
  {
    retval=create(xbuf,bufsize,XUTF8);
  }else if(!e->cmp("UTF16")|| !e->cmp("UTF-16")
        || !e->cmp("UCS2") || !e->cmp("UCS-2"))
  {
    retval=create(xbuf,bufsize,XUTF16);
  }else if(!e->cmp("UCS4") || !e->cmp("UCS-4"))
  {
    retval=create(xbuf,bufsize,XUCS4);
  }else
  {
#if HAS_ICONV
    EncodeType t=XOTHER;
    void const * const buf=checkmarks(xbuf,bufsize,t); 
    if(t != XOTHER)
    {
      retval=create(xbuf,bufsize,t);
    }else if(buf && bufsize)
    {
      unsigned char const *eptr=(unsigned char *)buf;
      unsigned int j=0;
      for(j=0;(j<bufsize)&&*eptr;j++,eptr++)
        EMPTY_LOOP;
      if (j)
      {
        unsigned char const *ptr=(unsigned char *)buf;
        if(e)
        {
          iconv_t cv=iconv_open("UTF-8",(const char *)e);
          if(cv == (iconv_t)(-1))
          { 
            const int i=e->search('-');
            if(i>=0)
            {
              cv=iconv_open("UTF-8",e->data+i+1);
            }
          }
          if(cv == (iconv_t)(-1))
          { 
            retval=create(0,0,XOTHER);
          }else
          {
            size_t ptrleft=(eptr-ptr); 
            char *utf8buf;
            size_t pleft=6*ptrleft+1;
            GPBuffer<char> gutf8buf(utf8buf,pleft);
            char *p=utf8buf;
            char *nptr = (char*)ptr;
            while(iconv_adaptor(iconv, cv, &nptr, &ptrleft, &p, &pleft)) 
              ptr = (unsigned char*)nptr;
            iconv_close(cv);
            retval=create(utf8buf,(size_t)ptr-(size_t)buf,t);
            retval->set_remainder(ptr,(size_t)eptr-(size_t)ptr,e);
          }
        }
      }else
      {
        retval=create(0,0,XOTHER);
        retval->set_remainder(0,0,e);
      }
    }
#else
    retval=create(xbuf,bufsize,XOTHER);
#endif
  }
  return retval;
}

GP<GStringRep>
GStringRep::Unicode::create(
  void const * const xbuf,
  unsigned int bufsize,
  EncodeType t)
{
  GP<GStringRep> gretval;
  GStringRep *retval=0;
  void const * const buf=checkmarks(xbuf,bufsize,t); 
  if(buf && bufsize)
  {
    unsigned char const *eptr=(unsigned char *)buf;
    unsigned int maxutf8size=0;
    void const* const xeptr=(void const *)((size_t)eptr+bufsize);
    switch(t)
    {
      case XUCS4:
      case XUCS4BE:
      case XUCS4LE:
      case XUCS4_2143:
      case XUCS4_3412:
      {
        for(uint32_t w;
          (eptr<xeptr)&&(w=*(uint32_t const *)eptr);
          eptr+=sizeof(uint32_t))
        {
          maxutf8size+=(w>0x7f)?6:1;
        }
        break;
      }
      case XUTF16:
      case XUTF16BE:
      case XUTF16LE:
      {
        for(uint16_t w;
          (eptr<xeptr)&&(w=*(uint16_t const *)eptr);
          eptr+=sizeof(uint16_t))
        {
          maxutf8size+=3;
        }
        break;
      }
      case XUTF8:
        for(;(eptr<xeptr)&&*eptr;maxutf8size++,eptr++)
          EMPTY_LOOP;
        break;
      case XEBCDIC:
        for(;(eptr<xeptr)&&*eptr;eptr++)
        {
          maxutf8size+=(*eptr>0x7f)?2:1;
        }
        break;
      default:
        break;
    }
    unsigned char *utf8buf=0;
    GPBuffer<unsigned char> gutf8buf(utf8buf,maxutf8size+1);
    utf8buf[0]=0;
    if (maxutf8size)
    {
      unsigned char *optr=utf8buf;
      int len=0;
      unsigned char const *iptr=(unsigned char *)buf;
      uint16_t const *sptr=(uint16_t *)buf;
      uint32_t w;
      switch(t)
      {
        case XUCS4:
          for(;
            (iptr<eptr)&&(w=*(uint32_t const *)iptr);
            len++,iptr+=sizeof(uint32_t const))
          {
            optr=UCS4toUTF8(w,optr);
          }
          break;
        case XUCS4BE:
          for(;(w=UCS4BEtoUCS4(iptr,eptr));len++)
          {
            optr=UCS4toUTF8(w,optr);
          }
          break;
        case XUCS4LE:
          for(;(w=UCS4LEtoUCS4(iptr,eptr));len++)
          {
            optr=UCS4toUTF8(w,optr);
          }
          break;
        case XUCS4_2143:
          for(;(w=UCS4_2143toUCS4(iptr,eptr));len++)
          {
            optr=UCS4toUTF8(w,optr);
          }
          break;
        case XUCS4_3412:
          for(;(w=UCS4_3412toUCS4(iptr,eptr));len++)
          {
            optr=UCS4toUTF8(w,optr);
          }
          break;
        case XUTF16:
          for(;(w=xUTF16toUCS4(sptr,eptr));len++)
          {
            optr=UCS4toUTF8(w,optr);
          }
          break;
        case XUTF16BE:
          for(;(w=UTF16BEtoUCS4(iptr,eptr));len++)
          {
            optr=UCS4toUTF8(w,optr);
          }
          break;
        case XUTF16LE:
          for(;(w=UTF16LEtoUCS4(iptr,eptr));len++)
          {
            optr=UCS4toUTF8(w,optr);
          }
          break;
        case XUTF8:
          for(;(w=UTF8toUCS4(iptr,eptr));len++)
          {
            optr=UCS4toUTF8(w,optr);
          }
          break;
        case XEBCDIC:
          for(;(iptr<eptr)&&(w=*iptr++);len++)
          {
            optr=UCS4toUTF8(w,optr);
          }
          break;
        default:
          break;
      }
      const unsigned int size=(size_t)optr-(size_t)utf8buf;
      if(size)
      {
        retval=(gretval=GStringRep::Unicode::create(size));
        memcpy(retval->data,utf8buf,size);
      }
      else
      {
        retval=(gretval=GStringRep::Unicode::create(1));
        retval->size=size;
      }
      retval->data[size]=0;
      gutf8buf.resize(0);
      const size_t s=(size_t)eptr-(size_t)iptr;
      retval->set_remainder(iptr,s,t);
    }
  }
  if(!retval)
    {
      retval=(gretval=GStringRep::Unicode::create(1));
      retval->data[0]=0;
      retval->size=0;
      retval->set_remainder(0,0,t);
    }
  return gretval;
}

static uint32_t
xUTF16toUCS4(uint16_t const *&s,void const * const eptr)
{
  uint32_t U=0;
  uint16_t const * const r=s+1;
  if(r <= eptr)
  {
    uint32_t const W1=s[0];
    if((W1<0xD800)||(W1>0xDFFF))
    {
      if((U=W1))
        {
          s=r;
        }
    }
    else if(W1<=0xDBFF)
      {
        uint16_t const * const rr=r+1;
        if(rr <= eptr)
          {
            uint32_t const W2=s[1];
            if(((W2>=0xDC00)||(W2<=0xDFFF))
               &&((U=(0x1000+((W1&0x3ff)<<10))|(W2&0x3ff))))
              {
                s=rr;
              }
            else
              {
                U=(unsigned int)(-1)-W1;
                s=r;
              }
          }
      }
  }
  return U;
}

static uint32_t
UTF16BEtoUCS4(unsigned char const *&s,void const * const eptr)
{
  uint32_t U=0;
  unsigned char const * const r=s+2;
  if(r <= eptr)
    {
      uint32_t const C1MSB=s[0];
      if((C1MSB<0xD8)||(C1MSB>0xDF))
        {
          if((U=((C1MSB<<8)|((uint32_t)s[1]))))
            {
              s=r;
            }
        }
      else if(C1MSB<=0xDB)
        {
          unsigned char const * const rr=r+2;
          if(rr <= eptr)
            {
              uint32_t const C2MSB=s[2];
              if((C2MSB>=0xDC)||(C2MSB<=0xDF))
                {
                  U=0x10000+((uint32_t)s[1]<<10)+(uint32_t)s[3]
                    +(((C1MSB<<18)|(C2MSB<<8))&0xc0300);
                  s=rr;
                }
              else
                {
                  U=(unsigned int)(-1)-((C1MSB<<8)|((uint32_t)s[1]));
                  s=r;
                }
            }
        }
    }
  return U;
}

static uint32_t
UTF16LEtoUCS4(unsigned char const *&s,void const * const eptr)
{
  uint32_t U=0;
  unsigned char const * const r=s+2;
  if(r <= eptr)
    {
      uint32_t const C1MSB=s[1];
      if((C1MSB<0xD8)||(C1MSB>0xDF))
        {
          if((U=((C1MSB<<8)|((uint32_t)s[0]))))
            {
              s=r;
            }
        }
      else if(C1MSB<=0xDB)
        {
          unsigned char const * const rr=r+2;
          if(rr <= eptr)
            {
              uint32_t const C2MSB=s[3];
              if((C2MSB>=0xDC)||(C2MSB<=0xDF))
                {
                  U=0x10000+((uint32_t)s[0]<<10)+(uint32_t)s[2]
                    +(((C1MSB<<18)|(C2MSB<<8))&0xc0300);
                  s=rr;
                }
              else
                {
                  U=(unsigned int)(-1)-((C1MSB<<8)|((uint32_t)s[1]));
                  s=r;
                }
            }
        }
    }
  return U;
}

static uint32_t
UCS4BEtoUCS4(unsigned char const *&s,void const * const eptr)
{
  uint32_t U=0;
  unsigned char const * const r=s+4;
  if(r<=eptr)
    {
      U=(((((((uint32_t)s[0]<<8)|(uint32_t)s[1])<<8)
           |(uint32_t)s[2])<<8)|(uint32_t)s[3]);
      if(U)
        {
          s=r;
        } 
    }
  return U;
}

static uint32_t
UCS4LEtoUCS4(unsigned char const *&s,void const * const eptr)
{
  uint32_t U=0;
  unsigned char const * const r=s+4;
  if(r<=eptr)
    {
      U=(((((((uint32_t)s[3]<<8)|(uint32_t)s[2])<<8)|
           (uint32_t)s[1])<<8)|(uint32_t)s[0]);
      if(U)
        {
          s=r;
        }
    }
  return U;
}

static uint32_t
UCS4_2143toUCS4(unsigned char const *&s,void const * const eptr)
{
  uint32_t U=0;
  unsigned char const * const r=s+4;
  if(r<=eptr)
    {
      U=(((((((uint32_t)s[1]<<8)|(uint32_t)s[0])<<8)
           |(uint32_t)s[3])<<8)|(uint32_t)s[2]);
      if(U)
        {
          s=r;
        }
    }
  return U;
}

static uint32_t
UCS4_3412toUCS4(unsigned char const *&s,void const * const eptr)
{
  uint32_t U=0;
  unsigned char const * const r=s+4;
  if(r<=eptr)
    {
      U=(((((((uint32_t)s[2]<<8)|(uint32_t)s[3])<<8)
           |(uint32_t)s[0])<<8)|(uint32_t)s[1]);
      if(U)
        {
          s=r;
        }
    }
  return U;
}

void
GStringRep::Unicode::set_remainder( void const * const buf,
   const unsigned int size, const EncodeType xencodetype )
{
  gremainder.resize(size,1);
  if(size)
    memcpy(remainder,buf,size);
  encodetype=xencodetype;
  encoding=0;
}

void
GStringRep::Unicode::set_remainder( void const * const buf,
   const unsigned int size, const GP<GStringRep> &xencoding )
{
  gremainder.resize(size,1);
  if(size)
    memcpy(remainder,buf,size);
  encoding=xencoding;
  encodetype=XOTHER;
}

void
GStringRep::Unicode::set_remainder( const GP<GStringRep::Unicode> &xremainder )
{
  if(xremainder)
  {
    const int size=xremainder->gremainder;
    gremainder.resize(size,1);
    if(size)
      memcpy(remainder,xremainder->remainder,size);
    encodetype=xremainder->encodetype;
  }
  else
  {
    gremainder.resize(0,1);
    encodetype=XUTF8;
  }
}

GP<GStringRep::Unicode>
GStringRep::Unicode::get_remainder( void ) const
{
  return const_cast<GStringRep::Unicode *>(this);
}

GUTF8String 
GUTF8String::create(void const * const buf,const unsigned int size,
    const EncodeType encodetype, const GUTF8String &encoding)
{
  return encoding.length()
    ?create(buf,size,encodetype)
    :create(buf,size,encoding);
}

GUTF8String 
GUTF8String::create( void const * const buf,
  unsigned int size, const EncodeType encodetype )
{
  GUTF8String retval;
  retval.init(GStringRep::Unicode::create(buf,size,encodetype));
  return retval;
}

GUTF8String 
GUTF8String::create( void const * const buf,
  const unsigned int size, const GP<GStringRep::Unicode> &remainder)
{
  GUTF8String retval;
  retval.init(GStringRep::Unicode::create(buf,size,remainder));
  return retval;
}

GUTF8String 
GUTF8String::create( void const * const buf,
  const unsigned int size, const GUTF8String &encoding )
{
  GUTF8String retval;
  retval.init(GStringRep::Unicode::create(buf,size,encoding ));
  return retval;
}


#ifdef HAVE_NAMESPACES
}
# ifndef NOT_USING_DJVU_NAMESPACE
using namespace DJVU;
# endif
#endif
