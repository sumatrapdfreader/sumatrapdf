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

// From: Leon Bottou, 1/31/2002
// This file has very little to do with my initial implementation.
// It has been practically rewritten by Lizardtech for i18n changes.
// Our original implementation consisted of multiple classes.
// <http://prdownloads.sourceforge.net/djvu/DjVu2_2b-src.tgz>.

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#if NEED_GNUG_PRAGMAS
# pragma implementation
#endif

// - Author: Leon Bottou, 04/1997

#include "DjVuGlobal.h"
#include "ByteStream.h"
#include "GOS.h"
#include "GURL.h"
#include "DjVuMessage.h"
#include <stddef.h>
#include <fcntl.h>
#if defined(_WIN32) || defined(__CYGWIN32__)
# include <io.h>
#endif
#if defined(__APPLE__)
# include <CoreFoundation/CFString.h>
#endif

#ifdef UNIX
# ifndef HAS_MEMMAP
#  define HAS_MEMMAP 1
# endif
#endif

#ifdef UNIX
# include <sys/types.h>
# include <sys/stat.h>
# include <unistd.h>
# include <errno.h>
# ifdef HAS_MEMMAP
#  include <sys/mman.h>
# endif
#endif

#ifdef macintosh
# ifndef UNIX
#  include <unistd.h>
_MSL_IMP_EXP_C int _dup(int);
_MSL_IMP_EXP_C int _dup2(int,int);
_MSL_IMP_EXP_C int _close(int);
__inline int dup(int _a ) { return _dup(_a);}
__inline int dup2(int _a, int _b ) { return _dup2(_a, _b);}
# endif
#endif

#if defined(_WIN32) && !defined(__CYGWIN32__)
#  define close _close
#  define fdopen _fdopen
#  define dup _dup
#endif

#ifdef HAVE_NAMESPACES
namespace DJVU {
# ifdef NOT_DEFINED // Just to fool emacs c++ mode
}
#endif
#endif

const char *ByteStream::EndOfFile=ERR_MSG("EOF");

/** ByteStream interface for stdio files. 
    The virtual member functions #read#, #write#, #tell# and #seek# are mapped
    to the well known stdio functions #fread#, #fwrite#, #ftell# and #fseek#.
    @see Unix man page fopen(3), fread(3), fwrite(3), ftell(3), fseek(3) */

class ByteStream::Stdio : public ByteStream {
public:
  Stdio(void);

  /** Constructs a ByteStream for accessing the file named #url#.
      Arguments #url# and #mode# are similar to the arguments of the well
      known stdio function #fopen#. In addition a url of #-# will be
      interpreted as the standard output or the standard input according to
      #mode#.  This constructor will open a stdio file and construct a
      ByteStream object accessing this file. Destroying the ByteStream object
      will flush and close the associated stdio file.  Returns an error code
      if the stdio file cannot be opened. */
  GUTF8String init(const GURL &url, const char * const mode);

  /** Constructs a ByteStream for accessing the stdio file #f#.
      Argument #mode# indicates the type of the stdio file, as in the
      well known stdio function #fopen#.  Destroying the ByteStream
      object will not close the stdio file #f# unless closeme is true. */
  GUTF8String init(FILE * const f, const char * const mode="rb", const bool closeme=false);

  /** Initializes from stdio */
  GUTF8String init(const char mode[]);

  // Virtual functions
  ~Stdio();
  virtual size_t read(void *buffer, size_t size);
  virtual size_t write(const void *buffer, size_t size);
  virtual void flush(void);
  virtual int seek(long offset, int whence = SEEK_SET, bool nothrow=false);
  virtual long tell(void) const;
private:
  // Cancel C++ default stuff
  Stdio(const Stdio &);
  Stdio & operator=(const Stdio &);
private:
  // Implementation
  bool can_read;
  bool can_write;
  bool must_close;
protected:
  FILE *fp;
  long pos;
};

inline GUTF8String
ByteStream::Stdio::init(FILE * const f,const char mode[],const bool closeme)
{
  fp=f;
  must_close=closeme;
  return init(mode);
}


/** ByteStream interface managing a memory buffer.  
    Class #ByteStream::Memory# manages a dynamically resizable buffer from
    which data can be read or written.  The buffer itself is organized as an
    array of blocks of 4096 bytes.  */

