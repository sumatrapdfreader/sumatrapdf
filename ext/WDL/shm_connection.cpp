#include "shm_connection.h"

#ifdef _WIN32
#define SHM_MINSIZE 64
#define SHM_HDRSIZE (4+4*4+2)
  // 4 bytes: buffer size (each channel)
  // 4 bytes: read pointer chan 0
  // 4 bytes: write pointer chan 0
  // 4 bytes: read pointer chan 1
  // 4 bytes: write pointer chan 1
  // 2 bytes: chan 0 refresh, chan 1 refresh
  // data follows (2x buffer size)


WDL_SHM_Connection::WDL_SHM_Connection(bool whichChan,
                      const char *uniquestring, // identify 
                      int shmsize, // bytes, whoever opens first decides
                      int timeout_sec,
                      int extra_flags // unused on win32

                    )
{
  m_timeout_cnt=0;
  m_timeout_sec=timeout_sec;
  m_last_recvt=time(NULL)+2; // grace period
  { // make shmsize the next power of two
    int a = shmsize;
    shmsize=2;
    while (shmsize < SHM_MINSIZE || shmsize<a) shmsize*=2;
  }

  m_file=INVALID_HANDLE_VALUE;
  m_filemap=NULL;
  m_mem=NULL;
  m_lockmutex=m_events[0]=m_events[1]=NULL;

  m_whichChan=whichChan ? 1 : 0;

  char buf[512];
  GetTempPath(sizeof(buf)-4,buf);
  if (!buf[0]) strcpy(buf,"C:\\");
  if (buf[strlen(buf)-1] != '/' && buf[strlen(buf)-1] != '\\') strcat(buf,"\\");
  m_tempfn.Set(buf);
  m_tempfn.Append("WDL_SHM_");
  m_tempfn.Append(uniquestring);
  m_tempfn.Append(".tmp");

  WDL_String tmp("Global\\WDL_SHM_");
#ifdef WDL_SUPPORT_WIN9X
  if (GetVersion()&0x80000000) tmp.Set("WDL_SHM_");
#endif
  tmp.Append(uniquestring);
  const size_t tmp_l = strlen(tmp.Get());

  tmp.Append(".m");
  HANDLE mutex = CreateMutex(NULL,FALSE,tmp.Get());

  if (mutex) WaitForSingleObject(mutex,INFINITE);

  tmp.Get()[tmp_l]=0;
  tmp.Append(whichChan?".l1":".l0");
  m_lockmutex = CreateMutex(NULL,FALSE,tmp.Get());
  if (m_lockmutex)
  {
    if (WaitForSingleObject(m_lockmutex,100) == WAIT_OBJECT_0)
    {
      DeleteFile(m_tempfn.Get()); // this is designed to fail if another process has it locked

      m_file=CreateFile(m_tempfn.Get(),GENERIC_READ|GENERIC_WRITE,
                        FILE_SHARE_READ|FILE_SHARE_WRITE ,
                        NULL,whichChan ? OPEN_EXISTING : OPEN_ALWAYS,FILE_ATTRIBUTE_TEMPORARY,NULL);
    }
    else
    {
      CloseHandle(m_lockmutex);
      m_lockmutex=0;
    }
  }
  
  int mapsize;
  if (m_file != INVALID_HANDLE_VALUE && 
        ((mapsize=GetFileSize(m_file,NULL)) < SHM_HDRSIZE+SHM_MINSIZE*2 || 
          mapsize == 0xFFFFFFFF))
  {
    char buf[4096];
    memset(buf,0,sizeof(buf));
    *(int *)buf=shmsize;

    int sz=shmsize*2 + SHM_HDRSIZE;
    while (sz>0)
    {
      DWORD d;
      int a = sz;
      if (a>sizeof(buf))a=sizeof(buf);
      WriteFile(m_file,buf,a,&d,NULL);
      sz-=a;
      *(int *)buf = 0;
    }
  }

  if (m_file!=INVALID_HANDLE_VALUE)
    m_filemap=CreateFileMapping(m_file,NULL,PAGE_READWRITE,0,0,NULL);

  if (m_filemap)
  {
    m_mem=(unsigned char *)MapViewOfFile(m_filemap,FILE_MAP_WRITE,0,0,0);

    tmp.Get()[tmp_l]=0;
    tmp.Append(".1");
    m_events[0]=CreateEvent(NULL,false,false,tmp.Get());
    tmp.Get()[strlen(tmp.Get())-1]++; 
    m_events[1]=CreateEvent(NULL,false,false,tmp.Get());
  }

  if (mutex) 
  {
    ReleaseMutex(mutex);
    CloseHandle(mutex);
  }

}


