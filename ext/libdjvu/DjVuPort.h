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

#ifndef _DJVUPORT_H
#define _DJVUPORT_H
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#if NEED_GNUG_PRAGMAS
# pragma interface
#endif


#include "GThreads.h"
#include "GURL.h"
#include "stddef.h"

#ifdef HAVE_NAMESPACES
namespace DJVU {
# ifdef NOT_DEFINED // Just to fool emacs c++ mode
}
#endif
#endif

class DataPool;

/** @name DjVuPort.h
    Files #"DjVuPort.h"# and #"DjVuPort.cpp"# implement a communication
    mechanism between different parties involved in decoding DjVu files.
    It should be pretty clear that the creator of \Ref{DjVuDocument} and
    \Ref{DjVuFile} would like to receive some information about the progress
    of decoding, errors occurred, etc. It may also want to provide source data
    for decoders (like it's done in the plugin where the real data is downloaded
    from the net and is fed into DjVu decoders).

    Normally this functionality is implemented by means of callbacks which are
    run when a given condition comes true. Unfortunately it's not quite easy
    to implement this strategy in our case. The reason is that there may be
    more than one "client" working with the same document, and the document
    should send the information to each of the clients. This could be done by
    means of callback {\em lists}, of course, but we want to achieve more
    bulletproof results: we want to be sure that the client that we're about
    to contact is still alive, and is not being destroyed by another thread.
    Besides, we are going to call these "callbacks" from many places, from
    many different classes.  Maintaining multi-thread safe callback lists is
    very difficult.

    Finally, we want to provide some default implementation of these
    "callbacks" in the library, which should attempt to process the requests
    themselves if they can, and contact the client only if they're unable to
    do it (like in the case of \Ref{DjVuPort::request_data}() with local URL
    where \Ref{DjVuDocument} can get the data from the hard drive itself not
    disturbing the document's creator.

    Two classes implement a general communication mechanism: \Ref{DjVuPort} and
    \Ref{DjVuPortcaster}. Any sender and recipient of requests should be a
    subclass of \Ref{DjVuPort}.  \Ref{DjVuPortcaster} maintains a map of
    routes between \Ref{DjVuPort}s, which should be configured by somebody
    else. Whenever a port wants to send a request, it calls the corresponding
    function of \Ref{DjVuPortcaster}, and the portcaster relays the request to
    all the destinations that it sees in the internal map.

    The \Ref{DjVuPortcaster} is responsible for keeping the map up to date by
    getting rid of destinations that have been destroyed.  Map updates are
    performed from a single place and are serialized by a global monitor.
    
    @memo DjVu decoder communication mechanism.
    @author Andrei Erofeev <eaf@geocities.com>\\
            L\'eon Bottou <leonb@research.att.com>
*/
//@{

class DjVuPort;
class DjVuPortcaster;
class DjVuFile;

/** Base class for notification targets.
    #DjVuPort# provides base functionality for classes willing to take part in
    sending and receiving messages generated during decoding process.  You
    need to derive your class from #DjVuPort# if you want it to be able to
    send or receive requests. In addition, for receiving requests you should
    override one or more virtual function.

    {\bf Important remark} --- All ports should be allocated on the heap using
    #operator new# and immediately secured using a \Ref{GP} smart pointer.
    Ports which are not secured by a smart-pointer are not considered
    ``alive'' and never receive notifications! */

class DJVUAPI DjVuPort : public GPEnabled
{
public:
   DjVuPort();
   virtual ~DjVuPort();
   static void *operator new (size_t sz);
   static void operator delete(void *addr);

      /**  Use this function to get a copy of the global \Ref{DjVuPortcaster}. */
   static DjVuPortcaster *get_portcaster(void);

      /** Copy constructor. When #DjVuPort#s are copied, the portcaster
          copies all incoming and outgoing routes of the original. */
   DjVuPort(const DjVuPort & port);

