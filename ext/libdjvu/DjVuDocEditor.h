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

#ifndef _DJVUDOCEDITOR_H
#define _DJVUDOCEDITOR_H
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#if NEED_GNUG_PRAGMAS
# pragma interface
#endif


#include "DjVuDocument.h"
#include "DjVmDoc.h"

#ifdef HAVE_NAMESPACES
namespace DJVU {
# ifdef NOT_DEFINED // Just to fool emacs c++ mode
}
#endif
#endif

/** @name DjVuDocEditor.h
    Files #"DjVuDocEditor.h"# and #"DjVuDocEditor.cpp"# contain extension
    of \Ref{DjVuDocument} class, which can create and modify existing
    DjVu document, generate thumbnails, etc. It does {\bf not} do
    compression though.

    @memo DjVu document editor class.
    @author Andrei Erofeev <eaf@geocities.com>
*/

//@{

/** #DjVuDocEditor# is an extension of \Ref{DjVuDocument} class with
    additional capabilities for editing the document contents.

    It can be used to:
    \begin{enumerate}
       \item Create (compose) new multipage DjVu documents using single
             page DjVu documents. The class does {\bf not} do compression.
       \item Insert and remove different pages of multipage DjVu documents.
       \item Change attributes ({\em names}, {\em IDs} and {\em titles})
             of files composing the DjVu document.
       \item Generate thumbnail images and integrate them into the document.
    \end{enumerate}
*/

class DJVUAPI DjVuDocEditor : public DjVuDocument
{
public:
   static int	thumbnails_per_file;

protected:
      /// Default constructor
   DjVuDocEditor(void);

      /** Initialization function. Initializes an empty document.

	  {\bf Note}: You must call either of the two
	  available \Ref{init}() function before you start doing
	  anything else with the #DjVuDocEditor#. */
   void		init(void);

      /** Initialization function. Opens document with name #filename#.

	  {\bf Note}: You must call either of the two
	  available \Ref{init}() function before you start doing
	  anything else with the #DjVuDocEditor#. */
   void		init(const GURL &url);

public:
     /** Creates a DjVuDocEditor class and initializes with #fname#. */
   static GP<DjVuDocEditor> create_wait(const GURL &url);

     /** Creates a DjVuDocEditor class and initializes an empty document. */
   static GP<DjVuDocEditor> create_wait(void);


      /// Destructor
   virtual ~DjVuDocEditor(void);

      /** Returns type of open document. #DjVuDocEditor# silently
	  converts any open DjVu document to #BUNDLED# format (see
	  \Ref{DjVuDocument}. Thus, \Ref{DjVuDocument::get_doc_type}()
	  will always be returning #BUNDLED#. Use this function to
	  learn the original format of the document being edited. */
   int		get_orig_doc_type(void) const;

      /** Returns #TRUE# if the document can be "saved" (sometimes
	  the only possibility is to do a "save as"). The reason why
	  we have this function is that #DjVuDocEditor# can save
	  documents in new formats only (#BUNDLED# and #INDIRECT#).
	  At the same time it recognizes all DjVu formats (#OLD_BUNDLED#,
	  #OLD_INDEXED#, #BUNDLED#, and #INDIRECT#).

	  #OLD_BUNDLED# and #BUNDLED# documents occupy only one file,
	  so in this case "saving" involves the automatic conversion
	  to #BUNDLED# format and storing data into the same file.

	  #OLD_INDEXED# documents, on the other hand, occupy more
	  than one file. They could be converted to #INDIRECT# format
	  if these two formats had the same set of files. Unfortunately,
	  these formats are too different, and the best thing to do
	  is to use "save as" capability. */
   bool		can_be_saved(void) const;

      /** Returns type of the document, which can be created by
	  \Ref{save}() function. Can be #INDIRECT#, #BUNDLED#,
	  #SINGLE_PAGE#, or #UNKNOWN_TYPE#. The latter indicates,
	  that \Ref{save}() will fail, and that \Ref{save_as}()
	  should be used instead */
   int		get_save_doc_type(void) const;

