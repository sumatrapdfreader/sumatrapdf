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

// From: Leon Bottou, 1/31/2002
// This file has very little to do with my initial implementation.
// It has been practically rewritten by Lizardtech for i18n changes.
// My original implementation was very small in comparison
// <http://prdownloads.sourceforge.net/djvu/DjVu2_2b-src.tgz>.
// In my opinion, the duplication of the string classes is a failed
// attempt to use the type system to enforce coding policies.
// This could be fixed.  But there are better things to do in djvulibre.

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#if NEED_GNUG_PRAGMAS
# pragma implementation
#endif

#include "GString.h"
#include "GThreads.h"
#include "debug.h"

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#if HAS_WCHAR
# include <locale.h>
# if !defined(AUTOCONF) || HAVE_WCHAR_H
#  include <wchar.h>
# endif
# if HAS_WCTYPE
#  include <wctype.h>
# endif
#endif
#include <ctype.h>

#ifndef LC_NUMERIC          //MingW
# undef DO_CHANGELOCALE
# define LC_NUMERIC 0
#endif
#ifndef DO_CHANGELOCALE
# define DO_CHANGELOCALE 0
#endif


#ifdef HAVE_NAMESPACES
namespace DJVU {
# ifdef NOT_DEFINED // Just to fool emacs c++ mode
}
#endif
#endif


GBaseString::~GBaseString() {}
GNativeString::~GNativeString() {}
GUTF8String::~GUTF8String() {}

#if !HAS_MBSTATE && HAS_WCHAR
// Under some systems, wctomb() and mbtowc() are not thread
// safe.  In those cases, wcrtomb and mbrtowc are preferred.
// For Solaris, wctomb() and mbtowc() are thread safe, and 
// wcrtomb() and mbrtowc() don't exist.

#define wcrtomb MYwcrtomb
#define mbrtowc MYmbrtowc
#define mbrlen  MYmbrlen

static inline int
wcrtomb(char *bytes,wchar_t w,mbstate_t *)
{
  return wctomb(bytes,w);
}

static inline int
mbrtowc(wchar_t *w,const char *source, size_t n, mbstate_t *)
{
  return mbtowc(w,source,n);
}

static inline size_t
mbrlen(const char *s, size_t n, mbstate_t *)
{
  return mblen(s,n);
}
#endif // !HAS_MBSTATE || HAS_WCHAR


GP<GStringRep>
GStringRep::upcase(void) const
{ return tocase(giswupper,gtowupper); }

GP<GStringRep>
GStringRep::downcase(void) const
{ return tocase(giswlower,gtowlower); }

GP<GStringRep>
GStringRep::UTF8::create(const unsigned int sz)
{
  return GStringRep::create(sz,(GStringRep::UTF8 *)0);
}

GP<GStringRep>
GStringRep::UTF8::create(const char *s)
{
  GStringRep::UTF8 dummy;
  return dummy.strdup(s);
}

GP<GStringRep> 
GStringRep::UTF8::create(const GP<GStringRep> &s1,const GP<GStringRep> &s2)
{
  GStringRep::UTF8 dummy;
  return dummy.concat(s1,s2);
}

GP<GStringRep> 
GStringRep::UTF8::create( const GP<GStringRep> &s1,const char *s2)
{
  GStringRep::UTF8 dummy;
  return dummy.concat(s1,s2);
}

GP<GStringRep> 
GStringRep::UTF8::create( const char *s1, const GP<GStringRep> &s2)
{
  GStringRep::UTF8 dummy;
  return dummy.concat(s1,s2);
}

GP<GStringRep> 
GStringRep::UTF8::create( const char *s1,const char *s2)
{ 
  GStringRep::UTF8 dummy;
  return dummy.concat(s1,s2);
}

GP<GStringRep> 
GStringRep::UTF8::create(const char *s,const int start,const int length)
{
  GStringRep::UTF8 dummy;
  return dummy.substr(s,start,length);
}

GP<GStringRep> 
GStringRep::UTF8::create(
  const uint16_t *s,const int start,const int length)
{
  GStringRep::UTF8 dummy;
  return dummy.substr(s,start,length);
}

GP<GStringRep>
GStringRep::UTF8::create(
  const uint32_t *s,const int start,const int length)
{
  GStringRep::UTF8 dummy;
  return dummy.substr(s,start,length);
}

GP<GStringRep> 
GStringRep::UTF8::blank(const unsigned int sz) const
{
   return GStringRep::create(sz,(GStringRep::UTF8 *)0);
}

bool
GStringRep::UTF8::isUTF8(void) const
{
  return true;
}

GP<GStringRep> 
GStringRep::UTF8::toThis(
    const GP<GStringRep> &rep,const GP<GStringRep> &) const
{
  return rep?(rep->toUTF8(true)):rep;
}

GP<GStringRep> 
GStringRep::UTF8::create(const char fmt[],va_list& args)
{ 
  const GP<GStringRep> s(create(fmt));
  return (s?(s->vformat(args)):s);
}

#if !HAS_WCHAR

#define NATIVE_CREATE(x) UTF8::create( x );

#ifdef LC_ALL
#undef LC_ALL
#endif
#define LC_ALL 0

class GStringRep::ChangeLocale
{
public:
  ChangeLocale(const int,const char *) {}
  ~ChangeLocale() {};
};

GP<GStringRep>
GStringRep::NativeToUTF8( const char *s )
{
  return GStringRep::UTF8::create(s);
}

#else

#define NATIVE_CREATE(x) Native::create( x );

// The declaration and implementation of GStringRep::ChangeLocale
// Not used in WinCE

class GStringRep::ChangeLocale
{
public:
  ChangeLocale(const int category,const char locale[]);
  ~ChangeLocale();
private:
  GUTF8String locale;
#if DO_CHANGELOCALE
  int category;
#endif
};

class GStringRep::Native : public GStringRep
{
public:
  // default constructor
  Native(void);
  // virtual destructor
  virtual ~Native();

    // Other virtual methods.
      // Create an empty string.
  virtual GP<GStringRep> blank(const unsigned int sz = 0) const;
      // Append a string.
  virtual GP<GStringRep> append(const GP<GStringRep> &s2) const;
      // Test if Native.
  virtual bool isNative(void) const;
      // Convert to Native.
  virtual GP<GStringRep> toNative(
    const EscapeMode escape=UNKNOWN_ESCAPED) const;
      // Convert to UTF8.
  virtual GP<GStringRep> toUTF8(const bool nothrow=false) const;
      // Convert to UTF8.
  virtual GP<GStringRep> toThis(
     const GP<GStringRep> &rep,const GP<GStringRep> &) const;
      // Compare with #s2#.
  virtual int cmp(const GP<GStringRep> &s2, const int len=(-1)) const;

  // Convert strings to numbers.
  virtual int toInt(void) const;
  virtual long toLong(
    const int pos, int &endpos, const int base=10) const;
  virtual unsigned long toULong(
    const int pos, int &endpos, const int base=10) const;
  virtual double toDouble(
    const int pos, int &endpos) const;

    // Create an empty string
  static GP<GStringRep> create(const unsigned int sz = 0);

    // Create a strdup string.
  static GP<GStringRep> create(const char *s);

  // Creates by appending to the current string

   // Creates with a concat operation.
  static GP<GStringRep> create(
    const GP<GStringRep> &s1,const GP<GStringRep> &s2);
  static GP<GStringRep> create( const GP<GStringRep> &s1,const char *s2);
  static GP<GStringRep> create( const char *s1, const GP<GStringRep> &s2);
  static GP<GStringRep> create(const char *s1,const char *s2);

    // Create with a strdup and substr operation.
  static GP<GStringRep> create(
    const char *s,const int start,const int length=(-1));
  static GP<GStringRep> create(
    const uint16_t *s,const int start,const int length=(-1));
  static GP<GStringRep> create(
    const uint32_t *s,const int start,const int length=(-1));

    // Create with an sprintf()
  static GP<GStringRep> create_format(const char fmt[],...);
  static GP<GStringRep> create(const char fmt[],va_list &args);

  virtual unsigned char *UCS4toString(
    const uint32_t w,unsigned char *ptr, mbstate_t *ps=0) const;

  // Tests if a string is legally encoded in the current character set.
  virtual bool is_valid(void) const;

  virtual int ncopy(wchar_t * const buf, const int buflen) const;

  friend class GBaseString;
protected:
  // Return the next character and increment the source pointer.
  virtual uint32_t getValidUCS4(const char *&source) const;
};

