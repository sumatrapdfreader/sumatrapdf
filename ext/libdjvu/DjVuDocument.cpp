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

#include "DjVuDocument.h"
#include "DjVmDoc.h"
#include "DjVmDir0.h"
#include "DjVmNav.h"
#include "DjVuNavDir.h"
#include "DjVuImage.h"
#include "DjVuFileCache.h"
#include "IFFByteStream.h"
#include "GOS.h"
#include "DataPool.h"
#include "IW44Image.h"
#include "GRect.h"

#include "debug.h"


#ifdef HAVE_NAMESPACES
namespace DJVU {
# ifdef NOT_DEFINED // Just to fool emacs c++ mode
}
#endif
#endif


static const char octets[4]={0x41,0x54,0x26,0x54};
const float	DjVuDocument::thumb_gamma=(float)2.20;

void (* DjVuDocument::djvu_import_codec)(
  GP<DataPool> &pool, const GURL &url, bool &needs_compression,
  bool &needs_rename )=0;

void (* DjVuDocument::djvu_compress_codec)(
  GP<ByteStream> &doc,const GURL &where,bool bundled)=0;

void
DjVuDocument::set_import_codec(
  void (*codec)(
    GP<DataPool> &pool, const GURL &url, bool &needs_compression, bool &needs_rename ))
{
  djvu_import_codec=codec;
}

void
DjVuDocument::set_compress_codec(
  void (* codec)(
    GP<ByteStream> &doc,const GURL &where,bool bundled))
{
  djvu_compress_codec=codec;
}

DjVuDocument::DjVuDocument(void)
  : doc_type(UNKNOWN_TYPE),
    needs_compression_flag(false),
    can_compress_flag(false),
    needs_rename_flag(false),
    has_url_names(false),
    recover_errors(ABORT),
    verbose_eof(false),
    init_started(false),
    cache(0) 
{
}

GP<DjVuDocument>
DjVuDocument::create(
  GP<DataPool> pool, GP<DjVuPort> xport, DjVuFileCache * const xcache)
{
  DjVuDocument *doc=new DjVuDocument;
  GP<DjVuDocument> retval=doc;
  doc->init_data_pool=pool;
  doc->start_init(GURL(),xport,xcache);
  return retval;
}

GP<DjVuDocument>
DjVuDocument::create(
  const GP<ByteStream> &bs, GP<DjVuPort> xport, DjVuFileCache * const xcache)
{
  return create(DataPool::create(bs),xport,xcache);
}

GP<DjVuDocument>
DjVuDocument::create_wait(
  const GURL &url, GP<DjVuPort> xport, DjVuFileCache * const xcache)
{
  GP<DjVuDocument> retval=create(url,xport,xcache);
  retval->wait_for_complete_init();
  return retval;
}

void
DjVuDocument::start_init(
  const GURL & url, GP<DjVuPort> xport, DjVuFileCache * xcache)
{
   DEBUG_MSG("DjVuDocument::start_init(): initializing class...\n");
   DEBUG_MAKE_INDENT(3);
   if (init_started)
      G_THROW( ERR_MSG("DjVuDocument.2nd_init") );
   if (!get_count())
      G_THROW( ERR_MSG("DjVuDocument.not_secure") );
   if(url.is_empty())
   {
     if (!init_data_pool)
       G_THROW( ERR_MSG("DjVuDocument.empty_url") );
     if(init_url.is_empty())
     {
       init_url=invent_url("document.djvu");
     }
   }else
   {
     init_url=url;
   }
   
      // Initialize
   cache=xcache;
   doc_type=UNKNOWN_TYPE;
   DataPool::close_all();
   DjVuPortcaster * pcaster=get_portcaster();
   if (!xport)
     xport=simple_port=new DjVuSimplePort();
   pcaster->add_route(this, xport);
   pcaster->add_route(this, this);

   if(!url.is_empty())
   {
     init_data_pool=pcaster->request_data(this, init_url);
     if(init_data_pool)
     {
       if(!init_url.is_empty() && init_url.is_local_file_url() && djvu_import_codec)
       {
         djvu_import_codec(init_data_pool,init_url,needs_compression_flag,needs_rename_flag);
       }
       if(needs_rename_flag)
         can_compress_flag=true;
     }
     if (!init_data_pool) 
     {
       G_THROW( ERR_MSG("DjVuDocument.fail_URL") "\t"+init_url.get_string());
     }
   }
      // Now we say it is ready
   init_started=true;

   init_thread_flags=STARTED;
   init_life_saver=this;
   init_thr.create(static_init_thread, this);
}

DjVuDocument::~DjVuDocument(void)
{
      // No more messages, please. We're being destroyed.
   get_portcaster()->del_port(this);

      // We want to stop any DjVuFile which has been created by us
      // and is still being decoded. We have to stop them manually because
      // they keep the "life saver" in the decoding thread and won't stop
      // when we clear the last reference to them
   {
      GCriticalSectionLock lock(&ufiles_lock);
      for(GPosition pos=ufiles_list;pos;++pos)
      {
          GP<DjVuFile> file=ufiles_list[pos]->file;
          file->stop_decode(false);
          file->stop(false);	// Disable any access to data
      }
      ufiles_list.empty();
   }

   GPList<DjVuPort> ports=get_portcaster()->prefix_to_ports(get_int_prefix());
   for(GPosition pos=ports;pos;++pos)
   {
     GP<DjVuPort> port=ports[pos];
     if (port->inherits("DjVuFile"))
     {
       DjVuFile * file=(DjVuFile *) (DjVuPort *) port;
       file->stop_decode(false);
       file->stop(false);	// Disable any access to data
     }
   }
   DataPool::close_all();
}

void
DjVuDocument::stop_init(void)
{
   DEBUG_MSG("DjVuDocument::stop_init(): making sure that the init thread dies.\n");
   DEBUG_MAKE_INDENT(3);

   GMonitorLock lock(&init_thread_flags);
   while((init_thread_flags & STARTED) &&
	 !(init_thread_flags & FINISHED))
   {
      if (init_data_pool) init_data_pool->stop(true);	// blocking operation

      if (ndir_file) ndir_file->stop(false);

      {
	 GCriticalSectionLock lock(&ufiles_lock);
	 for(GPosition pos=ufiles_list;pos;++pos)
	    ufiles_list[pos]->file->stop(false);	// Disable any access to data
	 ufiles_list.empty();
      }

      init_thread_flags.wait(50);
   }
}

void
DjVuDocument::check() const
{
  if (!init_started)
    G_THROW( ERR_MSG("DjVuDocument.not_init") );
}

void
DjVuDocument::static_init_thread(void * cl_data)
{
  DjVuDocument * th=(DjVuDocument *) cl_data;
  GP<DjVuDocument> life_saver=th;
  th->init_life_saver=0;
  G_TRY {
    th->init_thread();
  } G_CATCH(exc) {
    G_TRY {
      int changed = DjVuDocument::DOC_INIT_FAILED;
      th->flags |= changed;
      get_portcaster()->notify_doc_flags_changed(th, changed, 0);
    } G_CATCH_ALL {
    } G_ENDCATCH;
    G_TRY {
      th->check_unnamed_files();
      if (!exc.cmp_cause(ByteStream::EndOfFile) && th->verbose_eof)
        get_portcaster()->notify_error(th, ERR_MSG("DjVuDocument.init_eof"));
      else if (!exc.cmp_cause(DataPool::Stop))
        get_portcaster()->notify_status(th, ERR_MSG("DjVuDocument.stopped"));
      else
        get_portcaster()->notify_error(th, exc.get_cause());
    } G_CATCH_ALL {
    } G_ENDCATCH;
    th->init_thread_flags |= FINISHED;
  } G_ENDCATCH;
}