class ByteStream::Memory : public ByteStream
{
public:
  /** Constructs an empty ByteStream::Memory.
      The buffer is initially empty. You must first use function #write#
      to store data into the buffer, use function #seek# to rewind the
      current position, and function #read# to read the data back. */
  Memory();
  /** Constructs a Memory by copying initial data.  The
      Memory buffer is initialized with #size# bytes copied from the
      memory area pointed to by #buffer#. */
  GUTF8String init(const void * const buffer, const size_t size);
  // Virtual functions
  ~Memory();
  virtual size_t read(void *buffer, size_t size);
  virtual size_t write(const void *buffer, size_t size);
  virtual int    seek(long offset, int whence=SEEK_SET, bool nothrow=false);
  virtual long   tell(void) const;
  /** Erases everything in the Memory.
      The current location is reset to zero. */
  void empty();
  /** Returns the total number of bytes contained in the buffer.  Valid
      offsets for function #seek# range from 0 to the value returned by this
      function. */
  virtual long size(void) const;
  /** Returns a reference to the byte at offset #n#. This reference can be
      used to read (as in #mbs[n]#) or modify (as in #mbs[n]=c#) the contents
      of the buffer. */
  char &operator[] (int n);
  char &operator[] (long n);
  /** Copies all internal data into \Ref{TArray} and returns it */
private:
  // Cancel C++ default stuff
  Memory(const Memory &);
  Memory & operator=(const Memory &);
  // Current position
  long where;
protected:
  /** Reads data from a random position. This function reads at most #sz#
      bytes at position #pos# into #buffer# and returns the actual number of
      bytes read.  The current position is unchanged. */
  virtual size_t readat(void *buffer, size_t sz, long pos);
  /** Number of bytes in internal buffer. */
  long bsize;
  /** Number of 4096 bytes blocks. */
  int nblocks;
  /** Pointers (possibly null) to 4096 bytes blocks. */
  char **blocks;
  /** Pointers (possibly null) to 4096 bytes blocks. */
  GPBuffer<char *> gblocks;
};



inline long
ByteStream::Memory::size(void) const
{
  return bsize;
}

inline char &
ByteStream::Memory::operator[] (int n)
{
  return blocks[n>>12][n&0xfff];
}

inline char &
ByteStream::Memory::operator[] (long n)
{
  return blocks[n>>12][n&0xfff];
}



/** Read-only ByteStream interface to a memory area.  
    Class #ByteStream::Static# implements a read-only ByteStream interface for a
    memory area specified by the user at construction time. Calls to function
    #read# directly access this memory area.  The user must therefore make
    sure that its content remain valid long enough.  */

class ByteStream::Static : public ByteStream
{
public:

  /** Creates a Static object for allocating the memory area of
      length #sz# starting at address #buffer#. */
  Static(const void * const buffer, const size_t sz);
  ~Static();
  // Virtual functions
  virtual size_t read(void *buffer, size_t sz);
  virtual int    seek(long offset, int whence = SEEK_SET, bool nothrow=false);
  virtual long tell(void) const;
  /** Returns the total number of bytes contained in the buffer, file, etc.
      Valid offsets for function #seek# range from 0 to the value returned
      by this function. */
  virtual long size(void) const;
protected:
  const char *data;
  long bsize;
private:
  long where;
};

ByteStream::Static::~Static() {}

inline long
ByteStream::Static::size(void) const
{
  return bsize;
}

#if HAS_MEMMAP
/** Read-only ByteStream interface to a memmap area.
    Class #MemoryMapByteStream# implements a read-only ByteStream interface
    for a memory map to a file. */

class MemoryMapByteStream : public ByteStream::Static
{
public:
  MemoryMapByteStream(void);
  virtual ~MemoryMapByteStream();  
private:
  GUTF8String init(const int fd, const bool closeme);
  GUTF8String init(FILE *const f,const bool closeme);
  friend class ByteStream;
};
#endif

//// CLASS BYTESTREAM


ByteStream::~ByteStream()
{
}

int 
ByteStream::scanf(const char *fmt, ...)
{
  G_THROW( ERR_MSG("ByteStream.not_implemented") ); // This is a place holder function.
  return 0;
}

size_t 
ByteStream::read(void *buffer, size_t sz)
{
  G_THROW( ERR_MSG("ByteStream.cant_read") );      //  Cannot read from a ByteStream created for writing
  return 0;
}

size_t 
ByteStream::write(const void *buffer, size_t sz)
{
  G_THROW( ERR_MSG("ByteStream.cant_write") );      //  Cannot write from a ByteStream created for reading
  return 0;
}

void
ByteStream::flush()
{
}

