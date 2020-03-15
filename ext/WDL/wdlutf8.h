/*
WDL - wdlutf8.h
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

#ifndef _WDLUTF8_H_
#define _WDLUTF8_H_

/* todo: handle overlongs?
 * todo: handle multi-byte (make WideStr support UTF-16)
 */

#include "wdltypes.h"

#ifndef WDL_WCHAR
  #ifdef _WIN32
    #define WDL_WCHAR WCHAR
  #else
    // this is often 4 bytes on macOS/linux! beware dragons!
    #define WDL_WCHAR wchar_t
  #endif
#endif


// returns size, sets cOut to code point. 
// if invalid ITF-8, sets cOut to first character (as unsigned char).
// cOut may be NULL if you only want the size of the character
static int WDL_STATICFUNC_UNUSED wdl_utf8_parsechar(const char *rd, int *cOut) 
{
  const unsigned char *p = (const unsigned char *)rd;
  const unsigned char b0 = *p;
  unsigned char b1,b2,b3;

  if (cOut) *cOut = b0;
  if (b0 < 0x80) 
  {
    return 1;
  }
  if (((b1=p[1])&0xC0) != 0x80) return 1;

  if (b0 < 0xE0)
  {
    if (!(b0&0x1E)) return 1; // detect overlong
    if (cOut) *cOut = ((b0&0x1F)<<6)|(b1&0x3F);
    return 2;
  }

  if (((b2=p[2])&0xC0) != 0x80) return 1;

  if (b0 < 0xF0)
  {
    if (!(b0&0xF) && !(b1&0x20)) return 1; // detect overlong

    if (cOut) *cOut = ((b0&0x0F)<<12)|((b1&0x3F)<<6)|(b2&0x3f);
    return 3;
  }

  if (((b3=p[3])&0xC0) != 0x80) return 1;

  if (b0 < 0xF8)
  {
    if (!(b0&0x7) && !(b1&0x30)) return 1; // detect overlong

    if (cOut) *cOut = ((b0&7)<<18)|((b1&0x3F)<<12)|((b2&0x3F)<<6)|(b3&0x3F);
    return 4;
  }

  // UTF-8 does not actually support 5-6 byte sequences as of 2003 (RFC-3629)
  // skip them and return _
  if ((p[4]&0xC0) != 0x80) return 1;
  if (b0 < 0xFC) 
  {
    if (cOut) *cOut = '_';
    return 5;
  }

  if ((p[5]&0xC0) != 0x80) return 1;
  if (cOut) *cOut = '_';
  return 6;
}


// makes a character, returns length. does NOT nul terminate.
// returns 0 if insufficient space, -1 if out of range value
static int WDL_STATICFUNC_UNUSED wdl_utf8_makechar(int c, char *dest, int dest_len)
{
  if (c < 0) return -1; // out of range character

  if (c < 0x80)
  {
    if (dest_len<1) return 0;
    dest[0]=(char)c;
    return 1;
  }  
  if (c < 0x800)
  {
    if (dest_len < 2) return 0;

    dest[0]=0xC0|(c>>6);
    dest[1]=0x80|(c&0x3F);
    return 2;
  }
  if (c < 0x10000)
  {
    if (dest_len < 3) return 0;

    dest[0]=0xE0|(c>>12);
    dest[1]=0x80|((c>>6)&0x3F);
    dest[2]=0x80|(c&0x3F);
    return 3;
  }
  if (c < 0x200000)
  {
    if (dest_len < 4) return 0;
    dest[0]=0xF0|(c>>18);
    dest[1]=0x80|((c>>12)&0x3F);
    dest[2]=0x80|((c>>6)&0x3F);
    dest[3]=0x80|(c&0x3F);
    return 4;
  }

  return -1;
}


// invalid UTF-8 are now treated as ANSI characters for this function
static int WDL_STATICFUNC_UNUSED WDL_MBtoWideStr(WDL_WCHAR *dest, const char *src, int destlenbytes)
{
  WDL_WCHAR *w = dest, *dest_endp = dest+(size_t)destlenbytes/sizeof(WDL_WCHAR)-1;
  if (!dest || destlenbytes < 1) return 0;

  if (src) for (; *src && w < dest_endp; )
  {
    int c,sz=wdl_utf8_parsechar(src,&c);
    *w++ = c;
    src+=sz;
  }
  *w=0; 
  return (int)(w-dest);
}


// like wdl_utf8_makechar, except nul terminates and handles errors differently (returns _ and 1 on errors)
// negative values for character are treated as 0.
static int WDL_STATICFUNC_UNUSED WDL_MakeUTFChar(char* dest, int c, int destlen)
{
  if (destlen < 2)
  {
    if (destlen == 1) dest[0]=0;
    return 0;
  }
  else
  {
    const int v = wdl_utf8_makechar(c>0?c:0,dest,destlen-1);
    if (v < 1) // implies either insufficient space or out of range character
    {
      dest[0]='_';
      dest[1]=0;
      return 1;
    }
    dest[v]=0;
    return v;
  }
}

static int WDL_STATICFUNC_UNUSED WDL_WideToMBStr(char *dest, const WDL_WCHAR *src, int destlenbytes)
{
  char *p = dest, *dest_endp = dest + destlenbytes - 1;
  if (!dest || destlenbytes < 1) return 0;

  if (src) while (*src && p < dest_endp)
  {
    const int v = wdl_utf8_makechar(*src++,p,(int)(dest_endp-p));
    if (v > 0)
    {
      p += v;
    }
    else if (v == 0) break; // out of space
  }
  *p=0;
  return (int)(p-dest);
}

// returns >0 if UTF-8, -1 if 8-bit chars occur that are not UTF-8, or 0 if ASCII
static int WDL_STATICFUNC_UNUSED WDL_DetectUTF8(const char *str)
{
  int hasUTF=0;

  if (!str) return 0;
  
  for (;;)
  {
    const unsigned char c = *(const unsigned char *)str;

    if (c < 0xC2 || c > 0xF7) 
    {
      if (!c) return hasUTF;
      if (c >= 0x80) return -1;
      str++;
    }
    else
    {
      const int l = wdl_utf8_parsechar(str,NULL);
      if (l < 2) return -1; // wdl_utf8_parsechar returns length=1 if it couldn't parse UTF-8 properly
      str+=l;
      hasUTF=1;
    }
  }
}


static int WDL_STATICFUNC_UNUSED WDL_utf8_charpos_to_bytepos(const char *str, int charpos)
{
  int bpos = 0;
  while (charpos-- > 0 && str[bpos])
  {
    bpos += wdl_utf8_parsechar(str+bpos,NULL);
  }
  return bpos;
}
static int WDL_STATICFUNC_UNUSED WDL_utf8_bytepos_to_charpos(const char *str, int bytepos)
{
  int bpos = 0, cpos=0;
  while (bpos < bytepos && str[bpos])
  {
    bpos += wdl_utf8_parsechar(str+bpos,NULL);
    cpos++;
  }
  return cpos;
}

#define WDL_utf8_get_charlen(rd) WDL_utf8_bytepos_to_charpos((rd), 0x7fffffff)

#endif