void
DjVuDocument::init_thread(void)
      // This function is run in a separate thread.
      // The goal is to detect the document type (BUNDLED, OLD_INDEXED, etc.)
      // and decode navigation directory.
{
   DEBUG_MSG("DjVuDocument::init_thread(): guessing what we're dealing with\n");
   DEBUG_MAKE_INDENT(3);

   DjVuPortcaster * pcaster=get_portcaster();
      
   GP<ByteStream> stream=init_data_pool->get_stream();

   GP<IFFByteStream> giff=IFFByteStream::create(stream);
   IFFByteStream &iff=*giff;
   GUTF8String chkid;
   int size=iff.get_chunk(chkid);
   if (!size)
     G_THROW( ByteStream::EndOfFile );
   if (size < 0)
     G_THROW( ERR_MSG("DjVuDocument.no_file") );
   if (size<8)
     G_THROW( ERR_MSG("DjVuDocument.not_DjVu") );
   if (chkid=="FORM:DJVM")
   {
     DEBUG_MSG("Got DJVM document here\n");
     DEBUG_MAKE_INDENT(3);
     
     size=iff.get_chunk(chkid);
     if (chkid=="DIRM")
       {
	 djvm_dir=DjVmDir::create();
	 djvm_dir->decode(iff.get_bytestream());
	 iff.close_chunk();
	 if (djvm_dir->is_bundled())
           {
             DEBUG_MSG("Got BUNDLED file.\n");
             doc_type=BUNDLED;
           } 
         else
           {
             DEBUG_MSG("Got INDIRECT file.\n");
             doc_type=INDIRECT;
           }
	 flags|=DOC_TYPE_KNOWN | DOC_DIR_KNOWN;
	 pcaster->notify_doc_flags_changed(this, 
                                           DOC_TYPE_KNOWN | DOC_DIR_KNOWN, 0);
	 check_unnamed_files();
         
         /* Check for NAVM */
         size=iff.get_chunk(chkid);
         if (size && chkid=="NAVM")
           {
             djvm_nav=DjVmNav::create();
             djvm_nav->decode(iff.get_bytestream());
             iff.close_chunk();
           }
       }
     else if (chkid=="DIR0")
       {
	 DEBUG_MSG("Got OLD_BUNDLED file.\n");
	 doc_type=OLD_BUNDLED;
	 flags|=DOC_TYPE_KNOWN;
	 pcaster->notify_doc_flags_changed(this, DOC_TYPE_KNOWN, 0);
	 check_unnamed_files();
       } 
     else 
       G_THROW( ERR_MSG("DjVuDocument.bad_format") );
     
     if (doc_type==OLD_BUNDLED)
       {
         // Read the DjVmDir0 directory. We are unable to tell what
         // files are pages and what are included at this point.
         // We only know that the first file with DJVU (BM44 or PM44)
         // form *is* the first page. The rest will become known
         // after we decode DjVuNavDir
	 djvm_dir0=DjVmDir0::create();
	 djvm_dir0->decode(*iff.get_bytestream());
	 iff.close_chunk();
         // Get offset to the first DJVU, PM44 or BM44 chunk
	 int first_page_offset=0;
	 while(!first_page_offset)
           {
             int offset;
             size=iff.get_chunk(chkid, &offset);
             if (size==0) G_THROW( ERR_MSG("DjVuDocument.no_page") );
             if (chkid=="FORM:DJVU" || 
                 chkid=="FORM:PM44" || chkid=="FORM:BM44")
               {
                 DEBUG_MSG("Got 1st page offset=" << offset << "\n");
                 first_page_offset=offset;
               }
             iff.close_chunk();
           }
         
         // Now get the name of this file
	 int file_num;
	 for(file_num=0;file_num<djvm_dir0->get_files_num();file_num++)
           {
             DjVmDir0::FileRec & file=*djvm_dir0->get_file(file_num);
             if (file.offset==first_page_offset)
               {
                 first_page_name=file.name;
                 break;
               }
           }
	 if (!first_page_name.length())
           G_THROW( ERR_MSG("DjVuDocument.no_page") );
	 flags|=DOC_DIR_KNOWN;
	 pcaster->notify_doc_flags_changed(this, DOC_DIR_KNOWN, 0);
	 check_unnamed_files();
       }
   } 
   else // chkid!="FORM:DJVM"
     {
       // DJVU format
       DEBUG_MSG("Got DJVU OLD_INDEXED or SINGLE_PAGE document here.\n");
       doc_type=SINGLE_PAGE;
       flags |= DOC_TYPE_KNOWN;
       pcaster->notify_doc_flags_changed(this, DOC_TYPE_KNOWN, 0);
       check_unnamed_files();
     }
   if (doc_type==OLD_BUNDLED || doc_type==SINGLE_PAGE)
     {
       DEBUG_MSG("Searching for NDIR chunks...\n");
       ndir_file=get_djvu_file(-1);
       if (ndir_file) ndir=ndir_file->decode_ndir();
       ndir_file=0;	// Otherwise ~DjVuDocument() will stop (=kill) it
       if (!ndir)
         {
           // Seems to be 1-page old-style document. Create dummy NDIR
           if (doc_type==OLD_BUNDLED)
             {
               ndir=DjVuNavDir::create(GURL::UTF8("directory",init_url));
               ndir->insert_page(-1, first_page_name);
             } 
           else
             {
               ndir=DjVuNavDir::create(GURL::UTF8("directory",init_url.base()));
               ndir->insert_page(-1, init_url.fname());
             }
         } 
       else
         {
           if (doc_type==SINGLE_PAGE)
             doc_type=OLD_INDEXED;
         }
       flags|=DOC_NDIR_KNOWN;
       pcaster->notify_doc_flags_changed(this, DOC_NDIR_KNOWN, 0);
       check_unnamed_files();
     }
   
   flags |= DOC_INIT_OK;
   pcaster->notify_doc_flags_changed(this, DOC_INIT_OK, 0);
   check_unnamed_files();
   init_thread_flags|=FINISHED;
   DEBUG_MSG("DOCUMENT IS FULLY INITIALIZED now: doc_type='" <<
	     (doc_type==BUNDLED ? "BUNDLED" :
	      doc_type==OLD_BUNDLED ? "OLD_BUNDLED" :
	      doc_type==INDIRECT ? "INDIRECT" :
	      doc_type==OLD_INDEXED ? "OLD_INDEXED" :
	      doc_type==SINGLE_PAGE ? "SINGLE_PAGE" :
	      "UNKNOWN") << "'\n");
}

bool
DjVuDocument::wait_for_complete_init(void)
{
  flags.enter();
  while(!(flags & DOC_INIT_FAILED) &&
        !(flags & DOC_INIT_OK)) flags.wait();
  flags.leave();
  init_thread_flags.enter();
  while (!(init_thread_flags & FINISHED))
    init_thread_flags.wait();
  init_thread_flags.leave();
  return (flags & (DOC_INIT_OK | DOC_INIT_FAILED))!=0;
}

int
DjVuDocument::wait_get_pages_num(void) const
{
  GSafeFlags &f=const_cast<GSafeFlags &>(flags);
  f.enter();
  while(!(f & DOC_TYPE_KNOWN) &&
        !(f & DOC_INIT_FAILED) &&
        !(f & DOC_INIT_OK)) f.wait();
  f.leave();
  return get_pages_num();
}