int
ByteStream::seek(long offset, int whence, bool nothrow)
{
  long nwhere = 0;
  long ncurrent = tell();
  switch (whence)
    {
    case SEEK_SET:
      nwhere=0; break;
    case SEEK_CUR:
      nwhere=ncurrent; break;
    case SEEK_END: 
    {
      if(offset)
      {
        if (nothrow)
          return -1;
        G_THROW( ERR_MSG("ByteStream.backward") );
      }
      char buffer[1024];
      int bytes;
      while((bytes=read(buffer, sizeof(buffer))))
        EMPTY_LOOP;
      return 0;
    }
    default:
      G_THROW( ERR_MSG("ByteStream.bad_arg") );       //  Illegal argument in seek
    }
  nwhere += offset;
  if (nwhere < ncurrent) 
  {
    //  Seeking backwards is not supported by this ByteStream
    if (nothrow)
      return -1;
    G_THROW( ERR_MSG("ByteStream.backward") );
  }
  while (nwhere > ncurrent)
  {
    char buffer[1024];
      long xbytes = nwhere - ncurrent;
      if (xbytes > (long)sizeof(buffer))
        xbytes = sizeof(buffer);
      long bytes = (long)read(buffer, xbytes);
    ncurrent += bytes;
    if (!bytes)
      G_THROW( ByteStream::EndOfFile );
    //  Seeking works funny on this ByteStream (ftell() acts strange)
      if (ncurrent != tell())
      G_THROW( ERR_MSG("ByteStream.seek") );
  }
  return 0;
}

size_t 
ByteStream::readall(void *buffer, size_t size)
{
  size_t total = 0;
    while (size > 0)
    {
      int nitems = read(buffer, size);
      // Replaced perror() below with G_THROW(). It still makes little sense
      // as there is no guarantee, that errno is right. Still, throwing
      // exception instead of continuing to loop is better.
      // - eaf
      if(nitems < 0) 
        G_THROW(strerror(errno));               //  (No error in the DjVuMessageFile)
      if (nitems == 0)
        break;
      total += nitems;
      size -= nitems; 
      buffer = (void*)((char*)buffer + nitems);
    }
  return total;
}

size_t
ByteStream::format(const char *fmt, ... )
{
  va_list args;
  va_start(args, fmt); 
  const GUTF8String message(fmt,args);
  return writestring(message);
}

size_t
ByteStream::writestring(const GNativeString &s)
{
  int retval;
  if(cp != UTF8)
  {
    retval=writall((const char *)s,s.length());
    if(cp == AUTO)
      cp=NATIVE; // Avoid mixing string types.
  }else
  { 
    const GUTF8String msg(s.getNative2UTF8());
    retval=writall((const char *)msg,msg.length());
  }
  return retval;
}

size_t
ByteStream::writestring(const GUTF8String &s)
{
  int retval;
  if(cp != NATIVE)
  {
    retval=writall((const char *)s,s.length());
    if(cp == AUTO)
      cp=UTF8; // Avoid mixing string types.
  }else
  { 
    const GNativeString msg(s.getUTF82Native());
    retval=writall((const char *)msg,msg.length());
  }
  return retval;
}

size_t 
ByteStream::writall(const void *buffer, size_t size)
{
  size_t total = 0;
  while (size > 0)
    {
      size_t nitems = write(buffer, size);
      if (nitems == 0)
        G_THROW( ERR_MSG("ByteStream.write_error") );      //  Unknown error in write
      total += nitems;
      size -= nitems; 
      buffer = (void*)((char*)buffer + nitems);
    }
  return total;
}

size_t 
ByteStream::copy(ByteStream &bsfrom, size_t size)
{
  size_t total = 0;
  const size_t max_buffer_size=200*1024;
  const size_t buffer_size=(size>0 && size<max_buffer_size)?size:max_buffer_size;
  char *buffer;
  GPBuffer<char> gbuf(buffer,buffer_size);
  for(;;)
    {
      size_t bytes = buffer_size;
      if (size>0 && bytes+total>size)
        bytes = size - total;
      if (bytes == 0)
        break;
      bytes = bsfrom.read((void*)buffer, bytes);
      if (bytes == 0)
        break;
      writall((void*)buffer, bytes);
      total += bytes;
    }
  return total;
}


void 
ByteStream::write8 (unsigned int card)
{
  unsigned char c[1];
  c[0] = (card) & 0xff;
  if (write((void*)c, sizeof(c)) != sizeof(c))
    G_THROW(strerror(errno));   //  (No error in the DjVuMessageFile)
}

void 
ByteStream::write16(unsigned int card)
{
  unsigned char c[2];
  c[0] = (card>>8) & 0xff;
  c[1] = (card) & 0xff;
  if (writall((void*)c, sizeof(c)) != sizeof(c))
    G_THROW(strerror(errno));   //  (No error in the DjVuMessageFile)
}

void 
ByteStream::write24(unsigned int card)
{
  unsigned char c[3];
  c[0] = (card>>16) & 0xff;
  c[1] = (card>>8) & 0xff;
  c[2] = (card) & 0xff;
  if (writall((void*)c, sizeof(c)) != sizeof(c))
    G_THROW(strerror(errno));   //  (No error in the DjVuMessageFile)
}

