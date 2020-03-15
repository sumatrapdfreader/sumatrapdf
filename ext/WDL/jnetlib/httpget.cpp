/*
** JNetLib
** Copyright (C) 2008 Cockos Inc
** Copyright (C) 2000-2001 Nullsoft, Inc.
** Author: Justin Frankel
** File: httpget.cpp - JNL HTTP GET implementation
** License: see jnetlib.h
*/

#include "netinc.h"
#include "util.h"
#include "httpget.h"


JNL_HTTPGet::JNL_HTTPGet(JNL_IAsyncDNS *dns, int recvbufsize, char *proxy)
{
  m_recvbufsize=recvbufsize;
  m_dns=dns;
  m_con=NULL;
  m_http_proxylpinfo=0;
  m_http_proxyhost=0;
  m_http_proxyport=0;
  if (proxy && *proxy)
  {
    char *p=(char*)malloc(strlen(proxy)+1);
    if (p) 
    {
      char *r=NULL;
      strcpy(p,proxy);
      do_parse_url(p,&m_http_proxyhost,&m_http_proxyport,&r,&m_http_proxylpinfo);
      free(r);
      free(p);
    }
  }
  m_sendheaders=NULL;
  reinit();
}

void JNL_HTTPGet::reinit()
{
  m_errstr=0;
  m_recvheaders=NULL;
  m_recvheaders_size=0;
  m_http_state=0;
  m_http_port=0;
  m_http_url=0;
  m_reply=0;
  m_http_host=m_http_lpinfo=m_http_request=NULL;
}

void JNL_HTTPGet::deinit()
{
  delete m_con; m_con = NULL;
  free(m_recvheaders);

  free(m_http_url);
  free(m_http_host);
  free(m_http_lpinfo);
  free(m_http_request);
  free(m_errstr);
  free(m_reply);
  reinit();
}

JNL_HTTPGet::~JNL_HTTPGet()
{
  deinit();
  free(m_sendheaders);
  free(m_http_proxylpinfo);
  free(m_http_proxyhost);

}


void JNL_HTTPGet::addheader(const char *header)
{
  if (strstr(header,"\r") || strstr(header,"\n")) return;
  if (!m_sendheaders)
  {
    m_sendheaders=(char*)malloc(strlen(header)+3);
    if (m_sendheaders) 
    {
      strcpy(m_sendheaders,header);
      strcat(m_sendheaders,"\r\n");
    }
  }
  else
  {
    char *t=(char*)malloc(strlen(header)+strlen(m_sendheaders)+1+2);
    if (t)
    {
      strcpy(t,m_sendheaders);
      strcat(t,header);
      strcat(t,"\r\n");
      free(m_sendheaders);
      m_sendheaders=t;
    }
  }
}

void JNL_HTTPGet::do_encode_mimestr(char *in, char *out)
{
  char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  int shift = 0;
  int accum = 0;

  while (*in)
  {
    if (*in)
    {
      accum <<= 8;
      shift += 8;
      accum |= *in++;
    }
    while ( shift >= 6 )
    {
      shift -= 6;
      *out++ = alphabet[(accum >> shift) & 0x3F];
    }
  }
  if (shift == 4)
  {
    *out++ = alphabet[(accum & 0xF)<<2];
    *out++='=';  
  }
  else if (shift == 2)
  {
    *out++ = alphabet[(accum & 0x3)<<4];
    *out++='=';  
    *out++='=';  
  }

  *out++=0;
}


