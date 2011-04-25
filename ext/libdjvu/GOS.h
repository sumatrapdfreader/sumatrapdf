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

#ifndef _GOS_H_
#define _GOS_H_
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#if NEED_GNUG_PRAGMAS
# pragma interface
#endif

/** @name GOS.h
    Files #"GOS.h"# and #"GOS.cpp"# implement operating system 
    dependent functions with a unified interface.  All these functions
    are implemented as static member of class \Ref{GOS}. 
    Functions are provided for testing the presence of a file or a directory
    (\Ref{GOS::is_file}, \Ref{GOS::is_dir}), for manipulating file and directory names
    (\Ref{GOS::dirname}, \Ref{GOS::basename}, \Ref{GOS::expand_name},
    for obtaining and changing the current directory (\Ref{GOS::cwd}),
    for converting between file names and urls (\Ref{GOS::filename_to_url},
    \Ref{GOS::url_to_filename}), and for counting time (\Ref{GOS::ticks},
    \Ref{GOS::sleep}).
    
    @memo
    Operating System dependent functions.
    @author
    L\'eon Bottou <leonb@research.att.com> -- Initial implementation
*/
//@{

#include "DjVuGlobal.h"
#include "GString.h"

#ifdef HAVE_NAMESPACES
namespace DJVU {
# ifdef NOT_DEFINED // Just to fool emacs c++ mode
}
#endif
#endif


class GURL;

/** Operating System dependent functions. */
class DJVUAPI GOS 
{
 public:
  // -----------------------------------------
  // Functions for dealing with filenames
  // -----------------------------------------

  /** Returns the last component of file name #filename#.  If the filename
      suffix matches argument #suffix#, the filename suffix is removed.  This
      function works like the unix command #/bin/basename#, but also supports
      the naming conventions of other operating systems. */
  static GUTF8String basename(const GUTF8String &filename, const char *suffix=0);

  /** Sets and returns the current working directory.
      When argument #dirname# is specified, the current directory is changed
      to #dirname#. This function always returns the fully qualified name
      of the current directory. */
  static GUTF8String cwd(const GUTF8String &dirname=GUTF8String());

  // -----------------------------------------
  // Functions for measuring time
  // -----------------------------------------
  
  /** Returns a number of elapsed milliseconds.  This number counts elapsed
      milliseconds since a operating system dependent date. This function is
      useful for timing code. */
  static unsigned long ticks();

  /** Sleeps during the specified time expressed in milliseconds.
      Other threads can run while the calling thread sleeps. */
  static void sleep(int milliseconds);

  /// Read the named variable from the environment, and converts it to UTF8.
  static GUTF8String getenv(const GUTF8String &name);

#if 0
  // -------------------------------------------
  // Functions for converting filenames and urls
  // -------------------------------------------
  /** Encodes all reserved characters, so that the #filename# can be
      used inside a URL. Every reserved character is encoded using escape
      sequence in the form of #%XX#. The legal characters are alphanumeric and
      #$-_.+!*'(),:#.
      Use \Ref{decode_reserved}() to convert the URL back to the filename. */
//  static GString encode_reserved(const char * filename);
  static GString encode_mbcs_reserved(const char * filename);/*MBCS*/
#endif

};


//@}
// ------------

#ifdef HAVE_NAMESPACES
}
# ifndef NOT_USING_DJVU_NAMESPACE
using namespace DJVU;
# endif
#endif
#endif