GP<GStringRep>
GStringRep::Native::create(const unsigned int sz)
{
  return GStringRep::create(sz,(GStringRep::Native *)0);
}

    // Create a strdup string.
GP<GStringRep>
GStringRep::Native::create(const char *s)
{
  GStringRep::Native dummy;
  return dummy.strdup(s);
}

GP<GStringRep>
GStringRep::Native::create(const GP<GStringRep> &s1,const GP<GStringRep> &s2)
{
  GStringRep::Native dummy;
  return dummy.concat(s1,s2);
}

GP<GStringRep> 
GStringRep::Native::create( const GP<GStringRep> &s1,const char *s2)
{
  GStringRep::Native dummy;
  return dummy.concat(s1,s2);
}

GP<GStringRep>
GStringRep::Native::create( const char *s1, const GP<GStringRep> &s2)
{
  GStringRep::Native dummy;
  return dummy.concat(s1,s2);
}

GP<GStringRep>
GStringRep::Native::create(const char *s1,const char *s2)
{
  GStringRep::Native dummy;
  return dummy.concat(s1,s2);
}

GP<GStringRep>
GStringRep::Native::create(
  const char *s,const int start,const int length)
{
  GStringRep::Native dummy;
  return dummy.substr(s,start,length);
}

GP<GStringRep>
GStringRep::Native::create(
    const uint16_t *s,const int start,const int length)
{
  GStringRep::Native dummy;
  return dummy.substr(s,start,length);
}

GP<GStringRep>
GStringRep::Native::create(
  const uint32_t *s,const int start,const int length)
{
  GStringRep::Native dummy;
  return dummy.substr(s,start,length);
}

GP<GStringRep> 
GStringRep::Native::blank(const unsigned int sz) const
{
  return GStringRep::create(sz,(GStringRep::Native *)0);
}

bool
GStringRep::Native::isNative(void) const
{
  return true;
}

GP<GStringRep>
GStringRep::Native::toThis(
     const GP<GStringRep> &rep,const GP<GStringRep> &) const
{
  return rep?(rep->toNative(NOT_ESCAPED)):rep;
}

GP<GStringRep> 
GStringRep::Native::create(const char fmt[],va_list &args)
{ 
  const GP<GStringRep> s(create(fmt));
  return (s?(s->vformat(args)):s);
}

int
GStringRep::Native::ncopy(
  wchar_t * const buf, const int buflen ) const
{
  return toUTF8()->ncopy(buf,buflen);
}

GStringRep::ChangeLocale::ChangeLocale(const int xcategory, const char xlocale[] )
#if DO_CHANGELOCALE
  : category(xcategory)
#endif
{
#if DO_CHANGELOCALE
  // This is disabled under UNIX because 
  // it does not play nice with MT.
  if(xlocale)
    {
      locale=setlocale(xcategory,0);
      if(locale.length() &&(locale!=xlocale))
        {
          if(locale == setlocale(category,xlocale))
            {
              locale.empty();
            }
        }
      else
        {
          locale.empty();
        }
    }
#endif
}

GStringRep::ChangeLocale::~ChangeLocale()
{
#if DO_CHANGELOCALE
  if(locale.length())
    {
      setlocale(category,(const char *)locale);
    }
#endif
}

GNativeString &
GNativeString::format(const char fmt[], ... )
{
  va_list args;
  va_start(args, fmt);
  return init(GStringRep::Native::create(fmt,args));
}

// Gather the native implementations here. Not used in WinCE.

GStringRep::Native::Native(void) {}
GStringRep::Native::~Native() {}

GP<GStringRep>
GStringRep::Native::append(const GP<GStringRep> &s2) const
{
  GP<GStringRep> retval;
  if(s2)
  {
    if(s2->isUTF8())
    {
      G_THROW( ERR_MSG("GStringRep.appendUTF8toNative") );
    }
    retval=concat(data,s2->data);
  }else
  {
    retval=const_cast<GStringRep::Native *>(this); 
  }
  return retval;
}

GP<GStringRep>
GStringRep::Native::create_format(const char fmt[],...)
{
  va_list args;
  va_start(args, fmt);
  return create(fmt,args);
}

unsigned char *
GStringRep::Native::UCS4toString(
  const uint32_t w0,unsigned char *ptr, mbstate_t *ps) const
{
  return UCS4toNative(w0,ptr,ps);
}

// Convert a UCS4 to a multibyte string in the value bytes.  
// The data pointed to by ptr should be long enough to contain
// the results with a nill termination.  (Normally 7 characters
// is enough.)
unsigned char *
GStringRep::UCS4toNative(const uint32_t w0,unsigned char *ptr, mbstate_t *ps)
{
  uint16_t w1;
  uint16_t w2=1;
  for(int count=(sizeof(wchar_t)==sizeof(w1)) 
        ? UCS4toUTF16(w0,w1,w2) : 1;
      count;
      --count,w1=w2)
    {
      // wchar_t can be either UCS4 or UCS2
      const wchar_t w=(sizeof(wchar_t) == sizeof(w1))?(wchar_t)w1:(wchar_t)w0;
      int i=wcrtomb((char *)ptr,w,ps);
      if(i<0)
        break;
      ptr[i]=0;
      ptr += i;
    }
  ptr[0]=0;
  return ptr;
}

GP<GStringRep>
GStringRep::Native::toNative(const EscapeMode escape) const
{
  if(escape == UNKNOWN_ESCAPED)
    G_THROW( ERR_MSG("GStringRep.NativeToNative") );
  return const_cast<GStringRep::Native *>(this);
}

GP<GStringRep>
GStringRep::Native::toUTF8(const bool) const
{
  unsigned char *buf;
  GPBuffer<unsigned char> gbuf(buf,size*6+1);
  buf[0]=0;
  if(data && size)
  {
    size_t n=size;
    const char *source=data;
    mbstate_t ps;
    unsigned char *ptr=buf;
    //(void)mbrlen(source, n, &ps);
    memset(&ps,0,sizeof(mbstate_t));
    int i=0;
    if(sizeof(wchar_t) == sizeof(uint32_t))
      {
        wchar_t w = 0;
        for(;(n>0)&&((i=mbrtowc(&w,source,n,&ps))>=0); n-=i,source+=i)
          {
            ptr=UCS4toUTF8((uint32_t)w,ptr);
          }
      }
    else
      { 
        wchar_t w = 0;
        for(;(n>0)&&((i=mbrtowc(&w,source,n,&ps))>=0);n-=i,source+=i)
          {
            uint16_t s[2];
            s[0]=w;
            uint32_t w0;
            if(UTF16toUCS4(w0,s,s+1)<=0)
              {
                source+=i;
                n-=i;
                if((n>0)&&((i=mbrtowc(&w,source,n,&ps))>=0))
                  {
                    s[1]=w;
                    if(UTF16toUCS4(w0,s,s+2)<=0)
                      {
                        i=(-1);
                        break;
                      }
                  }
                else
                  {
                    i=(-1);
                    break;
                  }
              }
            ptr=UCS4toUTF8(w0,ptr);
          }
      }
    if(i<0)
      {
        gbuf.resize(0);
      }
    else
      {
        ptr[0]=0;
      }
  }
  return GStringRep::UTF8::create((const char *)buf);
}

GNativeString
GBaseString::UTF8ToNative(
  const bool currentlocale,const EscapeMode escape) const
{
  const char *source=(*this);
  GP<GStringRep> retval;
  if(source && source[0]) 
    {
#if DO_CHANGELOCALE
      GUTF8String lc_ctype(setlocale(LC_CTYPE,0));
      bool repeat;
      for(repeat=!currentlocale;;repeat=false)
        {
#endif
          retval=(*this)->toNative((GStringRep::EscapeMode)escape);
#if DO_CHANGELOCALE
          if (!repeat || retval || (lc_ctype == setlocale(LC_CTYPE,"")))
            break;
        }
      if(!repeat)
        setlocale(LC_CTYPE,(const char *)lc_ctype);
#endif
    }
  return GNativeString(retval);
}

/*MBCS*/
GNativeString
GBaseString::getUTF82Native( EscapeMode escape ) const
{ //MBCS cvt
  GNativeString retval;

  // We don't want to convert this if it 
  // already is known to be native...
//  if (isNative()) return *this;

  const size_t slen=length()+1;
  if(slen>1)
  {
    retval=UTF8ToNative(false,escape) ;
    if(!retval.length())
    {
      retval=(const char*)*this;
    }
  }
  return retval;
}

