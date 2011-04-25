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


#ifndef __DJVU_MESSAGE_H__
#define __DJVU_MESSAGE_H__
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#if NEED_GNUG_PRAGMAS
# pragma interface
#endif

// From: Leon Bottou, 1/31/2002
// All these I18N XML messages are Lizardtech innovations.
// For DjvuLibre, I changed the path extraction logic
// and added support for non I18N messages. 


#include "DjVuMessageLite.h"

#ifdef HAVE_NAMESPACES
namespace DJVU {
# ifdef NOT_DEFINED // Just to fool emacs c++ mode
}
#endif
#endif

class GURL;

class DJVUAPI DjVuMessage : public DjVuMessageLite
{
protected:
  void init(void);
  DjVuMessage(void);

public:
  /// Use the locale info and find the XML files on disk.
  static void use_language(void);

  /// Set the program name used when searching for XML files on disk.
  static void set_programname(const GUTF8String &programname);
  static GUTF8String &programname(void);

  /// creates this class specifically.
  static const DjVuMessageLite &create_full(void);

  /** Adds a byte stream to be parsed whenever the next DjVuMessage::create()
      call is made. */
  static void AddByteStreamLater(const GP<ByteStream> &bs)
  { use_language(); DjVuMessageLite::AddByteStreamLater(bs); }

  /** Destructor: Does any necessary cleanup. Actions depend on how the message
      file is implemented. */
  ~DjVuMessage();

  //// Same as LookUp, but this is a static method.
  static GUTF8String LookUpUTF8( const GUTF8String & MessageList )
  { use_language();return DjVuMessageLite::LookUpUTF8(MessageList); }

  /** Same as Lookup, but returns a multibyte character string in the
      current locale. */
  static GNativeString LookUpNative( const GUTF8String & MessageList )
  { use_language();return DjVuMessageLite::LookUpNative(MessageList); }

  /// This is a simple alias to the above class, but does an fprintf to stderr.
  static void perror( const GUTF8String & MessageList )
  { use_language();DjVuMessageLite::perror(MessageList); }

  static GList<GURL> GetProfilePaths(void);
};



#ifdef HAVE_NAMESPACES
}
# ifndef NOT_USING_DJVU_NAMESPACE
using namespace DJVU;
# endif
#endif
#endif /* __DJVU_MESSAGE_H__ */