      /** Copy operator. Similarly to the copy constructor, the portcaster
          copies all incoming and outgoing coming routes of the original. */
   DjVuPort & operator=(const DjVuPort & port);

      /** Should return 1 if the called class inherits class #class_name#.
	  When a destination receives a request, it can retrieve the pointer
	  to the source #DjVuPort#. This virtual function should be able
	  to help to identify the source of the request. For example,
	  \Ref{DjVuFile} is also derived from #DjVuPort#. In order for
	  the receiver to recognize the sender, the \Ref{DjVuFile} should
	  override this function to return #TRUE# when the #class_name#
	  is either #DjVuPort# or #DjVuFile# */
   virtual bool		inherits(const GUTF8String &class_name) const;

      /** @name Notifications. 
          These virtual functions may be overridden by the subclasses
          of #DjVuPort#.  They are called by the \Ref{DjVuPortcaster}
          when the port is alive and when there is a route between the 
          source of the notification and this port. */
      //@{

      /** This request is issued to request translation of the ID, used
	  in an DjVu INCL chunk to a URL, which may be used to request
	  data associated with included file. \Ref{DjVuDocument} usually
	  intercepts all such requests, and the user doesn't have to
	  worry about the translation */
   virtual GURL		id_to_url(const DjVuPort * source, const GUTF8String &id);

      /** This request is used to get a file corresponding to the
	  given ID. \Ref{DjVuDocument} is supposed to intercept it
	  and either create a new instance of \Ref{DjVuFile} or reuse
	  an existing one from the cache. */
   virtual GP<DjVuFile>	id_to_file(const DjVuPort * source, const GUTF8String &id);

      /** This request is issued when decoder needs additional data
	  for decoding.  Both \Ref{DjVuFile} and \Ref{DjVuDocument} are
	  initialized with a URL, not the document data.  As soon as
	  they need the data, they call this function, whose responsibility
	  is to locate the source of the data basing on the #URL# passed
	  and return it back in the form of the \Ref{DataPool}. If this
	  particular receiver is unable to fullfil the request, it should
	  return #0#. */
   virtual GP<DataPool>	request_data(const DjVuPort * source, const GURL & url);

      /** This notification is sent when an error occurs and the error message
	  should be shown to the user.  The receiver should return #0# if it is 
          unable to process the request. Otherwise the receiver should return 1. */
   virtual bool		notify_error(const DjVuPort * source, const GUTF8String &msg);

      /** This notification is sent to update the decoding status.  The
          receiver should return #0# if it is unable to process the
          request. Otherwise the receiver should return 1. */
   virtual bool		notify_status(const DjVuPort * source, const GUTF8String &msg);

      /** This notification is sent by \Ref{DjVuImage} when it should be
	  redrawn. It may be used to implement progressive redisplay.

	  @param source The sender of the request */
   virtual void		notify_redisplay(const class DjVuImage * source);

      /** This notification is sent by \ref{DjVuImage} when its geometry
	  has been changed as a result of decoding. It may be used to
	  implement progressive redisplay. */
   virtual void		notify_relayout(const class DjVuImage * source);

      /** This notification is sent when a new chunk has been decoded. */
   virtual void		notify_chunk_done(const DjVuPort * source, const GUTF8String &name);

      /** This notification is sent after the \Ref{DjVuFile} flags have
	  been changed. This happens, for example, when:
	  \begin{itemize}
	    \item Decoding succeeded, failed or just stopped
	    \item All data has been received
	    \item All included files have been created
	  \end{itemize}
	  
	  @param source \Ref{DjVuFile}, which flags have been changed
	  @param set_mask bits, which have been set
	  @param clr_mask bits, which have been cleared */
   virtual void		notify_file_flags_changed(const class DjVuFile * source,
						  long set_mask, long clr_mask);

