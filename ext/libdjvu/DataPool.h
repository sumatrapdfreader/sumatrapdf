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

#ifndef _DATAPOOL_H
#define _DATAPOOL_H
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#if NEED_GNUG_PRAGMAS
# pragma interface
#endif


#include "GThreads.h"
#include "GString.h"
#include "GURL.h"

#ifdef HAVE_NAMESPACES
namespace DJVU {
# ifdef NOT_DEFINED // Just to fool emacs c++ mode
}
#endif
#endif

class ByteStream;

/** @name DataPool.h
    Files #"DataPool.h"# and #"DataPool.cpp"# implement classes \Ref{DataPool}
    and \Ref{DataRange} used by DjVu decoder to access data.

    The main goal of class \Ref{DataPool} is to provide concurrent access
    to the same data from many threads with a possibility to add data
    from yet another thread. It is especially important in the case of the
    Netscape plugin when data is not immediately available, but decoding
    should be started as soon as possible. In this situation it is vital
    to provide transparent access to the data from many threads possibly
    blocking readers that try to access information that has not been
    received yet.

    When the data is local though, it can be accessed directly using
    standard IO mechanism. To provide a uniform interface for decoding
    routines, \Ref{DataPool} supports file mode as well.

    @memo Thread safe data storage
    @author Andrei Erofeev <eaf@geocities.com>
*/

//@{

/** Thread safe data storage.
    The purpose of #DataPool# is to provide a uniform interface for
    accessing data from decoding routines running in a multi-threaded
    environment. Depending on the mode of operation it may contain the
    actual data, may be connected to another #DataPool# or may be mapped
    to a file. Regardless of the mode, the class returns data in a
    thread-safe way, blocking reading threads if there is no data of
    interest available. This blocking is especially useful in the
    networking environment (plugin) when there is a running decoding thread,
    which wants to start decoding as soon as there is just one byte available
    blocking if necessary.

    Access to data in a #DataPool# may be direct (Using \Ref{get_data}()
    function) or sequential (See \Ref{get_stream}() function).

    If the #DataPool# is not connected to anything, that is it contains
    some real data, this data can be added to it by means of two
    \Ref{add_data}() functions. One of them adds data sequentially maintaining
    the offset of the last block of data added by it. The other can store
    data anywhere. Thus it's important to realize, that there may be "white
    spots" in the data storage.

    There is also a way to test if data is available for some given data
    range (See \Ref{has_data}()). In addition to this mechanism, there are
    so-called {\em trigger callbacks}, which are called, when there is
    all data available for a given data range.

    Let us consider all modes of operation in details:

    \begin{enumerate}
       \item {\bf Not connected #DataPool#}. In this mode the #DataPool#
             contains some real data. As mentioned above, it may be added  
             by means of two functions \Ref{add_data}() operating independent
	     of each other and allowing to add data sequentially and
	     directly to any place of data storage. It's important to call
	     function \Ref{set_eof}() after all data has been added.

	     Functions like \Ref{get_data}() or \Ref{get_stream}() can
	     be used to obtain direct or sequential access to the data. As
	     long as \Ref{is_eof}() is #FALSE#, #DataPool# will block every
	     reader, which is trying to read unavailable data until it
	     really becomes available. But as soon as \Ref{is_eof}() is
	     #TRUE#, any attempt to read non-existing data will read #0# bytes.

	     Taking into account the fact, that #DataPool# was designed to
	     store DjVu files, which are in IFF formats, it becomes possible
	     to predict the size of the #DataPool# as soon as the first
	     #32# bytes have been added. This is invaluable for estimating
	     download progress. See function \Ref{get_length}() for details.
	     If this estimate fails (which means, that stored data is not
	     in IFF format), \Ref{get_length}() returns #-1#.

	     Triggers may be added and removed by means of \Ref{add_trigger}()
	     and \Ref{del_trigger}() functions. \Ref{add_trigger}() takes
	     a data range. As soon as all data in that data range is
	     available, the trigger callback will be called.

	     All trigger callbacks will be called when #EOF# condition
	     has been set.

       \item {\bf #DataPool# connected to another #DataPool#}. In this
             {\em slave} mode you can map a given #DataPool# to any offsets
	     range inside another #DataPool#. You can connect the slave
	     #DataPool# even if there is no data in the master #DataPool#.
	     Any \Ref{get_data}() request will be forwarded to the master
	     #DataPool#, and it will be responsible for blocking readers
	     trying to access unavailable data.

	     The usage of \Ref{add_data}() functions is prohibited for
	     connected #DataPool#s.

	     The offsets range used to map a slave #DataPool# can be fully
	     specified (both start offset and length are positive numbers)
	     or partially specified (the length is negative). In this mode
	     the slave #DataPool# is assumed to extend up to the end
	     of the master #DataPool#.

	     Triggers may be used with slave #DataPool#s as well as with
	     the master ones.

	     Calling \Ref{stop}() function of a slave will stop only the slave
	     (and any other slave connected to it), but not the master.

	     \Ref{set_eof}() function is meaningless for slaves. They obtain
	     the #ByteStream::EndOfFile# status from their master.

	     Depending on the offsets range passed to the constructor,
	     \Ref{get_length}() returns different values. If the length
	     passed to the constructor was positive, then it is returned
	     by \Ref{get_length}() all the time. Otherwise the value returned
	     is either #-1# if master's length is still unknown (it didn't
	     manage to parse IFF data yet) or it is calculated as
	     #masters_length-slave_start#.

       \item {\bf #DataPool# connected to a file}. This mode is quite similar
             to the case, when the #DataPool# is connected to another
	     #DataPool#. Similarly, the #DataPool# stores no data inside.
	     It just forwards all \Ref{get_data}() requests to the underlying
	     source (a file in this case). Thus these requests will never
	     block the reader. But they may return #0# if there is no data
	     available at the requested offset.

	     The usage of \Ref{add_data}() functions is meaningless and
	     is prohibited.

	     \Ref{is_eof}() function always returns #TRUE#. Thus \Ref{set_eof}()
	     us meaningless and does nothing.

	     \Ref{get_length}() function always returns the file size.

	     Calling \Ref{stop}() function will stop this #DataPool# and
	     any other slave connected to it.

	     Trigger callbacks passed through \Ref{add_trigger}() function
	     are called immediately.

	     This mode is useful to read and decode DjVu files without reading
	     and storing them in full in memory.
    \end{enumerate}
*/

