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

#ifndef _DJVUFILE_H
#define _DJVUFILE_H
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#if NEED_GNUG_PRAGMAS
# pragma interface
#endif


#include "DjVuInfo.h"
#include "DjVuPalette.h"
#include "DjVuPort.h"

#ifdef HAVE_NAMESPACES
namespace DJVU {
# ifdef NOT_DEFINED // Just to fool emacs c++ mode
}
#endif
#endif

class DjVuTXT;
class ByteStream;
class DataPool;
class JB2Image;
class JB2Dict;
class IW44Image;
class IFFByteStream;
class GPixmap;
class DjVuNavDir;


/** @name DjVuFile.h
    Files #"DjVuFile.h"# and #"DjVuFile.cpp"# contain implementation of the
    \Ref{DjVuFile} class, which takes the leading role in decoding of
    \Ref{DjVuImage}s.

    In the previous releases of the library the work of decoding has been
    entirely done in \Ref{DjVuImage}. Now, due to the introduction of multipage
    documents, the decoding procedure became significantly more complex and
    has been moved out from \Ref{DjVuImage} into \Ref{DjVuFile}.

    There is not much point though in creating just \Ref{DjVuFile} alone.
    The maximum power of the decoder is achieved when you create the
    \Ref{DjVuDocument} and work with {\bf it} when decoding the image.

    @memo Classes representing DjVu files.
    @author Andrei Erofeev <eaf@geocities.com>
*/

//@{

/** #DjVuFile# plays the central role in decoding \Ref{DjVuImage}s.
    First of all, it represents a DjVu file whether it's part of a
    multipage all-in-one-file DjVu document, or part of a multipage
    DjVu document where every page is in a separate file, or the whole
    single page document. #DjVuFile# can read its contents from a file
    and store it back when necessary.

    Second, #DjVuFile# does the greatest part of decoding work. In the
    past this was the responsibility of \Ref{DjVuImage}. Now, with the
    introduction of the multipage DjVu formats, the decoding routines
    have been extracted from the \Ref{DjVuImage} and put into this separate
    class #DjVuFile#.

    As \Ref{DjVuImage} before, #DjVuFile# now contains public class
    variables corresponding to every component, that can ever be decoded
    from a DjVu file (such as #INFO# chunk, #BG44# chunk, #SJBZ# chunk, etc.).

    As before, the decoding is initiated by a single function
    (\Ref{start_decode}() in this case, and \Ref{DjVuImage::decode}() before).
    The difference is that #DjVuFile# now handles threads creation itself.
    When you call the \Ref{start_decode}() function, it creates the decoding
    thread, which starts decoding, and which can create additional threads:
    one per each file included into this one.

    {\bf Inclusion} is also a new feature specifically designed for a
    multipage document. Indeed, inside a given document there can be a lot
    of things shared between its pages. Examples can be the document
    annotation (\Ref{DjVuAnno}) and other things like shared shapes and
    dictionary (to be implemented). To avoid putting these chunks into
    every page, we have invented new chunk called #INCL# which purpose is
    to make the decoder open the specified file and decode it.
    
    {\bf Source of data.} The #DjVuFile# can be initialized in two ways:
    \begin{itemize}
       \item With #URL# and \Ref{DjVuPort}. In this case #DjVuFile# will
             request its data thru the communication mechanism provided by
	     \Ref{DjVuPort} in the constructor. If this file references
	     (includes) any other file, data for them will also be requested
	     in the same way.
       \item With \Ref{ByteStream}. In this case the #DjVuFile# will read
             its data directly from the passed stream. This constructor
	     has been added to simplify creation of #DjVuFile#s, which do
	     no include anything else. In this case the \Ref{ByteStream}
	     is enough for the #DjVuFile# to initialize.
    \end{itemize}
	     
    {\bf Progress information.} #DjVuFile# does not do decoding silently.
    Instead, it sends a whole set of notifications through the mechanism
    provided by \Ref{DjVuPort} and \Ref{DjVuPortcaster}. It tells the user
    of the class about the progress of the decoding, about possible errors,
    chunk being decoded, etc. The data is requested using this mechanism too.

    {\bf Creating.} Depending on where you have data of the DjVu file, the
    #DjVuFile# can be initialized in two ways:
    \begin{itemize}
       \item By providing #URL# and pointer to \Ref{DjVuPort}. In this case
             #DjVuFile# will request data using communication mechanism
	     provided by \Ref{DjVuPort}. This is useful when the data is on
	     the web or when this file includes other files.
       \item By providing a \Ref{ByteStream} with the data for the file. Use
             it only when the file doesn't include other files.
    \end{itemize}
    There is also a bunch of functions provided for composing
    the desired \Ref{DjVuDocument} and modifying #DjVuFile# structure. The
    examples are \Ref{delete_chunks}(), \Ref{insert_chunk}(),
    \Ref{include_file}() and \Ref{unlink_file}().

    {\bf Caching.} In the case of plugin it's important to do the caching
    of decoded images or files. #DjVuFile# appears to be the best candidate
    for caching, and that's why it supports this procedure. Whenever a
    #DjVuFile# is successfully decoded, it's added to the cache by
    \Ref{DjVuDocument}. Next time somebody needs it, it will be extracted
    from the cache directly by \Ref{DjVuDocument} and won't be decoded again.

    {\bf URLs.} Historically the biggest strain is put on making the decoder
    available for Netscape and IE plugins where the original files reside
    somewhere in the net. That is why #DjVuFile# uses {\bf URLs} to
    identify itself and other files. If you're working with files on the
    hard disk, you have to use the local URLs instead of file names.
    A good way to do two way conversion is the \Ref{GOS} class. Sometimes it
    happens that a given file does not reside anywhere but the memory. No
    problem in this case either. There is a special port \Ref{DjVuMemoryPort},
    which can associate any URL with the corresponding data in the memory.
    All you need to do is to invent your own URL prefix for this case.
    "#memory:#" will do. The usage of absolute URLs has many advantages among
    which is the capability to cache files with their URL being the cache key.

    Please note, that the #DjVuFile# class has been designed to work closely
    with \Ref{DjVuDocument}. So please review the documentation on this class
    too. */

