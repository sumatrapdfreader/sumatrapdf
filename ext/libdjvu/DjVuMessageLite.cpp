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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#if NEED_GNUG_PRAGMAS
# pragma implementation
#endif

// From: Leon Bottou, 1/31/2002
// All these I18N XML messages are Lizardtech innovations.
// For DjvuLibre, I changed the path extraction logic
// and added support for non I18N messages. 

#include "DjVuMessageLite.h"
#include "GOS.h"
#include "XMLTags.h"
#include "ByteStream.h"
#include "GURL.h"
#include "debug.h"
#include <ctype.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#ifdef _WIN32
#include <tchar.h>
#include <windows.h>
#include <winreg.h>
#endif
#ifdef UNIX
#include <unistd.h>
#include <pwd.h>
#include <sys/types.h>
#endif
#include <locale.h>


#ifdef HAVE_NAMESPACES
namespace DJVU {
# ifdef NOT_DEFINED // Just to fool emacs c++ mode
}
#endif
#endif


const DjVuMessageLite& (*DjVuMessageLite::create)(void) = 
  DjVuMessageLite::create_lite; 

static const char *failed_to_parse_XML=
  ERR_MSG("DjVuMessage.failed_to_parse_XML");
static const char *unrecognized=
  ERR_MSG("DjVuMessage.Unrecognized");
static const char *uparameter=
  ERR_MSG("DjVuMessage.Parameter");
#ifdef LIZARDTECH_1_800_NUMBER
static const char unrecognized_default[] =
  "** Unrecognized DjVu Message: [Contact LizardTech at " 
  LIZARDTECH_1_800_NUMBER " \n"
  "\t** Message name:  %1!s!";
#else
static const char unrecognized_default[] =
  "** Unrecognized DjVu Message:\n"
  "\t** Message name:  %1!s!";
#endif
static const char uparameter_default[] = 
  "\t   Parameter: %1!s!";
static const char failed_to_parse_XML_default[]=
  "Failed to parse XML message file:&#10;&#09;&apos;%1!s!&apos;.";

static const char namestring[]="name";
static const char valuestring[]="value";
static const char numberstring[]="number";
static const char bodystring[]="BODY";
static const char messagestring[]="MESSAGE";

GPList<ByteStream> &
DjVuMessageLite::getByteStream(void)
{
  static GPList<ByteStream> gbs;
  return gbs;
}

GP<DjVuMessageLite> &
DjVuMessageLite::getDjVuMessageLite(void)
{
  static GP<DjVuMessageLite> message;
  return message;
}

void
DjVuMessageLite::AddByteStreamLater(const GP<ByteStream> &bs)
{
  getByteStream().append(bs);
}

//  There is only object of class DjVuMessage in a program, and here it is:
//DjVuMessage  DjVuMsg;
const DjVuMessageLite &
DjVuMessageLite::create_lite(void)
{
  GP<DjVuMessageLite> &static_message=getDjVuMessageLite();
  if(!static_message)
  {
    static_message=new DjVuMessageLite;
  }
  DjVuMessageLite &m=*static_message;
  GPList<ByteStream> &bs = getByteStream();
  for(GPosition pos;(pos=bs);bs.del(pos))
  {
    m.AddByteStream(bs[pos]);
  }
  return m;
}

// Constructor
DjVuMessageLite::DjVuMessageLite( void ) {}

// Destructor
DjVuMessageLite::~DjVuMessageLite( ) {}


void
DjVuMessageLite::perror( const GUTF8String & MessageList )
{
  DjVuPrintErrorUTF8("%s\n",(const char *)DjVuMessageLite::LookUpUTF8(MessageList));
}