GUTF8String
GBaseString::NativeToUTF8(void) const
{
  GP<GStringRep> retval;
  if(length())
  {
    const char *source=(*this);
#if DO_CHANGELOCALE
    GUTF8String lc_ctype=setlocale(LC_CTYPE,0);
    bool repeat;
    for(repeat=true;;repeat=false)
      {
#endif
        if( (retval=GStringRep::NativeToUTF8(source)) )
          if(GStringRep::cmp(retval->toNative(),source))
            retval=GStringRep::UTF8::create((unsigned int)0);
#if DO_CHANGELOCALE
        if(!repeat || retval || (lc_ctype == setlocale(LC_CTYPE,"")))
          break;
      }
    if(!repeat)
      setlocale(LC_CTYPE,(const char *)lc_ctype);
#endif
  }
  return GUTF8String(retval);
}

GUTF8String
GBaseString::getNative2UTF8(void) const
{ //MBCS cvt

   // We don't want to do a transform this
   // if we already are in the given type.
//   if (isUTF8()) return *this;
   
  const size_t slen=length()+1;
  GUTF8String retval;
  if(slen > 1)
  {
    retval=NativeToUTF8();
    if(!retval.length())
    {
      retval=(const char *)(*this);
    }
  }
  return retval;
} /*MBCS*/

int
GStringRep::Native::cmp(const GP<GStringRep> &s2,const int len) const
{
  int retval;
  if(s2)
  {
    if(s2->isUTF8())
    {
      const GP<GStringRep> r(toUTF8(true));
      if(r)
      {
        retval=GStringRep::cmp(r->data,s2->data,len);
      }else
      {
        retval=cmp(s2->toNative(NOT_ESCAPED),len);
      }
    }else
    {
      retval=GStringRep::cmp(data,s2->data,len);
    }
  }else
  {
    retval=GStringRep::cmp(data,0,len);
  }
  return retval;
}

int
GStringRep::Native::toInt() const
{
  return atoi(data);
}

long
GStringRep::Native::toLong(
  const int pos, int &endpos, const int base) const
{
   char *edata=0;
   const long retval=strtol(data+pos, &edata, base);
   if(edata)
   {
     endpos=(int)((size_t)edata-(size_t)data);
   }else
   {
     endpos=(-1);
   }
   return retval;
}

unsigned long
GStringRep::Native::toULong(
  const int pos, int &endpos, const int base) const
{
   char *edata=0;
   const unsigned long retval=strtoul(data+pos, &edata, base);
   if(edata)
   {
     endpos=(int)((size_t)edata-(size_t)data);
   }else
   {
     endpos=(-1);
   }
   return retval;
}

double
GStringRep::Native::toDouble(
  const int pos, int &endpos) const
{
   char *edata=0;
   const double retval=strtod(data+pos, &edata);
   if(edata)
   {
     endpos=(int)((size_t)edata-(size_t)data);
   }else
   {
     endpos=(-1);
   }
   return retval;
}

uint32_t
GStringRep::Native::getValidUCS4(const char *&source) const
{
  uint32_t retval=0;
  int n=(int)((size_t)size+(size_t)data-(size_t)source);
  if(source && (n > 0))
  {
    mbstate_t ps;
    //(void)mbrlen(source, n, &ps);
    memset(&ps,0,sizeof(mbstate_t));
    wchar_t wt;
    const int len=mbrtowc(&wt,source,n,&ps); 
    if(len>=0)
    {
      if(sizeof(wchar_t) == sizeof(uint16_t))
      {
        source+=len;
        uint16_t s[2];
        s[0]=(uint16_t)wt;
        if(UTF16toUCS4(retval,s,s+1)<=0)
        {
          if((n-=len)>0)
          {
            const int len=mbrtowc(&wt,source,n,&ps);
            if(len>=0)
            {
              s[1]=(uint16_t)wt;
              uint32_t w;
              if(UTF16toUCS4(w,s,s+2)>0)
              {
                source+=len;
                retval=w;
              }
            }
          }
        }
      }else
      {
        retval=(uint32_t)wt;
        source++;
      } 
    }else
    {
      source++;
    }
  }
  return retval;
}

// Tests if a string is legally encoded in the current character set.
bool 
GStringRep::Native::is_valid(void) const
{
  bool retval=true;
  if(data && size)
  {
    size_t n=size;
    const char *s=data;
    mbstate_t ps;
    //(void)mbrlen(s, n, &ps);
    memset(&ps,0,sizeof(mbstate_t));
    do
    {
      size_t m=mbrlen(s,n,&ps);
      if(m > n)
      {
        retval=false;
        break;
      }else if(m)
      {
        s+=m;
        n-=m;
      }else
      {
        break;
      }
    } while(n);
  }
  return retval;
}

// These are dummy functions.
void 
GStringRep::set_remainder(void const * const, const unsigned int,
  const EncodeType) {}
void 
GStringRep::set_remainder(void const * const, const unsigned int,
  const GP<GStringRep> &encoding) {}
void
GStringRep::set_remainder( const GP<GStringRep::Unicode> &) {}

GP<GStringRep::Unicode>
GStringRep::get_remainder( void ) const
{
  return 0;
}

GNativeString::GNativeString(const char dat)
{
  init(GStringRep::Native::create(&dat,0,1));
}

GNativeString::GNativeString(const char *str)
{
  init(GStringRep::Native::create(str));
}

GNativeString::GNativeString(const unsigned char *str)
{
  init(GStringRep::Native::create((const char *)str));
}

GNativeString::GNativeString(const uint16_t *str)
{
  init(GStringRep::Native::create(str,0,-1));
}

GNativeString::GNativeString(const uint32_t *str)
{
  init(GStringRep::Native::create(str,0,-1));
}

GNativeString::GNativeString(const char *dat, unsigned int len)
{
  init(
    GStringRep::Native::create(dat,0,((int)len<0)?(-1):(int)len));
}

GNativeString::GNativeString(const uint16_t *dat, unsigned int len)
{
  init(
    GStringRep::Native::create(dat,0,((int)len<0)?(-1):(int)len));
}

GNativeString::GNativeString(const uint32_t *dat, unsigned int len)
{
  init(
    GStringRep::Native::create(dat,0,((int)len<0)?(-1):(int)len));
}

GNativeString::GNativeString(const GNativeString &str)
{
  init(str);
}

GNativeString::GNativeString(const GBaseString &gs, int from, int len)
{
  init(
    GStringRep::Native::create(gs,from,((int)len<0)?(-1):(int)len));
}

GNativeString::GNativeString(const int number)
{
  init(GStringRep::Native::create_format("%d",number));
}

GNativeString::GNativeString(const double number)
{
  init(GStringRep::Native::create_format("%f",number));
}

GNativeString&
GNativeString::operator= (const char str)
{ return init(GStringRep::Native::create(&str,0,1)); }

GNativeString&
GNativeString::operator= (const char *str)
{ return init(GStringRep::Native::create(str)); }

GNativeString
GBaseString::operator+(const GNativeString &s2) const
{
  return GStringRep::Native::create(*this,s2);
}

GP<GStringRep>
GStringRep::NativeToUTF8( const char *s )
{
  return GStringRep::Native::create(s)->toUTF8();
}

#endif // HAS_WCHAR

template <class TYPE>
GP<GStringRep>
GStringRep::create(const unsigned int sz, TYPE *)
{
  GP<GStringRep> gaddr;
  if (sz > 0)
  {
    GStringRep *addr;
    gaddr=(addr=new TYPE);
    addr->data=(char *)(::operator new(sz+1));
    addr->size = sz;
    addr->data[sz] = 0;
  }
  return gaddr;
}

GP<GStringRep>
GStringRep::strdup(const char *s) const
{
  GP<GStringRep> retval;
  const int length=s?strlen(s):0;
  if(length>0)
  {
    retval=blank(length);
    char const * const end=s+length;
    char *ptr=retval->data;
    for(;*s&&(s!=end);ptr++)
    {
      ptr[0]=s++[0];
    }
    ptr[0]=0;
  }
  return retval;
}

