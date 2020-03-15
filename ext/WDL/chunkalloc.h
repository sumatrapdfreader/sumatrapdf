#ifndef _WDL_CHUNKALLOC_H_
#define _WDL_CHUNKALLOC_H_

#include "wdltypes.h"

class WDL_ChunkAlloc
{
  struct _hdr
  {
    struct _hdr *_next;
    char data[16];
  };

  _hdr *m_chunks;
  int m_chunksize, m_chunkused;

  public:

    WDL_ChunkAlloc(int chunksize=65500) { m_chunks=NULL; m_chunkused=0; m_chunksize=chunksize>16?chunksize:16; }
    ~WDL_ChunkAlloc() { Free(); }

    void Free()
    {
      _hdr *a = m_chunks;
      m_chunks=0;
      m_chunkused=0;
      while (a) { _hdr *f=a; a=a->_next; free(f); }
    }

    void *Alloc(int sz, int align=0)
    {
      if (sz<1) return NULL;

      if (align < 1 || (align & (align-1))) align=1;

      if (m_chunks)
      {
        int use_sz=sz;
        char *p = m_chunks->data + m_chunkused;
        int a = ((int) (INT_PTR)p) & (align-1);
        if (a)
        {
          use_sz += align-a;
          p += align-a;
        }
        if (use_sz <= m_chunksize - m_chunkused)
        {
          m_chunkused += use_sz;
          return p;
        }
      }

      // we assume that malloc always gives at least 8 byte alignment, and our _next ptr may offset that by 4, 
      // so no need to allocate extra if less than 4 bytes of alignment requested
      int use_align = (align>=4 ? align : 0);
      int alloc_sz=sz+use_align; 
      if (alloc_sz < m_chunksize) 
      {
        // if existing chunk has less free space in it than we would at chunksize, allocate chunksize
        if (!m_chunks || m_chunkused > alloc_sz) alloc_sz=m_chunksize;
      }
      _hdr *nc = (_hdr *)malloc(sizeof(_hdr) + alloc_sz - 16);
      if (!nc) return NULL;

      int use_sz=sz;
      char *ret = nc->data;
      int a = ((int) (INT_PTR)ret) & (align-1);
      if (a)
      {
        use_sz += align-a;
        ret += align-a;
      }
      
      if (m_chunks && (m_chunksize-m_chunkused) >= (alloc_sz - use_sz))
      {
        // current chunk has as much or more free space than our chunk, put our chunk on the list second
        nc->_next = m_chunks->_next;
        m_chunks->_next=nc;
      }
      else
      {
        // push our chunk to the top of the list
        nc->_next = m_chunks;
        m_chunks=nc;
        m_chunkused = alloc_sz >= m_chunksize ? use_sz : m_chunksize;
      }

      return ret;
    }

    char *StrDup(const char *s)
    {
      if (!s) return NULL;
      const int l = (int) strlen(s)+1;
      char *ret = (char*)Alloc(l);
      if (!ret) return NULL;
      memcpy(ret,s,l);
      return ret;
    }

};

#endif
