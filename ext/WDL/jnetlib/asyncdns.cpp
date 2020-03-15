/*
** JNetLib
** Copyright (C) 2008 Cockos Inc
** Copyright (C) 2000-2001 Nullsoft, Inc.
** Author: Justin Frankel
** File: asyncdns.cpp - JNL portable asynchronous DNS implementation
** License: see jnetlib.h
*/

#include "netinc.h"
#include "util.h"
#include "asyncdns.h"
#ifdef _WIN32
#include <process.h>
#endif

JNL_AsyncDNS::JNL_AsyncDNS(int max_cache_entries)
{
  m_thread_kill=1;
  m_thread=0;
  m_cache_size=max_cache_entries;
  m_cache=(cache_entry *)::malloc(sizeof(cache_entry)*m_cache_size);
  if (m_cache) memset(m_cache,0,sizeof(cache_entry)*m_cache_size);
  else m_cache_size = 0;
}

JNL_AsyncDNS::~JNL_AsyncDNS()
{
#ifndef NO_DNS_SUPPORT
  m_thread_kill=1;

#ifdef _WIN32
  if (m_thread)
  {
    WaitForSingleObject(m_thread,INFINITE);
    CloseHandle(m_thread);
  }
#else
  if (m_thread)
  {
    void *p;
    pthread_join(m_thread,&p);
  }
#endif//!_WIN32
#endif//NO_DNS_SUPPORT
  free(m_cache);
}

#ifdef _WIN32
unsigned WINAPI JNL_AsyncDNS::_threadfunc(void *_d)
#else
unsigned int JNL_AsyncDNS::_threadfunc(void *_d)
#endif
{
#ifndef NO_DNS_SUPPORT
  int nowinsock=JNL::open_socketlib();
  JNL_AsyncDNS *_this=(JNL_AsyncDNS*)_d;
  int x;
  for (x = 0; x < _this->m_cache_size && !_this->m_thread_kill; x ++)
  {
    if (_this->m_cache[x].last_used && !_this->m_cache[x].resolved)
    {
      if (!nowinsock) 
      {
        if (_this->m_cache[x].mode==0)
        {
          struct hostent *hostentry;
          hostentry=::gethostbyname(_this->m_cache[x].hostname);
          if (hostentry)
          {
            _this->m_cache[x].addr=*((int*)hostentry->h_addr);
          }
          else
            _this->m_cache[x].addr=INADDR_NONE;
        }
        else if (_this->m_cache[x].mode==1)
        {
          struct hostent *ent;
          ent=::gethostbyaddr((const char *)&_this->m_cache[x].addr,4,AF_INET);
          if (ent)
          {
            strncpy(_this->m_cache[x].hostname,ent->h_name,255);
            _this->m_cache[x].hostname[255]=0;
          }
          else
          {
            _this->m_cache[x].hostname[0]=0;
          }
        }
        _this->m_cache[x].resolved=1;
      }
      else
      {
        if (_this->m_cache[x].mode==0)
        {
          _this->m_cache[x].addr=INADDR_NONE;
          _this->m_cache[x].resolved=1;
        }
        else if (_this->m_cache[x].mode==1)
        {
          _this->m_cache[x].hostname[0]=0;
          _this->m_cache[x].resolved=1;
        }
      }
    }
  }
  if (!nowinsock) JNL::close_socketlib();
  _this->m_thread_kill=1;
#endif // NO_DNS_SUPPORT

  return 0;
}

int JNL_AsyncDNS::resolve(const char *hostname, unsigned int *addr)
{
  // return 0 on success, 1 on wait, -1 on unresolvable
  int x;
  unsigned int ip=inet_addr(hostname);
  if (ip != INADDR_NONE) 
  {
    *addr=ip;
    return 0;
  }
#ifndef NO_DNS_SUPPORT

  for (x = 0; x < m_cache_size; x ++)
  {
    if (!stricmp(m_cache[x].hostname,hostname) && m_cache[x].mode==0)
    {
      m_cache[x].last_used=time(NULL);
      if (m_cache[x].resolved)
      {
        if (m_cache[x].addr == INADDR_NONE)
        {
          return -1;
        }
        *addr=m_cache[x].addr;
        return 0;
      }
      makesurethreadisrunning();
      return 1;
    }
  }
  // add to resolve list
  int oi=-1;
  for (x = 0; x < m_cache_size; x ++)
  {
    if (!m_cache[x].last_used)
    {
      oi=x;
      break;
    }
    if ((oi==-1 || m_cache[x].last_used < m_cache[oi].last_used) && m_cache[x].resolved)
    {
      oi=x;
    }
  }
  if (oi == -1)
  {
    return -1;
  }
  strcpy(m_cache[oi].hostname,hostname);
  m_cache[oi].mode=0;
  m_cache[oi].addr=INADDR_NONE;
  m_cache[oi].resolved=0;
  m_cache[oi].last_used=time(NULL);

  makesurethreadisrunning();
  return 1;
#else
  return -1;
#endif
}

int JNL_AsyncDNS::reverse(unsigned int addr, char *hostname)
{
  // return 0 on success, 1 on wait, -1 on unresolvable
  int x;
  if (addr == INADDR_NONE) 
  {
    return -1;
  }
#ifndef NO_DNS_SUPPORT
  for (x = 0; x < m_cache_size; x ++)
  {
    if (m_cache[x].addr==addr && m_cache[x].mode==1)
    {
      m_cache[x].last_used=time(NULL);
      if (m_cache[x].resolved)
      {
        if (!m_cache[x].hostname[0])
        {
          return -1;
        }
        strncpy(hostname,m_cache[x].hostname,255);
        hostname[255]=0;
        return 0;
      }
      makesurethreadisrunning();
      return 1;
    }
  }
  // add to resolve list
  int oi=-1;
  for (x = 0; x < m_cache_size; x ++)
  {
    if (!m_cache[x].last_used)
    {
      oi=x;
      break;
    }
    if ((oi==-1 || m_cache[x].last_used < m_cache[oi].last_used) && m_cache[x].resolved)
    {
      oi=x;
    }
  }
  if (oi == -1)
  {
    return -1;
  }
  m_cache[oi].addr=addr;
  m_cache[oi].hostname[0]=0;
  m_cache[oi].resolved=0;
  m_cache[oi].mode=1;
  m_cache[oi].last_used=time(NULL);

  makesurethreadisrunning();
  return 1;
#else
  return -1;
#endif
}


void JNL_AsyncDNS::makesurethreadisrunning(void)
{
#ifndef NO_DNS_SUPPORT
  if (m_thread_kill)
  {
  #ifdef _WIN32
    if (m_thread)
    {
      WaitForSingleObject(m_thread,INFINITE);
      CloseHandle(m_thread);
    }
    unsigned id;
    m_thread_kill=0;
    m_thread=(HANDLE)_beginthreadex(NULL,0,_threadfunc,(void *)this,0,&id);
    if (!m_thread)
    {
  #else
    if (m_thread)
    {
      void *p;
      pthread_join(m_thread,&p);
    }
    m_thread_kill=0;
    if (pthread_create(&m_thread,NULL,(void *(*) (void *))_threadfunc,(void*)this) != 0)
    {
  #endif
      m_thread_kill=1;
    }
  }
#endif//NO_DNS_SUPPORT
}
