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

#ifndef _DJVUANNO_H
#define _DJVUANNO_H
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#if NEED_GNUG_PRAGMAS
# pragma interface
#endif



/** @name DjVuAnno.h

    Files #"DjVuAnno.h"# and #"DjVuAnno.cpp"# implement the mechanism for
    annotating DjVuImages. Annotations are additional instructions for the
    plugin about how the image should be displayed.  The exact format of
    annotations is not strictly defined. The only requirement is that they
    have to be stored as a sequence of chunks inside a #FORM:ANNO#.

    This file implements annotations understood by the DjVu plugins 
    and encoders.


    using: contents of #ANT*# chunks.

    Contents of the #FORM:ANNO# should be passed to \Ref{DjVuAnno::decode}()
    for parsing, which initializes \Ref{DjVuAnno::ANT} 
    and fills them with decoded data. 
    @memo Implements support for DjVuImage annotations
    @author Andrei Erofeev <eaf@geocities.com>
*/
//@{


#include "GString.h"

#ifdef HAVE_NAMESPACES
namespace DJVU {
# ifdef NOT_DEFINED // Just to fool emacs c++ mode
}
#endif
#endif

class GMapArea;
class ByteStream;

// -------- DJVUANT --------

/** This class contains some trivial annotations of the page or of the
    document such as page border color, page alignment, initial zoom and
    display mode, hyperlinks and highlighted areas.  All this information is
    put inside a textual chunk #ANTa# in pseudo-lisp format. Decoding and
    encoding are normally done by \Ref{DjVuANT::decode}() and
    \Ref{DjVuANT::encode}() functions. */

class DJVUAPI DjVuANT : public GPEnabled
{
protected:
      /// Constructs an empty annotation object.
   DjVuANT(void);

public:
   enum { MODE_UNSPEC=0, MODE_COLOR, MODE_FORE, MODE_BACK, MODE_BW };
   enum { ZOOM_STRETCH=-4, ZOOM_ONE2ONE=-3, ZOOM_WIDTH=-2,
	  ZOOM_PAGE=-1, ZOOM_UNSPEC=0 };
   enum alignment { ALIGN_UNSPEC=0, ALIGN_LEFT, ALIGN_CENTER, ALIGN_RIGHT,
	  ALIGN_TOP, ALIGN_BOTTOM };

      /// Creates an empty annotation object.
   static GP<DjVuANT> create(void) { return new DjVuANT; }
   virtual ~DjVuANT();

      /** Background color. Is in #0x00RRBBGG# format. #0xffffffff# if
	  there were no background color records in the annotation chunk. */
   unsigned long int	bg_color;
      /** Initial zoom. Possible values are:
	  \begin{description}
          \item[ZOOM_STRETCH] the image is stretched to the viewport.
          \item[ZOOM_ONE2ONE] the image is displayed pixel-to-pixel.
          \item[ZOOM_WIDTH] "Fit width" mode.
          \item[ZOOM_PAGE] "Fit page" mode.
          \item[ZOOM_UNSPEC] Annotation does not specify a zoom factor.
          \end{description} */
   int		zoom;
      /** Initial mode. Possible values are:
	  \begin{description}
             \item[MODE_COLOR] color mode.
             \item[MODE_FORE] foreground mode.
             \item[MODE_BACK] background mode.
             \item[MODE_BW] black and white mode.
             \item[MODE_UNSPEC] Annotation does not specify a display mode.
	     \item[Any positive number] Zoom in \%. Please note that
                   all constants above are either negative or ZERO. Thus
                   it's possible to distinguish numerical zoom from those
                   special cases.
          \end{description} */
   int		mode;
      /** Horizontal page alignment. Possible values are #ALIGN_LEFT#,
	  #ALIGN_CENTER#, #ALIGN_RIGHT# and #ALIGN_UNSPEC#. */
   alignment hor_align;
      /** Vertical page alignment. Possible values are #ALIGN_TOP#,
	  #ALIGN_CENTER#, #ALIGN_BOTTOM# and #ALIGN_UNSPEC#. */
   alignment ver_align;
      /** List of defined map areas. They may be just areas of highlighting
	  or hyperlink. Please refer to \Ref{GMapArea}, \Ref{GMapRect},
	  \Ref{GMapPoly} and \Ref{GMapOval} for details. */
   GPList<GMapArea> map_areas;
      /** Metainformations like title, author ... */
   GMap<GUTF8String,GUTF8String> metadata;
      /** Metainformations like title, author ... */
   GUTF8String xmpmetadata;
      /** Returns TRUE if no features are specified or specified features
         are not different from default ones */
   bool	is_empty(void) const;