void 
ByteStream::write32(unsigned int card)
{
  unsigned char c[4];
  c[0] = (card>>24) & 0xff;
  c[1] = (card>>16) & 0xff;
  c[2] = (card>>8) & 0xff;
  c[3] = (card) & 0xff;
  if (writall((void*)c, sizeof(c)) != sizeof(c))
    G_THROW(strerror(errno));   //  (No error in the DjVuMessageFile)
}

unsigned int 
ByteStream::read8 ()
{
  unsigned char c[1];
  if (readall((void*)c, sizeof(c)) != sizeof(c))
    G_THROW( ByteStream::EndOfFile );
  return c[0];
}

unsigned int 
ByteStream::read16()
{
  unsigned char c[2];
  if (readall((void*)c, sizeof(c)) != sizeof(c))
    G_THROW( ByteStream::EndOfFile );
  return (c[0]<<8)+c[1];
}

unsigned int 
ByteStream::read24()
{
  unsigned char c[3];
  if (readall((void*)c, sizeof(c)) != sizeof(c))
    G_THROW( ByteStream::EndOfFile );
  return (((c[0]<<8)+c[1])<<8)+c[2];
}

unsigned int 
ByteStream::read32()
{
  unsigned char c[4];
  if (readall((void*)c, sizeof(c)) != sizeof(c))
    G_THROW( ByteStream::EndOfFile );
  return (((((c[0]<<8)+c[1])<<8)+c[2])<<8)+c[3];
}



//// CLASS ByteStream::Stdio

ByteStream::Stdio::Stdio(void)
: can_read(false),can_write(false),must_close(true),fp(0),pos(0)
{}

ByteStream::Stdio::~Stdio()
{
  if (fp && must_close)
    fclose(fp);
}

GUTF8String
ByteStream::Stdio::init(const char mode[])
{
  char const *mesg=0;
  bool binary=false;
  if(!fp)
    must_close=false;
  for (const char *s=mode; s && *s; s++)
  {
    switch(*s) 
    {
      case 'r':
        can_read=true;
        if(!fp) fp=stdin;
        break;
      case 'w': 
      case 'a':
        can_write=true;
        if(!fp) fp=stdout;
        break;
      case '+':
        can_read=can_write=true;
        break;
      case 'b':
        binary=true;
        break;
      default:
        mesg= ERR_MSG("ByteStream.bad_mode"); //  Illegal mode in Stdio
    }
  }
  if(binary && fp) {
#if defined(__CYGWIN32__)
    setmode(fileno(fp), O_BINARY);
#elif defined(_WIN32)
    _setmode(_fileno(fp), _O_BINARY);
#endif
  }
  GUTF8String retval;
  if(!mesg)
  {
    tell();
  }else
  {
    retval=mesg;
  }
  if(mesg &&(fp && must_close))
  {
    fclose(fp);
    fp=0;
    must_close=false;
  }
  return retval;
}

#ifdef _WIN32
static wchar_t *
utf8_to_wide(const char *cstr)
{
  int wlen = strlen(cstr) + 1;
  wchar_t *wstr = new wchar_t[wlen];
  if (GUTF8String(cstr).ncopy(wstr, wlen) > 0)
    return wstr;
  delete [] wstr;
  return 0;
}
#endif

#ifdef __APPLE__
static char *
utf8_to_utf8mac(const char *cstr)
{
  int len = strlen(cstr);
  CFStringRef utf8 = CFStringCreateWithCString(NULL, cstr, kCFStringEncodingUTF8);
  int buflen = CFStringGetMaximumSizeOfFileSystemRepresentation(utf8);
  if (buflen < len+1) buflen = len+1;
  char *nfdstr = new char[buflen];
  if (! CFStringGetFileSystemRepresentation(utf8, nfdstr, buflen))
    strcpy(nfdstr, cstr);
  return nfdstr;
}
#endif


static FILE *
urlfopen(const GURL &url,const char mode[])
{
  FILE *retval = 0;
#if defined(_WIN32)
  // On Win, try to use _wfopen instead of fopen
  wchar_t *wstr = utf8_to_wide((const char*)url.UTF8Filename());
  wchar_t *wmode = utf8_to_wide(mode);
  if (wstr && wmode)
    retval = _wfopen(wstr, wmode);
  delete [] wstr;
  delete [] wmode;
  if (! retval)
    retval = fopen((const char *)url.NativeFilename(),mode);
#elif defined(__APPLE__)
  // On Mac, prefer the NFD version of the UTF8 filename
  const char *cnfd = utf8_to_utf8mac((const char*)url.UTF8Filename());
  retval = fopen(cnfd, mode);
  delete [] cnfd;
  if (! retval) // Otherwise try unnormalized UTF8
    retval = fopen((const char*)url.UTF8Filename(), mode);
#else
  // Unix filesystems are usually in native encoding
  retval = fopen((const char *)url.NativeFilename(),mode);
  if (! retval)
    retval = fopen((const char *)url.UTF8Filename(),mode);
#endif
  return retval;
}

