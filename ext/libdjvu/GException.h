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

#ifndef _GEXCEPTION_H_
#define _GEXCEPTION_H_
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#if NEED_GNUG_PRAGMAS
# pragma interface
#endif

#ifndef no_return
#ifdef __GNUC__
#define no_return __attribute__ ((noreturn))
#else
#define no_return
#endif
#endif

/** @name GException.h

    Files #"GException.h"# and #"GException.cpp"# define a portable exception
    scheme used through the DjVu Reference Library. This scheme can use native
    C++ exceptions or an exception emulation based on #longjmp#/#setjmp#. A
    particular model can be forced a compile time by defining option
    #CPP_SUPPORTS_EXCEPTIONS# or #USE_EXCEPTION_EMULATION#.
    
    This emulation code was motivated because many compilers did not properly
    support exceptions as mandated by the C++ standard documents. This
    emulation is now considered obsolete because (a) it is not able to call
    the proper destructors when an exception occurs, and (b) it is not thread
    safe.  Although all modern C++ compiler handle exception decently, the
    exception handling intrinsics are not always thread safe.  Therefore we
    urge programmers to {\em only} use exceptions to signal error conditions
    that force the library to discontinue execution.
    
    There are four macros for handling exceptions.  Macros #G_TRY#, #G_CATCH# and
    #G_ENDCATCH# are used to define an exception catching block.  Exceptions can
    be thrown at all times using macro #G_THROW(cause)#. An exception can be
    re-thrown from a catch block using macro #G_RETHROW#.
    
    Example:
    \begin{verbatim}
    G_TRY
      {
        // program lines which may result in a call to THROW()
        G_THROW("message");
      }
    G_CATCH(ex) 
      {
        // Variable ex refers to a GException object.
        ex.perror();  
        // You can rethrow the exception to an outer exception handler.
        G_RETHROW;
      }
    G_ENDCATCH;
    \end{verbatim} 

    @memo 
    Portable exceptions.
    @author 
    L\'eon Bottou <leonb@research.att.com> -- initial implementation.\\
    Andrei Erofeev <eaf@geocities.com> -- fixed message memory allocation.
*/
//@{

#include "DjVuGlobal.h"

// Check if compiler supports native exceptions
#ifdef HAVE_CONFIG_H
# if HAVE_EXCEPTIONS
#  define CPP_SUPPORTS_EXCEPTIONS
# endif
#else
# if defined(_MSC_VER)
#  define CPP_SUPPORTS_EXCEPTIONS
# endif
# if defined(__MWERKS__)
#  define CPP_SUPPORTS_EXCEPTIONS
# endif
# if defined(__EXCEPTIONS)
#  define CPP_SUPPORTS_EXCEPTIONS
# endif
#endif
// Decide which exception model to use
#ifndef CPP_SUPPORTS_EXCEPTIONS
# ifndef USE_EXCEPTION_EMULATION
#  define USE_EXCEPTION_EMULATION
# endif
#endif

#ifdef USE_EXCEPTION_EMULATION
# include <setjmp.h>
#endif

#ifdef HAVE_NAMESPACES
namespace DJVU {
# ifdef NOT_DEFINED // Just to fool emacs c++ mode
}
# endif
#endif

/** Exception class.  
    The library always uses macros #G_TRY#, #G_THROW#, #G_CATCH# and #G_ENDCATCH# for
    throwing and catching exceptions (see \Ref{GException.h}). These macros
    only deal with exceptions of type #GException#. */

class DJVUAPI GException {
public:
  enum source_type { GINTERNAL=0, GEXTERNAL, GAPPLICATION, GOTHER };
  /** Constructs a GException.  This constructor is usually called by macro
      #G_THROW#.  Argument #cause# is a plain text error message. As a
      convention, string #ByteStream::EndOfFile# is used when reaching an unexpected
      end-of-file condition and string #DataPool::Stop# is used when the user
      interrupts the execution. The remaining arguments are usually provided
      by the predefined macros #__FILE__#, #__LINE__#, and (G++ and EGCS only)
      #__PRETTY_FUNCTION__#.  */
  GException (const char *cause, const char *file=0, int line=0, 
              const char *func=0, const source_type source=GINTERNAL);

  /** Copy Constructor. */
  GException (const GException & exc);
  
  /** Null Constructor. */
  GException ();
  
  /** Destructor. */
  virtual ~GException(void);
  
  /** Copy Operator. */
  GException & operator=(const GException & exc);
  
  /** Prints an error message on stderr.
      This function no longer takes a message parameter because 
      some instances used a i18n message id and other instances
      used a literal string. */
  void perror(void) const;
  
  /** Returns the string describing the cause of the exception.  The returned
      pointer is never null.  Exception handlers should not rely on the value
      of the string #cause#.  As a convention however, string
      #ByteStream::EndOfFile# is used
      when reaching an unexpected end-of-file condition and string
      #DataPool::Stop# is used when the user interrupts the execution. These
      strings can be tested by the exception handlers, with
      #cmp_cause#. Similar conventional strings may be defined
      in the future. They all will be small strings with only uppercase
      characters. */
  const char* get_cause(void) const;

  /** Compares the cause with the specified string, ignoring anything after
      the first tab. */
  int cmp_cause(const char s2[]) const;