GP<GStringRep>
GStringRep::substr(const char *s,const int start,const int len) const
{
  GP<GStringRep> retval;
  if(s && s[0])
  {
    const unsigned int length=(start<0 || len<0)?(unsigned int)strlen(s):(unsigned int)(-1);
    const char *startptr, *endptr;
    if(start<0)
    {
      startptr=s+length+start;
      if(startptr<s)
        startptr=s;
    }else
    { 
      startptr=s;
      for(const char * const ptr=s+start;(startptr<ptr)&&*startptr;++startptr)
        EMPTY_LOOP;
    }
    if(len<0)
    {
      if(s+length+1 < startptr+len)
      {
        endptr=startptr;
      }else
      {
        endptr=s+length+1+len;
      } 
    }else
    {
      endptr=startptr;
      for(const char * const ptr=startptr+len;(endptr<ptr)&&*endptr;++endptr)
        EMPTY_LOOP;
    }
    if(endptr>startptr)
    {
      retval=blank((size_t)(endptr-startptr));
      char *data=retval->data;
      for(; (startptr<endptr) && *startptr; ++startptr,++data)
      {
        data[0]=startptr[0];
      }
      data[0]=0;
    }
  }
  return retval;
}

GP<GStringRep>
GStringRep::substr(const uint16_t *s,const int start,const int len) const
{
  GP<GStringRep> retval;
  if(s && s[0])
  {
    uint16_t const *eptr;
    if(len<0)
    {
      for(eptr=s;eptr[0];++eptr)
        EMPTY_LOOP;
    }else
    {
      eptr=&(s[len]);
    }
    s=&s[start];
    if((size_t)s<(size_t)eptr)
    {
      mbstate_t ps;
      memset(&ps,0,sizeof(mbstate_t));
      unsigned char *buf,*ptr;
      GPBuffer<unsigned char> gbuf(buf,(((size_t)eptr-(size_t)s)/2)*3+7);
      for(ptr=buf;s[0];)
      {
        uint32_t w;
        int i=UTF16toUCS4(w,s,eptr);
        if(i<=0)
          break;
        s+=i;
        ptr=UCS4toString(w,ptr,&ps);
      }
      ptr[0]=0;
      retval = strdup( (const char *)buf );
    }
  }
  return retval;
}

GP<GStringRep>
GStringRep::substr(const uint32_t *s,const int start,const int len) const
{
  GP<GStringRep> retval;
  if(s && s[0])
  {
    uint32_t const *eptr;
    if(len<0)
    {
      for(eptr=s;eptr[0];++eptr)
        EMPTY_LOOP;
    }else
    {
      eptr=&(s[len]);
    }
    s=&s[start];
    if((size_t)s<(size_t)eptr)
    {
      mbstate_t ps;
      memset(&ps,0,sizeof(mbstate_t));
      unsigned char *buf,*ptr;
      GPBuffer<unsigned char> gbuf(buf,((((size_t)eptr-(size_t)s))/4)*6+7);
      for(ptr=buf;s[0];++s)
      {
        ptr=UCS4toString(s[0],ptr,&ps);
      }
      ptr[0]=0;
      retval = strdup( (const char *)buf );
    }
  }
  return retval;
}

GP<GStringRep>
GStringRep::append(const char *s2) const
{
  GP<GStringRep> retval;
  if(s2)
  {
    retval=concat(data,s2);
  }else
  {
    retval=const_cast<GStringRep *>(this);
  }
  return retval;
}

GP<GStringRep>
GStringRep::UTF8::append(const GP<GStringRep> &s2) const
{
  GP<GStringRep> retval;
  if(s2)
  {
    if(s2->isNative())
    {
      G_THROW( ERR_MSG("GStringRep.appendNativeToUTF8") );
    }
    retval=concat(data,s2->data);
  }else
  {
    retval=const_cast<GStringRep::UTF8 *>(this); 
  }
  return retval;
}

GP<GStringRep>
GStringRep::concat(const char *s1,const char *s2) const
{
  const int length1=(s1?strlen(s1):0);
  const int length2=(s2?strlen(s2):0);
  const int length=length1+length2;
  GP<GStringRep> retval;
  if(length>0)
  {
    retval=blank(length);
    GStringRep &r=*retval;
    if(length1)
    {
      strcpy(r.data,s1);
      if(length2)
        strcat(r.data,s2);
    }else
    {
      strcpy(r.data,s2);
    }
  }
  return retval;
}

const char *GBaseString::nullstr = "";

void
GBaseString::empty( void )
{
  init(0);
}

GP<GStringRep>
GStringRep::getbuf(int n) const
{
  GP<GStringRep> retval;
  if(n < 0)
    n=strlen(data);
  if(n >= 0)
  {
    retval=blank((n>0) ? n : 1);
    char *ndata=retval->data;
    strncpy(ndata,data,n);
    ndata[n]=0;
  }
  return retval;
}

const char *
GStringRep::isCharType(bool (*xiswtest)(const unsigned long wc), 
                       const char *ptr, 
                       const bool reverse) const
{
  const char *xptr = ptr;
  unsigned long w=getValidUCS4(xptr);
  if(ptr != xptr)
    {
      if (sizeof(wchar_t) == 2)
        w &= 0xffff;
      if (reverse ^ xiswtest(w))
        ptr = xptr;
    }
  return ptr;
}

int
GStringRep::nextCharType(
  bool (*xiswtest)(const unsigned long wc), const int from, const int len,
  const bool reverse) const
{
  // We want to return the position of the next
  // non white space starting from the #from#
  // location.  isspace should work in any locale
  // so we should only need to do this for the non-
  // native locales (UTF8)
  int retval;
  if(from<size)
  {
    retval=from;
    const char * ptr = data+from;
    for( const char * const eptr=ptr+((len<0)?(size-from):len);
      (ptr<eptr) && *ptr;)
    {
       // Skip characters that fail the isCharType test
      char const * const xptr=isCharType(xiswtest,ptr,!reverse);
      if(xptr == ptr)
        break;
      ptr=xptr;
    }
    retval=(int)((size_t)ptr-(size_t)data);
  }else
  {
    retval=size;
  }
  return retval;
}

bool
GStringRep::giswspace(const unsigned long w)
{
#if HAS_WCTYPE
  return !!iswspace((wchar_t)w);
#else
  return (w & ~0xff) ? false : !!isspace((int)(w & 0xff));
#endif
}

bool
GStringRep::giswupper(const unsigned long w)
{
#if HAS_WCTYPE
  return !!iswupper((wchar_t)w);
#else
  return (w & ~0xff) ? false : !!isupper((int)(w & 0xff));
#endif
}

bool
GStringRep::giswlower(const unsigned long w)
{
#if HAS_WCTYPE
  return !!iswlower((wchar_t)w);
#else
  return (w & ~0xff) ? false : !!islower((int)(w & 0xff));
#endif
}

unsigned long
GStringRep::gtowupper(const unsigned long w)
{
#if HAS_WCTYPE
  return (unsigned long)towupper((wchar_t)w);
#else
  return (w&~0xff) ? w : (unsigned long)toupper(w & 0xff);
#endif
}

unsigned long
GStringRep::gtowlower(const unsigned long w)
{
#if HAS_WCTYPE
  return (unsigned long)towlower((wchar_t)w);
#else
  return (w&~0xff) ? w : (unsigned long)tolower(w & 0xff);
#endif
}

GP<GStringRep>
GStringRep::tocase(
  bool (*xiswcase)(const unsigned long wc),
  unsigned long (*xtowcase)(const unsigned long wc)) const
{
  GP<GStringRep> retval;
  char const * const eptr=data+size;
  char const *ptr=data;
  while(ptr<eptr)
  {
    char const * const xptr=isCharType(xiswcase,ptr,false);
    if(ptr == xptr)
      break;
    ptr=xptr;
  }
  if(ptr<eptr)
  {
    const int n=(int)((size_t)ptr-(size_t)data);
    unsigned char *buf;
    GPBuffer<unsigned char> gbuf(buf,n+(1+size-n)*6);
    if(n>0)
    {
      strncpy((char *)buf,data,n);
    }
    unsigned char *buf_ptr=buf+n;
    for(char const *ptr=data+n;ptr<eptr;)
    {
      char const * const xptr=ptr;
      const unsigned long w=getValidUCS4(ptr);
      if(ptr == xptr)
        break;
      if(xiswcase(w))
      {
        const int len=(int)((size_t)ptr-(size_t)xptr);
        strncpy((char *)buf_ptr,xptr,len);
        buf_ptr+=len;
      }else
      {
        mbstate_t ps;
        memset(&ps,0,sizeof(mbstate_t));
        buf_ptr=UCS4toString(xtowcase(w),buf_ptr,&ps);
      }
    }
    buf_ptr[0]=0;
    retval=substr((const char *)buf,0,(int)((size_t)buf_ptr-(size_t)buf));
  }else
  {
    retval=const_cast<GStringRep *>(this);
  }
  return retval;
}