WDL_SHM_Connection::~WDL_SHM_Connection()
{
  if (m_mem) UnmapViewOfFile(m_mem);
  if (m_filemap) CloseHandle(m_filemap);
  if (m_file != INVALID_HANDLE_VALUE) CloseHandle(m_file);
  DeleteFile(m_tempfn.Get());

  if (m_events[0]) CloseHandle(m_events[0]);
  if (m_events[1]) CloseHandle(m_events[1]);
  if (m_lockmutex)
  {
    ReleaseMutex(m_lockmutex);
    CloseHandle(m_lockmutex);
  }
}

bool WDL_SHM_Connection::WantSendKeepAlive() { return false; }

int WDL_SHM_Connection::Run()
{
  if (!m_mem) return -1;

  int *hdr = (int *)m_mem;

  int shm_size = hdr[0];

  // todo: check to see if we just opened, if so, have a grace period
  if (shm_size < SHM_MINSIZE) return -1; 

  m_mem[4*5 + !!m_whichChan] = 1;
  if (m_timeout_sec > 0)
  {
    if (m_mem[4*5 + !m_whichChan])
    {
      m_timeout_cnt=0;
      m_last_recvt=time(NULL);
      m_mem[4*5 + !m_whichChan]=0;
    }
    else
    {
      if (time(NULL) > m_timeout_sec+m_last_recvt) 
      {
        if (m_timeout_cnt >= 4) return -1;

        m_timeout_cnt++;
        m_last_recvt=time(NULL);
      }
    }
  }

  int didStuff=0;

  // process writes
  int send_avail=send_queue.Available();
  if (send_avail>0)
  {
    int wc = !m_whichChan;
    unsigned char *data=m_mem+SHM_HDRSIZE+shm_size*wc;
    int rdptr = hdr[1 + wc*2];  // hopefully atomic
    int wrptr = hdr[1 + wc*2+1];
    int wrlen = shm_size - (wrptr-rdptr);
    if (wrlen > 0)
    {
      if (wrlen > send_avail) wrlen=send_avail;
      if (wrlen > shm_size) wrlen=shm_size; // should never happen !

      int idx = wrptr & (shm_size-1);
      int l = shm_size - idx;
      if (l > wrlen) l = wrlen;
      memcpy(data+idx,send_queue.Get(),l);
      if (l < wrlen) memcpy(data,(char*)send_queue.Get() + l,wrlen-l);

      hdr[1 + wc*2+1] = wrptr + wrlen; // advance write pointer, hopefluly atomic

      didStuff|=1;

      send_queue.Advance(wrlen);
      send_queue.Compact();

    }
  }

  // process reads
  {
    int wc = m_whichChan;
    unsigned char *data=m_mem+SHM_HDRSIZE+shm_size*wc;
    int rdptr = hdr[1 + wc*2]; 
    int wrptr = hdr[1 + wc*2+1]; // hopefully atomic
    int rdlen = wrptr-rdptr;
    if (rdlen > 0)
    {
      if (rdlen > shm_size) rdlen=shm_size; // should never happen !

      int idx = rdptr & (shm_size-1);
      int l = shm_size - idx;
      if (l > rdlen) l = rdlen;
      recv_queue.Add(data+idx,l);
      if (l < rdlen) recv_queue.Add(data,rdlen-l);

      hdr[1 + wc*2] = wrptr; // hopefully atomic, bring read pointer up to write pointer
      didStuff|=2;
    }
  }

  if (didStuff)
  {
    if (m_events[!m_whichChan]) SetEvent(m_events[!m_whichChan]);
    return 1;
  }


  return 0;
}


#else

#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#ifndef __APPLE__
#include <sys/file.h>
#endif
#include "swell/swell-internal.h"

static void sigpipehandler(int sig) { }