      /** This notification is sent after the \Ref{DjVuDocument} flags have
	  been changed. This happens, for example, after it receives enough
	  data and can determine its structure (#BUNDLED#, #OLD_INDEXED#, etc.).

	  @param source \Ref{DjVuDocument}, which flags have been changed
	  @param set_mask bits, which have been set
	  @param clr_mask bits, which have been cleared */
   virtual void		notify_doc_flags_changed(const class DjVuDocument * source,
						 long set_mask, long clr_mask);
   
      /** This notification is sent from time to time while decoding is in
	  progress. The purpose is obvious: to provide a way to know how much
	  is done and how long the decoding will continue.  Argument #done# is
	  a number from 0 to 1 reflecting the progress. */
   virtual void		notify_decode_progress(const DjVuPort * source, float done);

      /** This is the standard types for defining what to do in case of errors.
          This is only used by some of the subclasses, but it needs to be 
          defined here to guarantee all subclasses use the same enum types.
          In general, many errors are non recoverable.  Using a setting
          other than ABORT may just result in even more errors. */
   enum ErrorRecoveryAction {ABORT=0,SKIP_PAGES=1,SKIP_CHUNKS=2,KEEP_ALL=3 }; 
      //@}
public:
   class DjVuPortCorpse;
private:
   static GCriticalSection	* corpse_lock;
   static DjVuPortCorpse	* corpse_head;
   static DjVuPortCorpse        * corpse_tail;
   static int			corpse_num;
};

/** Simple port.  
    An instance of #DjVuSimplePort# is automatically created when you create a
    \Ref{DjVuFile} or a \Ref{DjVuDocument} without specifying a port.  This
    simple port can retrieve data for local urls (i.e. urls referring to local
    files) and display error messages on #stderr#.  All other notifications
    are ignored. */

class DJVUAPI DjVuSimplePort : public DjVuPort
{
public:
      /// Returns 1 if #class_name# is #"DjVuPort"# or #"DjVuSimplePort"#.
   virtual bool		inherits(const GUTF8String &class_name) const;

      /** If #url# is local, it created a \Ref{DataPool}, connects it to the
	  file with the given name and returns.  Otherwise returns #0#. */
   virtual GP<DataPool>	request_data(const DjVuPort * source, const GURL & url);

      /// Displays error on #stderr#. Always returns 1.
   virtual bool		notify_error(const DjVuPort * source, const GUTF8String &msg);
   
      /// Displays status on #stderr#. Always returns 1.
   virtual bool		notify_status(const DjVuPort * source, const GUTF8String &msg);
};


/** Memory based port.
    This \Ref{DjVuPort} maintains a map associating pseudo urls with data
    segments.  It processes the #request_data# notifications according to this
    map.  After initializing the port, you should add as many pairs #<url,
    pool># as needed need and add a route from a \Ref{DjVuDocument} or
    \Ref{DjVuFile} to this port. */

class DJVUAPI DjVuMemoryPort : public DjVuPort
{
public:
      /// Returns 1 if #class_name# is #"DjVuPort"# or #"DjVuMemoryPort"#
   virtual bool		inherits(const GUTF8String &class_name) const;

      /** If #url# is one of those, that have been added before by means
	  of \Ref{add_data}() function, it will return the associated
	  \Ref{DataPool}. #ZERO# otherwize. */
   virtual GP<DataPool>	request_data(const DjVuPort * source, const GURL & url);

      /** Adds #<url, pool># pair to the internal map. From now on, if
	  somebody asks for data corresponding to the #url#, it will
	  be returning the #pool# */
   void		add_data(const GURL & url, const GP<DataPool> & pool);
private:
   GCriticalSection	lock;
   GPMap<GURL, DataPool>map;
};