class DJVUAPI DataPool : public GPEnabled
{
public: // Classes used internally by DataPool
	// These are declared public to support buggy C++ compilers.
   class Incrementor;
   class Reader;
   class Trigger;
   class OpenFiles;
   class OpenFiles_File;
   class BlockList;
   class Counter;
protected:
   DataPool(void);

public:
      /** @name Initialization */
      //@{
      /** Default creator. Will prepare #DataPool# for accepting data
	  added through functions \Ref{add_data}(). Use \Ref{connect}()
	  functions if you want to map this #DataPool# to another or
	  to a file. */
   static GP<DataPool> create(void);

      /** Creates and initialized the #DataPool# with data from stream #str#.
	  The constructor will read the stream's contents and add them
	  to the pool using the \Ref{add_data}() function. Afterwards it
	  will call \Ref{set_eof}() function, and no other data will be
	  allowed to be added to the pool. */
   static GP<DataPool> create(const GP<ByteStream> & str);

      /** Initializes the #DataPool# in slave mode and connects it
	  to the specified offsets range of the specified master #DataPool#.
	  It is equivalent to calling default constructor and function
	  \Ref{connect}().

	  @param master_pool Master #DataPool# providing data for this slave
	  @param start Beginning of the offsets range which the slave is
	         mapped into
          @param length Length of the offsets range. If negative, the range
	         is assumed to extend up to the end of the master #DataPool#.
      */
   static GP<DataPool> create(const GP<DataPool> & master_pool, int start=0, int length=-1);

      /** Initializes the #DataPool# in slave mode and connects it
	  to the specified offsets range of the specified file.
	  It is equivalent to calling default constructor and function
	  \Ref{connect}().
	  @param url Name of the file to connect to.
	  @param start Beginning of the offsets range which the #DataPool# is
	         mapped into
          @param length Length of the offsets range. If negative, the range
	         is assumed to extend up to the end of the file.
      */
   static GP<DataPool> create(const GURL &url, int start=0, int length=-1);

   virtual ~DataPool();

