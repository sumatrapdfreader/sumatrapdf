/*
    WDL - ptrlist.h
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

  This file provides a simple templated class for a list of pointers. By default this list
  doesn't free any of the pointers, but you can call Empty(true) or Delete(x,true) to delete the pointer,
  or you can use Empty(true,free) etc to call free (or any other function).

  Note: on certain compilers, instantiating with WDL_PtrList<void> bla; will give a warning, since
  the template will create code for "delete (void *)x;" which isn't technically valid. Oh well.
 
*/

#ifndef _WDL_PTRLIST_H_
#define _WDL_PTRLIST_H_

#include "heapbuf.h"

template<class PTRTYPE> class WDL_PtrList 
{
  public:
    explicit WDL_PtrList(int defgran=4096) : m_hb(defgran WDL_HEAPBUF_TRACEPARM("WDL_PtrList"))
    {
    }

    ~WDL_PtrList()
    {
    }

    PTRTYPE **GetList() const { return (PTRTYPE**)m_hb.Get(); }
    PTRTYPE *Get(INT_PTR index) const 
    { 
      PTRTYPE **list = (PTRTYPE**)m_hb.Get(); 
      if (list && (UINT_PTR)index < (UINT_PTR)(m_hb.GetSize()/sizeof(PTRTYPE *))) return list[index]; 
      return NULL; 
    }

    int GetSize(void) const { return m_hb.GetSize()/(unsigned int)sizeof(PTRTYPE *); }  

    int Find(const PTRTYPE *p) const
    {
      if (p)
      {
        PTRTYPE **list=(PTRTYPE **)m_hb.Get();
        int x;     
        const int n = GetSize();
        for (x = 0; x < n; x ++) if (list[x] == p) return x;
      }
      return -1;
    }
    int FindR(const PTRTYPE *p) const
    {
      if (p)
      {
        PTRTYPE **list=(PTRTYPE **)m_hb.Get();
        int x = GetSize();
        while (--x >= 0) if (list[x] == p) return x;
      }
      return -1;
    }

    PTRTYPE *Add(PTRTYPE *item)
    {
      const int s=GetSize();
      PTRTYPE **list=(PTRTYPE **)m_hb.ResizeOK((s+1)*(unsigned int)sizeof(PTRTYPE*),false);
      if (list)
      {
        list[s]=item;
        return item;
      }
      return NULL;
    }

    PTRTYPE *Set(int index, PTRTYPE *item) 
    { 
      PTRTYPE **list=(PTRTYPE **)m_hb.Get();
      if (list && index >= 0 && index < GetSize()) return list[index]=item;
      return NULL;
    }

    PTRTYPE *Insert(int index, PTRTYPE *item)
    {
      int s=GetSize();
      PTRTYPE **list = (PTRTYPE **)m_hb.ResizeOK((s+1)*(unsigned int)sizeof(PTRTYPE*),false);

      if (!list) return item;

      if (index<0) index=0;
    
      int x;
      for (x = s; x > index; x --) list[x]=list[x-1];
      return (list[x] = item);
    }
    int FindSorted(const PTRTYPE *p, int (*compar)(const PTRTYPE **a, const PTRTYPE **b)) const
    {
      bool m;
      int i = LowerBound(p,&m,compar);
      return m ? i : -1;
    }
    PTRTYPE *InsertSorted(PTRTYPE *item, int (*compar)(const PTRTYPE **a, const PTRTYPE **b))
    {
      bool m;
      return Insert(LowerBound(item,&m,compar),item);
    }

    void Delete(int index)
    {
      PTRTYPE **list=GetList();
      int size=GetSize();
      if (list && index >= 0 && index < size)
      {
        if (index < --size) memmove(list+index,list+index+1,(unsigned int)sizeof(PTRTYPE *)*(size-index));
        m_hb.Resize(size * (unsigned int)sizeof(PTRTYPE*),false);
      }
    }
    void Delete(int index, bool wantDelete, void (*delfunc)(void *)=NULL)
    {
      PTRTYPE **list=GetList();
      int size=GetSize();
      if (list && index >= 0 && index < size)
      {
        if (wantDelete)
        {
          if (delfunc) delfunc(Get(index));
          else delete Get(index);
        }
        if (index < --size) memmove(list+index,list+index+1,(unsigned int)sizeof(PTRTYPE *)*(size-index));
        m_hb.Resize(size * (unsigned int)sizeof(PTRTYPE*),false);
      }
    }
    void Delete(int index, void (*delfunc)(PTRTYPE *))
    {
      PTRTYPE **list=GetList();
      int size=GetSize();
      if (list && index >= 0 && index < size)
      {
        if (delfunc) delfunc(Get(index));
        if (index < --size) memmove(list+index,list+index+1,(unsigned int)sizeof(PTRTYPE *)*(size-index));
        m_hb.Resize(size * (unsigned int)sizeof(PTRTYPE*),false);
      }
    }
    void DeletePtr(const PTRTYPE *p) { Delete(Find(p)); }
    void DeletePtr(const PTRTYPE *p, bool wantDelete, void (*delfunc)(void *)=NULL) { Delete(Find(p),wantDelete,delfunc); }
    void DeletePtr(const PTRTYPE *p, void (*delfunc)(PTRTYPE *)) { Delete(Find(p),delfunc); }

    void Empty()
    {
      m_hb.Resize(0,false);
    }
    void Empty(bool wantDelete, void (*delfunc)(void *)=NULL)
    {
      if (wantDelete)
      {
        int x;
        for (x = GetSize()-1; x >= 0; x --)
        {
          PTRTYPE* p = Get(x);
          if (p)
          {
            if (delfunc) delfunc(p);
            else delete p;
          }
          m_hb.Resize(x*(unsigned int)sizeof(PTRTYPE *),false);
        }
      }
      m_hb.Resize(0,false);
    }
    void Empty(void (*delfunc)(PTRTYPE *))
    {
      int x;
      for (x = GetSize()-1; x >= 0; x --)
      {
        PTRTYPE* p = Get(x);
        if (delfunc && p) delfunc(p);
        m_hb.Resize(x*(unsigned int)sizeof(PTRTYPE *),false);
      }
    }
    void EmptySafe(bool wantDelete=false,void (*delfunc)(void *)=NULL)
    {
      if (!wantDelete) Empty();
      else
      {
        WDL_PtrList<PTRTYPE> tmp;
        int x;
        for(x=0;x<GetSize();x++)tmp.Add(Get(x));
        Empty();
        tmp.Empty(true,delfunc);
      }
    }

    int LowerBound(const PTRTYPE *key, bool* ismatch, int (*compar)(const PTRTYPE **a, const PTRTYPE **b)) const
    {
      int a = 0;
      int c = GetSize();
      PTRTYPE **list=GetList();
      while (a != c)
      {
        int b = (a+c)/2;
        int cmp = compar((const PTRTYPE **)&key, (const PTRTYPE **)(list+b));
        if (cmp > 0) a = b+1;
        else if (cmp < 0) c = b;
        else
        {
          *ismatch = true;
          return b;
        }
      }
      *ismatch = false;
      return a;
    }

    void Compact() { m_hb.Resize(m_hb.GetSize(),true); }


    int DeleteBatch(bool (*proc)(PTRTYPE *p, void *ctx), void *ctx=NULL) // proc returns true to remove item. returns number removed
    {
      const int sz = GetSize();
      int cnt=0;
      PTRTYPE **rd = GetList(), **wr = rd;
      for (int x = 0; x < sz; x ++)
      {
        if (!proc(*rd,ctx))
        {
          if (rd != wr) *wr=*rd;
          wr++;
          cnt++;
        }
        rd++;
      }
      if (cnt < sz) m_hb.Resize(cnt * sizeof(PTRTYPE*),false);
      return sz - cnt;
    }

  private:
    WDL_HeapBuf m_hb;

};


template<class PTRTYPE> class WDL_PtrList_DeleteOnDestroy : public WDL_PtrList<PTRTYPE>
{
public:
  explicit WDL_PtrList_DeleteOnDestroy(void (*delfunc)(void *)=NULL, int defgran=4096) : WDL_PtrList<PTRTYPE>(defgran), m_delfunc(delfunc) {  } 
  ~WDL_PtrList_DeleteOnDestroy()
  {
    WDL_PtrList<PTRTYPE>::EmptySafe(true,m_delfunc);
  }
private:
  void (*m_delfunc)(void *);
};

#endif

