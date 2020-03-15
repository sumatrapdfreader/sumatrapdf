#ifndef _EEL_NET_H_
#define _EEL_NET_H_

// x = tcp_listen(port[,interface, connected_ip_out]) poll this, returns connection id  > 0, or <0 on error, or 0 if no new connect -- interface only valid on first call (or after tcp_listen_end(port))
// tcp_listen_end(port); 

// connection = tcp_connect(host, port[, block]) // connection id > 0 on ok
// tcp_set_block(connection, block?)
// tcp_close(connection)

// tcp_send(connection, string[, length]) // can return 0 if block, -1 if error, otherwise returns length sent
// tcp_recv(connection, string[, maxlength]) // 0 on nothing, -1 on error, otherwise returns length recv'd


// need:
// #define EEL_NET_GET_CONTEXT(opaque) (((sInst *)opaque)->m_net_state)

// you must pass a JNL_AsyncDNS object to eel_net_state to support nonblocking connect with DNS resolution, otherwise DNS will block
// #define EEL_NET_NO_SYNC_DNS -- never ever call gethostbyname() synchronously, may disable DNS for blocking connect, or if a JNL_IAsyncDNS is not provided.

#ifndef EEL_NET_MAXSEND
#define EEL_NET_MAXSEND (EEL_STRING_MAXUSERSTRING_LENGTH_HINT+4096)
#endif


#include "../jnetlib/netinc.h"
#define JNL_NO_IMPLEMENTATION
#include "../jnetlib/asyncdns.h"

class eel_net_state
{
  public:
    enum { STATE_FREE=0, STATE_RESOLVING, STATE_CONNECTED, STATE_ERR };
    enum { CONNECTION_ID_BASE=0x110000 };

    eel_net_state(int max_con, JNL_IAsyncDNS *dns);
    ~eel_net_state();

    struct connection_state {
      char *hostname; // set during resolve only
      SOCKET sock;
      int state; // STATE_RESOLVING...
      int port;
      bool blockmode;
    };
    WDL_TypedBuf<connection_state> m_cons;
    WDL_IntKeyedArray<SOCKET> m_listens;
    JNL_IAsyncDNS *m_dns;

    EEL_F onConnect(char *hostNameOwned, int port, int block);
    EEL_F onClose(void *opaque, EEL_F handle);
    EEL_F set_block(void *opaque, EEL_F handle, bool block);
    EEL_F onListen(void *opaque, EEL_F handle, int mode, EEL_F *ifStr, EEL_F *ipOut);

    int __run_connect(connection_state *cs, unsigned int ip);
    int __run(connection_state *cs);
    int do_send(void *opaque, EEL_F h, const char *src, int len);
    int do_recv(void *opaque, EEL_F h, char *buf, int maxlen);
  #ifdef _WIN32
    bool m_had_socketlib_init;
  #endif
};

eel_net_state::eel_net_state(int max_con, JNL_IAsyncDNS *dns)
{
  #ifdef _WIN32
    m_had_socketlib_init=false;
  #endif

  m_cons.Resize(max_con);
  int x;
  for (x=0;x<m_cons.GetSize();x++)
  {
    m_cons.Get()[x].state = STATE_FREE;
    m_cons.Get()[x].sock = INVALID_SOCKET;
    m_cons.Get()[x].hostname = NULL;
  }
  m_dns=dns;
}

eel_net_state::~eel_net_state()
{
  int x;
  for (x=0;x<m_cons.GetSize();x++)
  {
    SOCKET s=m_cons.Get()[x].sock;
    if (s != INVALID_SOCKET) 
    {
      shutdown(s,SHUT_RDWR);
      closesocket(s);
    }
    free(m_cons.Get()[x].hostname);
  }
  for (x=0;x<m_listens.GetSize();x++)
  {
    SOCKET s=m_listens.Enumerate(x);
    shutdown(s, SHUT_RDWR);
    closesocket(s);
  }
}
EEL_F eel_net_state::onConnect(char *hostNameOwned, int port, int block)
{
  int x;
#ifdef _WIN32
  if (!m_had_socketlib_init)
  {
    m_had_socketlib_init=1;
    WSADATA wsaData;
    WSAStartup(MAKEWORD(1, 1), &wsaData);
  }
#endif
  for(x=0;x<m_cons.GetSize();x++)
  {
    connection_state *s=m_cons.Get()+x;
    if (s->state == STATE_FREE)
    {
      unsigned int ip=inet_addr(hostNameOwned);
      if (m_dns && ip == INADDR_NONE && !block)
      {
        const int r=m_dns->resolve(hostNameOwned,&ip);
        if (r<0) break; // error!

        if (r>0) ip = INADDR_NONE;
      }
#ifndef EEL_NET_NO_SYNC_DNS
      else if (ip == INADDR_NONE)
      {
        struct hostent *he = gethostbyname(hostNameOwned);
        if (he) ip = *(int *)he->h_addr;
      }
#endif
      if (hostNameOwned || ip != INADDR_NONE)
      {
        if (ip != INADDR_NONE)
        {
          free(hostNameOwned);
          hostNameOwned=NULL;
        }

        s->state = STATE_RESOLVING;
        s->hostname = hostNameOwned;
        s->blockmode = !!block;
        s->port = port;
        if (hostNameOwned || __run_connect(s,ip)) return x + CONNECTION_ID_BASE;

        s->state=STATE_FREE;
        s->hostname=NULL;
      }
      break;
    }
  }
  free(hostNameOwned);
  return -1;
}

