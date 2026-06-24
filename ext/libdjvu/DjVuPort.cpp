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

#include "DjVuPort.h"
#include "GOS.h"
#include "DjVuImage.h"
#include "DjVuDocument.h"
#include "DjVuFile.h"
#include "DjVuMessageLite.h"
#include "DataPool.h"


#ifdef HAVE_NAMESPACES
namespace DJVU {
# ifdef NOT_DEFINED // Just to fool emacs c++ mode
}
#endif
#endif


//****************************************************************************
//******************************* Globals ************************************
//****************************************************************************

static DjVuPortcaster *pcaster;

DjVuPortcaster *
DjVuPort::get_portcaster(void)
{
   if (!pcaster) pcaster = new DjVuPortcaster();
   return pcaster;
}

class DjVuPort::DjVuPortCorpse
{
public:
   DjVuPort		* port;
   DjVuPortCorpse	* next;

   DjVuPortCorpse(DjVuPort * _port) : port(_port), next(0) {}
};

//****************************************************************************
//******************************* DjVuPort ***********************************
//****************************************************************************

#define MAX_CORPSE_NUM	128

// Last MAX_CORPSE_NUM addresses of dead DjVuPorts. We want to maintain this
// list because of the way DjVuPort::is_port_alive() works: it accepts an
// address and runs it thru its internal maps. The problem will occur if
// a new DjVuPort is created exactly on place of another one, which just
// died. Here we attempt to remember the last MAX_CORPSE_NUM addresses
// of dead DjVuPorts, and take them into account in DjVuPort::operator new();
GCriticalSection * DjVuPort::corpse_lock;
DjVuPort::DjVuPortCorpse	* DjVuPort::corpse_head;
DjVuPort::DjVuPortCorpse	* DjVuPort::corpse_tail;
int		DjVuPort::corpse_num;

void *
DjVuPort::operator new (size_t sz)
{
  if (!corpse_lock) corpse_lock=new GCriticalSection();
  
  // Loop until we manage to allocate smth, which is not mentioned in
  // the 'corpse' list. Thus we will avoid allocating a new DjVuPort
  // on place of a dead one. Not *absolutely* secure (only 64 items
  // in the list) but is still better than nothing.
  void * addr=0;
  {
    GCriticalSectionLock lock(corpse_lock);
    
    // Store here addresses, which were found in 'corpse' list.
    // We will free then in the end
    int addr_num=0;
    static void * addr_arr[MAX_CORPSE_NUM];
    
    // Make at most MAX_CORPSE_NUM attempts. During each attempt
    // we try to allocate a block of memory for DjVuPort. If
    // the address of this block is not in the corpse list, we break
    // All addresses will be recorder, so that we can delete them
    // after we're done.
    for(int attempt=0;attempt<MAX_CORPSE_NUM;attempt++)
    {
      void * test_addr=::operator new (sz);
      addr_arr[addr_num++]=test_addr;
      
      // See if 'test_addr' is in the 'corpse' list (was recently used)
      DjVuPortCorpse * corpse;
      for(corpse=corpse_head;corpse;corpse=corpse->next)
        if (test_addr==corpse->port) break;
      if (!corpse)
        {
          addr=test_addr;
          addr_num--;
          break;
        }
    }
    // If all attempts failed (all addresses generated are already
    // in the list of corpses, allocate a new one and proceed
    // w/o additional checks
    if (!addr) addr=::operator new(sz);
    
    // Here 'addr_arr[0<=i<addr_num]' contains addresses, that we
    // tried to allocate, and which need to be freed now
    // 'addr' contains address we want to use.
    addr_num--;
    while(addr_num>=0) ::operator delete(addr_arr[addr_num--]);
  }
  
  DjVuPortcaster * pcaster=get_portcaster();
  GCriticalSectionLock lock(&pcaster->map_lock);
  pcaster->cont_map[addr]=0;
  return addr;
}

