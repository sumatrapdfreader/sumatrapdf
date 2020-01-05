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

#ifndef _GCONTAINER_H_
#define _GCONTAINER_H_
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#if NEED_GNUG_PRAGMAS
# pragma interface
#endif


#include "GException.h"
#include "GSmartPointer.h"
#include <string.h>

#ifdef HAVE_NAMESPACES
namespace DJVU {
# ifdef NOT_DEFINED // Just to fool emacs c++ mode
}
#endif
#endif


// Supports old iterators (first/last/next/prev) on lists and maps?
#ifndef GCONTAINER_OLD_ITERATORS
#define GCONTAINER_OLD_ITERATORS 1
#endif

// Check array bounds at runtime ?
#ifndef GCONTAINER_BOUNDS_CHECK
#define GCONTAINER_BOUNDS_CHECK 1
#endif

// Clears allocated memory prior to running constructors ?
#ifndef GCONTAINER_ZERO_FILL
#define GCONTAINER_ZERO_FILL 1
#endif

// Avoid member templates (needed by old compilers)
#ifndef GCONTAINER_NO_MEMBER_TEMPLATES
#if defined(__GNUC__) && (__GNUC__==2) && (__GNUC_MINOR__<91)
#define GCONTAINER_NO_MEMBER_TEMPLATES 1
#elif defined(_MSC_VER) && !defined(__ICL)
#define GCONTAINER_NO_MEMBER_TEMPLATES 1
#elif defined(__MWERKS__)
#define GCONTAINER_NO_MEMBER_TEMPLATES 1
#else
#define GCONTAINER_NO_MEMBER_TEMPLATES 0
#endif
#endif

// Define typename when needed
#ifndef GCONTAINER_NO_TYPENAME
#define GCONTAINER_NO_TYPENAME 0
#endif
#if GCONTAINER_NO_TYPENAME
#define typename /**/
#endif


/** @name GContainer.h

    Files #"GContainer.h"# and #"GContainer.cpp"# implement three main
    template class for generic containers.  
    Class #GArray# (see \Ref{Dynamic Arrays}) implements an array of objects
    with variable bounds. Class #GList# (see \Ref{Doubly Linked Lists})
    implements a doubly linked list of objects.  Class #GMap# (see
    \Ref{Associative Maps}) implements a hashed associative map.  The
    container templates are not thread-safe. Thread safety can be implemented
    using the facilities provided in \Ref{GThreads.h}.
    
    @memo 
    Template class for generic containers.
    @author 
    L\'eon Bottou <leonb@research.att.com> -- initial implementation.\\
    Andrei Erofeev <eaf@geocities.com> -- bug fixes.
*/
//@{



// ------------------------------------------------------------
// HASH FUNCTIONS
// ------------------------------------------------------------


/** @name Hash functions
    These functions let you use template class \Ref{GMap} with the
    corresponding elementary types. The returned hash code may be reduced to
    an arbitrary range by computing its remainder modulo the upper bound of
    the range.
    @memo Hash functions for elementary types. */
//@{

/** Hashing function (unsigned int). */
static inline unsigned int 
hash(const unsigned int & x) 
{ 
  return x; 
}

/** Hashing function (int). */
static inline unsigned int 
hash(const int & x) 
{ 
  return (unsigned int)x;
}

/** Hashing function (long). */
static inline unsigned int
hash(const long & x) 
{ 
  return (unsigned int)x;
}

/** Hashing function (unsigned long). */
static inline unsigned int
hash(const unsigned long & x) 
{ 
  return (unsigned int)x;
}

/** Hashing function (const void *). */
static inline unsigned int 
hash(const void * const & x) 
{ 
  return (unsigned int)(size_t) x; 
}

/** Hashing function (float). */
static inline unsigned int
hash(const float & x) 
{ 
  // optimizer will get rid of unnecessary code  
  unsigned int *addr = (unsigned int*)&x;
  if (sizeof(float)<2*sizeof(unsigned int))
    return addr[0];
  else
    return addr[0]^addr[1];
}

/** Hashing function (double). */
static inline unsigned int
hash(const double & x) 
{ 
  // optimizer will get rid of unnecessary code
  unsigned int *addr = (unsigned int*)&x;
  if (sizeof(double)<2*sizeof(unsigned int))
    return addr[0];
  else if (sizeof(double)<4*sizeof(unsigned int))
    return addr[0]^addr[1];
  else
    return addr[0]^addr[1]^addr[2]^addr[3];    
}



// ------------------------------------------------------------
// HELPER CLASSES
// ------------------------------------------------------------



/* Namespace for containers support classes.  This class is used as a
   namespace for global identifiers related to the implementation of
   containers.  It is inherited by all container objects.  This is disabled by
   defining compilation symbol #GCONTAINER_NO_MEMBER_TEMPATES# to 1. */


#ifdef _MSC_VER
// Language lawyer say MS is wrong on that one. 
// Cf section 5.4.7 in november 1997 draft.
#pragma warning( disable : 4243 )
#endif


