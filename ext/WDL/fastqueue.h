/*
  WDL - fastqueue.h
  Copyright (C) 2006 and later Cockos Incorporated

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
  

  This file defines and implements a class which can queue arbitrary amounts of data. 
  It is optimized for lots of reads and writes with a significant queue (i.e. it doesnt 
  have to shuffle much memory around).

  The downside is that you can't just ask for a pointer to specific bytes, it may have to peice 
  it together into a buffer of your choosing (or you can step through the buffers using GetPtr()).


*/

#ifndef _WDL_FASTQUEUE_H_
#define _WDL_FASTQUEUE_H_


#include "ptrlist.h"

#define WDL_FASTQUEUE_ADD_NOZEROBUF ((void *)(INT_PTR)0xf0)

class WDL_FastQueue
{
  struct fqBuf
  {
    int alloc_size;
    int used;
    char data[8];
  };
public:
  WDL_FastQueue(int bsize=65536-64, int maxemptieskeep=-1)
  {
    m_avail=0;
    m_bsize=bsize<32?32:bsize;
    m_offs=0;
    m_maxemptieskeep=maxemptieskeep;
  }
  ~WDL_FastQueue()
  {
    m_queue.Empty(true,free);
    m_empties.Empty(true,free);
  }
  
  void *Add(const void *buf, int len) // buf can be NULL to add zeroes
  {
    if (len < 1) return NULL;

    fqBuf *qb=m_queue.Get(m_queue.GetSize()-1);
    if (!qb || (qb->used + len) > qb->alloc_size)
    {
      const int esz=m_empties.GetSize()-1;
      qb=m_empties.Get(esz);
      m_empties.Delete(esz);
      if (qb && qb->alloc_size < len) // spare buffer is not big enough, toss it
      {
        free(qb);
        qb=NULL;
      }
      if (!qb)
      {
        const int sz=len < m_bsize ? m_bsize : len;
        qb=(fqBuf *)malloc(sz + sizeof(fqBuf) - sizeof(qb->data));
        if (!qb) return NULL;
        qb->alloc_size = sz;
      }
      qb->used=0;
      m_queue.Add(qb);
    }

    void *ret = qb->data + qb->used;
    if (buf)
    {
      if (buf != WDL_FASTQUEUE_ADD_NOZEROBUF) 
      {
        memcpy(ret, buf, len);
      }
    }
    else 
    {
      memset(ret, 0, len);
    }

    qb->used += len;
    m_avail+=len;
    return ret;
  }

  void Clear(int limitmaxempties=-1)
  {
    int x=m_queue.GetSize();
    if (limitmaxempties<0) limitmaxempties = m_maxemptieskeep;
    while (x > 0)
    {
      if (limitmaxempties<0 || m_empties.GetSize()<limitmaxempties)
      {
        m_empties.Add(m_queue.Get(--x));
      }
      else
      {
        free(m_queue.Get(--x));
      }
      m_queue.Delete(x);      
    }
    m_offs=0;
    m_avail=0;
  }

  void Advance(int cnt)
  {
    m_offs += cnt;
    m_avail -= cnt;
    if (m_avail<0)m_avail=0;

    fqBuf *mq;
    while ((mq=m_queue.Get(0)))
    {
      const int sz=mq->used;
      if (m_offs < sz) break;
      m_offs -= sz;

      if (m_maxemptieskeep<0 || m_empties.GetSize()<m_maxemptieskeep)
      {
        m_empties.Add(mq);
      }
      else
      {
        free(mq);
      }
      m_queue.Delete(0);
    }
    if (!mq||m_offs<0) m_offs=0;
  }

  int Available() const // bytes available
  {
    return m_avail;
  }


  int GetPtr(int offset, void **buf) const // returns bytes available in this block
  {
    offset += m_offs;

    int x=0;
    fqBuf *mq;
    while ((mq=m_queue.Get(x)))
    {
      const int sz=mq->used;
      if (offset < sz)
      {
        *buf = (char *)mq->data + offset;
        return sz-offset;
      }
      x++;
      offset -= sz;
    }
    *buf=NULL;
    return 0;
  }

  int SetFromBuf(int offs, void *buf, int len) // returns length set
  {
    int pos=0;
    while (len > 0)
    {
      void *p=NULL;
      int l=GetPtr(offs+pos,&p);
      if (!l || !p) break;
      if (l > len) l=len;
      memcpy(p,(char *)buf + pos,l);
      pos += l;
      len -= l;
    }
    return pos;
  }

  int GetToBuf(int offs, void *buf, int len) const
  {
    int pos=0;
    while (len > 0)
    {
      void *p=NULL;
      int l=GetPtr(offs+pos,&p);
      if (!l || !p) break;
      if (l > len) l=len;
      memcpy((char *)buf + pos,p,l);
      pos += l;
      len -= l;
    }
    return pos;
  }

private:

  WDL_PtrList<fqBuf> m_queue, m_empties;
  int m_offs;
  int m_avail;
  int m_bsize;
  int m_maxemptieskeep;
} WDL_FIXALIGN;


#endif //_WDL_FASTQUEUE_H_