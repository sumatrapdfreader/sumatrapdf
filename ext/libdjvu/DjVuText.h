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

#ifndef _DJVUTEXT_H
#define _DJVUTEXT_H
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#if NEED_GNUG_PRAGMAS
# pragma interface
#endif



/** @name DjVuText.h

    Files #"DjVuText.h"# and #"DjVuText.cpp"# implement the mechanism for
    text in DjVuImages.

    This file implements annotations understood by the DjVu plugins 
    and encoders.


    using: contents of #TXT*# chunks.

    Contents of the #FORM:TEXT# should be passed to \Ref{DjVuText::decode}()
    for parsing, which initializes \Ref{DjVuText::TXT} 
    and fills them with decoded data. 
    @memo Implements support for DjVuImage hidden text.
    @author Andrei Erofeev <eaf@geocities.com>
*/
//@{


#include "GMapAreas.h"

#ifdef HAVE_NAMESPACES
namespace DJVU {
# ifdef NOT_DEFINED // Just to fool emacs c++ mode
}
#endif
#endif


class ByteStream;

// -------- DJVUTXT --------

/** Description of the text contained in a DjVu page.  This class contains the
    textual data for the page.  It describes the text as a hierarchy of zones
    corresponding to page, column, region, paragraph, lines, words, etc...
    The piece of text associated with each zone is represented by an offset
    and a length describing a segment of a global UTF8 encoded string.  */

class DJVUAPI DjVuTXT : public GPEnabled
{
protected:
  DjVuTXT(void) {}
public:
  /// Default creator
  static GP<DjVuTXT> create(void) {return new DjVuTXT();}

  /** These constants are used to tell what a zone describes.
      This can be useful for a copy/paste application. 
      The deeper we go into the hierarchy, the higher the constant. */
  enum ZoneType { PAGE=1, COLUMN=2, REGION=3, PARAGRAPH=4, 
                  LINE=5, WORD=6, CHARACTER=7 };
  /** Data structure representing document textual components.
      The text structure is represented by a hierarchy of rectangular zones. */
  class DJVUAPI Zone 
  {
  public:
    Zone();
    /** Type of the zone. */
    enum ZoneType ztype;
    /** Rectangle spanned by the zone */
    GRect rect;
    /** Position of the zone text in string #textUTF8#. */
    int text_start;
    /** Length of the zone text in string #textUTF8#. */
    int text_length;
    /** List of children zone. */
    GList<Zone> children;
    /** Appends another subzone inside this zone.  The new zone is initialized
        with an empty rectangle, empty text, and has the same type as this
        zone. */
    Zone *append_child();
    /** Find the text_start and text_end indicated by the given box. */
    void get_text_with_rect(const GRect &box, 
                            int &string_start,int &string_end ) const;
    /** Find the zones used by the specified string and append them to the list. */
    void find_zones(GList<Zone *> &list, 
                    const int string_start, const int string_end) const;
    /** Finds the smallest rectangles and appends them to the list. */
    void get_smallest(GList<GRect> &list) const;
    /** Finds the smallest rectangles and appends them to the list after 
        padding the smallest unit to fit width or height for the parent rectangle
        and adding the number of specified pixels. */
    void get_smallest(GList<GRect> &list,const int padding) const;
    /// Find out this Zone's parent.
    const Zone *get_parent(void) const;
  private:
    friend class DjVuTXT;
    const Zone *zone_parent;
    void cleartext();
    void normtext(const char *instr, GUTF8String &outstr);
    unsigned int memuse() const;
    static const int version;
    void encode(const GP<ByteStream> &bs, 
                const Zone * parent=0, const Zone * prev=0) const;
    void decode(const GP<ByteStream> &bs, int maxtext,
                const Zone * parent=0, const Zone * prev=0);
  };
  /** Textual data for this page.  
      The content of this string is encoded using the UTF8 code.
      This code corresponds to ASCII for the first 127 characters.
      Columns, regions, paragraph and lines are delimited by the following
      control character:
      \begin{tabular}{lll}
        {\bf Name} & {\bf Octal} & {\bf Ascii name} \\\hline\\
        {\tt DjVuText::end_of_column}    & 013 & VT, Vertical Tab \\
        {\tt DjVuText::end_of_region}    & 035 & GS, Group Separator \\
        {\tt DjVuText::end_of_paragraph} & 037 & US, Unit Separator \\
        {\tt DjVuText::end_of_line}      & 012 & LF: Line Feed
      \end{tabular} */
  GUTF8String textUTF8;
  static const char end_of_column    ;      // VT: Vertical Tab
  static const char end_of_region    ;      // GS: Group Separator
  static const char end_of_paragraph ;      // US: Unit Separator
  static const char end_of_line      ;      // LF: Line Feed
  /** Main zone in the document.
      This zone represent the page. */
  Zone page_zone;
  /** Tests whether there is a meaningful zone hierarchy. */
  int has_valid_zones() const;
  /** Normalize textual data.  Assuming that a zone hierarchy has been built
      and represents the reading order.  This function reorganizes the string
      #textUTF8# by gathering the highest level text available in the zone
      hierarchy.  The text offsets and lengths are recomputed for all the
      zones in the hierarchy. Separators are inserted where appropriate. */
  void normalize_text();
  /** Encode data for a TXT chunk. */
  void encode(const GP<ByteStream> &bs) const;
  /** Decode data from a TXT chunk. */
  void decode(const GP<ByteStream> &bs);
  /** Returns a copy of this object. */
  GP<DjVuTXT> copy(void) const;
  /// Write XML formated text.
  void writeText(ByteStream &bs,const int height) const;
  /// Get XML formatted text.
  GUTF8String get_xmlText(const int height) const;
  /** Find the text specified by the rectangle. */  
  GList<Zone*> find_text_in_rect(GRect target_rect, GUTF8String &text) const;
  /** Find the text specified by the rectangle. */
  GList<GRect> find_text_with_rect(const GRect &box, GUTF8String &text, const int padding=0) const;
  /** Get all zones of zone type zone_type under node parent. 
      zone_list contains the return value. */
  void get_zones(int zone_type, const Zone *parent, GList<Zone *> & zone_list) const;
  /** Returns the number of bytes needed by this data structure. It's
      used by caching routines to estimate the size of a \Ref{DjVuImage}. */
  unsigned int get_memory_usage() const;
};