// GPEnabled inhertenced removed again so the code works on more machines.
class GCont
#if GCONTAINER_NO_MEMBER_TEMPLATES
{
};
#else
{
public:
#endif
  // --- Pointers to type management functions
  struct Traits
  {
    int       size;
    void     *(*lea)     (void *base, int n);
    void      (*init)    (void *dst, int n); 
    void      (*copy)    (void *dst, const void* src, int n, int zap);
    void      (*fini)    (void *dst, int n);
  };
#if !GCONTAINER_NO_MEMBER_TEMPLATES
protected:
#endif
  // --- Management of simple types
  template <int SZ> class TrivTraits
  {
  public:
    // The unique object
    static const Traits & traits();
    // Offset in an array of T
    static void * lea(void* base, int n)
      { return (void*)( ((char*)base) + SZ*n ); }
    // Trivial default constructor
    static void   init(void* dst, int n) {}
    // Trivial copy constructor
    static void   copy(void* dst, const void* src, int n, int ) 
      { ::memcpy(dst, src, SZ*n); }
    // Trivial destructor
    static void   fini(void* dst, int n) {}
  };
  // --- Management of regular types
  template <class T> class NormTraits
  {
  public:
    // The unique object
    static const Traits & traits();
    // Offset in an array of T
    static void * lea(void* base, int n)
      { return (void*)( ((T*)base) + n ); }
    // Template based default constructor
    static void init(void* dst, int n) 
      { T* d = (T*)dst; while (--n>=0) { new ((void*)d) T; d++; } }
    // Template based copy constructor
    static void copy(void* dst, const void* src, int n, int zap)
      { T* d = (T*)dst; T* s = (T*)src; while (--n>=0) { 
          new ((void*)d) T(*s); if (zap) { s->~T(); }; d++; s++; } }
    // Template based destructor
    static void fini(void* dst, int n) 
      { T* d = (T*)dst; while (--n>=0) { d->~T(); d++; } }
  };
  // --- Base class for list nodes
  struct Node
  {
    Node *next;
    Node *prev;
  };
  // -- Class for list nodes
  template <class T> struct ListNode : public Node
  { 
    T val;
  };
  // -- Class for map nodes showing the hash
  struct HNode : public Node
  {
    HNode *hprev;
    unsigned int hashcode;
  };
  // -- Class for map nodes showing the hash and the key
  template <class K> struct SetNode : public HNode
  { 
    K key;
  };
  // -- Class for map nodes with everything
  template <class K, class T> struct MapNode : public SetNode<K>
  {
    T val;
  };
#if !GCONTAINER_NO_MEMBER_TEMPLATES
};
#endif


#if !GCONTAINER_NO_MEMBER_TEMPLATES
#define GCONT GCont::
#else
#define GCONT
#endif

template <int SZ> const GCONT Traits & 
GCONT TrivTraits<SZ>::traits()
{
  static const Traits theTraits = {
    SZ,
    TrivTraits<SZ>::lea,
    TrivTraits<SZ>::init,
    TrivTraits<SZ>::copy,
    TrivTraits<SZ>::fini
  };
  return theTraits;
}

template <class T> const GCONT Traits & 
GCONT NormTraits<T>::traits()
{
  static const Traits theTraits = {
    sizeof(T),
    NormTraits<T>::lea,
    NormTraits<T>::init,
    NormTraits<T>::copy,
    NormTraits<T>::fini
  };
  return theTraits;
}


// ------------------------------------------------------------
// DYNAMIC ARRAYS
// ------------------------------------------------------------


/** @name Dynamic Arrays

    These class implement arrays of objects of any type.  Each element is
    identified by an integer subscript.  The valid subscripts range is defined
    by dynamically adjustable lower- and upper-bounds.  Besides accessing and
    setting elements, member functions are provided to insert or delete
    elements at specified positions.

    Class \Ref{GArrayTemplate} implements all methods for manipulating arrays
    of type #TYPE#.  You should not however create instances of this class.
    You should instead use one of the following classes:
    \begin{itemize}
    \item Class \Ref{GArray<TYPE>} is the most general class,
    \item Class \Ref{GTArray<TYPE>} is more efficient, but only works for
          types that do not require sophisticated constructors or destructors,
          such as the plain old C types (e.g. #int# or #char# ...).
    \item Class \Ref{GPArray<TYPE>} implements an array of smart-pointers
          \Ref{GP<TYPE>} to objects of type #TYPE#.  Using this class 
          reduces the size of the code generated by the template instanciation.
    \end{itemize}

    Another variant of dynamic arrays is implemented in file \Ref{Arrays.h}.
    The main difference is that class \Ref{TArray}, \Ref{DArray} and
    \Ref{DPArray} implement a copy-on-demand scheme.

    @memo Dynamic arrays.  */
//@{

class DJVUAPI GArrayBase : public GCont
{
public:
  // -- CONSTRUCTORS
  GArrayBase(const GArrayBase &ref);
  GArrayBase(const Traits &traits);
  GArrayBase(const Traits &traits, int lobound, int hibound);
  // -- DESTRUCTOR
  ~GArrayBase();
  // -- ASSIGNMENT
  GArrayBase& operator= (const GArrayBase &ga);
  // -- ALTERATION
  void empty();
  void touch(int n);
  void resize(int lobound, int hibound);
  void shift(int disp);
  void del(int n, int howmany=1);
  void ins(int n, const void *src, int howmany=1);
  void steal(GArrayBase &ga);
protected:
  const Traits &traits;
  void  *data;
  int   minlo;
  int   maxhi;
  int   lobound;
  int   hibound;
};


/** Common base class for all dynamic arrays.  
    Class \Ref{GArrayTemplate} implements all methods for manipulating arrays
    of type #TYPE#.  You should not however create instances of this class.
    You should instead use class \Ref{GArray}, \Ref{GTArray} or
    \Ref{GPArray}. */

