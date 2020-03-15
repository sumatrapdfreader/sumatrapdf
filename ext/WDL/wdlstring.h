/*
    WDL - wdlstring.h
    Copyright (C) 2005 and later, Cockos Incorporated
  
    This software is provided 'as-is', without any express or implied
    warranty.  In no event will the authors be held liable for any damages
    arising from the use of this software.

    Permission is granted to anyone to use this software for any purpose,
    including commercial applications, and to alter it and redistribute it
    freely, subject to the following restrictions:

    1. The origin of this software must not be misrepresented; you must not
       claim that you wrote the original software. If you use this software
       in a product, an acknowledgment in the product documentation would be
       appreciated but is not required.
    2. Altered source versions must be plainly marked as such, and must not be
       misrepresented as being the original software.
    3. This notice may not be removed or altered from any source distribution.
      
*/

/*

  This file provides a simple class for variable-length string manipulation.
  It provides only the simplest features, and does not do anything confusing like
  operator overloading. It uses a WDL_HeapBuf for internal storage.

  Actually: there are WDL_String and WDL_FastString -- the latter's Get() returns const char, and tracks
  the length of the string, which is often faster. Because of this, you are not permitted to directly modify
  the buffer returned by Get().

  
*/

#ifndef _WDL_STRING_H_
#define _WDL_STRING_H_

#include "heapbuf.h"
#include <stdio.h>
#include <stdarg.h>

#ifndef WDL_STRING_IMPL_ONLY
class WDL_String
{
  public:
  #ifdef WDL_STRING_INTF_ONLY
    void Set(const char *str, int maxlen=0);
    void Set(const WDL_String *str, int maxlen=0);
    void Append(const char *str, int maxlen=0);
    void Append(const WDL_String *str, int maxlen=0);
    void DeleteSub(int position, int len);
    void Insert(const char *str, int position, int maxlen=0);
    void Insert(const WDL_String *str, int position, int maxlen=0);
    #ifdef WDL_STRING_FASTSUB_DEFINED
    bool SetLen(int length, bool resizeDown=false, char fillchar=' '); // returns true on success
    #else
    bool SetLen(int length, bool resizeDown=false); // returns true on success
    #endif
    void Ellipsize(int minlen, int maxlen);
    const char *get_filepart() const; // returns whole string if no dir chars
    const char *get_fileext() const; // returns ".ext" or end of string "" if no extension
    bool remove_fileext(); // returns true if extension was removed
    char remove_filepart(bool keepTrailingSlash=false); // returns dir character used, or zero if string emptied
    int remove_trailing_dirchars(); // returns trailing dirchar count removed, will not convert "/" into ""

    void SetAppendFormattedArgs(bool append, int maxlen, const char* fmt, va_list arglist);
    void WDL_VARARG_WARN(printf,3,4) SetFormatted(int maxlen, const char *fmt, ...);
    void WDL_VARARG_WARN(printf,3,4) AppendFormatted(int maxlen, const char *fmt, ...);
  #endif

    const char *Get() const { return m_hb.GetSize()?(char*)m_hb.Get():""; }

  #ifdef WDL_STRING_FASTSUB_DEFINED
    int GetLength() const { int a = m_hb.GetSize(); return a>0?a-1:0; }

    // for binary-safe manipulations
    void SetRaw(const char *str, int len) { __doSet(0,str,len,0); }
    void AppendRaw(const char *str, int len) { __doSet(GetLength(),str,len,0); }
    void InsertRaw(const char *str, int position, int ilen)
    {
      const int srclen = GetLength();
      if (position<0) position=0;
      else if (position>srclen) position=srclen;
      if (ilen>0) __doSet(position,str,ilen,srclen-position);
    }

  #else
    char *Get()
    {
      if (m_hb.GetSize()) return (char *)m_hb.Get();
      static char c; c=0; return &c; // don't return "", in case it gets written to.
    }
    int GetLength() const { return m_hb.GetSize()?(int)strlen((const char*)m_hb.Get()):0; }
  #endif

