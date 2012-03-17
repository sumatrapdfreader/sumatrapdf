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

#ifndef _IFFBYTESTREAM_H_
#define _IFFBYTESTREAM_H_
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#if NEED_GNUG_PRAGMAS
# pragma interface
#endif


/** @name IFFByteStream.h

    Files #"IFFByteStream.h"# and #"IFFByteStream.cpp"# implement a parser for
    files structured according the Electronic Arts ``EA IFF 85 Interchange
    File Format''.  IFF files are composed of a sequence of data {\em chunks}.
    Each chunk is identified by a four character {\em chunk identifier}
    describing the type of the data stored in the chunk.  A few special chunk
    identifiers, for instance #"FORM"#, are reserved for {\em composite
    chunks} which themselves contain a sequence of data chunks.  This
    conventions effectively provides IFF files with a convenient hierarchical
    structure.  Composite chunks are further identified by a secondary chunk
    identifier.
    
    We found convenient to define a {\em extended chunk identifier}.  In the
    case of a regular chunk, the extended chunk identifier is simply the
    chunk identifier, as in #"PM44"#. In the case of a composite chunk, the
    extended chunk identifier is composed by concatenating the main chunk
    identifier, a colon, and the secondary chunk identifier, as in
    #"FORM:DJVU"#.

    Class \Ref{IFFByteStream} provides a way to read or write IFF structured
    files.  Member functions provide an easy mean to position the underlying
    \Ref{ByteStream} at the beginning of each chunk and to read or write the
    data until reaching the end of the chunk.  The utility program
    \Ref{djvuinfo} demonstrates how to use class #IFFByteStream#.

    {\bf IFF Files and ZP-Coder} ---
    Class #IFFByteStream# repositions the underlying ByteStream whenever a new
    chunk is accessed.  It is possible to code chunk data with the ZP-Coder
    without worrying about the final file position. See class \Ref{ZPCodec}
    for more details.
    
    {\bf DjVu IFF Files} --- We had initially planned to exactly follow the
    IFF specifications.  Then we realized that certain versions of MSIE
    recognize any IFF file as a Microsoft AIFF sound file and pop a message
    box "Cannot play that sound".  It appears that the structure of AIFF files
    is entirely modeled after the IFF standard, with small variations
    regarding the endianness of numbers and the padding rules.  We eliminate
    this problem by casting the octet protection spell.  Our IFF files always
    start with the four octets #0x41,0x54,0x26,0x54# followed by the fully
    conformant IFF byte stream.  Class #IFFByteStream# silently skips these
    four octets when it encounters them.

    {\bf References} --- EA IFF 85 Interchange File Format specification:\\
    \URL{http://www.cica.indiana.edu/graphics/image_specs/ilbm.format.txt} or
    \URL{http://www.tnt.uni-hannover.de/soft/compgraph/fileformats/docs/iff.pre}

    @memo 
    IFF file parser.
    @author
    L\'eon Bottou <leonb@research.att.com>

// From: Leon Bottou, 1/31/2002
// This has been changed by Lizardtech to fit better 
// with their re-implementation of ByteStreams.

*/
//@{


#include "DjVuGlobal.h"
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "GException.h"
#include "GString.h"
#include "ByteStream.h"


#ifdef HAVE_NAMESPACES
namespace DJVU {
# ifdef NOT_DEFINED // Just to fool emacs c++ mode
}
#endif
#endif