template<class TYPE>
class GArrayTemplate : protected GArrayBase
{
public:
  // -- CONSTRUCTORS
  GArrayTemplate(const Traits &traits) : GArrayBase(traits) {}
  GArrayTemplate(const Traits &traits, int lobound, int hibound)
    : GArrayBase(traits, lobound, hibound) {}
  // -- ACCESS
  /** Returns the number of elements in the array. */
  int size() const
    { return hibound-lobound+1; }
  /** Returns the lower bound of the valid subscript range. */
  int lbound() const
    { return lobound; }
  /** Returns the upper bound of the valid subscript range. */
  int hbound() const
    { return hibound; }
  /** Returns a reference to the array element for subscript #n#.  This
      reference can be used for both reading (as "#a[n]#") and writing (as
      "#a[n]=v#") an array element.  This operation will not extend the valid
      subscript range: an exception \Ref{GException} is thrown if argument #n#
      is not in the valid subscript range. */
  inline TYPE& operator[](int const n);
  /** Returns a constant reference to the array element for subscript #n#.
      This reference can only be used for reading (as "#a[n]#") an array
      element.  This operation will not extend the valid subscript range: an
      exception \Ref{GException} is thrown if argument #n# is not in the valid
      subscript range.  This variant of #operator[]# is necessary when dealing
      with a #const GArray<TYPE>#. */
  inline const TYPE& operator[](int n) const;
  // -- CONVERSION
  /** Returns a pointer for reading or writing the array elements.  This
      pointer can be used to access the array elements with the same
      subscripts and the usual bracket syntax.  This pointer remains valid as
      long as the valid subscript range is unchanged. If you change the
      subscript range, you must stop using the pointers returned by prior
      invocation of this conversion operator. */
  operator TYPE* ()
    { return ((TYPE*)data)-minlo; }
  /** Returns a pointer for reading (but not modifying) the array elements.
      This pointer can be used to access the array elements with the same
      subscripts and the usual bracket syntax.  This pointer remains valid as
      long as the valid subscript range is unchanged. If you change the
      subscript range, you must stop using the pointers returned by prior
      invocation of this conversion operator. */
  operator const TYPE* () const
    { return ((const TYPE*)data)-minlo; }
  // -- ALTERATION
  /** Erases the array contents. All elements in the array are destroyed.  
      The valid subscript range is set to the empty range. */
  void empty()
    { GArrayBase::empty(); }
  /** Extends the subscript range so that it contains #n#.
      This function does nothing if #n# is already int the valid subscript range.
      If the valid range was empty, both the lower bound and the upper bound
      are set to #n#.  Otherwise the valid subscript range is extended
      to encompass #n#. This function is very handy when called before setting
      an array element:
      \begin{verbatim}
       int lineno=1;
       GArray<GString> a;
       while (! end_of_file()) { 
         a.touch(lineno); 
         a[lineno++] = read_a_line(); 
       }
      \end{verbatim} */
  void touch(int n)
    { if (n<lobound || n>hibound) GArrayBase::touch(n); }
  /** Resets the valid subscript range to #0#---#hibound#.
      This function may destroy some array elements and may construct
      new array elements with the null constructor. Setting #hibound# to
      #-1# resets the valid subscript range to the empty range. */
  void resize(int hibound)
    { GArrayBase::resize(0, hibound); }
  /** Resets the valid subscript range to #lobound#---#hibound#. 
      This function may destroy some array elements and may construct
      new array elements with the null constructor. Setting #lobound# to #0# and
      #hibound# to #-1# resets the valid subscript range to the empty range. */
  void resize(int lobound, int hibound)
    { GArrayBase::resize(lobound, hibound); }
  /** Shifts the valid subscript range. Argument #disp# is added to both 
      bounds of the valid subscript range. Array elements previously
      located at subscript #x# will now be located at subscript #x+disp#. */
  void shift(int disp)
    { GArrayBase::shift(disp); }
  /** Deletes array elements. The array elements corresponding to
      subscripts #n#...#n+howmany-1# are destroyed. All array elements
      previously located at subscripts greater or equal to #n+howmany#
      are moved to subscripts starting with #n#. The new subscript upper
      bound is reduced in order to account for this shift. */
  void del(int n, int howmany=1)
    { GArrayBase::del(n, howmany); }
  /** Insert new elements into an array. This function inserts
      #howmany# elements at position #n# into the array. These
      elements are constructed using the default constructor for type
      #TYPE#.  All array elements previously located at subscripts #n#
      and higher are moved to subscripts #n+howmany# and higher. The
      upper bound of the valid subscript range is increased in order
      to account for this shift. */
  void ins(int n, int howmany=1)
    { GArrayBase::ins(n, 0, howmany); }
  /** Insert new elements into an array. The new elements are
      constructed by copying element #val# using the copy constructor
      for type #TYPE#. See \Ref{ins(int n, unsigned int howmany=1)}. */
  void ins(int n, const TYPE &val, int howmany=1)
    { GArrayBase::ins(n, (const void*)&val, howmany); }
  /** Steals contents from array #ga#.  After this call, array #ga# is empty,
      and this array contains everything previously contained in #ga#. */
  void steal(GArrayTemplate &ga)
    { GArrayBase::steal(ga); }
  // -- SORTING
  /** Sort array elements.  Sort all array elements in ascending
      order according to the less-or-equal comparison
      operator for type #TYPE#. */
  void sort()
    { sort(lbound(), hbound()); }
  /** Sort array elements in subscript range #lo# to #hi#.  Sort all array
      elements whose subscripts are in range #lo# to #hi# in ascending order
      according to the less-or-equal comparison operator for type #TYPE#.  The
      other elements of the array are left untouched.  An exception is thrown
      if arguments #lo# and #hi# are not in the valid subscript range.  */
  void sort(int lo, int hi);
};



/* That one must be implemented as a regular template function. */
template <class TYPE> void
GArrayTemplate<TYPE>::sort(int lo, int hi)
{
  TYPE *data = (TYPE*)(*this);
  while(true)
    {
  if (hi <= lo)
    return;
  if (hi > hibound || lo<lobound)
    G_THROW( ERR_MSG("GContainer.illegal_subscript") );
  // Test for insertion sort
  if (hi <= lo + 50)
    {
      for (int i=lo+1; i<=hi; i++)
        {
          int j = i;
          TYPE tmp = data[i];
          while ((--j>=lo) && !(data[j]<=tmp))
            data[j+1] = data[j];
          data[j+1] = tmp;
        }
      return;
    }
      // -- determine median-of-three pivot
  TYPE tmp = data[lo];
  TYPE pivot = data[(lo+hi)/2];
  if (pivot <= tmp)
    { tmp = pivot; pivot=data[lo]; }
  if (data[hi] <= tmp)
    { pivot = tmp; }
  else if (data[hi] <= pivot)
    { pivot = data[hi]; }
  // -- partition set
  int h = hi;
  int l = lo;
  while (l < h)
    {
      while (! (pivot <= data[l])) l++;
      while (! (data[h] <= pivot)) h--;
      if (l < h)
        {
          tmp = data[l];
          data[l] = data[h];
          data[h] = tmp;
          l = l+1;
          h = h-1;
      }
    }
      // -- recurse, small partition first
      //    tail-recursion elimination
      if (h - lo <= hi - l) {
        sort(lo,h);
        lo = l; // sort(l,hi)
      } else {
        sort(l,hi);
        hi = h; // sort(lo,h)
      }
    }
}

