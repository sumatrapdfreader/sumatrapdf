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

#ifndef _BYTESTREAM_H
#define _BYTESTREAM_H
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#if NEED_GNUG_PRAGMAS
# pragma interface
#endif

/** @name ByteStream.h
    
    Files #"ByteStream.h"# and #"ByteStream.cpp"# define input/output classes
    similar in spirit to the well known C++ #iostream# classes.  Class
    \Ref{ByteStream} is an abstract base class for all byte streams.  It
    defines a virtual interface and also provides useful functions.  These
    files provide two subclasses. Class \Ref{ByteStream::Stdio} provides a
    simple interface to the Ansi C buffered input/output functions. Class
    \Ref{ByteStream::Memory} provides stream-like access to a dynamical array
    maintained in memory. Class \Ref{ByteStream::Static} provides read-only
    stream-like access to a user allocated data buffer.

    {\bf Notes} --- These classes were partly written because we did not want to
    depend on the standard C++ library.  The main reason however is related to
    the browser interface. We want to have a tight control over the
    implementation of subclasses because we want to use a byte stream to
    represent data passed by a web browser to a plugin.  This operation
    involves multi-threading issues that many implementations of the standard
    C++ library would squarely ignore.

    @memo 
    Input/output classes
    @author
    L\'eon Bottou <leonb@research.att.com> -- initial implementation\\
    Andrei Erofeev <eaf@geocities.com> -- 

// From: Leon Bottou, 1/31/2002
// This file has very little to do with my initial implementation.
// It has been practically rewritten by Lizardtech for i18n changes.
// Our original implementation consisted of multiple classes.
// <http://prdownloads.sourceforge.net/djvu/DjVu2_2b-src.tgz>.

*/
//@{


#include "Arrays.h"
#include <stdio.h>

#ifdef HAVE_NAMESPACES
namespace DJVU {
# ifdef NOT_DEFINED // Just to fool emacs c++ mode
}
#endif
#endif

class GURL;
class GUTF8String;
class GNativeString;

/** Abstract class for a stream of bytes.  Class #ByteStream# represent an
    object from which (resp. to which) bytes can be read (resp. written) as
    with a regular file.  Virtual functions #read# and #write# must implement
    these two basic operations.  In addition, function #tell# returns an
    offset identifying the current position, and function #seek# may be used
    to change the current position.

    {\bf Note}. Both the copy constructor and the copy operator are declared
    as private members. It is therefore not possible to make multiple copies
    of instances of this class, as implied by the class semantic.  
*/
class DJVUAPI ByteStream : public GPEnabled
{
public:
  class Stdio;
  class Static;
  class Memory;
  class Wrapper;
  enum codepage_type {RAW,AUTO,NATIVE,UTF8} cp;

