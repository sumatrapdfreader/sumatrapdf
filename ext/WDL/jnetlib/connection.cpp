/*
** JNetLib
** Copyright (C) 2008 Cockos Inc
** Copyright (C) 2000-2001 Nullsoft, Inc.
** Author: Justin Frankel
** File: connection.cpp - JNL TCP connection implementation
** License: see jnetlib.h
*/

#include "netinc.h"
#include "util.h"
#include "connection.h"


JNL_Connection::JNL_Connection(JNL_IAsyncDNS *dns, int sendbufsize, int recvbufsize)
{
  m_errorstr="";
  if (dns == JNL_CONNECTION_AUTODNS)
  {
    m_dns=new JNL_AsyncDNS();
    m_dns_owned=1;
  }
  else
  {
    m_dns=dns;
    m_dns_owned=0;
  }
  m_recv_buffer.Resize(recvbufsize);
  m_send_buffer.Resize(sendbufsize);
  m_socket=INVALID_SOCKET;
  m_remote_port=0;
  m_state=STATE_NOCONNECTION;
  m_localinterfacereq=INADDR_ANY;
  m_recv_len=m_recv_pos=0;
  m_send_len=m_send_pos=0;
  m_host[0]=0;
  m_saddr = new struct sockaddr_in;
  memset(m_saddr,0,sizeof(struct sockaddr_in));
}

void JNL_Connection::connect(SOCKET s, struct sockaddr_in *loc)
{
  close(1);
  m_socket=s;
  m_remote_port=0;
  m_dns=NULL;
  if (loc) *m_saddr=*loc;
  else memset(m_saddr,0,sizeof(struct sockaddr_in));
  if (m_socket != INVALID_SOCKET)
  {
    SET_SOCK_DEFAULTS(m_socket);
    SET_SOCK_BLOCK(m_socket,0);
    m_state=STATE_CONNECTED;
  }
  else 
  {
    m_errorstr="invalid socket passed to connect";
    m_state=STATE_ERROR;
  }
}

void JNL_Connection::connect(const char *hostname, int port)
{
  close(1);
  m_remote_port=(short)port;
  m_socket=::socket(AF_INET,SOCK_STREAM,0);
  if (m_socket==INVALID_SOCKET)
  {
    m_errorstr="creating socket";
    m_state=STATE_ERROR;
  }
  else
  {
    if (m_localinterfacereq != INADDR_ANY)
    {
      sockaddr_in sa={0,};
      sa.sin_family=AF_INET;
      sa.sin_addr.s_addr=m_localinterfacereq;
      bind(m_socket,(struct sockaddr *)&sa,16);
    }
    SET_SOCK_DEFAULTS(m_socket);
    SET_SOCK_BLOCK(m_socket,0);
    strncpy(m_host,hostname,sizeof(m_host)-1);
    m_host[sizeof(m_host)-1]=0;
    memset(m_saddr,0,sizeof(struct sockaddr_in));
    if (!m_host[0])
    {
      m_errorstr="empty hostname";
      m_state=STATE_ERROR;
    }
    else
    {
      m_state=STATE_RESOLVING;
      m_saddr->sin_family=AF_INET;
      m_saddr->sin_port=htons((unsigned short)port);
      m_saddr->sin_addr.s_addr=inet_addr(hostname);
    }
  }
}

JNL_Connection::~JNL_Connection()
{
  if (m_socket != INVALID_SOCKET)
  {
    ::shutdown(m_socket, SHUT_RDWR);
    ::closesocket(m_socket);
    m_socket=INVALID_SOCKET;
  }
  if (m_dns_owned) 
  {
    delete m_dns;
  }
  delete m_saddr;
}

