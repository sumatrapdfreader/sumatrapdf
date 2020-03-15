/*
  WDL - sharedpool.h
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
  


  This file defines a template for a simple object pool. 

  Objects are keyed by string (filename, or otherwise). The caller can add or get an object,
  increase or decrease its reference count (if it reaches zero the object is deleted).
  

  If you delete the pool itself, all objects are deleted, regardless of their reference count.

*/


#ifndef _WDL_SHAREDPOOL_H_
#define _WDL_SHAREDPOOL_H_

#include "ptrlist.h"

template<class OBJ> class WDL_SharedPool
{
  public:
    WDL_SharedPool() { }
    ~WDL_SharedPool() { m_listobjk.Empty(true); /* do not release m_list since it's redundant */ }

    void Add(OBJ *obj, const char *n) // no need to AddRef() after add, it defaults to a reference count of 1.
    {
      if (obj && n)
      {
        Ent *p = new Ent(obj,n);
        m_list.InsertSorted(p,_sortfunc_name);
        m_listobjk.InsertSorted(p,_sortfunc_obj);
      }
    }

    OBJ *Get(const char *s)
    {
      struct { void *obj; const char *name; } tmp = { NULL, s };
      Ent *t = m_list.Get(m_list.FindSorted((Ent *)&tmp,_sortfunc_name));
      
      if (t && t->obj)
      {
        t->refcnt++;
        return t->obj;
      }

      return 0;

    }

    void AddRef(OBJ *obj)
    {
      Ent *ent = m_listobjk.Get(m_listobjk.FindSorted((Ent *)&obj,_sortfunc_obj));
      if (ent) ent->refcnt++;
    }

    void Release(OBJ *obj)
    {
      int x = m_listobjk.FindSorted((Ent *)&obj,_sortfunc_obj);
      Ent *ent = m_listobjk.Get(x);
      if (ent && !--ent->refcnt) 
      {
        m_list.Delete(m_list.FindSorted(ent,_sortfunc_name));
        m_listobjk.Delete(x,true);
      }
    }

    OBJ *EnumItems(int x)
    {
      Ent *e=m_list.Get(x);
      return e?e->obj:NULL;
    }

  private:

    class Ent
    {
      public:
        OBJ *obj; // this order is used elsewhere for its own advantage
        char *name; 

        int refcnt;

        Ent(OBJ *o, const char *n) { obj=o; name=strdup(n); refcnt=1; }
        ~Ent() { delete obj; free(name); }

    };



    static int _sortfunc_name(const Ent **a, const Ent **b)
    {
      return stricmp((*a)->name,(*b)->name);
    }
    static int _sortfunc_obj(const Ent **a, const Ent **b)
    {
      if ((INT_PTR)(*a)->obj < (INT_PTR)(*b)->obj) return -1;
      if ((INT_PTR)(*a)->obj > (INT_PTR)(*b)->obj) return 1;
      return 0;
    }
    
    WDL_PtrList<Ent> m_list, // keyed by name
                     m_listobjk; // keyed by OBJ


};


#endif//_WDL_SHAREDPOOL_H_