EEL_F eel_net_state::onListen(void *opaque, EEL_F handle, int mode, EEL_F *ifStr, EEL_F *ipOut)
{
  const int port = (int) handle;
  if (port < 1 || port > 65535) 
  {
#ifdef EEL_STRING_DEBUGOUT
    EEL_STRING_DEBUGOUT("tcp_listen(%d): invalid port specified, will never succeed",port);
#endif
    return 0.0;
  }
#ifdef _WIN32
  if (!m_had_socketlib_init)
  {
    m_had_socketlib_init=1;
    WSADATA wsaData;
    WSAStartup(MAKEWORD(1, 1), &wsaData);
  }
#endif
  SOCKET *sockptr = m_listens.GetPtr(port);
  if (mode<0)
  {
    if (!sockptr) return -1.0;

    SOCKET ss=*sockptr;
    m_listens.Delete(port);
    if (ss != INVALID_SOCKET) 
    {
      shutdown(ss, SHUT_RDWR);
      closesocket(ss);
    }
    return 0.0;
  }
  if (!sockptr)
  {
    struct sockaddr_in sin;
    memset((char *) &sin, 0,sizeof(sin));
    if (ifStr)
    {
      EEL_STRING_MUTEXLOCK_SCOPE
      const char *fn = EEL_STRING_GET_FOR_INDEX(*ifStr,NULL);
#ifdef EEL_STRING_DEBUGOUT
      if (!fn) EEL_STRING_DEBUGOUT("tcp_listen(%d): bad string identifier %f for second parameter (interface)",port,*ifStr);
#endif
      if (fn && *fn) sin.sin_addr.s_addr=inet_addr(fn);
      
    }
    if (!sin.sin_addr.s_addr || sin.sin_addr.s_addr==INADDR_NONE) sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_family = AF_INET;
    sin.sin_port = htons( (short) port );
    SOCKET sock = socket(AF_INET,SOCK_STREAM,0);
    if (sock != INVALID_SOCKET)
    {
      SET_SOCK_DEFAULTS(sock);
      SET_SOCK_BLOCK(sock,0);
      if (bind(sock,(struct sockaddr *)&sin,sizeof(sin)) || listen(sock,8)==-1) 
      {
        shutdown(sock, SHUT_RDWR);
        closesocket(sock);
        sock=INVALID_SOCKET;
      }
    }
#ifdef EEL_STRING_DEBUGOUT
    //if (sock == INVALID_SOCKET) EEL_STRING_DEBUGOUT("tcp_listen(%d): failed listening on port",port);
    // we report -1 to the caller, no need to error message
#endif
    m_listens.Insert(port,sock);
    sockptr = m_listens.GetPtr(port);
  }
  if (!sockptr || *sockptr == INVALID_SOCKET) return -1;

  struct sockaddr_in saddr;
  socklen_t length = sizeof(struct sockaddr_in);
  SOCKET newsock = accept(*sockptr, (struct sockaddr *) &saddr, &length);
  if (newsock == INVALID_SOCKET) 
  {
    return 0; // nothing to report here
  }
  SET_SOCK_DEFAULTS(newsock);

  int x;
  for(x=0;x<m_cons.GetSize();x++)
  {
    connection_state *cs=m_cons.Get()+x;
    if (cs->state == STATE_FREE)
    {
      cs->state=STATE_CONNECTED;
      free(cs->hostname);
      cs->hostname=NULL;
      cs->sock = newsock;
      cs->blockmode=true;
      cs->port=0;
      if (ipOut)
      {
        EEL_STRING_MUTEXLOCK_SCOPE
        WDL_FastString *ws=NULL;
        EEL_STRING_GET_FOR_WRITE(*ipOut,&ws);
        if (ws)
        {
          const unsigned int a = ntohl(saddr.sin_addr.s_addr);
          ws->SetFormatted(128,"%d.%d.%d.%d",(a>>24)&0xff,(a>>16)&0xff,(a>>8)&0xff,a&0xff);
        }
        else
        {
#ifdef EEL_STRING_DEBUGOUT
          EEL_STRING_DEBUGOUT("tcp_listen(%d): bad string identifier %f for third parameter (IP-out)",port,*ipOut);
#endif
        }
      }
      return x + CONNECTION_ID_BASE;
    }
  }
  shutdown(newsock, SHUT_RDWR);
  closesocket(newsock);
  return -1;


}