class DJVUAPI DjVuFile : public DjVuPort
{
public:
   enum { DECODING=1, DECODE_OK=2, DECODE_FAILED=4, DECODE_STOPPED=8,
	  DATA_PRESENT=16, ALL_DATA_PRESENT=32, INCL_FILES_CREATED=64,
          MODIFIED=128, DONT_START_DECODE=256, STOPPED=512,
	  BLOCKED_STOPPED=1024, CAN_COMPRESS=2048, NEEDS_COMPRESSION=4096 };
   enum { STARTED=1, FINISHED=2 };

      /** @name Decoded file contents */
      //@{
      /// Pointer to the DjVu file information component.
   GP<DjVuInfo>		info;
      /// Pointer to the background component of DjVu image (IW44 encoded).
   GP<IW44Image>	bg44;
      /// Pointer to the background component of DjVu image (Raw).
   GP<GPixmap>		bgpm;
      /// Pointer to the mask of foreground component of DjVu image (JB2 encoded).
   GP<JB2Image>		fgjb;
      /// Pointer to the optional shape dictionary for the mask (JB2 encoded).
   GP<JB2Dict>		fgjd;
      /// Pointer to a colors layer for the foreground component of DjVu image.
   GP<GPixmap>		fgpm;
      /// Pointer to a colors vector for the foreground component of DjVu image.
   GP<DjVuPalette>	fgbc;
      /// Pointer to collected annotation chunks.
   GP<ByteStream>	anno;
      /// Pointer to collected hiddentext chunks.
   GP<ByteStream>	text;
      /// Pointer to meta data chunks.
   GP<ByteStream>	meta;
      /// Pointer to the *old* navigation directory contained in this file
   GP<DjVuNavDir>	dir;
      /// Description of the file formed during decoding
   GUTF8String		description;
      /// MIME type string describing the DjVu data.
   GUTF8String		mimetype;
      /// Size of the file.
   int			file_size;
      //@}

protected:
      /** Default constructor.  Must follow with an init() */
   DjVuFile(void);
public:
   virtual ~DjVuFile(void);

      /** Initializes a #DjVuFile# object. This is a simplified initializer,
	  which is not supposed to be used for decoding or creating
	  #DjVuFile#s, which include other files.

	  If the file is stored on the hard drive, you may also use the
	  other constructor and pass it the file's URL and #ZERO# #port#.
	  The #DjVuFile# will read the data itself.

	  If you want to receive error messages and notifications, you
	  may connect the #DjVuFile# to your own \Ref{DjVuPort} after
	  it has been constructed.

	  @param str The stream containing data for the file. */
   void init(const GP<ByteStream> & str);