GUTF8String
DjVuDocument::get_int_prefix(void) const
{
      // These NAMEs are used to enable DjVuFile sharing inside the same
      // DjVuDocument using DjVuPortcaster. Since URLs are unique to the
      // document, other DjVuDocuments cannot retrieve files until they're
      // assigned some permanent name. After '?' there should be the real
      // file's URL. Please note, that output of this function is used only
      // as name for DjVuPortcaster. Not as a URL.
   GUTF8String retval;
   return retval.format("document_%p%d?", this, hash(init_url));
}

void
DjVuDocument::set_file_aliases(const DjVuFile * file)
{
   DEBUG_MSG("DjVuDocument::set_file_aliases(): setting global aliases for file '"
	     << file->get_url() << "'\n");
   DEBUG_MAKE_INDENT(3);

   DjVuPortcaster * pcaster=DjVuPort::get_portcaster();
   
   GMonitorLock lock(&((DjVuFile *) file)->get_safe_flags());
   pcaster->clear_aliases(file);
   if (file->is_decode_ok() && cache)
   {
	 // If file is successfully decoded and caching is enabled,
	 // assign a global alias to this file, so that any other
	 // DjVuDocument will be able to use it.
      
      pcaster->add_alias(file, file->get_url().get_string());
      if (flags & (DOC_NDIR_KNOWN | DOC_DIR_KNOWN))
      {
	 int page_num=url_to_page(file->get_url());
	 if (page_num>=0)
	 {
	    if (page_num==0) pcaster->add_alias(file, init_url.get_string()+"#-1");
	    pcaster->add_alias(file, init_url.get_string()+"#"+GUTF8String(page_num));
	 }
      }
	 // The following line MUST stay here. For OLD_INDEXED documents
	 // a page may finish decoding before DIR or NDIR becomes known
	 // (multithreading, remember), so the code above would not execute
      pcaster->add_alias(file, file->get_url().get_string()+"#-1");
   } else pcaster->add_alias(file, get_int_prefix()+file->get_url());
}

void
DjVuDocument::check_unnamed_files(void)
{
  DEBUG_MSG("DjVuDocument::check_unnamed_files(): Seeing if we can fix some...\n");
  DEBUG_MAKE_INDENT(3);
  
  if (flags & DOC_INIT_FAILED)
  {
    // Init failed. All unnamed files should be terminated
    GCriticalSectionLock lock(&ufiles_lock);
    for(GPosition pos=ufiles_list;pos;++pos)
    {
      GP<DjVuFile> file=ufiles_list[pos]->file;
      file->stop_decode(true);
      file->stop(false);	// Disable any access to data
    }
    ufiles_list.empty();
    return;
  }
  
  if ((flags & DOC_TYPE_KNOWN)==0)
    return;
  
  // See the list of unnamed files (created when there was insufficient
  // information about DjVuDocument structure) and try to fix those,
  // which can be fixed at this time
  while(true)
  {
    DjVuPortcaster * pcaster=get_portcaster();
    
    GP<UnnamedFile> ufile;
    GURL new_url;
    GPosition pos ;   
	   GCriticalSectionLock lock(&ufiles_lock);
     for(pos=ufiles_list;pos;)
     {
	G_TRY
        {
          GP<UnnamedFile> f=ufiles_list[pos];
          if (f->id_type==UnnamedFile::ID) 
            new_url=id_to_url(f->id);
          else 
            new_url=page_to_url(f->page_num);
          if (!new_url.is_empty())
          {
            ufile=f;
            // Don't take it off the list. We want to be
            // able to stop the init from ~DjVuDocument();
            //
            // ufiles_list.del(pos);
            break;
          } else if (is_init_complete())
          {
            // No empty URLs are allowed at this point.
            // We now know all information about the document
            // and can determine if a page is inside it or not
            f->data_pool->set_eof();
            GUTF8String msg;
            if (f->id_type==UnnamedFile::ID)
              msg= ERR_MSG("DjVuDocument.miss_page_name") "\t"+f->id;
            else 
              msg= ERR_MSG("DjVuDocument.miss_page_num") "\t"+GUTF8String(f->page_num);
            G_THROW(msg);
          }
          ++pos;
        }
        G_CATCH(exc)
        {
          pcaster->notify_error(this, exc.get_cause());
          GP<DataPool> pool=ufiles_list[pos]->data_pool;
          if (pool)
            pool->stop();
          GPosition this_pos=pos;
          ++pos;
          ufiles_list.del(this_pos);
        }
        G_ENDCATCH;
     }
     
     if (ufile && !new_url.is_empty())
       {
         DEBUG_MSG("Fixing file: '" << ufile->url << "'=>'" << new_url << "'\n");
         // Now, once we know its real URL we can request a real DataPool and
         // can connect the DataPool owned by DjVuFile to that real one
         // Note, that now request_data() will not play fool because
         // we have enough information
         
         G_TRY
           {
             if (ufile->data_pool)
               {
                 GP<DataPool> new_pool=pcaster->request_data(ufile->file, new_url);
                 if(!new_pool)
                   G_THROW( ERR_MSG("DjVuDocument.fail_URL") "\t"+new_url.get_string());
                 ufile->data_pool->connect(new_pool);
               }
             ufile->file->set_name(new_url.fname());
             ufile->file->move(new_url.base());
             set_file_aliases(ufile->file);
           }
         G_CATCH(exc)
           {
             pcaster->notify_error(this, exc.get_cause());
           }   
         G_ENDCATCH;
       }
     else
       break;
     
     // Remove the 'ufile' from the list
     for(pos=ufiles_list;pos;++pos)
       if (ufiles_list[pos]==ufile)
         {
           ufiles_list.del(pos);
           break;
         }
  } // while(1)
}

int
DjVuDocument::get_pages_num(void) const
{
  check();
  if (flags & DOC_TYPE_KNOWN)
    {
      if (doc_type==BUNDLED || doc_type==INDIRECT)
	return djvm_dir->get_pages_num();
      else if (flags & DOC_NDIR_KNOWN)
	return ndir->get_pages_num();
    }
  return 1;
}

GURL
DjVuDocument::page_to_url(int page_num) const
{
   check();
   DEBUG_MSG("DjVuDocument::page_to_url(): page_num=" << page_num << "\n");
   DEBUG_MAKE_INDENT(3);
   
   GURL url;
   if (flags & DOC_TYPE_KNOWN)
      switch(doc_type)
      {
	 case SINGLE_PAGE:
         {
           if (page_num<1) 
             url=init_url;
           else
             G_THROW( ERR_MSG("DjVuDocument.big_num") );
           break;
         }
	 case OLD_INDEXED:
	 {
	    if (page_num<0) 
              url=init_url;
	    else if (flags & DOC_NDIR_KNOWN) 
              url=ndir->page_to_url(page_num);
	    break;
	 }
	 case OLD_BUNDLED:
	 {
	    if (page_num<0) 
              page_num=0;
	    if (page_num==0 && (flags & DOC_DIR_KNOWN))
              url=GURL::UTF8(first_page_name,init_url);
	    else if (flags & DOC_NDIR_KNOWN)
              url=ndir->page_to_url(page_num);
	    break;
	 }
	 case BUNDLED:
	 {
	    if (page_num<0)
              page_num=0;
	    if (flags & DOC_DIR_KNOWN)
	    {
	      GP<DjVmDir::File> file=djvm_dir->page_to_file(page_num);
	      if (!file) 
                G_THROW( ERR_MSG("DjVuDocument.big_num") );
	      url=GURL::UTF8(file->get_load_name(),init_url);
	    }
	    break;
	 }
	 case INDIRECT:
	 {
	    if (page_num<0) page_num=0;
	    if (flags & DOC_DIR_KNOWN)
	    {
	       GP<DjVmDir::File> file=djvm_dir->page_to_file(page_num);
	       if (!file)
                 G_THROW( ERR_MSG("DjVuDocument.big_num") );
	       url=GURL::UTF8(file->get_load_name(),init_url.base());
	    }
	    break;
	 }
	 default:
	    G_THROW( ERR_MSG("DjVuDocument.unk_type") );
      }
   return url;
}

