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

#ifndef _DJVUFILECACHE_H
#define _DJVUFILECACHE_H
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#if NEED_GNUG_PRAGMAS
# pragma interface
#endif


#include "DjVuFile.h"

#ifndef macintosh //MCW can't compile
# ifndef UNDER_CE
#  include <sys/types.h>
#  include <time.h>
# endif 
#else
# include <time.h>
#endif


#ifdef HAVE_NAMESPACES
namespace DJVU {
# ifdef NOT_DEFINED // Just to fool emacs c++ mode
}
#endif
#endif

/** @name DjVuFileCache.h
    Files #"DjVuFileCache.h"# and #"DjVuFileCache.cpp"# implement a simple
    caching mechanism for keeping a given number of \Ref{DjVuFile} instances
    alive. The cache estimates the size of its elements and gets rid of
    the oldest ones when necessary.

    See \Ref{DjVuFileCache} for details.
    
    @memo Simple DjVuFile caching class.
    @author Andrei Erofeev <eaf@geocities.com>
*/

//@{

/** #DjVuFileCache# is a simple list of \Ref{DjVuFile} instances. It keeps
    track of the total size of all elements and can get rid of the oldest
    one once the total size becomes over some threshold. Its main purpose
    is to keep the added \Ref{DjVuFile} instances alive until their size
    exceeds some given threshold (set by \Ref{set_maximum_size}() function).
    The user is supposed to use \Ref{DjVuPortcaster::name_to_port}() to
    find a file corresponding to a given name. The cache provides no
    naming services */
#ifdef UNDER_CE
class DjVuFileCache : public GPEnabled
{
protected:
   DjVuFileCache(const int) {}
public:
   static GP<DjVuFileCache> create(const int);
   virtual ~DjVuFileCache(void);
   void del_file(const DjVuFile *) {}
   void add_file(const GP<DjVuFile> &) {}
   void clear(void) {}
   void set_max_size(int) {}
   int get_max_size(void) const {return 0;}
   void enable(bool en) {}
   bool is_enabled(void) const {return false;}
} ;
#else
class DjVuFileCache : public GPEnabled
{
protected:
   DjVuFileCache(const int max_size=5*2*1024*1024);
public:
      /** Constructs the #DjVuFileCache#
	  @param max_size Maximum allowed size of the cache in bytes. */
   static GP<DjVuFileCache> create(const int max_size=5*2*1024*1024);

   virtual ~DjVuFileCache(void);

      /** Removes file #file# from the cache */
   void		del_file(const DjVuFile * file);

      /** Adds the given file to the cache. It it's already there, its
	  timestamp will be refreshed. */
   void		add_file(const GP<DjVuFile> & file);

      /** Clears the cache. All items will be deleted. */
   void		clear(void);
      /** Sets new maximum size. If the total size of all items in the cache
	  is greater than #max_size#, the cache will be deleting the oldest
	  items until the size is OK. */
   void		set_max_size(int max_size);

      /** Returns the maximum allowed size of the cache. */
   int		get_max_size(void) const;

      /** Enables or disables the cache. See \Ref{is_enabled}() for details
	  @param en - If {\em en} is TRUE, the cache will be enabled.
	         Otherwise it will be disabled.
	*/
   void		enable(bool en);

      /** Returns #TRUE# if the cache is enabled, #FALSE# otherwise.
	  When a cache is disabled, \Ref{add_file}(), and
	  \Ref{del_file}() do nothing. But the {\em maximum size} is preserved
	  inside the class so that next time the cache is enabled, it will
	  be configured the same way. Clearly this "enable/disable" thing is
	  for convenience only. One could easily simulate this behavior by
	  setting the {\em maximum size} of the cache to #ZERO#. */
   bool		is_enabled(void) const;

public:
   class Item;
   
   class Item : public GPEnabled
	{
	public:
	   virtual ~Item(void);
	   time_t		get_time(void) const;
	   GP<DjVuFile>	get_file(void) const;
	   unsigned int	get_size(void) const;
	   void		refresh(void);

	public:
	   GP<DjVuFile>	file;
	   time_t		time;
	   static int	qsort_func(const void * el1, const void * el2);
	   Item(void);
	   Item(const GP<DjVuFile> & xfile);
	};

protected:
   GCriticalSection	class_lock;
   
      /** This function is called right after the given file has been added
	  to the cache for management. */
   virtual void	file_added(const GP<DjVuFile> & file);
      /** This function is called when the given file is no longer
	  managed by the cache. */
   virtual void	file_deleted(const GP<DjVuFile> & file);
      /** This function is called when after the cache decides to get rid
	  of the file. */
   virtual void	file_cleared(const GP<DjVuFile> & file);

   GPList<Item>	get_items(void);
private:
   GPList<Item>	list;
   bool		enabled;
   int		max_size;
   int		cur_size;

   int		calculate_size(void);
   void		clear_to_size(int size);
};



//@}
   
inline
DjVuFileCache::Item::Item(void) : time(::time(0)) {};

inline
DjVuFileCache::Item::Item(const GP<DjVuFile> & xfile) :
      file(xfile), time(::time(0)) {}

inline
DjVuFileCache::Item::~Item(void) {}

inline GP<DjVuFile>
DjVuFileCache::Item::get_file(void) const
{
   return file;
}

inline unsigned int
DjVuFileCache::Item::get_size(void) const
{
   return file->get_memory_usage();
}

inline time_t
DjVuFileCache::Item::get_time(void) const
{
   return time;
}

inline void
DjVuFileCache::Item::refresh(void)
{
   time=::time(0);
}

inline
DjVuFileCache::DjVuFileCache(const int xmax_size) :
      enabled(true), max_size(xmax_size), cur_size(0) {}

inline void
DjVuFileCache::clear(void)
{
   clear_to_size(0);
}

inline bool
DjVuFileCache::is_enabled(void) const
{
   return enabled;
}

inline int
DjVuFileCache::get_max_size(void) const
{
   return max_size;
}

#endif

inline GP<DjVuFileCache>
DjVuFileCache::create(const int max_size)
{
  return new DjVuFileCache(max_size);
}


#ifdef HAVE_NAMESPACES
}
# ifndef NOT_USING_DJVU_NAMESPACE
using namespace DJVU;
# endif
#endif
#endif