template<class TYPE> inline TYPE&
GArrayTemplate<TYPE>::operator[](int const n)
{
#if GCONTAINER_BOUNDS_CHECK
  if (n<lobound || n>hibound)
  {
    G_THROW( ERR_MSG("GContainer.illegal_subscript") ); 
  }
#endif
  return ((TYPE*)data)[n-minlo];
}


template<class TYPE> inline const TYPE &
GArrayTemplate<TYPE>::operator[](int const n) const
{
#if GCONTAINER_BOUNDS_CHECK
  if (n<lobound || n>hibound)
  {
    G_THROW( ERR_MSG("GContainer.illegal_subscript") ); 
  }
#endif
  return ((const TYPE*)data)[n-minlo];
}



/** Dynamic array for general types.  
    Template class #GArray<TYPE># implements an array of elements of type
    #TYPE#. This template class must be able to access the following
    functions.
    \begin{itemize}
    \item a default constructor #TYPE::TYPE()#, 
    \item a copy constructor #TYPE::TYPE(const TYPE &)#,
    \item and optionally a destructor #TYPE::~TYPE()#.
    \end{itemize}
    This class only implement constructors.  See class \Ref{GArrayTemplate}
    for a description of all access methods. */

template<class TYPE>
class GArray : public GArrayTemplate<TYPE>
{
public:
  /** Constructs an empty array. The valid subscript range is initially
      empty. Member function #touch# and #resize# provide convenient ways
      to enlarge the subscript range. */
  GArray() 
    : GArrayTemplate<TYPE>(GCONT NormTraits<TYPE>::traits() ) {}
  /** Constructs an array with subscripts in range 0 to #hibound#. 
      The subscript range can be subsequently modified with member functions
      #touch# and #resize#. */
  GArray(int hi) 
    : GArrayTemplate<TYPE>(GCONT NormTraits<TYPE>::traits(), 0, hi ) {}
  /** Constructs an array with subscripts in range #lobound# to #hibound#.  
      The subscript range can be subsequently modified with member functions
      #touch# and #resize#. */
  GArray(int lo, int hi) 
    : GArrayTemplate<TYPE>(GCONT NormTraits<TYPE>::traits(), lo, hi ) {}
  // Copy operator
  GArray& operator=(const GArray &r)
    { GArrayBase::operator=(r); return *this; }
};


/** Dynamic array for smart pointers.  
    Template class #GPArray<TYPE># implements an array of elements of type
    #GP<TYPE># (see \Ref{GSmartPointer.h}).  Significantly smaller code sizes
    can be achieved by using this class instead of the more general
    #GArray<GP<TYPE>>#.  
    This class only implement constructors.  See class \Ref{GArrayTemplate}
    for a description of all access methods.  */

template<class TYPE>
class GPArray : public GArrayTemplate<GP<TYPE> >
{
public:
  GPArray() 
    : GArrayTemplate<GP<TYPE> >(GCONT NormTraits<GPBase>::traits() ) {}
  GPArray(int hi) 
    : GArrayTemplate<GP<TYPE> >(GCONT NormTraits<GPBase>::traits(), 0, hi ) {}
  GPArray(int lo, int hi) 
    : GArrayTemplate<GP<TYPE> >(GCONT NormTraits<GPBase>::traits(), lo, hi ) {}
  // Copy operator
  GPArray& operator=(const GPArray &r)
    { GArrayBase::operator=(r); return *this; }
};

/** Dynamic array for simple types.  
    Template class #GTArray<TYPE># implements an array of elements of {\em
    simple} type #TYPE#.  {\em Simple} means that objects of type #TYPE# can
    be created, copied, moved or destroyed without using specific constructors
    or destructor functions.  Class #GTArray<TYPE># will move or copy objects
    using simple bitwise copies.  Otherwise you must use class #GArray<TYPE>#. 
    This class only implement constructors.  See class \Ref{GArrayTemplate}
    for a description of all access methods.  */
template<class TYPE>
class GTArray : public GArrayTemplate<TYPE>
{
public:
  GTArray() 
    : GArrayTemplate<TYPE>(GCONT TrivTraits<sizeof(TYPE)>::traits() ) {}
  GTArray(int hi) 
    : GArrayTemplate<TYPE>(GCONT TrivTraits<sizeof(TYPE)>::traits(), 0, hi ) {}
  GTArray(int lo, int hi) 
    : GArrayTemplate<TYPE>(GCONT TrivTraits<sizeof(TYPE)>::traits(), lo, hi ) {}
  // Copy operator
  GTArray& operator=(const GTArray &r)
    { GArrayBase::operator=(r); return *this; }
};


//@}



// ------------------------------------------------------------
// DOUBLY LINKED LISTS
// ------------------------------------------------------------


/** @name Doubly Linked Lists

    The template classes \Ref{GList} and \Ref{GPList} implement a doubly
    linked list of objects of arbitrary types. Member functions are provided
    to search the list for an element, to insert or delete elements at
    specified positions.  Theses template class must be able to access
    \begin{itemize}
    \item a default constructor #TYPE::TYPE()#,
    \item a copy constructor #TYPE::TYPE(const TYPE &)#,
    \item optionally a destructor #TYPE::~TYPE()#,
    \item and optionally a comparison operator #TYPE::operator==(const TYPE &)#.
    \end{itemize} 
    @memo Doubly linked lists.  
*/
//@{