//  Expands message lists by looking up the message IDs and inserting
//  arguments into the retrieved messages.
//  N.B. The resulting string may be encoded in UTF-8 format (ISO 10646-1 Annex R)
//       and SHOULD NOT BE ASSUMED TO BE ASCII.
GUTF8String
DjVuMessageLite::LookUp( const GUTF8String & MessageList ) const
{
//  DEBUG_MSG( "========== DjVuMessageLite::LookUp ==========\n" <<
//             MessageList <<
//             "\n========== DjVuMessageLite::LookUp ==========\n" );
  GUTF8String result;                       // Result string; begins empty
  if(errors.length())
  {
    const GUTF8String err1(errors);
    (const_cast<GUTF8String &>(errors)).empty();
    result=LookUp(err1)+"\n";
  }

  int start = 0;                            // Beginning of next message
  int end = MessageList.length();           // End of the message string

  //  Isolate single messages and process them
  while( start < end )
  {
    if( MessageList[start] == '\n' )
    {
      result += MessageList[start++];       // move the newline to the result
                                            // and advance to the next message
    }
    else
    {
      //  Find the end of the next message and process it
      int next_ending = MessageList.search((unsigned long)'\n', start);
      if( next_ending < 0 )
        next_ending = end;
      result += LookUpSingle( MessageList.substr(start, next_ending-start) );
      //  Advance to the next message
      start = next_ending;
    }
  }

  //  All done 
  return result;
}


// Expands a single message and inserts the arguments. Single_Message
// contains no separators (newlines), but includes all the parameters
// separated by tabs.
GUTF8String
DjVuMessageLite::LookUpSingle( const GUTF8String &Single_Message ) const
{
#if HAS_CTRL_C_IN_ERR_MSG
  if (Single_Message[0] != '\003')
    return Single_Message;
#endif
  //  Isolate the message ID and get the corresponding message text
  int ending_posn = Single_Message.contains("\t\v");
  if( ending_posn < 0 )
    ending_posn = Single_Message.length();
  GUTF8String msg_text;
  GUTF8String msg_number;
  const GUTF8String message=Single_Message.substr(0,ending_posn);
  LookUpID( message, msg_text, msg_number );

  //  Check whether we found anything
  if( !msg_text.length())
  {
    if(message == unrecognized)
    {
      msg_text = unrecognized_default;
    }else if(message == uparameter)
    {
      msg_text = uparameter_default;
    }else if(message == failed_to_parse_XML)
    {
      msg_text = failed_to_parse_XML_default;
    }else
    {
      return LookUpSingle(unrecognized + ("\t" + Single_Message));
    }
  }
    
  //  Insert the parameters (if any)
  unsigned int param_num = 0;
  while( (unsigned int)ending_posn < Single_Message.length() )
  {
    GUTF8String arg;
    const int start_posn = ending_posn+1;
    if(Single_Message[ending_posn] == '\v')
    {
      ending_posn=Single_Message.length();
      arg=LookUpSingle(Single_Message.substr(start_posn,ending_posn));
    }else
    {
      ending_posn = Single_Message.contains("\v\t",start_posn);
      if( ending_posn < 0 )
        ending_posn = Single_Message.length();
      arg=Single_Message.substr(start_posn, ending_posn-start_posn);
    }
    InsertArg( msg_text, ++param_num, arg);
  }
  //  Insert the message number
  InsertArg( msg_text, 0, msg_number );

  return msg_text;
}


// Looks up the msgID in the file of messages and returns a pointer to
// the beginning of the translated message, if found; and an empty string
// otherwise.
void
DjVuMessageLite::LookUpID( const GUTF8String &xmsgID,
                       GUTF8String &message_text,
                       GUTF8String &message_number ) const
{
  if(!Map.isempty())
  {
    GUTF8String msgID = xmsgID;
#if HAS_CTRL_C_IN_ERR_MSG
    int start = 0;
    while (msgID[start] == '\003') 
      start ++;
    if (start > 0)
      msgID = msgID.substr(start, msgID.length() - start);
#endif
    GPosition pos=Map.contains(msgID);
    if(pos)
    {
      const GP<lt_XMLTags> tag=Map[pos];
      GPosition valuepos=tag->get_args().contains(valuestring);
      if(valuepos)
      {
        message_text=tag->get_args()[valuepos];
      }else
      {
        const GUTF8String raw(tag->get_raw());
        const int start_line=raw.search((unsigned long)'\n',0);
      
        const int start_text=raw.nextNonSpace(0);
        const int end_text=raw.firstEndSpace(0);
        if(start_line<0 || start_text<0 || start_text < start_line)
        {
          message_text=raw.substr(0,end_text).fromEscaped();
        }else
        {
          message_text=raw.substr(start_line+1,end_text-start_line-1).fromEscaped();
        }
      }
      GPosition numberpos=tag->get_args().contains(numberstring);
      if(numberpos)
      {
        message_number=tag->get_args()[numberpos];
      }
    }
  }
}


