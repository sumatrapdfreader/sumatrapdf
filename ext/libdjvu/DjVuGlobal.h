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

#ifndef _DJVUGLOBAL_H
#define _DJVUGLOBAL_H
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#if NEED_GNUG_PRAGMAS
# pragma interface
#endif

#if defined(HAVE_STDINCLUDES)
# include <new>
#elif defined(HAVE_NEW_H)
# include <new.h>
#else
# include <new> // try standard c++ anyway!
#endif

// SumatraPDF: allow to build as a static library (built-in)            
#ifdef WIN32_AND_NOT_STATIC
#ifndef DJVUAPI
# ifdef _WIN32
# ifdef DJVUAPI_EXPORT
#  define DJVUAPI __declspec(dllexport)
# else
#  define DJVUAPI __declspec(dllimport)
# endif
# endif
#endif
#endif

#ifndef DJVUAPI
# define DJVUAPI
#endif


/** @name DjVuGlobal.h 

    This file is included by all include files in the DjVu reference library.
    
	If compilation symbols #NEED_DJVU_MEMORY#, #NEED_DJVU_PROGRESS# 
	or #NEED_DJVU_NAMES# are defined, this file enables 
	features which are useful for certain applications of the
    DjVu Reference Library.  These features are still experimental and
    therefore poorly documented.
    
    @memo
    Global definitions.
    @author
    L\'eon Bottou <leonb@research.att.com> -- empty file.\\
    Bill Riemers <docbill@sourceforge.net> -- real work.  */
//@{


/** @name DjVu Memory 

    This section is enabled when compilation symbol #NEED_DJVU_MEMORY# is
    defined.  Function #_djvu_memory_callback# can be used to redefine the C++
    memory allocation operators.  Some operating systems (e.g. Macintoshes)
    require very peculiar memory allocation in shared objects.  We redefine
    the operators #new# and #delete# as #STATIC_INLINE# because we do not
    want to export these redefined versions to other libraries.  */
//@{
//@}

#ifdef NEED_DJVU_MEMORY

# include "DjVu.h"

// These define the two callbacks needed for C++
typedef void djvu_delete_callback(void *);
typedef void *djvu_new_callback(size_t);

// These functions allow users to set the callbacks.
int djvu_memoryObject_callback ( djvu_delete_callback*, djvu_new_callback*);
int djvu_memoryArray_callback ( djvu_delete_callback*, djvu_new_callback*);

// We need to use this inline function in all modules, but we never want it to
// appear in the symbol table.  It seems different compilers need different
// directives to do this...
# ifndef STATIC_INLINE
#  ifdef __GNUC__
#   define STATIC_INLINE extern inline
#  else /* !__GNUC__ */
#   define STATIC_INLINE static inline
#  endif /* __GNUC__ */
# endif /* STATIC_INLINE */

// This clause is used when overriding operator new
// because the standard has slightly changed.
# if defined( __GNUC__ ) && ( __GNUC__*1000 + __GNUC_MINOR__ >= 2091 )
#  ifndef new_throw_spec
#   define new_throw_spec throw(std::bad_alloc)
#  endif /* new_throw_spec */
#  ifndef delete_throw_spec
#   define delete_throw_spec throw()
#  endif /* delete_throw_spec */
# endif /* __GNUC__ ... */
// Old style
# ifndef new_throw_spec
#  define new_throw_spec
# endif /* new_throw_spec */
# ifndef delete_throw_spec
#  define delete_throw_spec
# endif  /* delete_throw_spec */

# ifdef UNIX
extern djvu_new_callback *_djvu_new_ptr;
extern djvu_new_callback *_djvu_newArray_ptr;
extern djvu_delete_callback *_djvu_delete_ptr;
extern djvu_delete_callback *_djvu_deleteArray_ptr;

#  ifndef NEED_DJVU_MEMORY_IMPLEMENTATION
void *operator new (size_t) new_throw_spec;
void *operator new[] (size_t) new_throw_spec;
void operator delete (void *) delete_throw_spec;
void operator delete[] (void *) delete_throw_spec;

