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

#ifndef _DJVMDOC_H
#define _DJVMDOC_H
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#if NEED_GNUG_PRAGMAS
# pragma interface
#endif


#include "DjVmDir.h"

#ifdef HAVE_NAMESPACES
namespace DJVU {
# ifdef NOT_DEFINED // Just to fool emacs c++ mode
}
#endif
#endif

class ByteStream;
class DataPool;
class GURL;
class GUTF8String;
class DjVmNav;

/** @name DjVmDoc.h
    Files #"DjVmDoc.h"# and #"DjVmDoc.cpp"# contain implementation of the
    \Ref{DjVmDoc} class used to read and write new DjVu multipage documents.

    @memo DjVu multipage documents reader/writer.
    @author Andrei Erofeev <eaf@geocities.com>
*/

//@{

/** Read/Write DjVu multipage documents.

    The "new" DjVu multipage documents can be of two types: {\em bundled} and
    {\em indirect}. In the first case all pages are packed into one file,
    which is very like an archive internally. In the second case every page
    is stored in a separate file. Plus there can be other components,
    included into one or more pages, which also go into separate files. In
    addition to pages and components, in the case of the {\em indirect} format
    there is one more top-level file with the document directory (see
    \Ref{DjVmDir}), which is basically an index file containing the
    list of all files composing the document.

    This class can read documents of both formats and can save them under any
    format.  It is therefore ideal for converting between {\em bundled} and
    {\em indirect} formats.  It cannot be used however for reading obsolete
    formats.  The best way to convert obsolete formats consists in reading
    them with class \Ref{DjVuDocument} class and saving them using
    \Ref{DjVuDocument::write} or \Ref{DjVuDocument::expand}.

    This class can also be used to create and modify multipage documents at
    the low level without decoding every page or component (See
    \Ref{insert_file}() and \Ref{delete_file}()). 
*/

class DJVUAPI DjVmDoc : public GPEnabled
{
      // Internal function.
protected:   
  DjVmDoc(void);
  void init(void);
public:
      /// Creator
   static GP<DjVmDoc> create(void);
      /** Inserts a file into the document.
          @param data  ByteStream containing the file data.
          @param file_type Describes the type of the file to be inserted.
	  	 See \Ref{DjVmDir::File} for details.
          @param name  Name of the file in the document (e.g. an URL).
          @param id    Identifier of the file (as used in INCL chunks).
          @param title Optional title of the file (shown in browsers).
          @param pos   Position of the file in the document (default is append).
      */
   void	insert_file(
     ByteStream &data, DjVmDir::File::FILE_TYPE file_type,
     const GUTF8String &name, const GUTF8String &id,
     const GUTF8String &title=GUTF8String(), int pos=-1 );
      /** Inserts a file into the document.
          @param pool  Data pool containing file data.
          @param file_type Describes the type of the file to be inserted.
	  	 See \Ref{DjVmDir::File} for details.
          @param name  Name of the file in the document (e.g. an URL).
          @param id    Identifier of the file (as used in INCL chunks).
          @param title Optional title of the file (shown in browsers).
          @param pos   Position of the file in the document (default is append).
      */
   void	insert_file(
     const GP<DataPool> &pool, DjVmDir::File::FILE_TYPE file_type,
     const GUTF8String &name, const GUTF8String &id,
     const GUTF8String &title=GUTF8String(), int pos=-1 );

      /** Inserts a file described by \Ref{DjVmDir::File} structure with
	  data #data# at position #pos#. If #pos# is negative, the file
          will be appended to the document. Otherwise it will be inserted
          at position #pos#. */
   void	insert_file(const GP<DjVmDir::File> & f,
                    GP<DataPool> data, int pos=-1);

      /** Removes file with the specified #id# from the document. Every
	  file inside a new DjVu multipage document has its unique ID
	  (refer to \Ref{DjVmDir} for details), which is passed to this
          function. */
   void	delete_file(const GUTF8String &id);

     /** Set the bookmarks */
   void set_djvm_nav(GP<DjVmNav> n);