int
DjVuDocument::url_to_page(const GURL & url) const
{
   check();
   DEBUG_MSG("DjVuDocument::url_to_page(): url='" << url << "'\n");
   DEBUG_MAKE_INDENT(3);

   int page_num=-1;
   if (flags & DOC_TYPE_KNOWN)
      switch(doc_type)
      {
	 case SINGLE_PAGE:
	 case OLD_BUNDLED:
	 case OLD_INDEXED:
	 {
	    if (flags & DOC_NDIR_KNOWN) page_num=ndir->url_to_page(url);
	    break;
	 }
	 case BUNDLED:
	 {
	    if (flags & DOC_DIR_KNOWN)
	    {
	       GP<DjVmDir::File> file;
	       if (url.base()==init_url)
                 file=djvm_dir->id_to_file(url.fname());
	       if (file)
                 page_num=file->get_page_num();
	    }
	    break;
	 }
	 case INDIRECT:
	 {
	    if (flags & DOC_DIR_KNOWN)
	    {
	       GP<DjVmDir::File> file;
	       if (url.base()==init_url.base())
                 file=djvm_dir->id_to_file(url.fname());
	       if (file)
                 page_num=file->get_page_num();
	    }
	    break;
	 }
	 default:
	    G_THROW( ERR_MSG("DjVuDocument.unk_type") );
      }
   return page_num;
}

GURL
DjVuDocument::id_to_url(const GUTF8String & id) const
{
   check();
   DEBUG_MSG("DjVuDocument::id_to_url(): translating ID='" << id << "' to URL\n");
   DEBUG_MAKE_INDENT(3);

   if (flags & DOC_TYPE_KNOWN)
      switch(doc_type)
      {
	 case BUNDLED:
	    if (flags & DOC_DIR_KNOWN)
	    {
	      GP<DjVmDir::File> file=djvm_dir->id_to_file(id);
	      if (!file)
                file=djvm_dir->name_to_file(id);
	        if (!file)
                  file=djvm_dir->title_to_file(id);
	      if (file)
	        return GURL::UTF8(file->get_load_name(),init_url);
	    }
	    break;
	 case INDIRECT:
	    if (flags & DOC_DIR_KNOWN)
	    {
	       GP<DjVmDir::File> file=djvm_dir->id_to_file(id);
	       if (!file)
                 file=djvm_dir->name_to_file(id);
	         if (!file)
                   file=djvm_dir->title_to_file(id);
	       if (file)
	         return GURL::UTF8(file->get_load_name(),init_url.base());
	    }
	    break;
	 case OLD_BUNDLED:
	    if (flags & DOC_DIR_KNOWN)
	    {
	       GP<DjVmDir0::FileRec> frec=djvm_dir0->get_file(id);
	       if (frec)
                 return GURL::UTF8(id,init_url);
	    }
	    break;
	 case OLD_INDEXED:
	 case SINGLE_PAGE:
	    {
	       GURL url = GURL::UTF8(id,init_url.base());
	       if (url.fname() == "-")
	          G_THROW("Illegal include chunk (corrupted file?)");
	       return url;
	    }
	    break;
      }
   return GURL();
}

GURL
DjVuDocument::id_to_url(const DjVuPort * source, const GUTF8String &id)
{
   return id_to_url(id);
}

GP<DjVuFile>
DjVuDocument::url_to_file(const GURL & url, bool dont_create) const
      // This function is private and is called from two places:
      // id_to_file() and get_djvu_file() ONLY when the structure is known
{
   check();
   DEBUG_MSG("DjVuDocument::url_to_file(): url='" << url << "'\n");
   DEBUG_MAKE_INDENT(3);

      // Try DjVuPortcaster to find existing files.
   DjVuPortcaster * pcaster=DjVuPort::get_portcaster();
   GP<DjVuPort> port;

   if (cache)
   {
	 // First - fully decoded files
      port=pcaster->alias_to_port(url.get_string());
      if (port && port->inherits("DjVuFile"))
      {
	 DEBUG_MSG("found fully decoded file using DjVuPortcaster\n");
	 return (DjVuFile *) (DjVuPort *) port;
      }
   }

      // Second - internal files
   port=pcaster->alias_to_port(get_int_prefix()+url);
   if (port && port->inherits("DjVuFile"))
   {
      DEBUG_MSG("found internal file using DjVuPortcaster\n");
      return (DjVuFile *) (DjVuPort *) port;
   }

   GP<DjVuFile> file;
   
   if (!dont_create)
   {
      DEBUG_MSG("creating a new file\n");
      file=DjVuFile::create(url,const_cast<DjVuDocument *>(this),recover_errors,verbose_eof);
      const_cast<DjVuDocument *>(this)->set_file_aliases(file);
   }

   return file;
}

GP<DjVuFile>
DjVuDocument::get_djvu_file(int page_num, bool dont_create) const
{
   check();
   DEBUG_MSG("DjVuDocument::get_djvu_file(): request for page " << page_num << "\n");
   DEBUG_MAKE_INDENT(3);

   DjVuPortcaster * pcaster=DjVuPort::get_portcaster();
   
   GURL url;
   {
	 // I'm locking the flags because depending on what page_to_url()
	 // returns me, I'll be creating DjVuFile in different ways.
	 // And I don't want the situation to change between the moment I call
	 // id_to_url() and I actually create DjVuFile
      GMonitorLock lock(&(const_cast<DjVuDocument *>(this)->flags));
      url=page_to_url(page_num);
      if (url.is_empty())
      {
	    // If init is complete and url is empty, we know for sure, that
	    // smth is wrong with the page_num. So we can return ZERO.
	    // Otherwise we create a temporary file and wait for init to finish
	 if (is_init_complete()) return 0;
	 
	 DEBUG_MSG("Structure is not known => check <doc_url>#<page_num> alias...\n");
	 GP<DjVuPort> port;
	 if (cache)
	    port=pcaster->alias_to_port(init_url.get_string()+"#"+GUTF8String(page_num));
	 if (!port || !port->inherits("DjVuFile"))
	 {
	    DEBUG_MSG("failed => invent dummy URL and proceed\n");
	 
	       // Invent some dummy temporary URL. I don't care what it will
	       // be. I'll remember the page_num and will generate the correct URL
	       // after I learn what the document is
            GUTF8String name("page");
            name+=GUTF8String(page_num);
            name+=".djvu";
            url=invent_url(name);

            GCriticalSectionLock(&(const_cast<DjVuDocument *>(this)->ufiles_lock));
	    for(GPosition pos=ufiles_list;pos;++pos)
	    {
	       GP<UnnamedFile> f=ufiles_list[pos];
	       if (f->url==url) return f->file;
	    }
	    GP<UnnamedFile> ufile=new UnnamedFile(UnnamedFile::PAGE_NUM, 0,
						  page_num, url, 0);

	       // We're adding the record to the list before creating the DjVuFile
	       // because DjVuFile::init() will call request_data(), and the
	       // latter should be able to find the record.
	       //
	       // We also want to keep ufiles_lock to make sure that when
	       // request_data() is called, the record is still there
	    const_cast<DjVuDocument *>(this)->ufiles_list.append(ufile);
      
	    GP<DjVuFile> file=
              DjVuFile::create(url,const_cast<DjVuDocument *>(this),recover_errors,verbose_eof);
	    ufile->file=file;
	    return file;
	 } else url=((DjVuFile *) (DjVuPort *) port)->get_url();
      }
   }
   
   GP<DjVuFile> file=url_to_file(url, dont_create);
   if (file) 
     pcaster->add_route(file, const_cast<DjVuDocument *>(this));
   return file;
}

