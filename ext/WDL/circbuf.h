/*
    WDL - circbuf.h
    Copyright (C) 2005 Cockos Incorporated

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

  This file provides a simple class for a circular FIFO queue of bytes. 

*/

#ifndef _WDL_CIRCBUF_H_
#define _WDL_CIRCBUF_H_

#include "heapbuf.h"

class WDL_CircBuf
{
public:
  WDL_CircBuf() : m_hb(4096 WDL_HEAPBUF_TRACEPARM("WDL_CircBuf"))
  {
    m_inbuf = m_wrptr = 0;
  }
  ~WDL_CircBuf()
  {
  }
  void SetSize(int size)
  {
    m_inbuf = m_wrptr = 0;
    m_hb.Resize(size,true);
  }
  void Reset() { m_inbuf = m_wrptr = 0; }
  int Add(const void *buf, int l)
  {
    const int bf = m_hb.GetSize() - m_inbuf;
    if (l > bf) l = bf;
    if (l > 0)
    {
      const int wr1 = m_hb.GetSize()-m_wrptr;
      if (wr1 < l)
      {
        memmove((char*)m_hb.Get() + m_wrptr, buf, wr1);
        memmove(m_hb.Get(), (char*)buf + wr1, l-wr1);
        m_wrptr = l-wr1;
      }
      else 
      {
        memmove((char*)m_hb.Get() + m_wrptr, buf, l);
        m_wrptr = wr1 == l ? 0 : m_wrptr+l;
      }
      m_inbuf += l;
    }
    return l;
  }
  int Peek(void *buf, int offs, int len) const 
  {
    if (offs<0) return 0;
    const int ibo = m_inbuf-offs;
    if (len > ibo) len = ibo;
    if (len > 0)
    {
      int rp = m_wrptr - ibo;
      if (rp < 0) rp += m_hb.GetSize();
      const int wr1 = m_hb.GetSize() - rp;
      if (wr1 < len)
      {
        memmove(buf,(char*)m_hb.Get()+rp,wr1);
        memmove((char*)buf+wr1,m_hb.Get(),len-wr1);
      }
      else
      {
        memmove(buf,(char*)m_hb.Get()+rp,len);
      }
    }
    return len;
  }
  int Get(void *buf, int l)
  {
    const int amt = Peek(buf,0,l);
    m_inbuf -= amt;
    return amt;
  }
  int NbFree() const { return m_hb.GetSize() - m_inbuf; } // formerly Available()
  int NbInBuf() const { return m_inbuf; }

private:
  WDL_HeapBuf m_hb;
  int m_inbuf, m_wrptr;
} WDL_FIXALIGN;


template <class T>
class WDL_TypedCircBuf
{
public:

    WDL_TypedCircBuf() {}
    ~WDL_TypedCircBuf() {}

    void SetSize(int size)
    {
        mBuf.SetSize(size * sizeof(T));
    }

    void Reset()
    {
        mBuf.Reset();
    }

    int Add(const T* buf, int l)
    {
        return mBuf.Add(buf, l * sizeof(T)) / sizeof(T);
    }

    int Get(T* buf, int l)
    {
        return mBuf.Get(buf, l * sizeof(T)) / sizeof(T);
    }

    int NbFree()  // formerly Available()
    {
        return mBuf.NbFree() / sizeof(T);
    }
     int NbInBuf() 
     { 
         return mBuf.NbInBuf() / sizeof(T);
     }

private:
    WDL_CircBuf mBuf;
} WDL_FIXALIGN;

#endif