// socket version
WDL_SHM_Connection::WDL_SHM_Connection(bool whichChan, // first created must be whichChan=0
                      const char *uniquestring, // identify 
                      int shmsize, 
                      int timeout_sec,
                      int extra_flags // set 1 for lockfile use on master
                      )
{
  m_sockbufsize = shmsize;
  if (m_sockbufsize<16384) m_sockbufsize=16384;
  else if (m_sockbufsize>1024*1024) m_sockbufsize=1024*1024;

  m_rdbufsize = wdl_min(m_sockbufsize,65536);
  m_rdbuf = (char *)malloc(m_rdbufsize);

  static bool hasSigHandler;
  if (!hasSigHandler) 
  {
    signal(SIGPIPE,sigpipehandler);
    hasSigHandler=true;
  }
  m_timeout_cnt=0;
  m_timeout_sec=timeout_sec;
  m_last_recvt=time(NULL)+3; // grace period
  m_next_keepalive = time(NULL)+1;
  
  m_tempfn.Set("/tmp/WDL_SHM.");
  m_tempfn.Append(uniquestring);
  m_tempfn.Append(".tmp");
  

  m_sockaddr=malloc(sizeof(struct sockaddr_un) + strlen(m_tempfn.Get()));
  m_lockhandle=-1;
  m_listen_socket=-1;
  m_socket=-1;
  m_waitevt=0;
  m_whichChan = whichChan;

  struct sockaddr_un *addr = (struct sockaddr_un *)m_sockaddr;
  addr->sun_family = AF_UNIX;
  strcpy(addr->sun_path,m_tempfn.Get());
  #ifdef __APPLE__
    int l = SUN_LEN(addr)+1;
    if (l>255)l=255;
    addr->sun_len=l;
  #endif

  if (!whichChan)
  {
    if (extra_flags & 1)
    {
      m_lockfn.Set(m_tempfn.Get());
      m_lockfn.Append(".lock");
      m_lockhandle = open(m_lockfn.Get(),O_RDWR|O_CREAT
#ifdef O_CLOEXEC
          |O_CLOEXEC
#endif
          ,0666);
      if (m_lockhandle < 0) return; // error getting lockfile, fail
      if (flock(m_lockhandle,LOCK_NB|LOCK_EX) < 0)
      {
        close(m_lockhandle);
        m_lockhandle=-1;
        return; // could not lock
      }
    }
    acquireListener();
    if (m_listen_socket<0) return;
  }
  else
  {
    struct stat sbuf;
    if (stat(addr->sun_path,&sbuf))
    {
      return; // fail
    }

    int s = socket(AF_UNIX,SOCK_STREAM,0);
    if (s<0) return;

#ifdef __APPLE__
    { int __flags=1; setsockopt(s, SOL_SOCKET, SO_NOSIGPIPE, &__flags, sizeof(__flags)); }
#endif
    int bsz=m_sockbufsize;
    setsockopt(s,SOL_SOCKET,SO_SNDBUF,(char *)&bsz,sizeof(bsz));
    bsz=m_sockbufsize;
    setsockopt(s,SOL_SOCKET,SO_RCVBUF,(char *)&bsz,sizeof(bsz));
    
    if (connect(s,(struct sockaddr*)addr,SUN_LEN(addr))) 
    {
      close(s);
    }
    else
    {
      fcntl(s, F_SETFL, fcntl(s,F_GETFL) | O_NONBLOCK);
      m_socket=s;

      // clean up the filesystem, our connection has been made
      unlink(m_tempfn.Get());
    }
  } 

  if (m_socket>=0 || m_listen_socket>=0)
  {
    SWELL_InternalObjectHeader_SocketEvent *se = (SWELL_InternalObjectHeader_SocketEvent*)malloc(sizeof(SWELL_InternalObjectHeader_SocketEvent));
    memset(se,0,sizeof(SWELL_InternalObjectHeader_SocketEvent));
    se->hdr.type = INTERNAL_OBJECT_EXTERNALSOCKET;
    se->hdr.count = 1;
    se->socket[0]=m_socket>=0? m_socket : m_listen_socket;
    m_waitevt = (HANDLE)se;
  }
}

WDL_SHM_Connection::~WDL_SHM_Connection()
{
  if (m_listen_socket>=0 || m_socket>=0)
  {
    if (m_socket>=0) close(m_socket);
    if (m_listen_socket>=0) close(m_listen_socket);

    // only delete temp socket file if the master and successfully had something open
    if (!m_whichChan && m_tempfn.Get()[0]) unlink(m_tempfn.Get());
  }

  free(m_waitevt); // don't CloseHandle(), since it's just referencing our socket
  free(m_sockaddr);
  free(m_rdbuf);

  if (m_lockhandle>=0)
  {
    flock(m_lockhandle,LOCK_UN);
    close(m_lockhandle);
    unlink(m_lockfn.Get());
  }
}

bool WDL_SHM_Connection::WantSendKeepAlive() 
{ 
  return !send_queue.GetSize() && time(NULL) >= m_next_keepalive;
}