      /** Switches the #DataPool# to slave mode and connects it to the
	  specified offsets range of the master #DataPool#.
	  @param master_pool Master #DataPool# providing data for this slave
	  @param start Beginning of the offsets range which the slave is
	         mapped into
          @param length Length of the offsets range. If negative, the range
	         is assumed to extend up to the end of the master #DataPool#.
      */
   void		connect(const GP<DataPool> & master_pool, int start=0, int length=-1);
      /** Connects the #DataPool# to the specified offsets range of
	  the named #url#.
	  @param url Name of the file to connect to.
	  @param start Beginning of the offsets range which the #DataPool# is
	         mapped into
          @param length Length of the offsets range. If negative, the range
	         is assumed to extend up to the end of the file.
      */
   void		connect(const GURL &url, int start=0, int length=-1);
      //@}

      /** Tells the #DataPool# to stop serving readers.

	  If #only_blocked# flag is #TRUE# then only those requests will
	  be processed, which would not block. Any attempt to get non-existing
	  data would result in a #STOP# exception (instead of blocking until
	  data is available).

	  If #only_blocked# flag is #FALSE# then any further attempt to read
	  from this #DataPool# (as well as from any #DataPool# connected
	  to this one) will result in a #STOP# exception. */
   void		stop(bool only_blocked=false);

      /** @name Adding data.
	  Please note, that these functions are for not connected #DataPool#s
	  only. You can not add data to a #DataPool#, which is connected
	  to another #DataPool# or to a file.
	*/
      //@{
      /** Appends the new block of data to the #DataPool#. There are two
	  \Ref{add_data}() functions available. One is for adding data
	  sequentially. It keeps track of the last byte position, which has
	  been stored {\bf by it} and always appends the next block after
	  this position. The other \Ref{add_data}() can store data anywhere.
	  
	  The function will unblock readers waiting for data if this data
	  arrives with this block. It may also trigger some {\em trigger
	  callbacks}, which may have been added by means of \Ref{add_trigger}()
	  function.

	  {\bf Note:} After all the data has been added, it's necessary
	  to call \Ref{set_eof}() to tell the #DataPool# that nothing else
	  is expected.

	  {\bf Note:} This function may not be called if the #DataPool#
	  has been connected to something.

	  @param buffer data to append
	  @param size length of the {\em buffer}
      */
   void		add_data(const void * buffer, int size);

      /** Stores the specified block of data at the specified offset.
	  Like the function above this one can also unblock readers
	  waiting for data and engage trigger callbacks. The difference
	  is that {\bf this} function can store data anywhere.

	  {\bf Note:} After all the data has been added, it's necessary
	  to call \Ref{set_eof}() to tell the #DataPool# that nothing else
	  is expected.

	  {\bf Note:} This function may not be called if the #DataPool#
	  has been connected to something.

	  @param buffer data to store
	  @param offset where to store the data
	  @param size length of the {\em buffer} */
   void		add_data(const void * buffer, int offset, int size);

      /** Tells the #DataPool# that all data has been added and nothing else
	  is anticipated. When #EOF# is true, any reader attempting to read
	  non existing data will not be blocked. It will either read #ZERO#
	  bytes or will get an #ByteStream::EndOfFile# exception (see \Ref{get_data}()).
	  Calling this function will also activate all registered trigger
	  callbacks.

	  {\bf Note:} This function is meaningless and does nothing
	  when the #DataPool# is connected to another #DataPool# or to
	  a file. */
   void		set_eof(void);
      //@}