/** Maintains associations between ports.
    It monitors the status of all ports (have they been destructed yet?),
    accepts requests and notifications from them and forwards them to
    destinations according to internally maintained map of routes.

    The caller can modify the route map any way he likes (see
    \Ref{add_route}(), \Ref{del_route}(), \Ref{copy_routes}(),
    etc. functions). Any port can be either a sender of a message, an
    intermediary receiver or a final destination.  

    When a request is sent, the #DjVuPortcaster# computes the list of
    destinations by consulting with the route map.  Notifications are only
    sent to ``alive'' ports.  A port is alive if it is referenced by a valid
    \Ref{GP} smartpointer.  As a consequence, a port usually becomes alive
    after running the constructor (since the returned pointer is then assigned
    to a smartpointer) and is no longer alive when the port is destroyed
    (because it would not be destroyed if a smartpointer was referencing it).

    Destination ports are sorted according to their distance from the source.
    For example, if port {\bf A} is connected to ports {\bf B} and {\bf C}
    directly, and port {\bf B} is connected to {\bf D}, then {\bf B} and {\bf
    C} are assumed to be one hop away from {\bf A}, while {\bf D} is two hops
    away from {\bf A}.

    In some cases the requests and notifications are sent to every possible
    destination, and the order is not significant (like it is for
    \Ref{notify_file_flags_changed}() request). Others should be sent to the closest
    destinations first, and only then to the farthest, in case if they have
    not been processed by the closest. The examples are \Ref{request_data}(),
    \Ref{notify_error}() and \Ref{notify_status}().

    The user is not expected to create the #DjVuPortcaster# itself. He should
    use \Ref{get_portcaster}() global function instead.  */
class DJVUAPI DjVuPortcaster
{
public:
      /**  Use this function to get a copy of the global \Ref{DjVuPortcaster}. */
   static DjVuPortcaster *get_portcaster(void)
    { return DjVuPort::get_portcaster(); } ;

      /** The default constructor. */
   DjVuPortcaster(void);

   virtual ~DjVuPortcaster(void);

      /** Removes the specified port from all routes. It will no longer
	  be able to receive or generate messages and will be considered
    {\bf "dead"} by \Ref{is_port_alive}() function. */
   void		del_port(const DjVuPort * port);
   
      /** Adds route from #src# to #dst#. Whenever a request is
	  sent or received by #src#, it will be forwarded to #dst# as well.
	  @param src The source
	  @param dst The destination */
   void		add_route(const DjVuPort *src, DjVuPort *dst);

      /** The opposite of \Ref{add_route}(). Removes the association
	  between #src# and #dst# */
   void		del_route(const DjVuPort *src, DjVuPort *dst);

      /** Copies all incoming and outgoing routes from #src# to
	  #dst#. This function should be called when a \Ref{DjVuPort} is
	  copied, if you want to preserve the connectivity. */
   void		copy_routes(DjVuPort *dst, const DjVuPort *src);

      /** Returns a smart pointer to the port if #port# is a valid pointer
          to an existing #DjVuPort#.  Returns a null pointer otherwise. */
   GP<DjVuPort> is_port_alive(DjVuPort *port);

      /** Assigns one more {\em alias} for the specified \Ref{DjVuPort}.
	  {\em Aliases} are names, which can be used later to retrieve this
	  \Ref{DjVuPort}, if it still exists. Any \Ref{DjVuPort} may have
	  more than one {\em alias}. But every {\em alias} must correspond
	  to only one \Ref{DjVuPort}. Thus, if the specified alias is
	  already associated with another port, this association will be
	  removed. */
   void		add_alias(const DjVuPort * port, const GUTF8String &alias);

      /** Removes all the aliases */
   static void		clear_all_aliases(void);

      /** Removes all aliases associated with the given \Ref{DjVuPort}. */
   void		clear_aliases(const DjVuPort * port);

      /** Returns \Ref{DjVuPort} associated with the given #alias#. If nothing
	  is known about name #alias#, or the port associated with it has
	  already been destroyed #ZERO# pointer will be returned. */
   GP<DjVuPort>	alias_to_port(const GUTF8String &name);

      /** Returns a list of \Ref{DjVuPort}s with aliases starting with
	  #prefix#. If no \Ref{DjVuPort}s have been found, empty
	  list is returned. */
   GPList<DjVuPort>	prefix_to_ports(const GUTF8String &prefix);

