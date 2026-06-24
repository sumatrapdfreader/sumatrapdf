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

#ifndef _DJVMDIR_H
#define _DJVMDIR_H
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#if NEED_GNUG_PRAGMAS
# pragma interface
#endif


/** @name DjVmDir.h
    Files #"DjVmDir.h"# and #"DjVmDir.cpp"# implement class \Ref{DjVmDir} for
    representing the directory of a DjVu multipage document.

    {\bf Bundled vs. Indirect format} --- There are currently two multipage
    DjVu formats supported: {\em bundled} and {\em indirect}.  In the first
    format all component files composing a given document are packaged (or
    bundled) into one file, in the second one every page and component is
    stored in a separate file and there is one more file, which contains the
    list of all others.

    {\bf Multipage DjVu format} --- Multipage DjVu documents follow the EA
    IFF85 format (cf. \Ref{IFFByteStream.h}.)  A document is composed of a
    #"FORM:DJVM"# whose first chunk is a #"DIRM"# chunk containing the {\em
    document directory}.  This directory lists all component files composing
    the given document, helps to access every component file and identify the
    pages of the document.
    \begin{itemize} 
    \item In a {\em bundled} multipage file, the component files 
         are stored immediately after the #"DIRM"# chunk,
         within the #"FORM:DJVU"# composite chunk.  
    \item In an {\em indirect} multipage file, the component files are 
          stored in different files whose URLs are composed using information 
          stored in the #"DIRM"# chunk.
    \end{itemize} 
    Most of the component files represent pages of a document.  Some files
    however represent data shared by several pages.  The pages refer to these
    supporting files by means of an inclusion chunk (#"INCL"# chunks)
    identifying the supporting file.

    {\bf Document Directory} --- Every directory record describes a component
    file.  Each component file is identified by a small string named the
    identifier (ID).  Each component file also contains a file name and a
    title.  The format of the #"DIRM"# chunk is described in section
    \Ref{Format of the DIRM chunk.}.

    Theoretically, IDs are used to uniquely identify each component file in
    #"INCL"# chunks, names are used to compose the the URLs of the component
    files in an indirect multipage DjVu file, and titles are cosmetic names
    possibly displayed when viewing a page of a document.  There are however
    many problems with this scheme, and we {\em strongly suggest}, with the
    current implementation to always make the file ID, the file name and the
    file title identical.

    @memo Implements DjVu multipage document directory
    @author Andrei Erofeev <eaf@geocities.com>
*/
//@{



#include "GString.h"
#include "GThreads.h"

#ifdef HAVE_NAMESPACES
namespace DJVU {
# ifdef NOT_DEFINED // Just to fool emacs c++ mode
}
#endif
#endif

class ByteStream;

/** Implements DjVu multipage document directory.  There are currently two
    multipage DjVu formats supported: {\em bundled} and {\em indirect}.  In
    the first format all component files composing a given document are
    packaged (or bundled) into one file, in the second one every page and
    component is stored in a separate file and there is one more file, which
    contains the list of all others.

    The multipage document directory lists all component files composing the
    given document, helps to access every file, identify pages and maintain
    user-specified shortcuts.  Every directory record describes a file
    composing the document.  Each file is identified by a small string named
    the identifier (ID).  Each file may also contain a file name and a title.

    The #DjVmDir# class represents a multipage document directory.  Its main
    purpose is to encode and decode the document directory when writing or
    reading the #DIRM# chunk.  Normally you don't have to create this class
    yourself. It's done automatically when \Ref{DjVmDoc} class initializes
    itself. It may be useful though to be able to access records in the
    directory because some classes (like \Ref{DjVuDocument} and \Ref{DjVmDoc})
    return a pointer to #DjVmDir# in some cases. */

class DJVUAPI DjVmDir : public GPEnabled
{
protected:
      /** Class \Ref{DjVmDir::File} represents the directory records
          managed by class \Ref{DjVmDir}. */
   DjVmDir(void) { } ;
public:
   class File;

   static const int version;

      /** Class \Ref{DjVmDir::File} represents the directory records
          managed by class \Ref{DjVmDir}. */
   static GP<DjVmDir> create(void) {return new DjVmDir; } ;