/** Generic iterator class.
    This class represents a position in a list (see \Ref{GList}) or a map
    (see \Ref{GMap}).   As demonstrated by the following examples,
    this class should be used to iterate over the objects contained
    in a list or a map:
    \begin{verbatim}
    void print_list(GList<GString> a)
    {
      for (GPosition i = a ; i; ++i) 
        DjVuPrintMessage("%s\n", (const char*) a[i] );
    }

    void print_list_backwards(GList<GString> a)
    {
      for (GPosition i = a.lastpos() ; i; --i) 
        DjVuPrintMessage("%s\n", (const char*) a[i] );
    }
    \end{verbatim}
    GPosition objects should only be used with the list or map for which they
    have been created (using the member functions #firstpos# or #lastpos# of
    the container).  Furthermore, you should never use a GPosition object
    which designates a list element which has been removed from the list
    (using member function #del# or by other means.)
*/

class DJVUAPI GPosition : protected GCont
{
public:
  /** Creates a null GPosition object. */
  GPosition() : ptr(0), cont(0) {}
  /** Creates a copy of a GPosition object. */
  GPosition(const GPosition &ref) : ptr(ref.ptr), cont(ref.cont) {}
  /** Tests whether this GPosition object is non null. */
  operator int() const 
    { return !!ptr; }
  /** Tests whether this GPosition object is null. */
  int operator !() const 
    { return !ptr; }
  /** Moves this GPosition object to the next object in the container. */
  GPosition& operator ++() 
    { if (ptr) ptr = ptr->next; return *this; }
  /** Moves this GPosition object to the previous object in the container. */
  GPosition& operator --() 
    { if (ptr) ptr = ptr->prev; return *this; }
  // Internal. Do not use.
  GPosition(Node *p, void *c) : ptr(p), cont(c) {}
#if GCONTAINER_BOUNDS_CHECK
  Node *check(void *c) 
    { if (!ptr || c!=cont) throw_invalid(c); return ptr; }
  const Node *check(void *c) const
    { if (!ptr || c!=cont) throw_invalid(c); return ptr; }
#else
  Node *check(void *c) 
    { return ptr; }
  const Node *check(void *c) const
    { return ptr; }
#endif
protected:
  Node *ptr;
  void *cont;
  friend class GListBase;
  friend class GSetBase;
  void throw_invalid(void *c) const no_return;
};


class DJVUAPI GListBase : public GCont
{
protected:
  GListBase(const Traits& traits);
  GListBase(const GListBase &ref);
  void append(Node *n);
  void prepend(Node *n);
  void insert_after(GPosition pos, Node *n);
  void insert_before(GPosition pos, Node *n);
  void insert_before(GPosition pos, GListBase &fromlist, GPosition &frompos);
  void del(GPosition &pos);
protected:
  const Traits &traits;
  int nelem;
  Node head;
public:
  ~GListBase();
  GListBase & operator= (const GListBase & gl);
  GPosition firstpos() const { return GPosition(head.next, (void*)this); }
  GPosition lastpos() const { return GPosition(head.prev, (void*)this); }
  bool isempty() const { return nelem==0; };
  GPosition nth(unsigned int n) const;
  void empty();
};


template<class TI>
class GListImpl : public GListBase
{
  typedef GCONT ListNode<TI> LNode;
protected:
  GListImpl();
  static Node * newnode(const TI &elt);
  int operator==(const GListImpl<TI> &l2) const;
  int search(const TI &elt, GPosition &pos) const;
};

template<class TI> 
GListImpl<TI>::GListImpl() 
  : GListBase( GCONT NormTraits<LNode>::traits() ) 
{ 
}

template<class TI> GCONT Node *
GListImpl<TI>::newnode(const TI &elt)
{
  LNode  *n = (LNode *) operator new (sizeof(LNode ));
#if GCONTAINER_ZERO_FILL
  memset((void*)n, 0, sizeof(LNode ));
#endif
  new ((void*)&(n->val)) TI(elt);
  return (Node*) n;
}

template<class TI> int
GListImpl<TI>::operator==(const GListImpl<TI> &l2) const
{
  Node *p, *q;
  for (p=head.next, q=l2.head.next; p && q; p=p->next, q=q->next )
    if (((LNode*)p)->val != ((LNode*)q)->val)
      return 0;
  return p==0 && q==0;
}

template<class TI> int
GListImpl<TI>::search(const TI &elt, GPosition &pos) const
{
  Node *n = (pos ? pos.check((void*)this) : head.next);
  for (; n; n=n->next) 
    if ( ((LNode *)n)->val == elt ) 
      break;
  if (n) pos = GPosition(n, (void*)this);
  return (n != 0);
}


/** Common base class for all doubly linked lists.  
    Class \Ref{GListTemplate} implements all methods for manipulating lists 
    of of objects of type #TYPE#.  You should not however create instances of 
    this class. You should instead use class \Ref{GList} or \Ref{GPList}. */

