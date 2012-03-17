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

#ifndef _GSMARTPOINTER_H_
#define _GSMARTPOINTER_H_
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#if NEED_GNUG_PRAGMAS
# pragma interface
#endif

/** @name GSmartPointer.h

    Files #"GSmartPointer.h"# and #"GSmartPointer.cpp"# define a smart-pointer
    class which automatically performs thread-safe reference counting.  Class
    \Ref{GP} implements smart-pointers by overloading the usual pointer
    assignment and dereferencing operators. The overloaded operators maintain
    the reference counters and destroy the pointed objects as soon as their
    reference counter reaches zero.  Transparent type conversions are provided
    between smart-pointers and regular pointers.  Objects referenced by
    smart-pointers must be derived from class \Ref{GPEnabled}.

    @memo 
    Thread-Safe reference counting smart-pointers.
    @author 
    L\'eon Bottou <leonb@research.att.com> -- initial implementation\\
    Andrei Erofeev <eaf@geocities.com> -- bug fix.

// From: Leon Bottou, 1/31/2002
// Class GPBuffer has been added (but not documented) by Lizardtech.
// Our original implementation consisted of multiple classes.
// <http://prdownloads.sourceforge.net/djvu/DjVu2_2b-src.tgz>.

    @args
*/
//@{

#if defined(_MSC_VER)
// Language lawyer say MSVC6 is wrong on that one. 
// Cf section 5.4.7 in november 1997 draft.
#pragma warning( disable : 4243 )
#endif

#include "DjVuGlobal.h"
#include "atomic.h"

#include <stddef.h>

#ifdef HAVE_NAMESPACES
namespace DJVU {
# ifdef NOT_DEFINED // Just to fool emacs c++ mode
}
#endif
#endif



/** Base class for reference counted objects.  
    This is the base class for all reference counted objects.
    Any instance of a subclass of #GPEnabled# can be used with 
    smart-pointers (see \Ref{GP}).  
 */
class DJVUAPI GPEnabled
{
  friend class GPBase;
  void destroy();
  void unref();
  void ref();
public:
  /// Null constructor.
  GPEnabled();
  /// Copy construcotr
  GPEnabled(const GPEnabled & obj);
  /// Virtual destructor.
  virtual ~GPEnabled();
  /// Copy operator
  GPEnabled & operator=(const GPEnabled & obj);
  /** Returns the number of references to this object.  This should be only
      used for debugging purposes. Other uses are not thread-safe. */
  int get_count(void) const;
protected:
  /// The reference counter
  volatile int count;
};



/** Base class for all smart-pointers.
    This class implements common mechanisms for all
    smart-pointers (see \Ref{GP}). There should be no need
    to use this class directly.  Its sole purpose consists
    in reducing the template expansion overhead.
*/

class DJVUAPI GPBase
{
public:
  /** Null Constructor. */
  GPBase();
  /** Copy Constructor.
      Increments the reference count. 
      @param sptr reference to a #GPBase# object. */
  GPBase(const GPBase &sptr);
  /** Construct a GPBase from a pointer.
      Increments the reference count.
      @param nptr pointer to a #GPEnabled# object. */
  GPBase(GPEnabled *nptr);
  /** Destructor. Decrements the reference count. */
  ~GPBase();
  /** Accesses the actual pointer. */
  GPEnabled* get() const;
  /** Assignment from smartpointer. 
      Increments the counter of the new value of the pointer.
      Decrements the counter of the previous value of the pointer. */
  GPBase& assign(const GPBase &sptr);
  /** Assignment from pointer. 
      Checks that the object is not being destroyed.
      Increments the counter of the new value of the pointer.
      Decrements the counter of the previous value of the pointer. */
  GPBase& assign(GPEnabled *nptr);
  /** Assignment operator. */
  GPBase & operator=(const GPBase & obj);
  /** Comparison operator. */
  int operator==(const GPBase & g2) const;
protected:
  /** Actual pointer */
  GPEnabled *ptr;
};