      /** Saves the document. May generate exception if the document
	  can not be saved, and \Ref{save_as}() should be used.
	  See \Ref{can_be_saved}() for details. */
   void		save(void);

      /** Saves the document. */
   virtual void	save_as(const GURL &where, bool bundled);

      /** Saves the document in the {\em new bundled} format. All the data
	  is "bundled" into one file and this file is written into the
	  passed stream.

	  If #force_djvm# is #TRUE# then even one page documents will be
	  saved in the #DJVM BUNDLED# format (inside a #FORM:DJVM#);

	  {\bf Plugin Warning}. This function will read contents of the whole
	  document. Thus, if you call it from the main thread (the thread,
	  which transfers data from Netscape), the plugin will block. */
   virtual void	write(const GP<ByteStream> &str, bool force_djvm=false);
     /** Always save as bundled, renaming any files conflicting with the
         the names in the supplied GMap. */
   virtual void	write(const GP<ByteStream> &str,
     const GMap<GUTF8String,void *> &reserved);

      /** Saves the specified pages in DjVu #BUNDLED# multipage document. */
   void		save_pages_as(
     const GP<ByteStream> &str, const GList<int> & page_list);

      /** Translates page number #page_num# to ID. If #page_num# is invalid,
	  an exception is thrown. */
   GUTF8String	page_to_id(int page_num) const;
   
   GUTF8String	insert_file(const GURL &url, const GUTF8String &parent_id,
			    int chunk_num=1, DjVuPort *source=0);
      /** Inserts the referenced file into this DjVu document.

	  @param fname Name of the top-level file containing the image of
	  	 the page to be inserted. This file must be a DjVu file and
		 may include one or more other DjVu files.

		 If it include other DjVu files, the function will try to
		 insert them into the document too. Should this attempt fail,
		 the corresponding #INCL# chunk will be removed from the
		 referencing file and an exception will be thrown.

		 When inserting a file, the function may modify its name
		 to be unique in the DjVu document.
	  @param page_num Position where the new page should be inserted at.
	  	 Negative value means "append" */
   void insert_page(const GURL &fname, int page_num=-1);
   /** Inserts a new page with data inside the #data_pool# as page
       number #page_num.

	  @param data_pool \Ref{DataPool} with data for this page.
	  @param file_name Name, which will be assigned to this page.
	  	 If you try to save the document in #INDIRECT# format,
		 a file with this name will be created to hold the
		 page's data. If there is already a file in the document
		 with the same name, the function will derive a new
		 unique name from file_name, which will be assigned
		 to the page.
	  @param page_num Describes where the page should be inserted.
	  	 Negative number means "append". */
   void		insert_page(GP<DataPool> & file_pool,
			    const GURL &fname, int page_num=-1);
      /** Inserts a group of pages into this DjVu document.
	  
	  Like \Ref{insert_page}() it will insert every page into the document.
	  The main advantage of calling this function once for the whole
	  group instead of calling \Ref{insert_page}() for every page is
	  the processing of included files:

	  The group of files may include one or more files, which are thus
	  shared by them. If you call \Ref{insert_page}() for every page,
	  this shared file will be inserted into the document more than once
	  though under different names. This is how \Ref{insert_page}() works:
	  whenever it inserts something, it checks for duplicate names with
	  only one purpose: invent a new name if a given one is already in
	  use.

	  On the other hand, if you call #insert_group#(), it will insert
	  shared included files only once. This is because it can analyze
	  the group of files before inserting them and figure out what files
	  are shared and thus should be inserted only once.

	  @param fname_list List of top-level files for the pages to be inserted
	  @param page_num Position where the new pages should be inserted at.
	  	 Negative value means "append" */
   void		insert_group(const GList<GURL> & furl_list, int page_num=-1,
			     void (* refresh_cb)(void *)=0, void * cl_data=0);
      /** Removes the specified page from the document. If #remove_unref#
	  is #TRUE#, the function will also remove from the document any file,
	  which became unreferenced due to the page's removal */
   void		remove_page(int page_num, bool remove_unref=true);
      /** Removes the specified pages from the document. If #remove_unref#
	  is #TRUE#, the function will also remove from the document any file,
	  which became unreferenced due to the pages' removal */
   void		remove_pages(const GList<int> & page_list, bool remove_unref=true);
      /** Removes a DjVu file with the specified #id#.

	  If some other files include this file, the corresponding #INCL#
	  chunks will be removed to avoid dead links.

	  If #remove_unref# is #TRUE#, the function will also remove every
	  file, which will become unreferenced after the removal of this file. */
   void		remove_file(const GUTF8String &id, bool remove_unref=true);
      /** Makes page number #page_num# to be #new_page_num#. If #new_page_num#
	  is negative or too big, the function will move page #page_num# to
	  the end of the document. */
   void		move_page(int page_num, int new_page_num);
      /** Shifts all pags from the #page_list# according to the #shift#.
	  The #shift# can be positive (shift toward the end of the document)
	  or negative (shift toward the beginning of the document).

	  It is OK to make #shift# too big in value. Pages will just be
	  moved to the end (or to the beginning, depending on the #shift#
	  sign) of the document. */
   void		move_pages(const GList<int> & page_list, int shift);
   