STATIC_INLINE void *
operator new(size_t sz) new_throw_spec
{ return (*_djvu_new_ptr)(sz); }
STATIC_INLINE void
operator delete(void *addr) delete_throw_spec
{ return (*_djvu_delete_ptr)(addr); }
STATIC_INLINE void *
operator new [] (size_t sz) new_throw_spec
{ return (*_djvu_newArray_ptr)(sz); }
STATIC_INLINE void
operator delete [] (void *addr) delete_throw_spec
{ return (*_djvu_deleteArray_ptr)(addr); }
#  endif /* NEED_DJVU_MEMORY_IMPLEMENTATION */

# else /* UNIX */

#  ifndef NEED_DJVU_MEMORY_IMPLEMENTATION
STATIC_INLINE void *
operator new(size_t sz) new_throw_spec
{ return _djvu_new(sz); }
inline_as_macro void
operator delete(void *addr) delete_throw_spec
{ return _djvu_delete(addr); }
inline_as_macro void *
operator new [] (size_t sz) new_throw_spec
{ return _djvu_new(sz); }
inline_as_macro void
operator delete [] (void *addr) delete_throw_spec
{ _djvu_deleteArray(addr); }
#  endif /* !NEED_DJVU_MEMORY_IMPLEMENTATION */

# endif /* UNIX */

#else

# define _djvu_free(ptr) free((ptr))
# define _djvu_malloc(siz) malloc((siz))
# define _djvu_realloc(ptr,siz) realloc((ptr),(siz))
# define _djvu_calloc(siz,items) calloc((siz),(items))

#endif /* NEED_DJVU_MEMORY */

/** @name DjVu Progress  

    This section is enabled when compilation symbol #NEED_DJVU_PROGRESS# is
    defined.  This macro setups callback function that may be used to
    implement a progress indicator for the encoding routines.  The decoding
    routines do not need such a facility because it is sufficient to monitor
    the calls to function \Ref{ByteStream::read} in class \Ref{ByteStream}.
    
    {\bf Code tracing macros} ---
    Monitoring the progress of such complex algorithms requires significant
    code support.  This is achieved by inserting {\em code tracing macros}
    in strategic regions of the code.  
    \begin{description}
    \item[DJVU_PROGRESS_TASK(name,task,nsteps)]  indicates that the current
         scope performs a task roughly divided in #nsteps# equal steps, with
	       the specified #task# string used in the callback.
    \item[DJVU_PROGRESS_RUN(name,tostep)] indicates that we are starting
         an operation which will take us to step #tostep#.  The operation
         will be considered finished when #DJVU_PROGRESS_RUN# will be called
         again with an argument greater than #tostep#.  The execution of
         this operation of course can be described by one subtask and so on.
    \end{description}
 
    {\bf Progress callback} --- Before defining the outermost task, you can
    store a callback function pointer into the static member variable
    #DjVuProgressTask::callback#.  This callback function is called
    periodically with two unsigned long arguments.  The first argument is the
    elapsed time. The second argument is the estimated total execution time.
    Both times are given in milliseconds.

    {\bf Important Note} --- This monitoring mechanism should not be used by
    multithreaded programs.  */
//@{

#ifndef HAS_DJVU_PROGRESS_CALLBACKS
# define HAS_DJVU_PROGRESS_CALLBACKS

# ifdef NEED_DJVU_PROGRESS
#  include "DjVu.h"

extern djvu_progress_callback *_djvu_progress_ptr;

#  define DJVU_PROGRESS_TASK(name,task,nsteps)  DjVuProgressTask task_##name(task,nsteps)
#  define DJVU_PROGRESS_RUN(name,tostep)   { task_##name.run(tostep); }

class DjVuProgressTask
{
public:
  class Data;
  ~DjVuProgressTask();
  DjVuProgressTask(const char *task,int nsteps);
  void run(int tostep);
  const char *task;
  static djvu_progress_callback *set_callback(djvu_progress_callback *ptr=0);
private:
  DjVuProgressTask *parent;
  int nsteps;
  int runtostep;
  unsigned long startdate;
  // Statics
  void *gdata;
  Data *data;
  // Helpers
  void signal(unsigned long curdate, unsigned long estdate);
};

# else  // ! NEED_DJVU_PROGRESS

#  define DJVU_PROGRESS_TASK(name,task,nsteps)
#  define DJVU_PROGRESS_RUN(name,step)

# endif // ! NEED_DJVU_PROGRESS
#endif // HAS_DJVU_PROGRESS_CALLBACKS
//@}