      /** Creator, does the init(ByteStream &str) */
   static GP<DjVuFile> create( const GP<ByteStream> & str,
     const ErrorRecoveryAction recover_action=ABORT,
     const bool verbose_eof=true);
   
      /** Initializes a #DjVuFile# object. As you can notice, the data is not
	  directly passed to this function. The #DjVuFile# will ask for it
	  through the \Ref{DjVuPort} mechanism before the constructor
	  finishes. If the data is stored locally on the hard disk then the
	  pointer to \Ref{DjVuPort} may be set to #ZERO#, which will make
	  #DjVuFile# read all data from the hard disk and report all errors
	  to #stderr#.

	  {\bf Note}. If the file includes (by means of #INCL# chunks) other
	  files then you should be ready to
	  \begin{enumerate}
	     \item Reply to requests \Ref{DjVuPort::id_to_url}() issued to
	           translate IDs (used in #INCL# chunks) to absolute URLs.
		   Usually, when the file is created by \Ref{DjVuDocument}
		   this job is done by it. If you construct such a file
		   manually, be prepared to do the ID to URL translation
	     \item Provide data for all included files.
	  \end{enumerate}

	  @param url The URL assigned to this file. It will be used when
	         the #DjVuFile# asks for data.
	  @param port All communication between #DjVuFile#s and \Ref{DjVuDocument}s
	         is done through the \Ref{DjVuPort} mechanism. If the {\em url}
		 is not local or the data does not reside on the hard disk,
		 the {\em port} parameter must not be #ZERO#. If the {\em port}
		 is #ZERO# then #DjVuFile# will create an internal instance
		 of \Ref{DjVuSimplePort} for accessing local files and
		 reporting errors. It can later be disabled by means
		 of \Ref{disable_standard_port}() function. */
   void init(const GURL & url, GP<DjVuPort> port=0);

      /** Creator, does the init(const GURL &url, GP<DjVuPort> port=0) */
   static GP<DjVuFile> create(
     const GURL & url, GP<DjVuPort> port=0,
     const ErrorRecoveryAction recover_action=ABORT,
     const bool verbose_eof=true);

      /** Disables the built-in port for accessing local files, which may
	  have been created in the case when the #port# argument to
	  the \Ref{DjVuFile::DjVuFile}() constructor is #ZERO# */
   void		disable_standard_port(void);

      /** Looks for #decoded# navigation directory (\Ref{DjVuNavDir}) in this
	  or included files. Returns #ZERO# if nothing could be found.

	  {\bf Note.} This function does {\bf not} attempt to decode #NDIR#
	  chunks. It is looking for predecoded components. #NDIR# can be
	  decoded either during regular decoding (initiated by
	  \Ref{start_decode}() function) or by \Ref{decode_ndir}() function,
	  which processes this and included files recursively in search
	  of #NDIR# chunks and decodes them. */
   GP<DjVuNavDir>	find_ndir(void);

      /** @name #DjVuFile# flags query functions */
      //@{
      /** Returns the #DjVuFile# flags. The value returned is the
	  result of ORing one or more of the following constants:
	  \begin{itemize}
	     \item #DECODING# The decoding is in progress
	     \item #DECODE_OK# The decoding has finished successfully
	     \item #DECODE_FAILED# The decoding has failed
	     \item #DECODE_STOPPED# The decoding has been stopped by
	           \Ref{stop_decode}() function
	     \item #DATA_PRESENT# All data for this file has been received.
	           It's especially important in the case of Netscape or IE
		   plugins when the data is being received while the
		   decoding is done.
	     \item #ALL_DATA_PRESENT# Not only data for this file, but also
	           for all included file has been received.
	     \item #INCL_FILES_CREATED# All #INCL# and #INCF# chunks have been
	           processed and the corresponding #DjVuFile#s created. This
		   is important to know to be sure that the list returned by
		   \Ref{get_included_files}() is OK.
	  \end{itemize} */
   long		get_flags(void) const;
      /// Returns #TRUE# if the file is being decoded.
   bool		is_decoding(void) const;
      /// Returns #TRUE# if decoding of the file has finished successfully.
   bool		is_decode_ok(void) const;
      /// Returns #TRUE# if decoding of the file has failed.
   bool		is_decode_failed(void) const;
      /** Returns #TRUE# if decoding of the file has been stopped by
	  \Ref{stop_decode}() function. */
   bool		is_decode_stopped(void) const;
      /// Returns #TRUE# if this file has received all data.
   bool		is_data_present(void) const;
      /** Returns #TRUE# if this file {\bf and} all included files have
	  received all data. */
   bool		is_all_data_present(void) const;
      /** Returns #TRUE# if all included files have been created. Only when
	  this function returns 1, the \Ref{get_included_files}() returns
	  the correct information. */
   bool		are_incl_files_created(void) const;
   bool		is_modified(void) const;
   bool		needs_compression(void) const;
   bool		can_compress(void) const;
   void		set_modified(bool m);
   void		set_needs_compression(bool m);
   void		set_can_compress(bool m);
      //@}