/** Reference counting pointer.
    Class #GP<TYPE># represents a smart-pointer to an object of type #TYPE#.
    Type #TYPE# must be a subclass of #GPEnabled#.  This class overloads the
    usual pointer assignment and dereferencing operators. The overloaded
    operators maintain the reference counters and destroy the pointed object
    as soon as their reference counter reaches zero.  Transparent type
    conversions are provided between smart-pointers and regular pointers.

    Using a smart-pointer is a convenience and not an obligation.  There is no
    need to use a smart-pointer to access a #GPEnabled# object.  As long as
    you never use a smart-pointer to access a #GPEnabled# object, its
    reference counter remains zero.  Since the reference counter is never
    decremented from one to zero, the object is never destroyed by the
    reference counting code.  You can therefore choose to only use regular
    pointers to access objects allocated on the stack (automatic variables) or
    objects allocated dynamically.  In the latter case you must explicitly
    destroy the dynamically allocated object with operator #delete#.

    The first time you use a smart-pointer to access #GPEnabled# object, the
    reference counter is incremented to one. Object destruction will then
    happen automatically when the reference counter is decremented back to
    zero (i.e. when the last smart-pointer referencing 
    this object stops doing so).
    This will happen regardless of how many regular pointers 
    reference this object.
    In other words, if you start using smart-pointers with a #GPEnabled#
    object, you engage automatic mode for this object.  You should only do
    this with objects dynamically allocated with operator #new#.  You should
    never destroy the object yourself, but let the smart-pointers control the
    life of the object.
    
    {\bf Performance considerations} --- Thread safe reference counting incurs
    a significant overhead. Smart-pointer are best used with sizeable objects
    for which the cost of maintaining the counters represent a small fraction
    of the processing time.  It is always possible to cache a smart-pointer
    into a regular pointer.  The cached pointer will remain valid until the
    smart-pointer object is destroyed or the smart-pointer value is changed.

    {\bf Safety considerations} --- As explained above, a #GPEnabled# object
    switches to automatic mode as soon as it becomes referenced by a
    smart-pointer.  There is no way to switch the object back to manual mode.
    Suppose that you have decided to only use regular pointers with a
    particular #GPEnabled# object.  You therefore plan to destroy the object
    explicitly when you no longer need it.  When you pass a regular pointer to
    this object as argument to a function, you really need to be certain that
    the function implementation will not assign this pointer to a
    smart-pointer.  Doing so would indeed destroy the object as soon as the
    function returns.  The bad news is that the fact that a function assigns a
    pointer argument to a smart-pointer does not necessarily appear in the
    function prototype.  Such a behavior must be {\em documented} with the
    function public interface.  As a convention, we usually write such
    functions with smart-pointer arguments instead of a regular pointer
    arguments.  This is not enough to catch the error at compile time, but
    this is a simple way to document such a behavior.  We still believe that
    this is a small problem in regard to the benefits of the smart-pointer.
    But one has to be aware of its existence.  */

template <class TYPE>
class GP : protected GPBase
{
public:
  /** Constructs a null smart-pointer. */
  GP();
  /** Constructs a copy of a smart-pointer.
      @param sptr smart-pointer to copy. */
  GP(const GP<TYPE> &sptr);
  /** Constructs a smart-pointer from a regular pointer.
      The pointed object must be dynamically allocated (with operator #new#).
      You should no longer explicitly destroy the object referenced by #sptr#
      since the object life is now controlled by smart-pointers.  
      @param nptr regular pointer to a {\em dynamically allocated object}. */
  GP(TYPE *nptr);
  /** Converts a smart-pointer into a regular pointer.  
      This is useful for caching the value of a smart-pointer for performances
      purposes.  The cached pointer will remain valid until the smart-pointer
      is destroyed or until the smart-pointer value is changed. */
  operator TYPE* () const;
  /** Assigns a regular pointer to a smart-pointer lvalue.
      The pointed object must be dynamically allocated (with operator #new#).
      You should no longer explicitly destroy the object referenced by #sptr#
      since the object life is now controlled by smart-pointers.  
      @param nptr regular pointer to a {\em dynamically allocated object}. */
  GP<TYPE>& operator= (TYPE *nptr);
  /** Assigns a smart-pointer to a smart-pointer lvalue.
      @param sptr smart-pointer copied into this smart-pointer. */
  GP<TYPE>& operator= (const GP<TYPE> &sptr);
  /** Indirection operator.
      This operator provides a convenient access to the members
      of a smart-pointed object. Operator #-># works with smart-pointers
      exactly as with regular pointers. */
  TYPE* operator->() const;
  /** Dereferencement operator.
      This operator provides a convenient access to the smart-pointed object. 
      Operator #*# works with smart-pointers exactly as with regular pointers. */
  TYPE& operator*() const;
  /** Comparison operator. 
      Returns true if both this smart-pointer and pointer #nptr# point to the
      same object.  The automatic conversion from smart-pointers to regular
      pointers allows you to compare two smart-pointers as well.  
      @param nptr pointer to compare with. */
  int operator== (TYPE *nptr) const;
  /** Comparison operator.  
      Returns true if this smart-pointer and pointer #nptr# point to different
      objects. The automatic conversion from smart-pointers to regular
      pointers allows you to compare two smart-pointers as well.  
      @param nptr pointer to compare with. */
  int operator!= (TYPE *nptr) const;
  /** Test operator.
      Returns true if the smart-pointer is null.  The automatic conversion 
      from smart-pointers to regular pointers allows you to test whether 
      a smart-pointer is non-null.  You can use both following constructs:
      \begin{verbatim}
      if (gp) { ... }
      while (! gp) { ... }
      \end{verbatim} */
  int operator! () const;
};

//@}

// INLINE FOR GPENABLED

inline
GPEnabled::GPEnabled()
  : count(0)
{
}