#ifdef UNIX
static int
urlopen(const GURL &url, const int mode, const int perm)
{
  int retval = -1;
#if defined(__APPLE__)
  // see above
  const char *cnfd = utf8_to_utf8mac((const char*)url.UTF8Filename());
  retval = open(cnfd, mode, perm);
  delete [] cnfd;
  if (retval < 0)
    retval = open((const char*)url.UTF8Filename(), mode, perm);
#else
  // see above
  retval = open((const char *)url.NativeFilename(),mode,perm);
  if (retval < 0)
    retval = open((const char *)url.UTF8Filename(),mode,perm);
#endif
  return retval;
}
#endif /* UNIX */

GUTF8String
ByteStream::Stdio::init(const GURL &url, const char mode[])
{
  GUTF8String retval;
  if (url.fname() != "-")
  {
    fp = urlfopen(url,mode);
    if (!fp)
    {
      //  Failed to open '%s': %s
      G_THROW( ERR_MSG("ByteStream.open_fail") "\t" + url.name()
               +"\t"+GNativeString(strerror(errno)).getNative2UTF8());
    }
  }
  return retval.length()?retval:init(mode);
}

size_t 
ByteStream::Stdio::read(void *buffer, size_t size)
{
  if (!can_read)
    G_THROW( ERR_MSG("ByteStream.no_read") ); //  Stdio not opened for reading
  size_t nitems;
  do
  {
    clearerr(fp);
    nitems = fread(buffer, 1, size, fp); 
    if (nitems<=0 && ferror(fp))
    {
#ifdef EINTR
      if (errno!=EINTR)
#endif
        G_THROW(strerror(errno)); //  (No error in the DjVuMessageFile)
    }
    else
      break;
  } while(true);
  pos += nitems;
  return nitems;
}

size_t 
ByteStream::Stdio::write(const void *buffer, size_t size)
{
  if (!can_write)
    G_THROW( ERR_MSG("ByteStream.no_write") ); //  Stdio not opened for writing
  size_t nitems;
  do
  {
    clearerr(fp);
    nitems = fwrite(buffer, 1, size, fp);
    if (nitems<=0 && ferror(fp))
    {
#ifdef EINTR
      if (errno!=EINTR)
#endif
        G_THROW(strerror(errno)); //  (No error in the DjVuMessageFile)
    }
    else
      break;
  } while(true);
  pos += nitems;
  return nitems;
}

void
ByteStream::Stdio::flush()
{
  if (fflush(fp) < 0)
    G_THROW(strerror(errno)); //  (No error in the DjVuMessageFile)
}

long 
ByteStream::Stdio::tell(void) const
{
  long x = ftell(fp);
  if (x >= 0)
  {
    Stdio *sbs=const_cast<Stdio *>(this);
    (sbs->pos) = x;
  }else
  {
    x=pos;
  }
  return x;
}

int
ByteStream::Stdio::seek(long offset, int whence, bool nothrow)
{
  if (whence==SEEK_SET && offset>=0 && offset==ftell(fp))
    return 0;
  clearerr(fp);
  if (fseek(fp, offset, whence)) 
    {
      if (nothrow) 
        return -1;
      G_THROW(strerror(errno)); //  (No error in the DjVuMessageFile)
    }
  return tell();
}




///////// ByteStream::Memory

ByteStream::Memory::Memory()
  : where(0), bsize(0), nblocks(0), gblocks(blocks,0)
{
}

GUTF8String
ByteStream::Memory::init(void const * const buffer, const size_t sz)
{
  GUTF8String retval;
  G_TRY
  {
    writall(buffer, sz);
    where = 0;
  }
  G_CATCH(ex) // The only error that should be thrown is out of memory...
  {
    retval=ex.get_cause();
  }
  G_ENDCATCH;
  return retval;
}

void 
ByteStream::Memory::empty()
{
  for (int b=0; b<nblocks; b++)
  {
    delete [] blocks[b];
    blocks[b]=0;
  }
  bsize = 0;
  where = 0;
  nblocks = 0;
}

ByteStream::Memory::~Memory()
{
  empty();
}