  /** @name Virtual Functions.
      These functions are usually implemented by each subclass of #ByteStream#.
  */
  //@{
public:
  /** Virtual destructor. */
  virtual ~ByteStream();
  /** Reads data from a ByteStream.  This function {\em must} be implemented
      by each subclass of #ByteStream#.  At most #size# bytes are read from
      the ByteStream and stored in the memory area pointed to by #buffer#.
      Function #read# returns immediately if #size# is zero. The actual number
      of bytes read is returned.  Function #read# returns a number of bytes
      smaller than #size# if the end-of-file mark is reached before filling
      the buffer. Subsequent invocations will always return value #0#.
      Function #read# may also return a value greater than zero but smaller
      than #size# for internal reasons. Programs must be ready to handle these
      cases or use function \Ref{readall}. Exception \Ref{GException} is
      thrown with a plain text error message whenever an error occurs. */
  virtual size_t read(void *buffer, size_t size);
  /** Writes data to a ByteStream.  This function {\em must} be implemented by
      each subclass of #ByteStream#.  At most #size# bytes from buffer
      #buffer# are written to the ByteStream.  Function #write# returns
      immediately if #size# is zero.  The actual number of bytes written is
      returned. Function #write# may also return a value greater than zero but
      smaller than #size# for internal reasons. Programs must be ready to
      handle these cases or use function \Ref{writall}. Exception
      \Ref{GException} is thrown with a plain text error message whenever an
      error occurs. */
  virtual size_t write(const void *buffer, size_t size);
  /** Returns the offset of the current position in the ByteStream.  This
      function {\em must} be implemented by each subclass of #ByteStream#. */
  virtual long tell(void) const  = 0;
  /** Sets the current position for reading or writing the ByteStream.  Class
      #ByteStream# provides a default implementation able to seek forward by
      calling function #read# until reaching the desired position.  Subclasses
      implementing better seek capabilities must override this default
      implementation.  The new current position is computed by applying
      displacement #offset# to the position represented by argument
      #whence#. The following values are recognized for argument #whence#:
      \begin{description}
      \item[#SEEK_SET#] Argument #offset# indicates the position relative to
      the beginning of the ByteStream.
      \item[#SEEK_CUR#] Argument #offset# is a signed displacement relative to
      the current position.
      \item[#SEEK_END#] Argument #offset# is a displacement relative to the end
      of the file. It is then advisable to provide a negative value for #offset#.
      \end{description}
      Results are undefined whenever the new position is greater than the
      total size of the ByteStream.

      {\bf Error reporting}:
      If #seek()# succeeds, #0# is returned. Otherwise it either returns
      #-1# (if #nothrow# is set to #FALSE#) or throws the \Ref{GException}
      exception. */
  virtual int seek(long offset, int whence = SEEK_SET, bool nothrow=false);
  /** Flushes all buffers in the ByteStream.  Calling this function
      guarantees that pending data have been actually written (i.e. passed to
      the operating system). Class #ByteStream# provides a default
      implementation which does nothing. */
  virtual void flush(void);
  //@}
  /** @name Utility Functions.  
      Class #ByteStream# implements these functions using the virtual
      interface functions only.  All subclasses of #ByteStream# inherit these
      functions. */
  //@{
public:
  /** Reads data and blocks until everything has been read.  This function is
      essentially similar to function #read#.  Unlike function #read# however,
      function #readall# will never return a value smaller than #size# unless
      an end-of-file mark is reached.  This is implemented by repeatedly
      calling function #read# until everything is read or until we reach an
      end-of-file mark.  Note that #read# and #readall# are equivalent when
      #size# is one. */
  size_t readall(void *buffer, size_t size);
  /** Writes data and blocks until everything has been written.  This function
      is essentially similar to function #write#.  Unlike function #write#
      however, function #writall# will only return after all #size# bytes have
      been written.  This is implemented by repeatedly calling function
      #write# until everything is written.  Note that #write# and #writall#
      are equivalent when #size# is one. */
  size_t writall(const void *buffer, size_t size);
  /** Copy data from another ByteStream.  A maximum of #size# bytes are read
      from the ByteStream #bsfrom# and are written to the ByteStream #*this#
      at the current position.  Less than #size# bytes may be written if an
      end-of-file mark is reached on #bsfrom#.  This function returns the
      total number of bytes copied.  Setting argument #size# to zero (the
      default value) has a special meaning: the copying process will continue
      until reaching the end-of-file mark on ByteStream #bsfrom#, regardless
      of the number of bytes transferred.  */
  size_t copy(ByteStream &bsfrom, size_t size=0);
  /// Allows printf() type operations to a bytestream.
  size_t format(const char *fmt, ... );
  /// Allows scanf() type operations on a bytestream.
  int scanf(const char *fmt, ... );
  /** Writes the string as is, to the specified stream. */
  size_t writestring(const GUTF8String &s);
  /** Writes the string as is, to the specified stream. */
  size_t writestring(const GNativeString &s);
  /** Formats the message string, looks up the external representation
      and writes it to the specified stream. */
  void formatmessage( const char *fmt, ... );
  /** Looks up the message and writes it to the specified stream. */
  void writemessage( const char *message );
  /** Writes a one-byte integer to a ByteStream. */
  void write8 (unsigned int card8);
  /** Writes a two-bytes integer to a ByteStream.
      The integer most significant byte is written first,
      regardless of the processor endianness. */
  void write16(unsigned int card16);
  /** Writes a three-bytes integer to a ByteStream.
      The integer most significant byte is written first,
      regardless of the processor endianness. */
  void write24(unsigned int card24);
  /** Writes a four-bytes integer to a ByteStream. 
      The integer most significant bytes are written first,
      regardless of the processor endianness. */
  void write32(unsigned int card32);
  /** Reads a one-byte integer from a ByteStream. */
  unsigned int read8 ();
  /** Reads a two-bytes integer from a ByteStream.
      The integer most significant byte is read first,
      regardless of the processor endianness. */
  unsigned int read16();
  /** Reads a three-bytes integer from a ByteStream.
      The integer most significant byte is read first,
      regardless of the processor endianness. */
  unsigned int read24();
  /** Reads a four-bytes integer from a ByteStream.
      The integer most significant bytes are read first,
      regardless of the processor endianness. */
  unsigned int read32();
  /** Returns the total number of bytes contained in the buffer, file, etc.
      Valid offsets for function #seek# range from 0 to the value returned
      by this function. */
  virtual int size(void) const;
  /// Use at your own risk, only guarenteed to work for ByteStream::Memorys.
  TArray<char> get_data(void);
  /** Reads data from a random position. This function reads at most #sz#
      bytes at position #pos# into #buffer# and returns the actual number of
      bytes read.  The current position is unchanged. */
  virtual size_t readat(void *buffer, size_t sz, int pos);
  //@}
protected:
  ByteStream(void) : cp(AUTO) {};
private:
  // Cancel C++ default stuff
  ByteStream(const ByteStream &);
  ByteStream & operator=(const ByteStream &);
public:
  /** Constructs an empty Memory ByteStream.  The buffer itself is organized
      as an array of 4096 byte blocks.  The buffer is initially empty. You
      must first use function #write# to store data into the buffer, use
      function #seek# to rewind the current position, and function #read# to
      read the data back. */
  static GP<ByteStream> create(void);
  /** Constructs a Memory ByteStream by copying initial data.  The
      Memory buffer is initialized with #size# bytes copied from the
      memory area pointed to by #buffer#. */
  static GP<ByteStream> create(void const * const buffer, const size_t size);
  /** Constructs a ByteStream for accessing the file named #url#.
      Arguments #url# and #mode# are similar to the arguments of the well
      known stdio function #fopen#. In addition a url of #-# will be
      interpreted as the standard output or the standard input according to
      #mode#.  This constructor will open a stdio file and construct a
      ByteStream object accessing this file. Destroying the ByteStream object
      will flush and close the associated stdio file.  Exception
      \Ref{GException} is thrown with a plain text error message if the stdio
      file cannot be opened. */
  static GP<ByteStream> create(
    const GURL &url, char const * const mode);
  /** Same as the above, but uses stdin or stdout */
  static GP<ByteStream> create( char const * const mode);