int eel_net_state::__run_connect(connection_state *cs, unsigned int ip)
{
  SOCKET s=socket(AF_INET,SOCK_STREAM,0);
  if (s == INVALID_SOCKET) return 0;
  SET_SOCK_DEFAULTS(s);

  if (!cs->blockmode) SET_SOCK_BLOCK(s,0);

  struct sockaddr_in sa={0,};
  sa.sin_family=AF_INET;
  sa.sin_addr.s_addr = ip;
  sa.sin_port = htons(cs->port);
  if (!connect(s,(struct sockaddr *)&sa,16) || (!cs->blockmode && JNL_ERRNO == JNL_EINPROGRESS))
  {
    cs->state = STATE_CONNECTED;
    cs->sock = s;
    return 1;
  }
  shutdown(s, SHUT_RDWR);
  closesocket(s);
  return 0;
}

int eel_net_state::__run(connection_state *cs)
{
  if (cs->sock != INVALID_SOCKET) return 0;

  if (!cs->hostname) return -1;

  unsigned int ip=INADDR_NONE;
  const int r=m_dns ? m_dns->resolve(cs->hostname,&ip) : -1;
  if (r>0) return 0;

  free(cs->hostname);
  cs->hostname=NULL;

  if (r<0 || !__run_connect(cs,ip)) 
  {
    cs->state = STATE_ERR;
    return -1;
  }

  return 0;
}

int eel_net_state::do_recv(void *opaque, EEL_F h, char *buf, int maxlen)
{
  const int idx=(int)h-CONNECTION_ID_BASE;
  if (idx>=0 && idx<m_cons.GetSize())
  {
    connection_state *s=m_cons.Get()+idx;
#ifdef EEL_STRING_DEBUGOUT
    if (s->sock == INVALID_SOCKET && !s->hostname) 
      EEL_STRING_DEBUGOUT("tcp_recv: connection identifier %f is not open",h);
#endif
    if (__run(s) || s->sock == INVALID_SOCKET) return s->state == STATE_ERR ? -1 : 0;

    if (maxlen == 0) return 0;

    const int rv=(int)recv(s->sock,buf,maxlen,0);
    if (rv < 0 && !s->blockmode && (JNL_ERRNO == JNL_EWOULDBLOCK || JNL_ERRNO == JNL_ENOTCONN)) return 0;

    if (!rv) return -1; // TCP, 0=connection terminated

    return rv;
  }
#ifdef EEL_STRING_DEBUGOUT
  EEL_STRING_DEBUGOUT("tcp_recv: connection identifier %f is out of range",h);
#endif
  return -1;
}

int eel_net_state::do_send(void *opaque, EEL_F h, const char *src, int len)
{
  const int idx=(int)h-CONNECTION_ID_BASE;
  if (idx>=0 && idx<m_cons.GetSize())
  {
    connection_state *s=m_cons.Get()+idx;
#ifdef EEL_STRING_DEBUGOUT
    if (s->sock == INVALID_SOCKET && !s->hostname) 
      EEL_STRING_DEBUGOUT("tcp_send: connection identifier %f is not open",h);
#endif
    if (__run(s) || s->sock == INVALID_SOCKET) return s->state == STATE_ERR ? -1 : 0;
    const int rv=(int)send(s->sock,src,len,0);
    if (rv < 0 && !s->blockmode && (JNL_ERRNO == JNL_EWOULDBLOCK || JNL_ERRNO == JNL_ENOTCONN)) return 0;
    return rv;
  }
  else
  {
#ifdef EEL_STRING_DEBUGOUT
    EEL_STRING_DEBUGOUT("tcp_send: connection identifier %f out of range",h);
#endif
  }
  return -1;
}