size_t 
ByteStream::Memory::write(const void *buffer, size_t sz)
{
  long nsz = (long)sz;
  if (nsz <= 0)
    return 0;
  // check memory
  if ( (where+nsz) > ((bsize+0xfff)&~0xfff) )
    {
      // reallocate pointer array
      if ( (where+nsz) > (nblocks<<12) )
        {
          const long old_nblocks=nblocks;
          nblocks = (((where+nsz)+0xffff)&~0xffff) >> 12;
          gblocks.resize(nblocks);
          char const ** eblocks=(char const **)(blocks+old_nblocks);
          for(char const * const * const new_eblocks=blocks+nblocks;
            eblocks <new_eblocks; eblocks++) 
          {
            *eblocks = 0;
          }
        }
      // allocate blocks
      for (long b=(where>>12); (b<<12)<(where+nsz); b++)
      {
        if (! blocks[b])
          blocks[b] = new char[0x1000];
      }
    }
  // write data to buffer
  while (nsz > 0)
    {
      long n = (where|0xfff) + 1 - where;
      n = ((nsz < n) ? nsz : n);
      memcpy( (void*)&blocks[where>>12][where&0xfff], buffer, (size_t)n);
      buffer = (void*) ((char*)buffer + n);
      where += n;
      nsz -= n;
    }
  // adjust size
  if (where > bsize)
    bsize = where;
  return sz;
}

size_t 
ByteStream::Memory::readat(void *buffer, size_t sz, long pos)
{
  if ((long)sz > bsize - pos)
    sz = (size_t)(bsize - pos);
  long nsz = (long)sz;
  if (nsz <= 0)
    return 0;
  // read data from buffer
  while (nsz > 0)
    {
      long n = (pos|0xfff) + 1 - pos;
      n = ((nsz < n) ? nsz : n);
      memcpy(buffer, (void*)&blocks[pos>>12][pos&0xfff], (size_t)n);
      buffer = (void*) ((char*)buffer + n);
      pos += n;
      nsz -= n;
    }
  return sz;
}

size_t 
ByteStream::Memory::read(void *buffer, size_t sz)
{
  sz = readat(buffer,sz,where);
  where += sz;
  return sz;
}

long 
ByteStream::Memory::tell(void) const
{
  return where;
}

int
ByteStream::Memory::seek(long offset, int whence, bool nothrow)
{
  long nwhere = 0;
  switch (whence)
    {
    case SEEK_SET: nwhere = 0; break;
    case SEEK_CUR: nwhere = where; break;
    case SEEK_END: nwhere = bsize; break;
    default: G_THROW( ERR_MSG("bad_arg") "\tByteStream::Memory::seek()");
    }
  nwhere += offset;
  if (nwhere<0)
    G_THROW( ERR_MSG("ByteStream.seek_error2") );
  where = nwhere;
  return 0;
}



/** This function has been moved into Arrays.cpp
    In order to avoid dependencies from ByteStream.o
    to Arrays.o */
#ifdef DO_NOT_MOVE_GET_DATA_TO_ARRAYS_CPP
TArray<char>
ByteStream::get_data(void)
{
   TArray<char> data(0, size()-1);
   readat((char*)data, size(), 0);
   return data;
}
#endif


///////// ByteStream::Static

ByteStream::Static::Static(const void * const buffer, const size_t sz)
  : data((const char *)buffer), bsize(sz), where(0)
{
}

size_t 
ByteStream::Static::read(void *buffer, size_t sz)
{
  long nsz = (long)sz;
  if (nsz > bsize - where)
    nsz = bsize - where;
  if (nsz <= 0)
    return 0;
  memcpy(buffer, data+where, nsz);
  where += nsz;
  return nsz;
}

int
ByteStream::Static::seek(long offset, int whence, bool nothrow)
{
  long nwhere = 0;
  switch (whence)
    {
    case SEEK_SET: nwhere = 0; break;
    case SEEK_CUR: nwhere = where; break;
    case SEEK_END: nwhere = bsize; break;
    default: G_THROW("bad_arg\tByteStream::Static::seek()");
      //  Illegal argument to ByteStream::Static::seek()
    }
  nwhere += offset;
  if (nwhere<0)
    G_THROW( ERR_MSG("ByteStream.seek_error2") );
  //  Attempt to seek before the beginning of the file
  where = nwhere;
  return 0;
}

long 
ByteStream::Static::tell(void) const
{
  return where;
}

GP<ByteStream>
ByteStream::create(void)
{
  return new Memory();
}

GP<ByteStream>
ByteStream::create(void const * const buffer, const size_t size)
{
  Memory *mbs=new Memory();
  GP<ByteStream> retval=mbs;
  mbs->init(buffer,size);
  return retval;
}