      /** Returns the directory of the DjVm document (the one which will
	  be encoded into #DJVM# chunk of the top-level file or the bundle). */
   GP<DjVmDir>	get_djvm_dir(void);
   
      /** Returns contents of file with ID #id# from the document.
	  Please refer to \Ref{DjVmDir} for the explanation of what
          IDs mean. */
   GP<DataPool>	get_data(const GUTF8String &id) const;

      /** Reading routines */
      //@{
      /** Reads contents of a {\em bundled} multipage DjVu document from
	  the stream. */
   void	read(ByteStream & str);
      /** Reads contents of a {\em bundled} multipage DjVu document from
	  the \Ref{DataPool}. */
   void	read(const GP<DataPool> & data_pool);
      /** Reads the DjVu multipage document in either {\em bundled} or
	  {\em indirect} format.

	  {\bf Note:} For {\em bundled} documents the file is not
	  read into memory. We just open it and access data directly there.
	  Thus you should not modify the file contents.

	  @param name For {\em bundled} documents this is the name
	         of the document. For {\em indirect} documents this is
		 the name of the top-level file of the document (containing
		 the \Ref{DjVmDir} with the list of all files).
		 The rest of the files are expected to be in the
		 same directory and will be read by this function as well. */
   void	read(const GURL &url);
      //@}

      /** Writing routines */
      //@{
      /** Writes the multipage DjVu document in the {\em bundled} format into
	  the stream. */
   void	write(const GP<ByteStream> &str);
      /** Writes the multipage DjVu document in the {\em bundled} format into
	  the stream, reserving any of the specified names. */
   void	write(const GP<ByteStream> &str,
              const GMap<GUTF8String,void *>& reserved);
      /** Stored index (top-level) file of the DjVu document in the {\em
	  indirect} format into the specified stream. */
   void	write_index(const GP<ByteStream> &str);
      /** Writes the multipage DjVu document in the {\em indirect} format
	  into the given directory. Every page and included file will be
          stored as a separate file. Besides, one top-level file with
          the document directory (named #idx_name#) will be created unless
	  #idx_name# is empty.

          @param dir_name Name of the directory where files should be
		 created
	  @param idx_name Name of the top-level file with the \Ref{DjVmDir}
		 with the list of files composing the given document.
		 If empty, the file will not be created. */
   void	expand(const GURL &codebase, const GUTF8String &idx_name);

      /** Writes an individual file, and all included files. 
          INCL chunks will be remapped as appropriate. */
   void save_page(const GURL &codebase, const DjVmDir::File &file) const;

      /** Writes an individual file if not mapped, and all included files. 
          INCL chunks will be remapped as appropriate.  All pages saved
          are added to the #incl# map. */
   void save_page(const GURL &codebase, const DjVmDir::File &file,
                  GMap<GUTF8String,GUTF8String> &incl) const;

      /** Writes an individual file specified, remapping INCL chunks as
          appropriate.  Included files will not be saved. */
   void save_file(const GURL &codebase, const DjVmDir::File &file) const;

      /** Writes the specified file from the given #pool#. */
   GUTF8String save_file(const GURL &codebase, const DjVmDir::File &file,
                         GMap<GUTF8String,GUTF8String> &incl, 
                         const GP<DataPool> &pool) const;
  //@}
private:
   void save_file(const GURL &codebase, const DjVmDir::File &file,
                  GMap<GUTF8String,GUTF8String> *incl) const;
   GP<DjVmDir> dir;
   GP<DjVmNav> nav;
   GPMap<GUTF8String, DataPool > data;
private: // dummy stuff
   static void write(ByteStream *);
   static void write_index(ByteStream *);
};

inline GP<DjVmDir>
DjVmDoc::get_djvm_dir(void)
{
   return dir;
}


//@}



#ifdef HAVE_NAMESPACES
}
# ifndef NOT_USING_DJVU_NAMESPACE
using namespace DJVU;
# endif
#endif
#endif
