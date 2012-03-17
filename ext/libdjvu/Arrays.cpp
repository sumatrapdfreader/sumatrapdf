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

#include "Arrays.h"
#include "GException.h"


#ifdef HAVE_NAMESPACES
namespace DJVU {
# ifdef NOT_DEFINED // Just to fool emacs c++ mode
}
#endif
#endif

ArrayRep::ArrayRep(int xelsize,
		   void (* xdestroy)(void *, int, int),
		   void (* xinit1)(void *, int, int),
		   void (* xinit2)(void *, int, int, const void *, int, int),
		   void (* xcopy)(void *, int, int, const void *, int, int),
		   void (* xinsert)(void *, int, int, const void *, int)) :
      data(0), minlo(0), maxhi(-1), lobound(0), hibound(-1),
      elsize(xelsize), destroy(xdestroy), init1(xinit1),
      init2(xinit2), copy(xcopy), insert(xinsert)
{
}

ArrayRep::ArrayRep(int xelsize,
		   void (* xdestroy)(void *, int, int),
		   void (* xinit1)(void *, int, int),
		   void (* xinit2)(void *, int, int, const void *, int, int),
		   void (* xcopy)(void *, int, int, const void *, int, int),
		   void (* xinsert)(void *, int, int, const void *, int),
		   int hi) : data(0), minlo(0), maxhi(-1),
   lobound(0), hibound(-1), elsize(xelsize), destroy(xdestroy), init1(xinit1),
   init2(xinit2), copy(xcopy), insert(xinsert)
{
   resize(0, hi);
}

ArrayRep::ArrayRep(int xelsize,
		   void (* xdestroy)(void *, int, int),
		   void (* xinit1)(void *, int, int),
		   void (* xinit2)(void *, int, int, const void *, int, int),
		   void (* xcopy)(void *, int, int, const void *, int, int),
		   void (* xinsert)(void *, int, int, const void *, int),
		   int lo, int hi) : data(0), minlo(0), maxhi(-1),
   lobound(0), hibound(-1), elsize(xelsize), destroy(xdestroy), init1(xinit1),
   init2(xinit2), copy(xcopy), insert(xinsert)
{
   resize(lo,hi);
}

ArrayRep::ArrayRep(const ArrayRep & arr) : data(0), minlo(0), maxhi(-1),
   lobound(0), hibound(-1), elsize(arr.elsize), destroy(arr.destroy),
   init1(arr.init1), init2(arr.init2), copy(arr.copy), insert(arr.insert)
{
   resize(arr.lobound, arr.hibound);
   arr.copy(data, lobound-minlo, hibound-minlo,
	    arr.data, arr.lobound-arr.minlo, arr.hibound-arr.minlo);
}

ArrayRep::~ArrayRep()
{
   destroy(data, lobound-minlo, hibound-minlo);
   operator delete(data);
   data=0;
}

ArrayRep & 
ArrayRep::operator= (const ArrayRep & rep)
{
   if (&rep == this) return *this;
   empty();
   resize(rep.lobound, rep.hibound);
   copy(data, lobound-minlo, hibound-minlo,
	rep.data, rep.lobound-rep.minlo, rep.hibound-rep.minlo);
   return *this;
}