void JNL_Connection::run(int max_send_bytes, int max_recv_bytes, int *bytes_sent, int *bytes_rcvd)
{
  int bytes_allowed_to_send=(max_send_bytes<0)?m_send_buffer.GetSize():max_send_bytes;
  int bytes_allowed_to_recv=(max_recv_bytes<0)?m_recv_buffer.GetSize():max_recv_bytes;

  if (bytes_sent) *bytes_sent=0;
  if (bytes_rcvd) *bytes_rcvd=0;

  switch (m_state)
  {
    case STATE_RESOLVING:
      if (m_saddr->sin_addr.s_addr == INADDR_NONE)
      {
        int a=m_dns?m_dns->resolve(m_host,(unsigned int *)&m_saddr->sin_addr.s_addr):-1;
        if (!a) { m_state=STATE_CONNECTING; }
        else if (a == 1)
        {
          m_state=STATE_RESOLVING; 
          break;
        }
        else
        {
          m_errorstr="resolving hostname"; 
          m_state=STATE_ERROR; 
          return;
        }
      }
      if (!::connect(m_socket,(struct sockaddr *)m_saddr,16)) 
      {
        m_state=STATE_CONNECTED;
      }
      else if (JNL_ERRNO!=JNL_EINPROGRESS)
      {
        m_errorstr="connecting to host";
        m_state=STATE_ERROR;
      }
      else { m_state=STATE_CONNECTING; }
    break;
    case STATE_CONNECTING:
      {		
        fd_set f[3];
        FD_ZERO(&f[0]);
        FD_ZERO(&f[1]);
        FD_ZERO(&f[2]);
        FD_SET(m_socket,&f[0]);
        FD_SET(m_socket,&f[1]);
        FD_SET(m_socket,&f[2]);
        struct timeval tv;
        memset(&tv,0,sizeof(tv));
        if (select(
#ifdef _WIN32
          0
#else
          m_socket+1
#endif
          ,&f[0],&f[1],&f[2],&tv)==-1)
        {
          m_errorstr="connecting to host (calling select())";
          m_state=STATE_ERROR;
        }
        else if (FD_ISSET(m_socket,&f[1])) 
        {
          m_state=STATE_CONNECTED;
        }
        else if (FD_ISSET(m_socket,&f[2]))
        {
          m_errorstr="connecting to host";
          m_state=STATE_ERROR;
        }
      }
    break;
    case STATE_CONNECTED:
    case STATE_CLOSING:
      if (m_send_len>0 && bytes_allowed_to_send>0)
      {
        int len=m_send_buffer.GetSize()-m_send_pos;
        if (len > m_send_len) len=m_send_len;
        if (len > bytes_allowed_to_send) len=bytes_allowed_to_send;
        if (len > 0)
        {
          int res=(int)::send(m_socket,(char*)m_send_buffer.Get()+m_send_pos,len,0);
          if (res==-1 && JNL_ERRNO != JNL_EWOULDBLOCK)
          {            
//            m_state=STATE_CLOSED;
//            return;
          }
          if (res>0)
          {
            bytes_allowed_to_send-=res;
            if (bytes_sent) *bytes_sent+=res;
            m_send_pos+=res;
            m_send_len-=res;
          }
        }
        if (m_send_pos>=m_send_buffer.GetSize()) 
        {
          m_send_pos=0;
          if (m_send_len>0)
          {
            len=m_send_buffer.GetSize()-m_send_pos;
            if (len > m_send_len) len=m_send_len;
            if (len > bytes_allowed_to_send) len=bytes_allowed_to_send;
            int res=(int)::send(m_socket,(char*)m_send_buffer.Get()+m_send_pos,len,0);
            if (res==-1 && JNL_ERRNO != JNL_EWOULDBLOCK)
            {
//              m_state=STATE_CLOSED;
            }
            if (res>0)
            {
              bytes_allowed_to_send-=res;
              if (bytes_sent) *bytes_sent+=res;
              m_send_pos+=res;
              m_send_len-=res;
            }
          }
        }
      }
      if (m_recv_len<m_recv_buffer.GetSize())
      {
        int len=m_recv_buffer.GetSize()-m_recv_pos;
        if (len > m_recv_buffer.GetSize()-m_recv_len) len=m_recv_buffer.GetSize()-m_recv_len;
        if (len > bytes_allowed_to_recv) len=bytes_allowed_to_recv;
        if (len>0)
        {
          int res=(int)::recv(m_socket,(char*)m_recv_buffer.Get()+m_recv_pos,len,0);
          if (res == 0 || (res < 0 && JNL_ERRNO != JNL_EWOULDBLOCK))
          {        
            m_state=STATE_CLOSED;
            break;
          }
          if (res > 0)
          {
            bytes_allowed_to_recv-=res;
            if (bytes_rcvd) *bytes_rcvd+=res;
            m_recv_pos+=res;
            m_recv_len+=res;
          }
        }
        if (m_recv_pos >= m_recv_buffer.GetSize())
        {
          m_recv_pos=0;
          if (m_recv_len < m_recv_buffer.GetSize())
          {
            len=m_recv_buffer.GetSize()-m_recv_len;
            if (len > bytes_allowed_to_recv) len=bytes_allowed_to_recv;
            if (len > 0)
            {
              int res=(int)::recv(m_socket,(char*)m_recv_buffer.Get()+m_recv_pos,len,0);
              if (res == 0 || (res < 0 && JNL_ERRNO != JNL_EWOULDBLOCK))
              {        
                m_state=STATE_CLOSED;
                break;
              }
              if (res > 0)
              {
                bytes_allowed_to_recv-=res;
                if (bytes_rcvd) *bytes_rcvd+=res;
                m_recv_pos+=res;
                m_recv_len+=res;
              }
            }
          }
        }
      }
      if (m_state == STATE_CLOSING)
      {
        if (m_send_len < 1) m_state = STATE_CLOSED;
      }
    break;
    default: break;
  }
}

void JNL_Connection::close(int quick)
{
  if (quick || m_state == STATE_RESOLVING || m_state == STATE_CONNECTING)
  {
    m_state=STATE_CLOSED;
    if (m_socket != INVALID_SOCKET)
    {
      ::shutdown(m_socket, SHUT_RDWR);
      ::closesocket(m_socket);
    }
    m_socket=INVALID_SOCKET;
    m_remote_port=0;
    m_recv_len=m_recv_pos=0;
    m_send_len=m_send_pos=0;
    m_host[0]=0;
    memset(m_saddr,0,sizeof(struct sockaddr_in));
  }
  else
  {
    if (m_state == STATE_CONNECTED) m_state=STATE_CLOSING;
  }
}

