//C-  -*- C++ -*-
//C- -------------------------------------------------------------------
//C- DjVuLibre-3.5
//C- Copyright (c) 2002  Leon Bottou and Yann Le Cun.
//C- Copyright (c) 2001  AT&T
//C-
//C- This software is subject to, and may be distributed under, the
//C- GNU General Public License, either Version 2 of the license,
//C- or (at your option) any later version. The license should have
//C- accompanied the software or you may obtain a copy of the license
//C- from the Free Software Foundation at http://www.fsf.org .
//C-
//C- This program is distributed in the hope that it will be useful,
//C- but WITHOUT ANY WARRANTY; without even the implied warranty of
//C- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//C- GNU General Public License for more details.
//C- 
//C- DjVuLibre-3.5 is derived from the DjVu(r) Reference Library from
//C- Lizardtech Software.  Lizardtech Software has authorized us to
//C- replace the original DjVu(r) Reference Library notice by the following
//C- text (see doc/lizard2002.djvu and doc/lizardtech2007.djvu):
//C-
//C-  ------------------------------------------------------------------
//C- | DjVu (r) Reference Library (v. 3.5)
//C- | Copyright (c) 1999-2001 LizardTech, Inc. All Rights Reserved.
//C- | The DjVu Reference Library is protected by U.S. Pat. No.
//C- | 6,058,214 and patents pending.
//C- |
//C- | This software is subject to, and may be distributed under, the
//C- | GNU General Public License, either Version 2 of the license,
//C- | or (at your option) any later version. The license should have
//C- | accompanied the software or you may obtain a copy of the license
//C- | from the Free Software Foundation at http://www.fsf.org .
//C- |
//C- | The computer code originally released by LizardTech under this
//C- | license and unmodified by other parties is deemed "the LIZARDTECH
//C- | ORIGINAL CODE."  Subject to any third party intellectual property
//C- | claims, LizardTech grants recipient a worldwide, royalty-free, 
//C- | non-exclusive license to make, use, sell, or otherwise dispose of 
//C- | the LIZARDTECH ORIGINAL CODE or of programs derived from the 
//C- | LIZARDTECH ORIGINAL CODE in compliance with the terms of the GNU 
//C- | General Public License.   This grant only confers the right to 
//C- | infringe patent claims underlying the LIZARDTECH ORIGINAL CODE to 
//C- | the extent such infringement is reasonably necessary to enable 
//C- | recipient to make, have made, practice, sell, or otherwise dispose 
//C- | of the LIZARDTECH ORIGINAL CODE (or portions thereof) and not to 
//C- | any greater extent that may be necessary to utilize further 
//C- | modifications or combinations.
//C- |
//C- | The LIZARDTECH ORIGINAL CODE is provided "AS IS" WITHOUT WARRANTY
//C- | OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
//C- | TO ANY WARRANTY OF NON-INFRINGEMENT, OR ANY IMPLIED WARRANTY OF
//C- | MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
//C- +------------------------------------------------------------------

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#if NEED_GNUG_PRAGMAS
# pragma implementation
#endif

#include "GException.h"
#include "GThreads.h"
#include "GOS.h"
#include "GURL.h"

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include <string.h>

#if defined(__CYGWIN32__)
# define UNIX 1
#endif

#if defined(_WIN32) && !defined(UNIX)
# include <windows.h>
# include <string.h>
# include <direct.h>
# define getcwd _getcwd
#endif

#if defined(OS2)
# define INCL_DOS
# include <os2.h>
#endif

#if defined(macintosh) && !defined(UNIX)
# include <unix.h>
# include <errno.h>
# include <unistd.h>
#endif

#if defined(UNIX) || defined(OS2)
# include <errno.h>
# include <sys/types.h>
# include <sys/stat.h>
# include <sys/time.h>
# include <fcntl.h>
# include <pwd.h>
# include <stdio.h>
# include <unistd.h>
#endif


// -- TRUE FALSE
#undef TRUE
#undef FALSE
#define TRUE 1
#define FALSE 0

// -- MAXPATHLEN
#ifndef MAXPATHLEN
# ifdef _MAX_PATH
#  define MAXPATHLEN _MAX_PATH
# else
#  define MAXPATHLEN 1024
# endif
#else
# if ( MAXPATHLEN < 1024 )
#  undef MAXPATHLEN
#  define MAXPATHLEN 1024
# endif
#endif

#ifdef HAVE_NAMESPACES
namespace DJVU {
# ifdef NOT_DEFINED // Just to fool emacs c++ mode
}
#endif
#endif


#if defined(AUTOCONF) && !defined(HAVE_STRERROR)
# define NEED_STRERROR
#elif defined(sun) && !defined(__svr4__) && !defined(__SVR4)
# define NEED_STRERROR
#elif defined(REIMPLEMENT_STRERROR)
# define NEED_STRERROR
#endif
#ifdef NEED_STRERROR
char *
strerror(int errno)
{
  extern int sys_nerr;
  extern char *sys_errlist[];
  if (errno>0 && errno<sys_nerr) 
    return sys_errlist[errno];
  return (char*) "unknown stdio error";
}
#endif



// -----------------------------------------
// Functions for dealing with filenames
// -----------------------------------------

static inline int
finddirsep(const GUTF8String &fname)
{
#if defined(_WIN32)
  return fname.rcontains("\\/",0);
#elif defined(UNIX)
  return fname.rsearch('/',0);
#elif defined(macintosh)
  return fname.rcontains(":/",0);
#else
# error "Define something here for your operating system"
#endif  
}


