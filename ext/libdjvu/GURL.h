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

#ifndef _GURL_H_
#define _GURL_H_
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#if NEED_GNUG_PRAGMAS
# pragma interface
#endif


#include "GString.h"
#include "Arrays.h"
#include "GThreads.h"


#ifdef HAVE_NAMESPACES
namespace DJVU {
# ifdef NOT_DEFINED // Just to fool emacs c++ mode
}
#endif
#endif

/** @name GURL.h
    Files #"GURL.h"# and #"GURL.cpp"# contain the implementation of the
    \Ref{GURL} class used to store URLs in a system independent format.
    @memo System independent URL representation.
    @author Andrei Erofeev <eaf@geocities.com>

// From: Leon Bottou, 1/31/2002
// This has been heavily changed by Lizardtech.
// They decided to use URLs for everyting, including
// the most basic file access.  The URL class now is a unholy 
// mixture of code for syntactically parsing the urls (which is was)
// and file status code (only for local file: urls).

*/

//@{

/** System independent URL representation.

    This class is used in the library to store URLs in a system independent
    format. The idea to use a general class to hold URL arose after we
    realized, that DjVu had to be able to access files both from the WEB
    and from the local disk. While it is strange to talk about system
    independence of HTTP URLs, file names formats obviously differ from
    platform to platform. They may contain forward slashes, backward slashes,
    colons as separators, etc. There maybe more than one URL corresponding
    to the same file name. Compare #file:/dir/file.djvu# and
    #file://localhost/dir/file.djvu#.

    To simplify a developer's life we have created this class, which contains
    inside a canonical representation of URLs.

    File URLs are converted to internal format with the help of \Ref{GOS} class.

    All other URLs are modified to contain only forward slashes.
*/

class DJVUAPI GURL
{
public:
  class Filename;
  class UTF8;
  class Native;
protected:
      /** @name Constructors
	  Accept the string URL, check that it starts from #file:/#
	  or #http:/# and convert to internal system independent
	  representation.
      */
      //@{
      ///
   GURL(const char * url_string);
      //@}

public:
   GURL(void);

   GURL(const GUTF8String & url_string);

   GURL(const GNativeString & url_string);

   GURL(const GUTF8String &xurl, const GURL &codebase);

   GURL(const GNativeString &xurl, const GURL &codebase);

      /// Copy constructor
   GURL(const GURL & gurl);

      /// The destructor
   virtual ~GURL(void) {}

private:
      // The 'class_lock' should be locked whenever you're accessing
      // url, or cgi_name_arr, or cgi_value_arr.
   GCriticalSection	class_lock;
protected:
   GUTF8String	url;
   DArray<GUTF8String>	cgi_name_arr, cgi_value_arr;
   bool validurl;

   void		init(const bool nothrow=false);
   void		convert_slashes(void);
   void		beautify_path(void);
   static GUTF8String	beautify_path(GUTF8String url);

   static GUTF8String	protocol(const GUTF8String& url);
   void		parse_cgi_args(void);
   void		store_cgi_args(void);
public:
   /// Test if the URL is valid. If invalid, reinitialize.
   bool is_valid(void) const;     // const lies to the compiler because of dependency problems

      /// Extracts the {\em protocol} part from the URL and returns it
   GUTF8String	protocol(void) const;

      /** Returns string after the first '\#' with decoded
	  escape sequences. */
   GUTF8String	hash_argument(void) const;

      /** Inserts the #arg# after a separating hash into the URL.
	  The function encodes any illegal character in #arg# using
	  \Ref{GOS::encode_reserved}(). */
   void		set_hash_argument(const GUTF8String &arg);

      /** Returns the total number of CGI arguments in the URL.
	  CGI arguments follow '#?#' sign and are separated by '#&#' signs */
   int		cgi_arguments(void) const;

      /** Returns the total number of DjVu-related CGI arguments (arguments
	  following #DJVUOPTS# in the URL). */
   int		djvu_cgi_arguments(void) const;

