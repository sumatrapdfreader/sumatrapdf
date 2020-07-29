/*
    WDL - wdlcstring.h
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
C string manipulation utilities -- [v]snprintf for Win32, also snprintf_append, lstrcatn, etc
  */
#ifndef _WDL_CSTRING_H_
#define _WDL_CSTRING_H_

#include <stdarg.h>
#include <stdio.h>
#include <ctype.h>

#include "wdltypes.h"

#ifdef _WDL_CSTRING_IMPL_ONLY_
  #ifdef _WDL_CSTRING_IF_ONLY_
    #undef _WDL_CSTRING_IF_ONLY_
  #endif
  #define _WDL_CSTRING_PREFIX 
#else
  #define _WDL_CSTRING_PREFIX static WDL_STATICFUNC_UNUSED
#endif



#if defined(_WIN32) && defined(_MSC_VER)
  // provide snprintf()/vsnprintf() for win32 -- note that these have no way of knowing
  // what the amount written was, code should(must) be written to not depend on this.
  #ifdef snprintf
  #undef snprintf
  #endif
  #define snprintf WDL_snprintf

  #ifdef vsnprintf
  #undef vsnprintf
  #endif
  #define vsnprintf WDL_vsnprintf

#endif // win32 snprintf/vsnprintf

// use wdlcstring.h's lstrcpyn_safe rather than the real lstrcpyn.
#ifdef _WIN32
  #ifdef lstrcpyn
  #undef lstrcpyn
  #endif
  #define lstrcpyn lstrcpyn_safe
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WDL_CSTRING_IF_ONLY_

  void lstrcpyn_safe(char *o, const char *in, INT_PTR count);
  void lstrcatn(char *o, const char *in, INT_PTR count);
  void WDL_VARARG_WARN(printf,3,4) snprintf_append(char *o, INT_PTR count, const char *format, ...);
  void vsnprintf_append(char *o, INT_PTR count, const char *format, va_list va);

  const char *WDL_get_filepart(const char *str); // returns whole string if no dir chars
  const char *WDL_get_fileext(const char *str); // returns ".ext" or end of string "" if no extension
  char *WDL_remove_fileext(char *str); // returns pointer to "ext" if ".ext" was removed (zero-d dot), or NULL
  char WDL_remove_filepart(char *str); // returns dir character that was zeroed, or 0 if new string is empty
  int WDL_remove_trailing_dirchars(char *str); // returns trailing dirchar count removed, will not convert "/" into ""
  size_t WDL_remove_trailing_crlf(char *str); // returns new length
  size_t WDL_remove_trailing_whitespace(char *str); // returns new length, removes crlf space tab
  const char *WDL_sanitize_ini_key_start(const char *p); // used for sanitizing the start of the "key" parameter to Write/GetPrivateProfile*. note does not fully santiize

  char *WDL_remove_trailing_decimal_zeros(char *str, unsigned int keep); // returns pointer to decimal point or end of string. removes final zeros after final decimal point only, keep=0 makes min length X, keep=1 X., keep=2 X.0, keep=3 X.00 etc, and also treats commas as decimal points

  #if defined(_WIN32) && defined(_MSC_VER)
    void WDL_vsnprintf(char *o, size_t count, const char *format, va_list args);
    void WDL_VARARG_WARN(printf,3,4) WDL_snprintf(char *o, size_t count, const char *format, ...);
  #endif

  int WDL_strcmp_logical(const char *s1, const char *s2, int case_sensitive);
  const char *WDL_stristr(const char* a, const char* b);