void
DjVuPort::operator delete(void * addr)
{
  if (corpse_lock)
  {
    GCriticalSectionLock lock(corpse_lock);
    
    // Add 'addr' to the list of corpses
    if (corpse_tail)
    {
      corpse_tail->next=new DjVuPortCorpse((DjVuPort *) addr);
      corpse_tail=corpse_tail->next;
      corpse_tail->next=0;
    } else
    {
      corpse_head=corpse_tail=new DjVuPortCorpse((DjVuPort *) addr);
      corpse_tail->next=0;
    }
    corpse_num++;
    if (corpse_num>=MAX_CORPSE_NUM)
    {
      DjVuPortCorpse * corpse=corpse_head;
      corpse_head=corpse_head->next;
      delete corpse;
      corpse_num--;
    }
  }
  ::operator delete(addr);
}

DjVuPort::DjVuPort()
{
  DjVuPortcaster *pcaster = get_portcaster();
  GCriticalSectionLock lock(& pcaster->map_lock );
  GPosition p = pcaster->cont_map.contains(this);
  if (!p) G_THROW( ERR_MSG("DjVuPort.not_alloc") );
  pcaster->cont_map[p] = (void*)this;
}

DjVuPort::DjVuPort(const DjVuPort & port)
{
  DjVuPortcaster *pcaster = get_portcaster();
  GCriticalSectionLock lock(& pcaster->map_lock );
  GPosition p = pcaster->cont_map.contains(this);
  if (!p) G_THROW( ERR_MSG("DjVuPort.not_alloc") );
  pcaster->cont_map[p] = (void*)this;
  pcaster->copy_routes(this, &port);
}

DjVuPort &
DjVuPort::operator=(const DjVuPort & port)
{
   if (this != &port)
      get_portcaster()->copy_routes(this, &port);
   return *this;
}

DjVuPort::~DjVuPort(void)
{
  get_portcaster()->del_port(this);
}


//****************************************************************************
//**************************** DjVuPortcaster ********************************
//****************************************************************************



DjVuPortcaster::DjVuPortcaster(void)
{
}

DjVuPortcaster::~DjVuPortcaster(void)
{
   GCriticalSectionLock lock(&map_lock);
   for(GPosition pos=route_map;pos;++pos)
      delete (GList<void *> *) route_map[pos];
}

GP<DjVuPort>
DjVuPortcaster::is_port_alive(DjVuPort *port)
{
   GP<DjVuPort> gp_port;
   GCriticalSectionLock lock(&map_lock);
   GPosition pos=cont_map.contains(port);
   if (pos && cont_map[pos] && ((DjVuPort *) port)->get_count()>0)
      gp_port=port;
   if (gp_port && gp_port->get_count() <= 0)
      gp_port = 0;
   return gp_port;
}

void
DjVuPortcaster::add_alias(const DjVuPort * port, const GUTF8String &alias)
{
   GCriticalSectionLock lock(&map_lock);
   a2p_map[alias]=port;
}

void
DjVuPortcaster::clear_all_aliases(void)
{
  DjVuPortcaster *p=get_portcaster();
  GCriticalSectionLock lock(&(p->map_lock));
  GPosition pos;
  while((pos=p->a2p_map))
  {
    p->a2p_map.del(pos);
  }
}

void
DjVuPortcaster::clear_aliases(const DjVuPort * port)
{
  GCriticalSectionLock lock(&map_lock);
  for(GPosition pos=a2p_map;pos;)
    if (a2p_map[pos]==port)
    {
      GPosition this_pos=pos;
      ++pos;
      a2p_map.del(this_pos);
    } else ++pos;
}

GP<DjVuPort>
DjVuPortcaster::alias_to_port(const GUTF8String &alias)
{
   GCriticalSectionLock lock(&map_lock);
   GPosition pos;
   if (a2p_map.contains(alias, pos))
   {
      DjVuPort * port=(DjVuPort *) a2p_map[pos];
      GP<DjVuPort> gp_port=is_port_alive(port);
      if (gp_port) return gp_port;
      else a2p_map.del(pos);
   }
   return 0;
}