GP<ByteStream>
ByteStream::create(const GURL &url,char const * const xmode)
{
  GP<ByteStream> retval;
  const char *mode = ((xmode) ? xmode : "rb");
#ifdef UNIX
  if (!strcmp(mode,"rb")) 
    {
      int fd = urlopen(url,O_RDONLY,0777);
      if (fd >= 0)
        {
#if HAS_MEMMAP && defined(S_IFREG)
          struct stat buf;
          if ( (fstat(fd, &buf) >= 0) && (buf.st_mode & S_IFREG) )
            {
              MemoryMapByteStream *rb = new MemoryMapByteStream();
              retval = rb;
              GUTF8String errmessage = rb->init(fd,true);
              if(errmessage.length())
                retval=0;
            }
#endif
          if (! retval)
            {
              FILE *f = fdopen(fd, mode);
              if (f) 
                {
                  Stdio *sbs=new Stdio();
                  retval=sbs;
                  GUTF8String errmessage=sbs->init(f, mode, true);
                  if(errmessage.length())
                    retval=0;
                }
            }
          if (! retval)
            close(fd);
        }     
    }
#endif
  if (! retval)
    {
      Stdio *sbs=new Stdio();
      retval=sbs;
      GUTF8String errmessage=sbs->init(url, mode);
      if(errmessage.length())
        G_THROW(errmessage);
    }
  return retval;
}

GP<ByteStream>
ByteStream::create(char const * const mode)
{
  GP<ByteStream> retval;
  Stdio *sbs=new Stdio();
  retval=sbs;
  GUTF8String errmessage=sbs->init(mode?mode:"rb");
  if(errmessage.length())
  {
    G_THROW(errmessage);
  }
  return retval;
}

GP<ByteStream>
ByteStream::create(const int fd,char const * const mode,const bool closeme)
{
  GP<ByteStream> retval;
  const char *default_mode="rb";
#if HAS_MEMMAP
  if (   (!mode&&(fd!=0)&&(fd!=1)&&(fd!=2)) 
      || (mode&&(GUTF8String("rb") == mode)))
  {
    MemoryMapByteStream *rb=new MemoryMapByteStream();
    retval=rb;
    GUTF8String errmessage=rb->init(fd,closeme);
    if(errmessage.length())
    {
      retval=0;
    }
  }
  if(!retval)
#endif
  {
    int fd2 = fd;
    FILE *f = 0;
    if (fd == 0 && !closeme 
        && (!mode || mode[0]=='r') )
      {
        f=stdin;
        default_mode = "r";
        fd2=(-1);
      }
    else if (fd == 1 && !closeme 
             && (!mode || mode[0]=='a' || mode[0]=='w') )
      {
        default_mode = "a";
        f=stdout;
        fd2 = -1;
      }
    else if (fd == 2 && !closeme
             && (!mode || mode[0]=='a' || mode[0]=='w') )
      {
        default_mode = "a";
        f=stderr;
        fd2 = -1;
      }
    else
      {
        if (! closeme)
          fd2 = dup(fd);
        f = fdopen(fd2,(char*)(mode?mode:default_mode));
      }

    if(!f)
      {
        if ( fd2 >= 0)
          close(fd2);
        G_THROW( ERR_MSG("ByteStream.open_fail2") );
      }
    Stdio *sbs=new Stdio();
    retval=sbs;
    GUTF8String errmessage=sbs->init(f,mode?mode:default_mode,(fd2>=0));
    if(errmessage.length())
      G_THROW(errmessage);
  }
  return retval;
}

GP<ByteStream>
ByteStream::create(FILE * const f,char const * const mode,const bool closeme)
{
  GP<ByteStream> retval;
#if HAS_MEMMAP
  if (!mode || (GUTF8String("rb") == mode))
  {
    MemoryMapByteStream *rb=new MemoryMapByteStream();
    retval=rb;
    GUTF8String errmessage=rb->init(fileno(f),false);
    if(errmessage.length())
    {
      retval=0;
    }else
    {
      fclose(f);
    }
  }
  if(!retval)
#endif
  {
    Stdio *sbs=new Stdio();
    retval=sbs;
    GUTF8String errmessage=sbs->init(f,mode?mode:"rb",closeme);
    if(errmessage.length())
    {
      G_THROW(errmessage);
    }
  }
  return retval;
}

GP<ByteStream>
ByteStream::create_static(const void * buffer, size_t sz)
{
  return new Static(buffer, sz);
}

#if HAS_MEMMAP
MemoryMapByteStream::MemoryMapByteStream(void)
: ByteStream::Static(0,0)
{}

GUTF8String
MemoryMapByteStream::init(FILE *const f,const bool closeme)
{
  GUTF8String retval;
  retval=init(fileno(f),false);
  if(closeme)
  {
    fclose(f);
  }
  return retval;
}

GUTF8String
MemoryMapByteStream::init(const int fd,const bool closeme)
{
  GUTF8String retval;
  data = (char*)(-1);
#if defined(PROT_READ) && defined(MAP_SHARED)
  struct stat statbuf;
  if(!fstat(fd,&statbuf) && statbuf.st_size)
    {
      bsize=statbuf.st_size;
      data=(char *)mmap(0,statbuf.st_size,PROT_READ,MAP_SHARED,fd,0);
    }
#endif
  if(data == (char *)(-1))
    retval = ERR_MSG("ByteStream.open_fail2");
  if(closeme)
    close(fd);
  return retval;
}

