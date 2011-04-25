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

#ifndef _DJVUERRORLIST_H
#define _DJVUERRORLIST_H
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#if NEED_GNUG_PRAGMAS
# pragma interface
#endif


#include "DjVuPort.h"

#ifdef HAVE_NAMESPACES
namespace DJVU {
# ifdef NOT_DEFINED // Just to fool emacs c++ mode
}
#endif
#endif

class ByteStream;

/** @name DjVuErrorList.h
    This file implements a very simple class for redirecting port caster
    messages that would normally end up on stderr to a double linked list.

    @memo DjVuErrorList class.
    @author Bill C Riemers <docbill@sourceforge.net>
*/

//@{

/** #DjVuErrorList# provides a convenient way to redirect error messages
    from classes derived from DjVuPort to a list that can be accessed at
    any time. */

class DjVuErrorList : public DjVuSimplePort
{
protected:
     /// The normal port caster constructor. 
  DjVuErrorList(void);
public:
  static GP<DjVuErrorList> create(void) {return new DjVuErrorList();}

     /// This constructor allows the user to specify the ByteStream.
  GURL set_stream(GP<ByteStream>);

     /// Append all error messages to the list
  virtual bool notify_error(const DjVuPort * source, const GUTF8String & msg);

     /// Append all status messages to the list
  virtual bool notify_status(const DjVuPort * source, const GUTF8String & msg);

     /// Add a new class to have its messages redirected here.
  inline void connect( const DjVuPort &port);

     /// Get the listing of errors, and clear the list.
  inline GList<GUTF8String> GetErrorList(void);

     /// Just clear the list.
  inline void ClearError(void);

     /// Get one error message and clear that message from the list.
  GUTF8String GetError(void);

     /// Check if there are anymore error messages.
  inline bool HasError(void) const;

     /// Get the listing of status messages, and clear the list.
  inline GList<GUTF8String> GetStatusList(void);

     /// Just clear the list.
  inline void ClearStatus(void);

     /// Get one status message and clear that message from the list.
  GUTF8String GetStatus(void);

     /// Check if there are any more status messages.
  inline bool HasStatus(void) const;

     /** This gets the data.  We can't use the simple port's request
       data since we want to allow the user to specify the ByteStream. */
  virtual GP<DataPool> request_data (
    const DjVuPort * source, const GURL & url );

private:
  GURL pool_url;
  GP<DataPool> pool;
  GList<GUTF8String> Errors;
  GList<GUTF8String> Status;
private: //dummy stuff
  static GURL set_stream(ByteStream *);
};

inline void
DjVuErrorList::connect( const DjVuPort &port )
{ get_portcaster()->add_route(&port, this); }

inline GList<GUTF8String>
DjVuErrorList::GetErrorList(void)
{
  GList<GUTF8String> retval=(const GList<GUTF8String>)Errors;
  Errors.empty();
  return retval;
}

inline void
DjVuErrorList::ClearError(void)
{ Errors.empty(); }

inline GList<GUTF8String>
DjVuErrorList::GetStatusList(void)
{
  GList<GUTF8String> retval=(const GList<GUTF8String>)Status;
  Status.empty();
  return retval;
}

inline void
DjVuErrorList::ClearStatus(void)
{ Status.empty(); }

inline bool
DjVuErrorList::HasError(void) const
{ return !Errors.isempty(); }

inline bool
DjVuErrorList::HasStatus(void) const
{ return !Status.isempty(); }


#ifdef HAVE_NAMESPACES
}
# ifndef NOT_USING_DJVU_NAMESPACE
using namespace DJVU;
# endif
#endif
#endif