GPList<DjVuPort>
DjVuPortcaster::prefix_to_ports(const GUTF8String &prefix)
{
  GPList<DjVuPort> list;
  {
    int length=prefix.length();
    if (length)
    {
      GCriticalSectionLock lock(&map_lock);
      for(GPosition pos=a2p_map;pos;++pos)
        if (!prefix.cmp(a2p_map.key(pos), length))
        {
          DjVuPort * port=(DjVuPort *) a2p_map[pos];
          GP<DjVuPort> gp_port=is_port_alive(port);
          if (gp_port) list.append(gp_port);
        }
    }
  }
  return list;
}

void
DjVuPortcaster::del_port(const DjVuPort * port)
{
  GCriticalSectionLock lock(&map_lock);
  
  GPosition pos;
  
  // Update the "aliases map"
  clear_aliases(port);
  
  // Update "contents map"
  if (cont_map.contains(port, pos)) cont_map.del(pos);
  
  // Update "route map"
  if (route_map.contains(port, pos))
  {
    delete (GList<void *> *) route_map[pos];
    route_map.del(pos);
  }
  for(pos=route_map;pos;)
  {
    GList<void *> & list=*(GList<void *> *) route_map[pos];
    GPosition list_pos;
    if (list.search((void *) port, list_pos)) list.del(list_pos);
    if (!list.size())
    {
      delete &list;
      GPosition tmp_pos=pos;
      ++pos;
      route_map.del(tmp_pos);
    } else ++pos;
  }
}

void
DjVuPortcaster::add_route(const DjVuPort * src, DjVuPort * dst)
      // Adds route src->dst
{
   GCriticalSectionLock lock(&map_lock);
   if (cont_map.contains(src) && src->get_count()>0 &&
       cont_map.contains(dst) && dst->get_count()>0)
   {
      if (!route_map.contains(src)) route_map[src]=new GList<void *>();
      GList<void *> & list=*(GList<void *> *) route_map[src];
      if (!list.contains(dst)) list.append(dst);
   }
}

void
DjVuPortcaster::del_route(const DjVuPort * src, DjVuPort * dst)
// Deletes route src->dst
{
  GCriticalSectionLock lock(&map_lock);
  
  if (route_map.contains(src))
  {
    GList<void *> & list=*(GList<void *> *) route_map[src];
    GPosition pos;
    if (list.search(dst, pos)) list.del(pos);
    if (!list.size())
    {
      delete &list;
      route_map.del(src);
    }
  }
}

void
DjVuPortcaster::copy_routes(DjVuPort * dst, const DjVuPort * src)
      // For every route src->x or x->src, it creates a new one:
      // dst->x or x->dst respectively. It's useful when you create a copy
      // of a port and you want the copy to stay connected.
{
  GCriticalSectionLock lock(&map_lock);
  
  if (!cont_map.contains(src) || src->get_count()<=0 ||
    !cont_map.contains(dst) || dst->get_count()<=0) return;
  
  for(GPosition pos=route_map;pos;++pos)
  {
    GList<void *> & list=*(GList<void *> *) route_map[pos];
    if (route_map.key(pos) == src)
      for(GPosition pos=list;pos;++pos)
        add_route(dst, (DjVuPort *) list[pos]);
    for(GPosition pos=list;pos;++pos)
      if ((DjVuPort*)(list[pos]) == src)
        add_route((DjVuPort *) route_map.key(pos), dst);
  }
}

void
DjVuPortcaster::add_to_closure(GMap<const void *, void *> & set,
			       const DjVuPort * dst, int distance)
{
  // Assuming that the map's already locked
  // GCriticalSectionLock lock(&map_lock);
  set[dst]= (void*) (size_t) distance;
  if (route_map.contains(dst))
    {
      GList<void *> & list=*(GList<void *> *) route_map[dst];
      for(GPosition pos=list;pos;++pos)
        {
          DjVuPort * new_dst=(DjVuPort *) list[pos];
          if (!set.contains(new_dst)) 
            add_to_closure(set, new_dst, distance+1);
        }
   }
}