// Returns a copy of this string with characters used in XML escaped as follows:
//      '<'  -->  "&lt;"
//      '>'  -->  "&gt;"
//      '&'  -->  "&amp;"
//      '\'' -->  "&apos;"
//      '\"' -->  "&quot;"
//  Also escapes characters 0x00 through 0x1f and 0x7e through 0x7f.
GP<GStringRep>
GStringRep::toEscaped( const bool tosevenbit ) const
{
  bool modified=false;
  char *ret;
  GPBuffer<char> gret(ret,size*7);
  ret[0]=0;
  char *retptr=ret;
  char const *start=data;
  char const *s=start;
  char const *last=s;
  GP<GStringRep> special;
  for(unsigned long w;(w=getValidUCS4(s));last=s)  
    // Whoever wrote this for statement should be __complete_here__
  {
    char const *ss=0;
    switch(w)
    {
    case '<':
      ss="&lt;";
      break;
    case '>':
      ss="&gt;";
      break;
    case '&':
      ss="&amp;";
      break;
    case '\47':
      ss="&apos;";
      break;
    case '\42':
      ss="&quot;";
      break;
    default:
      if((w<' ')||(w>=0x7e && (tosevenbit || (w < 0x80))))
      {
        special=toThis(UTF8::create_format("&#%lu;",w));
        ss=special->data;
      }
      break;
    }
    if(ss)
    {
      modified=true;
      if(s!=start)
      {
        size_t len=(size_t)last-(size_t)start;
        strncpy(retptr,start,len);
        retptr+=len;
        start=s;
      }
      if(ss[0])
      {
        size_t len=strlen(ss);
        strcpy(retptr,ss);
        retptr+=len;
      }
    }
  }
  GP<GStringRep> retval;
  if(modified)
  {
    strcpy(retptr,start);
    retval=strdup( ret );
  }else
  {
    retval=const_cast<GStringRep *>(this);
  }
//  DEBUG_MSG( "Escaped string is '" << ret << "'\n" );
  return retval;
}


static const GMap<GUTF8String,GUTF8String> &
BasicMap( void )
{
  static GMap<GUTF8String,GUTF8String> Basic;
  if (! Basic.size())
    {
      Basic["lt"]   = GUTF8String('<');
      Basic["gt"]   = GUTF8String('>');
      Basic["amp"]  = GUTF8String('&');
      Basic["apos"] = GUTF8String('\47');
      Basic["quot"] = GUTF8String('\42');
    }
  return Basic;
}

GUTF8String
GUTF8String::fromEscaped( const GMap<GUTF8String,GUTF8String> ConvMap ) const
{
  GUTF8String ret;                  // Build output string here
  int start_locn = 0;           // Beginning of substring to skip
  int amp_locn;                 // Location of a found ampersand

  while( (amp_locn = search( '&', start_locn )) > -1 )
  {
      // Found the next apostrophe
      // Locate the closing semicolon
    const int semi_locn = search( ';', amp_locn );
      // No closing semicolon, exit and copy
      //  the rest of the string.
    if( semi_locn < 0 )
      break;
    ret += substr( start_locn, amp_locn - start_locn );
    int const len = semi_locn - amp_locn - 1;
    if(len)
    {
      GUTF8String key = substr( amp_locn+1, len);
      //DEBUG_MSG( "key = '" << key << "'\n" );
      char const * s=key;
      if( s[0] == '#')
      {
        unsigned long value;
        char *ptr=0;
        if(s[1] == 'x' || s[1] == 'X')
        {
          value=strtoul((char const *)(s+2),&ptr,16);
        }else
        {
          value=strtoul((char const *)(s+1),&ptr,10);
        }
        if(ptr)
        {
          unsigned char utf8char[7];
          unsigned char const * const end=GStringRep::UCS4toUTF8(value,utf8char);
          ret+=GUTF8String((char const *)utf8char,(size_t)end-(size_t)utf8char);
        }else
        {
          ret += substr( amp_locn, semi_locn - amp_locn + 1 );
        }
      }else
      {  
        GPosition map_entry = ConvMap.contains( key );
        if( map_entry )
        {                           // Found in the conversion map, substitute
          ret += ConvMap[map_entry];
        } else
        {
          static const GMap<GUTF8String,GUTF8String> &Basic = BasicMap();
          GPosition map_entry = Basic.contains( key );
          if ( map_entry )
          {
            ret += Basic[map_entry];
          }else
          {
            ret += substr( amp_locn, len+2 );
          }
        }
      }
    }else
    {
      ret += substr( amp_locn, len+2 );
    }
    start_locn = semi_locn + 1;
//    DEBUG_MSG( "ret = '" << ret << "'\n" );
  }

                                // Copy the end of the string to the output
  ret += substr( start_locn, length()-start_locn );

//  DEBUG_MSG( "Unescaped string is '" << ret << "'\n" );
  return (ret == *this)?(*this):ret;
}

GUTF8String
GUTF8String::fromEscaped(void) const
{
  const GMap<GUTF8String,GUTF8String> nill;
  return fromEscaped(nill);
}

GP<GStringRep>
GStringRep::setat(int n, char ch) const
{
  GP<GStringRep> retval;
  if(n<0)
    n+=size;
  if (n < 0 || n>size) 
    GBaseString::throw_illegal_subscript();
  if(ch == data[n])
  {
    retval=const_cast<GStringRep *>(this);
  }else if(!ch)
  {
    retval=getbuf(n);
  }else
  {
    retval=getbuf((n<size)?size:n);
    retval->data[n]=ch;
    if(n == size)
      retval->data[n+1]=0;
  }
  return retval;
}

#if defined(AUTOCONF) && defined(HAVE_VSNPRINTF)
# define USE_VSNPRINTF vsnprintf
#elif defined(_WIN32) && !defined(__CYGWIN32__)
# define USE_VSNPRINTF _vsnprintf
#elif defined(linux)
# define USE_VSNPRINTF vsnprintf
#endif
 
GUTF8String &
GUTF8String::format(const char fmt[], ... )
{
  va_list args;
  va_start(args, fmt);
  return init(GStringRep::UTF8::create(fmt,args));
}

GP<GStringRep>
GStringRep::UTF8::create_format(const char fmt[],...)
{
  va_list args;
  va_start(args, fmt);
  return create(fmt,args);
}

GP<GStringRep>
GStringRep::vformat(va_list args) const
{
  GP<GStringRep> retval;
  if(size)
  {
    char const * const fmt=data;
    int buflen=32768;
    char *buffer;
    GPBuffer<char> gbuffer(buffer,buflen);
    ChangeLocale locale(LC_NUMERIC,(isNative()?0:"C"));
    // Format string
#ifdef USE_VSNPRINTF
    while(USE_VSNPRINTF(buffer, buflen, fmt, args)<0)
      {
        gbuffer.resize(0);
        gbuffer.resize(buflen+32768);
      }
    va_end(args);
#else
    buffer[buflen-1] = 0;
    vsprintf(buffer, fmt, args);
    va_end(args);
    if (buffer[buflen-1])
      {
        // This isn't as fatal since it is on the stack, but we
        // definitely should stop the current operation.
        G_THROW( ERR_MSG("GString.overwrite") );
      }
#endif
    retval=strdup((const char *)buffer);
  }
  // Go altering the string
  return retval;
}

int 
GStringRep::search(char c, int from) const
{
  if (from<0)
    from += size;
  int retval=(-1);
  if (from>=0 && from<size)
  {
    char const *const s = strchr(data+from,c);
    if(s)
      retval=(int)((size_t)s-(size_t)data);
  }
  return retval;
}

int 
GStringRep::search(char const *ptr, int from) const
{
  if(from<0)
  {
    from+=size;
    if(from<0)
      G_THROW( ERR_MSG("GString.bad_subscript") );
  }
  int retval=(-1);
  if (from>=0 && from<size)
  {
    char const *const s = strstr(data+from,ptr);
    if(s)
      retval=(int)((size_t)s-(size_t)data);
  }
  return retval;
}

int 
GStringRep::rsearch(char c, int from) const
{
  if(from<0)
  {
    from+=size;
    if(from<0)
      G_THROW( ERR_MSG("GString.bad_subscript") );
  }
  int retval=(-1);
  if ((from>=0) && (from<size))
  {
    char const *const s = strrchr(data+from,c);
    if(s)
      retval=(int)((size_t)s-(size_t)data);
  }
  return retval;
}

int 
GStringRep::rsearch(char const *ptr, int from) const
{
  if(from<0)
  {
    from+=size;
    if(from<0)
      G_THROW( ERR_MSG("GString.bad_subscript") );
  }
  int retval=(-1);
  for(int loc=from;(loc=search(ptr,loc)) >= 0;++loc)
    retval=loc;
  return retval;
}