void
ArrayRep::resize(int lo, int hi)
{
  int nsize = hi - lo + 1;
  // Validation
  if (nsize < 0)
    G_THROW( ERR_MSG("arrays.resize") );
  // Destruction
  if (nsize == 0)
    {
      destroy(data, lobound-minlo, hibound-minlo);
      operator delete(data);
      data = 0;
      lobound = minlo = lo; 
      hibound = maxhi = hi; 
      return;
    }
  // Simple extension
  if (lo >= minlo && hi <= maxhi)
    {
      init1(data, lo-minlo, lobound-1-minlo);
      destroy(data, lobound-minlo, lo-1-minlo);
      init1(data, hibound+1-minlo, hi-minlo);
      destroy(data, hi+1-minlo, hibound-minlo);
      lobound = lo;
      hibound = hi;
      return;
    }
  // General case
  int nminlo = minlo;
  int nmaxhi = maxhi;
  if (nminlo > nmaxhi)
    nminlo = nmaxhi = lo;
  while (nminlo > lo) {
    int incr = nmaxhi - nminlo;
    nminlo -= (incr < 8 ? 8 : (incr > 32768 ? 32768 : incr));
  }
  while (nmaxhi < hi) {
    int incr = nmaxhi - nminlo;
    nmaxhi += (incr < 8 ? 8 : (incr > 32768 ? 32768 : incr));
  }
  // allocate
  int bytesize=elsize*(nmaxhi-nminlo+1);
  void * ndata;
  GPBufferBase gndata(ndata,bytesize,1);
  memset(ndata, 0, bytesize);
  // initialize
  init1(ndata, lo-nminlo, lobound-1-nminlo);
  init2(ndata, lobound-nminlo, hibound-nminlo,
        data, lobound-minlo, hibound-minlo);
  init1(ndata, hibound+1-nminlo, hi-nminlo);
  destroy(data, lobound-minlo, hibound-minlo);

  // free and replace
  void *tmp=data;
  data = ndata;
  ndata=tmp;

  minlo = nminlo;
  maxhi = nmaxhi;
  lobound = lo;
  hibound = hi;
}

void
ArrayRep::shift(int disp)
{
   lobound += disp;
   hibound += disp;
   minlo += disp;
   maxhi += disp;
}

void
ArrayRep::del(int n, unsigned int howmany)
{
   if (howmany == 0)
      return;
   if ((int)(n + howmany) > hibound +1)
      G_THROW( ERR_MSG("arrays.ill_arg") );
   copy(data, n-minlo, hibound-howmany-minlo,
	data, n+howmany-minlo, hibound-minlo);
   destroy(data, hibound+1-howmany-minlo, hibound-minlo);
   hibound = hibound - howmany;
}

void
ArrayRep::ins(int n, const void * what, unsigned int howmany)
{
   int nhi = hibound + howmany;
   if (howmany == 0) return;
   if (maxhi < nhi)
   {
      int nmaxhi = maxhi;
      while (nmaxhi < nhi)
	 nmaxhi += (nmaxhi < 8 ? 8 : (nmaxhi > 32768 ? 32768 : nmaxhi));
      int bytesize = elsize*(nmaxhi-minlo+1);
      void *ndata;
      GPBufferBase gndata(ndata,bytesize,1);
      memset(ndata, 0, bytesize);
      copy(ndata, lobound-minlo, hibound-minlo,
	   data, lobound-minlo, hibound-minlo);
      destroy(data, lobound-minlo, hibound-minlo);
      data=ndata;
      maxhi = nmaxhi;
   }

   insert(data, hibound+1-minlo, n-minlo, what, howmany);
   hibound=nhi;
}



#ifdef HAVE_NAMESPACES
}
# ifndef NOT_USING_DJVU_NAMESPACE
using namespace DJVU;
# endif
#endif


// ---------------------------------------
// BEGIN HACK
// ---------------------------------------
// Included here to avoid dependency
// from ByteStream.o to Arrays.o

#ifndef DO_NOT_MOVE_GET_DATA_TO_ARRAYS_CPP
#include "ByteStream.h"

#ifdef HAVE_NAMESPACES
namespace DJVU {
# ifdef NOT_DEFINED // Just to fool emacs c++ mode
}
#endif
#endif
TArray<char>
ByteStream::get_data(void)
{
   const int s=size();
   if(s > 0)
   {
     TArray<char> data(0, s-1);
     readat((char*)data, s, 0);
     return data;
   }else
   {
     TArray<char> data(0, -1);
     return data;
   }
}

#ifdef HAVE_NAMESPACES
}
# ifndef NOT_USING_DJVU_NAMESPACE
using namespace DJVU;
# endif
#endif
#endif

// ---------------------------------------
// END HACK
// ---------------------------------------

