/*
** JNetLib
** Copyright (C) 2008 Cockos Inc
** Copyright (C) 2000-2001 Nullsoft, Inc.
** Author: Justin Frankel
** File: asyncdns.h - JNL portable asynchronous DNS interface
** License: see jnetlib.h
**
** Usage:
**   1. Create JNL_AsyncDNS object, optionally with the number of cache entries.
**   2. call resolve() to resolve a hostname into an address. The return value of 
**      resolve is 0 on success (host successfully resolved), 1 on wait (meaning
**      try calling resolve() with the same hostname in a few hundred milliseconds 
**      or so), or -1 on error (i.e. the host can't resolve).
**   3. call reverse() to do reverse dns (ala resolve()).
**   4. enjoy.
*/

#ifndef _ASYNCDNS_H_
#define _ASYNCDNS_H_

#include <time.h>

#ifndef JNL_NO_DEFINE_INTERFACES
class JNL_IAsyncDNS
{
public:
  virtual ~JNL_IAsyncDNS() { }
  virtual int resolve(const char *hostname, unsigned int *addr)=0; // return 0 on success, 1 on wait, -1 on unresolvable
  virtual int reverse(unsigned int addr, char *hostname)=0; // return 0 on success, 1 on wait, -1 on unresolvable. hostname must be at least 256 bytes.
};
#define JNL_AsyncDNS_PARENTDEF : public JNL_IAsyncDNS
#else 
#define JNL_IAsyncDNS JNL_AsyncDNS
#define JNL_AsyncDNS_PARENTDEF
#endif


#ifndef JNL_NO_IMPLEMENTATION

class JNL_AsyncDNS JNL_AsyncDNS_PARENTDEF
{
public:
  JNL_AsyncDNS(int max_cache_entries=64);
  ~JNL_AsyncDNS();

  int resolve(const char *hostname, unsigned int *addr); // return 0 on success, 1 on wait, -1 on unresolvable
  int reverse(unsigned int addr, char *hostname); // return 0 on success, 1 on wait, -1 on unresolvable. hostname must be at least 256 bytes.

private:
  typedef struct 
  {
    time_t last_used; // timestamp.
    char resolved;
    char mode; // 1=reverse
    char hostname[256];
    unsigned int addr;
  } 
  cache_entry;

  cache_entry *m_cache;
  int m_cache_size;
  volatile int m_thread_kill;
#ifdef _WIN32
  HANDLE m_thread;
  static unsigned WINAPI _threadfunc(void *_d);
#else
  pthread_t m_thread;
  static unsigned int _threadfunc(void *_d);
#endif
  void makesurethreadisrunning(void);

};
#endif // !JNL_NO_IMPLEMENTATION

#endif //_ASYNCDNS_H_
