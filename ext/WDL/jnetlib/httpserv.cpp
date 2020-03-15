/*
** JNetLib
** Copyright (C) 2008 Cockos Inc
** Copyright (C) 2001 Nullsoft, Inc.
** Author: Justin Frankel
** File: httpserv.cpp - JNL HTTP GET/POST serving implementation
** License: see jnetlib.h
**
** This class just manages the http reply/sending, not where the data 
** comes from, etc.
*/

#include "netinc.h"
#include "util.h"

#include "httpserv.h"

/*
  States for m_state:
    -1 error (connection closed, etc)
    0 not read request yet.
    1 reading headers
    2 headers read, have not sent reply
    3 sent reply
    4 closed
*/

JNL_HTTPServ::JNL_HTTPServ(JNL_IConnection *con)
{
  m_usechunk = false;
  m_keepalive = true;
  m_con=con;
  m_state=0;
  m_reply_ready=0;
}

JNL_HTTPServ::~JNL_HTTPServ()
{
  delete m_con;
}

void JNL_HTTPServ::write_bytes(const char *bytes, int length) 
{ 
  if (m_usechunk)
  {
    char buf[32];
    sprintf(buf,"%x\r\n",length);
    m_con->send_string(buf);
  }
  m_con->send(bytes,length); 
  if (m_usechunk) m_con->send_string("\r\n");
}

bool JNL_HTTPServ::want_keepalive_reset()
{
  if (m_state >= 2 && m_con && m_con->get_state() == JNL_Connection::STATE_CONNECTED)
  {
    m_usechunk = false;
    m_state = 0;
    m_reply_ready = 0;
    m_errstr.Set("");
    m_reply_headers.Set("");
    m_reply_string.Set("");
    m_recvheaders.Clear();
    m_recv_request.Resize(0,false);
    return true;
  }
  return false;
}

int JNL_HTTPServ::run()
{ // returns: < 0 on error, 0 on connection close, 1 if reading request, 2 if reply not sent, 3 if reply sent, sending data.
  int cnt=0;
run_again:
  m_con->run();
  if (m_con->get_state()==JNL_Connection::STATE_ERROR)
  {
    seterrstr(m_con->get_errstr());
    return -1;
  }
  if (m_con->get_state()==JNL_Connection::STATE_CLOSED) return 4;

  if (m_state == 0)
  {
    int reqlen = m_con->recv_get_linelen();
    if (reqlen>0)
    {
      if (!m_recv_request.ResizeOK(reqlen+2,false))
      {
        seterrstr("malloc fail");
        return -1;
      }

      reqlen = m_con->recv_bytes(m_recv_request.Get(),reqlen);
      char *buf = m_recv_request.Get() + reqlen;
      *buf=0;
      while (buf > m_recv_request.Get() && (buf[-1] == '\r' || buf[-1]=='\n')) *--buf=0;
      const char *endptr = buf;
      while (buf >= m_recv_request.Get() && *buf != ' ') buf--;

      if (buf < m_recv_request.Get() || strncmp(buf+1,"HTTP",4) || strncmp(m_recv_request.Get(),"GET ",4))
      {
        seterrstr("malformed HTTP request");
        m_state=-1;
        buf=m_recv_request.Get();
        buf[0]=buf[1]=0;
      }
      else
      {
        if (endptr[-1]=='0') m_keepalive = false; // old http 1.0
        m_state=1;
        cnt=0;
        buf[0]=buf[1]=0;

        buf=strstr(m_recv_request.Get(),"?");
        if (buf)
        {
          *buf++=0; // change &'s into 0s now.
          char *t=buf;
          int stat=1;
          while (*t) 
          {
            if (*t == '&' && !stat) { stat=1; *t=0; }
            else stat=0;
            t++;
          }
        }
      }
    }
    else if (!cnt++) goto run_again;
  }
  if (m_state == 1)
  {
    if (!cnt++ && m_con->recv_lines_available()<1) goto run_again;
    while (m_con->recv_get_linelen()>0)
    {
      char buf[4096];
      buf[0]=0;
      m_con->recv_line(buf,4096);
      if (!buf[0]) { m_state=2; break; }
      
      if (!strnicmp(buf,"Connection:",11))
      {
        const char *p=buf+11;
        while (*p && strnicmp(p,"close",5)) p++;
        if (*p) m_keepalive = false;
      }
      
      if (m_recvheaders.GetSize()) m_recvheaders.Add(NULL,-1); // remove doublenull
      m_recvheaders.Add(buf,strlen(buf)+1);
      m_recvheaders.Add("",1);
    }
  }
  if (m_state == 2)
  {
    if (m_reply_ready)
    {
      // send reply
      m_con->send_string((char*)(m_reply_string.GetLength()?m_reply_string.Get():"HTTP/1.1 200 OK"));
      m_con->send_string("\r\n");
      if (m_reply_headers.GetLength()) m_con->send_string(m_reply_headers.Get());
      if (m_keepalive) 
      {
        const char *p = m_reply_headers.Get();
        bool had_cl=false,had_con=false;
        while (*p && (!had_cl || !had_con))
        {
          if (!strnicmp(p,"Content-Length:",15)) had_cl=true;
          else if (!strnicmp(p,"Connection:",11)) had_con=true;

          while (*p && *p != '\r' && *p != '\n') p++;
          while (*p == '\r' || *p == '\n') p++;
        }
        if (!had_con) m_con->send_string("Connection: keep-alive\r\n");
        if (!had_cl) 
        {
          m_usechunk = true;
          m_con->send_string("Transfer-Encoding: chunked\r\n");
        }
      }
      m_con->send_string("\r\n");
      m_state=3;
    }
  }
  if (m_state == 3)
  {
    // nothing.
  }

  return m_state;
}

const char *JNL_HTTPServ::get_request_file()
{
  // file portion of http request
  char *t=m_recv_request.Get();
  if (!t) return NULL;

  while (*t != ' ' && *t) t++;
  if (!*t) return NULL;
  while (*t == ' ') t++;
  return t;
}

const char *JNL_HTTPServ::get_request_parm(const char *parmname) // parameter portion (after ?)
{
  const char *t=m_recv_request.Get();
  if (!t) return NULL;

  while (*t) t++;
  t++;
  while (*t)
  {
    while (*t == '&') t++;
    if (!strnicmp(t,parmname,strlen(parmname)) && t[strlen(parmname)] == '=')
    {
      return t+strlen(parmname)+1;
    }
    t+=strlen(t)+1;
  }
  return NULL;
}

const char *JNL_HTTPServ::getheader(const char *headername)
{
  const char *ret=NULL;
  if (strlen(headername)<1||!m_recvheaders.Available()) return NULL;
  const char *p=m_recvheaders.Get();
  const int hdrlen = (int) strlen(headername);
  while (*p)
  {
    if (!strnicmp(headername,p,hdrlen) && p[hdrlen] == ':')
    {
      ret=p+hdrlen+1;
      while (*ret == ' ') ret++;
      break;
    }
    while (*p) p++;
    p++;
  }
  return ret;
}

void JNL_HTTPServ::set_reply_string(const char *reply_string) // should be HTTP/1.1 OK or the like
{
  m_reply_string.Set(reply_string);
}

void JNL_HTTPServ::set_reply_size(int sz) // if set, size will also add keep-alive etc
{
  if (sz>=0)
  {
    char buf[512];
    sprintf(buf,"Content-length: %d",sz);
    set_reply_header(buf);
  }
}
void JNL_HTTPServ::set_reply_header(const char *header) // "Connection: close" for example
{
  m_reply_headers.Append(header);
  m_reply_headers.Append("\r\n");
}