MemoryMapByteStream::~MemoryMapByteStream()
{
  if(data)
  {
    munmap(const_cast<char *>(data),bsize);
  }
}

#endif

ByteStream::Wrapper::~Wrapper() {}


GP<ByteStream> 
ByteStream::get_stdin(char const *mode)
{
  static GP<ByteStream> gp = ByteStream::create(0,mode,false);
  return gp;
}

GP<ByteStream> 
ByteStream::get_stdout(char const *mode)
{
  static GP<ByteStream> gp = ByteStream::create(1,mode,false);
  return gp;
}

GP<ByteStream> 
ByteStream::get_stderr(char const *mode)
{
  static GP<ByteStream> gp = ByteStream::create(2,mode,false);
  return gp;
}


/** Looks up the message and writes it to the specified stream. */
void ByteStream::formatmessage( const char *fmt, ... )
{
  va_list args;
  va_start(args, fmt);
  const GUTF8String message(fmt,args);
  writemessage( message );
}

/** Looks up the message and writes it to the specified stream. */
void ByteStream::writemessage( const char *message )
{
  writestring( DjVuMessage::LookUpUTF8( message ) );
}

static void 
read_file(ByteStream &bs,char *&buffer,GPBuffer<char> &gbuffer)
{
  const int size=bs.size();
  int pos=0;
  if(size>0)
  {
    size_t readsize=size+1;
    gbuffer.resize(readsize);
    for(int i;readsize&&(i=bs.read(buffer+pos,readsize))>0;pos+=i,readsize-=i)
      EMPTY_LOOP;
  }else
  {
    const size_t readsize=32768;
    gbuffer.resize(readsize);
    for(int i;((i=bs.read(buffer+pos,readsize))>0);
      gbuffer.resize((pos+=i)+readsize))
      EMPTY_LOOP;
  }
  buffer[pos]=0;
}

GNativeString
ByteStream::getAsNative(void)
{
  char *buffer;
  GPBuffer<char> gbuffer(buffer);
  read_file(*this,buffer,gbuffer);
  return GNativeString(buffer);
}

GUTF8String
ByteStream::getAsUTF8(void)
{
  char *buffer;
  GPBuffer<char> gbuffer(buffer);
  read_file(*this,buffer,gbuffer);
  return GUTF8String(buffer);
}


#ifdef HAVE_NAMESPACES
}
# ifndef NOT_USING_DJVU_NAMESPACE
using namespace DJVU;
# endif
#endif

void
DjVuPrintErrorUTF8(const char *fmt, ... )
{
  G_TRY {
    GP<ByteStream> errout = ByteStream::get_stderr();
    if (errout)
      {
        errout->cp=ByteStream::NATIVE;
        va_list args;
        va_start(args, fmt); 
        const GUTF8String message(fmt,args);
        errout->writestring(message);
      }
    // Need to catch all exceptions because these might be 
    // called from an outer exception handler (with prejudice)
  } G_CATCH_ALL { } G_ENDCATCH;
}

void
DjVuPrintErrorNative(const char *fmt, ... )
{
  G_TRY {
    GP<ByteStream> errout = ByteStream::get_stderr();
    if (errout)
      {
        errout->cp=ByteStream::NATIVE;
        va_list args;
        va_start(args, fmt); 
        const GNativeString message(fmt,args);
        errout->writestring(message);
      }
    // Need to catch all exceptions because these might be 
    // called from an outer exception handler (with prejudice)
  } G_CATCH_ALL { } G_ENDCATCH;
}

void
DjVuPrintMessageUTF8(const char *fmt, ... )
{
  G_TRY {
    GP<ByteStream> strout = ByteStream::get_stdout();
    if (strout)
      {
        strout->cp=ByteStream::NATIVE;
        va_list args;
        va_start(args, fmt);
        const GUTF8String message(fmt,args);
        strout->writestring(message);
      }
    // Need to catch all exceptions because these might be 
    // called from an outer exception handler (with prejudice)
  } G_CATCH_ALL { } G_ENDCATCH;
}

void
DjVuPrintMessageNative(const char *fmt, ... )
{
  G_TRY {
    GP<ByteStream> strout = ByteStream::get_stdout();
    if (strout)
      {
        strout->cp=ByteStream::NATIVE;
        va_list args;
        va_start(args, fmt);
        const GNativeString message(fmt,args);
        strout->writestring(message);
      }
    // Need to catch all exceptions because these might be 
    // called from an outer exception handler (with prejudice)
  } G_CATCH_ALL { } G_ENDCATCH;
}