      /** Changes the name of the file with ID #id# to #name#.
	  Refer to \Ref{DjVmDir} for the explanation of {\em IDs},
          {\em names} and {\em titles}. */
   void		set_file_name(const GUTF8String &id, const GUTF8String &name);
      /** Changes the name of the page #page_num# to #name#.
	  Refer to \Ref{DjVmDir} for the explanation of {\em IDs},
          {\em names} and {\em titles}. */
   void		set_page_name(int page_num, const GUTF8String &name);
      /** Changes the title of the file with ID #id# to #title#.
	  Refer to \Ref{DjVmDir} for the explanation of {\em IDs},
          {\em names} and {\em titles}. */
   void		set_file_title(const GUTF8String &id, const GUTF8String &title);
      /** Changes the title of the page #page_num# to #title#.
	  Refer to \Ref{DjVmDir} for the explanation of {\em IDs},
          {\em names} and {\em titles}. */
   void		set_page_title(int page_num, const GUTF8String &title);

      /** @name Thumbnails */
      //@{
      /** Returns the number of thumbnails stored inside this document.

	  It may be #ZERO#, which means, that there are no thumbnails at all.

	  It may be equal to the number of pages, which is what should
	  normally be.

	  Finally, it may be greater than #ZERO# and less than the number
	  of pages, in which case thumbnails should be regenerated before
	  the document can be saved. */
   int		get_thumbnails_num(void) const;

      /** Returns the size of the first encountered thumbnail image. Since
	  thumbnails can currently be generated by \Ref{generate_thumbnails}()
	  only, all thumbnail images should be of the same size. Thus,
	  the number returned is actually the size of {\em all}
	  document thumbnails.

	  The function will return #-1# if there are no thumbnails. */
   int		get_thumbnails_size(void) const;

      /** Removes all thumbnails from the document */
   void		remove_thumbnails(void);

      /** Generates thumbnails for the specified page, if and only if
          it does not have a thumbnail yet.  If you want to regenerate
          thumbnails for all pages, call \Ref{remove_thumbnails}() prior
          to calling this function.

	  @param thumb_size The size of the thumbnails in pixels. DjVu viewer
	         is able to rescale the thumbnail images if necessary, so this
		 parameter affects thumbnails quality only. 128 is a good number.
	  @param page_num The page number to genate the thumbnail for.  */
   int		generate_thumbnails(int thumb_size, int page_num);

      /** Generates thumbnails for those pages, which do not have them yet.
	  If you want to regenerate thumbnails for all pages, call
	  \Ref{remove_thumbnails}() prior to calling this function.

	  @param thumb_size The size of the thumbnails in pixels. DjVu viewer
	         is able to rescale the thumbnail images if necessary, so this
		 parameter affects thumbnails quality only. 128 is a good number.
	  @param cb The callback, which will be called after thumbnail image
	         for the next page has been generated. Regardless of if
		 the document already has thumbnail images for some of its
		 pages, the callback will be called #pages_num# times, where
		 #pages_num# is the total number of pages in the document.
		 The callback should return #FALSE# if thumbnails generating
		 should proceed. #TRUE# will stop it. */
   void	generate_thumbnails(int thumb_size,
                            bool (* cb)(int page_num, void *)=0,
                            void * cl_data=0);
      //@}
      /** Use this function to simplify annotations in the document.
        The "simplified" format is when annotations are only allowed
        either in top-level page files or in a special file with
        #SHARED_ANNO# flag on. This file is supposed to be included into
        every page. */
   void simplify_anno(void (* progress_cb)(float progress, void *)=0,
                      void * cl_data=0);