      /** @name Accessing data.
	  These functions provide direct and sequential access to the
	  data of the #DataPool#. If the #DataPool# is not connected
	  (contains some real data) then it handles the requests itself.
	  Otherwise they are forwarded to the master #DataPool# or the file.
	*/
      //@{
      /** Attempts to return a block of data at the given #offset#
	  of the given #size#.

	  \begin{enumerate}
	     \item If the #DataPool# is connected to another #DataPool# or
	           to a file, the request will just be forwarded to them.
	     \item If the #DataPool# is not connected to anything and
	           some of the data requested is in the internal buffer,
		   the function copies available data to #buffer# and returns
		   immediately.

		   If there is no data available, and \Ref{is_eof}() returns
		   #FALSE#, the reader (and the thread) will be {\bf blocked}
		   until the data actually arrives. Please note, that since
		   the reader is blocked, it should run in a separate thread
		   so that other threads have a chance to call \Ref{add_data}().
		   If there is no data available, but \Ref{is_eof}() is #TRUE#
		   the behavior is different and depends on the #DataPool#'s
		   estimate of the file size:
		   \begin{itemize}
		      \item If #DataPool# learns from the IFF structure of the
		            data, that its size should be greater than it
			    really is, then any attempt to read non-existing
			    data in the range of {\em valid} offsets will
			    result in an #ByteStream::EndOfFile# exception. This is done to
			    indicate, that there was an error in adding data,
			    and the data requested is {\bf supposed} to be
			    there, but has actually not been added.
		      \item If #DataPool#'s expectations about the data size
		            coincide with the reality then any attempt to
			    read data beyond the legal range of offsets will
			    result in #ZERO# bytes returned.
		   \end{itemize}.
          \end{enumerate}.

	  @param buffer Buffer to be filled with data
	  @param offset Offset in the #DataPool# to read data at
	  @param size Size of the {\em buffer}
	  @return The number of bytes actually read
	  @exception STOP The stream has been stopped
	  @exception EOF The requested data is not there and will not be added,
	             although it should have been.
      */
   int		get_data(void * buffer, int offset, int size);

      /** Returns a \Ref{ByteStream} to access contents of the #DataPool#
	  sequentially. By reading from the returned stream you basically
          call \Ref{get_data}() function. Thus, everything said for it
	  remains true for the stream too. */
   GP<ByteStream>	get_stream(void);
      //@}

      /** @name State querying functions. */
      //@{
      /** Returns #TRUE# if this #DataPool# is connected to another #DataPool#
	  or to a file. */
   bool		is_connected(void) const;
   
      /** Returns #TRUE# if all data available for offsets from
	  #start# till #start+length-1#. If #length# is negative, the
          range is assumed to extend up to the end of the #DataPool#.
	  This function works both for connected and not connected #DataPool#s.
	  Once it returned #TRUE# for some offsets range, you can be
	  sure that the subsequent \Ref{get_data}() request will not block.
      */
   bool		has_data(int start, int length);

      /* Returns #TRUE# if no more data is planned to be added.

	 {\bf Note:} This function always returns #TRUE# when the #DataPool#
	 has been initialized with a file name. */
   bool		is_eof(void) const {return eof_flag;}

      /** Returns the {\em length} of data in the #DataPool#. The value
	  returned depends on the mode of operation:
	  \begin{itemize}
	     \item If the #DataPool# is not connected to anything then
	           the length returned is either calculated by interpreting
		   the IFF structure of stored data (if successful) or
		   by calculating the real size of data after \Ref{set_eof}()
		   has been called. Otherwise it is #-1#.
	     \item If the #DataPool# is connected to a file, the length
	           is calculated basing on the length passed to the
		   \Ref{connect}() function and the file size.
	     \item If the #DataPool# is connected to a master #DataPool#,
	           the length is calculated basing on the value returned
		   by the master's #get_length()# function and the length
		   passed to the \Ref{connect}() function.
	  \end{itemize}. */
   int		get_length(void) const;
      /** Returns the number of bytes of data available in this #DataPool#.
	  Contrary to the \Ref{get_length}() function, this one doesn't try
	  to interpret the IFF structure and predict the file length.
	  It just returns the number of bytes of data really available inside
	  the #DataPool#, if it contains data, or inside its range, if it's
	  connected to another #DataPool# or a file. */
   int		get_size(void) const {return get_size(0, -1);}
      //@}

      /** @name Trigger callbacks.
	  {\em Trigger callbacks} are special callbacks called when
	  all data for the given range of offsets has been made available.
	  Since reading unavailable data may result in a thread block,
	  which may be bad, the usage of {\em trigger callbacks} appears
	  to be a convenient way to signal availability of data.

	  You can add a trigger callback in two ways:
	  \begin{enumerate}
	     \item By specifying a range. This is the most general case
	     \item By providing just one {\em threshold}. In this case
	           the range is assumed to start from offset #ZERO# and
		   last for {\em threshold}+1 bytes.
	  \end{enumerate}
	*/
      //@{
      /** Associates the specified {\em trigger callback} with the
	  given data range.

	  {\bf Note:} The callback may be called immediately if all
	  data for the given range is already available or #EOF# is #TRUE#.

	  @param start The beginning of the range for which all data
	         should be available
	  @param length If the {\em length} is not negative then the callback
	         will be called when there is data available for every
		 offset from {\em start} to {\em start+length-1}.
	         If {\em thresh} is negative, the callback is called after
		 #EOF# condition has been set.
	  @param callback Function to call
	  @param cl_data Argument to pass to the callback when it's called. */
   void		add_trigger(int start, int length,
			    void (* callback)(void *), void * cl_data);