      /// Returns the URL assigned to this file
   GURL		get_url(void) const;

      /** @name Decode control routines */
      //@{
      /** Starts decode. If threads are enabled, the decoding will be
	  done in another thread. Be sure to use \Ref{wait_for_finish}()
	  or listen for notifications sent through the \Ref{DjVuPortcaster}
	  to remain in sync. */
   void		start_decode(void);
      /** Start the decode iff not already decoded.  If sync is true, wait
          wait for decode to complete.  Returns true of start_decode is called.
          */
   bool   resume_decode(const bool sync=false);
      /** Stops decode. If #sync# is 1 then the function will not return
	  until the decoding thread actually dies. Otherwise it will
	  just signal the thread to stop and will return immediately.
	  Decoding of all included files will be stopped too. */
   void		stop_decode(bool sync);
      /** Recursively stops all data-related operations.

	  Depending on the value of #only_blocked# flag this works as follows:
	  \begin{itemize}
	     \item If #only_blocked# is #TRUE#, the function will make sure,
	           that any further access to the file's data will result
		   in a #STOP# exception if the desired data is not available
		   (and the thread would normally block).
	     \item If #only_blocked# is #FALSE#, then {\bf any} further
	           access to the file's data will result in immediate
		   #STOP# exception.
	  \end{itemize}

	  The action of this function is recursive, meaning that any #DjVuFile#
	  included into this one will also be stopped.

	  Use this function when you don't need the #DjVuFile# anymore. The
	  results cannot be undone, and the whole idea is to make all threads
	  working with this file exit with the #STOP# exception. */
   void		stop(bool only_blocked);
      /** Wait for the decoding to finish. This will wait for the
	  termination of included files too. */
   void		wait_for_finish(void);
      /** Looks for #NDIR# chunk (navigation directory), and decodes its
	  contents. If the #NDIR# chunk has not been found in {\em this} file,
	  but this file includes others, the procedure will continue
	  recursively. This function is useful to obtain the document
	  navigation directory before any page has been decoded. After it
	  returns the directory can be obtained by calling \Ref{find_ndir}()
	  function.

	  {\bf Warning.} Contrary to \Ref{start_decode}(), this function
	  does not return before it completely decodes the directory.
	  Make sure, that this file and all included files have enough data. */
   GP<DjVuNavDir>	decode_ndir(void);
      /// Clears all decoded components.
   void		reset(void);
      /** Processes #INCL# chunks and creates included files.
	  Normally you won't need to call this function because included
	  files are created automatically when the file is being decoded.
	  But if due to some reason you'd like to obtain the list of included
	  files without decoding this file, this is an ideal function to call.

	  {\bf Warning.} This function does not return before it reads the
	  whole file, which may block your application under some circumstances
	  if not all data is available. */
   void		process_incl_chunks(void);
      //@}
   
      // Function needed by the cache
   unsigned int	get_memory_usage(void) const;

      /** Returns the list of included DjVuFiles.
	  
	  {\bf Warning.} Included files are normally created during decoding.
	  Before that they do not exist.   If you call this function at
	  that time and set #only_created# to #FALSE# then it will have to
	  read all the data from this file in order to find #INCL# chunks,
	  which may block your application, if not all data is available.

	  @param only_created If #TRUE#, the file will not try to process
	         #INCL# chunks and load referenced files. It will return
		 just those files, which have already been created during
		 the decoding procedure. */
   GPList<DjVuFile>	get_included_files(bool only_created=true);