      /** Decodes contents of annotation chunk #ANTa#. The chunk data is
	  read from ByteStream #bs# until reaching an end-of-stream marker.
	  This function is normally called after a call to
	  \Ref{IFFByteStream::get_chunk}(). */
   void	decode(ByteStream &bs);

      /** Same as \Ref{decode}() but adds the new data to what has
	  been decoded before. */
   void merge(ByteStream & bs);

      /** Encodes the #ANTa# chunk. The annotation data is simply written
	  into ByteStream #bs# with no IFF header. This function is normally
	  called after a call to \Ref{IFFByteStream::put_chunk}(). */
   void encode(ByteStream &bs);

      /// Encodes data back into raw annotation data.
   GUTF8String encode_raw(void) const;

      /// Returns a copy of this object
   GP<DjVuANT>	copy(void) const;
   
      /** Returns the number of bytes needed by this data structure. It's
	  used by caching routines to estimate the size of a \Ref{DjVuImage}. */
   unsigned int get_memory_usage() const;

      /// Converts color from string in \#RRGGBB notation to an unsigned integer
   static unsigned long int	cvt_color(const char * color, unsigned long int def);
      /// Obtain the <MAP></MAP> tag for these annotations.
   GUTF8String get_xmlmap(const GUTF8String &name, const int height) const;
      /// Write the <MAP></MAP> tag for these annotations.
   void writeMap(
     ByteStream &bs,const GUTF8String &name, const int height) const;
      /// Obtain the XML flags for the default specifications.
   GUTF8String get_paramtags(void) const;
      /// Write the XML flags for the default specifications.
   void writeParam(ByteStream &out_str) const;
private:
   void	decode(class GLParser & parser);
   static GUTF8String read_raw(ByteStream & str);
   static unsigned char	decode_comp(char ch1, char ch2);
   static unsigned long int get_bg_color(class GLParser & parser);
   static int get_zoom(class GLParser & parser);
   static int get_mode(class GLParser & parser);
   static alignment get_hor_align(class GLParser & parser);
   static alignment get_ver_align(class GLParser & parser);
   static GPList<GMapArea> get_map_areas(class GLParser & parser);
   static GMap<GUTF8String, GUTF8String>get_metadata(GLParser & parser);
   static GUTF8String get_xmpmetadata(GLParser & parser);
   static void del_all_items(const char * name, class GLParser & parser);
};

// -------- DJVUANNO --------


/** This is a top-level class containing annotations of a DjVu document (or
    just a page). It has only two functions: \Ref{encode}() and
    \Ref{decode}().  Both of them work with a sequence of annotation chunks
    from #FORM:ANNO# form. Basing on the name of the chunks they call
    #encode()# and #decode()# functions of the proper annotation structure
    (like \Ref{ANT}). The real work of encoding and decoding is done by
    lower-level classes. */
class DJVUAPI DjVuAnno : public GPEnabled
{
protected:
   DjVuAnno(void) {}
public:
   /// Creation method.
   static GP<DjVuAnno> create(void) { return new DjVuAnno; }

   GP<DjVuANT>	ant;

      /** Decodes a sequence of annotation chunks and merges contents of every
	  chunk with previously decoded information. This function
	  should be called right after applying \Ref{IFFByteStream::get_chunk}()
	  to data from #FORM:ANNO#. */
   void decode(const GP<ByteStream> &bs);

      /** Encodes all annotations back into a sequence of chunks to be put
	  inside a #FORM:ANNO#. */
   void	encode(const GP<ByteStream> &bs);

      /// Returns a copy of this object
   GP<DjVuAnno>	copy(void) const;

      /** Merged the contents of this class and of annotations
	  pointed by #anno# pointer */
   void		merge(const GP<DjVuAnno> & anno);

      /** Returns the number of bytes needed by this data structure. It's
	  used by caching routines to estimate the size of a \Ref{DjVuImage}. */
   inline unsigned int get_memory_usage() const;
      /// Obtain the <MAP></MAP> tag for these annotations.
   GUTF8String get_xmlmap(const GUTF8String &name, const int height) const;
      /// Write the <MAP></MAP> tag for these annotations.
   void writeMap(
     ByteStream &bs,const GUTF8String &name, const int height) const;
      /// Obtain the XML flags for the default specifications.
   GUTF8String get_paramtags(void) const;
      /// Write the XML flags for the default specifications.
   void writeParam(ByteStream &out_str) const;
private: // dummy stuff
   static void decode(ByteStream *);
   static void encode(ByteStream *);
};

//@}

inline unsigned int 
DjVuAnno::get_memory_usage() const
{
  return (ant)?(ant->get_memory_usage()):0;
}

// ----- THE END

#ifdef HAVE_NAMESPACES
}
# ifndef NOT_USING_DJVU_NAMESPACE
using namespace DJVU;
# endif
#endif
#endif
