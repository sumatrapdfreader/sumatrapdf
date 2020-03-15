/*
** JNetLib
** Copyright (C) 2000-2001 Nullsoft, Inc.
** Author: Justin Frankel
** File: netinc.h - network includes and portability defines (used internally)
** License: see jnetlib.h
*/

#ifndef _NETINC_H_
#define _NETINC_H_

#ifdef _WIN32

#include <windows.h>
#include <stdio.h>
#include <time.h>
#define JNL_ERRNO (WSAGetLastError())
#define SET_SOCK_BLOCK(s,block) { unsigned long __i=block?0:1; ioctlsocket(s,FIONBIO,&__i); }
#define SET_SOCK_DEFAULTS(s) do { } while (0)
#define JNL_EWOULDBLOCK WSAEWOULDBLOCK
#define JNL_EINPROGRESS WSAEWOULDBLOCK
#define JNL_ENOTCONN WSAENOTCONN

typedef int socklen_t;

#else

#ifndef THREAD_SAFE
#define THREAD_SAFE
#endif
#ifndef _REENTRANT
#define _REENTRANT
#endif
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdarg.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>


#define JNL_ERRNO ((errno)|0)
#define closesocket(s) close(s)
#define SET_SOCK_BLOCK(s,block) { int __flags; if ((__flags = fcntl(s, F_GETFL, 0)) != -1) { if (!block) __flags |= O_NONBLOCK; else __flags &= ~O_NONBLOCK; fcntl(s, F_SETFL, __flags);  } }
#ifdef __APPLE__
#define SET_SOCK_DEFAULTS(s) do { int __flags = 1; setsockopt((s), SOL_SOCKET, SO_NOSIGPIPE, &__flags, sizeof(__flags)); } while (0)
#else
#define SET_SOCK_DEFAULTS(s) do { } while (0)
#endif

typedef int SOCKET;
#define INVALID_SOCKET (-1)

#define JNL_EWOULDBLOCK EWOULDBLOCK
#define JNL_EINPROGRESS EINPROGRESS
#define JNL_ENOTCONN ENOTCONN

#ifndef stricmp
#define stricmp(x,y) strcasecmp(x,y)
#endif
#ifndef strnicmp
#define strnicmp(x,y,z) strncasecmp(x,y,z)
#endif

#endif // !_WIN32

#ifndef INADDR_NONE
#define INADDR_NONE 0xffffffff
#endif

#ifndef INADDR_ANY
#define INADDR_ANY 0
#endif

#ifndef SHUT_RDWR
#define SHUT_RDWR 2
#endif

#endif //_NETINC_H_
