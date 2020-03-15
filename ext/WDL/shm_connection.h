#ifndef _WDL_SHM_CONNECTION_H_
#define _WDL_SHM_CONNECTION_H_

#ifdef _WIN32
#include <windows.h>
#else
#include "swell/swell.h"
#include "swell/swell-internal.h"
#endif

#include <time.h>

#include "wdlstring.h"
#include "wdltypes.h"
#include "queue.h"

class WDL_SHM_Connection
{
public:
  WDL_SHM_Connection(bool whichChan, // a true con connects to a false con -- note on SHM false should be created FIRST.
                     const char *uniquestring, // identify 
                     int shmsize=262144, // bytes, whoever opens first decides
                     int timeout_sec=0,
                     int extra_flags=0 // on posix, set 1 for the master to create a .lock file as well
                     );

  ~WDL_SHM_Connection();

  int Run(); // call as often as possible, returns <0 error, >0 if did something

  bool WantSendKeepAlive(); // called when it needs a keepalive to be sent (may be never, or whatever interval it decides)
  
 // wait for this if you want to see when data comes in
  HANDLE GetWaitEvent() 
  { 
    #ifdef _WIN32
    return m_events[m_whichChan]; 
    #else
    return m_waitevt;
    #endif
  }
  
  // receiving and sending data
  WDL_Queue recv_queue;
  WDL_Queue send_queue;


private:


  int m_timeout_sec;
  time_t m_last_recvt;
  int m_timeout_cnt;

  WDL_String m_tempfn;

  int m_whichChan; // which channel we read from

#ifdef _WIN32

  HANDLE m_file, m_filemap; 
  HANDLE m_events[2]; // [m_whichChan] set when the other side did something useful
  HANDLE m_lockmutex;

  unsigned char *m_mem; 
#else
  time_t m_next_keepalive;
  
  int m_sockbufsize;

  int m_rdbufsize;
  char *m_rdbuf; // m_rdbufsize

  int m_listen_socket;
  int m_socket;

  HANDLE m_waitevt;
  void *m_sockaddr;

  void acquireListener();
  WDL_String m_lockfn;
  int m_lockhandle;
#endif

};

#endif
