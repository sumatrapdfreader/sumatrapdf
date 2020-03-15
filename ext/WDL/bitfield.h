#ifndef _WDL_BITFIELD_H_
#define _WDL_BITFIELD_H_

#include "heapbuf.h"

class WDL_BitField  // ultra simple bit field
{
public:
  bool SetSize(int sz) // clears state
  {
    void *b=m_hb.ResizeOK((sz+7)/8);
    if (b) memset(b,0,m_hb.GetSize());
    return !!b;
  }
  int GetApproxSize() const { return m_hb.GetSize()*8; } // may return slightly greater than the size set

  bool IsSet(unsigned int idx) const 
  { 
    const unsigned char mask = 1<<(idx&7);
    idx>>=3;
    return idx < (unsigned int)m_hb.GetSize() && (((unsigned char *)m_hb.Get())[idx]&mask);
  }
  void Set(unsigned int idx)
  {
    const unsigned char mask = 1<<(idx&7);
    idx>>=3;
    if (idx < (unsigned int)m_hb.GetSize()) ((unsigned char *)m_hb.Get())[idx] |= mask;
  }

private:
  WDL_HeapBuf m_hb;
};

#endif //_WDL_BITFIELD_H_