template <class TYPE, class TI>
class GListTemplate : protected GListImpl<TI>
{
  typedef GCONT ListNode<TI> LNode;
public:
  // -- ACCESS
  /** Returns the number of elements in the list. */
  int size() const
    { return this->nelem; }
  /** Returns the first position in the list. See \Ref{GPosition}. */
  GPosition firstpos() const
    { return GListImpl<TI>::firstpos(); }
  /** Returns the last position in the list. See \Ref{GPosition}. */
  GPosition lastpos() const
    { return GListImpl<TI>::lastpos(); }
  /** Implicit notation for GList::firstpos(). */
  operator GPosition() const
    { return firstpos(); }    
  /** Returns a reference to the list element at position #pos#.  This
      reference can be used for both reading (as "#a[n]#") and modifying (as
      "#a[n]=v#") a list element.  Using an invalid position will cause a
      segmentation violation. See \Ref{GPosition} for efficient operations on
      positions. */
  TYPE& operator[](GPosition pos)
    { return (TYPE&) (((LNode *)pos.check((void*)this))->val); }
  /** Returns a constant reference to the list element at position #pos#.
      This reference only be used for reading a list element.  An exception
      \Ref{GException} is thrown if #pos# is not a valid position. This
      variant of #operator[]# is necessary when dealing with a #const
      GList<TYPE>#.  See \Ref{GPosition} for efficient operations on
      positions. */
  const TYPE& operator[](GPosition pos) const
    { return (const TYPE&) (((const LNode *)pos.check((void*)this))->val); }
  // -- TEST
  /** Tests whether a list is empty.  
      Returns a non zero value if the list contains no elements. */
  bool isempty() const 
    { return this->nelem==0; }
  /** Compares two lists. Returns a non zero value if and only if both lists
      contain the same elements (as tested by #TYPE::operator==(const TYPE&)#
      in the same order. */
  int operator==(const GListTemplate<TYPE,TI> &l2) const
    { return GListImpl<TI>::operator==(l2); }
  // -- SEARCHING
  /** Returns the position #pos# of the #n#-th list element.  An invalid
      position is returned if the list contains less than #n# elements. The
      operation works by sequentially scanning the list until reaching the
      #n#-th element. */
  GPosition nth(unsigned int n) const
    { return GListImpl<TI>::nth(n); }
  /*  Compatibility */
  int nth(unsigned int n, GPosition &pos) const
    { GPosition npos=nth(n); if (npos) pos=npos; return !!pos; }
  /** Tests whether the list contains a given element.  If the list contains
      #elt#, the position of the the first list element equal to #elt# as
      checked by #TYPE::operator==(const TYPE&)# is returned.  Otherwise an
      invalid position is returned. */
  GPosition contains(const TYPE &elt) const
    { GPosition pos; GListImpl<TI>::search((const TI&)elt, pos); return pos; }
  /** Searches the list for a given element. If position #pos# is a valid
      position for this list, the search starts at the specified position. If
      position #pos# is not a valid position, the search starts at the
      beginning of the list.  The list elements are sequentially compared with
      #elt# using #TYPE::operator==(const TYPE&)#.  As soon as a list element
      is equal to #elt#, function #search# sets argument #pos# with the
      position of this list element and returns 1.  If however the search
      reaches the end of the list, function #search# returns 0 and leaves
      #pos# unchanged. */
  int search(const TYPE &elt, GPosition &pos) const
    { return GListImpl<TI>::search((const TI&)elt, pos); }
  // -- ALTERATION
  /** Erases the list contents.  All list elements are destroyed and
      unlinked. The list is left with zero elements. */
  void empty()
    { GListImpl<TI>::empty(); }
  /** Inserts an element after the last element of the list. 
      The new element is initialized with a copy of argument #elt#. */
  void append(const TYPE &elt)
    { GListImpl<TI>::append(this->newnode((const TI&)elt)); }
  /** Inserts an element before the first element of the list. 
      The new element is initialized with a copy of argument #elt#. */
  void prepend(const TYPE &elt)
    { GListImpl<TI>::prepend(this->newnode((const TI&)elt)); }
  /** Inserts a new element after the list element at position #pos#.  When
      position #pos# is null the element is inserted at the beginning of the
      list.  The new element is initialized with a copy of #elt#. */
  void insert_after(GPosition pos, const TYPE &elt)
    { GListImpl<TI>::insert_after(pos, this->newnode((const TI&)elt)); }
  /** Inserts a new element before the list element at position #pos#. When
      position #pos# is null the element is inserted at the end of the
      list. The new element is initialized with a copy of #elt#. */
  void insert_before(GPosition pos, const TYPE &elt)
    { GListImpl<TI>::insert_before(pos, this->newnode((const TI&)elt)); }
  /** Inserts an element of another list into this list.  This function
      removes the element at position #frompos# in list #frompos#, inserts it
      in the current list before the element at position #pos#, and advances
      #frompos# to the next element in list #fromlist#. When position #pos# is
      null the element is inserted at the end of the list. */
  void insert_before(GPosition pos, GListTemplate<TYPE,TI> &fromlist, GPosition &frompos)
    { GListImpl<TI>::insert_before(pos, fromlist, frompos); }
  /** Destroys the list element at position #pos#.  This function does 
      nothing unless position #pos# is a valid position. */
  void del(GPosition &pos)
    { GListImpl<TI>::del(pos); }
  /* Old iterators. Do not use. */
#if GCONTAINER_OLD_ITERATORS
  void first(GPosition &pos) const { pos = firstpos(); }
  void last(GPosition &pos) const { pos = lastpos(); }
  const TYPE *next(GPosition &pos) const 
    { if (!pos) return 0; const TYPE *x=&((*this)[pos]); ++pos; return x; }
  const TYPE *prev(GPosition &pos) const 
    { if (!pos) return 0; const TYPE *x=&((*this)[pos]); --pos; return x; }
  TYPE *next(GPosition &pos)
    { if (!pos) return 0; TYPE *x=&((*this)[pos]); ++pos; return x; }
  TYPE *prev(GPosition &pos)
    { if (!pos) return 0; TYPE *x=&((*this)[pos]); --pos; return x; }
#endif
};


/** Doubly linked lists.  Template class #GList<TYPE># implements a doubly
    linked list of elements of type #TYPE#.  This class only implement
    constructors.  See class \Ref{GListTemplate} and \Ref{GPosition} for a
    description of all access methods. */

template <class TYPE>
class GList : public GListTemplate<TYPE,TYPE>
{
public:
  /** Null Constructor. Constructs a list with zero elements. */
  GList() : GListTemplate<TYPE,TYPE>() {}
  GList& operator=(const GList &r) 
    { GListBase::operator=(r); return *this; }
};


/** Doubly linked lists for smart pointers. 
    Template class #GList<TYPE># implements a doubly linked list of elements
    of type #GP<TYPE># (see \Ref{GSmartPointer.h}).  Significantly smaller
    code sizes can be achieved by using this class instead of the more general
    #GArray<GP<TYPE>>#.  
    This class only implement constructors.  See class \Ref{GListTemplate} and
    \Ref{GPosition} for a description of all access methods. */

template <class TYPE>
class GPList : public GListTemplate<GP<TYPE>,GPBase>
{
public:
  /** Null Constructor. Constructs a list with zero elements. */
  GPList() : GListTemplate<GP<TYPE>,GPBase>() {}
  GPList& operator=(const GPList &r) 
    { GListBase::operator=(r); return *this; }
};


//@}



