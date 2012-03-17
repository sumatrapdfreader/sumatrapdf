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

#ifndef _UNICODEBYTESTREAM_H_
#define _UNICODEBYTESTREAM_H_
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#if NEED_GNUG_PRAGMAS
# pragma interface
#endif


/** @name UnicodeByteStream.h

    Files #"UnicodeByteStream.h"# and #"UnicodeByteStream.cpp"# 
    implement a parser for files structured W3C Extensible Markup 
    Language (XML) 1.0 (Second Edition).
    
    Class \Ref{UnicodeByteStream} provides a way to read or write XML files.
    files.  Member functions provide an easy mean to position the underlying
    \Ref{ByteStream}.

    {\bf References:} 
    W3C Extensible Markup Language (XML) 1.0 (Second Edition)
    \URL{http://www.w3.org/TR/2000/REC-xml-20001006.html}

    @memo 
    XML file parser.
    @author
    Bill C Riemers <docbill@sourceforge.net>
*/
//@{

#include "DjVuGlobal.h"
#include "GString.h"
#include "ByteStream.h"

#include <stddef.h>

#ifdef HAVE_NAMESPACES
namespace DJVU {
# ifdef NOT_DEFINED // Just to fool emacs c++ mode
}
#endif
#endif



/** ByteStream interface for an Unicode file. 

    Class #UnicodeByteStream# augments the #ByteStream# interface with
    functions for navigating Unicode documents.  It works in relation
    with a ByteStream specified at construction time. 

    {\bf Reading an Unicode file} --- You can read an Unicode file by
    constructing an #UnicodeByteStream# object attached to the ByteStream
    containing the Unicode file.
    
    {\bf Writing an Unicode file} --- You can write an Unicode file by
    constructing an #UnicodeByteStream# object attached to the seekable
    ByteStream object that will contain the XML file.

    Writing an XML file requires a seekable ByteStream (see
    \Ref{ByteStream::is_seekable}).  This is not much of a problem because you
    can always create the XML file into a \Ref{MemoryByteStream} and then use
    \Ref{ByteStream::copy} to transfer the XML file into a non seekable
    ByteStream.  */

class UnicodeByteStream : public ByteStream
{
protected:
  UnicodeByteStream(const UnicodeByteStream &bs);
  UnicodeByteStream(GP<ByteStream> bs,
    const GStringRep::EncodeType encodetype=GStringRep::XUTF8);
public:
  /** Constructs an UnicodeByteStream object attached to ByteStream #bs#.
      Any ByteStream can be used when reading an XML file.  Writing
      an XML file however requires a seekable ByteStream. */
  static GP<UnicodeByteStream> create(GP<ByteStream> bs,
    const GStringRep::EncodeType encodetype=GStringRep::XUTF8)
  { return new UnicodeByteStream(bs,encodetype); }

  // --- BYTESTREAM INTERFACE
  ~UnicodeByteStream();
  /// Sets the encoding type and seek's to position 0.
  void set_encodetype(const GStringRep::EncodeType et=GStringRep::XUTF8);
  void set_encoding(const GUTF8String &encoding);
  /// Simmular to fgets(), except read aheads effect the tell() position.
  virtual GUTF8String gets(size_t const t=0,unsigned long const stopat='\n',bool const inclusive=true); 
  /// Resets the gets buffering as well as physically seeking.
  virtual int seek(long offset, int whence = SEEK_SET, bool nothrow=false);
  /** Physically reads the specified bytes, and truncate the read ahead buffer.
    */
  virtual size_t read(void *buffer, size_t size);
  /// Not correctly implimented...
  virtual size_t write(const void *buffer, size_t size);
  /// tell will tell you the read position, including read ahead for gets()...
  virtual long tell(void) const;
  /// Does a flush, and clears the read ahead buffer.
  virtual void flush(void);

  /// Find out how many lines have been read with gets.
  int get_lines_read(void) const { return linesread; }
protected:
  /// The real byte stream.
  GP<ByteStream> bs;
  GUTF8String buffer;
  int bufferpos;
  int linesread;
  long startpos;
private:
  // Cancel C++ default stuff
  UnicodeByteStream & operator=(UnicodeByteStream &);
};


class XMLByteStream : public UnicodeByteStream
{
protected:
  XMLByteStream(GP<ByteStream> &bs);
  XMLByteStream(UnicodeByteStream &bs);
  void init(void);
public:
  static GP<XMLByteStream> create(GP<ByteStream> bs);
  static GP<XMLByteStream> create(UnicodeByteStream &bs);
  // --- BYTESTREAM INTERFACE
  ~XMLByteStream();
};

inline GP<XMLByteStream>
XMLByteStream::create(UnicodeByteStream &bs)
{
  return new XMLByteStream(bs);
}

//@}


#ifdef HAVE_NAMESPACES
}
# ifndef NOT_USING_DJVU_NAMESPACE
using namespace DJVU;
# endif
#endif
#endif