    explicit WDL_String(int hbgran) : m_hb(hbgran WDL_HEAPBUF_TRACEPARM("WDL_String(4)")) { }
    explicit WDL_String(const char *initial=NULL, int initial_len=0) : m_hb(128 WDL_HEAPBUF_TRACEPARM("WDL_String"))
    {
      if (initial) Set(initial,initial_len);
    }
    WDL_String(const WDL_String &s) : m_hb(128 WDL_HEAPBUF_TRACEPARM("WDL_String(2)")) { Set(&s); }
    WDL_String(const WDL_String *s) : m_hb(128 WDL_HEAPBUF_TRACEPARM("WDL_String(3)")) { if (s && s != this) Set(s); }
    ~WDL_String() { }
#endif // ! WDL_STRING_IMPL_ONLY

#ifndef WDL_STRING_INTF_ONLY
  #ifdef WDL_STRING_IMPL_ONLY
    #define WDL_STRING_FUNCPREFIX WDL_String::
    #define WDL_STRING_DEFPARM(x)
  #else
    #define WDL_STRING_FUNCPREFIX 
    #define WDL_STRING_DEFPARM(x) =(x)
  #endif

    void WDL_STRING_FUNCPREFIX Set(const char *str, int maxlen WDL_STRING_DEFPARM(0))
    {
      int s=0;
      if (str)
      {
        if (maxlen>0) while (s < maxlen && str[s]) s++;
        else s=(int)strlen(str);   
      }
      __doSet(0,str,s,0);
    }

    void WDL_STRING_FUNCPREFIX Set(const WDL_String *str, int maxlen WDL_STRING_DEFPARM(0))
    {
      #ifdef WDL_STRING_FASTSUB_DEFINED
        int s = str ? str->GetLength() : 0;
        if (maxlen>0 && maxlen<s) s=maxlen;

        __doSet(0,str?str->Get():NULL,s,0);
      #else
        Set(str?str->Get():NULL, maxlen); // might be faster: "partial" strlen
      #endif
    }

    void WDL_STRING_FUNCPREFIX Append(const char *str, int maxlen WDL_STRING_DEFPARM(0))
    {
      int s=0;
      if (str)
      {
        if (maxlen>0) while (s < maxlen && str[s]) s++;
        else s=(int)strlen(str);
      }

      __doSet(GetLength(),str,s,0);
    }

    void WDL_STRING_FUNCPREFIX Append(const WDL_String *str, int maxlen WDL_STRING_DEFPARM(0))
    {
      #ifdef WDL_STRING_FASTSUB_DEFINED
        int s = str ? str->GetLength() : 0;
        if (maxlen>0 && maxlen<s) s=maxlen;

        __doSet(GetLength(),str?str->Get():NULL,s,0);
      #else
        Append(str?str->Get():NULL, maxlen); // might be faster: "partial" strlen
      #endif
    }

    void WDL_STRING_FUNCPREFIX DeleteSub(int position, int len)
    {
      int l=m_hb.GetSize()-1;
      char *p=(char *)m_hb.Get();
      if (l<0 || !*p || position < 0 || position >= l) return;
      if (position+len > l) len=l-position;
      if (len>0)
      {
        memmove(p+position,p+position+len,l-position-len+1);
        m_hb.Resize(l+1-len,false);
      }
    }

    void WDL_STRING_FUNCPREFIX Insert(const char *str, int position, int maxlen WDL_STRING_DEFPARM(0))
    {
      int ilen=0;
      if (str)
      {
        if (maxlen>0) while (ilen < maxlen && str[ilen]) ilen++;
        else ilen=(int)strlen(str);
      }

      const int srclen = GetLength();
      if (position<0) position=0;
      else if (position>srclen) position=srclen;
      if (ilen>0) __doSet(position,str,ilen,srclen-position);
    }