EEL_F eel_net_state::set_block(void *opaque, EEL_F handle, bool block)
{
  int idx=(int)handle-CONNECTION_ID_BASE;
  if (idx>=0 && idx<m_cons.GetSize())
  {
    connection_state *s=m_cons.Get()+idx;
    if (s->blockmode != block)
    {
      s->blockmode=block;
      if (s->sock != INVALID_SOCKET) 
      {
        SET_SOCK_BLOCK(s->sock,(block?1:0));
      }
      else
      {
#ifdef EEL_STRING_DEBUGOUT
        if (!s->hostname) EEL_STRING_DEBUGOUT("tcp_set_block: connection identifier %f is not open",handle);
#endif
      }
      return 1;
    }
  }
  else
  {
#ifdef EEL_STRING_DEBUGOUT
    EEL_STRING_DEBUGOUT("tcp_set_block: connection identifier %f out of range",handle);
#endif
  }
  return 0;
}

EEL_F eel_net_state::onClose(void *opaque, EEL_F handle)
{
  int idx=(int)handle-CONNECTION_ID_BASE;
  if (idx>=0 && idx<m_cons.GetSize())
  {
    connection_state *s=m_cons.Get()+idx;
    const bool hadhn = !!s->hostname;
    free(s->hostname);
    s->hostname = NULL;
    s->state = STATE_ERR;
    if (s->sock != INVALID_SOCKET)
    {
      shutdown(s->sock,SHUT_RDWR);
      closesocket(s->sock);
      s->sock = INVALID_SOCKET;
      return 1.0;
    }
    else if (!hadhn)
    {
#ifdef EEL_STRING_DEBUGOUT
      EEL_STRING_DEBUGOUT("tcp_close: connection identifier %f is not open",handle);
#endif
    }
  }
  else
  {
#ifdef EEL_STRING_DEBUGOUT
    EEL_STRING_DEBUGOUT("tcp_close: connection identifier %f is out of range",handle);
#endif
  }
  return 0.0;
}


static EEL_F NSEEL_CGEN_CALL _eel_tcp_connect(void *opaque, INT_PTR np, EEL_F **parms)
{
  eel_net_state *ctx;
  if (np > 1 && NULL != (ctx=EEL_NET_GET_CONTEXT(opaque)))
  {
    char *dest=NULL;
    {
      EEL_STRING_MUTEXLOCK_SCOPE
      const char *fn = EEL_STRING_GET_FOR_INDEX(parms[0][0],NULL);
      if (fn) dest=strdup(fn);
      else
      {
#ifdef EEL_STRING_DEBUGOUT
         EEL_STRING_DEBUGOUT("tcp_connect(): host string identifier %f invalid",parms[0][0]);
#endif
      }
    }
    if (dest) return ctx->onConnect(dest, (int) (parms[1][0]+0.5), np < 3 || parms[2][0] >= 0.5);
  }
  return -1.0;
}

static EEL_F NSEEL_CGEN_CALL _eel_tcp_set_block(void *opaque, EEL_F *handle, EEL_F *bl)
{
  eel_net_state *ctx;
  if (NULL != (ctx=EEL_NET_GET_CONTEXT(opaque))) return ctx->set_block(opaque,*handle, *bl >= 0.5);
  return 0;
}

static EEL_F NSEEL_CGEN_CALL _eel_tcp_close(void *opaque, EEL_F *handle)
{
  eel_net_state *ctx;
  if (NULL != (ctx=EEL_NET_GET_CONTEXT(opaque))) return ctx->onClose(opaque,*handle);
  return 0;
}

static EEL_F NSEEL_CGEN_CALL _eel_tcp_recv(void *opaque, INT_PTR np, EEL_F **parms)
{
  eel_net_state *ctx;
  if (np > 1 && NULL != (ctx=EEL_NET_GET_CONTEXT(opaque)))
  {
    char buf[EEL_STRING_MAXUSERSTRING_LENGTH_HINT];
    int ml = np > 2 ? (int)parms[2][0] : 4096;
    if (ml < 0 || ml > EEL_STRING_MAXUSERSTRING_LENGTH_HINT) ml = EEL_STRING_MAXUSERSTRING_LENGTH_HINT;

    ml=ctx->do_recv(opaque,parms[0][0],buf,ml);

    {
      EEL_STRING_MUTEXLOCK_SCOPE
      WDL_FastString *ws=NULL;
      EEL_STRING_GET_FOR_WRITE(parms[1][0],&ws);
      if (ws)
      {
        if (ml<=0) ws->Set("");
        else ws->SetRaw(buf,ml);
      }
    }
    return ml;
  }
  return -1;
}

