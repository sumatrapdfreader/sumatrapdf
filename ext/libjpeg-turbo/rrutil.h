/* Copyright (C)2004 Landmark Graphics Corporation
 * Copyright (C)2005 Sun Microsystems, Inc.
 * Copyright (C)2010 D. R. Commander
 *
 * This library is free software and may be redistributed and/or modified under
 * the terms of the wxWindows Library License, Version 3.1 or (at your option)
 * any later version.  The full license is in the LICENSE.txt file included
 * with this distribution.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * wxWindows Library License for more details.
 */

#ifndef __RRUTIL_H__
#define __RRUTIL_H__

#ifdef _WIN32
	#include <windows.h>
	#define sleep(t) Sleep((t)*1000)
	#define usleep(t) Sleep((t)/1000)
#else
	#include <unistd.h>
	#define stricmp strcasecmp
	#define strnicmp strncasecmp
#endif

#ifndef min
 #define min(a,b) ((a)<(b)?(a):(b))
#endif

#ifndef max
 #define max(a,b) ((a)>(b)?(a):(b))
#endif

#define pow2(i) (1<<(i))
#define isPow2(x) (((x)&(x-1))==0)

#ifdef sgi
#define _SC_NPROCESSORS_CONF _SC_NPROC_CONF
#endif

#ifdef sun
#define __inline inline
#endif

static __inline int numprocs(void)
{
	#ifdef _WIN32
	DWORD_PTR ProcAff, SysAff, i;  int count=0;
	if(!GetProcessAffinityMask(GetCurrentProcess(), &ProcAff, &SysAff)) return(1);
	for(i=0; i<sizeof(long*)*8; i++) if(ProcAff&(1LL<<i)) count++;
	return(count);
	#elif defined (__APPLE__)
	return(1);
	#else
	long count=1;
	if((count=sysconf(_SC_NPROCESSORS_CONF))!=-1) return((int)count);
	else return(1);
	#endif
}

#define byteswap(i) ( \
	(((i) & 0xff000000) >> 24) | \
	(((i) & 0x00ff0000) >>  8) | \
	(((i) & 0x0000ff00) <<  8) | \
	(((i) & 0x000000ff) << 24) )

#define byteswap16(i) ( \
	(((i) & 0xff00) >> 8) | \
	(((i) & 0x00ff) << 8) )

static __inline int littleendian(void)
{
	unsigned int value=1;
	unsigned char *ptr=(unsigned char *)(&value);
	if(ptr[0]==1 && ptr[3]==0) return 1;
	else return 0;
}

#endif