      /** Returns that part of CGI argument number #num#, which is
	  before the equal sign. */
   GUTF8String	cgi_name(int num) const;

      /** Returns that part of DjVu-related CGI argument number #num#,
	  which is before the equal sign. */
   GUTF8String	djvu_cgi_name(int num) const;

      /** Returns that part of CGI argument number #num#, which is
	  after the equal sign. */
   GUTF8String	cgi_value(int num) const;
   
      /** Returns that part of DjVu-related CGI argument number #num#,
	  which is after the equal sign. */
   GUTF8String	djvu_cgi_value(int num) const;
   
      /** Returns array of all known CGI names (part of CGI argument before
	  the equal sign) */
   DArray<GUTF8String>cgi_names(void) const;

      /** Returns array of names of DjVu-related CGI arguments (arguments
	  following #DJVUOPTS# option. */
   DArray<GUTF8String>djvu_cgi_names(void) const;
   
      /** Returns array of all known CGI names (part of CGI argument before
	  the equal sign) */
   DArray<GUTF8String>cgi_values(void) const;

      /** Returns array of values of DjVu-related CGI arguments (arguments
	  following #DJVUOPTS# option. */
   DArray<GUTF8String>djvu_cgi_values(void) const;

      /// Erases everything after the first '\#' or '?'
   void		clear_all_arguments(void);

      /// Erases everything after the first '\#'
   void		clear_hash_argument(void);

      /// Erases DjVu CGI arguments (following "#DJVUOPTS#")
   void		clear_djvu_cgi_arguments(void);

      /// Erases all CGI arguments (following the first '?')
   void		clear_cgi_arguments(void);

      /** Appends the specified CGI argument. Will insert "#DJVUOPTS#" if
	  necessary */
   void		add_djvu_cgi_argument(const GUTF8String &name, const char * value=0);
   
      /** Returns the URL corresponding to the directory containing
	  the document with this URL. The function basically takes the
	  URL and clears everything after the last slash. */
   GURL		base(void) const;

      /// Returns the aboslute URL without the host part.
   GUTF8String pathname(void) const;

      /** Returns the name part of this URL.
	  For example, if the URL is #http://www.lizardtech.com/file%201.djvu# then
          this function will return #file%201.djvu#. \Ref{fname}() will
          return #file 1.djvu# at the same time. */
   GUTF8String	name(void) const;

      /** Returns the name part of this URL with escape sequences expanded.
	  For example, if the URL is #http://www.lizardtech.com/file%201.djvu# then
          this function will return #file 1.djvu#. \Ref{name}() will
          return #file%201.djvu# at the same time. */
   GUTF8String	fname(void) const;

      /// Returns the extention part of name of document in this URL.
   GUTF8String	extension(void) const;

      /// Checks if this is an empty URL
   bool		is_empty(void) const;

      /// Checks if the URL is local (starts from #file:/#) or not
   bool		is_local_file_url(void) const;

      /** @name Concatenation operators
	  Concatenate the GURL with the passed {\em name}. If the {\em name}
	  is absolute (has non empty protocol prefix), we just return
	  #GURL(name)#. Otherwise the #name# is appended to the GURL after a
	  separating slash.
      */
      //@{
      ///
//   GURL		operator+(const GUTF8String &name) const;
      //@}

      /// Returns TRUE if #gurl1# and #gurl2# are the same
   bool	operator==(const GURL & gurl2) const;

      /// Returns TRUE if #gurl1# and #gurl2# are different
   bool	operator!=(const GURL & gurl2) const;

      /// Assignment operator
   GURL &	operator=(const GURL & url);

      /// Returns Internal URL representation
   operator	const char*(void) const { return url; };

  /** Returns a string representing the URL.  This function normally
      returns a standard file URL as described in RFC 1738.  
      Some versions of MSIE do not support this standard syntax.
      A brain damaged MSIE compatible syntax is generated
      when the optional argument #useragent# contains string #"MSIE"# or
      #"Microsoft"#. */
   GUTF8String get_string(const GUTF8String &useragent) const;