    void WDL_STRING_FUNCPREFIX Insert(const WDL_String *str, int position, int maxlen WDL_STRING_DEFPARM(0))
    {
      #ifdef WDL_STRING_FASTSUB_DEFINED
        int ilen = str ? str->GetLength() : 0;
        if (maxlen>0 && maxlen<ilen) ilen=maxlen;

        const int srclen = m_hb.GetSize()>0 ? m_hb.GetSize()-1 : 0;
        if (position<0) position=0;
        else if (position>srclen) position=srclen;
        if (ilen>0) __doSet(position,str->Get(),ilen,srclen-position);
      #else
        Insert(str?str->Get():NULL, position, maxlen); // might be faster: "partial" strlen
      #endif
    }

    bool WDL_STRING_FUNCPREFIX SetLen(int length, bool resizeDown WDL_STRING_DEFPARM(false)
      #ifdef WDL_STRING_FASTSUB_DEFINED
        , char fillchar WDL_STRING_DEFPARM(' ')
      #endif
        )
    {                       
      #ifdef WDL_STRING_FASTSUB_DEFINED
      int osz = m_hb.GetSize()-1;
      if (osz<0)osz=0;
      #endif
      if (length < 0) length=0;
      char *b=(char*)m_hb.ResizeOK(length+1,resizeDown);
      if (b) 
      {
        #ifdef WDL_STRING_FASTSUB_DEFINED
          const int fill = length-osz;
          if (fill > 0) memset(b+osz,fillchar,fill);
        #endif
        b[length]=0;
        return true;
      }
      return false;
    }

    void WDL_STRING_FUNCPREFIX SetAppendFormattedArgs(bool append, int maxlen, const char* fmt, va_list arglist) 
    {
      int offs = append ? GetLength() : 0;
      char *b= (char*) m_hb.ResizeOK(offs+maxlen+1,false);
      
      if (!b) return;

      b+=offs;

      #ifdef _WIN32
        int written = _vsnprintf(b, maxlen+1, fmt, arglist);
        if (written < 0 || written>=maxlen) b[written=b[0]?maxlen:0]=0;
      #else
        int written = vsnprintf(b, maxlen+1, fmt, arglist);
        if (written > maxlen) written=maxlen;
      #endif

      m_hb.Resize(offs + written + 1,false);
    }

    void WDL_VARARG_WARN(printf,3,4) WDL_STRING_FUNCPREFIX SetFormatted(int maxlen, const char *fmt, ...) 
    {
      va_list arglist;
      va_start(arglist, fmt);
      SetAppendFormattedArgs(false,maxlen,fmt,arglist);
      va_end(arglist);
    }

    void WDL_VARARG_WARN(printf,3,4) WDL_STRING_FUNCPREFIX AppendFormatted(int maxlen, const char *fmt, ...) 
    {
      va_list arglist;
      va_start(arglist, fmt);
      SetAppendFormattedArgs(true,maxlen,fmt,arglist);
      va_end(arglist);
    }

    void WDL_STRING_FUNCPREFIX Ellipsize(int minlen, int maxlen)
    {
      if (maxlen >= 4 && m_hb.GetSize() && GetLength() > maxlen) 
      {
        if (minlen<0) minlen=0;
        char *b = (char *)m_hb.Get();
        int i;
        for (i = maxlen-4; i >= minlen; --i) 
        {
          if (b[i] == ' ') 
          {
            memcpy(b+i, "...",4);
            m_hb.Resize(i+4,false);
            break;
          }
        }
        if (i < minlen && maxlen >= 4) 
        {
          memcpy(b+maxlen-4, "...",4);    
          m_hb.Resize(maxlen,false);
        }
      }
    }
    const char * WDL_STRING_FUNCPREFIX get_filepart() const // returns whole string if no dir chars
    {
      const char *s = Get();
      const char *p = s + GetLength() - 1;
      while (p >= s && !WDL_IS_DIRCHAR(*p)) --p;
      return p + 1;
    }
    const char * WDL_STRING_FUNCPREFIX get_fileext() const // returns ".ext" or end of string "" if no extension
    {
      const char *s = Get();
      const char *endp = s + GetLength();
      const char *p = endp - 1;
      while (p >= s && !WDL_IS_DIRCHAR(*p))
      {
        if (*p == '.') return p;
        --p;
      }
      return endp;
    }
    bool WDL_STRING_FUNCPREFIX remove_fileext() // returns true if extension was removed
    {
      const char *str = Get();
      int pos = GetLength() - 1;
      while (pos >= 0)
      {
        char c = str[pos];
        if (WDL_IS_DIRCHAR(c)) break;
        if (c == '.')
        {
          SetLen(pos);
          return true;
        }
        --pos;
      }
      return false;
    }