      /** Includes a #DjVuFile# with the specified #id# into this one.
	  This function will also insert an #INCL# chunk at position
	  #chunk_num#. The function will request data for the included
	  file and will create it before returning. */
   void		insert_file(const GUTF8String &id, int chunk_num=1);
      /// Will get rid of included file with the given #id#
   void		unlink_file(const GUTF8String &id);
      /** Will find an #INCL# chunk containing #name# in input #data# and
	  will remove it */
   static GP<DataPool>	unlink_file(const GP<DataPool> & data, const GUTF8String &name);

      /// Returns the number of chunks in the IFF file data
   int		get_chunks_number(void);
      /// Returns the name of chunk number #chunk_num#
   GUTF8String	get_chunk_name(int chunk_num);
      /// Returns 1 if this file contains chunk with name #chunk_name#
   bool		contains_chunk(const GUTF8String &chunk_name);

      /** Processes the included files hierarchy and returns merged
	  annotations. This function may be used even when the #DjVuFile#
	  has not been decoded yet. If all data has been received for
	  this #DjVuFile# and all included #DjVuFile#s, it will will
	  gather annotations from them and will return the result.
	  If no annotations have been found, #ZERO# will be returned.
	  If either this #DjVuFile# or any of the included files do not
	  have all the data, the function will use the results of
	  decoding, which may have been started with the \Ref{start_decode}()
	  function. Otherwise #ZERO# will be returned as well.

	  If #max_level_ptr# pointer is not zero, the function will use
	  it to store the maximum level number from which annotations
	  have been obtained. #ZERO# level corresponds to the top-level
	  page file.

	  {\bf Summary:} This function will return complete annotations only
	  when the \Ref{is_all_data_present}() returns #TRUE#. */
   GP<ByteStream>	get_merged_anno(int * max_level_ptr=0);

      /** Returns the annotation chunks (#"ANTa"# and #"ANTz"#).  This
          function may be used even when the #DjVuFile# has not been decoded
          yet. If all data has been received for this #DjVuFile#, it will
          gather hidden text and return the result.  If no hidden text has
          been found, #ZERO# will be returned.

	  {\bf Summary:} This function will return complete annotations
	  only when the \Ref{is_all_data_present}() returns #TRUE#. */
   GP<ByteStream>	get_anno(void);

      /** Returns the text chunks (#"TXTa"# and #"TXTz"#).  This function may
          be used even when the #DjVuFile# has not been decoded yet. If all
          data has been received for this #DjVuFile#, it will gather hidden
          text and return the result.  If no hidden text has been found,
          #ZERO# will be returned.

	  {\bf Summary:} This function will return complete hidden text layers
	  only when the \Ref{is_all_data_present}() returns #TRUE#. */
   GP<ByteStream>	get_text(void);

      /** Returns the meta chunks (#"METa"# and #"METz"#).  This function may
          be used even when the #DjVuFile# has not been decoded yet. If all
          data has been received for this #DjVuFile#, it will gather metadata
          and return the result.  If no hidden text has been found, #ZERO#
          will be returned.

	  {\bf Summary:} This function will return complete meta data only
	  when the \Ref{is_all_data_present}() returns #TRUE#. */
   GP<ByteStream>	get_meta(void);

      /** Goes down the hierarchy of #DjVuFile#s and merges their annotations.
          (shouldn't this one be private?).
	  @param max_level_ptr If this pointer is not ZERO, the function
	         will use it to store the maximum level at which annotations
		 were found. Top-level page files have ZERO #level#.
	  @param ignore_list The function will not process included #DjVuFile#s
	         with URLs matching those mentioned in this #ignore_list#. */
   GP<ByteStream>	get_merged_anno(const GList<GURL> & ignore_list,
					int * max_level_ptr);

      /** Clears this file of all annotations. */
   void	remove_anno(void);

      /** Clears the hidden text. */
   void	remove_text(void);

      /// Clears the meta data.
   void remove_meta(void);

      /** Returns #TRUE# if the file contains annotation chunks.
	  Known annotation chunks at the time of writing this help are:
	  {\bf ANTa}, {\bf ANTz}, {\bf FORM:ANNO}. */
   bool		contains_anno(void);

      /** Returns #TRUE# if the file contains hiddentext chunks.
	  Known hiddentext chunks at the time of writing this help are:
	  {\bf TXTa}, and {\bf TXTz}. */
   bool		contains_text(void);

