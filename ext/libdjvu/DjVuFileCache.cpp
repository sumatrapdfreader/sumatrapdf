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

#include "DjVuFileCache.h"
#include "debug.h"

#include <stddef.h>
#include <stdlib.h>


#ifdef HAVE_NAMESPACES
namespace DJVU {
# ifdef NOT_DEFINED // Just to fool emacs c++ mode
}
#endif
#endif


DjVuFileCache::~DjVuFileCache(void) {}

int
DjVuFileCache::Item::qsort_func(const void *el1, const void *el2)
{
  const Item *item1 = *(Item**)el1;
  const Item *item2 = *(Item**)el2;
   time_t time1=item1->get_time();
   time_t time2=item2->get_time();
   return time1<time2 ? -1 : time1>time2 ? 1 : 0;
}

void
DjVuFileCache::set_max_size(int xmax_size)
{
   DEBUG_MSG("DjVuFileCache::set_max_size(): resizing to " << xmax_size << "\n");
   DEBUG_MAKE_INDENT(3);

   GCriticalSectionLock lock(&class_lock);
   
   max_size=xmax_size;
   cur_size=calculate_size();

   if (max_size>=0) clear_to_size(enabled ? max_size : 0);
}

void
DjVuFileCache::enable(bool en)
{
   enabled=en;
   set_max_size(max_size);
}

void
DjVuFileCache::add_file(const GP<DjVuFile> & file)
{
   DEBUG_MSG("DjVuFileCache::add_file(): trying to add a new item\n");
   DEBUG_MAKE_INDENT(3);

   GCriticalSectionLock lock(&class_lock);

      // See if the file is already cached
   GPosition pos;
   for(pos=list;pos;++pos)
      if (list[pos]->get_file()==file) break;
   
   if (pos) list[pos]->refresh();	// Refresh the timestamp
   else
   {
	 // Doesn't exist in the list yet
      int _max_size=enabled ? max_size : 0;
      if (max_size<0) _max_size=max_size;

      int add_size=file->get_memory_usage();
   
      if (_max_size>=0 && add_size>_max_size)
      {
	 DEBUG_MSG("but this item is way too large => doing nothing\n");
	 return;
      }

      if (_max_size>=0) clear_to_size(_max_size-add_size);

      list.append(new Item(file));
      cur_size+=add_size;
      file_added(file);
   }
}

void
DjVuFileCache::clear_to_size(int size)
{
   DEBUG_MSG("DjVuFileCache::clear_to_size(): dropping cache size to " << size << "\n");
   DEBUG_MAKE_INDENT(3);

   GCriticalSectionLock lock(&class_lock);
   
   if (size==0)
   {
      list.empty();
      cur_size=0;
     } 
   if (list.size() > 20)
      {
	    // More than 20 elements in the cache: use qsort to
	    // sort them before picking up the oldest
       GPArray<Item> item_arr(list.size()-1);
	 GPosition pos;
	 int i;
	 for(pos=list, i=0;pos;++pos, i++)
         item_arr[i] = list[pos];
       list.empty();
       qsort(&item_arr[0], item_arr.size(), sizeof(item_arr[0]), Item::qsort_func);
       for(i=0;i<item_arr.size() && cur_size > (int)size;i++)
	 {
           Item *item = item_arr[i];
           cur_size -= item->get_size();
           file_cleared(item->file);
           item_arr[i] = 0;
         }
       for (; i<item_arr.size(); i++)
         list.append(item_arr[i]);
       if (cur_size <= 0) 
         cur_size = calculate_size();
	 }

	    // Less than 20 elements: no reason to presort
   while(cur_size > (int)size && list.size() > 0)
	 {
	       // Remove the oldest cache item
	    GPosition oldest_pos=list;
	    GPosition pos=list;
	    for(++pos;pos;++pos)
	       if (list[pos]->get_time()<list[oldest_pos]->get_time())
		  oldest_pos=pos;
       cur_size -= list[oldest_pos]->get_size();
	    GP<DjVuFile> file=list[oldest_pos]->file;
	    list.del(oldest_pos);
	    file_cleared(file);
	       // cur_size *may* become negative because items may change their
	       // size after they've been added to the cache
       if (cur_size <= 0) 
         cur_size = calculate_size();
      }
   if (cur_size <= 0) 
     cur_size = calculate_size();
   
   DEBUG_MSG("done: current cache size=" << cur_size << "\n");
}

int
DjVuFileCache::calculate_size(void)
{
   GCriticalSectionLock lock(&class_lock);
   
   int size=0;
   for(GPosition pos=list;pos;++pos)
      size+=list[pos]->get_size();
   return size;
}

void
DjVuFileCache::del_file(const DjVuFile * file)
{
   DEBUG_MSG("DjVuFileCache::del_file(): Removing an item from cache\n");
   DEBUG_MAKE_INDENT(3);

   GCriticalSectionLock lock(&class_lock);

   for(GPosition pos=list;pos;++pos)
      if (list[pos]->get_file()==file)
      {
	 GP<DjVuFile> file=list[pos]->get_file();
	 cur_size-=list[pos]->get_size();
	 list.del(pos);
	 file_deleted(file);
	 break;
      }
   if (cur_size<0) cur_size=calculate_size();
   DEBUG_MSG("current cache size=" << cur_size << "\n");
}

GPList<DjVuFileCache::Item>
DjVuFileCache::get_items(void)
{
   GCriticalSectionLock lock(&class_lock);

   return list;
}

void
DjVuFileCache::file_added(const GP<DjVuFile> &) {}

void
DjVuFileCache::file_deleted(const GP<DjVuFile> &) {}

void
DjVuFileCache::file_cleared(const GP<DjVuFile> &) {}

#ifdef HAVE_NAMESPACES
}
# ifndef NOT_USING_DJVU_NAMESPACE
using namespace DJVU;
# endif
#endif