/** ByteStream interface for an IFF file. 

    Class #IFFByteStream# augments the #ByteStream# interface with
    functions for navigating from chunk to chunk.  It works in relation
    with a ByteStream specified at construction time. 

    {\bf Reading an IFF file} --- You can read an IFF file by constructing an
    #IFFByteStream# object attached to the ByteStream containing the IFF file.
    Calling function \Ref{get_chunk} positions the file pointer at the
    beginning of the first chunk.  You can then use \Ref{ByteStream::read} to
    access the chunk data.  Function #read# will return #0# if you attempt to
    read past the end of the chunk, just as if you were trying to read past
    the end of a file. You can at any time call function \Ref{close_chunk} to
    terminate reading data in this chunk.  The following chunks can be
    accessed by calling #get_chunk# and #close_chunk# repeatedly until you
    reach the end of the file.  Function #read# is not very useful when
    accessing a composite chunk.  You can instead make nested calls to
    functions #get_chunk# and #close_chunk# in order to access the chunks
    located inside the composite chunk.
    
    {\bf Writing an IFF file} --- You can write an IFF file by constructing an
    #IFFByteStream# object attached to the seekable ByteStream object that
    will contain the IFF file.  Calling function \Ref{put_chunk} creates a
    first chunk header and positions the file pointer at the beginning of the
    chunk.  You can then use \Ref{ByteStream::write} to store the chunk data.
    Calling function \Ref{close_chunk} terminates the current chunk.  You can
    append more chunks by calling #put_chunk# and #close_chunk# repeatedly.
    Function #write# is not very useful for writing a composite chunk.  You
    can instead make nested calls to function #put_chunk# and #close_chunk# in
    order to create chunks located inside the composite chunk.

    Writing an IFF file requires a seekable ByteStream (see
    \Ref{ByteStream::is_seekable}).  This is not much of a problem because you
    can always create the IFF file into a \Ref{MemoryByteStream} and then use
    \Ref{ByteStream::copy} to transfer the IFF file into a non seekable
    ByteStream.  */