      /** Will create a file that will be included into every page and
        marked with the #SHARED_ANNO# flag. This file can be used
        to store global annotations (annotations applicable to every page).

        {\bf Note:} There may be only one #SHARED_ANNO# file in any
        DjVu multipage document. */
   void create_shared_anno_file(void (* progress_cb)(float progress, void *)=0,
                                void * cl_data=0);

      /** Sets bookmark data */
   void set_djvm_nav(GP<DjVmNav> nav);

      /** Returns a pointer to the file with #SHARED_ANNO# flag on.
        This file should be used for storing document-wide annotations.

        {\bf Note:} There may be only one #SHARED_ANNO# file in any
        DjVu multipage document. */
   GP<DjVuFile>       get_shared_anno_file(void);

   GURL               get_doc_url(void) const;
                                                                              
      /** Returns TRUE if #class_name# is #"DjVuDocEditor"#,
	  #"DjVuDocument"# or #"DjVuPort"# */
   virtual bool		inherits(const GUTF8String &class_name) const;
   virtual GP<DataPool>	request_data(const DjVuPort * source, const GURL & url);
protected:
   virtual GP<DjVuFile>	url_to_file(const GURL & url, bool dont_create) const;
   virtual GP<DataPool> get_thumbnail(int page_num, bool dont_decode);
   friend class CThumbNails;
public:
   class File;
private:
   bool		initialized;
   GURL		doc_url;
   GP<DataPool>	doc_pool;
   int		orig_doc_type;
   int		orig_doc_pages;

   GPMap<GUTF8String, File>	files_map; 	// files_map[id]=GP<File>
   GCriticalSection	files_lock;

   GPMap<GUTF8String,DataPool> thumb_map;
   GCriticalSection	thumb_lock;

   void		(* refresh_cb)(void *);
   void		* refresh_cl_data;

   void		check(void);
   GUTF8String	find_unique_id(GUTF8String id);
   GP<DataPool>	strip_incl_chunks(const GP<DataPool> & pool);
   void		clean_files_map(void);
   bool		insert_file_type(const GURL &file_url,
                  DjVmDir::File::FILE_TYPE page_type,
		  int & file_pos,
                  GMap<GUTF8String, GUTF8String> & name2id);
   bool		insert_file( const GP<DataPool> &pool,
                  const GURL &file_url, bool is_page,
		  int & file_pos,
                  GMap<GUTF8String,GUTF8String> & name2id,
                  DjVuPort *source=0 );
   bool		insert_file(
                  const GURL &file_url, bool is_page,
		  int & file_pos,
                  GMap<GUTF8String,GUTF8String> & name2id,
                  DjVuPort *source=0 );
   void		remove_file(const GUTF8String &id, bool remove_unref,
			    GMap<GUTF8String, void *> & ref_map);
   void		generate_ref_map(const GP<DjVuFile> & file,
				 GMap<GUTF8String, void *> & ref_map,
				 GMap<GURL, void *> & visit_map);
   void		move_file(const GUTF8String &id, int & file_pos,
			  GMap<GUTF8String, void *> & map);
   void		unfile_thumbnails(void);
   void		file_thumbnails(void);
   void	save_file(const GUTF8String &id, const GURL &codebase,
     const bool only_modified, GMap<GUTF8String, GUTF8String> & map);
   void	save_file(const GUTF8String &id, const GURL &codebase,
     GMap<GUTF8String, GUTF8String> & map);
};

//@}


#ifdef HAVE_NAMESPACES
}
# ifndef NOT_USING_DJVU_NAMESPACE
using namespace DJVU;
# endif
#endif
#endif