int
GStringRep::contains(const char accept[],int from) const
{
  if(from<0)
  {
    from+=size;
    if(from<0)
      G_THROW( ERR_MSG("GString.bad_subscript") );
  }
  int retval=(-1);
  if (accept && accept[0] && from>=0 && from<size)
  {
    char const * const src = data+from;
    char const *ptr=strpbrk(src,accept);
    if(ptr)
    {
      retval=(int)(ptr-src)+from;
    }
  }
  return retval;
}

int
GStringRep::rcontains(const char accept[],int from) const
{
  int retval=(-1);
  while((from=contains(accept,from)) >= 0)
  {
    retval=from++;
  }
  return retval;
}

bool
GBaseString::is_int(void) const
{
  bool isLong=!!ptr;
  if(isLong)
  {
    int endpos;
    (*this)->toLong(0,endpos);
    if(endpos>=0)
    {
      isLong=((*this)->nextNonSpace(endpos) == (int)length());
    }
  }
  return isLong;
}

bool
GBaseString::is_float(void) const
{
  bool isDouble=!!ptr;
  if(isDouble)
  {
    int endpos;
    (*this)->toDouble(0,endpos);
    if(endpos>=0)
    {
      isDouble=((*this)->nextNonSpace(endpos) == (int)length());
    }
  }
  return isDouble;
}

unsigned int 
hash(const GBaseString &str)
{
  unsigned int x = 0;
  const char *s = (const char*)str;
  while (*s) 
    x = x ^ (x<<6) ^ (unsigned char)(*s++);
  return x;
}

void 
GBaseString::throw_illegal_subscript()
{
  G_THROW( ERR_MSG("GString.bad_subscript") );
}

unsigned char *
GStringRep::UTF8::UCS4toString(
  const uint32_t w0,unsigned char *ptr, mbstate_t *) const
{
  return UCS4toUTF8(w0,ptr);
}

int
GStringRep::UTF8::ncopy(wchar_t * const buf, const int buflen ) const
{
  int retval=(-1);
  if(buf && buflen)
    {
      buf[0]=0;
      if(data[0])
	{
          const size_t length=strlen(data);
          const unsigned char * const eptr=(const unsigned char *)(data+length);
	  wchar_t *r=buf;
	  wchar_t const * const rend=buf+buflen;
          for(const unsigned char *s=(const unsigned char *)data;
              (r<rend)&&(s<eptr)&&*s;)
            {
              const uint32_t w0=UTF8toUCS4(s,eptr);
              uint16_t w1;
              uint16_t w2=1;
              for(int count=(sizeof(wchar_t)==sizeof(w1))
                    ?UCS4toUTF16(w0,w1,w2):1;
                  count&&(r<rend);
                  --count,w1=w2,++r)
		{
		  r[0]=(sizeof(wchar_t) == sizeof(w1))?(wchar_t)w1:(wchar_t)w0;
		}
            }
	  if(r<rend)
            {
              r[0]=0;
              retval=((size_t)r-(size_t)buf)/sizeof(wchar_t);
            }
	}
      else
	{
	  retval=0;
	}
    }
  return retval;
}

GP<GStringRep> 
GStringRep::UTF8::toNative(const EscapeMode escape) const
{
  GP<GStringRep> retval;
  if(data[0])
  {
    const size_t length=strlen(data);
    const unsigned char * const eptr=(const unsigned char *)(data+length);
    unsigned char *buf;
    GPBuffer<unsigned char> gbuf(buf,12*length+12); 
    unsigned char *r=buf;
    mbstate_t ps;
    memset(&ps,0,sizeof(mbstate_t));
    for(const unsigned char *s=(const unsigned char *)data;(s<eptr)&& *s;)
      {
        const unsigned char * const s0 = s;
        const uint32_t w0=UTF8toUCS4(s,eptr);
        if (s == s0)
          {
            s += 1;
            *r++ = '?';
          }
        else
          {
            const unsigned char * const r0 = r;
            r=UCS4toNative(w0,r,&ps);
            if(r == r0)
              {
                if (escape == IS_ESCAPED)
                  {
                    sprintf((char *)r,"&#%lu;",(unsigned long)w0);
                    r += strlen((char *)r);
                  }
                else
                  {
                    *r++ = '?';
                  }
              }
          }
      }
    r[0]=0;
    retval = NATIVE_CREATE( (const char *)buf );
  } 
  else
  {
    retval = NATIVE_CREATE( (unsigned int)0 );
  }
  return retval;
}

GP<GStringRep>
GStringRep::UTF8::toUTF8(const bool nothrow) const
{
  if(!nothrow)
    G_THROW( ERR_MSG("GStringRep.UTF8ToUTF8") );
  return const_cast<GStringRep::UTF8 *>(this);
}

// Tests if a string is legally encoded in the current character set.
bool 
GStringRep::UTF8::is_valid(void) const
{
  bool retval=true;
  if(data && size)
  {
    const unsigned char * const eptr=(const unsigned char *)(data+size);
    for(const unsigned char *s=(const unsigned char *)data;(s<eptr)&& *s;)
    {
      const unsigned char * const r=s;
      (void)UTF8toUCS4(s,eptr);
      if(r == s)
      {
        retval=false;
        break;
      }
    }
  }
  return retval;
}

static inline uint32_t
add_char(uint32_t const U, unsigned char const * const r)
{
  uint32_t const C=r[0];
  return ((C|0x3f) == 0xbf)?((U<<6)|(C&0x3f)):0;
}

uint32_t
GStringRep::UTF8toUCS4(
  unsigned char const *&s,void const * const eptr)
{
  uint32_t U=0;
  unsigned char const *r=s;
  if(r < eptr)
  {
    uint32_t const C1=r++[0];
    if(C1&0x80)
    {
      if(r < eptr)
      {
        U=C1;
        if((U=((C1&0x40)?add_char(U,r++):0)))
        {
          if(C1&0x20)
          {
            if(r < eptr)
            {
              if((U=add_char(U,r++)))
              {
                if(C1&0x10)
                {
                  if(r < eptr)
                  {
                    if((U=add_char(U,r++)))
                    {
                      if(C1&0x8)
                      {
                        if(r < eptr)
                        {
                          if((U=add_char(U,r++)))
                          {
                            if(C1&0x4)
                            {
                              if(r < eptr)
                              {
                                if((U=((!(C1&0x2))?(add_char(U,r++)&0x7fffffff):0)))
                                {
                                  s=r;
                                }else
                                {
                                  U=(unsigned int)(-1)-s++[0];
                                }
                              }else
                              {
                                U=0;
                              }
                            }else if((U=((U&0x4000000)?0:(U&0x3ffffff))))
                            {
                              s=r;
                            }
                          }else
                          {
                            U=(unsigned int)(-1)-s++[0];
                          }
                        }else
                        {
                          U=0;
                        }
                      }else if((U=((U&0x200000)?0:(U&0x1fffff))))
                      {
                        s=r;
                      }
                    }else
                    {
                      U=(unsigned int)(-1)-s++[0];
                    }
                  }else
                  {
                    U=0;
                  }
                }else if((U=((U&0x10000)?0:(U&0xffff))))
                {
                  s=r;
                }
              }else
              {
                U=(unsigned int)(-1)-s++[0];
              }
            }else
            {
              U=0;
            }
          }else if((U=((U&0x800)?0:(U&0x7ff))))
          {
            s=r;
          }
        }else
        {
          U=(unsigned int)(-1)-s++[0];
        }
      }else
      {
        U=0;
      }
    }else if((U=C1))
    {
      s=r;
    }
  }
  return U;
}