inline
GPEnabled::GPEnabled(const GPEnabled & obj) 
  : count(0) 
{

}

inline int
GPEnabled::get_count(void) const
{
   return count;
}

inline GPEnabled & 
GPEnabled::operator=(const GPEnabled & obj)
{ 
  /* The copy operator should do nothing because the count should not be
     changed.  Subclasses of GPEnabled will call this version of the copy
     operator as part of the default 'memberwise copy' strategy. */
  return *this; 
}

inline void 
GPEnabled::ref()
{
#if PARANOID_DEBUG
  assert (count >= 0);
#endif
  atomicIncrement(&count);
}

inline void 
GPEnabled::unref()
{
#if PARANOID_DEBUG
  assert (count > 0);
#endif
  if (! atomicDecrement(&count))
    destroy();
}

// INLINE FOR GPBASE

inline
GPBase::GPBase()
  : ptr(0)
{
}

inline
GPBase::GPBase(GPEnabled *nptr)
  : ptr(0)
{
  assign(nptr);
}

inline
GPBase::GPBase(const GPBase &sptr)
{
  if (sptr.ptr)
    sptr.ptr->ref();
  ptr = sptr.ptr;
}

inline
GPBase::~GPBase()
{
  GPEnabled *old = ptr;
  ptr = 0;
  if (old)
    old->unref();
}

inline GPEnabled* 
GPBase::get() const
{
#if PARANOID_DEBUG
  if (ptr && ptr->get_count() <= 0)
    *(int*)0=0;
#endif
  return ptr;
}

inline GPBase &
GPBase::operator=(const GPBase & obj)
{
  return assign(obj);
}

inline int 
GPBase::operator==(const GPBase & g2) const
{
  return ptr == g2.ptr;
}




// INLINE FOR GP<TYPE>

template <class TYPE> inline
GP<TYPE>::GP()
{
}

template <class TYPE> inline
GP<TYPE>::GP(TYPE *nptr)
: GPBase((GPEnabled*)nptr)
{
}

template <class TYPE> inline
GP<TYPE>::GP(const GP<TYPE> &sptr)
: GPBase((const GPBase&) sptr)
{
}

template <class TYPE> inline
GP<TYPE>::operator TYPE* () const
{
  return (TYPE*) ptr;
}

template <class TYPE> inline TYPE*
GP<TYPE>::operator->() const
{
#if PARANOID_DEBUG
  if (ptr && ptr->get_count() <= 0)
    *(int*)0=0;
#endif
  return (TYPE*) ptr;
}

template <class TYPE> inline TYPE&
GP<TYPE>::operator*() const
{
#if PARANOID_DEBUG
  if (ptr && ptr->get_count() <= 0)
    *(int*)0=0;
#endif
  return *(TYPE*) ptr;
}

template <class TYPE> inline GP<TYPE>& 
GP<TYPE>::operator= (TYPE *nptr)
{
  return (GP<TYPE>&)( assign(nptr) );
}

template <class TYPE> inline GP<TYPE>& 
GP<TYPE>::operator= (const GP<TYPE> &sptr)
{
  return (GP<TYPE>&)( assign((const GPBase&)sptr) );
}

template <class TYPE> inline int
GP<TYPE>::operator== (TYPE *nptr) const
{
  return ( (TYPE*)ptr == nptr );
}

template <class TYPE> inline int
GP<TYPE>::operator!= (TYPE *nptr) const
{
  return ( (TYPE*)ptr != nptr );
}

template <class TYPE> inline int
GP<TYPE>::operator! () const
{
  return !ptr;
}

/* GPBUFFER */

/* What is this LT innovation ? 
   What does it do that a GArray does not do ? 
   What about the objects construction and destruction ? */

class DJVUAPI GPBufferBase
{
public:
  GPBufferBase(void *&,const size_t n,const size_t t);
  void swap(GPBufferBase &p);
  void resize(const size_t n,const size_t t);
  void replace(void *nptr,const size_t n);
  void set(const size_t t,const char c);
  ~GPBufferBase();
  operator int(void) const { return ptr ? num : 0; }
private:
  void *&ptr;
  size_t num;
};

template<class TYPE>
class GPBuffer : public GPBufferBase
{
public:
  GPBuffer(TYPE *&xptr,const size_t n=0) 
    : GPBufferBase((void *&)xptr,n,sizeof(TYPE)) {}
  inline void resize(const size_t n) {GPBufferBase::resize(n,sizeof(TYPE));}
  inline void clear(void) {GPBufferBase::set(sizeof(TYPE),0);}
  inline void set(const char c) {GPBufferBase::set(sizeof(TYPE),c);}
  inline operator int(void) const {return GPBufferBase::operator int();}
};



#ifdef HAVE_NAMESPACES
}
# ifndef NOT_USING_DJVU_NAMESPACE
using namespace DJVU;
# endif
#endif
#endif