   GUTF8String get_string(const bool nothrow=false) const;

      /// Escape special characters
   static GUTF8String encode_reserved(const GUTF8String &gs);

   /** Decodes reserved characters from the URL.
      See also: \Ref{encode_reserved}(). */
   static GUTF8String decode_reserved(const GUTF8String &url);

  /// Test if this url is an existing file, directory, or device.
  bool is_local_path(void) const;

  /// Test if this url is an existing file.
  bool is_file(void) const;

  /// Test if this url is an existing directory.
  bool is_dir(void) const;

  /// Follows symbolic links.
  GURL follow_symlinks(void) const;

  /// Creates the specified directory.
  int mkdir(void) const;

  /** Deletes file or directory.
      Directories are not deleted unless the directory is empty.
      Returns a negative number if an error occurs. */
  int deletefile(void) const;

  /** Recursively erases contents of directory. The directory
      itself will not be removed. */
  int cleardir(const int timeout=0) const;

  /// Rename a file or directory.
  int renameto(const GURL &newurl) const;

  /// List the contents of a directory. 
  GList<GURL> listdir(void) const;

  /** Returns a filename for a URL. Argument #url# must be a legal file URL.
      This function applies heuristic rules to convert the URL into a valid
      file name. It is guaranteed that this function can properly parse all
      URLs generated by #filename_to_url#. The heuristics also work better when
      the file actually exists.  The empty string is returned when this
      function cannot parse the URL or when the URL is not a file URL.
        URL formats are as described in RFC 1738 plus the following alternative
      formats for files on the local host:

                file://<letter>:/<path>
                file://<letter>|/<path>
                file:/<path>

      which are accepted because various browsers recognize them.*/
   GUTF8String UTF8Filename(void) const;
   /// Same but returns a native string.
   GNativeString NativeFilename(void) const;

      /** Hashing function.
	  @return hash suitable for usage in \Ref{GMap} */
   friend unsigned int	hash(const GURL & gurl);

  /** Returns fully qualified file names.  This functions constructs the fully
      qualified name of file or directory #filename#. When provided, the
      optional argument #fromdirname# is used as the current directory when
      interpreting relative specifications in #filename#.  Function
      #expand_name# is very useful for logically concatenating file names.  It
      knows which separators should be used for each operating system and it
      knows which syntactical rules apply. */
  static GUTF8String expand_name(const GUTF8String &filename, const char *fromdirname=0);
};

class DJVUAPI GURL::UTF8 : public GURL
{
public:
  UTF8(const GUTF8String &xurl);
  UTF8(const GUTF8String &xurl, const GURL &codebase);
};

class DJVUAPI GURL::Native : public GURL
{
public:
  Native(const GNativeString &xurl);
  Native(const GNativeString &xurl, const GURL &codebase);
};

class DJVUAPI GURL::Filename : public GURL
{
public:
  Filename(const GUTF8String &filename);
  Filename(const GNativeString &filename);
  class UTF8;
  class Native;
};

class DJVUAPI GURL::Filename::UTF8 : public GURL::Filename
{
public:
  UTF8(const GUTF8String &filename);
};

class DJVUAPI GURL::Filename::Native : public GURL::Filename
{
public:
  Native(const GNativeString &filename);
};


inline bool
GURL::operator!=(const GURL & gurl2) const
{
  return !(*this == gurl2);
}

inline GUTF8String
GURL::protocol(void) const
{
   return protocol(get_string());
}

inline bool
GURL::is_empty(void) const
{
   return !url.length()||!get_string().length();
}

// Test if the URL is valid.
// If invalid, reinitialize and return the result.
inline bool
GURL::is_valid(void) const
{
  if(!validurl)
    const_cast<GURL *>(this)->init(true);
  return validurl;
}



//@}


#ifdef HAVE_NAMESPACES
}
# ifndef NOT_USING_DJVU_NAMESPACE
using namespace DJVU;
# endif
#endif
#endif