GURL
DjVuDocument::invent_url(const GUTF8String &name) const
{
   GUTF8String buffer;
   buffer.format("djvufileurl://%p/%s", this, (const char *)name);
   return GURL::UTF8(buffer);
}

GP<DjVuFile>
DjVuDocument::get_djvu_file(const GUTF8String& id, bool dont_create)
{
  check();
  DEBUG_MSG("DjVuDocument::get_djvu_file(): ID='" << id << "'\n");
  DEBUG_MAKE_INDENT(3);
  if (!id.length())
    return get_djvu_file(-1);

// Integers are not supported, only ID's  
//  if (id.is_int())
//     return get_djvu_file(id.toInt(),dont_create);
  
  GURL url;
  // I'm locking the flags because depending on what id_to_url()
  // returns me, I'll be creating DjVuFile in different ways.
  // And I don't want the situation to change between the moment I call
  // id_to_url() and I actually create DjVuFile
  {
    GMonitorLock lock(&flags);
    url=id_to_url(id);
    if(url.is_empty() && !id.is_int())
    {
      // If init is complete, we know for sure, that there is no such
      // file with ID 'id' in the document. Otherwise we have to
      // create a temporary file and wait for the init to finish
      if (is_init_complete())
        return 0;
      // Invent some dummy temporary URL. I don't care what it will
      // be. I'll remember the ID and will generate the correct URL
      // after I learn what the document is
      url=invent_url(id);
      DEBUG_MSG("Invented url='" << url << "'\n");

      GCriticalSectionLock lock(&ufiles_lock);
      for(GPosition pos=ufiles_list;pos;++pos)
      {
        GP<UnnamedFile> f=ufiles_list[pos];
        if (f->url==url)
          return f->file;
      }
      GP<UnnamedFile> ufile=new UnnamedFile(UnnamedFile::ID, id, 0, url, 0);

      // We're adding the record to the list before creating the DjVuFile
      // because DjVuFile::init() will call request_data(), and the
      // latter should be able to find the record.
      //
      // We also want to keep ufiles_lock to make sure that when
      // request_data() is called, the record is still there
      ufiles_list.append(ufile);
      
      GP<DjVuFile> file=DjVuFile::create(url,this,recover_errors,verbose_eof);
      ufile->file=file;
      return file;
    }
  }
     
  return get_djvu_file(url,dont_create);
}

GP<DjVuFile>
DjVuDocument::get_djvu_file(const GURL& url, bool dont_create)
{
   check();
   DEBUG_MSG("DjVuDocument::get_djvu_file(): URL='" << url << "'\n");
   DEBUG_MAKE_INDENT(3);

   if (url.is_empty())
     return 0;

   const GP<DjVuFile> file(url_to_file(url, dont_create));

   if (file)
     get_portcaster()->add_route(file, this);

   return file;
}

GP<DjVuImage>
DjVuDocument::get_page(int page_num, bool sync, DjVuPort * port) const
{
   check();
   DEBUG_MSG("DjVuDocument::get_page(): request for page " << page_num << "\n");
   DEBUG_MAKE_INDENT(3);

   GP<DjVuImage> dimg;
   const GP<DjVuFile> file(get_djvu_file(page_num));
   if (file)
   {
     dimg=DjVuImage::create(file);
     if (port)
       DjVuPort::get_portcaster()->add_route(dimg, port);
   
     file->resume_decode();
     if (dimg && sync)
       dimg->wait_for_complete_decode();
   }
   return dimg;
}

GP<DjVuImage>
DjVuDocument::get_page(const GUTF8String &id, bool sync, DjVuPort * port)
{
   check();
   DEBUG_MSG("DjVuDocument::get_page(): ID='" << id << "'\n");
   DEBUG_MAKE_INDENT(3);

   GP<DjVuImage> dimg;
   const GP<DjVuFile> file(get_djvu_file(id));
   if(file)
   {
     dimg=DjVuImage::create(file);
     if (port)
       DjVuPort::get_portcaster()->add_route(dimg, port);
   
     file->resume_decode();
     if (dimg && sync)
       dimg->wait_for_complete_decode();
   }
   return dimg;
}

void
DjVuDocument::process_threqs(void)
      // Will look thru threqs_list and try to fulfil every request
{
  GCriticalSectionLock lock(&threqs_lock);
  for(GPosition pos=threqs_list;pos;)
  {
    GP<ThumbReq> req=threqs_list[pos];
    bool remove=false;
    if (req->thumb_file)
    {
      G_TRY {
	       // There is supposed to be a file with thumbnails
        if (req->thumb_file->is_data_present())
        {
          // Cool, we can extract the thumbnail now
          GP<ByteStream> str=req->thumb_file->get_init_data_pool()->get_stream();
          GP<IFFByteStream> giff=IFFByteStream::create(str);
          IFFByteStream &iff=*giff;
          GUTF8String chkid;
          if (!iff.get_chunk(chkid) || chkid!="FORM:THUM")
            G_THROW( ERR_MSG("DjVuDocument.bad_thumb") );          
          for(int i=0;i<req->thumb_chunk;i++)
          {
            if (!iff.get_chunk(chkid)) 
              G_THROW( ERR_MSG("DjVuDocument.bad_thumb") );
            iff.close_chunk();
          }
          if (!iff.get_chunk(chkid) || chkid!="TH44")
            G_THROW( ERR_MSG("DjVuDocument.bad_thumb") );
          
          // Copy the data
          char buffer[1024];
          int length;
          while((length=iff.read(buffer, 1024)))
            req->data_pool->add_data(buffer, length);
          req->data_pool->set_eof();
          
          // Also add this file to cache so that we won't have
          // to download it next time
          add_to_cache(req->thumb_file);
          req->thumb_file=0;
          req->image_file=0;
          remove=true;
        }
      } G_CATCH(exc) {
        GUTF8String msg= ERR_MSG("DjVuDocument.cant_extract") "\n";
        msg+=exc.get_cause();
        get_portcaster()->notify_error(this, msg);
	       // Switch this request to the "decoding" mode
        req->image_file=get_djvu_file(req->page_num);
        req->thumb_file=0;
        req->data_pool->set_eof();
        remove=true;
      } G_ENDCATCH;
    } // if (req->thumb_file)
    
    if (req->image_file)
    {
      G_TRY {
	       // Decode the file if necessary. Or just used predecoded image.
        GSafeFlags & file_flags=req->image_file->get_safe_flags();
        {
          GMonitorLock lock(&file_flags);
          if (!req->image_file->is_decoding())
          {
            if (req->image_file->is_decode_ok())
            {
              // We can generate it now
              const GP<DjVuImage> dimg(DjVuImage::create(req->image_file));
              
              dimg->wait_for_complete_decode();
              
              int width = 160;
              int height = 160;
              
              if( dimg->get_width() )
                width = dimg->get_width();
              if( dimg->get_height() )
                height = dimg->get_height();
              
              GRect rect(0, 0, 160, height*160/width);
              GP<GPixmap> pm=dimg->get_pixmap(rect, rect, thumb_gamma);
              if (!pm)
              {
                GP<GBitmap> bm=dimg->get_bitmap(rect, rect, sizeof(int));
                if(bm)
                  pm=GPixmap::create(*bm);
                else
                  pm = GPixmap::create(rect.height(), rect.width(), 
                                       &GPixel::WHITE);
              }
              
              // Store and compress the pixmap
              GP<IW44Image> iwpix=IW44Image::create_encode(*pm);
              GP<ByteStream> gstr=ByteStream::create();
              IWEncoderParms parms;
              parms.slices=97;
              parms.bytes=0;
              parms.decibels=0;
              iwpix->encode_chunk(gstr, parms);
              TArray<char> data=gstr->get_data();
              
              req->data_pool->add_data((const char *) data, data.size());
              req->data_pool->set_eof();
              
              req->thumb_file=0;
              req->image_file=0;
              remove=true;
            } else if (req->image_file->is_decode_failed())
            {
              // Unfortunately we cannot decode it
              req->thumb_file=0;
              req->image_file=0;
              req->data_pool->set_eof();
              remove=true;
            } else
            {
              req->image_file->start_decode();
            }
          }
        }
      } G_CATCH(exc) {
        GUTF8String msg="Failed to decode thumbnails:\n";
        msg+=exc.get_cause();
        get_portcaster()->notify_error(this, msg);
        
	       // Get rid of this request
        req->image_file=0;
        req->thumb_file=0;
        req->data_pool->set_eof();
        remove=true;
      } G_ENDCATCH;
    }
    
    if (remove)
    {
      GPosition this_pos=pos;
      ++pos;
      threqs_list.del(this_pos);
    } else ++pos;
  }
}