void
DjVuPortcaster::compute_closure(const DjVuPort * src, GPList<DjVuPort> &list, bool sorted)
{
   GCriticalSectionLock lock(&map_lock);
   GMap<const void*, void*> set;
   if (route_map.contains(src))
   {
      GList<void *> & list=*(GList<void *> *) route_map[src];
      for(GPosition pos=list;pos;++pos)
      {
	       DjVuPort * dst=(DjVuPort *) list[pos];
	       if (dst==src) add_to_closure(set, src, 0);
	       else add_to_closure(set, dst, 1);
      }
   }

   // Compute list
   GPosition pos;
   if (sorted)
     {
       // Sort in depth order
       int max_dist=0;
       for(pos=set;pos;++pos)
         if (max_dist < (int)(size_t)set[pos])
           max_dist = (int)(size_t)set[pos];
       GArray<GList<const void*> > lists(0,max_dist);
       for(pos=set;pos;++pos)
         lists[(int)(size_t)set[pos]].append(set.key(pos));
       for(int dist=0;dist<=max_dist;dist++)
         for(pos=lists[dist];pos;++pos)
           {
             GP<DjVuPort> p = is_port_alive((DjVuPort*) lists[dist][pos]);
             if (p) list.append(p);
           }
     }
   else
     {
       // Gather ports without order
       for(pos=set;pos;++pos)
         {
           GP<DjVuPort> p = is_port_alive((DjVuPort*) set.key(pos));
           if (p) list.append(p);
         }
     }
}

GURL
DjVuPortcaster::id_to_url(const DjVuPort * source, const GUTF8String &id)
{
   GPList<DjVuPort> list;
   compute_closure(source, list, true);
   GURL url;
   for(GPosition pos=list;pos;++pos)
   {
      url=list[pos]->id_to_url(source, id);
      if (!url.is_empty()) break;
   }
   return url;
}

GP<DjVuFile>
DjVuPortcaster::id_to_file(const DjVuPort * source, const GUTF8String &id)
{
   GPList<DjVuPort> list;
   compute_closure(source, list, true);
   GP<DjVuFile> file;
   for(GPosition pos=list;pos;++pos)
      if ((file=list[pos]->id_to_file(source, id))) break;
   return file;
}

GP<DataPool>
DjVuPortcaster::request_data(const DjVuPort * source, const GURL & url)
{
   GPList<DjVuPort> list;
   compute_closure(source, list, true);
   GP<DataPool> data;
   for(GPosition pos=list;pos;++pos)
     if ((data = list[pos]->request_data(source, url)))
       break;
   return data;
}

bool
DjVuPortcaster::notify_error(const DjVuPort * source, const GUTF8String &msg)
{
   GPList<DjVuPort> list;
   compute_closure(source, list, true);
   for(GPosition pos=list;pos;++pos)
     if (list[pos]->notify_error(source, msg))
       return 1;
   return 0;
}

bool
DjVuPortcaster::notify_status(const DjVuPort * source, const GUTF8String &msg)
{
   GPList<DjVuPort> list;
   compute_closure(source, list, true);
   for(GPosition pos=list;pos;++pos)
     if (list[pos]->notify_status(source, msg))
       return 1;
   return 0;
}

void
DjVuPortcaster::notify_redisplay(const DjVuImage * source)
{
   GPList<DjVuPort> list;
   compute_closure(source, list);
   for(GPosition pos=list; pos; ++pos)
     list[pos]->notify_redisplay(source);
}

void
DjVuPortcaster::notify_relayout(const DjVuImage * source)
{
   GPList<DjVuPort> list;
   compute_closure(source, list);
   for(GPosition pos=list; pos; ++pos)
     list[pos]->notify_relayout(source);
}