unsigned char *
GStringRep::UCS4toUTF8(const uint32_t w,unsigned char *ptr)
{
  if(w <= 0x7f)
  {
    *ptr++ = (unsigned char)w;
  }
  else if(w <= 0x7ff)
  {
    *ptr++ = (unsigned char)((w>>6)|0xC0);
    *ptr++ = (unsigned char)((w|0x80)&0xBF);
  }
  else if(w <= 0xFFFF)
  {
    *ptr++ = (unsigned char)((w>>12)|0xE0);
    *ptr++ = (unsigned char)(((w>>6)|0x80)&0xBF);
    *ptr++ = (unsigned char)((w|0x80)&0xBF);
  }
  else if(w <= 0x1FFFFF)
  {
    *ptr++ = (unsigned char)((w>>18)|0xF0);
    *ptr++ = (unsigned char)(((w>>12)|0x80)&0xBF);
    *ptr++ = (unsigned char)(((w>>6)|0x80)&0xBF);
    *ptr++ = (unsigned char)((w|0x80)&0xBF);
  }
  else if(w <= 0x3FFFFFF)
  {
    *ptr++ = (unsigned char)((w>>24)|0xF8);
    *ptr++ = (unsigned char)(((w>>18)|0x80)&0xBF);
    *ptr++ = (unsigned char)(((w>>12)|0x80)&0xBF);
    *ptr++ = (unsigned char)(((w>>6)|0x80)&0xBF);
    *ptr++ = (unsigned char)((w|0x80)&0xBF);
  }
  else if(w <= 0x7FFFFFFF)
  {
    *ptr++ = (unsigned char)((w>>30)|0xFC);
    *ptr++ = (unsigned char)(((w>>24)|0x80)&0xBF);
    *ptr++ = (unsigned char)(((w>>18)|0x80)&0xBF);
    *ptr++ = (unsigned char)(((w>>12)|0x80)&0xBF);
    *ptr++ = (unsigned char)(((w>>6)|0x80)&0xBF);
    *ptr++ = (unsigned char)((w|0x80)&0xBF);
  }
  else
  { 
    *ptr++ = '?';
  }
  return ptr;
}

   // Creates with a concat operation.
GP<GStringRep> 
GStringRep::concat( const char *s1, const GP<GStringRep> &s2) const
{
  GP<GStringRep> retval;
  if(s2)
  {
    retval=toThis(s2);
    if(s1 && s1[0])
    {
      if(retval)
      {
        retval=concat(s1,retval->data);
      }else
      {
        retval=strdup(s1);
      }
    }
  }else if(s1 && s1[0])
  {
    retval=strdup(s1);
  }
  return retval;
}

   // Creates with a concat operation.

GP<GStringRep> 
GStringRep::concat( const GP<GStringRep> &s1,const char *s2) const
{
  GP<GStringRep> retval;
  if(s1)
  {
    retval=toThis(s1);
    if(s2 && s2[0])
    {
      if(retval)
      {
        retval=retval->append(s2);
      }else
      {
        retval=strdup(s2);
      }
    }
  }else if(s2 && s2[0])
  {
    retval=strdup(s2);
  }
  return retval;
}

GP<GStringRep> 
GStringRep::concat(const GP<GStringRep> &s1,const GP<GStringRep> &s2) const
{ 
  GP<GStringRep> retval; 
  if(s1)
  {
    retval=toThis(s1,s2);
    if(retval && s2)
    {
      retval=retval->append(toThis(s2));
    }
  }else if(s2)
  {
    retval=toThis(s2);
  }
  return retval;
}

GStringRep::GStringRep(void)
{
  size=0;
  data=0;
}

GStringRep::~GStringRep()
{
  if(data)
  {
    data[0]=0;
    ::operator delete(data);
  }
  data=0;
}

GStringRep::UTF8::UTF8(void) {}

GStringRep::UTF8::~UTF8() {}

int
GStringRep::cmp(const char *s1,const int len) const
{
  return cmp(data,s1,len);
}

int
GStringRep::cmp(const char *s1, const char *s2,const int len)
{
  return (len
   ?((s1&&s1[0])
      ?((s2&&s2[0])
        ?((len>0)
          ?strncmp(s1,s2,len)
          :strcmp(s1,s2))
        :1)
      :((s2&&s2[0])?(-1):0))
   :0);
}

int 
GStringRep::cmp(const GP<GStringRep> &s1, const GP<GStringRep> &s2,
  const int len )
{
  return (s1?(s1->cmp(s2,len)):cmp(0,(s2?(s2->data):0),len));
}

int 
GStringRep::cmp(const GP<GStringRep> &s1, const char *s2, 
  const int len )
{
  return cmp((s1?s1->data:0),s2,len);
}

int 
GStringRep::cmp(const char *s1, const GP<GStringRep> &s2,
  const int len )
{
  return cmp(s1,(s2?(s2->data):0),len);
}

int
GStringRep::UTF8::cmp(const GP<GStringRep> &s2,const int len) const
{
  int retval;
  if(s2)
  {
    if(s2->isNative())
    {
      GP<GStringRep> r(s2->toUTF8(true));
      if(r)
      {
        retval=GStringRep::cmp(data,r->data,len);
      }else
      {
        retval=-(s2->cmp(toNative(NOT_ESCAPED),len));
      }
    }else
    {
      retval=GStringRep::cmp(data,s2->data,len);
    }
  }else
  { 
    retval=GStringRep::cmp(data,0,len);
  }
  return retval;
} 

int
GStringRep::UTF8::toInt() const
{
  int endpos;
  return (int)toLong(0,endpos);
}

static inline long
Cstrtol(char *data,char **edata, const int base)
{
  GStringRep::ChangeLocale locale(LC_NUMERIC,"C");
  while (data && *data==' ') data++;
  return strtol(data,edata,base);
}

long 
GStringRep::UTF8::toLong(
  const int pos, int &endpos, const int base) const
{
  char *edata=0;
  long retval=Cstrtol(data+pos,&edata, base);
  if(edata)
  {
    endpos=edata-data;
  }else
  {
    GP<GStringRep> ptr = GStringRep::UTF8::create();
    endpos=(-1);
    ptr=ptr->strdup(data+pos);
    if(ptr)
      ptr=ptr->toNative(NOT_ESCAPED);
    if(ptr)
    {
      int xendpos;
      retval=ptr->toLong(0,xendpos,base);
      if(xendpos> 0)
      {
        endpos=(int)size;
        ptr=ptr->strdup(data+xendpos);
        if(ptr)
        {
          ptr=ptr->toUTF8(true);
          if(ptr)
          {
            endpos-=(int)(ptr->size);
          }
        }
      }
    }
  }
  return retval;
}

static inline unsigned long
Cstrtoul(char *data,char **edata, const int base)
{
  GStringRep::ChangeLocale locale(LC_NUMERIC,"C");
  while (data && *data==' ') data++;
  return strtoul(data,edata,base);
}

unsigned long 
GStringRep::UTF8::toULong(
  const int pos, int &endpos, const int base) const
{
  char *edata=0;
  unsigned long retval=Cstrtoul(data+pos,&edata, base);
  if(edata)
  {
    endpos=edata-data;
  }else
  {
    GP<GStringRep> ptr = GStringRep::UTF8::create();
    endpos=(-1);
    ptr=ptr->strdup(data+pos);
    if(ptr)
      ptr=ptr->toNative(NOT_ESCAPED);
    if(ptr)
    {
      int xendpos;
      retval=ptr->toULong(0,xendpos,base);
      if(xendpos> 0)
      {
        endpos=(int)size;
        ptr=ptr->strdup(data+xendpos);
        if(ptr)
        {
          ptr=ptr->toUTF8(true);
          if(ptr)
          {
            endpos-=(int)(ptr->size);
          }
        }
      }
    }
  }
  return retval;
}

static inline double
Cstrtod(char *data,char **edata)
{
  GStringRep::ChangeLocale locale(LC_NUMERIC,"C");
  while (data && *data==' ') data++;
  return strtod(data,edata);
}

double
GStringRep::UTF8::toDouble(const int pos, int &endpos) const
{
  char *edata=0;
  double retval=Cstrtod(data+pos,&edata);
  if(edata)
  {
    endpos=edata-data;
  }else
  {
    GP<GStringRep> ptr = GStringRep::UTF8::create();
    endpos=(-1);
    ptr=ptr->strdup(data+pos);
    if(ptr)
      ptr=ptr->toNative(NOT_ESCAPED);
    if(ptr)
    {
      int xendpos;
      retval=ptr->toDouble(0,xendpos);
      if(xendpos >= 0)
      {
        endpos=(int)size;
        ptr=ptr->strdup(data+xendpos);
        if(ptr)
        {
          ptr=ptr->toUTF8(true);
          if(ptr)
          {
            endpos-=(int)(ptr->size);
          }
        }
      }
    }
  }
  return retval;
}

int 
GStringRep::getUCS4(uint32_t &w, const int from) const
{
  int retval;
  if(from>=size)
  {
    w=0;
    retval=size;
  }else if(from<0)
  {
    w=(unsigned int)(-1);
    retval=(-1);
  }else
  {
    const char *source=data+from;
    w=getValidUCS4(source);
    retval=(int)((size_t)source-(size_t)data);
  } 
  return retval;
}


