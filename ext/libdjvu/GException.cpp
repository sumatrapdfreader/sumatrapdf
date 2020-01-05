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

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "GException.h"
#include "DjVuMessageLite.h"
#include "debug.h"


#ifdef HAVE_NAMESPACES
namespace DJVU {
# ifdef NOT_DEFINED // Just to fool emacs c++ mode
}
#endif
#endif


// - Author: Leon Bottou, 05/1997

GException::GException() 
  : cause(0), file(0), func(0), line(0), source(GException::GINTERNAL)
{
}

const char * const
GException::outofmemory = ERR_MSG("GException.outofmemory");

GException::GException(const GException & exc) 
  : file(exc.file), func(exc.func), line(exc.line), source(exc.source)
{
  if (exc.cause && exc.cause!=outofmemory) 
    {
      char *s = new char[strlen(exc.cause)+1];
      strcpy(s, exc.cause);
      cause = s;
    }
  else
    {
      cause = exc.cause;
    }
}

GException::GException (const char *xcause, const char *file, int line,
   const char *func, const source_type xsource)
  : file(file), func(func), line(line), source(xsource)
{
  // good place to set a breakpoint and DEBUG message too. 
  // It'd hard to track exceptions which seem to go from nowhere
#ifdef DEBUG_MSG
  DEBUG_MSG("GException::GException(): cause=" << (xcause ? xcause : "unknown") << "\n");
#endif
  if (xcause && xcause!=outofmemory) 
    {
      char *s = new char[strlen(xcause)+1];
      strcpy(s, xcause);
      cause = s;
    }
  else
    {
      cause = xcause;
    }
}

GException::~GException(void)
{
  if (cause && cause!=outofmemory ) 
    delete [] const_cast<char*>(cause); 
  cause=file=func=0;
}

GException & 
GException::operator=(const GException & exc)
{
  if (cause && cause!=outofmemory) 
    delete [] const_cast<char*>(cause);
  cause = 0;
  file = exc.file;
  func = exc.func;
  line = exc.line;
  source=exc.source;
  if (exc.cause && exc.cause!=outofmemory) 
    {
      char *s = new char[strlen(exc.cause)+1];
      strcpy(s, exc.cause);
      cause = s;
    }
  else
    {
      cause = exc.cause;
    }
  return *this;
}

void
GException::perror(void) const
{
  fflush(0);
  DjVuPrintErrorUTF8("*** ");
  DjVuMessageLite::perror(get_cause());
  if (file && line>0)
    DjVuPrintErrorUTF8("*** (%s:%d)\n", file, line);    
  else if (file)
    DjVuPrintErrorUTF8("*** (%s)\n", file);
  if (func)
    DjVuPrintErrorUTF8("*** '%s'\n", func);    
  DjVuPrintErrorUTF8("\n");
}

const char* 
GException::get_cause(void) const
{
  if (! cause)
    return "Invalid exception";
  return cause;
}

int
GException::cmp_cause(const char s1[] , const char s2[])
{
  int retval;
  if(! s2 || !s2[0])
  {
    retval=(s1&&s1[0])?1:(-1);
  }else if(! s1 || !s1[0])
  {
    retval=(-1);
  }else
  {
    const char *end_s1=strpbrk(s1,"\t\n");
    const int n1=end_s1?(int)((size_t)end_s1-(size_t)s1):strlen(s1);
    const char *end_s2=strpbrk(s1,"\t\n");
    const int n2=end_s2?(int)((size_t)end_s2-(size_t)s2):strlen(s2);
    retval=(n1==n2)?strncmp(s1,s2,n1):strcmp(s1,s2);
  }
  return retval;
}

int
GException::cmp_cause(const char s2[]) const
{
  return cmp_cause(cause,s2);
}


// ------ MEMORY MANAGEMENT HANDLER

/* SumatraPDF: prevent exception handler overriding when not building stand-alone libdjvu */
#ifdef ALLOW_GLOBAL_OOM_HANDLING
#ifndef NEED_DJVU_MEMORY
// This is not activated when C++ memory management
// is overidden.  The overriding functions handle
// memory exceptions by themselves.
static void throw_memory_error() { G_THROW(GException::outofmemory); }
# if defined(_WIN32) || defined(__CYGWIN32__) || defined(OS2)
static void (*old_handler)() = std::set_new_handler(throw_memory_error);
# else 
#   ifdef HAVE_STDINCLUDES
static void (*old_handler)() = std::set_new_handler(throw_memory_error);
#   else
static void (*old_handler)() = set_new_handler(throw_memory_error);
#   endif // HAVE_STDINCLUDES
#  endif // ! WIN32
#endif // !NEED_DJVU_MEMORY
#endif


#ifdef HAVE_NAMESPACES
}
# ifndef NOT_USING_DJVU_NAMESPACE
using namespace DJVU;
# endif
#endif