/** @name General functions.

    This section contains functions that replace some of the standard
    system calls without any other header file dependancies.
 */

#ifdef __cplusplus
# define DJVUEXTERNCAPI(x) extern "C" DJVUAPI x;
#else
# define DJVUEXTERNCAPI(x) extern DJVUAPI x
#endif

/** This replaces fprintf(stderr,...), but with UTF8 encoded strings. */
DJVUEXTERNCAPI(void DjVuPrintErrorUTF8(const char *fmt, ...));

/** This replaces fprintf(stderr,...), but with UTF8 encoded strings. */
DJVUEXTERNCAPI(void DjVuPrintErrorNative(const char *fmt, ...));

/** This replaces printf(...), but requires UTF8 encoded strings. */
DJVUEXTERNCAPI(void DjVuPrintMessageUTF8(const char *fmt, ...));

/** This replaces printf(...), but requires UTF8 encoded strings. */
DJVUEXTERNCAPI(void DjVuPrintMessageNative(const char *fmt, ...));

/** The format (fmt) and arguments define a MessageList to be looked
    up in the external messages and printed to stderr. */
DJVUEXTERNCAPI(void DjVuFormatErrorUTF8(const char *fmt, ...));

/** The format (fmt) and arguments define a MessageList to be looked
    up in the external messages and printed to stderr. */
DJVUEXTERNCAPI(void DjVuFormatErrorNative(const char *fmt, ...));

/** Prints the translation of message to stderr. */
DJVUEXTERNCAPI(void DjVuWriteError( const char *message ));

/** Prints the translation of message to stdout. */
DJVUEXTERNCAPI(void DjVuWriteMessage( const char *message ));

/** A C function to perform a message lookup. Arguments are a buffer to
  received the translated message, a buffer size (bytes), and a
  message_list. The translated result is returned in msg_buffer encoded
  in UTF-8. In case of error, msg_buffer is empty
  (i.e., msg_buffer[0] == '\0').
*/
DJVUEXTERNCAPI(void DjVuMessageLookUpUTF8(
  char *msg_buffer, const unsigned int buffer_size, 
  const char *message ));
DJVUEXTERNCAPI(void DjVuMessageLookUpNative(
  char *msg_buffer, const unsigned int buffer_size, 
  const char *message ));

/** This function sets the program name used when 
    searching for language files.
*/
DJVUEXTERNCAPI(const char *djvu_programname(const char *programname));


/** @name DjVu Names  

    This section is enabled when compilation symbol #NEED_DJVU_NAMES# is
    defined.  This section redefines class names in order to unclutter the
    name space of shared objects.  This is useful on systems which
    automatically export all global symbols when building a shared object.
    @args */
//@{
//@}

#ifdef NEED_DJVU_NAMES
/* The contents of this section may be generated by this shell command :
 * % egrep -h '^(class|struct) +[A-Z_][A-Za-z0-9_]*' *.h *.cpp |\
 *   sed -e 's:[a-z]*  *\([A-Za-z_][A-Za-z0-9_]*\).*:#define \1 DJVU_\1:g' |\
 *   sort
 */
#endif // NEED_DJVU_NAMES

//@}

#if defined(macintosh)
# define EMPTY_LOOP continue
#else
# define EMPTY_LOOP /* nop */
#endif

//  The ERR_MSG(x) macro is intended to permit automated checking of the
//  externalized error message names against the source code. It has no
//  effect on the executed program. It should be used to surround each
//  message name that will need to be looked up in the external message
//  files. In particular, it should use on all strings passed to G_THROW.
#ifndef HAS_CTRL_C_IN_ERR_MSG
# define HAS_CTRL_C_IN_ERR_MSG 1
#endif
#ifndef ERR_MSG
# if HAS_CTRL_C_IN_ERR_MSG
// This hack allows for the coexistence of internationalized
// and non-internationalized code.  All internationalized error
// message names are prefixed with a ctrl-c.  Only these will
// be looked for in the message files.  Messages that do no 
// start with a ctrl-c will remain untranslated.
#  define ERR_MSG(x) "\003" x
# else
#  define ERR_MSG(x) x
# endif
#endif

#endif /* _DJVUGLOBAL_H_ */