class DJVUAPI IFFByteStream : protected ByteStream::Wrapper
{
protected: 
  IFFByteStream(const GP<ByteStream> &bs, const int pos);
public:
  /** Constructs an IFFByteStream object attached to ByteStream #bs#.
      Any ByteStream can be used when reading an IFF file.  Writing
      an IFF file however requires a seekable ByteStream. */
  static GP<IFFByteStream> create(const GP<ByteStream> &bs);
  // --- BYTESTREAM INTERFACE
  ~IFFByteStream();
  virtual size_t read(void *buffer, size_t size);
  virtual size_t write(const void *buffer, size_t size);
  virtual long tell(void) const;
  // -- NAVIGATING CHUNKS
  /** Enters a chunk for reading.  Function #get_chunk# returns zero when the
      last chunk has already been accessed.  Otherwise it parses a chunk
      header, positions the IFFByteStream at the beginning of the chunk data,
      stores the extended chunk identifier into string #chkid#, and returns
      the non zero chunk size.  The file offset of the chunk data may be
      retrieved using function #tell#.  The chunk data can then be read using
      function #read# until reaching the end of the chunk.  Advanced users may
      supply two pointers to integer variables using arguments #rawoffsetptr#
      and #rawsizeptr#. These variables will be overwritten with the offset
      and the length of the file segment containing both the chunk header and
      the chunk data. */
  int get_chunk(GUTF8String &chkid, int *rawoffsetptr=0, int *rawsizeptr=0);
  /** Enters a chunk for writing.  Function #put_chunk# prepares a chunk
      header and positions the IFFByteStream at the beginning of the chunk
      data.  Argument #chkid# defines a extended chunk identifier for this
      chunk.  The chunk data can then be written using function #write#.  The
      chunk is terminated by a matching call to function #close_chunk#.  When
      #insertmagic# is non zero, function #put_chunk# inserts the bytes:
      0x41, 0x54, 0x26, 0x54 before the chunk header, as discussed in
      \Ref{IFFByteStream.h}. */
  void put_chunk(const char *chkid, int insertmagic=0);
  /** Leaves the current chunk.  This function leaves the chunk previously
      entered by a matching call to #get_chunk# and #put_chunk#.  The
      IFFByteStream is then ready to process the next chunk at the same
      hierarchical level. */
  void close_chunk();
  /** This is identical to the above, plus it adds a seek to the start of
      the next chunk.  This way we catch EOF errors with the current chunk.*/
  void seek_close_chunk();
  /** Returns true when it is legal to call #read# or #write#. */
  int ready();
  /** Returns true when the current chunk is a composite chunk. */
  int composite();
  /** Returns the current chunk identifier of the current chunk.  String
      #chkid# is overwritten with the {\em extended chunk identifier} of the
      current chunk.  The extended chunk identifier of a regular chunk is
      simply the chunk identifier, as in #"PM44"#.  The extended chunk
      identifier of a composite chunk is the concatenation of the chunk
      identifier, of a semicolon #":"#, and of the secondary chunk identifier,
      as in #"FORM:DJVU"#. */
  void short_id(GUTF8String &chkid);
  /** Returns the qualified chunk identifier of the current chunk.  String
      #chkid# is overwritten with the {\em qualified chunk identifier} of the
      current chunk.  The qualified chunk identifier of a composite chunk is
      equal to the extended chunk identifier.  The qualified chunk identifier
      of a regular chunk is composed by concatenating the secondary chunk
      identifier of the closest #"FORM"# or #"PROP"# composite chunk
      containing the current chunk, a dot #"."#, and the current chunk
      identifier, as in #"DJVU.INFO"#.  According to the EA IFF 85 identifier
      scoping rules, the qualified chunk identifier uniquely defines how the
      chunk data should be interpreted. */
  void full_id(GUTF8String &chkid);
  /** Checks a potential chunk identifier.  This function categorizes the
      chunk identifier formed by the first four characters of string #chkid#.
      It returns #0# if this is a legal identifier for a regular chunk.  It
      returns #+1# if this is a reserved composite chunk identifier.  It
      returns #-1# if this is an illegal or otherwise reserved identifier
      which should not be used.  */
  static int check_id(const char *id);
  GP<ByteStream> get_bytestream(void) {return this;}
  /** Copy data from another ByteStream.  A maximum of #size# bytes are read
      from the ByteStream #bsfrom# and are written to the ByteStream #*this#
      at the current position.  Less than #size# bytes may be written if an
      end-of-file mark is reached on #bsfrom#.  This function returns the
      total number of bytes copied.  Setting argument #size# to zero (the
      default value) has a special meaning: the copying process will continue
      until reaching the end-of-file mark on ByteStream #bsfrom#, regardless
      of the number of bytes transferred.  */
  size_t copy(ByteStream &bsfrom, size_t size=0)
  { return get_bytestream()->copy(bsfrom,size); }
  /** Flushes all buffers in the ByteStream.  Calling this function
      guarantees that pending data have been actually written (i.e. passed to
      the operating system). Class #ByteStream# provides a default
      implementation which does nothing. */
  virtual void flush(void)
  { ByteStream::Wrapper::flush(); }
  /** This is a simple compare method.  The IFFByteStream may be read for
      the sake of the comparison.  Since IFFByteStreams are non-seekable,
      the stream is not valid for use after comparing, regardless of the
      result. */
  bool compare(IFFByteStream &iff);
  /** #has_magic_att# is true if the stream has 
      the DjVu magic 'AT&T' marker. */
  bool has_magic_att;
  /** #has_magic_sdjv# is true if the stream has 
      the Celartem magic 'SDJV' marker. */
  bool has_magic_sdjv;
private:
  // private datatype
  struct IFFContext
  {
    IFFContext *next;
    long offStart;
    long offEnd;
    char idOne[4];
    char idTwo[4];
    char bComposite;
  };
  // Implementation
  IFFContext *ctx;
  long offset;
  long seekto;
  int dir;
  // Cancel C++ default stuff
  IFFByteStream(const IFFByteStream &);
  IFFByteStream & operator=(const IFFByteStream &);
  static GP<IFFByteStream> create(ByteStream *bs);
};

//@}



#ifdef HAVE_NAMESPACES
}
# ifndef NOT_USING_DJVU_NAMESPACE
using namespace DJVU;
# endif
#endif
#endif