GP<DjVuDocument::ThumbReq>
DjVuDocument::add_thumb_req(const GP<ThumbReq> & thumb_req)
      // Will look through the list of pending requests for thumbnails
      // and try to add the specified request. If a duplicate is found,
      // it will be returned and the list will not be modified
{
   GCriticalSectionLock lock(&threqs_lock);
   for(GPosition pos=threqs_list;pos;++pos)
   {
      GP<ThumbReq> req=threqs_list[pos];
      if (req->page_num==thumb_req->page_num)
	 return req;
   }
   threqs_list.append(thumb_req);
   return thumb_req;
}

GList<GUTF8String>
DjVuDocument::get_id_list(void)
{
  GList<GUTF8String> ids;
  if (is_init_complete())
  {
    if(djvm_dir)
    {
      GPList<DjVmDir::File> files_list=djvm_dir->get_files_list();
      for(GPosition pos=files_list;pos;++pos)
      {
        ids.append(files_list[pos]->get_load_name());
      }
    }else
    {
      const int page_num=get_pages_num();
      for(int page=0;page<page_num;page++)
      { 
        ids.append(page_to_url(page).fname());
      }
    }
  }
  return ids;
}

void
DjVuDocument::map_ids(GMap<GUTF8String,void *> &map)
{
  GList<GUTF8String> ids=get_id_list();
  for(GPosition pos=ids;pos;++pos)
  {
    map[ids[pos]]=0;
  }
}

GP<DataPool>
DjVuDocument::get_thumbnail(int page_num, bool dont_decode)
{
   DEBUG_MSG("DjVuDocument::get_thumbnail(): page_num=" << page_num << "\n");
   DEBUG_MAKE_INDENT(3);

   if (!is_init_complete()) return 0;
   
   {
	 // See if we already have request for this thumbnail pending
      GCriticalSectionLock lock(&threqs_lock);
      for(GPosition pos=threqs_list;pos;++pos)
      {
	 GP<ThumbReq> req=threqs_list[pos];
	 if (req->page_num==page_num)
	    return req->data_pool;	// That's it. Just return it.
      }
   }

      // No pending request for this page... Create one
   GP<ThumbReq> thumb_req=new ThumbReq(page_num, DataPool::create());
   
      // First try to find predecoded thumbnail
   if (get_doc_type()==INDIRECT || get_doc_type()==BUNDLED)
   {
	 // Predecoded thumbnails exist for new formats only
      GPList<DjVmDir::File> files_list=djvm_dir->get_files_list();
      GP<DjVmDir::File> thumb_file;
      int thumb_start=0;
      int page_cnt=-1;
      for(GPosition pos=files_list;pos;++pos)
      {
	 GP<DjVmDir::File> f=files_list[pos];
	 if (f->is_thumbnails())
	 {
	    thumb_file=f;
	    thumb_start=page_cnt+1;
	 } else if (f->is_page())
         {
           page_cnt++;
         }
	 if (page_cnt==page_num) break;
      }
      if (thumb_file)
      {
	    // That's the file with the desired thumbnail image
	 thumb_req->thumb_file=get_djvu_file(thumb_file->get_load_name());
	 thumb_req->thumb_chunk=page_num-thumb_start;
	 thumb_req=add_thumb_req(thumb_req);
	 process_threqs();
	 return thumb_req->data_pool;
      }
   }

      // Apparently we're out of luck and need to decode the requested
      // page (unless it's already done and if it's allowed) and render
      // it into the thumbnail. If dont_decode is true, do not attempt
      // to create this file (because this will result in a request for data)
   GP<DjVuFile> file=get_djvu_file(page_num, dont_decode);
   if (file)
   {
      thumb_req->image_file=file;

	 // I'm locking the flags here to make sure, that DjVuFile will not
	 // change its state in between of the checks.
      GSafeFlags & file_flags=file->get_safe_flags();
      {
	 GMonitorLock lock(&file_flags);
	 if (thumb_req->image_file->is_decode_ok() || !dont_decode)
	 {
	       // Just add it to the list and call process_threqs(). It
	       // will start decoding if necessary
	    thumb_req=add_thumb_req(thumb_req);
	    process_threqs();
	 } else
	 {
	       // Nothing can be done return ZERO
	    thumb_req=0;
	 }
      }
   } else thumb_req=0;
   
   if (thumb_req) return thumb_req->data_pool;
   else return 0;
}

static void
add_to_cache(const GP<DjVuFile> & f, GMap<GURL, void *> & map,
	     DjVuFileCache * cache)
{
   GURL url=f->get_url();
   DEBUG_MSG("DjVuDocument::add_to_cache(): url='" << url << "'\n");
   DEBUG_MAKE_INDENT(3);
   
   if (!map.contains(url))
   {
      map[url]=0;
      cache->add_file(f);
      
      GPList<DjVuFile> list;
      for(GPosition pos=list;pos;++pos)
	 add_to_cache(list[pos], map, cache);
   }
}

void
DjVuDocument::add_to_cache(const GP<DjVuFile> & f)
{
   if (cache)
   {
      GMap<GURL, void *> map;
      ::add_to_cache(f, map, cache);
   }
}

void
DjVuDocument::notify_file_flags_changed(const DjVuFile * source,
					long set_mask, long clr_mask)
{
      // Don't check here if the document is initialized or not.
      // This function may be called when it's not.
      // check();
   if (set_mask & DjVuFile::DECODE_OK)
   {
      set_file_aliases(source);
      if (cache) add_to_cache((DjVuFile *) source);
      if(!needs_compression_flag)
      {
        if(source->needs_compression())
        {
          can_compress_flag=true;
          needs_compression_flag=true;
        }else if(source->can_compress())
        {
          can_compress_flag=true;
        }
      }
   }
   process_threqs();
}