      /** Returns #TRUE# if the file contains metadata chunks.
	  Known metadata chunks at the time of writing this help are:
	  {\bf METa}, and {\bf METz}. */
   bool		contains_meta(void);

     /** Changes the value of the hiddentext. */
   void change_info(GP<DjVuInfo> info, const bool do_reset=false);
   
     /** Changes the value of the hiddentext. */
   void change_text(GP<DjVuTXT> txt, const bool do_reset=false);
   
     /** Changes the value of the metadata. */
   void change_meta(const GUTF8String &meta, const bool do_reset=false);
   
      /** @name Encoding routines */
      //@{
      /** The main function that encodes data back into binary stream.
	  The data returned will reflect possible changes made into the
	  chunk structure, annotation chunks and navigation directory
	  chunk #NDIR#.

	  {\bf Note:} The file stream will not have the magic
          #0x41,0x54,0x26,0x54#
	  at the beginning.
	  
	  @param included_too Process included files too. */
   GP<ByteStream>	get_djvu_bytestream(const bool included_too, const bool no_ndir=true);

      /** Same as \Ref{get_djvu_bytestream}(), returning a DataPool.
	  @param included_too Process included files too. */
   GP<DataPool>		get_djvu_data(const bool included_too, const bool no_ndir=true );
      //@}

      // Internal. Used by DjVuDocument
   GP<DataPool>		get_init_data_pool(void) const { return data_pool; };

      // Internal. Used by DjVuDocument. May block for data.
   void			move(const GURL & dir_url);

      /** Internal. Used by DjVuDocument. The #name# should {\bf not}
	  be encoded with \Ref{GOS::encode_reserved}(). */
   void			set_name(const GUTF8String &name);

      // Internal. Used by DjVuDocument
   GSafeFlags &		get_safe_flags(void);

      // Internal. Used by DjVuImage
   void                 merge_anno(ByteStream &out);

      // Internal. Used by DjVuImage
   void                 get_text(ByteStream &out);

      // Internal. Used by DjVuImage
   void                 get_meta(ByteStream &out);

      // Internal. Used by DjVuDocEditor
   void			rebuild_data_pool(void);

      // Functions inherited from DjVuPort
   virtual bool		inherits(const GUTF8String &class_name) const;
   virtual void		notify_chunk_done(const DjVuPort * source, const GUTF8String &name);
   virtual void		notify_file_flags_changed(const DjVuFile * source,
						  long set_mask, long clr_mask);
   virtual void		set_recover_errors(const ErrorRecoveryAction=ABORT);
   virtual void		set_verbose_eof(const bool verbose_eof=true);
   virtual void		report_error(const GException &ex,const bool=true);
   static void set_decode_codec(GP<GPixmap> (*codec)(ByteStream &bs));

protected:
   GURL			url;
   GP<DataPool>		data_pool;

   GPList<DjVuFile>	inc_files_list;
   GCriticalSection	inc_files_lock;
   GCriticalSection	anno_lock;
   GCriticalSection	text_lock;
   GCriticalSection	meta_lock;
   ErrorRecoveryAction	recover_errors;
   bool			verbose_eof;
   int			chunks_number;
private:
   bool                 initialized;
   GSafeFlags		flags;

   GThread		* decode_thread;
   GP<DataPool>		decode_data_pool;
   GP<DjVuFile>		decode_life_saver;

   GP<DjVuPort>		simple_port;

   GMonitor		chunk_mon, finish_mon;

      // Functions called when the decoding thread starts
   static void	static_decode_func(void *);
   void	decode_func(void);
   void	decode(const GP<ByteStream> &str);
   GUTF8String decode_chunk(const GUTF8String &chkid,
     const GP<ByteStream> &str, bool djvi, bool djvu, bool iw44);
   int		get_dpi(int w, int h);

      // Functions dealing with the shape directory (fgjd)
   static GP<JB2Dict> static_get_fgjd(void *);
   GP<JB2Dict> get_fgjd(int block=0);

      // Functions used to wait for smth
   void		wait_for_chunk(void);
   bool		wait_for_finish(bool self);

      // INCL chunk processor
   GP<DjVuFile>	process_incl_chunk(ByteStream & str, int file_num=-1);

      // Trigger: called when DataPool has all data
   static void	static_trigger_cb(void *);
   void		trigger_cb(void);
   