int WDL_SHM_Connection::Run()
{
  if (m_socket < 0)
  {
    if (m_listen_socket < 0) return -1;

    struct sockaddr_un remote={0,};
    socklen_t t = sizeof(struct sockaddr_un);
    int s = accept(m_listen_socket,(struct sockaddr *)&remote,&t);
    if (s>=0)
    {
      close(m_listen_socket);
      m_listen_socket=-1;

      fcntl(s, F_SETFL, fcntl(s,F_GETFL) | O_NONBLOCK); // nonblocking
#ifdef __APPLE__
      { int __flags=1; setsockopt(s, SOL_SOCKET, SO_NOSIGPIPE, &__flags, sizeof(__flags)); }
#endif

      int bsz=m_sockbufsize;
      setsockopt(s,SOL_SOCKET,SO_SNDBUF,(char *)&bsz,sizeof(bsz));
      bsz=m_sockbufsize;
      setsockopt(s,SOL_SOCKET,SO_RCVBUF,(char *)&bsz,sizeof(bsz));

      if (m_waitevt)
      { 
        SWELL_InternalObjectHeader_SocketEvent *se = (SWELL_InternalObjectHeader_SocketEvent*)m_waitevt;
        se->socket[0]=s;
      }
      m_socket=s;
    }
    else 
    {
      if (m_timeout_sec>0 && time(NULL) > m_timeout_sec+m_last_recvt) 
      {
        if (m_timeout_cnt >= 2) return -1;
        m_timeout_cnt++;
        m_last_recvt=time(NULL);
      }
      return 0;
    }
  }

  bool sendcnt=false;
  bool recvcnt=false;
  for (;;)
  {
    bool hadAct=false;
    while (recv_queue.Available()<128*1024*1024) 
    {
      int n=read(m_socket,m_rdbuf,m_rdbufsize);
      if (n>0)
      {
        recv_queue.Add(m_rdbuf,n);
        hadAct=true;
        recvcnt=true;
      }
      else if (n<0&&errno!=EAGAIN) goto abortClose;
      else break;
    }
    while (send_queue.Available()>0)
    {
      int n = send_queue.Available();
      if (n > m_rdbufsize) n=m_rdbufsize;
      n = write(m_socket,send_queue.Get(),n);
      if (n > 0)
      {
        hadAct=true;
        sendcnt=true;
        send_queue.Advance(n); 
      }
      else if (n<0&&errno!=EAGAIN) goto abortClose;
      else break;
    }
    if (!hadAct) break;  
  }
  if (sendcnt) send_queue.Compact();
  
  if (m_timeout_sec>0)
  {
    time_t now = time(NULL);
    if (recvcnt) 
    {
      m_last_recvt=now; 
      m_timeout_cnt=0;
    }
    else if (now > m_timeout_sec+m_last_recvt) 
    {
      if (m_timeout_cnt >= 3) return -1;
      m_timeout_cnt++;
      m_last_recvt=now; 
    }
    
    if (sendcnt||send_queue.GetSize()) m_next_keepalive = now + (m_timeout_sec+1)/2;
  }
  
  return sendcnt||recvcnt;

abortClose:
  if (m_whichChan) return -1;

  acquireListener();
  recv_queue.Clear();
  send_queue.Clear();
  if (m_waitevt)
  { 
    SWELL_InternalObjectHeader_SocketEvent *se = (SWELL_InternalObjectHeader_SocketEvent*)m_waitevt;
    se->socket[0]=m_listen_socket;
  }
  close(m_socket); 
  m_socket=-1; 

  return m_listen_socket >= 0 ? 0 : -1; 
}

void WDL_SHM_Connection::acquireListener()
{
  // only ever called from whichChan==0
  if (m_listen_socket>=0) return; // no need to re-open

  unlink(m_tempfn.Get());

  int s = socket(AF_UNIX,SOCK_STREAM,0);
  if (s<0) return;
#ifdef __APPLE__
  { int __flags=1; setsockopt(s, SOL_SOCKET, SO_NOSIGPIPE, &__flags, sizeof(__flags)); }
#endif
  struct sockaddr_un *addr = (struct sockaddr_un *)m_sockaddr;
  if (bind(s,(struct sockaddr*)addr,SUN_LEN(addr)) < 0 || listen(s,1) < 0)
  {
    close(s);
  }
  else
  {
    fcntl(s, F_SETFL, fcntl(s,F_GETFL) | O_NONBLOCK); 
    m_listen_socket = s;
  }
}
#endif
