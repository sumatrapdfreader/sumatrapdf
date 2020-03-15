/* 
    WDL - membitmap.h
    Copyright (C) 2005 and later Cockos Incorporated

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


  This file provides a wrapper around the win32 bitmaps, to allow the callee to easily
  manage a framebuffer. It's mostly deprecated by LICE/, however.

  */



#ifndef _WDL_WINMEMBITMAP_H_
#define _WDL_WINMEMBITMAP_H_

#ifndef _WIN32
#include "../swell/swell.h"
#endif

class WDL_WinMemBitmap
{
public:
  WDL_WinMemBitmap() 
  { 
    m_w=m_h=-100; 
    m_hdc=0;
#ifdef _WIN32
    m_bm=0; m_oldbm=0;
#endif
  }
  ~WDL_WinMemBitmap()
  {
#ifdef _WIN32
    if (m_oldbm) SelectObject(m_hdc,m_oldbm);
    if (m_bm) DeleteObject(m_bm);
      
    if (m_hdc) DeleteDC(m_hdc);
#else
    if (m_hdc) SWELL_DeleteGfxContext(m_hdc);
#endif
  }

  int DoSize(HDC compatDC, int w, int h) // returns 1 if it was resized
  {
    if (m_w == w && m_h == h && m_hdc 
#ifdef _WIN32
        && m_bm
#endif
        ) return 0;
    
#ifdef _WIN32
    if (!m_hdc) m_hdc=CreateCompatibleDC(compatDC);
    if (m_oldbm) SelectObject(m_hdc,m_oldbm);
    if (m_bm) DeleteObject(m_bm);
    m_bm=CreateCompatibleBitmap(compatDC,m_w=w,m_h=h);
    m_oldbm=SelectObject(m_hdc,m_bm);
#else
    if (m_hdc) SWELL_DeleteGfxContext(m_hdc);
    m_hdc=SWELL_CreateMemContext(compatDC,m_w=w,m_h=h);
#endif
    return 1;
  }
  int GetW() { return m_w; }
  int GetH() { return m_h; }
  HDC GetDC() { return m_hdc; }

private:

  HDC m_hdc;
#ifdef _WIN32
  HBITMAP m_bm;
  HGDIOBJ m_oldbm;
#endif
  int m_w,m_h;
};


#endif