      /** Decodes the directory from the specified stream. */
   void decode(const GP<ByteStream> &stream);
      /** Encodes the directory into the specified stream. */
   void encode(const GP<ByteStream> &stream, const bool do_rename=false) const;
      /** Encodes the directory into the specified stream, 
          explicitely as bundled or indirect. */
  void encode(const GP<ByteStream> &stream, 
              const bool bundled, const bool do_rename) const;
      /** Tests if directory defines an {\em indirect} document. */
   inline bool is_indirect(void) const;
      /** Tests if the directory defines a {\em bundled} document. */
   inline bool is_bundled(void) const;
      /** Translates page numbers to file records. */
   GP<File> page_to_file(int page_num) const;
      /** Translates file names to file records. */
   GP<File> name_to_file(const GUTF8String & name) const;
      /** Translates file IDs to file records. */
   GP<File> id_to_file(const GUTF8String &id) const;
      /** Translates file shortcuts to file records. */
   GP<File> title_to_file(const GUTF8String &title, GPosition spos) const;
   GP<File> title_to_file(const GUTF8String &title) const; 
      /** Access file record by position. */
   GP<File> pos_to_file(int fileno, int *ppageno=0) const;
      /** Returns position of the file in the directory. */
   int get_file_pos(const File * f) const;
      /** Returns position of the given page in the directory. */
   int get_page_pos(int page_num) const;
      /** Check for duplicate names, and resolve them. */
   GPList<File> resolve_duplicates(const bool save_as_bundled);
      /** Returns a copy of the list of file records. */
   GPList<File> get_files_list(void) const;
      /** Returns the number of file records. */
   int get_files_num(void) const;
      /** Returns the number of file records representing pages. */
   int get_pages_num(void) const;
      /** Returns back pointer to the file with #SHARED_ANNO# flag.
        Note that there may be only one file with shared annotations
        in any multipage DjVu document. */
   GP<File> get_shared_anno_file(void) const;
      /** Changes the title of the file with ID #id#. */
   void set_file_title(const GUTF8String &id, const GUTF8String &title);
      /** Changes the name of the file with ID #id#. */
   void set_file_name(const GUTF8String &id, const GUTF8String &name);
      /** Inserts the specified file record at the specified position.
        Specifying #pos# equal to #-1# means to append.  The actual position
        inserted is returned. */
   int insert_file(const GP<File> & file, int pos=-1);
      /** Removes a file record with ID #id#. */
   void delete_file(const GUTF8String &id);
private:
   GCriticalSection class_lock;
   GPList<File>	files_list;
   GPArray<File> page2file;
   GPMap<GUTF8String, File> name2file;
   GPMap<GUTF8String, File> id2file;
private: //dummy stuff
   static void decode(ByteStream *);
   static void encode(ByteStream *);
};

class DJVUAPI DjVmDir::File : public GPEnabled
{
public:
  // Out of the record: INCLUDE below must be zero and PAGE must be one.
  // This is to avoid problems with the File constructor, which now takes
  // 'int file_type' as the last argument instead of 'bool is_page'
  
  /** File type. Possible file types are:
     \begin{description}
       \item[PAGE] This is a top level page file. It may include other
         #INCLUDE#d files, which may in turn be shared between
         different pages.
       \item[INCLUDE] This file is included into some other file inside
         this document.
       \item[THUMBNAILS] This file contains thumbnails for the document
         pages.
       \item[SHARED_ANNO] This file contains annotations shared by
         all the pages. It's supposed to be included into every page
         for the annotations to take effect. There may be only one
         file with shared annotations in a document.
     \end{description} */
  enum FILE_TYPE { INCLUDE=0, PAGE=1, THUMBNAILS=2, SHARED_ANNO=3 };
protected:
  /** Default constructor. */
  File(void);

public:
  static GP<File> create(void) { return new File(); }
  static GP<File> create(const GUTF8String &load_name,
     const GUTF8String &save_name, const GUTF8String &title,
     const FILE_TYPE file_type);

  /** Check for filenames that are not valid for the native encoding,
      and change them. */
  const GUTF8String &check_save_name(const bool as_bundled);

  /** File name.  The optional file name must be unique and is the name
      that will be used when the document is saved to an indirect file.
      If not assigned, the value of #id# will be used for this purpose.
      By keeping the name in {\em bundled} document we guarantee, that it
      can be expanded later into {\em indirect} document and files will
      still have the same names, if the name is legal on a given filesystem.
    */
  const GUTF8String &get_save_name(void) const;

  /** File identifier.  The encoder assigns a unique identifier to each file
      in a multipage document. This is the name used when loading files.
      Indirection chunks in other files (#"INCL"# chunks) may refer to another
      file using its identifier. */
  const GUTF8String &get_load_name(void) const;
  void set_load_name(const GUTF8String &id);

  /** File title.  The file title is assigned by the user and may be used as
      a shortcut for viewing a particular page.  Names like #"chapter1"# or
      #"appendix"# are appropriate. */
  const GUTF8String &get_title() const;
  void set_title(const GUTF8String &id);

  /** Reports an ascii string indicating file type. */
  GUTF8String get_str_type(void) const;

  /** Offset of the file data in a bundled DJVM file.  This number is
      relevant in the {\em bundled} case only when everything is packed into
      one single file. */
  int offset;

  /** Size of the file data in a bundled DJVM file.  This number is
      relevant in the {\em bundled} case only when everything is
      packed into one single file. */
  int size;

  /** Have we checked the saved file name, to see if it is valid on the
      local disk? */
  bool valid_name;

  /** Tests if this file represents a page of the document. */
  bool is_page(void) const 
  {
    return (flags & TYPE_MASK)==PAGE;
  }

  /** Returns #TRUE# if this file is included into some other files of
      this document. */
  bool is_include(void) const
  {
    return (flags & TYPE_MASK)==INCLUDE;
  }

  /** Returns #TRUE# if this file contains thumbnails for the document pages. */
  bool is_thumbnails(void) const
  {
    return (flags & TYPE_MASK)==THUMBNAILS;
  }

