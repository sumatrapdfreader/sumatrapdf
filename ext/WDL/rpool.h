/*
  WDL - rpool.h
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
  

  This file defines a template for a class that stores a list of objects, and allows the caller
  to periodically get an object, do something with it, and add it back into the pool.

  When the caller does this, it can set ownership of the object, and an expiration for that ownership.


  The PTYPE1 and PTYPE2entries for the template are there to store additional information (for use with poollist.h)


  This is pretty esoteric. But we use it for some things.

*/


#ifndef _WDL_RPOOL_H_
#define _WDL_RPOOL_H_

// resource pool (time based)

#include "ptrlist.h"
#include "mutex.h"

class WDL_ResourcePool_ResInfo // include in class RTYPE as WDL_ResourcePool_ResInfo m_rpoolinfo;
{
public:
  WDL_ResourcePool_ResInfo(){ m_owneduntil=0; m_ownerptr=0; next=0; }
  ~WDL_ResourcePool_ResInfo() {}

  unsigned int m_owneduntil;
  void *m_ownerptr;

  void *next;
} WDL_FIXALIGN;


template<class RTYPE, class EXTRAINFOTYPE> class WDL_ResourcePool
{
  public:
    WDL_ResourcePool(char *identstr)
    {
      WDL_POOLLIST_refcnt=0;
      WDL_POOLLIST_identstr=identstr;
      m_rlist=NULL;
      extraInfo=0;
      m_hadres=false;
    }
    ~WDL_ResourcePool()
    {
      while (m_rlist)
      {
        RTYPE *tp=m_rlist;
        m_rlist=(RTYPE *)m_rlist->m_rpoolinfo.next;
        delete tp;
      }
      delete extraInfo;
    }
    void Clear()
    {
      m_mutex.Enter();
      while (m_rlist)
      {
        RTYPE *tp=m_rlist;
        m_rlist=(RTYPE *)m_rlist->m_rpoolinfo.next;
        delete tp;
      }
      m_hadres=false;
      m_mutex.Leave();
    }
    bool HasResources()
    {
      return m_hadres;
    }

    void AddResource(RTYPE *item, void *own, unsigned int until)
    {
      item->m_rpoolinfo.m_ownerptr = own;
      item->m_rpoolinfo.m_owneduntil = until;

      m_mutex.Enter();
      item->m_rpoolinfo.next = m_rlist;
      m_rlist = item;
      m_hadres=true;
      m_mutex.Leave();
    }

    void ReleaseResources(void *own)
    {
      m_mutex.Enter();
      RTYPE *ent=m_rlist;
      while (ent)
      {
        if (ent->m_rpoolinfo.m_ownerptr == own)
        {
          ent->m_rpoolinfo.m_ownerptr = 0;
          ent->m_rpoolinfo.m_owneduntil=0;
        }
        ent=(RTYPE *)ent->m_rpoolinfo.next;
      }
      m_mutex.Leave();
    }

    RTYPE *GetResource(void *own, unsigned int now)
    {
      m_mutex.Enter();
      RTYPE *ent=m_rlist, *lastent=NULL, *bestent=NULL, *bestlastent=NULL;
      bool bestnoown=false;
      while (ent)
      {
        if (ent->m_rpoolinfo.m_ownerptr == own)
        {
          if (lastent) lastent->m_rpoolinfo.next = ent->m_rpoolinfo.next;
          else m_rlist = (RTYPE *)ent->m_rpoolinfo.next;
          m_mutex.Leave();
          return ent;
        }

        if (!bestnoown && (!ent->m_rpoolinfo.m_ownerptr || ent->m_rpoolinfo.m_owneduntil < now))
        {
          bestent=ent;
          bestlastent=lastent;
          if (!ent->m_rpoolinfo.m_ownerptr || !ent->m_rpoolinfo.m_owneduntil) bestnoown=true;
        }
        lastent=ent;
        ent=(RTYPE *)ent->m_rpoolinfo.next;
      }

      if (bestent)
      {
        if (bestlastent) bestlastent->m_rpoolinfo.next = bestent->m_rpoolinfo.next;
        else m_rlist = (RTYPE *)bestent->m_rpoolinfo.next;
      }

      m_mutex.Leave();
      return bestent;
    }

    int WDL_POOLLIST_refcnt;
    char *WDL_POOLLIST_identstr;
    bool m_hadres;

    EXTRAINFOTYPE *extraInfo;

    RTYPE *PeekList()
    {
      return m_rlist;
    }
    void LockList()
    {
      m_mutex.Enter();
    }
    void UnlockList()
    {
      m_mutex.Leave();
    }

private:

  WDL_Mutex m_mutex;
  RTYPE *m_rlist;

} WDL_FIXALIGN;



#endif