void
DjVuPortcaster::notify_chunk_done(const DjVuPort * source, const GUTF8String &name)
{
   GPList<DjVuPort> list;
   compute_closure(source, list);
   for(GPosition pos=list; pos; ++pos)
     list[pos]->notify_chunk_done(source, name);
}

void
DjVuPortcaster::notify_file_flags_changed(const DjVuFile * source,
					  long set_mask, long clr_mask)
{
   GPList<DjVuPort> list;
   compute_closure(source, list);
   for(GPosition pos=list; pos; ++pos)
     list[pos]->notify_file_flags_changed(source, set_mask, clr_mask);
}

void
DjVuPortcaster::notify_doc_flags_changed(const DjVuDocument * source,
					 long set_mask, long clr_mask)
{
   GPList<DjVuPort> list;
   compute_closure(source, list);
   for(GPosition pos=list; pos; ++pos)
     list[pos]->notify_doc_flags_changed(source, set_mask, clr_mask);
}

void
DjVuPortcaster::notify_decode_progress(const DjVuPort * source, float done)
{
   GPList<DjVuPort> list;
   compute_closure(source, list);
   for(GPosition pos=list; pos; ++pos)
     list[pos]->notify_decode_progress(source, done);
}

//****************************************************************************
//******************************* DjVuPort ***********************************
//****************************************************************************

GURL
DjVuPort::id_to_url(const DjVuPort *, const GUTF8String &) { return GURL(); }

GP<DjVuFile>
DjVuPort::id_to_file(const DjVuPort *, const GUTF8String &) { return 0; }

GP<DataPool>
DjVuPort::request_data(const DjVuPort *, const GURL &) { return 0; }

bool
DjVuPort::notify_error(const DjVuPort *, const GUTF8String &) { return 0; }

bool
DjVuPort::notify_status(const DjVuPort *, const GUTF8String &) { return 0; }

void
DjVuPort::notify_redisplay(const DjVuImage *) {}

void
DjVuPort::notify_relayout(const DjVuImage *) {}

void
DjVuPort::notify_chunk_done(const DjVuPort *, const GUTF8String &) {}

void
DjVuPort::notify_file_flags_changed(const DjVuFile *, long, long) {}

void
DjVuPort::notify_doc_flags_changed(const DjVuDocument *, long, long) {}

void
DjVuPort::notify_decode_progress(const DjVuPort *, float) {}

//****************************************************************************
//*************************** DjVuSimplePort *********************************
//****************************************************************************

GP<DataPool>
DjVuSimplePort::request_data(const DjVuPort * source, const GURL & url)
{
  G_TRY {
    if (url.is_local_file_url())
    {
//      GUTF8String fname=GOS::url_to_filename(url);
//      if (GOS::basename(fname)=="-") fname="-";
      return DataPool::create(url);
    }
  } G_CATCH_ALL {} G_ENDCATCH;
  return 0;
}

bool
DjVuSimplePort::notify_error(const DjVuPort * source, const GUTF8String &msg)
{
   DjVuMessageLite::perror(msg);
   return 1;
}

bool
DjVuSimplePort::notify_status(const DjVuPort * source, const GUTF8String &msg)
{
   DjVuMessageLite::perror(msg);
   return 1;
}





//****************************************************************************
//*************************** DjVuMemoryPort *********************************
//****************************************************************************



GP<DataPool>
DjVuMemoryPort::request_data(const DjVuPort * source, const GURL & url)
{
   GCriticalSectionLock lk(&lock);
   GP<DataPool> pool;
   GPosition pos;
   if (map.contains(url, pos))
      pool=map[pos];
   return pool;
}

void
DjVuMemoryPort::add_data(const GURL & url, const GP<DataPool> & pool)
{
   GCriticalSectionLock lk(&lock);
   map[url]=pool;
}


#ifdef HAVE_NAMESPACES
}
# ifndef NOT_USING_DJVU_NAMESPACE
using namespace DJVU;
# endif
#endif

