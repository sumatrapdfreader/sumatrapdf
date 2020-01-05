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

#include "debug.h"

#if ( DEBUGLVL > 0 )

#include "GThreads.h"
#include "GContainer.h"
#include "GString.h"
#include "GString.h"
#include "ByteStream.h"
#include "GURL.h"

#include <stdarg.h>
#include <stdio.h>
#include <errno.h>

#ifdef _WIN32
# include <windows.h>  // OutputDebugString
#endif 


#ifdef HAVE_NAMESPACES
namespace DJVU {
# ifdef NOT_DEFINED // Just to fool emacs c++ mode
}
#endif
#endif


#ifndef UNIX
#ifndef _WIN32
#ifndef macintosh
#define UNIX
#endif
#endif
#endif

static GCriticalSection debug_lock;
#ifdef RUNTIME_DEBUG_ONLY
static int              debug_level = 0;
#else
static int              debug_level = DEBUGLVL;
#endif
static int              debug_id;
static FILE            *debug_file;
static int              debug_file_count;

static GMap<long, DjVuDebug> &
debug_map(void)
{
  static GMap<long, DjVuDebug> xmap;
  return xmap;
}

DjVuDebug::DjVuDebug()
  : block(0), indent(0)
{
  id = debug_id++;
#ifdef UNIX
  if (debug_file_count++ == 0 && !debug_file)
    set_debug_file(stderr);
#endif
}

DjVuDebug::~DjVuDebug()
{
#ifdef UNIX
  if (--debug_file_count == 0)
    {
      if (debug_file && (debug_file != stderr))
        fclose(debug_file);
      debug_file = 0;
    }
#endif
}

void   
DjVuDebug::format(const char *fmt, ... )
{
  if (! block)
    {
      va_list ap;
      va_start(ap, fmt);
      GUTF8String buffer(fmt,ap);
      va_end(ap);
      GCriticalSectionLock glock(&debug_lock);
      if (debug_file)
        {
          fprintf(debug_file,"%s", (const char*)buffer);
          fflush(debug_file);
        }
#ifdef _WIN32
      else
        {
          OutputDebugStringA((const char *)buffer);
        }
#endif
    }
}

void   
DjVuDebug::set_debug_level(int lvl)
{
  debug_level = lvl;
}

void
DjVuDebug::set_debug_file(FILE * file)
{
  GCriticalSectionLock glock(&debug_lock);
  if (debug_file && (debug_file != stderr))
    fclose(debug_file);
  debug_file = file;
}

void
DjVuDebug::modify_indent(int rindent)
{
  indent += rindent;
}

DjVuDebug& 
DjVuDebug::lock(int lvl, int noindent)
{
  int threads_num=1;
  debug_lock.lock();
  // Get per-thread debug object
  long threadid = (long) GThread::current();
  DjVuDebug &dbg = debug_map()[threadid];
  threads_num=debug_map().size();
  // Check level
  dbg.block = (lvl > debug_level);
  // Output thread id and indentation
  if (! noindent)
    {
      if (threads_num>1)
        dbg.format("[T%d] ", dbg.id);
      int ind = dbg.indent;
      char buffer[257];
      memset(buffer,' ', sizeof(buffer)-1);
      buffer[sizeof(buffer)-1] = 0;
      while (ind > (int)sizeof(buffer)-1)
        {
          dbg.format("%s", buffer);
          ind -= sizeof(buffer)-1;
        }
      if (ind > 0)
        {
          buffer[ind] = 0;
          dbg.format("%s", buffer);
        }
    }
  // Return
  return dbg;
}

void
DjVuDebug::unlock()
{
  debug_lock.unlock();
}

#define OP(type, fmt) \
DjVuDebug& DjVuDebug::operator<<(type arg)\
{ format(fmt, arg); return *this; }

DjVuDebug& DjVuDebug::operator<<(bool arg)
{
   format("%s", arg ? "TRUE" : "FALSE"); return *this;
}

OP(char, "%c")
OP(unsigned char, "%c")
OP(int, "%d")
OP(unsigned int, "%u")
OP(short int, "%hd")
OP(unsigned short int, "%hu")
OP(long, "%ld")
OP(unsigned long, "%lu")
OP(float, "%g")
OP(double, "%g")
OP(const void * const, "0x%08x")

DjVuDebug& DjVuDebug::operator<<(const char * const ptr) 
{
  GUTF8String buffer(ptr?ptr:"(null)");
  if(buffer.length() > 255)
  {
    buffer=buffer.substr(0,252)+"...";
  }
  format("%s", (const char *)buffer);
  return *this; 
}

DjVuDebug& DjVuDebug::operator<<(const unsigned char * const ptr) 
{ 
  return operator<<( (const char *) ptr );
}

DjVuDebug& DjVuDebug::operator<<(const GUTF8String &ptr)
{
  GUTF8String buffer(ptr);
  if(buffer.length() > 255)
    buffer=buffer.substr(0,252)+"...";
  format("%s", (const char *)buffer);
  return *this; 
}

DjVuDebugIndent::DjVuDebugIndent(int inc)
  : inc(inc)
{
  DjVuDebug &dbg = DjVuDebug::lock(0,1);
  dbg.modify_indent(inc);
  dbg.unlock();
}

DjVuDebugIndent::~DjVuDebugIndent()
{
  DjVuDebug &dbg = DjVuDebug::lock(0,1);
  dbg.modify_indent(-inc);
  dbg.unlock();
}


#ifdef HAVE_NAMESPACES
}
# ifndef NOT_USING_DJVU_NAMESPACE
using namespace DJVU;
# endif
#endif
#endif