uint32_t
GStringRep::UTF8::getValidUCS4(const char *&source) const
{
  return GStringRep::UTF8toUCS4((const unsigned char *&)source,data+size);
}

int
GStringRep::nextNonSpace(const int from,const int len) const
{
  return nextCharType(giswspace,from,len,true);
}

int
GStringRep::nextSpace(const int from,const int len) const
{
  return nextCharType(giswspace,from,len,false);
}

int
GStringRep::nextChar(const int from) const
{
  char const * xptr=data+from;
  (void)getValidUCS4(xptr);
  return (int)((size_t)xptr-(size_t)data);
}

int 
GStringRep::firstEndSpace(int from,const int len) const
{
  const int xsize=(len<0)?size:(from+len);
  const int ysize=(size<xsize)?size:xsize;
  int retval=ysize;
  while(from<ysize)
  {
    from=nextNonSpace(from,ysize-from);
    if(from < size)
    {
      const int r=nextSpace(from,ysize-from);
      // If a character isn't legal, then it will return
      // tru for both nextSpace and nextNonSpace.
      if(r == from)
      {
        from++;
      }else
      {
        from=retval=r;
      }
    }
  }
  return retval;
}

int
GStringRep::UCS4toUTF16(
  const uint32_t w,uint16_t &w1, uint16_t &w2)
{
  int retval;
  if(w<0x10000)
  {
    w1=(uint16_t)w;
    w2=0;
    retval=1;
  }else
  {
    w1=(uint16_t)((((w-0x10000)>>10)&0x3ff)+0xD800);
    w2=(uint16_t)((w&0x3ff)+0xDC00);
    retval=2;
  }
  return retval;
}

int
GStringRep::UTF16toUCS4(
  uint32_t &U,uint16_t const * const s,void const * const eptr)
{
  int retval=0;
  U=0;
  uint16_t const * const r=s+1;
  if(r <= eptr)
  {
    uint32_t const W1=s[0];
    if((W1<0xD800)||(W1>0xDFFF))
    {
      if((U=W1))
      {
        retval=1;
      }
    }else if(W1<=0xDBFF)
    {
      uint16_t const * const rr=r+1;
      if(rr <= eptr)
      {
        uint32_t const W2=s[1];
        if(((W2>=0xDC00)||(W2<=0xDFFF))&&((U=(0x10000+((W1&0x3ff)<<10))|(W2&0x3ff))))
        {
          retval=2;
        }else
        {
          retval=(-1);
        }
      }
    }
  }
  return retval;
}


//bcr

GUTF8String&
GUTF8String::operator+= (char ch)
{
  return init(
    GStringRep::UTF8::create((const char*)*this,
    GStringRep::UTF8::create(&ch,0,1)));
}

GUTF8String&
GUTF8String::operator+= (const char *str)
{
  return init(GStringRep::UTF8::create(*this,str));
}

GUTF8String&
GUTF8String::operator+= (const GBaseString &str)
{
  return init(GStringRep::UTF8::create(*this,str));
}

GUTF8String
GUTF8String::substr(int from, int len) const
{ return GUTF8String(*this, from, len); }

GUTF8String
GUTF8String::operator+(const GBaseString &s2) const
{ return GStringRep::UTF8::create(*this,s2); }

GUTF8String
GUTF8String::operator+(const GUTF8String &s2) const
{ return GStringRep::UTF8::create(*this,s2); }

GUTF8String
GUTF8String::operator+(const char    *s2) const
{ return GStringRep::UTF8::create(*this,s2); }

char *
GUTF8String::getbuf(int n)
{
  if(ptr)
    init((*this)->getbuf(n));
  else if(n>0)
    init(GStringRep::UTF8::create(n));
  else
    init(0);
  return ptr?((*this)->data):0;
}

void 
GUTF8String::setat(const int n, const char ch)
{
  if((!n)&&(!ptr))
  {
    init(GStringRep::UTF8::create(&ch,0,1));
  }else
  {
    init((*this)->setat(CheckSubscript(n),ch));
  }
}

GP<GStringRep>
GStringRep::UTF8ToNative( const char *s, const EscapeMode escape )
{
  return GStringRep::UTF8::create(s)->toNative(escape);
}

GUTF8String::GUTF8String(const char dat)
{ init(GStringRep::UTF8::create(&dat,0,1)); }

GUTF8String::GUTF8String(const GUTF8String &fmt, va_list &args)
{ 
  if (fmt.ptr)
    init(fmt->vformat(args));
  else 
    init(fmt); 
}

GUTF8String::GUTF8String(const char *str)
{ init(GStringRep::UTF8::create(str)); }

GUTF8String::GUTF8String(const unsigned char *str)
{ init(GStringRep::UTF8::create((const char *)str)); }

GUTF8String::GUTF8String(const uint16_t *str)
{ init(GStringRep::UTF8::create(str,0,-1)); }

GUTF8String::GUTF8String(const uint32_t *str)
{ init(GStringRep::UTF8::create(str,0,-1)); }

GUTF8String::GUTF8String(const char *dat, unsigned int len)
{ init(GStringRep::UTF8::create(dat,0,((int)len<0)?(-1):(int)len)); }

GUTF8String::GUTF8String(const uint16_t *dat, unsigned int len)
{ init(GStringRep::UTF8::create(dat,0,((int)len<0)?(-1):(int)len)); }

GUTF8String::GUTF8String(const uint32_t *dat, unsigned int len)
{ init(GStringRep::UTF8::create(dat,0,((int)len<0)?(-1):(int)len)); }

GUTF8String::GUTF8String(const GBaseString &gs, int from, int len)
{ init(GStringRep::UTF8::create(gs,from,((int)len<0)?(-1):(int)len)); }

GUTF8String::GUTF8String(const int number)
{ init(GStringRep::UTF8::create_format("%d",number)); }

GUTF8String::GUTF8String(const double number)
{ init(GStringRep::UTF8::create_format("%f",number)); }

GUTF8String& GUTF8String::operator= (const char str)
{ return init(GStringRep::UTF8::create(&str,0,1)); }

GUTF8String& GUTF8String::operator= (const char *str)
{ return init(GStringRep::UTF8::create(str)); }

GUTF8String GBaseString::operator+(const GUTF8String &s2) const
{ return GStringRep::UTF8::create(*this,s2); }

#if HAS_WCHAR
GUTF8String
GNativeString::operator+(const GUTF8String &s2) const
{
  if (ptr)
    return GStringRep::UTF8::create((*this)->toUTF8(true),s2);
  else
    return GStringRep::UTF8::create((*this),s2);
}
#endif

GUTF8String
GUTF8String::operator+(const GNativeString &s2) const
{
  GP<GStringRep> g = s2;
  if (s2.ptr)
    g = s2->toUTF8(true);
  return GStringRep::UTF8::create(*this,g);
}

GUTF8String
operator+(const char    *s1, const GUTF8String &s2)
{ return GStringRep::UTF8::create(s1,s2); }

#if HAS_WCHAR
GNativeString
operator+(const char    *s1, const GNativeString &s2)
{ return GStringRep::Native::create(s1,s2); }

GNativeString&
GNativeString::operator+= (char ch)
{
  char s[2]; s[0]=ch; s[1]=0;
  return init(GStringRep::Native::create((const char*)*this, s));
}

GNativeString&
GNativeString::operator+= (const char *str)
{
  return init(GStringRep::Native::create(*this,str));
}

GNativeString&
GNativeString::operator+= (const GBaseString &str)
{
  return init(GStringRep::Native::create(*this,str));
}

GNativeString
GNativeString::operator+(const GBaseString &s2) const
{ return GStringRep::Native::create(*this,s2); }

GNativeString
GNativeString::operator+(const GNativeString &s2) const
{ return GStringRep::Native::create(*this,s2); }

GNativeString
GNativeString::operator+(const char    *s2) const
{ return GStringRep::Native::create(*this,s2); }

char *
GNativeString::getbuf(int n)
{
  if(ptr)
    init((*this)->getbuf(n));
  else if(n>0)
    init(GStringRep::Native::create(n));
  else
    init(0);
  return ptr?((*this)->data):0;
}

void
GNativeString::setat(const int n, const char ch)
{
  if((!n)&&(!ptr))
  {
    init(GStringRep::Native::create(&ch,0,1));
  }else
  {
    init((*this)->setat(CheckSubscript(n),ch));
  }
}

#endif


#ifdef HAVE_NAMESPACES
}
# ifndef NOT_USING_DJVU_NAMESPACE
using namespace DJVU;
# endif
#endif
