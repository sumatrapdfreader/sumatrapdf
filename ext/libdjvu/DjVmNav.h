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

#ifndef _DJVMNAV_H
#define _DJVMNAV_H
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#if NEED_GNUG_PRAGMAS
# pragma interface
#endif

#include "DjVuGlobal.h"
#include "GString.h"
#include "GThreads.h"

#ifdef HAVE_NAMESPACES
namespace DJVU {
# ifdef NOT_DEFINED // Just to fool emacs c++ mode
}
#endif
#endif


class ByteStream;

/** The NAVM chunk.
    The optional #"NAVM"# chunk which follows the DIRM chunk describes
    how the user can navigate the document.
    This is a list of DjVuBookMarks.
**/

class DJVUAPI DjVmNav : public GPEnabled
{
public:
   /** Class \Ref{DjVmNav::DjVuBookMark} represents a entry in the 
       hierarchy of contents. */
   class DjVuBookMark;
 
   static GP<DjVmNav> create(void);
      /** Decodes the directory from the specified stream. */
   void decode(const GP<ByteStream> &stream);
      /** Encodes the directory into the specified stream. */
   void encode(const GP<ByteStream> &stream) ;
   void dump(const GP<ByteStream> &stream) ;
      /** Return bookmark at zero-based position i */
   bool getBookMark(GP<DjVuBookMark> &gpBookMark, int i) ;
   int getBookMarkCount();
      /** Append the BookMark to the end of the list */
   void append (const GP<DjVuBookMark> &gpBookMark) ;
      /** This function will check the given bookmark is valid or not */
   bool isValidBookmark();
      /** This function determines if the given count_array is a tree
          sequence, that is if it fits a tree. */
   int get_tree(int index, int* count_array, int count_array_size);
protected:
   DjVmNav(void) { } ;
private:
   GCriticalSection class_lock;
   GPList<DjVuBookMark>	bookmark_list;
};

/** The DjVuBookMark.
    Each entry in the Navigation chunk (NAVM) is a bookmark.  A bookmark
    contains a count of immediate children, a display string and a url.
**/

class DJVUAPI DjVmNav::DjVuBookMark : public GPEnabled
{
protected:
  /** Default constructor. */
  DjVuBookMark(void);
public:
  static GP<DjVuBookMark> create(void);
  static GP<DjVuBookMark> create(const unsigned short count,
                                 const GUTF8String &displayname, 
                                 const GUTF8String &url);
  void encode(const GP<ByteStream> &stream);
  void dump(const GP<ByteStream> &stream);
  void decode(const GP<ByteStream> &stream);
  int count;	           // count of immediate children.
  GUTF8String displayname; // example:  "Section 3.5 - Encryption"
  GUTF8String url;	   // url, may be blank or relative.
};


#ifdef HAVE_NAMESPACES
}
# ifndef NOT_USING_DJVU_NAMESPACE
using namespace DJVU;
# endif
#endif
#endif