inline const DjVuTXT::Zone *
DjVuTXT::Zone::get_parent(void) const
{
  return zone_parent;
}


class DJVUAPI DjVuText : public GPEnabled
{
protected:
   DjVuText(void) {}
public:
   /// Default creator.
   static GP<DjVuText> create(void) {return new DjVuText();}

      /** Decodes a sequence of annotation chunks and merges contents of every
	  chunk with previously decoded information. This function
	  should be called right after applying \Ref{IFFByteStream::get_chunk}()
	  to data from #FORM:TEXT#. */
   void decode(const GP<ByteStream> &bs);

      /** Encodes all annotations back into a sequence of chunks to be put
	  inside a #FORM:TEXT#. */
   void	encode(const GP<ByteStream> &bs);

      /// Returns a copy of this object
   GP<DjVuText>	copy(void) const;

      /** Returns the number of bytes needed by this data structure. It's
	  used by caching routines to estimate the size of a \Ref{DjVuImage}. */
   inline unsigned int get_memory_usage() const;

   /// Write XML formated text.
   void writeText(ByteStream &bs,const int height) const;

   /// Get XML formatted text.
   GUTF8String get_xmlText(const int height) const;

   GP<DjVuTXT>  txt;
private: // dummy stuff
   static void decode(ByteStream *);
   static void	encode(ByteStream *);
};

//@}

inline unsigned int
DjVuText::get_memory_usage() const
{
  return (txt)?(txt->get_memory_usage()):0;
}


// ----- THE END

#ifdef HAVE_NAMESPACES
}
# ifndef NOT_USING_DJVU_NAMESPACE
using namespace DJVU;
# endif
#endif
#endif