// ------------------------------------------------------------
// ASSOCIATIVE MAPS
// ------------------------------------------------------------

/** @name Associative Maps

    These template classes implements a associative maps.  The associative map
    contains an arbitrary number of entries. Each entry is a pair containing
    one element of type #KTYPE# (named the "key") and one element of type
    #VTYPE# (named the "value").  All entries have distinct keys. 
    These template class must be able to access the following functions:
    \begin{itemize}
    \item a #VTYPE# default constructor #VTYPE::VTYPE()#, 
    \item a #VTYPE# copy constructor #VTYPE::VTYPE(const VTYPE &)#, 
    \item optionally a #VTYPE# destructor #VTYPE::~VTYPE()#,
    \item a #KTYPE# default constructor #KTYPE::KTYPE()#, 
    \item a #KTYPE# copy constructor #KTYPE::KTYPE(const KTYPE &)#, 
    \item optionally a #KTYPE# destructor #KTYPE::~KTYPE()#,
    \item a #KTYPE# comparison operator #KTYPE::operator==(const KTYPE &)#,
    \item and a #KTYPE# hashing function #hash(const KTYPE&)#.
    \end{itemize} 
    The hashing function must return an #unsigned int# number. Multiple
    invocations of the hashing function with equal arguments (in the sense of
    #KTYPE::operator==#) must always return the same number.  
    Position objects (see \Ref{GPosition}) may be used to iterate over the
    entries contained by an associative map. 
    @memo Associative maps.
*/
//@{

class DJVUAPI GSetBase : public GCont
{
protected:
  GSetBase(const Traits &traits);
  GSetBase(const GSetBase &ref);
  static GCONT HNode *newnode(const void *key);
  HNode *hashnode(unsigned int hashcode) const;
  HNode *installnode(HNode *n);
  void   deletenode(HNode *n);
protected:
  const Traits &traits;
  int nelems;
  int nbuckets;
  HNode **table;
  GPBuffer<HNode *> gtable;
  HNode *first;
private:
  void insertnode(HNode *n);
  void rehash(int newbuckets);
public:
  ~GSetBase();
  GSetBase& operator=(const GSetBase &ref);
  GPosition firstpos() const;
  void del(GPosition &pos); 
  void empty();
};

template <class K>
class GSetImpl : public GSetBase
{
  typedef GCONT SetNode<K> SNode;
protected:
  GSetImpl();
  GSetImpl(const Traits &traits);
  HNode *get(const K &key) const;
  HNode *get_or_throw(const K &key) const;
  HNode *get_or_create(const K &key);
public:
  GPosition contains(const K &key) const 
    { return GPosition( get(key), (void*)this); }
  void del(const K &key) 
    { deletenode(get(key)); }
};

template<class K>
GSetImpl<K>::GSetImpl()
  : GSetBase( GCONT NormTraits<GCONT SetNode<K> >::traits() )
{ 
}

template<class K>
GSetImpl<K>::GSetImpl(const Traits &traits)
  : GSetBase(traits) 
{ 
}

template<class K> GCONT HNode *
GSetImpl<K>::get(const K &key) const
{ 
  unsigned int hashcode = hash(key);
  for (SNode *s=(SNode*)hashnode(hashcode); s; s=(SNode*)(s->hprev))
    if (s->hashcode == hashcode && s->key == key) return s;
  return 0;
}

#if GCONTAINER_BOUNDS_CHECK
template<class K> GCONT HNode *
GSetImpl<K>::get_or_throw(const K &key) const
{ 
  HNode *m = get(key);
  if (!m)
  {
    G_THROW( ERR_MSG("GContainer.cannot_add") );
  }
  return m;
}
#else
template<class K> inline GCONT HNode *
GSetImpl<K>::get_or_throw(const K &key) const
{ 
  return get(key);
}
#endif

template<class K> GCONT HNode *
GSetImpl<K>::get_or_create(const K &key)
{
  HNode *m = get(key);
  if (m) return m;
  SNode *n = (SNode*) operator new (sizeof(SNode));
#if GCONTAINER_ZERO_FILL
  memset(n, 0, sizeof(SNode));
#endif
  new ((void*)&(n->key)) K ( key );
  n->hashcode = hash((const K&)(n->key));
  installnode(n);
  return n;
}

template <class K, class TI>
class GMapImpl : public GSetImpl<K>
{
  typedef GCONT MapNode<K,TI> MNode;
protected:
  GMapImpl();
  GMapImpl(const GCONT Traits &traits);
  GCONT HNode* get_or_create(const K &key);
};

template<class K, class TI>
GMapImpl<K,TI>::GMapImpl()
  : GSetImpl<K> ( GCONT NormTraits<GCONT MapNode<K,TI> >::traits() ) 
{ 
}

template<class K, class TI>
GMapImpl<K,TI>::GMapImpl(const GCONT Traits &traits)
  : GSetImpl<K>(traits) 
{ 
}

template<class K, class TI> GCONT HNode *
GMapImpl<K,TI>::get_or_create(const K &key)
{
  GCONT HNode *m = this->get(key);
  if (m) return m;
  MNode *n = (MNode*) operator new (sizeof(MNode));
#if GCONTAINER_ZERO_FILL
  memset(n, 0, sizeof(MNode));
#endif
  new ((void*)&(n->key)) K  (key);
  new ((void*)&(n->val)) TI ();
  n->hashcode = hash((const K&)(n->key));
  this->installnode(n);
  return n;
}



/** Common base class for all associative maps.
    Class \Ref{GArrayTemplate} implements all methods for manipulating 
    associative maps with key type #KTYPE# and value type #VTYPE#. 
    You should not however create instances of this class.
    You should instead use class \Ref{GMap} or \Ref{GPMap}. */