  /** Compares the cause with the specified string, ignoring anything after
      the first tab. */
  static int cmp_cause(const char s1[],const char s2[]);

  /** Returns the function name from which the exception was thrown.
      A null pointer is returned if no function name is available. */
  const char* get_function(void) const { return func; }
  
  /** Returns the file name from which the exception was thrown.
      A null pointer is returned if no file name is available. */
  const char* get_file(void) const { return file; }
 
  /** Returns the exception source */
  source_type get_source(void) const { return source; }
 
  /** Returns the line number from which the exception was thrown.
      A zero is returned if no line number is available. */
  int get_line(void) const { return line; };
  
  //  Magic cause string
  static const char * const outofmemory;

private:
  const char *cause;
  const char *file;
  const char *func;
  int line;
  source_type source;
};

//@}

#undef G_TRY
#undef G_CATCH
#undef G_CATCH_ALL
#undef G_ENDCATCH
#undef G_RETHROW
#undef G_THROW
#undef G_THROW_TYPE
#undef G_THROW_INTERNAL
#undef G_THROW_EXTERNAL
#undef G_THROW_APPLICATION
#undef G_THROW_OTHER

#ifndef USE_EXCEPTION_EMULATION

// Compiler supports ANSI C++ exceptions.
// Defined exception macros accordingly.

class DJVUAPI GExceptionHandler {
public:
#ifndef NO_LIBGCC_HOOKS
  static void exthrow(const GException &) no_return;
#else
  static void exthrow(const GException ) no_return;
#endif /* NO_LIBGCC_HOOKS */
  static void rethrow(void) no_return;
};

#define G_TRY        try
#define G_CATCH(n)   catch(const GException &n) { 
#define G_CATCH_ALL   catch(...) { 
#define G_ENDCATCH   } 
#define G_RETHROW    GExceptionHandler::rethrow()
#define G_EMTHROW(ex)  GExceptionHandler::exthrow(ex)
#ifdef __GNUG__
#define G_THROW_TYPE(msg,xtype) GExceptionHandler::exthrow \
  (GException(msg, __FILE__, __LINE__, __PRETTY_FUNCTION__, xtype))
#else
#define G_THROW_TYPE(msg,xtype) GExceptionHandler::exthrow \
  (GException(msg, __FILE__, __LINE__,0, xtype))
#endif

#else // USE_EXCEPTION_EMULATION

// Compiler does not support ANSI C++ exceptions.
// Emulate with setjmp/longjmp.

class DJVUAPI GExceptionHandler {
public:
  jmp_buf jump;
  GExceptionHandler *next;
  GException current;
public:
  static GExceptionHandler *head;
  static void emthrow(const GException &) no_return;
public:
  GExceptionHandler() { next = head; };
  ~GExceptionHandler() { head = next; };
};

#define G_TRY    do { GExceptionHandler __exh; \
                      if (!setjmp(__exh.jump)) \
                      { GExceptionHandler::head = &__exh;

#define G_CATCH_ALL } else { GExceptionHandler::head = __exh.next; 
#define G_CATCH(n) G_CATCH_ALL const GException& n = __exh.current;

#define G_ENDCATCH } } while(0)

#define G_RETHROW    GExceptionHandler::emthrow(__exh.current)

#ifdef __GNUG__
#define G_THROW_TYPE(msg,xtype) GExceptionHandler::emthrow \
  (GException(msg, __FILE__, __LINE__, __PRETTY_FUNCTION__, xtype)) 
#define G_EMTHROW(ex) GExceptionHandler::emthrow(ex)
#else
#define G_THROW_TYPE(m,xtype) GExceptionHandler::emthrow \
  (GException(m, __FILE__, __LINE__,0, xtype))
#define G_EMTHROW(ex) GExceptionHandler::emthrow(ex)
#endif

#endif // !CPP_SUPPORTS_EXCEPTIONS


inline void G_EXTHROW(const GException &ex,
   const char *msg=0,const char *file=0,int line=0, const char *func=0,
   const GException::source_type source=GException::GINTERNAL)
{
  G_EMTHROW( (msg||file||line||func)?
      GException(msg?msg:ex.get_cause(),
        file?file:ex.get_file(),
        line?line:ex.get_line(),
        func?func:ex.get_function(),
        source)
  :ex);
}

inline void G_EXTHROW(const char msg[],
   const char *file=0,int line=0,const char *func=0,
   const GException::source_type source=GException::GINTERNAL )
{
  G_EMTHROW(GException(msg,file,line,func,source));
}

#define G_THROW(msg) G_THROW_TYPE(msg,GException::GINTERNAL)
#define G_THROW_INTERNAL(msg) G_THROW_TYPE(msg,GException::GINTERNAL)
#define G_THROW_EXTERNAL(msg) G_THROW_TYPE(msg,GException::GEXTERNAL)
#define G_THROW_APPLICATION(msg) G_THROW_TYPE(msg,GException::GAPPLICATION)
#define G_THROW_OTHER(msg) G_THROW_TYPE(msg,GException::GOTHER)

// -------------- THE END

#ifdef HAVE_NAMESPACES
}
# ifndef NOT_USING_DJVU_NAMESPACE
using namespace DJVU;
# endif
#endif
#endif