// basename(filename[, suffix])
// -- returns the last component of filename and removes suffix
//    when present. works like /bin/basename.
GUTF8String 
GOS::basename(const GUTF8String &gfname, const char *suffix)
{
  if(!gfname.length())
    return gfname;

  const char *fname=gfname;
#if defined(_WIN32) || defined(OS2)
  // Special cases
  if (fname[1] == ':')
  {
    if(!fname[2])
    {
      return gfname;
    }
    if (!fname[3] && (fname[2]== '/' || fname[2]== '\\'))
    {
      char string_buffer[4];
      string_buffer[0] = fname[0];
      string_buffer[1] = ':';
      string_buffer[2] = '\\';
      string_buffer[3] = 0; 
      return string_buffer;
    }
  }
#endif


  // Allocate buffer
  GUTF8String retval(gfname,finddirsep(gfname)+1,(unsigned int)(-1));
  fname=retval;

  // Process suffix
  if (suffix)
  {
    if (suffix[0]== '.' )
      suffix ++;
    if (suffix[0])
    {
      const GUTF8String gsuffix(suffix);
      const int sl = gsuffix.length();
      const char *s = fname + strlen(fname);
      if (s > fname + sl)
      {
        s = s - (sl + 1);
        if(*s == '.' && (GUTF8String(s+1).downcase() == gsuffix.downcase()))
        {
          retval.setat((int)((size_t)s-(size_t)fname),0);
        }
      }
    }
  }
  return retval;
}



// errmsg --
// -- A small helper function returning a 
//    stdio error message in a static buffer.

static GNativeString 
errmsg()
{
  GNativeString buffer;
  const char *errname = strerror(errno);
  buffer.format("%s (errno = %d)", errname, errno);
  return buffer;
}



// -----------------------------------------
// Functions for measuring time
// -----------------------------------------

// ticks() --
// -- returns the number of milliseconds elapsed since 
//    a system dependent date.
unsigned long 
GOS::ticks()
{
#if defined(UNIX)
  struct timeval tv;
  if (gettimeofday(&tv, NULL) < 0)
    G_THROW(errmsg());
  return (unsigned long)( ((tv.tv_sec & 0xfffff)*1000) 
                          + (tv.tv_usec/1000) );
#elif defined(_WIN32)
  DWORD clk = GetTickCount();
  return (unsigned long)clk;
#elif defined(OS2)
  ULONG clk = 0;
  DosQuerySysInfo(QSV_MS_COUNT, QSV_MS_COUNT, (PVOID)&clk, sizeof(ULONG));
  return clk;
#elif defined(macintosh)
  return (unsigned long)((double)TickCount()*16.66);
#else
# error "Define something here for your operating system"
#endif
}

// sleep(int milliseconds) --
// -- sleeps during the specified time (in milliseconds)
void 
GOS::sleep(int milliseconds)
{
#if defined(UNIX)
  struct timeval tv;
  tv.tv_sec = milliseconds / 1000;
  tv.tv_usec = (milliseconds - (tv.tv_sec * 1000)) * 1000;
  ::select(0, NULL, NULL, NULL, &tv);
#elif defined(_WIN32)
  Sleep(milliseconds);
#elif defined(OS2)
  DosSleep(milliseconds);
#elif defined(macintosh)
  unsigned long tick = ticks(), now;
  while (1) {
    now = ticks();
    if ((tick+milliseconds) < now)
      break;
    GThread::yield();
  }
#endif
}


// -----------------------------------------
// Testing
// -----------------------------------------

// cwd([dirname])
// -- changes directory to dirname (when specified).
//    returns the full path name of the current directory. 
GUTF8String 
GOS::cwd(const GUTF8String &dirname)
{
#if defined(UNIX) || defined(macintosh) || defined(OS2)
  if (dirname.length() && chdir(dirname.getUTF82Native())==-1)//MBCS cvt
    G_THROW(errmsg());
  char *string_buffer;
  GPBuffer<char> gstring_buffer(string_buffer,MAXPATHLEN+1);
  char *result = getcwd(string_buffer,MAXPATHLEN);
  if (!result)
    G_THROW(errmsg());
  return GNativeString(result).getNative2UTF8();//MBCS cvt
#elif defined(_WIN32)
  char drv[2];
  if (dirname.length() && _chdir(dirname.getUTF82Native())==-1)//MBCS cvt
    G_THROW(errmsg());
  drv[0]= '.' ; drv[1]=0;
  char *string_buffer;
  GPBuffer<char> gstring_buffer(string_buffer,MAXPATHLEN+1);
  char *result = getcwd(string_buffer,MAXPATHLEN);
  GetFullPathName(drv, MAXPATHLEN, string_buffer, &result);
  return GNativeString(string_buffer).getNative2UTF8();//MBCS cvt
#else
# error "Define something here for your operating system"
#endif 
}

GUTF8String
GOS::getenv(const GUTF8String &name)
{
  GUTF8String retval;
  if(name.length())
  {
    const char *env=::getenv(name.getUTF82Native());
    if(env)
    {
      retval=GNativeString(env);
    }
  }
  return retval;
}



#ifdef HAVE_NAMESPACES
}
# ifndef NOT_USING_DJVU_NAMESPACE
using namespace DJVU;
# endif
#endif