GP<DjVuFile>
DjVuDocument::id_to_file(const DjVuPort * source, const GUTF8String &id)
{
   return (DjVuFile *) get_djvu_file(id);
}

GP<DataPool>
DjVuDocument::request_data(const DjVuPort * source, const GURL & url)
{
   DEBUG_MSG("DjVuDocument::request_data(): seeing if we can do it\n");
   DEBUG_MAKE_INDENT(3);

   if (url==init_url)
     return init_data_pool;

   check();	// Don't put it before 'init_data_pool'

   {
	 // See if there is a file in the "UnnamedFiles" list.
	 // If it's there, then create an empty DataPool and store its
	 // pointer in the list. The "init thread" will eventually
	 // do smth with it.
      GCriticalSectionLock lock(&ufiles_lock);
      for(GPosition pos=ufiles_list;pos;++pos)
      {
	 GP<UnnamedFile> f=ufiles_list[pos];
	 if (f->url==url)
	 {
	    DEBUG_MSG("Found tmp unnamed DjVuFile. Return empty DataPool\n");
	       // Remember the DataPool. We will connect it to the
	       // actual data after the document structure becomes known
	    f->data_pool=DataPool::create();
	    return f->data_pool;
	 }
      }
   }

      // Well, the url is not in the "UnnamedFiles" list, but it doesn't
      // mean, that it's not "artificial". Stay alert!
   GP<DataPool> data_pool;
   if (flags & DOC_TYPE_KNOWN)
      switch(doc_type)
      {
	 case OLD_BUNDLED:
	 {
	    if (flags & DOC_DIR_KNOWN)
	    {
	       DEBUG_MSG("The document is in OLD_BUNDLED format\n");
	       if (url.base()!=init_url)
		        G_THROW( ERR_MSG("DjVuDocument.URL_outside") "\t"+url.get_string());
	 
	       GP<DjVmDir0::FileRec> file=djvm_dir0->get_file(url.fname());
	       if (!file)
               {
                 G_THROW( ERR_MSG("DjVuDocument.file_outside") "\t"+url.fname());
               }
	       data_pool=DataPool::create(init_data_pool, file->offset, file->size);
	    }
	    break;
	 }
	 case BUNDLED:
	 {
	    if (flags & DOC_DIR_KNOWN)
	    {
	       DEBUG_MSG("The document is in new BUNDLED format\n");
	       if (url.base()!=init_url)
               {
		 G_THROW( ERR_MSG("DjVuDocument.URL_outside") "\t"
                   +url.get_string());
               }
	 
	       GP<DjVmDir::File> file=djvm_dir->id_to_file(url.fname());
	       if (!file)
               {
                 G_THROW( ERR_MSG("DjVuDocument.file_outside") "\t"+url.fname());
               }
	       data_pool=DataPool::create(init_data_pool, file->offset, file->size);
	    }
	    break;
	 }
	 case SINGLE_PAGE:
	 case OLD_INDEXED:
	 case INDIRECT:
	 {
	    DEBUG_MSG("The document is in SINGLE_PAGE or OLD_INDEXED or INDIRECT format\n");
	    if (flags & DOC_DIR_KNOWN)
	       if (doc_type==INDIRECT && !djvm_dir->id_to_file(url.fname()))
		        G_THROW( ERR_MSG("DjVuDocument.URL_outside2") "\t"+url.get_string());
	 
	    if (url.is_local_file_url())
	    {
//	       GUTF8String fname=GOS::url_to_filename(url);
//	       if (GOS::basename(fname)=="-") fname="-";
	       DEBUG_MSG("url=" << url << "\n");

	       data_pool=DataPool::create(url);
	    }
	 }
      }
   return data_pool;
}


static void
add_file_to_djvm(const GP<DjVuFile> & file, bool page,
		 DjVmDoc & doc, GMap<GURL, void *> & map)
      // This function is used only for obsolete formats.
      // For new formats there is no need to process files recursively.
      // All information is already available from the DJVM chunk
{
   GURL url=file->get_url();

   if (!map.contains(url))
   {
      map[url]=0;

      if (file->get_chunks_number()>0 && !file->contains_chunk("NDIR"))
      {
	    // Get the data and unlink any file containing NDIR chunk.
	    // Yes. We're lazy. We don't check if those files contain
	    // anything else.
	 GPosition pos;
	 GPList<DjVuFile> files_list=file->get_included_files(false);
	 GP<DataPool> data=file->get_djvu_data(false);
	 for(pos=files_list;pos;++pos)
	 {
	    GP<DjVuFile> f=files_list[pos];
	    if (f->contains_chunk("NDIR"))
	       data=DjVuFile::unlink_file(data, f->get_url().fname());
	 }
	 
	    // Finally add it to the document
	 GUTF8String name=file->get_url().fname();
	 GP<DjVmDir::File> file_rec=DjVmDir::File::create(
           name, name, name,
           page ? DjVmDir::File::PAGE : DjVmDir::File::INCLUDE );
	 doc.insert_file(file_rec, data, -1);

	    // And repeat for all included files
	 for(pos=files_list;pos;++pos)
	    add_file_to_djvm(files_list[pos], false, doc, map);
      }
   }
}

static void
add_file_to_djvm(const GP<DjVuFile> & file, bool page,
		 DjVmDoc & doc, GMap<GURL, void *> & map, 
                 bool &needs_compression_flag, bool &can_compress_flag )
{
  if(!needs_compression_flag)
  {
    if(file->needs_compression())
    {
      can_compress_flag=true;
      needs_compression_flag=true;
    }else if(file->can_compress())
    {
      can_compress_flag=true;
    }
  }
  add_file_to_djvm(file,page,doc,map);
}

static void
local_get_url_names(DjVuFile * f,const GMap<GURL, void *> & map,GMap<GURL,void *> &tmpmap)
{
   GURL url=f->get_url();
   if (!map.contains(url) && !tmpmap.contains(url))
   {
      tmpmap[url]=0;
      f->process_incl_chunks();
      GPList<DjVuFile> files_list=f->get_included_files(false);
      for(GPosition pos=files_list;pos;++pos)
         local_get_url_names(files_list[pos], map, tmpmap);
   }
}

static void
local_get_url_names(DjVuFile * f, GMap<GURL, void *> & map)
{
   GMap<GURL,void *> tmpmap;
   local_get_url_names(f,map,tmpmap);
   for(GPosition pos=tmpmap;pos;++pos)
     map[tmpmap.key(pos)]=0;
}

GList<GURL>
DjVuDocument::get_url_names(void)
{
  check();

  GCriticalSectionLock lock(&url_names_lock);
  if(has_url_names)
    return url_names;

  GMap<GURL, void *> map;
  int i;
  if (doc_type==BUNDLED || doc_type==INDIRECT)
  {
    GPList<DjVmDir::File> files_list=djvm_dir->get_files_list();
    for(GPosition pos=files_list;pos;++pos)
    {
      GURL url=id_to_url(files_list[pos]->get_load_name());
      map[url]=0;
    }
  }else
  {
    int pages_num=get_pages_num();
    for(i=0;i<pages_num;i++)
    {
      G_TRY
      {
        local_get_url_names(get_djvu_file(i), map);
      }
      G_CATCH(ex)
      {
        // Why is this try/catch block here?
        G_TRY { 
          get_portcaster()->notify_error(this, ex.get_cause()); 
          GUTF8String emsg = ERR_MSG("DjVuDocument.exclude_page") "\t" + GUTF8String(i+1);
          get_portcaster()->notify_error(this, emsg);
        }
        G_CATCH_ALL
        {
          G_RETHROW;
        }
        G_ENDCATCH;
      }
      G_ENDCATCH;
    }
  }
  for(GPosition j=map;j;++j)
  {
    if (map.key(j).is_local_file_url())
    {
      url_names.append(map.key(j));
    }
  }
  has_url_names=true;
  return url_names;
}

