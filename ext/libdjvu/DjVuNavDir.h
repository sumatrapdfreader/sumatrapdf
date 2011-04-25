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

#ifndef _DJVUNAVDIR_H
#define _DJVUNAVDIR_H
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#if NEED_GNUG_PRAGMAS
# pragma interface
#endif


#include "GString.h"
#include "GThreads.h"
#include "GURL.h"

#ifdef HAVE_NAMESPACES
namespace DJVU {
# ifdef NOT_DEFINED // Just to fool emacs c++ mode
}
#endif
#endif

class ByteStream;

/** @name DjVuNavDir.h
    Files #"DjVuNavDir.h"# and #"DjVuNavDir.cpp"# contain implementation of the
    multipage DjVu navigation directory. This directory lists all the pages,
    that a given document is composed of. The navigation (switching from page
    to page in the plugin) is not possible before this directory is decoded.

    Refer to the \Ref{DjVuNavDir} class description for greater details.

    @memo DjVu Navigation Directory
    @author Andrei Erofeev <eaf@geocities.com>
*/

//@{

//*****************************************************************************
//********************* Note: this class is thread-safe ***********************
//*****************************************************************************

/** DjVu Navigation Directory.

    This class implements the {\em navigation directory} of a multipage
    DjVu document - basically a list of pages that this document is composed
    of. We would like to emphasize, that this is the list of namely
    {\bf pages}, not {\bf files}. Any page may include any
    number of additional files. When you've got an all-in-one-file multipage
    DjVu document (DjVm archive) you may get the files list from \Ref{DjVmDir0}
    class.

    The \Ref{DjVuNavDir} class can decode and encode the navigation directory
    from {\bf NDIR} IFF chunk. It's normally created by the library during
    decoding procedure and can be accessed like any other component of
    the \Ref{DjVuImage} being decoded.
    
    In a typical multipage DjVu document the navigation directory is stored
    in a separate IFF file containing only one chunk: {\bf NDIR} chunk.
    This file should be included (by means of the {\bf INCL} chunk) into
    every page of the document to enable the navigation. */
class DjVuNavDir : public GPEnabled
{
private:
   GCriticalSection		lock;
   GURL				baseURL;
   GArray<GUTF8String>		page2name;
   GMap<GUTF8String, int>		name2page;
   GMap<GURL, int>		url2page;
protected:
   DjVuNavDir(const GURL &dir_url);
   DjVuNavDir(ByteStream & str, const GURL &dir_url);

public:
   int		get_memory_usage(void) const { return 1024; };

      /** Creates a #DjVuNavDir# object. #dir_url# is the URL of the file
	  containing the directory source data. It will be used later
	  in translation by functions like \Ref{url_to_page}() and
	  \Ref{page_to_url}() */
   static GP<DjVuNavDir> create(const GURL &dir_url)
   {return new DjVuNavDir(dir_url);}

      /** Creates #DjVuNavDir# object by decoding its contents from
	  the stream. #dir_url# is the URL of the file containing the
	  directory source data. */
   static GP<DjVuNavDir> create(ByteStream & str, const GURL &dir_url)
   { return new DjVuNavDir(str,dir_url); }

   virtual ~DjVuNavDir(void) {};

      /// Decodes the directory contents from the given \Ref{ByteStream}
   void		decode(ByteStream & str);

      /// Encodes the directory contents into the given \Ref{ByteStream}
   void		encode(ByteStream & str);

      /** Inserts a new page at position #where# pointing to a file
	  with name #name#.

	  @param where The position where the page should be inserted.
	  	 #-1# means to append.
	  @param name The name of the file corresponding to this page.
	  	 The name may not contain slashes. The file may include
		 other files. */
   void		insert_page(int where, const char * name);

      /// Deletes page with number #page_num# from the directory.
   void		delete_page(int page_num);

      /// Returns the number of pages in the directory.
   int		get_pages_num(void) const;
      /** Converts the #url# to page number. Returns #-1# if the #url#
	  does not correspond to anything in the directory. */
   int		url_to_page(const GURL & url) const;
      /** Converts file name #name# to page number. Returns #-1# if file
	  with given name cannot be found. */
   int		name_to_page(const char * name) const;
      /** Converts given #page# to URL. Throws an exception if page number
	  is invalid. */
   GURL		page_to_url(int page) const;
      /** Converts given #page# to URL. Throws an exception if page number
	  is invalid. */
   GUTF8String	page_to_name(int page) const;
};

//@}


#ifdef HAVE_NAMESPACES
}
# ifndef NOT_USING_DJVU_NAMESPACE
using namespace DJVU;
# endif
#endif
#endif