  /** Constructs a ByteStream for accessing the stdio file #f#.
      Argument #mode# indicates the type of the stdio file, as in the
      well known stdio function #fopen#.  Destroying the ByteStream
      object will not close the stdio file #f# unless closeme is true. */
  static GP<ByteStream> create(
    const int fd, char const * const mode, const bool closeme);

  /** Constructs a ByteStream for accessing the stdio file #f#.
      Argument #mode# indicates the type of the stdio file, as in the
      well known stdio function #fopen#.  Destroying the ByteStream
      object will not close the stdio file #f# unless closeme is true. */
  static GP<ByteStream> create(
    FILE * const f, char const * const mode, const bool closeme);
  /** Creates a ByteStream object for allocating the memory area of
      length #sz# starting at address #buffer#.  This call impliments 
      a read-only ByteStream interface for a memory area specified by
      the user at construction time. Calls to function #read# directly
      access this memory area.  The user must therefore make sure that its
      content remain valid long enough.  */
  static GP<ByteStream> create_static(void const *buffer, size_t size);
  
  /** Easy access to preallocated stdin/stdout/stderr bytestreams */
  static GP<ByteStream> get_stdin(char const * mode=0);
  static GP<ByteStream> get_stdout(char const * mode=0);  
  static GP<ByteStream> get_stderr(char const * mode=0);

  /** This is the conventional name for EOF exceptions */
  static const char *EndOfFile;
  /** Returns the contents of the file as a GNativeString */
  GNativeString getAsNative(void);
  /** Returns the contents of the file as a GUTF8String */
  GUTF8String getAsUTF8(void);
};

inline size_t
ByteStream::readat(void *buffer, size_t sz, int pos)
{
  size_t retval;
  long tpos=tell();
  seek(pos, SEEK_SET, true);
  retval=readall(buffer,sz);
  seek(tpos, SEEK_SET, true);
  return retval;
}

inline int
ByteStream::size(void) const
{
  ByteStream *bs=const_cast<ByteStream *>(this);
  int bsize=(-1);
  long pos=tell();
  if(bs->seek(0,SEEK_END,true))
  {
    bsize=(int)tell();
    (void)(bs->seek(pos,SEEK_SET,false));
  }
  return bsize;
}

/** ByteStream::Wrapper implements wrapping bytestream.  This is useful
    for derived classes that take a GP<ByteStream> as a creation argument,
    and the backwards compatible bytestreams.  */
class DJVUAPI ByteStream::Wrapper : public ByteStream
{
protected:
  GP<ByteStream> gbs;
  ByteStream *bs;
  Wrapper(void) : bs(0) {}
  Wrapper(const GP<ByteStream> &xbs) : gbs(xbs), bs(xbs) {}
public:
  ~Wrapper();
  ByteStream * operator & () const {return bs;}
  ByteStream * operator & () {return bs;}
  virtual size_t read(void *buffer, size_t size)
    { return bs->read(buffer,size); }
  virtual size_t write(const void *buffer, size_t size)
    { return bs->write(buffer,size); }
  virtual long tell(void) const
    { return bs->tell(); }
  virtual int seek(long offset, int whence = SEEK_SET, bool nothrow=false)
    { return bs->seek(offset,whence,nothrow); }
  virtual void flush(void)
    { bs->flush(); }
};


//@}

// ------------ THE END

#ifdef HAVE_NAMESPACES
}
# ifndef NOT_USING_DJVU_NAMESPACE
using namespace DJVU;
# endif
#endif
#endif

