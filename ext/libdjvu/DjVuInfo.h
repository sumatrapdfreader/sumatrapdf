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

#ifndef _DJVUINFO_H
#define _DJVUINFO_H
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#if NEED_GNUG_PRAGMAS
# pragma interface
#endif


/** @name DjVuInfo.h
    Each instance of class #DjVuInfo# represents the information
    contained in the information chunk of a DjVu file.  This #"INFO"#
    chunk is always the first chunk of a DjVu file.
    @memo
    DjVu information chunk.
    @author
    L\'eon Bottou <leonb@research.att.com>
*/
//@{



#include "GSmartPointer.h"
#include "GRect.h"

#ifdef HAVE_NAMESPACES
namespace DJVU {
# ifdef NOT_DEFINED // Just to fool emacs c++ mode
}
#endif
#endif

class ByteStream;

/** @name DjVu version constants
    @memo DjVu file format version. */
//@{
/** Current DjVu format version.  The value of this macro represents the
    version of the DjVu file format implemented by this release of the DjVu
    Reference Library. */
#define DJVUVERSION              26
/** DjVu format version. This is the value used in files produced 
    with DjVuLibre. This is smaller than DJVUVERSION because version
    number inflation causes problems with older software. */ 
#define DJVUVERSION_FOR_OUTPUT   24
/** This is the version which introduced orientations. */
#define DJVUVERSION_ORIENTATION  22
/** Oldest DjVu format version supported by this library.  This release of the
    library cannot completely decode DjVu files whose version field is less
    than or equal to this number. */
#define DJVUVERSION_TOO_OLD      15
/** Newest DjVu format partially supported by this library.  This release of
    the library will attempt to decode files whose version field is smaller
    than this macro.  If the version field is greater than or equal to this
    number, the decoder will just throw a \Ref{GException}.  */
#define DJVUVERSION_TOO_NEW      50
//@}


class GUTF8String;

/** Information component.
    Each instance of class #DjVuInfo# represents the information
    contained in the information chunk of a DjVu file.  This #"INFO"#
    chunk is always the first chunk of a DjVu file.
 */

class DJVUAPI DjVuInfo : public GPEnabled
{
protected:
  DjVuInfo(void);
public:
  /** Creates an empty DjVuInfo object.
      The #width# and #height# fields are set to zero.
      All other fields are initialized with suitable default values. */
  static GP<DjVuInfo> create(void) {return new DjVuInfo();}

  /** Decodes the DjVu #"INFO"# chunk.  This function reads binary data from
      ByteStream #bs# and populates the fields of this DjVuInfo object.  It is
      normally called after detecting an #"INFO"# chunk header with function
      \Ref{IFFByteStream::get_chunk}. */
  void decode(ByteStream &bs);
  /** Encodes the DjVu #"INFO"# chunk. This function writes the fields of this
      DjVuInfo object into ByteStream #bs#. It is normally called after
      creating an #"INFO"# chunk header with function
      \Ref{IFFByteStream::put_chunk}. */
  void encode(ByteStream &bs);  
  /** Returns the number of bytes used by this object. */
  unsigned int get_memory_usage() const;
  /** Width of the DjVu image (in pixels). */
  int width;
  /** Height of the DjVu image (in pixels). */
  int height;
  /** DjVu file version number.  This number characterizes the file format
      version used by the encoder to generate this DjVu image.  A decoder
      should compare this version number with the constants described in
      section "\Ref{DjVu version constants}". */
  int version;
  /** Resolution of the DjVu image.  The resolution is given in ``pixels per
      2.54 centimeters'' (this unit is sometimes called ``pixels per
      inch''). Display programs can use this information to determine the
      natural magnification to use for rendering a DjVu image. */
  int dpi;
  /** Gamma coefficient of the display for which the image was designed.  The
      rendering functions can use this information in order to perform color
      correction for the intended display device. */
  double gamma;

  /** Image orientation:
      0: no rotation      1: 90 degrees counter-clockwise
      2: 180 degrees      3: 270 degrees counter-clockwise */
  int orientation;

     /// Obtain the flags for the default specifications.
  GUTF8String get_paramtags(void) const;
     /// Obtain the flags for the default specifications.
  void writeParam(ByteStream &out_str) const;
};


//@}

// ----- THE END

#ifdef HAVE_NAMESPACES
}
# ifndef NOT_USING_DJVU_NAMESPACE
using namespace DJVU;
# endif
#endif
#endif