      /** Computes destination list for #source# and calls the corresponding
	  function in each of the ports from the destination list starting from
	  the closest until one of them returns non-empty \Ref{GURL}. */
   virtual GURL		id_to_url(const DjVuPort * source, const GUTF8String &id);

      /** Computes destination list for #source# and calls the corresponding
	  function in each of the ports from the destination list starting from
	  the closest until one of them returns non-zero pointer to
	  \Ref{DjVuFile}. */
   virtual GP<DjVuFile>	id_to_file(const DjVuPort * source, const GUTF8String &id);

      /** Computes destination list for #source# and calls the corresponding
	  function in each of the ports from the destination list starting from
	  the closest until one of them returns non-zero \Ref{DataPool}. */
   virtual GP<DataPool>	request_data(const DjVuPort * source, const GURL & url);

      /** Computes destination list for #source# and calls the corresponding.
	  function in each of the ports from the destination starting from
	  the closest until one of them returns 1. */
   virtual bool		notify_error(const DjVuPort * source, const GUTF8String &msg);

      /** Computes destination list for #source# and calls the corresponding
	  function in each of the ports from the destination list starting from
	  the closest until one of them returns 1. */
   virtual bool		notify_status(const DjVuPort * source, const GUTF8String &msg);

      /** Computes destination list for #source# and calls the corresponding
	  function in each of the ports from the destination list starting from
	  the closest. */
   virtual void		notify_redisplay(const class DjVuImage * source);

      /** Computes destination list for #source# and calls the corresponding
	  function in each of the ports from the destination list starting from
	  the closest. */
   virtual void		notify_relayout(const class DjVuImage * source);

      /** Computes destination list for #source# and calls the corresponding
	  function in each of the ports from the destination list starting from
	  the closest. */
   virtual void		notify_chunk_done(const DjVuPort * source, const GUTF8String &name);

      /** Computes destination list for #source# and calls the corresponding
	  function in each of the ports from the destination list starting from
	  the closest. */
   virtual void		notify_file_flags_changed(const class DjVuFile * source,
						  long set_mask, long clr_mask);

      /** Computes destination list for #source# and calls the corresponding
	  function in each of the ports from the destination list starting from
	  the closest. */
   virtual void		notify_doc_flags_changed(const class DjVuDocument * source,
						 long set_mask, long clr_mask);
   
      /** Computes destination list for #source# and calls the corresponding
	  function in each of the ports from the destination list starting from
	  the closest. */
   virtual void		notify_decode_progress(const DjVuPort * source, float done);

private:
      // We use these 'void *' to minimize template instantiations.
   friend class DjVuPort;
   GCriticalSection		map_lock;
   GMap<const void *, void *>	route_map;	// GMap<DjVuPort *, GList<DjVuPort *> *>
   GMap<const void *, void *>	cont_map;	// GMap<DjVuPort *, DjVuPort *>
   GMap<GUTF8String, const void *>	a2p_map;	// GMap<GUTF8String, DjVuPort *>
   void add_to_closure(GMap<const void*, void*> & set,
                       const DjVuPort *dst, int distance);
   void compute_closure(const DjVuPort *src, GPList<DjVuPort> &list,
                        bool sorted=false);
};


inline bool
DjVuPort::inherits(const GUTF8String &class_name) const
{
   return (class_name == "DjVuPort");
}

inline bool
DjVuSimplePort::inherits(const GUTF8String &class_name) const
{
   return
      (class_name == "DjVuSimplePort") || DjVuPort::inherits(class_name);
}

inline bool
DjVuMemoryPort::inherits(const GUTF8String &class_name) const
{
   return
      (class_name == "DjVuMemoryPort") || DjVuPort::inherits(class_name);
}

//@}


#ifdef HAVE_NAMESPACES
}
# ifndef NOT_USING_DJVU_NAMESPACE
using namespace DJVU;
# endif
#endif
#endif
