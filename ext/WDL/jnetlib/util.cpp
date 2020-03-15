/*
** JNetLib
** Copyright (C) 2000-2001 Nullsoft, Inc.
** Author: Justin Frankel
** File: util.cpp - JNL implementation of basic network utilities
** License: see jnetlib.h
*/

#include "netinc.h"

#include "util.h"

int JNL::open_socketlib()
{
#ifdef _WIN32
  WSADATA wsaData;
  if (WSAStartup(MAKEWORD(1, 1), &wsaData)) return 1;
#endif
  return 0;
}
void JNL::close_socketlib()
{
#ifdef _WIN32
  WSACleanup();
#endif
}
unsigned int JNL::ipstr_to_addr(const char *cp) 
{ 
  return ::inet_addr(cp); 
}

void JNL::addr_to_ipstr(unsigned int addr, char *host, int maxhostlen) 
{ 
  struct in_addr a; a.s_addr=addr;
  char *p=::inet_ntoa(a); strncpy(host,p?p:"",maxhostlen);
}