  /** Returns the page number of this file. This function returns
      #-1# if this file does not represent a page of the document. */
  bool is_shared_anno(void) const
  { return (flags & TYPE_MASK)==SHARED_ANNO; }

  int get_page_num(void) const 
  { return page_num; } 
protected:
  GUTF8String name;
  GUTF8String oldname;
  GUTF8String id;
  GUTF8String title; 
  void set_save_name(const GUTF8String &name);
private:
      friend class DjVmDir;
      enum FLAGS_0 { IS_PAGE_0=1, HAS_NAME_0=2, HAS_TITLE_0=4 };
      enum FLAGS_1 { HAS_NAME=0x80, HAS_TITLE=0x40, TYPE_MASK=0x3f };
      unsigned char flags;
      int page_num;
};

inline const GUTF8String &
DjVmDir::File::get_load_name(void) const
{ return id; }

inline const GUTF8String &
DjVmDir::File::get_title() const
{ return *(title.length()?&title:&id); }

inline void
DjVmDir::File::set_title(const GUTF8String &xtitle) { title=xtitle; }

/** @name Format of the DIRM chunk.

    {\bf Variants} --- There are two versions of the #"DIRM"# chunk format.
    The version number is identified by the seven low bits of the first byte
    of the chunk.  Version {\bf 0} is obsolete and should never be used.  This
    section describes version {\bf 1}.  There are two major multipage DjVu
    formats supported: {\em bundled} and {\em indirect}.  The #"DIRM"# chunk
    indicates which format is used in the most significant bit of the first
    byte of the chunk.  The document is bundled when this bit is set.
    Otherwise the document is indirect.

    {\bf Unencoded data} --- The #"DIRM"# chunk is composed some unencoded
    data followed by \Ref{bzz} encoded data.  The unencoded data starts with
    the version byte and a 16 bit integer representing the number of component
    files.  All integers are encoded with the most significant byte first.
    \begin{verbatim}
          BYTE:             Flags/Version:  0x<bundled>0000011
          INT16:            Number of component files.
    \end{verbatim}
    When the document is a bundled document (i.e. the flag #bundled# is set),
    this header is followed by the offsets of each of the component files within
    the #"FORM:DJVM"#.  These offsets allow for random component file access.
    \begin{verbatim}
          INT32:            Offset of first component file.
          INT32:            Offset of second component file.
          ...
          INT32:            Offset of last component file.
    \end{verbatim}

    {\bf BZZ encoded data} --- The rest of the chunk is entirely compressed
    with the BZZ general purpose compressor.  We describe now the data fed
    into (or retrieved from) the BZZ codec (cf. \Ref{BSByteStream}.)  First
    come the sizes and the flags associated with each component file.
    \begin{verbatim}
          INT24:             Size of the first component file.
          INT24:             Size of the second component file.
          ...
          INT24:             Size of the last component file.
          BYTE:              Flag byte for the first component file.
          BYTE:              Flag byte for the second component file.
          ...
          BYTE:              Flag byte for the last component file.
    \end{verbatim}
    The flag bytes have the following format:
    \begin{verbatim}
          0b<hasname><hastitle>000000     for a file included by other files.
          0b<hasname><hastitle>000001     for a file representing a page.
          0b<hasname><hastitle>000010     for a file containing thumbnails.
    \end{verbatim}
    Flag #hasname# is set when the name of the file is different from the file
    ID.  Flag #hastitle# is set when the title of the file is different from
    the file ID.  These flags are used to avoid encoding the same string three
    times.  Then come a sequence of zero terminated strings.  There are one to
    three such strings per component file.  The first string contains the ID
    of the component file.  The second string contains the name of the
    component file.  It is only present when the flag #hasname# is set. The third
    one contains the title of the component file. It is only present when the
    flag #hastitle# is set. The \Ref{bzz} encoding system makes sure that 
    all these strings will be encoded efficiently despite their possible
    redundancies.
    \begin{verbatim}
          ZSTR:     ID of the first component file.
          ZSTR:     Name of the first component file (only if #hasname# is set.)
          ZSTR:     Title of the first component file (only if #hastitle# is set.)
          ... 
          ZSTR:     ID of the last component file.
          ZSTR:     Name of the last component file (only if #hasname# is set.)
          ZSTR:     Title of the last component file (only if #hastitle# is set.)
    \end{verbatim}

    @memo Description of the format of the DIRM chunk.  */
//@}



// -------------- IMPLEMENTATION


inline bool
DjVmDir::is_bundled(void) const
{
  return ! is_indirect();
}

inline bool
DjVmDir::is_indirect(void) const
{
  GCriticalSectionLock lock((GCriticalSection *) &class_lock);
  return ( files_list.size() && files_list[files_list] != 0 &&
           files_list[files_list]->offset==0 );
}

inline GP<DjVmDir::File> 
DjVmDir::title_to_file(const GUTF8String &title) const
{
  GPosition pos;
  return title_to_file(title, pos);
}



// ----- THE END

#ifdef HAVE_NAMESPACES
}
# ifndef NOT_USING_DJVU_NAMESPACE
using namespace DJVU;
# endif
#endif
#endif
