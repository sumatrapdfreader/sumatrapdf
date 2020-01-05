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

// - Author: Leon Bottou, 05/1997

// From: Leon Bottou, 1/31/2002
// Class GPBuffer has been added (but not documented) by Lizardtech.
// Our original implementation consisted of multiple classes.
// <http://prdownloads.sourceforge.net/djvu/DjVu2_2b-src.tgz>.

#include <stddef.h>
#include <string.h>
#if PARANOID_DEBUG
# include <assert.h>
#endif

#include "GThreads.h"
#include "GSmartPointer.h"
#include "GException.h"

#ifdef HAVE_NAMESPACES
namespace DJVU {
# ifdef NOT_DEFINED // Just to fool emacs c++ mode
}
#endif
#endif


// ------ GPENABLED

GPEnabled::~GPEnabled()
{
#if DISABLED_BECAUSE_OF_CXX11_DESTRUCTORS_BEING_NOEXCEPT
  if (count > 0)
    G_THROW( ERR_MSG("GSmartPointer.suspicious") );
#endif
}

void
GPEnabled::destroy()
{
  // Only delete if the counter is still zero.
  // because someone may have rescued the object...
  // If yes, set the counter to -0x7fff to mark 
  // the object as doomed and make sure things
  // will work if the destructor uses a GP...
  if (! atomicCompareAndSwap(&count, 0, -0x7fff))
    delete this;
}


// ------ GPBASE


GPBase&
GPBase::assign (const GPBase &sptr)
{
  GPEnabled *nptr = sptr.ptr;
  if (nptr && atomicIncrement(&nptr->count) <= 0)
    nptr = 0;
  GPEnabled *optr = (GPEnabled*)atomicExchangePointer((void**)&ptr, (void*)nptr);
  if (optr)
    optr->unref();
  return *this;
}

GPBase&
GPBase::assign (GPEnabled *nptr)
{
  if (nptr && atomicIncrement(&nptr->count) <= 0)
    nptr = 0;
  GPEnabled *optr = (GPEnabled*)atomicExchangePointer((void**)&ptr, (void*)nptr);
  if (optr)
    optr->unref();
  return *this;
}




// ------ GPBUFFERBASE


void
GPBufferBase::replace(void *nptr,const size_t n)
{
  resize(0,0);
  ptr=nptr;
  num=n;
}

GPBufferBase::GPBufferBase(void *&xptr,const size_t n,const size_t t) 
  : ptr(xptr), num(n)
{
  if (n)
    xptr = ::operator new(n*t);
  else
    xptr = 0;
}

GPBufferBase::~GPBufferBase()
{
  ::operator delete(ptr);
}

void 
GPBufferBase::swap(GPBufferBase &other)
{
  void * const temp_ptr=ptr;
  ptr=other.ptr;
  other.ptr=temp_ptr;
  const size_t temp_num=num;
  num=other.num;
  other.num=temp_num;
}

void
GPBufferBase::resize(const size_t n, const size_t t)
{
  if(!n && !ptr)
    {
      num=0;
    }
  else
    {
      const size_t s=ptr?(((num<n)?num:n)*t):0;
      void *nptr;
      GPBufferBase gnptr(nptr, n, t);
      if(s)
        {
          memcpy(nptr, ptr, s);
        }
      swap(gnptr);
    }
}

void
GPBufferBase::set(const size_t t,const char c)
{
  if(num)
    memset(ptr,c,num*t);
}


#ifdef HAVE_NAMESPACES
}
# ifndef NOT_USING_DJVU_NAMESPACE
using namespace DJVU;
# endif
#endif