// Insert a string into the message text. Will insert into any field
// description.  Except for an ArgId of zero (message number), if the ArgId
// is not found, the routine adds a line with the parameter so information
// will not be lost.
void
DjVuMessageLite::InsertArg( GUTF8String &message,
  const int ArgId, const GUTF8String &arg ) const
{
    // argument target string
  const GUTF8String target= "%"+GUTF8String(ArgId)+"!";
    // location of target string
  int format_start = message.search( (const char *)target );
  if( format_start >= 0 )
  {
    do
    {
      const int n=format_start+target.length()+1;
      const int format_end=message.search((unsigned long)'!',n);
      if(format_end > format_start)
      { 
        const int len=1+format_end-n;
        if(len && isascii(message[n-1]))
        {
          GUTF8String narg;
          GUTF8String format="%"+message.substr(n-1,len);
          switch(format[len])
          {
            case 'd':
            case 'i':
              narg.format((const char *)format,arg.toInt());
              break;
            case 'u':
            case 'o':
            case 'x':
            case 'X':
              narg.format((const char *)format,(unsigned int)arg.toInt());
              break;
            case 'f':
            case 'g':
            case 'e':
              {
                int endpos;
                narg.format((const char *)format, arg.toDouble(0,endpos));
                if( endpos < 0 )
                  narg = arg;
              }
              break;
            default:
              narg.format((const char *)format,(const char *)arg);
              break;
          }
          message = message.substr( 0, format_start )+narg
            +message.substr( format_end+1, -1 );
        }else
        {
          message = message.substr( 0, format_start )+arg
            +message.substr( format_end+1, -1 );
        }
      }
      format_start=message.search((const char*)target, format_start+arg.length());
    } while(format_start >= 0);
  }
  else
  {
    //  Not found, fake it
    if( ArgId != 0 )
    {
      message += "\n"+LookUpSingle(uparameter+("\t"+arg));
    }
  }
}


//  A C function to perform a message lookup. Arguments are a buffer to received the
//  translated message, a buffer size (bytes), and a message_list. The translated
//  result is returned in msg_buffer encoded in UTF-8. In case of error, msg_buffer is
//  empty (i.e., msg_buffer[0] == '\0').
void 
DjVuMessageLite_LookUp( char *msg_buffer, const unsigned int buffer_size, const char *message )
{
  GUTF8String converted = DjVuMessageLite::LookUpUTF8( message );
  if( converted.length() >= buffer_size )
    msg_buffer[0] = '\0';
  else
    strcpy( msg_buffer, converted );
}

void
DjVuMessageLite::AddByteStream(const GP<ByteStream> &bs)
{
  const GP<lt_XMLTags> gtags(lt_XMLTags::create(bs));
  lt_XMLTags &tags=*gtags;
  GPList<lt_XMLTags> Bodies=tags.get_Tags(bodystring);
  if(! Bodies.isempty())
  {
    lt_XMLTags::get_Maps(messagestring,namestring,Bodies,Map);
  }
}



#ifdef HAVE_NAMESPACES
}
# ifndef NOT_USING_DJVU_NAMESPACE
using namespace DJVU;
# endif
#endif

void
DjVuWriteError( const char *message )
{
  G_TRY {
    GP<ByteStream> errout = ByteStream::get_stderr();
    if (errout)
      {
        const GUTF8String external = DjVuMessageLite::LookUpUTF8( message );
        errout->writestring(external+"\n");
      }
    // Need to catch all exceptions because these might be 
    // called from an outer exception handler (with prejudice)
  } G_CATCH_ALL { } G_ENDCATCH;
}

void
DjVuWriteMessage( const char *message )
{
  G_TRY {
    GP<ByteStream> strout = ByteStream::get_stdout();
    if (strout)
      {
        const GUTF8String external = DjVuMessageLite::LookUpUTF8( message );
        strout->writestring(external+"\n");
      }
    // Need to catch all exceptions because these might be 
    // called from an outer exception handler (with prejudice)
  } G_CATCH_ALL { } G_ENDCATCH;
}