#else


  #if defined(_WIN32) && defined(_MSC_VER)

    _WDL_CSTRING_PREFIX void WDL_vsnprintf(char *o, size_t count, const char *format, va_list args)
    {
      if (count>0)
      {
        int rv;
        o[0]=0;
        rv=_vsnprintf(o,count,format,args); // returns -1  if over, and does not null terminate, ugh
        if (rv < 0 || rv>=(int)count-1) o[count-1]=0;
      }
    }
    _WDL_CSTRING_PREFIX void WDL_VARARG_WARN(printf,3,4) WDL_snprintf(char *o, size_t count, const char *format, ...)
    {
      if (count>0)
      {
        int rv;
        va_list va;
        va_start(va,format);
        o[0]=0;
        rv=_vsnprintf(o,count,format,va); // returns -1  if over, and does not null terminate, ugh
        va_end(va);

        if (rv < 0 || rv>=(int)count-1) o[count-1]=0; 
      }
    }
  #endif

  _WDL_CSTRING_PREFIX void lstrcpyn_safe(char *o, const char *in, INT_PTR count)
  {
    if (count>0)
    {
      while (--count>0 && *in) *o++ = *in++;
      *o=0;
    }
  }

  _WDL_CSTRING_PREFIX void lstrcatn(char *o, const char *in, INT_PTR count)
  {
    if (count>0)
    {
      while (*o) { if (--count < 1) return; o++; }
      while (--count>0 && *in) *o++ = *in++;
      *o=0;
    }
  }

  _WDL_CSTRING_PREFIX const char *WDL_get_filepart(const char *str) // returns whole string if no dir chars
  {
    const char *p = str;
    while (*p) p++;
    while (p >= str && !WDL_IS_DIRCHAR(*p)) --p;
    return p + 1;
  }
  _WDL_CSTRING_PREFIX const char *WDL_get_fileext(const char *str) // returns ".ext" or end of string "" if no extension
  {
    const char *p=str, *ep;
    while (*p) p++;
    ep = p;
    while (p >= str && !WDL_IS_DIRCHAR(*p))
    {
      if (*p == '.') return p;
      --p;
    }
    return ep;
  }

  _WDL_CSTRING_PREFIX char *WDL_remove_fileext(char *str) // returns pointer to "ext" if ".ext" was removed (zero-d dot), or NULL
  {
    char *p=str;
    while (*p) p++;
    while (p >= str && !WDL_IS_DIRCHAR(*p))
    {
      if (*p == '.') 
      {
        *p = 0;
        return p+1;
      }
      --p;
    }
    return NULL;
  }

  _WDL_CSTRING_PREFIX char WDL_remove_filepart(char *str) // returns dir character that was zeroed, or 0 if new string is empty
  {
    char *p=str;
    while (*p) p++;
    while (p >= str)
    {
      char c = *p;
      if (WDL_IS_DIRCHAR(c)) 
      {
        *p = 0;
        return c;
      }
      --p;
    }
    str[0] = 0;
    return 0;
  }

  _WDL_CSTRING_PREFIX int WDL_remove_trailing_dirchars(char *str) // returns trailing dirchar count removed
  {
    int cnt = 0;
    char *p=str;
    while (*p) p++;
    while (p > str+1 && WDL_IS_DIRCHAR(p[-1])) 
    {
      cnt++;
      p--;
    }
    *p = 0;
    return cnt;
  }

  _WDL_CSTRING_PREFIX size_t WDL_remove_trailing_crlf(char *str) // returns new length
  {
    char *p=str;
    while (*p) p++;
    while (p > str && (p[-1] == '\r' || p[-1] == '\n')) p--;
    *p = 0;
    return p-str;
  }

  _WDL_CSTRING_PREFIX size_t WDL_remove_trailing_whitespace(char *str) // returns new length
  {
    char *p=str;
    while (*p) p++;
    while (p > str && (p[-1] == '\r' || p[-1] == '\n' || p[-1] == ' '|| p[-1] == '\t')) p--;
    *p = 0;
    return p-str;
  }

  _WDL_CSTRING_PREFIX char *WDL_remove_trailing_decimal_zeros(char *str, unsigned int keep)
     // returns pointer to decimal point or end of string. removes final zeros after final decimal point only, keep=0 makes min length X, keep=1 X., keep=2 X.0, keep=3 X.00 etc
     // treats commas as decimal points
  {
    char *end = str, *decimal, *last_z=NULL;
    while (*end) end++;
    decimal = end;
    while (--decimal >= str && *decimal >= '0' && *decimal <= '9') if (!last_z && *decimal != '0') last_z = decimal+1;
    if (decimal < str || (*decimal != '.' && *decimal != ',')) return end;
    if (!last_z || last_z < decimal+keep) last_z = decimal+keep;
    if (last_z < end)
    {
      *last_z=0;
      if (!str[0] || ((str[0] == '.' || str[0] == ',') && !str[1]))
      {
        str[0]='0';
        str[1]=0;
        return str+1;
      }
    }
    return decimal;
  }

  _WDL_CSTRING_PREFIX const char *WDL_sanitize_ini_key_start(const char *p) // used for sanitizing the beginning of "key" parameter to Write/GetPrivateProfile*. does not fully sanitize
  {
    while (*p == ' ' || *p == '\t' || *p == '[') p++;
    return p;
  }

  _WDL_CSTRING_PREFIX void WDL_VARARG_WARN(printf,3,4) snprintf_append(char *o, INT_PTR count, const char *format, ...)
  {
    if (count>0)
    {
      va_list va;
      while (*o) { if (--count < 1) return; o++; }
      va_start(va,format);
      vsnprintf(o,count,format,va);
      va_end(va);
    }
  } 

  _WDL_CSTRING_PREFIX void vsnprintf_append(char *o, INT_PTR count, const char *format, va_list va)
  {
    if (count>0)
    {
      while (*o) { if (--count < 1) return; o++; }
      vsnprintf(o,count,format,va);
    }
  }

  _WDL_CSTRING_PREFIX int WDL_strcmp_logical(const char *s1, const char *s2, int case_sensitive)
  {
    // also exists as WDL_LogicalSortStringKeyedArray::_cmpstr()

    char lastNonZeroChar=0;
    // last matching character, updated if not 0. this allows us to track whether
    // we are inside of a number with the same leading digits

    for (;;)
    {
      char c1=*s1++, c2=*s2++;
      if (!c1) return c1-c2;
      
      if (c1!=c2)
      {
        if (c1 >= '0' && c1 <= '9' && c2 >= '0' && c2 <= '9')
        {
          int lzdiff=0, cnt=0;
          if (lastNonZeroChar < '1' || lastNonZeroChar > '9')
          {
            while (c1 == '0') { c1=*s1++; lzdiff--; }
            while (c2 == '0') { c2=*s2++; lzdiff++; } // lzdiff = lz2-lz1, more leading 0s = earlier in list
          }

          for (;;)
          {
            if (c1 >= '0' && c1 <= '9')
            {
              if (c2 < '0' || c2 > '9') return 1;

              c1=s1[cnt];
              c2=s2[cnt++];
            }
            else
            {
              if (c2 >= '0' && c2 <= '9') return -1;
              break;
            }
          }

          s1--;
          s2--;
        
          while (cnt--)
          {
            const int d = *s1++ - *s2++;
            if (d) return d;
          }

          if (lzdiff) return lzdiff;
        }
        else
        {
          if (!case_sensitive)
          {
            if (c1>='a' && c1<='z') c1+='A'-'a';
            if (c2>='a' && c2<='z') c2+='A'-'a';
          }
          if (c1 != c2) return c1-c2;
        }
      }
      else if (c1 != '0') lastNonZeroChar=c1;
    }
  }
  _WDL_CSTRING_PREFIX const char *WDL_stristr(const char* a, const char* b)
  {
    const size_t blen = strlen(b);
    while (*a)
    {
      if (!strnicmp(a, b, blen)) return a;
      a++;
    }
    return NULL;
  }


#endif


#ifdef __cplusplus
};
#endif

#undef _WDL_CSTRING_PREFIX

#endif
