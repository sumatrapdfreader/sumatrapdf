#ifndef _WDL_PTRLIST_INDEXED_H_
#define _WDL_PTRLIST_INDEXED_H_

#include "assocarray.h"
#include "ptrlist.h"

template<class T> class WDL_IndexedPtrList {
  public:
    WDL_IndexedPtrList() { }
    ~WDL_IndexedPtrList() { }

    int GetSize() const {
      // do not _checkState(), GetSize() may be called while unlocked for estimation purposes
      return m_list.GetSize();
    }
    T * const *GetList() const { _checkState(); return m_list.GetList(); }
    T *Get(int idx) const { _checkState(); return m_list.Get(idx); }
    int Find(const T *p) const
    {
      _checkState();
      if (!p) return -1;
      const int *ret = m_index.GetPtr((INT_PTR)p);
      return ret ? *ret : -1;
    }
    void Empty() { _checkState(); m_list.Empty(); m_index.DeleteAll(); }
    void Delete(int idx)
    {
      _checkState();
      T *item = m_list.Get(idx);
      m_list.Delete(idx);
      if (item)
      {
        m_index.Delete((INT_PTR)item);
        const int indexsz = m_index.GetSize();
        WDL_ASSERT(m_list.GetSize() == indexsz);
        if (idx < indexsz)
        {
          for (int x=0;x<indexsz;x++)
          {
            int *val = m_index.EnumeratePtr(x);
            if (WDL_NORMALLY(val))
            {
              WDL_ASSERT(*val != idx);
              if (*val > idx) (*val)--;
            }
          }
        }
      }
    }
    void Add(T *p)
    {
      _checkState();
      WDL_ASSERT(Find(p) < 0);
      if (WDL_NORMALLY(p))
      {
        const int sz = m_list.GetSize();
        m_list.Add(p);
        m_index.Insert((INT_PTR)p,sz);
      }
    }
    void Swap(int index1, int index2)
    {
      _checkState();
      if (index1 != index2 &&
          WDL_NORMALLY(index1>=0) &&
          WDL_NORMALLY(index1<m_list.GetSize()) &&
          WDL_NORMALLY(index2>=0) && 
          WDL_NORMALLY(index2<m_list.GetSize()))
      {
        T **list = m_list.GetList();
        T *a = list[index1];
        T *b = list[index2];
        list[index2]=a;
        list[index1]=b;
        m_index.Insert((INT_PTR)a,index2);
        m_index.Insert((INT_PTR)b,index1);
      }
    }
    void Insert(int index, T *p)
    {
      _checkState();
      if (!WDL_NORMALLY(p)) return;
      const int listsz = m_list.GetSize();
      if (index < 0) index=0;
      if (index >= listsz) { Add(p); return; }

      const int indexsz = m_index.GetSize();
      WDL_ASSERT(listsz==indexsz);

      for (int x=0;x<indexsz;x++)
      {
        int *val = m_index.EnumeratePtr(x);
        if (WDL_NORMALLY(val))
        {
          if (*val >= index) (*val)++;
          WDL_ASSERT(*val != index);
        }
      }
      m_list.Insert(index,p);
      m_index.Insert((INT_PTR)p,index);
    }

    WDL_PtrList<T> m_list;
    WDL_PtrKeyedArray<int> m_index;

    void _checkState() const
    {
#ifdef _DEBUG
      const int idxsz = m_index.GetSize(), listsz = m_list.GetSize();
      WDL_ASSERT(idxsz == listsz);
#endif
    }
};

#endif