int JNL_Connection::send_bytes_in_queue(void)
{
  return m_send_len;
}

int JNL_Connection::send_bytes_available(void)
{
  return m_send_buffer.GetSize()-m_send_len;
}

int JNL_Connection::send(const void *_data, int length)
{
  const char *data = static_cast<const char *>(_data);
  if (length > send_bytes_available())
  {
    return -1;
  }
  
  int write_pos=m_send_pos+m_send_len;
  if (write_pos >= m_send_buffer.GetSize()) 
  {
    write_pos-=m_send_buffer.GetSize();
  }

  int len=m_send_buffer.GetSize()-write_pos;
  if (len > length) 
  {
    len=length;
  }

  memcpy(m_send_buffer.Get()+write_pos,data,len);
  if (length > len)
  {
    memcpy(m_send_buffer.Get(),data+len,length-len);
  }
  m_send_len+=length;
  return 0;
}

int JNL_Connection::send_string(const char *line)
{
  return send(line,(int)strlen(line));
}

int JNL_Connection::recv_bytes_available(void)
{
  return m_recv_len;
}

int JNL_Connection::peek_bytes(void *data, int maxlength)
{
  if (maxlength > m_recv_len)
  {
    maxlength=m_recv_len;
  }
  int read_pos=m_recv_pos-m_recv_len;
  if (read_pos < 0) 
  {
    read_pos += m_recv_buffer.GetSize();
  }
  int len=m_recv_buffer.GetSize()-read_pos;
  if (len > maxlength)
  {
    len=maxlength;
  }
  if (data != NULL) {
    memcpy(data,m_recv_buffer.Get()+read_pos,len);
    if (len < maxlength)
    {
      memcpy((char*)data+len,m_recv_buffer.Get(),maxlength-len);
    }
  }

  return maxlength;
}

int JNL_Connection::recv_bytes(void *_data, int maxlength)
{
  char *data = static_cast<char *>(_data);
  
  int ml=peek_bytes(data,maxlength);
  m_recv_len-=ml;
  return ml;
}

int JNL_Connection::getbfromrecv(int pos, int remove)
{
  int read_pos=m_recv_pos-m_recv_len + pos;
  if (pos < 0 || pos > m_recv_len) return -1;
  if (read_pos < 0) 
  {
    read_pos += m_recv_buffer.GetSize();
  }
  if (read_pos >= m_recv_buffer.GetSize())
  {
    read_pos-=m_recv_buffer.GetSize();
  }
  if (remove) m_recv_len--;
  return m_recv_buffer.Get()[read_pos];
}

int JNL_Connection::recv_lines_available(void)
{
  int l=recv_bytes_available();
  int lcount=0;
  int lastch=0;
  int pos;
  for (pos=0; pos < l; pos ++)
  {
    int t=getbfromrecv(pos,0);
    if (t == -1) return lcount;
    if ((t=='\r' || t=='\n') &&(
         (lastch != '\r' && lastch != '\n') || lastch==t
        )) lcount++;
    lastch=t;
  }
  return lcount;
}

int JNL_Connection::recv_get_linelen()
{
  int l = 0;
  while (l < m_recv_len)
  {
    int t=getbfromrecv(l,0);
    if (t<0) return 0;

    if (t == '\r' || t == '\n')
    {
      int r=getbfromrecv(++l,0);
      if ((r == '\r' || r == '\n') && r != t) l++;
      return l;
    }
    l++;
  }
  return 0;
}

int JNL_Connection::recv_line(char *line, int maxlength)
{
  maxlength--; // room for trailing NUL
  if (maxlength > m_recv_len) maxlength=m_recv_len;
  while (maxlength-- > 0)
  {
    int t=getbfromrecv(0,1);
    if (t == -1) 
    {
      *line=0;
      return 0;
    }
    if (t == '\r' || t == '\n')
    {
      int r=getbfromrecv(0,0);
      if ((r == '\r' || r == '\n') && r != t) getbfromrecv(0,1);
      *line=0;
      return 0;
    }
    *line++=(char)t;
  }
  *line=0;
  return 1;
}

void JNL_Connection::set_interface(int useInterface) // call before connect if needed
{
  m_localinterfacereq = useInterface;
}


unsigned int JNL_Connection::get_interface(void)
{
  if (m_socket==INVALID_SOCKET) return 0;
  struct sockaddr_in sin;
  memset(&sin,0,sizeof(sin));
  socklen_t len=16;
  if (::getsockname(m_socket,(struct sockaddr *)&sin,&len)) return 0;
  return (unsigned int) sin.sin_addr.s_addr;
}

unsigned int JNL_Connection::get_remote()
{
  return m_saddr->sin_addr.s_addr;
}

short JNL_Connection::get_remote_port()
{
  return m_remote_port;
}
