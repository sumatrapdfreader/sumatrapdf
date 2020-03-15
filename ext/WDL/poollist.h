/*
  WDL - poollist.h
  Copyright (C) 2006 and later, Cockos Incorporated

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
  


  This file defines a template class for hosting lists of referenced count, string-identified objects.

  We mostly use it with WDL_ResourcePool, but any class like this can use it:


  class SomeClass
  {
  public:
    SomeClass(char *identstr) { WDL_POOLLIST_identstr=identstr; WDL_POOLLIST_refcnt=0; }
    ~SomeClass() {}  // do NOT free or delete WDL_POOLLIST_identstr


    void Clear() {}  // will be called if ReleasePool(x,false) is called and refcnt gets to 0
    int WDL_POOLLIST_refcnt;
    char *WDL_POOLLIST_identstr;
  };



*/



#ifndef _WDL_POOLLIST_H_
#define _WDL_POOLLIST_H_

#include <stdlib.h>

#include "mutex.h"

template<class DATATYPE> class WDL_PoolList_NoFreeOnDestroy
{
public:

  WDL_PoolList_NoFreeOnDestroy()
  {
  }
  ~WDL_PoolList_NoFreeOnDestroy()
  {
  }

  DATATYPE *Get(const char *filename, bool createIfExists=true)
  {
    WDL_MutexLock lock(&mutex);

    DATATYPE *t = Find(filename,false);
    if (t)
    {
      t->WDL_POOLLIST_refcnt++;
      return t;
    }
    if (!createIfExists) return NULL;

    t = new DATATYPE(strdup(filename));
    t->WDL_POOLLIST_refcnt=1;

    int x;
    for(x=0;x<pool.GetSize();x++) if (stricmp(pool.Get(x)->WDL_POOLLIST_identstr,filename)>0) break;

    pool.Insert(x,t);

    return t;
  }

  DATATYPE *Find(const char *filename, bool lockMutex=true) // not threadsafe
  {
    if (lockMutex) mutex.Enter();
    DATATYPE  **_tmp=NULL;
    if (pool.GetSize())
    {
      DATATYPE tmp((char *)filename),*t=&tmp;
      _tmp = (DATATYPE**)bsearch(&t,pool.GetList(),pool.GetSize(),sizeof(void *),_sortfunc);
    }
    if (lockMutex) mutex.Leave();
    return _tmp ? *_tmp : NULL;
  }

  int ReleaseByName(const char *filename, bool isFull=true)
  {
    WDL_MutexLock lock(&mutex);
    return Release(Find(filename,false),isFull);
  }

  int Release(DATATYPE *tp, bool isFull=true)
  {
    if (!tp) return -1;
    WDL_MutexLock lock(&mutex);

    int refcnt;
    if (!(refcnt=--tp->WDL_POOLLIST_refcnt))
    {
      if (!isFull)
      {
        tp->Clear();        
      }
      else
      {
        int x;
        for (x = 0; x < pool.GetSize() && pool.Get(x) != tp; x ++);
        if (x<pool.GetSize()) 
        {
          pool.Delete(x);
        }
        free(tp->WDL_POOLLIST_identstr);
        delete tp;
      }
      // remove from list
    }
    return refcnt;
  }
 
  void RemoveAll() 
  {
    int x;
    for (x = 0; x < pool.GetSize(); x ++) 
    {
      DATATYPE *p = pool.Get(x);
      free(p->WDL_POOLLIST_identstr);
      delete p;
    }
    pool.Empty();
  }

  WDL_Mutex mutex;
  WDL_PtrList< DATATYPE > pool;

private:

  static int _sortfunc(const void *a, const void *b)
  {
    DATATYPE *ta = *(DATATYPE **)a;
    DATATYPE *tb = *(DATATYPE **)b;

    return stricmp(ta->WDL_POOLLIST_identstr,tb->WDL_POOLLIST_identstr);
  }
};

template<class DATATYPE> class WDL_PoolList : public WDL_PoolList_NoFreeOnDestroy<DATATYPE>
{
public:
  WDL_PoolList() { }
  ~WDL_PoolList() { WDL_PoolList_NoFreeOnDestroy<DATATYPE>::RemoveAll(); }


};

#endif