    char WDL_STRING_FUNCPREFIX remove_filepart(bool keepTrailingSlash WDL_STRING_DEFPARM(false)) // returns directory character used, or 0 if string emptied
    {
      char rv=0;
      const char *str = Get();
      int pos = GetLength() - 1;
      while (pos > 0)
      {
        char c = str[pos];
        if (WDL_IS_DIRCHAR(c)) 
        {
          rv=c;
          if (keepTrailingSlash) ++pos;
          break;
        }
        --pos;
      }
      SetLen(pos);
      return rv;
    }

    int WDL_STRING_FUNCPREFIX remove_trailing_dirchars() // returns trailing dirchar count removed
    {
      int cnt = 0;
      const char *str = Get();
      const int l = GetLength()-1;
      while (cnt < l)
      {
        char c = str[l - cnt];
        if (!WDL_IS_DIRCHAR(c)) break;
        ++cnt;
      }
      if (cnt > 0) SetLen(l + 1 - cnt);
      return cnt;
    }
#ifndef WDL_STRING_IMPL_ONLY
  private:
#endif
    void WDL_STRING_FUNCPREFIX __doSet(int offs, const char *str, int len, int trailkeep)
    {   
      // if non-empty, or (empty and allocated and Set() rather than append/insert), then allow update, otherwise do nothing
      if (len==0 && !trailkeep && !offs)
      {
        #ifdef WDL_STRING_FREE_ON_CLEAR
          m_hb.Resize(0,true);
        #else
          char *p = (char *)m_hb.Resize(1,false);
          if (p) *p=0;
        #endif
      }
      else if (len>0 && offs >= 0) 
      {
        const int oldsz = m_hb.GetSize();
        const int newsz=offs+len+trailkeep+1;
        const int growamt = newsz-oldsz;
        if (growamt > 0)
        {
          const char *oldb = (const char *)m_hb.Get();
          const char *newb = (const char *)m_hb.Resize(newsz,false); // resize up if necessary

          // in case str overlaps with input, keep it valid
          if (str && newb != oldb && str >= oldb && str < oldb+oldsz) str = newb + (str - oldb);
        }

        if (m_hb.GetSize() >= newsz)
        {
          char *newbuf = (char *)m_hb.Get();
          if (trailkeep>0) memmove(newbuf+offs+len,newbuf+offs,trailkeep);
          if (str) memmove(newbuf+offs,str,len);
          newbuf[newsz-1]=0;

          // resize down if necessary
          if (growamt < 0) m_hb.Resize(newsz,false);
        }
      }
    }

  #undef WDL_STRING_FUNCPREFIX
  #undef WDL_STRING_DEFPARM
#endif // ! WDL_STRING_INTF_ONLY

#ifndef WDL_STRING_IMPL_ONLY

  private:
    #ifdef WDL_STRING_INTF_ONLY
      void __doSet(int offs, const char *str, int len, int trailkeep);
    #endif

    WDL_HeapBuf m_hb;
};
#endif

#ifndef WDL_STRING_FASTSUB_DEFINED
#undef _WDL_STRING_H_
#define WDL_STRING_FASTSUB_DEFINED
#define WDL_String WDL_FastString
#include "wdlstring.h"
#undef WDL_STRING_FASTSUB_DEFINED
#undef WDL_String
#endif

#endif