      /** Associates the specified {\em trigger callback} with the
	  specified threshold.

	  This function is a simplified version of the function above.
	  The callback will be called when there is data available for
	  every offset from #0# to #thresh#, if #thresh# is positive, or
	  when #EOF# condition has been set otherwise. */

   void		add_trigger(int thresh, 
                            void (* callback)(void *), void * cl_data);

      /** Use this function to unregister callbacks, which are no longer
	  needed. {\bf Note!} It's important to do it when the client
	  is about to be destroyed. */
   void		del_trigger(void (* callback)(void *), void *  cl_data);

      //@}

      /** Loads data from the file into memory. This function is only useful
	  for #DataPool#s getting data from a file. It descends the #DataPool#s
	  hierarchy until it either reaches a file-connected #DataPool#
	  or #DataPool# containing the real data. In the latter case it
	  does nothing, in the first case it makes the #DataPool# read all
	  data from the file into memory and stop using the file.

	  This may be useful when you want to overwrite the file and leave
	  existing #DataPool#s with valid data. */
   void		load_file(void);
      /** This function will make every #DataPool# in the program, which
	  is connected to a file, to load the file contents to the main
	  memory and close the file. This feature is important when you
	  want to do something with the file like remove or overwrite it
	  not affecting the rest of the program. */
   static void	load_file(const GURL &url);

      /** This function will remove OpenFiles filelist. */
   static void	close_all(void);

      // Internal. Used by 'OpenFiles'
   void		clear_stream(const bool release = true);

      /** Useful in comparing data pools.  Returns true if dirived from
          same URL or bytestream. */
   bool simple_compare(DataPool &pool) const;


private:
   bool		eof_flag;
   bool		stop_flag;
   bool		stop_blocked_flag;

   Counter	*active_readers;
   
      // Source or storage of data
   GP<DataPool>		pool;
   GURL		furl;
   GP<OpenFiles_File>   fstream;
   GCriticalSection	class_stream_lock;
   GP<ByteStream>	data;
   GCriticalSection	data_lock;
   BlockList		*block_list;
   int			add_at;
   int			start, length;

      // List of readers waiting for data
   GPList<Reader>	readers_list;
   GCriticalSection	readers_lock;

      // Triggers
   GPList<Trigger>	triggers_list;		// List of passed or our triggers
   GCriticalSection	triggers_lock;		// Lock for the list above
   GCriticalSection	trigger_lock;		// Lock for static_trigger_cb()

   void		init(void);
   void		wait_for_data(const GP<Reader> & reader);
   void		wake_up_all_readers(void);
   void		check_triggers(void);
   int		get_data(void * buffer, int offset, int size, int level);
   int		get_size(int start, int length) const;
   void		restart_readers(void);

//   static void	static_trigger_cb(GP<GPEnabled> &);
   static void	static_trigger_cb(void *);
   void		trigger_cb(void);
   void		analyze_iff(void);
   void		added_data(const int offset, const int size);
public:
  static const char *Stop;
  friend class FCPools;
};

inline bool 
DataPool::simple_compare(DataPool &pool) const
{
  // return true if these pools are identical.  False means they may or may
  // not be identical.
  return (this == &pool)
    ||(furl.is_valid()&&!furl.is_empty()&&pool.furl.is_valid()&&(furl == pool.furl))
    ||(data && (data == pool.data));
}

inline bool
DataPool::is_connected(void) const
{
   return furl.is_local_file_url() || pool!=0;
}

//@}


#ifdef HAVE_NAMESPACES
}
# ifndef NOT_USING_DJVU_NAMESPACE
using namespace DJVU;
# endif
#endif
#endif