static EEL_F NSEEL_CGEN_CALL _eel_tcp_send(void *opaque, INT_PTR np, EEL_F **parms)
{
  eel_net_state *ctx;
  if (np > 1 && NULL != (ctx=EEL_NET_GET_CONTEXT(opaque)))
  {
    char buf[EEL_NET_MAXSEND];

    int l;
    {
      EEL_STRING_MUTEXLOCK_SCOPE
      WDL_FastString *ws=NULL;
      const char *fn = EEL_STRING_GET_FOR_INDEX(parms[1][0],&ws);
      l = ws ? ws->GetLength() : (int) strlen(fn);
      if (np > 2)
      {
        int al=(int)parms[2][0];
        if (al<0) al=0;
        if (al<l) l=al;
      }
      if (l > 0) memcpy(buf,fn,l);
    }
    if (l>0) return ctx->do_send(opaque,parms[0][0],buf,l);
    return 0;
  }
  return -1;
}

static EEL_F NSEEL_CGEN_CALL _eel_tcp_listen(void *opaque, INT_PTR np, EEL_F **parms)
{
  eel_net_state *ctx;
  if (NULL != (ctx=EEL_NET_GET_CONTEXT(opaque))) return ctx->onListen(opaque,parms[0][0],1,np>1?parms[1]:NULL,np>2?parms[2]:NULL);
  return 0;
}

static EEL_F NSEEL_CGEN_CALL _eel_tcp_listen_end(void *opaque, EEL_F *handle)
{
  eel_net_state *ctx;
  if (NULL != (ctx=EEL_NET_GET_CONTEXT(opaque))) return ctx->onListen(opaque,*handle,-1,NULL,NULL);
  return 0;
}


void EEL_tcp_register()
{
  NSEEL_addfunc_varparm("tcp_listen",1,NSEEL_PProc_THIS,&_eel_tcp_listen);
  NSEEL_addfunc_retval("tcp_listen_end",1,NSEEL_PProc_THIS,&_eel_tcp_listen_end);

  NSEEL_addfunc_varparm("tcp_connect",2,NSEEL_PProc_THIS,&_eel_tcp_connect);
  NSEEL_addfunc_varparm("tcp_send",2,NSEEL_PProc_THIS,&_eel_tcp_send);
  NSEEL_addfunc_varparm("tcp_recv",2,NSEEL_PProc_THIS,&_eel_tcp_recv);

  NSEEL_addfunc_retval("tcp_set_block",2,NSEEL_PProc_THIS,&_eel_tcp_set_block);
  NSEEL_addfunc_retval("tcp_close",1,NSEEL_PProc_THIS,&_eel_tcp_close);
}

#ifdef EEL_WANT_DOCUMENTATION
const char *eel_net_function_reference = 
  "tcp_listen\tport[,\"interface\",#ip_out]\tListens on port specified. Returns less than 0 if could not listen, 0 if no new connection available, or greater than 0 (as a TCP connection ID) if a new connection was made. If a connection made and #ip_out specified, it will be set to the remote IP. interface can be empty for all interfaces, otherwise an interface IP as a string.\0"
  "tcp_listen_end\tport\tEnds listening on port specified.\0"
  "tcp_connect\t\"address\",port[,block]\tCreate a new TCP connection to address:port. If block is specified and 0, connection will be made nonblocking. Returns TCP connection ID greater than 0 on success.\0"
  "tcp_send\tconnection,\"str\"[,len]\tSends a string to connection. Returns -1 on error, 0 if connection is non-blocking and would block, otherwise returns length sent. If len is specified and not less than 1, only the first len bytes of the string parameter will be sent.\0"
  "tcp_recv\tconnection,#str[,maxlen]\tReceives data from a connection to #str. If maxlen is specified, no more than maxlen bytes will be received. If non-blocking, 0 will be returned if would block. Returns less than 0 if error.\0"
  "tcp_set_block\tconnection,block\tSets whether a connection blocks.\0"
  "tcp_close\tconnection\tCloses a TCP connection created by tcp_listen() or tcp_connect().\0"
;
#endif

#endif