void JNL_HTTPGet::connect(const char *url, int ver, const char *requestmethod)
{
  deinit();
  m_http_url=(char*)malloc(strlen(url)+1);
  strcpy(m_http_url,url);
  do_parse_url(m_http_url,&m_http_host,&m_http_port,&m_http_request, &m_http_lpinfo);
  strcpy(m_http_url,url);
  if (!m_http_host || !m_http_host[0] || !m_http_port)
  {
    m_http_state=-1;
    seterrstr("invalid URL");
    return;
  }

  size_t sendbufferlen=0;

  if (!m_http_proxyhost || !m_http_proxyhost[0])
  {
    sendbufferlen += strlen(requestmethod)+1 /* GET */ + strlen(m_http_request) + 9 /* HTTP/1.0 */ + 2;
  }
  else
  {
    sendbufferlen += strlen(requestmethod)+1 /* GET */ + strlen(m_http_url) + 9 /* HTTP/1.0 */ + 2;
    if (m_http_proxylpinfo&&m_http_proxylpinfo[0])
    {
      sendbufferlen+=58+strlen(m_http_proxylpinfo)*2; // being safe here
    }
  }
  sendbufferlen += 5 /* Host: */ + strlen(m_http_host) + 2;

  if (m_http_lpinfo&&m_http_lpinfo[0])
  {
    sendbufferlen+=46+strlen(m_http_lpinfo)*2; // being safe here
  }

  if (m_sendheaders) sendbufferlen+=strlen(m_sendheaders);

  char *str=(char*)malloc(sendbufferlen+1024);
  if (!str)
  {
    seterrstr("error allocating memory");
    m_http_state=-1;    
  }

  if (!m_http_proxyhost || !m_http_proxyhost[0])
  {
    sprintf(str,"%s %s HTTP/1.%d\r\n",requestmethod,m_http_request,ver%10);
  }
  else
  {
    sprintf(str,"%s %s HTTP/1.%d\r\n",requestmethod, m_http_url,ver%10);
  }

  sprintf(str+strlen(str),"Host:%s\r\n",m_http_host);

  if (m_http_lpinfo&&m_http_lpinfo[0])
  {
    strcat(str,"Authorization: Basic ");
    do_encode_mimestr(m_http_lpinfo,str+strlen(str));
    strcat(str,"\r\n");
  }
  if (m_http_proxylpinfo&&m_http_proxylpinfo[0])
  {
    strcat(str,"Proxy-Authorization: Basic ");
    do_encode_mimestr(m_http_proxylpinfo,str+strlen(str));
    strcat(str,"\r\n");
  }

  if (m_sendheaders) strcat(str,m_sendheaders);
  strcat(str,"\r\n");

  int a=m_recvbufsize;
  if (a < 4096) a=4096;
  m_con=new JNL_Connection(m_dns,(int)strlen(str)+4,a);
  if (m_con)
  {
    if (!m_http_proxyhost || !m_http_proxyhost[0])
    {
      m_con->connect(m_http_host,m_http_port);
    }
    else
    {
      m_con->connect(m_http_proxyhost,m_http_proxyport);
    }
    m_con->send_string(str);
  }
  else
  {
    m_http_state=-1;
    seterrstr("could not create connection object");
  }
  free(str);

}

void JNL_HTTPGet::do_parse_url(char *url, char **host, int *port, char **req, char **lp)
{
  char *p,*np;
  free(*host); *host=0;
  free(*req); *req=0;
  free(*lp); *lp=0;

  if (strstr(url,"://")) np=p=strstr(url,"://")+3;
  else np=p=url;
  while (*np != '/' && *np) np++;
  if (*np)
  {
    *req=(char*)malloc(strlen(np)+1);
    if (*req) strcpy(*req,np);
    *np++=0;
  } 
  else 
  {
    *req=(char*)malloc(2);
    if (*req) strcpy(*req,"/");
  }

  np=p;
  while (*np != '@' && *np) np++;
  if (*np)
  {
    *np++=0;
    *lp=(char*)malloc(strlen(p)+1);
    if (*lp) strcpy(*lp,p);
    p=np;
  }
  else 
  {
    *lp=(char*)malloc(1);
    if (*lp) strcpy(*lp,"");
  }
  np=p;
  while (*np != ':' && *np) np++;
  if (*np)
  {
    *np++=0;
    *port=atoi(np);
  } else *port=80;
  *host=(char*)malloc(strlen(p)+1);
  if (*host) strcpy(*host,p);
}


const char *JNL_HTTPGet::getallheaders()
{ // double null terminated, null delimited list
  if (m_recvheaders) return m_recvheaders;
  else return "\0\0";
}