template <class KTYPE, class VTYPE, class TI>
class GMapTemplate : protected GMapImpl<KTYPE,TI>
{
  typedef GCONT MapNode<KTYPE,TI> MNode;
public:
  /** Returns the number of elements in the map. */
  int size() const
    { return this->nelems; }
  /** Returns the first position in the map. */
  GPosition firstpos() const
    { return GMapImpl<KTYPE,TI>::firstpos(); }
  /** Implicit notation for GMap::firstpos(). */
  operator GPosition() const
    { return firstpos(); }    
  /** Tests whether the associative map is empty.  
      Returns a non zero value if and only if the map contains zero entries. */
  bool isempty() const
    { return this->nelems==0; }
  /** Searches an entry for key #key#.  If the map contains an entry whose key
      is equal to #key# according to #KTYPE::operator==(const KTYPE&)#, this
      function returns its position.  Otherwise it returns an invalid
      position. */
  GPosition contains(const KTYPE &key) const
    { return GMapImpl<KTYPE,TI>::contains(key); }
  /*  Compatibility */
  GPosition contains(const KTYPE &key, GPosition &pos) const
    { return pos = GMapImpl<KTYPE,TI>::contains(key); }
  // -- ALTERATION
  /** Erases the associative map contents.  All entries are destroyed and
      removed. The map is left with zero entries. */
  void empty()
    { GMapImpl<KTYPE,TI>::empty(); }
  /** Returns a constant reference to the key of the map entry at position
      #pos#.  An exception \Ref{GException} is thrown if position #pos# is not
      valid.  There is no direct way to change the key of a map entry. */
  const KTYPE &key(const GPosition &pos) const
    { return (const KTYPE&)(((MNode*)(pos.check((void*)this)))->key); }
  /** Returns a reference to the value of the map entry at position #pos#.
      This reference can be used for both reading (as "#a[n]#") and modifying
      (as "#a[n]=v#").  An exception \Ref{GException} is thrown if position
      #pos# is not valid. */
  VTYPE& operator[](const GPosition &pos)
    { return (VTYPE&)(((MNode*)(pos.check((void*)this)))->val); }
  /** Returns a constant reference to the value of the map entry at position
      #pos#.  This reference can only be used for reading (as "#a[n]#") the
      entry value.  An exception \Ref{GException} is thrown if position #pos#
      is not valid. */
  const VTYPE& operator[](const GPosition &pos) const
    { return (const VTYPE&)(((MNode*)(pos.check((void*)this)))->val); }
  /** Returns a constant reference to the value of the map entry for key
      #key#.  This reference can only be used for reading (as "#a[n]#") the
      entry value.  An exception \Ref{GException} is thrown if no entry
      contains key #key#. This variant of #operator[]# is necessary when
      dealing with a #const GMAP<KTYPE,VTYPE>#. */
  const VTYPE& operator[](const KTYPE &key) const
    { return (const VTYPE&)(((const MNode*)(this->get_or_throw(key)))->val); }
  /** Returns a reference to the value of the map entry for key #key#.  This
      reference can be used for both reading (as "#a[n]#") and modifying (as
      "#a[n]=v#"). If there is no entry for key #key#, a new entry is created
      for that key with the null constructor #VTYPE::VTYPE()#. */
  VTYPE& operator[](const KTYPE &key)
    { return (VTYPE&)(((MNode*)(this->get_or_create(key)))->val); }
  /** Destroys the map entry for position #pos#.  
      Nothing is done if position #pos# is not a valid position. */
  void del(GPosition &pos)
    { GSetBase::del(pos); }
  /** Destroys the map entry for key #key#.  
      Nothing is done if there is no entry for key #key#. */
  void del(const KTYPE &key)
    { GMapImpl<KTYPE,TI>::del(key); }
  /* Old iterators. Do not use. */
#if GCONTAINER_OLD_ITERATORS
  void first(GPosition &pos) const { pos = firstpos(); }
  const VTYPE *next(GPosition &pos) const 
    { if (!pos) return 0; const VTYPE *x=&((*this)[pos]); ++pos; return x; }
  VTYPE *next(GPosition &pos)
    { if (!pos) return 0; VTYPE *x=&((*this)[pos]); ++pos; return x; }
#endif
};



/** Associative maps.  
    Template class #GMap<KTYPE,VTYPE># implements an associative map.
    The map contains an arbitrary number of entries. Each entry is a
    pair containing one element of type #KTYPE# (named the "key") and one
    element of type #VTYPE# (named the "value").  
    The entry associated to a particular value of the key can retrieved
    very efficiently.
    This class only implement constructors.  See class \Ref{GMapTemplate} and
    \Ref{GPosition} for a description of all access methods.*/

template <class KTYPE, class VTYPE>
class GMap : public GMapTemplate<KTYPE,VTYPE,VTYPE>
{
public:
  // -- ACCESS
  GMap() : GMapTemplate<KTYPE,VTYPE,VTYPE>() {}
  GMap& operator=(const GMap &r) 
    { GSetBase::operator=(r); return *this; }
};

/** Associative maps for smart-pointers.  
    Template class #GMap<KTYPE,VTYPE># implements an associative map for key
    type #KTYPE# and value type #GP<VTYPE># (see \Ref{GSmartPointer.h}).  The
    map contains an arbitrary number of entries. Each entry is a pair
    containing one element of type #KTYPE# (named the "key") and one aelement
    of type #VTYPE# (named the "value").  The entry associated to a particular
    value of the key can retrieved very efficiently.
    Significantly smaller code sizes can be achieved by using this class
    instead of the more general #GMap<KTYPE,GP<VTYPE>># (see \Ref{GMap}).
    This class only implement constructors.  See class \Ref{GMapTemplate} and
    \Ref{GPosition} for a description of all access methods.*/

template <class KTYPE, class VTYPE>
class GPMap : public GMapTemplate<KTYPE,GP<VTYPE>,GPBase>
{
public:
  GPMap() : GMapTemplate<KTYPE,GP<VTYPE>,GPBase>() {}
  GPMap& operator=(const GPMap &r) 
    { GSetBase::operator=(r); return *this; }
};



//@}
//@}
//@}

// ------------ THE END


#ifdef HAVE_NAMESPACES
}
# ifndef NOT_USING_DJVU_NAMESPACE
using namespace DJVU;
# endif
#endif
#endif


