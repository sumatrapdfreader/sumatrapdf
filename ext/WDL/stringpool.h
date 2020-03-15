#ifndef _WDL_STRINGPOOL_H_
#define _WDL_STRINGPOOL_H_


#include "wdlstring.h"
#include "assocarray.h"
#include "mutex.h"

class WDL_StringPool
{
  friend class WDL_PooledString;
public:
  WDL_StringPool(bool wantMutex) : m_strings(true) { m_mutex = wantMutex ? new WDL_Mutex : NULL; };
  ~WDL_StringPool() { delete m_mutex; }

private:
  WDL_StringKeyedArray<int> m_strings;
protected:
  WDL_Mutex *m_mutex;

};

class WDL_PooledString
{
public:
  WDL_PooledString(WDL_StringPool *pool, const char *value=NULL) { m_pool = pool; m_val = ""; Set(value); }
  ~WDL_PooledString() { Set(""); }

  const char *Get() { return m_val; }

  void Set(const char *value)
  {
    if (!value) value="";
    if (strcmp(value,m_val))
    {
      WDL_MutexLock(m_pool->m_mutex); // may or may not actually be a mutex

      const char *oldval = m_val;

      if (*value)
      {
        // set to new value
        const char *keyptr=NULL;
        int * ref = m_pool->m_strings.GetPtr(value,&keyptr);
        if (ref)
        {
          ++*ref;
          m_val=keyptr;
        }
        else m_pool->m_strings.Insert(value,1,&m_val);
      }
      else m_val="";

      if (oldval[0])
      {
        int *oldref = m_pool->m_strings.GetPtr(oldval);
        if (oldref && --*oldref<=0) m_pool->m_strings.Delete(oldval);
      }
    }
  }

  // utility for compat with WDL_String
  void Append(const char *value)
  {
    if (value&&*value)
    {
      WDL_String tmp(Get());
      tmp.Append(value);
      Set(tmp.Get());
    }
  }
  void Insert(const char *value, int pos)
  {
    WDL_String tmp(Get());
    tmp.Insert(value,pos);
    Set(tmp.Get());
  }
  void DeleteSub(int pos, int len)
  {
    WDL_String tmp(Get());
    tmp.DeleteSub(pos,len);
    Set(tmp.Get());
  }

private:
  const char *m_val;
  WDL_StringPool *m_pool;

};


#endif //_WDL_STRINGPOOL_H_