const char *JNL_HTTPGet::getheader(const char *headername)
{
  if (!headername || !m_recvheaders) return NULL;

  size_t headername_len = strlen(headername);
  if (headername_len<1) return NULL;

  if (headername[headername_len - 1] == ':') headername_len--;

  const char *p=m_recvheaders;
  while (*p)
  {
    if (!strnicmp(headername,p,headername_len) && p[headername_len] == ':')
    {
      p += headername_len + 1;
      while (*p == ' ') p++;
      return p;
    }
    p+=strlen(p)+1;
  }
  return NULL;
}

int JNL_HTTPGet::run()
{
  int cnt=0;
  if (m_http_state==-1||!m_con) return -1; // error


run_again:
  m_con->run();

  if (m_con->get_state()==JNL_Connection::STATE_ERROR)
  {
    seterrstr(m_con->get_errstr());
    return -1;
  }
  if (m_con->get_state()==JNL_Connection::STATE_CLOSED) return 1;

  if (m_http_state==0) // connected, waiting for reply
  {
    if (m_con->recv_lines_available()>0)
    {
      char buf[4096];
      m_con->recv_line(buf,4095);
      buf[4095]=0;
      m_reply=(char*)malloc(strlen(buf)+1);
      strcpy(m_reply,buf);
    
      int code=getreplycode();
      if (code == 200 || code==206) m_http_state=2; // proceed to read headers normally
      else if (code == 301 || code==302) 
      {
        m_http_state=1; // redirect city
      }
      else 
      {
        seterrstr(buf);
        m_http_state=-1;
        return -1;
      }
      cnt=0;
    }
    else if (!cnt++) goto run_again;
  }
  if (m_http_state == 1) // redirect
  {
    while (m_con->recv_lines_available() > 0)
    {
      char buf[4096];
      m_con->recv_line(buf,4096);
      if (!buf[0])  
      {
        m_http_state=-1;
        return -1;
      }
      if (!strnicmp(buf,"Location:",9))
      {
        const char *p=buf+9; while (*p== ' ') p++;
        if (*p)
        {
          connect(p);
          return 0;
        }
      }
    }
  }
  if (m_http_state==2)
  {
    if (!cnt++ && m_con->recv_lines_available() < 1) goto run_again;
    while (m_con->recv_lines_available() > 0)
    {
      char buf[4096];
      m_con->recv_line(buf,4096);
      if (!buf[0]) { m_http_state=3; break; }
      if (!m_recvheaders)
      {
        m_recvheaders_size=(int)strlen(buf)+1;
        m_recvheaders=(char*)malloc(m_recvheaders_size+1);
        if (m_recvheaders)
        {
          strcpy(m_recvheaders,buf);
          m_recvheaders[m_recvheaders_size]=0;
        }
      }
      else
      {
        int oldsize=m_recvheaders_size;
        m_recvheaders_size+=(int)strlen(buf)+1;
        char *n=(char*)malloc(m_recvheaders_size+1);
        if (n)
        {
          memcpy(n,m_recvheaders,oldsize);
          strcpy(n+oldsize,buf);
          n[m_recvheaders_size]=0;
          free(m_recvheaders);
          m_recvheaders=n;
        }
      }
    }
  }
  if (m_http_state==3)
  {
  }
  return 0;
}

int JNL_HTTPGet::get_status() // returns 0 if connecting, 1 if reading headers, 
                    // 2 if reading content, -1 if error.
{
  if (m_http_state < 0) return -1;
  if (m_http_state < 2) return 0;
  if (m_http_state == 2) return 1;
  if (m_http_state == 3) return 2;
  return -1;
}

int JNL_HTTPGet::getreplycode()// returns 0 if none yet, otherwise returns http reply code.
{
  if (!m_reply) return 0;
  char *p=m_reply;
  while (*p && *p != ' ') p++; // skip over HTTP/x.x
  if (!*p) return 0;
  return atoi(++p);
}

int JNL_HTTPGet::bytes_available()
{
  if (m_con && m_http_state==3) return m_con->recv_bytes_available();
  return 0;
}
int JNL_HTTPGet::get_bytes(char *buf, int len)
{
  if (m_con && m_http_state==3) return m_con->recv_bytes(buf,len);
  return 0;
}
int JNL_HTTPGet::peek_bytes(char *buf, int len)
{
  if (m_con && m_http_state==3) return m_con->peek_bytes(buf,len);
  return 0;
}
