/*
** JNetLib
** Copyright (C) 2008 Cockos Inc
** Copyright (C) 2000-2001 Nullsoft, Inc.
** Author: Justin Frankel
** File: listen.cpp - JNL TCP listen implementation
** License: see jnetlib.h
*/

#include "netinc.h"
#include "util.h"
#include "listen.h"

JNL_Listen::JNL_Listen(short port, unsigned int which_interface)
{
  m_port=port;
  m_socket = ::socket(AF_INET,SOCK_STREAM,0);
  if (m_socket == INVALID_SOCKET) 
  {
  }
  else
  {
    struct sockaddr_in sin;
    SET_SOCK_DEFAULTS(m_socket);
    SET_SOCK_BLOCK(m_socket,0);
    int bflag = 1;
    setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&bflag, sizeof(bflag));
    memset((char *) &sin, 0,sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons( (short) port );
    sin.sin_addr.s_addr = which_interface?which_interface:INADDR_ANY;
    if (::bind(m_socket,(struct sockaddr *)&sin,sizeof(sin))) 
    {
      shutdown(m_socket, SHUT_RDWR);
      closesocket(m_socket);
      m_socket=INVALID_SOCKET;
    }
    else
    {  
      if (::listen(m_socket,8)==-1) 
      {
        shutdown(m_socket, SHUT_RDWR);
        closesocket(m_socket);
        m_socket=INVALID_SOCKET;
      }
    }
  }
}

JNL_Listen::~JNL_Listen()
{
  if (m_socket!=INVALID_SOCKET)
  {
    shutdown(m_socket, SHUT_RDWR);
    closesocket(m_socket);
  }
}

JNL_IConnection *JNL_Listen::get_connect(int sendbufsize, int recvbufsize)
{
  if (m_socket == INVALID_SOCKET)
  {
    return NULL;
  }
  struct sockaddr_in saddr;
  socklen_t length = sizeof(struct sockaddr_in);
  SOCKET s = accept(m_socket, (struct sockaddr *) &saddr, &length);
  if (s != INVALID_SOCKET)
  {
    JNL_IConnection *c=new JNL_Connection(NULL,sendbufsize, recvbufsize);
    c->connect(s,&saddr);
    return c;
  }
  return NULL;
}