GP<DjVmDoc>
DjVuDocument::get_djvm_doc()
      // This function may block for data
{
   check();
   DEBUG_MSG("DjVuDocument::get_djvm_doc(): creating the DjVmDoc\n");
   DEBUG_MAKE_INDENT(3);

   if (!is_init_complete())
     G_THROW( ERR_MSG("DjVuDocument.init_not_done") );

   GP<DjVmDoc> doc=DjVmDoc::create();

   if (doc_type==BUNDLED || doc_type==INDIRECT)
     {
       GPList<DjVmDir::File> files_list=djvm_dir->get_files_list();
       for(GPosition pos=files_list;pos;++pos)
         {
           GP<DjVmDir::File> f=new DjVmDir::File(*files_list[pos]);
           GP<DjVuFile> file=url_to_file(id_to_url(f->get_load_name()));
           GP<DataPool> data;
           if (file->is_modified()) 
             data=file->get_djvu_data(false);
           else 
             data=file->get_init_data_pool();
           doc->insert_file(f, data);
         }
       if (djvm_nav)
         doc->set_djvm_nav(djvm_nav);
     } 
   else if (doc_type==SINGLE_PAGE)
     {
       DEBUG_MSG("Creating: djvm for a single page document.\n");
       GMap<GURL, void *> map_add;
       GP<DjVuFile> file=get_djvu_file(0);
       add_file_to_djvm(file, true, *doc, map_add,
                        needs_compression_flag,can_compress_flag);
     } 
   else
     {
       DEBUG_MSG("Converting: the document is in an old format.\n");
       GMap<GURL, void *> map_add;
       if(recover_errors == ABORT)
         {
           for(int page_num=0;page_num<ndir->get_pages_num();page_num++)
             {
               GP<DjVuFile> file=url_to_file(ndir->page_to_url(page_num));
               add_file_to_djvm(file, true, *doc, map_add,
                                needs_compression_flag,can_compress_flag);
             }
         }
       else
         {
           for(int page_num=0;page_num<ndir->get_pages_num();page_num++)
             {
               G_TRY
                 {
                   GP<DjVuFile> file=url_to_file(ndir->page_to_url(page_num));
                   add_file_to_djvm(file, true, *doc, map_add,
                                    needs_compression_flag,can_compress_flag);
                 }
               G_CATCH(ex)
                 {
                   G_TRY { 
                     get_portcaster()->notify_error(this, ex.get_cause());
                     GUTF8String emsg = ERR_MSG("DjVuDocument.skip_page") "\t" 
                                      + GUTF8String(page_num+1);
                     get_portcaster()->notify_error(this, emsg);
                   }
                   G_CATCH_ALL
                     {
                       G_RETHROW;
                     }
                   G_ENDCATCH;
                 }
               G_ENDCATCH;
             }
         }
     }
   return doc;
}

void
DjVuDocument::write( const GP<ByteStream> &gstr,
  const GMap<GUTF8String,void *> &reserved)
{
  DEBUG_MSG("DjVuDocument::write(): storing DjVmDoc into ByteStream\n");
  DEBUG_MAKE_INDENT(3);
  get_djvm_doc()->write(gstr,reserved); 
}

void
DjVuDocument::write(const GP<ByteStream> &gstr, bool force_djvm)
{
  DEBUG_MSG("DjVuDocument::write(): storing DjVmDoc into ByteStream\n");
  DEBUG_MAKE_INDENT(3);
   
  GP<DjVmDoc> doc=get_djvm_doc();
  GP<DjVmDir> dir=doc->get_djvm_dir();

  bool singlepage = (dir->get_files_num()==1 && !djvm_nav && !force_djvm);
  if (singlepage)
  {
    // maybe save as single page
    DjVmDir::File *file = dir->page_to_file(0);
    if (file->get_title() != file->get_load_name())
      singlepage = false;
  }
  if (! singlepage)
  {
    doc->write(gstr);
  }
  else
  {
    GPList<DjVmDir::File> files_list=dir->resolve_duplicates(false);
    GP<DataPool> pool=doc->get_data(files_list[files_list]->get_load_name());
    GP<ByteStream> pool_str=pool->get_stream();
    ByteStream &str=*gstr;
    str.writall(octets,4);
    str.copy(*pool_str);
  }
}

void
DjVuDocument::expand(const GURL &codebase, const GUTF8String &idx_name)
{
   DEBUG_MSG("DjVuDocument::expand(): codebase='" << codebase << "'\n");
   DEBUG_MAKE_INDENT(3);
   
   GP<DjVmDoc> doc=get_djvm_doc();
   doc->expand(codebase, idx_name);
}

void
DjVuDocument::save_as(const GURL &where, bool bundled)
{
   DEBUG_MSG("DjVuDocument::save_as(): where='" << where <<
	     "', bundled=" << bundled << "\n");
   DEBUG_MAKE_INDENT(3);
   
   if (needs_compression())
   { 
     if(!djvu_compress_codec)
     {
       G_THROW( ERR_MSG("DjVuDocument.comp_codec") );
     }
     GP<ByteStream> gmbs=ByteStream::create();
     write(gmbs);
     ByteStream &mbs=*gmbs;
     mbs.flush();
     mbs.seek(0,SEEK_SET);
     (*djvu_compress_codec)(gmbs,where,bundled);
   }else if (bundled)
   {
      DataPool::load_file(where);
      write(ByteStream::create(where, "wb"));
   } else 
   {
     expand(where.base(), where.fname());
   }
}

static const char prolog[]="<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<!DOCTYPE DjVuXML PUBLIC \"-//W3C//DTD DjVuXML 1.1//EN\" \"pubtext/DjVuXML-s.dtd\">\n<DjVuXML>\n<HEAD>";
static const char start_xml[]="</HEAD>\n<BODY>\n";
static const char end_xml[]="</BODY>\n</DjVuXML>\n";

void
DjVuDocument::writeDjVuXML(const GP<ByteStream> &gstr_out,
                           int flags, int page) const
{
  ByteStream &str_out=*gstr_out;
  str_out.writestring(
    prolog+get_init_url().get_string().toEscaped()+start_xml);
  const int pages=wait_get_pages_num();
  int pstart = (page < 0) ? 0 : page;
  int pend = (page < 0) ? pages : page+1;
  for(int page_num=pstart; page_num<pend; ++page_num)
  {
    const GP<DjVuImage> dimg(get_page(page_num,true));
    if(!dimg)
    {
      G_THROW( ERR_MSG("DjVuToText.decode_failed") );
    }
    dimg->writeXML(str_out,get_init_url(),flags);
  }
  str_out.writestring(GUTF8String(end_xml));
}


#ifdef HAVE_NAMESPACES
}
# ifndef NOT_USING_DJVU_NAMESPACE
using namespace DJVU;
# endif
#endif
