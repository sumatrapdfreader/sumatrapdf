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

#ifndef __DJVU_MESSAGE_LITE_H__
#define __DJVU_MESSAGE_LITE_H__
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


#include "GString.h"

#ifdef HAVE_NAMESPACES
namespace DJVU {
# ifdef NOT_DEFINED // Just to fool emacs c++ mode
}
#endif
#endif

class lt_XMLTags;
class ByteStream;

/** Exception causes and external messages are passed as message lists which
    have the following syntax:
  
    message_list ::= single_message |
                     single_message separator message_list
    
    separator ::= newline |
                  newline | separator
    
    single_message ::= message_ID |
                       message_ID parameters
    
    parameters ::= tab string |
                   tab string parameters
    
    Message_IDs are looked up an external file and replaced by the message
    text strings they are mapped to. The message text may contain the
    following:
    
    Parameter specifications: These are modelled after printf format
    specifications and have one of the following forms:
  
      %n!s! %n!d! %n!i! %n!u! %n!x! %n!X!
  
    where n is the parameter number. The parameter number is indicated
    explicitly to allow for the possibility that the parameter order may
    change when the message text is translated into another language.
    The final letter ('s', 'd', 'i', 'u', 'x', or 'X') indicates the form
    of the parameter (string, integer, integer, unsigned integer, lowercase
    hexadecimal, or uppercase hexadecimal, respectively).  In addition
    formating options such as precision available in sprintf, may be used.
    So, to print the third argument as 5 digit unsigned number, with leading
    zero's one could use:
      %3!05u!

    All of the arguments are actually strings.  For forms that don't take
    string input ('d', 'i', 'u', 'x', or 'X'), and atoi() conversion is done
    on the string before formatting.  In addition the form indicates to the
    translater whether to expect a word or a number.

    The strings are read in from XML.  To to format the strings, use the
    relavent XML escape sequences, such as follows:

            &#10;        [line feed]
            &#09;        [horizontal tab]
            &apos;       [single quote]
            &#34;        [double quote]
            &lt;         [less than sign]
            &gt;         [greater than sign]
  
    After parameters have been inserted in the message text, the formatting 
    strings are replaced by their usual equivalents (newline and tab
    respectively).

    If a message_id cannot be found in the external file, a message text
    is fabricated giving the message_id and the parameters (if any).

    Separators (newlines) are preserved in the translated message list.

    Expands message lists by looking up the message IDs and inserting
    arguments into the retrieved messages.

    N.B. The resulting string may be encoded in UTF-8 format (ISO 10646-1
    Annex R) and SHOULD NOT BE ASSUMED TO BE ASCII.
  */

class DJVUAPI DjVuMessageLite : public GPEnabled
{
protected:
  // Constructor:
  DjVuMessageLite( void );
  GMap<GUTF8String,GP<lt_XMLTags> > Map;
  GUTF8String errors;
  /// Creates a DjVuMessage class.
  static const DjVuMessageLite &real_create(void);

public:
  /// Creates a DjVuMessage class.
  static const DjVuMessageLite& (*create)(void);

  /// Creates this class specifically.
  static const DjVuMessageLite &create_lite(void);

  /** Adds a byte stream to be parsed whenever the next DjVuMessage::create()
      call is made. */
  static void AddByteStreamLater(const GP<ByteStream> &bs);

  /** Destructor: Does any necessary cleanup. Actions depend on how the message
      file is implemented. */
  ~DjVuMessageLite();

  /// Lookup the relavent string and parse the message.
  GUTF8String LookUp( const GUTF8String & MessageList ) const;

  //// Same as LookUp, but this is a static method.
  static GUTF8String LookUpUTF8( const GUTF8String & MessageList )
  { return create().LookUp(MessageList); }

  /** Same as Lookup, but returns the a multibyte character string in the
      current locale. */
  static GNativeString LookUpNative( const GUTF8String & MessageList )
  { return create().LookUp(MessageList).getUTF82Native(); }

  /// This is a simple alias to the above class, but does an fprintf to stderr.
  static void perror( const GUTF8String & MessageList );

protected:

  /*  Looks up the msgID in the file of messages. The strings message_text
      and message_number are returned if found. If not found, these strings
      are empty. */
  void LookUpID( const GUTF8String & msgID,
    GUTF8String &message_text, GUTF8String &message_number ) const;

  /*  Expands a single message and inserts the arguments. Single_Message
      contains no separators (newlines), but includes all the parameters
      separated by tabs. */
  GUTF8String LookUpSingle( const GUTF8String & Single_Message ) const;

  /*  Insert a string into the message text. Will insert into any field
      description.  Except for an ArgId of zero (message number), if the
      #ArgId# is not found, the routine adds a line with the parameter
      so information will not be lost. */
  void InsertArg(
    GUTF8String &message, const int ArgId, const GUTF8String &arg ) const;

  void AddByteStream(const GP<ByteStream> &bs);

protected:
  /*  Static storage of the DjVuMessage class. */
  static GP<DjVuMessageLite> &getDjVuMessageLite(void);
  /*  Static storage of the ByteStream list. */
  static GPList<ByteStream> &getByteStream(void);
};


#ifdef HAVE_NAMESPACES
}
# ifndef NOT_USING_DJVU_NAMESPACE
using namespace DJVU;
# endif
#endif
#endif /* __DJVU_MESSAGE_LITE_H__ */

