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

#ifdef NEED_DJVU_MEMORY
#ifndef NEED_DJVU_MEMORY_IMPLEMENTATION
#define NEED_DJVU_MEMORY_IMPLEMENTATION
#endif /* NEED_DJVU_MEMORY_IMPLEMENTATION */

#include "DjVuGlobal.h"
#include "GException.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "debug.h"

#ifdef UNIX
djvu_delete_callback *
_djvu_delete_ptr=(djvu_delete_callback *)&(operator delete);
djvu_delete_callback *
_djvu_deleteArray_ptr=(djvu_delete_callback *)&(operator delete []);
djvu_new_callback *
_djvu_new_ptr=(djvu_new_callback *)&(operator new);
djvu_new_callback *
_djvu_newArray_ptr=(djvu_new_callback *)&(operator new []);
#endif

static djvu_delete_callback *_djvu_delete_handler = 0;
static djvu_new_callback *_djvu_new_handler = 0;
static djvu_delete_callback *deleteArray_handler = 0;
static djvu_new_callback *newArray_handler = 0;

static djvu_free_callback *_djvu_free_handler = 0;
static djvu_realloc_callback *_djvu_realloc_handler = 0;
static djvu_calloc_callback *_djvu_calloc_handler = 0;
static djvu_malloc_callback *_djvu_malloc_handler = 0;

int
djvu_memoryObject_callback (
  djvu_delete_callback* delete_handler,
  djvu_new_callback* new_handler
) {
  if(delete_handler && new_handler)
  {
#ifdef UNIX
    _djvu_new_ptr=&_djvu_new;
    _djvu_delete_ptr=&_djvu_delete;
#endif
    _djvu_delete_handler=delete_handler;
    _djvu_new_handler=new_handler;
    return 1;
  }else
  {
#ifdef UNIX
    _djvu_new_ptr=(djvu_new_callback *)&(operator new);
    _djvu_delete_ptr=(djvu_delete_callback *)&(operator delete);
#endif
    _djvu_delete_handler=0;
    _djvu_new_handler=0;
    return (delete_handler||new_handler)?0:1;
  }
  return 0;
}

int 
djvu_set_memory_callbacks
(
  djvu_free_callback *free_handler,
  djvu_realloc_callback *realloc_handler,
  djvu_malloc_callback *malloc_handler,
  djvu_calloc_callback *calloc_handler
)
{
  if(free_handler && realloc_handler && malloc_handler)
  {
#ifdef UNIX
    _djvu_new_ptr=(djvu_new_callback *)&_djvu_new;
    _djvu_delete_ptr=(djvu_delete_callback *)&_djvu_delete;
#endif
    _djvu_new_handler=(djvu_new_callback *)malloc_handler;
    _djvu_delete_handler=(djvu_delete_callback *)free_handler;
    _djvu_malloc_handler=(djvu_malloc_callback *)malloc_handler;
    _djvu_free_handler=(djvu_free_callback *)free_handler;
    _djvu_realloc_handler=(djvu_realloc_callback *)realloc_handler;
    if(calloc_handler)
    {
      _djvu_calloc_handler=(djvu_calloc_callback *)&calloc_handler;
    }else
    {
      _djvu_calloc_handler=0;
    }
    return 1;
  }else
  {
#ifdef UNIX
    _djvu_new_ptr=(djvu_new_callback *)&(operator new);
    _djvu_delete_ptr=(djvu_delete_callback *)&(operator delete);
#endif
    _djvu_delete_handler=0;
    _djvu_new_handler=0;
    _djvu_malloc_handler=0;
    _djvu_free_handler=0;
    _djvu_realloc_handler=0;
    _djvu_calloc_handler=0;
    return !(_djvu_malloc_handler
      ||_djvu_free_handler
      ||_djvu_realloc_handler
      ||_djvu_calloc_handler);
  }
}

DJVUAPI void *
_djvu_new(size_t siz)
{
  void *ptr;
#ifndef UNIX
  if(_djvu_new_handler)
  {
#endif
    if(!(ptr=(*_djvu_new_handler)(siz?siz:1)))
    {
      G_THROW( ERR_MSG("DjVuGlobalMemory.exhausted") );
    }
#ifndef UNIX
  }else
  {
      ptr=::operator new(siz?siz:1);
  }
#endif
  return ptr;
}

void  
_djvu_delete(void *addr)
{
  if(addr)
  {
    if(_djvu_delete_handler)
    {
      (*_djvu_delete_handler)(addr);
    }else
    {
      operator delete(addr);
    }
  }
}

void *
_djvu_newArray(size_t siz)
{
  void *ptr;
#ifndef UNIX
  if(newArray_handler)
  {
#endif
    if(!(ptr=(*newArray_handler)(siz?siz:1)))
    {
      G_THROW( ERR_MSG("DjVuGlobalMemory.exhausted") );
    }
#ifndef UNIX
  }else
  {
      ptr=::new unsigned char[siz?siz:1];
  }
#endif
  return ptr;
}

void
_djvu_deleteArray(void *addr)
{
  if(addr)
  {
    if(deleteArray_handler)
    {
      (*deleteArray_handler)(addr);
    }else
    {
#ifdef WIN32
                delete [] (addr) ;
#else
        operator delete [] (addr);
#endif
    }
  }
}

void *
_djvu_malloc(size_t siz)
{
  DEBUG_MSG("_djvu_malloc: siz="<<siz<<"\n");
  return _djvu_malloc_handler?(*_djvu_malloc_handler)(siz?siz:1):malloc(siz?siz:1);
}

void *
_djvu_calloc(size_t siz, size_t items)
{
  DEBUG_MSG("_djvu_calloc: siz="<<siz<<" items="<<items<<"\n");
  void *ptr;
  if( _djvu_calloc_handler )
  {
    ptr = (*_djvu_calloc_handler)(siz?siz:1, items?items:1);
  }else if( _djvu_malloc_handler )
  {
    if((ptr = (*_djvu_malloc_handler)((siz?siz:1)*(items?items:1)))&&siz&&items)
    {
      memset(ptr,0,siz*items);
    }
  }else
  { 
    ptr = calloc(siz?siz:1, items?items:1);
  }
  return ptr;    
}

void *
_djvu_realloc(void* ptr, size_t siz)
{
  DEBUG_MSG("_djvu_realloc: ptr="<<ptr<<" siz="<<siz<<"\n");
  void *newptr;
  if( _djvu_realloc_handler )
  {
    newptr = (*_djvu_realloc_handler)(ptr, siz);
  }else
  {
    newptr = realloc(ptr, siz?siz:1);
  }
  return newptr;
}
 
void
_djvu_free(void *ptr)
{
  DEBUG_MSG("_djvu_free: ptr="<<ptr<<"\n");
  if(ptr)
  {
    if( _djvu_free_handler )
    {
      (*_djvu_free_handler)(ptr);
    }else
    {
      free(ptr);
    }
  }
}

#endif