      // Progress callback: called from time to time
   static void	progress_cb(int pos, void *);
   static void	get_merged_anno(const GP<DjVuFile> & file,
     const GP<ByteStream> &str_out, const GList<GURL> & ignore_list,
     int level, int & max_level, GMap<GURL, void *> & map);
   static void	get_anno(const GP<DjVuFile> & file,
     const GP<ByteStream> &str_out);
   static void	get_text(const GP<DjVuFile> & file,
     const GP<ByteStream> &str_out);
   static void	get_meta(const GP<DjVuFile> & file,
     const GP<ByteStream> &str_out);

   void          check() const;
   GP<DjVuNavDir>find_ndir(GMap<GURL, void *> & map);
   GP<DjVuNavDir>decode_ndir(GMap<GURL, void *> & map);
   void		add_djvu_data(IFFByteStream & str,
			      GMap<GURL, void *> & map,
			      const bool included_too, const bool no_ndir=true);
   void		move(GMap<GURL, void *> & map, const GURL & dir_url);
private: // dummy stuff
   static void decode(ByteStream *);
   static GUTF8String decode_chunk(const GUTF8String &, ByteStream *,bool,bool,bool);
   static void	get_merged_anno(const GP<DjVuFile> &,ByteStream *,
     const GList<GURL> &, int, int &, GMap<GURL, void *> &);
   static void	get_text(const GP<DjVuFile> &,ByteStream *);
   static void	get_meta(const GP<DjVuFile> &,ByteStream *);

};

inline long
DjVuFile::get_flags(void) const
{
   return flags;
}

inline GSafeFlags &
DjVuFile::get_safe_flags(void)
{
   return flags;
}

inline bool
DjVuFile::is_decoding(void) const
{
   return (flags & DECODING)!=0;
}

inline bool
DjVuFile::is_decode_ok(void) const
{
   return (flags & DECODE_OK)!=0;
}

inline bool
DjVuFile::is_decode_failed(void) const
{
   return (flags & DECODE_FAILED)!=0;
}

inline bool
DjVuFile::is_decode_stopped(void) const
{
   return (flags & DECODE_STOPPED)!=0;
}

inline bool
DjVuFile::is_data_present(void) const
{
   return (flags & DATA_PRESENT)!=0;
}

inline bool
DjVuFile::is_all_data_present(void) const
{
   return (flags & ALL_DATA_PRESENT)!=0;
}

inline bool
DjVuFile::are_incl_files_created(void) const
{
   return (flags & INCL_FILES_CREATED)!=0;
}

inline bool
DjVuFile::is_modified(void) const
{
   return (flags & MODIFIED)!=0;
}

inline void
DjVuFile::set_modified(bool m)
{
  flags=m ? (flags | MODIFIED) : (flags & ~MODIFIED);
}

inline bool
DjVuFile::needs_compression(void) const
{
   return (flags & NEEDS_COMPRESSION)!=0;
}

inline void
DjVuFile::set_needs_compression(bool m)
{
   if (m) flags=flags | NEEDS_COMPRESSION;
   else flags=flags & ~NEEDS_COMPRESSION;
}

inline bool
DjVuFile::can_compress(void) const
{
   return (flags & CAN_COMPRESS)!=0;
}

inline void
DjVuFile::set_can_compress(bool m)
{
   if (m)
     flags=flags | CAN_COMPRESS;
   else
     flags=flags & ~CAN_COMPRESS;
}

inline void
DjVuFile::disable_standard_port(void)
{
   simple_port=0;
}

inline bool
DjVuFile::inherits(const GUTF8String &class_name) const
{
   return
      (GUTF8String("DjVuFile") == class_name) ||
      DjVuPort::inherits(class_name);
//      !strcmp("DjVuFile", class_name) ||
//      DjVuPort::inherits(class_name);
}

inline void
DjVuFile::wait_for_finish(void)
{
   while(wait_for_finish(1))
     EMPTY_LOOP;
}

inline GURL
DjVuFile::get_url(void) const
{
   return url;
}

inline void
DjVuFile::set_verbose_eof
(const bool verbose)
{
  verbose_eof=verbose;
}

inline void
DjVuFile::set_recover_errors
(const ErrorRecoveryAction action)
{
  recover_errors=action;
}

//@}


#ifdef HAVE_NAMESPACES
}
# ifndef NOT_USING_DJVU_NAMESPACE
using namespace DJVU;
# endif
#endif
#endif
